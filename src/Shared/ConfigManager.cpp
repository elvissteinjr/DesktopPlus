#include "ConfigManager.h"

#include <algorithm>
#include <sstream>
#include <fstream>

#include "Util.h"
#include "OverlayManager.h"
#include "InterprocessMessaging.h"
#include "WindowManager.h"
#include "DesktopPlusWinRT.h"

#ifdef DPLUS_UI
    #include "UIManager.h"
    #include "TranslationManager.h"
#else
    #include "WindowManager.h"
#endif

static ConfigManager g_ConfigManager;
static const std::string g_EmptyString;       //This way we can still return a const reference. Worth it? iunno

OverlayConfigData::OverlayConfigData()
{
    std::fill(std::begin(ConfigBool),   std::end(ConfigBool),   false);
    std::fill(std::begin(ConfigInt),    std::end(ConfigInt),    -1);
    std::fill(std::begin(ConfigFloat),  std::end(ConfigFloat),  0.0f);
    std::fill(std::begin(ConfigHandle), std::end(ConfigHandle), 0);

    //Default the transform matrix to zero as an indicator to reset them when possible later
    float matrix_zero[16] = { 0.0f };
    ConfigTransform = matrix_zero;
}

ConfigManager::ConfigManager() : m_IsSteamInstall(false)
{
    std::fill(std::begin(m_ConfigBool),  std::end(m_ConfigBool),  false);
    std::fill(std::begin(m_ConfigInt),   std::end(m_ConfigInt),   -1);
    std::fill(std::begin(m_ConfigFloat), std::end(m_ConfigFloat), 0.0f);
    //We don't need to initialize m_ConfigString

    //Init desktop count to the system metric, which already correct for most users
    m_ConfigInt[configid_int_state_interface_desktop_count] = ::GetSystemMetrics(SM_CMONITORS);

    //Init laser pointer hint to HMD (not controllers since they could be disconnected)
    m_ConfigInt[configid_int_state_laser_pointer_device_hint]  = vr::k_unTrackedDeviceIndex_Hmd;
    m_ConfigInt[configid_int_state_dplus_laser_pointer_device] = vr::k_unTrackedDeviceIndexInvalid;

    //Init application path
    int buffer_size = 1024;
    DWORD read_length;
    WCHAR* buffer;

    while (true)
    {
        buffer = new WCHAR[buffer_size];

        read_length = ::GetModuleFileName(nullptr, buffer, buffer_size);

        if ( (read_length == buffer_size) && (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) )
        {
            delete[] buffer;
            buffer_size += 1024;
        }
        else
        {
            break;
        }
    }

    if (::GetLastError() == ERROR_SUCCESS)
    {
        std::string path_str = StringConvertFromUTF16(buffer);

        //We got the full executable path, so let's get the folder part
        std::size_t pos = path_str.find_last_of("\\");
        m_ApplicationPath = path_str.substr(0, pos + 1);	//Includes trailing backslash
        m_ExecutableName  = path_str.substr(pos + 1, std::string::npos);

        //Somewhat naive way to check if this install is from Steam without using Steam API or shipping different binaries
        //Convert to lower first since there can be capitalization differences for the Steam directories
        std::wstring path_wstr = buffer;
        ::CharLowerBuff(buffer, (DWORD)path_wstr.length());
        path_wstr = buffer;

        m_IsSteamInstall = (path_wstr.find(L"\\steamapps\\common\\desktopplus\\desktopplus") != std::wstring::npos); 
    }

    delete[] buffer;

    //Check if UIAccess is enabled
    m_ConfigBool[configid_bool_state_misc_uiaccess_enabled] = IsUIAccessEnabled();
}

ConfigManager& ConfigManager::Get()
{
    return g_ConfigManager;
}

void ConfigManager::LoadOverlayProfile(const Ini& config, unsigned int overlay_id)
{
    OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
    unsigned int current_id = OverlayManager::Get().GetCurrentOverlayID();

    std::string section;

    if (overlay_id != UINT_MAX)
    {
        std::stringstream ss;
        ss << "Overlay" << overlay_id;

        section = ss.str();
    }
    else
    {
        section = "Overlay";
    }

    data.ConfigNameStr = config.ReadString(section.c_str(), "Name");

    data.ConfigBool[configid_bool_overlay_name_custom]                 = config.ReadBool(section.c_str(),   "NameIsCustom", false);
    data.ConfigBool[configid_bool_overlay_enabled]                     = config.ReadBool(section.c_str(),   "Enabled", true);
    data.ConfigInt[configid_int_overlay_desktop_id]                    = config.ReadInt(section.c_str(),    "DesktopID", 0);
    data.ConfigInt[configid_int_overlay_capture_source]                = config.ReadInt(section.c_str(),    "CaptureSource", ovrl_capsource_desktop_duplication);
    data.ConfigInt[configid_int_overlay_winrt_desktop_id]              = config.ReadInt(section.c_str(),    "WinRTDesktopID", -2);
    data.ConfigStr[configid_str_overlay_winrt_last_window_title]       = config.ReadString(section.c_str(), "WinRTLastWindowTitle");
    data.ConfigStr[configid_str_overlay_winrt_last_window_class_name]  = config.ReadString(section.c_str(), "WinRTLastWindowClassName");
    data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name]    = config.ReadString(section.c_str(), "WinRTLastWindowExeName");
    data.ConfigFloat[configid_float_overlay_width]                     = config.ReadInt(section.c_str(),    "Width", 165) / 100.0f;
    data.ConfigFloat[configid_float_overlay_curvature]                 = config.ReadInt(section.c_str(),    "Curvature", 17) / 100.0f;
    data.ConfigFloat[configid_float_overlay_opacity]                   = config.ReadInt(section.c_str(),    "Opacity", 100) / 100.0f;
    data.ConfigFloat[configid_float_overlay_offset_right]              = config.ReadInt(section.c_str(),    "OffsetRight", 0) / 100.0f;
    data.ConfigFloat[configid_float_overlay_offset_up]                 = config.ReadInt(section.c_str(),    "OffsetUp", 0) / 100.0f;
    data.ConfigFloat[configid_float_overlay_offset_forward]            = config.ReadInt(section.c_str(),    "OffsetForward", 0) / 100.0f;
    data.ConfigInt[configid_int_overlay_display_mode]                  = config.ReadInt(section.c_str(),    "DisplayMode", ovrl_dispmode_always);
    data.ConfigInt[configid_int_overlay_origin]                        = config.ReadInt(section.c_str(),    "Origin", ovrl_origin_room);

    data.ConfigBool[configid_bool_overlay_crop_enabled]                = config.ReadBool(section.c_str(), "CroppingEnabled", false);
    data.ConfigInt[configid_int_overlay_crop_x]                        = config.ReadInt(section.c_str(),  "CroppingX", 0);
    data.ConfigInt[configid_int_overlay_crop_y]                        = config.ReadInt(section.c_str(),  "CroppingY", 0);
    data.ConfigInt[configid_int_overlay_crop_width]                    = config.ReadInt(section.c_str(),  "CroppingWidth", -1);
    data.ConfigInt[configid_int_overlay_crop_height]                   = config.ReadInt(section.c_str(),  "CroppingHeight", -1);

    data.ConfigBool[configid_bool_overlay_3D_enabled]                  = config.ReadBool(section.c_str(), "3DEnabled", false);
    data.ConfigInt[configid_int_overlay_3D_mode]                       = config.ReadInt(section.c_str(),  "3DMode", ovrl_3Dmode_hsbs);
    data.ConfigBool[configid_bool_overlay_3D_swapped]                  = config.ReadBool(section.c_str(), "3DSwapped", false);
    data.ConfigBool[configid_bool_overlay_gazefade_enabled]            = config.ReadBool(section.c_str(), "GazeFade", false);
    data.ConfigFloat[configid_float_overlay_gazefade_distance]         = config.ReadInt(section.c_str(),  "GazeFadeDistance", 0) / 100.0f;
    data.ConfigFloat[configid_float_overlay_gazefade_rate]             = config.ReadInt(section.c_str(),  "GazeFadeRate", 100) / 100.0f;
    data.ConfigFloat[configid_float_overlay_gazefade_opacity]          = config.ReadInt(section.c_str(),  "GazeFadeOpacity", 0) / 100.0f;
    data.ConfigInt[configid_int_overlay_update_limit_override_mode]    = config.ReadInt(section.c_str(),  "UpdateLimitOverrideMode", update_limit_mode_off);
    data.ConfigFloat[configid_float_overlay_update_limit_override_ms]  = config.ReadInt(section.c_str(),  "UpdateLimitMS", 0) / 100.0f;
    data.ConfigInt[configid_int_overlay_update_limit_override_fps]     = config.ReadInt(section.c_str(),  "UpdateLimitFPS", update_limit_fps_30);
    data.ConfigBool[configid_bool_overlay_input_enabled]               = config.ReadBool(section.c_str(), "InputEnabled", true);
    data.ConfigBool[configid_bool_overlay_input_dplus_lp_enabled]      = config.ReadBool(section.c_str(), "InputDPlusLPEnabled", true);
    data.ConfigInt[configid_int_overlay_group_id]                      = config.ReadInt(section.c_str(),  "GroupID", 0);
    data.ConfigBool[configid_bool_overlay_update_invisible]            = config.ReadBool(section.c_str(), "UpdateInvisible", false);

    data.ConfigBool[configid_bool_overlay_floatingui_enabled]          = config.ReadBool(section.c_str(), "ShowFloatingUI", true);
    data.ConfigBool[configid_bool_overlay_floatingui_desktops_enabled] = config.ReadBool(section.c_str(), "ShowDesktopButtons", false);
    data.ConfigBool[configid_bool_overlay_actionbar_enabled]           = config.ReadBool(section.c_str(), "ShowActionBar", false);
    data.ConfigBool[configid_bool_overlay_actionbar_order_use_global]  = config.ReadBool(section.c_str(), "ActionBarOrderUseGlobal", true);

    bool do_set_auto_name = ( (!data.ConfigBool[configid_bool_overlay_name_custom]) && (data.ConfigNameStr.empty()) );

    //Restore WinRT Capture state if possible
    if ( (data.ConfigInt[configid_int_overlay_winrt_desktop_id] == -2) && (!data.ConfigStr[configid_str_overlay_winrt_last_window_title].empty()) )
    {
        HWND window = WindowInfo::FindClosestWindowForTitle(data.ConfigStr[configid_str_overlay_winrt_last_window_title], data.ConfigStr[configid_str_overlay_winrt_last_window_class_name],
                                                            data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name], WindowManager::Get().WindowListGet());

        data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] = (uint64_t)window;

        //If we found a new match, adjust last window title and update the overlay name later (we want to keep the old name if the window is gone though)
        if (window != nullptr)
        {
            WindowInfo info(window);
            data.ConfigStr[configid_str_overlay_winrt_last_window_title] = StringConvertFromUTF16(info.GetTitle().c_str());
            //Exe & class name is not gonna change

            do_set_auto_name = true;
        }
        else if (m_ConfigInt[configid_int_windows_winrt_capture_lost_behavior] == window_caplost_hide_overlay) //Treat not found windows as lost capture and hide them if setting active
        {
            data.ConfigBool[configid_bool_overlay_enabled] = false;
        }
    }

    //If single desktop mirroring is active, set desktop ID to the first one's
    if ( (current_id != 0) && (m_ConfigBool[configid_bool_performance_single_desktop_mirroring]) )
    {
        data.ConfigInt[configid_int_overlay_desktop_id] = OverlayManager::Get().GetConfigData(0).ConfigInt[configid_int_overlay_desktop_id];
    }

    //Default the transform matrix to zero
    float matrix_zero[16] = { 0.0f };
    data.ConfigTransform = matrix_zero;

    std::string transform_str; //Only set these when it's really present in the file, or else it defaults to identity instead of zero
    transform_str = config.ReadString(section.c_str(), "Transform");
    if (!transform_str.empty())
        data.ConfigTransform = transform_str;

    //Load action order
    auto& action_order_global = ConfigManager::GetActionMainBarOrder();
    auto& action_order = data.ConfigActionBarOrder;
    action_order.clear();
    std::string order_str = config.ReadString(section.c_str(), "ActionBarOrderCustom");
    std::stringstream ss(order_str);
    int id;
    bool visible;
    char sep;

    for (;;)
    {
        ss >> id >> visible >> sep;

        if (ss.fail())
            break;

        action_order.push_back({ (ActionID)id, visible });
    }

    //If there is a mismatch or it's fully missing, reset to global
    if (action_order.size() != action_order_global.size())
    {
        action_order = action_order_global;
    }

    #ifdef DPLUS_UI
    //When loading an UI overlay, send config state over to ensure the correct process has rendering access even if the UI was restarted at some point
    if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui)
    {
        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, (int)overlay_id);
        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_overlay_capture_source, ovrl_capsource_ui);
        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

        UIManager::Get()->GetPerformanceWindow().ScheduleOverlaySharedTextureUpdate();
    }

    //Set auto name if there's a new window match
    if (do_set_auto_name)
    {
        OverlayManager::Get().SetCurrentOverlayNameAuto();
    }
    #endif
}

