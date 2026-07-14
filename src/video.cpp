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
#include "video.h"
#include "ui-renderer.h"

#include "input.h"
#include "libretro.h"

#include <ctime>
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include <stdexcept>
#include <string.h>
#include <string>
#include <sys/time.h>

#include <cmath>

#include "platform.h"


#include <chrono>
#include "video-helper.h"

#include <chrono>
#include <thread>



#define ALIGN(val, align) (((val) + (align)-1) & ~((align)-1))

// extern float opt_aspect;
extern int opt_backlight;



// float aspect_ratio;
uint32_t color_format;


int GLContextMajor = 0;
int GLContextMinor = 0;

int hasStencil = false;


int prevBacklight;
// bool isTate = false;


bool isWideScreen = false;
extern retro_hw_context_reset_t retro_context_reset;
extern retro_hw_context_reset_t retro_context_destroy;
extern rr_video_shader_t videoShader;


const char *batteryStateDesc[] = {"UNK", "DSC", "CHG", "FUL"};
extern rr_brightness_state_t brightnessState;
bool first_video_refresh = true;
float real_aspect_ratio = 0.0f;




extern float fps;
extern int retrorunLoopSkip;
extern int retrorunLoopCounter;



bool turn = false;
// screen info
int gs_w;
int gs_h;

int x;
int y;
int w;
int h;
float screen_aspect_ratio;
rr_rotation_t _351BlitRotation;
rr_rotation_t _351Rotation;
rr_rotation_t last351Rotation;
rr_rotation_t last351BlitRotation;
bool drawOneFrame;






