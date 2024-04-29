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
double lastInforequestTime = -1;

double lastScreenhotrequestTime = -1;

bool input_slot_memory_plus_requested = false;
double lastSlotPlusTime = -1;
bool input_slot_memory_minus_requested = false;
double lastSlotMinusTime = -1;
bool input_slot_memory_load_requested = false;
double lastSlotLoadTime = -1;
bool input_slot_memory_save_requested = false;
double lastSlotSaveTime = -1;

double pauseRequestTime = -1;

struct timeval valTime;
struct timeval exitTimeStop;
struct timeval exitTimeStart;

double lastR3Pressed = -1;
double lastL3Pressed = -1;

bool input_reset_requested = false;
bool input_pause_requested = false;

bool input_credits_requested = false;
// bool input_ffwd_requested = false;
go2_battery_state_t batteryState;
go2_brightness_state_t brightnessState;

static go2_input_state_t *gamepadState;
static go2_input_state_t *prevGamepadState;
static go2_input_t *input;
static bool has_triggers = false;
static bool has_right_analog = false;
// static bool isTate = false;
//  static unsigned lastId = 0;
static go2_input_button_t selectButton = Go2InputButton_F1; // hotkey is a special one
static go2_input_button_t startButton = Go2InputButton_F6;
static go2_input_button_t l1Button = Go2InputButton_TriggerLeft;
static go2_input_button_t r1Button = Go2InputButton_TriggerRight;
static go2_input_button_t l2Button = Go2InputButton_F4;
static go2_input_button_t r2Button = Go2InputButton_F3;
static go2_input_button_t l3Button = Go2InputButton_F2;
static go2_input_button_t r3Button = Go2InputButton_F5;
bool firstExecution = true;

