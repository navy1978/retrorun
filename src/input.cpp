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

#include "input.h"

#include "globals.h"
#include "video.h"
#include "libretro.h"

#include <go2/input.h>
#include <stdio.h>
#include <sys/time.h>

extern int opt_backlight;
extern int opt_volume;
bool input_ffwd_requested = false;
bool input_message = false;

bool input_exit_requested = false;
bool input_exit_requested_firstTime = false;

extern float fps;
bool input_fps_requested = false;
double lastFPSrequestTime = -1;
bool input_info_requested = false;
bool input_clean_screen = false;
bool input_info_requested_alternative = false;
double lastInforequestTime = -1;

double lastScreenhotrequestTime = -1;
double lastLoadSaveStateRequestTime = -1;
double lastLoadSaveStateDoneTime = -1;


double pauseRequestTime = -1;


struct timeval valTime;
struct timeval exitTimeStop;
struct timeval exitTimeStart;


double lastR3Pressed = -1;
double lastL3Pressed = -1;

bool input_reset_requested = false;
bool input_pause_requested = false;

bool input_credits_requested= false;
//bool input_ffwd_requested = false;
go2_battery_state_t batteryState;
go2_brightness_state_t brightnessState;

static go2_input_state_t *gamepadState;
static go2_input_state_t *prevGamepadState;
static go2_input_t *input;
static bool has_triggers = false;
static bool has_right_analog = false;
//static bool isTate = false;
// static unsigned lastId = 0;


static go2_input_button_t upButton;
static go2_input_button_t downButton;
static go2_input_button_t leftButton;
static go2_input_button_t rightButton;

static go2_input_button_t aButton;
static go2_input_button_t bButton;
static go2_input_button_t xButton;
static go2_input_button_t yButton;

static go2_input_button_t selectButton;
static go2_input_button_t startButton;
static go2_input_button_t l1Button;
static go2_input_button_t r1Button;
static go2_input_button_t l2Button;
static go2_input_button_t r2Button;
static go2_input_button_t l3Button;
static go2_input_button_t r3Button;
// these are special:
static go2_input_button_t f1Button;
static go2_input_button_t f2Button;
bool ignoreF2=true;


bool firstExecution = true;
bool enable_key_log = false;


bool input_slot_memory_plus_requested = false;
double lastSlotPlusTime = -1;
bool input_slot_memory_minus_requested = false;
double lastSlotMinusTime = -1;
bool input_slot_memory_load_requested = false;
double lastSlotLoadTime = -1;
bool input_slot_memory_save_requested = false;
double lastSlotSaveTime = -1;

bool input_slot_memory_load_done=false;
bool input_slot_memory_save_done=false;
bool input_slot_memory_reset_done=false;
bool lastLoadSaveStateDoneOk =true;


go2_input_button_t stringToGo2Button(const std::string& str) {
    if (str == "DPadUp") return Go2InputButton_DPadUp;
    if (str == "DPadDown") return Go2InputButton_DPadDown;
    if (str == "DPadLeft") return Go2InputButton_DPadLeft;
    if (str == "DPadRight") return Go2InputButton_DPadRight;
    if (str == "A") return Go2InputButton_A;
    if (str == "B") return Go2InputButton_B;
    if (str == "X") return Go2InputButton_X;
    if (str == "Y") return Go2InputButton_Y;
    if (str == "F1") return Go2InputButton_F1;
    if (str == "F2") return Go2InputButton_F2;
    if (str == "F3") return Go2InputButton_F3;
    if (str == "F4") return Go2InputButton_F4;
    if (str == "F5") return Go2InputButton_F5;
    if (str == "F6") return Go2InputButton_F6;
    if (str == "F7") return Go2InputButton_F7;
    if (str == "F8") return Go2InputButton_F8;
    if (str == "F9") return Go2InputButton_F9;
    if (str == "F10") return Go2InputButton_F10;
    if (str == "F11") return Go2InputButton_F11;
    if (str == "F12") return Go2InputButton_F12;
    if (str == "START") return Go2InputButton_START;
    if (str == "SELECT") return Go2InputButton_SELECT;
    if (str == "TopLeft") return Go2InputButton_TopLeft;
    if (str == "TopRight") return Go2InputButton_TopRight;
    if (str == "TriggerLeft") return Go2InputButton_TriggerLeft;
    if (str == "TriggerRight") return Go2InputButton_TriggerRight;
    if (str == "THUMBL") return Go2InputButton_THUMBL;
    if (str == "THUMBR") return Go2InputButton_THUMBR;

    return Go2InputButton_A; // fallback
}



