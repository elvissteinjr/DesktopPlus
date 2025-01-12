#include "ConfigManager.h"

#include <algorithm>
#include <sstream>
#include <fstream>

#include "Util.h"
#include "OpenVRExt.h"
#include "Logging.h"
#include "Ini.h"
#include "OverlayManager.h"
#include "InterprocessMessaging.h"
#include "WindowManager.h"
#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"

#ifdef DPLUS_UI
    #include "UIManager.h"
    #include "TranslationManager.h"
#else
    #include "WindowManager.h"
#endif

static ConfigManager g_ConfigManager;
static const std::string g_EmptyString;       //This way we can still return a const reference. Worth it? iunno

static const std::pair<OverlayOrigin, const char*> g_OvrlOriginConfigFileStrings[] = 
{
    {ovrl_origin_room,            "Room"}, 
    {ovrl_origin_hmd_floor,       "HMDFloor"}, 
    {ovrl_origin_seated_universe, "SeatedUniverse"}, 
    {ovrl_origin_dashboard,       "Dashboard"}, 
    {ovrl_origin_hmd,             "HMD"},
    {ovrl_origin_left_hand,       "LeftHand"}, 
    {ovrl_origin_right_hand,      "RightHand"}, 
    {ovrl_origin_aux,             "Aux"},
    {ovrl_origin_theater_screen,  "TheaterScreen"},
    //Legacy config compatibility names (old enum IDs)
    {ovrl_origin_room,            "0"},
    {ovrl_origin_hmd_floor,       "1"}, 
    {ovrl_origin_seated_universe, "2"}, 
    {ovrl_origin_dashboard,       "3"}, 
    {ovrl_origin_hmd,             "4"},
    {ovrl_origin_left_hand,       "5"}, 
    {ovrl_origin_right_hand,      "6"}, 
    {ovrl_origin_aux,             "7"},
};


std::string ConfigHotkey::Serialize() const
{
    std::stringstream ss(std::ios::out | std::ios::binary);

    ss.write((const char*)&KeyCode,   sizeof(KeyCode));
    ss.write((const char*)&Modifiers, sizeof(Modifiers));
    ss.write((const char*)&ActionUID, sizeof(ActionUID));

    return ss.str();
}

void ConfigHotkey::Deserialize(const std::string& str)
{
    std::stringstream ss(str, std::ios::in | std::ios::binary);

    ConfigHotkey new_hotkey;

    ss.read((char*)&new_hotkey.KeyCode,   sizeof(KeyCode));
    ss.read((char*)&new_hotkey.Modifiers, sizeof(Modifiers));
    ss.read((char*)&new_hotkey.ActionUID, sizeof(ActionUID));

    //Replace all data with the read hotkey if there were no stream errors
    if (ss.good())
    {
        *this = new_hotkey;
    }
}


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

    //Assume pen simulation is supported by default, as that's true for most users
    m_ConfigBool[configid_bool_state_pen_simulation_supported] = true;

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

    std::stringstream ss;
    ss << "Overlay" << overlay_id;

    std::string section = ss.str();

    data.ConfigNameStr = config.ReadString(section.c_str(), "Name");

    data.ConfigBool[configid_bool_overlay_name_custom]                  = config.ReadBool(section.c_str(),   "NameIsCustom", false);
    data.ConfigBool[configid_bool_overlay_enabled]                      = config.ReadBool(section.c_str(),   "Enabled", true);
    data.ConfigInt[configid_int_overlay_desktop_id]                     = config.ReadInt(section.c_str(),    "DesktopID", 0);
    data.ConfigInt[configid_int_overlay_capture_source]                 = config.ReadInt(section.c_str(),    "CaptureSource", ovrl_capsource_desktop_duplication);
    data.ConfigInt[configid_int_overlay_duplication_id]                 = config.ReadInt(section.c_str(),    "DuplicationID", -1);
    data.ConfigInt[configid_int_overlay_winrt_desktop_id]               = config.ReadInt(section.c_str(),    "WinRTDesktopID", -2);
    data.ConfigBool[configid_bool_overlay_winrt_window_matching_strict] = config.ReadBool(section.c_str(),   "WinRTWindowMatchingStrict", false);
    data.ConfigStr[configid_str_overlay_winrt_last_window_title]        = config.ReadString(section.c_str(), "WinRTLastWindowTitle");
    data.ConfigStr[configid_str_overlay_winrt_last_window_class_name]   = config.ReadString(section.c_str(), "WinRTLastWindowClassName");
    data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name]     = config.ReadString(section.c_str(), "WinRTLastWindowExeName");
    data.ConfigStr[configid_str_overlay_browser_url]                    = config.ReadString(section.c_str(), "BrowserURL");
    data.ConfigStr[configid_str_overlay_browser_url_user_last]          = config.ReadString(section.c_str(), "BrowserURLUserLast");
    data.ConfigStr[configid_str_overlay_browser_title]                  = config.ReadString(section.c_str(), "BrowserTitle");
    data.ConfigBool[configid_bool_overlay_browser_allow_transparency]   = config.ReadBool(section.c_str(),   "BrowserAllowTransparency", false);
    data.ConfigFloat[configid_float_overlay_width]                      = clamp(config.ReadInt(section.c_str(), "Width", 165)    / 100.0f, 0.00001f, 1000.0f);
    data.ConfigFloat[configid_float_overlay_curvature]                  = clamp(config.ReadInt(section.c_str(), "Curvature", 17) / 100.0f, 0.0f,     1.0f);
    data.ConfigFloat[configid_float_overlay_opacity]                    = config.ReadInt(section.c_str(),    "Opacity", 100) / 100.0f;
    data.ConfigFloat[configid_float_overlay_brightness]                 = config.ReadInt(section.c_str(),    "Brightness", 100) / 100.0f;
    data.ConfigFloat[configid_float_overlay_browser_zoom]               = config.ReadInt(section.c_str(),    "BrowserZoom", 100) / 100.0f;
    data.ConfigFloat[configid_float_overlay_offset_right]               = config.ReadInt(section.c_str(),    "OffsetRight", 0) / 100.0f;
    data.ConfigFloat[configid_float_overlay_offset_up]                  = config.ReadInt(section.c_str(),    "OffsetUp", 0) / 100.0f;
    data.ConfigFloat[configid_float_overlay_offset_forward]             = config.ReadInt(section.c_str(),    "OffsetForward", 0) / 100.0f;
    data.ConfigInt[configid_int_overlay_user_width]                     = config.ReadInt(section.c_str(),    "UserWidth", 1280);
    data.ConfigInt[configid_int_overlay_user_height]                    = config.ReadInt(section.c_str(),    "UserHeight", 720);
    data.ConfigInt[configid_int_overlay_display_mode]                   = config.ReadInt(section.c_str(),    "DisplayMode", ovrl_dispmode_always);
    data.ConfigInt[configid_int_overlay_origin]                         = GetOverlayOriginFromConfigString(config.ReadString(section.c_str(), "Origin"));
    data.ConfigBool[configid_bool_overlay_origin_hmd_floor_use_turning] = config.ReadBool(section.c_str(),   "OriginHMDFloorTurning", false);
    data.ConfigBool[configid_bool_overlay_transform_locked]             = config.ReadBool(section.c_str(),   "TransformLocked", false);

    data.ConfigBool[configid_bool_overlay_crop_enabled]                 = config.ReadBool(section.c_str(), "CroppingEnabled", false);
    data.ConfigInt[configid_int_overlay_crop_x]                         = config.ReadInt(section.c_str(),  "CroppingX", 0);
    data.ConfigInt[configid_int_overlay_crop_y]                         = config.ReadInt(section.c_str(),  "CroppingY", 0);
    data.ConfigInt[configid_int_overlay_crop_width]                     = config.ReadInt(section.c_str(),  "CroppingWidth", -1);
    data.ConfigInt[configid_int_overlay_crop_height]                    = config.ReadInt(section.c_str(),  "CroppingHeight", -1);

    data.ConfigBool[configid_bool_overlay_3D_enabled]                   = config.ReadBool(section.c_str(),   "3DEnabled", false);
    data.ConfigInt[configid_int_overlay_3D_mode]                        = config.ReadInt(section.c_str(),    "3DMode", ovrl_3Dmode_hsbs);
    data.ConfigBool[configid_bool_overlay_3D_swapped]                   = config.ReadBool(section.c_str(),   "3DSwapped", false);
    data.ConfigBool[configid_bool_overlay_gazefade_enabled]             = config.ReadBool(section.c_str(),   "GazeFade", false);
    data.ConfigFloat[configid_float_overlay_gazefade_distance]          = config.ReadInt(section.c_str(),    "GazeFadeDistance", 0) / 100.0f;
    data.ConfigFloat[configid_float_overlay_gazefade_rate]              = config.ReadInt(section.c_str(),    "GazeFadeRate", 100) / 100.0f;
    data.ConfigFloat[configid_float_overlay_gazefade_opacity]           = config.ReadInt(section.c_str(),    "GazeFadeOpacity", 0) / 100.0f;
    data.ConfigInt[configid_int_overlay_update_limit_override_mode]     = config.ReadInt(section.c_str(),    "UpdateLimitModeOverride", update_limit_mode_off);
    data.ConfigFloat[configid_float_overlay_update_limit_override_ms]   = config.ReadInt(section.c_str(),    "UpdateLimitMS", 0) / 100.0f;
    data.ConfigInt[configid_int_overlay_update_limit_override_fps]      = config.ReadInt(section.c_str(),    "UpdateLimitFPS", update_limit_fps_30);
    data.ConfigInt[configid_int_overlay_browser_max_fps_override]       = config.ReadInt(section.c_str(),    "BrowserMaxFPSOverride", -1);
    data.ConfigBool[configid_bool_overlay_input_enabled]                = config.ReadBool(section.c_str(),   "InputEnabled", true);
    data.ConfigBool[configid_bool_overlay_input_dplus_lp_enabled]       = config.ReadBool(section.c_str(),   "InputDPlusLPEnabled", true);
    data.ConfigStr[configid_str_overlay_tags]                           = config.ReadString(section.c_str(), "Tags");
    data.ConfigBool[configid_bool_overlay_update_invisible]             = config.ReadBool(section.c_str(),   "UpdateInvisible", false);

    data.ConfigBool[configid_bool_overlay_floatingui_enabled]           = config.ReadBool(section.c_str(), "ShowFloatingUI", true);
    data.ConfigBool[configid_bool_overlay_floatingui_desktops_enabled]  = config.ReadBool(section.c_str(), "ShowDesktopButtons", false);
    data.ConfigBool[configid_bool_overlay_floatingui_extras_enabled]    = config.ReadBool(section.c_str(), "ShowExtraButtons", true);
    data.ConfigBool[configid_bool_overlay_actionbar_enabled]            = config.ReadBool(section.c_str(), "ShowActionBar", false);
    data.ConfigBool[configid_bool_overlay_actionbar_order_use_global]   = config.ReadBool(section.c_str(), "ActionBarOrderUseGlobal", true);
    data.ConfigActionBarOrder                                           = m_ActionManager.ActionOrderListFromString( config.ReadString(section.c_str(), "ActionBarOrderCustom") );

    bool do_set_auto_name = ( (!data.ConfigBool[configid_bool_overlay_name_custom]) && (data.ConfigNameStr.empty()) );

    //Restore WinRT Capture state if possible
    if ( (data.ConfigInt[configid_int_overlay_winrt_desktop_id] == -2) && (!data.ConfigStr[configid_str_overlay_winrt_last_window_title].empty()) )
    {
        HWND window = WindowInfo::FindClosestWindowForTitle(data.ConfigStr[configid_str_overlay_winrt_last_window_title], data.ConfigStr[configid_str_overlay_winrt_last_window_class_name],
                                                            data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name], WindowManager::Get().WindowListGet(),
                                                            data.ConfigBool[configid_bool_overlay_winrt_window_matching_strict]);

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

    #ifdef DPLUS_UI
    //When loading an UI overlay, send config state over to ensure the correct process has rendering access even if the UI was restarted at some point
    if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui)
    {
        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, (int)overlay_id);
        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_overlay_capture_source, ovrl_capsource_ui);
        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

        UIManager::Get()->GetPerformanceWindow().ScheduleOverlaySharedTextureUpdate();
    }
    else if ( (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser) && (!DPBrowserAPIClient::Get().IsBrowserAvailable()) )
    {
        //Set warning if no browser available but overlays using it are loaded
        m_ConfigBool[configid_bool_state_misc_browser_used_but_missing] = true;
        UIManager::Get()->UpdateAnyWarningDisplayedState();
    }

    //Set auto name if there's a new window match
    if (do_set_auto_name)
    {
        OverlayManager::Get().SetCurrentOverlayNameAuto();
    }

    #endif //DPLUS_UI
}

