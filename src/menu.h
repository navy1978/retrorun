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
#ifndef MENU_H
#define MENU_H

#include <string>
#include <vector>
#include <mutex>


class MenuItem;

class Menu {
public:
     Menu(std::string name, std::vector<MenuItem> items);
    Menu();
    int getId() const;
    void setId(int id);
    std::vector<MenuItem>& getItems();
    void setItems(const std::vector<MenuItem>& items);
    Menu* getPreviousMenu();
    void setPreviousMenu(Menu* menu);
    bool hasPreviousMenu() const;
    std::string getName() const;
    void setName(const std::string& name);
    void resetSelected();
    void resetSelected(int newSel);
    int getSize();

private:
    int id_;
    std::vector<MenuItem> items_;
    Menu* previousMenu_;
    std::string name_;
    int size_;
    std::mutex items_mutex_;
};

#endif 