void video_configure(struct retro_game_geometry *geom)
{
    logger.log(Logger::DEB, "Configuring video...");
    if (isPPSSPP() && geom->base_height == 0)
    {
        // for PPSSPP is possible to receive geom with 0 values
        // in this case we need to set the resolution manually
        geom->base_height = 272;
        geom->base_width = 480;
        geom->max_height = 272;
        geom->max_width = 480;
    }

    if (isRG503()
#ifdef RR_PLATFORM_SDL
        || true
#endif
    )// || isRG353V() || isRG353M())
    {
        /*geom->base_height = 544;
        geom->base_width = 960;
        geom->max_height = 544;
        geom->max_width = 960;*/
        display = rr_display_create();
        display_width = rr_display_width_get(display);
        display_height = rr_display_height_get(display);
    }else {
        display = rr_display_create();
        display_width = rr_display_height_get(display);
        display_height = rr_display_width_get(display);
    }

    float aspect_ratio_display = (float)display_width / (float)display_height;
    if (aspect_ratio_display > 1)
    {
        isWideScreen = true;
    }
    logger.log(Logger::DEB, "Are we on wide screen? %s", isWideScreen == true ? "true" : "false");

    if (isDuckStation())
    {
        // for DuckStation we need to invert the width and the height
        geom->max_width = display_height;
        geom->max_height = display_width;
    }

#ifndef RR_PLATFORM_SDL
    presenter = rr_presenter_create(display, RR_PIXEL_FORMAT_RGB888, 0xff080808); // ABGR
#endif

    if (opt_backlight > -1)
    {
        rr_display_backlight_set(display, (uint32_t)opt_backlight);
    }
    else
    {
        opt_backlight = rr_display_backlight_get(display);
    }
    prevBacklight = opt_backlight;

    if (opt_aspect == 0.0f)
    {
        logger.log(Logger::DEB, "Using original game aspect ratio.");
        aspect_ratio = geom->aspect_ratio; // dont print the value here because is wrong
        // for PC games (the default apsect ratio should be 4:3)
        if (isDosBox())
        {
            logger.log(Logger::DEB, "Dosbox default apsect ratio 4/3.");
            aspect_ratio = 1.333333f;
        }
    }
    else
    {
        logger.log(Logger::DEB, "Forcing aspect ratio to: %f.", opt_aspect);
        aspect_ratio = opt_aspect;
    }
    game_aspect_ratio = geom->aspect_ratio;
    logger.log(Logger::DEB, "Display info: width=%d, height=%d", display_width, display_height);
    
    // some games like Resident Evil 2 for Flycast has an ovescan issue in 640x480
    bool skipGeomSet = ((isFlycast() || isFlycast2021()) && isRG552());

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

    logger.log(Logger::DEB, "Game info: base_width=%d, base_height=%d, max_width=%d, max_height=%d", geom->base_width, geom->base_height, geom->max_width, geom->max_height);

    base_width = geom->base_width;
    base_height = geom->base_height;
    max_width = geom->max_width;
    max_height = geom->max_height;

    if (isOpenGL)
    {
        rr_context_attributes_t attr;
        if (color_format == RR_PIXEL_FORMAT_XRGB8888 && !(isRG503()||isRG353V() ||isRG353M())) // should be always true
        {
#ifdef RR_SDL_GLES
            // GLES3 cores commonly leave version_major/minor at zero. Request
            // the widely supported ES 3.0 baseline unless a core explicitly
            // asks for a newer GLES_VERSION context.
            attr.major = GLContextMajor >= 3 ? GLContextMajor : 3;
            attr.minor = GLContextMajor >= 3 ? GLContextMinor : 0;
#elif defined(RR_PLATFORM_SDL)
            attr.major = GLContextMajor > 0 ? GLContextMajor : 3;
            attr.minor = GLContextMajor > 0 ? GLContextMinor : 2;
#else
            attr.major = 3;
            attr.minor = 2;
#endif
            attr.red_bits = 8;
            attr.green_bits = 8;
            attr.blue_bits = 8;
            attr.alpha_bits = 8;
            attr.depth_bits = 24;
            attr.stencil_bits = 8;
        }
        else
        {
#ifdef RR_SDL_GLES
            attr.major = GLContextMajor >= 3 ? GLContextMajor : 3;
            attr.minor = GLContextMajor >= 3 ? GLContextMinor : 0;
#else
            attr.major = 3;
            attr.minor = 2;
#endif
            attr.red_bits = 5;
            attr.green_bits = 6;
            attr.blue_bits = 5;
            attr.alpha_bits = 0;
            attr.depth_bits = 24;
            attr.stencil_bits = 8;
        }

   

        context3D = rr_context_create(display, getGeom_max_width(geom), getGeom_max_height(geom), &attr);
        if (!context3D)
        {
            logger.log(Logger::ERR, "Unable to create the requested hardware rendering context");
            throw std::runtime_error("hardware rendering context creation failed");
        }
        rr_context_make_current(context3D);
        retro_context_reset();
    }
    else
    {
#ifdef RR_PLATFORM_SDL
        presenter = rr_presenter_create(display, RR_PIXEL_FORMAT_RGB888, 0xff080808);
#endif
        if (surface)
            exit(1);

        int aw = ALIGN(getGeom_max_width(geom), 32);
        int ah = ALIGN(getGeom_max_height(geom), 32);
        logger.log(Logger::DEB, "video_configure: aw=%d, ah=%d", aw, ah);
        logger.log(Logger::DEB, "video_configure: base_width=%d, base_height=%d", geom->base_width, geom->base_height);

        if (color_format == RR_PIXEL_FORMAT_RGBA5551)
        {
            surface = rr_surface_create(display, aw, ah, format_565);
        }
        else
        {
            surface = rr_surface_create(display, aw, ah, color_format);
        }

        if (!surface)
        {
            logger.log(Logger::ERR, "rr_surface_create failed.\n");
            throw std::exception();
        }

        logger.log(Logger::DEB, "video_configured: rect=%d, %d, %d, %d\n", y, x, h, w);
    }
}

void video_prepare_core_unload()
{
#ifdef RR_PLATFORM_SDL
    if (isOpenGL && context3D != NULL && retro_context_destroy != NULL)
    {
        rr_context_make_current(context3D);
        retro_context_destroy();
        retro_context_destroy = NULL;
    }
#endif
}

