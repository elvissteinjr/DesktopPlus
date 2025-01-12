#include "UIManager.h"

#include <windowsx.h>

#include "imgui.h"
#include "imgui_impl_win32_openvr.h"
#include "implot.h"

#include "resource.h"
#include "InterprocessMessaging.h"
#include "ConfigManager.h"
#include "OverlayManager.h"
#include "Util.h"
#include "OpenVRExt.h"
#include "WindowManager.h"

#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"

//This one holds mostly constant data, but depends on how the application was launched
static UITextureSpaces g_UITextureSpaces;

UITextureSpaces& UITextureSpaces::Get()
{
    return g_UITextureSpaces;
}

void UITextureSpaces::Init(bool desktop_mode, bool keyboard_editor_mode)
{
    if (!desktop_mode)
    {
        const int vertical_spacing = 2;

        m_TexspaceRects[ui_texspace_total]               = {0, 0, 1920, -1};
        m_TexspaceRects[ui_texspace_overlay_bar]         = {0, 0, m_TexspaceRects[ui_texspace_total].GetWidth(), 420};

        m_TexspaceRects[ui_texspace_floating_ui] =         {0, 
                                                            m_TexspaceRects[ui_texspace_overlay_bar].GetBR().y + vertical_spacing,
                                                            m_TexspaceRects[ui_texspace_total].GetWidth(),
                                                            m_TexspaceRects[ui_texspace_overlay_bar].GetBR().y + vertical_spacing + 320};

        m_TexspaceRects[ui_texspace_overlay_properties] =  {0,
                                                            m_TexspaceRects[ui_texspace_floating_ui].GetBR().y + vertical_spacing,
                                                            959,
                                                            m_TexspaceRects[ui_texspace_floating_ui].GetBR().y + vertical_spacing + 800};

        m_TexspaceRects[ui_texspace_settings] =            {m_TexspaceRects[ui_texspace_total].GetWidth() - m_TexspaceRects[ui_texspace_overlay_properties].GetWidth(),
                                                            m_TexspaceRects[ui_texspace_overlay_properties].GetTL().y,
                                                            m_TexspaceRects[ui_texspace_total].GetWidth(),
                                                            m_TexspaceRects[ui_texspace_overlay_properties].GetBR().y};

        m_TexspaceRects[ui_texspace_keyboard] =            {0,
                                                            m_TexspaceRects[ui_texspace_overlay_properties].GetBR().y + vertical_spacing,
                                                            m_TexspaceRects[ui_texspace_total].GetWidth(), 
                                                            m_TexspaceRects[ui_texspace_overlay_properties].GetBR().y + vertical_spacing + 750};

        m_TexspaceRects[ui_texspace_performance_monitor] = {0, 
                                                            m_TexspaceRects[ui_texspace_keyboard].GetBR().y + vertical_spacing,
                                                            850, 
                                                            m_TexspaceRects[ui_texspace_keyboard].GetBR().y + vertical_spacing + 550};

        m_TexspaceRects[ui_texspace_aux_ui] =              {m_TexspaceRects[ui_texspace_performance_monitor].GetWidth() + vertical_spacing, 
                                                            m_TexspaceRects[ui_texspace_keyboard].GetBR().y + vertical_spacing,
                                                            m_TexspaceRects[ui_texspace_total].GetWidth(),
                                                            m_TexspaceRects[ui_texspace_keyboard].GetBR().y + vertical_spacing + 550};

        //Set total height last
        m_TexspaceRects[ui_texspace_total].Max.y = m_TexspaceRects[ui_texspace_performance_monitor].GetBR().y;
    }
    else
    {
        //Desktop mode only initializes total texture space (as the unscaled window size) and windows used in it
        if (keyboard_editor_mode)
        {
            m_TexspaceRects[ui_texspace_total] = {0, 0, 2100, 1152};    //Results in 1312x720px at 100% DPI scaling
        }
        else
        {
            m_TexspaceRects[ui_texspace_total] = {0, 0, 1153, 1042};    //Results in 720x651px at 100% DPI scaling
        }

        m_TexspaceRects[ui_texspace_overlay_properties] = m_TexspaceRects[ui_texspace_total];
        m_TexspaceRects[ui_texspace_settings] = m_TexspaceRects[ui_texspace_total];
        m_TexspaceRects[ui_texspace_keyboard] = {0, 0, m_TexspaceRects[ui_texspace_total].GetWidth(), 750};
    }
}

const DPRect& UITextureSpaces::GetRect(UITexspaceID texspace_id) const
{
    return m_TexspaceRects[texspace_id];
}

ImVec4 UITextureSpaces::GetRectAsVec4(UITexspaceID texspace_id) const
{
    const DPRect& rect = UITextureSpaces::Get().GetRect(texspace_id);
    return ImVec4((float)rect.Min.x, (float)rect.Min.y, (float)rect.Max.x, (float)rect.Max.y);
}

//While this is a singleton like many other classes, we want to be careful about initializing it at global scope, so we leave that until a bit later in main()
UIManager* g_UIManagerPtr = nullptr;

