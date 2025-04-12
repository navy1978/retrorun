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
#include <cstring>
#include <string>
#include <sstream>
#include <regex>
#include <iostream>
#include <unistd.h>


std::string release = "2.7.5";



Logger logger(Logger::ERR);

// #include <cstring>
const std::string OS_ARCH_FILE = "/storage/.config/.OS_ARCH"; // AmberElec has this file that contains info about the device
std::string OS_ARCH = "cat " + OS_ARCH_FILE;

static const int DEVICE_NAME_SIZE = 1024;
static char DEVICE_NAME[DEVICE_NAME_SIZE];
static bool deviceInitialized = false;

TateState tateState = DISABLED;

RETRORUN_CORE_TYPE Retrorun_Core = RETRORUN_CORE_UNKNOWN;
//Device device = UNKNOWN;
Resolution resolution = R_UNKNOWN;
bool force_left_analog_stick = true;

bool opt_triggers = false;
bool gpio_joypad = false;
bool swapL1R1WithL2R2 = false;
bool swapSticks = false;
float opt_aspect = 0.0f;
float aspect_ratio = 0.0f;
float game_aspect_ratio = 0.0f;
bool audio_disabled = false;

std::string romName;
std::string coreName;
std::string coreVersion;
bool coreReadZippedFiles;

std::string screenShotFolder;

std::string status_message;

float fps = 0.0f;
float originalFps = 0.0f;
float newFps = 0.0f;
int retrorunLoopCounter = 0;
int retrorunLoopSkip = 15; // how many loops we skip before update the FPS

int frameCounter = 0;
int frameCounterSkip = 4;

int audioCounter = 0;
int audioCounterSkip = 6;

bool processVideoInAnotherThread = true;

bool adaptiveFps = false;

bool runLoopAtDeclaredfps = true;

int retrorun_audio_buffer = -1; // means it will be fixed to a value related with the original FPS of the game
int new_retrorun_audio_buffer = -1;
int retrorun_mouse_speed_factor = 5;
std::string retrorun_device_name;
std::vector<CpuInfo> cpu_info_list;

int numberOfStateSlots = 3;
int currentSlot = 1;

float avgFps = 0;

int current_volume = 0;

bool twiceTimeCorrectFrame=true;
bool showLoading=true;

MenuManager menuManager = MenuManager();

bool screenshot_requested = false;
bool pause_requested = false;

int deviceTypeSelected=1;
std::map<unsigned, std::string> controllerMap;
AnalogToDigital analogToDigital = LEFT_ANALOG;

joypad joy= joypad();

std::map<std::string, std::string> conf_map;
bool pixel_perfect =false;


const char *getEnv(const char *tag) noexcept
{
    const char *ret = std::getenv(tag);
    return ret ? ret : "";
}

bool isFlycast()
{
    return coreName == "Flycast";
}

bool isFlycast2021()
{
    return coreName == "Flycast 2021" || 
            (isFlycast() &&  !coreVersion.empty() && coreVersion[0] != 'v');
}

bool isParalleln64()
{
    return coreName == "ParaLLEl N64";
}

bool isSwanStation()
{
    return coreName == "SwanStation";
}

bool isMGBA()
{
    return coreName == "mGBA";
}

bool isVBA()
{
    return coreName == "VBA-M";
}

bool isJaguar()
{
    return coreName == "Virtual Jaguar";
}

bool isDosBox()
{
    return coreName == "DOSBox-pure";
}

bool isDosCore()
{
    return coreName == "DOSBox-core";
}

bool isBeetleVB()
{
    return coreName == "Beetle VB";
}

bool isMame()
{
    return coreName.find("MAME") != std::string::npos;
}

bool isPPSSPP()
{
    return coreName == "PPSSPP";
}

bool isDuckStation()
{
    return coreName == "DuckStation";
}

/*bool isPUAE()
{
    return coreName == "PUAE";
}*/

