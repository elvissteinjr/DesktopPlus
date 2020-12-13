#include "UIManager.h"

#include "imgui.h"
#include "imgui_impl_win32_openvr.h"

#include <windows.h>

#include "InterprocessMessaging.h"
#include "ConfigManager.h"
#include "OverlayManager.h"
#include "Util.h"
#include "WindowList.h"

#include "WindowKeyboardHelper.h"

//While this is a singleton like many other classes, we want to be careful about initializing it at global scope, so we leave that until a bit later in main()
UIManager* g_UIManagerPtr = nullptr;

void UIManager::DisplayDashboardAppError(const std::string& str) //Ideally this is never called
{
    //Hide UI overlay
    vr::VROverlay()->HideOverlay(m_OvrlHandle);
    m_OvrlVisible = false;

    //Hide all dashboard app overlays as well. Usually the dashboard app closes, but it may sometimes get stuck which could put its overlays in the way of the message overlay.
    for (unsigned int i = k_ulOverlayID_Dashboard; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().FindOverlayHandle(i);

        if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->HideOverlay(ovrl_handle);
        }
    }

    vr::VRMessageOverlayResponse res = vr::VROverlay()->ShowMessageOverlay(str.c_str(), "Desktop+ Error", "Ok", "Restart Desktop+");

    if (res == vr::VRMessageOverlayResponse_ButtonPress_1)
    {
        RestartDashboardApp();
    }

    //Dashboard will be closed after dismissing the message overlay, so open it back up with Desktop+
    vr::VROverlay()->ShowDashboard("elvissteinjr.DesktopPlusDashboard");
}

UIManager* UIManager::Get()
{
    return g_UIManagerPtr;
}

