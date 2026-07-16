#pragma once

/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include <functional>
#include <string>
#include <map>

// Simple menu callback functions
void fake(int);
void resume(int button);
void showCredit(int button);
int testRumble(int);

// Aspect ratio
int getAspectRatioSettings();
int getClosestValue(float value);
extern std::function<void(int)> setAspectRatioSettings;

// Tate mode (int getter for menu use)
int getTateMode();
extern std::function<void(int)> setTateMode;

// Swap triggers/sticks
int getSwapTriggers();
extern std::function<void(int)> setSwapTriggers;
int getSwapSticks();
extern std::function<void(int)> setSwapSticks;

// Lock FPS
int getLockDeclaredFPS();
extern std::function<void(int)> setLockDeclaredFPS;

// Video filter / shader / pixel perfect
int getDecorationSetting();
extern std::function<void(int)> setDecorationSetting;
int getVideoFilter();
extern std::function<void(int)> setVideoFilter;
int getVideoShader();
extern std::function<void(int)> setVideoShader;
int getPixelPerfect();
extern std::function<void(int)> setPixelPerfect;

// UI profile
int getUIProfileSetting();
extern std::function<void(int)> setUIProfileSetting;

// Audio
int getAudioDisabled();
extern std::function<void(int)> setAudioDisabled;
int getAudioBuffer();
extern std::function<void(int)> setAudioBuffer;
int getAudioValue();
extern std::function<void(int)> setAudioValue;

// Rumble
int getRumbleDisabled();
extern std::function<void(int)> setRumbleDisabled;

// Brightness / volume
int getBrightnessValue();
extern std::function<void(int)> setBrightnessValue;

// Device type
int getDeviceType();
extern std::function<void(int)> setDeviceType;

// SDL-specific
#ifdef RR_PLATFORM_SDL
int getSDLVideoRenderer();
extern std::function<void(int)> setSDLVideoRenderer;
int getSDLVsync();
extern std::function<void(int)> setSDLVsync;
#endif

// Save/load slot helpers
std::string getSlotNameStr(int slotNumber, std::string type);
void loadSaveSlotWrapper(int button, int slotNumber, std::string type);
void restartCore(int button);

// Aspect ratio data (needed for menu display)
extern const char *aspect_ratio_names_array[];
extern std::map<float, int> aspectRatioMap;
