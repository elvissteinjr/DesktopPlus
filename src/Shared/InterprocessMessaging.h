//Desktop+ uses custom Win32 messages sent across processes for interprocess communication
//It's generally expected to use matching builds of the dashboard overlay and UI application, as the UI is launched by the dashboard process
//Due to that, there's no version checking or similar, just some raw messages to get things done
//This header and its implemenation is shared between both applications' code
//The IPCManager class doesn't write to any variables after construction, so calling it from other threads is safe

#pragma once

#include <string>
#define NOMINMAX
#include <windows.h>

#include "ConfigManager.h"

LPCWSTR const g_WindowClassNameDashboardApp = L"elvdesktop";
LPCWSTR const g_WindowClassNameUIApp        = L"elvdesktopUI";
LPCWSTR const g_WindowClassNameElevatedMode = L"elvdesktopelevated";
const char* const g_AppKeyDashboardApp      = "steam.overlay.1494460";                  //1494460 is the appid on Steam, but we just use this for all builds
const char* const g_AppKeyUIApp             = "elvissteinjr.DesktopPlusUI";
const char* const g_AppKeyTheaterScreen     = "elvissteinjr.DesktopPlusTheaterScreen";  //We need an app with "starts_theater_mode", but we can't add that to the app manifest written by Steam

enum IPCMsgID
{
    ipcmsg_action,            //wParam = IPCActionID, lParam = Action-specific value. Many things will get away with being stuffed inside this. Saves some global message IDs
    ipcmsg_set_config,        //wParam = ConfigID, lParam = Value. Generic ConfigIDs are derived from their specific ID + predecending *_MAX values.
                              //e.g. configid_float_stuff is configid_bool_MAX + configid_int_MAX + configid_float_stuff. Strings are handled separately
    ipcmsg_elevated_action,   //wParam = IPCElevatedActionID, lParam = Action-specific value. Actions sent to the elevated mode process
    ipcmsg_MAX
};

