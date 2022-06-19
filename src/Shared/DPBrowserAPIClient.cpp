#include "DPBrowserAPIClient.h"

#include "ConfigManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"

#ifdef DPLUS_UI
    #include "UIManager.h"
    #include "OverlayManager.h"
#else
    #include "OutputManager.h"
#endif

static DPBrowserAPIClient g_DPBrowserAPIClient;

bool DPBrowserAPIClient::LaunchServerIfNotRunning()
{
    //This is not going to work with the executable missing or previously discovered API mismatch
    if ( (!m_IsServerAvailable) || (m_HasServerAPIMismatch) )
        return false;

    //Check if it's already running and update cached handle just in case
    m_ServerWindowHandle = ::FindWindow(g_WindowClassNameBrowserApp, nullptr);

    if (m_ServerWindowHandle != nullptr)
        return true;

    //Prepare command-line
    std::wstring browser_args_wstr = L"--DPBrowserServer ";
    browser_args_wstr += WStringConvertFromUTF8(ConfigManager::Get().GetValue(configid_str_browser_extra_arguments).c_str());

    auto browser_arg_buffer = std::unique_ptr<WCHAR[]>{new WCHAR[browser_args_wstr.size() + 1]};        //CreateProcess() requires a modifiable buffer for the command-line
    size_t copied_length = browser_args_wstr.copy(browser_arg_buffer.get(), browser_args_wstr.size());
    browser_arg_buffer[copied_length] = '\0';

    //Launch the process
    std::wstring browser_working_dir = WStringConvertFromUTF8( (ConfigManager::Get().GetApplicationPath() + g_RelativeWorkingDirBrowserApp).c_str() );
    std::wstring browser_exe_path    = browser_working_dir + L"/" + WStringConvertFromUTF8(g_ExeNameBrowserApp);

    #ifndef DPLUS_UI
    //If the process is elevated, do *not* launch a web browser with admin privileges. We're not insane.
    if (ConfigManager::Get().GetValue(configid_bool_state_misc_process_elevated))
    {
        //Only the dashboard app can be elevated in normal operation, so it's restricted to that
        if (OutputManager* outmgr = OutputManager::Get())
        {
            outmgr->InitComIfNeeded();

            //Launch browser process unelevated if possible. May fail under certain circumstances, but we won't fall back to normal launch
            if (!ShellExecuteUnelevated(browser_exe_path.c_str(), browser_arg_buffer.get(), browser_working_dir.c_str()))
            {
                return false;
            }
        }
    }
    else
    #endif
    {
        STARTUPINFO si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);

        if (::CreateProcess(browser_exe_path.c_str(), browser_arg_buffer.get(), nullptr, nullptr, FALSE, 0, nullptr, browser_working_dir.c_str(), &si, &pi) == 0)
        {
            //Don't try waiting if creating failed somehow
            ::CloseHandle(pi.hProcess);
            ::CloseHandle(pi.hThread);

            return false;
        }

        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
    }

    //Wait for it to be ready
    ULONGLONG start_tick = ::GetTickCount64();

    while ( (m_ServerWindowHandle = ::FindWindow(g_WindowClassNameBrowserApp, nullptr), m_ServerWindowHandle == nullptr) &&
            (::GetTickCount64() - start_tick < 10000) )                                                                     //Wait 10 seconds max (should usually be faster though)
    {
        #ifndef DPLUS_UI
            //Call busy update while waiting to appear a bit more responsive. Not ideal, but beats running in parallel and having to queue up all function calls when busy
            if (OutputManager* outmgr = OutputManager::Get())
            {
                outmgr->BusyUpdate();
                ::Sleep(outmgr->GetMaxRefreshDelay());
            }
        #else
            ::Sleep(50);
        #endif
    }

    //Check if the API versions match
    if ( (m_ServerWindowHandle != nullptr) && (::SendMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_get_api_version, 0) != k_lDPBrowserAPIVersion) )
    {
        m_HasServerAPIMismatch = true;
        m_ServerWindowHandle = nullptr;

        //Post config state to UI to allow displaying a warning since the UI doesn't try to launch the browser process on its own in most cases
        IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_misc_browser_version_mismatch, true);

        return false;
    }

    //Apply pending settings, if there are any
    if (m_PendingSettingGlobalFPS != -1)
    {
        DPBrowser_GlobalSetFPS(m_PendingSettingGlobalFPS);
        m_PendingSettingGlobalFPS = -1;
    }

    return true;
}

