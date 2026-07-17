#include "decoration.h"

#include "config.h"
#include "globals.h"
#include "video.h"
#include "video-helper.h"

#include <png.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace {
bool enabled = false;
bool load_attempted = false;
bool pixel_perfect_before_enable = false;
std::string content;
std::string loaded_path;
rr_surface_t* cached_surface = nullptr;
rr_surface_t* cached_background = nullptr;

struct DecorationLayout {
    bool valid = false;
    double width = 0.0;
    double height = 0.0;
    double top = 0.0;
    double left = 0.0;
    double bottom = 0.0;
    double right = 0.0;
};

DecorationLayout layout;
int viewport_display_width = 0;
int viewport_display_height = 0;
int viewport_x = 0;
int viewport_y = 0;
int viewport_width = 0;
int viewport_height = 0;

void decoration_canvas_size(int* width, int* height) {
    *width = rr_display_width_get(display);
    *height = rr_display_height_get(display);
#ifndef RR_PLATFORM_SDL
    const rr_rotation_t rotation = getRotation();
    if (rotation == RR_ROTATION_DEGREES_90 ||
        rotation == RR_ROTATION_DEGREES_270)
        std::swap(*width, *height);
#endif
}

bool value_true(const std::string& value) {
    return value == "true" || value == "enabled" || value == "1" || value == "auto";
}

std::filesystem::path decoration_root() {
    const auto configured = conf_map.find("retrorun_decorations_path");
    if (configured != conf_map.end() && !configured->second.empty())
        return configured->second;
    if (!activeConfigFile.empty()) {
        const std::filesystem::path config(activeConfigFile);
        if (config.has_parent_path()) return config.parent_path() / "decorations";
    }
    return "decorations";
}

std::string clean_stem(const std::filesystem::path& path) {
    return path.stem().string();
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char c) { return std::isspace(c); });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char c) { return std::isspace(c); }).base();
    if (first >= last) return {};
    value = std::string(first, last);
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\'')))
        value = value.substr(1, value.size() - 2);
    return value;
}

std::string system_from_rom(const std::filesystem::path& rom) {
    std::string previous;
    for (const auto& component : rom.parent_path()) {
        const std::string current = component.string();
        if (previous == "roms" || previous == "roms2") return current;
        previous = current;
    }
    return rom.has_parent_path() ? rom.parent_path().filename().string() : "";
}

std::map<std::string, std::string> read_settings(const std::filesystem::path& path) {
    std::map<std::string, std::string> settings;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        const size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = trim(line.substr(0, equals));
        if (!key.empty()) settings[key] = trim(line.substr(equals + 1));
    }
    return settings;
}

std::string setting_value(const std::map<std::string, std::string>& settings,
                          const std::string& system, const std::string& rom_name,
                          const std::string& name) {
    const std::vector<std::string> keys = {
        system + "[\"" + rom_name + "\"]." + name,
        system + "." + name,
        "global." + name
    };
    for (const auto& key : keys) {
        const auto found = settings.find(key);
        if (found != settings.end() && !found->second.empty()) return found->second;
    }
    return {};
}

struct DecorationCandidate {
    std::filesystem::path path;
    bool distribution = false;
};

void append_amberelec_game_candidate(std::vector<DecorationCandidate>& result,
                                     const std::filesystem::path& games,
                                     const std::string& name) {
    const std::filesystem::path config = games / (name + ".cfg");
    std::ifstream file(config);
    std::string image;
    if (file && std::getline(file, image)) {
        image = trim(image);
        if (!image.empty()) result.push_back({games / image, true});
    }

    // AmberELEC also accepts numbered alternatives (name.1.cfg, name.2.cfg...).
    // Pick the first one deterministically; random changes between launches are
    // surprising in a small frontend and add no compatibility benefit.
    std::error_code error;
    std::vector<std::filesystem::path> numbered;
    if (std::filesystem::is_directory(games, error)) {
        const std::string prefix = name + ".";
        for (const auto& entry : std::filesystem::directory_iterator(games, error)) {
            const std::string filename = entry.path().filename().string();
            if (filename.rfind(prefix, 0) == 0 && entry.path().extension() == ".cfg")
                numbered.push_back(entry.path());
        }
    }
    std::sort(numbered.begin(), numbered.end());
    if (!numbered.empty()) {
        std::ifstream alternative(numbered.front());
        if (alternative && std::getline(alternative, image)) {
            image = trim(image);
            if (!image.empty()) result.push_back({games / image, true});
        }
    }
}