void ConfigManager::SaveOverlayProfile(Ini& config, unsigned int overlay_id)
{
    const OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

    std::stringstream ss;
    ss << "Overlay" << overlay_id;

    std::string section = ss.str();

    config.WriteString(section.c_str(), "Name", data.ConfigNameStr.c_str());

    config.WriteBool(section.c_str(),   "NameIsCustom",           data.ConfigBool[configid_bool_overlay_name_custom]);
    config.WriteBool(section.c_str(),   "Enabled",                data.ConfigBool[configid_bool_overlay_enabled]);
    config.WriteInt( section.c_str(),   "DesktopID",              data.ConfigInt[configid_int_overlay_desktop_id]);
    config.WriteInt( section.c_str(),   "CaptureSource",          data.ConfigInt[configid_int_overlay_capture_source]);
    config.WriteInt( section.c_str(),   "DuplicationID",          data.ConfigInt[configid_int_overlay_duplication_id]);
    config.WriteInt( section.c_str(),   "Width",              int(data.ConfigFloat[configid_float_overlay_width]           * 100.0f));
    config.WriteInt( section.c_str(),   "Curvature",          int(data.ConfigFloat[configid_float_overlay_curvature]       * 100.0f));
    config.WriteInt( section.c_str(),   "Opacity",            int(data.ConfigFloat[configid_float_overlay_opacity]         * 100.0f));
    config.WriteInt( section.c_str(),   "Brightness",         int(data.ConfigFloat[configid_float_overlay_brightness]      * 100.0f));
    config.WriteInt( section.c_str(),   "BrowserZoom",        int(data.ConfigFloat[configid_float_overlay_browser_zoom]    * 100.0f));
    config.WriteInt( section.c_str(),   "OffsetRight",        int(data.ConfigFloat[configid_float_overlay_offset_right]    * 100.0f));
    config.WriteInt( section.c_str(),   "OffsetUp",           int(data.ConfigFloat[configid_float_overlay_offset_up]       * 100.0f));
    config.WriteInt( section.c_str(),   "OffsetForward",      int(data.ConfigFloat[configid_float_overlay_offset_forward]  * 100.0f));
    config.WriteInt( section.c_str(),   "UserWidth",              data.ConfigInt[configid_int_overlay_user_width]);
    config.WriteInt( section.c_str(),   "UserHeight",             data.ConfigInt[configid_int_overlay_user_height]);
    config.WriteInt( section.c_str(),   "DisplayMode",            data.ConfigInt[configid_int_overlay_display_mode]);
    config.WriteString(section.c_str(), "Origin",                 GetConfigStringForOverlayOrigin((OverlayOrigin)data.ConfigInt[configid_int_overlay_origin]));
    config.WriteBool(section.c_str(),   "OriginHMDFloorTurning",  data.ConfigBool[configid_bool_overlay_origin_hmd_floor_use_turning]);
    config.WriteBool(section.c_str(),   "TransformLocked",        data.ConfigBool[configid_bool_overlay_transform_locked]);

    config.WriteBool(section.c_str(), "CroppingEnabled",        data.ConfigBool[configid_bool_overlay_crop_enabled]);
    config.WriteInt( section.c_str(), "CroppingX",              data.ConfigInt[configid_int_overlay_crop_x]);
    config.WriteInt( section.c_str(), "CroppingY",              data.ConfigInt[configid_int_overlay_crop_y]);
    config.WriteInt( section.c_str(), "CroppingWidth",          data.ConfigInt[configid_int_overlay_crop_width]);
    config.WriteInt( section.c_str(), "CroppingHeight",         data.ConfigInt[configid_int_overlay_crop_height]);

    config.WriteBool(section.c_str(),   "3DEnabled",              data.ConfigBool[configid_bool_overlay_3D_enabled]);
    config.WriteInt( section.c_str(),   "3DMode",                 data.ConfigInt[configid_int_overlay_3D_mode]);
    config.WriteBool(section.c_str(),   "3DSwapped",              data.ConfigBool[configid_bool_overlay_3D_swapped]);
    config.WriteBool(section.c_str(),   "GazeFade",               data.ConfigBool[configid_bool_overlay_gazefade_enabled]);
    config.WriteInt( section.c_str(),   "GazeFadeDistance",   int(data.ConfigFloat[configid_float_overlay_gazefade_distance] * 100.0f));
    config.WriteInt( section.c_str(),   "GazeFadeRate",       int(data.ConfigFloat[configid_float_overlay_gazefade_rate]     * 100.0f));
    config.WriteInt( section.c_str(),   "GazeFadeOpacity",    int(data.ConfigFloat[configid_float_overlay_gazefade_opacity]  * 100.0f));
    config.WriteInt( section.c_str(),   "UpdateLimitModeOverride",data.ConfigInt[configid_int_overlay_update_limit_override_mode]);
    config.WriteInt( section.c_str(),   "UpdateLimitMS",      int(data.ConfigFloat[configid_float_overlay_update_limit_override_ms] * 100.0f));
    config.WriteInt( section.c_str(),   "UpdateLimitFPS",         data.ConfigInt[configid_int_overlay_update_limit_override_fps]);
    config.WriteInt( section.c_str(),   "BrowserMaxFPSOverride",  data.ConfigInt[configid_int_overlay_browser_max_fps_override]);
    config.WriteBool(section.c_str(),   "InputEnabled",           data.ConfigBool[configid_bool_overlay_input_enabled]);
    config.WriteBool(section.c_str(),   "InputDPlusLPEnabled",    data.ConfigBool[configid_bool_overlay_input_dplus_lp_enabled]);
    config.WriteString(section.c_str(), "Tags",                   data.ConfigStr[configid_str_overlay_tags].c_str());
    config.WriteBool(section.c_str(),   "UpdateInvisible",        data.ConfigBool[configid_bool_overlay_update_invisible]);

    config.WriteBool(  section.c_str(), "ShowFloatingUI",          data.ConfigBool[configid_bool_overlay_floatingui_enabled]);
    config.WriteBool(  section.c_str(), "ShowDesktopButtons",      data.ConfigBool[configid_bool_overlay_floatingui_desktops_enabled]);
    config.WriteBool(  section.c_str(), "ShowExtraButtons",        data.ConfigBool[configid_bool_overlay_floatingui_extras_enabled]);
    config.WriteBool(  section.c_str(), "ShowActionBar",           data.ConfigBool[configid_bool_overlay_actionbar_enabled]);
    config.WriteBool(  section.c_str(), "ActionBarOrderUseGlobal", data.ConfigBool[configid_bool_overlay_actionbar_order_use_global]);
    config.WriteString(section.c_str(), "ActionBarOrderCustom",    ActionManager::ActionOrderListToString(data.ConfigActionBarOrder).c_str());

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

    config.WriteString(section.c_str(), "WinRTLastWindowTitle",      last_window_title.c_str());
    config.WriteString(section.c_str(), "WinRTLastWindowClassName",  last_window_class_name.c_str());
    config.WriteString(section.c_str(), "WinRTLastWindowExeName",    last_window_exe_name.c_str());
    config.WriteInt(   section.c_str(), "WinRTDesktopID",            data.ConfigInt[configid_int_overlay_winrt_desktop_id]);
    config.WriteBool(  section.c_str(), "WinRTWindowMatchingStrict", data.ConfigBool[configid_bool_overlay_winrt_window_matching_strict]);

    //Browser
    config.WriteString(section.c_str(), "BrowserURL",               data.ConfigStr[configid_str_overlay_browser_url].c_str());
    config.WriteString(section.c_str(), "BrowserURLUserLast",       data.ConfigStr[configid_str_overlay_browser_url_user_last].c_str());
    config.WriteString(section.c_str(), "BrowserTitle",             data.ConfigStr[configid_str_overlay_browser_title].c_str());
    config.WriteBool(  section.c_str(), "BrowserAllowTransparency", data.ConfigBool[configid_bool_overlay_browser_allow_transparency]);
}

