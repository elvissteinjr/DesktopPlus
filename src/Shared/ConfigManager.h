//Both the Desktop+ dashboard app and the UI hold a copy of the current configuration
//Both load the initial state from config.ini
//Changes are generally done by the UI and sent to the dashboard application via Win32 messages
//Only the UI writes to config.ini

#pragma once

#include <string>
#include <vector>
#define NOMINMAX
#include <windows.h>

#include "Matrices.h"
#include "Actions.h"
#include "AppProfiles.h"
#include "openvr.h"

//Settings enums
//These IDs are also passed via IPC
//configid_*_state entries are not stored/persistent and are just a simpler way to sync state

enum ConfigID_Bool
{
    configid_bool_overlay_name_custom,
    configid_bool_overlay_enabled,
    configid_bool_overlay_crop_enabled,
    configid_bool_overlay_browser_allow_transparency,
    configid_bool_overlay_3D_enabled,
    configid_bool_overlay_3D_swapped,
    configid_bool_overlay_origin_hmd_floor_use_turning,
    configid_bool_overlay_transform_locked,
    configid_bool_overlay_gazefade_enabled,
    configid_bool_overlay_input_enabled,
    configid_bool_overlay_input_dplus_lp_enabled,
    configid_bool_overlay_winrt_window_matching_strict,
    configid_bool_overlay_update_invisible,
    configid_bool_overlay_floatingui_enabled,
    configid_bool_overlay_floatingui_desktops_enabled,
    configid_bool_overlay_actionbar_enabled,
    configid_bool_overlay_actionbar_order_use_global,
    configid_bool_overlay_state_no_output,
    configid_bool_overlay_state_browser_allow_transparency_is_pending,
    configid_bool_overlay_state_browser_nav_can_go_back,
    configid_bool_overlay_state_browser_nav_can_go_forward,
    configid_bool_overlay_state_browser_nav_is_loading,
    configid_bool_overlay_MAX,
    configid_bool_interface_no_ui,
    configid_bool_interface_no_notification_icon,
    configid_bool_interface_show_advanced_settings,
    configid_bool_interface_large_style,
    configid_bool_interface_dim_ui,
    configid_bool_interface_blank_space_drag_enabled,
    configid_bool_interface_desktop_buttons_include_combined,
    configid_bool_interface_warning_compositor_res_hidden,
    configid_bool_interface_warning_compositor_quality_hidden,
    configid_bool_interface_warning_process_elevation_hidden,
    configid_bool_interface_warning_elevated_mode_hidden,
    configid_bool_interface_warning_browser_missing_hidden,
    configid_bool_interface_warning_browser_version_mismatch_hidden,
    configid_bool_interface_warning_app_profile_active_hidden,
    configid_bool_interface_warning_welcome_hidden,
    configid_bool_interface_window_settings_restore_state,
    configid_bool_interface_window_properties_restore_state,
    configid_bool_interface_window_keyboard_restore_state,
    configid_bool_performance_rapid_laser_pointer_updates,
    configid_bool_performance_single_desktop_mirroring,
    configid_bool_performance_show_fps,
    configid_bool_performance_monitor_large_style,
    configid_bool_performance_monitor_show_graphs,
    configid_bool_performance_monitor_show_time,
    configid_bool_performance_monitor_show_cpu,
    configid_bool_performance_monitor_show_gpu,
    configid_bool_performance_monitor_show_fps,
    configid_bool_performance_monitor_show_battery,
    configid_bool_performance_monitor_show_trackers,
    configid_bool_performance_monitor_show_vive_wireless,
    configid_bool_performance_monitor_disable_gpu_counters,
    configid_bool_input_global_hmd_pointer,
    configid_bool_input_mouse_render_cursor,
    configid_bool_input_mouse_render_intersection_blob,
    configid_bool_input_mouse_scroll_smooth,
    configid_bool_input_mouse_allow_pointer_override,
    configid_bool_input_keyboard_cluster_function_enabled,
    configid_bool_input_keyboard_cluster_navigation_enabled,
    configid_bool_input_keyboard_cluster_numpad_enabled,
    configid_bool_input_keyboard_cluster_extra_enabled,
    configid_bool_input_keyboard_sticky_modifiers,
    configid_bool_input_keyboard_key_repeat,
    configid_bool_input_keyboard_auto_show_desktop,
    configid_bool_input_keyboard_auto_show_browser,
    configid_bool_input_laser_pointer_block_input,
    configid_bool_input_drag_auto_docking,
    configid_bool_input_drag_force_upright,
    configid_bool_input_drag_fixed_distance,
    configid_bool_input_drag_fixed_distance_auto_curve,
    configid_bool_input_drag_fixed_distance_auto_tilt,
    configid_bool_input_drag_snap_position,
    configid_bool_windows_auto_focus_scene_app_dashboard,
    configid_bool_windows_winrt_auto_focus,
    configid_bool_windows_winrt_keep_on_screen,
    configid_bool_windows_winrt_auto_size_overlay,
    configid_bool_windows_winrt_auto_focus_scene_app,
    configid_bool_browser_content_blocker,
    configid_bool_misc_no_steam,                              //Restarts without Steam when it detects to have been launched by Steam
    configid_bool_misc_uiaccess_was_enabled,                  //Tracks if UIAccess was enabled to show a warning after it isn't anymore due to updates or modified executable
    configid_bool_state_overlay_dragmode,
    configid_bool_state_overlay_selectmode,
    configid_bool_state_overlay_dragselectmode_show_hidden,   //True if mode is from a popup
    configid_bool_state_overlay_dragmode_temp,
    configid_bool_state_window_focused_process_elevated,
    configid_bool_state_keyboard_visible,
    configid_bool_state_performance_gpu_copy_active,
    configid_bool_state_misc_process_elevated,                //True if the dashboard application is running with admin privileges
    configid_bool_state_misc_elevated_mode_active,            //True if the elevated mode process is running
    configid_bool_state_misc_browser_used_but_missing,        //True if the browser capture source is used, but browser component isn't available
    configid_bool_state_misc_browser_version_mismatch,        //True if the browser is available, but the API version isn't compatible
    configid_bool_state_misc_process_started_by_steam,
    configid_bool_state_misc_uiaccess_enabled,
    configid_bool_MAX
};