void video_deinit()
{

    if (status_surface_bottom_right != NULL)
        rr_surface_destroy(status_surface_bottom_right);
    if (status_surface_bottom_left != NULL)
        rr_surface_destroy(status_surface_bottom_left);
    if (status_surface_bottom_center != NULL)
        rr_surface_destroy(status_surface_bottom_center);
    if (status_surface_top_right != NULL)
        rr_surface_destroy(status_surface_top_right);
    if (status_surface_top_left != NULL)
        rr_surface_destroy(status_surface_top_left);
    if (status_surface_full != NULL)
        rr_surface_destroy(status_surface_full);
    if (surface != NULL)
        rr_surface_destroy(surface);
    if (context3D != NULL)
        rr_context_destroy(context3D);
    if (presenter != NULL)
        rr_presenter_destroy(presenter);
    if (display != NULL)
        rr_display_destroy(display);
}

uintptr_t core_video_get_current_framebuffer()
{
    return rr_context_framebuffer_get(context3D);
}


bool pixel_perfect_max_scaling = true;

void prepareScreenPixelPerfect(int width, int height) {

    int display_height = rr_display_height_get(display);
    int display_width = rr_display_width_get(display);

    int scaled_w = width;
    int scaled_h = height;

    if (pixel_perfect_max_scaling ) {
        int scale_x = display_width / width;
        int scale_y = display_height / height;
        int scale = (scale_x < scale_y) ? scale_x : scale_y;
        if (scale < 1) scale = 1;

        scaled_w = width * scale;
        scaled_h = height * scale;
    }

    x = (display_width - scaled_w) / 2;
    y = (display_height - scaled_h) / 2;
    w = scaled_w;
    h = scaled_h;

    
#ifndef RR_PLATFORM_SDL
    if (isWideScreen && !isRG503()) {
        x = (display_width - scaled_h) / 2;
        y = (display_height - scaled_w) / 2;
        w = scaled_h;
        h = scaled_w;
    }
#endif
    

    if (first_video_refresh) {
        logger.log(Logger::DEB,
            "Pixel perfect mode enabled: max_scaling=%d, x=%d, y=%d, w=%d, h=%d",
            pixel_perfect_max_scaling, x, y, w, h);
    }
}


