/*
retrorun - libretro keyboard dispatcher
Copyright (C) 2021-present navy1978

Shared between all platform backends. Platform code translates native key
events into RRKeyEvent and calls rr_keyboard_event(). This module forwards
the event to the core's retro_keyboard_event_t callback if one was registered.
*/

#include "keyboard.h"
#include "logger.h"
#include "fonts.h"

#include <algorithm>
#include <cstring>

// The core's registered callback (NULL when none is active).
static retro_keyboard_event_t s_core_keyboard_cb = nullptr;
static bool s_virtual_visible = false;
static bool s_shift = false;
static bool s_virtual_just_opened = false;
static int s_row = 0;
static int s_column = 0;

struct VirtualKey { const char* label; unsigned code; char normal; char shifted; };
static const VirtualKey rows[][12] = {
    {{"1",RETROK_1,'1','!'},{"2",RETROK_2,'2','@'},{"3",RETROK_3,'3','#'},{"4",RETROK_4,'4','$'},{"5",RETROK_5,'5','%'},{"6",RETROK_6,'6','^'},{"7",RETROK_7,'7','&'},{"8",RETROK_8,'8','*'},{"9",RETROK_9,'9','('},{"0",RETROK_0,'0',')'}},
    {{"Q",RETROK_q,'q','Q'},{"W",RETROK_w,'w','W'},{"E",RETROK_e,'e','E'},{"R",RETROK_r,'r','R'},{"T",RETROK_t,'t','T'},{"Y",RETROK_y,'y','Y'},{"U",RETROK_u,'u','U'},{"I",RETROK_i,'i','I'},{"O",RETROK_o,'o','O'},{"P",RETROK_p,'p','P'}},
    {{"A",RETROK_a,'a','A'},{"S",RETROK_s,'s','S'},{"D",RETROK_d,'d','D'},{"F",RETROK_f,'f','F'},{"G",RETROK_g,'g','G'},{"H",RETROK_h,'h','H'},{"J",RETROK_j,'j','J'},{"K",RETROK_k,'k','K'},{"L",RETROK_l,'l','L'}},
    {{"Z",RETROK_z,'z','Z'},{"X",RETROK_x,'x','X'},{"C",RETROK_c,'c','C'},{"V",RETROK_v,'v','V'},{"B",RETROK_b,'b','B'},{"N",RETROK_n,'n','N'},{"M",RETROK_m,'m','M'},{"BS",RETROK_BACKSPACE,0,0},{"ENT",RETROK_RETURN,0,0}},
    {{"SPACE",RETROK_SPACE,' ',' '},{"TAB",RETROK_TAB,0,0},{"ESC",RETROK_ESCAPE,0,0}}
};
static const int row_sizes[] = {10,10,9,9,3};

extern Logger logger;

void rr_keyboard_set_callback(retro_keyboard_event_t cb)
{
    s_core_keyboard_cb = cb;
    logger.log(Logger::DEB, "Keyboard callback registered by core: %p", (void*)cb);
}

void rr_keyboard_clear_callback()
{
    s_core_keyboard_cb = nullptr;
    s_virtual_visible = false;
}

bool rr_keyboard_virtual_visible() { return s_virtual_visible; }
void rr_keyboard_virtual_open() { if (s_core_keyboard_cb) { s_virtual_visible = true; s_virtual_just_opened = true; } }
void rr_keyboard_virtual_close() { s_virtual_visible = false; }

void rr_keyboard_virtual_input(bool up, bool down, bool left, bool right,
                               bool accept, bool cancel, bool shift)
{
    if (!s_virtual_visible) return;
    if (s_virtual_just_opened) { s_virtual_just_opened = false; return; }
    if (cancel) { s_virtual_visible = false; return; }
    if (shift) s_shift = !s_shift;
    if (up) s_row = (s_row + 4) % 5;
    if (down) s_row = (s_row + 1) % 5;
    s_column = std::min(s_column, row_sizes[s_row] - 1);
    if (left) s_column = (s_column + row_sizes[s_row] - 1) % row_sizes[s_row];
    if (right) s_column = (s_column + 1) % row_sizes[s_row];
    if (!accept) return;
    const VirtualKey& key = rows[s_row][s_column];
    const uint32_t character = static_cast<unsigned char>(s_shift ? key.shifted : key.normal);
    const uint16_t modifiers = s_shift ? RETROKMOD_SHIFT : RETROKMOD_NONE;
    RRKeyEvent event = {true, key.code, character, modifiers};
    rr_keyboard_event(&event);
    event.down = false;
    rr_keyboard_event(&event);
}

void rr_keyboard_virtual_render(rr_surface_t* surface, int width, int height)
{
    if (!surface) return;
    uint16_t* pixels = static_cast<uint16_t*>(rr_surface_map(surface));
    const int stride = rr_surface_stride_get(surface) / 2;
    std::fill(pixels, pixels + stride * height, static_cast<uint16_t>(0x0841));
    basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, 8,
                                     "VIRTUAL KEYBOARD", 0xffff);
    basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, 20,
                                     "D-PAD Move  A Type  B Close  X Shift", 0xbdf7);
    const int top = 42;
    for (int row = 0; row < 5; ++row) {
        const int cell_w = std::max(24, (width - 16) / row_sizes[row]);
        for (int col = 0; col < row_sizes[row]; ++col) {
            const int px = 8 + col * cell_w;
            const int py = top + row * 24;
            const bool selected = row == s_row && col == s_column;
            if (selected) {
                for (int yy = py - 3; yy < py + 13 && yy < height; ++yy)
                    std::fill(pixels + yy * stride + px - 3,
                              pixels + yy * stride + std::min(width, px + cell_w - 2),
                              static_cast<uint16_t>(0x39e7));
            }
            basic_text_out16_nf_color_clipped(pixels, stride, width, height, px, py,
                                             rows[row][col].label,
                                             selected ? 0xffe0 : 0xffff);
        }
    }
    basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, height - 14,
                                     s_shift ? "SHIFT: ON" : "SHIFT: off", s_shift ? 0xffe0 : 0x7bef);
    rr_surface_unmap(surface);
}

bool rr_keyboard_has_callback()
{
    return s_core_keyboard_cb != nullptr;
}

// Called by platform backends (SDL2, GO2, future virtual keyboard).
// Forwards the event to the core if a callback is registered.
void rr_keyboard_event(const RRKeyEvent* event)
{
    if (!event) return;
    if (s_core_keyboard_cb)
    {
        s_core_keyboard_cb(event->down, event->keycode,
                           event->character, event->modifiers);
    }
}
