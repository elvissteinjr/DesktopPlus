#include "WindowList.h"

#include <dwmapi.h>
#include <Psapi.h>

#include "Util.h"

bool inline MatchTitleAndClassName(WindowInfo const& window, std::wstring const& title, std::wstring const& className)
{
    return ( (window.Title == title) && (window.ClassName == className) );
}

bool IsKnownBlockedWindow(WindowInfo const& window)
{
    bool is_blocked = // Task View
                      MatchTitleAndClassName(window, L"Task View", L"Windows.UI.Core.CoreWindow") ||
                      // XAML Islands
                      MatchTitleAndClassName(window, L"DesktopWindowXamlSource", L"Windows.UI.Core.CoreWindow") ||
                      // XAML Popups
                      MatchTitleAndClassName(window, L"PopupHost", L"Xaml_WindowedPopupClass");

    // Capture Picker Window (child of ApplicationFrameWindow)
    if ( (!is_blocked) && (window.ClassName == L"ApplicationFrameWindow") )
    {
        is_blocked = (::FindWindowExW(window.WindowHandle, nullptr, L"Windows.UI.Core.CoreWindow", L"CapturePicker") != nullptr);
    }

    return is_blocked;    
}

bool IsCapturableWindow(WindowInfo const& window)
{
    if ((window.Title.empty()) || (window.WindowHandle == GetShellWindow()) || (!IsWindowVisible(window.WindowHandle)) || (GetAncestor(window.WindowHandle, GA_ROOT) != window.WindowHandle))
    {
        return false;
    }

    LONG style = ::GetWindowLongW(window.WindowHandle, GWL_STYLE);
    if (style & WS_DISABLED)
    {
        return false;
    }

    LONG exStyle = ::GetWindowLongW(window.WindowHandle, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW)    // No tooltips
    {
        return false;
    }

    // Check to see if the window is cloaked if it's a UWP
    if ((wcscmp(window.ClassName.c_str(), L"Windows.UI.Core.CoreWindow") == 0) || (wcscmp(window.ClassName.c_str(), L"ApplicationFrameWindow") == 0))
    {
        DWORD cloaked = FALSE;
        if (SUCCEEDED(DwmGetWindowAttribute(window.WindowHandle, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && (cloaked == DWM_CLOAKED_SHELL))
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
    WindowHandle = window_handle;

    if (WindowHandle == nullptr)
    {
        Title     = L"[No Window]";
        ListTitle =  "[No Window]";

        return;
    }

    auto title_length = ::GetWindowTextLengthW(WindowHandle);
    if (title_length > 0)
    {
        title_length++;

        WCHAR* title_buffer = new WCHAR[title_length];

        if (::GetWindowTextW(WindowHandle, title_buffer, title_length) != 0)
        {
            Title = title_buffer;
        }

        delete[] title_buffer;
    }

    WCHAR class_buffer[256];

    if (::GetClassNameW(WindowHandle, class_buffer, 256) != 0)
    {
        ClassName = class_buffer;
    }
}

std::vector<WindowInfo> WindowInfo::CreateCapturableWindowList()
{
    std::vector<WindowInfo> window_list;

    EnumWindows([](HWND hwnd, LPARAM lParam)
                {
                    if (GetWindowTextLengthW(hwnd) > 0)
                    {
                        WindowInfo window = WindowInfo(hwnd);

                        if (!IsCapturableWindow(window))
                        {
                            return TRUE;
                        }

                        //Since it's capturable, create title for window listing as UTF8, and get the executable name too
                        window.ExeName = WindowInfo::GetExeName(hwnd);
                        window.ListTitle = "[" + window.ExeName + "]: " + StringConvertFromUTF16(window.Title.c_str());

                        ((std::vector<WindowInfo>*)lParam)->push_back(window);
                    }

                    return TRUE;
                },
                (LPARAM)&window_list);

    return window_list;
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

HWND WindowInfo::FindClosestWindowForTitle(const std::string title_str, const std::string exe_str)
{
    //The idea is that most applications with changing titles keep their name at the end after a dash, so we separate that part if we can find it
    //Apart from that, we try to find matches by removing one space-separated chunk in each iteration (first one is 1:1 match, last just the app-name)
    //This still creates matches for apps that don't have their name after a dash but appended something to the previously known name
    //As a last resort we just match up the first window with the same executable name

    std::vector<WindowInfo> window_list = CreateCapturableWindowList();

    std::wstring title_wstr = WStringConvertFromUTF8(title_str.c_str());
    std::wstring title_search = title_wstr;
    std::wstring app_name;
    size_t search_pos = title_wstr.find_last_of(L" - ");

    if (search_pos != std::wstring::npos)
    {
        app_name = title_wstr.substr(search_pos - 2);
    }

    //Try to find a partial match by removing the last word from the title string and appending the application name
    for (;;)
    {
        if (search_pos == 0)
            break;

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

        auto it = std::find_if(window_list.begin(), window_list.end(), [&](const auto& info){ return ( (info.ExeName == exe_str) && (info.Title.find(title_search) != std::wstring::npos) ); });

        if (it != window_list.end())
        {
            return it->WindowHandle;
        }

        if (search_pos == std::wstring::npos)
            break;
    }

    //Nothing found, try to get a window from the same exe name at least
    auto it = std::find_if(window_list.begin(), window_list.end(), [&](const auto& info){ return (info.ExeName == exe_str); });

    if (it != window_list.end())
    {
        return it->WindowHandle;
    }

    return nullptr; //We tried
}