void prepareScreen(int width, int height)
{
   
    
   
#ifdef RR_PLATFORM_SDL
        screen_aspect_ratio = (float)rr_display_width_get(display) / (float)rr_display_height_get(display);
#else
        screen_aspect_ratio = (float)rr_display_height_get(display) / (float)rr_display_width_get(display);
#endif
    
        if (isDuckStation() && !wideScreenNotRotated())
        {
            x = 0;
            y = 0;
            w = display_height;
            h = display_width;
            if (isWideScreen)
            {
                int temp = h;
                h = w * 4 / 3;
                y = (temp - h) / 2;
                x = 0;
            }
            return;
        }
    
        if (pixel_perfect)
        {
            prepareScreenPixelPerfect(width,height);
            return;
        }

#ifdef RR_PLATFORM_SDL
    // Desktop windows can change size at any time (maximize/fullscreen/drag).
    // Fit the core's display aspect ratio inside the current SDL window and
    // center it, leaving pillarbox or letterbox bars as required.
    if (!isTate())
    {
        const int window_width = rr_display_width_get(display);
        const int window_height = rr_display_height_get(display);
        const float target_aspect = aspect_ratio > 0.0f
                                        ? aspect_ratio
                                        : (height > 0 ? static_cast<float>(width) / height : 1.0f);
        const float window_aspect = static_cast<float>(window_width) / window_height;

        if (target_aspect < window_aspect)
        {
            h = window_height;
            w = static_cast<int>(std::lround(h * target_aspect));
            x = (window_width - w) / 2;
            y = 0;
        }
        else
        {
            w = window_width;
            h = static_cast<int>(std::lround(w / target_aspect));
            x = 0;
            y = (window_height - h) / 2;
        }
        return;
    }
#endif

    if (game_aspect_ratio >= 1.0f)
    {
        if (first_video_refresh)
        logger.log(Logger::DEB, "game is landscape");
        isGameVertical = false;
        if (isWideScreen)
        {
            if (first_video_refresh)
            logger.log(Logger::DEB, "device is widescreen");

            if (isTate())
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "Tate mode active");
                x = 0;
                y = 0;
                h = rr_display_height_get(display);
                w = rr_display_width_get(display);
            }
            else
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "Tate mode not active");
                if (cmpf(aspect_ratio, screen_aspect_ratio))
                {
                    if (first_video_refresh)
                    logger.log(Logger::DEB, "aspect_ratio = screen_aspect_ratio");
                    h = rr_display_height_get(display);
                    w = rr_display_width_get(display);
                    x = 0;
                    y = 0;
                }
                else if (aspect_ratio < screen_aspect_ratio)
                {
                    if (first_video_refresh)
                    logger.log(Logger::DEB, "aspect_ratio < screen_aspect_ratio");
                    w = rr_display_width_get(display);
                    h = w * aspect_ratio;
                    h = (h > rr_display_height_get(display)) ? rr_display_height_get(display) : h;
                    y = (rr_display_height_get(display) / 2) - (h / 2);
                    x = 0;
                }
                else if (aspect_ratio > screen_aspect_ratio)
                {
                    if (first_video_refresh)
                    logger.log(Logger::DEB, "aspect_ratio > screen_aspect_ratio");
                    h = rr_display_height_get(display);
                    if (wideScreenNotRotated()){
                        w = h * aspect_ratio;
                    }else{
                        w = h / aspect_ratio;
                    }
                    w = (w > rr_display_width_get(display)) ? rr_display_width_get(display) : w;
                    x = (rr_display_width_get(display) / 2) - (w / 2);
                    y = 0;
                    
                }
            }
        }
        else
        {
            if (first_video_refresh)
            logger.log(Logger::DEB, "screen is NOT widescreen");
            
            screen_aspect_ratio = 1 / screen_aspect_ratio; // screen is rotated

            if (cmpf(aspect_ratio, screen_aspect_ratio))
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "aspect_ratio = screen_aspect_ratio");
                h = rr_display_height_get(display);
                w = rr_display_width_get(display);
                x = 0;
                y = 0;
            }
            else if (aspect_ratio < screen_aspect_ratio)
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "aspect_ratio < screen_aspect_ratio");
                h = rr_display_height_get(display);
                w = h / aspect_ratio;
                w = (w > rr_display_width_get(display)) ? rr_display_width_get(display) : w;
                x = (rr_display_width_get(display) / 2) - (w / 2);
                y = 0;
            }
            else if (aspect_ratio > screen_aspect_ratio)
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "aspect_ratio > screen_aspect_ratio");
                w = rr_display_width_get(display);
                h = w / aspect_ratio;
                h = (h > rr_display_height_get(display)) ? rr_display_height_get(display) : h;
                y = (rr_display_height_get(display) / 2) - (h / 2);
                x = 0;
            }
        }
    }
    else
    {
        // the game is vertical
        isGameVertical = true;
        if (first_video_refresh)
        logger.log(Logger::DEB, "game is portrait (vertical)");
        if (isTate())
        {
            if (first_video_refresh)
            logger.log(Logger::DEB, "Tate mode is active");
            x = 0;
            y = 0;
            h = rr_display_height_get(display);
            w = rr_display_width_get(display);
        }
        else
        {
            if (first_video_refresh)
            logger.log(Logger::DEB, "Tate mode is NOT active");
            if (aspect_ratio < screen_aspect_ratio)
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "aspect_ratio < screen_aspect_ratio");
                w = rr_display_width_get(display);
                h = w / aspect_ratio;
                h = (h > rr_display_height_get(display)) ? rr_display_height_get(display) : h;
                y = (rr_display_height_get(display) / 2) - (h / 2);
                x = 0;
            }
            else if (aspect_ratio > screen_aspect_ratio)
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "aspect_ratio > screen_aspect_ratio");
                h = rr_display_height_get(display);
                w = h / aspect_ratio;
                w = (w > rr_display_width_get(display)) ? rr_display_width_get(display) : w;
                x = (rr_display_width_get(display) / 2) - (w / 2);
                y = 0;
            }
        }
    }
    if (first_video_refresh)
    logger.log(Logger::DEB, "Pixel perfect mode disabled: x=%d, y=%d, w=%d, h=%d", x, y, w, h);
}


