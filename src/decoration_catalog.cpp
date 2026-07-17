#include "decoration_catalog.h"

#include "config.h"
#include "decoration.h"
#include "globals.h"
#include "menu/menu_manager.h"

#include <curl/curl.h>
#include <png.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Pack {
    const char* id;
    const char* name;
    const char* image;
    double viewport_x;
    double viewport_y;
    double viewport_width;
    double viewport_height;
};

// Static, non-interactive borders from libretro/common-overlays. Keeping the
// manifest in RetroRun avoids downloading and parsing a large remote catalog
// on memory-constrained handhelds.
constexpr Pack packs[] = {
    {"gameboy", "Game Boy", "gb.png", 0.19, 0.0, 0.636, 1.0},
    {"nes-smb2", "NES - SMB2", "nes-smb2-integer.png", 0.2335, 0.0855, 0.5335, 0.830},
    {"snes-chrono", "SNES - Chrono", "snes-ct-integer.png", 0.2335, 0.0855, 0.5335, 0.830},
    {"snes-metroid", "SNES - Metroid", "snes-metroid-integer.png", 0.2335, 0.0855, 0.5335, 0.830},
    {"snes-allstars", "SNES - All-Stars", "snes-smas-integer.png", 0.2335, 0.0855, 0.5335, 0.830},
    {"tv", "Classic TV", "tv-integer.png", 0.2335, 0.0855, 0.5335, 0.830},
};

constexpr size_t pack_count = sizeof(packs) / sizeof(packs[0]);
constexpr curl_off_t maximum_download_size = 8 * 1024 * 1024;
constexpr const char* source_root =
    "https://raw.githubusercontent.com/libretro/common-overlays/master/borders/img/";

std::mutex state_mutex;
std::thread worker;
size_t selected = 0;
std::string status_text = "Ready";
std::atomic<int> progress{-1};
std::atomic<bool> running{false};
std::atomic<bool> reload_pending{false};

struct Source {
    std::string id;
    std::string label;
};

std::vector<Source> sources;
size_t selected_source = 0;

void add_source(const std::string& id, const std::string& label) {
    for (const auto& source : sources)
        if (source.id == id) return;
    sources.push_back({id, label});
}

void scan_sources(const std::filesystem::path& root, const char* id_prefix,
                  const char* label_prefix) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return;
    if (std::filesystem::is_directory(root / "systems", error))
        add_source(std::string(id_prefix) + ":default",
                   std::string(label_prefix) + ": installed");
    for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
        if (!entry.is_directory(error) ||
            !std::filesystem::is_directory(entry.path() / "systems", error)) continue;
        const std::string pack = entry.path().filename().string();
        add_source(std::string(id_prefix) + ":" + pack,
                   std::string(label_prefix) + ": " + pack);
    }
}

std::filesystem::path install_directory() {
    return std::filesystem::path(decoration_directory()) / "downloads" / "libretro";
}

std::filesystem::path image_path(const Pack& pack) {
    return install_directory() / (std::string(pack.id) + ".png");
}

std::filesystem::path info_path(const Pack& pack) {
    return install_directory() / (std::string(pack.id) + ".info");
}

bool installed(const Pack& pack) {
    std::error_code error;
    return std::filesystem::is_regular_file(image_path(pack), error) &&
           std::filesystem::is_regular_file(info_path(pack), error);
}

void set_status(std::string value) {
    std::lock_guard<std::mutex> lock(state_mutex);
    status_text = std::move(value);
}

size_t write_download(char* data, size_t size, size_t count, void* userdata) {
    FILE* file = static_cast<FILE*>(userdata);
    return std::fwrite(data, size, count, file);
}

int report_progress(void*, curl_off_t total, curl_off_t current, curl_off_t, curl_off_t) {
    if (total > 0)
        progress = static_cast<int>(current * 100 / total);
    return 0;
}

