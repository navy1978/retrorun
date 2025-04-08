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

#include "uinput.h"
#include "joypads.h"

#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdexcept>
#include <string.h>
#include <sys/epoll.h>

#include "../logger.h"
#include "../globals.h"

/*

Microsoft X-Box 360 pad	driver version is 1.0.1	device ID: bus 0x3 vendor 0x45e product 0x28e version 0x110

Input driver version is 1.0.1
Input device ID: bus 0x3 vendor 0x45e product 0x28e version 0x110
Supported events:
  Event type 0
  Event type 1
304, 305, 307, 308, 310, 311, 314, 315, 316, 317, 318, 
  Event type 3
0, 1, 2, 3, 4, 5, 16, 17, 

Device [Sony PLAYSTATION(R)3 Controller] on '/dev/input/event22' version: 1.0.1 PS3 USB
Device id info: Bus 0x0003, Vendor 0x054c, Product 0x0268, Version 0x0111
Supported events:
	Event type 0
	Event type 1
0x0120 0x0121 0x0122 0x0123 0x0124 0x0125 0x0126 0x0127 0x0128 0x0129 0x012a 0x012b 0x012c 0x012d 0x012e 0x012f 0x02c0 0x02c1 0x02c2 
	Event type 3
0x0000(0,0,255,0,15,0) 0x0001(0,0,255,0,15,0) 0x0002(0,0,255,0,15,0) 0x0005(0,0,255,0,15,0) 0x0028(0,0,255,0,15,0) 0x0029(0,0,255,0,15,0) 
0x002a(0,0,255,0,15,0) 0x002b(0,0,255,0,15,0) 0x002c(0,0,255,0,15,0) 0x002d(0,0,255,0,15,0) 0x002e(0,0,255,0,15,0) 0x002f(0,0,255,0,15,0) 
0x0030(0,0,255,0,15,0) 0x0031(0,0,255,0,15,0) 0x0032(0,0,255,0,15,0) 0x0033(0,0,255,0,15,0) 0x0034(0,0,255,0,15,0) 0x0035(0,0,255,0,15,0) 
0x0036(0,0,255,0,15,0) 0x0037(0,0,255,0,15,0) 0x0038(0,0,255,0,15,0) 0x0039(0,0,255,0,15,0) 0x003a(0,0,255,0,15,0) 0x003b(0,0,1023,3,63,0) 
0x003c(0,0,1023,3,63,0) 0x003d(0,0,1023,3,63,0) 0x003e(0,0,1023,3,63,0) 
	Event type 4
0x0004 
	Event type 21
0x0050 0x0051 0x0058 0x0059 0x005a 0x0060 

Device [PLAYSTATION(R)3 Controller] on '/dev/input/event22' version: 1.0.1 PS3 Bluetooth
Device id info: Bus 0x0005, Vendor 0x054c, Product 0x0268, Version 0x0100
Supported events:
	Event type 0
	Event type 1
0x0120 0x0121 0x0122 0x0123 0x0124 0x0125 0x0126 0x0127 0x0128 0x0129 0x012a 0x012b 0x012c 0x012d 0x012e 0x012f 0x02c0 0x02c1 0x02c2 
	Event type 3
0x0000(130,0,255,0,15,0) 0x0001(128,0,255,0,15,0) 0x0002(128,0,255,0,15,0) 0x0005(129,0,255,0,15,0) 0x0028(0,0,255,0,15,0) 
0x0029(0,0,255,0,15,0) 0x002a(0,0,255,0,15,0) 0x002b(0,0,255,0,15,0) 0x002c(0,0,255,0,15,0) 0x002d(0,0,255,0,15,0) 
0x002e(0,0,255,0,15,0) 0x002f(0,0,255,0,15,0) 0x0030(0,0,255,0,15,0) 0x0031(0,0,255,0,15,0) 0x0032(0,0,255,0,15,0) 
0x0033(0,0,255,0,15,0) 0x0034(0,0,255,0,15,0) 0x0035(0,0,255,0,15,0) 0x0036(0,0,255,0,15,0) 0x0037(0,0,255,0,15,0) 
0x0038(0,0,255,0,15,0) 0x0039(0,0,255,0,15,0) 0x003a(0,0,255,0,15,0) 0x003b(0,0,1023,3,63,0) 0x003c(0,0,1023,3,63,0) 
0x003d(0,0,1023,3,63,0) 0x003e(494,0,1023,3,63,0) 
	Event type 4
0x0004 
	Event type 21
0x0050 0x0051 0x0058 0x0059 0x005a 0x0060 

Device [Sony Computer Entertainment Wireless Controller] on '/dev/input/event23' version: 1.0.1 PS4 USB
Device id info: Bus 0x0003, Vendor 0x054c, Product 0x05c4, Version 0x0111
Supported events:
	Event type 0
	Event type 1
0x0130 0x0131 0x0132 0x0133 0x0134 0x0135 0x0136 0x0137 0x0138 0x0139 0x013a 0x013b 0x013c 0x013d 
	Event type 3
0x0000(133,0,255,0,15,0) 0x0001(123,0,255,0,15,0) 0x0002(126,0,255,0,15,0) 0x0003(0,0,255,0,15,0) 0x0004(0,0,255,0,15,0)
0x0005(125,0,255,0,15,0) 0x0010(0,-1,1,0,0,0) 0x0011(0,-1,1,0,0,0) 0x002f(0,0,1,0,0,0) 0x0035(0,0,1920,0,0,0) 
0x0036(0,0,942,0,0,0) 0x0039(0,0,65535,0,0,0) 
	Event type 4
0x0004 
	Event type 21
0x0050 0x0051 0x0058 0x0059 0x005a 0x0060 

Device [OpenSimHardware OSH PB Controller] on '/dev/input/event7' version: 1.0.1
Device id info: Bus 0x0003, Vendor 0x1209, Product 0x3100, Version 0x0111
Supported events:
	Event type 0
	Event type 1
0x0130 0x0131 0x0132 0x0133 0x0134 0x0135 0x0136 0x0137 0x0138 0x0139 0x013a 0x013b 0x013c 0x013d 0x013e 0x013f 0x02c0 0x02c1 0x02c2 0x02c3 0x02c4 0x02c5 0x02c6 0x02c7 0x02c8 0x02c9 0x02ca 0x02cb 0x02cc 0x02cd 0x02ce 0x02cf 0x02d0 0x02d1 0x02d2 0x02d3 0x02d4 0x02d5 0x02d6 0x02d7 0x02d8 0x02d9 0x02da 0x02db 0x02dc 0x02dd 0x02de 0x02df 0x02e0 0x02e1 0x02e2 0x02e3 0x02e4 0x02e5 0x02e6 0x02e7 0x02e8 0x02e9 0x02ea 0x02eb 0x02ec 0x02ed 0x02ee 0x02ef 
	Event type 3
0x0002(2035,0,4095,15,255,0) 0x0003(2006,0,4095,15,255,0) 0x0004(2055,0,4095,15,255,0) 0x0005(2000,0,4095,15,255,0) 0x0006(2183,0,4095,15,255,0) 0x0010(0,-1,1,0,0,0) 0x0011(0,-1,1,0,0,0) 
	Event type 4
0x0004 
*/

