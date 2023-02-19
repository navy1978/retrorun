#pragma once


#include <go2/display.h>



enum STATUS_POSITION
{
  TOP_LEFT= 0,
  TOP_RIGHT,
  BUTTOM_LEFT,
  BUTTOM_RIGHT,
  FULL,

};

class status  {

    public: 
    go2_surface_t *top_right; 
    go2_surface_t *bottom_right;
    go2_surface_t *top_left;
    go2_surface_t *bottom_left;
    go2_surface_t *full;
    bool show_top_right;
    bool show_bottom_right;
    bool show_top_left;
    bool show_bottom_left;
    bool show_full;
} ;