inline void presenter_post(int width, int height)
{
    rr_presenter_post(presenter,
                       gles_surface,
                       0, (gs_h - height), width, height,
                       x, y, w, h,
                       getRotation());
}

void drawNonOpenGL(const void *data, unsigned width, unsigned height, size_t pitch)
{

    uint8_t *src = (uint8_t *)data;
    uint8_t *mapped = (uint8_t *)rr_surface_map(surface);
    uint8_t *dst = mapped;
    if (dst == nullptr)
    {
        return;
    }
    int bpp = rr_pixel_format_bpp(rr_surface_format_get(surface)) / 8;

    int yy = height;
    while (yy > 0)
    {
        if (color_format == RR_PIXEL_FORMAT_RGBA5551)
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
        dst += rr_surface_stride_get(surface);
        --yy;
    }

    if (videoShader != RR_VIDEO_SHADER_OFF)
    {
        const uint32_t format = rr_surface_format_get(surface);
        const int stride = rr_surface_stride_get(surface);
        const bool crt = videoShader == RR_VIDEO_SHADER_CRT;
        // Rebuild the effect map only when geometry or mode changes. Q8 gains
        // remove floating point, division and vignette math from every frame.
        static std::vector<uint16_t> effect_gain;
        static std::vector<uint8_t> grille_mask;
        static unsigned cached_width = 0;
        static unsigned cached_height = 0;
        static bool cached_crt = false;
        if (cached_width != width || cached_height != height ||
            cached_crt != crt || effect_gain.size() != width * height)
        {
            effect_gain.resize(static_cast<size_t>(width) * height);
            grille_mask.resize(width);
            for (unsigned px = 0; px < width; ++px)
                grille_mask[px] = static_cast<uint8_t>(px % 3U);
            for (unsigned py = 0; py < height; ++py)
            {
                const float normalized_y = height > 1
                    ? (2.0f * py / static_cast<float>(height - 1)) - 1.0f : 0.0f;
                const float scanline = (py & 1U) ? 0.68f : 1.0f;
                for (unsigned px = 0; px < width; ++px)
                {
                    float factor = scanline;
                    if (crt)
                    {
                        const float normalized_x = width > 1
                            ? (2.0f * px / static_cast<float>(width - 1)) - 1.0f : 0.0f;
                        factor *= std::max(0.62f,
                            1.0f - 0.18f * (normalized_x * normalized_x +
                                            normalized_y * normalized_y));
                    }
                    effect_gain[static_cast<size_t>(py) * width + px] =
                        static_cast<uint16_t>(std::clamp(factor, 0.0f, 1.0f) * 256.0f);
                }
            }
            cached_width = width;
            cached_height = height;
            cached_crt = crt;
        }
        auto scale_channel = [](unsigned value, unsigned gain, unsigned maximum) {
            return std::min(maximum, (value * gain + 128U) >> 8U);
        };

        for (unsigned py = 0; py < height; ++py)
        {
            uint8_t *row = mapped + static_cast<size_t>(py) * stride;
            for (unsigned px = 0; px < width; ++px)
            {
                const unsigned gain = effect_gain[static_cast<size_t>(py) * width + px];
                const unsigned dim_gain = (gain * 225U) >> 8U;
                const unsigned mask = grille_mask[px];

                if (format == RR_PIXEL_FORMAT_RGB565)
                {
                    uint16_t &pixel = reinterpret_cast<uint16_t *>(row)[px];
                    unsigned red = (pixel >> 11) & 0x1f;
                    unsigned green = (pixel >> 5) & 0x3f;
                    unsigned blue = pixel & 0x1f;
                    if (crt) {
                        red = scale_channel(red, mask == 0 ? gain : dim_gain, 0x1f);
                        green = scale_channel(green, mask == 1 ? gain : dim_gain, 0x3f);
                        blue = scale_channel(blue, mask == 2 ? gain : dim_gain, 0x1f);
                    } else {
                        red = scale_channel(red, gain, 0x1f);
                        green = scale_channel(green, gain, 0x3f);
                        blue = scale_channel(blue, gain, 0x1f);
                    }
                    pixel = static_cast<uint16_t>((red << 11) | (green << 5) | blue);
                }
                else if (format == RR_PIXEL_FORMAT_XRGB8888 ||
                         format == RR_PIXEL_FORMAT_RGBA8888)
                {
                    uint32_t &pixel = reinterpret_cast<uint32_t *>(row)[px];
                    unsigned red = (pixel >> 16) & 0xff;
                    unsigned green = (pixel >> 8) & 0xff;
                    unsigned blue = pixel & 0xff;
                    red = scale_channel(red, crt && mask != 0 ? dim_gain : gain, 0xff);
                    green = scale_channel(green, crt && mask != 1 ? dim_gain : gain, 0xff);
                    blue = scale_channel(blue, crt && mask != 2 ? dim_gain : gain, 0xff);
                    pixel = (pixel & 0xff000000U) | (red << 16) | (green << 8) | blue;
                }
                else if (format == RR_PIXEL_FORMAT_RGB888)
                {
                    uint8_t *pixel = row + px * 3;
                    pixel[0] = static_cast<uint8_t>(scale_channel(pixel[0], crt && mask != 0 ? dim_gain : gain, 0xff));
                    pixel[1] = static_cast<uint8_t>(scale_channel(pixel[1], crt && mask != 1 ? dim_gain : gain, 0xff));
                    pixel[2] = static_cast<uint8_t>(scale_channel(pixel[2], crt && mask != 2 ? dim_gain : gain, 0xff));
                }
            }
        }
    }
    rr_surface_unmap(surface);
}
int timeCorrectFrame =0;

