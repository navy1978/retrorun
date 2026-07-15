#include "achievements.h"

#include "core_loader.h"
#include "config.h"
#include "globals.h"
#include "input.h"
#include "logger.h"
#include "fonts.h"
#include "keyboard.h"

extern "C" {
#include "rc_client.h"
#include "rc_error.h"
#include "rc_libretro.h"
}

#include <curl/curl.h>
#include <png.h>

#include <chrono>
#include <cctype>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

extern const char* opt_savedir;

namespace {
rc_client_t* client = nullptr;
rc_libretro_memory_regions_t memory = {};
std::string content;
std::vector<uint8_t> content_data;
bool enabled = false;
bool login_pending = false;
std::string login_error;
std::string game_load_error;
bool used_password_login = false;
uint64_t frames_processed = 0;
uint64_t memory_read_failures = 0;
std::chrono::steady_clock::time_point notification_until;
std::string notification_text;
std::string notification_badge_url;
struct PendingNotification { std::string text; std::string badge_url; };
std::deque<PendingNotification> notification_queue;
struct AchievementEntry {
    std::string title;
    std::string description;
    uint32_t points;
    uint8_t state;
    uint8_t unlocked;
    std::string badge_url;
};
std::vector<AchievementEntry> achievement_entries;
bool achievement_view_visible = false;
bool achievement_view_detail = false;
bool achievement_view_just_opened = false;
int achievement_view_filter = 0; // 0 all, 1 locked, 2 unlocked
size_t achievement_view_selected = 0;
struct BadgeImage {
    std::vector<uint16_t> pixels;
    int width = 0;
    int height = 0;
    bool loading = false;
    bool failed = false;
};
struct BadgeDownload {
    std::string url;
    std::string cache_path;
    std::vector<unsigned char> data;
    bool success = false;
};
std::unordered_map<std::string, BadgeImage> badge_cache;
std::mutex badge_mutex;
std::vector<std::unique_ptr<BadgeDownload>> badge_completed;
void request_badge(const std::string& url);
std::vector<retro_memory_descriptor> descriptors;
retro_memory_map memory_map = {};
struct HttpResult {
    rc_client_server_callback_t callback;
    void* callback_data;
    std::string body;
    int status;
};
std::mutex http_mutex;
std::vector<std::unique_ptr<HttpResult>> http_completed;
std::vector<std::thread> http_workers;

void RC_CCONV core_memory_info(uint32_t id, rc_libretro_core_memory_info_t* info);

bool config_bool(const char* key, bool fallback = false) {
    const auto it = conf_map.find(key);
    if (it == conf_map.end()) return fallback;
    return it->second == "true" || it->second == "enabled" || it->second == "1";
}

std::string config_string(const char* key) {
    const auto it = conf_map.find(key);
    return it == conf_map.end() ? std::string() : it->second;
}

void notify(const std::string& text, const std::string& badge_url = {}) {
    if (notification_text == text ||
        std::any_of(notification_queue.begin(), notification_queue.end(),
                    [&](const PendingNotification& item) { return item.text == text; })) {
        logger.log(Logger::DEB, "RetroAchievements: duplicate notification suppressed: %s",
                   text.c_str());
        return;
    }
    if (notification_text.empty()) {
        notification_text = text;
        notification_badge_url = badge_url;
        notification_until = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    } else {
        notification_queue.push_back({text, badge_url});
    }
    if (!badge_url.empty()) request_badge(badge_url);
    logger.log(Logger::INF, "RetroAchievements: %s", text.c_str());
}

void update_notification() {
    if (!notification_text.empty() &&
        std::chrono::steady_clock::now() >= notification_until) {
        notification_text.clear();
        notification_badge_url.clear();
        if (!notification_queue.empty()) {
            notification_text = notification_queue.front().text;
            notification_badge_url = notification_queue.front().badge_url;
            notification_queue.pop_front();
            notification_until = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        }
    }
}

size_t curl_write(char* data, size_t size, size_t count, void* userdata) {
    static_cast<std::string*>(userdata)->append(data, size * count);
    return size * count;
}

void RC_CCONV server_call(const rc_api_request_t* request,
                          rc_client_server_callback_t callback,
                          void* callback_data, rc_client_t*) {
    const std::string url = request->url ? request->url : "";
    const std::string post = request->post_data ? request->post_data : "";
    const std::string content_type = request->content_type ? request->content_type : "";
    http_workers.emplace_back([url, post, content_type, callback, callback_data]() {
        auto result = std::make_unique<HttpResult>();
        result->callback = callback;
        result->callback_data = callback_data;
        result->status = RC_API_SERVER_RESPONSE_CLIENT_ERROR;
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::lock_guard<std::mutex> lock(http_mutex);
            http_completed.push_back(std::move(result));
            return;
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "RetroRun/3.0 rcheevos");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result->body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        CURLcode curl_result;
        if (!post.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.c_str());
            if (!content_type.empty()) {
                const std::string header_value = std::string("Content-Type: ") + content_type;
                curl_slist* headers = curl_slist_append(nullptr, header_value.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_result = curl_easy_perform(curl);
                curl_slist_free_all(headers);
            } else {
                curl_result = curl_easy_perform(curl);
            }
        } else {
            curl_result = curl_easy_perform(curl);
        }
        if (curl_result == CURLE_OK) {
            long status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
            result->status = static_cast<int>(status);
        }
        curl_easy_cleanup(curl);
        std::lock_guard<std::mutex> lock(http_mutex);
        http_completed.push_back(std::move(result));
    });
}

