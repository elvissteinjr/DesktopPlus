#include "NotificationIcon.h"

#include <windowsx.h>
#include <shellapi.h>
#include <cwchar>

#include "UIManager.h"
#include "InterprocessMessaging.h"
#include "resource.h"

#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)

#define WM_DPLUSUI_NOTIFICATIONICON WM_USER
static LPCWSTR const g_WindowClassNameNotificationIcon = L"elvdesktopUINotifIcon";

LRESULT __stdcall NotificationIcon::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DPLUSUI_NOTIFICATIONICON)
    {
        if (UIManager::Get())
        {
            UIManager::Get()->GetNotificationIcon().OnCallbackMessage(wParam, lParam);
        }
        return 0;
    }

    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

NotificationIcon::NotificationIcon() : m_Instance(nullptr), m_PopupMenu(nullptr)
{
    m_IconData = { sizeof(NOTIFYICONDATA) };
}

NotificationIcon::~NotificationIcon()
{
    if (m_PopupMenu != nullptr)
    {
        DestroyMenu(m_PopupMenu);
    }

    ::Shell_NotifyIcon(NIM_DELETE, &m_IconData);

    if (m_IconData.hWnd != nullptr)
    {
        ::DestroyWindow(m_IconData.hWnd);
        ::UnregisterClass(g_WindowClassNameNotificationIcon, m_Instance);
    }
}

bool NotificationIcon::Init(HINSTANCE hinstance)
{
    m_Instance = hinstance;

    //Register window class
    //Using an extra window allows us to have the popup menu not focus the main window and also prevent Windows from treating desktop and non-desktop mode as separate icons
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, NotificationIcon::WndProc, 0L, 0L, m_Instance, nullptr, nullptr, nullptr, nullptr, g_WindowClassNameNotificationIcon, nullptr };
    ::RegisterClassEx(&wc);

    //Create notification icon
    m_IconData.hWnd = ::CreateWindow(wc.lpszClassName, L"Desktop+ UI Notification Icon", 0, 0, 0, 1, 1, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    m_IconData.uID = 0;
    m_IconData.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    m_IconData.uCallbackMessage = WM_DPLUSUI_NOTIFICATIONICON;
    m_IconData.hIcon = (HICON)::LoadImage(m_Instance, MAKEINTRESOURCE(IDI_DPLUS), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    wcscpy(m_IconData.szTip, L"Desktop+");
    m_IconData.uVersion = NOTIFYICON_VERSION_4;

    bool ret = ::Shell_NotifyIcon(NIM_ADD, &m_IconData);
    ::Shell_NotifyIcon(NIM_SETVERSION, &m_IconData);

    //Create popup menu
    m_PopupMenu = ::CreatePopupMenu();
    if (m_PopupMenu != nullptr)
    {
        if (UIManager::Get()->IsInDesktopMode())
        {
            ::InsertMenu(m_PopupMenu, 0, MF_BYPOSITION | MF_STRING, 1, L"Restore VR Interface");
        }
        else
        {
            ::InsertMenu(m_PopupMenu, 0, MF_BYPOSITION | MF_STRING, 1, L"Open Settings on Desktop");
        }

        ::InsertMenu(m_PopupMenu, 1, MF_BYPOSITION | MF_STRING, 2, L"Quit");
    }

    return ret;
}

void NotificationIcon::OnCallbackMessage(WPARAM wparam, LPARAM lparam)
{
    switch (LOWORD(lparam))
    {
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_CONTEXTMENU:
        { 
            POINT const pt = { GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam) };

            //Respect menu drop alignment
            UINT flags = TPM_BOTTOMALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD;
            if (::GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
            {
                flags |= TPM_RIGHTALIGN;
            }
            else
            {
                flags |= TPM_LEFTALIGN;
            }

            ::SetForegroundWindow(m_IconData.hWnd);
            BOOL sel = ::TrackPopupMenu(m_PopupMenu, flags, pt.x, pt.y, 0, m_IconData.hWnd, nullptr);

            switch (sel)
            {
                case 1 /*Open Settings on Desktop / Restore VR Interface*/:
                {
                    UIManager::Get()->Restart(!UIManager::Get()->IsInDesktopMode());
                    break;
                }
                case 2 /*Quit*/:
                {
                    //Kindly ask dashboard process to quit
                    if (HWND window = ::FindWindow(g_WindowClassNameDashboardApp, nullptr))
                    {
                        ::PostMessage(window, WM_QUIT, 0, 0);
                    }

                    UIManager::Get()->DisableRestartOnExit();
                    ::PostMessage(UIManager::Get()->GetWindowHandle(), WM_QUIT, 0, 0);
                    break;
                }
            }

            break;
        }
        break;
    }
}
