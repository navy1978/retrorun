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
#include "video-helper.h"
#include "libretro.h"
#include "keyboard.h"
#include "file_browser.h"
#include "achievements.h"

#include "platform.h"
#include <stdio.h>
#include <sys/time.h>
#include <algorithm>
#include <array>
#include <cctype>

extern int opt_backlight;
extern int opt_volume;
bool input_ffwd_requested = false;
static retro_fastforwarding_override fastForwardOverride = {-1.0f, false, true, false};
static bool fastForwardOverrideSet = false;
bool input_message = false;
static std::array<bool, 16> coreAnalogRequested{};

const char* analogToDigitalModeName(AnalogToDigital mode)
{
    switch (mode) {
    case NONE: return "none";
    case LEFT_ANALOG: return "left";
    case RIGHT_ANALOG: return "right";
    case LEFT_ANALOG_FORCED: return "left_forced";
    case RIGHT_ANALOG_FORCED: return "right_forced";
    default: return "none";
    }
}

bool setAnalogToDigitalMode(const std::string& value)
{
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::replace(normalized.begin(), normalized.end(), '-', '_');
    std::replace(normalized.begin(), normalized.end(), ' ', '_');

    if (normalized == "none" || normalized == "disabled" || normalized == "0")
        analogToDigital = NONE;
    else if (normalized == "left" || normalized == "left_analog" || normalized == "1")
        analogToDigital = LEFT_ANALOG;
    else if (normalized == "right" || normalized == "right_analog" || normalized == "2")
        analogToDigital = RIGHT_ANALOG;
    else if (normalized == "left_forced" || normalized == "left_analog_forced" || normalized == "3")
        analogToDigital = LEFT_ANALOG_FORCED;
    else if (normalized == "right_forced" || normalized == "right_analog_forced" || normalized == "4")
        analogToDigital = RIGHT_ANALOG_FORCED;
    else
        return false;

    force_left_analog_stick = analogToDigital == LEFT_ANALOG_FORCED;
    return true;
}

AnalogToDigital effectiveAnalogToDigitalMode(unsigned port)
{
    if ((analogToDigital == LEFT_ANALOG || analogToDigital == RIGHT_ANALOG) &&
        port < coreAnalogRequested.size() && coreAnalogRequested[port])
        return NONE;
    return analogToDigital;
}

static bool analogToDigitalDirectionPressed(const rr_thumb_t& thumb, unsigned id)
{
    constexpr float threshold = 0.35f;

    if (!isTate()) {
        if (id == RETRO_DEVICE_ID_JOYPAD_UP) return thumb.y < -threshold;
        if (id == RETRO_DEVICE_ID_JOYPAD_DOWN) return thumb.y > threshold;
        if (id == RETRO_DEVICE_ID_JOYPAD_LEFT) return thumb.x < -threshold;
        return thumb.x > threshold;
    }

    // Use the same physical-to-logical rotation as getInputUp/Down/Left/Right.
    if (tateState == REVERSED) {
        if (id == RETRO_DEVICE_ID_JOYPAD_UP) return thumb.x > threshold;
        if (id == RETRO_DEVICE_ID_JOYPAD_DOWN) return thumb.x < -threshold;
        if (id == RETRO_DEVICE_ID_JOYPAD_LEFT) return thumb.y < -threshold;
        return thumb.y > threshold;
    }

    if (id == RETRO_DEVICE_ID_JOYPAD_UP) return thumb.x < -threshold;
    if (id == RETRO_DEVICE_ID_JOYPAD_DOWN) return thumb.x > threshold;
    if (id == RETRO_DEVICE_ID_JOYPAD_LEFT) return thumb.y > threshold;
    return thumb.y < -threshold;
}

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
rr_battery_state_t batteryState;
rr_brightness_state_t brightnessState;

static rr_input_state_t *gamepadState;
static rr_input_state_t *prevGamepadState;
static rr_input_t *input;
static bool has_triggers = false;
static bool has_right_analog = false;
//static bool isTate = false;
// static unsigned lastId = 0;


static rr_input_button_t upButton;
static rr_input_button_t downButton;
static rr_input_button_t leftButton;
static rr_input_button_t rightButton;

static rr_input_button_t aButton;
static rr_input_button_t bButton;
static rr_input_button_t xButton;
static rr_input_button_t yButton;

static rr_input_button_t selectButton;
static rr_input_button_t startButton;
static rr_input_button_t l1Button;
static rr_input_button_t r1Button;
static rr_input_button_t l2Button;
static rr_input_button_t r2Button;
static rr_input_button_t l3Button;
static rr_input_button_t r3Button;
// these are special:
static rr_input_button_t f1Button;
static rr_input_button_t f2Button;
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


