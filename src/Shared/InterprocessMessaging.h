//Desktop+ uses custom Win32 messages sent across processes for interprocess communication
//It's generally expected to use matching builds of the dashboard overlay and UI application, as the UI is launched by the dashboard process
//Due to that, there's no version checking or similar, just some raw messages to get things done
//This header and its implemenation is shared between both applications' code

#pragma once

#include <string>
#define NOMINMAX
#include <windows.h>

#include "ConfigManager.h"

LPCWSTR const g_WindowClassNameDashboardApp = L"elvdesktop";
LPCWSTR const g_WindowClassNameUIApp = L"elvdesktopUI";

enum IPCMsgID
{
    ipcmsg_action,            //wParam = IPCActionID, lParam = Action-specific value. Many things will get away with being stuffed inside this. Saves some global message IDs
    ipcmsg_set_config,        //wParam = ConfigID, lParam = Value. Generic ConfigIDs are derived from their specific ID + predecending *_MAX values.
                              //e.g. configid_float_stuff is configid_bool_MAX + configid_int_MAX + configid_float_stuff. Strings are handled separately
	ipcmsg_MAX
};

enum IPCActionID
{
    ipcact_nop,
    ipcact_mirror_reset,            //Sent by dashboard application to itself when WndProc needs to trigger a mirror reset
    ipcact_resolution_update,       //Sent by dashboard application when a resolution change occured. UI gets the resolution itself from OpenVR, so no data in lParam
    ipcact_overlay_position_reset,  //Sent by UI application to reset the detached overlay position. No data in lParam
    ipcact_overlay_position_adjust, //Sent by UI application to adjust detached overlay position. lParam = IPCActionOverlayPosAdjustValue
    ipcact_action_delete,           //Sent by UI application to delete an Action. lParam is Custom(!) Action ID
    ipcact_action_do,               //Sent by UI application to do an Action. lParam is Action ID
    ipcact_action_start,            //Sent by UI application to start an Action. lParam is Action ID. This is currently only used for input actions, other things fall back to *_do
    ipcact_action_stop,             //Sent by UI application to stop an Action. lParam is Action ID. This is currently only used for input actions, other things fall back to *_do
    ipcact_keyboard_helper,         //Sent by UI application in response to a keyboard helper button press. lParam is win32 key code
    ipcact_vrkeyboard_closed,       //Sent by dashboard application when VREvent_Closed occured and the keyboard is open for the UI application. No data in lParam
    ipcact_overlay_profile_load,    //Sent by UI application when loading a profile. No data in lParam, but profile name is stored in configid_str_state_profile_name_load beforehand
    ipcact_crop_to_active_window,    //Sent by UI application to adjust crop values to the active window. No data in lParam
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
    ipcactv_ovrl_pos_adjust_increase = 0x10
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

		void PostMessageToDashboardApp(IPCMsgID IPC_id, WPARAM w_param = 0, LPARAM l_param = 0) const;
		void PostMessageToUIApp(IPCMsgID IPC_id, WPARAM w_param = 0, LPARAM l_param = 0) const;
        void SendStringToDashboardApp(ConfigID_String config_id, const std::string& str, HWND source_window) const;
        void SendStringToUIApp(ConfigID_String config_id, const std::string& str, HWND source_window) const;
};