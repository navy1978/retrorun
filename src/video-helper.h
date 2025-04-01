#pragma once

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
#include <stdint.h>
#include <stdlib.h>
#include "go2/display.h"
#include "go2/struct.h"


constexpr int PIXELS_PER_FRAME = 1; // Adjust the number of pixels scrolled per frame

extern uint32_t format_565; // DRM_FORMAT_RGB888; // DRM_FORMAT_XRGB8888;//color_format;

extern int width_fixed;
extern int height_fixed;

extern int INFO_MENU_WIDTH;  // 288;
extern int INFO_MENU_HEIGHT; // 192;

extern int display_width, display_height;
extern int base_width;
extern int base_height; 
extern int max_width;
extern int max_height;
extern int aw;
extern int ah;

 

extern bool isGameVertical;
extern bool isOpenGL;
extern unsigned currentWidth;
extern unsigned currentHeight;

extern go2_display_t *display;
extern go2_surface_t *surface;
extern go2_surface_t *status_surface_bottom_right;
extern go2_surface_t *status_surface_bottom_left;
extern go2_surface_t *status_surface_bottom_center;
extern go2_surface_t *status_surface_top_right;
extern go2_surface_t *status_surface_top_left;
extern go2_surface_t *status_surface_full;

extern go2_surface_t *display_surface;
extern go2_frame_buffer_t *frame_buffer;
extern go2_presenter_t *presenter;
extern go2_context_t *context3D;
extern go2_surface_t *gles_surface;






bool cmpf(float A, float B, float epsilon = 0.005f);
go2_rotation getBlitRotation();
go2_rotation getRotation();
int getFixedWidth(int alternative);
int getFixedHeight(int alternative);
int getBase_width();
int getBase_height();
int getMax_width();
int getMax_height();
int getGeom_max_width(const struct retro_game_geometry *geom);
int getGeom_max_height(const struct retro_game_geometry *geom);

//
void showText(int x, int y, const char *text, unsigned short color, go2_surface_t **surface);
void showTextBigger(int x, int y, const char *text, unsigned short color, go2_surface_t **surface);
int getRowForText();
std::string stripReturnCarriage(std::string input);
bool canCreditBeDrawn(int pos);
void resetCredisPosition();
void showCenteredText(int y, const char *text, unsigned short color, go2_surface_t **surface);
void showLongCenteredText(int y, const char *text, unsigned short color, go2_surface_t **surface);
void drawCreditLine(int y, const char *text, unsigned short color, go2_surface_t **surface);
void showInfoDevice(int w, go2_surface_t **surface, int posX);
void showInfoCore(int w, go2_surface_t **surface, int posX);
void showInfoGame(int w, go2_surface_t **surface, int posX);
void showCredits(go2_surface_t **surface);
void showInfo(int w, go2_surface_t **surface);
std::string getCurrentTimeForFileName();
void showNumberSprite(int x, int y, int number, int width, int height, const uint8_t *src);
int getDigit(int n, int position);
int getWidthFPS();
void showFPSImage();
void showFullImage_888(int x, int y, int width, int height, const uint8_t *src, go2_surface_t **surface);
void showFullImage(int x, int y, int width, int height, const uint8_t *src, go2_surface_t **surface);
// refactor
void showImage(Image img, go2_surface_t **surface);
void takeScreenshot(int w, int h);
void makeScreenBlackCredits(go2_surface_t *go2_surface, int res_width, int res_height);
void makeScreenTotalBlack(go2_surface_t *go2_surface, int res_width, int res_height);
void makeScreenBlack(go2_surface_t *go2_surface, int res_width, int res_height);
void drawMenuInfoBackgroud(go2_surface_t *go2_surface, int res_width, int res_height);
bool continueToShowScreenshotImage();
bool continueToShowSaveLoadStateImage();
bool continueToShowSaveLoadStateDoneImage();
void checkPaused();



