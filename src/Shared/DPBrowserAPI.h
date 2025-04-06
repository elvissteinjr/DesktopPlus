#pragma once

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

#include "openvr.h"

static const int k_lDPBrowserAPIVersion = 5;
LPCWSTR const g_WindowClassNameBrowserApp        = L"elvdesktopbrowser";
LPCWSTR const g_WindowMessageNameBrowserApp      = L"WMIPC_DPLUS_BrowserCommand";
const char* const g_AppKeyBrowserApp             = "elvissteinjr.DesktopPlusBrowser";
const char* const g_RelativeWorkingDirBrowserApp = "DesktopPlusBrowser";                //Path relative to Desktop+ application directory
const char* const g_ExeNameBrowserApp            = "DesktopPlusBrowser.exe";            //Path relative to g_RelativeWorkingDirBrowserApp

enum DPBrowserICPCommandID
{
    dpbrowser_ipccmd_get_api_version,           //No value in lParam, use SendMessage(), gets the supported API version from the browser process
    dpbrowser_ipccmd_set_overlay_target,        //lParam = overlay_handle, sets target overlay for commands that use lParam for other arguments | Also sent by browser process
    dpbrowser_ipccmd_start_browser,             //lParam = use_transparent_background bool, uses set_overlay_target & dpbrowser_ipcstr_url arg
    dpbrowser_ipccmd_duplicate_browser_output,  //lParam = overlay_handle_dst, uses set_overlay_target arg (overlay_handle_src)
    dpbrowser_ipccmd_pause_browser,             //lParam = pause bool, uses set_overlay_target arg
    dpbrowser_ipccmd_recreate_browser,          //lParam = use_transparent_background bool, uses set_overlay_target & dpbrowser_ipcstr_url arg
    dpbrowser_ipccmd_stop_browser,              //lParam = overlay_handle
    dpbrowser_ipccmd_set_url,                   //lParam = overlay_handle, uses dpbrowser_ipcstr_url arg
    dpbrowser_ipccmd_set_resoution,             //lParam = Width & Height (in low/high word order, signed), uses set_overlay_target arg
    dpbrowser_ipccmd_set_fps,                   //lParam = fps, uses set_overlay_target arg
    dpbrowser_ipccmd_set_zoom,                  //lParam = Zoom (float packed as LPARAM), uses set_overlay_target arg
    dpbrowser_ipccmd_set_ou3d_crop,             //lParam = DPRect packed with DPRect::Pack16() or -1 to disable, uses set_overlay_target arg
    dpbrowser_ipccmd_mouse_move,                //lParam = X & Y (in low/high word order, signed), uses set_overlay_target arg
    dpbrowser_ipccmd_mouse_leave,               //lParam = overlay_handle
    dpbrowser_ipccmd_mouse_down,                //lParam = EVRMouseButton, uses set_overlay_target arg
    dpbrowser_ipccmd_mouse_up,                  //lParam = EVRMouseButton, uses set_overlay_target arg
    dpbrowser_ipccmd_scroll,                    //lParam = X-Delta & Y-Delta (in low/high word order, floats packed as DWORDs), uses set_overlay_target arg
    dpbrowser_ipccmd_keyboard_vkey,             //lParam = DPBrowserIPCKeyboardKeystateFlags + Win32 key code (low/high word order), uses set_overlay_target arg
    dpbrowser_ipccmd_keyboard_vkey_toggle,      //lParam = Win32 key code, uses set_overlay_target arg
    dpbrowser_ipccmd_keyboard_wchar,            //lParam = 1 wchar + key down bool (low/high word order), uses set_overlay_target arg
    dpbrowser_ipccmd_keyboard_string,           //lParam = overlay_handle, uses dpbrowser_ipcstr_keyboard_string
    dpbrowser_ipccmd_go_back,                   //lParam = overlay_handle
    dpbrowser_ipccmd_go_forward,                //lParam = overlay_handle
    dpbrowser_ipccmd_refresh,                   //lParam = overlay_handle, will stop if the page is currently loading
    dpbrowser_ipccmd_global_set_fps,            //lParam = fps, global setting
    dpbrowser_ipccmd_cblock_set_enabled,        //lParam = enabled bool
    dpbrowser_ipccmd_error_set_strings,         //No value in lParam, uses dpbrowser_ipcstr_tstr_error_* args
    dpbrowser_ipccmd_notify_ready,              //No value in lParam | Sent by browser process to dashboard & UI process
    dpbrowser_ipccmd_notify_nav_state,          //lParam = DPBrowserIPCNavStateFlags, uses set_overlay_target arg | Sent by browser process to UI process
    dpbrowser_ipccmd_notify_url_changed,        //lParam = overlay_handle, uses dpbrowser_ipcstr_url arg | Sent by browser process to UI process
    dpbrowser_ipccmd_notify_title_changed,      //lParam = overlay_handle, uses dpbrowser_ipcstr_title arg | Sent by browser process to UI process
    dpbrowser_ipccmd_notify_fps,                //lParam = fps, uses set_overlay_target arg | Sent by browser process to UI process
    dpbrowser_ipccmd_notify_lpointer_haptics,   //No value in lParam, triggers short UI interaction burst on primary device | Sent by browser process to dashboard process
    dpbrowser_ipccmd_notify_keyboard_show,      //lParam = show bool, uses set_overlay_target arg | Sent by browser process to UI process
    dpbrowser_ipccmd_notify_cblock_list_count,  //lParam = list count | Sent by browser process to UI process
};

