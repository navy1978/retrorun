
/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "globals.h"
#include "video-helper.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include <fstream>
#include <iterator>
#include <string.h>
#include <string>
#include <sys/time.h>

#include <cmath>
#ifdef RR_PLATFORM_GO2
#include <sys/sysinfo.h>
#endif

#include "platform.h"

#include "fonts.h"
#include "imgs/imgs_retrorun.h"
#include "input.h"
#include "system-info.h"
#include "video.h"

#include <chrono>
#include <cstring>
#include <unordered_map>

int size_char = 8;

int switchColor = 30;
int step = 1;
int posRetro = 3;
bool loop = true;
std::string tabSpaces = "";
int stepCredits = 15;
int posYCredits = 8;
int time_credit = 2;
bool credits_fast = false;
int offset = 0;
bool direction_forward = true;
int rowForText = 0;
int colorInc = 0;

uint32_t format_565 = RR_PIXEL_FORMAT_RGB565; // RR_PIXEL_FORMAT_RGB888; // RR_PIXEL_FORMAT_XRGB8888;//color_format;

int width_fixed = 640;
int height_fixed = 480;

int INFO_MENU_WIDTH = 240;  // 288;
int INFO_MENU_HEIGHT = 160; // 192;
UIProfile uiProfile = UIProfile::Auto;

void setUIProfile(UIProfile profile)
{
    uiProfile = profile;
}

UIProfile getUIProfile()
{
    return uiProfile;
}

UIProfile getResolvedUIProfile()
{
    if (uiProfile != UIProfile::Auto)
        return uiProfile;
#ifdef RR_PLATFORM_GO2
    return UIProfile::Handheld;
#else
    (void)getDeviceName();
    if (isRG351M() || isRG351P() || isRG351V() || isRG351MP() || isRG552() ||
        isRG503() || isRG353V() || isRG353M() || isMiniloongPocket1())
        return UIProfile::Handheld;
    return UIProfile::Desktop;
#endif
}

void updateUIMenuDimensions(int destination_width, int destination_height)
{
    const int previous_width = INFO_MENU_WIDTH;
    const int previous_height = INFO_MENU_HEIGHT;
    if (getResolvedUIProfile() == UIProfile::Handheld) {
        INFO_MENU_WIDTH = 240;
        INFO_MENU_HEIGHT = 160;
    } else {
        INFO_MENU_WIDTH = std::max(320, destination_width / 2);
        INFO_MENU_HEIGHT = std::max(240, destination_height / 2);
    }

    if (INFO_MENU_WIDTH != previous_width || INFO_MENU_HEIGHT != previous_height)
        posYCredits = 8;
}



int colSwitch = 145;
// int col = 42;
int col_increase = 0;
int col = 72;

int display_width=0; 
int display_height=0;
int base_width=0;
int base_height=0;
int max_width=0; 
int max_height=0;
int aw=0; 
int ah=0;
bool isGameVertical= false;
bool isOpenGL= false;
unsigned currentWidth = 0;
unsigned currentHeight = 0;
auto t_flash_start = std::chrono::high_resolution_clock::now();
bool flash = false;

extern rr_battery_state_t batteryState;


rr_display_t *display= NULL;
rr_surface_t *surface= NULL;
rr_surface_t *status_surface_bottom_right= NULL;
rr_surface_t *status_surface_bottom_center= NULL;
rr_surface_t *status_surface_bottom_left= NULL;
rr_surface_t *status_surface_top_right= NULL;
rr_surface_t *status_surface_top_left= NULL;
rr_surface_t *status_surface_full= NULL;

rr_surface_t *display_surface= NULL;
rr_frame_buffer_t *frame_buffer= NULL;
rr_presenter_t *presenter= NULL;
rr_context_t *context3D= NULL;

rr_surface_t *gles_surface= NULL;
struct timeval valTime2;





bool cmpf(float A, float B, float epsilon)
{
    return (fabs(A - B) < epsilon);
}


rr_rotation_t getBlitRotation()
{
#ifdef RR_PLATFORM_SDL
    if (!isTate()) return RR_ROTATION_DEGREES_0;
    return tateState == REVERSED ? RR_ROTATION_DEGREES_270
                                 : RR_ROTATION_DEGREES_90;
#endif
    
    if (isGameVertical) // portrait
    {
        if (!isTate())
        {

            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_0 : RR_ROTATION_DEGREES_270;
        }
        if (tateState == REVERSED)
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_90 : RR_ROTATION_DEGREES_0;
        }
        else
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_270 : RR_ROTATION_DEGREES_180;
        }
    }
    else // landscape
    {

        if (!isTate() && tateState != REVERSED)
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_0 : RR_ROTATION_DEGREES_270;
        }
        if (tateState == REVERSED)
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_90 : RR_ROTATION_DEGREES_0;
        }
        else
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_270 : RR_ROTATION_DEGREES_180;
        }
    }
}

rr_rotation_t getRotation()
{
#ifdef RR_PLATFORM_SDL
    if (!isTate()) return RR_ROTATION_DEGREES_0;
    return tateState == REVERSED ? RR_ROTATION_DEGREES_270
                                 : RR_ROTATION_DEGREES_90;
#endif

    if (isGameVertical) // portrait
    {
        if (!isTate())
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_270 : RR_ROTATION_DEGREES_180;
        }
        if (tateState == REVERSED)
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_0 : RR_ROTATION_DEGREES_270;
        }
        else
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_180 : RR_ROTATION_DEGREES_90;
        }
    }
    else
    { // landscape
        if (!isTate() && tateState != REVERSED)
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_0 : RR_ROTATION_DEGREES_270;
        }
        if (tateState == REVERSED)
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_90 : RR_ROTATION_DEGREES_0;
        }
        else
        {
            return (hasDeviceRotatedScreen()) ? RR_ROTATION_DEGREES_270 : RR_ROTATION_DEGREES_180;
        }
    }
}

int getFixedWidth(int alternative)
{
    // some games like Resident Evil 2 for Flycast has an ovescan issue in 640x480
    if (isFlycast() || isFlycast2021())
    {
        if (resolution == R_320_240)
            return 320;
        else if (resolution == R_640_480 && isRG552())
            return 640;
        else
            return alternative;
    }
    else if (isMGBA())
        return 240;
    else
    {
        return alternative;
    }
}

