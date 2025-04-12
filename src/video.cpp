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

        logger.log(Logger::DEB, "video_configured: rect=%d, %d, %d, %d\n", y, x, h, w);
    }
}

void video_deinit()
{

    if (status_surface_bottom_right != NULL)
        go2_surface_destroy(status_surface_bottom_right);
    if (status_surface_bottom_left != NULL)
        go2_surface_destroy(status_surface_bottom_left);
    if (status_surface_bottom_center != NULL)
        go2_surface_destroy(status_surface_bottom_center);
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


bool pixel_perfect_max_scaling = true;

void prepareScreenPixelPerfect(int width, int height) {

    int display_height = go2_display_height_get(display);
    int display_width = go2_display_width_get(display);

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

    
    if (isWideScreen && !isRG503()) {
        x = (display_width - scaled_h) / 2;
        y = (display_height - scaled_w) / 2;
        w = scaled_h;
        h = scaled_w;
    }
    

    if (first_video_refresh) {
        logger.log(Logger::DEB,
            "Pixel perfect mode enabled: max_scaling=%d, x=%d, y=%d, w=%d, h=%d",
            pixel_perfect_max_scaling, x, y, w, h);
    }
}


void prepareScreen(int width, int height)
{
   
    
   
        screen_aspect_ratio = (float)go2_display_height_get(display) / (float)go2_display_width_get(display);
    
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
                h = go2_display_height_get(display);
                w = go2_display_width_get(display);
            }
            else
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "Tate mode not active");
                if (cmpf(aspect_ratio, screen_aspect_ratio))
                {
                    if (first_video_refresh)
                    logger.log(Logger::DEB, "aspect_ratio = screen_aspect_ratio");
                    h = go2_display_height_get(display);
                    w = go2_display_width_get(display);
                    x = 0;
                    y = 0;
                }
                else if (aspect_ratio < screen_aspect_ratio)
                {
                    if (first_video_refresh)
                    logger.log(Logger::DEB, "aspect_ratio < screen_aspect_ratio");
                    w = go2_display_width_get(display);
                    h = w * aspect_ratio;
                    h = (h > go2_display_height_get(display)) ? go2_display_height_get(display) : h;
                    y = (go2_display_height_get(display) / 2) - (h / 2);
                    x = 0;
                }
                else if (aspect_ratio > screen_aspect_ratio)
                {
                    if (first_video_refresh)
                    logger.log(Logger::DEB, "aspect_ratio > screen_aspect_ratio");
                    h = go2_display_height_get(display);
                    if (wideScreenNotRotated()){
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
            if (first_video_refresh)
            logger.log(Logger::DEB, "screen is NOT widescreen");
            
            screen_aspect_ratio = 1 / screen_aspect_ratio; // screen is rotated

            if (cmpf(aspect_ratio, screen_aspect_ratio))
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "aspect_ratio = screen_aspect_ratio");
                h = go2_display_height_get(display);
                w = go2_display_width_get(display);
                x = 0;
                y = 0;
            }
            else if (aspect_ratio < screen_aspect_ratio)
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "aspect_ratio < screen_aspect_ratio");
                h = go2_display_height_get(display);
                w = h / aspect_ratio;
                w = (w > go2_display_width_get(display)) ? go2_display_width_get(display) : w;
                x = (go2_display_width_get(display) / 2) - (w / 2);
                y = 0;
            }
            else if (aspect_ratio > screen_aspect_ratio)
            {
                if (first_video_refresh)
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
        if (first_video_refresh)
        logger.log(Logger::DEB, "game is portrait (vertical)");
        if (isTate())
        {
            if (first_video_refresh)
            logger.log(Logger::DEB, "Tate mode is active");
            x = 0;
            y = 0;
            h = go2_display_height_get(display);
            w = go2_display_width_get(display);
        }
        else
        {
            if (first_video_refresh)
            logger.log(Logger::DEB, "Tate mode is NOT active");
            if (aspect_ratio < screen_aspect_ratio)
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "aspect_ratio < screen_aspect_ratio");
                w = go2_display_width_get(display);
                h = w / aspect_ratio;
                h = (h > go2_display_height_get(display)) ? go2_display_height_get(display) : h;
                y = (go2_display_height_get(display) / 2) - (h / 2);
                x = 0;
            }
            else if (aspect_ratio > screen_aspect_ratio)
            {
                if (first_video_refresh)
                logger.log(Logger::DEB, "aspect_ratio > screen_aspect_ratio");
                h = go2_display_height_get(display);
                w = h / aspect_ratio;
                w = (w > go2_display_width_get(display)) ? go2_display_width_get(display) : w;
                x = (go2_display_width_get(display) / 2) - (w / 2);
                y = 0;
            }
        }
    }
    if (first_video_refresh)
    logger.log(Logger::DEB, "Pixel perfect mode disabled: x=%d, y=%d, w=%d, h=%d", x, y, w, h);
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
int maxFrameBlack= 6;
int clearScreenDelay =maxFrameBlack;