UIManager::UIManager(bool desktop_mode, bool keyboard_editor_mode) : 
    m_WindowHandle(nullptr),
    m_SharedTextureRef(nullptr),
    m_RepeatFrame(false),
    m_DesktopMode(desktop_mode),
    m_KeyboardEditorMode(keyboard_editor_mode),
    m_OpenVRLoaded(false),
    m_NoRestartOnExit(false),
    m_UIScale(1.0f),
    m_FontCompact(nullptr),
    m_FontLarge(nullptr),
    m_LowCompositorRes(false),
    m_LowCompositorQuality(false),
    m_HasAnyWarning(false),
    m_OverlayErrorLast(vr::VROverlayError_None),
    m_WinRTErrorLast(S_OK),
    m_ElevatedTaskSetUp(false),
    m_ComInitDone(false),
    m_OvrlHandleOverlayBar(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleFloatingUI(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleSettings(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleOverlayProperties(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleKeyboard(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleAuxUI(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleDPlusDashboard(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleSystemUI(vr::k_ulOverlayHandleInvalid),
    m_IsSystemUIHoveredFromSwitch(false),
    m_IsDummyOverlayTransformUnstable(false),
    m_OvrlVisible(false),
    m_OvrlOverlayBarAlpha(0.0f),
    m_SystemUIActiveTick(0),
    m_OverlayBarFadeInTick(0),
    m_OvrlPixelWidth(1),
    m_OvrlPixelHeight(1),
    m_TransformSyncValueCount(0),
    m_TransformSyncValues{0}
{
    g_UIManagerPtr = this;

    //Activate WindowManager
    WindowManager::Get().SetActive(true);

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

void UIManager::DisplayDashboardAppError(const std::string& str) //Ideally this is never called
{
    //Hide UI overlay
    vr::VROverlay()->HideOverlay(m_OvrlHandleOverlayBar);
    m_OvrlVisible = false;

    //Hide all dashboard app overlays as well. Usually the dashboard app closes, but it may sometimes get stuck which could put its overlays in the way of the message overlay.
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().GetConfigData(i).ConfigHandle[configid_handle_overlay_state_overlay_handle];;

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

void UIManager::DisplayInitialSetupNotification()
{
    //Check if the user is currently using the HMD and display the initial setup message as a VR notification instead then
    bool use_vr_notification = false;
    vr::EDeviceActivityLevel activity_level = vr::VRSystem()->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd);

    if ((activity_level == vr::k_EDeviceActivityLevel_UserInteraction) || (activity_level == vr::k_EDeviceActivityLevel_UserInteraction_Timeout))
    {
        //Also check if the HMD is tracking properly right now so the notification can actually be seen (fresh SteamVR start is active but not tracking for example)
        vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, vr::IVRSystemEx::GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

        use_vr_notification = (poses[vr::k_unTrackedDeviceIndex_Hmd].eTrackingResult == vr::TrackingResult_Running_OK);
    }

    if (use_vr_notification)
    {
        vr::VRNotificationId notification_id = 0; //Unused, but documentation doesn't say if passing nullptr is allowed, so we pass this
        std::string message = TranslationManager::GetString(tstr_NotificationInitialStartupTitleVR);
        message += "\n";
        message += TranslationManager::GetString(tstr_NotificationInitialStartupMessage);

        //Despite being sent after overlay creation, it may not be returned right away, so keep trying a bit
        for (int tries = 0; tries < 20; ++tries)
        {
            vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &m_OvrlHandleDPlusDashboard);

            if (m_OvrlHandleDPlusDashboard != vr::k_ulOverlayHandleInvalid)
            {
                break;
            }
            ::Sleep(50);
        }

        vr::VRNotifications()->CreateNotification(m_OvrlHandleDPlusDashboard, 0, vr::EVRNotificationType_Transient, message.c_str(), vr::EVRNotificationStyle_Application, nullptr, &notification_id);
    }
    else
    {
        ::MessageBoxW(nullptr,
                      WStringConvertFromUTF8(TranslationManager::GetString(tstr_NotificationInitialStartupMessage)).c_str(), 
                      WStringConvertFromUTF8(TranslationManager::GetString(tstr_NotificationInitialStartupTitleDesktop)).c_str(), MB_OK);
    }
}

void UIManager::SetOverlayInputEnabled(bool is_enabled)
{
    vr::VROverlayInputMethod input_method = (is_enabled) ? vr::VROverlayInputMethod_Mouse : vr::VROverlayInputMethod_None;

    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleOverlayBar,        input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleFloatingUI,        input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleSettings,          input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleOverlayProperties, input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleKeyboard,          input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleAuxUI,             input_method);
}

UITexspaceID UIManager::GetTexspaceIDForOverlayHandle(vr::VROverlayHandle_t overlay_handle) const
{
    UITexspaceID overlay_texspace = ui_texspace_total;
    if (overlay_handle == m_OvrlHandleOverlayBar)
        overlay_texspace = ui_texspace_overlay_bar;
    else if (overlay_handle == m_OvrlHandleFloatingUI)
        overlay_texspace = ui_texspace_floating_ui;
    else if (overlay_handle == m_OvrlHandleSettings)
        overlay_texspace = ui_texspace_settings;
    else if (overlay_handle == m_OvrlHandleOverlayProperties)
        overlay_texspace = ui_texspace_overlay_properties;
    else if (overlay_handle == m_OvrlHandleKeyboard)
        overlay_texspace = ui_texspace_keyboard;
    else if (overlay_handle == m_OvrlHandleAuxUI)
        overlay_texspace = ui_texspace_aux_ui;
    
    return overlay_texspace;
}

void UIManager::HandleOverlayProfileLoadMessage(LPARAM lparam)
{
    IPCActionOverlayProfileLoadArg profile_load_arg = (IPCActionOverlayProfileLoadArg)LOWORD(lparam);
    int profile_overlay_id = GET_Y_LPARAM(lparam);

    const std::string& profile_name = ConfigManager::GetValue(configid_str_state_profile_name_load);

    if (profile_overlay_id == -2)
    {
        ConfigManager::Get().LoadOverlayProfileDefault(true);
    }
    else if (profile_load_arg == ipcactv_ovrl_profile_multi)
    {
        ConfigManager::Get().LoadMultiOverlayProfileFromFile(profile_name + ".ini", true);
    }
    else if (profile_load_arg == ipcactv_ovrl_profile_multi_add)
    {
        IM_ASSERT(profile_overlay_id != -1);   //Exclusion is unhandled for now. Shouldn't be sent anyways

        ConfigManager::Get().LoadMultiOverlayProfileFromFile(profile_name + ".ini", false);
    }

    OnProfileLoaded();
}

UIManager* UIManager::Get()
{
    return g_UIManagerPtr;
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
        for (int tries = 0; tries < 10; ++tries)
        {
            ovrl_error = vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusUI", "Desktop+UI", &m_OvrlHandleOverlayBar);

            if (ovrl_error == vr::VROverlayError_KeyInUse)  //If the key is already in use, kill the owning process (hopefully another instance of this app)
            {
                ovrl_error = vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusUI", &m_OvrlHandleOverlayBar);

                if ((ovrl_error == vr::VROverlayError_None) && (m_OvrlHandleOverlayBar != vr::k_ulOverlayHandleInvalid))
                {
                    LOG_F(INFO, "Overlay key already in use, killing owning process...");

                    uint32_t pid = vr::VROverlay()->GetOverlayRenderingPid(m_OvrlHandleOverlayBar);

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
                    }
                }
                else
                {
                    ovrl_error = vr::VROverlayError_KeyInUse;
                }
            }
            else
            {
                break;
            }

            //Try again in a bit to check if it's just a race with some external cleanup
            ::Sleep(200);
        }

        if (m_OvrlHandleOverlayBar != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleOverlayBar, OVERLAY_WIDTH_METERS_DASHBOARD_UI);

            //Init additional overlays
            vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusUIFloating",          "Desktop+ Floating UI", &m_OvrlHandleFloatingUI);
            vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusUISettings",          "Desktop+ Settings UI", &m_OvrlHandleSettings);
            vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusUIOverlayProperties", "Desktop+ Settings UI", &m_OvrlHandleOverlayProperties);
            vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusUIKeyboard",          "Desktop+ Keyboard",    &m_OvrlHandleKeyboard);
            vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusUIAux",               "Desktop+ Aux UI",      &m_OvrlHandleAuxUI);

            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleFloatingUI,        OVERLAY_WIDTH_METERS_DASHBOARD_UI);
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleSettings,          OVERLAY_WIDTH_METERS_SETTINGS);
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleOverlayProperties, OVERLAY_WIDTH_METERS_SETTINGS);
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleKeyboard,          OVERLAY_WIDTH_METERS_KEYBOARD);

            vr::VROverlay()->SetOverlayAlpha(m_OvrlHandleFloatingUI, 0.0f);

            //Set input parameters
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleOverlayBar,        vr::VROverlayFlags_SendVRSmoothScrollEvents, true);
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleSettings,          vr::VROverlayFlags_SendVRSmoothScrollEvents, true);
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleOverlayProperties, vr::VROverlayFlags_SendVRSmoothScrollEvents, true);
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleKeyboard,          vr::VROverlayFlags_SendVRSmoothScrollEvents, true);
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleKeyboard,          vr::VROverlayFlags_MultiCursor,              true);
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleAuxUI,             vr::VROverlayFlags_SendVRSmoothScrollEvents, true);

            vr::HmdVector2_t mouse_scale;
            mouse_scale.v[0] = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetWidth();
            mouse_scale.v[1] = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetHeight();

            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleOverlayBar,         &mouse_scale);
            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleFloatingUI,         &mouse_scale);
            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleSettings,           &mouse_scale);
            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleOverlayProperties,  &mouse_scale);
            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleKeyboard,           &mouse_scale);
            vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleAuxUI,              &mouse_scale);

            SetOverlayInputEnabled(true);

            //Setup texture bounds for all overlays
            //The UI windows are rendered on the same texture as a form of discount multi-viewport rendering
            vr::VRTextureBounds_t bounds = {};

            const DPRect& rect_total = UITextureSpaces::Get().GetRect(ui_texspace_total);
            float tex_width  = (float)rect_total.GetWidth();
            float tex_height = (float)rect_total.GetHeight();

            const DPRect& rect_overlay_bar = UITextureSpaces::Get().GetRect(ui_texspace_overlay_bar);
            bounds.uMin = rect_overlay_bar.GetTL().x / tex_width;
            bounds.vMin = rect_overlay_bar.GetTL().y / tex_height;
            bounds.uMax = rect_overlay_bar.GetBR().x / tex_width;
            bounds.vMax = rect_overlay_bar.GetBR().y / tex_height;
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandleOverlayBar, &bounds);

            const DPRect& rect_floating_ui = UITextureSpaces::Get().GetRect(ui_texspace_floating_ui);
            bounds.uMin = rect_floating_ui.GetTL().x / tex_width;
            bounds.vMin = rect_floating_ui.GetTL().y / tex_height;
            bounds.uMax = rect_floating_ui.GetBR().x / tex_width;
            bounds.vMax = rect_floating_ui.GetBR().y / tex_height;
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandleFloatingUI, &bounds);

            const DPRect& rect_settings = UITextureSpaces::Get().GetRect(ui_texspace_settings);
            bounds.uMin = rect_settings.GetTL().x / tex_width;
            bounds.vMin = rect_settings.GetTL().y / tex_height;
            bounds.uMax = rect_settings.GetBR().x / tex_width;
            bounds.vMax = rect_settings.GetBR().y / tex_height;
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandleSettings, &bounds);

            const DPRect& rect_ovrlprops = UITextureSpaces::Get().GetRect(ui_texspace_overlay_properties);
            bounds.uMin = rect_ovrlprops.GetTL().x / tex_width;
            bounds.vMin = rect_ovrlprops.GetTL().y / tex_height;
            bounds.uMax = rect_ovrlprops.GetBR().x / tex_width;
            bounds.vMax = rect_ovrlprops.GetBR().y / tex_height;
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandleOverlayProperties, &bounds);

            const DPRect& rect_keyboard = UITextureSpaces::Get().GetRect(ui_texspace_keyboard);
            bounds.uMin = rect_keyboard.GetTL().x / tex_width;
            bounds.vMin = rect_keyboard.GetTL().y / tex_height;
            bounds.uMax = rect_keyboard.GetBR().x / tex_width;
            bounds.vMax = rect_keyboard.GetBR().y / tex_height;
            vr::VROverlay()->SetOverlayTextureBounds(m_OvrlHandleKeyboard, &bounds);

            //Set curve pitch for overlay bar. This adjusts the pitch to match the SteamVR dashboard
            vr::VROverlay()->SetOverlayPreCurvePitch(m_OvrlHandleOverlayBar, 0.25f);
        }
    }

    //Cache SystemUI handle as it won't change during the session anyways
    //We do not cache the GamepadUI handle as it may disappear during the session when Steam closes and make SteamVR switch to the previous dashboard
    vr::VROverlay()->FindOverlay("system.systemui", &m_OvrlHandleSystemUI);

    m_OpenVRLoaded = true;
    m_LowCompositorRes = (vr::VRSettings()->GetFloat("GpuSpeed", "gpuSpeedRenderTargetScale") < 1.0f);

    UpdateDesktopOverlayPixelSize();
    UpdateCompositorRenderQualityLow();

    m_WindowSettings.ApplyCurrentOverlayState();
    m_WindowOverlayProperties.ApplyCurrentOverlayState();
    m_VRKeyboard.GetWindow().ApplyCurrentOverlayState();

    m_WindowPerformance.ResetCumulativeValues();
    m_WindowPerformance.RefreshTrackerBatteryList();

    ConfigManager::Get().InitConfigForWMR();

    if ((ovrl_error == vr::VROverlayError_None))
        return vr::VRInitError_None;
    else
        return vr::VRInitError_Compositor_OverlayInitFailed;
}

