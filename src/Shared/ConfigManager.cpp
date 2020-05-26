#include "ConfigManager.h"

#include <algorithm>
#include <sstream>

#include "Util.h"

static ConfigManager g_ConfigManager;
static const std::string g_EmptyString;       //This way we can still return a const reference. Worth it? iunno

ConfigManager::ConfigManager()
{
    std::fill(std::begin(m_ConfigBool),  std::end(m_ConfigBool),  false);
    std::fill(std::begin(m_ConfigInt),   std::end(m_ConfigInt),   -1);
    std::fill(std::begin(m_ConfigFloat), std::end(m_ConfigFloat), 0.0f);
    //We don't need to initialize m_ConfigString

    //Default the transform matrices to zero as an indicator to reset them when possible later
    float matrix_zero[16] = { 0.0f };
    std::fill(std::begin(m_ConfigOverlayDetachedTransform), std::end(m_ConfigOverlayDetachedTransform), matrix_zero);
    

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
        m_ExecutableName = path_str.substr(pos + 1, std::string::npos);
    }

    delete[] buffer;
}

ConfigManager& ConfigManager::Get()
{
    return g_ConfigManager;
}

void ConfigManager::LoadOverlayProfile(Ini& config)
{
    m_ConfigInt[configid_int_overlay_desktop_id]            = config.ReadInt("Overlay", "DesktopID", 0);
    m_ConfigBool[configid_bool_overlay_detached]            = config.ReadBool("Overlay", "Detached", false);
    m_ConfigFloat[configid_float_overlay_width]             = config.ReadInt("Overlay", "Width", 350) / 100.0f;
    m_ConfigFloat[configid_float_overlay_curvature]         = config.ReadInt("Overlay", "Curvature", -100) / 100.0f;
    m_ConfigFloat[configid_float_overlay_opacity]           = config.ReadInt("Overlay", "Opacity", 100) / 100.0f;
    m_ConfigFloat[configid_float_overlay_offset_right]      = config.ReadInt("Overlay", "OffsetRight", 0) / 100.0f;
    m_ConfigFloat[configid_float_overlay_offset_up]         = config.ReadInt("Overlay", "OffsetUp", 0) / 100.0f;
    m_ConfigFloat[configid_float_overlay_offset_forward]    = config.ReadInt("Overlay", "OffsetForward", 0) / 100.0f;
    m_ConfigInt[configid_int_overlay_detached_display_mode] = config.ReadInt("Overlay", "DetachedDisplayMode", ovrl_dispmode_always);
    m_ConfigInt[configid_int_overlay_detached_origin]       = config.ReadInt("Overlay", "DetachedOrigin", ovrl_origin_room);
    m_ConfigInt[configid_int_overlay_crop_x]                = config.ReadInt("Overlay", "CroppingX", 0);
    m_ConfigInt[configid_int_overlay_crop_y]                = config.ReadInt("Overlay", "CroppingY", 0);
    m_ConfigInt[configid_int_overlay_crop_width]            = config.ReadInt("Overlay", "CroppingWidth", -1);
    m_ConfigInt[configid_int_overlay_crop_height]           = config.ReadInt("Overlay", "CroppingHeight", -1);
    m_ConfigInt[configid_int_overlay_3D_mode]               = config.ReadInt("Overlay", "3DMode", ovrl_3Dmode_none);
    m_ConfigBool[configid_bool_overlay_3D_swapped]          = config.ReadBool("Overlay", "3DSwapped", false);
    m_ConfigBool[configid_bool_overlay_gazefade_enabled]    = config.ReadBool("Overlay", "GazeFade", false);
    m_ConfigFloat[configid_float_overlay_gazefade_distance] = config.ReadInt("Overlay", "GazeFadeDistance", 40) / 100.0f;
    m_ConfigFloat[configid_float_overlay_gazefade_rate]     = config.ReadInt("Overlay", "GazeFadeRate", 100) / 100.0f;

    //Default the transform matrices to zero
    float matrix_zero[16] = { 0.0f };
    std::fill(std::begin(m_ConfigOverlayDetachedTransform), std::end(m_ConfigOverlayDetachedTransform), matrix_zero);

    std::string transform_str; //Only set these when it's really present in the file, or else it defaults to identity instead of zero
    transform_str = config.ReadString("Overlay", "DetachedTransformPlaySpace");
    if (!transform_str.empty())
        m_ConfigOverlayDetachedTransform[ovrl_origin_room] = transform_str;

    transform_str = config.ReadString("Overlay", "DetachedTransformHMDFloor");
    if (!transform_str.empty())
        m_ConfigOverlayDetachedTransform[ovrl_origin_hmd_floor] = transform_str;

    transform_str = config.ReadString("Overlay", "DetachedTransformDashboard");
    if (!transform_str.empty())
        m_ConfigOverlayDetachedTransform[ovrl_origin_dashboard] = transform_str;

    transform_str = config.ReadString("Overlay", "DetachedTransformHMD");
    if (!transform_str.empty())
        m_ConfigOverlayDetachedTransform[ovrl_origin_hmd] = transform_str;

    transform_str = config.ReadString("Overlay", "DetachedTransformRightHand");
    if (!transform_str.empty())
        m_ConfigOverlayDetachedTransform[ovrl_origin_right_hand] = transform_str;

    transform_str = config.ReadString("Overlay", "DetachedTransformLeftHand");
    if (!transform_str.empty())
        m_ConfigOverlayDetachedTransform[ovrl_origin_left_hand] = transform_str;

    transform_str = config.ReadString("Overlay", "DetachedTransformAux");
    if (!transform_str.empty())
        m_ConfigOverlayDetachedTransform[ovrl_origin_aux] = transform_str;
}

