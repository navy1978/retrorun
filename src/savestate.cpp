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
#include <stdexcept>
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

    logger.log(Logger::DEB, "Calling retro_unserialize: ptr=%p, size=%ld", ptr, size);
    logger.log(Logger::DEB, "First 16 bytes of state buffer: %02X %02X %02X %02X %02X %02X %02X %02X",
        ((unsigned char*)ptr)[0], ((unsigned char*)ptr)[1], ((unsigned char*)ptr)[2], ((unsigned char*)ptr)[3],
        ((unsigned char*)ptr)[4], ((unsigned char*)ptr)[5], ((unsigned char*)ptr)[6], ((unsigned char*)ptr)[7]);

    if (pthread_self() != main_thread_id) {
        logger.log(Logger::ERR, "Error: retro_unserialize() doesn't run in the main thread!");
        free(ptr);
        return -1;
    }

    fflush(stdout);
    pthread_mutex_lock(&stateMutex);
    video_synchronize();
    audio_flush();
    mprotect(ptr, size, PROT_READ);
    bool result = g_retro.retro_unserialize(ptr, size);
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
    pthread_mutex_unlock(&stateMutex);

    free(ptr);

    if (result)
        logger.log(Logger::DEB, "File '%s': loaded correctly!", saveName);
    else
        logger.log(Logger::WARN, "File '%s': loaded correctly but with no effects!", saveName);

    return result ? 0 : -1;
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