namespace {
	const char	DEV_UINPUT[] = "/dev/uinput";

	void UINPUT_SET_ABS_P(uinput_user_dev *dev, int axis, int min, int max, int fuzz, int flat) {
		dev->absmax[axis] = max;
		dev->absmin[axis] = min;
		dev->absfuzz[axis] = fuzz;
		dev->absflat[axis] = flat;
	}

	// According to xboxdrv
	/*
	DPad -> ABS_HAT0X and ABS_HAT0Y
	X -> BTN_X
	Y -> BTN_Y
	A -> BTN_A
	B -> BTN_B
	-- back
	TL -> BTN_TL
	TR -> BTN_TR
	BL -> ABS_Z
	BR -> ABS_RZ
	-- middle
	Select -> BTN_SELECT
	PS -> BTN_MODE
	Start -> BTN_START
	-- analog
	Left -> ABS_X and ABS_Y
	Right -> ABS_RX and ABS_RY
	*/

	
	class oga_target : public uinput::pad {
	public:
		oga_target(void) {
			if(ioctl(_fd, UI_SET_EVBIT, EV_KEY))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_KEY");
			if(ioctl(_fd, UI_SET_EVBIT, EV_SYN))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_SYN");
			if(ioctl(_fd, UI_SET_EVBIT, EV_ABS))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_ABS");

			// setup X-Box 360 pad buttons
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_SOUTH))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_SOUTH");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_EAST))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_EAST");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_NORTH))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_NORTH");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_WEST))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_WEST");

			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TL))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TL");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TR))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TR");
				
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_DPAD_UP))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_DPAD_UP");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_DPAD_DOWN))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_DPAD_DOWN");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_DPAD_LEFT))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_DPAD_LEFT");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_DPAD_RIGHT");

			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY1))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TRIGGER_HAPPY1");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY2))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TRIGGER_HAPPY2");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY3))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TRIGGER_HAPPY3");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY4))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TRIGGER_HAPPY4");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY5))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TRIGGER_HAPPY5");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY6))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TRIGGER_HAPPY6");
									
			// absolute (sticks)
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_X))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_X");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_Y))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_Y");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_RX))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_RX");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_RY))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_RY");

			// add the final touches
			uinput_user_dev	ud = {0};
			strncpy(ud.name, "odroidgo2_joypad", UINPUT_MAX_NAME_SIZE-1);
			ud.id.bustype = joypads::j_oga.bus;
			ud.id.vendor = joypads::j_oga.vendor;
			ud.id.product = joypads::j_oga.product;
			ud.id.version = joypads::j_oga.version;

			UINPUT_SET_ABS_P(&ud, ABS_X, -1800, 1800, 16, 128);
			UINPUT_SET_ABS_P(&ud, ABS_Y, -1800, 1800, 16, 128);
			UINPUT_SET_ABS_P(&ud, ABS_RX, -1800, 1800, 16, 128);
			UINPUT_SET_ABS_P(&ud, ABS_RY, -1800, 1800, 16, 128);
			UINPUT_SET_ABS_P(&ud, ABS_HAT0X, -1, 1, 0, 0);
			UINPUT_SET_ABS_P(&ud, ABS_HAT0Y, -1, 1, 0, 0);

			// initialize
			if(sizeof(ud) != write(_fd, &ud, sizeof(ud)))
				throw std::runtime_error("Can't setup uinput_user_dev");
			if(ioctl(_fd, UI_DEV_CREATE))
				throw std::runtime_error("Can't UI_DEV_CREATE");
		}

		~oga_target() {
			// try to destroy and close the device
			ioctl(_fd, UI_DEV_DESTROY);
		}
	};

	
	class rg351p_2_oga : public oga_target {
		public:
			int direction_hatX = BTN_DPAD_LEFT;
			int direction_hatY = BTN_DPAD_UP;
			virtual bool translate_event(input_event& ev) {
				switch(ev.type) {
				case EV_SYN:
					return true;
				case EV_KEY:{
					switch(ev.code) {
					// setup buttons
					case BTN_TR: ev.code = BTN_TRIGGER_HAPPY1; break;
					case BTN_TL: ev.code = BTN_TRIGGER_HAPPY6; break;
					// action buttons
					case BTN_C: ev.code = BTN_NORTH; break; // square
					case BTN_NORTH: ev.code = BTN_WEST; break; // triangle
					case BTN_SOUTH: ev.code = BTN_EAST; break; // cross
					case BTN_EAST: ev.code = BTN_SOUTH; break; // round
					// back
					case BTN_WEST: ev.code = BTN_TL; break;
					case BTN_Z: ev.code = BTN_TR; break;
					// analog stick buttons
					case BTN_TL2: ev.code = BTN_TRIGGER_HAPPY2; break;
					case BTN_TR2: ev.code = BTN_TRIGGER_HAPPY5; break;
					// do not report any other button
					case BTN_START:  ev.code = BTN_TRIGGER_HAPPY3; break;
					case BTN_SELECT: ev.code = BTN_TRIGGER_HAPPY4; break;
					default:
						return false;
					}
				}	return true;
				case EV_ABS: {
					switch(ev.code) {
					// D-Pad - on PS3 is digital, we'll use that one
					case ABS_HAT0Y: {
								ev.type = EV_KEY;
								 if(ev.value == -1){
									 ev.code = BTN_DPAD_UP;
									 direction_hatY = BTN_DPAD_UP;
								 }else if(ev.value == 1){
									 ev.code = BTN_DPAD_DOWN;
									 direction_hatY = BTN_DPAD_DOWN;
								 }else if(ev.value == 0){
									 ev.code = direction_hatY;
								 }
								ev.value = ev.value;
							 }
							 break; // up
					case ABS_HAT0X: {
								ev.type = EV_KEY;
								 if(ev.value == -1){
									 ev.code = BTN_DPAD_LEFT;
									 direction_hatX = BTN_DPAD_LEFT;
								 }else if(ev.value == 1){
									 ev.code = BTN_DPAD_RIGHT;
									 direction_hatX = BTN_DPAD_RIGHT;
								 }else if(ev.value == 0){
									 ev.code = direction_hatX;
								 }
								ev.value = ev.value;
							 }
							 break; // left
					// left stick
					case ABS_Z: 
									   ev.code = ABS_X;
									   if (ev.value <= 1900)
									   {
										  ev.value = 1800-(ev.value*1800/4096) * 1;
										  return true;
									   }
									   else if (ev.value >= 2300)
									   {
										  ev.value = (ev.value*1800/4096) * -1;
										  return true;
									   }
									   else
									   {
										  ev.value = 0;
										  return true;
									   }
					case ABS_RX:
									   ev.code = ABS_Y; 
									   if (ev.value <= 1700)
									   {
										  ev.value = 1800-(ev.value*1800/4096) * 1;
										  return true;
									   }
									   else if (ev.value >= 2300)
									   {
										  ev.value = (ev.value*1800/4096) * -1;
										  return true;
									   }
									   else
									   {
										  ev.value = 0;
										  return true;
									   }
					case ABS_RY: 
									   ev.code = ABS_RX;
									   if (ev.value <= 1900)
									   {
										  ev.value = (1800-(ev.value*1800/4096) * 1) * -1;
										  return true;
									   }
									   else if (ev.value >= 2300)
									   {
										  ev.value = ((ev.value*1800/4096) * -1) * -1;
										  return true;
									   }
									   else
									   {
										  ev.value = 0;
										  return true;
									   }
					case ABS_RZ:
									   ev.code = ABS_RY; 
									   if (ev.value <= 1900)
									   {
										  ev.value = (1800-(ev.value*1800/4096) * 1) * -1;
										  return true;
									   }
									   else if (ev.value >= 2300)
									   {
										  ev.value = ((ev.value*1800/4096) * -1) * -1;
										  return true;
									   }
									   else
									   {
										  ev.value = 0;
										  return true;
									   }
					// do not report any other axis
					default:
						return false;
					}
				} return true;
				default:
					break;
				}
				return false;
			}
		};
	
	

}