int getFixedHeight(int alternative)
{
    // some games like Resident Evil 2 for Flycast has an ovescan issue in 640x480
    if (isFlycast() || isFlycast2021())
    {
        if (resolution == R_320_240)
            return 240;
        else if (resolution == R_640_480 && isRG552())
            return 480;
        else
            return alternative;
    }
    else if (isMGBA())
        return 160;
    else
    {
        return alternative;
    }
}
int getBase_width()
{
    return getFixedWidth(base_width);
}

int getBase_height()
{
    return getFixedHeight(base_height);
}
int getMax_width()
{
    return getFixedWidth(max_width);
}

int getMax_height()
{
    return getFixedHeight(max_height);
}

int getGeom_max_width(const struct retro_game_geometry *geom)
{
    return getFixedWidth(static_cast<int>(geom->max_width));
}

int getGeom_max_height(const struct retro_game_geometry *geom)
{
    return getFixedHeight(static_cast<int>(geom->max_height));
}


///////




void showText(int x, int y, const char *text, unsigned short color, rr_surface_t **surface)
{

    if (*surface == nullptr)
    {

        *surface = rr_surface_create(display, 200, 20, format_565);
    }

    uint8_t *dst = (uint8_t *)rr_surface_map(*surface);
    if (dst == nullptr)
    {
        return;
    }
    int dst_stride = rr_surface_stride_get(*surface);
    basic_text_out16_nf_color_clipped(dst, dst_stride / 2,
                                      rr_surface_width_get(*surface), rr_surface_height_get(*surface),
                                      x, y, text, color);
}

void showTextBigger(int x, int y, const char *text, unsigned short color, rr_surface_t **surface)
{

    if (*surface == nullptr)
    {

        *surface = rr_surface_create(display, 150, 20, format_565);
    }

    uint8_t *dst = (uint8_t *)rr_surface_map(*surface);
    if (dst == nullptr)
    {
        return;
    }
    int dst_stride = rr_surface_stride_get(*surface);
    basic_text_out16_nf_color_clipped(dst, dst_stride / 2,
                                      rr_surface_width_get(*surface), rr_surface_height_get(*surface),
                                      x, y, text, color);
    //basic_text_out16x16_nf_color_scaled_from_8x8(dst, dst_stride / 2, x, y, text, color);
}


int getRowForText()
{
    const int row_step = getResolvedUIProfile() == UIProfile::Handheld
                             ? 10
                             : 12;
    rowForText += row_step;
    return rowForText;
}

std::string stripReturnCarriage(std::string input)
{
    // Remove newline characters from the input
    int len = 30;
    int i, j;
    for (i = 0, j = 0; i < len; i++)
    {
        if (input[i] != '\n')
        {
            input[j++] = input[i];
        }
        else
        {
            break;
        }
    }
    input[j] = '\0';
    return input;
}


bool canCreditBeDrawn(int pos)
{
    return pos > 0 && pos < INFO_MENU_HEIGHT - 10;
}

void resetCredisPosition()
{
    posYCredits = 8;
    time_credit = 2;
    credits_fast = false;
}

void setCreditsAccelerated(bool accelerated)
{
    credits_fast = accelerated;
    if (credits_fast)
        time_credit = 0;
}

static void drawCreditLogo(int image_y, rr_surface_t **surface)
{
    if (!surface || !*surface) return;
    const int image_x = (INFO_MENU_WIDTH - static_cast<int>(retrorun_logo.width)) / 2;
    const int source_width = static_cast<int>(retrorun_logo.width);
    const int source_height = static_cast<int>(retrorun_logo.height);
    const int start_x = std::max(0, image_x);
    const int end_x = std::min(INFO_MENU_WIDTH, image_x + source_width);
    const int start_y = std::max(0, image_y);
    const int end_y = std::min(INFO_MENU_HEIGHT, image_y + source_height);
    if (start_x >= end_x || start_y >= end_y) return;

    auto* destination = static_cast<uint8_t*>(rr_surface_map(*surface));
    if (!destination) return;
    const int destination_stride = rr_surface_stride_get(*surface);
    const auto* source = reinterpret_cast<const uint16_t*>(retrorun_logo.pixel_data);
    for (int y = start_y; y < end_y; ++y) {
        const int source_y = y - image_y;
        const int source_x = start_x - image_x;
        std::memcpy(destination + y * destination_stride + start_x * sizeof(uint16_t),
                    source + source_y * source_width + source_x,
                    static_cast<size_t>(end_x - start_x) * sizeof(uint16_t));
    }
    rr_surface_unmap(*surface);
}


void showLongCenteredText(int y, const char *text, unsigned short color, rr_surface_t **surface)
{
    struct MarqueeState {
        std::string text;
        int viewport_width = 0;
        std::chrono::steady_clock::time_point started;
    };
    static std::unordered_map<int, MarqueeState> states;

    const std::string title(text ? text : "");
    const int total_text_width = static_cast<int>(title.length()) * 8;

    if (total_text_width > INFO_MENU_WIDTH) {
        MarqueeState &state = states[y];
        if (state.text != title || state.viewport_width != INFO_MENU_WIDTH) {
            state.text = title;
            state.viewport_width = INFO_MENU_WIDTH;
            state.started = std::chrono::steady_clock::now();
        }

        constexpr double speed_pixels_per_second = 20.0;
        constexpr double endpoint_pause_seconds = 0.7;
        const int maximum_offset = total_text_width - INFO_MENU_WIDTH;
        const double travel_seconds = maximum_offset / speed_pixels_per_second;
        const double cycle_seconds = endpoint_pause_seconds * 2.0 + travel_seconds * 2.0;
        double elapsed = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - state.started).count();
        elapsed = std::fmod(elapsed, cycle_seconds);

        double offset = 0.0;
        if (elapsed < endpoint_pause_seconds) {
            offset = 0.0;
        } else if (elapsed < endpoint_pause_seconds + travel_seconds) {
            offset = (elapsed - endpoint_pause_seconds) * speed_pixels_per_second;
        } else if (elapsed < endpoint_pause_seconds * 2.0 + travel_seconds) {
            offset = maximum_offset;
        } else {
            offset = maximum_offset -
                     (elapsed - endpoint_pause_seconds * 2.0 - travel_seconds) *
                         speed_pixels_per_second;
        }

        showText(-static_cast<int>(offset), y, title.c_str(), color, surface);
    } else {
        showText(INFO_MENU_WIDTH / 2 - total_text_width / 2, y, title.c_str(), color, surface);
    }
}


