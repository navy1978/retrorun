#pragma once

/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include <string>

// Save state operations
int LoadState(const char *saveName);
bool StartLoadStateAsync(const char *saveName, int slotNumber, bool autoLoad);
void PumpLoadStateAsync();
bool LoadStateAsyncBusy();
int LoadStateProgress();
std::string LoadStateStatusMessage();
bool LoadStateMissingAuto();
void ShutdownLoadStateAsync();
void SaveState(const char *saveName);
int LoadSram(const char *saveName);
void SaveSram(const char *saveName);

// Path creation helpers (caller must free() the returned pointer)
char *createSramPath(const std::string &arg_rom, const std::string &opt_savedir);
char *createSavePath(const std::string &arg_rom, const std::string &opt_savedir);
