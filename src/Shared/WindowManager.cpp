
#include "WindowManager.h"

#include <algorithm>
#include <iostream>

#include <dwmapi.h>
#include <Psapi.h>

#include "ConfigManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"

#ifndef DPLUS_UI
    #include "InputSimulator.h"
#endif

WindowManager g_WindowManager;

bool inline MatchTitleAndClassName(WindowInfo const& window, std::wstring const& title, std::wstring const& className)
{
    return ( (window.GetTitle() == title) && (window.GetWindowClassName() == className) );
}

bool IsKnownBlockedWindow(WindowInfo const& window)
{
    bool is_blocked = ( // Task View
                       (MatchTitleAndClassName(window, L"Task View", L"Windows.UI.Core.CoreWindow")) ||
                        // XAML Islands
                       (MatchTitleAndClassName(window, L"DesktopWindowXamlSource", L"Windows.UI.Core.CoreWindow")) ||
                        // XAML Popups
                       (MatchTitleAndClassName(window, L"PopupHost", L"Xaml_WindowedPopupClass")) );

    // Capture Picker Window (child of ApplicationFrameWindow)
    if ( (!is_blocked) && (window.GetWindowClassName() == L"ApplicationFrameWindow") )
    {
        is_blocked = (::FindWindowExW(window.GetWindowHandle(), nullptr, L"Windows.UI.Core.CoreWindow", L"CapturePicker") != nullptr);
    }

    // Jumplists and Action Center (and maybe others)
    if ( (!is_blocked) && (window.GetExeName() == "ShellExperienceHost.exe") )
    {
        is_blocked = true;
    }

    return is_blocked;
}