UIManager::UIManager(bool desktop_mode) : m_WindowHandle(nullptr),
                                          m_RepeatFrame(false),
                                          m_DesktopMode(desktop_mode),
                                          m_OpenVRLoaded(false),
                                          m_NoRestartOnExit(false),
                                          m_UIScale(1.0f),
                                          m_FontCompact(nullptr),
                                          m_FontLarge(nullptr),
                                          m_LowCompositorRes(false),
                                          m_LowCompositorQuality(false),
                                          m_OverlayErrorLast(vr::VROverlayError_None),
                                          m_WinRTErrorLast(S_OK),
                                          m_ElevatedTaskSetUp(false),
                                          m_OvrlHandle(vr::k_ulOverlayHandleInvalid),
                                          m_OvrlHandleFloatingUI(vr::k_ulOverlayHandleInvalid),
                                          m_OvrlHandleKeyboardHelper(vr::k_ulOverlayHandleInvalid),
                                          m_OvrlVisible(false),
                                          m_OvrlVisibleKeyboardHelper(false),
                                          m_OvrlPixelWidth(1),
                                          m_OvrlPixelHeight(1)
{
    g_UIManagerPtr = this;

    //Check if the scheduled task is set up
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    WCHAR cmd[] = L"\"schtasks\" /Query /TN \"DesktopPlus Elevated\"";

    if (::CreateProcess(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        //Wait for it to exit, which should be pretty much instant
        ::WaitForSingleObject(pi.hProcess, INFINITE);

        //Get the exit code. It should be 0 on success
        DWORD exit_code;
        ::GetExitCodeProcess(pi.hProcess, &exit_code);

        m_ElevatedTaskSetUp = (exit_code == 0);
    }
    
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
}

UIManager::~UIManager()
{
    g_UIManagerPtr = nullptr;
}

vr::EVRInitError UIManager::InitOverlay()
{
    vr::EVRInitError init_error;
    vr::IVRSystem* vr_ptr = vr::VR_Init(&init_error, vr::VRApplication_Overlay);

    if (init_error != vr::VRInitError_None)
        return init_error;

    if (!vr::VROverlay())
        return vr::VRInitError_Init_InvalidInterface;

    vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyUIApp);

    vr::VROverlayError ovrl_error = vr::VROverlayError_None;

    if (!m_DesktopMode) //For desktop mode we only init OpenVR, but don't set up any overlays
    {
        //This loop gets rid of any other process hogging our overlay key. Though in normal situations another Desktop+UI process would've already be killed before this
        while (true)
        {
            ovrl_error = vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusUI", "Desktop+UI", &m_OvrlHandle);

            if (ovrl_error == vr::VROverlayError_KeyInUse)  //If the key is already in use, kill the owning process (hopefully another instance of this app)
            {
                ovrl_error = vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusUI", &m_OvrlHandle);

                if ((ovrl_error == vr::VROverlayError_None) && (m_OvrlHandle != vr::k_ulOverlayHandleInvalid))
                {
                    uint32_t pid = vr::VROverlay()->GetOverlayRenderingPid(m_OvrlHandle);

                    HANDLE phandle;
                    phandle = ::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, TRUE, pid);

                    if (phandle != nullptr)
                    {
                        ::TerminateProcess(phandle, 0);
                        ::CloseHandle(phandle);
                    }
                    else
                    {
                        ovrl_error = vr::VROverlayError_KeyInUse;
                        break;
                    }
                }
                else
                {
                    ovrl_error = vr::VROverlayError_KeyInUse;
                    break;
                }
            }
            else
            {
                break;
            }
        }

        if (m_OvrlHandle != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandle, OVERLAY_WIDTH_METERS_DASHBOARD_UI);

            //Init Floating UI and Keyboard Helper overlay
            vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusUIFloating", "Desktop+ Floating UI", &m_OvrlHandleFloatingUI);
            vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusKeyboardHelper", "Desktop+ Keyboard Helper", &m_OvrlHandleKeyboardHelper);
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleFloatingUI, OVERLAY_WIDTH_METERS_DASHBOARD_UI);
            vr::VROverlay()->SetOverlayAlpha(m_OvrlHandleFloatingUI, 0.0f);

            //Set input parameters
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandle, vr::VROverlayFlags_SendVRSmoothScrollEvents, true);

            vr::HmdVector2_t mouse_scale;
            mouse_scale.v[0] = TEXSPACE_TOTAL_WIDTH;
            mouse_scale.v[1] = TEXSPACE_TOTAL_HEIGHT;

            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandle, &mouse_scale);
            vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandle, vr::VROverlayInputMethod_Mouse);
            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleFloatingUI, &mouse_scale);
            vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleFloatingUI, vr::VROverlayInputMethod_Mouse);
            vr::VROverlay()->SetOverlaySortOrder(m_OvrlHandleFloatingUI, 1);
            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleKeyboardHelper, &mouse_scale);
            vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleKeyboardHelper, vr::VROverlayInputMethod_Mouse);

            //Setup texture bounds for all overlays
            //The floating UI/keyboard helper is rendered on the same texture as a form of discount multi-viewport rendering
            float spacing_size = (float)TEXSPACE_VERTICAL_SPACING / TEXSPACE_TOTAL_HEIGHT;
            float texel_offset = 0.5f / TEXSPACE_TOTAL_HEIGHT;
            vr::VRTextureBounds_t bounds;
            bounds.uMin = 0.0f;
            bounds.vMin = 0.0f;
            bounds.uMax = 1.0f;
            bounds.vMax = ((float)TEXSPACE_DASHBOARD_UI_HEIGHT / TEXSPACE_TOTAL_HEIGHT) + texel_offset;
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandle, &bounds);
            bounds.vMin = bounds.vMax + spacing_size;
            bounds.vMax = bounds.vMax + spacing_size + ((float)TEXSPACE_FLOATING_UI_HEIGHT / TEXSPACE_TOTAL_HEIGHT);
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandleFloatingUI, &bounds);
            bounds.vMin = bounds.vMax + spacing_size;
            bounds.vMax = 1.0f;
            bounds.uMax = TEXSPACE_KEYBOARD_HELPER_SCALE;
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandleKeyboardHelper, &bounds);
        }
    }

    m_OpenVRLoaded = true;
    m_LowCompositorRes = (vr::VRSettings()->GetFloat("GpuSpeed", "gpuSpeedRenderTargetScale") < 1.0f);

    UpdateDesktopOverlayPixelSize();

    //Check if it's a WMR system and set up for that if needed
    SetConfigForWMR(ConfigManager::Get().GetConfigIntRef(configid_int_interface_wmr_ignore_vscreens));

    if ((ovrl_error == vr::VROverlayError_None))
        return vr::VRInitError_None;
    else
        return vr::VRInitError_Compositor_OverlayInitFailed;
}

