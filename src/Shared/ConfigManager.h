//Both the Desktop+ dashboard overlay and the UI hold a copy of the current configuration
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
#include "Ini.h"

//Settings enums
//These IDs are also passed via IPC
//configid_*_state entries are not stored/persistent and are just a simpler way to sync state

enum ConfigID_Bool
{
    configid_bool_overlay_detached,
    configid_bool_overlay_3D_swapped,
    configid_bool_interface_no_ui,
    configid_bool_interface_mainbar_desktop_include_all,
    configid_bool_interface_warning_compositor_res_hidden,
    configid_bool_interface_warning_compositor_quality_hidden,
    configid_bool_interface_warning_process_elevation_hidden,
    configid_bool_performance_ignore_early_updates,
    configid_bool_performance_rapid_laser_pointer_updates,
    configid_bool_input_enabled,
    configid_bool_input_mouse_render_cursor,
    configid_bool_input_mouse_render_intersection_blob,
    configid_bool_input_mouse_hmd_pointer_override,
    configid_bool_input_keyboard_helper_enabled,
    configid_bool_state_overlay_dragmode,
    configid_bool_state_keyboard_visible_for_dashboard,
    configid_bool_state_performance_stats_active,             //Only count when the stats are visible
    configid_bool_state_performance_gpu_copy_active,
    configid_bool_state_misc_process_elevated,                //True if the dashboard application is running with admin privileges
	configid_bool_MAX
};

enum ConfigID_Int
{
    configid_int_overlay_desktop_id,
    configid_int_overlay_crop_x,
    configid_int_overlay_crop_y,
    configid_int_overlay_crop_width,
    configid_int_overlay_crop_height,
    configid_int_overlay_3D_mode,
    configid_int_overlay_detached_display_mode,
    configid_int_overlay_detached_origin,
    configid_int_interface_mainbar_desktop_listing,
    configid_int_interface_wmr_ignore_vscreens_selection,         //This and the setting below assumes that the WMR virtual screens are the 3 last ones... this can only go well
    configid_int_interface_wmr_ignore_vscreens_combined_desktop,  //For both, -1 means auto/unset which is the value non-WMR users get
    configid_int_input_go_home_action_id,
    configid_int_input_go_back_action_id,
    configid_int_input_shortcut01_action_id,
    configid_int_input_shortcut02_action_id,
    configid_int_input_shortcut03_action_id,
    configid_int_input_mouse_dbl_click_assist_duration_ms,
    configid_int_state_action_current,                      //Action changes are synced through a series of individually sent state settings. This one sets the target custom action (ID start 0)
    configid_int_state_action_current_sub,                  //Target variable. 0 = Name, 1 = Function Type. Remaining values depend on the function. Not the cleanest way but easier
    configid_int_state_action_value_int,                    //to set up with existing IPC stuff
    configid_int_state_mouse_dbl_click_assist_duration_ms,  //Internally used value, which will replace -1 with the current double-click delay automatically
    configid_int_state_performance_duplication_fps,
	configid_int_MAX
};

enum ConfigID_Float
{
    configid_float_overlay_width,
    configid_float_overlay_curvature,
    configid_float_overlay_opacity,
    configid_float_overlay_offset_right,
    configid_float_overlay_offset_up,
    configid_float_overlay_offset_forward,
    configid_float_input_keyboard_detached_size,
    configid_float_input_detached_interaction_max_distance,
    configid_float_interface_last_vr_ui_scale,
    configid_float_performance_early_update_limit_multiplier,
	configid_float_MAX
};

enum ConfigID_String
{
    configid_str_state_detached_transform_current,
    configid_str_state_action_value_string,
    configid_str_state_ui_keyboard_string,          //SteamVR keyboard input for the UI application
    configid_str_state_dashboard_error_string,      //Error messages are displayed in VR through the UI app
    configid_str_state_profile_name_load,           //Name of the profile to load 
	configid_str_MAX
};

//Actually stored as ints, but still have this for readability
enum Overlay3DMode
{
    ovrl_3Dmode_none,
    ovrl_3Dmode_hsbs,
    ovrl_3Dmode_sbs
};

enum OverlayDisplayMode
{
    ovrl_dispmode_always,
    ovrl_dispmode_dashboard,
    ovrl_dispmode_scene,
    ovrl_dispmode_dplustab
};

enum OverlayOrigin
{
    ovrl_origin_room,
    ovrl_origin_hmd_floor,
    ovrl_origin_dashboard,
    ovrl_origin_hmd,
    ovrl_origin_right_hand,
    ovrl_origin_left_hand,
    ovrl_origin_aux,        //Tracker or whatever. No proper autodetection of additional devices yet, maybe in the future
    ovrl_origin_MAX         //Used for storing one matrix per origin
};

enum MainbarDesktopListing
{
    mainbar_desktop_listing_none,
    mainbar_desktop_listing_individual,
    mainbar_desktop_listing_cycle
};

class ConfigManager
{
	private:
		bool m_ConfigBool[configid_bool_MAX];
		int m_ConfigInt[configid_int_MAX];
		float m_ConfigFloat[configid_float_MAX];
		std::string m_ConfigString[configid_str_MAX];
        Matrix4 m_ConfigOverlayDetachedTransform[ovrl_origin_MAX];

        ActionManager m_ActionManager;

        std::string m_ApplicationPath;
        std::string m_ExecutableName;

        void LoadOverlayProfile(Ini& config);
        void SaveOverlayProfile(Ini& config);

	public:
		ConfigManager();
		static ConfigManager& Get();
        
        bool LoadConfigFromFile();
        void SaveConfigToFile();

        void LoadOverlayProfileDefault();
        bool LoadOverlayProfileFromFile(const std::string filename);
        void SaveOverlayProfileToFile(const std::string filename);
        bool DeleteOverlayProfile(const std::string filename);
        std::vector<std::string> GetOverlayProfileList();

        static WPARAM GetWParamForConfigID(ConfigID_Bool id);
        static WPARAM GetWParamForConfigID(ConfigID_Int id);
        static WPARAM GetWParamForConfigID(ConfigID_Float id);

        void SetConfigBool(ConfigID_Bool id, bool value);
        void SetConfigInt(ConfigID_Int id, int value);
        void SetConfigFloat(ConfigID_Float id, float value);
        void SetConfigString(ConfigID_String id, const std::string& value);
        bool GetConfigBool(ConfigID_Bool id) const;
        int GetConfigInt(ConfigID_Int id) const;
        float GetConfigFloat(ConfigID_Float id) const;
        const std::string& GetConfigString(ConfigID_String id) const;
        //These are meant for direct use with ImGui widgets
        bool& GetConfigBoolRef(ConfigID_Bool id);
        int& GetConfigIntRef(ConfigID_Int id);
        float& GetConfigFloatRef(ConfigID_Float id);

        ActionManager& GetActionManager();
        std::vector<CustomAction>& GetCustomActions();
        std::vector<ActionMainBarOrderData>& GetActionMainBarOrder();
        Matrix4& GetOverlayDetachedTransform();

		const std::string& GetApplicationPath() const;
		const std::string& GetExecutableName() const;
};