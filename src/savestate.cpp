/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include "savestate.h"
#include "core_loader.h"
#include "config.h"
#include "globals.h"
#include "input.h"
#include "audio.h"
#include "video.h"
#include "platform.h"
#include "libretro.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <pthread.h>
#include <sys/mman.h>

extern pthread_t main_thread_id;

// --- Helpers ---

static const char *FileNameFromPath(const char *fullpath)
{
    const char *ptr = strrchr(fullpath, '/');
    if (!ptr)
        ptr = fullpath;
    else
        ++ptr;
    return ptr;
}

static inline int getRetroMemory()
{
    return RETRO_MEMORY_SAVE_RAM;
}

// --- LoadState ---

static pthread_mutex_t stateMutex = PTHREAD_MUTEX_INITIALIZER;

namespace {
struct PendingLoadState {
    void *data = nullptr;
    size_t size = 0;
    std::string path;
    int slot = 0;
    bool auto_load = false;
    bool success = false;
    std::string error;
};

std::mutex asyncLoadMutex;
std::thread asyncLoadThread;
std::unique_ptr<PendingLoadState> asyncLoadReady;
std::atomic<bool> asyncLoadBusy{false};
std::atomic<bool> asyncLoadReadyFlag{false};
std::atomic<int> asyncLoadProgress{-1};
std::string asyncLoadMessage;

void set_async_load_message(const std::string& message)
{
    std::lock_guard<std::mutex> lock(asyncLoadMutex);
    asyncLoadMessage = message;
}

std::string load_label(int slot, bool autoLoad)
{
    return autoLoad || slot <= 0 ? "auto state" : "slot " + std::to_string(slot);
}

bool apply_state_buffer(const void *ptr, size_t size)
{
    logger.log(Logger::DEB, "Calling retro_unserialize: ptr=%p, size=%zu", ptr, size);
    const unsigned char *bytes = static_cast<const unsigned char *>(ptr);
    unsigned char header[8] = {};
    if (bytes)
        std::memcpy(header, bytes, std::min<size_t>(size, sizeof(header)));
    logger.log(Logger::DEB,
        "First 8 bytes of state buffer: %02X %02X %02X %02X %02X %02X %02X %02X",
        header[0], header[1], header[2], header[3],
        header[4], header[5], header[6], header[7]);

    if (pthread_self() != main_thread_id) {
        logger.log(Logger::ERR, "Error: retro_unserialize() doesn't run in the main thread!");
        return false;
    }

    fflush(stdout);
    pthread_mutex_lock(&stateMutex);
    video_synchronize();
    audio_flush();
    mprotect(const_cast<void *>(ptr), size, PROT_READ);
    bool result = g_retro.retro_unserialize(ptr, size);
    mprotect(const_cast<void *>(ptr), size, PROT_READ | PROT_WRITE);
    pthread_mutex_unlock(&stateMutex);
    return result;
}
}