rr_input_button_t stringToGo2Button(const std::string& str) {
    if (str == "DPadUp") return RRInputButton_DPadUp;
    if (str == "DPadDown") return RRInputButton_DPadDown;
    if (str == "DPadLeft") return RRInputButton_DPadLeft;
    if (str == "DPadRight") return RRInputButton_DPadRight;
    if (str == "A") return RRInputButton_A;
    if (str == "B") return RRInputButton_B;
    if (str == "X") return RRInputButton_X;
    if (str == "Y") return RRInputButton_Y;
    if (str == "F1") return RRInputButton_F1;
    if (str == "F2") return RRInputButton_F2;
    if (str == "F3") return RRInputButton_F3;
    if (str == "F4") return RRInputButton_F4;
    if (str == "F5") return RRInputButton_F5;
    if (str == "F6") return RRInputButton_F6;
    if (str == "F7") return RRInputButton_F7;
    if (str == "F8") return RRInputButton_F8;
    if (str == "F9") return RRInputButton_F9;
    if (str == "F10") return RRInputButton_F10;
    if (str == "F11") return RRInputButton_F11;
    if (str == "F12") return RRInputButton_F12;
    if (str == "START") return RRInputButton_START;
    if (str == "SELECT") return RRInputButton_SELECT;
    if (str == "TopLeft") return RRInputButton_TopLeft;
    if (str == "TopRight") return RRInputButton_TopRight;
    if (str == "TriggerLeft") return RRInputButton_TriggerLeft;
    if (str == "TriggerRight") return RRInputButton_TriggerRight;
    if (str == "THUMBL") return RRInputButton_THUMBL;
    if (str == "THUMBR") return RRInputButton_THUMBR;

    return RRInputButton_A; // fallback
}



