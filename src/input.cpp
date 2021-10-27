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

#include "input.h"

#include "globals.h"
#include "video.h"
#include "libretro.h"

#include <go2/input.h>
#include <stdio.h>
#include <sys/time.h>

extern int opt_backlight;
extern int opt_volume;

bool input_exit_requested = false;
bool input_exit_requested_firstTime = false;

extern float fps;
bool input_fps_requested = false;
bool input_info_requested = false;
struct timeval valTime;
struct timeval exitTimeStop;
struct timeval exitTimeStart;
double lastFPSrequestTime = -1;

bool input_reset_requested = false;
bool input_pause_requested = false;
//bool input_ffwd_requested = false;
go2_battery_state_t batteryState;

static go2_input_state_t *gamepadState;
static go2_input_state_t *prevGamepadState;
static go2_input_t *input;
static bool has_triggers = false;
static bool has_right_analog = false;
static bool isTate = false;
// static unsigned lastId = 0;
static go2_input_button_t Hotkey = Go2InputButton_F2; // hotkey is a special one
static go2_input_button_t startButton = Go2InputButton_F6;
static go2_input_button_t rightAnalogButton = Go2InputButton_F5;
static go2_input_button_t l2Button = Go2InputButton_TriggerLeft;
static go2_input_button_t r2Button = Go2InputButton_TriggerRight;
bool firstExecution = true;
bool elable_key_log = false;