void ConfigManager::SaveOverlayProfile(Ini& config, unsigned int overlay_id)
{
    const OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

    std::string section;

    if (overlay_id != UINT_MAX)
    {
        std::stringstream ss;
        ss << "Overlay" << overlay_id;

        section = ss.str();
    }
    else
    {
        section = "Overlay";
    }

    config.WriteString(section.c_str(), "Name", data.ConfigNameStr.c_str());

    config.WriteBool(section.c_str(), "NameIsCustom",           data.ConfigBool[configid_bool_overlay_name_custom]);
    config.WriteBool(section.c_str(), "Enabled",                data.ConfigBool[configid_bool_overlay_enabled]);
    config.WriteInt( section.c_str(), "DesktopID",              data.ConfigInt[configid_int_overlay_desktop_id]);
    config.WriteInt( section.c_str(), "CaptureSource",          data.ConfigInt[configid_int_overlay_capture_source]);
    config.WriteInt( section.c_str(), "Width",              int(data.ConfigFloat[configid_float_overlay_width]           * 100.0f));
    config.WriteInt( section.c_str(), "Curvature",          int(data.ConfigFloat[configid_float_overlay_curvature]       * 100.0f));
    config.WriteInt( section.c_str(), "Opacity",            int(data.ConfigFloat[configid_float_overlay_opacity]         * 100.0f));
    config.WriteInt( section.c_str(), "OffsetRight",        int(data.ConfigFloat[configid_float_overlay_offset_right]    * 100.0f));
    config.WriteInt( section.c_str(), "OffsetUp",           int(data.ConfigFloat[configid_float_overlay_offset_up]       * 100.0f));
    config.WriteInt( section.c_str(), "OffsetForward",      int(data.ConfigFloat[configid_float_overlay_offset_forward]  * 100.0f));
    config.WriteInt( section.c_str(), "DisplayMode",            data.ConfigInt[configid_int_overlay_display_mode]);
    config.WriteInt( section.c_str(), "Origin",                 data.ConfigInt[configid_int_overlay_origin]);

    config.WriteBool(section.c_str(), "CroppingEnabled",        data.ConfigBool[configid_bool_overlay_crop_enabled]);
    config.WriteInt( section.c_str(), "CroppingX",              data.ConfigInt[configid_int_overlay_crop_x]);
    config.WriteInt( section.c_str(), "CroppingY",              data.ConfigInt[configid_int_overlay_crop_y]);
    config.WriteInt( section.c_str(), "CroppingWidth",          data.ConfigInt[configid_int_overlay_crop_width]);
    config.WriteInt( section.c_str(), "CroppingHeight",         data.ConfigInt[configid_int_overlay_crop_height]);

    config.WriteBool(section.c_str(), "3DEnabled",              data.ConfigBool[configid_bool_overlay_3D_enabled]);
    config.WriteInt( section.c_str(), "3DMode",                 data.ConfigInt[configid_int_overlay_3D_mode]);
    config.WriteBool(section.c_str(), "3DSwapped",              data.ConfigBool[configid_bool_overlay_3D_swapped]);
    config.WriteBool(section.c_str(), "GazeFade",               data.ConfigBool[configid_bool_overlay_gazefade_enabled]);
    config.WriteInt( section.c_str(), "GazeFadeDistance",   int(data.ConfigFloat[configid_float_overlay_gazefade_distance] * 100.0f));
    config.WriteInt( section.c_str(), "GazeFadeRate",       int(data.ConfigFloat[configid_float_overlay_gazefade_rate]     * 100.0f));
    config.WriteInt( section.c_str(), "GazeFadeOpacity",    int(data.ConfigFloat[configid_float_overlay_gazefade_opacity]  * 100.0f));
    config.WriteInt( section.c_str(), "UpdateLimitModeOverride",data.ConfigInt[configid_int_overlay_update_limit_override_mode]);
    config.WriteInt( section.c_str(), "UpdateLimitMS",      int(data.ConfigFloat[configid_float_overlay_update_limit_override_ms] * 100.0f));
    config.WriteInt( section.c_str(), "UpdateLimitFPS",         data.ConfigInt[configid_int_overlay_update_limit_override_fps]);
    config.WriteBool(section.c_str(), "InputEnabled",           data.ConfigBool[configid_bool_overlay_input_enabled]);
    config.WriteBool(section.c_str(), "InputDPlusLPEnabled",    data.ConfigBool[configid_bool_overlay_input_dplus_lp_enabled]);
    config.WriteInt( section.c_str(), "GroupID",                data.ConfigInt[configid_int_overlay_group_id]);
    config.WriteBool(section.c_str(), "UpdateInvisible",        data.ConfigBool[configid_bool_overlay_update_invisible]);

    config.WriteBool(section.c_str(), "ShowFloatingUI",          data.ConfigBool[configid_bool_overlay_floatingui_enabled]);
    config.WriteBool(section.c_str(), "ShowDesktopButtons",      data.ConfigBool[configid_bool_overlay_floatingui_desktops_enabled]);
    config.WriteBool(section.c_str(), "ShowActionBar",           data.ConfigBool[configid_bool_overlay_actionbar_enabled]);
    config.WriteBool(section.c_str(), "ActionBarOrderUseGlobal", data.ConfigBool[configid_bool_overlay_actionbar_order_use_global]);

    config.WriteString(section.c_str(), "Transform", data.ConfigTransform.toString().c_str());

    //Save WinRT Capture state
    HWND window_handle = (HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd];
    std::string last_window_title, last_window_class_name, last_window_exe_name;

    if (window_handle != nullptr)
    {
        WindowInfo info(window_handle);

        last_window_title      = StringConvertFromUTF16(info.GetTitle().c_str());
        last_window_class_name = StringConvertFromUTF16(info.GetWindowClassName().c_str());
        last_window_exe_name   = info.GetExeName();
    }

    if (last_window_title.empty()) //Save last known title and exe name even when handle is nullptr or getting title failed so we can still restore the window on the next load if it happens to exist
    {
        last_window_title      = data.ConfigStr[configid_str_overlay_winrt_last_window_title];
        last_window_class_name = data.ConfigStr[configid_str_overlay_winrt_last_window_class_name];
        last_window_exe_name   = data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name];
    }

    config.WriteString(section.c_str(), "WinRTLastWindowTitle",     last_window_title.c_str());
    config.WriteString(section.c_str(), "WinRTLastWindowClassName", last_window_class_name.c_str());
    config.WriteString(section.c_str(), "WinRTLastWindowExeName",   last_window_exe_name.c_str());
    config.WriteInt(section.c_str(), "WinRTDesktopID", data.ConfigInt[configid_int_overlay_winrt_desktop_id]);

    //Save action order
    std::stringstream ss;

    for (auto& data_order : data.ConfigActionBarOrder)
    {
        ss << data_order.action_id << ' ' << data_order.visible << ";";
    }

    config.WriteString(section.c_str(), "ActionBarOrderCustom", ss.str().c_str());
}

