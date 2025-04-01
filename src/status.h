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
#pragma once


#include <go2/display.h>



enum STATUS_POSITION
{
  TOP_LEFT= 0,
  TOP_RIGHT,
  BUTTOM_LEFT,
  BUTTOM_RIGHT,
  BUTTOM_CENTER,
  FULL,

};

class status  {

    public: 
    go2_surface_t *top_right; 
    go2_surface_t *bottom_right;
    go2_surface_t *bottom_center;
    go2_surface_t *top_left;
    go2_surface_t *bottom_left;
    go2_surface_t *full;
    bool show_top_right;
    bool show_bottom_right;
    bool show_bottom_center;
    bool show_top_left;
    bool show_bottom_left;
    bool show_full;

    bool clean_top_right;
    bool clean_bottom_right;
    bool clean_bottom_center;
    bool clean_top_left;
    bool clean_bottom_left;
    bool clean_full;

    bool last_top_right;
    bool last_bottom_right;
    bool last_bottom_center;
    bool last_top_left;
    bool last_bottom_left;
    bool last_full;

} ;