void applyButtonRemapping()
{
    logger.log(Logger::DEB, "Default buttons remapping if it's the case...");
    
    for (const auto& [key, value] : conf_map)
    {
        go2_input_button_t button = stringToGo2Button(value);

        if (key == "retrorun_mapping_button_up") {
            upButton = button;
            logger.log(Logger::DEB, "Remapped UP to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_down") {
            downButton = button;
            logger.log(Logger::DEB, "Remapped DOWN to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_left") {
            leftButton = button;
            logger.log(Logger::DEB, "Remapped LEFT to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_right") {
            rightButton = button;
            logger.log(Logger::DEB, "Remapped RIGHT to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_a") {
            aButton = button;
            logger.log(Logger::DEB, "Remapped A to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_b") {
            bButton = button;
            logger.log(Logger::DEB, "Remapped B to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_x") {
            xButton = button;
            logger.log(Logger::DEB, "Remapped X to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_y") {
            yButton = button;
            logger.log(Logger::DEB, "Remapped Y to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_select") {
            selectButton = button;
            logger.log(Logger::DEB, "Remapped SELECT to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_start") {
            startButton = button;
            logger.log(Logger::DEB, "Remapped START to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_l1") {
            l1Button = button;
            logger.log(Logger::DEB, "Remapped L1 to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_r1") {
            r1Button = button;
            logger.log(Logger::DEB, "Remapped R1 to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_l2") {
            l2Button = button;
            logger.log(Logger::DEB, "Remapped L2 to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_r2") {
            r2Button = button;
            logger.log(Logger::DEB, "Remapped R2 to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_l3") {
            l3Button = button;
            logger.log(Logger::DEB, "Remapped L3 to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_r3") {
            r3Button = button;
            logger.log(Logger::DEB, "Remapped R3 to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_f1") {
            f1Button = button;
            logger.log(Logger::DEB, "Remapped F1 to '%s'", value.c_str());
        }
        else if (key == "retrorun_mapping_button_f2") {
            f2Button = button;
            logger.log(Logger::DEB, "Remapped F2 to '%s'", value.c_str());
        }
        
    }
}


void initButtons(){
    logger.log(Logger::DEB, "Default buttons initialization...");
    upButton =Go2InputButton_DPadUp;
    downButton =Go2InputButton_DPadDown;
    leftButton =Go2InputButton_DPadLeft;
    rightButton =Go2InputButton_DPadRight;

    aButton = Go2InputButton_A;
    bButton = Go2InputButton_B;
    xButton = Go2InputButton_X;
    yButton = Go2InputButton_Y;

    selectButton = Go2InputButton_F1;
    startButton = Go2InputButton_F6;
    l1Button = Go2InputButton_TopLeft;
    r1Button = Go2InputButton_TopRight;
    l2Button = Go2InputButton_F4;
    r2Button = Go2InputButton_F3;
    l3Button = Go2InputButton_F2;
    r3Button = Go2InputButton_F5;
    // these are special:
    //f1Button = Go2InputButton_F1;
    // this is problematic! ask why F2 is needed in alternative stuff

     // if the device has a gpio_joypad (RG351MP) some buttons are reverted
     if (gpio_joypad == true){
         startButton = Go2InputButton_F2;
         l2Button = Go2InputButton_TriggerLeft;
         r2Button = Go2InputButton_TriggerRight;
         l3Button = Go2InputButton_F3;
         r3Button = Go2InputButton_F4;
         logger.log(Logger::DEB, "gpio_joypad type detected");
     }else{
        f2Button = Go2InputButton_F2;
        ignoreF2 = false;
     }
 
     if ( isRG503()||isRG353V() || isRG353M() )
     {
         l2Button = Go2InputButton_TriggerLeft;
         r2Button = Go2InputButton_TriggerRight;
         selectButton = Go2InputButton_SELECT; // check if this is ok!
         startButton = Go2InputButton_START;
         l3Button = Go2InputButton_THUMBL;
         r3Button = Go2InputButton_THUMBR;   
         f2Button = Go2InputButton_F2;
         ignoreF2 = false;
         logger.log(Logger::DEB, "RG503/RG353 joypad configuration detected.");
     }else{
        logger.log(Logger::DEB, "RG351 joypad configuration detected.");
     }
 
     applyButtonRemapping();
 
}




void input_gamepad_read()
{

    if (!input)
    {
        if (firstExecution){
            initButtons();
            input = go2_input_create(getDeviceName());
            firstExecution = false;
        }
        // I think this part has no sense: the has_triggered it's false even when should be true...
        if (go2_input_features_get(input) & Go2InputFeatureFlags_Triggers)
        {
            has_triggers = true;
            logger.log(Logger::DEB, "input: Hardware triggers enabled.");
        }else{
            logger.log(Logger::DEB, "input: Hardware triggers disabled.");
        }

        if (go2_input_features_get(input) & Go2InputFeatureFlags_RightAnalog)
        {
            has_right_analog = true;
            logger.log(Logger::DEB, "input: Right analog enabled.");
        }else{
            logger.log(Logger::DEB, "input: Right analog disabled.");
        }

        gamepadState = go2_input_state_create();
        prevGamepadState = go2_input_state_create();
    }

    // Swap current/previous state
    go2_input_state_t *tempState = prevGamepadState;
    prevGamepadState = gamepadState;
    gamepadState = tempState;

    go2_input_state_read(input, gamepadState);
}

go2_input_state_t *input_gampad_current_get()
{
    return gamepadState;
}

void manageCredits(){
if (go2_input_state_button_get(gamepadState, bButton) == ButtonState_Pressed)
    {
        menuManager.handle_input_credits(B_BUTTON);
    }
    if (go2_input_state_button_get(gamepadState, aButton) == ButtonState_Pressed)
    {
        menuManager.handle_input_credits(A_BUTTON);
    }
}

void manageMenu()
{
    // mutexInput.lock();
    if (go2_input_state_button_get(gamepadState, bButton) == ButtonState_Pressed)
    {
        menuManager.handle_input(B_BUTTON);
    }
    if (go2_input_state_button_get(gamepadState, aButton) == ButtonState_Pressed)
    {
        menuManager.handle_input(A_BUTTON);
    }
    if (go2_input_state_button_get(gamepadState, upButton) == ButtonState_Pressed)
    {
        menuManager.handle_input(UP);
    }
    if (go2_input_state_button_get(gamepadState, downButton) == ButtonState_Pressed)
    {
        menuManager.handle_input(DOWN);
    }
    if (go2_input_state_button_get(gamepadState, leftButton) == ButtonState_Pressed)
    {
        menuManager.handle_input(LEFT);
    }
    if (go2_input_state_button_get(gamepadState, rightButton) == ButtonState_Pressed)
    {
        menuManager.handle_input(RIGHT);
    }
    // mutexInput.unlock();
}



void core_input_poll(void)
{

    if (!input)
    {
        input = go2_input_create(getDeviceName());
    }

    // Read inputs
    input_gamepad_read();
    go2_input_battery_read(input, &batteryState);
    go2_input_brightness_read(input, &brightnessState);
    gettimeofday(&exitTimeStop, NULL);
    //double now = exitTime.tv_sec + (exitTime.tv_usec / 1000000.0);
    double now_seconds = (exitTimeStop.tv_sec - exitTimeStart.tv_sec);
    double now_milliseconds = ((double)(exitTimeStop.tv_usec - exitTimeStart.tv_usec)) / 1000000.0;
    // double elapsed_time_ms = now - lastExitTime;
    double elapsed_time_ms = now_seconds + now_milliseconds;
    if (elapsed_time_ms > 2.5)
    {
        input_exit_requested_firstTime = false;
    }

    if (enable_key_log)
    {
        
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F1) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY1] - [F1]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F2) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY2] - [F2]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F3) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY3] - [F3]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F4) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY4] - [F4]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F5) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY5] - [F5]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F6) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY6] - [F6]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F7) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY7] - [F7]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F8) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY8] - [F8]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F9) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY9] - [F9]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F10) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY10[] - [F10]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F11) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY11] - [F11]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_F12) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY12] - [F12]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_SELECT) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_SELECT] - [SELECT]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_START) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_START] - [START]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_THUMBR) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_THUMBR] - [THUMBR]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_THUMBL) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_THUMBL] - [THUMBL]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_A) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_EAST] - [A]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_B) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_SOUTH] - [B]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_X) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_NORTH] - [X]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_Y) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_WEST] - [Y]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadUp) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_DPAD_UP] - [UP]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadDown) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_DPAD_DOWN] - [DOWN]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadLeft) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_DPAD_LEFT] - [LEFT]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadRight) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_DPAD_RIGHT] - [RIGHT]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_TopLeft) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TL] - [TopLeft]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_TopRight) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TR] - [TopRight]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_TriggerLeft) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TL2] - [TriggerLeft]");
            }
            if (go2_input_state_button_get(gamepadState, Go2InputButton_TriggerRight) == ButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TR2] - [TriggerRight]");
            }
        
    }




    // if we are in the menu info we have to manage the input for that
    if (input_credits_requested) {
        manageCredits();
    }else
    if (input_info_requested)
    {
          // Release the lock before calling manageMenu()
            // lock.unlock();
            manageMenu();
       
    }