std::string gpu_name;
void getCpuInfo()
{
    logger.log(Logger::DEB, "Getting CPU info...");
    std::vector<std::string> output = exec("lscpu | egrep 'Model name|Model|Thread|CPU\\(s\\)'");

    // Ensure that cpu_info_list is cleared before populating it
    cpu_info_list.clear();

    std::string current_cpu_model = "";
    for (const auto &line : output)
    {
        std::regex regex(R"(^\s*([^:]+):\s*(.+?)\s*$)");

        std::smatch match;
        if (std::regex_match(line, match, regex))
        {
            std::string key = std::regex_replace(match[1].str(), std::regex("^\\s+|\\s+$"), "");
            std::string value = std::regex_replace(match[2].str(), std::regex("^\\s+|\\s+$"), "");

            if (key == "Model name")
            {
                CpuInfo cpu_info;
                current_cpu_model = value;
                cpu_info.cpu_name = current_cpu_model;
                cpu_info_list.push_back(cpu_info);
            }
            else if (!cpu_info_list.empty()) // Check if cpu_info_list is not empty
            {
                if (key == "Model")
                {
                    cpu_info_list.back().number_of_cpu = value;
                }
                else if (key == "Thread(s) per core")
                {
                    cpu_info_list.back().thread_per_cpu = value;
                }
            }
        }
    }

    // Check if output2 is not empty
    std::vector<std::string> output2 = exec("find /sys/devices/platform/ -maxdepth 2 -type d -name '*.gpu' | xargs -I{} sh -c 'cat {}/gpuinfo | grep -o \"^[^ ]* [^ ]* cores\"'");
    if (!output2.empty())
    {
        gpu_name = output2[0]; // Assign the first line to gpu_name
        // Remove newline characters
        gpu_name.erase(std::remove(gpu_name.begin(), gpu_name.end(), '\n'), gpu_name.end());
    }

    logger.log(Logger::DEB, "OK CPU info...");
}


const char *getDeviceName() noexcept
{

    if (!deviceInitialized)
    {
        logger.log(Logger::INF, "Getting device info...");
            
        if (!retrorun_device_name.empty())
        {
            logger.log(Logger::ERR, "Getting device info from configuration...");
            // get extra info
            getCpuInfo();
            deviceInitialized = true;
            // Ensure retrorun_device_name can fit into DEVICE_NAME
            if (retrorun_device_name.size() < DEVICE_NAME_SIZE)
            {
                // Copy the string to DEVICE_NAME
                strncpy(DEVICE_NAME, retrorun_device_name.c_str(), DEVICE_NAME_SIZE - 1);
                // Add null terminator
                DEVICE_NAME[DEVICE_NAME_SIZE - 1] = '\0';
            }
            else
            {
                // Handle the case where retrorun_device_name is too long to fit into DEVICE_NAME
                // For example, truncate the string or handle the error as appropriate
                // Here, we'll just truncate the string
                strncpy(DEVICE_NAME, retrorun_device_name.c_str(), DEVICE_NAME_SIZE - 1);
                DEVICE_NAME[DEVICE_NAME_SIZE - 1] = '\0'; // Ensure null termination
            }
            return retrorun_device_name.c_str();
        }
        else
        {

            if (access(OS_ARCH_FILE.c_str(), F_OK) == 0)
            {
                logger.log(Logger::DEB, "File %s found! ", OS_ARCH_FILE.c_str());
                FILE *pipe = popen(OS_ARCH.c_str(), "r");
                if (!pipe)
                {
                    logger.log(Logger::ERR, "Could not open pipe to `cat` command.");
                    return "";
                }

                char *result = fgets(DEVICE_NAME, DEVICE_NAME_SIZE, pipe);
                if (!result)
                {
                    logger.log(Logger::ERR, "Could not read output from `cat` command.");
                    return "";
                }

                // Close the pipe
                pclose(pipe);

                logger.log(Logger::INF, "Device found: %s", DEVICE_NAME);
                
                // get extra info
                getCpuInfo();
                deviceInitialized = true;
                return DEVICE_NAME;
            }
            else
            {
                logger.log(Logger::WARN, "File %s not found. Let's try to search in envioronment variables to identify the device name...", OS_ARCH_FILE.c_str());
                const char *envVar = std::getenv("DEVICE_NAME");
                if (envVar != nullptr)
                {
                    // Copy the environment variable value to DEVICE_NAME
                    std::strncpy(DEVICE_NAME, envVar, DEVICE_NAME_SIZE - 1);
                    DEVICE_NAME[DEVICE_NAME_SIZE - 1] = '\0'; // Ensure null-termination
                    logger.log(Logger::DEB, "Environment variable value: %s", DEVICE_NAME);
                }
                else
                {
                    logger.log(Logger::ERR, "Environment variable \"DEVICE_NAME\" not set. Device name undefined!");
                }
                logger.log(Logger::INF, "Device found: %s", DEVICE_NAME);
                // get extra info
                getCpuInfo();
                deviceInitialized = true;
                logger.log(Logger::DEB, "Device name: %s", DEVICE_NAME);
                
                return DEVICE_NAME;
            }
        }
    }

    return DEVICE_NAME;
}