bool validate_png(const std::filesystem::path& path, unsigned* width, unsigned* height) {
    png_image image = {};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, path.string().c_str())) return false;
    *width = image.width;
    *height = image.height;
    png_image_free(&image);
    return *width > 0 && *height > 0;
}

bool write_info(const Pack& pack, unsigned width, unsigned height,
                const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::trunc);
    if (!file) return false;
    const double left = pack.viewport_x * width;
    const double top = pack.viewport_y * height;
    const double right = (1.0 - pack.viewport_x - pack.viewport_width) * width;
    const double bottom = (1.0 - pack.viewport_y - pack.viewport_height) * height;
    file << "{\n"
         << "  \"width\": " << width << ",\n"
         << "  \"height\": " << height << ",\n"
         << "  \"top\": " << top << ",\n"
         << "  \"left\": " << left << ",\n"
         << "  \"bottom\": " << bottom << ",\n"
         << "  \"right\": " << right << "\n"
         << "}\n";
    return file.good();
}

void write_attribution() {
    std::ofstream file(install_directory() / "ATTRIBUTION.txt", std::ios::trunc);
    if (!file) return;
    file << "Artwork: libretro/common-overlays\n"
         << "Source: https://github.com/libretro/common-overlays\n"
         << "License: Creative Commons Attribution 4.0 International\n"
         << "License URL: https://creativecommons.org/licenses/by/4.0/\n";
}

void download_pack(Pack pack) {
    std::error_code error;
    std::filesystem::create_directories(install_directory(), error);
    if (error) {
        set_status("Error: cannot create folder");
        logger.log(Logger::ERR, "Decoration download: cannot create %s",
                   install_directory().string().c_str());
        running = false;
        return;
    }

    const auto destination = image_path(pack);
    const auto temporary_image = destination.string() + ".part";
    const auto temporary_info = info_path(pack).string() + ".part";
    FILE* file = std::fopen(temporary_image.c_str(), "wb");
    if (!file) {
        set_status("Error: cannot create file");
        logger.log(Logger::ERR, "Decoration download: cannot create %s",
                   temporary_image.c_str());
        running = false;
        return;
    }

    CURL* curl = curl_easy_init();
    CURLcode result = CURLE_FAILED_INIT;
    long http_status = 0;
    if (curl) {
        const std::string url = std::string(source_root) + pack.image;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "RetroRun/3.0 decoration-downloader");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
        curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, maximum_download_size);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_download);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, report_progress);
        result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
        curl_easy_cleanup(curl);
    }
    std::fclose(file);

    unsigned width = 0, height = 0;
    if (result != CURLE_OK || http_status != 200 ||
        !validate_png(temporary_image, &width, &height) ||
        !write_info(pack, width, height, temporary_info)) {
        std::filesystem::remove(temporary_image, error);
        std::filesystem::remove(temporary_info, error);
        set_status(result == CURLE_FILESIZE_EXCEEDED ? "Error: file too large"
                                                     : "Error: download failed");
        logger.log(Logger::ERR, "Decoration download failed: pack=%s, curl=%s, HTTP=%ld",
                   pack.id, curl_easy_strerror(result), http_status);
        progress = -1;
        running = false;
        return;
    }

    std::filesystem::rename(temporary_image, destination, error);
    if (!error)
        std::filesystem::rename(temporary_info, info_path(pack), error);
    if (error) {
        const std::string install_error = error.message();
        std::filesystem::remove(temporary_image, error);
        std::filesystem::remove(temporary_info, error);
        set_status("Error: install failed");
        logger.log(Logger::ERR, "Decoration install failed: pack=%s, error=%s",
                   pack.id, install_error.c_str());
    } else {
        write_attribution();
        set_status("Installed");
        logger.log(Logger::INF, "Decoration installed from libretro/common-overlays: %s",
                   pack.id);
        reload_pending = true;
    }
    progress = -1;
    running = false;
}
}

