
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
#include <drm/drm_fourcc.h>
#include <ctime>
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include <string.h>
#include <string>
#include <sys/time.h>

#include <cmath>
#include <sys/sysinfo.h>

#include "go2/display.h"
#include "go2/struct.h"
#include "fonts.h"
#include "imgs/imgs_numbers.h"
#include "input.h"

int size_char = 8;

int switchColor = 30;
int step = 1;
int posRetro = 3;
bool loop = true;
std::string tabSpaces = "";
int stepCredits = 15;
int posYCredits = INFO_MENU_HEIGHT + 8 * 2;
int time_credit = 2;
int offset = 0;
bool direction_forward = true;
int rowForText = 0;
int colorInc = 0;

uint32_t format_565 = DRM_FORMAT_RGB565; // DRM_FORMAT_RGB888; // DRM_FORMAT_XRGB8888;//color_format;

int width_fixed = 640;
int height_fixed = 480;

int INFO_MENU_WIDTH = 240;  // 288;
int INFO_MENU_HEIGHT = 160; // 192;



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

extern go2_battery_state_t batteryState;


go2_display_t *display= NULL;
go2_surface_t *surface= NULL;
go2_surface_t *status_surface_bottom_right= NULL;
go2_surface_t *status_surface_bottom_left= NULL;
go2_surface_t *status_surface_top_right= NULL;
go2_surface_t *status_surface_top_left= NULL;
go2_surface_t *status_surface_full= NULL;

go2_surface_t *display_surface= NULL;
go2_frame_buffer_t *frame_buffer= NULL;
go2_presenter_t *presenter= NULL;
go2_context_t *context3D= NULL;

go2_surface_t *gles_surface= NULL;
struct timeval valTime2;





bool cmpf(float A, float B, float epsilon)
{
    return (fabs(A - B) < epsilon);
}


go2_rotation getBlitRotation()
{
    
    if (isGameVertical) // portrait
    {
        if (!isTate())
        {

            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_0 : GO2_ROTATION_DEGREES_270;
        }
        if (tateState == REVERSED)
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_90 : GO2_ROTATION_DEGREES_0;
        }
        else
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_270 : GO2_ROTATION_DEGREES_180;
        }
    }
    else // landscape
    {

        if (!isTate() && tateState != REVERSED)
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_0 : GO2_ROTATION_DEGREES_270;
        }
        if (tateState == REVERSED)
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_90 : GO2_ROTATION_DEGREES_0;
        }
        else
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_270 : GO2_ROTATION_DEGREES_180;
        }
    }
}

