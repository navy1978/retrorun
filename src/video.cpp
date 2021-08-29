/*
retrorun-go2 - libretro frontend for the ODROID-GO Advance
Copyright (C) 2020  OtherCrashOverride

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

#include <cmath>

#include <go2/display.h>

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

#define FBO_DIRECT 1
#define ALIGN(val, align) (((val) + (align)-1) & ~((align)-1))

//extern float opt_aspect;
extern int opt_backlight;

go2_display_t *display;
go2_surface_t *surface;
go2_surface_t *status_surface = NULL;
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
int prevBacklight;
bool isTate = false;
int display_width, display_height;
int base_width, base_height;
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
enum Device
{
    P_M,
    V,
    UNKNOWN
};
Device device = UNKNOWN;

void video_configure(const struct retro_game_geometry *geom)
{

    display = go2_display_create();
    display_width = go2_display_height_get(display);
    display_height = go2_display_width_get(display);

    // old
    //presenter = go2_presenter_create(display, DRM_FORMAT_XRGB8888, 0xff080808);  // ABGR
    // new
    presenter = go2_presenter_create(display, DRM_FORMAT_RGB565, 0xff080808); // ABGR

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
        printf("Using original game aspect ratio.\n");
        aspect_ratio = geom->aspect_ratio; // dont print the value here because is wrong
    }
    else
    {
        printf("Forcing aspect ratio to: %f.\n", opt_aspect);
        aspect_ratio = opt_aspect;
    }

    printf("-- Display info: width=%d, height=%d\n", display_width, display_height);
    //Display info: width=480, height=320
    if (display_width == 480 && display_height == 320)
    {
        printf("-- Device info: RG351 P/M\n");
        device = P_M;
    }
    else if (display_width == 480 && display_height == 640)
    {
        printf("-- Device info: RG351 V\n");
        device = V;
    }
    else
    {
        printf("-- Device info: unknown! V\n");
        device = UNKNOWN;
    }
    printf("-- Game info: base_width=%d, base_height=%d, max_width=%d, max_height=%d\n", geom->base_width, geom->base_height, geom->max_width, geom->max_height);
    base_width = geom->base_width;
    base_height = geom->base_height;

    float aspect_ratio_display = (float)display_width / (float)display_height;
    if (aspect_ratio_display > 1)
    {
        isWideScreen = true;
    }
    printf("-- Are we on wide screen? %s\n", isWideScreen == true ? "true" : "false");

    if (isOpenGL)
    {
        go2_context_attributes_t attr;
        attr.major = 3;
        attr.minor = 2;
        attr.red_bits = 5;
        attr.green_bits = 6;
        attr.blue_bits = 5;
        attr.alpha_bits = 0;
        attr.depth_bits = 24;
        attr.stencil_bits = 8;
        if (isWideScreen)
        {
            context3D = go2_context_create(display, geom->base_width, geom->base_height, &attr);
        }
        else
        {
            context3D = go2_context_create(display, geom->max_width, geom->max_height, &attr);
        }
        go2_context_make_current(context3D);
        retro_context_reset();

        // printf("geom->base_width>%d, geom->base_height:%d, display_width:%d display_height:%d \n", geom->base_width, geom->base_height, display_width, display_height);
        status_surface = go2_surface_create(display, geom->base_width, geom->base_height, DRM_FORMAT_RGB565);
        // status_surface = go2_surface_create(display, display_width, display_height, DRM_FORMAT_RGB565);
        if (!status_surface)
        {
            printf("go2_surface_create failed.:status_surface\n");
            throw std::exception();
        }
    }
    else
    {
        if (surface)
            abort();

        int aw = ALIGN(geom->max_width, 32);
        int ah = ALIGN(geom->max_height, 32);
        printf("video_configure: aw=%d, ah=%d\n", aw, ah);

        if (color_format == DRM_FORMAT_RGBA5551)
        {
            surface = go2_surface_create(display, aw, ah, DRM_FORMAT_RGB565);
        }
        else
        {
            surface = go2_surface_create(display, aw, ah, color_format);
        }

        if (!surface)
        {
            printf("go2_surface_create failed.\n");
            throw std::exception();
        }

        if (color_format == DRM_FORMAT_RGBA5551)
        {
            status_surface = go2_surface_create(display, geom->base_width, geom->base_height, DRM_FORMAT_RGB565);
        }
        else
        {
            status_surface = go2_surface_create(display, geom->base_width, geom->base_height, color_format);
        }

        if (!status_surface)
        {
            printf("go2_surface_create failed.:status_surface\n");
            throw std::exception();
        }
        //printf("video_configure: rect=%d, %d, %d, %d\n", y, x, h, w);
    }
}

void video_deinit()
{
    if (status_surface != NULL)
        go2_surface_destroy(status_surface);
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

void showText(int x, int y, const char *text)
{

    uint8_t *dst = (uint8_t *)go2_surface_map(status_surface);
    int dst_stride = go2_surface_stride_get(status_surface);
    basic_text_out16(dst, dst_stride / 2, x, y, text);
}

void showInfo(int w)
{
    // batteryState.level, batteryStateDesc[batteryState.status]
    showText(0, 0, "Retrorun (RG351* version)");
    showText(0, 10, "Release: 1.1.2");
    std::string res = "Resolution:";
    showText(0, 20, const_cast<char *>(res.append(std::to_string(base_width)).append("x").append(std::to_string(base_height)).c_str()));
    std::string bat = "Battery:";
    showText(0, 30, const_cast<char *>(bat.append(std::to_string(batteryState.level)).append("%%").c_str()));
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

// it simulates a flash for some milliseconds to give the user the impression the screenhsot has been taken
void flashEffect()
{
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end - t_flash_start).count();
    if (elapsed_time_ms > 1000)
    {
        flash = false;
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void showNumberSprite(int x, int y, int number, int width, int height, const uint8_t *src)
{
    int height_sprite = height / 10; //10 are the total number of sprites present in the image
    int src_stride = width * sizeof(short);
    uint8_t *dst = (uint8_t *)go2_surface_map(status_surface);
    int dst_stride = go2_surface_stride_get(status_surface);
    int brightnessIndex = number;
    src += (brightnessIndex * height_sprite * src_stride); //18
    dst += x * sizeof(short) + y * dst_stride;
    for (int y = 0; y < height_sprite; ++y) //16
    {
        memcpy(dst, src, width * sizeof(short));
        src += src_stride;
        dst += dst_stride;
    }
}

int getDigit(int n, int position)
{
    int res = (int)(n / pow(10, (position - 1))) % 10;
    if (res > 9)
        res = 9;
    if (res < 0)
        res = 0;
    return res;
}

void showFPSImage()
{

    if (base_width == 640 || base_height == 640)
    {
        int x = base_width - (numbers_image_high.width * 2) - 10; //depends on the width of the image
        int y = 10;
        showNumberSprite(x, y, getDigit(fps, 2), numbers_image_high.width, numbers_image_high.height, numbers_image_high.pixel_data);
        showNumberSprite(x + numbers_image_high.width, y, getDigit(fps, 1), numbers_image_high.width, numbers_image_high.height, numbers_image_high.pixel_data);
    }
    else
    {
        int x = base_width - (numbers_image_low.width * 2) - 10; //depends on the width of the image
        int y = 10;
        showNumberSprite(x, y, getDigit(fps, 2), numbers_image_low.width, numbers_image_low.height, numbers_image_low.pixel_data);
        showNumberSprite(x + numbers_image_low.width, y, getDigit(fps, 1), numbers_image_low.width, numbers_image_low.height, numbers_image_low.pixel_data);
    }
}

void showFullImage(int x, int y, int width, int height, const uint8_t *src)
{
    y = y - height;

    int src_stride = width * sizeof(short);
    uint8_t *dst = (uint8_t *)go2_surface_map(status_surface);
    int dst_stride = go2_surface_stride_get(status_surface);
    src += 0;
    dst += x * sizeof(short) + y * dst_stride;
    for (int y = 0; y < height; ++y)
    {
        memcpy(dst, src, width * sizeof(short));
        src += src_stride;
        dst += dst_stride;
    }
}

void showQuitImage()
{
    int x, y;
    if (base_width == 640 || base_height == 640)
    {
        x = 0;
        y = base_height - press_high.height / 2;
        showFullImage(x, y, press_high.width, press_high.height, press_high.pixel_data);
    }
    else
    {
        x = 0;
        y = base_height - press_low.height / 2;
        showFullImage(x, y, press_low.width, press_low.height, press_low.pixel_data);
    }
}

void takeScreenshot(int ss_w, int ss_h, go2_rotation_t _351BlitRotation)
{
    printf("Screenshot.\n");
    go2_surface_t *screenshot = go2_surface_create(display, ss_w, ss_h, DRM_FORMAT_RGB888);
    if (!screenshot)
    {
        printf("go2_surface_create for screenshot failed.\n");
        throw std::exception();
    }
    go2_surface_blit(status_surface,
                     0, 0, ss_w, ss_h,
                     screenshot,
                     0, 0, ss_w, ss_h,
                     _351BlitRotation);

    // snap in screenshot directory
    std::string fullPath = screenShotFolder + "/" + romName + "-" + getCurrentTimeForFileName() + ".png";
    go2_surface_save_as_png(screenshot, fullPath.c_str());
    printf("Screenshot saved:'%s'\n", fullPath.c_str());
    go2_surface_destroy(screenshot);
    screenshot_requested = false;
    flash = true;
    t_flash_start = std::chrono::high_resolution_clock::now();
}

void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{

    int gs_w;
    int gs_h;

    int x;
    int y;
    int w;
    int h;
    if (aspect_ratio >= 1.0f)
    {
        if (isWideScreen)
        {
            w = go2_display_width_get(display);
            h = w * aspect_ratio;
            h = (h > go2_display_height_get(display)) ? go2_display_height_get(display) : h;
            y = (go2_display_height_get(display) / 2) - (h / 2);
            x = 0;
        }
        else
        {
            w = go2_display_width_get(display);
            h = w / aspect_ratio;
            h = (h > go2_display_height_get(display)) ? go2_display_height_get(display) : h;
            y = (go2_display_height_get(display) / 2) - (h / 2);
            x = 0;
        }
    }
    else
    {
        x = 0;
        y = 0;
        h = go2_display_height_get(display);
        w = go2_display_width_get(display);
        isTate = true;
    }
    if (first_video_refresh)
    {
        printf("-- Real aspect_ratio=%f\n", aspect_ratio);
        printf("-- Drawing info: w=%d, h=%d, x=%d, y=%d\n", w, h, x, y);
        printf("-- OpenGL=%s\n", isOpenGL ? "true" : "false");
        real_aspect_ratio = aspect_ratio;
        first_video_refresh = false;
    }

    go2_rotation_t _351BlitRotation = isTate ? GO2_ROTATION_DEGREES_270 : GO2_ROTATION_DEGREES_0;
    go2_rotation_t _351Rotation = isTate ? GO2_ROTATION_DEGREES_180 : GO2_ROTATION_DEGREES_270;
    if (isOpenGL)
    {
        if (data != RETRO_HW_FRAME_BUFFER_VALID)
            return;

        /*if (!isWideScreen)
    {  //on V tate games should be rotated on the opposide side
        _351BlitRotation = GO2_ROTATION_DEGREES_270;
        _351Rotation = GO2_ROTATION_DEGREES_90;
        
    }*/

        // Swap
        go2_context_swap_buffers(context3D);

        gles_surface = go2_context_surface_lock(context3D);
        //get some util info
        gs_w = go2_surface_width_get(gles_surface);
        gs_h = go2_surface_height_get(gles_surface);
        int ss_w = go2_surface_width_get(status_surface);
        int ss_h = go2_surface_height_get(status_surface);

        go2_context_surface_unlock(context3D, gles_surface);

        // let's copy the content of gles_surface on status_surface (with the current roration based on the device)

        if (isWideScreen)
        {

            go2_surface_blit(gles_surface,
                             0, 0, gs_w, gs_h,
                             status_surface,
                             0, 0, ss_w, ss_h,
                             _351BlitRotation);
        }
        else
        {
            go2_surface_blit(gles_surface,
                             0, gs_h - height, width, height,
                             status_surface,
                             0, 0, ss_w, ss_h,
                             _351BlitRotation);
        }

        // screenshot requested
        if (screenshot_requested)
        {
            takeScreenshot(ss_w, ss_h, _351BlitRotation);
        }

        if (input_fps_requested)
        {
            showFPSImage();
        }
        if (input_info_requested)
        {
            showInfo(gs_w);
        }
        if (flash)
        {
            flashEffect();
        }
        if (input_exit_requested_firstTime)
        {
            showQuitImage();
        }

        // post the result on the presenter

        go2_presenter_post(presenter,
                           status_surface,
                           0, 0, ss_w, ss_h,
                           x, y, w, h,
                           _351Rotation);
    }
    else
    {
        if (!data)
            return;
        gs_w = go2_surface_width_get(surface);
        gs_h = go2_surface_height_get(surface);
        int ss_w = go2_surface_width_get(status_surface);
        int ss_h = go2_surface_height_get(status_surface);

        if (isWideScreen)
        {

            go2_surface_blit(surface,
                             0, 0, gs_w, gs_h,
                             status_surface,
                             0, 0, ss_w, ss_h,
                             _351BlitRotation);
        }
        else
        {
            go2_surface_blit(surface,
                             0, gs_h - height, width, height,
                             status_surface,
                             0, 0, ss_w, ss_h,
                             _351BlitRotation);
        }

        uint8_t *src = (uint8_t *)data;
        uint8_t *dst = (uint8_t *)go2_surface_map(status_surface);
        int bpp = go2_drm_format_get_bpp(go2_surface_format_get(status_surface)) / 8;

        int yy = height;
        while (yy > 0)
        {
            if (color_format == DRM_FORMAT_RGBA5551)
            {
                // uint16_t* src2 = (uint16_t*)src;
                // uint16_t* dst2 = (uint16_t*)dst;

                uint32_t *src2 = (uint32_t *)src;
                uint32_t *dst2 = (uint32_t *)dst;

                for (int x = 0; x < width / 2; ++x)
                {
                    // uint16_t pixel = src2[x];
                    // pixel = (pixel << 1) & (~0x1f) | pixel & 0x1f;
                    // dst2[x] = pixel;

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
            dst += go2_surface_stride_get(status_surface);

            --yy;
        }

        if (screenshot_requested)
        {
            takeScreenshot(ss_w, ss_h, _351BlitRotation);
        }

        if (input_fps_requested)
        {
            showFPSImage();
        }
        if (input_info_requested)
        {
            showInfo(gs_w);
        }
        if (flash)
        {
            flashEffect();
        }
        if (input_exit_requested_firstTime)
        {
            showQuitImage();
        }

        go2_presenter_post(presenter,
                           status_surface,
                           0, 0, ss_w, ss_h,
                           x, y, w, h,
                           _351Rotation);
    }
}