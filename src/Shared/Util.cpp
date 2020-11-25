#include "Util.h"

#include <d3d11.h>
#include <wrl/client.h>

std::string StringConvertFromUTF16(LPCWSTR str)
{
	std::string stdstr;
	int length_utf8 = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);

	if (length_utf8 != 0)
	{
		char* str_utf8 = new char[length_utf8];
		
		if (WideCharToMultiByte(CP_UTF8, 0, str, -1, str_utf8, length_utf8, nullptr, nullptr) != 0)
		{
			stdstr = str_utf8;
		}

		delete[] str_utf8;
	}
		
	return stdstr;
}

std::wstring WStringConvertFromUTF8(const char * str)
{
	std::wstring wstr;
	int length_utf16 = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);

	if (length_utf16 != 0)
	{
		WCHAR* str_utf16 = new WCHAR[length_utf16];

		if (MultiByteToWideChar(CP_UTF8, 0, str, -1, str_utf16, length_utf16) != 0)
		{
			wstr = str_utf16;
		}

		delete[] str_utf16;
	}

	return wstr;
}

//This is only needed for std::error_code.message(), thanks to it being in the local ANSI codepage instead of UTF-8
std::wstring WStringConvertFromLocalEncoding(const char* str)
{
    std::wstring wstr;
    int length_utf16 = MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0);

    if (length_utf16 != 0)
    {
        WCHAR* str_utf16 = new WCHAR[length_utf16];

        if (MultiByteToWideChar(CP_ACP, 0, str, -1, str_utf16, length_utf16) != 0)
        {
            wstr = str_utf16;
        }

        delete[] str_utf16;
    }

    return wstr;
}

void OffsetTransformFromSelf(vr::HmdMatrix34_t& matrix, float offset_right, float offset_up, float offset_forward)
{
	matrix.m[0][3] += offset_right * matrix.m[0][0];
	matrix.m[1][3] += offset_right * matrix.m[1][0];
	matrix.m[2][3] += offset_right * matrix.m[2][0];

	matrix.m[0][3] += offset_up * matrix.m[0][1];
	matrix.m[1][3] += offset_up * matrix.m[1][1];
	matrix.m[2][3] += offset_up * matrix.m[2][1];

	matrix.m[0][3] += offset_forward * matrix.m[0][2];
	matrix.m[1][3] += offset_forward * matrix.m[1][2];
	matrix.m[2][3] += offset_forward * matrix.m[2][2];
}

void OffsetTransformFromSelf(Matrix4& matrix, float offset_right, float offset_up, float offset_forward)
{
    matrix[12] += offset_right * matrix[0];
    matrix[13] += offset_right * matrix[1];
    matrix[14] += offset_right * matrix[2];

    matrix[12] += offset_up * matrix[4];
    matrix[13] += offset_up * matrix[5];
    matrix[14] += offset_up * matrix[6];

    matrix[12] += offset_forward * matrix[8];
    matrix[13] += offset_forward * matrix[9];
    matrix[14] += offset_forward * matrix[10];
}

void TransformLookAt(Matrix4& matrix, const Vector3 pos_target, const Vector3 up)
{
    const Vector3 pos(matrix.getTranslation());

    Vector3 z_axis = pos_target - pos;
    z_axis.normalize();
    Vector3 x_axis = up.cross(z_axis);
    x_axis.normalize();
    Vector3 y_axis = z_axis.cross(x_axis);

    matrix = { x_axis.x, x_axis.y, x_axis.z, 0.0f,
               y_axis.x, y_axis.y, y_axis.z, 0.0f,
               z_axis.x, z_axis.y, z_axis.z, 0.0f,
               pos.x,    pos.y,    pos.z,    1.0f };
}

vr::TrackedDeviceIndex_t GetFirstVRTracker()
{
    //Get the first generic tracker
    for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
        if (vr::VRSystem()->GetTrackedDeviceClass(i) == vr::TrackedDeviceClass_GenericTracker)
        {
            return i;
        }
    }

    return vr::k_unTrackedDeviceIndexInvalid;
}

Matrix4 GetControllerTipMatrix(bool right_hand)
{
    char buffer[vr::k_unMaxPropertyStringSize];
    vr::VRInputValueHandle_t input_value = vr::k_ulInvalidInputValueHandle;

    if (right_hand)
    {
        vr::VRSystem()->GetStringTrackedDeviceProperty(vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand), 
                                                       vr::Prop_RenderModelName_String, buffer, vr::k_unMaxPropertyStringSize);
        vr::VRInput()->GetInputSourceHandle("/user/hand/right", &input_value);
    }
    else
    {
        vr::VRSystem()->GetStringTrackedDeviceProperty(vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand), 
                                                       vr::Prop_RenderModelName_String, buffer, vr::k_unMaxPropertyStringSize);
        vr::VRInput()->GetInputSourceHandle("/user/hand/left", &input_value);
    }

    vr::RenderModel_ControllerMode_State_t controller_state = {0};
    vr::RenderModel_ComponentState_t component_state = {0};

    if (vr::VRRenderModels()->GetComponentStateForDevicePath(buffer, vr::k_pch_Controller_Component_Tip, input_value, &controller_state, &component_state))
    {
        return component_state.mTrackingToComponentLocal;
    }

    return Matrix4();
}