int LoadState(const char *saveName)
{
    static time_t lastLoadTime = 0;
    time_t currentTime = time(NULL);

    if (difftime(currentTime, lastLoadTime) < 1.0)
    {
        logger.log(Logger::WARN, "LoadState called too quickly; skipping execution.");
        return -1;
    }
    lastLoadTime = currentTime;

    logger.log(Logger::DEB, "Trying to Load state...");
    FILE *file = fopen(saveName, "rb");
    if (!file)
    {
        logger.log(Logger::ERR, "Error loading state: File '%s' not found!", saveName);
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    if (size < 1)
    {
        logger.log(Logger::ERR, "Error loading state, in file '%s': size is wrong!", saveName);
        fclose(file);
        return -1;
    }
    void *ptr;
    if (posix_memalign(&ptr, 16, size) != 0)
    {
        logger.log(Logger::ERR, "Memory allocation failed!");
        fclose(file);
        return -1;
    }
    for (long i = 0; i < size; i += 4096) {
        ((char *)ptr)[i] = 0;
    }

    size_t count = fread(ptr, 1, size, file);
    fclose(file);

    if ((size_t)size != count)
    {
        logger.log(Logger::ERR, "Error loading state, in file '%s': size mismatch!", saveName);
        free(ptr);
        return -1;
    }

    bool result = apply_state_buffer(ptr, size);

    free(ptr);

    if (result)
        logger.log(Logger::DEB, "File '%s': loaded correctly!", saveName);
    else
        logger.log(Logger::WARN, "File '%s': loaded correctly but with no effects!", saveName);

    return result ? 0 : -1;
}

bool StartLoadStateAsync(const char *saveName, int slotNumber, bool autoLoad)
{
    if (asyncLoadBusy.load(std::memory_order_acquire) ||
        asyncLoadReadyFlag.load(std::memory_order_acquire)) {
        logger.log(Logger::WARN, "Save state load already in progress; ignoring '%s'", saveName);
        return false;
    }
    if (asyncLoadThread.joinable())
        asyncLoadThread.join();

    const std::string path = saveName ? saveName : "";
    const std::string label = load_label(slotNumber, autoLoad);
    input_slot_memory_load_done = false;
    lastLoadSaveStateDoneOk = true;
    asyncLoadProgress.store(0, std::memory_order_release);
    set_async_load_message("Loading state " + label + "...");
    asyncLoadBusy.store(true, std::memory_order_release);

    asyncLoadThread = std::thread([path, slotNumber, autoLoad, label]() {
        auto pending = std::make_unique<PendingLoadState>();
        pending->path = path;
        pending->slot = slotNumber;
        pending->auto_load = autoLoad;

        FILE *file = fopen(path.c_str(), "rb");
        if (!file) {
            pending->error = "state file not found";
            logger.log(Logger::ERR, "Error loading state: File '%s' not found!", path.c_str());
            set_async_load_message("State file not found");
        } else {
            fseek(file, 0, SEEK_END);
            long fileSize = ftell(file);
            rewind(file);
            if (fileSize < 1) {
                pending->error = "state file is empty";
                logger.log(Logger::ERR, "Error loading state, in file '%s': size is wrong!", path.c_str());
                set_async_load_message("State file is empty");
            } else if (posix_memalign(&pending->data, 16, static_cast<size_t>(fileSize)) != 0) {
                pending->error = "memory allocation failed";
                logger.log(Logger::ERR, "Memory allocation failed while loading state '%s'", path.c_str());
                set_async_load_message("State load failed");
            } else {
                pending->size = static_cast<size_t>(fileSize);
                constexpr size_t chunkSize = 1024 * 1024;
                size_t totalRead = 0;
                while (totalRead < pending->size) {
                    const size_t toRead = std::min(chunkSize, pending->size - totalRead);
                    const size_t count = fread(static_cast<char *>(pending->data) + totalRead,
                                               1, toRead, file);
                    if (count == 0) break;
                    totalRead += count;
                    const int progress = static_cast<int>((totalRead * 100) / pending->size);
                    asyncLoadProgress.store(progress, std::memory_order_release);
                    set_async_load_message("Loading state " + label + "...");
                }
                if (totalRead == pending->size) {
                    pending->success = true;
                    asyncLoadProgress.store(100, std::memory_order_release);
                    set_async_load_message("Applying state " + label + "...");
                } else {
                    pending->error = "state file read failed";
                    logger.log(Logger::ERR, "Error loading state, in file '%s': size mismatch!", path.c_str());
                    set_async_load_message("State file read failed");
                    free(pending->data);
                    pending->data = nullptr;
                    pending->size = 0;
                }
            }
            fclose(file);
        }

        {
            std::lock_guard<std::mutex> lock(asyncLoadMutex);
            asyncLoadReady = std::move(pending);
        }
        asyncLoadReadyFlag.store(true, std::memory_order_release);
        asyncLoadBusy.store(false, std::memory_order_release);
    });
    return true;
}

void PumpLoadStateAsync()
{
    if (!asyncLoadReadyFlag.load(std::memory_order_acquire))
        return;

    std::unique_ptr<PendingLoadState> pending;
    {
        std::lock_guard<std::mutex> lock(asyncLoadMutex);
        pending = std::move(asyncLoadReady);
        asyncLoadReadyFlag.store(false, std::memory_order_release);
    }
    if (asyncLoadThread.joinable())
        asyncLoadThread.join();
    if (!pending)
        return;

    bool loaded = false;
    if (pending->success && pending->data && pending->size > 0) {
        // A libretro core's serialized state size is not guaranteed to remain
        // constant while content is running. Flycast 2021, for example,
        // computes retro_serialize_size() by serializing the current machine
        // and valid files from a later frame can legitimately be larger.
        // Pass the exact file size to retro_unserialize(), which is the API
        // responsible for validating compatibility and content integrity.
        asyncLoadProgress.store(100, std::memory_order_release);
        set_async_load_message("Applying state " + load_label(pending->slot, pending->auto_load) + "...");
        loaded = apply_state_buffer(pending->data, pending->size);
        if (loaded) {
            logger.log(Logger::DEB, "File '%s': loaded correctly!", pending->path.c_str());
            set_async_load_message(pending->auto_load ? "Auto state loaded" :
                                   "Slot " + std::to_string(pending->slot) + " loaded");
            input_pause_requested = false;
            pause_requested = false;
        } else {
            logger.log(Logger::WARN, "File '%s': failed to load or had no effect!", pending->path.c_str());
            set_async_load_message(pending->auto_load ? "Auto state failed" : "Load failed");
        }
    }
    if (!loaded && !pending->error.empty()) {
        logger.log(Logger::WARN, "State load failed for '%s': %s",
                   pending->path.c_str(), pending->error.c_str());
        set_async_load_message(pending->error);
    }

    free(pending->data);
    asyncLoadProgress.store(-1, std::memory_order_release);
    lastLoadSaveStateDoneOk = loaded;
    input_slot_memory_load_done = true;
    input_slot_memory_load_requested = false;
    lastLoadSaveStateDoneTime = static_cast<double>(time(NULL));
}

bool LoadStateAsyncBusy()
{
    return asyncLoadBusy.load(std::memory_order_acquire) ||
           asyncLoadReadyFlag.load(std::memory_order_acquire);
}

int LoadStateProgress()
{
    return asyncLoadProgress.load(std::memory_order_acquire);
}

std::string LoadStateStatusMessage()
{
    std::lock_guard<std::mutex> lock(asyncLoadMutex);
    return asyncLoadMessage;
}

void ShutdownLoadStateAsync()
{
    if (asyncLoadThread.joinable())
        asyncLoadThread.join();
    asyncLoadBusy.store(false, std::memory_order_release);
    asyncLoadProgress.store(-1, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(asyncLoadMutex);
        if (asyncLoadReady && asyncLoadReady->data)
            free(asyncLoadReady->data);
        asyncLoadReady.reset();
        asyncLoadMessage.clear();
    }
    asyncLoadReadyFlag.store(false, std::memory_order_release);
}

// --- LoadSram ---

int LoadSram(const char *saveName)
{
    try
    {
        FILE *file = fopen(saveName, "rb");
        if (!file)
        {
            logger.log(Logger::DEB, "Loading sram: File '%s' not found!", saveName);
            return -1;
        }

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);

        size_t sramSize = g_retro.retro_get_memory_size(getRetroMemory());

        if (size < 1)
        {
            logger.log(Logger::ERR, "Error loading sram, memory size wrong!");
            fclose(file);
            return -1;
        }

        if (size != (long)sramSize)
        {
            logger.log(Logger::ERR, "Error loading sram, in file '%s': size mismatch!", saveName);
            fclose(file);
            return -1;
        }

        void *ptr = g_retro.retro_get_memory_data(getRetroMemory());
        if (!ptr)
        {
            logger.log(Logger::ERR, "Error loading sram, file '%s': contains wrong memory data!", saveName);
            fclose(file);
            exit(1);
        }

        size_t count = fread(ptr, 1, size, file);
        if ((size_t)size != count)
        {
            logger.log(Logger::ERR, "Error loading sram, in file '%s': size mismatch!", saveName);
            fclose(file);
            exit(1);
        }

        fclose(file);
        logger.log(Logger::DEB, "File '%s': loaded correctly!\n", saveName);
    }
    catch (const std::exception &e)
    {
        logger.log(Logger::ERR, "a standard exception was caught, with message: '%s'", e.what());
    }
    return 0;
}

