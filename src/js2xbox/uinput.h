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

#ifndef _UINPUT_H_
#define _UINPUT_H_

#include <linux/uinput.h>
#include "events.h"

namespace uinput {
	class pad {
		pad(const pad&);
		pad& operator=(const pad&);
	protected:
		int	_fd;
	public:
		pad(void);
		~pad();
		
		virtual bool translate_event(input_event& ev) = 0;
		void send_event(const input_event& ev);
		char *get_device_name(char *evt_path, const int sz);
	};

	pad*	get_pad(const events::js_desc *in_type, const events::js_desc *out_type);

	class evt_reader {
		evt_reader(const evt_reader&);
		evt_reader& operator=(const evt_reader&);

		int	_fd,
			_efd;
	public:
		evt_reader(const char* evt_file);
		~evt_reader();
		
		bool read(input_event& ev, const int tmout = 250);
	};
}

#endif //_UINPUT_H_

