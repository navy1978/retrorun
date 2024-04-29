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

#include "input.h"
#include "hardware.h"
#include "../globals.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <stdbool.h>
#include <pthread.h>

#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/limits.h>

#define BATTERY_BUFFER_SIZE (128)
#define BRIGHTNESS_BUFFER_SIZE (128)
// joypad
static const char *EVDEV_NAME = "/dev/input/by-path/platform-odroidgo2-joypad-event-joystick";
static const char *EVDEV_NAME_2 = "/dev/input/by-path/platform-odroidgo3-joypad-event-joystick";
static const char* EVDEV_NAME_3 = "/dev/input/by-path/platform-singleadc-joypad-event-joystick";
// battery
static const char *BATTERY_STATUS_NAME = "/sys/class/power_supply/battery/status";
static const char *BATTERY_CAPACITY_NAME = "/sys/class/power_supply/battery/capacity";
static const char *BATTERY_STATUS_NAME_2 = "/sys/class/power_supply/bat/status";
static const char *BATTERY_CAPACITY_NAME_2 = "/sys/class/power_supply/bat/capacity";
// brightness
static const char *BRIGHTNESS_VALUE_NAME = "/sys/class/backlight/backlight/brightness";

#define GO2_THUMBSTICK_COUNT (Go2InputThumbstick_Right + 1)
#define GO2_BUTTON_COUNT (Go2InputButton_TriggerRight + 1)

typedef struct go2_input_state
{
    go2_thumb_t thumbs[GO2_THUMBSTICK_COUNT];
    go2_button_state_t buttons[GO2_BUTTON_COUNT];
    float rumble_intensity;
} go2_input_state_t;

typedef struct go2_input
{
    int fd;
    struct libevdev *dev;
    go2_input_state_t current_state;
    go2_input_state_t pending_state;
    pthread_mutex_t gamepadMutex;
    pthread_mutex_t batteryMutex;
    pthread_mutex_t brightnessMutex;
    pthread_t thread_id;
    go2_battery_state_t current_battery;
    pthread_t battery_thread;
    go2_brightness_state_t current_brightness;
    pthread_t brightness_thread;
    pthread_t volume_thread;
    bool terminating;
    const char *device;
} go2_input_t;

static void *battery_task(void *arg)
{

    go2_input_t *input = (go2_input_t *)arg;
    int fd;
    void *result = 0;
    char buffer[BATTERY_BUFFER_SIZE + 1];
    go2_battery_state_t battery;

    memset(&battery, 0, sizeof(battery));

    const char *batteryStatus = isRG552() ? BATTERY_STATUS_NAME_2 : BATTERY_STATUS_NAME;
    const char *batteryCapacity = isRG552() ? BATTERY_CAPACITY_NAME_2 : BATTERY_CAPACITY_NAME;

    while (!input->terminating)
    {
        fd = open(batteryStatus, O_RDONLY);
        if (fd > 0)
        {
            memset(buffer, 0, BATTERY_BUFFER_SIZE + 1);
            ssize_t count = read(fd, buffer, BATTERY_BUFFER_SIZE);
            if (count > 0)
            {
                // printf("BATT: buffer='%s'\n", buffer);

                if (buffer[0] == 'D')
                {
                    battery.status = Battery_Status_Discharging;
                }
                else if (buffer[0] == 'C')
                {
                    battery.status = Battery_Status_Charging;
                }
                else if (buffer[0] == 'F')
                {
                    battery.status = Battery_Status_Full;
                }
                else
                {
                    battery.status = Battery_Status_Unknown;
                }
            }

            close(fd);
        }

        fd = open(batteryCapacity, O_RDONLY);
        if (fd > 0)
        {
            memset(buffer, 0, BATTERY_BUFFER_SIZE + 1);
            ssize_t count = read(fd, buffer, BATTERY_BUFFER_SIZE);
            if (count > 0)
            {
                battery.level = atoi(buffer);
            }
            else
            {
                battery.level = 0;
            }

            close(fd);
        }

        pthread_mutex_lock(&input->batteryMutex);

        input->current_battery = battery;

        pthread_mutex_unlock(&input->batteryMutex);

        // printf("BATT: status=%d, level=%d\n", input->current_battery.status, input->current_battery.level);

        sleep(1);
    }

    // printf("BATT: exit.\n");
    return result;
}