bool IsCapturableWindow(WindowInfo const& window)
{
    HWND window_handle = window.GetWindowHandle();
    if ((window.GetTitle().empty()) || (window_handle == ::GetShellWindow()) || (!::IsWindowVisible(window_handle)) || (::GetAncestor(window_handle, GA_ROOT) != window_handle))
    {
        return false;
    }

    LONG style = ::GetWindowLongW(window_handle, GWL_STYLE);
    if (style & WS_DISABLED)
    {
        return false;
    }

    LONG exStyle = ::GetWindowLongW(window_handle, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW)    // No tooltips
    {
        return false;
    }

    // Check to see if the window is cloaked if it's a UWP
    if ((wcscmp(window.GetWindowClassName().c_str(), L"Windows.UI.Core.CoreWindow") == 0) || (wcscmp(window.GetWindowClassName().c_str(), L"ApplicationFrameWindow") == 0))
    {
        DWORD cloaked = FALSE;
        if (SUCCEEDED(::DwmGetWindowAttribute(window_handle, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && (cloaked == DWM_CLOAKED_SHELL))
        {
            return false;
        }
    }

    // Unfortunate work-around. Not sure how to avoid this.
    if (IsKnownBlockedWindow(window))
    {
        return false;
    }

    return true;
}

WindowInfo::WindowInfo(HWND window_handle)
{
    m_WindowHandle = window_handle;
    m_Icon = nullptr;

    if (m_WindowHandle == nullptr)
    {
        m_Title     = L"[No Window]";
        m_ListTitle =  "[No Window]";

        return;
    }

    UpdateWindowTitle();

    WCHAR class_buffer[256];

    if (::GetClassNameW(m_WindowHandle, class_buffer, 256) != 0)
    {
        m_ClassName = class_buffer;
    }
}

HICON WindowInfo::GetIcon() const
{
    return (m_Icon == nullptr) ? m_Icon = GetIcon(m_WindowHandle) : m_Icon;
}

const std::wstring& WindowInfo::GetTitle() const
{
    return m_Title;
}

const std::wstring& WindowInfo::GetWindowClassName() const
{
    return m_ClassName;
}

const std::string& WindowInfo::GetExeName() const
{
    return (m_ExeName.empty()) ? m_ExeName = GetExeName(m_WindowHandle) : m_ExeName;
}

const std::string& WindowInfo::GetListTitle() const
{
    if (m_ListTitle.empty())
    {
        m_ListTitle = "[" + GetExeName() + "]: " + StringConvertFromUTF16(GetTitle().c_str());
    }

    return m_ListTitle;
}

bool WindowInfo::UpdateWindowTitle()
{
    bool ret = false;
    int title_length = ::GetWindowTextLengthW(m_WindowHandle);
    if (title_length > 0)
    {
        title_length++;

        auto title_buffer = std::unique_ptr<WCHAR[]>{new WCHAR[title_length]};

        if (::GetWindowTextW(m_WindowHandle, title_buffer.get(), title_length) != 0)
        {
            std::wstring title_new = title_buffer.get();

            //Some applications send a lot of title updates without actually changing the title (notepad does it on every mouse move!), so check if the title is different and pass that info along
            if (m_Title == title_new)
            {
                return false;
            }

            m_Title = title_new;
            ret = true;
        }

        //Clear list title so it gets generated again on next GetListTitle() call
        m_ListTitle = "";
    }

    return ret;
}

std::string WindowInfo::GetExeName(HWND window_handle)
{
    std::string exe_name;

    if (window_handle == nullptr)
        return exe_name;

    DWORD proc_id;
    GetWindowThreadProcessId(window_handle, &proc_id);
    HANDLE proc_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc_id);
    if (proc_handle)
    {
        WCHAR exe_buffer[MAX_PATH];
        //GetProcessImageFileNameW has more info than we need, but takes the least access permissions so we can get elevated process names through it
        if (::GetProcessImageFileNameW(proc_handle, exe_buffer, MAX_PATH) != 0)
        {
            exe_name = StringConvertFromUTF16(exe_buffer);

            //Cut off the path we're not interested in
            std::size_t pos = exe_name.find_last_of("\\");
            if ((pos != std::string::npos) && (pos + 1 < exe_name.length())) //String should be well-formed, but let's be careful
            {
                exe_name = exe_name.substr(pos + 1);
            }
        }

        CloseHandle(proc_handle);
    }

    return exe_name;
}

HWND WindowInfo::GetWindowHandle() const
{
    return m_WindowHandle;
}

HICON WindowInfo::GetIcon(HWND window_handle)
{
    HICON icon_handle = nullptr;
    icon_handle = (HICON)::SendMessage(window_handle, WM_GETICON, ICON_BIG, 0);

    if (icon_handle == nullptr)
    {
        icon_handle = (HICON)::GetClassLongPtr(window_handle, GCLP_HICON);
    }

    if (icon_handle == nullptr)
    {
        icon_handle = ::LoadIcon(nullptr, IDI_APPLICATION);
    }

    return icon_handle;
}

HWND WindowInfo::FindClosestWindowForTitle(const std::string& title_str, const std::string& class_str, const std::string& exe_str, const std::vector<WindowInfo>& window_list)
{
    //The idea is that most applications with changing titles keep their name at the end after a dash, so we separate that part if we can find it
    //Apart from that, we try to find matches by removing one space-separated chunk in each iteration (first one is 1:1 match, last just the app-name)
    //This still creates matches for apps that don't have their name after a dash but appended something to the previously known name
    //As a last resort we just match up the first window with the same executable and class name

    std::wstring title_wstr = WStringConvertFromUTF8(title_str.c_str());
    std::wstring class_wstr = WStringConvertFromUTF8(class_str.c_str());

    //Just straight look for a complete match when strict matching is enabled
    if (ConfigManager::GetValue(configid_bool_windows_winrt_window_matching_strict))
    {
        auto it = std::find_if(window_list.begin(), window_list.end(), 
                               [&](const auto& info){ return ( (info.GetWindowClassName() == class_wstr) && (info.GetExeName() == exe_str) && (info.GetTitle() == title_wstr) ); });

        return (it != window_list.end()) ? it->GetWindowHandle() : nullptr;
    }

    std::wstring title_search = title_wstr;
    std::wstring app_name;
    size_t search_pos = title_wstr.find_last_of(L" - ");

    if (search_pos != std::wstring::npos)
    {
        app_name = title_wstr.substr(search_pos - 2);
    }

    //Try to find a partial match by removing the last word from the title string and appending the application name
    while ((search_pos != 0) && (search_pos != std::wstring::npos))
    {
        search_pos--;
        search_pos = title_wstr.find_last_of(L' ', search_pos);

        if (search_pos != std::wstring::npos)
        {
            title_search = title_wstr.substr(0, search_pos) + app_name;
        }
        else if (!app_name.empty()) //Last attempt, just the app-name
        {
            title_search = app_name;
        }
        else
        {
            break;
        }

        auto it = std::find_if(window_list.begin(), window_list.end(), 
                               [&](const auto& info){ return ( (info.GetWindowClassName() == class_wstr) && (info.GetExeName() == exe_str) && (info.GetTitle().find(title_search) != std::wstring::npos) ); });

        if (it != window_list.end())
        {
            return it->GetWindowHandle();
        }
    }

    //Nothing found, try to get a window from the same class and exe name at least
    auto it = std::find_if(window_list.begin(), window_list.end(), [&](const auto& info){ return (info.GetWindowClassName() == class_wstr) && (info.GetExeName() == exe_str); });

    if (it != window_list.end())
    {
        return it->GetWindowHandle();
    }

    return nullptr; //We tried
}


#define WM_WINDOWMANAGER_UPDATE_DATA WM_APP //Sent to WindowManager thread to update the local thread data

WindowManager& WindowManager::Get()
{
    return g_WindowManager;
}

void WindowManager::UpdateConfigState()
{
    WindowManagerThreadData thread_data_new;
    thread_data_new.BlockDrag       = ((m_IsOverlayActive) && (ConfigManager::GetValue(configid_int_windows_winrt_dragging_mode) != window_dragging_none));
    thread_data_new.DoOverlayDrag   = ((m_IsOverlayActive) && (ConfigManager::GetValue(configid_int_windows_winrt_dragging_mode) == window_dragging_overlay));
    thread_data_new.KeepOnScreen    = ((m_IsOverlayActive) &&  ConfigManager::GetValue(configid_bool_windows_winrt_keep_on_screen));
    thread_data_new.TargetWindow    = m_TargetWindow;
    thread_data_new.TargetOverlayID = m_TargetOverlayID;

    if (m_IsActive)
    {
        //Create WindowManager thread if there is none
        if (m_ThreadHandle == nullptr)
        {
            WindowListInit();

            m_ThreadData = thread_data_new; //No need to lock since no other thread exists to race with
            m_ThreadHandle = ::CreateThread(nullptr, 0, WindowManagerThreadEntry, nullptr, 0, &m_ThreadID);
        }
        else if (m_ThreadData != thread_data_new) //If just data has changed, update existing thread
        {
            {
                std::lock_guard<std::mutex> lock(m_ThreadMutex);
                m_ThreadData = thread_data_new;
            }

            //Reset done flag before sending update
            {
                std::lock_guard<std::mutex> lock(m_UpdateDoneMutex);
                m_UpdateDoneFlag = false;
            }

            ::PostThreadMessage(m_ThreadID, WM_WINDOWMANAGER_UPDATE_DATA, 0, 0);

            //Wait for thread to be done with update before continuing (for 500ms in case the window message goes poof... shouldn't happen though)
            {
                std::unique_lock<std::mutex> lock(m_UpdateDoneMutex);
                m_UpdateDoneCV.wait_for(lock, std::chrono::milliseconds(500), [&]{ return m_UpdateDoneFlag; });
            }
        }
    }
    else //If thread is no longer needed, remove it
    {
        if (m_ThreadHandle != nullptr)
        {
            ::PostThreadMessage(m_ThreadID, WM_QUIT, 0, 0);

            ::CloseHandle(m_ThreadHandle);
            m_ThreadHandle = nullptr;
            m_ThreadID     = 0;
        }
    }
}

void WindowManager::SetTargetWindow(HWND window, unsigned int overlay_id)
{
    if (window != m_TargetWindow)
        m_DragOverlayMsgSent = false;

    m_TargetWindow    = window;
    m_TargetOverlayID = overlay_id;
    UpdateConfigState();
}

HWND WindowManager::GetTargetWindow() const
{
    return m_TargetWindow;
}

void WindowManager::SetActive(bool is_active)
{
    m_IsActive = is_active;
    UpdateConfigState();
}

bool WindowManager::IsActive() const
{
    return m_IsActive;
}

void WindowManager::SetOverlayActive(bool is_active)
{
    m_IsOverlayActive = is_active;
    UpdateConfigState();
}

bool WindowManager::IsOverlayActive() const
{
    return m_IsOverlayActive;
}

const WindowInfo& WindowManager::WindowListAdd(HWND window)
{
    auto it = std::find_if(m_WindowList.begin(), m_WindowList.end(), [&](const auto& info){ return (info.GetWindowHandle() == window); });

    if (it == m_WindowList.end())
    {
        m_WindowList.emplace_back(window);
        return m_WindowList.back();
    }

    return *it;
}

std::wstring WindowManager::WindowListRemove(HWND window)
{
    std::wstring last_title;

    auto it = std::find_if(m_WindowList.begin(), m_WindowList.end(), [&](const auto& info){ return (info.GetWindowHandle() == window); });

    if (it != m_WindowList.end())
    {
        last_title = it->GetTitle();
        m_WindowList.erase(it);
    }

    return last_title;
}

WindowInfo const* WindowManager::WindowListUpdateTitle(HWND window, bool* has_title_changed)
{
    auto it = std::find_if(m_WindowList.begin(), m_WindowList.end(), [&](const auto& info){ return (info.GetWindowHandle() == window); });

    if (it != m_WindowList.end())
    {
        bool title_changed = it->UpdateWindowTitle();

        if (has_title_changed != nullptr)
            *has_title_changed = title_changed;

        return &*it;
    }
    else if (IsCapturableWindow(window)) //Window not in the list, check if it's capturable and add it then. This is for windows that are not created with a title and are skipped without this
    {
        if (has_title_changed != nullptr)
            *has_title_changed = true;

        return &WindowListAdd(window);
    }

    if (has_title_changed != nullptr)
        *has_title_changed = false;

    return nullptr;
}

const std::vector<WindowInfo>& WindowManager::WindowListGet() const
{
    return m_WindowList;
}

WindowInfo const* WindowManager::WindowListFindWindow(HWND window) const
{
    auto it = std::find_if(m_WindowList.begin(), m_WindowList.end(), [&](const auto& info){ return (info.GetWindowHandle() == window); });

    if (it != m_WindowList.end())
    {
        return &*it;
    }

    return nullptr;
}

bool WindowManager::WouldDragMaximizedTitleBar(HWND window, int prev_cursor_x, int prev_cursor_y, int new_cursor_x, int new_cursor_y)
{
    //If the target window matches (simulated left mouse down), the window is maximized and the cursor position changed
    if ( (window == m_TargetWindow) && (::IsZoomed(window)) && ( (prev_cursor_x != new_cursor_x) || (prev_cursor_y != new_cursor_y) ) )
    {
        //Return true if previous cursor position was on the title bar
        return (::SendMessage(window, WM_NCHITTEST, 0, MAKELPARAM(prev_cursor_x, prev_cursor_y)) == HTCAPTION);
    }

    return false;
}

void WindowManager::RaiseAndFocusWindow(HWND window, InputSimulator* input_sim_ptr)
{
    bool focus_success = true;
    if ( ( (m_LastFocusFailedWindow != window) || (m_LastFocusFailedTick + 3000 >= ::GetTickCount64()) ) && (::GetForegroundWindow() != window) )
    {
        focus_success = (::IsIconic(window)) ? ::OpenIcon(window) /*Also focuses*/: ::SetForegroundWindow(window);
        
        if (focus_success)
        {
            m_LastFocusFailedWindow = nullptr;
        }
        else if (input_sim_ptr != nullptr)
        {
            #ifndef DPLUS_UI

            //We failed, let's try again with a hack: Pressing the ALT key before trying to switch. This releases most locks that block SetForegroundWindow()
            bool alt_was_down = (::GetAsyncKeyState(VK_MENU) < 0);

            if (alt_was_down) //Release ALT first if it's already down so it can actually be pressed again
            {
                input_sim_ptr->KeyboardSetUp(VK_MENU);
            }

            input_sim_ptr->KeyboardSetDown(VK_MENU);
            ::Sleep(100); //Allow for a little bit of time for the system to register the key press

            //Try again
            focus_success = (::IsIconic(window)) ? ::OpenIcon(window) : ::SetForegroundWindow(window);

            if (!alt_was_down) //Leave the key down if it was previously
            {
                input_sim_ptr->KeyboardSetUp(VK_MENU);
            }

            if (focus_success)
            {
                m_LastFocusFailedWindow = nullptr;
            }

            #endif
        }
    }

    if (!focus_success)
    {
        //If a focusing attempt failed we either wait until it succeeded on another window or 3 seconds in order to not spam attempts when it's blocked for some reason
        m_LastFocusFailedWindow = window;
        m_LastFocusFailedTick = ::GetTickCount64();
    }
}

void WindowManager::FocusActiveVRSceneApp(InputSimulator* input_sim_ptr)
{
    //Try finding and focusing the window of the current scene application
    uint32_t pid = vr::VRApplications()->GetCurrentSceneProcessId();

    if (pid != 0)
    {
        HWND scene_process_window = FindMainWindow(pid);

        if (scene_process_window != nullptr)
        {
            RaiseAndFocusWindow(scene_process_window, input_sim_ptr);
        }
    }
}

void WindowManager::MoveWindowIntoWorkArea(HWND window)
{
    if ( (::IsIconic(window)) || ((::IsZoomed(window))) )
        return;

    //Get position of the window and DWM frame bounds
    int offset_x = 0, offset_y = 0;
    RECT window_rect, dwm_rect;
    ::GetWindowRect(window, &window_rect);
    
    if (::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &dwm_rect, sizeof(dwm_rect)) == S_OK)
    {
        //We take the frame bounds as well since the window rect contains the window shadows, but we don't want those to to take up any space in the calculations
        offset_x = window_rect.left - dwm_rect.left;
        offset_y = window_rect.top  - dwm_rect.top;

        //Replace the window rect with the DWM one. We apply the stored offset later when setting the window position
        window_rect = dwm_rect; 
    }

    //Get the nearest monitor to the window
    HMONITOR hmon = ::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);

    //Get monitor info to access the work area rect
    MONITORINFO monitor_info = {0};
    monitor_info.cbSize = sizeof(monitor_info);
    ::GetMonitorInfo(hmon, &monitor_info);

    int width  = window_rect.right  - window_rect.left;
    int height = window_rect.bottom - window_rect.top;

    //Clamp position to the work area. This does move windows even if they'd be fully accessible across multiple screens, but whatever
    window_rect.left   = std::max(monitor_info.rcWork.left, std::min(monitor_info.rcWork.right  - width,  window_rect.left));
    window_rect.top    = std::max(monitor_info.rcWork.top,  std::min(monitor_info.rcWork.bottom - height, window_rect.top));
    window_rect.right  = window_rect.left + width;
    window_rect.bottom = window_rect.top  + height;

    ::SetWindowPos(window, nullptr,  window_rect.left + offset_x, window_rect.top + offset_y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool WindowManager::IsHoveringCapturableTitleBar(HWND window, int cursor_x, int cursor_y)
{
    if (WindowListFindWindow(window) != nullptr)
    {
        LRESULT result = ::SendMessage(window, WM_NCHITTEST, 0, MAKELPARAM(cursor_x, cursor_y));

        if (result == HTCAPTION)
        {
            return true;
        }
        else if ( (result == HTNOWHERE) || (result == HTCLIENT) ) //Fallback for windows that don't handle NCHITTEST correctly
        {
            //This is not DPI-aware at all and making it be that would require calling Windows 10 and up functions. Not worth to handle just for a small fallback.
            //It gives us a somewhat reasonable title bar region to work with either way
            int title_bar_height = (GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXPADDEDBORDER));

            //Get position of the window
            RECT window_rect = {0};

            if (::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect, sizeof(window_rect)) == S_OK)
            {
                //Check if the cursor is in the region where an ill-implemented custom drawn title bar could be
                if ( (cursor_x >= window_rect.left) && (cursor_x < window_rect.right) && (cursor_y >= window_rect.top) && (cursor_y < window_rect.top + title_bar_height) )
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void WindowManager::WindowListInit()
{
    m_WindowList.clear();

    EnumWindows([](HWND hwnd, LPARAM lParam)
                {
                    if (::GetWindowTextLengthW(hwnd) > 0)
                    {
                        WindowInfo window = WindowInfo(hwnd);

                        if (!IsCapturableWindow(window))
                        {
                            return TRUE;
                        }

                        ((std::vector<WindowInfo>*)lParam)->push_back(window);
                    }

                    return TRUE;
                },
                (LPARAM)&m_WindowList);
}

void WindowManager::HandleWinEvent(DWORD win_event, HWND hwnd, LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time)
{
    switch (win_event)
    {
        case EVENT_OBJECT_DESTROY:
        case EVENT_OBJECT_HIDE:
        case EVENT_OBJECT_CLOAKED:
        {
            if ( (id_object == OBJID_WINDOW) && (id_child == CHILDID_SELF) && (hwnd != nullptr) )
            {
                #ifdef DPLUS_UI
                    IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_winmanager_winlist_remove, (LPARAM)hwnd);
                #else
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_winmanager_winlist_remove, (LPARAM)hwnd);
                #endif
            }
            return;
        }
        case EVENT_OBJECT_SHOW:
        case EVENT_OBJECT_UNCLOAKED:
        {
            if ( (id_object == OBJID_WINDOW) && (id_child == CHILDID_SELF) && (hwnd != nullptr) && (GetAncestor(hwnd, GA_ROOT) == hwnd) && (GetWindowTextLengthW(hwnd) > 0) )
            {
                WindowInfo info(hwnd);
                
                if (IsCapturableWindow(info))
                {
                    #ifdef DPLUS_UI
                        IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_winmanager_winlist_add, (LPARAM)hwnd);
                    #else
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_winmanager_winlist_add, (LPARAM)hwnd);
                    #endif
                }
            }
            return;
        }
        case EVENT_OBJECT_NAMECHANGE:
        {
            if ( (id_object == OBJID_WINDOW) && (id_child == CHILDID_SELF) && (hwnd != nullptr) && (::GetAncestor(hwnd, GA_ROOT) == hwnd) && (::GetWindowTextLengthW(hwnd) > 0) )
            {
                //We don't bother checking if the window is capturable here. Windows that were created with an empty title may get late added from this (which then checks).
                #ifdef DPLUS_UI
                    IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_winmanager_winlist_update, (LPARAM)hwnd);
                #else
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_winmanager_winlist_update, (LPARAM)hwnd);
                #endif
            }
            return;
        }

        #ifndef DPLUS_UI

        case EVENT_OBJECT_FOCUS:
        {
            //Check if focus is from a different process than last time
            DWORD process_id;
            ::GetWindowThreadProcessId(hwnd, &process_id);
            if (process_id != m_FocusLastProcess)
            {
                m_FocusLastProcess = process_id;

                //Send updated process elevation state to UI
                IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_window_focused_process_elevated, IsProcessElevated(process_id));
            }

            //Send focus update to UI
            IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_winmanager_focus_changed);

            return;
        }
        case EVENT_SYSTEM_MOVESIZESTART:
        case EVENT_OBJECT_LOCATIONCHANGE:
        case EVENT_SYSTEM_MOVESIZEEND:
        {
            //Limit to visible top-level windows
            if ( (hwnd == nullptr) || (id_object != OBJID_WINDOW) || (id_child != CHILDID_SELF) || (GetWindowTextLength(hwnd) == 0) || 
                 (!::IsWindowVisible(hwnd)) || (::GetAncestor(hwnd, GA_ROOT) != hwnd) )
            {
                return;
            }

            if ( (m_ThreadLocalData.BlockDrag) || (m_ThreadLocalData.KeepOnScreen) )
            {
                if (win_event == EVENT_SYSTEM_MOVESIZESTART)
                {
                    if (hwnd == m_ThreadLocalData.TargetWindow)
                    {
                        m_DragWindow = hwnd;
                        m_DragOverlayMsgSent = false;
                    }
                    return;
                }
                else if (win_event == EVENT_OBJECT_LOCATIONCHANGE)
                {
                    if (hwnd == m_DragWindow)
                    {
                        if (m_ThreadLocalData.BlockDrag)
                        {
                            RECT window_rect;
                            ::GetWindowRect(hwnd, &window_rect);
                            SIZE start_size = {m_DragStartWindowRect.right - m_DragStartWindowRect.left, m_DragStartWindowRect.bottom - m_DragStartWindowRect.top};
                            SIZE current_size = {window_rect.right - window_rect.left,window_rect.bottom - window_rect.top};

                            //Only block/start overlay drag if the position changed, but not the size (allows in-place resizing)
                            if ( ((window_rect.left != m_DragStartWindowRect.left) || (window_rect.top != m_DragStartWindowRect.top)) && (start_size.cx == current_size.cx) && 
                                 (start_size.cy == current_size.cy) )
                            {
                                ::SetCursorPos(m_DragStartMousePos.x, m_DragStartMousePos.y);
                                ::SetWindowPos(hwnd, nullptr, m_DragStartWindowRect.left, m_DragStartWindowRect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

                                if (!m_DragOverlayMsgSent)
                                {
                                    //Start the overlay drag or send overlay ID 0 to have the mouse button be released (so the desktop drag stops)
                                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_winmanager_drag_start, (m_ThreadLocalData.DoOverlayDrag) ? m_ThreadLocalData.TargetOverlayID : UINT_MAX);
                                    m_DragOverlayMsgSent = true;
                                }
                            }
                            else if ((start_size.cx != current_size.cx) || (start_size.cy != current_size.cy))
                            {
                                //Adapt to size change in case this happened from a normal drag (when dragging from a maximized window for example)
                                ::GetWindowRect(hwnd, &m_DragStartWindowRect);
                            }
                        }
                        else if (m_ThreadLocalData.KeepOnScreen)
                        {
                            MoveWindowIntoWorkArea(hwnd);
                        }
                    }
                    else if (hwnd == m_ThreadLocalData.TargetWindow)
                    {
                        //Fallback for windows that do their own dragging logic.
                        //This has the chance of picking up a programmatic position change, 
                        //but the time window for that is very small since the target window is only set while the simulated mouse is down

                        //Check if the current cursor is a sizing cursor, though. There are edge cases we want to avoid if that's the case
                        bool is_sizing_cursor = false;
                        CURSORINFO cinfo;
                        cinfo.cbSize = sizeof(CURSORINFO);

                        if (::GetCursorInfo(&cinfo))
                        {
                            is_sizing_cursor = ( (cinfo.hCursor == ::LoadCursor(nullptr, IDC_SIZENESW)) ||
                                                 (cinfo.hCursor == ::LoadCursor(nullptr, IDC_SIZENS))   ||
                                                 (cinfo.hCursor == ::LoadCursor(nullptr, IDC_SIZENWSE)) ||
                                                 (cinfo.hCursor == ::LoadCursor(nullptr, IDC_SIZEWE)) );
                        }


                        if (!is_sizing_cursor)
                        {
                            m_DragWindow = hwnd;
                        }

                        m_DragOverlayMsgSent = false;
                    }
                }
                else if (win_event == EVENT_SYSTEM_MOVESIZEEND)
                {
                    if (m_DragWindow != nullptr)
                    {
                        ::SetCursorPos(m_DragStartMousePos.x, m_DragStartMousePos.y);
                        ::SetWindowPos(hwnd, nullptr, m_DragStartWindowRect.left, m_DragStartWindowRect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

                        if (m_ThreadLocalData.KeepOnScreen)
                        {
                            MoveWindowIntoWorkArea(hwnd);
                        }
                    }

                    m_DragWindow = nullptr;
                }
            }
        }

        #endif
    }
}