bool ConfigManager::LoadConfigFromFile()
{
    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_newui.ini").c_str() );
    bool existed = FileExists(wpath.c_str());

    //If config.ini doesn't exist (yet), load from config_default.ini instead, which hopefully does (would still work to a lesser extent though)
    if (!existed)
    {
        wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_default.ini").c_str() );
    }

    Ini config(wpath.c_str());

    m_ConfigBool[configid_bool_interface_no_ui]                              = config.ReadBool(  "Interface", "NoUIAutoLaunch", false);
    m_ConfigBool[configid_bool_interface_no_notification_icon]               = config.ReadBool(  "Interface", "NoNotificationIcon", false);
    m_ConfigString[configid_str_interface_language_file]                     = config.ReadString("Interface", "LanguageFile");
    m_ConfigBool[configid_bool_interface_show_advanced_settings]             = config.ReadBool(  "Interface", "ShowAdvancedSettings", true);
    m_ConfigBool[configid_bool_interface_large_style]                        = config.ReadBool(  "Interface", "DisplaySizeLarge", false);
    m_ConfigInt[configid_int_interface_overlay_current_id]                   = config.ReadInt(   "Interface", "OverlayCurrentID", 0);
    m_ConfigInt[configid_int_interface_mainbar_desktop_listing]              = config.ReadInt(   "Interface", "DesktopButtonCyclingMode", mainbar_desktop_listing_individual);
    m_ConfigBool[configid_bool_interface_mainbar_desktop_include_all]        = config.ReadBool(  "Interface", "DesktopButtonIncludeAll", false);

    //Read color string as unsigned int but store it as signed
    m_ConfigInt[configid_int_interface_background_color] = pun_cast<unsigned int, int>( std::stoul(config.ReadString("Interface", "EnvironmentBackgroundColor", "00000080"), nullptr, 16) );

    m_ConfigInt[configid_int_interface_background_color_display_mode]        = config.ReadInt( "Interface", "EnvironmentBackgroundColorDisplayMode", ui_bgcolor_dispmode_never);
    m_ConfigBool[configid_bool_interface_dim_ui]                             = config.ReadBool("Interface", "DimUI", false);
    m_ConfigBool[configid_bool_interface_blank_space_drag_enabled]           = config.ReadBool("Interface", "BlankSpaceDragEnabled", true);
    m_ConfigFloat[configid_float_interface_last_vr_ui_scale]                 = config.ReadInt( "Interface", "LastVRUIScale", 100) / 100.0f;
    m_ConfigBool[configid_bool_interface_warning_compositor_res_hidden]      = config.ReadBool("Interface", "WarningCompositorResolutionHidden", false);
    m_ConfigBool[configid_bool_interface_warning_compositor_quality_hidden]  = config.ReadBool("Interface", "WarningCompositorQualityHidden", false);
    m_ConfigBool[configid_bool_interface_warning_process_elevation_hidden]   = config.ReadBool("Interface", "WarningProcessElevationHidden", false);
    m_ConfigBool[configid_bool_interface_warning_elevated_mode_hidden]       = config.ReadBool("Interface", "WarningElevatedModeHidden", false);
    m_ConfigBool[configid_bool_interface_warning_welcome_hidden]             = config.ReadBool("Interface", "WarningWelcomeHidden", false);
    m_ConfigInt[configid_int_interface_wmr_ignore_vscreens]                  = config.ReadInt( "Interface", "WMRIgnoreVScreens", -1);

    OverlayManager::Get().SetCurrentOverlayID(m_ConfigInt[configid_int_interface_overlay_current_id]);

    //Load action order
    auto& action_order = m_ActionManager.GetActionMainBarOrder();
    action_order.clear();
    std::string order_str = config.ReadString("Interface", "ActionOrder");
    std::stringstream ss(order_str);
    int id;
    bool visible;
    char sep;

    for (;;)
    {
        ss >> id >> visible >> sep;

        if (ss.fail())
            break;

        action_order.push_back({ (ActionID)id, visible });
    }

    #ifdef DPLUS_UI
        LoadConfigPersistentWindowState(config);
    #endif

    m_ConfigInt[configid_int_input_go_home_action_id]                       = config.ReadInt( "Input", "GoHomeButtonActionID", 0);
    m_ConfigInt[configid_int_input_go_back_action_id]                       = config.ReadInt( "Input", "GoBackButtonActionID", 0);
    m_ConfigInt[configid_int_input_shortcut01_action_id]                    = config.ReadInt( "Input", "GlobalShortcut01ActionID", 0);
    m_ConfigInt[configid_int_input_shortcut02_action_id]                    = config.ReadInt( "Input", "GlobalShortcut02ActionID", 0);
    m_ConfigInt[configid_int_input_shortcut03_action_id]                    = config.ReadInt( "Input", "GlobalShortcut03ActionID", 0);

    m_ConfigInt[configid_int_input_hotkey01_modifiers]                      = config.ReadInt( "Input", "GlobalHotkey01Modifiers", 0);
    m_ConfigInt[configid_int_input_hotkey01_keycode]                        = config.ReadInt( "Input", "GlobalHotkey01KeyCode",   0);
    m_ConfigInt[configid_int_input_hotkey01_action_id]                      = config.ReadInt( "Input", "GlobalHotkey01ActionID",  0);
    m_ConfigInt[configid_int_input_hotkey02_modifiers]                      = config.ReadInt( "Input", "GlobalHotkey02Modifiers", 0);
    m_ConfigInt[configid_int_input_hotkey02_keycode]                        = config.ReadInt( "Input", "GlobalHotkey02KeyCode",   0);
    m_ConfigInt[configid_int_input_hotkey02_action_id]                      = config.ReadInt( "Input", "GlobalHotkey02ActionID",  0);
    m_ConfigInt[configid_int_input_hotkey03_modifiers]                      = config.ReadInt( "Input", "GlobalHotkey03Modifiers", 0);
    m_ConfigInt[configid_int_input_hotkey03_keycode]                        = config.ReadInt( "Input", "GlobalHotkey03KeyCode",   0);
    m_ConfigInt[configid_int_input_hotkey03_action_id]                      = config.ReadInt( "Input", "GlobalHotkey03ActionID",  0);

    m_ConfigFloat[configid_float_input_detached_interaction_max_distance]   = config.ReadInt( "Input", "DetachedInteractionMaxDistance", 200) / 100.0f;
    m_ConfigBool[configid_bool_input_global_hmd_pointer]                    = config.ReadBool("Input", "GlobalHMDPointer", false);
    m_ConfigFloat[configid_float_input_global_hmd_pointer_max_distance]     = config.ReadInt( "Input", "GlobalHMDPointerMaxDistance", 0) / 100.0f;
    m_ConfigBool[configid_bool_input_laser_pointer_block_input]             = config.ReadBool("Input", "LaserPointerBlockInput", false);
    m_ConfigBool[configid_bool_input_drag_force_upright]                    = config.ReadBool("Input", "DragForceUpright", false);

    m_ConfigBool[configid_bool_input_mouse_render_cursor]                   = config.ReadBool("Mouse", "RenderCursor", true);
    m_ConfigBool[configid_bool_input_mouse_render_intersection_blob]        = config.ReadBool("Mouse", "RenderIntersectionBlob", false);
    m_ConfigInt[configid_int_input_mouse_dbl_click_assist_duration_ms]      = config.ReadInt( "Mouse", "DoubleClickAssistDuration", -1);
    m_ConfigBool[configid_bool_input_mouse_allow_pointer_override]          = config.ReadBool("Mouse", "AllowPointerOverride", true);

    m_ConfigString[configid_str_input_keyboard_layout_file]                 = config.ReadString("Keyboard", "LayoutFile", "qwerty_usa.ini");
    m_ConfigBool[configid_bool_input_keyboard_cluster_function_enabled]     = config.ReadBool("Keyboard", "LayoutClusterFunction",   true);
    m_ConfigBool[configid_bool_input_keyboard_cluster_navigation_enabled]   = config.ReadBool("Keyboard", "LayoutClusterNavigation", true);
    m_ConfigBool[configid_bool_input_keyboard_cluster_numpad_enabled]       = config.ReadBool("Keyboard", "LayoutClusterNumpad",     false);
    m_ConfigBool[configid_bool_input_keyboard_cluster_extra_enabled]        = config.ReadBool("Keyboard", "LayoutClusterExtra",      false);
    m_ConfigBool[configid_bool_input_keyboard_sticky_modifiers]             = config.ReadBool("Keyboard", "StickyModifiers",         true);
    m_ConfigBool[configid_bool_input_keyboard_key_repeat]                   = config.ReadBool("Keyboard", "KeyRepeat",               true);

    m_ConfigBool[configid_bool_windows_auto_focus_scene_app_dashboard]      = config.ReadBool("Windows", "AutoFocusSceneAppDashboard", false);
    m_ConfigBool[configid_bool_windows_winrt_auto_focus]                    = config.ReadBool("Windows", "WinRTAutoFocus", true);
    m_ConfigBool[configid_bool_windows_winrt_keep_on_screen]                = config.ReadBool("Windows", "WinRTKeepOnScreen", true);
    m_ConfigInt[configid_int_windows_winrt_dragging_mode]                   = config.ReadInt( "Windows", "WinRTDraggingMode", window_dragging_overlay);
    m_ConfigBool[configid_bool_windows_winrt_auto_size_overlay]             = config.ReadBool("Windows", "WinRTAutoSizeOverlay", false);
    m_ConfigBool[configid_bool_windows_winrt_auto_focus_scene_app]          = config.ReadBool("Windows", "WinRTAutoFocusSceneApp", false);
    m_ConfigBool[configid_bool_windows_winrt_window_matching_strict]        = config.ReadBool("Windows", "WinRTWindowMatchingStrict", false);
    m_ConfigInt[configid_int_windows_winrt_capture_lost_behavior]           = config.ReadInt( "Windows", "WinRTOnCaptureLost", window_caplost_hide_overlay);

    m_ConfigInt[configid_int_performance_update_limit_mode]                 = config.ReadInt( "Performance", "UpdateLimitMode", update_limit_mode_off);
    m_ConfigFloat[configid_float_performance_update_limit_ms]               = config.ReadInt( "Performance", "UpdateLimitMS", 0) / 100.0f;
    m_ConfigInt[configid_int_performance_update_limit_fps]                  = config.ReadInt( "Performance", "UpdateLimitFPS", update_limit_fps_30);
    m_ConfigBool[configid_bool_performance_rapid_laser_pointer_updates]     = config.ReadBool("Performance", "RapidLaserPointerUpdates", false);
    m_ConfigBool[configid_bool_performance_single_desktop_mirroring]        = config.ReadBool("Performance", "SingleDesktopMirroring", false);
    m_ConfigBool[configid_bool_performance_monitor_large_style]             = config.ReadBool("Performance", "PerformanceMonitorStyleLarge", true);
    m_ConfigBool[configid_bool_performance_monitor_show_graphs]             = config.ReadBool("Performance", "PerformanceMonitorShowGraphs", true);
    m_ConfigBool[configid_bool_performance_monitor_show_time]               = config.ReadBool("Performance", "PerformanceMonitorShowTime", false);
    m_ConfigBool[configid_bool_performance_monitor_show_cpu]                = config.ReadBool("Performance", "PerformanceMonitorShowCPU", true);
    m_ConfigBool[configid_bool_performance_monitor_show_gpu]                = config.ReadBool("Performance", "PerformanceMonitorShowGPU", true);
    m_ConfigBool[configid_bool_performance_monitor_show_fps]                = config.ReadBool("Performance", "PerformanceMonitorShowFPS", true);
    m_ConfigBool[configid_bool_performance_monitor_show_battery]            = config.ReadBool("Performance", "PerformanceMonitorShowBattery", true);
    m_ConfigBool[configid_bool_performance_monitor_show_trackers]           = config.ReadBool("Performance", "PerformanceMonitorShowTrackers", true);
    m_ConfigBool[configid_bool_performance_monitor_show_vive_wireless]      = config.ReadBool("Performance", "PerformanceMonitorShowViveWireless", false);
    m_ConfigBool[configid_bool_performance_monitor_disable_gpu_counters]    = config.ReadBool("Performance", "PerformanceMonitorDisableGPUCounters", false);


    m_ConfigBool[configid_bool_misc_no_steam]             = config.ReadBool("Misc", "NoSteam", false);
    m_ConfigBool[configid_bool_misc_uiaccess_was_enabled] = config.ReadBool("Misc", "UIAccessWasEnabled", false);

    //Load custom actions (this is where using ini feels dumb, but it still kinda works)
    auto& custom_actions = m_ActionManager.GetCustomActions();
    custom_actions.clear();
    int custom_action_count = config.ReadInt("CustomActions", "Count", 0);

    for (int i = 0; i < custom_action_count; ++i)
    {
        std::string action_ini_name = "Action" + std::to_string(i);
        CustomAction action;
        action.Name = config.ReadString("CustomActions", (action_ini_name + "Name").c_str(), action_ini_name.c_str());
        action.FunctionType = ActionManager::ParseCustomActionFunctionString( config.ReadString("CustomActions", (action_ini_name + "FunctionType").c_str()) );

        switch (action.FunctionType)
        {
            case caction_press_keys:
            {
                action.KeyCodes[0] = config.ReadInt( "CustomActions", (action_ini_name + "KeyCode1").c_str(),   0);
                action.KeyCodes[1] = config.ReadInt( "CustomActions", (action_ini_name + "KeyCode2").c_str(),   0);
                action.KeyCodes[2] = config.ReadInt( "CustomActions", (action_ini_name + "KeyCode3").c_str(),   0);
                action.IntID       = config.ReadBool("CustomActions", (action_ini_name + "ToggleKeys").c_str(), false);
                break;
            }
            case caction_type_string:
            {
                action.StrMain = config.ReadString("CustomActions", (action_ini_name + "TypeString").c_str());
                break;
            }
            case caction_launch_application:
            {
                action.StrMain = config.ReadString("CustomActions", (action_ini_name + "ExecutablePath").c_str());
                action.StrArg  = config.ReadString("CustomActions", (action_ini_name + "ExecutableArg").c_str());
                break;
            }
            case caction_toggle_overlay_enabled_state:
            {
                action.IntID = config.ReadInt("CustomActions", (action_ini_name + "OverlayID").c_str(), 0);
                break;
            }
        }
        
        #ifdef DPLUS_UI
            action.IconFilename = config.ReadString("CustomActions", (action_ini_name + "IconFilename").c_str());
        #endif

        custom_actions.push_back(action);
    }

    //Provide default for empty order list
    if (action_order.empty()) 
    {
        for (int i = action_show_keyboard; i < action_built_in_MAX; ++i)
        {
            action_order.push_back({(ActionID)i, false});
        }

        for (int i = action_custom; i < action_custom + custom_actions.size(); ++i)
        {
            action_order.push_back({(ActionID)i, false});
        }
    }
    else
    {
        //Validate order list in case some manual editing was made
        action_order.erase(std::remove_if(action_order.begin(),
                                          action_order.end(),
                                          [](const ActionMainBarOrderData& data) { return !ActionManager::Get().IsActionIDValid(data.action_id); }),
                                          action_order.end());

        //Automatically add actions if they're missing
        bool is_action_present;

        for (int i = action_show_keyboard; i < action_custom + custom_actions.size(); ++i)
        {
            is_action_present = false;

            for (const auto& order_data : action_order)
            {
                if (order_data.action_id == i)
                {
                    is_action_present = true;
                    break;
                }
            }

            if (!is_action_present)
            {
                action_order.push_back({(ActionID)i, false});
            }

            //After built-in actions are checked, jump to custom range
            if (i == action_built_in_MAX - 1)
            {
                i = action_custom - 1;
            }
        }
    }

    //Validate action IDs for controller bindings too
    if (!ActionManager::Get().IsActionIDValid((ActionID)m_ConfigInt[configid_int_input_go_home_action_id]))
    {
        m_ConfigInt[configid_int_input_go_home_action_id] = action_none;
    }
    if (!ActionManager::Get().IsActionIDValid((ActionID)m_ConfigInt[configid_int_input_go_back_action_id]))
    {
        m_ConfigInt[configid_int_input_go_back_action_id] = action_none;
    }
    if (!ActionManager::Get().IsActionIDValid((ActionID)m_ConfigInt[configid_int_input_shortcut01_action_id]))
    {
        m_ConfigInt[configid_int_input_shortcut01_action_id] = action_none;
    }
    if (!ActionManager::Get().IsActionIDValid((ActionID)m_ConfigInt[configid_int_input_shortcut02_action_id]))
    {
        m_ConfigInt[configid_int_input_shortcut02_action_id] = action_none;
    }
    if (!ActionManager::Get().IsActionIDValid((ActionID)m_ConfigInt[configid_int_input_shortcut03_action_id]))
    {
        m_ConfigInt[configid_int_input_shortcut03_action_id] = action_none;
    }

    //Apply render cursor setting for WinRT Capture
    #ifndef DPLUS_UI
        if (DPWinRT_IsCaptureCursorEnabledPropertySupported())
            DPWinRT_SetCaptureCursorEnabled(m_ConfigBool[configid_bool_input_mouse_render_cursor]);
    #endif

    //Set WindowManager active (no longer gets deactivated during runtime)
    WindowManager::Get().SetActive(true);

    //Query elevated mode state
    m_ConfigBool[configid_bool_state_misc_elevated_mode_active] = IPCManager::IsElevatedModeProcessRunning();

    #ifdef DPLUS_UI
        UIManager::Get()->GetSettingsWindow().ApplyCurrentOverlayState();
        UIManager::Get()->GetOverlayPropertiesWindow().ApplyCurrentOverlayState();
        UIManager::Get()->GetVRKeyboard().LoadCurrentLayout();
        UIManager::Get()->GetVRKeyboard().GetWindow().ApplyCurrentOverlayState();

        TranslationManager::Get().LoadTranslationFromFile( m_ConfigString[configid_str_interface_language_file].c_str() );

        UIManager::Get()->UpdateAnyWarningDisplayedState();
    #endif

    //Load last used overlay config
    LoadMultiOverlayProfile(config);

    return existed; //We use default values if it doesn't, but still return if the file existed
}

