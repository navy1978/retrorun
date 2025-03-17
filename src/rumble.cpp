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
#include "rumble.h"
#include "globals.h"




int pwm= TRIBOOL_NULL;
bool disableRumble= false; // by default rumble is enabled (if supported by the core)


// for event rumble
std::string DEVICE_PATH = "";
static int rumble_fd = -1;
static struct ff_effect rumble_effect;
static int rumble_id = -1;

// for pwm rumble
std::string  PWM_RUMBLE_PATH = "";



bool retrorun_input_set_rumble(unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{

    if (disableRumble) return true;

    bool pwmDevices = pwm!= TRIBOOL_NULL ? pwm : joy.pwm;

    if (pwmDevices){ // pwm devices
    //logger.log(Logger::DEB, "PWM RUMBLE on:%s",PWM_RUMBLE_PATH.c_str());
        
         FILE *fp;
    
    if (strength > 0)
    {
        // Attiva la vibrazione scrivendo PWM_RUMBLE_ON
        if (!PWM_RUMBLE_PATH.empty()) {
            fp = fopen(PWM_RUMBLE_PATH.c_str(), "w");
        }else{
            fp = fopen(joy.rumble.c_str(), "w");
        }
        if (!fp)
        {
            perror("Error opening PWM rumble");
            return false;
        }
        fprintf(fp, PWM_RUMBLE_ON);
        fclose(fp);
    }
    else
    {
        // Disattiva la vibrazione scrivendo PWM_RUMBLE_OFF
        if (!PWM_RUMBLE_PATH.empty()) {
            fp = fopen(PWM_RUMBLE_PATH.c_str(), "w");
        }else{
            fp = fopen(joy.rumble.c_str(), "w");
        }
        if (!fp)
        {
            perror("Error opening PWM rumble");
            return false;
        }
        fprintf(fp, PWM_RUMBLE_OFF);
        fclose(fp);
    }

    return true;
    }else{ // event devices

    //logger.log(Logger::DEB, "EVENT RUMBLE on:%s",DEVICE_PATH.c_str());
        struct input_event play;

    // Open the device if it's not already open
    if (rumble_fd < 0)
    {
        if (!DEVICE_PATH.empty()) {
            rumble_fd = open(DEVICE_PATH.c_str(), O_RDWR);
        }else{
            rumble_fd = open(joy.event.c_str(), O_RDWR);
        }
        if (rumble_fd < 0)
        {
            perror("Error opening rumble device");
            return false;
        }
    }

    // If strength is 0, stop and remove effect
    if (strength == 0)
    {
        if (rumble_id >= 0)
        {
            memset(&play, 0, sizeof(play));
            play.type = EV_FF;
            play.code = rumble_id;
            play.value = 0; // Stop effect

            write(rumble_fd, &play, sizeof(play));

            ioctl(rumble_fd, EVIOCRMFF, rumble_id); // Remove effect
            rumble_id = -1;
        }

        return true;
    }

    // Initialize the effect structure
    memset(&rumble_effect, 0, sizeof(rumble_effect));
    rumble_effect.type = FF_RUMBLE;
    rumble_effect.id =  rumble_id;
    rumble_effect.u.rumble.strong_magnitude = strength * 256; // Scale to full 16-bit range
    rumble_effect.u.rumble.weak_magnitude = 0;

    if (ioctl(rumble_fd, EVIOCSFF, &rumble_effect) == -1)
    {
        perror("Error uploading force feedback effect");
        return false;
    }

    rumble_id = rumble_effect.id;

    // Play the effect
    memset(&play, 0, sizeof(play));
    play.type = EV_FF;
    play.code = rumble_id;
    play.value = 1; // Start effect

    if (write(rumble_fd, &play, sizeof(play)) == -1)
    {
        perror("Error playing rumble effect");
        return false;
    }

    return true;
    }
}