static void *brightness_task(void *arg)
{

    go2_input_t *input = (go2_input_t *)arg;
    int fd;
    void *result = 0;
    char buffer[BRIGHTNESS_BUFFER_SIZE + 1];
    go2_brightness_state_t brightness;

    memset(&brightness, 0, sizeof(brightness));

    const char *brightnessValue = BRIGHTNESS_VALUE_NAME;

    while (!input->terminating)
    {

        fd = open(brightnessValue, O_RDONLY);
        if (fd > 0)
        {
            memset(buffer, 0, BRIGHTNESS_BUFFER_SIZE + 1);
            ssize_t count = read(fd, buffer, BRIGHTNESS_BUFFER_SIZE);
            if (count > 0)
            {
                brightness.level = atoi(buffer);
            }
            else
            {
                brightness.level = 0;
            }

            close(fd);
        }

        pthread_mutex_lock(&input->brightnessMutex);

        input->current_brightness = brightness;

        pthread_mutex_unlock(&input->brightnessMutex);

        // printf("BRIGHTNESS: level=%d\n",  input->current_brightness.level);

        sleep(1);
    }

    // printf("BRIGHTNESS: exit.\n");
    return result;
}

static void *input_task(void *arg)
{
    go2_input_t *input = (go2_input_t *)arg;

    if (!input->dev)
        return NULL;

    const int abs_x_max = libevdev_get_abs_maximum(input->dev, ABS_X);
    const int abs_y_max = libevdev_get_abs_maximum(input->dev, ABS_Y);

    const int abs_rx_max = libevdev_get_abs_maximum(input->dev, ABS_RX);
    const int abs_ry_max = libevdev_get_abs_maximum(input->dev, ABS_RY);

    // printf("abs: x_max=%d, y_max=%d\n", abs_x_max, abs_y_max);

    // Get current state
    input->current_state.buttons[Go2InputButton_DPadUp] = libevdev_get_event_value(input->dev, EV_KEY, BTN_DPAD_UP) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_DPadDown] = libevdev_get_event_value(input->dev, EV_KEY, BTN_DPAD_DOWN) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_DPadLeft] = libevdev_get_event_value(input->dev, EV_KEY, BTN_DPAD_LEFT) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_DPadRight] = libevdev_get_event_value(input->dev, EV_KEY, BTN_DPAD_RIGHT) ? ButtonState_Pressed : ButtonState_Released;

    input->current_state.buttons[Go2InputButton_A] = libevdev_get_event_value(input->dev, EV_KEY, BTN_EAST) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_B] = libevdev_get_event_value(input->dev, EV_KEY, BTN_SOUTH) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_X] = libevdev_get_event_value(input->dev, EV_KEY, BTN_NORTH) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_Y] = libevdev_get_event_value(input->dev, EV_KEY, BTN_WEST) ? ButtonState_Pressed : ButtonState_Released;

    input->current_state.buttons[Go2InputButton_TopLeft] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TL) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_TopRight] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TR) ? ButtonState_Pressed : ButtonState_Released;

    input->current_state.buttons[Go2InputButton_SELECT] = libevdev_get_event_value(input->dev, EV_KEY, BTN_SELECT) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_START] = libevdev_get_event_value(input->dev, EV_KEY, BTN_START) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_THUMBL] = libevdev_get_event_value(input->dev, EV_KEY, BTN_THUMBL) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_THUMBR] = libevdev_get_event_value(input->dev, EV_KEY, BTN_THUMBR) ? ButtonState_Pressed : ButtonState_Released;




    input->current_state.buttons[Go2InputButton_F1] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY1) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F2] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY2) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F3] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY3) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F4] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY4) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F5] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY5) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F6] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY6) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F7] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY7) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F8] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY8) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F9] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY9) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F10] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY10) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F11] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY11) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F12] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY12) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F13] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY13) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F14] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY14) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F15] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY15) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_F16] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY16) ? ButtonState_Pressed : ButtonState_Released;

    input->current_state.buttons[Go2InputButton_TriggerLeft] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TL2) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons[Go2InputButton_TriggerRight] = libevdev_get_event_value(input->dev, EV_KEY, BTN_TR2) ? ButtonState_Pressed : ButtonState_Released;

    input->current_state.thumbs[Go2InputThumbstick_Left].x = libevdev_get_event_value(input->dev, EV_ABS, ABS_X) / (float)abs_x_max;
    input->current_state.thumbs[Go2InputThumbstick_Left].y = libevdev_get_event_value(input->dev, EV_ABS, ABS_Y) / (float)abs_y_max;

    input->current_state.thumbs[Go2InputThumbstick_Right].x = libevdev_get_event_value(input->dev, EV_ABS, ABS_RX) / (float)abs_rx_max;
    input->current_state.thumbs[Go2InputThumbstick_Right].y = libevdev_get_event_value(input->dev, EV_ABS, ABS_RY) / (float)abs_ry_max;

    // Events
    while (!input->terminating)
    {
        /* EAGAIN is returned when the queue is empty */
        struct input_event ev;
        int rc = libevdev_next_event(input->dev, LIBEVDEV_READ_FLAG_BLOCKING, &ev);
        if (rc == 0)
        {
#if 0
			printf("Gamepad Event: %s-%s(%d)=%d\n",
			       libevdev_event_type_get_name(ev.type),
			       libevdev_event_code_get_name(ev.type, ev.code), ev.code,
			       ev.value);
#endif

            if (ev.type == EV_FF) { // Evento rumble
                if (ev.code == FF_RUMBLE) {
                    // Calcola l'intensità del rumble
                    float rumble_intensity = (float)ev.value / 255.0f;
                    
                    // Aggiorna lo stato del rumble
                    pthread_mutex_lock(&input->dev->gamepadMutex);
                    input->dev->current_state.rumble_intensity = rumble_intensity;
                    pthread_mutex_unlock(&input->dev->gamepadMutex);
                }
            }
            else if (ev.type == EV_KEY)
            {
                go2_button_state_t state = ev.value ? ButtonState_Pressed : ButtonState_Released;

                switch (ev.code)
                {
                case BTN_DPAD_UP:
                    input->pending_state.buttons[Go2InputButton_DPadUp] = state;
                    break;
                case BTN_DPAD_DOWN:
                    input->pending_state.buttons[Go2InputButton_DPadDown] = state;
                    break;
                case BTN_DPAD_LEFT:
                    input->pending_state.buttons[Go2InputButton_DPadLeft] = state;
                    break;
                case BTN_DPAD_RIGHT:
                    input->pending_state.buttons[Go2InputButton_DPadRight] = state;
                    break;

                case BTN_EAST:
                    input->pending_state.buttons[Go2InputButton_A] = state;
                    break;
                case BTN_SOUTH:
                    input->pending_state.buttons[Go2InputButton_B] = state;
                    break;
                case BTN_NORTH:
                    input->pending_state.buttons[Go2InputButton_X] = state;
                    break;
                case BTN_WEST:
                    input->pending_state.buttons[Go2InputButton_Y] = state;
                    break;

                case BTN_TL:
                    input->pending_state.buttons[Go2InputButton_TopLeft] = state;
                    break;
                case BTN_TR:
                    input->pending_state.buttons[Go2InputButton_TopRight] = state;
                    break;

                case BTN_SELECT:
                    input->pending_state.buttons[Go2InputButton_SELECT] = state;
                    break;                    
                case BTN_START:          
                    input->pending_state.buttons[Go2InputButton_START] = state;
                    break;

                case BTN_THUMBR:
                    input->pending_state.buttons[Go2InputButton_THUMBR] = state;
                    break;                    
                case BTN_THUMBL:          
                    input->pending_state.buttons[Go2InputButton_THUMBL] = state;
                    break;    

                case BTN_TRIGGER_HAPPY1:
                    input->pending_state.buttons[Go2InputButton_F1] = state;
                    break;
                case BTN_TRIGGER_HAPPY2:
                    input->pending_state.buttons[Go2InputButton_F2] = state;
                    break;
                case BTN_TRIGGER_HAPPY3:
                    input->pending_state.buttons[Go2InputButton_F3] = state;
                    break;
                case BTN_TRIGGER_HAPPY4:
                    input->pending_state.buttons[Go2InputButton_F4] = state;
                    break;
                case BTN_TRIGGER_HAPPY5:
                    input->pending_state.buttons[Go2InputButton_F5] = state;
                    break;
                case BTN_TRIGGER_HAPPY6:
                    input->pending_state.buttons[Go2InputButton_F6] = state;
                    break;
                case BTN_TRIGGER_HAPPY7:
                    input->pending_state.buttons[Go2InputButton_F7] = state;
                    break;
                case BTN_TRIGGER_HAPPY8:
                    input->pending_state.buttons[Go2InputButton_F8] = state;
                    break;
                case BTN_TRIGGER_HAPPY9:
                    input->pending_state.buttons[Go2InputButton_F9] = state;
                    break;
                case BTN_TRIGGER_HAPPY10:
                    input->pending_state.buttons[Go2InputButton_F10] = state;
                    break;
                case BTN_TRIGGER_HAPPY11:
                    input->pending_state.buttons[Go2InputButton_F11] = state;
                    break;
                case BTN_TRIGGER_HAPPY12:
                    input->pending_state.buttons[Go2InputButton_F12] = state;
                    break;
                case BTN_TRIGGER_HAPPY13:
                    input->pending_state.buttons[Go2InputButton_F13] = state;
                    break;
                case BTN_TRIGGER_HAPPY14:
                    input->pending_state.buttons[Go2InputButton_F14] = state;
                    break;
                case BTN_TRIGGER_HAPPY15:
                    input->pending_state.buttons[Go2InputButton_F15] = state;
                    break;
                case BTN_TRIGGER_HAPPY16:
                    input->pending_state.buttons[Go2InputButton_F16] = state;
                    break;

                case BTN_TL2:
                    input->pending_state.buttons[Go2InputButton_TriggerLeft] = state;
                    break;
                case BTN_TR2:
                    input->pending_state.buttons[Go2InputButton_TriggerRight] = state;
                    break;
                }
            }
            else if (ev.type == EV_ABS)
            {
                switch (ev.code)
                {
                case ABS_X:
                    input->pending_state.thumbs[Go2InputThumbstick_Left].x = ev.value / (float)abs_x_max;
                    break;
                case ABS_Y:
                    input->pending_state.thumbs[Go2InputThumbstick_Left].y = ev.value / (float)abs_y_max;
                    break;

                case ABS_RX:
                    input->pending_state.thumbs[Go2InputThumbstick_Right].x = ev.value / (float)abs_rx_max;
                    break;
                case ABS_RY:
                    input->pending_state.thumbs[Go2InputThumbstick_Right].y = ev.value / (float)abs_ry_max;
                    break;
                }
            }
            else if (ev.type == EV_SYN)
            {
                pthread_mutex_lock(&input->gamepadMutex);

                input->current_state = input->pending_state;

                pthread_mutex_unlock(&input->gamepadMutex);
            }
        }
    }

    return NULL;
}

