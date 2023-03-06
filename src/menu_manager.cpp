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
#include "menu_manager.h"
#include <queue>
#include <mutex> 

std::mutex mutexManager;


constexpr std::chrono::milliseconds MENU_INPUT_THRESHOLD{250};
//constexpr std::chrono::milliseconds MENU_INPUT_LEFT_RIGHT_THRESHOLD{50};
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

void MenuManager::handle_input(int buttonPressed)
{
    // mutex.lock();

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
            printf("Nothing to do: current menu is null!\n");
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
        if (buttonPressed == UP)
        {
            selected--;
            if (selected < 0)
                selected = 0;
        }
        else if (buttonPressed == DOWN)
        {
            selected++;
            if (selected > menu.getSize()- 1)
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
        if (buttonPressed == LEFT)
        {
            MenuItem &mi = menu.getItems()[selected];
            mi.execute(LEFT);
        }

        if (buttonPressed == RIGHT)
        {
            MenuItem &mi = menu.getItems()[selected];
            mi.execute(RIGHT);
        }

        // managing the navigation with A and B button
        if (buttonPressed == A_BUTTON)
        {
            MenuItem &mi = menu.getItems()[selected];
            if (mi.get_name()== SHOW_DEVICE || mi.get_name()== SHOW_CORE  || mi.get_name()== SHOW_GAME){
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
            }else{
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