enum ConfigID_Int
{
    configid_int_overlay_desktop_id,                        //-1 is combined desktop, -2 is a default value that initializes crop to desktop 0
    configid_int_overlay_capture_source,
    configid_int_overlay_duplication_id,                    //-1 is none, overlay id, only used for browser overlays
    configid_int_overlay_winrt_desktop_id,                  //-1 is combined desktop, -2 is unset
    configid_int_overlay_user_width,
    configid_int_overlay_user_height,
    configid_int_overlay_crop_x,
    configid_int_overlay_crop_y,
    configid_int_overlay_crop_width,
    configid_int_overlay_crop_height,
    configid_int_overlay_3D_mode,
    configid_int_overlay_display_mode,
    configid_int_overlay_origin,
    configid_int_overlay_update_limit_override_mode,
    configid_int_overlay_update_limit_override_fps,
    configid_int_overlay_browser_max_fps_override,
    configid_int_overlay_state_content_width,
    configid_int_overlay_state_content_height,
    configid_int_overlay_state_fps,
    configid_int_overlay_MAX,
    configid_int_interface_overlay_current_id,
    configid_int_interface_desktop_listing_style,
    configid_int_interface_background_color,
    configid_int_interface_background_color_display_mode,
    configid_int_interface_wmr_ignore_vscreens,             //-1 means auto/unset which is the value non-WMR users get
    configid_int_input_hotkey01_modifiers,
    configid_int_input_hotkey01_keycode,
    configid_int_input_hotkey02_modifiers,
    configid_int_input_hotkey02_keycode,
    configid_int_input_hotkey03_modifiers,
    configid_int_input_hotkey03_keycode,
    configid_int_input_mouse_dbl_click_assist_duration_ms,
    configid_int_input_drag_fixed_distance_shape,           //0 = Sphere, 1 = Cylinder
    configid_int_windows_winrt_dragging_mode,
    configid_int_windows_winrt_capture_lost_behavior,
    configid_int_browser_max_fps,                           //Browser overlays use this instead of update limits
    configid_int_performance_update_limit_mode,
    configid_int_performance_update_limit_fps,              //This is the enum ID, not the actual number. See ApplySettingUpdateLimiter() code for more info
    configid_int_state_overlay_current_id_override,         //This is used to send config changes to overlays which aren't the current, mainly to avoid the UI switching around (-1 is disabled)
    configid_int_state_overlay_transform_sync_target_id,    //Target overlay ID for transform sync. -1 = None
    configid_int_state_overlay_focused_id,                  //Focused overlay ID (set by last click) for keyboard overlay target if applicable. -1 = None
    configid_int_state_mouse_dbl_click_assist_duration_ms,  //Internally used value, which will replace -1 with the current double-click delay automatically
    configid_int_state_performance_duplication_fps,
    configid_int_state_interface_desktop_count,             //Count of desktops after optionally filtering virtual WMR displays
    configid_int_state_interface_floating_ui_hovered_id,    //Floating UI target overlay ID set only while the laser pointer is pointing at the Floating UI overlay. -1 = None
    configid_int_state_auto_docking_state,                  //0 = Off, 1 = Left Hand, 2 = Right Hand (matches ETrackedControllerRole). +2 for detaching
    configid_int_state_drag_hint_device,                    //Value is tracked device index. Set before changing configid_int_state_drag_hint_type
    configid_int_state_drag_hint_type,                      //0 = Off, 1 = Overlay Position Locked, 2 = Theater Screen Overlay Drag Block
    configid_int_state_laser_pointer_device_hint,           //Used by dragging functions when laser pointer device can't be determined via other means (value is tracked device index)
    configid_int_state_dplus_laser_pointer_device,          //Tracked device index for active Desktop+ laser pointer
    configid_int_state_browser_content_blocker_list_count,
    configid_int_MAX
};