void showCenteredText(int y, const char *text, unsigned short color, rr_surface_t **surface)
{
    /*std::string title(text); // The text to scroll
    int title_length = title.length();
    showText(INFO_MENU_WIDTH / 2 - title_length * 8 / 2, y, title.c_str(), color, surface);
    */
    showLongCenteredText(y,text,color,surface);
    // showText(0, y, title.c_str(), color, surface);
}

static void drawSelectedMenuRow(int y, rr_surface_t **surface)
{
    if (!surface || !*surface) return;
    auto *pixels = static_cast<uint16_t *>(rr_surface_map(*surface));
    if (!pixels) return;

    const int width = rr_surface_width_get(*surface);
    const int height = rr_surface_height_get(*surface);
    const int stride = rr_surface_stride_get(*surface) / sizeof(uint16_t);
    const int row_height = getResolvedUIProfile() == UIProfile::Handheld ? 10 : 12;
    const int top = std::max(0, y - 1);
    const int bottom = std::min(height, top + row_height);
    const int left = 4;
    const int right = std::max(left, width - 4);
#ifdef RR_PLATFORM_GO2
    // The native handheld panels render very dark RGB565 blues close to black.
    constexpr uint16_t selected_blue = 0x2b5f;
#else
    constexpr uint16_t selected_blue = 0x214a;
#endif

    for (int py = top; py < bottom; ++py)
        std::fill(pixels + py * stride + left,
                  pixels + py * stride + right,
                  selected_blue);
    rr_surface_unmap(*surface);
}

static void drawMenuRowColumns(int y, const std::string &label,
                               const std::string &right_text,
                               unsigned short color, rr_surface_t **surface)
{
    constexpr int margin = 12;
    constexpr int glyph_width = 8;
    const int right_edge = INFO_MENU_WIDTH - margin;
    const int right_width = static_cast<int>(right_text.size()) * glyph_width;
    const int right_x = right_edge - right_width;
    const int gap = right_text.empty() ? 0 : glyph_width;
    const int required_width = static_cast<int>(label.size()) * glyph_width +
                               gap + right_width;
    const int available_width = INFO_MENU_WIDTH - margin * 2;

    if (required_width <= available_width) {
        showText(margin, y, label.c_str(), color, surface);
        if (!right_text.empty())
            showText(right_x, y, right_text.c_str(), color, surface);
    } else {
        const std::string complete_row = right_text.empty()
                                             ? label
                                             : label + "  " + right_text;
        showLongCenteredText(y, complete_row.c_str(), color, surface);
    }
}

static void drawInfoRow(const std::string &label, const std::string &value,
                        rr_surface_t **surface)
{
    constexpr int margin = 12;
    constexpr int glyph_width = 8;
    const int row = getRowForText();
    const int value_width = static_cast<int>(value.size()) * glyph_width;
    const int value_x = INFO_MENU_WIDTH - margin - value_width;
    const int required_width = static_cast<int>(label.size()) * glyph_width +
                               glyph_width + value_width;
    const int available_width = INFO_MENU_WIDTH - margin * 2;

    if (required_width <= available_width) {
        showText(margin, row, label.c_str(), DARKGREY, surface);
        showText(value_x, row, value.c_str(), LIGHTGREY, surface);
    } else {
        const std::string complete_row = label + ": " + value;
        showLongCenteredText(row, complete_row.c_str(), LIGHTGREY, surface);
    }
}

void showMovingHeaderText(int y, const char *text, unsigned short color, rr_surface_t **surface)
{
    const std::string title(text ? text : "");
    const int text_width = static_cast<int>(title.length()) * 8;
    if (text_width >= INFO_MENU_WIDTH) {
        showLongCenteredText(y, title.c_str(), color, surface);
        return;
    }

    struct HeaderState {
        std::string text;
        int viewport_width = 0;
        std::chrono::steady_clock::time_point started;
    };
    static HeaderState state;
    if (state.text != title || state.viewport_width != INFO_MENU_WIDTH) {
        state.text = title;
        state.viewport_width = INFO_MENU_WIDTH;
        state.started = std::chrono::steady_clock::now();
    }

    constexpr double speed_pixels_per_second = 30.0;
    const int maximum_x = INFO_MENU_WIDTH - text_width;
    const double travel_seconds = maximum_x / speed_pixels_per_second;
    const double cycle_seconds = travel_seconds * 2.0;
    double elapsed = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - state.started).count();
    elapsed = std::fmod(elapsed, cycle_seconds);

    // Bounce as soon as the text touches either edge. The previous endpoint
    // pause made the header appear unresponsive before changing direction.
    const double position = elapsed < travel_seconds
        ? elapsed * speed_pixels_per_second
        : maximum_x - (elapsed - travel_seconds) * speed_pixels_per_second;

    showText(static_cast<int>(position), y, title.c_str(), color, surface);
}











void drawCreditLine(int y, const char *text, unsigned short color, rr_surface_t **surface)
{

    int currentY = y;
    if (canCreditBeDrawn(currentY))
    {
        showCenteredText(currentY, text, color, surface);
    }
}

