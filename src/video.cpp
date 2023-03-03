/*
retrorun-go2 - libretro frontend for the ODROID-GO Advance
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
#include "video.h"

#include "input.h"
#include "libretro.h"

#include <ctime>
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include <string.h>
#include <string>
#include <sys/time.h>

#include <cmath>

#include "go2/display.h"
#include "go2/struct.h"

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <drm/drm_fourcc.h>
#include <map>
#include "fonts.h"

#include <chrono>

#include "imgs_press.h"
#include "imgs_numbers.h"
#include "imgs_pause.h"
#include "imgs_screenshot.h"
#include <chrono>
#include <thread>

#define FBO_DIRECT 1
#define ALIGN(val, align) (((val) + (align)-1) & ~((align)-1))

// extern float opt_aspect;
extern int opt_backlight;

go2_display_t *display;
go2_surface_t *surface;
go2_surface_t *status_surface_bottom_right = NULL;
go2_surface_t *status_surface_bottom_left = NULL;
go2_surface_t *status_surface_top_right = NULL;
go2_surface_t *status_surface_top_left = NULL;
go2_surface_t *status_surface_full = NULL;

status *status_obj = new status(); // quit, pause, screenshot,FPS, fastforward
go2_surface_t *display_surface;
go2_frame_buffer_t *frame_buffer;
go2_presenter_t *presenter;
go2_context_t *context3D;
// float aspect_ratio;
uint32_t color_format;

bool isOpenGL = false;
int GLContextMajor = 0;
int GLContextMinor = 0;
GLuint fbo;
int hasStencil = false;
bool screenshot_requested = false;
bool pause_requested = false;
int prevBacklight;
bool isTate = false;
int display_width, display_height;
int base_width, base_height, max_width, max_height;
int aw, ah;
go2_surface_t *gles_surface;
bool isWideScreen = false;
extern retro_hw_context_reset_t retro_context_reset;
auto t_flash_start = std::chrono::high_resolution_clock::now();
bool flash = false;
extern go2_battery_state_t batteryState;
const char *batteryStateDesc[] = {"UNK", "DSC", "CHG", "FUL"};
bool first_video_refresh = true;
float real_aspect_ratio = 0.0f;
unsigned currentWidth = 0;
unsigned currentHeight = 0;

int rowForText = 0;

extern float fps;
extern int retrorunLoopSkip;
extern int retrorunLoopCounter;

struct timeval valTime2;

bool turn = false;
// screen info
int gs_w;
int gs_h;

int x;
int y;
int w;
int h;
float screen_aspect_ratio;
go2_rotation_t _351BlitRotation;
go2_rotation_t _351Rotation;

int width_fixed = 640;
int height_fixed = 480;

int INFO_MENU_WIDTH =240;//288;
int INFO_MENU_HEIGHT= 160;//192;

uint32_t format_565 = DRM_FORMAT_RGB565; // DRM_FORMAT_RGB888; // DRM_FORMAT_XRGB8888;//color_format;

int getFixedWidth(int alternative)
{
    // some games like Resident Evil 2 for Flycast has an ovescan issue in 640x480
    if (isFlycast())
    {
        if (resolution == R_320_240)
            return 320;
        else if (resolution == R_640_480 && device == RG_552)
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
    if (isFlycast())
    {
        if (resolution == R_320_240)
            return 240;
        else if (resolution == R_640_480 && device == RG_552)
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
    return getFixedWidth(max_width);
}

int getGeom_max_height(const struct retro_game_geometry *geom)
{
    return getFixedHeight(max_height);
}

void video_configure(struct retro_game_geometry *geom)
{

    display = go2_display_create();
    display_width = go2_display_height_get(display);
    display_height = go2_display_width_get(display);

    presenter = go2_presenter_create(display, DRM_FORMAT_RGB888, 0xff080808); // ABGR

    if (opt_backlight > -1)
    {
        go2_display_backlight_set(display, (uint32_t)opt_backlight);
    }
    else
    {
        opt_backlight = go2_display_backlight_get(display);
    }
    prevBacklight = opt_backlight;

    if (opt_aspect == 0.0f)
    {
        printf("-RR- Using original game aspect ratio.\n");
        aspect_ratio = geom->aspect_ratio; // dont print the value here because is wrong
        // for PC games (the default apsect ratio should be 4:3)
        if (isDosBox())
        {
            printf("-RR- Dosbox default apsect ratio 4/3.\n");
            aspect_ratio = 1.333333f;
        }
    }
    else
    {
        printf("-RR- Forcing aspect ratio to: %f.\n", opt_aspect);
        aspect_ratio = opt_aspect;
    }

    printf("-RR- Display info: width=%d, height=%d\n", display_width, display_height);
    // Display info: width=480, height=320
    if (display_width == 480 && display_height == 320)
    {
        printf("-RR- Device info: RG351-P / RG351-M\n");
        device = P_M;
    }
    else if (display_width == 480 && display_height == 640)
    {
        printf("-RR- Device info: RG351-V / RG351-MP\n");
        device = V_MP;
    }
    else if (display_width == 1920 && display_height == 1152)
    {
        printf("-RR- Device info: RG552 \n");
        device = RG_552;
    }
    else
    {
        printf("-RR- Device info: unknown! display_width:%d, display_height:%d\n", display_width, display_height);
        device = UNKNOWN;
    }
    // some games like Resident Evil 2 for Flycast has an ovescan issue in 640x480
    bool skipGeomSet = (isFlycast() && device == RG_552);

    if (resolution == R_320_240)
    {
        geom->base_height = 240;
        geom->base_width = 320;
        geom->max_height = 240;
        geom->max_width = 320;
    }
    else if (resolution == R_640_480 && !skipGeomSet)
    {
        geom->base_height = 480;
        geom->base_width = 640;
        geom->max_height = 480;
        geom->max_width = 640;
    }

    printf("-RR- Game info: base_width=%d, base_height=%d, max_width=%d, max_height=%d\n", geom->base_width, geom->base_height, geom->max_width, geom->max_height);
    base_width = geom->base_width;
    base_height = geom->base_height;
    max_width = geom->max_width;
    max_height = geom->max_height;

    float aspect_ratio_display = (float)display_width / (float)display_height;
    if (aspect_ratio_display > 1)
    {
        isWideScreen = true;
    }
    printf("-RR- Are we on wide screen? %s\n", isWideScreen == true ? "true" : "false");

    if (isOpenGL)
    {
        go2_context_attributes_t attr;
        if (color_format == DRM_FORMAT_XRGB8888) // should be always true
        {
            attr.major = 3;
            attr.minor = 2;
            attr.red_bits = 8;
            attr.green_bits = 8;
            attr.blue_bits = 8;
            attr.alpha_bits = 8;
            attr.depth_bits = 24;
            attr.stencil_bits = 8;
        }
        else
        {
            attr.major = 3;
            attr.minor = 2;
            attr.red_bits = 5;
            attr.green_bits = 6;
            attr.blue_bits = 5;
            attr.alpha_bits = 0;
            attr.depth_bits = 24;
            attr.stencil_bits = 8;
        }

        /* attr.major = 3;
         attr.minor = 2;
         attr.red_bits = 8;
         attr.green_bits = 8;
         attr.blue_bits = 8;
         attr.alpha_bits = 8;
         attr.depth_bits = 16;
         attr.stencil_bits = 8;*/

        context3D = go2_context_create(display, getGeom_max_width(geom), getGeom_max_height(geom), &attr);
        go2_context_make_current(context3D);
        retro_context_reset();
    }
    else
    {
        if (surface)
            abort();

        int aw = ALIGN(getGeom_max_width(geom), 32);
        int ah = ALIGN(getGeom_max_height(geom), 32);
        printf("-RR- video_configure: aw=%d, ah=%d\n", aw, ah);
        printf("-RR- video_configure: base_width=%d, base_height=%d\n", geom->base_width, geom->base_height);

        if (color_format == DRM_FORMAT_RGBA5551)
        {
            surface = go2_surface_create(display, aw, ah, format_565);
        }
        else
        {
            surface = go2_surface_create(display, aw, ah, color_format);
        }

        if (!surface)
        {
            printf("-RR- go2_surface_create failed.\n");
            throw std::exception();
        }

        // printf("video_configure: rect=%d, %d, %d, %d\n", y, x, h, w);
    }
}

