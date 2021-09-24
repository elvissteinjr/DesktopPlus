#include "UIManager.h"

#include <windowsx.h>

#include "imgui.h"
#include "imgui_impl_win32_openvr.h"

#include "InterprocessMessaging.h"
#include "ConfigManager.h"
#include "OverlayManager.h"
#include "Util.h"
#include "WindowManager.h"

//This one holds mostly constant data, but depends on how the application was launched
static UITextureSpaces g_UITextureSpaces;

UITextureSpaces& UITextureSpaces::Get()
{
    return g_UITextureSpaces;
}

void UITextureSpaces::Init(bool desktop_mode)
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
        //Desktop mode only initializes total texture space (as the unscaled window size), rest is testing for now as we have no proper desktop mode yet
        m_TexspaceRects[ui_texspace_total] = {0, 0, 1920, 1080};

        m_TexspaceRects[ui_texspace_overlay_properties] =  {0,
                                                            0,
                                                            959,
                                                            800};

        m_TexspaceRects[ui_texspace_settings] =            {m_TexspaceRects[ui_texspace_total].GetWidth() - m_TexspaceRects[ui_texspace_overlay_properties].GetWidth(),
                                                            0,
                                                            m_TexspaceRects[ui_texspace_total].GetWidth(),
                                                            800};

        m_TexspaceRects[ui_texspace_keyboard] =            {0,
                                                            250,
                                                            m_TexspaceRects[ui_texspace_total].GetWidth(), 
                                                            250 + 750};

        m_TexspaceRects[ui_texspace_aux_ui] =              {0, 
                                                            50,
                                                            m_TexspaceRects[ui_texspace_total].GetWidth() - (850 + 2), 
                                                            50 + 550};
    }
}

const DPRect& UITextureSpaces::GetRect(UITexspaceID texspace_id) const
{
    return m_TexspaceRects[texspace_id];
}

//While this is a singleton like many other classes, we want to be careful about initializing it at global scope, so we leave that until a bit later in main()
UIManager* g_UIManagerPtr = nullptr;

UIManager::UIManager(bool desktop_mode) : 
    m_WindowHandle(nullptr),
    m_SharedTextureRef(nullptr),
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

void UIManager::SetOverlayInputEnabled(bool is_enabled)
{
    vr::VROverlayInputMethod input_method = (is_enabled) ? vr::VROverlayInputMethod_Mouse : vr::VROverlayInputMethod_None;

    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleOverlayBar,       input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleFloatingUI,        input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleSettings,          input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleOverlayProperties, input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleKeyboard,          input_method);
    vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleAuxUI,             input_method);
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
        while (true)
        {
            ovrl_error = vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusUI", "Desktop+UI", &m_OvrlHandleOverlayBar);

            if (ovrl_error == vr::VROverlayError_KeyInUse)  //If the key is already in use, kill the owning process (hopefully another instance of this app)
            {
                ovrl_error = vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusUI", &m_OvrlHandleOverlayBar);

                if ((ovrl_error == vr::VROverlayError_None) && (m_OvrlHandleOverlayBar != vr::k_ulOverlayHandleInvalid))
                {
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
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleKeyboard,          OVERLAY_WIDTH_METERS_DASHBOARD_UI);

            vr::VROverlay()->SetOverlayAlpha(m_OvrlHandleFloatingUI, 0.0f);

            //Set input parameters
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleOverlayBar,       vr::VROverlayFlags_SendVRSmoothScrollEvents, true);
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleSettings,          vr::VROverlayFlags_SendVRSmoothScrollEvents, true);
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleOverlayProperties, vr::VROverlayFlags_SendVRSmoothScrollEvents, true);
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleKeyboard,          vr::VROverlayFlags_SendVRSmoothScrollEvents, true);
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
        }
    }

    //Cache systemui handle as it won't change during the session anyways
    vr::VROverlay()->FindOverlay("system.systemui", &m_OvrlHandleSystemUI);

    m_OpenVRLoaded = true;
    m_LowCompositorRes = (vr::VRSettings()->GetFloat("GpuSpeed", "gpuSpeedRenderTargetScale") < 1.0f);

    UpdateDesktopOverlayPixelSize();

    m_VRKeyboard.GetWindow().UpdateOverlaySize();
    m_VRKeyboard.GetWindow().ResetTransform();

    m_WindowPerformance.ResetCumulativeValues();
    m_WindowPerformance.RefreshTrackerBatteryList();

    //Check if it's a WMR system and set up for that if needed
    SetConfigForWMR(ConfigManager::Get().GetConfigIntRef(configid_int_interface_wmr_ignore_vscreens));

    if ((ovrl_error == vr::VROverlayError_None))
        return vr::VRInitError_None;
    else
        return vr::VRInitError_Compositor_OverlayInitFailed;
}