bool ConfigManager::LoadConfigFromFile()
{
    LOG_F(INFO, "Loading config...");

    //Prioritize config_newui.ini if it exists (will be deleted on save to rename)
    bool using_config_newui_file = true;
    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_newui.ini").c_str() );

    bool existed = FileExists(wpath.c_str());
    if (!existed)
    {
        wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config.ini").c_str() );
        existed = FileExists(wpath.c_str());
        using_config_newui_file = false;
    }

    //If config.ini doesn't exist (yet), load from config_default.ini instead, which hopefully does (would still work to a lesser extent though)
    if (!existed)
    {
        wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_default.ini").c_str() );
        LOG_F(INFO, "Config file not found. Loading default config file instead");
    }

    Ini config(wpath.c_str());

    const int config_version = config.ReadInt("Misc", "ConfigVersion", 1);

    //If config is versioned below 2 (or unversioned, really), assume legacy config
    //Though if we're loading from config_newui.ini (not legacy config but no version key yet), we're only doing the renaming, nothing else
    if ((existed) && (config_version < 2))
    {
        #ifdef DPLUS_UI
            MigrateLegacyConfig(config, using_config_newui_file);
            //We need to save and restart dashboard process, but only after we read everything else
        #else   //Only UI process writes files, so dashboard process will have to wait and restart later
            LOG_IF_F(INFO, !using_config_newui_file, "Legacy config detected, expecting UI process to migrate and request restart...");
        #endif
    }

    //Do the actual config reading
    m_ConfigBool[configid_bool_interface_no_ui]                            = config.ReadBool(  "Interface", "NoUIAutoLaunch", false);
    m_ConfigBool[configid_bool_interface_no_notification_icon]             = config.ReadBool(  "Interface", "NoNotificationIcon", false);
    m_ConfigString[configid_str_interface_language_file]                   = config.ReadString("Interface", "LanguageFile");
    m_ConfigBool[configid_bool_interface_show_advanced_settings]           = config.ReadBool(  "Interface", "ShowAdvancedSettings", false);
    m_ConfigBool[configid_bool_interface_large_style]                      = config.ReadBool(  "Interface", "DisplaySizeLarge", false);
    m_ConfigInt[configid_int_interface_overlay_current_id]                 = config.ReadInt(   "Interface", "OverlayCurrentID", 0);
    m_ConfigInt[configid_int_interface_desktop_listing_style]              = config.ReadInt(   "Interface", "DesktopButtonCyclingMode", desktop_listing_style_individual);
    m_ConfigBool[configid_bool_interface_desktop_buttons_include_combined] = config.ReadBool(  "Interface", "DesktopButtonIncludeAll", false);

    //Read color string as unsigned int but store it as signed
    m_ConfigInt[configid_int_interface_background_color] = pun_cast<unsigned int, int>( std::stoul(config.ReadString("Interface", "EnvironmentBackgroundColor", "00000080"), nullptr, 16) );

    m_ConfigInt[configid_int_interface_background_color_display_mode]             = config.ReadInt( "Interface", "EnvironmentBackgroundColorDisplayMode", ui_bgcolor_dispmode_never);
    m_ConfigBool[configid_bool_interface_dim_ui]                                  = config.ReadBool("Interface", "DimUI", false);
    m_ConfigBool[configid_bool_interface_blank_space_drag_enabled]                = config.ReadBool("Interface", "BlankSpaceDragEnabled", true);
    m_ConfigFloat[configid_float_interface_last_vr_ui_scale]                      = config.ReadInt( "Interface", "LastVRUIScale", 100) / 100.0f;
    m_ConfigFloat[configid_float_interface_desktop_ui_scale_override]             = config.ReadInt( "Interface", "DesktopUIScaleOverride", 0) / 100.0f;
    m_ConfigBool[configid_bool_interface_warning_compositor_res_hidden]           = config.ReadBool("Interface", "WarningCompositorResolutionHidden",   false);
    m_ConfigBool[configid_bool_interface_warning_compositor_quality_hidden]       = config.ReadBool("Interface", "WarningCompositorQualityHidden",      false);
    m_ConfigBool[configid_bool_interface_warning_process_elevation_hidden]        = config.ReadBool("Interface", "WarningProcessElevationHidden",       false);
    m_ConfigBool[configid_bool_interface_warning_elevated_mode_hidden]            = config.ReadBool("Interface", "WarningElevatedModeHidden",           false);
    m_ConfigBool[configid_bool_interface_warning_browser_missing_hidden]          = config.ReadBool("Interface", "WarningBrowserMissingHidden",         false);
    m_ConfigBool[configid_bool_interface_warning_browser_version_mismatch_hidden] = config.ReadBool("Interface", "WarningBrowserVersionMismatchHidden", false);
    m_ConfigBool[configid_bool_interface_warning_app_profile_active_hidden]       = config.ReadBool("Interface", "WarningAppProfileActiveHidden",       false);
    m_ConfigBool[configid_bool_interface_window_settings_restore_state]           = config.ReadBool("Interface", "WindowSettingsRestoreState",   false);
    m_ConfigBool[configid_bool_interface_window_properties_restore_state]         = config.ReadBool("Interface", "WindowPropertiesRestoreState", false);
    m_ConfigBool[configid_bool_interface_window_keyboard_restore_state]           = config.ReadBool("Interface", "WindowKeyboardRestoreState",   true);
    m_ConfigBool[configid_bool_interface_quick_start_hidden]                      = config.ReadBool("Interface", "QuickStartGuideHidden",        false);
    m_ConfigInt[configid_int_interface_wmr_ignore_vscreens]                       = config.ReadInt( "Interface", "WMRIgnoreVScreens", -1);

    OverlayManager::Get().SetCurrentOverlayID(m_ConfigInt[configid_int_interface_overlay_current_id]);

    #ifdef DPLUS_UI
        TranslationManager::Get().LoadTranslationFromFile( m_ConfigString[configid_str_interface_language_file].c_str() );
        LoadConfigPersistentWindowState(config);
    #endif

    m_ConfigHandle[configid_handle_input_go_home_action_uid] = std::strtoull(config.ReadString("Input", "GoHomeButtonActionUID", "0").c_str(), nullptr, 10);
    m_ConfigHandle[configid_handle_input_go_back_action_uid] = std::strtoull(config.ReadString("Input", "GoBackButtonActionUID", "0").c_str(), nullptr, 10);

    //Global Shortcuts
    m_ConfigInt[configid_int_input_global_shortcuts_max_count] = config.ReadInt("Input", "GlobalShortcutsMaxCount", 20);

    m_ConfigGlobalShortcuts.clear();
    int shortcut_id = 0;
    for (;;)
    {
        std::stringstream ss;
        ss << "GlobalShortcut" << std::setfill('0') << std::setw(2) << shortcut_id + 1 << "ActionUID";   //Naming pattern is backwards-compatible to legacy shortcut 01-06 entries

        if (config.KeyExists("Input", ss.str().c_str()))
        {
            m_ConfigGlobalShortcuts.push_back(std::strtoull(config.ReadString("Input", ss.str().c_str(), "0").c_str(), nullptr, 10));
        }
        else
        {
            break;
        }

        ++shortcut_id;
    }

    if (m_ConfigGlobalShortcuts.empty())    //Enforce minimum one entry
    {
        m_ConfigGlobalShortcuts.emplace_back();
    }

    //Hotkeys
    m_ConfigHotkey.clear();
    int hotkey_id = 0;
    for (;;)
    {
        std::stringstream ss;
        ss << "GlobalHotkey" << std::setfill('0') << std::setw(2) << hotkey_id + 1;   //Naming pattern is backwards-compatible to legacy hotkey 01-03 entries

        if (config.KeyExists("Input", (ss.str() + "Modifiers").c_str()))
        {
            ConfigHotkey hotkey;
            hotkey.Modifiers = config.ReadInt("Input", (ss.str() + "Modifiers").c_str(), 0);
            hotkey.KeyCode   = config.ReadInt("Input", (ss.str() + "KeyCode"  ).c_str(), 0);
            hotkey.ActionUID = std::strtoull(config.ReadString("Input", (ss.str() + "ActionUID").c_str(), "0").c_str(), nullptr, 10);

            m_ConfigHotkey.push_back(hotkey);
        }
        else
        {
            break;
        }

        ++hotkey_id;
    }

    if (m_ConfigHotkey.empty())    //Enforce minimum one entry
    {
        m_ConfigHotkey.emplace_back();
    }

    m_ConfigFloat[configid_float_input_detached_interaction_max_distance]   = config.ReadInt( "Input", "DetachedInteractionMaxDistance", 200) / 100.0f;
    m_ConfigBool[configid_bool_input_laser_pointer_block_input]             = config.ReadBool("Input", "LaserPointerBlockInput", false);
    m_ConfigBool[configid_bool_input_laser_pointer_hmd_device]              = config.ReadBool("Input", "GlobalHMDPointer", false);
    m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_toggle] = config.ReadInt( "Input", "LaserPointerHMDKeyCodeToggle", 0);
    m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_left]   = config.ReadInt( "Input", "LaserPointerHMDKeyCodeLeft",   0);
    m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_right]  = config.ReadInt( "Input", "LaserPointerHMDKeyCodeRight",  0);
    m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_middle] = config.ReadInt( "Input", "LaserPointerHMDKeyCodeMiddle", 0);
    m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_drag]   = config.ReadInt( "Input", "LaserPointerHMDKeyCodeDrag",   0);

    m_ConfigBool[configid_bool_input_drag_auto_docking]                     = config.ReadBool("Input", "DragAutoDocking", true);
    m_ConfigBool[configid_bool_input_drag_force_upright]                    = config.ReadBool("Input", "DragForceUpright", false);
    m_ConfigBool[configid_bool_input_drag_fixed_distance]                   = config.ReadBool("Input", "DragFixedDistance", false);
    m_ConfigFloat[configid_float_input_drag_fixed_distance_m]               = config.ReadInt( "Input", "DragFixedDistanceCM", 200) / 100.0f;
    m_ConfigInt[configid_int_input_drag_fixed_distance_shape]               = config.ReadInt( "Input", "DragFixedDistanceShape", 0);
    m_ConfigBool[configid_bool_input_drag_fixed_distance_auto_curve]        = config.ReadBool("Input", "DragFixedDistanceAutoCurve", true);
    m_ConfigBool[configid_bool_input_drag_fixed_distance_auto_tilt]         = config.ReadBool("Input", "DragFixedDistanceAutoTilt", true);
    m_ConfigBool[configid_bool_input_drag_snap_position]                    = config.ReadBool("Input", "DragSnapPosition", false);
    m_ConfigFloat[configid_float_input_drag_snap_position_size]             = config.ReadInt( "Input", "DragSnapPositionSize", 10) / 100.0f;

    m_ConfigBool[configid_bool_input_mouse_render_cursor]                   = config.ReadBool("Mouse", "RenderCursor", true);
    m_ConfigBool[configid_bool_input_mouse_render_intersection_blob]        = config.ReadBool("Mouse", "RenderIntersectionBlob", false);
    m_ConfigBool[configid_bool_input_mouse_scroll_smooth]                   = config.ReadBool("Mouse", "ScrollSmooth", false);
    m_ConfigBool[configid_bool_input_mouse_allow_pointer_override]          = config.ReadBool("Mouse", "AllowPointerOverride", true);
    m_ConfigBool[configid_bool_input_mouse_simulate_pen_input]              = config.ReadBool("Mouse", "SimulatePenInput", false);
    m_ConfigInt[configid_int_input_mouse_dbl_click_assist_duration_ms]      = config.ReadInt( "Mouse", "DoubleClickAssistDuration", -1);
    m_ConfigInt[configid_int_input_mouse_input_smoothing_level]             = config.ReadInt( "Mouse", "InputSmoothingLevel", 0);

    m_ConfigString[configid_str_input_keyboard_layout_file]                 = config.ReadString("Keyboard", "LayoutFile", "qwerty_usa.ini");
    m_ConfigBool[configid_bool_input_keyboard_cluster_function_enabled]     = config.ReadBool("Keyboard", "LayoutClusterFunction",   true);
    m_ConfigBool[configid_bool_input_keyboard_cluster_navigation_enabled]   = config.ReadBool("Keyboard", "LayoutClusterNavigation", true);
    m_ConfigBool[configid_bool_input_keyboard_cluster_numpad_enabled]       = config.ReadBool("Keyboard", "LayoutClusterNumpad",     false);
    m_ConfigBool[configid_bool_input_keyboard_cluster_extra_enabled]        = config.ReadBool("Keyboard", "LayoutClusterExtra",      false);
    m_ConfigBool[configid_bool_input_keyboard_sticky_modifiers]             = config.ReadBool("Keyboard", "StickyModifiers",         true);
    m_ConfigBool[configid_bool_input_keyboard_key_repeat]                   = config.ReadBool("Keyboard", "KeyRepeat",               true);
    m_ConfigBool[configid_bool_input_keyboard_auto_show_desktop]            = config.ReadBool("Keyboard", "AutoShowDesktop",         true);
    m_ConfigBool[configid_bool_input_keyboard_auto_show_browser]            = config.ReadBool("Keyboard", "AutoShowBrowser",         true);

    m_ConfigBool[configid_bool_windows_auto_focus_scene_app_dashboard]      = config.ReadBool("Windows", "AutoFocusSceneAppDashboard", false);
    m_ConfigBool[configid_bool_windows_winrt_auto_focus]                    = config.ReadBool("Windows", "WinRTAutoFocus", true);
    m_ConfigBool[configid_bool_windows_winrt_keep_on_screen]                = config.ReadBool("Windows", "WinRTKeepOnScreen", true);
    m_ConfigInt[configid_int_windows_winrt_dragging_mode]                   = config.ReadInt( "Windows", "WinRTDraggingMode", window_dragging_overlay);
    m_ConfigBool[configid_bool_windows_winrt_auto_size_overlay]             = config.ReadBool("Windows", "WinRTAutoSizeOverlay", false);
    m_ConfigBool[configid_bool_windows_winrt_auto_focus_scene_app]          = config.ReadBool("Windows", "WinRTAutoFocusSceneApp", false);
    m_ConfigInt[configid_int_windows_winrt_capture_lost_behavior]           = config.ReadInt( "Windows", "WinRTOnCaptureLost", window_caplost_hide_overlay);

    m_ConfigString[configid_str_browser_extra_arguments]                    = config.ReadString("Browser", "CommandLineArguments");
    m_ConfigInt[configid_int_browser_max_fps]                               = config.ReadInt(   "Browser", "BrowserMaxFPS", 60);
    m_ConfigBool[configid_bool_browser_content_blocker]                     = config.ReadBool(  "Browser", "BrowserContentBlocker", false);

    m_ConfigInt[configid_int_performance_update_limit_mode]                 = config.ReadInt( "Performance", "UpdateLimitMode", update_limit_mode_off);
    m_ConfigFloat[configid_float_performance_update_limit_ms]               = config.ReadInt( "Performance", "UpdateLimitMS", 0) / 100.0f;
    m_ConfigInt[configid_int_performance_update_limit_fps]                  = config.ReadInt( "Performance", "UpdateLimitFPS", update_limit_fps_30);
    m_ConfigBool[configid_bool_performance_rapid_laser_pointer_updates]     = config.ReadBool("Performance", "RapidLaserPointerUpdates", false);
    m_ConfigBool[configid_bool_performance_single_desktop_mirroring]        = config.ReadBool("Performance", "SingleDesktopMirroring", false);
    m_ConfigBool[configid_bool_performance_hdr_mirroring]                   = config.ReadBool("Performance", "HDRMirroring", false);
    m_ConfigBool[configid_bool_performance_show_fps]                        = config.ReadBool("Performance", "ShowFPS", false);
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

    //Load actions
    if (!m_ActionManager.LoadActionsFromFile())
    {
        MigrateLegacyActionsFromConfig(config);
    }

    //Load action order lists (needs to happen after load as the UIDs are validated for existence)
    #ifdef DPLUS_UI
        m_ActionManager.SetActionOrderListUI(         m_ActionManager.ActionOrderListFromString( config.ReadString("Interface", "ActionOrder") ));
        m_ActionManager.SetActionOrderListBarDefault( m_ActionManager.ActionOrderListFromString( config.ReadString("Interface", "ActionOrderBarDefault") ));
        m_ActionManager.SetActionOrderListOverlayBar( m_ActionManager.ActionOrderListFromString( config.ReadString("Interface", "ActionOrderOverlayBar") ));
    #endif

    //Validate action IDs for controller bindings too
    if (!m_ActionManager.ActionExists(m_ConfigHandle[configid_handle_input_go_home_action_uid]))
    {
        m_ConfigHandle[configid_handle_input_go_home_action_uid] = k_ActionUID_Invalid;
    }
    if (!m_ActionManager.ActionExists(m_ConfigHandle[configid_handle_input_go_back_action_uid]))
    {
        m_ConfigHandle[configid_handle_input_go_back_action_uid] = k_ActionUID_Invalid;
    }

    shortcut_id = 0;
    for (ActionUID& uid : m_ConfigGlobalShortcuts)
    {
        if ((uid != k_ActionUID_Invalid) && (!m_ActionManager.ActionExists(uid)))
        {
            LOG_F(WARNING, "Global Shortcut %02d is referencing unknown action %llu, resetting to [None]", shortcut_id + 1, uid);
            uid = k_ActionUID_Invalid;
        }

        ++shortcut_id;
    }

    //Validate hotkey ActionUIDs
    hotkey_id = 0;
    for (ConfigHotkey& hotkey : m_ConfigHotkey)
    {
        if ((hotkey.ActionUID != k_ActionUID_Invalid) && (!m_ActionManager.ActionExists(hotkey.ActionUID)))
        {
            LOG_F(WARNING, "Hotkey %02d is referencing unknown action %llu, resetting to [None]", hotkey_id + 1, hotkey.ActionUID);
            hotkey.ActionUID = k_ActionUID_Invalid;
        }

        ++hotkey_id;
    }

    #ifndef DPLUS_UI
        //Apply render cursor setting for WinRT Capture
        if (DPWinRT_IsCaptureCursorEnabledPropertySupported())
        {
            DPWinRT_SetCaptureCursorEnabled(m_ConfigBool[configid_bool_input_mouse_render_cursor]);
        }

        DPWinRT_SetHDREnabled(m_ConfigBool[configid_bool_performance_hdr_mirroring]);

        //Apply global settings for DPBrowser
        if (DPBrowserAPIClient::Get().IsBrowserAvailable())
        {
            DPBrowserAPIClient::Get().DPBrowser_GlobalSetFPS(m_ConfigInt[configid_int_browser_max_fps]);
            DPBrowserAPIClient::Get().DPBrowser_ContentBlockSetEnabled(m_ConfigBool[configid_bool_browser_content_blocker]);
        }
    #endif

    //Set WindowManager active (no longer gets deactivated during runtime)
    WindowManager::Get().SetActive(true);

    //Query elevated mode state
    m_ConfigBool[configid_bool_state_misc_elevated_mode_active] = IPCManager::IsElevatedModeProcessRunning();
    LOG_IF_F(INFO, m_ConfigBool[configid_bool_state_misc_elevated_mode_active], "Elevated mode is active");

    #ifdef DPLUS_UI
        UIManager::Get()->GetSettingsWindow().ApplyCurrentOverlayState();
        UIManager::Get()->GetOverlayPropertiesWindow().ApplyCurrentOverlayState();
        UIManager::Get()->GetVRKeyboard().LoadCurrentLayout();
        UIManager::Get()->GetVRKeyboard().GetWindow().ApplyCurrentOverlayState();

        UIManager::Get()->UpdateAnyWarningDisplayedState();
    #endif

    //Load last used overlay config
    LoadMultiOverlayProfile(config);

    m_AppProfileManager.LoadProfilesFromFile();

    LOG_F(INFO, "Loaded config");

    //Save after loading if we did a config migration
    #ifdef DPLUS_UI
        if (m_ConfigBool[configid_bool_state_misc_config_migrated])
        {
            if (IPCManager::Get().IsDashboardAppRunning())
            {
                LOG_F(INFO, "Legacy config migration finished, saving new config and restarting dashboard app...");
                SaveConfigToFile();
                UIManager::Get()->RestartDashboardApp();
            }
            else
            {
                LOG_F(INFO, "Legacy config migration finished, saving new config...");
                SaveConfigToFile();
            }
        }
    #endif

    return existed; //We use default values if it doesn't, but still return if the file existed
}