void ConfigManager::SaveOverlayProfile(Ini& config)
{
    config.WriteInt( "Overlay", "DesktopID",            m_ConfigInt[configid_int_overlay_desktop_id]);
    config.WriteBool("Overlay", "Detached",             m_ConfigBool[configid_bool_overlay_detached]);
    config.WriteInt( "Overlay", "Width",            int(m_ConfigFloat[configid_float_overlay_width]           * 100.0f));
    config.WriteInt( "Overlay", "Curvature",        int(m_ConfigFloat[configid_float_overlay_curvature]       * 100.0f));
    config.WriteInt( "Overlay", "Opacity",          int(m_ConfigFloat[configid_float_overlay_opacity]         * 100.0f));
    config.WriteInt( "Overlay", "OffsetRight",      int(m_ConfigFloat[configid_float_overlay_offset_right]    * 100.0f));
    config.WriteInt( "Overlay", "OffsetUp",         int(m_ConfigFloat[configid_float_overlay_offset_up]       * 100.0f));
    config.WriteInt( "Overlay", "OffsetForward",    int(m_ConfigFloat[configid_float_overlay_offset_forward]  * 100.0f));
    config.WriteInt( "Overlay", "DetachedDisplayMode",  m_ConfigInt[configid_int_overlay_detached_display_mode]);
    config.WriteInt( "Overlay", "DetachedOrigin",       m_ConfigInt[configid_int_overlay_detached_origin]);
    config.WriteInt( "Overlay", "CroppingX",            m_ConfigInt[configid_int_overlay_crop_x]);
    config.WriteInt( "Overlay", "CroppingY",            m_ConfigInt[configid_int_overlay_crop_y]);
    config.WriteInt( "Overlay", "CroppingWidth",        m_ConfigInt[configid_int_overlay_crop_width]);
    config.WriteInt( "Overlay", "CroppingHeight",       m_ConfigInt[configid_int_overlay_crop_height]);
    config.WriteInt( "Overlay", "3DMode",               m_ConfigInt[configid_int_overlay_3D_mode]);
    config.WriteBool("Overlay", "3DSwapped",            m_ConfigBool[configid_bool_overlay_3D_swapped]);
    config.WriteBool("Overlay", "GazeFade",             m_ConfigBool[configid_bool_overlay_gazefade_enabled]);
    config.WriteInt( "Overlay", "GazeFadeDistance", int(m_ConfigFloat[configid_float_overlay_gazefade_distance]  * 100.0f));
    config.WriteInt( "Overlay", "GazeFadeRate",     int(m_ConfigFloat[configid_float_overlay_gazefade_rate]  * 100.0f));

    config.WriteString("Overlay", "DetachedTransformPlaySpace", m_ConfigOverlayDetachedTransform[ovrl_origin_room].toString().c_str());
    config.WriteString("Overlay", "DetachedTransformHMDFloor",  m_ConfigOverlayDetachedTransform[ovrl_origin_hmd_floor].toString().c_str());
    config.WriteString("Overlay", "DetachedTransformDashboard", m_ConfigOverlayDetachedTransform[ovrl_origin_dashboard].toString().c_str());
    config.WriteString("Overlay", "DetachedTransformHMD",       m_ConfigOverlayDetachedTransform[ovrl_origin_hmd].toString().c_str());
    config.WriteString("Overlay", "DetachedTransformRightHand", m_ConfigOverlayDetachedTransform[ovrl_origin_right_hand].toString().c_str());
    config.WriteString("Overlay", "DetachedTransformLeftHand",  m_ConfigOverlayDetachedTransform[ovrl_origin_left_hand].toString().c_str());
    config.WriteString("Overlay", "DetachedTransformAux",       m_ConfigOverlayDetachedTransform[ovrl_origin_aux].toString().c_str());
}