void DPBrowserAPIClient::SendStringMessage(DPBrowserICPStringID str_id, const std::string& str) const
{
    HWND source_window = nullptr;
    #ifdef DPLUS_UI
        if (UIManager* uimgr = UIManager::Get())
        {
            source_window = uimgr->GetWindowHandle();
        }
    #else
        if (OutputManager* outmgr = OutputManager::Get())
        {
            source_window = OutputManager::Get()->GetWindowHandle();
        }
    #endif

    if (m_ServerWindowHandle != nullptr)
    {
        COPYDATASTRUCT cds = {0};
        cds.dwData = str_id;
        cds.cbData = (DWORD)str.length();  //We do not include the NUL byte
        cds.lpData = (void*)str.c_str();

        ::SendMessage(m_ServerWindowHandle, WM_COPYDATA, (WPARAM)source_window, (LPARAM)(LPVOID)&cds);
    }
}

std::string& DPBrowserAPIClient::GetIPCString(DPBrowserICPStringID str_id)
{
    return m_IPCStrings[str_id - dpbrowser_ipcstr_MIN];
}

DPBrowserAPIClient& DPBrowserAPIClient::Get()
{
    return g_DPBrowserAPIClient;
}

bool DPBrowserAPIClient::Init()
{
    //Check if Desktop+Browser server executable is available
    std::string browser_exe_path = ConfigManager::Get().GetApplicationPath() + g_RelativeWorkingDirBrowserApp + "/" + g_ExeNameBrowserApp;
    m_IsServerAvailable = FileExists(WStringConvertFromUTF8(browser_exe_path.c_str()).c_str());

    //Register custom message ID
    m_Win32MessageID = ::RegisterWindowMessage(g_WindowMessageNameBrowserApp);

    return true;
}

void DPBrowserAPIClient::Quit()
{
    //We're not going to launch the server for this, but passing nullptr will send quit to ourselves, so don't do that
    if (m_ServerWindowHandle == nullptr)
        return;

    //Ask browser process to quit cleanly
    ::PostMessage(m_ServerWindowHandle, WM_QUIT, 0, 0);

    //Reset some variables, though this usually is just called during Desktop+ shutdown
    m_HasServerAPIMismatch = false;
    m_ServerWindowHandle   = nullptr;
}

bool DPBrowserAPIClient::IsBrowserAvailable() const
{
    return m_IsServerAvailable;
}

DWORD DPBrowserAPIClient::GetServerAppProcessID()
{
    if (!LaunchServerIfNotRunning())
        return 0;

    DWORD pid = 0;
    ::GetWindowThreadProcessId(m_ServerWindowHandle, &pid);

    return pid;
}

UINT DPBrowserAPIClient::GetRegisteredMessageID() const
{
    return m_Win32MessageID;
}