void UIManager::HandleIPCMessage(const MSG& msg, bool handle_delayed)
{
    //Handle messages sent by browser process in the APIClient
    if (msg.message == DPBrowserAPIClient::Get().GetRegisteredMessageID())
    {
        DPBrowserAPIClient::Get().HandleIPCMessage(msg);
        return;
    }

    //Apply overlay id override if needed
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    int overlay_override_id = ConfigManager::GetValue(configid_int_state_overlay_current_id_override);

    if (overlay_override_id != -1)
    {
        OverlayManager::Get().SetCurrentOverlayID(overlay_override_id);
    }

    //Config strings come as WM_COPYDATA
    if (msg.message == WM_COPYDATA)
    {
        COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)msg.lParam;

        //Arbitrary size limit to prevent some malicous applications from sending bad data
        if ( (pcds->dwData < configid_str_MAX) && (pcds->cbData <= 4096) ) 
        {
            std::string copystr((char*)pcds->lpData, pcds->cbData); //We rely on the data length. The data is sent without the NUL byte

            ConfigID_String str_id = (ConfigID_String)pcds->dwData;
            ConfigManager::SetValue(str_id, copystr);

            switch (str_id)
            {
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
        else if ( (pcds->dwData >= dpbrowser_ipcstr_MIN) && (pcds->dwData < dpbrowser_ipcstr_MAX) ) //Probably a string for DPBrowserAPIClient
        {
            DPBrowserAPIClient::Get().HandleIPCMessage(msg);
        }

        //Restore overlay id override
        if (overlay_override_id != -1)
        {
            OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
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
                case ipcact_overlays_reset:
                {
                    UpdateDesktopOverlayPixelSize();
                    m_WindowPerformance.ScheduleOverlaySharedTextureUpdate();
                    break;
                }
                case ipcact_overlays_ui_reset:
                {
                    m_WindowPerformance.ScheduleOverlaySharedTextureUpdate();
                    break;
                }
                case ipcact_overlay_profile_load:
                {
                    HandleOverlayProfileLoadMessage(msg.lParam);
                    break;
                }
                case ipcact_overlay_new_drag:
                {
                    int desktop_id = GET_X_LPARAM(msg.lParam); //(No need to extract pointer distance)

                    OverlayCaptureSource capsource;

                    switch (desktop_id)
                    {
                        case -2: capsource = ovrl_capsource_winrt_capture;       break;
                        case -3: capsource = ovrl_capsource_ui;                  break;
                        default: capsource = ovrl_capsource_desktop_duplication;
                    }

                    OverlayManager::Get().AddOverlay(capsource, desktop_id, (HWND)ConfigManager::GetValue(configid_handle_state_arg_hwnd));
                    break;
                }
                case ipcact_overlay_remove:
                {
                    unsigned int overlay_id = (unsigned int)msg.lParam;

                    OverlayManager::Get().RemoveOverlay(overlay_id);

                    //Hide properties window if it's open for this overlay
                    if (m_WindowOverlayProperties.GetActiveOverlayID() == overlay_id)
                    {
                        m_WindowOverlayProperties.SetActiveOverlayID(k_ulOverlayID_None, true);
                        m_WindowOverlayProperties.Hide();
                    }
                    else if (m_WindowOverlayProperties.GetActiveOverlayID() > overlay_id) //Adjust properties window active overlay ID if it's open for an overlay that had its shifted
                    {
                        m_WindowOverlayProperties.SetActiveOverlayID(m_WindowOverlayProperties.GetActiveOverlayID() - 1, true);
                    }

                    m_WindowOverlayBar.HideMenus();
                    break;
                }
                case ipcact_overlay_creation_error:
                {
                    m_OverlayErrorLast = (vr::VROverlayError)msg.lParam;
                    UpdateAnyWarningDisplayedState();
                    break;
                }
                case ipcact_winrt_thread_error:
                {
                    m_WinRTErrorLast = (HRESULT)msg.lParam;
                    UpdateAnyWarningDisplayedState();
                    break;
                }
                case ipcact_notification_show:
                {
                    DisplayInitialSetupNotification();
                    break;
                }
                case ipcact_winmanager_winlist_add:
                case ipcact_winmanager_winlist_update:
                {
                    if (handle_delayed)
                    {
                        const WindowInfo* window_info = nullptr;
                        bool has_title_changed = true;

                        if (msg.wParam == ipcact_winmanager_winlist_add)
                            window_info = &WindowManager::Get().WindowListAdd((HWND)msg.lParam);
                        else
                            window_info = WindowManager::Get().WindowListUpdateTitle((HWND)msg.lParam, &has_title_changed);

                        if ( (window_info != nullptr) && (has_title_changed) ) //Only do this when the title changed
                        {
                            if (ImGui::StringContainsUnmappedCharacter(window_info->GetListTitle().c_str()))
                            {
                                TextureManager::Get().ReloadAllTexturesLater();
                                UIManager::Get()->RepeatFrame();
                            }

                            //Update last window info strings for overlays using this window
                            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                            {
                                OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                                if ( (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] == msg.lParam) && (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) )
                                {
                                    data.ConfigStr[configid_str_overlay_winrt_last_window_title]      = StringConvertFromUTF16(window_info->GetTitle().c_str());
                                    data.ConfigStr[configid_str_overlay_winrt_last_window_class_name] = StringConvertFromUTF16(window_info->GetWindowClassName().c_str());
                                    data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name]   = window_info->GetExeName();

                                    OverlayManager::Get().SetOverlayNameAuto(i, window_info);
                                    OnOverlayNameChanged();
                                }
                            }
                        }
                    }
                    else
                    {
                        m_DelayedICPMessages.push_back(msg);
                    }

                    break;
                }
                case ipcact_winmanager_winlist_remove:
                {
                    std::wstring last_title_w = WindowManager::Get().WindowListRemove((HWND)msg.lParam);
                    std::string last_title = StringConvertFromUTF16(last_title_w.c_str());

                    //Some windows clear their title entirely before ceasing to exist, skip those
                    if (last_title.empty())
                        break;

                    //Set last known title for overlays that captured this window
                    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                    {
                        OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                        if (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] == msg.lParam)
                        {
                            data.ConfigStr[configid_str_overlay_winrt_last_window_title] = last_title;
                        }
                    }
                    break;
                }
                case ipcact_winmanager_focus_changed:
                {
                    m_FloatingUI.GetMainBarWindow().MarkCurrentWindowCapturableStateOutdated();
                    break;
                }
                case ipcact_keyboard_show:
                {
                    m_VRKeyboard.GetWindow().SetAssignedOverlayID((int)msg.lParam);
                    (msg.lParam != -1) ? m_VRKeyboard.GetWindow().Show() : m_VRKeyboard.GetWindow().Hide();
                    break;
                }
                case ipcact_keyboard_show_auto:
                {
                    int overlay_id = (msg.lParam != -1) ? (int)msg.lParam : m_VRKeyboard.GetWindow().GetAssignedOverlayID();

                    //Only set auto-visibility if source overlay is desktop/window capture (avoid overriding browser auto-visible keyboards)
                    if ( (overlay_id >= 0) && (OverlayManager::Get().GetConfigData((unsigned int)overlay_id).ConfigInt[configid_int_overlay_capture_source] <= ovrl_capsource_winrt_capture) )
                    {
                        m_VRKeyboard.GetWindow().SetAutoVisibility(overlay_id, (msg.lParam != -1));
                    }
                    break;
                }
                case ipcact_lpointer_ui_drag:
                {
                    (msg.lParam == 1) ? StartOverlayDrag(ConfigManager::GetValue(configid_handle_state_dplus_laser_pointer_target_overlay)) : FinishOverlayDrag();
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
                ConfigManager::SetValue(bool_id, (msg.lParam != 0) );

                switch (bool_id)
                {
                    case configid_bool_state_window_focused_process_elevated:
                    case configid_bool_state_misc_process_elevated:
                    case configid_bool_state_misc_elevated_mode_active:
                    case configid_bool_state_misc_uiaccess_enabled:
                    case configid_bool_state_misc_browser_version_mismatch:
                    case configid_bool_state_misc_browser_used_but_missing:
                    {
                        UpdateAnyWarningDisplayedState();
                        break;
                    }
                    case configid_bool_state_overlay_dragmode_temp:
                    {
                        SetOverlayInputEnabled((msg.lParam == 0));
                        break;
                    }
                }
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX)
            {
                ConfigID_Int int_id = (ConfigID_Int)(msg.wParam - configid_bool_MAX);

                int previous_value = ConfigManager::GetValue(int_id);
                ConfigManager::SetValue(int_id, (int)msg.lParam);

                switch (int_id)
                {
                    case configid_int_overlay_winrt_desktop_id:
                    {
                        OverlayManager::Get().SetCurrentOverlayNameAuto();
                        break;
                    }
                    case configid_int_overlay_crop_x:
                    case configid_int_overlay_crop_y:
                    case configid_int_overlay_crop_width:
                    case configid_int_overlay_crop_height:
                    {
                        //Crop changed while properties window is visible for the current overlay, force refresh of cached crop button label
                        if ( (m_WindowOverlayProperties.IsVisibleOrFading()) && (m_WindowOverlayProperties.GetActiveOverlayID() == OverlayManager::Get().GetCurrentOverlayID()) )
                        {
                            m_WindowOverlayProperties.SetActiveOverlayID(m_WindowOverlayProperties.GetActiveOverlayID(), true);
                        }
                        break;
                    }
                    case configid_int_interface_overlay_current_id:
                    {
                        OverlayManager::Get().SetCurrentOverlayID((unsigned int)msg.lParam);
                        current_overlay_old = (unsigned int)msg.lParam;
                        break;
                    }
                    case configid_int_state_overlay_transform_sync_target_id:
                    {
                        m_TransformSyncValueCount = 0;
                        std::fill(m_TransformSyncValues, std::end(m_TransformSyncValues), 0.0f);
                        break;
                    }
                    case configid_int_state_interface_desktop_count:
                    {
                        RepeatFrame();
                        break;
                    }
                    case configid_int_state_auto_docking_state:
                    {
                        if (msg.lParam == 0)
                        {
                            m_AuxUI.GetDragHintWindow().Hide();
                        }
                        else
                        {
                            //Even config values above 0 are right hand, odd ones are left hand
                            const vr::ETrackedControllerRole role = (msg.lParam % 2 == 0) ? vr::TrackedControllerRole_RightHand : vr::TrackedControllerRole_LeftHand;
                            const bool is_docking = (msg.lParam <= 2);

                            m_AuxUI.GetDragHintWindow().SetHintType(vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(role), (is_docking) ? WindowDragHint::hint_docking : WindowDragHint::hint_undocking);
                            m_AuxUI.GetDragHintWindow().Show();
                        }

                        break;
                    }
                    case configid_int_state_drag_hint_type:
                    {
                        if (msg.lParam != 0)
                        {
                            //We don't use this config value for auto-docking hints since their display is handled alongside the docking state
                            const WindowDragHint::HintType hint_type = (msg.lParam == 1) ? WindowDragHint::hint_ovrl_locked : WindowDragHint::hint_ovrl_theater_screen_blocked;

                            m_AuxUI.GetDragHintWindow().SetHintType(ConfigManager::GetValue(configid_int_state_drag_hint_device), hint_type);
                            m_AuxUI.GetDragHintWindow().Show();
                        }
                        else
                        {
                            m_AuxUI.GetDragHintWindow().Hide();
                        }

                        break;
                    }
                    case configid_int_state_overlay_focused_id:
                    {
                        //Hide auto-visible keyboard if there was one for the previously focused overlay
                        if ( (previous_value != -1) && (previous_value != msg.lParam) )
                        {
                            m_VRKeyboard.GetWindow().SetAutoVisibility((unsigned int)previous_value, false);
                        }
                        break;
                    }
                    default: break;
                }
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX + configid_float_MAX)
            {
                ConfigID_Float float_id = (ConfigID_Float)(msg.wParam - configid_bool_MAX - configid_int_MAX);
                float value = pun_cast<float, LPARAM>(msg.lParam);
                ConfigManager::SetValue(float_id, value);

                switch (float_id)
                {
                    case configid_float_state_overlay_transform_sync_value:
                    {
                        if (m_TransformSyncValueCount < IM_ARRAYSIZE(m_TransformSyncValues))
                        {
                            m_TransformSyncValues[m_TransformSyncValueCount] = value;
                            m_TransformSyncValueCount++;
                        }

                        if (m_TransformSyncValueCount >= IM_ARRAYSIZE(m_TransformSyncValues))
                        {
                            OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)ConfigManager::GetValue(configid_int_state_overlay_transform_sync_target_id));
                            data.ConfigTransform.set(m_TransformSyncValues);

                            m_TransformSyncValueCount = 0;
                            std::fill(m_TransformSyncValues, std::end(m_TransformSyncValues), 0.0f);
                        }

                        break;
                    }
                    default: break;
                }
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX + configid_float_MAX + configid_handle_MAX)
            {
                ConfigID_Handle handle_id = (ConfigID_Handle)(msg.wParam - configid_bool_MAX - configid_int_MAX - configid_float_MAX);
                uint64_t value = pun_cast<uint64_t, LPARAM>(msg.lParam);
                ConfigManager::SetValue(handle_id, value);

                switch (handle_id)
                {
                    case configid_handle_overlay_state_winrt_hwnd:
                    {
                        const WindowInfo* window_info = nullptr;

                        if (value != 0)
                            window_info = WindowManager::Get().WindowListFindWindow((HWND)value);

                        //Set last known title and exe name from new handle
                        if (window_info != nullptr)
                        {
                            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_title,      StringConvertFromUTF16(window_info->GetTitle().c_str()));
                            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_class_name, StringConvertFromUTF16(window_info->GetWindowClassName().c_str()));
                            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_exe_name,   window_info->GetExeName());
                        }
                        else if (value == 0) //Only clear if HWND is really null
                        {
                            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_title, "");
                            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_class_name, "");
                            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_exe_name, "");
                            ConfigManager::SetValue(configid_handle_overlay_state_winrt_last_hicon, 0);
                        }

                        OverlayManager::Get().SetCurrentOverlayNameAuto();

                        //Refresh Overlay Properties window if it's currently active for this overlay
                        unsigned int overlay_props_active_id = m_WindowOverlayProperties.GetActiveOverlayID();

                        if (OverlayManager::Get().GetCurrentOverlayID() == overlay_props_active_id)
                        {
                            m_WindowOverlayProperties.SetActiveOverlayID(overlay_props_active_id, true);
                        }
                    }
                    default: break;
                }
            }

            break;
        }
    }

    //Restore overlay id override
    if (overlay_override_id != -1)
    {
        OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
    }
}

