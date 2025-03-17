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

#include "joypads.h"
#include <linux/uinput.h>

// supported joypads
const events::js_desc	joypads::j_ps3_bt =	{ "ps3_bt", BUS_BLUETOOTH, 0x054c, 0x0268, 0x0110 },
			joypads::j_ps3_usb =	{ "ps3_usb", BUS_USB, 0x054c, 0x0268, 0x0111 },
			joypads::j_xbox_360 =	{ "xbox_360", BUS_USB, 0x045e, 0x028e, 0x0110 },
			joypads::j_rg351p =	{ "rg351p_joypad", BUS_USB, 0x1209, 0x3100, 0x0111 },
			joypads::j_oga =	{ "oga_joypad", BUS_USB, 0x1, 0x1, 0x0101 },
			joypads::j_vkb =	{ "at_vkb", 0x11, 0x1, 0x1, 0xab41 };
// supported input joypads
const events::js_desc	*joypads::in[] = {
	&j_ps3_bt,
	&j_ps3_usb,
	&j_rg351p,
	0
};

// supported output joypads
const events::js_desc	*joypads::out[] = {
	&j_xbox_360,
	&j_ps3_bt,
	&j_ps3_usb,
	&j_oga,
	&j_vkb,
	0
};