// Store button states to avoid multiple function calls
bool isL3Pressed = go2_input_state_button_get(gamepadState, l3Button) == ButtonState_Pressed;
bool isR3Pressed = go2_input_state_button_get(gamepadState, r3Button) == ButtonState_Pressed;
bool isSelectPressed = go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed;
bool isStartPressed = go2_input_state_button_get(gamepadState, startButton) == ButtonState_Pressed;
bool isXPressed = go2_input_state_button_get(gamepadState, xButton) == ButtonState_Pressed;
bool isYPressed = go2_input_state_button_get(gamepadState, yButton) == ButtonState_Pressed;
bool isBPressed = go2_input_state_button_get(gamepadState, bButton) == ButtonState_Pressed;
bool isAPressed = go2_input_state_button_get(gamepadState, aButton) == ButtonState_Pressed;
//bool isF1Pressed = go2_input_state_button_get(gamepadState, f1Button) == ButtonState_Pressed;
bool isF2Pressed = ignoreF2 ? false: go2_input_state_button_get(gamepadState, f2Button) == ButtonState_Pressed;
bool isR2Pressed = go2_input_state_button_get(gamepadState, r2Button) == ButtonState_Pressed;
bool wasR2Released = go2_input_state_button_get(prevGamepadState, r2Button) == ButtonState_Released;