void video_deinit()
{

    if (status_surface_bottom_right != NULL)
        go2_surface_destroy(status_surface_bottom_right);
    if (status_surface_bottom_left != NULL)
        go2_surface_destroy(status_surface_bottom_left);
    if (status_surface_top_right != NULL)
        go2_surface_destroy(status_surface_top_right);
    if (status_surface_top_left != NULL)
        go2_surface_destroy(status_surface_top_left);
    if (status_surface_full != NULL)
        go2_surface_destroy(status_surface_full);
    if (surface != NULL)
        go2_surface_destroy(surface);
    if (context3D != NULL)
        go2_context_destroy(context3D);
    if (presenter != NULL)
        go2_presenter_destroy(presenter);
    if (display != NULL)
        go2_display_destroy(display);
}

uintptr_t core_video_get_current_framebuffer()
{

#ifndef FBO_DIRECT
    return fbo;
#else
    return 0;
#endif
}

inline void showText(int x, int y, const char *text, unsigned short color, go2_surface_t **surface)
{

    if (*surface == nullptr)
    {

        *surface = go2_surface_create(display, 200, 20, format_565);
    }

    uint8_t *dst = (uint8_t *)go2_surface_map(*surface);
    if (dst == nullptr)
    {
        return;
    }
    int dst_stride = go2_surface_stride_get(*surface);
    basic_text_out16_nf_color(dst, dst_stride / 2, x, y, text, color);
}