void UIManager::HandleIPCMessage(const MSG& msg, bool handle_delayed)
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
                case ipcact_overlays_reset:
                {
                    UpdateDesktopOverlayPixelSize();
                    m_WindowPerformance.ScheduleOverlaySharedTextureUpdate();
                    break;
                }
                case ipcact_vrkeyboard_closed:
                {
                    ImGui_ImplOpenVR_InputOnVRKeyboardClosed();
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

                    OverlayManager::Get().AddOverlay(capsource, desktop_id, (HWND)ConfigManager::Get().GetConfigHandle(configid_handle_state_arg_hwnd));
                    break;
                }
                case ipcact_overlay_remove:
                {
                    unsigned int overlay_id = (unsigned int)msg.lParam;

                    //Hide properties window if it's open for this overlay
                    if (m_WindowOverlayProperties.GetActiveOverlayID() == overlay_id)
                    {
                        m_WindowOverlayProperties.Hide();
                        m_WindowOverlayProperties.SetActiveOverlayID(k_ulOverlayID_None);
                    }

                    OverlayManager::Get().RemoveOverlay(overlay_id);

                    m_WindowOverlayBar.HideMenus();
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

                                    OverlayManager::Get().SetOverlayNameAuto(i, (HWND)msg.lParam);

                                    //Update overlay properties title if this is the current overlay
                                    if (UIManager::Get()->GetOverlayPropertiesWindow().GetActiveOverlayID() == i)
                                    {
                                        UIManager::Get()->GetOverlayPropertiesWindow().SetActiveOverlayID(i, true);
                                    }
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
                    (msg.lParam) ? m_VRKeyboard.GetWindow().Show() : m_VRKeyboard.GetWindow().Hide();
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

                switch (bool_id)
                {
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
                ConfigManager::Get().SetConfigInt(int_id, (int)msg.lParam);

                switch (int_id)
                {
                    case configid_int_overlay_winrt_desktop_id:
                    {
                        OverlayManager::Get().SetCurrentOverlayNameAuto();
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
                            vr::ETrackedControllerRole role = (msg.lParam % 2 == 0) ? vr::TrackedControllerRole_RightHand : vr::TrackedControllerRole_LeftHand;

                            m_AuxUI.GetDragHintWindow().SetHintType(role, (msg.lParam <= 2));
                            m_AuxUI.GetDragHintWindow().Show();
                        }

                        break;
                    }
                    default: break;
                }
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX + configid_float_MAX)
            {
                ConfigID_Float float_id = (ConfigID_Float)(msg.wParam - configid_bool_MAX - configid_int_MAX);
                float value = *(float*)&msg.lParam; //Interpret lParam as a float variable
                ConfigManager::Get().SetConfigFloat(float_id, value);

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
                            OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)ConfigManager::Get().GetConfigInt(configid_int_state_overlay_transform_sync_target_id));
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
                uint64_t value = *(uint64_t*)&msg.lParam; //Interpret lParam as a uint64_t variable
                ConfigManager::Get().SetConfigHandle(handle_id, value);

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
                            ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_title,      StringConvertFromUTF16(window_info->GetTitle().c_str()));
                            ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_class_name, StringConvertFromUTF16(window_info->GetWindowClassName().c_str()));
                            ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_exe_name,   window_info->GetExeName());
                        }
                        else if (value == 0) //Only clear if HWND is really null
                        {
                            ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_title, "");
                            ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_class_name, "");
                            ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_exe_name, "");
                            ConfigManager::Get().SetConfigHandle(configid_handle_overlay_state_winrt_last_hicon, 0);
                        }

                        OverlayManager::Get().SetCurrentOverlayNameAuto();
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