static std::string readDeviceTreeSoC(const char *path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.good())
        return {};

    const std::string compatible((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
    size_t offset = 0;
    while (offset < compatible.size())
    {
        const size_t end = compatible.find('\0', offset);
        const std::string entry = compatible.substr(
            offset, end == std::string::npos ? std::string::npos : end - offset);
        static const std::string prefix = "rockchip,rk";
        if (entry.compare(0, prefix.size(), prefix) == 0)
        {
            std::string model = entry.substr(std::string("rockchip,").size());
            std::transform(model.begin(), model.end(), model.begin(), [](unsigned char value) {
                return static_cast<char>(std::toupper(value));
            });
            return "Rockchip " + model;
        }
        if (end == std::string::npos)
            break;
        offset = end + 1;
    }
    return {};
}

static std::string getSoCName()
{
    std::string soc = readDeviceTreeSoC("/sys/firmware/devicetree/base/compatible");
    if (soc.empty())
        soc = readDeviceTreeSoC("/proc/device-tree/compatible");
    if (!soc.empty())
        return soc;

    // Device-tree information is occasionally hidden by older distributions.
    // Keep a small fallback for devices already identified by RetroRun.
    if (isRG552())
        return "Rockchip RK3399";
    if (isRG503() || isRG353Family())
        return "Rockchip RK3566";
    if (isRG351M() || isRG351P() || isRG351V() || isRG351MP())
        return "Rockchip RK3326";
    return "Unknown";
}



void showInfoDevice(rr_surface_t **surface)
{
#ifdef RR_PLATFORM_SDL
    const SystemInfo info = querySystemInfo();
    drawInfoRow("Host", info.hostname, surface);
    drawInfoRow("Model", info.model, surface);
    drawInfoRow("OS", info.operating_system, surface);
    drawInfoRow("CPU", info.cpu, surface);
    drawInfoRow("CPU threads", std::to_string(info.logical_cpus), surface);
    drawInfoRow("Renderer", info.gpu, surface);
    std::string memory = std::to_string(info.total_memory_mb) + " MB";
    if (info.available_memory_mb)
        memory += " (free " + std::to_string(info.available_memory_mb) + " MB)";
    drawInfoRow("RAM", memory, surface);
    return;
#endif
    std::string hostName(getDeviceName());
    hostName = stripReturnCarriage(hostName);
    drawInfoRow("Model", hostName, surface);
    drawInfoRow("SoC", getSoCName(), surface);

    std::string tot_ram = "Total RAM: N/A";
#ifdef RR_PLATFORM_GO2
    struct sysinfo sys_info;
    // std::string procs = "Number of processes: N/A";
    if (sysinfo(&sys_info) == 0)
    {
        long total_ram_val = sys_info.totalram / (1024 * 1024);
        // long number_procs = sys_info.procs;
        tot_ram = std::to_string(total_ram_val) + " MB";
        // procs = "# of processes:" + std::to_string(number_procs);
    }
#endif

    for (const auto &cpu_info : cpu_info_list)
    {
        drawInfoRow("CPU(s)", cpu_info.number_of_cpu + " " + cpu_info.cpu_name, surface);
        // showCenteredText(getRowForText(), ("CPU(s) Model:"+cpu_info.cpu_name).c_str(), DARKGREY, surface);
        // showCenteredText(getRowForText(), ("CPU(s) Thread per Core:"+cpu_info.thread_per_cpu).c_str(), DARKGREY, surface);
    }
    // This will print the values of number_of_cpu, cpu_name, thread_per_cpu, and device_name for each element in the cpu_info_list vector.

    // showCenteredText(getRowForText(), (tot_ram).c_str(), DARKGREY, surface);

    drawInfoRow("GPU", gpu_name, surface);
    drawInfoRow("RAM", tot_ram, surface);
    // showCenteredText(getRowForText(), (procs).c_str(), DARKGREY, surface);
}

void showInfoCore(rr_surface_t **surface)
{
    drawInfoRow("Name", coreName, surface);
    drawInfoRow("Version", coreVersion, surface);
    drawInfoRow("ZIP files", coreReadZippedFiles ? "Allowed" : "Not allowed", surface);
    drawInfoRow("OpenGL", isOpenGL ? "Yes" : "No", surface);
}

void showInfoGame(rr_surface_t **surface)
{
    drawInfoRow("Original FPS", std::to_string(static_cast<int>(originalFps)), surface);
    drawInfoRow("Average FPS", std::to_string(static_cast<int>(avgFps)), surface);
    drawInfoRow("Resolution", std::to_string(currentWidth) + "x" +
                std::to_string(currentHeight), surface);
    drawInfoRow("Orientation", isGameVertical ? "Portrait" : "Landscape", surface);
}

void showInfoGraphics(int, rr_surface_t **surface, int)
{
    const std::string backend = std::string(rr_platform_backend_name()) == "sdl2"
                                    ? "SDL2" : "GO2 / DRM";
    drawInfoRow("Backend", backend, surface);
    drawInfoRow("Renderer", rr_platform_renderer_name(), surface);
    drawInfoRow("Core video", isOpenGL ? "Hardware OpenGL" : "Software", surface);

    const std::string output = std::to_string(rr_display_width_get(display)) + "x" +
                               std::to_string(rr_display_height_get(display)) + " / Core " +
                               std::to_string(currentWidth) + "x" + std::to_string(currentHeight);
    drawInfoRow("Output", output, surface);

    const char *pixel_format = "Unknown";
    if (color_format == RR_PIXEL_FORMAT_RGB565) pixel_format = "RGB565";
    else if (color_format == RR_PIXEL_FORMAT_RGB888) pixel_format = "RGB888";
    else if (color_format == RR_PIXEL_FORMAT_XRGB8888) pixel_format = "XRGB8888";
    else if (color_format == RR_PIXEL_FORMAT_RGBA8888) pixel_format = "RGBA8888";
    else if (color_format == RR_PIXEL_FORMAT_RGBA5551) pixel_format = "RGBA5551";
    drawInfoRow("Pixel format", pixel_format, surface);

    char aspect[64] = {};
    std::snprintf(aspect, sizeof(aspect), "%.3f", aspect_ratio);
    drawInfoRow("Aspect ratio", aspect, surface);

    static const char *filter_names[] = {"Off", "Nearest", "Linear"};
    static const char *shader_names[] = {"Off", "Scanlines", "CRT"};
    const int filter = static_cast<int>(rr_video_filter_get());
    const int shader = static_cast<int>(rr_video_shader_get());
    const std::string effects = std::string(filter_names[filter]) +
                                " / Shader " + shader_names[shader];
    drawInfoRow("Filter", effects, surface);

#ifdef RR_PLATFORM_SDL
    const std::string sync = std::string(rr_video_vsync_get() ? "On" : "Off") +
                             " / Pixel " + (pixel_perfect ? "On" : "Off");
#else
    const std::string sync = std::string("Backend / Pixel ") +
                             (pixel_perfect ? "On" : "Off");
#endif
    drawInfoRow("VSync", sync, surface);

    static const char *profile_names[] = {"Auto", "Handheld", "Desktop"};
    const std::string profile =
        std::string(profile_names[static_cast<int>(getUIProfile())]) + " -> " +
        profile_names[static_cast<int>(getResolvedUIProfile())];
    drawInfoRow("UI profile", profile, surface);
}

void showInfoDRM(rr_surface_t **surface)
{
    rr_display_diagnostics_t diagnostics = {};
    rr_display_diagnostics_get(display, &diagnostics);
    if (!diagnostics.available)
    {
        drawInfoRow("DRM", "Not available", surface);
        return;
    }

    std::string setting;
    if (drmDirectScanoutMode == DRMDirectScanoutMode::Enabled)
        setting = "True";
    else
        setting = "False";
    drawInfoRow("Direct setting", setting, surface);

    drawInfoRow("DRM driver", diagnostics.driver, surface);
    drawInfoRow("Panel mode", std::to_string(diagnostics.mode_width) + "x" +
                std::to_string(diagnostics.mode_height) + "@" +
                std::to_string(diagnostics.refresh_hz), surface);
    drawInfoRow("Connector / CRTC", std::to_string(diagnostics.connector_id) + " / " +
                std::to_string(diagnostics.crtc_id), surface);

    char format[5] = {};
    for (int i = 0; i < 4; ++i)
    {
        const char value = static_cast<char>((diagnostics.plane_format >> (i * 8)) & 0xff);
        format[i] = value >= 32 && value <= 126 ? value : '-';
    }
    const std::string plane = diagnostics.plane_id
        ? std::to_string(diagnostics.plane_id) + " / " + format
        : "Not probed";
    drawInfoRow("Plane / format", plane, surface);

    const std::string plane_rotation = diagnostics.plane_id
        ? std::string(diagnostics.rotation_property_found ? "Property" : "No property") +
              (diagnostics.rotation_applied ? " / OK" : " / Failed")
        : "Not probed";
    drawInfoRow("Plane rotation", plane_rotation, surface);
    drawInfoRow("Page flip", diagnostics.page_flip_fallback ? "SetCrtc fallback" : "Enabled",
                surface);

    std::string direct_result;
    if (diagnostics.direct_rejected)
        direct_result = "Rejected / errno " + std::to_string(diagnostics.direct_errno);
    else if (diagnostics.direct_frames)
        direct_result = "Accepted / " + std::to_string(diagnostics.direct_frames);
    else if (drmDirectScanoutDiagnosticCompleted)
        direct_result = "No direct frames";
    else
        direct_result = "Not tested";
    drawInfoRow("Direct result", direct_result, surface);

    const std::string vblank = diagnostics.direct_frames
        ? std::to_string(diagnostics.vblank_average_us) + " / " +
              std::to_string(diagnostics.vblank_last_us) + " / " +
              std::to_string(diagnostics.vblank_max_us) + " E" +
              std::to_string(diagnostics.vblank_failures)
        : "No samples";
    drawInfoRow("VBlank avg/last/max", vblank, surface);
    drawInfoRow("Press A", "Run 3s test", surface);
}

void showCredits(rr_surface_t **surface)
{
    if (credits_fast)
    {
        posYCredits--;
    }
    else if (time_credit > 0)
    {
        time_credit--;
    }
    else
    {
        posYCredits--;
        time_credit = 3;
    }

    /// DEV
    int currentY = posYCredits;

    drawCreditLogo(currentY, surface);
    currentY += static_cast<int>(retrorun_logo.height) + stepCredits;

    drawCreditLine(currentY, "RetroRun", ORANGE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "Light libretro front-end", YELLOW, surface);

    currentY += stepCredits * 3;
    drawCreditLine(currentY, "Developers", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "OtherCrashOverride", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "navy1978", WHITE, surface);

    currentY += stepCredits * 3;
    drawCreditLine(currentY, "js2xbox developers", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "Emanem", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "lualiliu", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "christianhaitian", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "navy1978", WHITE, surface);
    
    currentY += stepCredits * 3;
    drawCreditLine(currentY, "libgo2 developers", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "OtherCrashOverride", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "navy1978", WHITE, surface);

    currentY += stepCredits * 3;

    drawCreditLine(currentY, "RetroAchievements", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "rcheevos (MIT License)", WHITE, surface);

    currentY += stepCredits * 3;
    drawCreditLine(currentY, "Portable libraries", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "SDL2 / libpng", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "libcurl / zlib", WHITE, surface);

    currentY += stepCredits * 3;
    drawCreditLine(currentY, "GO2 platform libraries", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "DRM / GBM / EGL", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "OpenGL ES / RGA", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "ALSA / OpenAL Soft", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "libevdev", WHITE, surface);

    currentY += stepCredits * 3;
    drawCreditLine(currentY, "Artwork", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "Streamline Core Line Free", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "libretro/common-overlays", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "CC BY 4.0", WHITE, surface);

    currentY += stepCredits * 3;
    
    drawCreditLine(currentY, "Thanks to", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "Cebion", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "christianhaitian", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "dhwz", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "flyinghead", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "madcat1990", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "pkegg", WHITE, surface);
     currentY += stepCredits;
    drawCreditLine(currentY, "superdealloc", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "Szalik", WHITE, surface);

    if (currentY < 0)
    {                                     // they are over
        std::string title = "Thank you!"; // The text to scroll
        int title_length = title.length();
        showText(INFO_MENU_WIDTH / 2 - title_length * 8 / 2, INFO_MENU_HEIGHT / 2 - 8 / 2, title.c_str(), WHITE, surface);
    }
}




void showInfo(int w, rr_surface_t **surface)
{

    rowForText = 0;
    int posX = 0;

    std::string title = "RetroRun - " + release;
    showMovingHeaderText(2, title.c_str(), WHITE, surface);

    showText(posX, getRowForText(), " ", ORANGE, surface);
    showText(posX, getRowForText(), " ", ORANGE, surface);

    Menu &menu = menuManager.getCurrentMenu();
    std::string menuTitle = menu.getName();
    int menuTitle_length = menuTitle.length();

    showText(INFO_MENU_WIDTH / 2 - menuTitle_length * size_char / 2, getRowForText(), (menu.getName()).c_str(), ORANGE, surface);
    showText(posX, getRowForText(), " ", ORANGE, surface);
    // showText(posX, getRowForText(), " ", ORANGE, surface);
    // showText(posX, getRowForText(), " ", ORANGE, surface);
    int firstVisibleItem = 0;
    int lastVisibleItem = menu.getSize();
    if (menuTitle == "Catalog")
    {
        const int maxVisibleItems = 18;
        int selectedItem = 0;
        for (int i = 0; i < menu.getSize(); i++)
        {
            if (menu.getItems()[i].isSelected())
            {
                selectedItem = i;
                break;
            }
        }
        firstVisibleItem = std::max(0, selectedItem - maxVisibleItems / 2);
        firstVisibleItem = std::min(firstVisibleItem,
                                    std::max(0, menu.getSize() - maxVisibleItems));
        lastVisibleItem = std::min(menu.getSize(),
                                   firstVisibleItem + maxVisibleItems);
    }

    for (int i = firstVisibleItem; i < lastVisibleItem; i++)
    {

        MenuItem &mi = menu.getItems()[i];
        if (mi.get_name() == SHOW_DEVICE)
        {
            showInfoDevice(surface);
        }
        else if (mi.get_name() == SHOW_CORE)
        {
            showInfoCore(surface);
        }
        else if (mi.get_name() == SHOW_GAME)
        {
            showInfoGame(surface);
        }
        else if (mi.get_name() == SHOW_GRAPHICS)
        {
            showInfoGraphics(w, surface, posX);
        }
        else if (mi.get_name() == SHOW_DRM)
        {
            showInfoDRM(surface);
        }

        else if (mi.isQuit()|| mi.isQuestion())
        {
            const int row = getRowForText();
            if (mi.isSelected()) drawSelectedMenuRow(row, surface);
            showCenteredText(row, (tabSpaces + mi.get_name() + ": < " + mi.getValues()[mi.getValue()] + " >").c_str(), mi.isSelected() ? WHITE : DARKGREY, surface);
        }
        else if (mi.getMenu() != NULL)
        {
            const int row = getRowForText();
            if (mi.isSelected()) drawSelectedMenuRow(row, surface);
            drawMenuRowColumns(row, mi.get_name(), ">",
                               mi.isSelected() ? WHITE : DARKGREY, surface);
        }
        else if (mi.m_valueCalculator != NULL)
        {
            const int row = getRowForText();
            if (mi.isSelected()) drawSelectedMenuRow(row, surface);
            const std::string value = "< " + mi.getStringValue() + mi.getMisUnit() + " >";
            drawMenuRowColumns(row, mi.get_name(), value,
                               mi.isSelected() ? WHITE : DARKGREY, surface);
        }
        else
        {
            const int row = getRowForText();
            if (mi.isSelected()) drawSelectedMenuRow(row, surface);
            const MenuItem::Presentation presentation = mi.getPresentation();
            const auto color = presentation == MenuItem::Presentation::SectionHeader
                                   ? ORANGE
                                   : (presentation == MenuItem::Presentation::Detail
                                          ? WHITE
                                          : (mi.isSelected() ? WHITE : DARKGREY));
            drawMenuRowColumns(row, mi.get_name(), "",
                               color, surface);
        }
    }

    time_t curr_time;
    tm *curr_tm;

    char time_string[100];

    std::time(&curr_time);
    curr_tm = localtime(&curr_time);

    strftime(time_string, 50, "%R", curr_tm);
    std::string timeString(time_string);
    int timeString_length = timeString.length();

    std::string delimiter = ":";

    std::string arr[2];

    size_t pos = 0;
    std::string token;
    int i = 0;
    while ((pos = timeString.find(delimiter)) != std::string::npos)
    {
        token = timeString.substr(0, pos);
        arr[i] = token.c_str();
        timeString.erase(0, pos + delimiter.length());
        i++;
    }
    arr[i] = timeString.c_str();
    posX = INFO_MENU_WIDTH - 1 - timeString_length * size_char;

    showText(posX, INFO_MENU_HEIGHT - 1 - size_char, arr[0].c_str(), WHITE, surface);
    if (switchColor > 0)
    {
        showText(posX + 2 * size_char, INFO_MENU_HEIGHT - 1 - size_char, ":", WHITE, surface);
    }
    else
    {
        showText(posX + 2 * size_char, INFO_MENU_HEIGHT - 1 - size_char, " ", WHITE, surface);
        if (switchColor < -30)
        {
            switchColor = 30;
        }
    }
    showText(posX + 3 * size_char, INFO_MENU_HEIGHT - 1 - size_char, arr[1].c_str(), WHITE, surface);
    switchColor--;

    std::string bat = tabSpaces + "Battery:";
    showText(1, INFO_MENU_HEIGHT - 1 - size_char, const_cast<char *>(bat.append(std::to_string(batteryState.level)).append("%").c_str()), WHITE, surface);
}

std::string getCurrentTimeForFileName()
{
    time_t t = time(0); // get time now
    struct tm *now = localtime(&t);
    char buffer[80];
    strftime(buffer, 80, "%y%m%d-%H%M%S", now);
    std::string str(buffer);
    return str;
}

void showFPSImage()
{
    static rr_surface_t* rendered_surface = nullptr;
    static int rendered_fps = -1;
    const int displayed_fps = std::max(0, static_cast<int>(fps));
#ifdef RR_PLATFORM_SDL
    const bool large_handheld_fps = getResolvedUIProfile() == UIProfile::Handheld;
#else
    constexpr bool large_handheld_fps = false;
#endif

    // Hot path: this function is called every presented frame, while the FPS
    // value changes much less frequently. Avoid string creation, surface
    // queries and mapping unless the displayed integer actually changes.
    if (rendered_surface == status_surface_top_right &&
        status_surface_top_right && rendered_fps == displayed_fps)
        return;

    const std::string label = "FPS: " + std::to_string(displayed_fps);
    const int horizontal_padding = large_handheld_fps ? 8 : 6;
    const int glyph_width = large_handheld_fps ? 16 : 8;
    const int surface_height = large_handheld_fps ? 28 : 20;
    // Reserve three FPS digits from the first frame. If an unusually large
    // value needs more room, grow the surface but never shrink it again during
    // the session: GO2 recycles scanout buffers and a narrower replacement
    // would otherwise leave part of the previous rectangle visible.
    constexpr int minimum_characters = 8; // "FPS: " + three digits
    const int requested_width = horizontal_padding * 2 +
        std::max(minimum_characters, static_cast<int>(label.size())) * glyph_width;
    const int current_width = status_surface_top_right
        ? rr_surface_width_get(status_surface_top_right) : 0;
    const int surface_width = std::max(current_width, requested_width);

    if (status_surface_top_right &&
        (current_width < requested_width ||
         rr_surface_height_get(status_surface_top_right) != surface_height)) {
        rr_surface_destroy(status_surface_top_right);
        status_surface_top_right = nullptr;
    }
    if (!status_surface_top_right)
        status_surface_top_right = rr_surface_create(display, surface_width,
                                                     surface_height, format_565);

    rendered_surface = status_surface_top_right;
    rendered_fps = displayed_fps;

    const int stride = rr_surface_stride_get(status_surface_top_right) / 2;
    auto* destination = static_cast<uint16_t*>(rr_surface_map(status_surface_top_right));
    if (!destination) return;
    std::fill(destination, destination + stride * surface_height,
              static_cast<uint16_t>(0x0000));
    // Opaque two-tone bevel matching the menu and notification frame.
    std::fill(destination + 1 * stride + 3,
              destination + 1 * stride + surface_width - 3,
              static_cast<uint16_t>(0x6b4d));
    std::fill(destination + 2 * stride + 2,
              destination + 2 * stride + surface_width - 2,
              static_cast<uint16_t>(0x5acb));
    std::fill(destination + (surface_height - 2) * stride + 3,
              destination + (surface_height - 2) * stride + surface_width - 2,
              static_cast<uint16_t>(0x18e3));
    for (int y = 3; y < surface_height - 2; ++y) {
        destination[y * stride + 1] = 0x5acb;
        destination[y * stride + surface_width - 2] = 0x2124;
    }
    if (large_handheld_fps) {
        basic_text_out16x16_nf_color_scaled_from_8x8(
            destination, stride, horizontal_padding + 1, 7,
            label.c_str(), 0x4208);
        basic_text_out16x16_nf_color_scaled_from_8x8(
            destination, stride, horizontal_padding, 6,
            label.c_str(), WHITE);
    } else {
        basic_text_out16_nf_color_clipped(destination, stride, surface_width,
                                          surface_height, horizontal_padding + 1, 7,
                                          label.c_str(), 0x4208);
        basic_text_out16_nf_color_clipped(destination, stride, surface_width,
                                          surface_height, horizontal_padding, 6,
                                          label.c_str(), WHITE);
    }
    rr_surface_unmap(status_surface_top_right);
}

void showFullImage_888(int x, int y, int width, int height, const uint8_t *src, rr_surface_t **surface)
{
    int bytes = 4;
    // create the different surfaces for the statues
    if (*surface == nullptr)
    {
        *surface = rr_surface_create(display, width, height, RR_PIXEL_FORMAT_RGBA8888);
    }
    int src_stride = width * bytes;
    uint8_t *dst = (uint8_t *)rr_surface_map(*surface);
    if (dst == nullptr)
    {
        return;
    }
    int dst_stride = rr_surface_stride_get(*surface);
    src += 0;
    dst += x * bytes + y * dst_stride;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // Get the pixel color and alpha value
            const uint8_t *src_pixel = src + x * bytes;
            uint8_t alpha = src_pixel[3];

            // If the alpha value is 0, set the pixel to transparent
            if (alpha == 0)
            {
                dst[x * bytes + 0] = 0;
                dst[x * bytes + 1] = 0;
                dst[x * bytes + 2] = 0;
                dst[x * bytes + 3] = 0;
            }
            // Otherwise, set the pixel color and alpha value
            else
            {
                dst[x * bytes + 0] = src_pixel[0];
                dst[x * bytes + 1] = src_pixel[1];
                dst[x * bytes + 2] = src_pixel[2];
                dst[x * bytes + 3] = alpha;
            }
        }

        src += src_stride;
        dst += dst_stride;
    }
}