go2_input_t *go2_input_create(const char *device)
{

    int rc = 1;

    go2_input_t *result = (go2_input_t *)malloc(sizeof(*result));
    if (!result)
    {
        printf("malloc failed.\n");
        return NULL;
    }

    memset(result, 0, sizeof(*result));

    result->device = device;

    result->fd = open(EVDEV_NAME, O_RDONLY);
    if (result->fd < 0)
    {
        if (isRG503()){
            result->fd = open(EVDEV_NAME_3, O_RDONLY);
        }else{
            result->fd = open(EVDEV_NAME_2, O_RDONLY);
        }
        if (result->fd < 0)
        {
            printf("Joystick: No gamepad found.\n");
        }
    }

    if (result->fd > -1)
    {
        rc = libevdev_new_from_fd(result->fd, &result->dev);
        if (rc < 0)
        {
            printf("Joystick: Failed to init libevdev (%s)\n", strerror(-rc));
            close(result->fd);
            free(result);
            return NULL;
        }

        memset(&result->current_state, 0, sizeof(result->current_state));
        memset(&result->pending_state, 0, sizeof(result->pending_state));

        // printf("Input device name: \"%s\"\n", libevdev_get_name(result->dev));
        // printf("Input device ID: bus %#x vendor %#x product %#x\n",
        //     libevdev_get_id_bustype(result->dev),
        //     libevdev_get_id_vendor(result->dev),
        //     libevdev_get_id_product(result->dev));

        if (pthread_create(&result->thread_id, NULL, input_task, (void *)result) < 0)
        {
            printf("could not create input_task thread\n");
            libevdev_free(result->dev);
            close(result->fd);
            free(result);
            return NULL;
        }

        if (pthread_create(&result->battery_thread, NULL, battery_task, (void *)result) < 0)
        {
            printf("could not create battery_task thread\n");
        }

        if (pthread_create(&result->brightness_thread, NULL, brightness_task, (void *)result) < 0)
        {
            printf("could not create brightness_task thread\n");
        }
    }

    return result;

    /*
    err_01:
        libevdev_free(result->dev);

    err_00:
        close(result->fd);
        free(result);

    out:
        return NULL;*/
}