void UIManager::HandleDelayedIPCMessages()
{
    while (!m_DelayedICPMessages.empty())
    {
        HandleIPCMessage(m_DelayedICPMessages.back(), true);
        m_DelayedICPMessages.pop_back();
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

    if (m_ComInitDone)
    {
        ::CoUninitialize();
    }

    WindowManager::Get().SetActive(false);
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

WindowSettingsNew& UIManager::GetSettingsWindow()
{
    return m_WindowSettingsNew;
}

WindowOverlayProperties& UIManager::GetOverlayPropertiesWindow()
{
    return m_WindowOverlayProperties;
}

WindowPerformance& UIManager::GetPerformanceWindow()
{
    return m_WindowPerformance;
}

void UIManager::SetWindowHandle(HWND handle)
{
    m_WindowHandle = handle;
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
    return m_SharedTextureRef;
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

        if (ConfigManager::Get().GetConfigBool(configid_bool_state_misc_uiaccess_enabled)) //UIAccess enabled executable doesn't run straight from CreateProcess()
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

void UIManager::UpdateOverlayDimming()
{
    if (ConfigManager::Get().GetConfigBool(configid_bool_interface_dim_ui))
    {
        vr::VROverlay()->SetOverlayColor(m_OvrlHandleOverlayBar, 0.05f, 0.05f, 0.05f);
        ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f; //Set window bg alpha to 100% to not have the contrast be even worse on light backgrounds
    }
    else
    {
        vr::VROverlay()->SetOverlayColor(m_OvrlHandleOverlayBar, 1.0f, 1.0f, 1.0f);
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
        int& desktop_id = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_desktop_id);

        if (desktop_id >= ConfigManager::Get().GetConfigInt(configid_int_state_interface_desktop_count))
            desktop_id = -1;
        else if (desktop_id == -2)  //-2 tell the dashboard application to crop it to desktop 0 and the value changes afterwards, though that doesn't happen when it's not running
            desktop_id = 0;

        if ( (desktop_id == -1) || (!ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring)) )   //All desktops, get virtual screen dimensions
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

void UIManager::PositionOverlay()
{
    vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &m_OvrlHandleDPlusDashboard);

    if (m_OvrlHandleDPlusDashboard != vr::k_ulOverlayHandleInvalid)
    {
        //Imagine if SetOverlayTransformOverlayRelative() actually worked
        vr::HmdMatrix34_t matrix_ovr;
        vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;

        vr::VROverlay()->GetTransformForOverlayCoordinates(m_OvrlHandleDPlusDashboard, origin, { 0.5f, 0.0f }, &matrix_ovr);

        //Adjust curve for dashboard position
        float curve = 0.145f;
        int32_t dashboard_pos = vr::VRSettings()->GetInt32(vr::k_pch_Dashboard_Section, "position_2");

        switch (dashboard_pos)
        {
            case 0: curve = 0.17f; break; //Near
            case 1: curve = 0.16f; break; //Middle
            case 2: curve = 0.15f; break; //Far
        }

        //Offset the overlay
        //It's offset in so it's as close to the dashboard as possible while not messing up pointer input. Most problematic dashboard element is the current application button.
        if (m_WindowOverlayBar.IsScrollBarVisible())
        {
            TransformOpenVR34TranslateRelative(matrix_ovr, 0.0f, -0.272f, 0.195f);
        }
        else
        {
            TransformOpenVR34TranslateRelative(matrix_ovr, 0.0f, -0.225f, 0.185f);
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

        m_IsDummyOverlayTransformUnstable = ((!anti_flicker_can_move) || (GetRepeatFrame()) || (ImGui::IsMouseDown(ImGuiMouseButton_Left))); //Also block transform updates when left mouse is held down

        if (!m_IsDummyOverlayTransformUnstable)
        {
            vr::VROverlay()->SetOverlayTransformAbsolute(m_OvrlHandleOverlayBar, origin, &matrix_ovr);
            vr::VROverlay()->SetOverlayCurvature(m_OvrlHandleOverlayBar, curve);
        }

        //Set visibility
        if (vr::VROverlay()->IsOverlayVisible(m_OvrlHandleDPlusDashboard))
        {
            bool is_systemui_hovered = vr::VROverlay()->IsHoverTargetOverlay(m_OvrlHandleSystemUI);

            if (!m_OvrlVisible)
            {
                vr::VROverlay()->ShowOverlay(m_OvrlHandleOverlayBar);
                m_OvrlVisible = true;

                //We prevent the fade-out when Overlay Bar is newly visible while the dashboard SystemUI is being hovered until ithe pointer leaves that overlay at least once
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
                                m_OvrlOverlayBarAlpha -= 0.2f;

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

                    if (m_OvrlOverlayBarAlpha != 1.0f)
                    {
                        m_OvrlOverlayBarAlpha += 0.2f;

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
        else
        {
            if (m_OvrlVisible)
            {
                vr::VROverlay()->HideOverlay(m_OvrlHandleOverlayBar);
                m_WindowOverlayBar.HideMenus();

                m_OvrlVisible = false;
            }
        }
    }
    else if (m_OvrlVisible) //Dashboard overlay has gone missing, hide
    {
        vr::VROverlay()->HideOverlay(m_OvrlHandleOverlayBar);

        m_OvrlVisible = false;
    }

    UpdateOverlayDrag();
}

void UIManager::UpdateOverlayDrag()
{
    if (m_OverlayDragger.IsDragActive())
    {
        m_OverlayDragger.DragUpdate();

        vr::VROverlayHandle_t drag_overlay_handle = m_OverlayDragger.GetDragOverlayHandle();

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            Matrix4 matrix_relative_offset = m_OverlayDragger.DragFinish();

            //Store changed transform to the previously dragged overlay handle
            if (drag_overlay_handle == m_OvrlHandleSettings)
            {
                m_WindowSettingsNew.SetTransform(matrix_relative_offset);
            }
            else if (drag_overlay_handle == m_OvrlHandleOverlayProperties)
            {
                m_WindowOverlayProperties.SetTransform(matrix_relative_offset);
            }
            else if (drag_overlay_handle == m_OvrlHandleKeyboard)
            {
                m_VRKeyboard.GetWindow().SetTransform(matrix_relative_offset);
            }

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
                m_OverlayDragger.DragAddDistance(io.MouseWheel);
            }
            else
            {
                float new_width = m_OverlayDragger.DragAddWidth(io.MouseWheelH * -0.25f);

                //Adjust persistent width settings after changing width
                if (drag_overlay_handle == m_OvrlHandleKeyboard)
                {
                     ConfigManager::Get().SetConfigFloat(configid_float_input_keyboard_detached_size, new_width / OVERLAY_WIDTH_METERS_KEYBOARD);
                }
            }
        }

        //Invalidate mouse pos so it can't trigger any hover effects on fast movements
        io.MousePos.x = -FLT_MAX;
        io.MousePos.y = -FLT_MAX;
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
                m_WindowSettingsNew.SetTransform(matrix_relative_offset);
            }
            else if (drag_overlay_handle == m_OvrlHandleOverlayProperties)
            {
                m_WindowOverlayProperties.SetTransform(matrix_relative_offset);
            }
            else if (drag_overlay_handle == m_OvrlHandleKeyboard)
            {
                m_VRKeyboard.GetWindow().SetTransform(matrix_relative_offset);
                ConfigManager::Get().SetConfigFloat(configid_float_input_keyboard_detached_size, new_width / OVERLAY_WIDTH_METERS_KEYBOARD);
            }
        }
        else
        {
            ImGuiIO& io = ImGui::GetIO();

            //Invalidate mouse pos so it can't trigger any hover effects on fast movements
            io.MousePos.x = -FLT_MAX;
            io.MousePos.y = -FLT_MAX;
        }
    }
}

void UIManager::HighlightOverlay(unsigned int overlay_id)
{
    //Indicate the current overlay by tinting it when hovering the overlay selector
    if (m_OpenVRLoaded)
    {
        static unsigned int colored_id = k_ulOverlayID_None;

        //Tint overlay if no other overlay is currently tinted (adds one frame delay on switching but it doesn't really matter)
        if ( (overlay_id != k_ulOverlayID_None) && (colored_id == k_ulOverlayID_None) )
        {
            vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().FindOverlayHandle(overlay_id);

            if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
            {
                ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
                vr::VROverlay()->SetOverlayColor(ovrl_handle, col.x, col.y, col.z);

                colored_id = overlay_id;
            }
        }
        else if ( (colored_id != k_ulOverlayID_None) && (colored_id != overlay_id) ) //Remove tint if overlay id is different or k_ulOverlayID_None
        {
            vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().FindOverlayHandle(colored_id);

            if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
            {
                vr::VROverlay()->SetOverlayColor(ovrl_handle, 1.0f, 1.0f, 1.0f);
            }

            colored_id = k_ulOverlayID_None;
        }
    }
}