void ConfigManager::LoadMultiOverlayProfile(const Ini& config, bool clear_existing_overlays, std::vector<char>* ovrl_inclusion_list)
{
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();

    if (clear_existing_overlays)
    {
        OverlayManager::Get().RemoveAllOverlays();

        //Reset browser missing warning. It'll get set again if appropriate.
        //Other cases are unhandled (e.g. only removing offending overlay) as it'd require littering checks everywhere for little use
        m_ConfigBool[configid_bool_state_misc_browser_used_but_missing] = false;
    }

    const int config_version = config.ReadInt("Misc", "ConfigVersion", 1);

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

    config.WriteInt("Misc", "ConfigVersion", k_nDesktopPlusConfigVersion);

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

        //Skip if set to not restore state
        if (!m_ConfigBool[configid_bool_interface_window_settings_restore_state + i])
            continue;

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

void ConfigManager::MigrateLegacyConfig(Ini& config, bool only_rename_config_file)
{
    if (!only_rename_config_file)
    {
        LOG_F(INFO, "Migrating legacy config...");

        //Do action migration first so we have the legacy ActionID to ActionUID mapping from it
        LegacyActionIDtoActionUID legacy_id_to_uid;
        if (!m_ActionManager.LoadActionsFromFile())
        {
            legacy_id_to_uid = MigrateLegacyActionsFromConfig(config);
        }

        //Rename and migrate action order key
        config.WriteString("Interface", "ActionOrderBarDefault", MigrateLegacyActionOrderString(config.ReadString("Interface", "ActionOrder"), legacy_id_to_uid).c_str() );
        config.RemoveKey("Interface", "ActionOrder");

        //Nothing else really has to be adapted from the global config, but we need to migrate every overlay profile
        bool apply_steamvr2_dashboard_offset = config.ReadBool("Misc", "ApplySteamVR2DashboardOffset", true);

        LOG_F(INFO, "Migrating legacy global overlay profile...");
        MigrateLegacyOverlayProfileFromConfig(config, apply_steamvr2_dashboard_offset, legacy_id_to_uid);

        //We need to traverse all existing overlay profiles, migrate them and save them to the new file structure
        const std::wstring wpath_dest   =  WStringConvertFromUTF8( std::string(m_ApplicationPath + "profiles/"               ).c_str() );
        const std::wstring wpath_src[2] = {WStringConvertFromUTF8( std::string(m_ApplicationPath + "profiles/overlays/"      ).c_str() ),
                                           WStringConvertFromUTF8( std::string(m_ApplicationPath + "profiles/multi-overlays/").c_str() )};
        const std::wstring wpath_sub[2] = {L" (single-overlay)", L" (multi-overlay)"};

        for (int i = 0; i < 2; ++i)
        {
            WIN32_FIND_DATA find_data = {};
            HANDLE handle_find = ::FindFirstFileW((wpath_src[i] + L"*.ini").c_str(), &find_data);

            if (handle_find != INVALID_HANDLE_VALUE)
            {
                do
                {
                    LOG_F(INFO, "Migrating legacy overlay profile \"%s\"...", StringConvertFromUTF16(find_data.cFileName).c_str());

                    Ini config_profile(wpath_src[i] + find_data.cFileName);
                    MigrateLegacyOverlayProfileFromConfig(config_profile, apply_steamvr2_dashboard_offset, legacy_id_to_uid);

                    std::wstring save_path = wpath_dest + find_data.cFileName;

                    //Avoid overwriting anything existing
                    if (FileExists(save_path.c_str()))
                    {
                        //Try just adding a subfolder identifier
                        std::wstring profile_name_with_sub = std::wstring(find_data.cFileName).substr(0, save_path.size() - 4);
                        profile_name_with_sub += wpath_sub[i];

                        std::wstring filename_log = profile_name_with_sub + L".ini";
                        std::wstring save_path_with_sub = wpath_dest + filename_log;

                        int duplicate_count = 1;
                        while (FileExists(save_path_with_sub.c_str()))
                        {
                            //Add numbers if we really still run into conflicts
                            duplicate_count++;

                            filename_log = profile_name_with_sub + L"(" + std::to_wstring(duplicate_count) + L")" + L".ini";
                            save_path_with_sub = wpath_dest + filename_log;
                        }

                        save_path = save_path_with_sub;
                        LOG_F(INFO, "Profile already existed in new location, saving as \"%s\" instead...", StringConvertFromUTF16(filename_log.c_str()).c_str());
                    }

                    config_profile.Save(save_path);
                }
                while (::FindNextFileW(handle_find, &find_data) != 0);

                ::FindClose(handle_find);
            }
        }
    }

    //Rename old config.ini to config_legacy.ini, which has loading priority in 2.8+ versions
    const std::wstring wpath_config = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config.ini").c_str() );
    if (FileExists(wpath_config.c_str()))
    {
        ::MoveFileW(wpath_config.c_str(), WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_legacy.ini").c_str() ).c_str());
        LOG_F(INFO, "Renamed legacy \"config.ini\" to \"config_legacy.ini\". Desktop+ 2.x will still be able to load this file");
    }

    if (!only_rename_config_file)
    {
        m_ConfigBool[configid_bool_state_misc_config_migrated] = true;
    }
}

void ConfigManager::MigrateLegacyOverlayProfileFromConfig(Ini& config, bool apply_steamvr2_dashboard_offset, LegacyActionIDtoActionUID& legacy_id_to_uid)
{
    //Rename unnumbered "Overlay" section if it exists (legacy single overlay profile)
    config.RenameSection("Overlay", "Overlay0");

    unsigned int overlay_id = 0;

    std::stringstream ss;
    ss << "Overlay" << overlay_id;

    std::string section = ss.str();

    //Migrate all sequential overlay sections that exist
    while (config.SectionExists(section.c_str()))
    {
        //Set 3D enabled value to true if any 3D is active
        config.WriteBool(section.c_str(), "3DEnabled", (config.ReadInt(section.c_str(), "3DMode", 0) != 0) );

        //Create tag string for group ID
        int overlay_group_id = config.ReadInt(section.c_str(), "GroupID", 0);
        if (overlay_group_id != 0)
        {
            std::string tag_str = "OVRL_GROUP_" + std::to_string(overlay_group_id);
            config.WriteString(section.c_str(), "Tags", tag_str.c_str());
        }

        //0-Dashboard overlay had no Floating UI, always only showing the Action Bar, but didn't make use of the property value which defaulted and stayed false, so force it to true
        if (overlay_id == 0)
        {
            config.WriteBool(section.c_str(), "ShowActionBar", true);
        }

        //Write Display Mode to new config string (0 is always Desktop+ tab)
        config.WriteInt(section.c_str(), "DisplayMode", (overlay_id == 0) ? ovrl_dispmode_dplustab : config.ReadInt(section.c_str(), "DetachedDisplayMode", ovrl_dispmode_always) );

        //Write Origin to new config string in new format (0 is always dashboard)
        const OverlayOrigin transform_origin = (overlay_id == 0) ? ovrl_origin_dashboard : GetOverlayOriginFromConfigString(config.ReadString(section.c_str(), "DetachedOrigin"));
        config.WriteString(section.c_str(), "Origin", GetConfigStringForOverlayOrigin(transform_origin) );

        //Write single transform string, chosen by active origin
        std::string transform_str;

        switch (transform_origin)
        {
            case ovrl_origin_room:
            {
                transform_str = config.ReadString(section.c_str(), "DetachedTransformPlaySpace");
                break;
            }
            case ovrl_origin_hmd_floor:
            {
                transform_str = config.ReadString(section.c_str(), "DetachedTransformHMDFloor");
                break;
            }
            case ovrl_origin_seated_universe:
            {
                transform_str = config.ReadString(section.c_str(), "DetachedTransformSeatedPosition");
                break;
            }
            case ovrl_origin_dashboard:
            {
                //Overlay 0 is always dashboard with no transform saved, set to identity scaled to counteract dashboard scale
                if (overlay_id == 0)
                {
                    Matrix4 matrix;
                    matrix.scale(2.08464f);  //We don't have OpenVR initialized at this step, but this hardcoded value will do the job for the gamepadui dashboard
                    transform_str = matrix.toString();
                }
                else
                {
                    transform_str = config.ReadString(section.c_str(), "DetachedTransformDashboard");

                    //Try to convert transforms that have implicit offsets used with apply_steamvr2_dashboard_offset disabled
                    if (!apply_steamvr2_dashboard_offset)
                    {
                        Matrix4 matrix(transform_str);

                        //Magic number, from taking the difference of both version's dashboard origins at the same HMD position
                        Matrix4 matrix_to_old_dash(1.14634132f,      3.725290300e-09f, -3.725290300e-09f, 0.00000000f,
                                                   0.00000000f,      0.878148496f,      0.736854136f,     0.00000000f,
                                                   7.45058060e-09f, -0.736854076f,      0.878148496f,     0.00000000f,
                                                  -5.96046448e-08f,  2.174717430f,      0.123533726f,     1.00000000f);

                        //Move transform roughly back to where it was in the old dashboard
                        matrix_to_old_dash.invert();
                        matrix = matrix_to_old_dash * matrix;

                        //Try to compensate for origin point differences
                        //Without being able to know the overlay content height this is only an approximation for 16:9 overlays
                        float width = clamp(config.ReadInt(section.c_str(), "Width", 165) / 100.0f, 0.00001f, 1000.0f);
                        matrix.translate_relative(0.00f, 1.22f - (width * 0.5625f * 0.5f), -0.12f);

                        transform_str = matrix.toString();
                    }
                }

                break;
            }
            case ovrl_origin_hmd:
            {
                transform_str = config.ReadString(section.c_str(), "DetachedTransformHMD");
                break;
            }
            case ovrl_origin_left_hand:
            {
                transform_str = config.ReadString(section.c_str(), "DetachedTransformLeftHand");
                break;
            }
            case ovrl_origin_right_hand:
            {
                transform_str = config.ReadString(section.c_str(), "DetachedTransformRightHand");
                break;
            }
            case ovrl_origin_aux:
            {
                transform_str = config.ReadString(section.c_str(), "DetachedTransformAux");
                break;
            }
        }

        //Only write transform when it was really present in the file, or else it defaults to identity instead of zero
        if (!transform_str.empty())
        {
            config.WriteString(section.c_str(), "Transform", transform_str.c_str());
        }

        config.WriteString(section.c_str(), "ActionBarOrderCustom", MigrateLegacyActionOrderString(config.ReadString(section.c_str(), "ActionBarOrderCustom"), legacy_id_to_uid).c_str() );

        overlay_id++;

        std::stringstream ss;
        ss << "Overlay" << overlay_id;

        section = ss.str();
    }

    //Remove obsolte keys
    //Other obsolete values may remain in the file but will be cleared the next time the profile is saved properly, so it's not really an issue
    config.RemoveKey(section.c_str(), "GroupID");
    config.RemoveKey(section.c_str(), "DetachedDisplayMode");
    config.RemoveKey(section.c_str(), "DetachedOrigin");
    config.RemoveKey(section.c_str(), "DetachedTransformPlaySpace");
    config.RemoveKey(section.c_str(), "DetachedTransformHMDFloor");
    config.RemoveKey(section.c_str(), "DetachedTransformSeatedPosition");
    config.RemoveKey(section.c_str(), "DetachedTransformDashboard");
    config.RemoveKey(section.c_str(), "DetachedTransformHMD");
    config.RemoveKey(section.c_str(), "DetachedTransformLeftHand");
    config.RemoveKey(section.c_str(), "DetachedTransformRightHand");
    config.RemoveKey(section.c_str(), "DetachedTransformAux");
}

std::string ConfigManager::MigrateLegacyActionOrderString(const std::string& order_str, LegacyActionIDtoActionUID& legacy_id_to_uid)
{
    //Migrate action order legacy IDs to UIDs and new string format
    std::stringstream ss(order_str);
    std::stringstream ss_out;
    int id;
    bool visible;
    char sep;

    for (;;)
    {
        ss >> id >> visible >> sep;

        if (ss.fail())
            break;

        //Invisible/unselected actions are omitted in the new format
        if (visible)
        {
            const ActionUID uid = legacy_id_to_uid[id];

            if (uid != k_ActionUID_Invalid)
            {
                ss_out << legacy_id_to_uid[id] << ';';
            }
        }
    }

    return ss_out.str();
}

#endif //ifdef DPLUS_UI

ConfigManager::LegacyActionIDtoActionUID ConfigManager::MigrateLegacyActionsFromConfig(const Ini& config)
{
    //Add new defaults first
    m_ActionManager.RestoreActionsFromDefault();

    //Skip rest if CustomActions section doesn't exist (nothing to migrate)
    if (!config.SectionExists("CustomActions"))
        return ConfigManager::LegacyActionIDtoActionUID();

    LOG_F(INFO, "Migrating legacy actions...");

    //Read legacy custom actions and create actions with equivalent commands
    ConfigManager::LegacyActionIDtoActionUID legacy_id_to_uid;

    //There's no surefire way to detect old default custom actions, but we at least match the old names and ignore them during migration to avoid double entries
    const char* default_names[] = 
    {
        "Middle Mouse Button",
        "Back Mouse Button",
        "\xE2\x80\x89\xE2\x80\x89Open ReadMe",  //Used two "thin space" characters for alignment
        "tstr_DefActionMiddleMouse",
        "tstr_DefActionBackMouse",
        "tstr_DefActionReadMe"
    };

    //Map legacy built-in IDs to now default custom actions (same ID is still required as the mapping will default to 0 if unset)
    legacy_id_to_uid.insert({1, 1}); //Show Keyboard
    legacy_id_to_uid.insert({2, 2}); //Crop to Active Window
    legacy_id_to_uid.insert({3, 4}); //Toggle Overlay Group 1 (these all just migrate to the "toggle all" default since they need manual setup anyhow)
    legacy_id_to_uid.insert({4, 4}); //Toggle Overlay Group 2
    legacy_id_to_uid.insert({5, 4}); //Toggle Overlay Group 3
    legacy_id_to_uid.insert({6, 3}); //Switch Task

    int custom_action_count = config.ReadInt("CustomActions", "Count", 0);

    for (int i = 0; i < custom_action_count; ++i)
    {
        std::string action_ini_name = "Action" + std::to_string(i);

        Action action;
        action.Name = config.ReadString("CustomActions", (action_ini_name + "Name").c_str(), action_ini_name.c_str());

        //Skip if name matches legacy default ones
        const auto it = std::find(std::begin(default_names), std::end(default_names), action.Name);
        if (it != std::end(default_names))
        {
            //Map detected legacy default name to new default IDs (a bit flaky but the name array won't change)
            size_t default_name_id = std::distance(std::begin(default_names), it);
            const int action_id_custom_base = 1000; //Old value of action_custom, minimum custon ID

            if ((default_name_id == 0) || (default_name_id == 3))
            {
                legacy_id_to_uid.insert({action_id_custom_base + i, 5});
            }
            else if ((default_name_id == 1) || (default_name_id == 4))
            {
                legacy_id_to_uid.insert({action_id_custom_base + i, 6});
            }
            else if ((default_name_id == 2) || (default_name_id == 5))
            {
                legacy_id_to_uid.insert({action_id_custom_base + i, 7});
            }

            continue;
        }

        action.UID = m_ActionManager.GenerateUID();
        legacy_id_to_uid.insert({i, action.UID});

        const std::string function_type_str = config.ReadString("CustomActions", (action_ini_name + "FunctionType").c_str());
        if (function_type_str == "PressKeys")
        {
            ActionCommand command;
            command.Type = ActionCommand::command_key;
            command.UIntArg = config.ReadBool("CustomActions", (action_ini_name + "ToggleKeys").c_str(), false);

            command.UIntID = config.ReadInt( "CustomActions", (action_ini_name + "KeyCode1").c_str(), 0);
            action.Commands.push_back(command);
            command.UIntID = config.ReadInt( "CustomActions", (action_ini_name + "KeyCode2").c_str(), 0);
            action.Commands.push_back(command);
            command.UIntID = config.ReadInt( "CustomActions", (action_ini_name + "KeyCode3").c_str(), 0);
            action.Commands.push_back(command);
        }
        else if (function_type_str == "TypeString")
        {
            ActionCommand command;
            command.Type = ActionCommand::command_string;
            command.StrMain = config.ReadString("CustomActions", (action_ini_name + "TypeString").c_str());

            action.Commands.push_back(command);
        }
        else if (function_type_str == "LaunchApplication")
        {
            ActionCommand command;
            command.Type = ActionCommand::command_launch_app;
            command.StrMain = config.ReadString("CustomActions", (action_ini_name + "ExecutablePath").c_str());
            command.StrArg  = config.ReadString("CustomActions", (action_ini_name + "ExecutableArg").c_str());

            action.Commands.push_back(command);
        }
        else if (function_type_str == "ToggleOverlayEnabledState")
        {
            //This function can't be migrated 1:1, but we leave a tag with original intent at least
            ActionCommand command;
            command.Type = ActionCommand::command_show_overlay;
            command.StrMain = std::string("Overlay_") + std::to_string( config.ReadInt("CustomActions", (action_ini_name + "OverlayID").c_str(), 0) );
            command.UIntArg = ActionCommand::command_arg_toggle;
            command.UIntID  = MAKELPARAM(true, false);

            action.Commands.push_back(command);
        }

        #ifdef DPLUS_UI
            action.IconFilename = config.ReadString("CustomActions", (action_ini_name + "IconFilename").c_str());

            //Remove folder from path as it's no longer part of the string
            if (action.IconFilename.find("images/icons/") == 0)
            {
                action.IconFilename = action.IconFilename.substr(sizeof("images/icons/") - 1);
            }

            action.NameTranslationID  = ActionManager::GetTranslationIDForName(action.Name);
            action.LabelTranslationID = ActionManager::GetTranslationIDForName(action.Label);
        #endif

        m_ActionManager.StoreAction(action);
    }

    //Adapt references to legacy actions if possible (this might fail for the default ones though)
    m_ConfigHandle[configid_handle_input_go_home_action_uid] = legacy_id_to_uid[config.ReadInt("Input", "GoHomeButtonActionID", 0)];
    m_ConfigHandle[configid_handle_input_go_back_action_uid] = legacy_id_to_uid[config.ReadInt("Input", "GoBackButtonActionID", 0)];

    for (size_t i = 0; i < 2; ++i)
    {
        if (i < m_ConfigGlobalShortcuts.size())
        {
            std::string config_name = "GlobalShortcut0" + std::to_string(i + 1) + "ActionID";
            m_ConfigGlobalShortcuts[i] = legacy_id_to_uid[config.ReadInt("Input", config_name.c_str(), 0)];
        }
    }

    for (size_t i = 0; i < 2; ++i)
    {
        if (i < m_ConfigHotkey.size())
        {
            std::string config_name = "GlobalHotkey0" + std::to_string(i + 1) + "ActionID";
            m_ConfigHotkey[i].ActionUID = legacy_id_to_uid[config.ReadInt("Input", config_name.c_str(), 0)];
        }
    }

    //Return the action ID mapping so action order can be migrated from it
    return legacy_id_to_uid;
}

OverlayOrigin ConfigManager::GetOverlayOriginFromConfigString(const std::string& str)
{
    const auto it = std::find_if(std::begin(g_OvrlOriginConfigFileStrings), std::end(g_OvrlOriginConfigFileStrings), [&](const auto& pair){ return (pair.second == str); });

    if (it != std::end(g_OvrlOriginConfigFileStrings))
    {
        return it->first;
    }

    LOG_F(WARNING, "Overlay has origin with unknown config string \"%s\"", str.c_str());

    return ovrl_origin_room;    //Fallback to Room instead of throwing invalid values around
}

const char* ConfigManager::GetConfigStringForOverlayOrigin(OverlayOrigin origin)
{
    const auto it = std::find_if(std::begin(g_OvrlOriginConfigFileStrings), std::end(g_OvrlOriginConfigFileStrings), [&](const auto& pair){ return (pair.first == origin); });

    if (it != std::end(g_OvrlOriginConfigFileStrings))
    {
        return it->second;
    }

    LOG_F(WARNING, "Overlay has origin with unknown ID %d", (int)origin);

    return "UnknownOrigin";
}

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
    const std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config.ini").c_str() );
    Ini config(wpath.c_str());

    //Only save overlay config if no app profile that has loaded an overlay profile is active
    if (!m_AppProfileManager.IsActiveProfileWithOverlayProfile())
    {
        SaveMultiOverlayProfile(config);
    }

    config.WriteString("Interface", "LanguageFile",             m_ConfigString[configid_str_interface_language_file].c_str());
    config.WriteInt(   "Interface", "OverlayCurrentID",         m_ConfigInt[configid_int_interface_overlay_current_id]);
    config.WriteInt(   "Interface", "DesktopButtonCyclingMode", m_ConfigInt[configid_int_interface_desktop_listing_style]);
    config.WriteBool(  "Interface", "ShowAdvancedSettings",     m_ConfigBool[configid_bool_interface_show_advanced_settings]);
    config.WriteBool(  "Interface", "DisplaySizeLarge",         m_ConfigBool[configid_bool_interface_large_style]);
    config.WriteBool(  "Interface", "DesktopButtonIncludeAll",  m_ConfigBool[configid_bool_interface_desktop_buttons_include_combined]);

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
    config.WriteBool("Interface", "WarningBrowserMissingHidden",           m_ConfigBool[configid_bool_interface_warning_browser_missing_hidden]);
    config.WriteBool("Interface", "WarningBrowserVersionMismatchHidden",   m_ConfigBool[configid_bool_interface_warning_browser_version_mismatch_hidden]);
    config.WriteBool("Interface", "WarningAppProfileActiveHidden",         m_ConfigBool[configid_bool_interface_warning_app_profile_active_hidden]);
    config.WriteBool("Interface", "WindowSettingsRestoreState",            m_ConfigBool[configid_bool_interface_window_settings_restore_state]);
    config.WriteBool("Interface", "WindowPropertiesRestoreState",          m_ConfigBool[configid_bool_interface_window_properties_restore_state]);
    config.WriteBool("Interface", "WindowKeyboardRestoreState",            m_ConfigBool[configid_bool_interface_window_keyboard_restore_state]);
    config.WriteBool("Interface", "QuickStartGuideHidden",                 m_ConfigBool[configid_bool_interface_quick_start_hidden]);

    //Only write WMR settings when they're not -1 since they get set to that when using a non-WMR system. We want to preserve them for HMD-switching users
    if (m_ConfigInt[configid_int_interface_wmr_ignore_vscreens] != -1)
        config.WriteInt("Interface", "WMRIgnoreVScreens", m_ConfigInt[configid_int_interface_wmr_ignore_vscreens]);

    SaveConfigPersistentWindowState(config);

    config.WriteString("Interface", "ActionOrder",           m_ActionManager.ActionOrderListToString(m_ActionManager.GetActionOrderListUI()).c_str() );
    config.WriteString("Interface", "ActionOrderBarDefault", m_ActionManager.ActionOrderListToString(m_ActionManager.GetActionOrderListBarDefault()).c_str() );
    config.WriteString("Interface", "ActionOrderOverlayBar", m_ActionManager.ActionOrderListToString(m_ActionManager.GetActionOrderListOverlayBar()).c_str() );

    config.WriteString("Input", "GoHomeButtonActionUID", std::to_string(m_ConfigHandle[configid_handle_input_go_home_action_uid]).c_str());
    config.WriteString("Input", "GoBackButtonActionUID", std::to_string(m_ConfigHandle[configid_handle_input_go_back_action_uid]).c_str());

    //Global Shorcuts
    int shortcut_id = 0;

    for (;;) //Remove any previous entries
    {
        ss = std::stringstream();
        ss << "GlobalShortcut" << std::setfill('0') << std::setw(2) << shortcut_id + 1 << "ActionUID";

        if (config.KeyExists("Input", ss.str().c_str()))
        {
            config.RemoveKey("Input", ss.str().c_str());
        }
        else
        {
            break;
        }

        ++shortcut_id;
    }

    shortcut_id = 0;
    for (const ActionUID uid: m_ConfigGlobalShortcuts)
    {
        ss = std::stringstream();
        ss << "GlobalShortcut" << std::setfill('0') << std::setw(2) << shortcut_id + 1 << "ActionUID";

        config.WriteString("Input", ss.str().c_str(), std::to_string(uid).c_str());

        ++shortcut_id;
    }

    //Hotkeys
    int hotkey_id = 0;

    for (;;) //Remove any previous entries
    {
        ss = std::stringstream();
        ss << "GlobalHotkey" << std::setfill('0') << std::setw(2) << hotkey_id + 1;

        if (config.KeyExists("Input", (ss.str() + "Modifiers").c_str()))
        {
            config.RemoveKey("Input", (ss.str() + "Modifiers").c_str());
            config.RemoveKey("Input", (ss.str() + "KeyCode"  ).c_str());
            config.RemoveKey("Input", (ss.str() + "ActionUID").c_str());
        }
        else
        {
            break;
        }

        ++hotkey_id;
    }

    hotkey_id = 0;
    for (const ConfigHotkey& hotkey : m_ConfigHotkey)
    {
        ss = std::stringstream();
        ss << "GlobalHotkey" << std::setfill('0') << std::setw(2) << hotkey_id + 1;

        config.WriteInt(   "Input", (ss.str() + "Modifiers").c_str(), hotkey.Modifiers);
        config.WriteInt(   "Input", (ss.str() + "KeyCode"  ).c_str(), hotkey.KeyCode);
        config.WriteString("Input", (ss.str() + "ActionUID").c_str(), std::to_string(hotkey.ActionUID).c_str());

        ++hotkey_id;
    }

    config.WriteInt( "Input", "DetachedInteractionMaxDistance", int(m_ConfigFloat[configid_float_input_detached_interaction_max_distance] * 100.0f));
    config.WriteBool("Input", "LaserPointerBlockInput",             m_ConfigBool[configid_bool_input_laser_pointer_block_input]);
    config.WriteBool("Input", "GlobalHMDPointer",                   m_ConfigBool[configid_bool_input_laser_pointer_hmd_device]);
    config.WriteInt( "Input", "LaserPointerHMDKeyCodeToggle",       m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_toggle]);
    config.WriteInt( "Input", "LaserPointerHMDKeyCodeLeft",         m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_left]);
    config.WriteInt( "Input", "LaserPointerHMDKeyCodeRight",        m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_right]);
    config.WriteInt( "Input", "LaserPointerHMDKeyCodeMiddle",       m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_middle]);
    config.WriteInt( "Input", "LaserPointerHMDKeyCodeDrag",         m_ConfigInt[configid_int_input_laser_pointer_hmd_device_keycode_drag]);

    config.WriteBool("Input", "DragAutoDocking",                    m_ConfigBool[configid_bool_input_drag_auto_docking]);
    config.WriteBool("Input", "DragForceUpright",                   m_ConfigBool[configid_bool_input_drag_force_upright]);
    config.WriteBool("Input", "DragFixedDistance",                  m_ConfigBool[configid_bool_input_drag_fixed_distance]);
    config.WriteInt( "Input", "DragFixedDistanceCM",            int(m_ConfigFloat[configid_float_input_drag_fixed_distance_m] * 100.0f));
    config.WriteInt( "Input", "DragFixedDistanceShape",             m_ConfigInt[configid_int_input_drag_fixed_distance_shape]);
    config.WriteBool("Input", "DragFixedDistanceAutoCurve",         m_ConfigBool[configid_bool_input_drag_fixed_distance_auto_curve]);
    config.WriteBool("Input", "DragFixedDistanceAutoTilt",          m_ConfigBool[configid_bool_input_drag_fixed_distance_auto_tilt]);
    config.WriteBool("Input", "DragSnapPosition",                   m_ConfigBool[configid_bool_input_drag_snap_position]);
    config.WriteInt( "Input", "DragSnapPositionSize",           int(m_ConfigFloat[configid_float_input_drag_snap_position_size] * 100.0f));

    config.WriteBool("Mouse", "RenderCursor",              m_ConfigBool[configid_bool_input_mouse_render_cursor]);
    config.WriteBool("Mouse", "RenderIntersectionBlob",    m_ConfigBool[configid_bool_input_mouse_render_intersection_blob]);
    config.WriteBool("Mouse", "ScrollSmooth",              m_ConfigBool[configid_bool_input_mouse_scroll_smooth]);
    config.WriteBool("Mouse", "AllowPointerOverride",      m_ConfigBool[configid_bool_input_mouse_allow_pointer_override]);
    config.WriteBool("Mouse", "SimulatePenInput",          m_ConfigBool[configid_bool_input_mouse_simulate_pen_input]);
    config.WriteInt( "Mouse", "DoubleClickAssistDuration", m_ConfigInt[configid_int_input_mouse_dbl_click_assist_duration_ms]);
    config.WriteInt( "Mouse", "InputSmoothingLevel",       m_ConfigInt[configid_int_input_mouse_input_smoothing_level]);

    config.WriteString("Keyboard", "LayoutFile",                m_ConfigString[configid_str_input_keyboard_layout_file].c_str());
    config.WriteBool("Keyboard", "LayoutClusterFunction",       m_ConfigBool[configid_bool_input_keyboard_cluster_function_enabled]);
    config.WriteBool("Keyboard", "LayoutClusterNavigation",     m_ConfigBool[configid_bool_input_keyboard_cluster_navigation_enabled]);
    config.WriteBool("Keyboard", "LayoutClusterNumpad",         m_ConfigBool[configid_bool_input_keyboard_cluster_numpad_enabled]);
    config.WriteBool("Keyboard", "LayoutClusterExtra",          m_ConfigBool[configid_bool_input_keyboard_cluster_extra_enabled]);
    config.WriteBool("Keyboard", "StickyModifiers",             m_ConfigBool[configid_bool_input_keyboard_sticky_modifiers]);
    config.WriteBool("Keyboard", "KeyRepeat",                   m_ConfigBool[configid_bool_input_keyboard_key_repeat]);
    config.WriteBool("Keyboard", "AutoShowDesktop",             m_ConfigBool[configid_bool_input_keyboard_auto_show_desktop]);
    config.WriteBool("Keyboard", "AutoShowBrowser",             m_ConfigBool[configid_bool_input_keyboard_auto_show_browser]);

    config.WriteBool("Windows", "AutoFocusSceneAppDashboard",   m_ConfigBool[configid_bool_windows_auto_focus_scene_app_dashboard]);
    config.WriteBool("Windows", "WinRTAutoFocus",               m_ConfigBool[configid_bool_windows_winrt_auto_focus]);
    config.WriteBool("Windows", "WinRTKeepOnScreen",            m_ConfigBool[configid_bool_windows_winrt_keep_on_screen]);
    config.WriteInt( "Windows", "WinRTDraggingMode",            m_ConfigInt[configid_int_windows_winrt_dragging_mode]);
    config.WriteBool("Windows", "WinRTAutoSizeOverlay",         m_ConfigBool[configid_bool_windows_winrt_auto_size_overlay]);
    config.WriteBool("Windows", "WinRTAutoFocusSceneApp",       m_ConfigBool[configid_bool_windows_winrt_auto_focus_scene_app]);
    config.WriteInt( "Windows", "WinRTOnCaptureLost",           m_ConfigInt[configid_int_windows_winrt_capture_lost_behavior]);

    config.WriteInt( "Browser", "BrowserMaxFPS",                m_ConfigInt[configid_int_browser_max_fps]);
    config.WriteBool("Browser", "BrowserContentBlocker",        m_ConfigBool[configid_bool_browser_content_blocker]);

    config.WriteInt( "Performance", "UpdateLimitMode",                      m_ConfigInt[configid_int_performance_update_limit_mode]);
    config.WriteInt( "Performance", "UpdateLimitMS",                    int(m_ConfigFloat[configid_float_performance_update_limit_ms] * 100.0f));
    config.WriteInt( "Performance", "UpdateLimitFPS",                       m_ConfigInt[configid_int_performance_update_limit_fps]);
    config.WriteBool("Performance", "RapidLaserPointerUpdates",             m_ConfigBool[configid_bool_performance_rapid_laser_pointer_updates]);
    config.WriteBool("Performance", "SingleDesktopMirroring",               m_ConfigBool[configid_bool_performance_single_desktop_mirroring]);
    config.WriteBool("Performance", "HDRMirroring",                         m_ConfigBool[configid_bool_performance_hdr_mirroring]);
    config.WriteBool("Performance", "ShowFPS",                              m_ConfigBool[configid_bool_performance_show_fps]);
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

    config.WriteInt( "Misc", "ConfigVersion",      k_nDesktopPlusConfigVersion);
    config.WriteBool("Misc", "NoSteam",            m_ConfigBool[configid_bool_misc_no_steam]);
    config.WriteBool("Misc", "UIAccessWasEnabled", (m_ConfigBool[configid_bool_misc_uiaccess_was_enabled] || m_ConfigBool[configid_bool_state_misc_uiaccess_enabled]));

    //Remove old CustomSection section (actions are now saved separately)
    config.RemoveSection("CustomActions");

    //Save config & if it succeeded, remove potential leftover unused config_newui.ini
    if (config.Save())
    {
        std::wstring wpath_newui = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_newui.ini").c_str() );
        if (FileExists(wpath_newui.c_str()))
        {
            ::DeleteFileW(wpath_newui.c_str());
        }
    }

    m_ActionManager.SaveActionsToFile();
    m_AppProfileManager.SaveProfilesToFile();
}