void pump_http() {
    std::vector<std::unique_ptr<HttpResult>> completed;
    {
        std::lock_guard<std::mutex> lock(http_mutex);
        completed.swap(http_completed);
    }
    for (const auto& result : completed) {
        const rc_api_server_response_t response = {
            result->body.c_str(), result->body.size(), result->status};
        result->callback(&response, result->callback_data);
    }
}

bool decode_badge_png(const std::vector<unsigned char>& data, BadgeImage& badge) {
    if (data.empty()) return false;
    png_image image = {};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, data.data(), data.size())) return false;
    image.format = PNG_FORMAT_RGBA;
    std::vector<unsigned char> rgba(PNG_IMAGE_SIZE(image));
    if (!png_image_finish_read(&image, nullptr, rgba.data(), 0, nullptr)) {
        png_image_free(&image);
        return false;
    }
    badge.width = static_cast<int>(image.width);
    badge.height = static_cast<int>(image.height);
    badge.pixels.resize(static_cast<size_t>(badge.width) * badge.height);
    for (size_t i = 0; i < badge.pixels.size(); ++i) {
        const unsigned char r = rgba[i * 4];
        const unsigned char g = rgba[i * 4 + 1];
        const unsigned char b = rgba[i * 4 + 2];
        badge.pixels[i] = static_cast<uint16_t>(((r & 0xf8) << 8) |
                                                ((g & 0xfc) << 3) | (b >> 3));
    }
    png_image_free(&image);
    return true;
}

void pump_badges() {
    std::vector<std::unique_ptr<BadgeDownload>> completed;
    {
        std::lock_guard<std::mutex> lock(badge_mutex);
        completed.swap(badge_completed);
    }
    for (const auto& result : completed) {
        BadgeImage& badge = badge_cache[result->url];
        badge.loading = false;
        badge.failed = !result->success || !decode_badge_png(result->data, badge);
        if (!badge.failed && !result->cache_path.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(
                std::filesystem::path(result->cache_path).parent_path(), ec);
            std::ofstream output(result->cache_path, std::ios::binary);
            if (output.good())
                output.write(reinterpret_cast<const char*>(result->data.data()),
                             static_cast<std::streamsize>(result->data.size()));
        }
    }
}

std::string badge_cache_path(const std::string& url) {
    const size_t slash = url.find_last_of('/');
    std::string name = slash == std::string::npos ? url : url.substr(slash + 1);
    const size_t query = name.find('?');
    if (query != std::string::npos) name.resize(query);
    if (name.empty()) return {};
    return (std::filesystem::path(opt_savedir ? opt_savedir : ".") /
            "retroachievements" / "badges" / name).string();
}

void request_badge(const std::string& url) {
    if (url.empty()) return;
    BadgeImage& badge = badge_cache[url];
    if (badge.loading || badge.failed || !badge.pixels.empty()) return;
    const std::string cache_path = badge_cache_path(url);
    if (!cache_path.empty()) {
        std::ifstream input(cache_path, std::ios::binary);
        if (input.good()) {
            std::vector<unsigned char> data((std::istreambuf_iterator<char>(input)),
                                            std::istreambuf_iterator<char>());
            if (decode_badge_png(data, badge)) return;
        }
    }
    badge.loading = true;
    http_workers.emplace_back([url, cache_path]() {
        auto result = std::make_unique<BadgeDownload>();
        result->url = url;
        result->cache_path = cache_path;
        CURL* curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "RetroRun/3.0 rcheevos");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                +[](char* data, size_t size, size_t count, void* userdata) -> size_t {
                    auto* bytes = static_cast<std::vector<unsigned char>*>(userdata);
                    const size_t total = size * count;
                    bytes->insert(bytes->end(), data, data + total);
                    return total;
                });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result->data);
            result->success = curl_easy_perform(curl) == CURLE_OK;
            curl_easy_cleanup(curl);
        }
        std::lock_guard<std::mutex> lock(badge_mutex);
        badge_completed.push_back(std::move(result));
    });
}