enum IPCActionID
{
    ipcact_nop,
    ipcact_mirror_reset,                //Sent by dashboard application to itself when WndProc needs to trigger a mirror reset
    ipcact_overlays_reset,              //Sent by dashboard application when all overlays were reset. No data in lParam
    ipcact_overlays_ui_reset,           //Sent by dashboard application when UI overlays were reset or need shared textures re-applied. No data in lParam
    ipcact_overlay_position_reset,      //Sent by UI application to reset the detached overlay position. No data in lParam
    ipcact_overlay_position_adjust,     //Sent by UI application to adjust detached overlay position. lParam = IPCActionOverlayPosAdjustValue
    ipcact_action_delete,               //Sent by UI application to delete an Action. lParam is Action UID
    ipcact_action_do,                   //Sent by UI application to do an Action. lParam is Action UID
    ipcact_action_start,                //Sent by UI application to start an Action. lParam is Action UID. This is currently only used for input actions, other things fall back to *_do
    ipcact_action_stop,                 //Sent by UI application to stop an Action. lParam is Action UID. This is currently only used for input actions, other things fall back to *_do
    ipcact_overlay_profile_load,        //Sent by UI application when loading a profile. lParam is IPCActionOverlayProfileLoadArg + profile overlay ID (low/high word order). -2 ID is default.
                                        //Profile name is stored in configid_str_state_profile_name_load beforehand. ipcactv_ovrl_profile_multi_add queues profile IDs until called with -1
    ipcact_crop_to_active_window,       //Sent by UI application to adjust crop values to the active window. No data in lParam
    ipcact_switch_task,                 //Sent by UI application to do the Switch Task action command independent of any action
    ipcact_overlay_duplicate,           //Sent by UI application to duplicate an overlay, also making it the active one. lParam is ID of overlay the config is copied from (typically the active ID)
    ipcact_overlay_new,                 //Sent by UI application to add a new overlay. lParam is desktop ID or -2 for HWND, -3 for UI overlay
                                        //HWND is stored in configid_handle_state_arg_hwnd beforehand
    ipcact_overlay_new_drag,            //Sent by UI application to add a new overlay. lParam is desktop ID or -2 for HWND, -3 for UI overlay, + pointer distance * 100 (low/high word order, signed)
                                        //HWND is stored in configid_handle_state_arg_hwnd beforehand
    ipcact_overlay_remove,              //Sent by UI or dashboard application to remove a overlay. lParam is ID of overlay to remove (typically the active ID)
    ipcact_overlay_creation_error,      //Sent by dashboard application when an error occured during overlay creation. lParam is EVROverlayError
    ipcact_overlay_transform_sync,      //Sent by the UI application to request a sync of overlay transforms. lParam is ID of overlay to sync transform of (or -1 for full sync)
    ipcact_overlay_swap,                //Sent by the UI application to swap two overlays. lParam is the ID of overlay to swap with the current overlay
    ipcact_overlay_gaze_fade_auto,      //Sent by the UI application to automatically configure gaze fade values. No data in lParam
    ipcact_overlay_make_standalone,     //Sent by the UI application to converted overlays with duplication ID to standalone ones. lParam is ID of overlay to convert (typically the active ID)
    ipcact_winrt_thread_error,          //Sent by dashboard application when an error occured in a Graphics Capture thread. lParam is HRESULT
    ipcact_notification_show,           //Sent by dashboard application to show a notification (currently only initial setup message). No data in lParam
    ipcact_browser_navigate_to_url,     //Sent by UI application to have the overlay's browser navigate to configid_str_overlay_browser_url. lParam is ID of overlay
    ipcact_browser_recreate_context,    //Sent by UI application to have the overlay's browser context be recreated. lParam is ID of overlay
    ipcact_winmanager_drag_start,       //Sent by dashboard application's WindowManager thread to main thread to start an overlay drag. lParam is ID of overlay to drag
    ipcact_winmanager_winlist_add,      //Sent by either application's WindowManager thread to main thread to add a window to the window list. lParam is HWND
    ipcact_winmanager_winlist_update,   //Sent by WindowManager thread to main thread to update a window from the window list. lParam is HWND
    ipcact_winmanager_winlist_remove,   //Sent by WindowManager thread to main thread to remove a window from the window list. lParam is HWND
    ipcact_winmanager_focus_changed,    //Sent by dashboard application's WindowManager thread to UI app when the foreground window changed. No data in lParam
    ipcact_winmanager_text_input_focus, //Sent by dashboard application's WindowManager thread to main thread to update text input focus state. lParam is focus bool
    ipcact_sync_config_state,           //Sent by the UI application to request overlay and config state variables after a restart
    ipcact_focus_window,                //Sent by the UI application to focus a window. lParam is HWND
    ipcact_keyboard_show,               //Sent by dashboard application to show the VR keyboard. lParam is assigned overlay ID (-1 to hide, -2 for UI/global)
    ipcact_keyboard_show_auto,          //Sent by dashboard application to show the VR keyboard with auto-visibility. lParam is assigned overlay ID (-1 to hide)
    ipcact_keyboard_vkey,               //Sent by UI application in response of a VR keyboard press. lParam is IPCKeyboardKeystateFlags + Win32 key code (low/high word order)
    ipcact_keyboard_wchar,              //Sent by UI application in response of a VR keyboard press. lParam is 1 wchar + key down bool (low/high word order)
    ipcact_keyboard_ovrl_focus_enter,   //Sent by UI application in response to a VREvent_FocusEnter on the keyboard overlay. No data in lParam
    ipcact_keyboard_ovrl_focus_leave,   //Sent by UI application in response to a VREvent_FocusLeave on the keyboard overlay. No data in lParam
    ipcact_lpointer_trigger_haptics,    //Sent by UI application to trigger laser pointer haptics (short UI interaction burst). lParam is tracked device index
    ipcact_lpointer_ui_mask_rect,       //Sent by UI application to update the UI intersection mask for the dplus laser pointer. lParam is DPRect packed with DPRect::Pack16() or -1 to finish the mask
    ipcact_lpointer_ui_drag,            //Sent by dashboard application to start or finish an overlay drag of an UI laser pointer target overlay. lParam is 1 for start or 0 to finish
    ipcact_app_profile_remove,          //Sent by UI application to remove an app profile. No data in lParam, uses app key stored in configid_str_state_app_profile_key beforehand
    ipcact_global_shortcut_set,         //Sent by UI application to set a global shortcut. lParam is shortcut ID, uses Action UID stored in configid_handle_state_action_uid beforehand
    ipcact_hotkey_set,                  //Sent by UI application to set a hotkey. lParam is hotkey ID (out of range ID to create new), uses configid_str_state_hotkey_data as source (blank to delete)
    ipcact_MAX
};

//lParam for ipcact_overlay_position_adjust. First 4 bits are target, 5th bit sets increase operation
enum IPCActionOverlayPosAdjustTarget 
{
    ipcactv_ovrl_pos_adjust_updown = 0x0,
    ipcactv_ovrl_pos_adjust_rightleft,
    ipcactv_ovrl_pos_adjust_forwback,
    ipcactv_ovrl_pos_adjust_rotx,
    ipcactv_ovrl_pos_adjust_roty,
    ipcactv_ovrl_pos_adjust_rotz,
    ipcactv_ovrl_pos_adjust_lookat,
    ipcactv_ovrl_pos_adjust_increase = 0x10
};

enum IPCActionOverlayProfileLoadArg
{
    ipcactv_ovrl_profile_multi,
    ipcactv_ovrl_profile_multi_add
};