void append_distribution_candidates(std::vector<DecorationCandidate>& result,
                                    const std::filesystem::path& rom,
                                    const std::string& system,
                                    const std::string& requested_source) {
    if (system.empty()) return;
    const std::filesystem::path amber_config(
        "/storage/.config/distribution/configs/distribution.conf");
    const auto settings = read_settings(amber_config);
    std::string pack = setting_value(settings, system, rom.filename().string(), "bezel");
    if (pack.empty()) pack = "default";
    const size_t separator = requested_source.find(':');
    if (separator != std::string::npos && separator + 1 < requested_source.size())
        pack = requested_source.substr(separator + 1);
    std::string bezel_system = setting_value(settings, system, rom.filename().string(),
                                              "bezel.system.override");
    if (bezel_system.empty()) bezel_system = system;

    // AmberELEC uses the first location while preparing a launch and the second
    // for user-installed packs. /roms and /roms2 cover ArkOS/user packs too.
    std::vector<std::filesystem::path> roots;
    if (requested_source.rfind("arkos:", 0) != 0) {
        roots.emplace_back("/tmp/overlays/bezels");
        roots.emplace_back("/storage/roms/bezels");
    }
    if (requested_source.rfind("amberelec:", 0) != 0) {
        roots.emplace_back("/roms/bezels");
        roots.emplace_back("/roms2/bezels");
    }
    for (const auto& root : roots) {
        std::error_code error;
        std::vector<std::filesystem::path> pack_roots = {root / pack};
        // Some ArkOS/user installations put systems directly below bezels
        // instead of introducing AmberELEC's named-pack directory.
        if (std::filesystem::is_directory(root / "systems", error))
            pack_roots.push_back(root);
        for (const auto& pack_root : pack_roots) {
            if (!std::filesystem::is_directory(pack_root, error)) continue;
            const std::filesystem::path games =
                pack_root / "systems" / bezel_system / "games";
            const std::string full_name = clean_stem(rom);
            std::string short_name = full_name;
            const size_t region = short_name.find(" (");
            if (region != std::string::npos) short_name.resize(region);
            append_amberelec_game_candidate(result, games, full_name);
            if (short_name != full_name)
                append_amberelec_game_candidate(result, games, short_name);
            append_amberelec_game_candidate(result, games, "default");
            result.push_back({pack_root / "systems" / (bezel_system + ".png"), true});
        }
    }
}

std::vector<DecorationCandidate> candidates() {
    const std::filesystem::path rom(content);
    const std::string game = clean_stem(rom);
    const std::string system = system_from_rom(rom);
    const std::filesystem::path root = decoration_root();
    std::vector<DecorationCandidate> result;
    const auto source_setting = conf_map.find("retrorun_decoration_source");
    const std::string source = source_setting == conf_map.end()
        ? "auto" : source_setting->second;
    const bool include_retrorun = source == "auto" || source == "retrorun" || source.empty();
    const bool include_distribution = source != "retrorun";
    const auto selected_pack = conf_map.find("retrorun_decoration_pack");
    if (include_retrorun && selected_pack != conf_map.end() && !selected_pack->second.empty())
        result.push_back({root / "downloads" / "libretro" /
                          (selected_pack->second + ".png"), false});
    if (include_retrorun) {
        if (!system.empty()) result.push_back({root / "games" / system / (game + ".png"), false});
        result.push_back({root / "games" / (game + ".png"), false});
        if (!system.empty()) result.push_back({root / "systems" / (system + ".png"), false});
    }
    if (include_distribution)
        append_distribution_candidates(result, rom, system, source);
    if (include_retrorun) result.push_back({root / "default.png", false});
    return result;
}

bool number_field(const std::string& json, const char* name, double* value) {
    const std::string key = std::string("\"") + name + "\"";
    const size_t key_position = json.find(key);
    if (key_position == std::string::npos) return false;
    const size_t colon = json.find(':', key_position + key.length());
    if (colon == std::string::npos) return false;
    const char* start = json.c_str() + colon + 1;
    char* end = nullptr;
    const double parsed = std::strtod(start, &end);
    if (end == start) return false;
    *value = parsed;
    return true;
}