// Get current time once
struct timeval valTime;
gettimeofday(&valTime, NULL);
double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
// Handle input_info_requested_alternative condition
if (input_info_requested_alternative) { // this are the alternative combinations used by ArkOs
    // Handle emntering in Menu request
    if (!showLoading && ((isSelectPressed && isXPressed) || (isF2Pressed && isXPressed))) {
        double elapsed = currentTime - lastInforequestTime;
        logger.log(Logger::DEB, "Input: Info requested");
        if (elapsed >= 0.5) {
            input_info_requested = !input_info_requested;
            pause_requested = input_info_requested;
            input_credits_requested = false;
            lastInforequestTime = currentTime;
            logger.log(Logger::DEB, "Input: Info requested OK");
        }
    }
    
    if (!input_info_requested){ // we are not in the menu...
        // Handle exit request
        if ((isF2Pressed && isStartPressed) || (isSelectPressed && isStartPressed)) {
            if (input_exit_requested_firstTime && elapsed_time_ms > 0.5) {
                input_exit_requested = true;
            } else if (!input_exit_requested_firstTime) {
                gettimeofday(&exitTimeStart, NULL);
                input_exit_requested_firstTime = true;
            }
        }
        // Handle FPS request
        if ( !showLoading && ((isF2Pressed && isYPressed) || (isSelectPressed && isYPressed)) ) {
            double elapsed = currentTime - lastFPSrequestTime;
            if (elapsed >= 0.5) {
                input_fps_requested = !input_fps_requested;
                lastFPSrequestTime = currentTime;
            }
        }
        // Handle screenshot request
        if (!showLoading && ((isF2Pressed && isBPressed)|| (isSelectPressed && isBPressed))) {
            screenshot_requested = true;
            lastScreenhotrequestTime = currentTime;
            logger.log(Logger::DEB, "Input: Screenshot requested");
        }

        // Handle pause request
        if (!showLoading && ((isF2Pressed && isAPressed) || (isSelectPressed && isAPressed))) {
            double elapsed = currentTime - pauseRequestTime;
            if (elapsed >= 0.5) {
                logger.log(Logger::DEB, "Input: Pause requested");
                input_pause_requested = !input_pause_requested;
                if (!input_pause_requested) {
                    pause_requested = false;
                }
                logger.log(Logger::DEB, "Input: %s", input_pause_requested ? "Paused" : "Un-paused");
                pauseRequestTime = currentTime;
            }
        }
}

} else { // this is the oermal behaviour used in AmberElec

    // Handle emntering in Menu request
    if (!showLoading && isL3Pressed && isR3Pressed) {
        double elapsed = currentTime - lastInforequestTime;
        logger.log(Logger::DEB, "Input: Info requested");
        if (elapsed >= 0.5) {
            input_info_requested = !input_info_requested;
            pause_requested = input_info_requested;
            input_credits_requested = false;
            lastInforequestTime = currentTime;
            logger.log(Logger::DEB, "Input: Info requested OK");
        }
    }
    if (!input_info_requested){ // we aqre not in the menu
        // Handle exit request
        if ( isSelectPressed && isStartPressed) {
            if (input_exit_requested_firstTime && elapsed_time_ms > 0.5) {
                input_exit_requested = true;
            } else if (!input_exit_requested_firstTime) {
                gettimeofday(&exitTimeStart, NULL);
                input_exit_requested_firstTime = true;
         }
        }
        // Handle FPS request
        if (!showLoading && isSelectPressed && isYPressed) {
            double elapsed = currentTime - lastFPSrequestTime;
            if (elapsed >= 0.5) {
                input_fps_requested = !input_fps_requested;
                lastFPSrequestTime = currentTime;
            }
        }
        // Handle screenshot request
        if (!showLoading && isSelectPressed && isBPressed) {
            screenshot_requested = true;
            lastScreenhotrequestTime = currentTime;
            logger.log(Logger::DEB, "Input: Screenshot requested");
        }
        // Handle pause request
        if (!showLoading && isSelectPressed && isAPressed) {
            double elapsed = currentTime - pauseRequestTime;
            if (elapsed >= 0.5) {
                logger.log(Logger::DEB, "Input: Pause requested");
                input_pause_requested = !input_pause_requested;
                if (!input_pause_requested) {
                    pause_requested = false;
                }
                logger.log(Logger::DEB, "Input: %s", input_pause_requested ? "Paused" : "Un-paused");
                pauseRequestTime = currentTime;
            }
        }
        // Handle fast-forward request
        if (!showLoading && isSelectPressed && isR2Pressed && wasR2Released) {
            input_ffwd_requested = !input_ffwd_requested;
            logger.log(Logger::DEB, "Input: Fast-forward %s", input_ffwd_requested ? "on" : "off");
        }
    }




}