enum ConfigID_Float
{
    configid_float_overlay_width,
    configid_float_overlay_curvature,
    configid_float_overlay_opacity,
    configid_float_overlay_brightness,
    configid_float_overlay_browser_zoom,
    configid_float_overlay_offset_right,
    configid_float_overlay_offset_up,
    configid_float_overlay_offset_forward,
    configid_float_overlay_gazefade_distance,
    configid_float_overlay_gazefade_rate,
    configid_float_overlay_gazefade_opacity,
    configid_float_overlay_update_limit_override_ms,
    configid_float_overlay_MAX,
    configid_float_input_detached_interaction_max_distance,
    configid_float_input_global_hmd_pointer_max_distance,
    configid_float_input_drag_fixed_distance_m,
    configid_float_input_drag_snap_position_size,
    configid_float_interface_desktop_ui_scale_override,
    configid_float_interface_last_vr_ui_scale,
    configid_float_performance_update_limit_ms,
    configid_float_state_overlay_transform_sync_value,      //Used for sending overlay transforms asynchronously. This is done by sending 16 of these after setting the target id
    configid_float_MAX
};

enum ConfigID_Handle
{
    configid_handle_overlay_state_overlay_handle,             //Overlay handle for syncing with the UI. Shouldn't change during runtime
    configid_handle_overlay_state_winrt_hwnd,                 //HWNDs are technically always in 32-bit range, but avoiding truncation warnings and perhaps some other issues here
    configid_handle_overlay_state_winrt_last_hicon,           //HICON kept around for when window goes missing but the icon itself is still cached in UI app
    configid_handle_overlay_MAX,
    configid_handle_input_go_home_action_uid,
    configid_handle_input_go_back_action_uid,
    configid_handle_input_shortcut01_action_uid,
    configid_handle_input_shortcut02_action_uid,
    configid_handle_input_shortcut03_action_uid,
    configid_handle_input_shortcut04_action_uid,
    configid_handle_input_shortcut05_action_uid,
    configid_handle_input_shortcut06_action_uid,
    configid_handle_input_hotkey01_action_uid,
    configid_handle_input_hotkey02_action_uid,
    configid_handle_input_hotkey03_action_uid,
    configid_handle_state_arg_hwnd,                           //Used when a HWND is needed as an ipcact message argument
    configid_handle_state_dplus_laser_pointer_target_overlay, //Overlay handle for active Desktop+ laser pointer
    configid_handle_state_action_uid,                         //Used when an action UID is needed as an ipcact message argument and message space is needed for something else
    configid_handle_MAX
};

