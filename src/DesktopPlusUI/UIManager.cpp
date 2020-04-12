#include "UIManager.h"

#include "imgui.h"
#include "imgui_impl_win32_openvr.h"

#include <windows.h>

#include "InterprocessMessaging.h"
#include "ConfigManager.h"
#include "Util.h"

#include "WindowKeyboardHelper.h"

//While this is a singleton like many other classes, we want to be careful about initializing it at global scope, so we leave that until a bit later in main()
UIManager* g_UIManagerPtr = nullptr;

void UIManager::DisplayDashboardAppError(const std::string& str) //Ideally this is never called
{
    vr::VROverlay()->HideOverlay(m_OvrlHandle);

    vr::VRMessageOverlayResponse res = vr::VROverlay()->ShowMessageOverlay(str.c_str(), "Desktop+ Error", "Ok", "Restart Desktop+");

    if (res == vr::VRMessageOverlayResponse_ButtonPress_1)
    {
        ConfigManager::Get().SaveConfigToFile();

        STARTUPINFO si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);

        ::CreateProcess(L"DesktopPlus.exe", nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

        //We don't care about these, so close right away
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
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
                                          m_UIScale(1.0f),
                                          m_LowCompositorRes(false),
                                          m_LowCompositorQuality(false),
                                          m_ElevatedTaskSetUp(false),
                                          m_OvrlHandle(vr::k_ulOverlayHandleInvalid),
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
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandle, 2.75f);

            //Init Keyboard Helper overlay
            vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusKeyboardHelper", "Desktop+ Keyboard Helper", &m_OvrlHandleKeyboardHelper);

            //Set input parameters
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandle, vr::VROverlayFlags_SendVRSmoothScrollEvents, true);

            vr::HmdVector2_t mouse_scale;
            mouse_scale.v[0] = OVERLAY_WIDTH;
            mouse_scale.v[1] = OVERLAY_HEIGHT;

            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandle, &mouse_scale);
            vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandle, vr::VROverlayInputMethod_Mouse);
            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleKeyboardHelper, &mouse_scale);
            vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleKeyboardHelper, vr::VROverlayInputMethod_Mouse);

            //Setup texture bounds for both overlays. The keyboard helper is rendered on the same texture as a form of discount multi-viewport rendering
            vr::VRTextureBounds_t bounds;
            bounds.uMin = 0.0f;
            bounds.vMin = 0.0f;
            bounds.uMax = 1.0f;
            bounds.vMax = (float)MAIN_SURFACE_HEIGHT / OVERLAY_HEIGHT;
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandle, &bounds);
            bounds.vMin = bounds.vMax;
            bounds.vMax = 1.0f;
            bounds.uMax = KEYBOARD_HELPER_SCALE;
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandleKeyboardHelper, &bounds);
        }
    }

    m_OpenVRLoaded = true;
    m_LowCompositorRes = (vr::VRSettings()->GetFloat("GpuSpeed", "gpuSpeedRenderTargetScale") < 1.0f);

    UpdateOverlayPixelSize();

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
                    UpdateOverlayPixelSize();
                    break;
                }
                case ipcact_vrkeyboard_closed:
                {
                    ImGui_ImplOpenVR_InputOnVRKeyboardClosed();
                    break;
                }
            }
            break;
        }
        case ipcmsg_set_config:
        {
            if (msg.wParam < configid_bool_MAX)
            {
                ConfigID_Bool bool_id = (ConfigID_Bool)msg.wParam;
                ConfigManager::Get().SetConfigBool(bool_id, (msg.lParam != 0) );
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX)
            {
                ConfigID_Int int_id = (ConfigID_Int)(msg.wParam - configid_bool_MAX);
                ConfigManager::Get().SetConfigInt(int_id, (int)msg.lParam);
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX + configid_float_MAX)
            {
                ConfigID_Float float_id = (ConfigID_Float)(msg.wParam - configid_bool_MAX - configid_int_MAX);
                ConfigManager::Get().SetConfigFloat(float_id, *(float*)&msg.lParam);    //Interpret lParam as a float variable
            }

            break;
        }
    }
}