void ConfigManager::LoadMultiOverlayProfile(const Ini& config, bool clear_existing_overlays, std::vector<char>* ovrl_inclusion_list)
{
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();

    if (clear_existing_overlays)
    {
        OverlayManager::Get().RemoveAllOverlays();
    }

    unsigned int overlay_id = 0;

    std::stringstream ss;
    ss << "Overlay" << overlay_id;

    //Load all sequential overlay sections that exist
    while (config.SectionExists(ss.str().c_str()))
    {
        //Don't add if not in list (or none was passed)
        if ( (ovrl_inclusion_list == nullptr) || (ovrl_inclusion_list->size() <= overlay_id) || ((*ovrl_inclusion_list)[overlay_id] != 0) )
        {
            OverlayManager::Get().DuplicateOverlay(OverlayConfigData());
            OverlayManager::Get().SetCurrentOverlayID(OverlayManager::Get().GetOverlayCount() - 1);

            LoadOverlayProfile(config, overlay_id);
        }

        overlay_id++;

        ss = std::stringstream();
        ss << "Overlay" << overlay_id;
    }

    OverlayManager::Get().SetCurrentOverlayID( std::min(current_overlay_old, (OverlayManager::Get().GetOverlayCount() == 0) ? k_ulOverlayID_None : OverlayManager::Get().GetOverlayCount() - 1) );
}