void applyButtonRemapping()
{
    logger.log(Logger::DEB, "Default buttons remapping if it's the case...");
    
    for (const auto& [key, value] : conf_map)
    {
        rr_input_button_t button = stringToGo2Button(value);

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
    upButton =RRInputButton_DPadUp;
    downButton =RRInputButton_DPadDown;
    leftButton =RRInputButton_DPadLeft;
    rightButton =RRInputButton_DPadRight;

    aButton = RRInputButton_A;
    bButton = RRInputButton_B;
    xButton = RRInputButton_X;
    yButton = RRInputButton_Y;

    selectButton = RRInputButton_F1;
    startButton = RRInputButton_F6;
    l1Button = RRInputButton_TopLeft;
    r1Button = RRInputButton_TopRight;
    l2Button = RRInputButton_F4;
    r2Button = RRInputButton_F3;
    l3Button = RRInputButton_F2;
    r3Button = RRInputButton_F5;
    // these are special:
    //f1Button = RRInputButton_F1;
    // this is problematic! ask why F2 is needed in alternative stuff

     // if the device has a gpio_joypad (RG351MP) some buttons are reverted
     if (gpio_joypad == true){
         startButton = RRInputButton_F2;
         l2Button = RRInputButton_TriggerLeft;
         r2Button = RRInputButton_TriggerRight;
         l3Button = RRInputButton_F3;
         r3Button = RRInputButton_F4;
         logger.log(Logger::DEB, "gpio_joypad type detected");
     }else{
        f2Button = RRInputButton_F2;
        ignoreF2 = false;
     }
 
     if ( isRG503()||isRG353V() || isRG353M() )
     {
         l2Button = RRInputButton_TriggerLeft;
         r2Button = RRInputButton_TriggerRight;
         selectButton = RRInputButton_SELECT; // check if this is ok!
         startButton = RRInputButton_START;
         l3Button = RRInputButton_THUMBL;
         r3Button = RRInputButton_THUMBR;
         f2Button = RRInputButton_F2;
         ignoreF2 = false;
         logger.log(Logger::DEB, "RG503/RG353 joypad configuration detected.");
     }else{
        logger.log(Logger::DEB, "RG351 joypad configuration detected.");
     }

#ifdef RR_PLATFORM_SDL
     if (isRG351V()) {
         // Keep exactly the same portable layout produced by GO2/js2xbox.
         selectButton = RRInputButton_F1;
         startButton = RRInputButton_F6;
         l1Button = RRInputButton_TopLeft;
         r1Button = RRInputButton_TopRight;
         l2Button = RRInputButton_F4;
         r2Button = RRInputButton_F3;
         l3Button = RRInputButton_F2;
         r3Button = RRInputButton_F5;
         f2Button = RRInputButton_F2;
         ignoreF2 = false;
         logger.log(Logger::DEB, "SDL2 RG351V uses the GO2 F1-F6 physical layout.");
     } else {
         // Standard SDL controller layout for desktop and newer handhelds.
         selectButton = RRInputButton_SELECT;
         startButton = RRInputButton_START;
         l1Button = RRInputButton_TopLeft;
         r1Button = RRInputButton_TopRight;
         l2Button = RRInputButton_TriggerLeft;
         r2Button = RRInputButton_TriggerRight;
         l3Button = RRInputButton_THUMBL;
         r3Button = RRInputButton_THUMBR;
         ignoreF2 = true;
         logger.log(Logger::DEB, "SDL2 logical joypad configuration detected.");
     }
#endif
 
     applyButtonRemapping();
 
}




void input_gamepad_read()
{

    if (!input)
    {
        if (firstExecution){
            initButtons();
            input = rr_input_create(getDeviceName());
            firstExecution = false;
        }
        // I think this part has no sense: the has_triggered it's false even when should be true...
        if (rr_input_features_get(input) & RRInputFeatureFlags_Triggers)
        {
            has_triggers = true;
            logger.log(Logger::DEB, "input: Hardware triggers enabled.");
        }else{
            logger.log(Logger::DEB, "input: Hardware triggers disabled.");
        }

        if (rr_input_features_get(input) & RRInputFeatureFlags_RightAnalog)
        {
            has_right_analog = true;
            logger.log(Logger::DEB, "input: Right analog enabled.");
        }else{
            logger.log(Logger::DEB, "input: Right analog disabled.");
        }

        gamepadState = rr_input_state_create();
        prevGamepadState = rr_input_state_create();
    }

    // Swap current/previous state
    rr_input_state_t *tempState = prevGamepadState;
    prevGamepadState = gamepadState;
    gamepadState = tempState;

    rr_input_state_read(input, gamepadState);
}

rr_input_state_t *input_gampad_current_get()
{
    return gamepadState;
}

void manageCredits(){
setCreditsAccelerated(
    rr_input_state_button_get(gamepadState, aButton) == RRButtonState_Pressed);
if (rr_input_state_button_get(gamepadState, bButton) == RRButtonState_Pressed &&
    rr_input_state_button_get(prevGamepadState, bButton) == RRButtonState_Released)
    {
        menuManager.handle_input_credits(B_BUTTON);
    }
}

void fastForwardSetOverride(const retro_fastforwarding_override* override_state)
{
    if (!override_state)
        return;
    fastForwardOverride = *override_state;
    fastForwardOverrideSet = true;
    input_ffwd_requested = fastForwardOverride.fastforward;
    logger.log(Logger::DEB,
               "Fast-forward override: active=%s ratio=%.2f notification=%s inhibit_toggle=%s",
               input_ffwd_requested ? "true" : "false", fastForwardOverride.ratio,
               fastForwardOverride.notification ? "true" : "false",
               fastForwardOverride.inhibit_toggle ? "true" : "false");
}

void fastForwardResetOverride()
{
    fastForwardOverride = {-1.0f, false, true, false};
    fastForwardOverrideSet = false;
    input_ffwd_requested = false;
}

bool fastForwardToggleAllowed()
{
    return !fastForwardOverrideSet || !fastForwardOverride.inhibit_toggle;
}

float fastForwardRatio()
{
    if (fastForwardOverrideSet && fastForwardOverride.fastforward &&
        fastForwardOverride.ratio >= 0.0f)
        return fastForwardOverride.ratio;
    return 0.0f;
}

bool fastForwardNotificationVisible()
{
    return input_ffwd_requested &&
           (!fastForwardOverrideSet || !fastForwardOverride.fastforward ||
            fastForwardOverride.notification);
}

void manageMenu()
{
    struct RepeatState {
        bool held = false;
        std::chrono::steady_clock::time_point next;
    };
    static RepeatState up_repeat, down_repeat, left_repeat, right_repeat;
    const auto now = std::chrono::steady_clock::now();

    auto pressed_edge = [](rr_input_button_t button) {
        return rr_input_state_button_get(gamepadState, button) == RRButtonState_Pressed &&
               rr_input_state_button_get(prevGamepadState, button) == RRButtonState_Released;
    };
    auto direction_trigger = [&](rr_input_button_t button, RepeatState& repeat) {
        const bool pressed = rr_input_state_button_get(gamepadState, button) == RRButtonState_Pressed;
        if (!pressed) { repeat.held = false; return false; }
        if (!repeat.held) {
            repeat.held = true;
            repeat.next = now + std::chrono::milliseconds(350);
            return true;
        }
        if (now >= repeat.next) {
            repeat.next = now + std::chrono::milliseconds(90);
            return true;
        }
        return false;
    };

    if (pressed_edge(bButton)) { menuManager.handle_input(B_BUTTON); return; }
    if (pressed_edge(aButton)) { menuManager.handle_input(A_BUTTON); return; }
    if (direction_trigger(upButton, up_repeat)) menuManager.handle_input(UP);
    if (direction_trigger(downButton, down_repeat)) menuManager.handle_input(DOWN);
    if (direction_trigger(leftButton, left_repeat)) menuManager.handle_input(LEFT);
    if (direction_trigger(rightButton, right_repeat)) menuManager.handle_input(RIGHT);
}



void core_input_poll(void)
{

    if (!input)
    {
        input = rr_input_create(getDeviceName());
    }

    // Read inputs
    input_gamepad_read();
    if (drmDirectScanoutDiagnosticActive)
        return;
    if (rr_input_state_button_get(gamepadState, RRInputButton_Quit) == RRButtonState_Pressed)
    {
        input_exit_requested = true;
    }
    // These values are produced by slow platform status workers and are only
    // displayed by frontend UI. Reading them every emulated frame adds two
    // synchronized accesses to the hottest input path without improving
    // freshness, so refresh the cached copies at a human-visible cadence.
    gettimeofday(&exitTimeStop, NULL);
    static time_t nextPlatformStatusRead = 0;
    if (exitTimeStop.tv_sec >= nextPlatformStatusRead)
    {
        rr_input_battery_read(input, &batteryState);
        rr_input_brightness_read(input, &brightnessState);
        nextPlatformStatusRead = exitTimeStop.tv_sec + 1;
    }
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
        
            if (rr_input_state_button_get(gamepadState, RRInputButton_F1) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY1] - [F1]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F2) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY2] - [F2]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F3) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY3] - [F3]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F4) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY4] - [F4]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F5) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY5] - [F5]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F6) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY6] - [F6]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F7) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY7] - [F7]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F8) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY8] - [F8]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F9) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY9] - [F9]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F10) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY10[] - [F10]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F11) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY11] - [F11]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_F12) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TRIGGER_HAPPY12] - [F12]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_SELECT) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_SELECT] - [SELECT]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_START) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_START] - [START]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_THUMBR) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_THUMBR] - [THUMBR]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_THUMBL) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_THUMBL] - [THUMBL]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_A) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_EAST] - [A]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_B) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_SOUTH] - [B]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_X) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_NORTH] - [X]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_Y) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_WEST] - [Y]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_DPadUp) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_DPAD_UP] - [UP]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_DPadDown) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_DPAD_DOWN] - [DOWN]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_DPadLeft) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_DPAD_LEFT] - [LEFT]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_DPadRight) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_DPAD_RIGHT] - [RIGHT]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_TopLeft) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TL] - [TopLeft]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_TopRight) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TR] - [TopRight]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_TriggerLeft) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TL2] - [TriggerLeft]");
            }
            if (rr_input_state_button_get(gamepadState, RRInputButton_TriggerRight) == RRButtonState_Pressed) {
                logger.log(Logger::INF, "Joypad button pressed: [BTN_TR2] - [TriggerRight]");
            }
        
    }




    // if we are in the menu info we have to manage the input for that
    if (rr_keyboard_virtual_visible() || rr_file_browser_visible() || achievements_view_visible()) {
        // Modal frontend screens consume input below.
    } else if (input_credits_requested) {
        manageCredits();
    }else
    if (input_info_requested)
    {
          // Release the lock before calling manageMenu()
            // lock.unlock();
            manageMenu();
       
    }
