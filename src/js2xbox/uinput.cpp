/*
*	js2xbox (C) 2017 E. Oriani, ema <AT> fastwebnet <DOT> it
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

	class xbox_target : public uinput::pad {
	public:
		xbox_target(void) {
			if(ioctl(_fd, UI_SET_EVBIT, EV_KEY))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_KEY");
			if(ioctl(_fd, UI_SET_EVBIT, EV_SYN))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_SYN");
			if(ioctl(_fd, UI_SET_EVBIT, EV_ABS))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_ABS");

			// setup X-Box 360 pad buttons
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_A))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_A");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_B))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_B");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_X))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_X");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_Y))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_Y");

			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TL))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TL");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_TR))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_TR");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_THUMBL))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_THUMBL");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_THUMBR))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_THUMBR");

			if(ioctl(_fd, UI_SET_KEYBIT, BTN_SELECT))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_SELECT");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_START))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_START");
			if(ioctl(_fd, UI_SET_KEYBIT, BTN_MODE))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_MODE");

			// absolute (sticks)
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_X))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_X");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_Y))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_Y");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_Z))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_Z");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_RX))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_RX");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_RY))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_RY");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_RZ))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_RZ");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_HAT0X))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_HAT0X");
			if(ioctl(_fd, UI_SET_ABSBIT, ABS_HAT0Y))
				throw std::runtime_error("Can't UI_SET_ABSBIT ABS_HAT0Y");

			// add the final touches
			uinput_user_dev	ud = {0};
			strncpy(ud.name, "Microsoft X-Box 360 pad", UINPUT_MAX_NAME_SIZE-1);
			ud.id.bustype = joypads::j_xbox_360.bus; // fake it on the USB
			ud.id.vendor = joypads::j_xbox_360.vendor; // Microsoft
			ud.id.product = joypads::j_xbox_360.product; // X-box 360 pad
			ud.id.version = joypads::j_xbox_360.version; // same version as xboxdrv

			UINPUT_SET_ABS_P(&ud, ABS_X, -32768, 32768, 16, 128);
			UINPUT_SET_ABS_P(&ud, ABS_Y, -32768, 32768, 16, 128);
			UINPUT_SET_ABS_P(&ud, ABS_RX, -32768, 32768, 16, 128);
			UINPUT_SET_ABS_P(&ud, ABS_RY, -32768, 32768, 16, 128);
			UINPUT_SET_ABS_P(&ud, ABS_HAT0X, -1, 1, 0, 0);
			UINPUT_SET_ABS_P(&ud, ABS_HAT0Y, -1, 1, 0, 0);
			UINPUT_SET_ABS_P(&ud, ABS_Z, 0, 255, 0, 0);
			UINPUT_SET_ABS_P(&ud, ABS_RZ, 0, 255, 0, 0);

			// initialize
			if(sizeof(ud) != write(_fd, &ud, sizeof(ud)))
				throw std::runtime_error("Can't setup uinput_user_dev");
			if(ioctl(_fd, UI_DEV_CREATE))
				throw std::runtime_error("Can't UI_DEV_CREATE");
		}

		~xbox_target() {
			// try to destroy and close the device
			ioctl(_fd, UI_DEV_DESTROY);
		}
	};

	// this works for both PS3 USB and PS3 Bluetooth
	// we don't forward FF nor lights...
	class ps3_target : public uinput::pad {
	public:
		ps3_target(const uint16_t bustype) {
			if(ioctl(_fd, UI_SET_EVBIT, EV_KEY))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_KEY");
			if(ioctl(_fd, UI_SET_EVBIT, EV_SYN))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_SYN");
			if(ioctl(_fd, UI_SET_EVBIT, EV_ABS))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_ABS");

			// buttons
			static const uint16_t	btns[] = { BTN_JOYSTICK, BTN_THUMB, 
			BTN_THUMB2, BTN_TOP, BTN_TOP2, BTN_PINKIE, BTN_BASE, BTN_BASE2,
			BTN_BASE3, BTN_BASE4, BTN_BASE5, BTN_BASE6, 0x012c, 0x012d,
			0x012e, BTN_DEAD, BTN_TRIGGER_HAPPY1, BTN_TRIGGER_HAPPY2,
			BTN_TRIGGER_HAPPY3 },
			// main axis (0 --> 255, 0, 15)
						axis[] = { ABS_X, ABS_Y, ABS_Z, ABS_RZ,
			ABS_MISC, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, ABS_MT_SLOT,
			ABS_MT_TOUCH_MAJOR, ABS_MT_TOUCH_MINOR, ABS_MT_WIDTH_MAJOR, ABS_MT_WIDTH_MINOR,
			ABS_MT_ORIENTATION, ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TOOL_TYPE,
			ABS_MT_BLOB_ID, ABS_MT_TRACKING_ID, ABS_MT_PRESSURE},
			// large axis (0 --> 1023, 3, 63)
						axis_lrg[] = { ABS_MT_DISTANCE, ABS_MT_TOOL_X,
			ABS_MT_TOOL_Y, 0x003e };

			// set them up
			for(size_t i =0; i < sizeof(btns)/sizeof(uint16_t); ++i) {
				if(ioctl(_fd, UI_SET_KEYBIT, btns[i]))
					throw std::runtime_error(std::string("Can't UI_SET_KEYBIT ") + std::to_string(btns[i]));
			}
			// also setup the ud axis data
			uinput_user_dev	ud = {0};
			for(size_t i =0; i < sizeof(axis)/sizeof(uint16_t); ++i) {
				if(ioctl(_fd, UI_SET_ABSBIT, axis[i]))
					throw std::runtime_error(std::string("Can't UI_SET_ABSBIT ") + std::to_string(axis[i]));
				UINPUT_SET_ABS_P(&ud, axis[i], 0, 255, 0, 15);
			}
			for(size_t i =0; i < sizeof(axis_lrg)/sizeof(uint16_t); ++i) {
				if(ioctl(_fd, UI_SET_ABSBIT, axis_lrg[i]))
					throw std::runtime_error(std::string("Can't UI_SET_ABSBIT ") + std::to_string(axis_lrg[i]));
				UINPUT_SET_ABS_P(&ud, axis_lrg[i], 0, 1023, 3, 63);
			}

			// add the final touches
			strncpy(ud.name, "Sony PLAYSTATION(R)3 Controller (js2xbox)", UINPUT_MAX_NAME_SIZE-1);
			ud.id.bustype = bustype; // setup the bus type
			// other info is equivalent between the USB and Bluetooth device
			ud.id.vendor = joypads::j_ps3_usb.vendor;
			ud.id.product = joypads::j_ps3_usb.product;
			ud.id.version = (BUS_USB == bustype) ? joypads::j_ps3_usb.version : joypads::j_ps3_bt.version; // version is different

			// initialize
			if(sizeof(ud) != write(_fd, &ud, sizeof(ud)))
				throw std::runtime_error("Can't setup uinput_user_dev");
			if(ioctl(_fd, UI_DEV_CREATE))
				throw std::runtime_error("Can't UI_DEV_CREATE");
		}

		~ps3_target() {
			// try to destroy and close the device
			ioctl(_fd, UI_DEV_DESTROY);
		}
	};
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

	class vkb_target : public uinput::pad {
	public:
		vkb_target(void) {
			if(ioctl(_fd, UI_SET_EVBIT, EV_KEY))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_KEY");
			if(ioctl(_fd, UI_SET_EVBIT, EV_SYN))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_SYN");
			if(ioctl(_fd, UI_SET_EVBIT, EV_ABS))
				throw std::runtime_error("Can't UI_SET_EVBIT EV_ABS");

			// setup vkb buttons
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_UP))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_SOUTH");
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_DOWN))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_EAST");
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_LEFT))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_NORTH");
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_RIGHT))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_WEST");

			if(ioctl(_fd, UI_SET_KEYBIT, KEY_ENTER))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_SOUTH");
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_Z))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_EAST");
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_X))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_NORTH");
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_S))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_WEST");
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_A))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_WEST");
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_D))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_NORTH");
			if(ioctl(_fd, UI_SET_KEYBIT, KEY_F))
				throw std::runtime_error("Can't UI_SET_KEYBIT BTN_EAST");

			// add the final touches
			uinput_user_dev	ud = {0};
			strncpy(ud.name, "AT Translated Set 2 keyboard", UINPUT_MAX_NAME_SIZE-1);
			ud.id.bustype = joypads::j_vkb.bus;
			ud.id.vendor = joypads::j_vkb.vendor;
			ud.id.product = joypads::j_vkb.product;
			ud.id.version = joypads::j_vkb.version;

			// initialize
			if(sizeof(ud) != write(_fd, &ud, sizeof(ud)))
				throw std::runtime_error("Can't setup uinput_user_dev");
			if(ioctl(_fd, UI_DEV_CREATE))
				throw std::runtime_error("Can't UI_DEV_CREATE");
		}

		~vkb_target() {
			// try to destroy and close the device
			ioctl(_fd, UI_DEV_DESTROY);
		}
	};



	// layered conversion
	class ps3_2_xbox : public xbox_target {
	public:
		virtual bool translate_event(input_event& ev) {
			switch(ev.type) {
			case EV_SYN:
				return true;
			case EV_KEY:{
				switch(ev.code) {
				// setup buttons
				case BTN_JOYSTICK: ev.code = BTN_SELECT; break;
				case BTN_TOP: ev.code = BTN_START; break;
				case BTN_TRIGGER_HAPPY: ev.code = BTN_MODE; break;
				// action buttons
				case BTN_DEAD: ev.code = BTN_X; break; // square
				case 0x12c: ev.code = BTN_Y; break; // triangle
				case 0x12e: ev.code = BTN_A; break; // cross
				case 0x12d: ev.code = BTN_B; break; // round
				// back
				case BTN_BASE5: ev.code = BTN_TL; break;
				case BTN_BASE6: ev.code = BTN_TR; break;
				// D-Pad - on PS3 is digital, we'll use that one
				case BTN_TOP2: { ev.type = EV_ABS; ev.code = ABS_HAT0Y; ev.value = (ev.value) ? -1 : 0;} break; // up
				case BTN_BASE: { ev.type = EV_ABS; ev.code = ABS_HAT0Y; ev.value = (ev.value) ? 1 : 0;} break; // down
				case BTN_BASE2: { ev.type = EV_ABS; ev.code = ABS_HAT0X; ev.value = (ev.value) ? -1 : 0;} break; // left
				case BTN_PINKIE: { ev.type = EV_ABS; ev.code = ABS_HAT0X; ev.value = (ev.value) ? 1 : 0;} break; // left
				// analog stick buttons
				case BTN_THUMB: ev.code = BTN_THUMBL; break;
				case BTN_THUMB2: ev.code = BTN_THUMBR; break;
				// do not report any other button
				default:
					return false;
				}
			}	return true;
			case EV_ABS: {
				switch(ev.code) {
				// left stick
				case ABS_X: { ev.value = ev.value*(32768*2)/255 -32768; } break; // from 0 --> 255 to -32768 --> 32768
				case ABS_Y: { ev.value = ev.value*(32768*2)/255 -32768; } break;
				// right stick
				case ABS_Z: { ev.code = ABS_RX; ev.value = ev.value*(32768*2)/255 -32768; } break;
				case ABS_RZ: { ev.code = ABS_RY; ev.value = ev.value*(32768*2)/255 -32768; } break;
				// thumb triggers
				case 48: ev.code = ABS_Z; break;
				case 49: ev.code = ABS_RZ; break;
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

	class ps3_2_ps3 : public ps3_target {
	public:
		ps3_2_ps3(const uint16_t bustype) : ps3_target(bustype) {
		}

		virtual bool translate_event(input_event& ev) {
			switch(ev.type) {
			case EV_SYN:
				return true;
			case EV_KEY:
				return true;
			case EV_ABS:
				return true;
			default:
				break;
			}
			return false;
		}
	};

	// layered conversion
	class rg351p_2_xbox : public xbox_target {
	public:
		virtual bool translate_event(input_event& ev) {
			switch(ev.type) {
			case EV_SYN:
				return true;
			case EV_KEY:{
				switch(ev.code) {
				// setup buttons
				case BTN_TR: ev.code = BTN_SELECT; break;
				case BTN_TL: ev.code = BTN_START; break;
				// action buttons
				case BTN_C: ev.code = BTN_X; break; // square
				case BTN_NORTH: ev.code = BTN_Y; break; // triangle
				case BTN_SOUTH: ev.code = BTN_A; break; // cross
				case BTN_EAST: ev.code = BTN_B; break; // round
				// back
				case BTN_WEST: ev.code = BTN_TL; break;
				case BTN_Z: ev.code = BTN_TR; break;
				// analog stick buttons
				case BTN_TL2: ev.code = BTN_THUMBL; break;
				case BTN_TR2: ev.code = BTN_THUMBR; break;
				// do not report any other button
				case BTN_START: { ev.type = EV_ABS; ev.code = ABS_Z; ev.value = ev.value*255;}  break;
				case BTN_SELECT: { ev.type = EV_ABS; ev.code = ABS_RZ; ev.value = ev.value*255;}  break;
				default:
					return false;
				}
			}	return true;
			case EV_ABS: {
				switch(ev.code) {
				// D-Pad - on PS3 is digital, we'll use that one
				case ABS_HAT0Y: { ev.type = EV_ABS; ev.code = ABS_HAT0Y; ev.value = ev.value;} break; // up
				case ABS_HAT0X: { ev.type = EV_ABS; ev.code = ABS_HAT0X; ev.value = ev.value;} break; // left
				// left stick
				case ABS_Z: { ev.code = ABS_X; ev.value = (ev.value*(32768*2)/4095-32768) * -1;} break;
				case ABS_RX: { ev.code = ABS_Y; ev.value = (ev.value*(32768*2)/4095-32768) * -1;} break;
				// right stick
				case ABS_RY: { ev.code = ABS_RX; ev.value = ev.value*(32768*2)/4095-32768;} break;
				case ABS_RZ: { ev.code = ABS_RY; ev.value = ev.value*(32768*2)/4095-32768;} break;
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
	/*class rg351p_2_oga : public oga_target {
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
	};*/
	class ps3_2_oga : public oga_target {
public:
    int direction_hatX = BTN_DPAD_LEFT;
    int direction_hatY = BTN_DPAD_UP;

    ps3_2_oga() : oga_target() {} // Constructor added

    virtual bool translate_event(input_event& ev) {
        switch (ev.type) {
            case EV_SYN:
                return true;

            case EV_KEY: {
                switch (ev.code) {
                    // Map PS3 buttons to OGA equivalents
                    case BTN_SELECT: ev.code = BTN_SELECT; break; // Select -> Custom
                    case BTN_START: ev.code = BTN_START; break;  // Start -> Custom
                    case BTN_TL: ev.code = BTN_TRIGGER_HAPPY1; break;    // L1 -> Custom
                    case BTN_TR: ev.code = BTN_TRIGGER_HAPPY6; break;    // R1 -> Custom
                    case BTN_TL2: ev.code = BTN_TRIGGER_HAPPY2; break;   // L2 -> Custom
                    case BTN_TR2: ev.code = BTN_TRIGGER_HAPPY5; break;   // R2 -> Custom
                    
                    // Face buttons
                    case BTN_C: ev.code = BTN_NORTH; break; // Square -> Y
                    case BTN_NORTH: ev.code = BTN_WEST; break; // Triangle -> X
                    case BTN_SOUTH: ev.code = BTN_EAST; break; // Cross -> B
                    case BTN_EAST: ev.code = BTN_SOUTH; break; // Circle -> A

                    // Left/Right Analog Stick Clicks
                    case BTN_THUMBL: ev.code = BTN_TRIGGER_HAPPY3; break;
                    case BTN_THUMBR: ev.code = BTN_TRIGGER_HAPPY4; break;

                    default:
                        return false;
                }
            }
            return true;

            case EV_ABS: {
                // D-Pad translation
                if (ev.code == ABS_HAT0Y) {
                    ev.type = EV_KEY;
                    if (ev.value == -1) {
                        ev.code = BTN_DPAD_UP;
                        direction_hatY = BTN_DPAD_UP;
                    } else if (ev.value == 1) {
                        ev.code = BTN_DPAD_DOWN;
                        direction_hatY = BTN_DPAD_DOWN;
                    } else if (ev.value == 0) {
                        ev.code = direction_hatY;
                    }
                    return true;
                } 
                else if (ev.code == ABS_HAT0X) {
                    ev.type = EV_KEY;
                    if (ev.value == -1) {
                        ev.code = BTN_DPAD_LEFT;
                        direction_hatX = BTN_DPAD_LEFT;
                    } else if (ev.value == 1) {
                        ev.code = BTN_DPAD_RIGHT;
                        direction_hatX = BTN_DPAD_RIGHT;
                    } else if (ev.value == 0) {
                        ev.code = direction_hatX;
                    }
                    return true;
                }

                // Analog Sticks Mapping
                else if (ev.code == ABS_X) {
                    ev.code = ABS_X;
                    ev.value = (ev.value * 1800 / 4096) - 900;
                    return true;
                }
                else if (ev.code == ABS_Y) {
                    ev.code = ABS_Y;
                    ev.value = (ev.value * 1800 / 4096) - 900;
                    return true;
                }
                else if (ev.code == ABS_Z) {
                    ev.code = ABS_RX;
                    ev.value = (ev.value * 1800 / 4096) - 900;
                    return true;
                }
                else if (ev.code == ABS_RZ) {
                    ev.code = ABS_RY;
                    ev.value = (ev.value * 1800 / 4096) - 900;
                    return true;
                }
                return false;
            }

            default:
                break;
        }
        return false;
    }
};

	class rg351p_2_vkb : public vkb_target {
	public:
		int direction_hatX = KEY_LEFT;
		int direction_hatY = KEY_UP;
		virtual bool translate_event(input_event& ev) {
			switch(ev.type) {
			case EV_SYN:
				return true;
			case EV_KEY:{
				switch(ev.code) {
				// setup buttons
				case BTN_TL: ev.code = KEY_ENTER; break;    //start
				// action buttons
				case BTN_C: ev.code = KEY_Z; break; // square
				case BTN_NORTH: ev.code = KEY_X; break; // triangle
				case BTN_SOUTH: ev.code = KEY_S; break; // cross
				case BTN_EAST: ev.code = KEY_A; break; // round
				// back
				case BTN_WEST: ev.code = KEY_D; break;
				case BTN_Z: ev.code = KEY_F; break;
				default:
					return false;
				}
			}	return true;
			case EV_ABS: {
                if(ev.code == ABS_HAT0Y){
							ev.type = EV_KEY;
							 if(ev.value == -1){
							 	ev.code = KEY_UP;
							 	direction_hatY = KEY_UP;
                                  ev.value = 1;
							 }else if(ev.value == 1){
							 	ev.code = KEY_DOWN;
							 	direction_hatY = KEY_DOWN;
                                  ev.value = 1;
							 }else if(ev.value == 0){
							 	ev.code = direction_hatY;
                                  ev.value = 0;
							 }
							ev.value = ev.value;
                             return true;
						 }
                else if(ev.code == ABS_HAT0X){
							ev.type = EV_KEY;
							 if(ev.value == -1){
							 	ev.code = KEY_LEFT;
							 	direction_hatX = KEY_LEFT;
                                  ev.value = 1;
							 }else if(ev.value == 1){
							 	ev.code = KEY_RIGHT;
							 	direction_hatX = KEY_RIGHT;
                                  ev.value = 1;
							 }else if(ev.value == 0){
							 	ev.code = direction_hatX;
                                  ev.value = 0;
							 }
							
                              return true;						 
                           }
                             
                else{
                    return false;
                }
			}
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

	// Add support for ps3_bt → oga_joypad
	/*if(out_type == &joypads::j_oga) {
		if(in_type == &joypads::j_ps3_bt || in_type == &joypads::j_ps3_usb)
			return new ps3_2_oga();
		if(in_type == &joypads::j_rg351p)
			return new rg351p_2_oga();
	} */

	if(out_type == &joypads::j_oga) {
		if(in_type == &joypads::j_rg351p){
			return new rg351p_2_oga();
		}
		// for the moment we dont manage this because this type of devices
		// they have already a good event file
		printf("They have alreadfy a good file event\n");
		return nullptr;
	}

	// Switch logic for VKB
	if(out_type == &joypads::j_vkb) {
		if(in_type == &joypads::j_rg351p)
			return new rg351p_2_vkb();
	} 

	// Switch logic for Xbox 360
	if(out_type == &joypads::j_xbox_360) {
		if(in_type == &joypads::j_ps3_bt || in_type == &joypads::j_ps3_usb)
			return new ps3_2_xbox();
		if(in_type == &joypads::j_rg351p)
			return new rg351p_2_xbox();
	} 

	// Switch logic for PS3 devices
	else if(out_type == &joypads::j_ps3_usb || out_type == &joypads::j_ps3_bt) {
		if(in_type == &joypads::j_ps3_usb || in_type == &joypads::j_ps3_bt)
			return new ps3_2_ps3(out_type->bus);
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