bool ConfigManager::LoadConfigFromFile()
{
    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "/config.ini").c_str() );
    bool existed = FileExists(wpath.c_str());

    Ini config(wpath.c_str());

    LoadOverlayProfile(config);

    m_ConfigBool[configid_bool_interface_no_ui]                                = config.ReadBool("Interface", "NoUIAutoLaunch", false);
    m_ConfigInt[configid_int_interface_mainbar_desktop_listing]                = config.ReadInt("Interface", "DesktopButtonCyclingMode", mainbar_desktop_listing_individual);
    m_ConfigBool[configid_bool_interface_mainbar_desktop_include_all]          = config.ReadBool("Interface", "DesktopButtonIncludeAll", false);
    m_ConfigFloat[configid_float_interface_last_vr_ui_scale]                   = config.ReadInt("Interface", "LastVRUIScale", 100) / 100.0f;
    m_ConfigBool[configid_bool_interface_warning_compositor_res_hidden]        = config.ReadBool("Interface", "WarningCompositorResolutionHidden", false);
    m_ConfigBool[configid_bool_interface_warning_compositor_quality_hidden]    = config.ReadBool("Interface", "WarningCompositorQualityHidden", false);
    m_ConfigBool[configid_bool_interface_warning_process_elevation_hidden]     = config.ReadBool("Interface", "WarningProcessElevationHidden", false);
    m_ConfigInt[configid_int_interface_wmr_ignore_vscreens_selection]          = config.ReadInt("Interface", "WMRIgnoreVScreensSelection", -1);
    m_ConfigInt[configid_int_interface_wmr_ignore_vscreens_combined_desktop]   = config.ReadInt("Interface", "WMRIgnoreVScreensCombinedDesktop", -1);

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

	m_ConfigBool[configid_bool_input_enabled]                               = config.ReadBool("Input", "EnableInput", true);
	m_ConfigInt[configid_int_input_go_home_action_id]                       = config.ReadInt("Input",  "GoHomeButtonActionID", 0);
	m_ConfigInt[configid_int_input_go_back_action_id]                       = config.ReadInt("Input",  "GoBackButtonActionID", 0);
	m_ConfigInt[configid_int_input_shortcut01_action_id]                    = config.ReadInt("Input",  "GlobalShortcut01ActionID", 0);
	m_ConfigInt[configid_int_input_shortcut02_action_id]                    = config.ReadInt("Input",  "GlobalShortcut02ActionID", 0);
	m_ConfigInt[configid_int_input_shortcut03_action_id]                    = config.ReadInt("Input",  "GlobalShortcut03ActionID", 0);
    m_ConfigFloat[configid_float_input_detached_interaction_max_distance]   = config.ReadInt("Input",  "DetachedInteractionMaxDistance", 0) / 100.0f;

    m_ConfigBool[configid_bool_input_mouse_render_cursor]              = config.ReadBool("Mouse", "RenderCursor", true);
    m_ConfigBool[configid_bool_input_mouse_render_intersection_blob]   = config.ReadBool("Mouse", "RenderIntersectionBlob", false);
	m_ConfigInt[configid_int_input_mouse_dbl_click_assist_duration_ms] = config.ReadInt("Mouse", "DoubleClickAssistDuration", -1);
	m_ConfigBool[configid_bool_input_mouse_hmd_pointer_override]       = config.ReadBool("Mouse", "HMDPointerOverride", true);

    m_ConfigBool[configid_bool_input_keyboard_helper_enabled]          = config.ReadBool("Keyboard", "EnableKeyboardHelper", true);
    m_ConfigFloat[configid_float_input_keyboard_detached_size]         = config.ReadInt("Keyboard", "KeyboardDetachedSize", 100) / 100.0f;

    m_ConfigInt[configid_int_performance_update_limit_mode]             = config.ReadInt("Performance", "UpdateLimitMode", update_limit_mode_off);
    m_ConfigFloat[configid_float_performance_update_limit_ms]           = config.ReadInt("Performance", "UpdateLimitMS", 0) / 100.0f;
    m_ConfigInt[configid_int_performance_update_limit_fps]              = config.ReadInt("Performance", "UpdateLimitFPS", update_limit_fps_30);
    m_ConfigBool[configid_bool_performance_rapid_laser_pointer_updates] = config.ReadBool("Performance", "RapidLaserPointerUpdates", false);

    //Load custom actions (this is where using ini feels dumb, but it still kinda works)
    auto& custom_actions = m_ActionManager.GetCustomActions();
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
                action.KeyCodes[0] = config.ReadInt("CustomActions", (action_ini_name + "KeyCode1").c_str(), 0);
                action.KeyCodes[1] = config.ReadInt("CustomActions", (action_ini_name + "KeyCode2").c_str(), 0);
                action.KeyCodes[2] = config.ReadInt("CustomActions", (action_ini_name + "KeyCode3").c_str(), 0);
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

            for (const auto order_data : action_order)
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
    
    return existed; //We use default values if it doesn't, but still return if the file existed
}