void UIManager::HandleDelayedIPCMessages()
{
    while (!m_DelayedICPMessages.empty())
    {
        HandleIPCMessage(m_DelayedICPMessages.back(), true);
        m_DelayedICPMessages.pop_back();
    }
}

void UIManager::OnInitDone()
{
    //Re-apply active overlay ID since the first time config load couldn't apply it properly with the overlay not yet existing
    unsigned int overlay_props_active_id = m_WindowOverlayProperties.GetActiveOverlayID();
    m_WindowOverlayProperties.SetActiveOverlayID(overlay_props_active_id, true);

    //Hide properties window if the overlay doesn't exist anymore for some reason
    if ((overlay_props_active_id == k_ulOverlayID_None) || (OverlayManager::Get().GetOverlayCount() <= overlay_props_active_id))
    {
        m_WindowOverlayProperties.Hide(true);
        m_WindowOverlayProperties.GetOverlayState(floating_window_ovrl_state_room).IsVisible = false;
        m_WindowOverlayProperties.GetOverlayState(floating_window_ovrl_state_dashboard_tab).IsVisible = false;
    }

    UpdateDesktopOverlayPixelSize();
    ConfigManager::Get().GetAppProfileManager().ActivateProfileForCurrentSceneApp();
}

void UIManager::OnExit()
{
    //Re-launch in VR when we were in desktop mode and probably got switched from VR mode before
    //This is likely more intuitive than just removing the UI entirely when clicking X
    if ( (m_DesktopMode) && (IPCManager::Get().IsDashboardAppRunning()) && (!ConfigManager::GetValue(configid_bool_interface_no_ui)) && (!m_NoRestartOnExit) )
    {
        Restart(false);
    }
    else
    {
        //Save config, just in case (we don't need to do this when calling Restart())
        ConfigManager::Get().SaveConfigToFile();
    }

    //Release any held down keys
    m_VRKeyboard.ResetState();

    if (m_ComInitDone)
    {
        ::CoUninitialize();
    }

    m_SharedTextureRef.Reset();

    WindowManager::Get().SetActive(false);

    vr::VR_Shutdown();
}

void UIManager::OnProfileLoaded()
{
    //Adjust current overlay ID for UI since this may have made the old selection invalid
    int& current_overlay = ConfigManager::Get().GetRef(configid_int_interface_overlay_current_id);
    current_overlay = clamp(current_overlay, 0, (int)OverlayManager::Get().GetOverlayCount() - 1);

    //Adjust overlay properties window    
    //Hide window if overlay ID no longer in range
    if (m_WindowOverlayProperties.GetActiveOverlayID() >= OverlayManager::Get().GetOverlayCount())
    {
        m_WindowOverlayProperties.SetActiveOverlayID(k_ulOverlayID_None, true);
        m_WindowOverlayProperties.Hide();
    }
    else //Just adjust switch if it is still is
    {
        m_WindowOverlayProperties.SetActiveOverlayID(m_WindowOverlayProperties.GetActiveOverlayID(), true);
    }

    //Check if new overlays have any unmapped characters
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        AddFontBuilderStringIfAnyUnmappedCharacters(OverlayManager::Get().GetConfigData(i).ConfigNameStr.c_str());
    }

    RepeatFrame();
}

FloatingUI& UIManager::GetFloatingUI()
{
    return m_FloatingUI;
}

VRKeyboard& UIManager::GetVRKeyboard()
{
    return m_VRKeyboard;
}

AuxUI& UIManager::GetAuxUI()
{
    return m_AuxUI;
}

WindowOverlayBar& UIManager::GetOverlayBarWindow()
{
    return m_WindowOverlayBar;
}

WindowSettings& UIManager::GetSettingsWindow()
{
    return m_WindowSettings;
}

WindowOverlayProperties& UIManager::GetOverlayPropertiesWindow()
{
    return m_WindowOverlayProperties;
}

WindowPerformance& UIManager::GetPerformanceWindow()
{
    return m_WindowPerformance;
}

WindowDesktopMode& UIManager::GetDesktopModeWindow()
{
    return m_WindowDesktopMode;
}

void UIManager::SetWindowHandle(HWND handle)
{
    m_WindowHandle = handle;

    //WindowManager will be initialized before our window is ready, but won't be ready to receive window updates before our window is fully created, so we add it manually here
    if (m_DesktopMode)
        WindowManager::Get().WindowListAdd(handle);
}

HWND UIManager::GetWindowHandle() const
{
    return m_WindowHandle;
}

NotificationIcon& UIManager::GetNotificationIcon()
{
    return m_NotificationIcon;
}

void UIManager::SetSharedTextureRef(ID3D11Resource* ref)
{
   m_SharedTextureRef = ref;
}

ID3D11Resource* UIManager::GetSharedTextureRef() const
{
    return m_SharedTextureRef.Get();
}