// --- SaveState ---

void SaveState(const char *saveName)
{
    static time_t lastSaveTime = 0;
    time_t currentTime = time(NULL);

    if (difftime(currentTime, lastSaveTime) < 1.0)
    {
        logger.log(Logger::WARN, "SaveState called too quickly; skipping execution.");
        return;
    }
    lastSaveTime = currentTime;

    size_t size = g_retro.retro_serialize_size();
    void *ptr = malloc(size);
    if (!ptr)
    {
        logger.log(Logger::ERR, "Error saving state: ptr not valid!");
        exit(1);
    }

    g_retro.retro_serialize(ptr, size);
    FILE *file = fopen(saveName, "wb");
    if (!file)
    {
        logger.log(Logger::ERR, "Error saving state: File '%s' cannot be opened!", saveName);
        free(ptr);
        exit(1);
    }

    size_t count = fwrite(ptr, 1, size, file);
    if (count != size)
    {
        logger.log(Logger::ERR, "Error saving state: File '%s' count not valid!", saveName);
        free(ptr);
        exit(1);
    }

    fclose(file);
    free(ptr);
    logger.log(Logger::DEB, "File '%s': saved correctly!", saveName);
    input_slot_memory_save_done = true;
    lastLoadSaveStateDoneTime = (double)time(NULL);
}