void ConfigManager::SaveConfigToFile()
{
    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "/config.ini").c_str() );
    Ini config(wpath.c_str());

    SaveOverlayProfile(config);

    config.WriteInt( "Interface", "DesktopButtonCyclingMode",          m_ConfigInt[configid_int_interface_mainbar_desktop_listing]);
    config.WriteBool("Interface", "DesktopButtonIncludeAll",           m_ConfigBool[configid_bool_interface_mainbar_desktop_include_all]);
    config.WriteInt( "Interface", "LastVRUIScale",                 int(m_ConfigFloat[configid_float_interface_last_vr_ui_scale] * 100.0f));
    config.WriteBool("Interface", "WarningCompositorResolutionHidden", m_ConfigBool[configid_bool_interface_warning_compositor_res_hidden]);
    config.WriteBool("Interface", "WarningCompositorQualityHidden",    m_ConfigBool[configid_bool_interface_warning_compositor_quality_hidden]);
    config.WriteBool("Interface", "WarningProcessElevationHidden",     m_ConfigBool[configid_bool_interface_warning_process_elevation_hidden]);

    //Only write WMR settings when they're not -1 since they get set to that when using a non-WMR system. We want to preserve them for HMD-switching users
    if (m_ConfigInt[configid_int_interface_wmr_ignore_vscreens_selection] != -1)
        config.WriteInt("Interface", "WMRIgnoreVScreensSelection", m_ConfigInt[configid_int_interface_wmr_ignore_vscreens_selection]);
    if (m_ConfigInt[configid_int_interface_wmr_ignore_vscreens_combined_desktop] != -1)
        config.WriteInt("Interface", "WMRIgnoreVScreensCombinedDesktop", m_ConfigInt[configid_int_interface_wmr_ignore_vscreens_combined_desktop]);

    //Save action order
    std::stringstream ss;

    for (auto& data : m_ActionManager.GetActionMainBarOrder())
    {
        ss << data.action_id << ' ' << data.visible << ";";
    }

    config.WriteString("Interface", "ActionOrder", ss.str().c_str());

    m_ConfigBool[configid_bool_input_enabled] = config.ReadBool("Input", "EnableInput", true);
    config.WriteBool("Input",  "EnableInput",                        m_ConfigBool[configid_bool_input_enabled]);
    config.WriteInt( "Input",  "GoHomeButtonActionID",               m_ConfigInt[configid_int_input_go_home_action_id]);
    config.WriteInt( "Input",  "GoBackButtonActionID",               m_ConfigInt[configid_int_input_go_back_action_id]);
    config.WriteInt( "Input",  "GlobalShortcut01ActionID",           m_ConfigInt[configid_int_input_shortcut01_action_id]);
    config.WriteInt( "Input",  "GlobalShortcut02ActionID",           m_ConfigInt[configid_int_input_shortcut02_action_id]);
    config.WriteInt( "Input",  "GlobalShortcut03ActionID",           m_ConfigInt[configid_int_input_shortcut03_action_id]);
    config.WriteInt( "Input",  "DetachedInteractionMaxDistance", int(m_ConfigFloat[configid_float_input_detached_interaction_max_distance] * 100.0f));

    config.WriteBool("Mouse", "RenderCursor",              m_ConfigBool[configid_bool_input_mouse_render_cursor]);
    config.WriteBool("Mouse", "RenderIntersectionBlob",    m_ConfigBool[configid_bool_input_mouse_render_intersection_blob]);
    config.WriteBool("Mouse", "HMDPointerOverride",        m_ConfigBool[configid_bool_input_mouse_hmd_pointer_override]);
    config.WriteInt( "Mouse", "DoubleClickAssistDuration", m_ConfigInt[configid_int_input_mouse_dbl_click_assist_duration_ms]);

    config.WriteBool("Keyboard", "EnableKeyboardHelper",        m_ConfigBool[configid_bool_input_keyboard_helper_enabled]);
    config.WriteInt( "Keyboard", "KeyboardDetachedSize",    int(m_ConfigFloat[configid_float_input_keyboard_detached_size] * 100.0f));
    
    config.WriteInt( "Performance", "UpdateLimitMode",          m_ConfigInt[configid_int_performance_update_limit_mode]);
    config.WriteInt( "Performance", "UpdateLimitMS",        int(m_ConfigFloat[configid_float_performance_update_limit_ms] * 100.0f));
    config.WriteInt( "Performance", "UpdateLimitFPS",           m_ConfigInt[configid_int_performance_update_limit_fps]);
    config.WriteBool("Performance", "RapidLaserPointerUpdates", m_ConfigBool[configid_bool_performance_rapid_laser_pointer_updates]);

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
                config.WriteInt("CustomActions", (action_ini_name + "KeyCode1").c_str(), action.KeyCodes[0]);
                config.WriteInt("CustomActions", (action_ini_name + "KeyCode2").c_str(), action.KeyCodes[1]);
                config.WriteInt("CustomActions", (action_ini_name + "KeyCode3").c_str(), action.KeyCodes[2]);
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
                config.WriteString("CustomActions", (action_ini_name + "ExecutableArg").c_str(), action.StrArg.c_str());
                break;
            }
        }

        #ifdef DPLUS_UI
            config.WriteString("CustomActions", (action_ini_name + "IconFilename").c_str(), action.IconFilename.c_str());
        #endif
    }

    config.Save();
}