enum IPCKeyboardKeystateFlags : unsigned char
{
    kbd_keystate_flag_key_down         = 1 << 0,
    kbd_keystate_flag_lshift_down      = 1 << 1,
    kbd_keystate_flag_rshift_down      = 1 << 2,
    kbd_keystate_flag_lctrl_down       = 1 << 3,
    kbd_keystate_flag_rctrl_down       = 1 << 4,
    kbd_keystate_flag_lalt_down        = 1 << 5,
    kbd_keystate_flag_ralt_down        = 1 << 6,
    kbd_keystate_flag_capslock_toggled = 1 << 7,
    kbd_keystate_flag_MAX              = 1 << 7
};

enum IPCElevatedActionID
{
    ipceact_refresh,                   //Prompts to refresh possibly changed data, such as InputSimulator screen offsets. No data in lParam
    ipceact_mouse_move,                //lParam = X & Y (in low/high word order, signed)
    ipceact_mouse_hwheel,              //lParam = delta (float)
    ipceact_mouse_vwheel,              //lParam = delta (float)
    ipceact_pen_move,                  //lParam = X & Y (in low/high word order, signed)
    ipceact_pen_button_down,           //lParam = Button ID (0/1)
    ipceact_pen_button_up,             //lParam = Button ID (0/1)
    ipceact_pen_leave,                 //No data in lParam
    ipceact_key_down,                  //lParam = Keycodes (3 unsigned chars)
    ipceact_key_up,                    //lParam = Keycodes (3 unsigned chars)
    ipceact_key_toggle,                //lParam = Keycodes (3 unsigned chars)
    ipceact_key_press_and_release,     //lParam = Keycode  (1 unsigned char)
    ipceact_key_togglekey_set,         //lParam = Keycode & bool (low/high word order)
    ipceact_keystate_w32_set,          //lParam = Win32 Keystate & bool (low/high word order)
    ipceact_keystate_set,              //lParam = IPCKeyboardKeystateFlags & Keycode (low/high word order)
    ipceact_keyboard_text_finish,      //Finishes the keyboard text queue. Keyboard text is queued by sending strings with ipcestrid_keyboard_text*. No data in lParam
    ipceact_launch_application,        //Launches application previously defined by sending ipcestrid_launch_application_path and ipcestrid_launch_application_arg strings. No data in lParam
    ipceact_window_topmost_set,        //Sets the temporary topmost window (WindowManager::SetTempTopMostWindow()). lParam = HWND (nullptr/0 to clear)
    ipceact_MAX
};

enum IPCElevatedStringID
{
    ipcestrid_keyboard_text,
    ipcestrid_keyboard_text_force_unicode,
    ipcestrid_launch_application_path,            //This also resets ipcestrid_launch_application_arg so sending that can be avoided
    ipcestrid_launch_application_arg
};

class IPCManager
{
    private:
        UINT m_RegisteredMessages[ipcmsg_MAX];

    public:
        IPCManager();
        static IPCManager& Get();
        void DisableUIPForRegisteredMessages(HWND window_handle) const;   //Disables User Interface Privilege Isolation in order to enable unelevated UI application to send messages to the overlay
        UINT GetWin32MessageID(IPCMsgID IPC_id) const;
        IPCMsgID GetIPCMessageID(UINT win32_id) const;
        bool GetAllMesssagesRegistered() const;			//Sanity check in case registering the messages fails, which should usually not happen
        static bool IsDashboardAppRunning();
        static bool IsUIAppRunning();
        static bool IsElevatedModeProcessRunning();
        static DWORD GetDashboardAppProcessID();
        static DWORD GetUIAppProcessID();

        void PostMessageToDashboardApp(IPCMsgID IPC_id, WPARAM w_param = 0, LPARAM l_param = 0) const;
        void PostConfigMessageToDashboardApp(ConfigID_Bool   configid, LPARAM l_param = 0) const;
        void PostConfigMessageToDashboardApp(ConfigID_Int    configid, LPARAM l_param = 0) const;
        void PostConfigMessageToDashboardApp(ConfigID_Float  configid, float value = 0.0f) const;
        void PostConfigMessageToDashboardApp(ConfigID_Handle configid, LPARAM l_param = 0) const;

        void PostMessageToUIApp(IPCMsgID IPC_id, WPARAM w_param = 0, LPARAM l_param = 0) const;
        void PostConfigMessageToUIApp(ConfigID_Bool   configid, LPARAM l_param = 0) const;
        void PostConfigMessageToUIApp(ConfigID_Int    configid, LPARAM l_param = 0) const;
        void PostConfigMessageToUIApp(ConfigID_Float  configid, float value = 0.0f) const;
        void PostConfigMessageToUIApp(ConfigID_Handle configid, LPARAM l_param = 0) const;

        void PostMessageToElevatedModeProcess(IPCMsgID IPC_id, WPARAM w_param = 0, LPARAM l_param = 0) const;

        void SendStringToDashboardApp(ConfigID_String config_id, const std::string& str, HWND source_window) const;
        void SendStringToUIApp(ConfigID_String config_id, const std::string& str, HWND source_window) const;
        void SendStringToElevatedModeProcess(IPCElevatedStringID elevated_str_id, const std::string& str, HWND source_window) const;
};