uinput::pad::pad(void) : _fd(open(DEV_UINPUT, O_WRONLY | O_NONBLOCK)) {
	if(_fd < 0)
		throw std::runtime_error("Can't open uinput device");
}

uinput::pad::~pad() {
	close(_fd);
}

void uinput::pad::send_event(const input_event& ev) {
	if(sizeof(ev) != write(_fd, &ev, sizeof(ev)))
		throw std::runtime_error("Error on write input_event");
}

char* uinput::pad::get_device_name(char *evt_path, const int sz) {
	if(sz < 1)
		throw std::runtime_error("Buffer for name too small");

	char		sysfs_device_name[16];
	std::string	nm;
	if(0 < ioctl(_fd, UI_GET_SYSNAME(sizeof(sysfs_device_name)), sysfs_device_name)) {
		const std::string	v_name = std::string("/sys/devices/virtual/input/") + sysfs_device_name;
		struct dirent		**namelist = 0;
		const int 		ndev = scandir(v_name.c_str(), &namelist, events::is_event_device, alphasort);
		if(ndev < 0)
			throw std::runtime_error("Can't find virtual dev path");
		for(int i =0; i < ndev; ++i) {
			// pick the first one
			if(nm.empty()) nm = std::string(events::DEV_INPUT_EVENT) + "/" + namelist[i]->d_name;
			free(namelist[i]);
		}
		free(namelist);
	}
	if(nm.empty()) {
		logger.log(Logger::WARN, "Can't find a virtual name for the virtual pad, will continue");
		evt_path[0] = '\0';
	} else {
		strncpy(evt_path, nm.c_str(), sz-1);
		evt_path[sz-1] = '\0';
	}
	return evt_path;
}
uinput::pad* uinput::get_pad(const events::js_desc *in_type, const events::js_desc *out_type) {
	// If both pads are the same, just throw an exception straight away...
	if(in_type == out_type)
		throw std::runtime_error(std::string("Invalid translation and abstraction for '") + in_type->i_name + "' to '" + out_type->i_name + "' (no point in creating one identical abstract device)");

	

	if(out_type == &joypads::j_oga) {
		if(in_type == &joypads::j_rg351p){
			logger.log(Logger::DEB, "Usig rg351p_2_oga");
			return new rg351p_2_oga();
		}
		// for the moment we dont manage this because this type of devices
		// they have already a good event file
		logger.log(Logger::DEB, "Input not managed for oga output - ìt should have already a good event file.");
		return nullptr;
	}

	

	// If no valid translation is found, throw an error
	throw std::runtime_error(std::string("Can't find valid translation and abstraction for '") + in_type->i_name + "' to '" + out_type->i_name + "'");
}

