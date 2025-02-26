
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
#include <string>
#include <vector>
#include <functional>

class Menu; // forward declaration

typedef int (*ValueCalculator)();
typedef std::string (*NameCalculator)();

class MenuItem
{
public:
    MenuItem(std::string name, std::vector<std::string> values, int valueSelected, std::function<void(int)> action);
    // MenuItem(std::string name, int (*valueFunc)(), void (*action)());
    MenuItem(std::string name, ValueCalculator valueCalculator, std::function<void(int)> action, std::string mis_unit);
    MenuItem(std::string name, Menu *menu, std::function<void(int)> action);
    MenuItem(NameCalculator nameCalculator, Menu *menu, std::function<void(int)> action);
    //MenuItem(std::string name, void (*action)(int));
    MenuItem(std::string name, std::function<void(int)> action);
    void execute(int button);
    std::string get_name();
    std::string getMisUnit();
    void setName(std::string value);
    void setSelected(bool value);
    bool isSelected();
    std::vector<std::string> getValues();
    void setValue(int value);
    void setQuitItem();
    void setQuestionItem();
    bool isQuit();
    bool isQuestion();
    int getValue();
    Menu *getMenu();
    // bool isMenu();
    bool selected_;
    bool is_menu_;
    ValueCalculator m_valueCalculator;
    NameCalculator m_nameCalculator;
    std::string getStringValue();

private:
    std::string name_;
    std::string mis_unit_;
    std::function<void(int)> action_;
    bool is_quit_item;
    bool is_question_item;
    std::string key_;
    std::vector<std::string> values_;
    int valueSelected_;
    Menu *menu_;
};

// #endif  // MENU_ITEM_H