// Store button states to avoid multiple function calls
bool isL3Pressed = rr_input_state_button_get(gamepadState, l3Button) == RRButtonState_Pressed;
bool isR3Pressed = rr_input_state_button_get(gamepadState, r3Button) == RRButtonState_Pressed;
bool wasL3Released = rr_input_state_button_get(prevGamepadState, l3Button) == RRButtonState_Released;
bool wasR3Released = rr_input_state_button_get(prevGamepadState, r3Button) == RRButtonState_Released;
bool isSelectPressed = rr_input_state_button_get(gamepadState, selectButton) == RRButtonState_Pressed;
bool isStartPressed = rr_input_state_button_get(gamepadState, startButton) == RRButtonState_Pressed;
bool isXPressed = rr_input_state_button_get(gamepadState, xButton) == RRButtonState_Pressed;
bool isYPressed = rr_input_state_button_get(gamepadState, yButton) == RRButtonState_Pressed;
bool isBPressed = rr_input_state_button_get(gamepadState, bButton) == RRButtonState_Pressed;
bool isAPressed = rr_input_state_button_get(gamepadState, aButton) == RRButtonState_Pressed;
bool wasBReleased = rr_input_state_button_get(prevGamepadState, bButton) == RRButtonState_Released;
//bool isF1Pressed = rr_input_state_button_get(gamepadState, f1Button) == RRButtonState_Pressed;
bool isF2Pressed = ignoreF2 ? false: rr_input_state_button_get(gamepadState, f2Button) == RRButtonState_Pressed;
bool isR2Pressed = rr_input_state_button_get(gamepadState, r2Button) == RRButtonState_Pressed;
bool wasR2Released = rr_input_state_button_get(prevGamepadState, r2Button) == RRButtonState_Released;

