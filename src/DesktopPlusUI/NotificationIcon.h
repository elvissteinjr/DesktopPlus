//Provides a notification area/tray icon
//Technically a singleton, but is designed to have its one instance live in UIManager for proper lifetime management
//Once intialized, it's fully self-contained

#pragma once

#define NOMINMAX
#include <Windows.h>

class NotificationIcon
{
    private:
        HINSTANCE m_Instance;
        NOTIFYICONDATA m_IconData;
        HMENU m_PopupMenu;

        static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    public:
        NotificationIcon();
        ~NotificationIcon();
        bool Init(HINSTANCE hinstance);
        void RefreshPopupMenu();        //Destroys the popup menu if it exists and recreates it. Called after Init() to update translation strings
        void OnCallbackMessage(WPARAM wparam, LPARAM lparam);
};