bool checkDeviceName(char *target)
{

    size_t targetLength = strlen(target);

    // Check if the first part of DEVICE_NAME matches the target
    if (strncmp(DEVICE_NAME, target, targetLength) == 0)
    {
        // Check if the next character is '\n' or if DEVICE_NAME is exactly "RG351M"
        return (DEVICE_NAME[targetLength] == '\n' || DEVICE_NAME[targetLength] == '\0');
    }
    return false;
}

bool isRG351M()
{
    return checkDeviceName((char *)"RG351M"); // strcmp(DEVICE_NAME, "RG351M\n") == 0;
}
bool isRG351P()
{
    return checkDeviceName((char *)"RG351P");
}
bool isRG351V()
{
    return checkDeviceName((char *)"RG351V"); 
}
bool isRG351MP()
{
    return checkDeviceName((char *)"RG351MP"); 
}
bool isRG552()
{
    return checkDeviceName((char *)"RG552"); 
}
bool isRG503()
{
    return checkDeviceName((char *)"RG503"); 
}
bool isRG353V()
{
    return checkDeviceName((char *)"RG353V"); 
}
bool isRG353M()
{
    return checkDeviceName((char *)"RG353M"); 
}
bool hasDeviceRotatedScreen(){
   return  isRG351V() || isRG351MP() || isRG503() || isRG353V() || isRG353M();
}
bool wideScreenNotRotated(){
    return  isRG503();
}


// if getDeviceName was not able to identify the device name we 
// take this from the retrorun.cfg and then we need to reset the Device name
void resetDeviceName(){
    strncpy(DEVICE_NAME, retrorun_device_name.c_str(), DEVICE_NAME_SIZE - 1);
    DEVICE_NAME[DEVICE_NAME_SIZE - 1] = '\0'; // Ensure null termination
}

std::vector<std::string> exec(const char *cmd)
{
    std::vector<std::string> output;
    char buffer[128];
    FILE *pipe = popen(cmd, "r");
    if (!pipe)
    {
        logger.log(Logger::ERR, "Error executing command");
        return output;
    }
    while (!feof(pipe))
    {
        if (fgets(buffer, 128, pipe) != nullptr)
        {
            output.push_back(buffer);
        }
    }
    pclose(pipe);
    return output;
}

bool isTate()
{
    // return true; //(aspect_ratio < 1.0f && (isFlycast() || isFlycast2021()));

    switch (tateState)
    {
    case DISABLED:
        return false;
        break;
    case ENABLED:
        return true;
        break;
    case REVERSED:
        return true; //(aspect_ratio < 1.0f && (isFlycast() || isFlycast2021()));
        break;
    case AUTO:
        return (aspect_ratio < 1.0f && (isFlycast() || isFlycast2021()));
        break;
    default:
        return false;
    }
}