void go2_input_destroy(go2_input_t *input)
{
    input->terminating = true;

    pthread_cancel(input->thread_id);

    pthread_join(input->thread_id, NULL);
    pthread_join(input->battery_thread, NULL);
    pthread_join(input->brightness_thread, NULL);

    libevdev_free(input->dev);
    close(input->fd);
    free(input);
}

void go2_input_gamepad_read(go2_input_t *input, go2_gamepad_state_t *outGamepadState)
{


// Leggi lo stato del rumble da input->current_state.rumble_intensity
    pthread_mutex_lock(&input->gamepadMutex);
    float rumble_intensity = input->current_state.rumble_intensity;
    pthread_mutex_unlock(&input->gamepadMutex);

    pthread_mutex_lock(&input->gamepadMutex);

    outGamepadState->thumb.x = input->current_state.thumbs[Go2InputThumbstick_Left].x;
    outGamepadState->thumb.y = input->current_state.thumbs[Go2InputThumbstick_Left].y;

    outGamepadState->dpad.up = input->current_state.buttons[Go2InputButton_DPadUp];
    outGamepadState->dpad.down = input->current_state.buttons[Go2InputButton_DPadDown];
    outGamepadState->dpad.left = input->current_state.buttons[Go2InputButton_DPadLeft];
    outGamepadState->dpad.right = input->current_state.buttons[Go2InputButton_DPadRight];

    outGamepadState->buttons.a = input->current_state.buttons[Go2InputButton_A];
    outGamepadState->buttons.b = input->current_state.buttons[Go2InputButton_B];
    outGamepadState->buttons.x = input->current_state.buttons[Go2InputButton_X];
    outGamepadState->buttons.y = input->current_state.buttons[Go2InputButton_Y];

    outGamepadState->buttons.top_left = input->current_state.buttons[Go2InputButton_TopLeft];
    outGamepadState->buttons.top_right = input->current_state.buttons[Go2InputButton_TopRight];

    outGamepadState->buttons.f1 = input->current_state.buttons[Go2InputButton_F1];
    outGamepadState->buttons.f2 = input->current_state.buttons[Go2InputButton_F2];
    outGamepadState->buttons.f3 = input->current_state.buttons[Go2InputButton_F3];
    outGamepadState->buttons.f4 = input->current_state.buttons[Go2InputButton_F4];
    outGamepadState->buttons.f5 = input->current_state.buttons[Go2InputButton_F5];
    outGamepadState->buttons.f6 = input->current_state.buttons[Go2InputButton_F6];

    pthread_mutex_unlock(&input->gamepadMutex);

       // Includi lo stato del rumble nella struttura di output
    outGamepadState->rumble_intensity = rumble_intensity;
}