void showFullImage(int x, int y, int width, int height, const uint8_t *src, rr_surface_t **surface)
{

    // create the different surfaces for the statues
    if (*surface == nullptr)
    {

        *surface = rr_surface_create(display, width, height, format_565);
    }
    int src_stride = width * sizeof(short);
    uint8_t *dst = (uint8_t *)rr_surface_map(*surface);
    if (dst == nullptr)
    {
        return;
    }
    int dst_stride = rr_surface_stride_get(*surface);
    src += 0;
    dst += x * sizeof(short) + y * dst_stride;
    for (int y = 0; y < height; ++y)
    {
        memcpy(dst, src, width * sizeof(short));
        src += src_stride;
        dst += dst_stride;
    }
}
// refactor

void showImage(Image img, rr_surface_t **surface)
{
    showFullImage(0, 0, img.width, img.height, img.pixel_data, surface);
}



void takeScreenshot(int w, int h)
{
    logger.log(Logger::DEB, "Taking a screenshot!");
    rr_surface_t *source = surface;
#ifdef RR_PLATFORM_SDL
    rr_surface_t *captured_surface = nullptr;
    if (isOpenGL)
    {
        captured_surface = rr_context_surface_lock(context3D);
        source = captured_surface;
    }
#else
    if (isOpenGL)
        source = gles_surface;
#endif
    if (!source)
    {
        logger.log(Logger::ERR, "Screenshot capture failed: video surface is not available.");
        screenshot_requested = false;
        return;
    }

    w = rr_surface_width_get(source);
    h = rr_surface_height_get(source);
    rr_surface_t *screenshot = rr_surface_create(display, w, h, RR_PIXEL_FORMAT_RGB888);
    if (!screenshot)
    {
        logger.log(Logger::ERR, "rr_surface_create for screenshot failed.");
#ifdef RR_PLATFORM_SDL
        if (captured_surface)
            rr_context_surface_unlock(context3D, captured_surface);
#endif
        screenshot_requested = false;
        return;
    }

    rr_surface_blit(source,
                     0, 0, w, h,
                     screenshot,
                     0, 0, w, h,
                     getBlitRotation());
#ifdef RR_PLATFORM_SDL
    if (captured_surface)
        rr_context_surface_unlock(context3D, captured_surface);
#endif

    // snap in screenshot directory
    std::string fullPath = screenShotFolder + "/" + romName + "-" + getCurrentTimeForFileName() + ".png";
    rr_surface_save_as_png(screenshot, fullPath.c_str());
    logger.log(Logger::DEB, "Screenshot saved:'%s'\n", fullPath.c_str());
    rr_surface_destroy(screenshot);
    screenshot_requested = false;
    flash = true;
    t_flash_start = std::chrono::high_resolution_clock::now();
}



