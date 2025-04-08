/*
*	js2xbox (C) 2017 E. Oriani, ema <AT> fastwebnet <DOT> it
*   Copyright (C) 2025-present  navy1978
*
*	This file is part of js2xbox.
*
*	js2xbox is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	js2xbox is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with js2xbox.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "events.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <linux/input.h>

#include "../logger.h"
#include "../globals.h"


#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)
namespace {
	struct fd_holder {
		int	fd;

		fd_holder(const char* fname) : fd(open(fname, O_RDONLY)) {
			if(fd < 0)
				throw std::runtime_error(std::string("Can't open '") + fname + "'");
		}

		~fd_holder() {
			close(fd);
		}
	};

}

namespace events {
	std::string extra_retrogame_name;
	std::string extra_osh_name;
	std::string extra_evdev_name;
}

int events::is_event_device(const struct dirent *dir) {
	return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

const char* find_existing_evdev() {
    static const char *EVDEV_NAMES[] = {
        "/dev/input/by-path/platform-odroidgo2-joypad-event-joystick",
        "/dev/input/by-path/platform-odroidgo3-joypad-event-joystick",
        "/dev/input/by-path/platform-singleadc-joypad-event-joystick"
		
    };

    for (const char* dev : EVDEV_NAMES) {
		if (access(dev, F_OK) == 0) {
			return dev;
		}
	}
	if (!events::extra_evdev_name.empty() && access(events::extra_evdev_name.c_str(), F_OK) == 0) {
		return events::extra_evdev_name.c_str();
	}

    return nullptr;
}

const char* find_existing_rumble_evdev() {
    static const char *EVDEV_RUMBLE = "/sys/class/pwm/pwmchip0/pwm0/duty_cycle";
    return (access(EVDEV_RUMBLE, F_OK) == 0) ? EVDEV_RUMBLE : nullptr;
}

joypad events::find_event_js(const js_desc** js, js_desc const **out) {
	if (!js)
		throw std::runtime_error("You need to pass at least one js_desc descriptor");

	struct dirent **namelist = nullptr;
	joypad ret;

	const int ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, alphasort);
	if (ndev <= 0)
		throw std::runtime_error("Can't open DEV_INPUT_EVENT");

	for (int i = 0; i < ndev; i++) {
		char name[256] = "???";
		struct input_id id = {0};

		std::string fname = std::string(DEV_INPUT_EVENT) + "/" + namelist[i]->d_name;
		int fd = open(fname.c_str(), O_RDONLY);

		free(namelist[i]);  // Free each entry after processing
		if (fd < 0) continue;

		// Get device name and ID
		const int nm_len = ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		const int id_ctl = ioctl(fd, EVIOCGID, &id);
		close(fd);

		if (nm_len < 0 || id_ctl)
			continue;

		// Search for a matching vendor/product
		for (const js_desc** p = js; *p; ++p) {
			logger.log(Logger::DEB,
				"Checking device: [%s] (bus: 0x%x, vendor: 0x%x, product: 0x%x, version: 0x%x) against expected (bus: 0x%x, vendor: 0x%x, product: 0x%x)", 
				name, 
				id.bustype, id.vendor, id.product, id.version, 
				(*p)->bus, (*p)->vendor, (*p)->product);

				bool isRetrogameJoypad = 
				(strcmp(name, "retrogame_joypad") == 0 &&
				 id.bustype == 0x19 &&
				 id.vendor == 0x484b &&
				 id.product == 0x1101 &&
				 id.version == 0x100) ||
				strcmp(name, "GO-Super Gamepad") == 0 ||
				strcmp(name, "GO-Advance Gamepad") == 0 ||
				strcmp(name, "GO-Advance Gamepad (rev 1.1)") == 0 ||
				(!events::extra_retrogame_name.empty() && strcmp(name, events::extra_retrogame_name.c_str()) == 0);

				bool isOSHController = 
				(strcmp(name, "OpenSimHardware OSH PB Controller") == 0 ||
				 (!events::extra_osh_name.empty() && strcmp(name, events::extra_osh_name.c_str()) == 0)) &&
				 id.bustype == (*p)->bus &&
				 id.vendor == (*p)->vendor &&
				 id.product == (*p)->product;

			if (isRetrogameJoypad || isOSHController) {
				if (isRetrogameJoypad) {
					ret.setValues(find_existing_evdev(), fname,false);
				} else {
					ret.setValues(fname, find_existing_rumble_evdev(),true);
				}

				if (out) *out = *p;

				logger.log(Logger::DEB, "Found matching device: [%s] (id: 0x%04x,0x%04x,0x%04x,0x%04x) on '%s'", 
							name, id.bustype, id.vendor, id.product, id.version, fname.c_str());

				// Free remaining memory before returning
				for (int j = i + 1; j < ndev; ++j) free(namelist[j]);
				free(namelist);
				return ret;
			}
		}
	}

	logger.log(Logger::WARN, "No matching joystick found! Check device name and ID values.");
	free(namelist);

	if (!ret.isValid()) {
		throw std::runtime_error("Couldn't find any joypad in the specified list");
	}
	return ret;
}

void events::print_info_js(const std::string& fname) {
	fd_holder	f(fname.c_str());

	int 		version = 0;
	unsigned long	bit[EV_MAX][NBITS(KEY_MAX)] = {0};
	char 		name[256] = "???";
 
	if(0 > ioctl(f.fd, EVIOCGNAME(sizeof(name)), name)) {
		sprintf(name, "Unknown name");
	}

	if(ioctl(f.fd, EVIOCGVERSION, &version))
		throw std::runtime_error("Can't get EVIOCGVERSION");
 	
	char	buf[128];
	snprintf(buf, 128, "%d.%d.%d", version >> 16, (version >> 8) & 0xff, version & 0xff);
	
	logger.log(Logger::DEB, "Device [%s] on '%s' version: %s", name, fname.c_str(), buf);

	struct input_id id = {0};
	if(ioctl(f.fd, EVIOCGID, &id))
		throw std::runtime_error("Can't get EVIOCGID");

	snprintf(buf, 128, "Bus 0x%04x, Vendor 0x%04x, Product 0x%04x, Version 0x%04x", id.bustype, id.vendor, id.product, id.version);
	logger.log(Logger::DEB, "Device id info: %s", buf);

	memset(bit, 0, sizeof(bit));
	ioctl(f.fd, EVIOCGBIT(0, EV_MAX), bit[0]);

	logger.log(Logger::DEB, "Supported events:");

	for (int i = 0; i < EV_MAX; i++) {
		if (test_bit(i, bit[0])) {
			logger.log(Logger::DEB, "\tEvent type %d", i);
			if (!i) continue;

			ioctl(f.fd, EVIOCGBIT(i, KEY_MAX), bit[i]);

			std::string keyLog;
			for (int j = 0; j < KEY_MAX; j++) {
				if (test_bit(j, bit[i])) {
					char buf[16];
					snprintf(buf, sizeof(buf), "0x%04x", j);
					keyLog += std::string(buf); // Store the log in a string buffer

					// If it's absolute, print out extra details
					if (i == EV_ABS) {
						struct input_absinfo abs = {0};
						ioctl(f.fd, EVIOCGABS(j), &abs);
						
						char absDetails[128];
						snprintf(absDetails, sizeof(absDetails),
								"(%d, %d, %d, %d, %d, %d)", 
								abs.value, abs.minimum, abs.maximum, 
								abs.fuzz, abs.flat, abs.resolution);

						keyLog += std::string(absDetails); // Append absolute data
					}

					keyLog += " ";
				}
			}

			if (!keyLog.empty()) {
				logger.log(Logger::INF, "%s", keyLog.c_str());
			}
		}
	}
}