void go2_input_battery_read(go2_input_t *input, go2_battery_state_t *outBatteryState)
{
    pthread_mutex_lock(&input->batteryMutex);

    *outBatteryState = input->current_battery;

    pthread_mutex_unlock(&input->batteryMutex);
}

void go2_input_brightness_write(int value)
{
    FILE *brightness_file = fopen(BRIGHTNESS_VALUE_NAME, "w");
    if (brightness_file == NULL)
    {
        perror("Cannot open file for brightness");
        return;
    }

    if (fprintf(brightness_file, "%d", value) < 0)
    {
        perror("Error writing value for brightness");
        return;
    }

    if (fclose(brightness_file) != 0)
    {
        perror("Error closing file for brightness");
        return;
    }

    printf("Brightness set to %d\n", value);

    return;
}

void go2_input_brightness_read(go2_input_t *input, go2_brightness_state_t *outBrightnessState)
{
    pthread_mutex_lock(&input->brightnessMutex);

    *outBrightnessState = input->current_brightness;

    pthread_mutex_unlock(&input->brightnessMutex);
}

// v1.1 API
go2_input_feature_flags_t go2_input_features_get(go2_input_t *input)
{
    go2_input_feature_flags_t result = Go2InputFeatureFlags_None;

    // if (go2_hardware_revision_get() == Go2HardwareRevision_V1_1)

    if (libevdev_has_event_code(input->dev, EV_KEY, BTN_TL2) &&
        libevdev_has_event_code(input->dev, EV_KEY, BTN_TR2))
    {
        int resultInt = (int)result;
        resultInt |= Go2InputFeatureFlags_Triggers;
        result = (go2_input_feature_flags_t)resultInt;
    }

    if (libevdev_has_event_code(input->dev, EV_ABS, ABS_RX) &&
        libevdev_has_event_code(input->dev, EV_ABS, ABS_RY))
    {

        int resultInt = (int)result;
        resultInt |= Go2InputFeatureFlags_RightAnalog;
        result = (go2_input_feature_flags_t)resultInt;
    }

    return result;
}

