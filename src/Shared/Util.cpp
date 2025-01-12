#include "Util.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <shldisp.h>
#include <shlobj.h>
#include <shellapi.h>

std::string StringConvertFromUTF16(LPCWSTR str)
{
    std::string stdstr;
    int length_utf8 = ::WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);

    if (length_utf8 != 0)
    {
        auto str_utf8 = std::unique_ptr<char[]>{new char[length_utf8]};

        if (::WideCharToMultiByte(CP_UTF8, 0, str, -1, str_utf8.get(), length_utf8, nullptr, nullptr) != 0)
        {
            stdstr = str_utf8.get();
        }
    }

    return stdstr;
}

std::wstring WStringConvertFromUTF8(const char * str)
{
    std::wstring wstr;
    int length_utf16 = ::MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);

    if (length_utf16 != 0)
    {
        auto str_utf16 = std::unique_ptr<WCHAR[]>{new WCHAR[length_utf16]};

        if (::MultiByteToWideChar(CP_UTF8, 0, str, -1, str_utf16.get(), length_utf16) != 0)
        {
            wstr = str_utf16.get();
        }
    }

    return wstr;
}

//This is only needed for std::error_code.message(), thanks to it being in the local ANSI codepage instead of UTF-8
std::wstring WStringConvertFromLocalEncoding(const char* str)
{
    std::wstring wstr;
    int length_utf16 = ::MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0);

    if (length_utf16 != 0)
    {
        auto str_utf16 = std::unique_ptr<WCHAR[]>{new WCHAR[length_utf16]};

        if (::MultiByteToWideChar(CP_ACP, 0, str, -1, str_utf16.get(), length_utf16) != 0)
        {
            wstr = str_utf16.get();
        }
    }

    return wstr;
}

DEVMODE GetDevmodeForDisplayID(int display_id, bool wmr_ignore_vscreens, HMONITOR* hmon)
{
    if (display_id == -1)
        display_id = 0;

    DEVMODE mode = {0};
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory_ptr;

    //This needs to go through DXGI as EnumDisplayDevices()'s order can be different
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory_ptr);
    if (!FAILED(hr))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr;
        UINT i = 0;
        int output_count = 0;

        while (factory_ptr->EnumAdapters(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND)
        {
            //Check if this a WMR virtual display adapter and skip it when the option is enabled
            if (wmr_ignore_vscreens)
            {
                DXGI_ADAPTER_DESC adapter_desc;
                adapter_ptr->GetDesc(&adapter_desc);

                if (wcscmp(adapter_desc.Description, L"Virtual Display Adapter") == 0)
                {
                    ++i;
                    continue;
                }
            }

            //Enum the available outputs
            Microsoft::WRL::ComPtr<IDXGIOutput> output_ptr;
            UINT output_index = 0;
            while (adapter_ptr->EnumOutputs(output_index, &output_ptr) != DXGI_ERROR_NOT_FOUND)
            {
                //Check if this happens to be the output we're looking for
                if (display_id == output_count)
                {
                    //Get devmode
                    DXGI_OUTPUT_DESC output_desc;
                    output_ptr->GetDesc(&output_desc);

                    mode.dmSize = sizeof(DEVMODE);

                    if (EnumDisplaySettings(output_desc.DeviceName, ENUM_CURRENT_SETTINGS, &mode) != FALSE)
                    {
                        //Set hmon if requested
                        if (hmon != nullptr)
                        {
                            *hmon = output_desc.Monitor;
                        }

                        //Get out early
                        return mode;
                    }

                    mode.dmSize = 0;    //Reset dmSize to 0 if the call failed
                }

                ++output_index;
                ++output_count;
            }

            ++i;
        }
    }

    //Set hmon to nullptr
    if (hmon != nullptr)
    {
        *hmon = nullptr;
    }

    return mode;
}