void ConfigManager::LoadOverlayProfileDefault()
{
    Ini config(L"");
    LoadOverlayProfile(config); //All read calls will fail end fill in default values as a result
}

bool ConfigManager::LoadOverlayProfileFromFile(const std::string filename)
{
    std::wstring wpath = WStringConvertFromUTF8( std::string(m_ApplicationPath + "/profiles/overlays/" + filename).c_str() );

    if (FileExists(wpath.c_str()))
    {
        Ini config(wpath);
        LoadOverlayProfile(config);
        return true;
    }

    return false;
}

void ConfigManager::SaveOverlayProfileToFile(const std::string filename)
{
    std::string path = m_ApplicationPath + "/profiles/overlays/" + filename;
    Ini config(WStringConvertFromUTF8(path.c_str()));

    SaveOverlayProfile(config);
    config.Save();
}

bool ConfigManager::DeleteOverlayProfile(const std::string filename)
{
    std::string path = m_ApplicationPath + "/profiles/overlays/" + filename;
    return (::DeleteFileW(WStringConvertFromUTF8(path.c_str()).c_str()) != 0);
}

std::vector<std::string> ConfigManager::GetOverlayProfileList()
{
    std::vector<std::string> list;
    list.emplace_back("Default");

    const std::wstring wpath = WStringConvertFromUTF8(std::string(m_ApplicationPath + "profiles/overlays/*.ini").c_str());
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

    list.emplace_back("[New Profile]");

    return list;
}

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