void makeScreenBlackCredits(rr_surface_t *rr_surface, int res_width, int res_height)
{
    // bool specialCase = (isJaguar() || isBeetleVB() || isDosBox() || isDosCore() || isMame());
    //  res_width = specialCase? res_width * 2 : res_width; // just to be sure to cover the full screen (in some emulators is not enough to use res_width)
    uint8_t *dst = (uint8_t *)rr_surface_map(rr_surface);
    if (dst == nullptr)
    {
        return;
    }

    /* int lineWidth = 2;
     int lineSpacing = 14; // spacing between the two lines

     // Calculate the x-coordinates of the lines
     int lineX1 = res_width - lineWidth - lineSpacing - 20;
     int lineX2 = res_width - lineWidth - lineSpacing * 2 - 30;
     int lineX3 = res_width - lineSpacing - 2;*/

    int yy = res_height;
    while (yy > 0)
    {
        for (int x = 0; x < (short)res_width * 2; ++x)
        {
            if (false) //(x < 30 || x > res_width * 2 - 30)
            {
                int newColor = ((colorInc + x + yy) % 16) + 160; // >255 ;
                dst[x] = newColor >= 255 ? 0 : newColor;         // 240; // white color for the lines
            }
            else
            {
                dst[x] = 0x000000; // black color for the rest of the screen
            }
        }
        dst += rr_surface_stride_get(rr_surface);
        --yy;
    }
    colorInc++;
}