void SetConfigForWMR(int& wmr_ignore_vscreens)
{
    //Check if system is WMR and set WMR-specific default values if needed
    char buffer[vr::k_unMaxPropertyStringSize];
    vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, buffer, vr::k_unMaxPropertyStringSize);

    bool is_wmr_system = (strcmp(buffer, "holographic") == 0);

    if (is_wmr_system) //Is WMR, enable settings by default
    {
        if (wmr_ignore_vscreens == -1)
        {
            wmr_ignore_vscreens = 1;
        }        
    }
    else //Not a WMR system, set values to -1. -1 settings will not be save to disk so a WMR user's settings is preserved if they switch around HMDs, but the setting is still false
    {
        wmr_ignore_vscreens = -1;
    }
}

vr::EVROverlayError SetSharedOverlayTexture(vr::VROverlayHandle_t ovrl_handle_source, vr::VROverlayHandle_t ovrl_handle_target, ID3D11Resource* device_texture_ref)
{
    //Get overlay texture handle from OpenVR and set it as handle for the target overlay
    ID3D11ShaderResourceView* ovrl_shader_res;
    uint32_t ovrl_width;
    uint32_t ovrl_height;
    uint32_t ovrl_native_format;
    vr::ETextureType ovrl_api_type;
    vr::EColorSpace ovrl_color_space;
    vr::VRTextureBounds_t ovrl_tex_bounds;

    vr::VROverlayError ovrl_error = vr::VROverlayError_None;
    ovrl_error = vr::VROverlay()->GetOverlayTexture(ovrl_handle_source, (void**)&ovrl_shader_res, device_texture_ref, &ovrl_width, &ovrl_height, &ovrl_native_format,
                                                    &ovrl_api_type, &ovrl_color_space, &ovrl_tex_bounds);

    if (ovrl_error == vr::VROverlayError_None)
    {
        {
            Microsoft::WRL::ComPtr<ID3D11Resource> ovrl_tex;
            Microsoft::WRL::ComPtr<IDXGIResource> ovrl_dxgi_resource;
            ovrl_shader_res->GetResource(&ovrl_tex);

            HRESULT hr = ovrl_tex.As(&ovrl_dxgi_resource);//ovrl_tex->QueryInterface(__uuidof(IDXGIResource), (void**)&ovrl_dxgi_resource);

            if (!FAILED(hr))
            {
                HANDLE ovrl_tex_handle = nullptr;
                ovrl_dxgi_resource->GetSharedHandle(&ovrl_tex_handle);

                vr::Texture_t vrtex_target;
                vrtex_target.eType = vr::TextureType_DXGISharedHandle;
                vrtex_target.eColorSpace = vr::ColorSpace_Gamma;
                vrtex_target.handle = ovrl_tex_handle;

                vr::VROverlay()->SetOverlayTexture(ovrl_handle_target, &vrtex_target);
            }
        }

        vr::VROverlay()->ReleaseNativeOverlayHandle(ovrl_handle_source, (void*)ovrl_shader_res);
        ovrl_shader_res = nullptr;
    }

    return ovrl_error;
}

DEVMODE GetDevmodeForDisplayID(int display_id, HMONITOR* hmon)
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
            //Enum the available outputs
            Microsoft::WRL::ComPtr<IDXGIOutput> output_ptr;
            while (adapter_ptr->EnumOutputs(output_count, &output_ptr) != DXGI_ERROR_NOT_FOUND)
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

int GetMonitorRefreshRate(int display_id)
{
    DEVMODE mode = GetDevmodeForDisplayID(display_id);

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

bool FileExists(LPCTSTR path)
{
    DWORD attrib = GetFileAttributes(path);

    return ((attrib != INVALID_FILE_ATTRIBUTES) && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
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
    std::pair<HWND, DWORD> params = { 0, pid };

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
                                             ::SetLastError(-1);
                                             pParams->first = hwnd;
                                             return FALSE;
                                         }
                                     }

                                      //Continue enumerating
                                      return TRUE;
                                  },
                                  (LPARAM)&params);

    if ( (!bResult) && (::GetLastError() == -1) && (params.first) )
    {
        return params.first;
    }

    return 0;
}

//This ain't pretty, but GetKeyNameText() works with scancodes, which are not exactly the same and the output strings aren't that nice either (and always localized)
//Those duplicate lines are optimized away to the same address by any sane compiler, nothing to worry about.
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