enum DPBrowserICPStringID
{
    dpbrowser_ipcstr_MIN = 1000,                //Start IDs at a higher value to avoid conflicts with config string messages on a client
    dpbrowser_ipcstr_url = 1000,                //URL string for upcoming command/notification
    dpbrowser_ipcstr_title,                     //Title string for upcoming notification
    dpbrowser_ipcstr_keyboard_string,           //Keyboard string for upcoming command
    dpbrowser_ipcstr_tstr_error_title,          //Error page translation string
    dpbrowser_ipcstr_tstr_error_heading,        //Error page translation string
    dpbrowser_ipcstr_tstr_error_message,        //Error page translation string
    dpbrowser_ipcstr_MAX
};

enum DPBrowserIPCNavStateFlags : unsigned char
{
    dpbrowser_ipcnavstate_flag_can_go_back    = 1 << 0,
    dpbrowser_ipcnavstate_flag_can_go_forward = 1 << 1,
    dpbrowser_ipcnavstate_flag_is_loading     = 1 << 2,
    dpbrowser_ipcnavstate_flag_MAX            = 1 << 7
};

enum DPBrowserIPCKeyboardKeystateFlags : unsigned char
{
    dpbrowser_ipckbd_keystate_flag_key_down         = 1 << 0,
    dpbrowser_ipckbd_keystate_flag_lshift_down      = 1 << 1,
    dpbrowser_ipckbd_keystate_flag_rshift_down      = 1 << 2,
    dpbrowser_ipckbd_keystate_flag_lctrl_down       = 1 << 3,
    dpbrowser_ipckbd_keystate_flag_rctrl_down       = 1 << 4,
    dpbrowser_ipckbd_keystate_flag_lalt_down        = 1 << 5,
    dpbrowser_ipckbd_keystate_flag_ralt_down        = 1 << 6,
    dpbrowser_ipckbd_keystate_flag_capslock_toggled = 1 << 7,
    dpbrowser_ipckbd_keystate_flag_MAX              = 1 << 7
};

//Common interface for implemented by DPBrowserAPIServer and DPBrowserAPIClient
class DPBrowserAPI
{
    public:
        virtual void DPBrowser_StartBrowser(vr::VROverlayHandle_t overlay_handle, const std::string& url, bool use_transparent_background) = 0;
        virtual void DPBrowser_DuplicateBrowserOutput(vr::VROverlayHandle_t overlay_handle_src, vr::VROverlayHandle_t overlay_handle_dst) = 0;
        virtual void DPBrowser_PauseBrowser(vr::VROverlayHandle_t overlay_handle, bool pause) = 0;
        virtual void DPBrowser_RecreateBrowser(vr::VROverlayHandle_t overlay_handle, bool use_transparent_background) = 0;
        virtual void DPBrowser_StopBrowser(vr::VROverlayHandle_t overlay_handle) = 0;

        virtual void DPBrowser_SetURL(vr::VROverlayHandle_t overlay_handle, const std::string& url) = 0;
        virtual void DPBrowser_SetResolution(vr::VROverlayHandle_t overlay_handle, int width, int height) = 0;
        virtual void DPBrowser_SetFPS(vr::VROverlayHandle_t overlay_handle, int fps) = 0;
        virtual void DPBrowser_SetZoomLevel(vr::VROverlayHandle_t overlay_handle, float zoom_level) = 0;
        virtual void DPBrowser_SetOverUnder3D(vr::VROverlayHandle_t overlay_handle, bool is_over_under_3D, int crop_x, int crop_y, int crop_width, int crop_height) = 0;

        virtual void DPBrowser_MouseMove(vr::VROverlayHandle_t overlay_handle, int x, int y) = 0;
        virtual void DPBrowser_MouseLeave(vr::VROverlayHandle_t overlay_handle) = 0;
        virtual void DPBrowser_MouseDown(vr::VROverlayHandle_t overlay_handle, vr::EVRMouseButton button) = 0;
        virtual void DPBrowser_MouseUp(vr::VROverlayHandle_t overlay_handle, vr::EVRMouseButton button) = 0;
        virtual void DPBrowser_Scroll(vr::VROverlayHandle_t overlay_handle, float x_delta, float y_delta) = 0;

        virtual void DPBrowser_KeyboardSetKeyState(vr::VROverlayHandle_t overlay_handle, DPBrowserIPCKeyboardKeystateFlags flags, unsigned char keycode) = 0;
        virtual void DPBrowser_KeyboardToggleKey(vr::VROverlayHandle_t overlay_handle, unsigned char keycode) = 0;
        virtual void DPBrowser_KeyboardTypeWChar(vr::VROverlayHandle_t overlay_handle, wchar_t wchar, bool down) = 0;
        virtual void DPBrowser_KeyboardTypeString(vr::VROverlayHandle_t overlay_handle, const std::string& str) = 0;

        virtual void DPBrowser_GoBack(vr::VROverlayHandle_t overlay_handle) = 0;
        virtual void DPBrowser_GoForward(vr::VROverlayHandle_t overlay_handle) = 0;
        virtual void DPBrowser_Refresh(vr::VROverlayHandle_t overlay_handle) = 0;

        virtual void DPBrowser_GlobalSetFPS(int fps) = 0;
        virtual void DPBrowser_ContentBlockSetEnabled(bool enable) = 0;
        virtual void DPBrowser_ErrorPageSetStrings(const std::string& title, const std::string& heading, const std::string& message) = 0;
};