const bool upEdge = rr_input_state_button_get(gamepadState, upButton) == RRButtonState_Pressed &&
                    rr_input_state_button_get(prevGamepadState, upButton) == RRButtonState_Released;
const bool downEdge = rr_input_state_button_get(gamepadState, downButton) == RRButtonState_Pressed &&
                      rr_input_state_button_get(prevGamepadState, downButton) == RRButtonState_Released;
const bool leftEdge = rr_input_state_button_get(gamepadState, leftButton) == RRButtonState_Pressed &&
                      rr_input_state_button_get(prevGamepadState, leftButton) == RRButtonState_Released;
const bool rightEdge = rr_input_state_button_get(gamepadState, rightButton) == RRButtonState_Pressed &&
                       rr_input_state_button_get(prevGamepadState, rightButton) == RRButtonState_Released;
const bool aEdge = isAPressed && rr_input_state_button_get(prevGamepadState, aButton) == RRButtonState_Released;
const bool bEdge = isBPressed && wasBReleased;
const bool xEdge = isXPressed && rr_input_state_button_get(prevGamepadState, xButton) == RRButtonState_Released;

if (rr_keyboard_virtual_visible()) {
    if (rr_keyboard_virtual_controller_input_enabled())
        rr_keyboard_virtual_input(upEdge, downEdge, leftEdge, rightEdge, aEdge, bEdge, xEdge);
    return;
}
if (rr_file_browser_visible()) {
    rr_file_browser_input(upEdge, downEdge, aEdge, bEdge);
    return;
}
if (achievements_view_visible()) {
    achievements_view_input(upEdge, downEdge, leftEdge, rightEdge, aEdge, bEdge);
    return;
}