void ConfigManager::SaveMultiOverlayProfile(Ini& config, std::vector<char>* ovrl_inclusion_list)
{
    //Remove single overlay section in case it still exists
    config.RemoveSection("Overlay");

    unsigned int overlay_id = 0;
    std::stringstream ss;
    ss << "Overlay" << overlay_id;

    //Remove all sequential overlay sections that exist first
    while (config.SectionExists(ss.str().c_str()))
    {
        config.RemoveSection(ss.str().c_str());

        overlay_id++;

        ss = std::stringstream();
        ss << "Overlay" << overlay_id;
    }

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    overlay_id = 0;

    //Save all overlays in separate sections
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        //Don't save if not in list (or none was passed)
        if ( (ovrl_inclusion_list == nullptr) || (ovrl_inclusion_list->size() <= i) || ((*ovrl_inclusion_list)[i] != 0) )
        {
            OverlayManager::Get().SetCurrentOverlayID(i);

            SaveOverlayProfile(config, overlay_id);
            overlay_id++;
        }
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

#ifdef DPLUS_UI

void ConfigManager::LoadConfigPersistentWindowState(Ini& config)
{
    //Load persistent UI window state (not stored in config variables)
    FloatingWindow* const windows[]  = { &UIManager::Get()->GetSettingsWindow(), &UIManager::Get()->GetOverlayPropertiesWindow(), &UIManager::Get()->GetVRKeyboard().GetWindow() };
    const char* const window_names[] = { "WindowSettings", "WindowProperties", "WindowKeyboard" };

    for (size_t i = 0; i < IM_ARRAYSIZE(windows); ++i)
    {
        FloatingWindow& window = *windows[i];
        FloatingWindowOverlayState& ovrl_state_room          = window.GetOverlayState(floating_window_ovrl_state_room);
        FloatingWindowOverlayState& ovrl_state_dashboard_tab = window.GetOverlayState(floating_window_ovrl_state_dashboard_tab);
        const std::string key_base = window_names[i];
        std::string transform_str;

        ovrl_state_room.IsVisible = config.ReadBool(  "Interface",  (key_base + "RoomVisible").c_str(), false);
        ovrl_state_room.IsPinned  = config.ReadBool(  "Interface",  (key_base + "RoomPinned").c_str(),  false);
        ovrl_state_room.Size      = config.ReadInt(   "Interface",  (key_base + "RoomSize").c_str(),      100) / 100.0f;
        transform_str             = config.ReadString("Interface",  (key_base + "RoomTransform").c_str());

        if (!transform_str.empty())
        {
            (ovrl_state_room.IsPinned) ? ovrl_state_room.TransformAbs = transform_str : ovrl_state_room.Transform = transform_str;
        }

        ovrl_state_dashboard_tab.IsVisible = config.ReadBool(  "Interface",  (key_base + "DashboardTabVisible").c_str(), false);
        ovrl_state_dashboard_tab.IsPinned  = config.ReadBool(  "Interface",  (key_base + "DashboardTabPinned").c_str(),  false);
        ovrl_state_dashboard_tab.Size      = config.ReadInt(   "Interface",  (key_base + "DashboardTabSize").c_str(),      100) / 100.0f;
        transform_str                      = config.ReadString("Interface",  (key_base + "DashboardTabTransform").c_str());

        if (!transform_str.empty())
        {
            (ovrl_state_dashboard_tab.IsPinned) ? ovrl_state_dashboard_tab.TransformAbs = transform_str : ovrl_state_dashboard_tab.Transform = transform_str;
        }

        //Show window if it should be visible. For most windows this has no effect, but the keyboard one has overridden Show() for example.
        if ( ((window.GetOverlayStateCurrentID() == floating_window_ovrl_state_room)          && (ovrl_state_room.IsVisible)) ||
             ((window.GetOverlayStateCurrentID() == floating_window_ovrl_state_dashboard_tab) && (ovrl_state_dashboard_tab.IsVisible)) )
        {
            window.Show();
        }
    }

    //Load other specific window state
    //SetActiveOverlayID() will not be effective if the config load happens before overlays were loaded (so during startup), 
    //but it is also called again by UIManager::OnInitDone() on the ID set here, so it still works
    int last_active_overlay_id = config.ReadInt("Interface", "WindowPropertiesLastOverlayID", -1);
    UIManager::Get()->GetOverlayPropertiesWindow().SetActiveOverlayID((last_active_overlay_id != -1) ? (unsigned int)last_active_overlay_id : k_ulOverlayID_None, true);

    //Only room state's is saved/restored here
    UIManager::Get()->GetVRKeyboard().GetWindow().SetAssignedOverlayID(config.ReadInt("Interface", "WindowKeyboardLastAssignedOverlayID", -1), floating_window_ovrl_state_room);
}

void ConfigManager::SaveConfigPersistentWindowState(Ini& config)
{
    //Save persistent UI window state (not stored in config variables)
    FloatingWindow* const windows[]  = { &UIManager::Get()->GetSettingsWindow(), &UIManager::Get()->GetOverlayPropertiesWindow(), &UIManager::Get()->GetVRKeyboard().GetWindow() };
    const char* const window_names[] = { "WindowSettings", "WindowProperties", "WindowKeyboard" };

    for (size_t i = 0; i < IM_ARRAYSIZE(windows); ++i)
    {
        const bool is_keyboard = (i == 2);
        const FloatingWindowOverlayState& ovrl_state_room          = windows[i]->GetOverlayState(floating_window_ovrl_state_room);
        const FloatingWindowOverlayState& ovrl_state_dashboard_tab = windows[i]->GetOverlayState(floating_window_ovrl_state_dashboard_tab);
        std::string key_base = window_names[i];

        const Matrix4& transform_room = (ovrl_state_room.IsPinned) ? ovrl_state_room.TransformAbs : ovrl_state_room.Transform;
        config.WriteBool(  "Interface",  (key_base + "RoomVisible").c_str(),       ovrl_state_room.IsVisible);
        config.WriteBool(  "Interface",  (key_base + "RoomPinned").c_str(),        ovrl_state_room.IsPinned);
        config.WriteInt(   "Interface",  (key_base + "RoomSize").c_str(),      int(ovrl_state_room.Size * 100.0f));
        config.WriteString("Interface",  (key_base + "RoomTransform").c_str(),     transform_room.toString().c_str());

        const Matrix4& transform_dashboard_tab = (ovrl_state_dashboard_tab.IsPinned) ? ovrl_state_dashboard_tab.TransformAbs : ovrl_state_dashboard_tab.Transform;
        config.WriteBool(  "Interface",  (key_base + "DashboardTabVisible").c_str(),       ovrl_state_dashboard_tab.IsVisible);
        config.WriteBool(  "Interface",  (key_base + "DashboardTabPinned").c_str(),        ovrl_state_dashboard_tab.IsPinned);
        config.WriteInt(   "Interface",  (key_base + "DashboardTabSize").c_str(),      int(ovrl_state_dashboard_tab.Size * 100.0f));
        config.WriteString("Interface",  (key_base + "DashboardTabTransform").c_str(),     transform_dashboard_tab.toString().c_str());
    }

    //Store other specific window state
    unsigned int last_active_overlay_id = UIManager::Get()->GetOverlayPropertiesWindow().GetActiveOverlayID();
    config.WriteInt("Interface", "WindowPropertiesLastOverlayID", (last_active_overlay_id != k_ulOverlayID_None) ? (int)last_active_overlay_id : -1);

    //Only room state's is saved/restored here
    config.WriteInt("Interface", "WindowKeyboardLastAssignedOverlayID", UIManager::Get()->GetVRKeyboard().GetWindow().GetAssignedOverlayID(floating_window_ovrl_state_room));
}

#endif //DPLUS_UI

bool ConfigManager::IsUIAccessEnabled()
{
    std::ifstream file_manifest("DesktopPlus.exe.manifest");

    if (file_manifest.good())
    {
        //Read lines and see if 'uiAccess="true"' can be found, otherwise assume it's not enabled
        std::string line_str;

        while (file_manifest.good())
        {
            std::getline(file_manifest, line_str);

            if (line_str.find("uiAccess=\"true\"") != std::string::npos)
            {
                return true;
            }
        }
    }

    return false;
}

void ConfigManager::RemoveScaleFromTransform(Matrix4& transform, float* width)
{
    Vector3 row_1(transform[0], transform[1], transform[2]);
    float scale_x = row_1.length(); //Scaling is always uniform so we just check the x-axis

    if (scale_x == 0.0f)
        return;

    Vector3 translation = transform.getTranslation();
    transform.setTranslation({0.0f, 0.0f, 0.0f});

    transform.scale(1.0f / scale_x);

    transform.setTranslation(translation);

    //Correct the width value so it gives the same visual result as before
    if (width != nullptr)
        *width *= scale_x;
}

#ifdef DPLUS_UI

void ConfigManager::SaveConfigToFile()
{
    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_newui.ini").c_str() );
    Ini config(wpath.c_str());

    SaveMultiOverlayProfile(config);

    config.WriteString("Interface", "LanguageFile",             m_ConfigString[configid_str_interface_language_file].c_str());
    config.WriteInt(   "Interface", "OverlayCurrentID",         m_ConfigInt[configid_int_interface_overlay_current_id]);
    config.WriteInt(   "Interface", "DesktopButtonCyclingMode", m_ConfigInt[configid_int_interface_mainbar_desktop_listing]);
    config.WriteBool(  "Interface", "ShowAdvancedSettings",     m_ConfigBool[configid_bool_interface_show_advanced_settings]);
    config.WriteBool(  "Interface", "DisplaySizeLarge",         m_ConfigBool[configid_bool_interface_large_style]);
    config.WriteBool(  "Interface", "DesktopButtonIncludeAll",  m_ConfigBool[configid_bool_interface_mainbar_desktop_include_all]);

    //Write color string
    std::stringstream ss;
    ss << std::setw(8) << std::setfill('0') << std::hex << pun_cast<unsigned int, int>(m_ConfigInt[configid_int_interface_background_color]);
    config.WriteString("Interface", "EnvironmentBackgroundColor", ss.str().c_str());

    config.WriteInt( "Interface", "EnvironmentBackgroundColorDisplayMode", m_ConfigInt[configid_int_interface_background_color_display_mode]);
    config.WriteBool("Interface", "DimUI",                                 m_ConfigBool[configid_bool_interface_dim_ui]);
    config.WriteBool("Interface", "BlankSpaceDragEnabled",                 m_ConfigBool[configid_bool_interface_blank_space_drag_enabled]);
    config.WriteInt( "Interface", "LastVRUIScale",                     int(m_ConfigFloat[configid_float_interface_last_vr_ui_scale] * 100.0f));
    config.WriteBool("Interface", "WarningCompositorResolutionHidden",     m_ConfigBool[configid_bool_interface_warning_compositor_res_hidden]);
    config.WriteBool("Interface", "WarningCompositorQualityHidden",        m_ConfigBool[configid_bool_interface_warning_compositor_quality_hidden]);
    config.WriteBool("Interface", "WarningProcessElevationHidden",         m_ConfigBool[configid_bool_interface_warning_process_elevation_hidden]);
    config.WriteBool("Interface", "WarningElevatedModeHidden",             m_ConfigBool[configid_bool_interface_warning_elevated_mode_hidden]);
    config.WriteBool("Interface", "WarningWelcomeHidden",                  m_ConfigBool[configid_bool_interface_warning_welcome_hidden]);

    //Only write WMR settings when they're not -1 since they get set to that when using a non-WMR system. We want to preserve them for HMD-switching users
    if (m_ConfigInt[configid_int_interface_wmr_ignore_vscreens] != -1)
        config.WriteInt("Interface", "WMRIgnoreVScreens", m_ConfigInt[configid_int_interface_wmr_ignore_vscreens]);

    //Save action order
    ss = std::stringstream();

    for (auto& data : m_ActionManager.GetActionMainBarOrder())
    {
        ss << data.action_id << ' ' << data.visible << ";";
    }

    SaveConfigPersistentWindowState(config);

    config.WriteString("Interface", "ActionOrder", ss.str().c_str());

    config.WriteInt( "Input",  "GoHomeButtonActionID",               m_ConfigInt[configid_int_input_go_home_action_id]);
    config.WriteInt( "Input",  "GoBackButtonActionID",               m_ConfigInt[configid_int_input_go_back_action_id]);
    config.WriteInt( "Input",  "GlobalShortcut01ActionID",           m_ConfigInt[configid_int_input_shortcut01_action_id]);
    config.WriteInt( "Input",  "GlobalShortcut02ActionID",           m_ConfigInt[configid_int_input_shortcut02_action_id]);
    config.WriteInt( "Input",  "GlobalShortcut03ActionID",           m_ConfigInt[configid_int_input_shortcut03_action_id]);

    config.WriteInt( "Input",  "GlobalHotkey01Modifiers",            m_ConfigInt[configid_int_input_hotkey01_modifiers]);
    config.WriteInt( "Input",  "GlobalHotkey01KeyCode",              m_ConfigInt[configid_int_input_hotkey01_keycode]);
    config.WriteInt( "Input",  "GlobalHotkey01ActionID",             m_ConfigInt[configid_int_input_hotkey01_action_id]);
    config.WriteInt( "Input",  "GlobalHotkey02Modifiers",            m_ConfigInt[configid_int_input_hotkey02_modifiers]);
    config.WriteInt( "Input",  "GlobalHotkey02KeyCode",              m_ConfigInt[configid_int_input_hotkey02_keycode]);
    config.WriteInt( "Input",  "GlobalHotkey02ActionID",             m_ConfigInt[configid_int_input_hotkey02_action_id]);
    config.WriteInt( "Input",  "GlobalHotkey03Modifiers",            m_ConfigInt[configid_int_input_hotkey03_modifiers]);
    config.WriteInt( "Input",  "GlobalHotkey03KeyCode",              m_ConfigInt[configid_int_input_hotkey03_keycode]);
    config.WriteInt( "Input",  "GlobalHotkey03ActionID",             m_ConfigInt[configid_int_input_hotkey03_action_id]);

    config.WriteInt( "Input",  "DetachedInteractionMaxDistance", int(m_ConfigFloat[configid_float_input_detached_interaction_max_distance] * 100.0f));
    config.WriteBool("Input",  "GlobalHMDPointer",                   m_ConfigBool[configid_bool_input_global_hmd_pointer]);
    config.WriteInt( "Input",  "GlobalHMDPointerMaxDistance",    int(m_ConfigFloat[configid_float_input_global_hmd_pointer_max_distance] * 100.0f));
    config.WriteBool("Input",  "LaserPointerBlockInput",             m_ConfigBool[configid_bool_input_laser_pointer_block_input]);
    config.WriteBool("Input",  "DragForceUpright",                   m_ConfigBool[configid_bool_input_drag_force_upright]);

    config.WriteBool("Mouse", "RenderCursor",              m_ConfigBool[configid_bool_input_mouse_render_cursor]);
    config.WriteBool("Mouse", "RenderIntersectionBlob",    m_ConfigBool[configid_bool_input_mouse_render_intersection_blob]);
    config.WriteBool("Mouse", "AllowPointerOverride",      m_ConfigBool[configid_bool_input_mouse_allow_pointer_override]);
    config.WriteInt( "Mouse", "DoubleClickAssistDuration", m_ConfigInt[configid_int_input_mouse_dbl_click_assist_duration_ms]);

    config.WriteString("Keyboard", "LayoutFile",                m_ConfigString[configid_str_input_keyboard_layout_file].c_str());
    config.WriteBool("Keyboard", "LayoutClusterFunction",       m_ConfigBool[configid_bool_input_keyboard_cluster_function_enabled]);
    config.WriteBool("Keyboard", "LayoutClusterNavigation",     m_ConfigBool[configid_bool_input_keyboard_cluster_navigation_enabled]);
    config.WriteBool("Keyboard", "LayoutClusterNumpad",         m_ConfigBool[configid_bool_input_keyboard_cluster_numpad_enabled]);
    config.WriteBool("Keyboard", "LayoutClusterExtra",          m_ConfigBool[configid_bool_input_keyboard_cluster_extra_enabled]);
    config.WriteBool("Keyboard", "StickyModifiers",             m_ConfigBool[configid_bool_input_keyboard_sticky_modifiers]);
    config.WriteBool("Keyboard", "KeyRepeat",                   m_ConfigBool[configid_bool_input_keyboard_key_repeat]);

    config.WriteBool("Windows", "AutoFocusSceneAppDashboard",   m_ConfigBool[configid_bool_windows_auto_focus_scene_app_dashboard]);
    config.WriteBool("Windows", "WinRTAutoFocus",               m_ConfigBool[configid_bool_windows_winrt_auto_focus]);
    config.WriteBool("Windows", "WinRTKeepOnScreen",            m_ConfigBool[configid_bool_windows_winrt_keep_on_screen]);
    config.WriteInt( "Windows", "WinRTDraggingMode",            m_ConfigInt[configid_int_windows_winrt_dragging_mode]);
    config.WriteBool("Windows", "WinRTAutoSizeOverlay",         m_ConfigBool[configid_bool_windows_winrt_auto_size_overlay]);
    config.WriteBool("Windows", "WinRTAutoFocusSceneApp",       m_ConfigBool[configid_bool_windows_winrt_auto_focus_scene_app]);
    config.WriteBool("Windows", "WinRTWindowMatchingStrict",    m_ConfigBool[configid_bool_windows_winrt_window_matching_strict]);
    config.WriteInt("Windows",  "WinRTOnCaptureLost",           m_ConfigInt[configid_int_windows_winrt_capture_lost_behavior]);
    
    config.WriteInt( "Performance", "UpdateLimitMode",                      m_ConfigInt[configid_int_performance_update_limit_mode]);
    config.WriteInt( "Performance", "UpdateLimitMS",                    int(m_ConfigFloat[configid_float_performance_update_limit_ms] * 100.0f));
    config.WriteInt( "Performance", "UpdateLimitFPS",                       m_ConfigInt[configid_int_performance_update_limit_fps]);
    config.WriteBool("Performance", "RapidLaserPointerUpdates",             m_ConfigBool[configid_bool_performance_rapid_laser_pointer_updates]);
    config.WriteBool("Performance", "SingleDesktopMirroring",               m_ConfigBool[configid_bool_performance_single_desktop_mirroring]);
    config.WriteBool("Performance", "PerformanceMonitorStyleLarge",         m_ConfigBool[configid_bool_performance_monitor_large_style]);
    config.WriteBool("Performance", "PerformanceMonitorShowGraphs",         m_ConfigBool[configid_bool_performance_monitor_show_graphs]);
    config.WriteBool("Performance", "PerformanceMonitorShowTime",           m_ConfigBool[configid_bool_performance_monitor_show_time]);
    config.WriteBool("Performance", "PerformanceMonitorShowCPU",            m_ConfigBool[configid_bool_performance_monitor_show_cpu]);
    config.WriteBool("Performance", "PerformanceMonitorShowGPU",            m_ConfigBool[configid_bool_performance_monitor_show_gpu]);
    config.WriteBool("Performance", "PerformanceMonitorShowFPS",            m_ConfigBool[configid_bool_performance_monitor_show_fps]);
    config.WriteBool("Performance", "PerformanceMonitorShowBattery",        m_ConfigBool[configid_bool_performance_monitor_show_battery]);
    config.WriteBool("Performance", "PerformanceMonitorShowTrackers",       m_ConfigBool[configid_bool_performance_monitor_show_trackers]);
    config.WriteBool("Performance", "PerformanceMonitorShowViveWireless",   m_ConfigBool[configid_bool_performance_monitor_show_vive_wireless]);
    config.WriteBool("Performance", "PerformanceMonitorDisableGPUCounters", m_ConfigBool[configid_bool_performance_monitor_disable_gpu_counters]);

    config.WriteBool("Misc", "NoSteam",            m_ConfigBool[configid_bool_misc_no_steam]);
    config.WriteBool("Misc", "UIAccessWasEnabled", (m_ConfigBool[configid_bool_misc_uiaccess_was_enabled] || m_ConfigBool[configid_bool_state_misc_uiaccess_enabled]));

    //Save custom actions
    config.RemoveSection("CustomActions"); //Remove old section first to avoid any leftovers

    auto& custom_actions = m_ActionManager.GetCustomActions();
    int custom_action_count = (int)custom_actions.size();
    config.WriteInt("CustomActions", "Count", custom_action_count);

    for (int i = 0; i < custom_action_count; ++i)
    {
        const CustomAction& action = custom_actions[i];
        std::string action_ini_name = "Action" + std::to_string(i);
        
        config.WriteString("CustomActions", (action_ini_name + "Name").c_str(), action.Name.c_str());
        config.WriteString("CustomActions", (action_ini_name + "FunctionType").c_str(), ActionManager::CustomActionFunctionToString(action.FunctionType));

        switch (action.FunctionType)
        {
            case caction_press_keys:
            {
                config.WriteInt( "CustomActions", (action_ini_name + "KeyCode1").c_str(),    action.KeyCodes[0]);
                config.WriteInt( "CustomActions", (action_ini_name + "KeyCode2").c_str(),    action.KeyCodes[1]);
                config.WriteInt( "CustomActions", (action_ini_name + "KeyCode3").c_str(),    action.KeyCodes[2]);
                config.WriteBool("CustomActions", (action_ini_name + "ToggleKeys").c_str(), (action.IntID == 1));
                break;
            }
            case caction_type_string:
            {
                config.WriteString("CustomActions", (action_ini_name + "TypeString").c_str(), action.StrMain.c_str());
                break;
            }
            case caction_launch_application:
            {
                config.WriteString("CustomActions", (action_ini_name + "ExecutablePath").c_str(), action.StrMain.c_str());
                config.WriteString("CustomActions", (action_ini_name + "ExecutableArg").c_str(),  action.StrArg.c_str());
                break;
            }
            case caction_toggle_overlay_enabled_state:
            {
                config.WriteInt("CustomActions", (action_ini_name + "OverlayID").c_str(), action.IntID);
                break;
            }
        }

        #ifdef DPLUS_UI
            config.WriteString("CustomActions", (action_ini_name + "IconFilename").c_str(), action.IconFilename.c_str());
        #endif
    }

    config.Save();
}

#endif //DPLUS_UI

void ConfigManager::RestoreConfigFromDefault()
{
    //Basically delete the config file and then load it again which will fall back to config_default.ini
    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_newui.ini").c_str() );
    ::DeleteFileW(wpath.c_str());

    LoadConfigFromFile();
}

void ConfigManager::LoadOverlayProfileDefault(bool multi_overlay)
{
    //Multi-Overlay "default" config is loaded from the default configuration file
    if (multi_overlay)
    {
        LoadMultiOverlayProfileFromFile("../config_default.ini", true);     //This path is relative to the absolute profile path used in LoadMultiOverlayProfileFromFile()
        return;
    }
    else
    {
        Ini config(L"");
        LoadOverlayProfile(config); //All read calls will fail end fill in default values as a result
    }
}

bool ConfigManager::LoadMultiOverlayProfileFromFile(const std::string& filename, bool clear_existing_overlays, std::vector<char>* ovrl_inclusion_list)
{
    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "profiles/" + filename).c_str() );

    if (FileExists(wpath.c_str()))
    {
        Ini config(wpath);
        LoadMultiOverlayProfile(config, clear_existing_overlays, ovrl_inclusion_list);
        return true;
    }

    return false;
}