void WindowManager::WindowManager::WinEventProc(HWINEVENTHOOK /*event_hook_handle*/, DWORD win_event, HWND hwnd, LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time)
{
    Get().HandleWinEvent(win_event, hwnd, id_object, id_child, event_thread, event_time);
}

void WindowManager::ManageEventHooks(HWINEVENTHOOK& hook_handle_move_size, HWINEVENTHOOK& hook_handle_location_change, HWINEVENTHOOK& hook_handle_focus_change, HWINEVENTHOOK& hook_handle_destroy_show)
{
    #ifndef DPLUS_UI

    if ( (m_ThreadLocalData.BlockDrag) && (hook_handle_move_size == nullptr) )
    {
        hook_handle_move_size       = ::SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND, nullptr, WindowManager::WinEventProc, 0, 0, 
                                                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        hook_handle_location_change = ::SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, WindowManager::WinEventProc, 0, 0,
                                                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }
    else if ( (!m_ThreadLocalData.BlockDrag) && (hook_handle_move_size != nullptr) )
    {
        UnhookWinEvent(hook_handle_move_size);
        UnhookWinEvent(hook_handle_location_change);

        hook_handle_move_size       = nullptr;
        hook_handle_location_change = nullptr;
    }

    if (hook_handle_focus_change == nullptr)
    {
        //Set initial elevated process focus state beforehand
        DWORD process_id;
        ::GetWindowThreadProcessId(::GetForegroundWindow(), &process_id);
        m_FocusLastProcess = process_id;

        //Send process elevation state to UI
        IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_window_focused_process_elevated, IsProcessElevated(process_id));

        hook_handle_focus_change = ::SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, nullptr, WindowManager::WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    #endif

    if (hook_handle_destroy_show == nullptr)
    {
        hook_handle_destroy_show = ::SetWinEventHook(EVENT_OBJECT_DESTROY, /*EVENT_OBJECT_SHOW*/EVENT_OBJECT_UNCLOAKED, nullptr, WindowManager::WinEventProc, 0, 0,
                                                     WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    //Reset drag window state when target window is nullptr
    if (m_ThreadLocalData.TargetWindow == nullptr)
    {
        m_DragWindow = nullptr;
        m_DragOverlayMsgSent = false;
    }
    else //Store drag start mouse position and window rect now, since doing on actual drag start will be too late
    {
        ::GetCursorPos(&m_DragStartMousePos);
        ::GetWindowRect(m_ThreadLocalData.TargetWindow, &m_DragStartWindowRect);
    }
}

DWORD WindowManager::WindowManagerThreadEntry(void* /*param*/)
{
    //Copy thread data for lock-free reads later
    {
        WindowManager& wman = Get();
        std::lock_guard<std::mutex> lock(wman.m_ThreadMutex);

        wman.m_ThreadLocalData = wman.m_ThreadData;
    }

    //Create event hooks
    HWINEVENTHOOK hook_handle_move_size       = nullptr;
    HWINEVENTHOOK hook_handle_location_change = nullptr;
    HWINEVENTHOOK hook_handle_focus_change    = nullptr;
    HWINEVENTHOOK hook_handle_destroy_show    = nullptr;
    
    Get().ManageEventHooks(hook_handle_move_size, hook_handle_location_change, hook_handle_focus_change, hook_handle_destroy_show);

    //Wait for callbacks, update or quit message
    MSG msg;
    while (::GetMessage(&msg, 0, 0, 0))
    {
        if (msg.message == WM_WINDOWMANAGER_UPDATE_DATA)
        {
            WindowManager& wman = Get();

            //Copy new thread data
            {
                std::lock_guard<std::mutex> lock(wman.m_ThreadMutex);

                if (wman.m_ThreadData.TargetWindow == nullptr)
                {
                    //Give a potentially dragged window's process a little bit of time to realize the mouse release
                    ::Sleep(20);
                }

                //Process all pending messages/callbacks before continuing
                while (::PeekMessage(&msg, 0, 0, WM_APP, PM_REMOVE));

                wman.m_ThreadLocalData = wman.m_ThreadData;
            }

            wman.ManageEventHooks(hook_handle_move_size, hook_handle_location_change, hook_handle_focus_change, hook_handle_destroy_show);

            //Notify main thread we're done
            {
                std::lock_guard<std::mutex> lock(wman.m_UpdateDoneMutex);
                wman.m_UpdateDoneFlag = true;
            }
            wman.m_UpdateDoneCV.notify_one();
        }
    }

    ::UnhookWinEvent(hook_handle_move_size);
    ::UnhookWinEvent(hook_handle_location_change);
    ::UnhookWinEvent(hook_handle_focus_change);
    ::UnhookWinEvent(hook_handle_destroy_show);

    return 0;
}
