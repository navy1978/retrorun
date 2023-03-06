
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
#include "menu.h"
#include "menu_item.h"



Menu::Menu(std::string name, std::vector<MenuItem> items)
{
    name_ = name;
    items_ = items;
    for (std::vector<MenuItem>::size_type i = 0; i < items_.size(); i++)
    {
        if (i == 0)
            items_[i].setSelected(true);
        else
            items_[i].setSelected(false);
    }
    size_ = static_cast<std::vector<MenuItem>::size_type>(items_.size());
}
Menu::Menu()
{
}


int Menu::getSize() 
{
    return size_;
}

int Menu::getId() const
{
    return id_;
}

void Menu::setId(int id)
{
    id_ = id;
}

std::vector<MenuItem> &Menu::getItems()
{
    std::lock_guard<std::mutex> lock(items_mutex_);
    return items_;
}

void Menu::setItems(const std::vector<MenuItem> &items)
{
    std::lock_guard<std::mutex> lock(items_mutex_);
    items_ = items;
}

Menu *Menu::getPreviousMenu()
{
    return previousMenu_;
}

void Menu::setPreviousMenu(Menu *menu)
{
    previousMenu_ = menu;
}

bool Menu::hasPreviousMenu() const
{
    return previousMenu_ != nullptr;
}

std::string Menu::getName() const
{
    return name_;
}

void Menu::setName(const std::string &name)
{
    name_ = name;
}

void Menu::resetSelected()
{
    for (std::vector<MenuItem>::size_type i = 0; i < items_.size(); i++)
    {
        if (i == 0)
            items_[i].setSelected(true);
        else
            items_[i].setSelected(false);
    }
}

void Menu::resetSelected(int newSele)
{
    for (std::vector<MenuItem>::size_type i = 0; i < items_.size(); i++)
    {
        if (i == newSele)
            items_[i].setSelected(true);
        else
            items_[i].setSelected(false);
    }
}