#endif //ifdef DPLUS_UI

void ConfigManager::RestoreConfigFromDefault()
{
    //Basically delete the config files and then load it again which will fall back to config_default.ini
    const std::wstring wpath_newui = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_newui.ini").c_str() );
    const std::wstring wpath       = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config.ini").c_str() );
    ::DeleteFileW(wpath_newui.c_str());
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
        LoadOverlayProfile(config, 0); //All read calls will fail end fill in default values as a result
    }
}

bool ConfigManager::LoadMultiOverlayProfileFromFile(const std::string& filename, bool clear_existing_overlays, std::vector<char>* ovrl_inclusion_list)
{
    LOG_F(INFO, "Loading overlay profile \"%s\"...", filename.c_str());

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
    LOG_F(INFO, "Saving overlay profile \"%s\"...", filename.c_str());

    std::string path = m_ApplicationPath + "profiles/" + filename;
    Ini config(WStringConvertFromUTF8(path.c_str()), true);

    SaveMultiOverlayProfile(config, ovrl_inclusion_list);
    return config.Save();
}

bool ConfigManager::DeleteOverlayProfile(const std::string& filename)
{
    LOG_F(INFO, "Deleting overlay profile \"%s\"...", filename.c_str());

    std::string path = m_ApplicationPath + "profiles/" + filename;
    bool ret = (::DeleteFileW(WStringConvertFromUTF8(path.c_str()).c_str()) != 0);

    LOG_IF_F(WARNING, !ret, "Failed to delete overlay profile!");

    return ret;
}