uinput::evt_reader::evt_reader(const char* evt_file) : _fd(open(evt_file, O_RDONLY)), _efd(epoll_create(1)) {
	if(_fd < 0) {
		if(_efd >= 0) close(_efd);
		if (errno == EACCES && getuid() != 0)
			throw std::runtime_error(std::string("You don't have access to the file '") + evt_file + "', perhaps try running as 'root'?");
		throw std::runtime_error(std::string("Can't access file '") + evt_file + "' (does it exist?)");
	}
	if(_efd < 0) {
		close(_fd);
		throw std::runtime_error("Error in epoll_create");
	}
	// add the fd to efd
	struct epoll_event	ev = {0};
	ev.events = EPOLLIN;
	ev.data.fd = _fd;
	if(epoll_ctl(_efd, EPOLL_CTL_ADD, _fd, &ev)) {
		close(_efd);
		close(_fd);
		throw std::runtime_error("Error in epoll_ctl EPOLL_CTL_ADD");
	}
}

uinput::evt_reader::~evt_reader() {
	close(_efd);
	close(_fd);
}

bool uinput::evt_reader::read(input_event& ev, const int tmout) {
	epoll_event	epv = {0};
	const int	rv = epoll_wait(_efd, &epv, 1, tmout);
	if(rv < 0) {
		if(EINTR == errno) return false;
		throw std::runtime_error(std::string("Error in epoll_wait ") + strerror(errno));
	} else if(!rv) return false;

	// we should be looking at the content of epv, like
	// its file descriptor, the event itself and whatnot,
	// but really, this would waste CPU cycles
	const int	rb = ::read(_fd, &ev, sizeof(struct input_event));
	if(rb != sizeof(struct input_event))
		throw std::runtime_error("Fatal error when reading input_event");

	return true;
}