void UIManager::HandleIPCMessage(const MSG& msg)
{
    //Config strings come as WM_COPYDATA
    if (msg.message == WM_COPYDATA)
    {
        COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)msg.lParam;
        
        //Arbitrary size limit to prevent some malicous applications from sending bad data
        if ( (pcds->dwData < configid_str_MAX) && (pcds->cbData > 0) && (pcds->cbData <= 4096) ) 
        {
            //Apply overlay id override if needed
            unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
            int overlay_override_id = ConfigManager::Get().GetConfigInt(configid_int_state_overlay_current_id_override);

            if (overlay_override_id != -1)
            {
                OverlayManager::Get().SetCurrentOverlayID(overlay_override_id);
            }

            std::string copystr((char*)pcds->lpData, pcds->cbData); //We rely on the data length. The data is sent without the NUL byte

            ConfigID_String str_id = (ConfigID_String)pcds->dwData;
            ConfigManager::Get().SetConfigString(str_id, copystr);

            switch (str_id)
            {
                case configid_str_state_detached_transform_current: //UI only needs the transform to be able to save it later. It's never modified from here
                {
                    ConfigManager::Get().GetOverlayDetachedTransform() = Matrix4(copystr);
                    break;
                }
                case configid_str_state_ui_keyboard_string:
                {
                    ImGui_ImplOpenVR_AddInputFromOSK(copystr.c_str());
                    break;
                }
                case configid_str_state_dashboard_error_string:
                {
                    DisplayDashboardAppError(copystr);
                    break;
                }
            }

            //Restore overlay id override
            if (overlay_override_id != -1)
            {
                OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
            }
        }

        return;
    }

    IPCMsgID msgid = IPCManager::Get().GetIPCMessageID(msg.message);

    switch (msgid)
    {
        case ipcmsg_action:
        {
            switch (msg.wParam)
            {
                case ipcact_resolution_update:
                {
                    UpdateDesktopOverlayPixelSize();
                    break;
                }
                case ipcact_vrkeyboard_closed:
                {
                    ImGui_ImplOpenVR_InputOnVRKeyboardClosed();
                    break;
                }
                case ipcact_overlay_creation_error:
                {
                    m_OverlayErrorLast = (vr::VROverlayError)msg.lParam;
                    break;
                }
                case ipcact_winrt_thread_error:
                {
                    m_WinRTErrorLast = (HRESULT)msg.lParam;
                    break;
                }
            }
            break;
        }
        case ipcmsg_set_config:
        {
            //Apply overlay id override if needed
            unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
            int overlay_override_id = ConfigManager::Get().GetConfigInt(configid_int_state_overlay_current_id_override);

            if (overlay_override_id != -1)
            {
                OverlayManager::Get().SetCurrentOverlayID(overlay_override_id);
            }

            if (msg.wParam < configid_bool_MAX)
            {
                ConfigID_Bool bool_id = (ConfigID_Bool)msg.wParam;
                ConfigManager::Get().SetConfigBool(bool_id, (msg.lParam != 0) );
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX)
            {
                ConfigID_Int int_id = (ConfigID_Int)(msg.wParam - configid_bool_MAX);
                ConfigManager::Get().SetConfigInt(int_id, (int)msg.lParam);

                switch (int_id)
                {
                    case configid_int_interface_overlay_current_id:
                    {
                        OverlayManager::Get().SetCurrentOverlayID((unsigned int)msg.lParam);
                        current_overlay_old = (unsigned int)msg.lParam;
                        break;
                    }
                    default: break;
                }
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX + configid_float_MAX)
            {
                ConfigID_Float float_id = (ConfigID_Float)(msg.wParam - configid_bool_MAX - configid_int_MAX);
                ConfigManager::Get().SetConfigFloat(float_id, *(float*)&msg.lParam);    //Interpret lParam as a float variable
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX + configid_float_MAX + configid_intptr_MAX)
            {
                ConfigID_IntPtr intptr_id = (ConfigID_IntPtr)(msg.wParam - configid_bool_MAX - configid_int_MAX - configid_float_MAX);
                ConfigManager::Get().SetConfigIntPtr(intptr_id, msg.lParam);

                switch (intptr_id)
                {
                    case configid_intptr_overlay_state_winrt_hwnd:
                    {
                        //Set last known title and exe name from new handle
                        HWND window_handle = (HWND)msg.lParam;
                        WindowInfo info(window_handle);
                        info.ExeName = WindowInfo::GetExeName(window_handle);

                        ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_title,    StringConvertFromUTF16(info.Title.c_str()));
                        ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_exe_name, info.ExeName);
                    }
                    default: break;
                }
            }

            //Restore overlay id override
            if (overlay_override_id != -1)
            {
                OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
            }

            break;
        }
    }
}