bool ConfigManager::SaveMultiOverlayProfileToFile(const std::string& filename, std::vector<char>* ovrl_inclusion_list)
{
    std::string path = m_ApplicationPath + "profiles/" + filename;
    Ini config(WStringConvertFromUTF8(path.c_str()));

    SaveMultiOverlayProfile(config, ovrl_inclusion_list);
    return config.Save();
}

bool ConfigManager::DeleteOverlayProfile(const std::string& filename)
{
    std::string path = m_ApplicationPath + "profiles/" + filename;
    return (::DeleteFileW(WStringConvertFromUTF8(path.c_str()).c_str()) != 0);
}

#ifdef DPLUS_UI

std::vector<std::string> ConfigManager::GetOverlayProfileList()
{
    std::vector<std::string> list;
    list.emplace_back(TranslationManager::GetString(tstr_SettingsProfilesOverlaysNameDefault));

    const std::wstring wpath = WStringConvertFromUTF8(std::string(m_ApplicationPath + "profiles/*.ini").c_str());
    WIN32_FIND_DATA find_data;
    HANDLE handle_find = ::FindFirstFileW(wpath.c_str(), &find_data);

    if (handle_find != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::string name = StringConvertFromUTF16(find_data.cFileName);
            name = name.substr(0, name.length() - 4);   //Remove extension

            list.push_back(name);
        }
        while (::FindNextFileW(handle_find, &find_data) != 0);

        ::FindClose(handle_find);
    }

    list.emplace_back(TranslationManager::GetString(tstr_SettingsProfilesOverlaysNameNew));

    return list;
}