// --- SaveSram ---

void SaveSram(const char *saveName)
{
    size_t size = g_retro.retro_get_memory_size(getRetroMemory());

    if (size < 1)
    {
        logger.log(Logger::ERR, "nothing to save in srm file!, %zu", size);
        return;
    }

    void *ptr = g_retro.retro_get_memory_data(getRetroMemory());
    if (!ptr)
    {
        logger.log(Logger::ERR, "Error saving sram: ptr not valid!");
        exit(1);
    }

    FILE *file = fopen(saveName, "wb");
    if (!file)
    {
        logger.log(Logger::ERR, "Error saving sram: File '%s' cannot be opened!", saveName);
        exit(1);
    }

    size_t count = fwrite(ptr, 1, size, file);
    if (count != size)
    {
        logger.log(Logger::ERR, "Error saving sram: File '%s' count not valid!", saveName);
        exit(1);
    }

    fclose(file);
    logger.log(Logger::DEB, "Sram saved!");
}

// --- Path creation ---

char *createSramPath(const std::string &arg_rom, const std::string &opt_savedir)
{
    const char *fileName = FileNameFromPath(arg_rom.c_str());
    std::string fullNameString(fileName);
    size_t lastindex = fullNameString.find_last_of(".");
    std::string rawname = fullNameString.substr(0, lastindex);

    std::string srmAutoPath = opt_savedir + "/<gameName>.srm";
    std::string srmPathFinal = replace(srmAutoPath, "<gameName>", rawname);

    char *sramPath = (char *)malloc(srmPathFinal.length() + 1);
    strcpy(sramPath, srmPathFinal.c_str());
    return sramPath;
}

char *createSavePath(const std::string &arg_rom, const std::string &opt_savedir)
{
    const char *fileName = FileNameFromPath(arg_rom.c_str());
    std::string fullNameString(fileName);
    size_t lastindex = fullNameString.find_last_of(".");
    std::string rawname = fullNameString.substr(0, lastindex);

    std::string stateAutoPath = opt_savedir + "/<gameName>.rrstate.auto";
    if (isFlycast2021())
        stateAutoPath = opt_savedir + "/<gameName>.fc2021-rrstate.auto";

    std::string statePathFinal = replace(stateAutoPath, "<gameName>", rawname);

    char *savePath = (char *)malloc(statePathFinal.length() + 1);
    strcpy(savePath, statePathFinal.c_str());
    return savePath;
}