void load_info(const std::filesystem::path& image_path) {
    layout = {};
    viewport_display_width = 0;
    viewport_display_height = 0;
    std::filesystem::path info_path = image_path;
    info_path.replace_extension(".info");
    std::ifstream file(info_path);
    if (!file) return;
    const std::string json((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    DecorationLayout parsed;
    if (!number_field(json, "width", &parsed.width) ||
        !number_field(json, "height", &parsed.height) ||
        !number_field(json, "top", &parsed.top) ||
        !number_field(json, "left", &parsed.left) ||
        !number_field(json, "bottom", &parsed.bottom) ||
        !number_field(json, "right", &parsed.right) ||
        parsed.width <= 0.0 || parsed.height <= 0.0 ||
        parsed.left < 0.0 || parsed.top < 0.0 ||
        parsed.right < 0.0 || parsed.bottom < 0.0 ||
        parsed.left + parsed.right >= parsed.width ||
        parsed.top + parsed.bottom >= parsed.height) {
        logger.log(Logger::WARN, "Screen decoration layout ignored: invalid %s",
                   info_path.string().c_str());
        return;
    }
    parsed.valid = true;
    layout = parsed;
    logger.log(Logger::INF, "Screen decoration layout loaded: %s",
               info_path.string().c_str());
}

void load_distribution_viewport() {
    if (layout.valid) return;
    const auto settings = read_settings("/tmp/raappend.cfg");
    const auto value = [&settings](const char* key, int* output) {
        const auto found = settings.find(key);
        if (found == settings.end()) return false;
        char* end = nullptr;
        const long parsed = std::strtol(found->second.c_str(), &end, 10);
        if (end == found->second.c_str()) return false;
        *output = static_cast<int>(parsed);
        return true;
    };
    int x = 0, y = 0, width = 0, height = 0;
    if (!value("custom_viewport_x", &x) || !value("custom_viewport_y", &y) ||
        !value("custom_viewport_width", &width) ||
        !value("custom_viewport_height", &height)) return;
    int display_width = 0;
    int display_height = 0;
    decoration_canvas_size(&display_width, &display_height);
    if (display_width <= 0 || display_height <= 0 || x < 0 || y < 0 ||
        width <= 0 || height <= 0 || x + width > display_width ||
        y + height > display_height) {
        logger.log(Logger::WARN,
                   "Distribution decoration viewport ignored: x=%d, y=%d, w=%d, h=%d",
                   x, y, width, height);
        return;
    }
    layout.valid = true;
    layout.width = display_width;
    layout.height = display_height;
    layout.left = x;
    layout.top = y;
    layout.right = display_width - x - width;
    layout.bottom = display_height - y - height;
    logger.log(Logger::INF,
               "Distribution decoration viewport loaded from /tmp/raappend.cfg");
}

bool load_png(const std::filesystem::path& path, bool distribution) {
    png_image image = {};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, path.string().c_str())) return false;
    image.format = PNG_FORMAT_RGBA;
    std::vector<unsigned char> rgba(PNG_IMAGE_SIZE(image));
    if (!png_image_finish_read(&image, nullptr, rgba.data(), 0, nullptr)) {
        png_image_free(&image);
        return false;
    }
    int output_width = 0;
    int output_height = 0;
    decoration_canvas_size(&output_width, &output_height);
    if (output_width <= 0 || output_height <= 0) return false;
    rr_surface_t* loaded = rr_surface_create(display, output_width, output_height,
                                              RR_PIXEL_FORMAT_RGBA8888);
    rr_surface_t* background_surface = rr_surface_create(display, output_width, output_height,
                                                          RR_PIXEL_FORMAT_RGB565);
    if (!loaded || !background_surface) {
        if (loaded) rr_surface_destroy(loaded);
        if (background_surface) rr_surface_destroy(background_surface);
        return false;
    }
    auto* pixels = static_cast<uint8_t*>(rr_surface_map(loaded));
    auto* background = static_cast<uint16_t*>(rr_surface_map(background_surface));
    if (!pixels || !background) {
        if (pixels) rr_surface_unmap(loaded);
        if (background) rr_surface_unmap(background_surface);
        rr_surface_destroy(loaded);
        rr_surface_destroy(background_surface);
        return false;
    }
    const int stride = rr_surface_stride_get(loaded);
    const int background_stride = rr_surface_stride_get(background_surface) / sizeof(uint16_t);
    for (int y = 0; y < output_height; ++y) {
        const unsigned source_y = static_cast<unsigned>(
            static_cast<uint64_t>(y) * image.height / output_height);
        for (int x = 0; x < output_width; ++x) {
            const unsigned source_x = static_cast<unsigned>(
                static_cast<uint64_t>(x) * image.width / output_width);
            const size_t offset = (static_cast<size_t>(source_y) * image.width + source_x) * 4;
            const size_t destination = static_cast<size_t>(y) * stride + x * 4;
            pixels[destination] = rgba[offset];
            pixels[destination + 1] = rgba[offset + 1];
            pixels[destination + 2] = rgba[offset + 2];
            pixels[destination + 3] = rgba[offset + 3];
            const uint8_t alpha = rgba[offset + 3];
            const uint8_t r = static_cast<uint8_t>(rgba[offset] * alpha / 255);
            const uint8_t g = static_cast<uint8_t>(rgba[offset + 1] * alpha / 255);
            const uint8_t b = static_cast<uint8_t>(rgba[offset + 2] * alpha / 255);
            background[y * background_stride + x] = static_cast<uint16_t>(
                ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
        }
    }
    rr_surface_unmap(loaded);
    rr_surface_unmap(background_surface);
    if (cached_surface) rr_surface_destroy(cached_surface);
    if (cached_background) rr_surface_destroy(cached_background);
    cached_surface = loaded;
    cached_background = background_surface;
    loaded_path = path.string();
    load_info(path);
    if (distribution) load_distribution_viewport();
    logger.log(Logger::INF, "Screen decoration loaded: %s", loaded_path.c_str());
    return true;
}

bool create_builtin_decoration() {
    int width = 0;
    int height = 0;
    decoration_canvas_size(&width, &height);
    if (width <= 0 || height <= 0) return false;
    rr_surface_t* generated = rr_surface_create(display, width, height,
                                                RR_PIXEL_FORMAT_RGB565);
    if (!generated) return false;
    auto* pixels = static_cast<uint16_t*>(rr_surface_map(generated));
    if (!pixels) { rr_surface_destroy(generated); return false; }
    const int stride = rr_surface_stride_get(generated) / sizeof(uint16_t);

    constexpr uint16_t background = 0x0842;
    constexpr uint16_t stripe_dark = 0x10a6;
    constexpr uint16_t stripe_blue = 0x2295;
    constexpr uint16_t accent = 0x43bf;
    const int side_width = std::max(24, width / 8);
    const int line_width = std::max(1, width / 320);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint16_t color = background;
            const int edge_distance = std::min(x, width - 1 - x);
            if (edge_distance < side_width) {
                const int diagonal = (edge_distance + y / 3) % 18;
                color = diagonal < 3 ? stripe_blue : stripe_dark;
                if (edge_distance < line_width * 2) color = accent;
            }
            pixels[y * stride + x] = color;
        }
    }
    rr_surface_unmap(generated);
    if (cached_surface) rr_surface_destroy(cached_surface);
    cached_surface = generated;
    loaded_path = "built-in";
    layout = {};
    viewport_display_width = 0;
    viewport_display_height = 0;
    logger.log(Logger::INF, "Screen decoration loaded: built-in RetroRun fallback");
    return true;
}
}