void input_gamepad_read()
{

    // if the device has a gpio_joypad (RG351MP) some buttons are reverted
    if (gpio_joypad == true)
    {
        Hotkey = Go2InputButton_F1;
        startButton = Go2InputButton_F2;
        l2Button = Go2InputButton_TopLeft;
        r2Button = Go2InputButton_TopRight;
        rightAnalogButton = Go2InputButton_F4;
        if (firstExecution)
        {
            printf("GPIO JOYPAD: ENABLED!\n");
            firstExecution = false;
        }
    }

    if (!input)
    {
        input = go2_input_create();

        if (go2_input_features_get(input) & Go2InputFeatureFlags_Triggers)
        {
            has_triggers = true;
            printf("input: Hardware triggers enabled.\n");
        }

        if (go2_input_features_get(input) & Go2InputFeatureFlags_RightAnalog)
        {
            has_right_analog = true;
            printf("input: Right analog enabled.\n");
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

void core_input_poll(void)
{
    if (!input)
    {
        input = go2_input_create();
    }

    // Read inputs
    input_gamepad_read();
    go2_input_battery_read(input, &batteryState);
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
            printf("input: L1.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_TopRight) == ButtonState_Pressed)
        {
            printf("input: R1.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_TriggerLeft) == ButtonState_Pressed)
        {
            printf("input: L2.\n");
        }

        if (go2_input_state_button_get(gamepadState, Go2InputButton_TriggerRight) == ButtonState_Pressed)
        {
            printf("input: R2.\n");
        }
    }

    if (go2_input_state_button_get(gamepadState, Go2InputButton_F1) == ButtonState_Pressed &&
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

    if (go2_input_state_button_get(gamepadState, Go2InputButton_F1) == ButtonState_Pressed &&
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

    if (go2_input_state_button_get(gamepadState, Go2InputButton_F1) == ButtonState_Pressed &&
        go2_input_state_button_get(gamepadState, Go2InputButton_X) == ButtonState_Pressed &&
        go2_input_state_button_get(gamepadState, Go2InputButton_A) == ButtonState_Pressed)
    {
        input_info_requested = !input_info_requested;
    }

    if (go2_input_state_button_get(gamepadState, Go2InputButton_F1) == ButtonState_Pressed &&
        go2_input_state_button_get(gamepadState, Go2InputButton_B) == ButtonState_Pressed)
    {
        screenshot_requested = true;
        printf("input: Screenshot requested\n");
    }

    // if (!prevGamepadState.buttons.f2 && gamepadState.buttons.f2)
    // {
    //     screenshot_requested = true;
    // }

    if (go2_input_state_button_get(gamepadState, Hotkey) == ButtonState_Pressed)
    {
        /*if (go2_input_state_button_get(gamepadState, Go2InputButton_B) == ButtonState_Pressed &&
            go2_input_state_button_get(prevGamepadState, Go2InputButton_B) == ButtonState_Released)
        {
            input_ffwd_requested = !input_ffwd_requested;
            printf("Fast-forward %s\n", input_ffwd_requested ? "on" : "off");
        }*/
        if (go2_input_state_button_get(gamepadState, Go2InputButton_A) == ButtonState_Pressed &&
            go2_input_state_button_get(prevGamepadState, Go2InputButton_A) == ButtonState_Released)
        {
            input_pause_requested = !input_pause_requested;
            printf("%s\n", input_pause_requested ? "Paused" : "Un-paused");
        }
        /*if (go2_input_state_button_get(gamepadState, Go2InputButton_X) == ButtonState_Pressed &&
            go2_input_state_button_get(prevGamepadState, Go2InputButton_X) == ButtonState_Released)
        {
            input_reset_requested = true;
            printf("Reset requested\n");
        }*/
    }
}

int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{

    if (aspect_ratio < 1.0f)
    {
        isTate = true;
    }
    //int16_t result;

    // if (port || index || device != RETRO_DEVICE_JOYPAD)
    //         return 0;

    //if (go2_input_state_button_get(gamepadState, Hotkey) == ButtonState_Pressed)
    //    return 0;

    if (!Retrorun_UseAnalogStick)
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

    /*if (Retrorun_Core == RETRORUN_CORE_PARALLEL_N64)
    {
        // Map thumbstick to dpad (force to enable the right analog stick mapping to it the DPAD)
        const float TRIM = 0.35f;
        go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Right);

        if (thumb.y < -TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadUp, ButtonState_Pressed);
        if (thumb.y > TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadDown, ButtonState_Pressed);
        if (thumb.x < -TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadLeft, ButtonState_Pressed);
        if (thumb.x > TRIM)
            go2_input_state_button_set(gamepadState, Go2InputButton_DPadRight, ButtonState_Pressed);
    }*/
    else if (isTate)
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
    }

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
        if (device == RETRO_DEVICE_JOYPAD)
        {

            if (isTate)
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
            }
            switch (id)
            {
            case RETRO_DEVICE_ID_JOYPAD_B:
                return go2_input_state_button_get(gamepadState, Go2InputButton_B);
                break;

            case RETRO_DEVICE_ID_JOYPAD_Y:
                return go2_input_state_button_get(gamepadState, Go2InputButton_Y);
                break;

            case RETRO_DEVICE_ID_JOYPAD_SELECT:
                return go2_input_state_button_get(gamepadState, Go2InputButton_F1);
                break;

            case RETRO_DEVICE_ID_JOYPAD_START:
                return go2_input_state_button_get(gamepadState, startButton);
                break;

            case RETRO_DEVICE_ID_JOYPAD_UP:
                return go2_input_state_button_get(gamepadState, Go2InputButton_DPadUp);
                break;

            case RETRO_DEVICE_ID_JOYPAD_DOWN:
                return go2_input_state_button_get(gamepadState, Go2InputButton_DPadDown);
                break;

            case RETRO_DEVICE_ID_JOYPAD_LEFT:
                return go2_input_state_button_get(gamepadState, Go2InputButton_DPadLeft);
                break;

            case RETRO_DEVICE_ID_JOYPAD_RIGHT:
                return go2_input_state_button_get(gamepadState, Go2InputButton_DPadRight);
                break;

            case RETRO_DEVICE_ID_JOYPAD_A:
                return go2_input_state_button_get(gamepadState, Go2InputButton_A);
                break;

            case RETRO_DEVICE_ID_JOYPAD_X:
                return go2_input_state_button_get(gamepadState, Go2InputButton_X);
                break;

            case RETRO_DEVICE_ID_JOYPAD_L:
                if (has_triggers)
                {
                    return go2_input_state_button_get(gamepadState, Go2InputButton_TopLeft);
                }
                else
                {
                    return opt_triggers ? go2_input_state_button_get(gamepadState, rightAnalogButton) : go2_input_state_button_get(gamepadState, Go2InputButton_TopLeft);
                }
                break;

            case RETRO_DEVICE_ID_JOYPAD_R:
                if (has_triggers)
                {
                    return go2_input_state_button_get(gamepadState, Go2InputButton_TopRight);
                }
                else
                {
                    return opt_triggers ? go2_input_state_button_get(gamepadState, startButton) : go2_input_state_button_get(gamepadState, Go2InputButton_TopRight);
                }
                break;

            case RETRO_DEVICE_ID_JOYPAD_L2:
                if (has_triggers)
                {
                    return go2_input_state_button_get(gamepadState, l2Button);
                }
                else
                {
                    return opt_triggers ? go2_input_state_button_get(gamepadState, Go2InputButton_TopLeft) : go2_input_state_button_get(gamepadState, rightAnalogButton);
                }
                break;

            case RETRO_DEVICE_ID_JOYPAD_R2:
                if (has_triggers)
                {
                    return go2_input_state_button_get(gamepadState, r2Button);
                }
                else
                {
                    return opt_triggers ? go2_input_state_button_get(gamepadState, Go2InputButton_TopRight) : go2_input_state_button_get(gamepadState, startButton);
                }
                break;

            default:
                return 0;
                break;
            }
        }
        else if (Retrorun_UseAnalogStick && device == RETRO_DEVICE_ANALOG && index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
        {

            if (isTate)
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

            go2_thumb_t thumb = go2_input_state_thumbstick_get(gamepadState, Go2InputThumbstick_Left);

            if (thumb.x > 1.0f)
                thumb.x = 1.0f;
            else if (thumb.x < -1.0f)
                thumb.x = -1.0f;

            if (thumb.y > 1.0f)
                thumb.y = 1.0f;
            else if (thumb.y < -1.0f)
                thumb.y = -1.0f;

            if (isTate)
            {
                float temp = thumb.x;
                thumb.x = thumb.y * -1.0f;
                thumb.y = temp;
            }

            switch (id)
            {
            case RETRO_DEVICE_ID_ANALOG_X:
                return thumb.x * 0x7fff;
                break;

            case RETRO_DEVICE_ID_JOYPAD_Y:
                return thumb.y * 0x7fff;
                break;

            default:
                return 0;
                break;
            }
        }
    }

    return 0;
}