OverlayDragger& UIManager::GetOverlayDragger()
{
    return m_OverlayDragger;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleOverlayBar() const
{
    return m_OvrlHandleOverlayBar;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleFloatingUI() const
{
    return m_OvrlHandleFloatingUI;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleSettings() const
{
    return m_OvrlHandleSettings;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleOverlayProperties() const
{
    return m_OvrlHandleOverlayProperties;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleKeyboard() const
{
    return m_OvrlHandleKeyboard;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleAuxUI() const
{
    return m_OvrlHandleAuxUI;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleDPlusDashboard() const
{
    return m_OvrlHandleDPlusDashboard;
}

vr::VROverlayHandle_t UIManager::GetOverlayHandleSystemUI() const
{
    return m_OvrlHandleSystemUI;
}

std::array<vr::VROverlayHandle_t, 6> UIManager::GetUIOverlayHandles() const
{
    return {m_OvrlHandleOverlayBar, m_OvrlHandleFloatingUI, m_OvrlHandleSettings, m_OvrlHandleOverlayProperties, m_OvrlHandleKeyboard, m_OvrlHandleAuxUI};
}

bool UIManager::IsDummyOverlayTransformUnstable() const
{
    return m_IsDummyOverlayTransformUnstable;
}

void UIManager::SendUIIntersectionMaskToDashboardApp(std::vector<vr::VROverlayIntersectionMaskPrimitive_t>& primitives) const
{
    static ULONGLONG last_tick = 0;

    //Mask can change at any time, any frame. We don't really want to send too many messages either though, so we limit the rate and don't update at all if the pointer isn't active
    if ( (ConfigManager::GetValue(configid_int_state_dplus_laser_pointer_device) != vr::k_unTrackedDeviceIndexInvalid) && (last_tick + 100 > ::GetTickCount64()) )
        return;

    for (const auto& rect : primitives)
    {
        DPRect dp_rect((int)rect.m_Primitive.m_Rectangle.m_flTopLeftX,  (int)rect.m_Primitive.m_Rectangle.m_flTopLeftY, 
                       (int)rect.m_Primitive.m_Rectangle.m_flTopLeftX + (int)rect.m_Primitive.m_Rectangle.m_flWidth, (int)rect.m_Primitive.m_Rectangle.m_flTopLeftY + (int)rect.m_Primitive.m_Rectangle.m_flHeight);

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_lpointer_ui_mask_rect, (LPARAM)dp_rect.Pack16());
    }

    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_lpointer_ui_mask_rect, -1); //Mark end of mask

    last_tick = ::GetTickCount64();
}

void UIManager::RepeatFrame(int extra_frame_count)
{
    if (m_RepeatFrame < extra_frame_count)
        m_RepeatFrame = extra_frame_count;
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

bool UIManager::IsInKeyboardEditorMode() const
{
    return m_KeyboardEditorMode;
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
    LOG_IF_F(INFO, !desktop_mode, "Restarting...");
    LOG_IF_F(INFO,  desktop_mode, "Restarting into desktop mode...");

    ConfigManager::Get().SaveConfigToFile();

    UIManager::Get()->DisableRestartOnExit();

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    std::wstring path = WStringConvertFromUTF8(ConfigManager::Get().GetApplicationPath().c_str()) + L"DesktopPlusUI.exe";
    WCHAR cmd[] = L"--DesktopMode";

    ::CreateProcess(path.c_str(), (desktop_mode) ? cmd : nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

    //We don't care about these, so close right away
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
}

void UIManager::RestartIntoKeyboardEditor()
{
    LOG_F(INFO, "Restarting into Keyboard Editor...");

    ConfigManager::Get().SaveConfigToFile();

    UIManager::Get()->DisableRestartOnExit();

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    std::wstring path = WStringConvertFromUTF8(ConfigManager::Get().GetApplicationPath().c_str()) + L"DesktopPlusUI.exe";
    WCHAR cmd[] = L"--KeyboardEditor";

    ::CreateProcess(path.c_str(), cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

    //We don't care about these, so close right away
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
}

void UIManager::RestartDashboardApp(bool force_steam)
{
    LOG_F(INFO, "Restarting dashboard app...%s", (force_steam) ? " (using Steam)" : "");

    ConfigManager::Get().ResetConfigStateValues();
    ConfigManager::Get().SaveConfigToFile();

    bool use_steam = ( (force_steam) || (ConfigManager::GetValue(configid_bool_state_misc_process_started_by_steam)) );

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
        vr::EVRApplicationError app_error = vr::VRApplications()->LaunchDashboardOverlay(g_AppKeyDashboardApp);

        while ( (app_error == vr::VRApplicationError_ApplicationAlreadyRunning) && (::GetTickCount64() - start_tick < 5000) ) //Try 5 seconds max
        {
            ::Sleep(250);
            app_error = vr::VRApplications()->LaunchDashboardOverlay(g_AppKeyDashboardApp);
        }

        //Try without Steam below if launching failed somehow
        if (app_error != vr::VRApplicationError_None)
        {
            use_steam = false;
        }
    }
    
    if (!use_steam)
    {
        std::wstring path = WStringConvertFromUTF8(ConfigManager::Get().GetApplicationPath().c_str()) + L"DesktopPlus.exe";

        if (ConfigManager::GetValue(configid_bool_state_misc_uiaccess_enabled)) //UIAccess enabled executable doesn't run straight from CreateProcess()
        {
            if (!m_ComInitDone) //Let's only do this if really needed
            {
                m_ComInitDone = (::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) != RPC_E_CHANGED_MODE);
            }

            ::ShellExecute(nullptr, nullptr, path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        else
        {
            STARTUPINFO si = {0};
            PROCESS_INFORMATION pi = {0};
            si.cb = sizeof(si);

            ::CreateProcess(path.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

            //We don't care about these, so close right away
            ::CloseHandle(pi.hProcess);
            ::CloseHandle(pi.hThread);
        }
    }

    m_WindowPerformance.ScheduleOverlaySharedTextureUpdate();
    m_WindowOverlayProperties.Hide();                           //Current overlay won't be set on dashboard app, so close this now
}

void UIManager::ElevatedModeEnter()
{
    LOG_F(INFO, "Entered elevated mode");

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
    LOG_F(INFO, "Left elevated mode");

    //Kindly ask elevated mode process to quit
    if (HWND window = ::FindWindow(g_WindowClassNameElevatedMode, nullptr))
    {
        ::PostMessage(window, WM_QUIT, 0, 0);
    }
}

void UIManager::UpdateStyle()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;

    ImGuiIO& io = ImGui::GetIO();

    //Setup Dear ImGui style
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark();
    //Do a bit of custom styling
    style.DisabledAlpha  = 0.5f;
    style.WindowRounding = 7.0f;
    style.FrameRounding  = 4.0f;
    style.GrabRounding   = 3.0f;
    style.IndentSpacing  = style.ItemSpacing.x;

    if (m_DesktopMode)
    {
        style.WindowPadding.x /= 2.0f;
        style.WindowPadding.y /= 2.0f;
    }

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.085f, 0.135f, 0.155f, 0.96f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.088f, 0.138f, 0.158f, 0.96f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.185f, 0.245f, 0.285f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.109f, 0.175f, 0.224f, 1.000f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.08f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.298f, 0.596f, 0.859f, 1.000f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.333f, 0.616f, 1.000f, 1.000f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.063f, 0.548f, 1.000f, 1.000f);
    colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.28f, 0.305f, 0.3f, 0.25f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    Style_ImGuiCol_TextNotification        = ImVec4(0.64f, 0.97f, 0.26f, 1.00f);
    Style_ImGuiCol_TextWarning             = ImVec4(0.98f, 0.81f, 0.26f, 1.00f);
    Style_ImGuiCol_TextError               = ImVec4(0.97f, 0.33f, 0.33f, 1.00f);
    Style_ImGuiCol_ButtonPassiveToggled    = ImVec4(0.122f, 0.220f, 0.322f, 1.000f);
    Style_ImGuiCol_SteamVRCursor           = ImVec4(0.463f, 0.765f, 0.882f, 1.000f);
    Style_ImGuiCol_SteamVRCursorBorder     = ImVec4(0.161f, 0.176f, 0.196f, 0.929f);

    //Setup ImPlot style
    ImPlotStyle& plot_style = ImPlot::GetStyle();
    plot_style.PlotPadding               = {0.0f, 0.0f};
    plot_style.FillAlpha                 = 0.25f;
    plot_style.Colors[ImPlotCol_FrameBg] = ImVec4(0.03f, 0.05f, 0.06f, 0.10f);
    plot_style.Colors[ImPlotCol_PlotBg]  = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    //Adapt to DPI
    float ui_scale = 1.0f;
    if (UIManager::Get()->IsInDesktopMode())
    {
        float dpi_scale = ImGui_ImplWin32_GetDpiScaleForHwnd(m_WindowHandle);

        //Allow overriding the UI scale
        float scale_override = ConfigManager::GetValue(configid_float_interface_desktop_ui_scale_override);

        if (scale_override != 0.0f)
        {
            dpi_scale = clamp(scale_override, 0.10f, 5.0f);
        }

        ui_scale = dpi_scale * 0.625f;      //Scaling based on 100% being the VR font at 32pt and desktop 100% DPI font being at 20pt

        style.ScaleAllSizes(dpi_scale);     //Scale based on DPI scale, not UI scale (basically only go larger)
    }

    UIManager::Get()->SetUIScale(ui_scale);

    //Set DPI-dependent style
    style.LogSliderDeadzone = (float)int(58.0f * ui_scale); //Force whole pixel size

    if (UIManager::Get()->IsInDesktopMode())
    {
        io.DisplaySize.x = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetWidth()  * ui_scale;
        io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetHeight() * ui_scale;

        style.ScrollbarSize = (float)int(23.0f * ui_scale); 
    }
    else
    {
        io.DisplaySize.x = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetWidth();
        io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetHeight();

        style.ScrollbarSize = (float)int(32.0f * ui_scale);

        //UpdateOverlayDimming() relies on loaded ImGui/style, so do the initial call to that here
        UIManager::Get()->UpdateOverlayDimming();
    }

    TextureManager::Get().LoadAllTexturesAndBuildFonts();
    RepeatFrame();
}

void UIManager::SetUIScale(float scale)
{
    m_UIScale = scale;

    if (!m_DesktopMode)
    {
        ConfigManager::SetValue(configid_float_interface_last_vr_ui_scale, scale);
    }

    m_WindowOverlayProperties.ApplyUIScale();
    m_WindowSettings.ApplyUIScale();
    m_VRKeyboard.GetWindow().ApplyUIScale();
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

void UIManager::AddFontBuilderStringIfAnyUnmappedCharacters(const char* str)
{
    if (ImGui::StringContainsUnmappedCharacter(str))
    {
        if (TextureManager::Get().AddFontBuilderString(str))
        {
            TextureManager::Get().ReloadAllTexturesLater();
            RepeatFrame();
        }
    }
}

void UIManager::OnDPIChanged(int new_dpi, const RECT& new_window_rect)
{
    if (!m_DesktopMode)
        return;

    UpdateStyle();

    //Set new window position from the provided rect
    ::SetWindowPos(m_WindowHandle, nullptr, 
                   new_window_rect.left, 
                   new_window_rect.top, 
                   new_window_rect.right  - new_window_rect.left, 
                   new_window_rect.bottom - new_window_rect.top, SWP_NOZORDER | SWP_NOACTIVATE);

    //Reload window icon in DPI-appropriate size. We don't really want to reload the big icon, but it gets reset to the small one if that one's changed, so we do both
    HINSTANCE instance = (HINSTANCE)::GetWindowLongPtr(m_WindowHandle, GWLP_HINSTANCE);
    const float new_scale = (float)new_dpi / USER_DEFAULT_SCREEN_DPI;

    HICON icon_small = (HICON)LoadImage(instance, MAKEINTRESOURCE(IDI_DPLUS), IMAGE_ICON, int(GetSystemMetrics(SM_CXSMICON) * new_scale), int(GetSystemMetrics(SM_CYSMICON) * new_scale), LR_DEFAULTCOLOR);
    HICON icon       = (HICON)LoadImage(instance, MAKEINTRESOURCE(IDI_DPLUS), IMAGE_ICON,     GetSystemMetrics(SM_CXICON),                    GetSystemMetrics(SM_CYICON),                LR_DEFAULTCOLOR);

    HICON icon_prev_small = (HICON)::SendMessage(m_WindowHandle, WM_SETICON, ICON_SMALL, (LPARAM)icon_small);
    HICON icon_prev       = (HICON)::SendMessage(m_WindowHandle, WM_SETICON, ICON_BIG,   (LPARAM)icon);

    //Destroy the previous icons
    ::DestroyIcon(icon_prev_small);
    ::DestroyIcon(icon_prev);
}

void UIManager::OnTranslationChanged()
{
    TextureManager::Get().ReloadAllTexturesLater();

    m_NotificationIcon.RefreshPopupMenu();
    m_WindowSettings.ClearCachedTranslationStrings();
    m_WindowOverlayProperties.SetActiveOverlayID(m_WindowOverlayProperties.GetActiveOverlayID(), true);

    DPBrowserAPIClient::Get().DPBrowser_ErrorPageSetStrings(TranslationManager::GetString(tstr_BrowserErrorPageTitle), 
                                                            TranslationManager::GetString(tstr_BrowserErrorPageHeading), 
                                                            TranslationManager::GetString(tstr_BrowserErrorPageMessage));

    RepeatFrame();
}

void UIManager::OnOverlayNameChanged()
{
    m_WindowOverlayProperties.SetActiveOverlayID(m_WindowOverlayProperties.GetActiveOverlayID(), true);

    if ( (m_VRKeyboard.GetInputTarget() == kbdtarget_overlay) && (m_VRKeyboard.GetWindow().IsVisible()) )
    {
        m_VRKeyboard.GetWindow().Show();
    }

    RepeatFrame();
}

void UIManager::UpdateOverlayDimming()
{
    if ( (ConfigManager::GetValue(configid_bool_interface_dim_ui)) && (vr::VROverlay()->IsOverlayVisible(m_OvrlHandleDPlusDashboard)) )
    {
        for (const auto& overlay_handle : GetUIOverlayHandles())
        {
            vr::VROverlay()->SetOverlayColor(overlay_handle, 0.05f, 0.05f, 0.05f);
        }

        vr::VROverlay()->SetOverlayColor(m_OvrlHandleOverlayBar, 0.05f, 0.05f, 0.05f);
        ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f; //Set window bg alpha to 100% to not have the contrast be even worse on light backgrounds
    }
    else
    {
        for (const auto& overlay_handle : GetUIOverlayHandles())
        {
            vr::VROverlay()->SetOverlayColor(overlay_handle, 1.0f, 1.0f, 1.0f);
        }
        
        ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 0.96f;
    }
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

    UpdateAnyWarningDisplayedState();
}

bool UIManager::IsAnyWarningDisplayed() const
{
    return m_HasAnyWarning;
}

void UIManager::UpdateAnyWarningDisplayedState()
{
    m_HasAnyWarning = false;

    //Check all possible warnings. This has to be in sync with what WindowSettings::UpdateWarnings() does to be correct.

    //Compositor resolution warning
    if ( (!ConfigManager::GetValue(configid_bool_interface_warning_compositor_res_hidden)) && (m_LowCompositorRes) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //Compositor quality warning
    if ( (!ConfigManager::GetValue(configid_bool_interface_warning_compositor_quality_hidden)) && (m_LowCompositorQuality) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //Dashboard app process elevation warning
    if ( (!ConfigManager::GetValue(configid_bool_interface_warning_process_elevation_hidden)) && (ConfigManager::GetValue(configid_bool_state_misc_process_elevated)) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //Elevated mode warning (this is different from elevated dashboard process)
    if ( (!ConfigManager::GetValue(configid_bool_interface_warning_elevated_mode_hidden)) && (ConfigManager::GetValue(configid_bool_state_misc_elevated_mode_active)) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //Browser missing warning
    if ( (!ConfigManager::GetValue(configid_bool_interface_warning_browser_missing_hidden)) && (ConfigManager::GetValue(configid_bool_state_misc_browser_used_but_missing)) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //Browser version mismatch warning
    if ( (!ConfigManager::GetValue(configid_bool_interface_warning_browser_version_mismatch_hidden)) && (ConfigManager::GetValue(configid_bool_state_misc_browser_version_mismatch)) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //Focused process elevation warning
    if (  (ConfigManager::GetValue(configid_bool_state_window_focused_process_elevated)) && (!ConfigManager::GetValue(configid_bool_state_misc_process_elevated)) && 
         (!ConfigManager::GetValue(configid_bool_state_misc_elevated_mode_active))       && (!ConfigManager::GetValue(configid_bool_state_misc_uiaccess_enabled)) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //UIAccess lost warning
    if ( (ConfigManager::GetValue(configid_bool_misc_uiaccess_was_enabled)) && (!ConfigManager::GetValue(configid_bool_state_misc_uiaccess_enabled)) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //Overlay error warning
    if ( (m_OverlayErrorLast != vr::VROverlayError_None) && (m_OpenVRLoaded) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //WinRT Capture error warning
    if ( (m_WinRTErrorLast != S_OK) && (m_OpenVRLoaded) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //App profile with overlay profile active warning
    if ( (!ConfigManager::GetRef(configid_bool_interface_warning_app_profile_active_hidden)) && (ConfigManager::Get().GetAppProfileManager().IsActiveProfileWithOverlayProfile()) )
    {
        m_HasAnyWarning = true;
        return;
    }

    //App profile with overlay profile active warning
    if (ConfigManager::GetValue(configid_bool_state_misc_config_migrated))
    {
        m_HasAnyWarning = true;
        return;
    }
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

void UIManager::TryChangingWindowFocus() const
{
    //This is a non-exhaustive attempt to get a different window to set focus on, but it works in most cases
    HWND window_top    = ::GetForegroundWindow();
    HWND window_switch = nullptr;

    //Try current VR app window first
    if (m_OpenVRLoaded)
    {
        uint32_t pid = vr::VRApplications()->GetCurrentSceneProcessId();

        if ( (pid != 0) && (!IsProcessElevated(pid)) )
        {
            window_switch = FindMainWindow(pid);
        }
    }

    //Try getting the next window
    if (window_switch == nullptr)
    {
        //Just use the capturable window list as a base, it's about what we want here anyways
        auto& window_list = WindowManager::Get().WindowListGet();
        auto it = std::find_if(window_list.begin(), window_list.end(), [&](const auto& info){ return (info.GetWindowHandle() == window_top); });

        //Find the next window in that is not elevated
        if (it != window_list.end())
        {
            for (++it; it != window_list.end(); ++it)
            {
                //Check if the window is also of an elevated process
                DWORD process_id = 0;
                ::GetWindowThreadProcessId(it->GetWindowHandle(), &process_id);

                if (!IsProcessElevated(process_id))
                {
                    window_switch = it->GetWindowHandle();
                    break;
                }
            }
        }
    }

    //If no window was found fall back
    if (window_switch == nullptr)
    {
        //Focusing the desktop as last resort works but can be awkward since the focus is not obvious and keyboard input could do unintended stuff
        window_switch = ::GetShellWindow(); 
    }

    //Dashboard app is more successful at changing focus for some reason, so let it try instead
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_focus_window, (LPARAM)window_switch);
}

bool UIManager::IsOverlayBarOverlayVisible() const
{
    return m_OvrlVisible;
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
        vr::VROverlayHandle_t ovrl_handle_dplus = vr::k_ulOverlayHandleInvalid;
        vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDesktopTexture", &ovrl_handle_dplus);

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
        int& desktop_id = ConfigManager::GetRef(configid_int_overlay_desktop_id);

        if (desktop_id >= ConfigManager::GetValue(configid_int_state_interface_desktop_count))
            desktop_id = -1;
        else if (desktop_id == -2)  //-2 tell the dashboard application to crop it to desktop 0 and the value changes afterwards, though that doesn't happen when it's not running
            desktop_id = 0;

        if ( (desktop_id == -1) || (!ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring)) )   //All desktops, get virtual screen dimensions
        {
            m_OvrlPixelWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            m_OvrlPixelHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        }
        else    //Single desktop, try to get the screen resolution for it
        {
            DEVMODE mode = GetDevmodeForDisplayID(desktop_id, (ConfigManager::GetValue(configid_int_interface_wmr_ignore_vscreens) == 1));

            if (mode.dmSize != 0)
            {
                m_OvrlPixelWidth  = mode.dmPelsWidth;
                m_OvrlPixelHeight = mode.dmPelsHeight;
            }
        }
    }
}

void UIManager::PositionOverlay()
{
    vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &m_OvrlHandleDPlusDashboard);

    if (m_OvrlHandleDPlusDashboard != vr::k_ulOverlayHandleInvalid)
    {
        //Adjust behavior if gamepad ui (SteamVR 2 dashboard) exists
        vr::VROverlayHandle_t handle_gamepad_ui = vr::k_ulOverlayHandleInvalid;
        vr::VROverlay()->FindOverlay("valve.steam.gamepadui.bar", &handle_gamepad_ui);

        //Imagine if SetOverlayTransformOverlayRelative() actually worked
        vr::HmdMatrix34_t matrix_ovr;
        vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;

        vr::VROverlay()->GetTransformForOverlayCoordinates(m_OvrlHandleDPlusDashboard, origin, { 0.5f, 0.0f }, &matrix_ovr);

        //Adjust curve for dashboard position
        float curve = 0.145f;

        if (handle_gamepad_ui != vr::k_ulOverlayHandleInvalid)
        {
            curve = 0.19f;  //SteamVR 2 removed dashboard distance setting
        }
        else
        {
            int32_t dashboard_pos = vr::VRSettings()->GetInt32(vr::k_pch_Dashboard_Section, "position_2");

            switch (dashboard_pos)
            {
                case 0: curve = 0.17f; break; //Near
                case 1: curve = 0.16f; break; //Middle
                case 2: curve = 0.15f; break; //Far
            }
        }

        //Offset the overlay
        //It's offset in so it's as close to the dashboard as possible while not messing up pointer input. 
        //Most problematic dashboard element is the current application button (SystemUI) and dashboard reposition bar (GamepadUI).
        if (m_WindowOverlayBar.IsScrollBarVisible())
        {
            if (handle_gamepad_ui != vr::k_ulOverlayHandleInvalid)
            {
                vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix_ovr, 0.0f, -0.247f, 0.392f);
            }
            else
            {
                vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix_ovr, 0.0f, -0.272f, 0.195f);
            }
        }
        else
        {
            if (handle_gamepad_ui != vr::k_ulOverlayHandleInvalid)
            {
                vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix_ovr, 0.0f, -0.200f, 0.382f);
            }
            else
            {
                vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix_ovr, 0.0f, -0.225f, 0.185f);
            }
        }

        //SteamVR 2.9.1 made changes that affected the dashboard matrix
        //The changes themselves are odd as they add a control bar to *every* dashboard overlay
        //Hopefully temporary, but for now fix this up when running those beta versions
        if (strstr(vr::VRSystem()->GetRuntimeVersion(), "2.9.") != nullptr)
        {
            if (handle_gamepad_ui != vr::k_ulOverlayHandleInvalid)
            {
                vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix_ovr, 0.0f, -0.11f, -0.000f);
            }
            else
            {
                vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix_ovr, 0.0f, -0.15f, -0.000f);
            }
        }

        //Rotate slightly forward (local rotation)
        Matrix4 mat_m4;                 //is identity
        mat_m4.rotateX(-14.0f);
        mat_m4 = Matrix4(matrix_ovr) * mat_m4;
        matrix_ovr = mat_m4.toOpenVR34();

        //Try to reduce flicker by blocking abrupt Y movements (unless X has changed as well, which we assume to happen on real movement)
        //The flicker itself comes from a race condition of the UI possibly getting the overlay transform while it's changing width and position, hard to predict
        bool anti_flicker_can_move = false;
        float anti_flicker_x = matrix_ovr.m[0][3];
        float anti_flicker_y = matrix_ovr.m[1][3];
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

        //Block transform updates on abrupt movements, repeated frames, mouse down and shortly after switching desktops
        m_IsDummyOverlayTransformUnstable = ( (!anti_flicker_can_move) || (GetRepeatFrame()) || (ImGui::IsMouseDown(ImGuiMouseButton_Left)) || 
                                              (ImGui::GetIO().MouseClickedTime[ImGuiMouseButton_Left] + 0.3 >= ImGui::GetTime()) ||
                                              (m_FloatingUI.GetActionBarWindow().GetLastDesktopSwitchTime() + 0.3 >= ImGui::GetTime()) ); 

        if (!m_IsDummyOverlayTransformUnstable)
        {
            vr::VROverlay()->SetOverlayTransformAbsolute(m_OvrlHandleOverlayBar, origin, &matrix_ovr);
            vr::VROverlay()->SetOverlayCurvature(m_OvrlHandleOverlayBar, curve);
        }

        //Set visibility
        if (vr::VROverlay()->IsOverlayVisible(m_OvrlHandleDPlusDashboard))
        {
            bool is_systemui_hovered = vr::VROverlay()->IsHoverTargetOverlay(m_OvrlHandleSystemUI);

            //Check for GamepadUI as well if it exists
            if (handle_gamepad_ui != vr::k_ulOverlayHandleInvalid)
            {
                is_systemui_hovered = (is_systemui_hovered || vr::VROverlay()->IsHoverTargetOverlay(handle_gamepad_ui));
            }

            if (!m_OvrlVisible)
            {
                vr::VROverlay()->ShowOverlay(m_OvrlHandleOverlayBar);
                m_OvrlVisible = true;
                UpdateOverlayDimming();
                m_OverlayDragger.UpdateTempStandingPosition();

                //We prevent the fade-out when Overlay Bar is newly visible while the dashboard SystemUI is being hovered until the pointer leaves that overlay at least once
                if (is_systemui_hovered)
                {
                    m_IsSystemUIHoveredFromSwitch = true;
                }
            }
            else
            {
                //Fade out Overlay Bar when the dashboard SystemUI is being used as overlay z-order isn't fine-grained enough for it to just work
                if ( (!m_IsSystemUIHoveredFromSwitch) && (is_systemui_hovered) )
                {
                    m_OverlayBarFadeInTick = 0;

                    if (m_SystemUIActiveTick == 0)
                    {
                        m_SystemUIActiveTick = ::GetTickCount64();
                    }
                    else
                    {
                        unsigned int delay = (m_WindowOverlayBar.IsAnyMenuVisible()) ? 800 : 300;

                        if (m_SystemUIActiveTick + delay < ::GetTickCount64())
                        {
                            if (m_OvrlOverlayBarAlpha != 0.0f)
                            {
                                m_OvrlOverlayBarAlpha -= ImGui::GetIO().DeltaTime * 12.0f;

                                if (m_OvrlOverlayBarAlpha < 0.0f)
                                    m_OvrlOverlayBarAlpha = 0.0f;

                                vr::VROverlay()->SetOverlayAlpha(m_OvrlHandleOverlayBar, m_OvrlOverlayBarAlpha);
                            }
                            else if (vr::VROverlay()->IsOverlayVisible(m_OvrlHandleOverlayBar))
                            {
                                m_WindowOverlayBar.HideMenus();
                                vr::VROverlay()->HideOverlay(m_OvrlHandleOverlayBar); //Hide to avoid input flicker
                            }
                        }
                    }
                }
                else
                {
                    m_SystemUIActiveTick = 0;

                    //Add a small delay before fading in again so the grab handle can potentially be hovered before this happens
                    if ((m_OverlayBarFadeInTick == 0) && (!m_IsSystemUIHoveredFromSwitch))
                    {
                        m_OverlayBarFadeInTick = ::GetTickCount64();
                    }

                    if (m_OverlayBarFadeInTick + 300 < ::GetTickCount64())
                    {
                        if (m_OvrlOverlayBarAlpha != 1.0f)
                        {
                            m_OvrlOverlayBarAlpha += ImGui::GetIO().DeltaTime * 12.0f;

                            if (m_OvrlOverlayBarAlpha > 1.0f)
                                m_OvrlOverlayBarAlpha = 1.0f;

                            vr::VROverlay()->SetOverlayAlpha(m_OvrlHandleOverlayBar, m_OvrlOverlayBarAlpha);
                            vr::VROverlay()->ShowOverlay(m_OvrlHandleOverlayBar);
                        }
                        else if (!is_systemui_hovered)
                        {
                            m_IsSystemUIHoveredFromSwitch = false;
                        }
                    }
                }
            }
        }
        else
        {
            if (m_OvrlVisible)
            {
                vr::VROverlay()->HideOverlay(m_OvrlHandleOverlayBar);
                m_WindowOverlayBar.HideMenus();
                UpdateOverlayDimming();

                m_OvrlVisible = false;
            }
        }
    }
    else if (m_OvrlVisible) //Dashboard overlay has gone missing, hide
    {
        vr::VROverlay()->HideOverlay(m_OvrlHandleOverlayBar);

        m_OvrlVisible = false;
    }
}

void UIManager::UpdateOverlayDrag()
{
    if (m_OverlayDragger.IsDragActive())
    {
        m_OverlayDragger.DragUpdate();

        vr::VROverlayHandle_t drag_overlay_handle = m_OverlayDragger.GetDragOverlayHandle();

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            FinishOverlayDrag();
            return;
        }

        ImGuiIO& io = ImGui::GetIO();

        //Wheel input (add distance & add width)
        float hwheel_abs = fabs(io.MouseWheelH);
        float ywheel_abs = fabs(io.MouseWheel);

        //Deadzone
        if ((hwheel_abs > 0.05f) || (ywheel_abs > 0.05f))
        {
            //Add distance as long as y-delta input is bigger
            if (hwheel_abs < ywheel_abs)
            {
                m_OverlayDragger.DragAddDistance(io.MouseWheel * 0.5f);
            }
            else
            {
                m_OverlayDragger.DragAddWidth(io.MouseWheelH * -0.15f);
            }
        }

        //Prevent widget input during active drag, except for the keyboard where this doesn't matter and would unfocus active InputTexts
        if (drag_overlay_handle != m_OvrlHandleKeyboard)
        {
            ImGui::BlockWidgetInput();
        }
    }
    else if (m_OverlayDragger.IsDragGestureActive())
    {
        m_OverlayDragger.DragGestureUpdate();

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            vr::VROverlayHandle_t drag_overlay_handle = m_OverlayDragger.GetDragOverlayHandle();

            Matrix4 matrix_relative_offset = m_OverlayDragger.DragGestureFinish();
            float new_width = 1.0f;
            vr::VROverlay()->GetOverlayWidthInMeters(drag_overlay_handle, &new_width);

            //Store changed transform to the previously dragged overlay handle and update width/size config value
            if (drag_overlay_handle == m_OvrlHandleSettings)
            {
                m_WindowSettings.SetTransform(matrix_relative_offset);
            }
            else if (drag_overlay_handle == m_OvrlHandleOverlayProperties)
            {
                m_WindowOverlayProperties.SetTransform(matrix_relative_offset);
            }
            else if (drag_overlay_handle == m_OvrlHandleKeyboard)
            {
                m_VRKeyboard.GetWindow().SetTransform(matrix_relative_offset);
                m_VRKeyboard.GetWindow().RebaseTransform();
            }
        }
        else
        {
            //Prevent widget input during active drag
            ImGui::BlockWidgetInput();
        }
    }
}

void UIManager::StartOverlayDrag(vr::VROverlayHandle_t overlay_handle)
{
    if (overlay_handle == m_OvrlHandleSettings)
    {
        m_WindowSettings.StartDrag();
    }
    else if (overlay_handle == m_OvrlHandleOverlayProperties)
    {
        m_WindowOverlayProperties.StartDrag();
    }
    else if (overlay_handle == m_OvrlHandleKeyboard)
    {
        m_VRKeyboard.GetWindow().StartDrag();
    }
}

void UIManager::FinishOverlayDrag()
{
    vr::VROverlayHandle_t drag_overlay_handle = m_OverlayDragger.GetDragOverlayHandle();
    Matrix4 matrix_relative_offset = m_OverlayDragger.DragFinish();

    //Store changed transform to the previously dragged overlay handle
    if (drag_overlay_handle == m_OvrlHandleSettings)
    {
        m_WindowSettings.SetTransform(matrix_relative_offset);
    }
    else if (drag_overlay_handle == m_OvrlHandleOverlayProperties)
    {
        m_WindowOverlayProperties.SetTransform(matrix_relative_offset);
    }
    else if (drag_overlay_handle == m_OvrlHandleKeyboard)
    {
        m_VRKeyboard.GetWindow().SetTransform(matrix_relative_offset);
        m_VRKeyboard.GetWindow().RebaseTransform();
    }
}

void UIManager::HighlightOverlay(unsigned int overlay_id)
{
    //Indicate the current overlay by tinting it when hovering the overlay selector
    if (m_OpenVRLoaded)
    {
        static vr::VROverlayHandle_t colored_handle = vr::k_ulOverlayHandleInvalid;

        vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().GetConfigData(overlay_id).ConfigHandle[configid_handle_overlay_state_overlay_handle];;

        //Tint overlay if no other overlay is currently tinted (adds one frame delay on switching but it doesn't really matter)
        if ( (ovrl_handle != vr::k_ulOverlayHandleInvalid) && (colored_handle == vr::k_ulOverlayHandleInvalid) )
        {
            const OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)overlay_id);
            float brightness = lin2log(data.ConfigFloat[configid_float_overlay_brightness]) * data.ConfigFloat[configid_float_overlay_state_brightness_extra_multiplier];

            ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);

            vr::VROverlay()->SetOverlayColor(ovrl_handle, col.x * brightness, col.y * brightness, col.z * brightness);

            colored_handle = ovrl_handle;
        }
        else if ( (colored_handle != vr::k_ulOverlayHandleInvalid) && (colored_handle != ovrl_handle) ) //Remove tint if overlay handle is different or vr::k_ulOverlayHandleInvalid
        {
            const OverlayConfigData& data = OverlayManager::Get().GetConfigData(OverlayManager::Get().FindOverlayID(colored_handle));
            float brightness = lin2log(data.ConfigFloat[configid_float_overlay_brightness]) * data.ConfigFloat[configid_float_overlay_state_brightness_extra_multiplier];

            vr::VROverlay()->SetOverlayColor(colored_handle, brightness, brightness, brightness);

            colored_handle = vr::k_ulOverlayHandleInvalid;
        }
    }
}

float UIManager::GetOverlayHeight(vr::VROverlayHandle_t overlay_handle) const
{
    //This only checks for overlays that are draggable by the UI
    float overlay_width = 1.0f;
    vr::VROverlay()->GetOverlayWidthInMeters(overlay_handle, &overlay_width);

    const UITexspaceID overlay_texspace = GetTexspaceIDForOverlayHandle(overlay_handle);
    if (overlay_texspace == ui_texspace_total)
    {
        return -1.0f;
    }

    const DPRect& rect_tex = UITextureSpaces::Get().GetRect(overlay_texspace);
    return ((float)rect_tex.GetHeight() / (float)rect_tex.GetWidth()) * overlay_width;
}

Matrix4 UIManager::GetOverlay2DPointTransform(Vector2 point_2d, vr::VROverlayHandle_t overlay_handle) const
{
    //GetTransformForOverlayCoordinates() appears to be pretty much just offset from the center point of the overlay (well, that is the 0-point for the transform position anyways)
    //It doesn't account for UV coordinates though, so it can be a bit messy to set up. That's why it's wrapped up for UI overlay usage
    //The 2D coordinates it expects are also in GL-space (y+ is up)
    const DPRect& rect_tex = UITextureSpaces::Get().GetRect( GetTexspaceIDForOverlayHandle(overlay_handle) );

    float tex_y = UITextureSpaces::Get().GetRect(ui_texspace_total).GetHeight() / 2.0f; //Middle of overlay (UV is unaccounted for, so half of total texture height)
    tex_y += rect_tex.GetHeight() / 2.0f;                                               //Offset from the middle however, so adding half of the texspace height gets us to the zero top point
    tex_y -= point_2d.y;                                                                //Then we can substract the passed coordinate (not adding as y+ is up)

    //Now we can get the coordinates as intended
    vr::HmdMatrix34_t hmd_mat = {};
    vr::VROverlay()->GetTransformForOverlayCoordinates(overlay_handle, vr::TrackingUniverseStanding, {rect_tex.GetTL().x + point_2d.x, tex_y}, &hmd_mat);

    //Returned matrix usually contains dashboard scale, so undo it
    Vector3 row_1(hmd_mat.m[0][0], hmd_mat.m[1][0], hmd_mat.m[2][0]);
    Matrix4 mat(hmd_mat);
    Vector3 pos = mat.getTranslation();
    mat.setTranslation({0.0f, 0.0f, 0.0f});
    mat.scale(1.0f / row_1.length());
    mat.setTranslation(pos);

    return mat;
}

void UIManager::TriggerLaserPointerHaptics(vr::VROverlayHandle_t overlay_handle, vr::TrackedDeviceIndex_t device_index) const
{
    if (!m_OpenVRLoaded)
        return;

    //Trigger directly when dashboard pointer is active as it's going to be the right device anyways
    if ( (device_index == vr::k_unTrackedDeviceIndexInvalid) && (vr::IVROverlayEx::IsSystemLaserPointerActive()) )
    {
        vr::VROverlay()->TriggerLaserMouseHapticVibration(overlay_handle, 0.0f, 1.0f, 0.16f);
    }
    else  //Let dashboard app handle it as we'll need VR Input access
    {
        //If no device specified, get the primary laser pointer one
        if (device_index == vr::k_unTrackedDeviceIndexInvalid)
        {
            device_index = ConfigManager::Get().GetPrimaryLaserPointerDevice();
        }

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_lpointer_trigger_haptics, device_index);
    }
}
