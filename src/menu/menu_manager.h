/*
retrorun-go2 - libretro frontend for the ODROID-GO Advance
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
#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <string>
#include <vector>

#include "menu.h"
#include "menu_item.h"
#include <stack>
#include <mutex>



class MenuManager {
public:
    MenuManager();
    void handle_input(int buttonPressed);
    void handle_input_credits(int buttonPressed);
    Menu& getCurrentMenu();
    void setCurrentMenu(Menu* menu);
    Menu& getPreviousMenu();
    void setPreviousMenu(Menu* menu);
    //void verify();

private:
    Menu* currentMenu_;
    Menu* previousMenu_;
    std::stack<Menu*> queueMenus;
    //std::queue<int> queueItems;
    std::mutex current_menu_mutex_;
};

#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3
#define A_BUTTON 4
#define B_BUTTON 5

const std::string SHOW_DEVICE = "SHOW_DEVICE";
const std::string SHOW_CORE = "SHOW_CORE";
const std::string SHOW_GAME = "SHOW_GAME";
const std::string SHOW_CREDITS = "Credits";


#endif  // MENU_H


