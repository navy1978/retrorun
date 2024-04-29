#pragma once

/*
libgo2 - Support library for the ODROID-GO Advance
Copyright (C) 2020 OtherCrashOverride
Copyright (C) 2023-present  navy1978

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdint.h>


typedef struct 
{
    float x;
    float y;
} go2_thumb_t;

typedef enum 
{
    ButtonState_Released = 0,
    ButtonState_Pressed
} go2_button_state_t;


typedef struct 
{
    go2_button_state_t a;
    go2_button_state_t b;
    go2_button_state_t x;
    go2_button_state_t y;

    go2_button_state_t top_left;
    go2_button_state_t top_right;

    go2_button_state_t f1;
    go2_button_state_t f2;
    go2_button_state_t f3;
    go2_button_state_t f4;
    go2_button_state_t f5;
    go2_button_state_t f6;

} go2_gamepad_buttons_t;

typedef struct 
{
    go2_button_state_t up;
    go2_button_state_t down;
    go2_button_state_t left;
    go2_button_state_t right;
} go2_dpad_t;

typedef struct 
{
    go2_thumb_t thumb;
    go2_dpad_t dpad;
    go2_gamepad_buttons_t buttons;
    float rumble_intensity;
} go2_gamepad_state_t;

typedef struct go2_input go2_input_t;


typedef enum 
{
    Battery_Status_Unknown = 0,
    Battery_Status_Discharging,
    Battery_Status_Charging,
    Battery_Status_Full,

    Battery_Status_MAX = 0x7fffffff
} go2_battery_status_t;

typedef struct
{
    uint32_t level;
    go2_battery_status_t status;
} go2_battery_state_t;

typedef struct
{
    uint32_t level;
} go2_brightness_state_t;



// v1.1 API
typedef enum
{
    Go2InputFeatureFlags_None = (1 << 0),
    Go2InputFeatureFlags_Triggers = (1 << 1),
    Go2InputFeatureFlags_RightAnalog = (1 << 2),
} go2_input_feature_flags_t;

typedef enum
{
    Go2InputThumbstick_Left = 0,
    Go2InputThumbstick_Right
} go2_input_thumbstick_t;

typedef enum
{
    Go2InputButton_DPadUp = 0,
    Go2InputButton_DPadDown,
    Go2InputButton_DPadLeft,
    Go2InputButton_DPadRight,

    Go2InputButton_A,
    Go2InputButton_B,
    Go2InputButton_X,
    Go2InputButton_Y,

    Go2InputButton_F1,
    Go2InputButton_F2,
    Go2InputButton_F3,
    Go2InputButton_F4,
    Go2InputButton_F5,
    Go2InputButton_F6,
    Go2InputButton_F7,
    Go2InputButton_F8,
    Go2InputButton_F9,
    Go2InputButton_F10,
    Go2InputButton_F11,
    Go2InputButton_F12,
    Go2InputButton_F13,
    Go2InputButton_F14,
    Go2InputButton_F15,
    Go2InputButton_F16,


    
    Go2InputButton_SELECT,
    Go2InputButton_START,
    Go2InputButton_THUMBR,
    Go2InputButton_THUMBL,

    Go2InputButton_TopLeft,
    Go2InputButton_TopRight,

    Go2InputButton_TriggerLeft,
    Go2InputButton_TriggerRight
} go2_input_button_t;

typedef struct go2_input_state go2_input_state_t;


#ifdef __cplusplus
extern "C" {
#endif

go2_input_t* go2_input_create(const char* device);
void go2_input_destroy(go2_input_t* input);
void go2_input_gamepad_read(go2_input_t* input, go2_gamepad_state_t* outGamepadState);
void go2_input_battery_read(go2_input_t* input, go2_battery_state_t* outBatteryState);
void go2_input_brightness_read(go2_input_t* input, go2_brightness_state_t* outBrightnessState);

void go2_input_brightness_write(int value);


// v1.1 API
go2_input_feature_flags_t go2_input_features_get(go2_input_t* input);
void go2_input_state_read(go2_input_t* input, go2_input_state_t* outState);


go2_input_state_t* go2_input_state_create();
void go2_input_state_destroy(go2_input_state_t* state);
go2_button_state_t go2_input_state_button_get(go2_input_state_t* state, go2_input_button_t button);
void go2_input_state_button_set(go2_input_state_t* state, go2_input_button_t button, go2_button_state_t value);
go2_thumb_t go2_input_state_thumbstick_get(go2_input_state_t* state, go2_input_thumbstick_t thumbstick);
void go2_input_rumble_start( go2_input_t *input, float intensity) ;

#ifdef __cplusplus
}
#endif