void drawMenuInfoBackgroud(rr_surface_t *rr_surface, int res_width, int res_height)
{
    // this was intended to make the screen black but it contained an error, but I resued because it draws the menu context well
    uint8_t *dst = (uint8_t *)rr_surface_map(rr_surface);
    if (dst == nullptr)
    {
        return;
    }
    int yy = res_height;

   

   
    while (yy > 0)
    {

        for (int x = 0; x < (short)res_width * 2; ++x)
        {

            int color = 74; // 162;

            if (yy < 12)
            {
                dst[x] = color; // 33; // 42;
            }
            else if (yy > res_height - 12)
            {
                dst[x] = color; // 33;
            }
            else if (x < 2 || x >= (short)res_width * 2 - 2)
            {
                dst[x] = color; // 33; // set to any color you want
            }
            else
            {

                dst[x] = 0x000000;
            }
        }
        dst += rr_surface_stride_get(rr_surface);
        --yy;
    }

    // col_increase++;
}

void makeScreenBlack(rr_surface_t *rr_surface, int res_width, int res_height)
{
    uint16_t *dst = (uint16_t *)rr_surface_map(rr_surface);
    if (dst == nullptr)
        return;

    int stride = rr_surface_stride_get(rr_surface) / sizeof(uint16_t);

    for (int y = 0; y < res_height; ++y)
    {
        for (int x = 0; x < res_width; ++x)
        {
            dst[y * stride + x] = 0x0000; // Nero in RGB565
        }
    }
}


