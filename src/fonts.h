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

extern unsigned char fontdata8x8[64*16];
extern unsigned char fontdata6x8[256-32][8];

void basic_text_out16_nf(void *fb, int w, int x, int y, const char *text);
void basic_text_out16(void *fb, int w, int x, int y, const char *texto, ...);
void basic_text_out16_color(void *fb, int w, int x, int y, unsigned short color, const char *texto, ...);
void basic_text_out_uyvy_nf(void *fb, int w, int x, int y, const char *text);
void basic_text_out16_nf_color(void *fb, int w, int x, int y, const char *text, unsigned short color);
void basic_text_out16x16_nf_color_scaled_from_8x8(void *fb, int w, int x, int y, const char *text, unsigned short color);