enum ConfigID_String
{
    configid_str_overlay_winrt_last_window_title,
    configid_str_overlay_winrt_last_window_class_name,
    configid_str_overlay_winrt_last_window_exe_name,
    configid_str_overlay_browser_url,                       //Current URL
    configid_str_overlay_browser_url_user_last,             //Last manually entered URL
    configid_str_overlay_browser_title,
    configid_str_overlay_tags,
    configid_str_overlay_MAX,
    configid_str_interface_language_file,
    configid_str_input_keyboard_layout_file,
    configid_str_browser_extra_arguments,                   //Extra command-line arguments passed to DPBrowser executable
    configid_str_state_ui_keyboard_string,                  //SteamVR keyboard input for the UI application
    configid_str_state_keyboard_string,                     //VR keyboard input for the dashboard application
    configid_str_state_dashboard_error_string,              //Error messages are displayed in VR through the UI app
    configid_str_state_profile_name_load,                   //Name of the profile to load 
    configid_str_state_app_profile_key,                     //Target app key for app profile synching
    configid_str_state_app_profile_data,                    //Serialized data string of app profile for synching
    configid_str_state_action_data,                         //Serialized data string of action for synching
    configid_str_MAX
};

//Actually stored as ints, but still have this for readability
enum OverlayCaptureSource
{
    ovrl_capsource_desktop_duplication,
    ovrl_capsource_winrt_capture,
    ovrl_capsource_ui,
    ovrl_capsource_browser
};

enum Overlay3DMode
{
    ovrl_3Dmode_hsbs,
    ovrl_3Dmode_sbs,
    ovrl_3Dmode_hou,
    ovrl_3Dmode_ou,
    ovrl_3Dmode_MAX
};

enum OverlayDisplayMode
{
    ovrl_dispmode_always,
    ovrl_dispmode_dashboard,
    ovrl_dispmode_scene,
    ovrl_dispmode_dplustab,
    ovrl_dispmode_MAX
};

enum OverlayOrigin
{
    ovrl_origin_room,
    ovrl_origin_hmd_floor,
    ovrl_origin_seated_universe,
    ovrl_origin_dashboard,
    ovrl_origin_hmd,
    ovrl_origin_left_hand,
    ovrl_origin_right_hand,
    ovrl_origin_aux,            //Tracker or whatever. No proper autodetection of additional devices yet, maybe in the future
    ovrl_origin_theater_screen,
    ovrl_origin_dplus_tab,      //Desktop+ dashboard dummy overlay, more reliable than dashboard origin. Not used by user overlays
    ovrl_origin_MAX
};

//Not stored in config but used to pass origin config values to functions dealing with overlay origins
struct OverlayOriginConfig
{
    bool HMDFloorUseTurning = false;
};

enum DesktopListingStyle
{
    desktop_listing_style_none,
    desktop_listing_style_individual,
    desktop_listing_style_cycle
};

enum InterfaceBGColorDisplayMode
{
    ui_bgcolor_dispmode_never,
    ui_bgcolor_dispmode_dplustab,
    ui_bgcolor_dispmode_always
};

enum UpdateLimitMode
{
    update_limit_mode_off,
    update_limit_mode_ms,
    update_limit_mode_fps
};

enum UpdateLimitFPS
{
    update_limit_fps_1,
    update_limit_fps_2,
    update_limit_fps_5,
    update_limit_fps_10,
    update_limit_fps_15,
    update_limit_fps_20,
    update_limit_fps_25,
    update_limit_fps_30,
    update_limit_fps_40,
    update_limit_fps_50
};

enum WindowDraggingMode
{
    window_dragging_none,
    window_dragging_block,
    window_dragging_overlay,
    window_dragging_MAX
};

enum WindowCaptureLostBehavior
{
    window_caplost_do_nothing,
    window_caplost_hide_overlay,
    window_caplost_remove_overlay,
    window_caplost_MAX
};

class OverlayConfigData
{
    public:
        std::string ConfigNameStr;
        bool ConfigBool[configid_bool_overlay_MAX];
        int ConfigInt[configid_int_overlay_MAX];
        float ConfigFloat[configid_float_overlay_MAX];
        uint64_t ConfigHandle[configid_handle_overlay_MAX];
        std::string ConfigStr[configid_str_overlay_MAX];
        Matrix4 ConfigTransform;
        ActionManager::ActionList ConfigActionBarOrder;

        OverlayConfigData();
};

class Ini;