void decoration_init(const char* content_path) {
    content = content_path ? content_path : "";
    const auto setting = conf_map.find("retrorun_decorations");
    enabled = setting != conf_map.end() && value_true(setting->second);
    pixel_perfect_before_enable = pixel_perfect;
    if (enabled)
        pixel_perfect = true;
    load_attempted = false;
}

void decoration_shutdown() {
    if (cached_surface) rr_surface_destroy(cached_surface);
    if (cached_background) rr_surface_destroy(cached_background);
    cached_surface = nullptr;
    cached_background = nullptr;
    load_attempted = false;
    loaded_path.clear();
    layout = {};
    viewport_display_width = 0;
    viewport_display_height = 0;
}

bool decoration_enabled() { return enabled; }

void decoration_set_enabled(bool value) {
    if (value == enabled)
        return;
    if (value) {
        pixel_perfect_before_enable = pixel_perfect;
        pixel_perfect = true;
    } else {
        pixel_perfect = pixel_perfect_before_enable;
    }
    enabled = value;
    load_attempted = false;
    if (!enabled && cached_surface) {
        rr_surface_destroy(cached_surface);
        cached_surface = nullptr;
    }
    if (!enabled && cached_background) {
        rr_surface_destroy(cached_background);
        cached_background = nullptr;
    }
    if (!enabled) {
        layout = {};
        viewport_display_width = 0;
        viewport_display_height = 0;
    }
    conf_map["retrorun_decorations"] = enabled ? "auto" : "off";
    persistVideoSetting("retrorun_decorations", enabled ? "auto" : "off");
    prepareScreen(currentWidth, currentHeight);
}