bool refresh_memory() {
    if (!client) return false;
    const rc_client_game_t* game = rc_client_get_game_info(client);
    if (!game) return false;
    const retro_memory_map* map = descriptors.empty() ? nullptr : &memory_map;
    return rc_libretro_memory_init(&memory, map, core_memory_info, game->console_id) != 0;
}

void RC_CCONV core_memory_info(uint32_t id, rc_libretro_core_memory_info_t* info) {
    info->data = static_cast<uint8_t*>(g_retro.retro_get_memory_data(id));
    info->size = g_retro.retro_get_memory_size(id);
}

uint32_t RC_CCONV read_memory(uint32_t address, uint8_t* buffer,
                              uint32_t bytes, rc_client_t*) {
    if (memory.count == 0) {
        // rc_client validates achievement addresses while the game-load
        // callback is still in progress. Initialize as soon as the identified
        // game's console is known, rather than waiting for game_loaded().
        if (!refresh_memory()) {
            // Some cores expose RAM only after their first retro_run(). Do not
            // disable achievements during validation: provide temporary zeroed
            // memory and retry initialization once loading has completed.
            if (!client || !rc_client_is_game_loaded(client)) {
                std::memset(buffer, 0, bytes);
                return bytes;
            }
            ++memory_read_failures;
            return 0;
        }
    }
    const uint32_t read = rc_libretro_memory_read(&memory, address, buffer, bytes);
    if (read != bytes) ++memory_read_failures;
    return read;
}

void RC_CCONV client_log(const char* message, const rc_client_t*) {
    logger.log(Logger::DEB, "rcheevos: %s", message ? message : "");
}

void RC_CCONV memory_log(const char* message) {
    logger.log(Logger::DEB, "rcheevos memory: %s", message ? message : "");
}

void RC_CCONV event_handler(const rc_client_event_t* event, rc_client_t*) {
    switch (event->type) {
    case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
        if (event->achievement)
            notify(event->achievement->title,
                   event->achievement->badge_url ? event->achievement->badge_url : "");
        break;
    case RC_CLIENT_EVENT_GAME_COMPLETED:
        notify("All achievements completed!");
        break;
    case RC_CLIENT_EVENT_DISCONNECTED:
        notify("RetroAchievements disconnected");
        break;
    case RC_CLIENT_EVENT_RECONNECTED:
        notify("RetroAchievements reconnected");
        break;
    case RC_CLIENT_EVENT_SERVER_ERROR:
        if (event->server_error && event->server_error->error_message)
            notify(std::string("RetroAchievements error: ") + event->server_error->error_message);
        break;
    case RC_CLIENT_EVENT_RESET:
        g_retro.retro_reset();
        break;
    default:
        break;
    }
}

void RC_CCONV game_loaded(int result, const char* error, rc_client_t* active_client, void*) {
    if (result != RC_OK) {
        game_load_error = error ? error : rc_error_str(result);
        notify(std::string("Achievements unavailable: ") + game_load_error);
        return;
    }
    game_load_error.clear();
    const rc_client_game_t* game = rc_client_get_game_info(active_client);
    if (!game) return;
    if (!refresh_memory()) {
        notify("Achievements: core memory is unavailable");
        return;
    }
    logger.log(Logger::INF,
               "RetroAchievements runtime: console=%u, memory_regions=%u, memory_bytes=%zu, processing=%s",
               game->console_id, memory.count, memory.total_size,
               rc_client_is_processing_required(active_client) ? "yes" : "no");
    rc_client_user_game_summary_t summary = {};
    rc_client_get_user_game_summary(active_client, &summary);
    notify(std::string("Achievements: ") + game->title + " (" +
           std::to_string(summary.num_unlocked_achievements) + "/" +
           std::to_string(summary.num_core_achievements) + ")");
}

void identify_game() {
    const uint8_t* data = content_data.empty() ? nullptr : content_data.data();
    logger.log(Logger::INF,
               "RetroAchievements identify: path='%s', source=%s, bytes=%zu",
               content.c_str(), data ? "memory" : "file", content_data.size());
    game_load_error.clear();
    rc_client_begin_identify_and_load_game(client, 0, content.c_str(), data, content_data.size(),
                                           game_loaded, nullptr);
}