void input_gamepad_read()
{

    // if the device has a gpio_joypad (RG351MP) some buttons are reverted
    if (gpio_joypad == true)
    {
        selectButton = Go2InputButton_F1; // check if this is ok!
        startButton = Go2InputButton_F2;
        l1Button = Go2InputButton_TopLeft;
        r1Button = Go2InputButton_TopRight;

        l2Button = Go2InputButton_TriggerLeft;
        r2Button = Go2InputButton_TriggerRight;
        l3Button = Go2InputButton_F3;
        r3Button = Go2InputButton_F4;

        if (firstExecution)
        {
            logger.log(Logger::INF, "GPIO JOYPAD: ENABLED!");
            // firstExecution = false;
        }
    }

    // TO BE VERIFIED

    if (isRG351V())
    {
        l1Button = Go2InputButton_TopLeft;
        r1Button = Go2InputButton_TopRight;

        l2Button = Go2InputButton_TriggerLeft;
        r2Button = Go2InputButton_TriggerRight;
    }

    if (isRG503())
    {
        selectButton = Go2InputButton_SELECT; // check if this is ok!
        startButton = Go2InputButton_START;
        l3Button = Go2InputButton_THUMBL;
        r3Button = Go2InputButton_THUMBR;
    }

    if (!input)
    {
        if (firstExecution)
        {
            input = go2_input_create(getDeviceName());
            firstExecution = false;
        }
        // I think this part has no sense: the has_triggered it's false even when should be true...
        if (go2_input_features_get(input) & Go2InputFeatureFlags_Triggers)
        {
            has_triggers = true;
            logger.log(Logger::INF, "input: Hardware triggers enabled.");
        }

        if (go2_input_features_get(input) & Go2InputFeatureFlags_RightAnalog)
        {
            has_right_analog = true;
            logger.log(Logger::INF, "input: Right analog enabled.");
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

void manageCredits()
{
    if (go2_input_state_button_get(gamepadState, Go2InputButton_B) == ButtonState_Pressed)
    {
        menuManager.handle_input_credits(B_BUTTON);
    }
    if (go2_input_state_button_get(gamepadState, Go2InputButton_A) == ButtonState_Pressed)
    {
        menuManager.handle_input_credits(A_BUTTON);
    }
}

void manageMenu()
{
    // mutexInput.lock();
    if (go2_input_state_button_get(gamepadState, Go2InputButton_B) == ButtonState_Pressed)
    {
        menuManager.handle_input(B_BUTTON);
    }
    if (go2_input_state_button_get(gamepadState, Go2InputButton_A) == ButtonState_Pressed)
    {
        menuManager.handle_input(A_BUTTON);
    }
    if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadUp) == ButtonState_Pressed)
    {
        menuManager.handle_input(UP);
    }
    if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadDown) == ButtonState_Pressed)
    {
        menuManager.handle_input(DOWN);
    }
    if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadLeft) == ButtonState_Pressed)
    {
        menuManager.handle_input(LEFT);
    }
    if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadRight) == ButtonState_Pressed)
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
    // double now = exitTime.tv_sec + (exitTime.tv_usec / 1000000.0);
    double now_seconds = (exitTimeStop.tv_sec - exitTimeStart.tv_sec);
    double now_milliseconds = ((double)(exitTimeStop.tv_usec - exitTimeStart.tv_usec)) / 1000000.0;
    // double elapsed_time_ms = now - lastExitTime;
    double elapsed_time_ms = now_seconds + now_milliseconds;
    if (elapsed_time_ms > 2.5)
    {
        input_exit_requested_firstTime = false;
    }

    if (elable_key_log)
    {
        if (go2_input_state_button_get(gamepadState, Go2InputButton_F1) == ButtonState_Pressed)
        {
            printf("input: F1.\n");
        }
        if (go2_input_state_button_get(gamepadState, Go2InputButton_F2) == ButtonState_Pressed)
        {
            printf("input: F2.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F3) == ButtonState_Pressed)
        {
            printf("input: F3.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F4) == ButtonState_Pressed)
        {
            printf("input: F4.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F5) == ButtonState_Pressed)
        {
            printf("input: F5.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F6) == ButtonState_Pressed)
        {
            printf("input: F6.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F7) == ButtonState_Pressed)
        {
            printf("input: F7.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F8) == ButtonState_Pressed)
        {
            printf("input: F8.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F9) == ButtonState_Pressed)
        {
            printf("input: F9.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F10) == ButtonState_Pressed)
        {
            printf("input: F10.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F11) == ButtonState_Pressed)
        {
            printf("input: F11.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F12) == ButtonState_Pressed)
        {
            printf("input: F12.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F13) == ButtonState_Pressed)
        {
            printf("input: F13.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F14) == ButtonState_Pressed)
        {
            printf("input: F14.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F15) == ButtonState_Pressed)
        {
            printf("input: F15.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_F16) == ButtonState_Pressed)
        {
            printf("input: F16.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_SELECT) == ButtonState_Pressed)
        {
            printf("input: SELECT.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_START) == ButtonState_Pressed)
        {
            printf("input: START.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_THUMBR) == ButtonState_Pressed)
        {
            printf("input: THUMBR.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_THUMBL) == ButtonState_Pressed)
        {
            printf("input: THUMBR.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_A) == ButtonState_Pressed)
        {
            printf("input: A.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_B) == ButtonState_Pressed)
        {
            printf("input: B.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_X) == ButtonState_Pressed)
        {
            printf("input: X.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_Y) == ButtonState_Pressed)
        {
            printf("input: Y.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadUp) == ButtonState_Pressed)
        {
            printf("input: UP.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadDown) == ButtonState_Pressed)
        {
            printf("input: DOWN.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadLeft) == ButtonState_Pressed)
        {
            printf("input: LEFT.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_DPadRight) == ButtonState_Pressed)
        {
            printf("input: RIGHT.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_TopLeft) == ButtonState_Pressed)
        {
            printf("input: TopLeft.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_TopRight) == ButtonState_Pressed)
        {
            printf("input TopRight.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_TriggerLeft) == ButtonState_Pressed)
        {
            printf("input: TriggerLeft.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_TriggerRight) == ButtonState_Pressed)
        {
            printf("input: TriggerRight.\n");
        }
    }

    // if we are in the menu info we have to manage the input for that
    if (input_credits_requested)
    {
        manageCredits();
    }
    else if (input_info_requested)
    {
        // Release the lock before calling manageMenu()
        // lock.unlock();
        manageMenu();
    }

    if (!input_info_requested && go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed &&
        go2_input_state_button_get(gamepadState, startButton) == ButtonState_Pressed)
    {
        if (input_exit_requested_firstTime && elapsed_time_ms > 0.5)
        {
            input_exit_requested = true;
        }
        else if (!input_exit_requested_firstTime)
        {
            gettimeofday(&exitTimeStart, NULL);
            input_exit_requested_firstTime = true;
        }
    }

    if (!input_info_requested && go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed &&
        go2_input_state_button_get(gamepadState, Go2InputButton_Y) == ButtonState_Pressed)
    {

        gettimeofday(&valTime, NULL);
        double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        bool updateFPSRequest = false;
        double elapsed = currentTime - lastFPSrequestTime;
        if (elapsed >= 0.5)
        {
            updateFPSRequest = true;
        }
        else
        {
            updateFPSRequest = false;
        }
        if (updateFPSRequest)
        {
            input_fps_requested = !input_fps_requested;
            gettimeofday(&valTime, NULL);
            lastFPSrequestTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        }
    }

    /*if (go2_input_state_button_get(gamepadState, l3Button) == ButtonState_Pressed)
    {
        gettimeofday(&valTime, NULL);
        double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        double elapsed = currentTime - lastL3Pressed;
        lastL3Pressed = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
    }

    if (go2_input_state_button_get(gamepadState, l3Button) == ButtonState_Pressed)
    {
        gettimeofday(&valTime, NULL);
        double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        double elapsed = currentTime - lastR3Pressed;
        lastR3Pressed = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
    }*/

    if ((go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed) &&
        (go2_input_state_button_get(gamepadState, r1Button) == ButtonState_Pressed))
    {
        
        gettimeofday(&valTime, NULL);
        double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        double elapsed = currentTime - lastSlotSaveTime;
        if (elapsed >= 0.5)
        {
            input_slot_memory_save_requested = true;
            // printf("pause_requested:%s input_info_requested:%s\n", pause_requested? "true": "false", input_info_requested? "true": "false");
            lastSlotSaveTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
            logger.log(Logger::DEB, "Input: We need to save the state\n");
        }  
    }

    if ((go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed) &&
        (go2_input_state_button_get(gamepadState, Go2InputButton_DPadUp) == ButtonState_Pressed))
    {

        gettimeofday(&valTime, NULL);
        double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        double elapsed = currentTime - lastSlotPlusTime;
        if (elapsed >= 0.5)
        {
            input_slot_memory_plus_requested = true;
            // printf("pause_requested:%s input_info_requested:%s\n", pause_requested? "true": "false", input_info_requested? "true": "false");
            lastSlotPlusTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
            logger.log(Logger::DEB, "Input: We need to switch the slot to +1\n");
        }  
        
    }

    if ((go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed) &&
        (go2_input_state_button_get(gamepadState, Go2InputButton_DPadDown) == ButtonState_Pressed))
    {
        
        gettimeofday(&valTime, NULL);
        double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        double elapsed = currentTime - lastSlotPlusTime;
        if (elapsed >= 0.5)
        {
            input_slot_memory_minus_requested = true;
            // printf("pause_requested:%s input_info_requested:%s\n", pause_requested? "true": "false", input_info_requested? "true": "false");
            lastSlotPlusTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
            logger.log(Logger::DEB, "Input: We need to switch the slot to -1\n");
        } 
    }

    if ((go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed) &&
        (go2_input_state_button_get(gamepadState, l1Button) == ButtonState_Pressed))
    {
        
        gettimeofday(&valTime, NULL);
        double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        double elapsed = currentTime - lastSlotPlusTime;
        if (elapsed >= 0.5)
        {
            input_slot_memory_load_requested = true;
            // printf("pause_requested:%s input_info_requested:%s\n", pause_requested? "true": "false", input_info_requested? "true": "false");
            lastSlotPlusTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
            logger.log(Logger::DEB, "Input: We need to load\n");
        } 
    }

    if ((go2_input_state_button_get(gamepadState, l3Button) == ButtonState_Pressed /*|| (currentTime - lastL3Pressed <=0.2)*/) &&
        (go2_input_state_button_get(gamepadState, r3Button) == ButtonState_Pressed /*|| (currentTime - lastR3Pressed <=0.2)*/))
    {
        gettimeofday(&valTime, NULL);
        double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        double elapsed = currentTime - lastInforequestTime;
        logger.log(Logger::DEB, "Input: Info requested");
        if (elapsed >= 0.5)
        {
            input_info_requested = !input_info_requested;
            pause_requested = input_info_requested;
            input_credits_requested = false;
            // printf("pause_requested:%s input_info_requested:%s\n", pause_requested? "true": "false", input_info_requested? "true": "false");
            lastInforequestTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
            logger.log(Logger::DEB, "Input: Info requested OK");
        }
        else
        {
            //  printf("input: Info requested NOT OK (too quick)\n");

            /*input_info_requested = false;
              pause_requested = false;*/
        }
    }

    if (!input_info_requested && go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed &&
        go2_input_state_button_get(gamepadState, Go2InputButton_B) == ButtonState_Pressed)
    {
        screenshot_requested = true;
        gettimeofday(&valTime, NULL);
        lastScreenhotrequestTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        logger.log(Logger::DEB, "Input: Screenshot requested");
    }

    if ((go2_input_state_button_get(gamepadState, selectButton) == ButtonState_Pressed /*|| (currentTime - lastL3Pressed <=0.2)*/) &&
        (go2_input_state_button_get(gamepadState, Go2InputButton_A) == ButtonState_Pressed /*|| (currentTime - lastR3Pressed <=0.2)*/))
    {
        gettimeofday(&valTime, NULL);
        double currentTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        double elapsed = currentTime - pauseRequestTime;

        if (elapsed >= 0.5)
        {
            logger.log(Logger::DEB, "Input: Pause requested");
            input_pause_requested = !input_pause_requested;
            if (!input_pause_requested)
            {
                pause_requested = false;
            }
            logger.log(Logger::DEB, "Input: %s", input_pause_requested ? "Paused" : "Un-paused");
            pauseRequestTime = valTime.tv_sec + (valTime.tv_usec / 1000000.0);
        }
        else
        {
            // printf("input: pause requested NOT OK (too quick)\n");
        }
    }

    if (!input_info_requested && go2_input_state_button_get(gamepadState, Go2InputButton_F1) == ButtonState_Pressed)
    {
        if (go2_input_state_button_get(gamepadState, r2Button) == ButtonState_Pressed &&
            go2_input_state_button_get(prevGamepadState, r2Button) == ButtonState_Released)
        {
            input_ffwd_requested = !input_ffwd_requested;
            logger.log(Logger::DEB, "Input: Fast-forward %s", input_ffwd_requested ? "on" : "off");
        }
    }
}

go2_button_state_t getInputUp()
{
    if (!isTate())
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadUp);
    }
    if (tateState == REVERSED)
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadRight);
    }
    else
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadLeft);
    }
}

go2_button_state_t getInputDown()
{
    if (!isTate())
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadDown);
    }
    if (tateState == REVERSED)
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadLeft);
    }
    else
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadRight);
    }
}

go2_button_state_t getInputLeft()
{
    if (!isTate())
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadLeft);
    }
    if (tateState == REVERSED)
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadUp);
    }
    else
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadDown);
    }
}

go2_button_state_t getInputRight()
{
    if (!isTate())
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadRight);
    }
    if (tateState == REVERSED)
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadDown);
    }
    else
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_DPadUp);
    }
}

go2_button_state_t getInputA()
{
    if (!isTate())
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_A);
    }
    if (tateState == REVERSED)
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_B);
    }
    else
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_X);
    }
}

go2_button_state_t getInputB()
{
    if (!isTate())
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_B);
    }
    if (tateState == REVERSED)
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_Y);
    }
    else
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_A);
    }
}

go2_button_state_t getInputX()
{
    if (!isTate())
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_X);
    }
    if (tateState == REVERSED)
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_A);
    }
    else
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_Y);
    }
}

go2_button_state_t getInputY()
{
    if (!isTate())
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_Y);
    }
    if (tateState == REVERSED)
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_X);
    }
    else
    {
        return go2_input_state_button_get(gamepadState, Go2InputButton_B);
    }
}

int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{

    go2_input_button_t realL1 = gpio_joypad ? l1Button : Go2InputButton_TopLeft;
    go2_input_button_t realR1 = gpio_joypad ? r1Button : Go2InputButton_TopRight;
    // for set we dont need to take care of tate mode

    if (force_left_analog_stick)
    {
        // Map thumbstick to dpad (force to enable the left analog stick mapping to it the DPAD)
        const float TRIM = 0.35f;
        go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Left);

        if (thumb.y < -TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadUp, ButtonState_Pressed);
        if (thumb.y > TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadDown, ButtonState_Pressed);
        if (thumb.x < -TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadLeft, ButtonState_Pressed);
        if (thumb.x > TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadRight, ButtonState_Pressed);
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
        if (thumb.y > TRIM) // DOWN
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, Go2InputButton_B, ButtonState_Pressed);
        }
        if (thumb.x < -TRIM) // LEFT
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, Go2InputButton_Y, ButtonState_Pressed);
        }
        if (thumb.x > TRIM) // RIGHT
        {
            go2_input_state_button_set(gamepadState, r2Button, ButtonState_Pressed);
            go2_input_state_button_set(gamepadState, Go2InputButton_A, ButtonState_Pressed);
        }
        thumb.x = 0;
        thumb.y = 0;
    }

    /*else if (isTate())
    {
        const float TRIM = 0.35f;
        go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);

        if (thumb.y < -TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadRight, ButtonState_Pressed);
        if (thumb.y > TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadLeft, ButtonState_Pressed);
        if (thumb.x < -TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadUp, ButtonState_Pressed);
        if (thumb.x > TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadDown, ButtonState_Pressed);

    }*/

    /*
#define RETRO_DEVICE_ID_JOYPAD_B        0
#define RETRO_DEVICE_ID_JOYPAD_Y        1
#define RETRO_DEVICE_ID_JOYPAD_SELECT   2
#define RETRO_DEVICE_ID_JOYPAD_START    3
#define RETRO_DEVICE_ID_JOYPAD_UP       4
#define RETRO_DEVICE_ID_JOYPAD_DOWN     5
#define RETRO_DEVICE_ID_JOYPAD_LEFT     6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT    7
#define RETRO_DEVICE_ID_JOYPAD_A        8
#define RETRO_DEVICE_ID_JOYPAD_X        9
#define RETRO_DEVICE_ID_JOYPAD_L       10
#define RETRO_DEVICE_ID_JOYPAD_R       11
#define RETRO_DEVICE_ID_JOYPAD_L2      12
#define RETRO_DEVICE_ID_JOYPAD_R2      13
#define RETRO_DEVICE_ID_JOYPAD_L3      14
#define RETRO_DEVICE_ID_JOYPAD_R3      15
*/

    if (port == 0)
    {

        // manage mouse ( for certain cores like dosbox)
        if (device == RETRO_DEVICE_MOUSE)
        {

            go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);

            switch (id)
            {
            case RETRO_DEVICE_ID_ANALOG_X:
                return thumb.x * retrorun_mouse_speed_factor;
                break;

            case RETRO_DEVICE_ID_JOYPAD_Y:
                return thumb.y * retrorun_mouse_speed_factor;
                break;

            default:
                return 0;
                break;
            }
        }

        if (device == RETRO_DEVICE_JOYPAD)
        {

            /*if (isTate)
            {
                // remap buttons
                // ABYX = XABY
                switch (id)
                {
                case RETRO_DEVICE_ID_JOYPAD_A:
                    id = RETRO_DEVICE_ID_JOYPAD_X;
                    break;

                case RETRO_DEVICE_ID_JOYPAD_B:
                    id = RETRO_DEVICE_ID_JOYPAD_A;
                    break;

                case RETRO_DEVICE_ID_JOYPAD_Y:
                    id = RETRO_DEVICE_ID_JOYPAD_B;
                    break;

                case RETRO_DEVICE_ID_JOYPAD_X:
                    id = RETRO_DEVICE_ID_JOYPAD_Y;
                    break;

                default:
                    break;
                }
            }*/
            // printf("ID: %u\n", id);
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
                return go2_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? l2Button : realL1);
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

        else if (!force_left_analog_stick && device == RETRO_DEVICE_ANALOG && (index == RETRO_DEVICE_INDEX_ANALOG_LEFT || RETRO_DEVICE_INDEX_ANALOG_RIGHT))
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
            // go2_thumb_t thumb2;
            if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            {
                thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Left);
            }
            else
            {
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

            if (!isTate())
            {
                // in normal mode we invert the up down of the analog stick
                // to align this to retroarch
                thumb.y = thumb.y * -1.0f;
            }
            if (tateState == REVERSED)
            {

                thumb.x = thumb.x * 1.0f;
                thumb.y = thumb.y * -1.0f;
            }
            else
            {
                thumb.x = thumb.x * 1.0f;
                thumb.y = thumb.y * -1.0f;
            }

            // Prevent optimization
            // asm volatile("" ::: "memory");

            // printf("thumb.x %f\n", thumb.x);
            // printf("thumb.y %f\n", thumb.y);

            switch (id)
            {
            case RETRO_DEVICE_ID_ANALOG_X:
                if (!isTate())
                {
                    return thumb.x * 0x7fff;
                }
                if (tateState == REVERSED)
                { // TODO: review this
                    return -1 * thumb.y * 0x7fff;
                }
                else
                {
                    return thumb.y * 0x7fff;
                }

                break;

            case RETRO_DEVICE_ID_ANALOG_Y:

                if (!isTate())
                {
                    return thumb.y * 0x7fff;
                }
                if (tateState == REVERSED)
                { // TODO : review this
                    return -1 * thumb.x * 0x7fff;
                }
                else
                {
                    return thumb.x * 0x7fff;
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