inline int getRowForText()
{
    rowForText = rowForText + 10;
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
int switchColor = 30;
int step = 1;
int posRetro = 3;
bool loop = true;
inline void showInfo(int w, go2_surface_t **surface)
{
     rowForText = 0;
    int posX = 0;

    // showText(posRetro, getRowForText(), ("▬▬ι═══════ﺤ Retrorun -═══════ι▬▬ " + release+ " (2023)").c_str(), YELLOW, surface);
    std::string title = "Retrorun - " + release;  // The text to scroll
    int title_length = title.length();
    //printf("title_length :%d\n",title_length);
    showText(posRetro, 0, title.c_str(), YELLOW, surface);
    //printf("pos:%d\n",posRetro);
    if (posRetro == (INFO_MENU_WIDTH - (title_length *8)))
    { // 240 is the width size of the info , 3 is the initial position
        step = -1;
        //printf("resetto a negativo");
    }else if (posRetro == 0)
    { // 240 is the width size of the info , 3 is the initial position
        step = 1;
        //printf("resetto positivo");
    }
    posRetro += step;
    showText(posX, getRowForText(), " ", ORANGE, surface);
    showText(posX, getRowForText(), " ", ORANGE, surface);
    showText(posX, getRowForText(), " ", ORANGE, surface);
    // const char* hostName= getEnv("HOSTNAME");
    std::string hostName(getDeviceName());
    hostName = stripReturnCarriage(hostName);
    showText(posX, getRowForText(), ("- Device: " + hostName).c_str(), DARKGREY, surface);
    std::string bat = "- Battery: ";
    showText(posX, getRowForText(), const_cast<char *>(bat.append(std::to_string(batteryState.level)).append("%").c_str()), DARKGREY, surface);

    std::string time = "- Time: ";
    time_t curr_time;
    tm *curr_tm;

    char time_string[100];

    std::time(&curr_time);
    curr_tm = localtime(&curr_time);

    strftime(time_string, 50, "%R", curr_tm);
    showText(posX, getRowForText(), const_cast<char *>(time.append(time_string).c_str()), DARKGREY, surface);

    std::string core = "- Core: ";
    showText(posX, getRowForText(), const_cast<char *>(core.append(coreName).c_str()), DARKGREY, surface);
    std::string openGl = "- OpenGL: ";
    if (isOpenGL)
    {
        showText(posX, getRowForText(), const_cast<char *>(openGl.append(isOpenGL ? "true" : "false").c_str()), DARKGREY, surface);
    }
    else
    {
        showText(posX, getRowForText(), const_cast<char *>(openGl.append(isOpenGL ? "true" : "false").c_str()), DARKGREY, surface);
    }

    std::string origFps = "- Orignal FPS (Game): ";
    showText(posX, getRowForText(), const_cast<char *>(origFps.append(std::to_string((int)originalFps)).c_str()), DARKGREY, surface);

    /*
        std::string res = "Resolution (base): ";
        showText(posX, getRowForText(), const_cast<char *>(res.append(std::to_string(base_width)).append("x").append(std::to_string(base_height)).c_str()), 0xffff);
        std::string res2 = "Resolution (int.): ";
        showText(posX, getRowForText(), const_cast<char *>(res2.append(std::to_string(currentWidth)).append("x").append(std::to_string(currentHeight)).c_str()), 0xffff);

        std::string displ = "Resolution (dis.): ";
        showText(posX, getRowForText(), const_cast<char *>(displ.append(std::to_string(w)).append("x").append(std::to_string(h)).c_str()), 0xffff);
    */

    std::string averageFps = "- Average FPS (Game): ";
    showText(posX, getRowForText(), const_cast<char *>(averageFps.append(std::to_string((int)avgFps)).c_str()), DARKGREY, surface);

    std::string res2 = "- Resolution (Game): ";
    showText(posX, getRowForText(), const_cast<char *>(res2.append(std::to_string(currentWidth)).append("x").append(std::to_string(currentHeight)).c_str()), DARKGREY, surface);

    showText(posX, getRowForText(), " ", 0xf800, surface);
    showText(posX, getRowForText(), " ", 0xf800, surface);
    showText(posX, getRowForText(), " ", 0xf800, surface);
    if (switchColor > 0)
    {
        showText(posX, getRowForText(), "   Press L3 + R3 to resume", DARKGREY, surface);
    }
    else
    {
        showText(posX, getRowForText(), "   Press L3 + R3 to resume", WHITE, surface);
        if (switchColor < -30)
        {
            switchColor = 30;
        }
    }
    switchColor--;
}

inline std::string getCurrentTimeForFileName()
{
    time_t t = time(0); // get time now
    struct tm *now = localtime(&t);
    char buffer[80];
    strftime(buffer, 80, "%y%m%d-%H%M%S", now);
    std::string str(buffer);
    return str;
}

inline void showNumberSprite(int x, int y, int number, int width, int height, const uint8_t *src)
{
    int height_sprite = height / 10; // 10 are the total number of sprites present in the image
    int src_stride = width * sizeof(short);
    uint8_t *dst = (uint8_t *)go2_surface_map(status_surface_top_right);
    if (dst == nullptr)
    {
        return;
    }
    int dst_stride = go2_surface_stride_get(status_surface_top_right);
    int brightnessIndex = number;
    src += (brightnessIndex * height_sprite * src_stride); // 18
    dst += x * sizeof(short) + y * dst_stride;
    for (int y = 0; y < height_sprite; ++y) // 16
    {
        memcpy(dst, src, width * sizeof(short));
        src += src_stride;
        dst += dst_stride;
    }
}

inline int getDigit(int n, int position)
{
    int res = (int)(n / pow(10, (position - 1))) % 10;
    if (res > 9)
        res = 9;
    if (res < 0)
        res = 0;
    return res;
}

inline int getWidthFPS()
{

    return go2_surface_width_get(status_surface_top_right);
}

inline void showFPSImage()
{
    int x = getWidthFPS() - (numbers.width * 2); // depends on the width of the image
    int y = 0;
    showNumberSprite(x, y, getDigit(fps, 2), numbers.width, numbers.height, numbers.pixel_data);
    showNumberSprite(x + numbers.width, y, getDigit(fps, 1), numbers.width, numbers.height, numbers.pixel_data);
}

inline void showFullImage(int x, int y, int width, int height, const uint8_t *src, go2_surface_t **surface)
{

    // create the different surfaces for the statues
    if (*surface == nullptr)
    {

        *surface = go2_surface_create(display, width, height, format_565);
    }
    int src_stride = width * sizeof(short);
    uint8_t *dst = (uint8_t *)go2_surface_map(*surface);
    if (dst == nullptr)
    {
        return;
    }
    int dst_stride = go2_surface_stride_get(*surface);
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

inline void showImage(Image img, go2_surface_t **surface)
{
    showFullImage(0, 0, img.width, img.height, img.pixel_data, surface);
}

inline void takeScreenshot(int w, int h, go2_rotation_t _351BlitRotation)
{
    printf("-RR- Screenshot.\n");
    w = isOpenGL ? gles_surface->width : surface->width;
    h = isOpenGL ? gles_surface->height : surface->height;
    go2_surface_t *screenshot = go2_surface_create(display, w, h, DRM_FORMAT_RGB888);
    if (!screenshot)
    {
        printf("-RR- go2_surface_create for screenshot failed.\n");
        throw std::exception();
    }

    go2_surface_blit(isOpenGL ? gles_surface : surface,
                     0, 0, w, h,
                     screenshot,
                     0, 0, w, h,
                     _351BlitRotation);

    // snap in screenshot directory
    std::string fullPath = screenShotFolder + "/" + romName + "-" + getCurrentTimeForFileName() + ".png";
    go2_surface_save_as_png(screenshot, fullPath.c_str());
    printf("-RR- Screenshot saved:'%s'\n", fullPath.c_str());
    go2_surface_destroy(screenshot);
    screenshot_requested = false;
    flash = true;
    t_flash_start = std::chrono::high_resolution_clock::now();
}

inline bool cmpf(float A, float B, float epsilon = 0.005f)
{
    return (fabs(A - B) < epsilon);
}

inline void prepareScreen(int width, int height)
{
    screen_aspect_ratio = (float)go2_display_height_get(display) / (float)go2_display_width_get(display);
    if (aspect_ratio >= 1.0f)
    {
        if (isWideScreen)
        {
            if (cmpf(aspect_ratio, screen_aspect_ratio))
            {
                h = go2_display_height_get(display);
                w = go2_display_width_get(display);
                x = 0;
                y = 0;
            }
            else if (aspect_ratio < screen_aspect_ratio)
            {
                w = go2_display_width_get(display);
                h = w * aspect_ratio;
                h = (h > go2_display_height_get(display)) ? go2_display_height_get(display) : h;
                y = (go2_display_height_get(display) / 2) - (h / 2);
                x = 0;
            }
            else if (aspect_ratio > screen_aspect_ratio)
            {
                h = go2_display_height_get(display);
                w = h / aspect_ratio;
                w = (w > go2_display_width_get(display)) ? go2_display_width_get(display) : w;
                x = (go2_display_width_get(display) / 2) - (w / 2);
                y = 0;
            }
        }
        else
        {
            screen_aspect_ratio = 1 / screen_aspect_ratio; // screen is rotated

            if (cmpf(aspect_ratio, screen_aspect_ratio))
            {
                h = go2_display_height_get(display);
                w = go2_display_width_get(display);
                x = 0;
                y = 0;
            }
            else if (aspect_ratio < screen_aspect_ratio)
            {
                h = go2_display_height_get(display);
                w = h / aspect_ratio;
                w = (w > go2_display_width_get(display)) ? go2_display_width_get(display) : w;
                x = (go2_display_width_get(display) / 2) - (w / 2);
                y = 0;
            }
            else if (aspect_ratio > screen_aspect_ratio)
            {
                w = go2_display_width_get(display);
                h = w / aspect_ratio;
                h = (h > go2_display_height_get(display)) ? go2_display_height_get(display) : h;
                y = (go2_display_height_get(display) / 2) - (h / 2);
                x = 0;
            }
        }
    }
    else
    {
        // tate mode
        x = 0;
        y = 0;
        h = go2_display_height_get(display);
        w = go2_display_width_get(display);
        isTate = (Retrorun_Core == RETRORUN_CORE_FLYCAST); // we rotate the screen (Tate) for some arcade games when apsect ratio < 0
    }
}

inline void makeScreenBlack(go2_surface_t *go2_surface, int res_width, int res_height)
{
    res_width = (isJaguar() || isBeetleVB() || isDosBox() || isDosCore() || isMame()) ? res_width * 2 : res_width; // just to be sure to cover the full screen (in some emulators is not enough to use res_width)
    uint8_t *dst = (uint8_t *)go2_surface_map(go2_surface);
    if (dst == nullptr)
    {
        return;
    }
    int yy = res_height;
    while (yy > 0)
    {
        if (color_format == DRM_FORMAT_RGBA5551)
        {
            uint32_t *dst2 = (uint32_t *)dst;
            for (int x = 0; x < (short)res_width / 2; ++x)
            {
                dst2[x] = 0x000000;
            }
        }
        for (int x = 0; x < (short)res_width * 2; ++x)
        {
            dst[x] = 0x000000;
        }
        dst += go2_surface_stride_get(go2_surface);
        --yy;
    }
}

inline bool continueToShowScreenshotImage()
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

inline void checkPaused()
{
    if (input_pause_requested || input_info_requested)
    {
        pause_requested = true;
    }
    else
    {
        pause_requested = false;
    }
}

inline void presenter_post(int width, int height)
{
    go2_presenter_post(presenter,
                       gles_surface,
                       0, (gs_h - height), width, height,
                       x, y, w, h,
                       _351Rotation);
}

void drawNonOpenGL(const void *data, unsigned width, unsigned height, size_t pitch)
{

    uint8_t *src = (uint8_t *)data;
    uint8_t *dst = (uint8_t *)go2_surface_map(surface);
    if (dst == nullptr)
    {
        return;
    }
    int bpp = go2_drm_format_get_bpp(go2_surface_format_get(surface)) / 8;

    int yy = height;
    while (yy > 0)
    {
        if (color_format == DRM_FORMAT_RGBA5551)
        {
            uint32_t *src2 = (uint32_t *)src;
            uint32_t *dst2 = (uint32_t *)dst;

            for (int x = 0; x < (short)width / 2; ++x)
            {
                uint32_t pixel = src2[x];
                pixel = ((pixel << 1) & (~0x3f003f)) | (pixel & 0x1f001f);
                dst2[x] = pixel;
            }
        }
        else
        {
            memcpy(dst, src, width * bpp);
        }

        src += pitch;
        dst += go2_surface_stride_get(surface);
        --yy;
    }
}

bool osdDrawing(const void *data, unsigned width, unsigned height, size_t pitch)
{

    bool showStatus = false;
    int res_width = width;
    int res_height = height;
    if (input_info_requested)
    {
        res_width =INFO_MENU_WIDTH;
        res_height = INFO_MENU_HEIGHT;

        if (status_surface_full == nullptr)
        {
            status_surface_full = go2_surface_create(display, res_width, res_height, format_565);
        }
        makeScreenBlack(status_surface_full, res_width, res_height);
        showInfo(gs_w, &status_surface_full);
        showStatus = true;
        status_obj->show_full = true;
    }
    else
    {
        status_obj->show_full = false;
        if (!isOpenGL)
        {
            drawNonOpenGL(data, width, height, pitch);
        }
    }
    if (input_fps_requested && !input_info_requested)
    {
        if (status_surface_top_right == nullptr)
        {
            status_surface_top_right = go2_surface_create(display, numbers.width * 2, (numbers.height / 10), format_565);
        }
        showFPSImage();
        showStatus = true;
        status_obj->show_top_right = true;
    }
    else
    {
        status_obj->show_top_right = false;
    }
    if (screenshot_requested && !input_info_requested)
    {
        takeScreenshot(res_width, res_height, _351BlitRotation);
    }
    if (continueToShowScreenshotImage())
    {
        showImage(screenshot, &status_surface_bottom_right);
        showStatus = true;
        status_obj->show_bottom_right = true;
    }
    else
    {
        status_obj->show_bottom_right = false;
    }
    if (input_ffwd_requested)
    {
        showText(10, 10, ">> Fast Forwarding >>", 0xf800, &status_surface_top_left);
        showStatus = true;
        status_obj->show_top_left = true;
    }
    else
    {
        status_obj->show_top_left = false;
    }
    if (input_exit_requested_firstTime && !input_info_requested)
    {
        showImage(quit, &status_surface_bottom_left);
        showStatus = true;
        status_obj->show_bottom_left = true;
    }
    if (input_pause_requested && !input_info_requested)
    {
        showImage(pause_img, &status_surface_bottom_left);
        showStatus = true;
        status_obj->show_bottom_left = true;
    }
    if (!input_exit_requested_firstTime && !input_pause_requested)
    {
        status_obj->show_bottom_left = false;
    }
    checkPaused();

    if (showStatus)
    {
        if (status_surface_bottom_left != nullptr)
        {
            status_obj->bottom_left = status_surface_bottom_left;
        }
        if (status_surface_bottom_right != nullptr)
        {
            status_obj->bottom_right = status_surface_bottom_right;
        }
        if (status_surface_top_right != nullptr)
        {
            status_obj->top_right = status_surface_top_right;
        }
        if (status_surface_top_left != nullptr)
        {
            status_obj->top_left = status_surface_top_left;
        }
        if (status_surface_full != nullptr)
        {
            status_obj->full = status_surface_full;
        }
        if (isOpenGL)
        {
            go2_presenter_post_multiple(presenter,
                                        gles_surface, status_obj,
                                        0, (gs_h - height), width, height,
                                        x, y, w, h,
                                        _351Rotation);
        }
        else
        {
            go2_presenter_post_multiple(presenter,
                                        surface, status_obj,
                                        0, 0, res_width, res_height,
                                        x, y, w, h,
                                        _351Rotation);
        }
    }
    return showStatus;
}

inline void core_video_refresh_NON_OPENGL(const void *data, unsigned width, unsigned height, size_t pitch)
{

    if (!data)
        return;

    gs_w = go2_surface_width_get(surface);
    gs_h = go2_surface_height_get(surface);

    bool showStatus = osdDrawing(data, width, height, pitch);

    if (!showStatus)
    {
        go2_presenter_post(presenter,
                           surface,
                           0, 0, width, height,
                           x, y, w, h,
                           _351Rotation);
    }
}

inline void core_video_refresh_OPENGL(const void *data, unsigned width, unsigned height, size_t pitch)
{

    // eglSwapInterval(display, 0);
    if (data != RETRO_HW_FRAME_BUFFER_VALID)
        return;

    /*if (!isWideScreen)
   {  //on V tate games should be rotated on the opposide side
       _351BlitRotation = GO2_ROTATION_DEGREES_270;
       _351Rotation = GO2_ROTATION_DEGREES_90;

   }*/

    // Swap

    //  go2_context_swap_buffers(context3D);

    //    gles_surface = go2_context_surface_lock(context3D);
    // get some util info
    gs_w = go2_surface_width_get(gles_surface);
    gs_h = go2_surface_height_get(gles_surface);

    bool showStatus = osdDrawing(data, width, height, pitch);

    if (!showStatus)
    {
        go2_presenter_post(presenter,
                           gles_surface,
                           0, (gs_h - height), width, height,
                           x, y, w, h,
                           _351Rotation);
    }
}

const void *lastData;
size_t lastPitch;
void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{

    // if (width==0 && height==0 && pitch==0 ){
    bool realPause = pause_requested && input_pause_requested;
    bool showInfo = pause_requested && input_info_requested;
    if (realPause || showInfo)
    {
        width = currentWidth;
        height = currentHeight;
        data = lastData;
        pitch = lastPitch;
        processVideoInAnotherThread = false;
    }
    else
    {
        lastData = data;
        lastPitch = pitch;
        processVideoInAnotherThread = isRG552() ? true : false;
    }
    frameCounter++;
    // the following is for Fast Forwarding
    if (frameCounter == frameCounterSkip)
    {
        frameCounter = 0;
    }
    else
    {
        if (input_ffwd_requested)
        {
            return;
        }
    }

    if (first_video_refresh)
    {
        prepareScreen(width, height);

        printf("-RR- Real aspect_ratio=%f\n", aspect_ratio);
        printf("-RR- Screen aspect_ratio=%f\n", screen_aspect_ratio);
        printf("-RR- Drawing info: w=%d, h=%d, x=%d, y=%d\n", w, h, x, y);
        printf("-RR- OpenGL=%s\n", isOpenGL ? "true" : "false");
        printf("-RR- isTate=%s\n", isTate ? "true" : "false");

        if (color_format == DRM_FORMAT_RGBA5551)
        {
            printf("-RR- Color format:DRM_FORMAT_RGBA5551\n");
        }
        else if (color_format == DRM_FORMAT_RGB888)
        {
            printf("-RR- Color format:DRM_FORMAT_RGB888\n");
        }
        else if (color_format == DRM_FORMAT_XRGB8888)
        {
            printf("-RR- Color format:DRM_FORMAT_XRGB8888\n");
        }
        else
        {
            printf("-RR- Color format:Unknown\n");
        }

        real_aspect_ratio = aspect_ratio;
        _351BlitRotation = isTate ? GO2_ROTATION_DEGREES_270 : GO2_ROTATION_DEGREES_0;
        _351Rotation = isTate ? GO2_ROTATION_DEGREES_180 : GO2_ROTATION_DEGREES_270;
        first_video_refresh = false;
    }
    if (height != currentHeight || width != currentWidth)
    {
        printf("-RR- Resolution switched to width=%d, height=%d\n", width, height);
        currentWidth = width;
        currentHeight = height;
    }

    if (isOpenGL)
    {

        go2_context_swap_buffers(context3D);

        gles_surface = go2_context_surface_lock(context3D);
        if (processVideoInAnotherThread)
        {
            std::thread th(core_video_refresh_OPENGL, data, width, height, pitch);
            th.detach();
        }
        else
        {
            core_video_refresh_OPENGL(data, width, height, pitch);
        }
        go2_context_surface_unlock(context3D, gles_surface);
    }
    else
    {

        // non-OpenGL

        if (processVideoInAnotherThread)
        {
            std::thread th(core_video_refresh_NON_OPENGL, data, width, height, pitch);
            th.detach();
        }
        else
        {
            core_video_refresh_NON_OPENGL(data, width, height, pitch);
        }
    }
}