void go2_input_state_read(go2_input_t *input, go2_input_state_t *outState)
{
    *outState = input->current_state;
}

go2_input_state_t *go2_input_state_create()
{
    go2_input_state_t *result = NULL;

    result = (go2_input_state_t *)malloc(sizeof(*result));
    if (result)
    {
        memset(result, 0, sizeof(*result));
    }

    return result;
}

void go2_input_state_destroy(go2_input_state_t *state)
{
    free(state);
}

go2_button_state_t go2_input_state_button_get(go2_input_state_t *state, go2_input_button_t button)
{
    return state->buttons[button];
}

void go2_input_state_button_set(go2_input_state_t *state, go2_input_button_t button, go2_button_state_t value)
{
    state->buttons[button] = value;
}

go2_thumb_t go2_input_state_thumbstick_get(go2_input_state_t *state, go2_input_thumbstick_t thumbstick)
{
    return state->thumbs[thumbstick];
}


void go2_input_rumble_start(go2_input_t *input, float intensity) {
    // Verifica che l'intensità sia nel range valido [0, 1]
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    // Calcola il valore da 0 a 255 corrispondente all'intensità del rumble
    int rumble_value = (int)(intensity * 255.0f);

    // Prepara l'evento di rumble
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = EV_FF;
    event.code = FF_RUMBLE;
    event.value = rumble_value;

    // Invia l'evento di rumble al dispositivo
    if (write(input->fd, &event, sizeof(event)) < 0) {
        perror("Error sending rumble event");
        return;
    }

    // Aggiorna lo stato del rumble nella struttura input->current_state
    pthread_mutex_lock(&input->gamepadMutex);
    input->current_state.rumble_intensity = intensity;
    pthread_mutex_unlock(&input->gamepadMutex);
}

void go2_input_rumble_stop(go2_input_t *input) {
    // Prepara un evento per arrestare il rumble
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = EV_FF;
    event.code = FF_RUMBLE;
    event.value = 0;

    // Invia l'evento di rumble al dispositivo per arrestare il rumble
    if (write(input->fd, &event, sizeof(event)) < 0) {
        perror("Error stopping rumble event");
        return;
    }

    // Azzera lo stato del rumble nella struttura input->current_state
    pthread_mutex_lock(&input->gamepadMutex);
    input->current_state.rumble_intensity = 0.0f;
    pthread_mutex_unlock(&input->gamepadMutex);
}
