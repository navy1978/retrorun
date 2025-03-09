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

#include "menu_item.h"

// #include <iostream>

MenuItem::MenuItem(std::string name, std::vector<std::string> values, int valueSelected, std::function<void(int)> action)
{
    name_ = name;
    values_ = values;
    valueSelected_ = valueSelected;
    action_ = action;
    is_menu_ = true;
    menu_ = nullptr;
    mis_unit_ = "";
    is_quit_item = false;
    is_question_item= false;
    m_valueCalculator = nullptr;
    m_nameCalculator=nullptr;
}
MenuItem::MenuItem(std::string name, ValueCalculator valueCalculator, std::function<void(int)> action, std::string mis_unit)
{
    name_ = name;
    valueSelected_ = -1;
    action_ = action;
    is_menu_ = true;
    menu_ = nullptr;
    mis_unit_ = mis_unit;
    is_quit_item = false;
    is_question_item= false;
    m_valueCalculator = valueCalculator;
    m_nameCalculator=nullptr;
}

//MenuItem::MenuItem(std::string name, void (*action)(int))
MenuItem::MenuItem(std::string name, std::function<void(int)> action) 
{
    name_ = name;
    valueSelected_ = -1;
    action_ = action;
    is_menu_ = false;
    menu_ = nullptr;
    mis_unit_ = "";
    is_quit_item = false;
    is_question_item = false;
    m_valueCalculator = nullptr;
    m_nameCalculator = nullptr;
    
}


MenuItem::MenuItem(std::string name, Menu *menu, std::function<void(int)> action)
{
    name_ = name;
    valueSelected_=0;
    action_ = action;
    is_menu_ = false;
    menu_ = menu;
    mis_unit_ = "";
    is_quit_item = false;
    is_question_item= false;
    m_valueCalculator = nullptr;
    m_nameCalculator=nullptr;
    
}


MenuItem::MenuItem(NameCalculator nameCalculator, Menu *menu, std::function<void(int)> action)
{
    
    name_ = "fake"; // we will use the nameCalculator instead
    valueSelected_=0;
    action_ = action;
    is_menu_ = false;
    menu_ = menu;
    mis_unit_ = "";
    is_quit_item = false;
    is_question_item= false;
    m_valueCalculator = nullptr;
    m_nameCalculator=nameCalculator;
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

void MenuItem::setQuestionItem()
{
    is_question_item = true;
    values_ = {"no", "yes"};
    selected_ = "no";
    valueSelected_ = 0;
}

bool MenuItem::isQuestion()
{
    return is_question_item;
}



void MenuItem::execute(int button)
{
    if (action_ != NULL){
        //printf( "Executing action for button: %d\n", button);
        action_(button);
    }
}

std::string MenuItem::get_name()
{
    if (m_nameCalculator!= nullptr){
        return m_nameCalculator();
    }else return name_;
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

        return valueSelected_ ;
    }
}

Menu *MenuItem::getMenu()
{
    return menu_;
}

std::string MenuItem::getMisUnit()
{

    if (mis_unit_ == "bool" 
    || mis_unit_ == "rotation" 
    || mis_unit_ == "aspect-ratio"
    || mis_unit_ == "device-type"
    || mis_unit_ == "analog-to-digital"
    || mis_unit_ == "test-rumble"
    )
    {
        return "";
    }
    else
    {
        return mis_unit_;
    }
}

void MenuItem::setPossibleValues(std::map<unsigned, std::string> possiblevaluesMap){
    valuesMap=possiblevaluesMap;
}

const char *rotation_names[] = {
    "DISABLED",
    "ENABLED",
    "REVERSED",
    "AUTO"};

const char *analog_to_digital_names[] = {
    "NONE",
  "LEFT",
  "RIGHT",
  "LEFT FORCED",
  "RIGHT FORCED"};  

const char *aspect_ratio_names[] = {
    "2:1",
    "4:3",
    "5:4",
    "16:9",
    "16:10",
    "1:1",
    "3:2",
    "auto"};


    std::string MenuItem::getDeviceType(int deviceIndex)
{
    auto it = valuesMap.find(deviceIndex);
    if (it != valuesMap.end())
    {
        return it->second; // Return description if ID is found
    }
    else
    {
        return "ID not found"; // Return error message if ID is not found
    }
}


std::string MenuItem::getStringValue()
{
    if (mis_unit_ == "bool")
    {
        return getValue() == 1 ? "yes" : "no";
    }
    else if (mis_unit_ == "rotation")
    {
        return rotation_names[getValue()];
    }
    else if (mis_unit_ == "aspect-ratio")
    {
        return aspect_ratio_names[getValue()];
    }
    else if (mis_unit_ == "device-type")
    {
        return getDeviceType(getValue());
    }
    else if (mis_unit_ == "analog-to-digital")
    {
        return analog_to_digital_names[getValue()];
    }else if (mis_unit_ == "test-rumble")
    {
        return "Press button";
    }
    else
    {
        return std::to_string(getValue());
    }
}