void ConfigManager::SetConfigBool(ConfigID_Bool id, bool value)
{
    if (id < configid_bool_MAX)
        m_ConfigBool[id] = value;
}

void ConfigManager::SetConfigInt(ConfigID_Int id, int value)
{
    if (id < configid_int_MAX)
        m_ConfigInt[id] = value;
}

void ConfigManager::SetConfigFloat(ConfigID_Float id, float value)
{
    if (id < configid_float_MAX)
        m_ConfigFloat[id] = value;
}

void ConfigManager::SetConfigString(ConfigID_String id, const std::string& value)
{
    if (id < configid_str_MAX)
        m_ConfigString[id] = value;
}

//The GetConfig*() functions assume the caller knows what they're doing and don't shove *_MAX or an unchecked cast in there. For performance
bool ConfigManager::GetConfigBool(ConfigID_Bool id) const
{
    return m_ConfigBool[id];
}

int ConfigManager::GetConfigInt(ConfigID_Int id) const
{
    return m_ConfigInt[id];
}

float ConfigManager::GetConfigFloat(ConfigID_Float id) const
{
    return m_ConfigFloat[id];
}

const std::string& ConfigManager::GetConfigString(ConfigID_String id) const
{
    return m_ConfigString[id];
}

bool& ConfigManager::GetConfigBoolRef(ConfigID_Bool id)
{
    return m_ConfigBool[id];
}

int& ConfigManager::GetConfigIntRef(ConfigID_Int id)
{
    return m_ConfigInt[id];
}

float& ConfigManager::GetConfigFloatRef(ConfigID_Float id)
{
    return m_ConfigFloat[id];
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
    if (m_ConfigInt[configid_int_overlay_detached_origin] < ovrl_origin_MAX)
        return m_ConfigOverlayDetachedTransform[m_ConfigInt[configid_int_overlay_detached_origin]];
    else
        return m_ConfigOverlayDetachedTransform[ovrl_origin_room];
}

const std::string& ConfigManager::GetApplicationPath() const
{
    return m_ApplicationPath;
}

const std::string& ConfigManager::GetExecutableName() const
{
	return m_ExecutableName;
}