class ConfigManager
{
    private:
        bool m_ConfigBool[configid_bool_MAX];
        int m_ConfigInt[configid_int_MAX];
        float m_ConfigFloat[configid_float_MAX];
        uint64_t m_ConfigHandle[configid_handle_MAX];
        std::string m_ConfigString[configid_str_MAX];

        ActionManager m_ActionManager;
        AppProfileManager m_AppProfileManager;

        std::string m_ApplicationPath;
        std::string m_ExecutableName;
        bool m_IsSteamInstall;

        void LoadOverlayProfile(const Ini& config, unsigned int overlay_id = UINT_MAX);
        void SaveOverlayProfile(Ini& config, unsigned int overlay_id = UINT_MAX);
        void LoadMultiOverlayProfile(const Ini& config, bool clear_existing_overlays = true, std::vector<char>* ovrl_inclusion_list = nullptr);
        void SaveMultiOverlayProfile(Ini& config, std::vector<char>* ovrl_inclusion_list = nullptr);

        #ifdef DPLUS_UI
            void LoadConfigPersistentWindowState(Ini& config);
            void SaveConfigPersistentWindowState(Ini& config);
        #endif

        void MigrateLegacyActionsFromConfig(const Ini& config);
        OverlayOrigin GetOverlayOriginFromConfigString(const std::string& str);
        const char* GetConfigStringForOverlayOrigin(OverlayOrigin origin);

        static bool IsUIAccessEnabled();
        static void RemoveScaleFromTransform(Matrix4& transform, float* width);

    public:
        ConfigManager();
        static ConfigManager& Get();

        bool LoadConfigFromFile();
        void SaveConfigToFile();
        void RestoreConfigFromDefault();

        void LoadOverlayProfileDefault(bool multi_overlay = false);
        bool LoadMultiOverlayProfileFromFile(const std::string& filename, bool clear_existing_overlays = true, std::vector<char>* ovrl_inclusion_list = nullptr);
        bool SaveMultiOverlayProfileToFile(const std::string& filename, std::vector<char>* ovrl_inclusion_list = nullptr);
        bool DeleteOverlayProfile(const std::string& filename);
        void DeleteAllOverlayProfiles();

        #ifdef DPLUS_UI
            std::vector<std::string> GetOverlayProfileList();
            std::vector< std::pair<std::string, OverlayOrigin> > GetOverlayProfileOverlayNameList(const std::string& filename);
        #endif

        static WPARAM GetWParamForConfigID(ConfigID_Bool id);
        static WPARAM GetWParamForConfigID(ConfigID_Int id);
        static WPARAM GetWParamForConfigID(ConfigID_Float id);
        static WPARAM GetWParamForConfigID(ConfigID_Handle id);

        static void SetValue(ConfigID_Bool   configid, bool               value);
        static void SetValue(ConfigID_Int    configid, int                value);
        static void SetValue(ConfigID_Float  configid, float              value);
        static void SetValue(ConfigID_Handle configid, uint64_t           value);
        static void SetValue(ConfigID_String configid, const std::string& value);

        static bool               GetValue(ConfigID_Bool   configid);
        static int                GetValue(ConfigID_Int    configid);
        static float              GetValue(ConfigID_Float  configid);
        static uint64_t           GetValue(ConfigID_Handle configid);
        static const std::string& GetValue(ConfigID_String configid);

        static bool&     GetRef(ConfigID_Bool   configid);
        static int&      GetRef(ConfigID_Int    configid);
        static float&    GetRef(ConfigID_Float  configid);
        static uint64_t& GetRef(ConfigID_Handle configid);

        void ResetConfigStateValues();  //Reset all configid_*_state_* settings. Used when restarting a Desktop+ process

        ActionManager& GetActionManager();
        AppProfileManager& GetAppProfileManager();
        Matrix4& GetOverlayDetachedTransform();

        const std::string& GetApplicationPath() const;
        const std::string& GetExecutableName() const;
        bool IsSteamInstall() const;
        vr::TrackedDeviceIndex_t GetPrimaryLaserPointerDevice() const;                 //GetPrimaryDashboardDevice() but works with Desktop+'s laser pointer as well
        bool IsLaserPointerTargetOverlay(vr::VROverlayHandle_t ulOverlayHandle) const; //IsHoverTargetOverlay() but works with Desktop+'s laser pointer as well
};