static void finishLoadingScreenWhenReady(const char *frame_type)
{
    if (!showLoading || timeCorrectFrame <= 2 || !uiLoadingMinimumDurationElapsed())
        return;

    logger.log(Logger::DEB,
               "Loading screen: disabled after %d valid %s frames and at least 700 ms",
               timeCorrectFrame, frame_type);
    showLoading = false;
    twiceTimeCorrectFrame = false;
}

inline void core_video_refresh_NON_OPENGL(const void *data, unsigned width, unsigned height, size_t pitch)
{
    static std::vector<uint16_t> black_frame;
    if (!data)
    {
        if (!input_info_requested )
        {
            logger.log(Logger::DEB, "DATA NOT VALID - skipping frame.");
            core_input_poll();
            //return;
            
        }
        if (black_frame.empty() && showLoading ) {
            if (width==0 || height==0 ){
                logger.log(Logger::DEB, "Setting a valid size for width and height");
                width=320;
                height=200;
            }
            if (pitch == 0) {
                pitch = width * sizeof(uint16_t);  // fallback for RGB565
            }
            
            size_t num_pixels = (pitch / sizeof(uint16_t)) * height;
            black_frame.resize(num_pixels);
            memset(black_frame.data(), 0x00, num_pixels * sizeof(uint16_t)); // RGB565 black
            data = black_frame.data();
        }
        if (!showLoading){
            return; // if the data is not valid and we are not loading the game then we need to dont draw this frame
        }
        


    }else{
        timeCorrectFrame++;
        finishLoadingScreenWhenReady("video");
    }
    gs_w = rr_surface_width_get(surface);
    gs_h = rr_surface_height_get(surface);

    bool showStatus = uiRenderOverlays(data, width, height, pitch);
    // printf("showStatus %s\n:", showStatus ? "true" : "false");
    if (!showStatus)
    {
        rr_presenter_post(presenter,
                           surface,
                           0, 0, width, height,
                           x, y, w, h,
                           getRotation());
    }
}

