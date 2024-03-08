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

#include "menu_item.h"
// #include <iostream>

MenuItem::MenuItem(std::string name, std::vector<std::string> values, int valueSelected, void (*action)(int))
{
    name_ = name;
    values_ = values;
    valueSelected_ = valueSelected;
    action_ = action;
    is_menu_ = true;
    menu_ = NULL;
    is_quit_item = false;
}
MenuItem::MenuItem(std::string name, ValueCalculator valueCalculator, void (*action)(int), std::string mis_unit)
{
    name_ = name;
    m_valueCalculator = valueCalculator;
    action_ = action;
    is_menu_ = true;
    menu_ = NULL;
    mis_unit_ = mis_unit;
    is_quit_item = false;
}

MenuItem::MenuItem(std::string name, void (*action)(int))
{
    name_ = name;
    action_ = action;
    is_menu_ = false;
    valueSelected_ = -1; // to distinguish from a normal menu item with options...
    menu_ = NULL;
    m_valueCalculator = NULL;
    mis_unit_ = "";
    is_quit_item = false;
}

MenuItem::MenuItem(std::string name, Menu *menu, void (*action)(int))
{
    name_ = name;
    menu_ = menu;
    action_ = action;
    is_menu_ = false;
    m_valueCalculator = NULL;
    mis_unit_ = "";
    is_quit_item = false;
}

void MenuItem::setQuitItem()
{
    is_quit_item = true;
    values_ = {"no", "yes"};
    selected_ = "no";
    valueSelected_ = 0;
}

bool MenuItem::isQuit()
{
    return is_quit_item;
}

void MenuItem::execute(int button)
{
    if (action_ != NULL)
        action_(button);
}

std::string MenuItem::get_name()
{
    return name_;
}

void MenuItem::setName(std::string value)
{
    name_ = value;
}

void MenuItem::setSelected(bool value)
{
    selected_ = value;
}
bool MenuItem::isSelected()
{
    return selected_;
}

std::vector<std::string> MenuItem::getValues()
{
    return values_;
}

void MenuItem::setValue(int value)
{
    valueSelected_ = value;
}

int MenuItem::getValue()
{
    if (m_valueCalculator != nullptr)
    {
        return m_valueCalculator();
    }
    else
    {

        return valueSelected_;
    }
}

Menu *MenuItem::getMenu()
{
    return menu_;
}

std::string MenuItem::getMisUnit()
{

    if (mis_unit_ == "bool"|| mis_unit_ == "rotation")
    {
        return "";
    }
    else
    {
        return mis_unit_;
    }
}
/*bool MenuItem::isMenu(){
    return is_menu_;
}*/

const char* rotation_names[] = {
    "DISABLED",
    "ENABLED",
    "REVERSED",
    "AUTO"
};


std::string MenuItem::getStringValue()
{
    if (mis_unit_ == "bool")
    {
        return getValue() == 1 ? "yes" : "no";
    }else if (mis_unit_ == "rotation")
    {
        return rotation_names[getValue()];
    }
    else
    {
        return std::to_string(getValue());
    }
}