std::vector< std::pair<std::string, OverlayOrigin> > ConfigManager::GetOverlayProfileOverlayNameList(const std::string& filename)
{
    std::vector< std::pair<std::string, OverlayOrigin> > list;

    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "profiles/" + filename).c_str() );

    if (FileExists(wpath.c_str()))
    {
        Ini config(wpath);
        unsigned int overlay_id = 0;

        std::stringstream ss;
        ss << "Overlay" << overlay_id;

        //Get names and origin from all sequential overlay sections that exist
        while (config.SectionExists(ss.str().c_str()))
        {
            overlay_id++;

            std::string name(config.ReadString(ss.str().c_str(), "Name"));
            OverlayOrigin origin = (OverlayOrigin)config.ReadInt(ss.str().c_str(), "Origin", ovrl_origin_room);

            list.push_back( std::make_pair(name.empty() ? ss.str() : name, origin) );   //Name should never be blank with compatible profiles, but offer alternative just in case

            ss = std::stringstream();
            ss << "Overlay" << overlay_id;
        }
    }

    return list;
}

#endif //ifdef DPLUS_UI

WPARAM ConfigManager::GetWParamForConfigID(ConfigID_Bool id)    //This is a no-op, but for consistencies' sake and in case anything changes there, it still exists
{
    return id;
}