// new
if (!showLoading && (go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed) &&
(go2_input_state_button_get(gamepadState, r1Button) == ButtonState_Pressed))
{
gettimeofday(&valTime, NULL);
double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
lastLoadSaveStateRequestTime= currentTime;
double elapsed = currentTime - lastSlotSaveTime;
if (elapsed >= 0.5)
{
    input_slot_memory_save_requested = true;
    lastSlotSaveTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
    logger.log(Logger::DEB, "Input: We need to save the state");
}  
}

if (!showLoading && (go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed) &&
(go2_input_state_button_get(gamepadState, upButton) == ButtonState_Pressed))
{
gettimeofday(&valTime, NULL);
double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
lastLoadSaveStateRequestTime= currentTime;
double elapsed = currentTime - lastSlotPlusTime;
if (elapsed >= 0.5)
{
    currentSlot = (currentSlot % numberOfStateSlots) + 1;
    input_slot_memory_plus_requested = true;
    lastSlotPlusTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
    logger.log(Logger::DEB, "Input: We need to switch the slot to +1");
    
}  

}

if (!showLoading && (go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed) &&
(go2_input_state_button_get(gamepadState, downButton) == ButtonState_Pressed))
{
gettimeofday(&valTime, NULL);
double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
lastLoadSaveStateRequestTime= currentTime;
double elapsed = currentTime - lastSlotMinusTime;
if (elapsed >= 0.5)
{
    currentSlot = (currentSlot + numberOfStateSlots - 2) % numberOfStateSlots + 1;
    input_slot_memory_minus_requested = true;
    lastSlotMinusTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
    logger.log(Logger::DEB, "Input: We need to switch the slot to -1");
} 
}

if (!showLoading && (go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed) &&
(go2_input_state_button_get(gamepadState, l1Button) == ButtonState_Pressed))
{
gettimeofday(&valTime, NULL);
double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
lastLoadSaveStateRequestTime= currentTime;
double elapsed = currentTime - lastSlotLoadTime;
if (elapsed >= 0.5)
{
    input_slot_memory_load_requested = true;
    lastSlotLoadTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
    logger.log(Logger::DEB, "Input: We need to load the state");
} 
}

}


go2_button_state_t getInputUp(){
    if (!isTate()){
       return  go2_input_state_button_get(gamepadState, upButton);
    }
    if (tateState== REVERSED){
        return go2_input_state_button_get(gamepadState, rightButton);
    }else{
        return go2_input_state_button_get(gamepadState, leftButton);
    }
}

go2_button_state_t getInputDown(){
    if (!isTate()){
       return  go2_input_state_button_get(gamepadState, downButton);
    }
    if (tateState== REVERSED){
        return go2_input_state_button_get(gamepadState, leftButton);
    }else{
        return go2_input_state_button_get(gamepadState, rightButton);
    }
}


go2_button_state_t getInputLeft(){
    if (!isTate()){
       return  go2_input_state_button_get(gamepadState, leftButton);
    }
    if (tateState== REVERSED){
        return go2_input_state_button_get(gamepadState, upButton);
    }else{
        return go2_input_state_button_get(gamepadState, downButton);
    }
}

go2_button_state_t getInputRight(){
    if (!isTate()){
       return  go2_input_state_button_get(gamepadState, rightButton);
    }
    if (tateState== REVERSED){
        return go2_input_state_button_get(gamepadState, downButton);
    }else{
        return go2_input_state_button_get(gamepadState, upButton);
    }
}

go2_button_state_t getInputA(){
    if (!isTate()){
       return go2_input_state_button_get(gamepadState, aButton);
    }
    if (tateState== REVERSED){
        return go2_input_state_button_get(gamepadState, bButton);
    }else{
        return go2_input_state_button_get(gamepadState, xButton);
    }
}

go2_button_state_t getInputB(){
    if (!isTate()){
       return go2_input_state_button_get(gamepadState, bButton);
    }
    if (tateState== REVERSED){
        return go2_input_state_button_get(gamepadState, yButton);
    }else{
        return go2_input_state_button_get(gamepadState, aButton);
    }
}

go2_button_state_t getInputX(){
    if (!isTate()){
       return go2_input_state_button_get(gamepadState, xButton);
    }
    if (tateState== REVERSED){
        return go2_input_state_button_get(gamepadState, aButton);
    }else{
        return go2_input_state_button_get(gamepadState, yButton);
    }
}