void decoration_catalog_init() {
    sources.clear();
    selected_source = 0;
    add_source("auto", "Automatic");
    add_source("retrorun", "RetroRun");
    scan_sources("/tmp/overlays/bezels", "amberelec", "AmberELEC");
    scan_sources("/storage/roms/bezels", "amberelec", "AmberELEC");
    scan_sources("/roms/bezels", "arkos", "ArkOS");
    scan_sources("/roms2/bezels", "arkos", "ArkOS");
    const auto source_config = conf_map.find("retrorun_decoration_source");
    if (source_config != conf_map.end()) {
        bool found_source = false;
        for (size_t index = 0; index < sources.size(); ++index)
            if (sources[index].id == source_config->second) {
                selected_source = index;
                found_source = true;
                break;
            }
        if (!found_source && source_config->second != "auto" &&
            source_config->second != "retrorun") {
            add_source(source_config->second,
                       "Unavailable: " + source_config->second);
            selected_source = sources.size() - 1;
        }
    }
    const auto configured = conf_map.find("retrorun_decoration_pack");
    if (configured != conf_map.end()) {
        for (size_t index = 0; index < pack_count; ++index) {
            if (configured->second == packs[index].id) {
                selected = index;
                break;
            }
        }
    }
    set_status(installed(packs[selected]) ? "Installed" : "Not installed");
}

void decoration_catalog_update() {
    if (reload_pending.exchange(false)) {
        conf_map["retrorun_decoration_pack"] = packs[selected].id;
        persistVideoSetting("retrorun_decoration_pack", packs[selected].id);
        decoration_set_enabled(true);
        decoration_reload();
    }
    if (!running && worker.joinable()) worker.join();
}

void decoration_catalog_shutdown() {
    if (worker.joinable()) worker.join();
}

std::string decoration_catalog_pack_label() {
    return std::string("RetroRun pack: ") + packs[selected].name;
}

std::string decoration_catalog_source_label() {
    return sources.empty() ? "Source: Automatic"
                           : "Source: " + sources[selected_source].label;
}

std::string decoration_catalog_active_label() {
    return "Using: " + decoration_source_label();
}

std::string decoration_catalog_status_label() {
    if (running) {
        const int value = progress.load();
        return value >= 0 ? "RetroRun: Downloading " + std::to_string(value) + "%"
                          : "RetroRun: Connecting...";
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    return "RetroRun: " + status_text;
}

void decoration_catalog_select(int button) {
    if (running) return;
    if (button == LEFT)
        selected = selected == 0 ? pack_count - 1 : selected - 1;
    else if (button == RIGHT || button == A_BUTTON)
        selected = (selected + 1) % pack_count;
    else
        return;
    conf_map["retrorun_decoration_pack"] = packs[selected].id;
    persistVideoSetting("retrorun_decoration_pack", packs[selected].id);
    set_status(installed(packs[selected]) ? "Installed" : "Not installed");
    decoration_reload();
}

void decoration_catalog_select_source(int button) {
    if (running || sources.empty()) return;
    if (button == LEFT)
        selected_source = selected_source == 0 ? sources.size() - 1 : selected_source - 1;
    else if (button == RIGHT || button == A_BUTTON)
        selected_source = (selected_source + 1) % sources.size();
    else
        return;
    conf_map["retrorun_decoration_source"] = sources[selected_source].id;
    persistVideoSetting("retrorun_decoration_source", sources[selected_source].id);
    decoration_reload();
}

void decoration_catalog_install() {
    if (running) return;
    if (worker.joinable()) worker.join();
    progress = -1;
    running = true;
    set_status("Connecting...");
    worker = std::thread(download_pack, packs[selected]);
}

void decoration_catalog_remove() {
    if (running) return;
    std::error_code error;
    std::filesystem::remove(image_path(packs[selected]), error);
    error.clear();
    std::filesystem::remove(info_path(packs[selected]), error);
    set_status("Not installed");
    logger.log(Logger::INF, "Decoration removed: %s", packs[selected].id);
    decoration_reload();
}