int dot_counter = 0;
static int frame_counter = 0;
bool osdDrawing(const void *data, unsigned width, unsigned height, size_t pitch)
{

    bool showStatus = false;
    int res_width = width;
    int res_height = height;
    if (input_info_requested || input_credits_requested || last_input_info_requested || showLoading)
    {
        
        
        res_width = INFO_MENU_WIDTH;
        res_height = INFO_MENU_HEIGHT;

        if (status_surface_full == nullptr)
        {
            status_surface_full = go2_surface_create(display, res_width, res_height, format_565);
        }

        if (input_credits_requested && !showLoading )
        {
            
            makeScreenBlackCredits(status_surface_full, res_width, res_height);
            showCredits(&status_surface_full);
        }else if (last_input_info_requested && !input_info_requested && clearScreenDelay>0) // Se il menu si chiude
        {
            logger.log(Logger::DEB, "Ensuring full screen clear before resuming game...");
            makeScreenBlackCredits(status_surface_full, res_width, res_height);
            clearScreenDelay--;
            if (clearScreenDelay==0){
                clearScreenDelay=maxFrameBlack;
            }
        }
        else if (showLoading){
            frame_counter++;
            std::string label = "  Please wait  ";
            if (frame_counter % 30 > 15) {
               label = ". Please wait .";
            }
            makeScreenBlack(status_surface_full, res_width, res_height);
            
            int lenghtLabel= ((label.length())*8/2);
            showTextBigger(res_width/2 -lenghtLabel, res_height/2, label.c_str(), WHITE, &status_surface_full);
        }
        else{

            
            drawMenuInfoBackgroud(status_surface_full, res_width, res_height);
            showInfo(gs_w, &status_surface_full);

            
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


    bool show_bottom_center=false;
    if ( continueToShowSaveLoadStateDoneImage() || input_slot_memory_load_done ||
        input_slot_memory_save_done  || input_slot_memory_reset_done 
    ) // the order here it's ompirtrant because the method continueToShowSaveLoadStateDoneImage is going to put agan to fales the value of these two variables
    {

        if (status_surface_bottom_center == nullptr)
        {
            status_surface_bottom_center = go2_surface_create(display, 150, 20, format_565);
        }
        makeScreenBlack(status_surface_bottom_center, 150, 20);
        showStatus = true;
        show_bottom_center= true;

        if (input_slot_memory_load_done)
        {
            if (lastLoadSaveStateDoneOk){
                std::string label = " SLOT:" + std::to_string(currentSlot) + " LOADED."; 
                showTextBigger(0, 5, label.c_str(), WHITE, &status_surface_bottom_center); 
            } else{
                std::string label = " LOAD FAILED!"; 
                showTextBigger(0, 5, label.c_str(), RED, &status_surface_bottom_center); 
            } 
        }
        else if (input_slot_memory_save_done)
        {
            std::string label = " SLOT:" + std::to_string(currentSlot) + " SAVED."; 
            showTextBigger(0, 5 ,label.c_str(), WHITE, &status_surface_bottom_center);
        }else if (input_slot_memory_reset_done)
        {
            std::string label = " CORE RESET DONE."; 
            showTextBigger(0,5 ,label.c_str(), WHITE, &status_surface_bottom_center);
        }
       
    }

    if (input_slot_memory_load_requested ||
        input_slot_memory_save_requested ||
        input_slot_memory_plus_requested ||
        input_slot_memory_minus_requested || continueToShowSaveLoadStateImage())
    {

        if (status_surface_bottom_center == nullptr)
        {
            status_surface_bottom_center = go2_surface_create(display, 150, 20, format_565);
        }
        makeScreenBlack(status_surface_bottom_center, 150, 20);
        showStatus = true;
        show_bottom_center =true;
        

        if (input_slot_memory_load_requested)
        {
            std::string label = " LOADING SLOT:" + std::to_string(currentSlot)+" ...";
            showTextBigger(0, 5, label.c_str(), ORANGE, &status_surface_bottom_center);   
        }
        else if (input_slot_memory_save_requested)
        {
            std::string label = " SAVING SLOT:"  + std::to_string(currentSlot)+" ...";
            showTextBigger(0, 5 ,label.c_str(), ORANGE, &status_surface_bottom_center);
        }
        else if (input_slot_memory_plus_requested)
        {
            std::string label = " SLOT:"  + std::to_string(currentSlot)+" SELECTED.";
            showTextBigger(0, 5 ,label.c_str(), WHITE, &status_surface_bottom_center);
        }
        else if (input_slot_memory_minus_requested)
        {
            std::string label = " SLOT:"  + std::to_string(currentSlot)+" SELECTED.";
            showTextBigger(0, 5 ,label.c_str(), WHITE, &status_surface_bottom_center);
        }
    }
    
    if (show_bottom_center){
        status_obj->show_bottom_center = true;
    }else{
        status_obj->show_bottom_center = false;
    }

    if (showStatus)
    {
        if (status_surface_bottom_center != nullptr)
        {
            status_obj->bottom_center = status_surface_bottom_center;
        }

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
int timeCorrectFrame =0;
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
        if (showLoading && timeCorrectFrame>2){
            //logger.log(Logger::DEB, "---------------------->Loading finished!");
            showLoading=false;
            twiceTimeCorrectFrame=false;
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
        if (showLoading && timeCorrectFrame>2){
            //logger.log(Logger::DEB, "Loading finished!");
            twiceTimeCorrectFrame=false;
            showLoading=false;
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
//bool lastPixelPerfect= pixel_perfect;
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

   /* if (true )
    {*/
        
        prepareScreen(width, height);
        if (first_video_refresh){
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