go2_button_state_t getInputY(){
    if (!isTate()){
       return go2_input_state_button_get(gamepadState, yButton);
    }
    if (tateState== REVERSED){
        return go2_input_state_button_get(gamepadState, xButton);
    }else{
        return go2_input_state_button_get(gamepadState, bButton);
    }
}





/*
bool core_support_analog=false;

int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    //logger.log(Logger::DEB, "core_input_state called: port=%d, device=%d, index=%d, id=%d", port, device, index, id);

    go2_input_button_t realL1 = gpio_joypad ? l1Button : Go2InputButton_TopLeft;
    go2_input_button_t realR1 = gpio_joypad ? r1Button : Go2InputButton_TopRight;

    
    //logger.log(Logger::DEB, "Core supports analog input: %s", core_support_analog ? "YES" : "NO");

    // Se è selezionato NODE, lasciamo tutto invariato
    if (analogToDigital != NONE)
    
    {
        const float TRIM = 0.35f;
        go2_thumb_t thumbL = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Left);
        go2_thumb_t thumbR = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);

        // Mappatura degli analogici in base alla modalità selezionata
        if (analogToDigital == LEFT_ANALOG || analogToDigital == LEFT_ANALOG_FORCED)
        {
            //logger.log(Logger::DEB, "Mapping LEFT Analog to D-pad.");
            go2_input_state_thumbstick_setNul(gamepadState, Go2InputThumbstick_Right);
            if (thumbL.y < -TRIM) go2_input_state_button_set(gamepadState, Go2InputButton_DPadUp, ButtonState_Pressed);
            if (thumbL.y > TRIM) go2_input_state_button_set(gamepadState, Go2InputButton_DPadDown, ButtonState_Pressed);
            if (thumbL.x < -TRIM) go2_input_state_button_set(gamepadState, Go2InputButton_DPadLeft, ButtonState_Pressed);
            if (thumbL.x > TRIM) go2_input_state_button_set(gamepadState, Go2InputButton_DPadRight, ButtonState_Pressed);
        }

        if (analogToDigital == RIGHT_ANALOG || analogToDigital == RIGHT_ANALOG_FORCED)
        {
            //logger.log(Logger::DEB, "Mapping RIGHT Analog to D-pad.");
            go2_input_state_thumbstick_setNul(gamepadState, Go2InputThumbstick_Left);
            if (thumbR.y < -TRIM) go2_input_state_button_set(gamepadState, Go2InputButton_DPadUp, ButtonState_Pressed);
            if (thumbR.y > TRIM) go2_input_state_button_set(gamepadState, Go2InputButton_DPadDown, ButtonState_Pressed);
            if (thumbR.x < -TRIM) go2_input_state_button_set(gamepadState, Go2InputButton_DPadLeft, ButtonState_Pressed);
            if (thumbR.x > TRIM) go2_input_state_button_set(gamepadState, Go2InputButton_DPadRight, ButtonState_Pressed);
        }

        
    }


    if (Retrorun_Core == RETRORUN_CORE_PARALLEL_N64) // C buttons
    {

        const float TRIM = 0.35f;
        go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);

        if (thumb.y < -TRIM) // UP
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, Go2InputButton_X, ButtonState_Pressed);
        }
        if (thumb.y > TRIM) //DOWN
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, Go2InputButton_B, ButtonState_Pressed);
        }
        if (thumb.x < -TRIM)//LEFT
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, Go2InputButton_Y, ButtonState_Pressed);
        }
        if (thumb.x > TRIM)// RIGHT
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, Go2InputButton_A, ButtonState_Pressed);
        }
        thumb.x = 0;
        thumb.y = 0;    
    }
    
    // Gestione input digitale (joypad)
    if (port == 0 )
    {
        
        // manage mouse ( for certain cores like dosbox)
        if (device == RETRO_DEVICE_MOUSE)
        {
            go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);
            switch (id)
            {
            case RETRO_DEVICE_ID_ANALOG_X:
                return thumb.x *retrorun_mouse_speed_factor;
                break;
            case RETRO_DEVICE_ID_JOYPAD_Y:
                return thumb.y *retrorun_mouse_speed_factor;
                break;
            default:
                return 0;
                break;
            }
        }
        if (device == RETRO_DEVICE_JOYPAD)
        {
           
        switch (id)
        {
        
        
        case RETRO_DEVICE_ID_JOYPAD_UP:
            if (analogToDigital == LEFT_ANALOG_FORCED || analogToDigital == RIGHT_ANALOG_FORCED) return 0;
            return getInputUp();
        case RETRO_DEVICE_ID_JOYPAD_DOWN:
        if (analogToDigital == LEFT_ANALOG_FORCED || analogToDigital == RIGHT_ANALOG_FORCED) return 0;    
            return getInputDown();
        case RETRO_DEVICE_ID_JOYPAD_LEFT:
        if (analogToDigital == LEFT_ANALOG_FORCED || analogToDigital == RIGHT_ANALOG_FORCED) return 0;
                return getInputLeft();
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
        if (analogToDigital == LEFT_ANALOG_FORCED || analogToDigital == RIGHT_ANALOG_FORCED) return 0;
                return getInputRight();
        case RETRO_DEVICE_ID_JOYPAD_SELECT:
            return go2_input_state_button_get(gamepadState, selectButton);
        case RETRO_DEVICE_ID_JOYPAD_START:
            return go2_input_state_button_get(gamepadState, startButton);
        case RETRO_DEVICE_ID_JOYPAD_A:
            return getInputA();
        case RETRO_DEVICE_ID_JOYPAD_B:
            return getInputB();
        case RETRO_DEVICE_ID_JOYPAD_X:
            return getInputX();
        case RETRO_DEVICE_ID_JOYPAD_Y:
            return getInputY();
        case RETRO_DEVICE_ID_JOYPAD_L:
            return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? l2Button : realL1);
        case RETRO_DEVICE_ID_JOYPAD_R:
            return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? r2Button : realR1);
        case RETRO_DEVICE_ID_JOYPAD_L2:
        if (isRG503()||isRG353V() || isRG353M()){
            return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? l2Button : realL1);
        }else{
            return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? realL1 : l2Button);
        }
        case RETRO_DEVICE_ID_JOYPAD_R2:
        if (isRG503()||isRG353V() || isRG353M()){
            return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? r2Button : realR1);
        }else{
            return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? realR1 : r2Button);
        }
        case RETRO_DEVICE_ID_JOYPAD_L3:
            return go2_input_state_button_get(gamepadState, l3Button);
        case RETRO_DEVICE_ID_JOYPAD_R3:
            return go2_input_state_button_get(gamepadState, r3Button);
        
        
        
        
        default:
            
            return 0;
        }
    }


    
       
        go2_thumb_t thumb;

// Se un analogico è forzato, usiamo quello corrispondente
if (analogToDigital == LEFT_ANALOG_FORCED)
{
    thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Left);
    //thumb.y = -thumb.y;
}
else if (analogToDigital == RIGHT_ANALOG_FORCED)
{
    thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);
    //thumb.y = -thumb.y;
}
else
{
    // Se non è forzato, selezioniamo in base all'index
    thumb = go2_input_state_thumbstick_get(gamepadState,
                                           index == RETRO_DEVICE_INDEX_ANALOG_LEFT ? Go2InputThumbstick_Left: Go2InputThumbstick_Right);
       thumb.y = -thumb.y;                                    
}
if(isTate()){
    thumb.y = -thumb.y;
    thumb.x = -thumb.x;
}


        switch (id)
        {
        case RETRO_DEVICE_ID_ANALOG_X:
            return thumb.x * 0x7fff;
        case RETRO_DEVICE_ID_ANALOG_Y:
            return thumb.y * 0x7fff;
        default:
            return 0;
        }
    
    }// end port ==0

    // Gestione input analogico (se non è forzato)
    

    return 0;
}
    */


