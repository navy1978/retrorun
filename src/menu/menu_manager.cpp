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
#include "menu_manager.h"
#include "../globals.h"
#include <queue>
#include <mutex>

std::mutex mutexManager;

constexpr std::chrono::milliseconds MENU_INPUT_THRESHOLD{250};
// constexpr std::chrono::milliseconds MENU_INPUT_LEFT_RIGHT_THRESHOLD{50};
auto lastPress = std::chrono::steady_clock::now();
auto currentPress = std::chrono::steady_clock::now();

MenuManager::MenuManager()
{
}

void MenuManager::handle_input_credits(int buttonPressed)
{

    Menu &menu = *currentMenu_;

    // managing the selected item in the Menu via UP and DOWN buttons
    int selected = 0;
    for (int i = 0; i < menu.getSize(); i++)
    {
        if (menu.getItems()[i].isSelected())
        {
            selected = i;
            break; // add this line
        }
    }

    MenuItem &mi = menu.getItems()[selected];
    mi.execute(buttonPressed);
}


bool buttonPressedIs(int buttonPressed, int direction) {
    if (!isTate() &&
        (buttonPressed == direction)) {
        return true;
    } else if (tateState == REVERSED){
         if ((buttonPressed == UP && direction== LEFT) || 
         (buttonPressed == DOWN && direction== RIGHT)  ||
         (buttonPressed == RIGHT && direction== UP)  ||
         (buttonPressed == LEFT && direction== DOWN)  
         ){
        return true;
         }
    }else if (isTate()){
        if ((buttonPressed == UP && direction== RIGHT) || 
         (buttonPressed == DOWN && direction== LEFT)  ||
         (buttonPressed == RIGHT && direction== DOWN)  ||
         (buttonPressed == LEFT && direction== UP)  
         ){
        return true;
         }
    }
    
    return false;
}



void MenuManager::handle_input(int buttonPressed)
{
   
    std::lock_guard<std::mutex> lock(mutexManager);
    currentPress = std::chrono::steady_clock::now();
    auto now_seconds = std::chrono::duration_cast<std::chrono::seconds>(currentPress - lastPress).count();
    auto now_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(currentPress - lastPress).count() % 1000;
    auto elapsed_time_ms = now_seconds * 1000 + now_milliseconds;

    // std::chrono::milliseconds THRESHOLD = (buttonPressed == LEFT || buttonPressed == RIGHT) ? MENU_INPUT_LEFT_RIGHT_THRESHOLD : MENU_INPUT_THRESHOLD;

    if (elapsed_time_ms > MENU_INPUT_THRESHOLD.count())
    {
        lastPress = currentPress;

        std::lock_guard<std::mutex> lock(current_menu_mutex_);

        if (currentMenu_ == nullptr)
        {
            logger.log(Logger::DEB, "Nothing to do: current menu is null!\n");
            return;
        }

        Menu &menu = *currentMenu_;

        // managing the selected item in the Menu via UP and DOWN buttons
        int selected = 0;
        for (int i = 0; i < menu.getSize(); i++)
        {
            if (menu.getItems()[i].isSelected())
            {
                selected = i;
                break; // add this line
            }
        }
        if (buttonPressedIs(buttonPressed, UP))
        {
            selected--;
            if (selected < 0)
                selected = 0;
                
        }
        if (buttonPressedIs(buttonPressed, DOWN))
        {
            selected++;
            if (selected > menu.getSize() - 1)
                selected = menu.getSize() - 1;
        }

        std::vector<MenuItem> &items = menu.getItems(); // move this line outside the loop

        for (int i = 0; i < menu.getSize(); i++)
        {
            if (i == selected)
            {
                items[i].selected_ = true;
            }
            else
            {
                items[i].selected_ = false;
            }
        }

        // managing the change of the values vie LEFT and RIGHT
        if (buttonPressedIs(buttonPressed, LEFT))
        {
            MenuItem &mi = menu.getItems()[selected];
            if (mi.isQuit() || mi.isQuestion())
            {
                int newSelected = mi.getValue() == 0 ? 1 : 0;
                mi.setValue(newSelected);
            }
            else
            {
                mi.execute(LEFT);
            }
        }

        if (buttonPressedIs(buttonPressed, RIGHT))
        {
            MenuItem &mi = menu.getItems()[selected];
            if (mi.isQuit() || mi.isQuestion())
            {
                int newSelected = mi.getValue() == 0 ? 1 : 0;
                mi.setValue(newSelected);
            }
            else
            {
                mi.execute(RIGHT);
            }
        }

        // managing the navigation with A and B button
        if (buttonPressed == A_BUTTON)
        {
            MenuItem &mi = menu.getItems()[selected];
            if (mi.isQuit() || mi.isQuestion())
            {
                int quit = mi.getValue();
                if (quit == 1)
                {
                    mi.execute(A_BUTTON);
                }
                if (mi.isQuestion())
                {
                    // we go back to the main menu
                    if (!queueMenus.empty())
                    {
                        Menu *prevMenu = queueMenus.top();
                        queueMenus.pop();
                        currentMenu_ = prevMenu;
                        currentMenu_->resetSelected();
                    }
                }
                return;
                
            }

            if (mi.get_name() == SHOW_DEVICE || mi.get_name() == SHOW_CORE 
            || mi.get_name() == SHOW_GAME 
            || (mi.get_name().find("empty") != std::string::npos && mi.get_name().find("<-") != std::string::npos ))
            {
                return;
            }

            if (mi.getMenu() != nullptr)
            {
                if (currentMenu_ != nullptr)
                {
                    queueMenus.push(currentMenu_); // push the current menu onto the queue before updating it
                }
                currentMenu_ = mi.getMenu();
                currentMenu_->resetSelected();
            }
            else
            {
                mi.execute(A_BUTTON);
            }
        }

        if (buttonPressed == B_BUTTON)
        {
            if (!queueMenus.empty())
            {
                Menu *prevMenu = queueMenus.top();
                queueMenus.pop();
                currentMenu_ = prevMenu;
                currentMenu_->resetSelected();
            }
        }
    }
}

Menu &MenuManager::getCurrentMenu()
{
    std::lock_guard<std::mutex> lock(current_menu_mutex_);
    return *currentMenu_;
}

void MenuManager::setCurrentMenu(Menu *menu)
{
    std::lock_guard<std::mutex> lock(current_menu_mutex_);
    currentMenu_ = menu;
}

void MenuManager::resetMenu(){
    Menu *prevMenu = queueMenus.top();
    queueMenus.pop();
    currentMenu_ = prevMenu;
    currentMenu_->resetSelected();
}