void ConfigManager::DeleteAllOverlayProfiles()
{
    LOG_F(INFO, "Deleting all overlay profiles...");

    const std::wstring wpath = WStringConvertFromUTF8(std::string(m_ApplicationPath + "profiles/*.ini").c_str());
    WIN32_FIND_DATA find_data;
    HANDLE handle_find = ::FindFirstFileW(wpath.c_str(), &find_data);

    if (handle_find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                std::wstring wpath = WStringConvertFromUTF8(std::string(m_ApplicationPath + "profiles/").c_str()) + find_data.cFileName;
                ::DeleteFileW(wpath.c_str());
            }
        }
        while (::FindNextFileW(handle_find, &find_data) != 0);

        ::FindClose(handle_find);
    }
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
            OverlayOrigin origin = GetOverlayOriginFromConfigString(config.ReadString(ss.str().c_str(), "Origin"));

            list.push_back( std::make_pair(name.empty() ? ss.str() : name, origin) );   //Name should never be blank with compatible profiles, but offer alternative just in case

            ss = std::stringstream();
            ss << "Overlay" << overlay_id;
        }
    }

    return list;
}

void ConfigManager::RestoreActionOrdersFromDefault()
{
    LOG_F(INFO, "Restoring action orders from default config...");

    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "config_default.ini").c_str() );

    Ini config(wpath.c_str());
    m_ActionManager.SetActionOrderListUI(         m_ActionManager.ActionOrderListFromString( config.ReadString("Interface", "ActionOrder") ));
    m_ActionManager.SetActionOrderListBarDefault( m_ActionManager.ActionOrderListFromString( config.ReadString("Interface", "ActionOrderBarDefault") ));
    m_ActionManager.SetActionOrderListOverlayBar( m_ActionManager.ActionOrderListFromString( config.ReadString("Interface", "ActionOrderOverlayBar") ));
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