void UIManager::OnExit()
{
    //Re-launch in VR when we were in desktop mode and probably got switched from VR mode before
    //This is likely more intuitive than just removing the UI entirely when clicking X
    if ( (m_DesktopMode) && (IPCManager::Get().IsDashboardAppRunning()) && (!ConfigManager::Get().GetConfigBool(configid_bool_interface_no_ui)) && (!m_NoRestartOnExit) )
    {
        Restart(false);
    }
    else
    {
        //Save config, just in case (we don't need to do this when calling Restart())
        ConfigManager::Get().SaveConfigToFile();
    }
}

DashboardUI& UIManager::GetDashboardUI()
{
    return m_DashboardUI;
}

FloatingUI& UIManager::GetFloatingUI()
{
    return m_FloatingUI;
}

void UIManager::SetWindowHandle(HWND handle)
{
    m_WindowHandle = handle;
}

HWND UIManager::GetWindowHandle() const
{
    return m_WindowHandle;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandle() const
{
    return m_OvrlHandle;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleFloatingUI() const
{
    return m_OvrlHandleFloatingUI;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleKeyboardHelper() const
{
    return m_OvrlHandleKeyboardHelper;
}

void UIManager::RepeatFrame()
{
    m_RepeatFrame = 2;
}

bool UIManager::GetRepeatFrame() const
{
    return (m_RepeatFrame != 0);
}

void UIManager::DecreaseRepeatFrameCount()
{
    if (m_RepeatFrame != 0)
        m_RepeatFrame--;
}

bool UIManager::IsInDesktopMode() const
{
    return m_DesktopMode;
}

bool UIManager::IsOpenVRLoaded() const
{
    return m_OpenVRLoaded;
}

void UIManager::DisableRestartOnExit()
{
    m_NoRestartOnExit = true;
}

void UIManager::Restart(bool desktop_mode)
{
    ConfigManager::Get().SaveConfigToFile();

    UIManager::Get()->DisableRestartOnExit();

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    std::wstring path = WStringConvertFromUTF8(ConfigManager::Get().GetApplicationPath().c_str()) + L"DesktopPlusUI.exe";
    WCHAR cmd[] = L"-DesktopMode";

    ::CreateProcess(path.c_str(), (desktop_mode) ? cmd : nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

    //We don't care about these, so close right away
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
}

void UIManager::RestartDashboardApp(bool force_steam)
{
    ConfigManager::Get().ResetConfigStateValues();
    ConfigManager::Get().SaveConfigToFile();

    bool use_steam = ( (force_steam) || (ConfigManager::Get().GetConfigBool(configid_bool_state_misc_process_started_by_steam)) );

    //LaunchDashboardOverlay() technically also launches the non-Steam version if it's registered, but there's no reason to use it in that case
    if (use_steam)
    {
        //We need OpenVR loaded for this
        if (!m_OpenVRLoaded)
        {
            InitOverlay();
        }

        //Steam will not launch the overlay if it's already running, so in this case we need to get rid of the running instance now
        StopProcessByWindowClass(g_WindowClassNameDashboardApp);

        ULONGLONG start_tick = ::GetTickCount64();
        while ( (vr::VRApplications()->LaunchDashboardOverlay(g_AppKeyDashboardApp) == vr::VRApplicationError_ApplicationAlreadyRunning) && (::GetTickCount64() - start_tick < 3000) ) //Try 3 seconds max
        {
            ::Sleep(100);
        }
    }
    else
    {
        STARTUPINFO si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);

        std::wstring path = WStringConvertFromUTF8(ConfigManager::Get().GetApplicationPath().c_str()) + L"DesktopPlus.exe";
        ::CreateProcess(path.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

        //We don't care about these, so close right away
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
    }
}

void UIManager::ElevatedModeEnter()
{
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    WCHAR cmd[] = L"\"schtasks\" /Run /TN \"DesktopPlus Elevated\""; //"CreateProcessW, can modify the contents of this string", so don't go optimize this away
    ::CreateProcess(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    //We don't care about these, so close right away
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
}

void UIManager::ElevatedModeLeave()
{
    //Kindly ask elevated mode process to quit
    if (HWND window = ::FindWindow(g_WindowClassNameElevatedMode, nullptr))
    {
        ::PostMessage(window, WM_QUIT, 0, 0);
    }
}

void UIManager::SetUIScale(float scale)
{
    m_UIScale = scale;

    if (!m_DesktopMode)
    {
        ConfigManager::Get().SetConfigFloat(configid_float_interface_last_vr_ui_scale, scale);
    }
}

float UIManager::GetUIScale() const
{
    return m_UIScale;
}

void UIManager::SetFonts(ImFont* font_compact, ImFont* font_large)
{
    m_FontCompact = font_compact;
    m_FontLarge = font_large;
}

ImFont* UIManager::GetFontCompact() const
{
    return m_FontCompact;
}

ImFont* UIManager::GetFontLarge() const
{
    return m_FontLarge;
}

bool UIManager::IsCompositorResolutionLow() const
{
    return m_LowCompositorRes;
}

bool UIManager::IsCompositorRenderQualityLow() const
{
    return m_LowCompositorQuality;
}

void UIManager::UpdateCompositorRenderQualityLow()
{
    if (!m_OpenVRLoaded)
        return;

    int compositor_quality = vr::VRSettings()->GetInt32("steamvr", "overlayRenderQuality_2");
    m_LowCompositorQuality = ((compositor_quality > 0) && (compositor_quality < 3)); //0 is Auto (not sure if the result of that is accessible), 3 is High
}

vr::EVROverlayError UIManager::GetOverlayErrorLast() const
{
    return m_OverlayErrorLast;
}

HRESULT UIManager::GetWinRTErrorLast() const
{
    return m_WinRTErrorLast;
}

void UIManager::ResetOverlayErrorLast()
{
    m_OverlayErrorLast = vr::VROverlayError_None;
}

void UIManager::ResetWinRTErrorLast()
{
    m_WinRTErrorLast = S_OK;
}

bool UIManager::IsElevatedTaskSetUp() const
{
    return m_ElevatedTaskSetUp;
}

void UIManager::TryChangingWindowFocus()
{
    //This is a non-exhaustive attempt to get a different window to set focus on, but it works in most cases
    HWND window_top    = ::GetForegroundWindow();
    HWND window_switch = nullptr;

    //Just use a capturable window list as a base, it's about what we want here anyways
    std::vector<WindowInfo> window_list = WindowInfo::CreateCapturableWindowList();
    auto it = std::find_if(window_list.begin(), window_list.end(), [&](const auto& info){ return (info.WindowHandle == window_top); });

    //Find the next window in that is not elevated
    if (it != window_list.end())
    {
        for (++it; it != window_list.end(); ++it)
        {
            //Check if the window is also of an elevated process
            DWORD process_id = 0;
            ::GetWindowThreadProcessId(it->WindowHandle, &process_id);

            if (!IsProcessElevated(process_id))
            {
                window_switch = it->WindowHandle;
                break;
            }
        }
    }

    //If no window was found fall back
    if (window_switch == nullptr)
    {
        //Focusing the desktop as last resort works but can be awkward since the focus is not obvious and keyboard input could do unintended stuff
        window_switch = ::GetShellWindow(); 
    }

    ::SetForegroundWindow(window_switch); //I still do wonder why it is possible to switch away from elevated windows. Documentation says otherwise too.
}

bool UIManager::IsOverlayVisible() const
{
    return m_OvrlVisible;
}

bool UIManager::IsOverlayKeyboardHelperVisible() const
{
    return m_OvrlVisibleKeyboardHelper;
}

void UIManager::GetDesktopOverlayPixelSize(int& width, int& height) const
{
    width  = m_OvrlPixelWidth;
    height = m_OvrlPixelHeight;
}

void UIManager::UpdateDesktopOverlayPixelSize()
{
    //If OpenVR was loaded, get it from the overlay
    if (m_OpenVRLoaded)
    {
        vr::VROverlayHandle_t ovrl_handle_dplus = OverlayManager::Get().FindOverlayHandle(k_ulOverlayID_Dashboard);

        if (ovrl_handle_dplus != vr::k_ulOverlayHandleInvalid)
        {
            vr::HmdVector2_t mouse_scale;
            vr::VROverlay()->GetOverlayMouseScale(ovrl_handle_dplus, &mouse_scale); //Mouse scale is pretty much the underlying pixel count

            m_OvrlPixelWidth  = (int)mouse_scale.v[0];
            m_OvrlPixelHeight = (int)mouse_scale.v[1];
        }
    }
    else //What we get here may not reflect the real values, but let's do some good guesswork
    {
        int& desktop_id = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_desktop_id);

        if (desktop_id >= GetSystemMetrics(SM_CMONITORS))
            desktop_id = -1;
        else if (desktop_id == -2)  //-2 tell the dashboard application to crop it to desktop 0 and the value changes afterwards, though that doesn't happen when it's not running
            desktop_id = 0;

        if (desktop_id == -1)   //All desktops, get virtual screen dimensions
        {
            m_OvrlPixelWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            m_OvrlPixelHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        }
        else    //Single desktop, try to get the screen resolution for it
        {
            DEVMODE mode = GetDevmodeForDisplayID(desktop_id);

            if (mode.dmSize != 0)
            {
                m_OvrlPixelWidth  = mode.dmPelsWidth;
                m_OvrlPixelHeight = mode.dmPelsHeight;
            }
        }
    }
}

void UIManager::PositionOverlay(WindowKeyboardHelper& window_kdbhelper)
{
    vr::VROverlayHandle_t ovrl_handle_dplus;    
    vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &ovrl_handle_dplus);

    if (ovrl_handle_dplus != vr::k_ulOverlayHandleInvalid)
    {
        const OverlayConfigData& config_data = OverlayManager::Get().GetConfigData(k_ulOverlayID_Dashboard);

        //Imagine if SetOverlayTransformOverlayRelative() actually worked
        vr::HmdMatrix34_t matrix;
        vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;
                
        vr::VROverlay()->GetTransformForOverlayCoordinates(ovrl_handle_dplus, origin, { 0.5f, 0.0f }, &matrix);

        //If the desktop overlay is set to be closer than normal and not far behind the user either, move the UI along so it stays usable
        const float& dplus_forward = config_data.ConfigFloat[configid_float_overlay_offset_forward];
        float distance_forward;

        if ( (!config_data.ConfigBool[configid_bool_overlay_detached]) && (dplus_forward > 0.0f) && (dplus_forward <= 2.5f) ) 
            distance_forward = dplus_forward;
        else
            distance_forward = 0.0f;

        //Update Curvature
        float curve = config_data.ConfigFloat[configid_float_overlay_curvature];

        //Adjust curve value used for UI by the overlay width difference so it's curved according to its size
        if (config_data.ConfigFloat[configid_float_overlay_width] >= 2.0f)
        {
            curve *= (2.75f / config_data.ConfigFloat[configid_float_overlay_width]);
            curve = clamp(curve, 0.0f, 1.0f);
        }
        else //Smaller than 2m can more easily curve into the UI overlay, adjust curve further
        {
            //For higher curve values, also move UI overlay forward a bit when needed, even if it doesn't look so nice
            if ( (curve > 0.16f) && (dplus_forward > -0.1f) )
            {
                distance_forward += 0.1f;
            }

            curve *= 1.5f;
            curve = clamp(curve, 0.0f, 1.0f);
        }

        //Offset the overlay
        //Y offset probably dependent on UI overlay height. It's static, but something to keep in mind if it ever changes
        //Z offset makes the UI stand a bit in front of the desktop overlay
        OffsetTransformFromSelf(matrix, 0.0f, 0.75f, 0.025f + distance_forward);

        //Try to reduce flicker by blocking abrupt Y movements (unless X has changed as well, which we assume to happen on real movement)
        //The flicker itself comes from a race condition of the UI possibly getting the overlay transform while it's changing width and position, hard to predict
        bool anti_flicker_can_move = false;
        float anti_flicker_x = matrix.m[0][3];
        float anti_flicker_y = matrix.m[1][3];
        static float anti_flicker_x_last = anti_flicker_x;
        static float anti_flicker_y_last = anti_flicker_y;
        static int anti_flicker_block_count = 0;

        if ( (anti_flicker_x != anti_flicker_x_last) || (fabs(anti_flicker_y - anti_flicker_y_last) < 0.001f) || (anti_flicker_block_count >= 2) )
        {
            anti_flicker_can_move = true;
            anti_flicker_x_last = anti_flicker_x;
            anti_flicker_y_last = anti_flicker_y;
            anti_flicker_block_count = 0;
        }
        else
        {
            anti_flicker_block_count++;
        }

        //Only apply when left mouse is not held down, prevent moving it around while dragging or holding a widget modifying the forward distance value
        //Same goes for the curvature
        if ( (anti_flicker_can_move) && (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) )
        {
            vr::VROverlay()->SetOverlayTransformAbsolute(m_OvrlHandle, origin, &matrix);
            vr::VROverlay()->SetOverlayCurvature(m_OvrlHandle, curve);
        }

        //Set visibility
        if (vr::VROverlay()->IsOverlayVisible(ovrl_handle_dplus))
        {
            if (!m_OvrlVisible)
            {
                vr::VROverlay()->ShowOverlay(m_OvrlHandle);
                m_OvrlVisible = true;
            }
        }
        else
        {
            if (m_OvrlVisible)
            {
                vr::VROverlay()->HideOverlay(m_OvrlHandle);
                m_OvrlVisible = false;
            }
        }
    }
    else if (m_OvrlVisible) //Dashboard overlay has gone missing, hide
    {
        vr::VROverlay()->HideOverlay(m_OvrlHandle);
        m_OvrlVisible = false;
    }

    //Position and show keyboard helper if active
    if ( (ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_helper_enabled)) &&
         (ConfigManager::Get().GetConfigInt(configid_int_state_keyboard_visible_for_overlay_id) >= (int)k_ulOverlayID_Dashboard) )
    {
        vr::VROverlayHandle_t ovrl_handle_keyboard;
        vr::VROverlay()->FindOverlay("system.keyboard", &ovrl_handle_keyboard);

        if (ovrl_handle_keyboard != vr::k_ulOverlayHandleInvalid)
        {
            if (vr::VROverlay()->IsOverlayVisible(ovrl_handle_keyboard))
            {
                vr::HmdMatrix34_t matrix;
                vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;

                //This anchors it on the bottom end of the used minimal mode SteamVR keyboard space
                vr::VROverlay()->GetTransformForOverlayCoordinates(ovrl_handle_keyboard, origin, {960.0f, 29.0f}, &matrix);
                //Slighty lift it so input goes here (minimal mode SteamVR keyboard has a larger overlay than used for the keys)
                OffsetTransformFromSelf(matrix, 0.0f, 0.0f, 0.00001f); 
                vr::VROverlay()->SetOverlayTransformAbsolute(m_OvrlHandleKeyboardHelper, origin, &matrix);

                vr::VROverlay()->ShowOverlay(m_OvrlHandleKeyboardHelper);
                m_OvrlVisibleKeyboardHelper = true;
            }
            else if (m_OvrlVisibleKeyboardHelper)
            {
                vr::VROverlay()->HideOverlay(m_OvrlHandleKeyboardHelper);
                window_kdbhelper.Hide();
                m_OvrlVisibleKeyboardHelper = false;
            }
        }
    }
    else if ( (vr::VROverlay()->IsOverlayVisible(m_OvrlHandleKeyboardHelper)) && (m_OvrlVisibleKeyboardHelper) ) //Hide when disabled and still visible
    {
        vr::VROverlay()->HideOverlay(m_OvrlHandleKeyboardHelper);
        window_kdbhelper.Hide();
        m_OvrlVisibleKeyboardHelper = false;
    }
}