void DPBrowserAPIClient::HandleIPCMessage(const MSG& msg)
{
    if (msg.message == WM_COPYDATA)
    {
        COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)msg.lParam;

        //Arbitrary size limit to prevent some malicous applications from sending bad data
        if ( (pcds->dwData >= dpbrowser_ipcstr_MIN) && (pcds->dwData < dpbrowser_ipcstr_MAX) && (pcds->cbData <= 4096) ) 
        {
            std::string copystr((char*)pcds->lpData, pcds->cbData); //We rely on the data length. The data is sent without the NUL byte

            DPBrowserICPStringID str_id = (DPBrowserICPStringID)pcds->dwData;
            GetIPCString(str_id) = copystr;
        }

        return;
    }
    else if (msg.message != m_Win32MessageID)
    {
        return;
    }

    switch (msg.wParam)
    {
        case dpbrowser_ipccmd_set_overlay_target:
        {
            m_IPCOverlayTarget = msg.lParam;
            break;
        }
        case dpbrowser_ipccmd_notify_nav_state:
        {
            unsigned int overlay_id = OverlayManager::Get().FindOverlayID(m_IPCOverlayTarget);

            if (overlay_id != k_ulOverlayID_None)
            {
                OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
                data.ConfigBool[configid_bool_overlay_state_browser_nav_can_go_back]    = (msg.lParam & dpbrowser_ipcnavstate_flag_can_go_back);
                data.ConfigBool[configid_bool_overlay_state_browser_nav_can_go_forward] = (msg.lParam & dpbrowser_ipcnavstate_flag_can_go_forward);
                data.ConfigBool[configid_bool_overlay_state_browser_nav_is_loading]     = (msg.lParam & dpbrowser_ipcnavstate_flag_is_loading);
            }

            m_IPCOverlayTarget = vr::k_ulOverlayHandleInvalid;
            break;
        }
        case dpbrowser_ipccmd_notify_url_changed:
        {
            unsigned int overlay_id = OverlayManager::Get().FindOverlayID(msg.lParam);

            if (overlay_id != k_ulOverlayID_None)
            {
                OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
                data.ConfigStr[configid_str_overlay_browser_url] = GetIPCString(dpbrowser_ipcstr_url);

                #ifdef DPLUS_UI
                    if (UIManager* uimgr = UIManager::Get())
                    {
                        uimgr->GetOverlayPropertiesWindow().MarkBrowserURLChanged();
                    }
                #endif
            }
            break;
        }
        case dpbrowser_ipccmd_notify_title_changed:
        {
            unsigned int overlay_id = OverlayManager::Get().FindOverlayID(msg.lParam);

            if (overlay_id != k_ulOverlayID_None)
            {
                OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
                data.ConfigStr[configid_str_overlay_browser_title] = GetIPCString(dpbrowser_ipcstr_title);

                #ifdef DPLUS_UI
                    //Update auto name for the overlay and any using it as a duplication source
                    OverlayManager::Get().SetOverlayNameAuto(overlay_id);

                    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                    {
                        const OverlayConfigData& dup_data = OverlayManager::Get().GetConfigData(i);

                        if ( (dup_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser) && (dup_data.ConfigInt[configid_int_overlay_duplication_id] == (int)overlay_id) )
                        {
                            OverlayManager::Get().SetOverlayNameAuto(i);
                        }
                    }


                    if (UIManager* uimgr = UIManager::Get())
                    {
                        uimgr->OnOverlayNameChanged();

                        if (ImGui::StringContainsUnmappedCharacter(data.ConfigStr[configid_str_overlay_browser_title].c_str()))
                        {
                            TextureManager::Get().ReloadAllTexturesLater();
                            uimgr->RepeatFrame();
                        }
                    }
                #endif
            }
            break;
        }
        case dpbrowser_ipccmd_notify_fps:
        {
            unsigned int overlay_id = OverlayManager::Get().FindOverlayID(m_IPCOverlayTarget);

            if (overlay_id != k_ulOverlayID_None)
            {
                OverlayManager::Get().GetConfigData(overlay_id).ConfigInt[configid_int_overlay_state_fps] = (int)msg.lParam;
            }
            break;
        }
        case dpbrowser_ipccmd_notify_lpointer_haptics:
        {
            //Forward to OutputManager as window message
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_lpointer_trigger_haptics, (vr::TrackedDeviceIndex_t)ConfigManager::GetValue(configid_int_state_dplus_laser_pointer_device));
            break;
        }
        case dpbrowser_ipccmd_notify_keyboard_show:
        {
            unsigned int overlay_id = OverlayManager::Get().FindOverlayID(m_IPCOverlayTarget);

            if ( (overlay_id != k_ulOverlayID_None) && (ConfigManager::GetValue(configid_bool_input_keyboard_auto_show_browser)) )
            {
                #ifdef DPLUS_UI
                    if (UIManager* uimgr = UIManager::Get())
                    {
                        uimgr->GetVRKeyboard().GetWindow().SetAutoVisibility(overlay_id, msg.lParam);
                    }
                #endif
            }
            break;
        }
    }
}

void DPBrowserAPIClient::DPBrowser_StartBrowser(vr::VROverlayHandle_t overlay_handle, const std::string& url, bool use_transparent_background)
{
    if (!LaunchServerIfNotRunning())
        return;

    SendStringMessage(dpbrowser_ipcstr_url, url);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_start_browser, use_transparent_background);
}

void DPBrowserAPIClient::DPBrowser_DuplicateBrowserOutput(vr::VROverlayHandle_t overlay_handle_src, vr::VROverlayHandle_t overlay_handle_dst)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle_src);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_duplicate_browser_output, overlay_handle_dst);
}

void DPBrowserAPIClient::DPBrowser_PauseBrowser(vr::VROverlayHandle_t overlay_handle, bool pause)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_pause_browser, pause);
}

void DPBrowserAPIClient::DPBrowser_RecreateBrowser(vr::VROverlayHandle_t overlay_handle, bool use_transparent_background)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_recreate_browser, use_transparent_background);
}