ActionManager::ActionList& ConfigManager::GetGlobalShortcuts()
{
    return m_ConfigGlobalShortcuts;
}

const ActionManager::ActionList& ConfigManager::GetGlobalShortcuts() const
{
    return m_ConfigGlobalShortcuts;
}

ConfigHotkeyList& ConfigManager::GetHotkeys()
{
    return m_ConfigHotkey;
}

const ConfigHotkeyList& ConfigManager::GetHotkeys() const
{
    return m_ConfigHotkey;
}

void ConfigManager::InitConfigForWMR()
{
    int& wmr_ignore_vscreens = m_ConfigInt[configid_int_interface_wmr_ignore_vscreens];

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

void ConfigManager::ResetConfigStateValues()
{
    std::fill(std::begin(m_ConfigBool) + configid_bool_state_overlay_dragmode,                std::begin(m_ConfigBool) + configid_bool_state_misc_process_started_by_steam,     false);
    std::fill(std::begin(m_ConfigInt)  + configid_int_state_overlay_current_id_override,      std::begin(m_ConfigInt)  + configid_int_state_performance_duplication_fps,        -1);
    //configid_int_state_interface_desktop_count is not reset
    std::fill(std::begin(m_ConfigInt)  + configid_int_state_interface_floating_ui_hovered_id, std::begin(m_ConfigInt)  + configid_int_state_browser_content_blocker_list_count, -1);

    //Also reset overlay states
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);
        std::fill(std::begin(data.ConfigBool) + configid_bool_overlay_state_browser_allow_transparency_is_pending, 
                  std::begin(data.ConfigBool) + configid_bool_overlay_state_browser_nav_is_loading, false);
    }
}