inline void core_video_refresh_OPENGL(const void *data, unsigned width, unsigned height, size_t pitch)
{
    static std::vector<uint16_t> black_frame;
    // eglSwapInterval(display, 0);
    if (data != RETRO_HW_FRAME_BUFFER_VALID)
    {
        if (!input_info_requested )
        {
            logger.log(Logger::DEB, "DATA NOT VALID - skipping frame.");
            core_input_poll();
            //return;
            
        }
        if (black_frame.empty() && showLoading ) {
            if (width==0 || height==0 ){
                logger.log(Logger::DEB, "Setting a valid size for width and height");
                width=320;
                height=200;
            }
            if (pitch == 0) {
                pitch = width * sizeof(uint16_t);  // fallback for RGB565
            }
            
            size_t num_pixels = (pitch / sizeof(uint16_t)) * height;
            black_frame.resize(num_pixels);
            memset(black_frame.data(), 0x00, num_pixels * sizeof(uint16_t)); // RGB565 black
            data = black_frame.data();
        }
        if (!showLoading){
            return; // if the data is not valid and we are not loading the game then we need to dont draw this frame
        }
    }else{
        timeCorrectFrame++;
        finishLoadingScreenWhenReady("OpenGL");
    }

    
    gs_w = rr_surface_width_get(gles_surface);
    gs_h = rr_surface_height_get(gles_surface);

    bool showStatus = uiRenderOverlays(data, width, height, pitch);

    if (!showStatus)
    {
        rr_presenter_post(presenter,
                           gles_surface,
                           0, (gs_h - height), width, height,
                           x, y, w, h,
                           getRotation());
    }
}