void UIManager::OnExit()
{
    //Re-launch in VR when we were in desktop mode and probably got switched from VR mode before
    //This is likely more intuitive than just removing the UI entirely when clicking X
    if ( (m_DesktopMode) && (IPCManager::Get().IsDashboardAppRunning()) && (!ConfigManager::Get().GetConfigBool(configid_bool_interface_no_ui)) )
    {
        STARTUPINFO si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);

        ::CreateProcess(L"DesktopPlusUI.exe", nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

        //We don't care about these, so close right away
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
    }
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
    int compositor_quality = vr::VRSettings()->GetInt32("steamvr", "overlayRenderQuality_2");
    m_LowCompositorQuality = ((compositor_quality > 0) && (compositor_quality < 3)); //0 is Auto (not sure if the result of that is accessible), 3 is High
}

bool UIManager::IsElevatedTaskSetUp() const
{
    return m_ElevatedTaskSetUp;
}

bool UIManager::IsOverlayVisible() const
{
    return m_OvrlVisible;
}

bool UIManager::IsOverlayKeyboardHelperVisible() const
{
    return m_OvrlVisibleKeyboardHelper;
}

void UIManager::GetOverlayPixelSize(int& width, int& height) const
{
    width = m_OvrlPixelWidth;
    height = m_OvrlPixelHeight;
}

void UIManager::UpdateOverlayPixelSize()
{
    //If OpenVR was loaded, get it from the overlay
    if (m_OpenVRLoaded)
    {
        vr::VROverlayHandle_t ovrl_handle_dplus;
        vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlus", &ovrl_handle_dplus);

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
        //Imagine if SetOverlayTransformOverlayRelative() actually worked
        vr::HmdMatrix34_t matrix;
        vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;
                
        vr::VROverlay()->GetTransformForOverlayCoordinates(ovrl_handle_dplus, origin, { 0.5f, 0.0f }, &matrix);

        //If the desktop overlay is set to be closer than normal and not far behind the user either, move the UI along so it stays usable
        const float& dplus_forward = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_offset_forward);
        float distance_forward;

        if ( (!ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached)) && (dplus_forward > 0.0f) && (dplus_forward <= 2.5f) ) 
            distance_forward = dplus_forward;
        else
            distance_forward = 0.0f;

        //Y offset probably dependent on UI overlay height. It's static, but something to keep in mind if it ever changes
        //Z offset makes the UI stand a bit in front of the desktop overlay
        OffsetTransformFromSelf(matrix, 0.0f, 0.75f, 0.025f + distance_forward);

        //Update Curvature
        float curve = ConfigManager::Get().GetConfigFloat(configid_float_overlay_curvature);

        if ( (curve == -1.0f) || (ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached)) ) //-1 is auto, match the dashboard (also do it when detached)
        {
            vr::VROverlayHandle_t system_dashboard;
            vr::VROverlay()->FindOverlay("system.systemui", &system_dashboard);

            if (system_dashboard != vr::k_ulOverlayHandleInvalid)
            {
                vr::VROverlay()->GetOverlayCurvature(system_dashboard, &curve);
            }
            else //Very odd, but hey
            {
                curve = 0.0f;
            }
        }
        else
        {
            //Adjust curve value used for UI by the overlay width difference so it's curved according to its size
            curve *= (2.75f / ConfigManager::Get().GetConfigFloat(configid_float_overlay_width));
        }

        //Only apply when left mouse is not held down, prevent moving it around while dragging or holding a widget modifying the forward distance value
        //Same goes for the curvature
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
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
         (ConfigManager::Get().GetConfigBool(configid_bool_state_keyboard_visible_for_dashboard)) )
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
            else
            {
                vr::VROverlay()->HideOverlay(m_OvrlHandleKeyboardHelper);
                window_kdbhelper.Hide();
                m_OvrlVisibleKeyboardHelper = false;
            }
        }
    }
    else if (vr::VROverlay()->IsOverlayVisible(m_OvrlHandleKeyboardHelper)) //Hide when disabled and still visible
    {
        vr::VROverlay()->HideOverlay(m_OvrlHandleKeyboardHelper);
        window_kdbhelper.Hide();
        m_OvrlVisibleKeyboardHelper = false;
    }
}