void RC_CCONV login_complete(int result, const char* error, rc_client_t*, void*) {
    login_pending = false;
    if (result != RC_OK) {
        login_error = error ? error : rc_error_str(result);
        notify(std::string("RetroAchievements login failed: ") +
               login_error);
        return;
    }
    login_error.clear();
    if (used_password_login) {
        const rc_client_user_t* user = rc_client_get_user_info(client);
        if (user && user->token && *user->token) {
            if (persistVideoSetting("retrorun_achievements_token", user->token)) {
                persistVideoSetting("retrorun_achievements_password", "");
                logger.log(Logger::INF,
                           "RetroAchievements: login token saved; plaintext password removed");
            } else {
                logger.log(Logger::WARN,
                           "RetroAchievements: could not save the login token");
            }
        }
    }
    identify_game();
}

void RC_CCONV media_changed(int result, const char* error, rc_client_t*, void*) {
    if (result != RC_OK)
        logger.log(Logger::WARN, "RetroAchievements media change failed: %s",
                   error ? error : rc_error_str(result));
}
}

void achievements_init(const char* content_path) {
    achievements_shutdown();
    enabled = config_bool("retrorun_achievements_enabled");
    if (!enabled || !content_path || !*content_path) return;
    logger.log(Logger::INF,
               "RetroAchievements integration: buffered-hash-v2; requested content='%s'",
               content_path);
    const std::string username = config_string("retrorun_achievements_username");
    const std::string token = config_string("retrorun_achievements_token");
    const std::string password = config_string("retrorun_achievements_password");
    if (username.empty() || (token.empty() && password.empty())) {
        notify("RetroAchievements credentials are missing");
        enabled = false;
        return;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::error_code path_error;
    const std::filesystem::path absolute_content =
        std::filesystem::absolute(std::filesystem::path(content_path), path_error);
    content = path_error ? std::string(content_path)
                         : absolute_content.lexically_normal().string();
    logger.log(Logger::INF, "RetroAchievements resolved content: '%s'", content.c_str());
    if (!std::filesystem::is_regular_file(content, path_error)) {
        notify(std::string("Achievements content path is invalid: ") + content);
        enabled = false;
        return;
    }
    // Hash cartridge-sized content from memory. This avoids platform-specific
    // stdio/file-reader behavior in rcheevos and matches cores that receive a
    // buffered retro_game_info. Disc descriptors must remain file-backed even
    // when tiny: rcheevos has to follow their references to track files.
    constexpr uintmax_t maximum_buffered_content = 64U * 1024U * 1024U;
    const uintmax_t content_size = std::filesystem::file_size(content, path_error);
    std::string extension = std::filesystem::path(content).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const bool file_backed_content = extension == ".cue" || extension == ".gdi" ||
                                     extension == ".m3u" || extension == ".ccd" ||
                                     extension == ".toc" || extension == ".chd" ||
                                     extension == ".iso" || extension == ".pbp";
    if (!path_error && !file_backed_content && content_size > 0 &&
        content_size <= maximum_buffered_content) {
        std::ifstream input(content, std::ios::binary);
        if (input.good()) {
            content_data.assign(std::istreambuf_iterator<char>(input),
                                std::istreambuf_iterator<char>());
            logger.log(Logger::DEB,
                       "RetroAchievements: hashing %zu bytes from memory",
                       content_data.size());
        }
        if (content_data.size() != content_size) {
            notify("Achievements disabled: could not read the complete game file");
            logger.log(Logger::ERR,
                       "RetroAchievements buffered read failed: expected=%llu, read=%zu, path='%s'",
                       static_cast<unsigned long long>(content_size), content_data.size(),
                       content.c_str());
            enabled = false;
            return;
        }
    } else if (path_error) {
        notify("Achievements disabled: could not determine the game file size");
        logger.log(Logger::ERR, "RetroAchievements file size failed for '%s': %s",
                   content.c_str(), path_error.message().c_str());
        enabled = false;
        return;
    } else if (file_backed_content) {
        logger.log(Logger::INF,
                   "RetroAchievements: optical content '%s', using file-backed hashing",
                   extension.c_str());
    } else {
        logger.log(Logger::INF,
                   "RetroAchievements: large content (%llu bytes), using file-backed hashing",
                   static_cast<unsigned long long>(content_size));
    }
    client = rc_client_create(read_memory, server_call);
    if (!client) { enabled = false; notify("Cannot initialize RetroAchievements"); return; }
    rc_client_set_event_handler(client, event_handler);
    rc_client_enable_logging(client, RC_CLIENT_LOG_LEVEL_INFO, client_log);
    rc_libretro_init_verbose_message_callback(memory_log);
    // RetroRun currently exposes softcore mode only. Hardcore requires
    // enforcing additional restrictions on save states and fast-forward.
    rc_client_set_hardcore_enabled(client, 0);
    rc_client_set_encore_mode_enabled(client,
        config_bool("retrorun_achievements_encore") ? 1 : 0);
    rc_client_set_unofficial_enabled(client,
        config_bool("retrorun_achievements_unofficial") ? 1 : 0);
    if (!token.empty())
    {
        login_pending = true;
        rc_client_begin_login_with_token(client, username.c_str(), token.c_str(), login_complete, nullptr);
    }
    else {
        used_password_login = true;
        login_pending = true;
        rc_client_begin_login_with_password(client, username.c_str(), password.c_str(), login_complete, nullptr);
    }
}

void achievements_frame() {
    pump_http();
    pump_badges();
    if (client && rc_client_is_game_loaded(client)) {
        rc_client_do_frame(client);
        ++frames_processed;
        // Some cores allocate or replace their RAM shortly after the first
        // retro_run(). Refresh the pointers once the emulation is underway.
        if (frames_processed == 60) refresh_memory();
        if (frames_processed == 600 || (frames_processed > 0 && frames_processed % 18000 == 0))
            logger.log(Logger::DEB,
                       "RetroAchievements runtime health: frames=%llu, memory_read_failures=%llu",
                       static_cast<unsigned long long>(frames_processed),
                       static_cast<unsigned long long>(memory_read_failures));
    }
    update_notification();
}
void achievements_idle() { pump_http(); pump_badges(); if (client) rc_client_idle(client); update_notification(); }
void achievements_reset() { if (client) { rc_client_reset(client); refresh_memory(); } }
bool achievements_active() { return client && rc_client_is_game_loaded(client); }
bool achievements_enabled() { return config_bool("retrorun_achievements_enabled"); }

std::string achievements_status_label() {
    if (!achievements_enabled()) return "Status: Disabled";
    if (!login_error.empty()) return "Status: Login failed";
    if (client) {
        const rc_client_user_t* user = rc_client_get_user_info(client);
        if (user && user->username)
            return std::string("Status: Logged in as ") + user->username;
        if (login_pending) return "Status: Signing in...";
    }
    const std::string username = config_string("retrorun_achievements_username");
    const std::string token = config_string("retrorun_achievements_token");
    const std::string password = config_string("retrorun_achievements_password");
    if (!username.empty() && (!token.empty() || !password.empty()))
        return "Status: Configured";
    return "Status: Credentials required";
}

std::string achievements_username_label() {
    const std::string username = config_string("retrorun_achievements_username");
    return username.empty() ? "Username: Not set" : "Username: " + username;
}

static void save_credential(const char* key, const std::string& value) {
    conf_map[key] = value;
    if (!persistVideoSetting(key, value))
        logger.log(Logger::WARN, "Could not save RetroAchievements credential setting");
}

void achievements_edit_username(const char* content_path) {
    const std::string current = config_string("retrorun_achievements_username");
    const std::string path = content_path ? content_path : "";
    rr_keyboard_text_open("RETROACHIEVEMENTS USERNAME", current, false,
        [path, current](const std::string& value) {
            if (value.empty() || value == current) return;
            save_credential("retrorun_achievements_username", value);
            // Login tokens belong to a specific account. Require the password
            // when the username changes instead of trying an unrelated token.
            save_credential("retrorun_achievements_token", "");
            achievements_init(path.c_str());
        });
}

void achievements_edit_password(const char* content_path) {
    const std::string path = content_path ? content_path : "";
    rr_keyboard_text_open("RETROACHIEVEMENTS PASSWORD", "", true,
        [path](const std::string& value) {
            if (value.empty()) return;
            save_credential("retrorun_achievements_password", value);
            save_credential("retrorun_achievements_token", "");
            achievements_init(path.c_str());
        });
}

void achievements_set_enabled(bool value, const char* content_path) {
    conf_map["retrorun_achievements_enabled"] = value ? "true" : "false";
    if (!persistVideoSetting("retrorun_achievements_enabled", value ? "true" : "false"))
        logger.log(Logger::WARN, "Could not save the RetroAchievements enabled setting");
    if (value) {
        logger.log(Logger::INF, "RetroAchievements enabled from menu");
        achievements_init(content_path);
    } else {
        logger.log(Logger::INF, "RetroAchievements disabled from menu");
        achievements_shutdown();
    }
}
bool achievements_notification_visible() { return !notification_text.empty(); }

std::vector<size_t> filtered_achievement_indices() {
    std::vector<size_t> indices;
    for (size_t i = 0; i < achievement_entries.size(); ++i) {
        const bool unlocked = achievement_entries[i].unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
        if (achievement_view_filter == 0 ||
            (achievement_view_filter == 1 && !unlocked) ||
            (achievement_view_filter == 2 && unlocked))
            indices.push_back(i);
    }
    return indices;
}

void achievements_view_open() {
    if (!client || !rc_client_is_game_loaded(client)) {
        const int state = client ? rc_client_get_load_game_state(client)
                                 : RC_CLIENT_LOAD_GAME_STATE_NONE;
        logger.log(Logger::INF,
                   "RetroAchievements view: no set loaded, load_state=%d, error='%s'",
                   state, game_load_error.c_str());
        achievement_entries.clear();
        achievement_view_selected = 0;
        achievement_view_filter = 0;
        achievement_view_detail = false;
        achievement_view_visible = true;
        achievement_view_just_opened = true;
        return;
    }
    achievement_entries.clear();
    rc_client_achievement_list_t* list = rc_client_create_achievement_list(
        client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (!list) return;
    for (uint32_t bucket = 0; bucket < list->num_buckets; ++bucket) {
        const rc_client_achievement_bucket_t& source = list->buckets[bucket];
        for (uint32_t i = 0; i < source.num_achievements; ++i) {
            const rc_client_achievement_t* item = source.achievements[i];
            achievement_entries.push_back({item->title ? item->title : "",
                                           item->description ? item->description : "",
                                           item->points, item->state, item->unlocked,
                                           item->unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE
                                               ? (item->badge_url ? item->badge_url : "")
                                               : (item->badge_locked_url ? item->badge_locked_url : "")});
        }
    }
    rc_client_destroy_achievement_list(list);
    achievement_view_selected = 0;
    achievement_view_filter = 0;
    achievement_view_detail = false;
    achievement_view_visible = true;
    achievement_view_just_opened = true;
}

bool achievements_view_visible() { return achievement_view_visible; }
void achievements_view_close() { achievement_view_visible = false; achievement_view_detail = false; }

void achievements_view_input(bool up, bool down, bool left, bool right,
                             bool accept, bool cancel) {
    if (!achievement_view_visible) return;
    if (achievement_view_just_opened) { achievement_view_just_opened = false; return; }
    if (cancel) {
        if (achievement_view_detail) achievement_view_detail = false;
        else achievements_view_close();
        return;
    }
    if (achievement_view_detail) return;
    if (left) achievement_view_filter = (achievement_view_filter + 2) % 3;
    if (right) achievement_view_filter = (achievement_view_filter + 1) % 3;
    std::vector<size_t> indices = filtered_achievement_indices();
    if (indices.empty()) { achievement_view_selected = 0; return; }
    achievement_view_selected = std::min(achievement_view_selected, indices.size() - 1);
    if (up) achievement_view_selected = (achievement_view_selected + indices.size() - 1) % indices.size();
    if (down) achievement_view_selected = (achievement_view_selected + 1) % indices.size();
    if (accept) {
        achievement_view_detail = true;
        request_badge(achievement_entries[indices[achievement_view_selected]].badge_url);
    }
}

static void draw_wrapped(uint16_t* pixels, int stride, int width, int height,
                         int x, int& y, const std::string& text, uint16_t color,
                         int max_lines) {
    const size_t max_chars = static_cast<size_t>(std::max(1, (width - x - 8) / 8));
    size_t start = 0;
    for (int line_number = 0; start < text.size() && line_number < max_lines; ++line_number) {
        size_t length = std::min(max_chars, text.size() - start);
        if (start + length < text.size()) {
            const size_t space = text.rfind(' ', start + length);
            if (space != std::string::npos && space > start) length = space - start;
        }
        const std::string line = text.substr(start, length);
        basic_text_out16_nf_color_clipped(pixels, stride, width, height, x, y, line.c_str(), color);
        start += length;
        while (start < text.size() && text[start] == ' ') ++start;
        y += 10;
    }
}

void achievements_view_render(rr_surface_t* surface, int width, int height) {
    if (!surface) return;
    uint16_t* pixels = static_cast<uint16_t*>(rr_surface_map(surface));
    if (!pixels) return;
    const int stride = rr_surface_stride_get(surface) / 2;
    std::fill(pixels, pixels + stride * height, static_cast<uint16_t>(0x0841));
    const rc_client_game_t* game = client ? rc_client_get_game_info(client) : nullptr;
    if (!client || !rc_client_is_game_loaded(client)) {
        basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, 7,
                                         "ACHIEVEMENTS", 0xffe0);
        int y = 29;
        draw_wrapped(pixels, stride, width, height, 8, y,
                     "No achievement set is loaded.", 0xffff, 2);
        static const char* states[] = {"Not started", "Waiting for login", "Identifying game",
                                      "Fetching game data", "Starting session", "Loaded", "Aborted"};
        const int state = client ? rc_client_get_load_game_state(client)
                                 : RC_CLIENT_LOAD_GAME_STATE_NONE;
        const std::string state_text = std::string("State: ") +
            ((state >= 0 && state <= RC_CLIENT_LOAD_GAME_STATE_ABORTED) ? states[state] : "Unknown");
        draw_wrapped(pixels, stride, width, height, 8, y, state_text, 0xbdf7, 2);
        if (!game_load_error.empty())
            draw_wrapped(pixels, stride, width, height, 8, y,
                         "Error: " + game_load_error, 0xf800,
                         std::max(1, (height - y - 24) / 10));
        else
            draw_wrapped(pixels, stride, width, height, 8, y,
                         "Check the startup log for identification details.", 0x7bef, 3);
        basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, height - 13,
                                         "B Back", 0xffff);
        rr_surface_unmap(surface);
        return;
    }
    std::string game_title = game && game->title ? game->title : "ACHIEVEMENTS";
    unsigned unlocked_count = 0;
    for (const AchievementEntry& item : achievement_entries)
        if (item.unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE) ++unlocked_count;
    const std::string count = std::to_string(unlocked_count) + "/" +
                              std::to_string(achievement_entries.size());
    const size_t title_chars = static_cast<size_t>(std::max(
        1, (width - static_cast<int>(count.size()) * 8 - 32) / 8));
    if (game_title.size() > title_chars) game_title.resize(title_chars);
    basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, 7,
                                     game_title.c_str(), 0xffe0);
    basic_text_out16_nf_color_clipped(pixels, stride, width, height,
                                     std::max(8, width - static_cast<int>(count.size()) * 8 - 8),
                                     7, count.c_str(), 0xffff);

    std::vector<size_t> indices = filtered_achievement_indices();
    if (achievement_view_detail && !indices.empty()) {
        const AchievementEntry& item = achievement_entries[indices[achievement_view_selected]];
        request_badge(item.badge_url);
        int y = 31;
        draw_wrapped(pixels, stride, width, height, 66, y, item.title, 0xffff, 3);
        const auto badge_it = badge_cache.find(item.badge_url);
        if (badge_it != badge_cache.end() && !badge_it->second.pixels.empty()) {
            const BadgeImage& badge = badge_it->second;
            constexpr int badge_size = 48;
            for (int dy = 0; dy < badge_size && 27 + dy < height; ++dy) {
                const int sy = dy * badge.height / badge_size;
                for (int dx = 0; dx < badge_size && 8 + dx < width; ++dx) {
                    const int sx = dx * badge.width / badge_size;
                    pixels[(27 + dy) * stride + 8 + dx] =
                        badge.pixels[static_cast<size_t>(sy) * badge.width + sx];
                }
            }
        } else {
            basic_text_out16_nf_color_clipped(pixels, stride, width, height,
                                             8, 45, "[badge]", 0x7bef);
        }
        // The title is placed beside the 48x48 badge. Start the description
        // below both elements so long or short titles never overlap it.
        y = std::max(y + 6, 85);
        draw_wrapped(pixels, stride, width, height, 8, y, item.description, 0xbdf7,
                     std::max(1, (height - y - 35) / 10));
        const bool unlocked = item.unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
        const std::string footer = std::string("Points: ") + std::to_string(item.points) +
                                   "  Status: " + (unlocked ? "Unlocked" : "Locked");
        basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, height - 25,
                                         footer.c_str(), unlocked ? 0x07e0 : 0x7bef);
        basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, height - 13,
                                         "B Back", 0xffff);
    } else {
        static const char* filters[] = {"ALL", "LOCKED", "UNLOCKED"};
        const std::string filter_line = std::string("< ") + filters[achievement_view_filter] + " >";
        basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, 21,
                                         filter_line.c_str(), 0xbdf7);
        const int visible_rows = std::max(1, (height - 55) / 11);
        const int first = achievement_view_selected >= static_cast<size_t>(visible_rows)
            ? static_cast<int>(achievement_view_selected) - visible_rows + 1 : 0;
        for (int row = 0; row < visible_rows && first + row < static_cast<int>(indices.size()); ++row) {
            const size_t filtered_index = static_cast<size_t>(first + row);
            const AchievementEntry& item = achievement_entries[indices[filtered_index]];
            const bool unlocked = item.unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
            const bool unsupported = item.state == RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
            std::string label = unsupported ? "! " : (unlocked ? "+ " : "- ");
            label += item.title;
            const size_t max_chars = static_cast<size_t>(std::max(1, (width - 16) / 8));
            if (label.size() > max_chars) label.resize(max_chars);
            const uint16_t color = filtered_index == achievement_view_selected ? 0xffe0
                                   : unlocked ? 0x07e0 : unsupported ? 0xf800 : 0xffff;
            basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8,
                                             35 + row * 11, label.c_str(), color);
        }
        basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, height - 13,
                                         "D-PAD Move/Filter  A Info  B Back", 0x7bef);
    }
    rr_surface_unmap(surface);
}

