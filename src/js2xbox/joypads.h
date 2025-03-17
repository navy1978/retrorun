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

#ifndef _JOYPADS_H_
#define _JOYPADS_H_

#include "events.h"

namespace joypads {
	// supported joypads
	extern const events::js_desc	j_ps3_bt,
					j_ps3_usb,
					j_xbox_360,
					j_rg351p,
					j_vkb,
					j_oga;

	// supported input joypads
	extern const events::js_desc	*in[];

	// supported output joypads
	extern const events::js_desc	*out[];
}

#endif //_JOYPADS_H_