int GetDisplayIDFromHMonitor(HMONITOR monitor_handle, bool wmr_ignore_vscreens)
{
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory_ptr;

    //We want the display/desktop ID as enumerated by DXGI
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory_ptr);
    if (!FAILED(hr))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr;
        UINT i = 0;
        int output_count = 0;

        while (factory_ptr->EnumAdapters(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND)
        {
            //Check if this a WMR virtual display adapter and skip it when the option is enabled
            if (wmr_ignore_vscreens)
            {
                DXGI_ADAPTER_DESC adapter_desc;
                adapter_ptr->GetDesc(&adapter_desc);

                if (wcscmp(adapter_desc.Description, L"Virtual Display Adapter") == 0)
                {
                    ++i;
                    continue;
                }
            }

            //Enum the available outputs
            Microsoft::WRL::ComPtr<IDXGIOutput> output_ptr;
            UINT output_index = 0;
            while (adapter_ptr->EnumOutputs(output_index, &output_ptr) != DXGI_ERROR_NOT_FOUND)
            {
                //Get hmonitor and compare
                DXGI_OUTPUT_DESC output_desc;
                output_ptr->GetDesc(&output_desc);

                if (output_desc.Monitor == monitor_handle)
                {
                    return output_count;
                }

                ++output_index;
                ++output_count;
            }

            ++i;
        }
    }

    return -1;
}

int GetMonitorRefreshRate(int display_id, bool wmr_ignore_vscreens)
{
    DEVMODE mode = GetDevmodeForDisplayID(display_id, wmr_ignore_vscreens);

    if ( (mode.dmSize != 0) && (mode.dmFields & DM_DISPLAYFREQUENCY) ) //Something would be wrong if that field isn't supported, but let's check anyways
    {
        return mode.dmDisplayFrequency;
    }

    return 60;	//Fallback value
}

void CenterRectToMonitor(LPRECT prc)
{
    HMONITOR    hmonitor;
    MONITORINFO mi;
    RECT        rc;
    int         w = prc->right  - prc->left;
    int         h = prc->bottom - prc->top;

    //Get the nearest monitor to the passed rect
    hmonitor = ::MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST);

    //Get monitor rect
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfo(hmonitor, &mi);

    rc = mi.rcMonitor;

    //Center the passed rect to the monitor rect 
    prc->left   = rc.left + (rc.right  - rc.left - w) / 2;
    prc->top    = rc.top  + (rc.bottom - rc.top  - h) / 2;
    prc->right  = prc->left + w;
    prc->bottom = prc->top  + h;
}

