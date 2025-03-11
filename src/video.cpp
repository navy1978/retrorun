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

#include "imgs/imgs_press.h"
#include "imgs/imgs_numbers.h"
#include "imgs/imgs_pause.h"
#include "imgs/imgs_screenshot.h"
#include "imgs/imgs_fast_forwarding.h"
#include "video-helper.h"

#include <chrono>
#include <thread>



#define ALIGN(val, align) (((val) + (align)-1) & ~((align)-1))

// extern float opt_aspect;
extern int opt_backlight;



status *status_obj = new status(); // quit, pause, screenshot,FPS, fastforward

// float aspect_ratio;
uint32_t color_format;


int GLContextMajor = 0;
int GLContextMinor = 0;

int hasStencil = false;


int prevBacklight;
// bool isTate = false;


bool isWideScreen = false;
extern retro_hw_context_reset_t retro_context_reset;


const char *batteryStateDesc[] = {"UNK", "DSC", "CHG", "FUL"};
extern go2_brightness_state_t brightnessState;
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
go2_rotation_t _351BlitRotation;
go2_rotation_t _351Rotation;
go2_rotation last351Rotation;
go2_rotation last351BlitRotation;
bool drawOneFrame;






void video_configure(struct retro_game_geometry *geom)
{
    logger.log(Logger::DEB, "Cofiguring video...");
    if (isPPSSPP() && geom->base_height == 0)
    {
        // for PPSSPP is possible to receive geom with 0 values
        // in this case we need to set the resolution manually
        geom->base_height = 272;
        geom->base_width = 480;
        geom->max_height = 272;
        geom->max_width = 480;
    }

    if (isRG503())// || isRG353V() || isRG353M())
    {
        /*geom->base_height = 544;
        geom->base_width = 960;
        geom->max_height = 544;
        geom->max_width = 960;*/
        display = go2_display_create();
        display_width = go2_display_width_get(display);
        display_height = go2_display_height_get(display);
    }else {
        display = go2_display_create();
        display_width = go2_display_height_get(display);
        display_height = go2_display_width_get(display);
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
    // Display info: width=480, height=320
    /*if (display_width == 480 && display_height == 320)
    {
        logger.log(Logger::DEB, "Display info: RG351-P / RG351-M");
        
    }
    else if (display_width == 480 && display_height == 640)
    {
        logger.log(Logger::DEB, "Display info: RG351-V / RG351-MP");
        
    }
    else if (display_width == 1920 && display_height == 1152)
    {
        logger.log(Logger::DEB, "Display info: RG552");
        
    }
    else if (display_width == 544 && display_height == 960)
    {
        logger.log(Logger::DEB, "Display info: RG503");
        
    } else if (display_width == 960 && display_height == 544)
    {
        logger.log(Logger::DEB, "Display info: RG353-V / RG353-M");
        
    }

    // width=544, height=960
    else
    {
        logger.log(Logger::WARN, "Display info: unknown! display_width:%d, display_height:%d\n", display_width, display_height);

        
    }*/
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
        go2_context_attributes_t attr;
        if (color_format == DRM_FORMAT_XRGB8888 && !(isRG503()||isRG353V() ||isRG353M())) // should be always true
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

   

        context3D = go2_context_create(display, getGeom_max_width(geom), getGeom_max_height(geom), &attr);
        go2_context_make_current(context3D);
        retro_context_reset();
    }
    else
    {
        if (surface)
            exit(1);

        int aw = ALIGN(getGeom_max_width(geom), 32);
        int ah = ALIGN(getGeom_max_height(geom), 32);
        logger.log(Logger::DEB, "video_configure: aw=%d, ah=%d", aw, ah);
        logger.log(Logger::DEB, "video_configure: base_width=%d, base_height=%d", geom->base_width, geom->base_height);

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
            logger.log(Logger::ERR, "go2_surface_create failed.\n");
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
    return 0;
}







void prepareScreen(int width, int height)
{

    bool wideScreenNotRotated= isRG503();
    screen_aspect_ratio = (float)go2_display_height_get(display) / (float)go2_display_width_get(display);
    if (isDuckStation())
    {
        // for DuckStation we need to invert the width and the height
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

    if (game_aspect_ratio >= 1.0f)
    {
        logger.log(Logger::DEB, "game is landscape");
        isGameVertical = false;
        if (isWideScreen)
        {
            logger.log(Logger::DEB, "device is widescreen");

            if (isTate())
            {
                logger.log(Logger::DEB, "Tate mode active");
                x = 0;
                y = 0;
                h = go2_display_height_get(display);
                w = go2_display_width_get(display);
            }
            else
            {
                logger.log(Logger::DEB, "Tate mode not active");
                if (cmpf(aspect_ratio, screen_aspect_ratio))
                {
                    logger.log(Logger::DEB, "aspect_ratio = screen_aspect_ratio");
                    h = go2_display_height_get(display);
                    w = go2_display_width_get(display);
                    x = 0;
                    y = 0;
                }
                else if (aspect_ratio < screen_aspect_ratio)
                {
                    logger.log(Logger::DEB, "aspect_ratio < screen_aspect_ratio");
                    w = go2_display_width_get(display);
                    h = w * aspect_ratio;
                    h = (h > go2_display_height_get(display)) ? go2_display_height_get(display) : h;
                    y = (go2_display_height_get(display) / 2) - (h / 2);
                    x = 0;
                }
                else if (aspect_ratio > screen_aspect_ratio)
                {
                    logger.log(Logger::DEB, "aspect_ratio > screen_aspect_ratio");
                    h = go2_display_height_get(display);
                    if (wideScreenNotRotated){
                        w = h * aspect_ratio;
                    }else{
                        w = h / aspect_ratio;
                    }
                    w = (w > go2_display_width_get(display)) ? go2_display_width_get(display) : w;
                    x = (go2_display_width_get(display) / 2) - (w / 2);
                    y = 0;
                    
                }
            }
        }
        else
        {
            logger.log(Logger::DEB, "screen is NOT widescreen");
            
            screen_aspect_ratio = 1 / screen_aspect_ratio; // screen is rotated

            if (cmpf(aspect_ratio, screen_aspect_ratio))
            {
                logger.log(Logger::DEB, "aspect_ratio = screen_aspect_ratio");
                h = go2_display_height_get(display);
                w = go2_display_width_get(display);
                x = 0;
                y = 0;
            }
            else if (aspect_ratio < screen_aspect_ratio)
            {
                logger.log(Logger::DEB, "aspect_ratio < screen_aspect_ratio");
                h = go2_display_height_get(display);
                w = h / aspect_ratio;
                w = (w > go2_display_width_get(display)) ? go2_display_width_get(display) : w;
                x = (go2_display_width_get(display) / 2) - (w / 2);
                y = 0;
            }
            else if (aspect_ratio > screen_aspect_ratio)
            {
                logger.log(Logger::DEB, "aspect_ratio > screen_aspect_ratio");
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
        // the game is vertical
        isGameVertical = true;
        logger.log(Logger::DEB, "game is portrait (vertical)");
        if (isTate())
        {
            logger.log(Logger::DEB, "Tate mode is active");
            x = 0;
            y = 0;
            h = go2_display_height_get(display);
            w = go2_display_width_get(display);
        }
        else
        {
            logger.log(Logger::DEB, "Tate mode is NOT active");
            if (aspect_ratio < screen_aspect_ratio)
            {
                logger.log(Logger::DEB, "aspect_ratio < screen_aspect_ratio");
                w = go2_display_width_get(display);
                h = w / aspect_ratio;
                h = (h > go2_display_height_get(display)) ? go2_display_height_get(display) : h;
                y = (go2_display_height_get(display) / 2) - (h / 2);
                x = 0;
            }
            else if (aspect_ratio > screen_aspect_ratio)
            {
                logger.log(Logger::DEB, "aspect_ratio > screen_aspect_ratio");
                h = go2_display_height_get(display);
                w = h / aspect_ratio;
                w = (w > go2_display_width_get(display)) ? go2_display_width_get(display) : w;
                x = (go2_display_width_get(display) / 2) - (w / 2);
                y = 0;
            }
        }
    }
}


inline void presenter_post(int width, int height)
{
    go2_presenter_post(presenter,
                       gles_surface,
                       0, (gs_h - height), width, height,
                       x, y, w, h,
                       getRotation());
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
/*
bool lastWasInfo = false;
bool cleanUpScreen = false;
*/
bool last_input_info_requested=false;
int maxFrameBlack= 15;
int clearScreenDelay =maxFrameBlack;
bool osdDrawing(const void *data, unsigned width, unsigned height, size_t pitch)
{

    bool showStatus = false;
    int res_width = width;
    int res_height = height;
    if (input_info_requested || input_credits_requested || last_input_info_requested)
    {
        
        // printf("cleanUpScreen:%s\n", cleanUpScreen ? "true" : "false");
        res_width = INFO_MENU_WIDTH;
        res_height = INFO_MENU_HEIGHT;

        if (status_surface_full == nullptr)
        {
            status_surface_full = go2_surface_create(display, res_width, res_height, format_565);
        }

        if (input_credits_requested )
        {
            
            makeScreenBlackCredits(status_surface_full, res_width, res_height);
            showCredits(&status_surface_full);
        }if (last_input_info_requested && !input_info_requested && clearScreenDelay>0) // Se il menu si chiude
        {
            logger.log(Logger::DEB, "Ensuring full screen clear before resuming game...");
            makeScreenBlackCredits(status_surface_full, res_width, res_height);
            clearScreenDelay--;
            if (clearScreenDelay==0){
                clearScreenDelay=maxFrameBlack;
            }
        }
        else
        {

            // if (!cleanUpScreen)
            // {
            makeScreenBlack(status_surface_full, res_width, res_height);
            showInfo(gs_w, &status_surface_full);

            /*}
            else
            {
                printf("Devo fare tutto neor!!!!\n");


               makeScreenTotalBlack(status_surface_full, res_width, res_height);
            }*/
        }
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
    if (input_fps_requested && !input_info_requested && !input_credits_requested)
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
    if (screenshot_requested && !input_info_requested && !input_credits_requested)
    {
        takeScreenshot(res_width, res_height);
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
    if (input_ffwd_requested || input_message)
    {
        if (input_message)
        {
            showText(10, 10, status_message.c_str(), 0xffff, &status_surface_top_left);
            showStatus = true;
            status_obj->show_top_left = true;
        }
        else
        {
            showImage(fast, &status_surface_top_left);

            showStatus = true;
            status_obj->show_top_left = true;
        }
    }

    else
    {
        status_obj->show_top_left = false;
    }
    if (input_exit_requested_firstTime && !input_info_requested && !input_credits_requested)
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
    if (!input_exit_requested_firstTime && !input_pause_requested && !input_credits_requested)
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

            // blit_surface_status(presenter,status_obj->full, dstSurface, dstWidth, dstHeight,rotation, FULL);
            /* if (cleanUpScreen){

                  go2_presenter_black(presenter,
                            x, y, width, height,
                            _351Rotation);
             }else{*/
            go2_presenter_post_multiple(presenter,
                                        gles_surface, status_obj,
                                        0, (gs_h - height), width, height,
                                        x, y, w, h,
                                        getRotation(), getBlitRotation(), isWideScreen);

            //}
        }
        else
        {

            go2_presenter_post_multiple(presenter,
                                        surface, status_obj,
                                        0, 0, res_width, res_height,
                                        x, y, w, h,
                                        getRotation(), getBlitRotation(), isWideScreen);
        }
    }
     // Update status for the ext frame
     last_input_info_requested = input_info_requested ? true : clearScreenDelay==maxFrameBlack ? false : true;
    return showStatus;
}

inline void core_video_refresh_NON_OPENGL(const void *data, unsigned width, unsigned height, size_t pitch)
{

    if (!data)
    {
        if (!input_info_requested )
        {
            logger.log(Logger::DEB, "DATA NOT VALID - skipping frame.");
            core_input_poll();
            return;
        }
    }else{
        if (firstTimeCorrectFrame){
            logger.log(Logger::DEB, "Loading finished!");
            firstTimeCorrectFrame=false;
        }
    }
    gs_w = go2_surface_width_get(surface);
    gs_h = go2_surface_height_get(surface);

    bool showStatus = osdDrawing(data, width, height, pitch);
    // printf("showStatus %s\n:", showStatus ? "true" : "false");
    if (!showStatus)
    {
        go2_presenter_post(presenter,
                           surface,
                           0, 0, width, height,
                           x, y, w, h,
                           getRotation());
    }
}

inline void core_video_refresh_OPENGL(const void *data, unsigned width, unsigned height, size_t pitch)
{

    // eglSwapInterval(display, 0);
    if (data != RETRO_HW_FRAME_BUFFER_VALID)
    {
        if (!input_info_requested)
        {
            logger.log(Logger::DEB, "RETRO HW FRAME BUFFER NOT VALID - skipping frame.");
            core_input_poll();
            return;
        }
    }else{
        if (firstTimeCorrectFrame){
            logger.log(Logger::DEB, "Loading finished!");
            firstTimeCorrectFrame=false;
        }
    }

    
    gs_w = go2_surface_width_get(gles_surface);
    gs_h = go2_surface_height_get(gles_surface);

    bool showStatus = osdDrawing(data, width, height, pitch);

    if (!showStatus)
    {
        go2_presenter_post(presenter,
                           gles_surface,
                           0, (gs_h - height), width, height,
                           x, y, w, h,
                           getRotation());
    }
}


const void *lastData;
size_t lastPitch;
void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{

    

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
        processVideoInAnotherThread = (isRG552() /*|| isRG503()*/) ? true : false;

        if (isPPSSPP() && width < 1)
        {
            // for PPSSPP is possible to receive  with with 0 values
            // in this case we need to set the resolution manually
            width = 480;
            height = 272;
        }
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

        logger.log(Logger::DEB, "Real aspect_ratio=%f", aspect_ratio);
        logger.log(Logger::DEB, "Screen aspect_ratio=%f\n", screen_aspect_ratio);
        logger.log(Logger::DEB, "Drawing info: w=%d, h=%d, x=%d, y=%d\n", w, h, x, y);
        logger.log(Logger::DEB, "OpenGL=%s", isOpenGL ? "true" : "false");
        logger.log(Logger::DEB, "isTate=%s", isTate() ? "true" : "false");

        if (color_format == DRM_FORMAT_RGBA5551)
        {
            logger.log(Logger::DEB, "Color format:DRM_FORMAT_RGBA5551");
        }
        else if (color_format == DRM_FORMAT_RGB888)
        {
            logger.log(Logger::DEB, "Color format:DRM_FORMAT_RGB888");
        }
        else if (color_format == DRM_FORMAT_XRGB8888)
        {
            logger.log(Logger::DEB, "Color format:DRM_FORMAT_XRGB8888");
        }
        else
        {
            logger.log(Logger::WARN, "Color format:Unknown");
        }

        real_aspect_ratio = aspect_ratio;
        _351BlitRotation = getBlitRotation();
        _351Rotation = getRotation();
        last351Rotation = _351Rotation;
        last351BlitRotation = _351BlitRotation;
        first_video_refresh = false;
    }
    if (height != currentHeight || width != currentWidth)
    {
        logger.log(Logger::DEB, "Resolution switched to width=%d, height=%d", width, height);
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