void makeScreenTotalBlack(rr_surface_t *rr_surface, int res_width, int res_height)
{
    // res_width = (isJaguar() || isBeetleVB() || isDosBox() || isDosCore() || isMame()) ? res_width * 2 : res_width; // just to be sure to cover the full screen (in some emulators is not enough to use res_width)
    uint8_t *dst = (uint8_t *)rr_surface_map(rr_surface);
    if (dst == nullptr)
    {
        return;
    }
    int yy = res_height;
    while (yy > 0)
    {

        for (int x = 0; x < (short)res_width * 2; ++x)
        {

            dst[x] = 0x000000;
        }
        dst += rr_surface_stride_get(rr_surface);
        --yy;
    }
}


bool continueToShowScreenshotImage()
{
    gettimeofday(&valTime2, NULL);
    double currentTime = valTime2.tv_sec + (valTime2.tv_usec / 1000000.0);
    double elapsed = currentTime - lastScreenhotrequestTime;
    if (elapsed < 2)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool continueToShowSaveLoadStateImage(){
    gettimeofday(&valTime2, NULL);
    double currentTime = valTime2.tv_sec + (valTime2.tv_usec / 1000000.0);
    double elapsed = currentTime - lastLoadSaveStateRequestTime;
    if (elapsed < 2)
    {
        return true;
    }
    else
    {
        return false;
    }
}


bool continueToShowSaveLoadStateDoneImage(){
    gettimeofday(&valTime2, NULL);
    double currentTime = valTime2.tv_sec + (valTime2.tv_usec / 1000000.0);
    double elapsed = currentTime - lastLoadSaveStateDoneTime;
    if (elapsed < 6)
    {
        return true;
    }
    else
    {
        input_slot_memory_load_done =false;
        input_slot_memory_save_done =false;
        input_slot_memory_reset_done =false;
        return false;
    }
}

void checkPaused()
{
    if (drmDirectScanoutDiagnosticActive)
    {
        pause_requested = false;
        return;
    }
    if (input_pause_requested || input_info_requested)
    {
        pause_requested = true;
    }
    else
    {
        pause_requested = false;
    }
}