void CenterWindowToMonitor(HWND hwnd, bool use_cursor_pos)
{
    RECT rc;
    ::GetWindowRect(hwnd, &rc);

    HMONITOR    hmonitor;
    MONITORINFO mi;
    RECT rcm;
    int w = rc.right  - rc.left;
    int h = rc.bottom - rc.top;

    if (use_cursor_pos) //Cursor position is used to determine the screen to center on
    {
        POINT mouse_pos = {0};
        ::GetCursorPos(&mouse_pos); 
        RECT mouse_rc;
        mouse_rc.left   = mouse_pos.x;
        mouse_rc.right  = mouse_pos.x;
        mouse_rc.top    = mouse_pos.y;
        mouse_rc.bottom = mouse_pos.y;

        hmonitor = ::MonitorFromRect(&mouse_rc, MONITOR_DEFAULTTONEAREST);
    }
    else
    {
        //Get the nearest monitor to the passed rect
        hmonitor = ::MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    }

    //Get monitor rect
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfo(hmonitor, &mi);

    rcm = mi.rcMonitor;

    //Center the passed rect to the monitor rect 
    rc.left   = rcm.left + (rcm.right  - rcm.left - w) / 2;
    rc.top    = rcm.top  + (rcm.bottom - rcm.top  - h) / 2;
    rc.right  = rc.left + w;
    rc.bottom = rc.top  + h;

    ::SetWindowPos(hwnd, nullptr, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void ForceScreenRefresh()
{
    //This is a hacky workaround for occasionally not getting a full desktop image after resetting duplication until a screen change occurs
    //For secondary screens that could possibly not happen until manual user interaction, so instead we force the desktop to redraw itself
    //Unproblematic, but proper fix would be welcome too
    if (HWND shell_window = ::GetShellWindow())
        ::SendMessage(shell_window, WM_SETTINGCHANGE, 0, 0); 
}

bool IsProcessElevated() 
{
    TOKEN_ELEVATION elevation;
    DWORD cb_size = sizeof(TOKEN_ELEVATION);

    if (::GetTokenInformation(::GetCurrentProcessToken(), TokenElevation, &elevation, sizeof(elevation), &cb_size) ) 
    {
        return elevation.TokenIsElevated;
    }

    return false;
}

bool IsProcessElevated(DWORD process_id) 
{
    bool ret = false;
    HANDLE handle_window_process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (handle_window_process != nullptr)
    {
        HANDLE handle_token = nullptr;
        if (::OpenProcessToken(handle_window_process, TOKEN_QUERY, &handle_token))
        {
            TOKEN_ELEVATION elevation;
            DWORD cb_size = sizeof(TOKEN_ELEVATION);

            if (::GetTokenInformation(handle_token, TokenElevation, &elevation, sizeof(elevation), &cb_size))
            {
                ret = elevation.TokenIsElevated;
            }
        }

        if (handle_token) 
        {
            CloseHandle(handle_token);
        }

        CloseHandle(handle_window_process);
    }

    return ret;
}

bool ShellExecuteUnelevated(LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, LPCWSTR lpOperation, INT nShowCmd)
{
    //This function will fail if explorer.exe is not running, but it could be argued that this scenario is not exactly the sanest for a desktop viewing application
    //Elevated mode should be avoided in the first place to be fair

    //Use desktop automation to get the active shell view for the desktop
    Microsoft::WRL::ComPtr<IShellView> shell_view;
    Microsoft::WRL::ComPtr<IShellWindows> shell_windows;
    HRESULT hr = ::CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER, IID_IShellWindows, &shell_windows);

    if (SUCCEEDED(hr))
    {
        HWND hwnd = nullptr;
        Microsoft::WRL::ComPtr<IDispatch> dispatch;
        VARIANT v_empty = {};

        if (shell_windows->FindWindowSW(&v_empty, &v_empty, SWC_DESKTOP, (long*)&hwnd, SWFO_NEEDDISPATCH, &dispatch) == S_OK)
        {
            Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
            hr = dispatch.As(&service_provider);

            if (SUCCEEDED(hr))
            {
                Microsoft::WRL::ComPtr<IShellBrowser> shell_browser;
                hr = service_provider->QueryService(SID_STopLevelBrowser, __uuidof(IShellBrowser), (void**)&shell_browser);

                if (SUCCEEDED(hr))
                {
                    hr = shell_browser->QueryActiveShellView(&shell_view);
                }
            }
        }
        else
        {
            hr = E_FAIL;
        }
    }

    if (FAILED(hr))
        return false;

    //Use the shell view to get the shell dispatch interface
    Microsoft::WRL::ComPtr<IShellDispatch2> shell_dispatch2;
    Microsoft::WRL::ComPtr<IDispatch> dispatch_background;
    hr = shell_view->GetItemObject(SVGIO_BACKGROUND, __uuidof(IDispatch), (void**)&dispatch_background);
    if (SUCCEEDED(hr))
    {
        Microsoft::WRL::ComPtr<IShellFolderViewDual> shell_folderview_dual;
        hr = dispatch_background.As(&shell_folderview_dual);

        if (SUCCEEDED(hr))
        {
            Microsoft::WRL::ComPtr<IDispatch> dispatch;
            hr = shell_folderview_dual->get_Application(&dispatch);

            if (SUCCEEDED(hr))
            {
                hr = dispatch.As(&shell_dispatch2);
            }
        }
    }

    if (FAILED(hr))
        return false;

    //Use the shell dispatch interface to call ShellExecuteW() in the explorer process, which is running unelevated in most cases
    BSTR bstr_file = ::SysAllocString(lpFile);
    hr = (bstr_file != nullptr) ? S_OK : E_OUTOFMEMORY;

    if (SUCCEEDED(hr))
    {
        VARIANT v_args = {};
        VARIANT v_dir = {};
        VARIANT v_operation = {};
        VARIANT v_show = {};
        v_show.vt = VT_I4;
        v_show.intVal = nShowCmd;

        //Optional parameters (SysAllocString() returns nullptr on nullptr input)
        v_args.bstrVal      = ::SysAllocString(lpParameters);
        v_dir.bstrVal       = ::SysAllocString(lpDirectory);
        v_operation.bstrVal = ::SysAllocString(lpOperation);
        v_args.vt       = (v_args.bstrVal != nullptr)      ? VT_BSTR : VT_EMPTY;
        v_dir.vt        = (v_dir.bstrVal != nullptr)       ? VT_BSTR : VT_EMPTY;
        v_operation.vt  = (v_operation.bstrVal != nullptr) ? VT_BSTR : VT_EMPTY;

        hr = shell_dispatch2->ShellExecuteW(bstr_file, v_args, v_dir, v_operation, v_show);

        ::SysFreeString(bstr_file);
        ::SysFreeString(v_args.bstrVal);
        ::SysFreeString(v_dir.bstrVal);
        ::SysFreeString(v_operation.bstrVal);

        return true;
    }

    return false;
}

bool FileExists(LPCTSTR path)
{
    DWORD attrib = GetFileAttributes(path);

    return ((attrib != INVALID_FILE_ATTRIBUTES) && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool DirectoryExists(LPCTSTR path)
{
    DWORD attrib = GetFileAttributes(path);

    return ((attrib != INVALID_FILE_ATTRIBUTES) && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

void StopProcessByWindowClass(LPCTSTR class_name)
{
    //Try to close it gracefully first so it can save the config
    if (HWND window_handle = ::FindWindow(class_name, nullptr))
    {
        ::PostMessage(window_handle, WM_QUIT, 0, 0);
    }

    ULONGLONG start_tick = ::GetTickCount64();

    while ( (::FindWindow(class_name, nullptr) != nullptr) && (::GetTickCount64() - start_tick < 3000) ) //Wait 3 seconds max
    {
        Sleep(5); //Should be usually quick though, so don't wait around too long
    }

    //Still running? Time to kill it
    if (HWND window_handle = ::FindWindow(class_name, nullptr))
    {
        DWORD pid;
        ::GetWindowThreadProcessId(window_handle, &pid);

        HANDLE phandle;
        phandle = ::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, TRUE, pid);

        if (phandle != nullptr)
        {
            ::TerminateProcess(phandle, 0);
            ::WaitForSingleObject(phandle, INFINITE);
            ::CloseHandle(phandle);
        }
    }
}

HWND FindMainWindow(DWORD pid)
{
    std::pair<HWND, DWORD> params = {nullptr, pid};

    //Enumerate the windows using a lambda to process each window
    BOOL bResult = ::EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL 
                                 {
                                     auto pParams = (std::pair<HWND, DWORD>*)(lParam);

                                     DWORD processId;
                                     if ( (::GetWindowThreadProcessId(hwnd, &processId)) && (processId == pParams->second) )
                                     {
                                         //If it's an unowned top-level window and visible, it's assumed to be the main window
                                         //Take the first match in the process, should be good enough for our use-case
                                         if ( (::GetWindow(hwnd, GW_OWNER) == (HWND)0) && (::IsWindowVisible(hwnd)) )
                                         {
                                             //Stop enumerating
                                             pParams->first = hwnd;
                                             return FALSE;
                                         }
                                     }

                                      //Continue enumerating
                                      return TRUE;
                                  },
                                  (LPARAM)&params);

    if ((!bResult) && (params.first != nullptr))
    {
        return params.first;
    }

    return 0;
}

unsigned int GetKeyboardModifierState()
{
    unsigned int modifiers = 0;

    if (::GetAsyncKeyState(VK_SHIFT) < 0)
        modifiers |= MOD_SHIFT;
    if (::GetAsyncKeyState(VK_CONTROL) < 0)
        modifiers |= MOD_CONTROL;
    if (::GetAsyncKeyState(VK_MENU) < 0)
        modifiers |= MOD_ALT;
    if ((::GetAsyncKeyState(VK_LWIN) < 0) || (::GetAsyncKeyState(VK_RWIN) < 0))
        modifiers |= MOD_WIN;

    return modifiers;
}

void StringReplaceAll(std::string& source, const std::string& from, const std::string& to)
{
    std::string new_string;
    new_string.reserve(source.length());

    std::string::size_type last_pos = 0;
    std::string::size_type find_pos;

    while ((find_pos = source.find(from, last_pos)) != std::string::npos)
    {
        new_string.append(source, last_pos, find_pos - last_pos);
        new_string += to;
        last_pos = find_pos + from.length();
    }

    //Append the remaining string
    new_string.append(source, last_pos, source.length() - last_pos);

    source.swap(new_string);
}

void WStringReplaceAll(std::wstring& source, const std::wstring& from, const std::wstring& to)
{
    std::wstring new_string;
    new_string.reserve(source.length());

    std::wstring::size_type last_pos = 0;
    std::wstring::size_type find_pos;

    while ((find_pos = source.find(from, last_pos)) != std::wstring::npos)
    {
        new_string.append(source, last_pos, find_pos - last_pos);
        new_string += to;
        last_pos = find_pos + from.length();
    }

    //Append the remaining string
    new_string.append(source, last_pos, source.length() - last_pos);

    source.swap(new_string);
}

bool IsWCharInvalidForFileName(wchar_t wchar)
{
    if ( (wchar < 32) || ( (wchar < 256) && (strchr("<>:\"/\\|?*", (char)wchar) != nullptr) ) )
        return true;

    return false;
}

void SanitizeFileNameWString(std::wstring& str)
{
    //This doesn't care about reserved names, but neither do we where this is used
    str.erase( std::remove_if(str.begin(), str.end(), IsWCharInvalidForFileName), str.end() );
}

bool WStringCompareNatural(std::wstring& str1, std::wstring& str2)
{
    return (::CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS, 
                              str1.c_str(), (int)str1.size(), str2.c_str(), (int)str2.size(), nullptr, nullptr, 0) == CSTR_LESS_THAN);
}

//This ain't pretty, but GetKeyNameText() works with scancodes, which are not exactly the same and the output strings aren't that nice either (and always localized)
//Should this be translatable?
const char* g_VK_name[256] = 
{
    "[None]",
    "Left Mouse",
    "Right Mouse",
    "Control Break",
    "Middle Mouse",
    "X1 Mouse",
    "X2 Mouse",
    "[Undefined] (7)",
    "Backspace",
    "Tab",
    "[Reserved] (10)",
    "[Reserved] (11)",
    "Clear",
    "Enter",
    "[Undefined] (14)",
    "[Undefined] (15)",
    "Shift",
    "Ctrl",
    "Alt",
    "Pause",
    "Caps Lock",
    "IME Kana",
    "[Undefined] (22)",
    "IME Junja",
    "IME Final",
    "IME Kanji",
    "[Undefined] (26)",
    "Esc",
    "IME Convert",
    "IME Non Convert",
    "IME Accept",
    "IME Mode Change",
    "Space",
    "Page Up",
    "Page Down",
    "End",
    "Home",
    "Left Arrow",
    "Up Arrow",
    "Right Arrow",
    "Down Arrow",
    "Select",
    "Print",
    "Execute",
    "Print-Screen",
    "Insert",
    "Delete",
    "Help",
    "0",  //0x30 - 0x5A are ASCII equivalent, but we want iterate this array directly for listing too
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "[Undefined] (58)",
    "[Undefined] (59)",
    "[Undefined] (60)",
    "[Undefined] (61)",
    "[Undefined] (62)",
    "[Undefined] (63)",
    "[Undefined] (64)",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "Left Windows",
    "Right Windows",
    "Context Menu",
    "[Reserved] (94)",
    "Sleep",
    "Numpad 0",
    "Numpad 1",
    "Numpad 2",
    "Numpad 3",
    "Numpad 4",
    "Numpad 5",
    "Numpad 6",
    "Numpad 7",
    "Numpad 8",
    "Numpad 9",
    "Numpad Multiply",
    "Numpad Add",
    "Separator",
    "Numpad Subtract",
    "Numpad Decimal",
    "Numpad Divide",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "F13",
    "F14",
    "F15",
    "F16",
    "F17",
    "F18",
    "F19",
    "F20",
    "F21",
    "F22",
    "F23",
    "F24",
    "[Unassigned] (136)",
    "[Unassigned] (137)",
    "[Unassigned] (138)",
    "[Unassigned] (139)",
    "[Unassigned] (140)",
    "[Unassigned] (141)",
    "[Unassigned] (142)",
    "[Unassigned] (143)",
    "Num Lock",
    "Scroll Lock",
    "OEM 1",
    "OEM 2",
    "OEM 3",
    "OEM 4",
    "OEM 5",
    "[Unassigned] (151)",
    "[Unassigned] (152)",
    "[Unassigned] (153)",
    "[Unassigned] (154)",
    "[Unassigned] (155)",
    "[Unassigned] (156)",
    "[Unassigned] (157)",
    "[Unassigned] (158)",
    "[Unassigned] (159)",
    "Left Shift",
    "Right Shift",
    "Left Ctrl",
    "Right Ctrl",
    "Left Alt",
    "Right Alt",
    "Browser Back",
    "Browser Forward",
    "Browser Refresh",
    "Browser Stop",
    "Browser Search",
    "Browser Favorites",
    "Browser Home",
    "Volume Mute",
    "Volume Down",
    "Volume Up",
    "Media Next",
    "Media Previous",
    "Media Stop",
    "Media Play/Pause",
    "Launch Mail",
    "Select Media",
    "Launch Application 1",
    "Launch Application 2",
    "[Reserved] (184)",
    "[Reserved] (185)",
    "[Layout-Specific 1] (186)",
    "+",
    ",",
    "-",
    ".",
    "[Layout-Specific 2] (191)",
    "[Layout-Specific 3] (192)",
    "[Reserved] (193)",
    "[Reserved] (194)",
    "[Reserved] (195)",
    "[Reserved] (196)",
    "[Reserved] (197)",
    "[Reserved] (198)",
    "[Reserved] (199)",
    "[Reserved] (200)",
    "[Reserved] (201)",
    "[Reserved] (202)",
    "[Reserved] (203)",
    "[Reserved] (204)",
    "[Reserved] (205)",
    "[Reserved] (206)",
    "[Reserved] (207)",
    "[Reserved] (208)",
    "[Reserved] (209)",
    "[Reserved] (210)",
    "[Reserved] (211)",
    "[Reserved] (212)",
    "[Reserved] (213)",
    "[Reserved] (214)",
    "[Reserved] (215)",
    "[Unassigned] (216)",
    "[Unassigned] (217)",
    "[Unassigned] (218)",
    "[Layout-Specific 4] (219)",
    "[Layout-Specific 5] (220)",
    "[Layout-Specific 6] (221)",
    "[Layout-Specific 7] (222)",
    "[Layout-Specific 8] (223)",
    "[Reserved] (224)",
    "[Reserved] (225)",
    "[Layout-Specific 102] (226)", //Big jump, but that's VK_OEM_102, so dunno
    "OEM 6",
    "OEM 7",
    "IME Process",
    "OEM 8",
    "Unicode Packet",
    "[Unassigned] (232)",
    "OEM 9",
    "OEM 10",
    "OEM 11",
    "OEM 12",
    "OEM 13",
    "OEM 14",
    "OEM 15",
    "OEM 16",
    "OEM 17",
    "OEM 18",
    "OEM 19",
    "OEM 20",
    "OEM 21",
    "Attn",
    "CrSel",
    "ExSel",
    "Erase EOF",
    "Play",
    "Zoom",
    "NoName",
    "PA1",
    "OEM Clear",
    "[Unassigned] (255)",
};

//Attempt at making a list of indicies to sort the key codes in a way an end-user would make expect them in, leaving the obscure stuff at the end.
const unsigned char g_VK_name_order_list[256] = 
{ 0, 1, 2, 4, 5, 6, 27, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 44, 145, 19, 8, 9, 13, 20, 16, 17, 18, 160, 161, 162, 163, 164, 165, 
91, 92, 93, 32, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 
89, 90, 187, 189, 190, 188, 45, 46, 36, 35, 33, 34, 37, 38, 39, 40, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 144, 107, 109, 106, 111, 110, 167, 166,
168, 169, 170, 171, 172, 173, 175, 174, 176, 177, 179, 178, 180, 181, 182, 183, 186, 191, 192, 219, 220, 221, 222, 223, 226, 146, 147, 148, 149, 150, 227,
228, 230, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 21, 23, 25, 28, 29,
30, 31, 229, 3, 95, 12, 41, 42, 43, 47, 108, 246, 247, 248, 249, 250, 251, 252, 253, 254, 231, 7, 14, 15, 22, 26, 58, 59, 60, 61, 62, 63, 64, 24, 136, 137,
138, 139, 140, 141, 142, 143, 151, 152, 153, 154, 155, 156, 157, 158, 159, 216, 217, 218, 232, 255, 10, 11, 94, 184, 185, 193, 194, 195, 196, 197, 198, 199,
200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 224, 225 };

const char* GetStringForKeyCode(unsigned char keycode)
{
    return g_VK_name[keycode];
}

unsigned char GetKeyCodeForListID(unsigned char list_id)
{
    return g_VK_name_order_list[list_id];
}