// Reuse the timestamp already read for the exit-button state machine.
double currentTime = exitTimeStop.tv_sec + (exitTimeStop.tv_usec / 1000000.0);
// Handle input_info_requested_alternative condition
if (input_info_requested_alternative) { // this are the alternative combinations used by ArkOs
    // Handle emntering in Menu request
    const bool sticksMenuPressed = isL3Pressed && isR3Pressed &&
                                   (wasL3Released || wasR3Released);
    if (!showLoading && ((isSelectPressed && isXPressed) ||
                         (isF2Pressed && isXPressed) || sticksMenuPressed)) {
        double elapsed = currentTime - lastInforequestTime;
        logger.log(Logger::DEB, "Input: Info requested");
        if (elapsed >= 0.5) {
            input_info_requested = !input_info_requested;
            pause_requested = input_info_requested;
            input_credits_requested = false;
            if (input_info_requested)
                menuManager.beginSession();
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
        if (!showLoading && wasBReleased &&
            ((isF2Pressed && isBPressed)|| (isSelectPressed && isBPressed))) {
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
        if (!showLoading && (isF2Pressed || isSelectPressed) &&
            isR2Pressed && wasR2Released && fastForwardToggleAllowed()) {
            input_ffwd_requested = !input_ffwd_requested;
            logger.log(Logger::DEB, "Input: Fast-forward %s",
                       input_ffwd_requested ? "on" : "off");
        }
}

} else { // this is the oermal behaviour used in AmberElec

    // Handle emntering in Menu request
    if (!showLoading && isL3Pressed && isR3Pressed &&
        (wasL3Released || wasR3Released)) {
        double elapsed = currentTime - lastInforequestTime;
        logger.log(Logger::DEB, "Input: Info requested");
        if (elapsed >= 0.5) {
            input_info_requested = !input_info_requested;
            pause_requested = input_info_requested;
            input_credits_requested = false;
            if (input_info_requested)
                menuManager.beginSession();
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
        if (!showLoading && isSelectPressed && isBPressed && wasBReleased) {
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
            if (fastForwardToggleAllowed()) {
                input_ffwd_requested = !input_ffwd_requested;
                logger.log(Logger::DEB, "Input: Fast-forward %s",
                           input_ffwd_requested ? "on" : "off");
            }
        }
    }




}


// new
if (!showLoading && (rr_input_state_button_get(gamepadState, selectButton) == RRButtonState_Pressed) &&
(rr_input_state_button_get(gamepadState, r1Button) == RRButtonState_Pressed))
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

if (!showLoading && (rr_input_state_button_get(gamepadState, selectButton) == RRButtonState_Pressed) &&
(rr_input_state_button_get(gamepadState, upButton) == RRButtonState_Pressed))
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

if (!showLoading && (rr_input_state_button_get(gamepadState, selectButton) == RRButtonState_Pressed) &&
(rr_input_state_button_get(gamepadState, downButton) == RRButtonState_Pressed))
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

if (!showLoading && (rr_input_state_button_get(gamepadState, selectButton) == RRButtonState_Pressed) &&
(rr_input_state_button_get(gamepadState, l1Button) == RRButtonState_Pressed))
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


rr_button_state_t getInputUp(){
    if (!isTate()){
       return  rr_input_state_button_get(gamepadState, upButton);
    }
    if (tateState== REVERSED){
        return rr_input_state_button_get(gamepadState, rightButton);
    }else{
        return rr_input_state_button_get(gamepadState, leftButton);
    }
}

rr_button_state_t getInputDown(){
    if (!isTate()){
       return  rr_input_state_button_get(gamepadState, downButton);
    }
    if (tateState== REVERSED){
        return rr_input_state_button_get(gamepadState, leftButton);
    }else{
        return rr_input_state_button_get(gamepadState, rightButton);
    }
}


rr_button_state_t getInputLeft(){
    if (!isTate()){
       return  rr_input_state_button_get(gamepadState, leftButton);
    }
    if (tateState== REVERSED){
        return rr_input_state_button_get(gamepadState, upButton);
    }else{
        return rr_input_state_button_get(gamepadState, downButton);
    }
}

rr_button_state_t getInputRight(){
    if (!isTate()){
       return  rr_input_state_button_get(gamepadState, rightButton);
    }
    if (tateState== REVERSED){
        return rr_input_state_button_get(gamepadState, downButton);
    }else{
        return rr_input_state_button_get(gamepadState, upButton);
    }
}

rr_button_state_t getInputA(){
    if (!isTate()){
       return rr_input_state_button_get(gamepadState, aButton);
    }
    if (tateState== REVERSED){
        return rr_input_state_button_get(gamepadState, bButton);
    }else{
        return rr_input_state_button_get(gamepadState, xButton);
    }
}

rr_button_state_t getInputB(){
    if (!isTate()){
       return rr_input_state_button_get(gamepadState, bButton);
    }
    if (tateState== REVERSED){
        return rr_input_state_button_get(gamepadState, yButton);
    }else{
        return rr_input_state_button_get(gamepadState, aButton);
    }
}

rr_button_state_t getInputX(){
    if (!isTate()){
       return rr_input_state_button_get(gamepadState, xButton);
    }
    if (tateState== REVERSED){
        return rr_input_state_button_get(gamepadState, aButton);
    }else{
        return rr_input_state_button_get(gamepadState, yButton);
    }
}

rr_button_state_t getInputY(){
    if (!isTate()){
       return rr_input_state_button_get(gamepadState, yButton);
    }
    if (tateState== REVERSED){
        return rr_input_state_button_get(gamepadState, xButton);
    }else{
        return rr_input_state_button_get(gamepadState, bButton);
    }
}





/*
bool core_support_analog=false;

int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    //logger.log(Logger::DEB, "core_input_state called: port=%d, device=%d, index=%d, id=%d", port, device, index, id);

    rr_input_button_t realL1 = gpio_joypad ? l1Button : RRInputButton_TopLeft;
    rr_input_button_t realR1 = gpio_joypad ? r1Button : RRInputButton_TopRight;

    
    //logger.log(Logger::DEB, "Core supports analog input: %s", core_support_analog ? "YES" : "NO");

    // Se è selezionato NODE, lasciamo tutto invariato
    if (analogToDigital != NONE)
    
    {
        const float TRIM = 0.35f;
        rr_thumb_t thumbL = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Left);
        rr_thumb_t thumbR = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Right);

        // Mappatura degli analogici in base alla modalità selezionata
        if (analogToDigital == LEFT_ANALOG || analogToDigital == LEFT_ANALOG_FORCED)
        {
            //logger.log(Logger::DEB, "Mapping LEFT Analog to D-pad.");
            rr_input_state_thumbstick_set_null(gamepadState, RRInputThumbstick_Right);
            if (thumbL.y < -TRIM) rr_input_state_button_set(gamepadState, RRInputButton_DPadUp, RRButtonState_Pressed);
            if (thumbL.y > TRIM) rr_input_state_button_set(gamepadState, RRInputButton_DPadDown, RRButtonState_Pressed);
            if (thumbL.x < -TRIM) rr_input_state_button_set(gamepadState, RRInputButton_DPadLeft, RRButtonState_Pressed);
            if (thumbL.x > TRIM) rr_input_state_button_set(gamepadState, RRInputButton_DPadRight, RRButtonState_Pressed);
        }

        if (analogToDigital == RIGHT_ANALOG || analogToDigital == RIGHT_ANALOG_FORCED)
        {
            //logger.log(Logger::DEB, "Mapping RIGHT Analog to D-pad.");
            rr_input_state_thumbstick_set_null(gamepadState, RRInputThumbstick_Left);
            if (thumbR.y < -TRIM) rr_input_state_button_set(gamepadState, RRInputButton_DPadUp, RRButtonState_Pressed);
            if (thumbR.y > TRIM) rr_input_state_button_set(gamepadState, RRInputButton_DPadDown, RRButtonState_Pressed);
            if (thumbR.x < -TRIM) rr_input_state_button_set(gamepadState, RRInputButton_DPadLeft, RRButtonState_Pressed);
            if (thumbR.x > TRIM) rr_input_state_button_set(gamepadState, RRInputButton_DPadRight, RRButtonState_Pressed);
        }

        
    }


    if (Retrorun_Core == RETRORUN_CORE_PARALLEL_N64) // C buttons
    {

        const float TRIM = 0.35f;
        rr_thumb_t thumb = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Right);

        if (thumb.y < -TRIM) // UP
        {
            rr_input_state_button_set(gamepadState, r2Button, RRButtonState_Pressed);
            rr_input_state_button_set(gamepadState, RRInputButton_X, RRButtonState_Pressed);
        }
        if (thumb.y > TRIM) //DOWN
        {
            rr_input_state_button_set(gamepadState, r2Button, RRButtonState_Pressed);
            rr_input_state_button_set(gamepadState, RRInputButton_B, RRButtonState_Pressed);
        }
        if (thumb.x < -TRIM)//LEFT
        {
            rr_input_state_button_set(gamepadState, r2Button, RRButtonState_Pressed);
            rr_input_state_button_set(gamepadState, RRInputButton_Y, RRButtonState_Pressed);
        }
        if (thumb.x > TRIM)// RIGHT
        {
            rr_input_state_button_set(gamepadState, r2Button, RRButtonState_Pressed);
            rr_input_state_button_set(gamepadState, RRInputButton_A, RRButtonState_Pressed);
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
            rr_thumb_t thumb = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Right);
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
            return rr_input_state_button_get(gamepadState, selectButton);
        case RETRO_DEVICE_ID_JOYPAD_START:
            return rr_input_state_button_get(gamepadState, startButton);
        case RETRO_DEVICE_ID_JOYPAD_A:
            return getInputA();
        case RETRO_DEVICE_ID_JOYPAD_B:
            return getInputB();
        case RETRO_DEVICE_ID_JOYPAD_X:
            return getInputX();
        case RETRO_DEVICE_ID_JOYPAD_Y:
            return getInputY();
        case RETRO_DEVICE_ID_JOYPAD_L:
            return rr_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? l2Button : realL1);
        case RETRO_DEVICE_ID_JOYPAD_R:
            return rr_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? r2Button : realR1);
        case RETRO_DEVICE_ID_JOYPAD_L2:
        if (isRG503()||isRG353V() || isRG353M()){
            return rr_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? l2Button : realL1);
        }else{
            return rr_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? realL1 : l2Button);
        }
        case RETRO_DEVICE_ID_JOYPAD_R2:
        if (isRG503()||isRG353V() || isRG353M()){
            return rr_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? r2Button : realR1);
        }else{
            return rr_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? realR1 : r2Button);
        }
        case RETRO_DEVICE_ID_JOYPAD_L3:
            return rr_input_state_button_get(gamepadState, l3Button);
        case RETRO_DEVICE_ID_JOYPAD_R3:
            return rr_input_state_button_get(gamepadState, r3Button);
        
        
        
        
        default:
            
            return 0;
        }
    }


    
       
        rr_thumb_t thumb;

// Se un analogico è forzato, usiamo quello corrispondente
if (analogToDigital == LEFT_ANALOG_FORCED)
{
    thumb = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Left);
    //thumb.y = -thumb.y;
}
else if (analogToDigital == RIGHT_ANALOG_FORCED)
{
    thumb = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Right);
    //thumb.y = -thumb.y;
}
else
{
    // Se non è forzato, selezioniamo in base all'index
    thumb = rr_input_state_thumbstick_get(gamepadState,
                                           index == RETRO_DEVICE_INDEX_ANALOG_LEFT ? RRInputThumbstick_Left: RRInputThumbstick_Right);
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
    if (drmDirectScanoutDiagnosticActive || rr_keyboard_virtual_visible() ||
        rr_file_browser_visible() || achievements_view_visible())
        return 0;

    if (device == RETRO_DEVICE_ANALOG &&
        (index == RETRO_DEVICE_INDEX_ANALOG_LEFT ||
         index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) &&
        port < coreAnalogRequested.size())
        coreAnalogRequested[port] = true;

    const AnalogToDigital analogMode = effectiveAnalogToDigitalMode(port);


    rr_input_button_t realL1 =  gpio_joypad ? l1Button : RRInputButton_TopLeft;
    rr_input_button_t realR1 = gpio_joypad ? r1Button : RRInputButton_TopRight ;
// for set we dont need to take care of tate mode
    
    if (Retrorun_Core == RETRORUN_CORE_PARALLEL_N64) // C buttons
    {

        const float TRIM = 0.35f;
        rr_thumb_t thumb = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Right);

        if (thumb.y < -TRIM) // UP
        {
            rr_input_state_button_set(gamepadState, r2Button, RRButtonState_Pressed);
            rr_input_state_button_set(gamepadState, xButton, RRButtonState_Pressed);
        }
        if (thumb.y > TRIM) //DOWN
        {
            rr_input_state_button_set(gamepadState, r2Button, RRButtonState_Pressed);
            rr_input_state_button_set(gamepadState, bButton, RRButtonState_Pressed);
        }
        if (thumb.x < -TRIM)//LEFT
        {
            rr_input_state_button_set(gamepadState, r2Button, RRButtonState_Pressed);
            rr_input_state_button_set(gamepadState, yButton, RRButtonState_Pressed);
        }
        if (thumb.x > TRIM)// RIGHT
        {
            rr_input_state_button_set(gamepadState, r2Button, RRButtonState_Pressed);
            rr_input_state_button_set(gamepadState, aButton, RRButtonState_Pressed);
        }
        thumb.x = 0;
        thumb.y = 0;    
    }



    if (port == 0)
    {
        
        
        // manage mouse ( for certain cores like dosbox)
        if (device == RETRO_DEVICE_MOUSE)
        {
         
            rr_thumb_t thumb = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Right);

          

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
                return rr_input_state_button_get(gamepadState, selectButton);
                break;
            case RETRO_DEVICE_ID_JOYPAD_START:
                return rr_input_state_button_get(gamepadState, startButton);
                break;
            case RETRO_DEVICE_ID_JOYPAD_UP:
            case RETRO_DEVICE_ID_JOYPAD_DOWN:
            case RETRO_DEVICE_ID_JOYPAD_LEFT:
            case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            {
                rr_button_state_t physical = RRButtonState_Released;
                if (id == RETRO_DEVICE_ID_JOYPAD_UP) physical = getInputUp();
                else if (id == RETRO_DEVICE_ID_JOYPAD_DOWN) physical = getInputDown();
                else if (id == RETRO_DEVICE_ID_JOYPAD_LEFT) physical = getInputLeft();
                else physical = getInputRight();
                if (physical == RRButtonState_Pressed || analogMode == NONE)
                    return physical;

                rr_input_thumbstick_t stick =
                    (analogMode == LEFT_ANALOG || analogMode == LEFT_ANALOG_FORCED)
                        ? RRInputThumbstick_Left : RRInputThumbstick_Right;
                if (swapSticks)
                    stick = stick == RRInputThumbstick_Left
                                ? RRInputThumbstick_Right : RRInputThumbstick_Left;
                const rr_thumb_t thumb = rr_input_state_thumbstick_get(gamepadState, stick);
                return analogToDigitalDirectionPressed(thumb, id);
            }
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
                return rr_input_state_button_get(gamepadState,swapL1R1WithL2R2 ? l2Button: realL1);
                break;
            case RETRO_DEVICE_ID_JOYPAD_R:
                return rr_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? r2Button : realR1);
                break;
            case RETRO_DEVICE_ID_JOYPAD_L2:
                return rr_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? realL1 : l2Button);
                break;
            case RETRO_DEVICE_ID_JOYPAD_R2:
                return rr_input_state_button_get(gamepadState, swapL1R1WithL2R2 ? realR1 : r2Button);
                break;
            case RETRO_DEVICE_ID_JOYPAD_L3:
                return rr_input_state_button_get(gamepadState, l3Button);
                break;
            case RETRO_DEVICE_ID_JOYPAD_R3:
                return rr_input_state_button_get(gamepadState, r3Button);
                break;
            default:
                return 0;
                break;
            }
        }



        else if (device == RETRO_DEVICE_ANALOG
        && (index == RETRO_DEVICE_INDEX_ANALOG_LEFT ||
            index == RETRO_DEVICE_INDEX_ANALOG_RIGHT))
        {
            const unsigned requestedIndex = index;

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

            const bool mappedLeft =
                (analogMode == LEFT_ANALOG || analogMode == LEFT_ANALOG_FORCED) &&
                requestedIndex == RETRO_DEVICE_INDEX_ANALOG_LEFT;
            const bool mappedRight =
                (analogMode == RIGHT_ANALOG || analogMode == RIGHT_ANALOG_FORCED) &&
                requestedIndex == RETRO_DEVICE_INDEX_ANALOG_RIGHT;
            if (mappedLeft || mappedRight)
                return 0;

            rr_thumb_t thumb;
            //rr_thumb_t thumb2;
            if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT ){
            thumb = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Left);
            }else{
                thumb = rr_input_state_thumbstick_get(gamepadState, RRInputThumbstick_Right);
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
