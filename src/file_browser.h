#pragma once

#include <string>
struct rr_surface;

bool rr_file_browser_visible();
void rr_file_browser_open(const std::string& initial_path);
void rr_file_browser_close();
void rr_file_browser_input(bool up, bool down, bool accept, bool cancel);
void rr_file_browser_render(rr_surface* surface, int width, int height);