int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{


    go2_input_button_t realL1 =  gpio_joypad ? l1Button : Go2InputButton_TopLeft;
    go2_input_button_t realR1 = gpio_joypad ? r1Button : Go2InputButton_TopRight ;
// for set we dont need to take care of tate mode
    
    if (force_left_analog_stick)
    {
        // Map thumbstick to dpad (force to enable the left analog stick mapping to it the DPAD)
        const float TRIM = 0.35f;
        go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Left);

        if (thumb.y < -TRIM)
            go2_input_state_button_set(gamepadState, upButton, ButtonState_Pressed);
        if (thumb.y > TRIM)
            go2_input_state_button_set(gamepadState, downButton, ButtonState_Pressed);
        if (thumb.x < -TRIM)
            go2_input_state_button_set(gamepadState, leftButton, ButtonState_Pressed);
        if (thumb.x > TRIM)
            go2_input_state_button_set(gamepadState, rightButton, ButtonState_Pressed);
    }


    if (Retrorun_Core == RETRORUN_CORE_PARALLEL_N64) // C buttons
    {

        const float TRIM = 0.35f;
        go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);

        if (thumb.y < -TRIM) // UP
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, xButton, ButtonState_Pressed);
        }
        if (thumb.y > TRIM) //DOWN
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, bButton, ButtonState_Pressed);
        }
        if (thumb.x < -TRIM)//LEFT
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, yButton, ButtonState_Pressed);
        }
        if (thumb.x > TRIM)// RIGHT
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, aButton, ButtonState_Pressed);
        }
        thumb.x = 0;
        thumb.y = 0;    
    }



    if (port == 0)
    {
        
        
        // manage mouse ( for certain cores like dosbox)
        if (device == RETRO_DEVICE_MOUSE)
        {
         
            go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);

          

            switch (id)
            {
            case RETRO_DEVICE_ID_ANALOG_X:
                return thumb.x *retrorun_mouse_speed_factor;
                break;

            case RETRO_DEVICE_ID_JOYPAD_Y:
                return thumb.y *retrorun_mouse_speed_factor;
                break;

            default:
                return 0;
                break;
            }

        }
        
        
        if (device == RETRO_DEVICE_JOYPAD)
        {

            switch (id)
            {
                
            case RETRO_DEVICE_ID_JOYPAD_SELECT:
                return go2_input_state_button_get(gamepadState, selectButton);
                break;
            case RETRO_DEVICE_ID_JOYPAD_START:
                return go2_input_state_button_get(gamepadState, startButton);
                break;
            case RETRO_DEVICE_ID_JOYPAD_UP:
                return getInputUp();
                break;
            case RETRO_DEVICE_ID_JOYPAD_DOWN:
                return getInputDown();
                break;
            case RETRO_DEVICE_ID_JOYPAD_LEFT:
                return getInputLeft();
                break;
            case RETRO_DEVICE_ID_JOYPAD_RIGHT:
                return getInputRight();
                break;
            case RETRO_DEVICE_ID_JOYPAD_A:
                return getInputA();
                break;
            case RETRO_DEVICE_ID_JOYPAD_B:
                return getInputB();
                break;    
            case RETRO_DEVICE_ID_JOYPAD_X:
                return getInputX();
                break;
            case RETRO_DEVICE_ID_JOYPAD_Y:
                return getInputY();
                break;    
            case RETRO_DEVICE_ID_JOYPAD_L:
                return go2_input_state_button_get(gamepadState,swapL1R1WithL2R2 ? l2Button: realL1);
                break;
            case RETRO_DEVICE_ID_JOYPAD_R:
                return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? r2Button : realR1);
                break;
            case RETRO_DEVICE_ID_JOYPAD_L2:
                return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? realL1 : l2Button);
                break;
            case RETRO_DEVICE_ID_JOYPAD_R2:
                return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? realR1 : r2Button);
                break;
            case RETRO_DEVICE_ID_JOYPAD_L3:
                return go2_input_state_button_get(gamepadState, l3Button);
                break;
            case RETRO_DEVICE_ID_JOYPAD_R3:
                return go2_input_state_button_get(gamepadState, r3Button);
                break;
            default:
                return 0;
                break;
            }
        }



        else if (!force_left_analog_stick 
        && device == RETRO_DEVICE_ANALOG 
        && (index == RETRO_DEVICE_INDEX_ANALOG_LEFT || RETRO_DEVICE_INDEX_ANALOG_RIGHT)) 
        {

            if (swapSticks)
            {
                if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
                {
                    index = RETRO_DEVICE_INDEX_ANALOG_RIGHT;
                }
                else
                {
                    index = RETRO_DEVICE_INDEX_ANALOG_LEFT;
                }
            }

            go2_thumb_t thumb;
            //go2_thumb_t thumb2;
            if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT ){
            thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Left);
            }else{
                thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);
            }

            if (thumb.x > 1.0f)
                thumb.x = 1.0f;
            else if (thumb.x < -1.0f)
                thumb.x = -1.0f;

            if (thumb.y > 1.0f)
                thumb.y = 1.0f;
            else if (thumb.y < -1.0f)
                thumb.y = -1.0f;

    if (!isTate()){
        // in normal mode we invert the up down of the analog stick 
        // to align this to retroarch
        thumb.y = thumb.y * -1.0f; 
    }
    if (tateState== REVERSED){
        
        
        thumb.x = thumb.x * 1.0f;
        thumb.y = thumb.y * -1.0f;
    }else{
        thumb.x = thumb.x * 1.0f;
        thumb.y = thumb.y * -1.0f;
    }

// Prevent optimization
//asm volatile("" ::: "memory");

//printf("thumb.x %f\n", thumb.x);
//printf("thumb.y %f\n", thumb.y);
           

            switch (id)
            {
            case RETRO_DEVICE_ID_ANALOG_X:
                if (!isTate()){
                    return thumb.x * 0x7fff;
                }
                if (tateState== REVERSED){ // TODO: review this
                    return -1 * thumb.y * 0x7fff;
                }else{
                    return  thumb.y * 0x7fff;
                }
                
                break;

            case RETRO_DEVICE_ID_ANALOG_Y:
                
                if (!isTate()){
                    return thumb.y * 0x7fff;
                }
                if (tateState== REVERSED){ // TODO : review this
                    return -1 * thumb.x * 0x7fff;
                }else{
                    return  thumb.x * 0x7fff;
                }
                break;

            default:
                return 0;
                break;
            }
        }
    }

    return 0;
}