WPARAM ConfigManager::GetWParamForConfigID(ConfigID_Int id)
{
    return id + configid_bool_MAX;
}

WPARAM ConfigManager::GetWParamForConfigID(ConfigID_Float id)
{
    return id + configid_bool_MAX + configid_int_MAX;
}

WPARAM ConfigManager::GetWParamForConfigID(ConfigID_Handle id)
{
    return id + configid_bool_MAX + configid_int_MAX + configid_float_MAX;
}

void ConfigManager::SetValue(ConfigID_Bool configid, bool value)
{
    if (configid < configid_bool_overlay_MAX)
        OverlayManager::Get().GetCurrentConfigData().ConfigBool[configid] = value;
    else if (configid < configid_bool_MAX)
        Get().m_ConfigBool[configid] = value;
}

void ConfigManager::SetValue(ConfigID_Int configid, int value)
{
    if (configid < configid_int_overlay_MAX)
        OverlayManager::Get().GetCurrentConfigData().ConfigInt[configid] = value;
    else if (configid < configid_int_MAX)
        Get().m_ConfigInt[configid] = value;
}

void ConfigManager::SetValue(ConfigID_Float configid, float value)
{
    if (configid < configid_float_overlay_MAX)
        OverlayManager::Get().GetCurrentConfigData().ConfigFloat[configid] = value;
    else if (configid < configid_float_MAX)
        Get().m_ConfigFloat[configid] = value;
}

void ConfigManager::SetValue(ConfigID_Handle configid, uint64_t value)
{
    if (configid < configid_handle_overlay_MAX)
        OverlayManager::Get().GetCurrentConfigData().ConfigHandle[configid] = value;
    else if (configid < configid_handle_MAX)
        Get().m_ConfigHandle[configid] = value;
}

void ConfigManager::SetValue(ConfigID_String configid, const std::string& value)
{
    if (configid < configid_str_overlay_MAX)
        OverlayManager::Get().GetCurrentConfigData().ConfigStr[configid] = value;
    else if (configid < configid_str_MAX)
        Get().m_ConfigString[configid] = value;
}

bool ConfigManager::GetValue(ConfigID_Bool configid)
{
    return (configid < configid_bool_overlay_MAX) ? OverlayManager::Get().GetCurrentConfigData().ConfigBool[configid] : Get().m_ConfigBool[configid];
}

int ConfigManager::GetValue(ConfigID_Int configid)
{
    return (configid < configid_int_overlay_MAX) ? OverlayManager::Get().GetCurrentConfigData().ConfigInt[configid] : Get().m_ConfigInt[configid];
}

float ConfigManager::GetValue(ConfigID_Float configid)
{
    return (configid < configid_float_overlay_MAX) ? OverlayManager::Get().GetCurrentConfigData().ConfigFloat[configid] : Get().m_ConfigFloat[configid];
}

uint64_t ConfigManager::GetValue(ConfigID_Handle configid)
{
    return (configid < configid_handle_overlay_MAX) ? OverlayManager::Get().GetCurrentConfigData().ConfigHandle[configid] : Get().m_ConfigHandle[configid];
}

const std::string& ConfigManager::GetValue(ConfigID_String configid)
{
    return (configid < configid_str_overlay_MAX) ? OverlayManager::Get().GetCurrentConfigData().ConfigStr[configid] : Get().m_ConfigString[configid];
}

bool& ConfigManager::GetRef(ConfigID_Bool configid)
{
    return (configid < configid_bool_overlay_MAX) ? OverlayManager::Get().GetCurrentConfigData().ConfigBool[configid] : Get().m_ConfigBool[configid];
}

int& ConfigManager::GetRef(ConfigID_Int configid)
{
    return (configid < configid_int_overlay_MAX) ? OverlayManager::Get().GetCurrentConfigData().ConfigInt[configid] : Get().m_ConfigInt[configid];
}

float& ConfigManager::GetRef(ConfigID_Float configid)
{
    return (configid < configid_float_overlay_MAX) ? OverlayManager::Get().GetCurrentConfigData().ConfigFloat[configid] : Get().m_ConfigFloat[configid];
}

uint64_t& ConfigManager::GetRef(ConfigID_Handle configid)
{
    return (configid < configid_handle_overlay_MAX) ? OverlayManager::Get().GetCurrentConfigData().ConfigHandle[configid] : Get().m_ConfigHandle[configid];
}

void ConfigManager::ResetConfigStateValues()
{
    std::fill(std::begin(m_ConfigBool) + configid_bool_state_overlay_dragmode,           std::begin(m_ConfigBool) + configid_bool_state_misc_process_started_by_steam, false);
    std::fill(std::begin(m_ConfigInt)  + configid_int_state_overlay_current_id_override, std::begin(m_ConfigInt)  + configid_int_state_performance_duplication_fps,    -1);
    //configid_int_state_interface_desktop_count is not reset
}

ActionManager& ConfigManager::GetActionManager()
{
    return m_ActionManager;
}

std::vector<CustomAction>& ConfigManager::GetCustomActions()
{
    return m_ActionManager.GetCustomActions();
}

std::vector<ActionMainBarOrderData>& ConfigManager::GetActionMainBarOrder()
{
    return m_ActionManager.GetActionMainBarOrder();
}

Matrix4& ConfigManager::GetOverlayDetachedTransform()
{
    return OverlayManager::Get().GetCurrentConfigData().ConfigTransform;
}

const std::string& ConfigManager::GetApplicationPath() const
{
    return m_ApplicationPath;
}

const std::string& ConfigManager::GetExecutableName() const
{
    return m_ExecutableName;
}

bool ConfigManager::IsSteamInstall() const
{
    return m_IsSteamInstall;
}

vr::TrackedDeviceIndex_t ConfigManager::GetPrimaryLaserPointerDevice() const
{
    if (vr::VROverlay() == nullptr)
        return vr::k_unTrackedDeviceIndexInvalid;

    vr::TrackedDeviceIndex_t device_index = vr::VROverlay()->GetPrimaryDashboardDevice();

    //No dashboard device, try Desktop+ laser pointer device
    if (device_index == vr::k_unTrackedDeviceIndexInvalid)
        return (vr::TrackedDeviceIndex_t)m_ConfigInt[configid_int_state_dplus_laser_pointer_device];

    return device_index;
}

bool ConfigManager::IsLaserPointerTargetOverlay(vr::VROverlayHandle_t ulOverlayHandle) const
{
    if (vr::VROverlay() == nullptr)
        return false;

    bool ret = vr::VROverlay()->IsHoverTargetOverlay(ulOverlayHandle);

    if (!ret)
        return (ulOverlayHandle == m_ConfigHandle[configid_handle_state_dplus_laser_pointer_target_overlay]);

    return ret;
}