void achievements_render_notification(rr_surface_t* surface) {
    if (!surface || notification_text.empty()) return;
    uint16_t* pixels = static_cast<uint16_t*>(rr_surface_map(surface));
    if (!pixels) return;
    const int width = rr_surface_width_get(surface);
    const int height = rr_surface_height_get(surface);
    const int stride = rr_surface_stride_get(surface) / 2;
    std::fill(pixels, pixels + stride * height, static_cast<uint16_t>(0x0841));
    const bool has_badge = !notification_badge_url.empty();
    const int text_x = has_badge ? 46 : 6;
    if (has_badge) {
        request_badge(notification_badge_url);
        const auto badge_it = badge_cache.find(notification_badge_url);
        if (badge_it != badge_cache.end() && !badge_it->second.pixels.empty()) {
            const BadgeImage& badge = badge_it->second;
            constexpr int badge_size = 32;
            for (int dy = 0; dy < badge_size && 10 + dy < height; ++dy) {
                const int sy = dy * badge.height / badge_size;
                for (int dx = 0; dx < badge_size && 6 + dx < width; ++dx) {
                    const int sx = dx * badge.width / badge_size;
                    pixels[(10 + dy) * stride + 6 + dx] =
                        badge.pixels[static_cast<size_t>(sy) * badge.width + sx];
                }
            }
        }
    }
    basic_text_out16_nf_color_clipped(pixels, stride, width, height,
                                     text_x, 5, "ACHIEVEMENT", 0xffe0);
    // basic_text_out16 uses 8-pixel-wide glyph cells.
    const size_t max_chars = static_cast<size_t>(std::max(1, (width - text_x - 6) / 8));
    size_t start = 0;
    int y = 17;
    while (start < notification_text.size() && y + 8 <= height) {
        size_t length = std::min(max_chars, notification_text.size() - start);
        if (start + length < notification_text.size()) {
            const size_t space = notification_text.rfind(' ', start + length);
            if (space != std::string::npos && space > start) length = space - start;
        }
        const std::string line = notification_text.substr(start, length);
        basic_text_out16_nf_color_clipped(pixels, stride, width, height,
                                         text_x, y, line.c_str(), 0xffff);
        start += length;
        while (start < notification_text.size() && notification_text[start] == ' ') ++start;
        y += 10;
    }
    rr_surface_unmap(surface);
}