rr_surface_t* decoration_surface() {
    if (!enabled || !display) return nullptr;
    if (cached_surface) return cached_surface;
    if (load_attempted) return nullptr;
    load_attempted = true;
    for (const auto& candidate : candidates())
        if (std::filesystem::is_regular_file(candidate.path) &&
            load_png(candidate.path, candidate.distribution)) return cached_surface;
    logger.log(Logger::INF, "Screen decorations: no matching PNG found in %s; using built-in fallback",
               decoration_root().string().c_str());
    return create_builtin_decoration() ? cached_surface : nullptr;
}

rr_surface_t* decoration_background_surface() {
    rr_surface_t* foreground = decoration_surface();
    return cached_background ? cached_background : foreground;
}

std::string decoration_directory() {
    return decoration_root().string();
}

std::string decoration_source_label() {
    if (loaded_path.empty()) return enabled ? "Not loaded" : "Disabled";
    if (loaded_path == "built-in") return "RetroRun: built-in";
    const auto distribution_label = [](const std::string& prefix, const char* name) {
        const size_t start = prefix.size();
        const size_t end = loaded_path.find('/', start);
        const std::string pack = loaded_path.substr(start, end - start);
        return std::string(name) + (pack == "systems" || pack.empty() ? "" : ": " + pack);
    };
    for (const std::string prefix : {"/tmp/overlays/bezels/",
                                     "/storage/roms/bezels/"})
        if (loaded_path.rfind(prefix, 0) == 0)
            return distribution_label(prefix, "AmberELEC");
    for (const std::string prefix : {"/roms/bezels/", "/roms2/bezels/"})
        if (loaded_path.rfind(prefix, 0) == 0)
            return distribution_label(prefix, "ArkOS");
    return "RetroRun: local/libretro";
}

void decoration_reload() {
    if (cached_surface) rr_surface_destroy(cached_surface);
    if (cached_background) rr_surface_destroy(cached_background);
    cached_surface = nullptr;
    cached_background = nullptr;
    load_attempted = false;
    loaded_path.clear();
    layout = {};
    viewport_display_width = 0;
    viewport_display_height = 0;
}

bool decoration_game_viewport(int* x, int* y, int* width, int* height) {
    if (!x || !y || !width || !height || !decoration_surface() || !layout.valid)
        return false;
    int display_width = 0;
    int display_height = 0;
    decoration_canvas_size(&display_width, &display_height);
    if (display_width <= 0 || display_height <= 0) return false;

    if (display_width != viewport_display_width || display_height != viewport_display_height) {
        const double scale_x = display_width / layout.width;
        const double scale_y = display_height / layout.height;
        viewport_x = static_cast<int>(layout.left * scale_x + 0.5);
        viewport_y = static_cast<int>(layout.top * scale_y + 0.5);
        viewport_width = display_width - viewport_x -
            static_cast<int>(layout.right * scale_x + 0.5);
        viewport_height = display_height - viewport_y -
            static_cast<int>(layout.bottom * scale_y + 0.5);
        viewport_display_width = display_width;
        viewport_display_height = display_height;
        logger.log(Logger::INF, "Screen decoration viewport: x=%d, y=%d, w=%d, h=%d",
                   viewport_x, viewport_y, viewport_width, viewport_height);
    }
    if (viewport_width <= 0 || viewport_height <= 0) return false;
    // The decoration viewport is the maximum available opening, not a request
    // to distort the core image. Fit the frontend's already-correct game
    // rectangle inside it and preserve that aspect ratio.
    const double game_aspect = *height > 0
        ? static_cast<double>(*width) / *height : 1.0;
    const double viewport_aspect = static_cast<double>(viewport_width) / viewport_height;
    int fitted_width = viewport_width;
    int fitted_height = viewport_height;
    if (game_aspect > viewport_aspect)
        fitted_height = std::max(1, static_cast<int>(viewport_width / game_aspect + 0.5));
    else
        fitted_width = std::max(1, static_cast<int>(viewport_height * game_aspect + 0.5));
    *x = viewport_x + (viewport_width - fitted_width) / 2;
    *y = viewport_y + (viewport_height - fitted_height) / 2;
    *width = fitted_width;
    *height = fitted_height;
    return true;
}