go2_rotation getRotation()
{

    if (isGameVertical) // portrait
    {
        if (!isTate())
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_270 : GO2_ROTATION_DEGREES_180;
        }
        if (tateState == REVERSED)
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_0 : GO2_ROTATION_DEGREES_270;
        }
        else
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_180 : GO2_ROTATION_DEGREES_90;
        }
    }
    else
    { // landscape
        if (!isTate() && tateState != REVERSED)
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_0 : GO2_ROTATION_DEGREES_270;
        }
        if (tateState == REVERSED)
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_90 : GO2_ROTATION_DEGREES_0;
        }
        else
        {
            return (isRG351V() || isRG351MP() || isRG503()) ? GO2_ROTATION_DEGREES_270 : GO2_ROTATION_DEGREES_180;
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
    if (isFlycast() || isFlycast2021())
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


///////




void showText(int x, int y, const char *text, unsigned short color, go2_surface_t **surface)
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

int getRowForText()
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


bool canCreditBeDrawn(int pos)
{
    return pos > 0 && pos < INFO_MENU_HEIGHT - 10;
}

void resetCredisPosition()
{
    posYCredits = INFO_MENU_HEIGHT + 8 * 2;
}


void showLongCenteredText(int y, const char *text, unsigned short color, go2_surface_t **surface)
{
    static int offset = 0;
    static bool direction_forward = true;
    static int frame_counter = 0;  // Contatore per rallentare lo scorrimento
    const int FRAME_DELAY = 3;  // Numero di frame da aspettare prima di aggiornare l'offset

    std::string title(text);
    int title_length = title.length();
    int total_text_width = title_length * 8;

    if (total_text_width > INFO_MENU_WIDTH) {
        // Aggiungiamo spazio per il looping
        title = "   " + title + "   ";
        title_length = title.length();
        total_text_width = title_length * 8;

        // Rallenta lo scorrimento: aggiorna offset solo ogni FRAME_DELAY chiamate
        frame_counter++;
        if (frame_counter >= FRAME_DELAY) {
            frame_counter = 0; // Reset contatore solo quando raggiungiamo il delay

            if (direction_forward) {
                offset += 1; // Movimento più lento (prima era PIXELS_PER_FRAME)
                if (offset >= total_text_width - INFO_MENU_WIDTH) {
                    offset = total_text_width - INFO_MENU_WIDTH;
                    direction_forward = false;
                }
            } else {
                offset -= 1;
                if (offset <= 0) {
                    offset = 0;
                    direction_forward = true;
                }
            }
        }

        // Calcola l'indice iniziale per il testo visibile
        int start_index = offset / 8;
        if (start_index < 0) start_index = 0;

        int remaining_text_width = total_text_width - offset;
        int truncated_text_width = std::min(INFO_MENU_WIDTH / 8, remaining_text_width / 8);

        std::string truncated_text = title.substr(start_index, truncated_text_width);

        int x = -(offset % 8); // Mantiene il movimento fluido

        showText(x, y, truncated_text.c_str(), color, surface);
    } else {
        showText(INFO_MENU_WIDTH / 2 - total_text_width / 2, y, title.c_str(), color, surface);
    }
}


void showCenteredText(int y, const char *text, unsigned short color, go2_surface_t **surface)
{
    /*std::string title(text); // The text to scroll
    int title_length = title.length();
    showText(INFO_MENU_WIDTH / 2 - title_length * 8 / 2, y, title.c_str(), color, surface);
    */
    showLongCenteredText(y,text,color,surface);
    // showText(0, y, title.c_str(), color, surface);
}











void drawCreditLine(int y, const char *text, unsigned short color, go2_surface_t **surface)
{

    int currentY = y;
    if (canCreditBeDrawn(currentY))
    {
        showCenteredText(currentY, text, color, surface);
    }
}



void showInfoDevice(int w, go2_surface_t **surface, int posX)
{
    std::string hostName(getDeviceName());
    hostName = stripReturnCarriage(hostName);
    showCenteredText(getRowForText(), (tabSpaces + "Model: " + hostName).c_str(), DARKGREY, surface);

    struct sysinfo sys_info;
    std::string tot_ram = "Total RAM: N/A";
    std::string free_ram = "Free RAM: N/A";
    // std::string procs = "Number of processes: N/A";
    if (sysinfo(&sys_info) == 0)
    {
        long total_ram_val = sys_info.totalram / (1024 * 1024);
        long free_ram_val = sys_info.freeram / (1024 * 1024);
        // long number_procs = sys_info.procs;
        tot_ram = std::to_string(total_ram_val) + " MB";
        free_ram = std::to_string(free_ram_val) + " MB";
        // procs = "# of processes:" + std::to_string(number_procs);
    }

    for (const auto &cpu_info : cpu_info_list)
    {
        showCenteredText(getRowForText(), ("CPU(s): " + cpu_info.number_of_cpu + " " + cpu_info.cpu_name).c_str(), DARKGREY, surface);
        // showCenteredText(getRowForText(), ("CPU(s) Model:"+cpu_info.cpu_name).c_str(), DARKGREY, surface);
        // showCenteredText(getRowForText(), ("CPU(s) Thread per Core:"+cpu_info.thread_per_cpu).c_str(), DARKGREY, surface);
    }
    // This will print the values of number_of_cpu, cpu_name, thread_per_cpu, and device_name for each element in the cpu_info_list vector.

    // showCenteredText(getRowForText(), (tot_ram).c_str(), DARKGREY, surface);

    showCenteredText(getRowForText(), ("GPU: " + gpu_name).c_str(), DARKGREY, surface);
    showCenteredText(getRowForText(), ("RAM: " + tot_ram).c_str(), DARKGREY, surface);
    // showCenteredText(getRowForText(), (procs).c_str(), DARKGREY, surface);
}

void showInfoCore(int w, go2_surface_t **surface, int posX)
{
    std::string core = tabSpaces + "Name: ";
    showCenteredText(getRowForText(), const_cast<char *>(core.append(coreName).c_str()), DARKGREY, surface);
    std::string version = tabSpaces + "Version: ";
    showCenteredText(getRowForText(), const_cast<char *>(version.append(coreVersion).c_str()), DARKGREY, surface);
    std::string canzip = tabSpaces + "Files .zip allowed: ";
    showCenteredText(getRowForText(), const_cast<char *>(canzip.append(coreReadZippedFiles ? "true" : "false").c_str()), DARKGREY, surface);

    std::string openGl = tabSpaces + "OpenGL: ";
    showCenteredText(getRowForText(), const_cast<char *>(openGl.append(isOpenGL ? "true" : "false").c_str()), DARKGREY, surface);
}

void showInfoGame(int w, go2_surface_t **surface, int posX)
{
    std::string origFps = tabSpaces + "Orignal FPS: ";
    showCenteredText(getRowForText(), const_cast<char *>(origFps.append(std::to_string((int)originalFps)).c_str()), DARKGREY, surface);

    std::string averageFps = tabSpaces + "Average FPS: ";
    showCenteredText(getRowForText(), const_cast<char *>(averageFps.append(std::to_string((int)avgFps)).c_str()), DARKGREY, surface);

    std::string res2 = tabSpaces + "Resolution: ";
    showCenteredText(getRowForText(), const_cast<char *>(res2.append(std::to_string(currentWidth)).append("x").append(std::to_string(currentHeight)).c_str()), DARKGREY, surface);
    std::string orientation = tabSpaces + "Orientation: ";
    showCenteredText(getRowForText(), const_cast<char *>(orientation.append(isGameVertical ? "Portrait" : "Landscape").c_str()), DARKGREY, surface);
}

void showCredits(go2_surface_t **surface)
{

    if (time_credit > 0)
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

    drawCreditLine(currentY, "Retrorun", ORANGE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "Light libretro front-end", YELLOW, surface);

    currentY += stepCredits * 3;
    drawCreditLine(currentY, "Developers", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "OtherCrashOverride", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "navy1978", WHITE, surface);
    currentY += stepCredits * 3;
    drawCreditLine(currentY, "Thanks to", DARKGREY, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "Cebion", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "Christian_Haitian", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "dhwz", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "madcat1990", WHITE, surface);
    currentY += stepCredits;
    drawCreditLine(currentY, "Szalik", WHITE, surface);

    if (currentY < 0)
    {                                     // they are over
        std::string title = "Thank you!"; // The text to scroll
        int title_length = title.length();
        showText(INFO_MENU_WIDTH / 2 - title_length * 8 / 2, INFO_MENU_HEIGHT / 2 - 8 / 2, title.c_str(), WHITE, surface);
    }
}




void showInfo(int w, go2_surface_t **surface)
{

    rowForText = 0;
    int posX = 0;

    std::string title = "Retrorun - " + release; // The text to scroll
    int title_length = title.length();
    showText(posRetro, 2, title.c_str(), WHITE, surface);
    if (posRetro == (INFO_MENU_WIDTH - (title_length * size_char)))
    {
        step = -1;
    }
    else if (posRetro == 0)
    {
        step = 1;
    }
    posRetro += step;

    showText(posX, getRowForText(), " ", ORANGE, surface);
    showText(posX, getRowForText(), " ", ORANGE, surface);

    Menu &menu = menuManager.getCurrentMenu();
    std::string menuTitle = menu.getName();
    int menuTitle_length = menuTitle.length();

    showText(INFO_MENU_WIDTH / 2 - menuTitle_length * size_char / 2, getRowForText(), (menu.getName()).c_str(), ORANGE, surface);
    showText(posX, getRowForText(), " ", ORANGE, surface);
    // showText(posX, getRowForText(), " ", ORANGE, surface);
    // showText(posX, getRowForText(), " ", ORANGE, surface);
    for (int i = 0; i < menu.getSize(); i++)
    {

        MenuItem &mi = menu.getItems()[i];
        if (mi.get_name() == SHOW_DEVICE)
        {
            showInfoDevice(w, surface, posX);
        }
        else if (mi.get_name() == SHOW_CORE)
        {
            showInfoCore(w, surface, posX);
        }
        else if (mi.get_name() == SHOW_GAME)
        {
            showInfoGame(w, surface, posX);
        }

        else if (mi.isQuit()|| mi.isQuestion())
        {
            showCenteredText(getRowForText(), (tabSpaces + mi.get_name() + ": < " + mi.getValues()[mi.getValue()] + " >").c_str(), mi.isSelected() ? WHITE : DARKGREY, surface);
        }
        else if (mi.getMenu() != NULL)
        {

            showCenteredText(getRowForText(), (tabSpaces + mi.get_name()).c_str(), mi.isSelected() ? WHITE : DARKGREY, surface);
        }
        else if (mi.m_valueCalculator != NULL)
        {
            showCenteredText(getRowForText(), (tabSpaces + mi.get_name() + ": < " + mi.getStringValue() + mi.getMisUnit() + " >").c_str(), mi.isSelected() ? WHITE : DARKGREY, surface);
        }
        else
        {
            showCenteredText(getRowForText(), (tabSpaces + mi.get_name()).c_str(), mi.isSelected() ? WHITE : DARKGREY, surface);
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

void showNumberSprite(int x, int y, int number, int width, int height, const uint8_t *src)
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

int getDigit(int n, int position)
{
    int res = (int)(n / pow(10, (position - 1))) % 10;
    if (res > 9)
        res = 9;
    if (res < 0)
        res = 0;
    return res;
}

int getWidthFPS()
{

    return go2_surface_width_get(status_surface_top_right);
}

void showFPSImage()
{
    int x = getWidthFPS() - (numbers.width * 2); // depends on the width of the image
    int y = 0;
    int capFps = fps>99 ? 99: fps;
    showNumberSprite(x, y, getDigit(capFps, 2), numbers.width, numbers.height, numbers.pixel_data);
    showNumberSprite(x + numbers.width, y, getDigit(capFps, 1), numbers.width, numbers.height, numbers.pixel_data);
}

void showFullImage_888(int x, int y, int width, int height, const uint8_t *src, go2_surface_t **surface)
{
    int bytes = 4;
    // create the different surfaces for the statues
    if (*surface == nullptr)
    {
        *surface = go2_surface_create(display, width, height, DRM_FORMAT_RGBA8888);
    }
    int src_stride = width * bytes;
    uint8_t *dst = (uint8_t *)go2_surface_map(*surface);
    if (dst == nullptr)
    {
        return;
    }
    int dst_stride = go2_surface_stride_get(*surface);
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

void showFullImage(int x, int y, int width, int height, const uint8_t *src, go2_surface_t **surface)
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

void showImage(Image img, go2_surface_t **surface)
{
    showFullImage(0, 0, img.width, img.height, img.pixel_data, surface);
}



void takeScreenshot(int w, int h)
{
    logger.log(Logger::DEB, "Taking a screenshot!");
    w = isOpenGL ? gles_surface->width : surface->width;
    h = isOpenGL ? gles_surface->height : surface->height;
    go2_surface_t *screenshot = go2_surface_create(display, w, h, DRM_FORMAT_RGB888);
    if (!screenshot)
    {
        logger.log(Logger::ERR, "go2_surface_create for screenshot failed.");
        throw std::exception();
    }

    go2_surface_blit(isOpenGL ? gles_surface : surface,
                     0, 0, w, h,
                     screenshot,
                     0, 0, w, h,
                     getBlitRotation());

    // snap in screenshot directory
    std::string fullPath = screenShotFolder + "/" + romName + "-" + getCurrentTimeForFileName() + ".png";
    go2_surface_save_as_png(screenshot, fullPath.c_str());
    logger.log(Logger::DEB, "Screenshot saved:'%s'\n", fullPath.c_str());
    go2_surface_destroy(screenshot);
    screenshot_requested = false;
    flash = true;
    t_flash_start = std::chrono::high_resolution_clock::now();
}



void makeScreenBlackCredits(go2_surface_t *go2_surface, int res_width, int res_height)
{
    // bool specialCase = (isJaguar() || isBeetleVB() || isDosBox() || isDosCore() || isMame());
    //  res_width = specialCase? res_width * 2 : res_width; // just to be sure to cover the full screen (in some emulators is not enough to use res_width)
    uint8_t *dst = (uint8_t *)go2_surface_map(go2_surface);
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
        dst += go2_surface_stride_get(go2_surface);
        --yy;
    }
    colorInc++;
}



void makeScreenBlack(go2_surface_t *go2_surface, int res_width, int res_height)
{
    // res_width = (isJaguar() || isBeetleVB() || isDosBox() || isDosCore() || isMame()) ? res_width * 2 : res_width; // just to be sure to cover the full screen (in some emulators is not enough to use res_width)
    uint8_t *dst = (uint8_t *)go2_surface_map(go2_surface);
    if (dst == nullptr)
    {
        return;
    }
    int yy = res_height;

    /* if (col_increase % colSwitch ==0)
     col++;*/

    // printf("color:%d\n",col);
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
        dst += go2_surface_stride_get(go2_surface);
        --yy;
    }

    // col_increase++;
}

void makeScreenTotalBlack(go2_surface_t *go2_surface, int res_width, int res_height)
{
    // res_width = (isJaguar() || isBeetleVB() || isDosBox() || isDosCore() || isMame()) ? res_width * 2 : res_width; // just to be sure to cover the full screen (in some emulators is not enough to use res_width)
    uint8_t *dst = (uint8_t *)go2_surface_map(go2_surface);
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
        dst += go2_surface_stride_get(go2_surface);
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

void checkPaused()
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