void DPBrowserAPIClient::DPBrowser_StopBrowser(vr::VROverlayHandle_t overlay_handle)
{
    if (m_ServerWindowHandle == nullptr)
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_stop_browser, overlay_handle);
}

void DPBrowserAPIClient::DPBrowser_SetURL(vr::VROverlayHandle_t overlay_handle, const std::string& url)
{
    if (!LaunchServerIfNotRunning())
        return;

    SendStringMessage(dpbrowser_ipcstr_url, url);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_url, overlay_handle);
}

void DPBrowserAPIClient::DPBrowser_SetResolution(vr::VROverlayHandle_t overlay_handle, int width, int height)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_resoution, MAKELPARAM(width, height));
}

void DPBrowserAPIClient::DPBrowser_SetFPS(vr::VROverlayHandle_t overlay_handle, int fps)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_fps, fps);
}

void DPBrowserAPIClient::DPBrowser_SetZoomLevel(vr::VROverlayHandle_t overlay_handle, float zoom_level)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_zoom, pun_cast<LPARAM, float>(zoom_level));
}

void DPBrowserAPIClient::DPBrowser_MouseMove(vr::VROverlayHandle_t overlay_handle, int x, int y)
{
    if (m_ServerWindowHandle == nullptr)
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_mouse_move, MAKELPARAM(x, y));
}

void DPBrowserAPIClient::DPBrowser_MouseLeave(vr::VROverlayHandle_t overlay_handle)
{
    if (m_ServerWindowHandle == nullptr)
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_mouse_leave, overlay_handle);
}

void DPBrowserAPIClient::DPBrowser_MouseDown(vr::VROverlayHandle_t overlay_handle, vr::EVRMouseButton button)
{
    if (m_ServerWindowHandle == nullptr)
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_mouse_down, button);
}

void DPBrowserAPIClient::DPBrowser_MouseUp(vr::VROverlayHandle_t overlay_handle, vr::EVRMouseButton button)
{
    if (m_ServerWindowHandle == nullptr)
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_mouse_up, button);
}

void DPBrowserAPIClient::DPBrowser_Scroll(vr::VROverlayHandle_t overlay_handle, float x_delta, float y_delta)
{
    if (m_ServerWindowHandle == nullptr)
        return;

    //Squeeze the floats into DWORDs so they'll survive MAKEQWORD
    DWORD x_delta_uint = pun_cast<DWORD, float>(x_delta);
    DWORD y_delta_uint = pun_cast<DWORD, float>(y_delta);

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_scroll, MAKEQWORD(x_delta_uint, y_delta_uint));
}

void DPBrowserAPIClient::DPBrowser_KeyboardSetKeyState(vr::VROverlayHandle_t overlay_handle, DPBrowserIPCKeyboardKeystateFlags flags, unsigned char keycode)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_keyboard_vkey, MAKELPARAM(flags, keycode));
}

void DPBrowserAPIClient::DPBrowser_KeyboardToggleKey(vr::VROverlayHandle_t overlay_handle, unsigned char keycode)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_keyboard_vkey_toggle, keycode);
}

void DPBrowserAPIClient::DPBrowser_KeyboardTypeWChar(vr::VROverlayHandle_t overlay_handle, wchar_t wchar, bool down)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_set_overlay_target, overlay_handle);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_keyboard_wchar, MAKELPARAM(wchar, down));
}

void DPBrowserAPIClient::DPBrowser_KeyboardTypeString(vr::VROverlayHandle_t overlay_handle, const std::string& str)
{
    SendStringMessage(dpbrowser_ipcstr_keyboard_string, str);
    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_keyboard_string, overlay_handle);
}

void DPBrowserAPIClient::DPBrowser_GoBack(vr::VROverlayHandle_t overlay_handle)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_go_back, overlay_handle);
}

void DPBrowserAPIClient::DPBrowser_GoForward(vr::VROverlayHandle_t overlay_handle)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_go_forward, overlay_handle);
}

void DPBrowserAPIClient::DPBrowser_Refresh(vr::VROverlayHandle_t overlay_handle)
{
    if (!LaunchServerIfNotRunning())
        return;

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_refresh, overlay_handle);
}

void DPBrowserAPIClient::DPBrowser_GlobalSetFPS(int fps)
{
    if (m_ServerWindowHandle == nullptr)
    {
        m_PendingSettingGlobalFPS = fps;    //Will be applied after launching the server instead
        return;
    }

    ::PostMessage(m_ServerWindowHandle, m_Win32MessageID, dpbrowser_ipccmd_global_set_fps, fps);
}