ActionManager& ConfigManager::GetActionManager()
{
    return m_ActionManager;
}

AppProfileManager& ConfigManager::GetAppProfileManager()
{
    return m_AppProfileManager;
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

    //No dashboard device, try Desktop+ laser pointer device
    if (!vr::IVROverlayEx::IsSystemLaserPointerActive())
        return (vr::TrackedDeviceIndex_t)m_ConfigInt[configid_int_state_dplus_laser_pointer_device];

    return vr::VROverlay()->GetPrimaryDashboardDevice();
}

bool ConfigManager::IsLaserPointerTargetOverlay(vr::VROverlayHandle_t ulOverlayHandle, bool no_intersection_check) const
{
    if (vr::VROverlay() == nullptr)
        return false;

    bool ret = false;

    if (vr::IVROverlayEx::IsSystemLaserPointerActive())
    {
        if (vr::VROverlay()->IsHoverTargetOverlay(ulOverlayHandle))
        {
            ret = true;

            if (!no_intersection_check)
            {
                //Double check IsHoverTargetOverlay() with an intersection check as it's not guaranteed to always return false after leaving an overlay
                //(it mostly does, but it's documented as returning the last overlay)
                vr::VROverlayIntersectionResults_t results;
                ret = vr::IVROverlayEx::ComputeOverlayIntersectionForDevice(ulOverlayHandle, vr::VROverlay()->GetPrimaryDashboardDevice(), vr::TrackingUniverseStanding, &results);
            }
        }
    }

    if (!ret)
        return (ulOverlayHandle == m_ConfigHandle[configid_handle_state_dplus_laser_pointer_target_overlay]);

    return ret;
}