void achievements_set_memory_map(const retro_memory_map* map) {
    descriptors.clear(); memory_map = {};
    if (!map || !map->descriptors || !map->num_descriptors) return;
    descriptors.assign(map->descriptors, map->descriptors + map->num_descriptors);
    memory_map.descriptors = descriptors.data();
    memory_map.num_descriptors = static_cast<unsigned>(descriptors.size());
}

void achievements_change_media(const char* path) {
    if (client && path && *path)
        rc_client_begin_identify_and_change_media(client, path, nullptr, 0,
                                                  media_changed, nullptr);
}

void achievements_shutdown() {
    for (std::thread& worker : http_workers) if (worker.joinable()) worker.join();
    http_workers.clear();
    {
        std::lock_guard<std::mutex> lock(http_mutex);
        http_completed.clear();
    }
    if (client) { rc_client_destroy(client); client = nullptr; }
    rc_libretro_memory_destroy(&memory);
    content.clear(); enabled = false;
    content_data.clear();
    used_password_login = false;
    login_pending = false;
    login_error.clear();
    game_load_error.clear();
    frames_processed = memory_read_failures = 0;
    notification_text.clear();
    notification_badge_url.clear();
    notification_queue.clear();
    badge_cache.clear();
    {
        std::lock_guard<std::mutex> lock(badge_mutex);
        badge_completed.clear();
    }
    curl_global_cleanup();
}