const void *lastData;
size_t lastPitch;
//bool lastPixelPerfect= pixel_perfect;
void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{

#ifndef RR_PLATFORM_SDL
    // Fixed presentation-only frameskip, equivalent to the strategy used by
    // many emulators: value 1 presents one frame out of two, value 2 one out
    // of three, and so on. The display keeps scanning out the last submitted
    // framebuffer, while retro_run(), audio and input continue normally.
    static int fixedFramesRemaining = 0;
    const bool forcePresentation = showLoading || input_info_requested ||
        input_message || input_credits_requested || screenshot_requested ||
        input_ffwd_requested;

    if (fixedFrameSkip <= 0 || forcePresentation)
    {
        fixedFramesRemaining = 0;
    }
    else if (fixedFramesRemaining > 0)
    {
        --fixedFramesRemaining;
        return;
    }
    else
    {
        fixedFramesRemaining = fixedFrameSkip;
    }
#endif

  
    if (input_info_requested)
    {
        width = currentWidth;
        height = currentHeight;
        data = lastData;
        pitch = lastPitch;
        processVideoInAnotherThread = false;
    }
    else if (input_message)
    {
        width = INFO_MENU_WIDTH;
        height = INFO_MENU_HEIGHT;
        processVideoInAnotherThread = false;
    }
    else
    {

        lastData = data;
        lastPitch = pitch;
        // Preserve the RG552/Flycast2021 performance workaround and keep an
        // explicit override for other combinations.
        processVideoInAnotherThread =
            (isRG552() && isFlycast2021()) || forceVideoMultithread;

        if (isPPSSPP() && width < 1)
        {
            // for PPSSPP is possible to receive  with with 0 values
            // in this case we need to set the resolution manually
            width = 480;
            height = 272;
        }
    }

#ifndef RR_PLATFORM_SDL
    if (skipNextVideoFrame && !showLoading && !input_info_requested &&
        !input_message && !input_ffwd_requested)
    {
        skipNextVideoFrame = false;
        return;
    }
#endif

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

   /* if (true )
    {*/
        
        prepareScreen(width, height);
        if (first_video_refresh){
        logger.log(Logger::DEB, "Real aspect_ratio=%f", aspect_ratio);
        logger.log(Logger::DEB, "Screen aspect_ratio=%f\n", screen_aspect_ratio);
        logger.log(Logger::DEB, "Drawing info: w=%d, h=%d, x=%d, y=%d\n", w, h, x, y);
        logger.log(Logger::DEB, "OpenGL=%s", isOpenGL ? "true" : "false");
        logger.log(Logger::DEB, "isTate=%s", isTate() ? "true" : "false");

        if (color_format == RR_PIXEL_FORMAT_RGBA5551)
        {
            logger.log(Logger::DEB, "Color format:RR_PIXEL_FORMAT_RGBA5551");
        }
        else if (color_format == RR_PIXEL_FORMAT_RGB888)
        {
            logger.log(Logger::DEB, "Color format:RR_PIXEL_FORMAT_RGB888");
        }
        else if (color_format == RR_PIXEL_FORMAT_XRGB8888)
        {
            logger.log(Logger::DEB, "Color format:RR_PIXEL_FORMAT_XRGB8888");
        }
        else
        {
            logger.log(Logger::WARN, "Color format:Unknown");
        }
    }

        real_aspect_ratio = aspect_ratio;
        _351BlitRotation = getBlitRotation();
        _351Rotation = getRotation();
        last351Rotation = _351Rotation;
        last351BlitRotation = _351BlitRotation;
        first_video_refresh = false;
    //}

    if (first_video_refresh){
        first_video_refresh = false;
    }
   /* if ( lastPixelPerfect!=pixel_perfect){
        printf("Settgin screen!\n");
        prepareScreen(width, height);
    }
    if (!input_info_requested){
    lastPixelPerfect = pixel_perfect;
    }*/
   // printf("lastPixelPerfect:%s\n",lastPixelPerfect? "true":"false");
    if (height != currentHeight || width != currentWidth)
    {
        logger.log(Logger::DEB, "Resolution switched to width=%d, height=%d", width, height);
        currentWidth = width;
        currentHeight = height;
    }

    if (isOpenGL)
    {
#ifdef RR_PLATFORM_SDL
        // Hardware cores signal a completed frame with this libretro sentinel.
        // The GO2 path handled it in core_video_refresh_OPENGL(), while the
        // direct SDL presentation path used to bypass that loading-state code.
        if (data == RETRO_HW_FRAME_BUFFER_VALID)
        {
            ++timeCorrectFrame;
            finishLoadingScreenWhenReady("OpenGL");
        }
        const bool overlays_visible = uiRenderOverlays(data, width, height, pitch);
        rr_context_swap_buffers(context3D, width, height, x, y, w, h,
                                overlays_visible ? uiCurrentOverlays() : nullptr,
                                getRotation());
        if (overlays_visible && showLoading)
            uiNotifyLoadingPresented();
        return;
#else
        rr_context_swap_buffers(context3D, width, height, x, y, w, h, nullptr,
                                getRotation());

        gles_surface = rr_context_surface_lock(context3D);
        if (processVideoInAnotherThread)
        {
            std::thread th(core_video_refresh_OPENGL, data, width, height, pitch);
            th.detach();
        }
        else
        {
            core_video_refresh_OPENGL(data, width, height, pitch);
        }

        rr_context_surface_unlock(context3D, gles_surface);
#endif
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
