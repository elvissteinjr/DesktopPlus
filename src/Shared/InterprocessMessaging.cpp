#include "InterprocessMessaging.h"

static IPCManager g_IPCManager;

IPCManager::IPCManager()
{
	//Register messages
	m_RegisteredMessages[ipcmsg_action]          = ::RegisterWindowMessage(L"WMIPC_DPLUS_Action");
	m_RegisteredMessages[ipcmsg_set_config]      = ::RegisterWindowMessage(L"WMIPC_DPLUS_SetConfig");
	m_RegisteredMessages[ipcmsg_elevated_action] = ::RegisterWindowMessage(L"WMIPC_DPLUS_ElevatedAction");
}

IPCManager & IPCManager::Get()
{
    return g_IPCManager;
}

void IPCManager::DisableUIPForRegisteredMessages(HWND window_handle) const
{
    ::ChangeWindowMessageFilterEx(window_handle, WM_QUIT,     MSGFLT_ALLOW, nullptr);
    ::ChangeWindowMessageFilterEx(window_handle, WM_COPYDATA, MSGFLT_ALLOW, nullptr); //This seems to be not the quite the safest thing to allow to be fair

	for (UINT msgid : m_RegisteredMessages)
	{
        if (msgid != 0)
        {
            ::ChangeWindowMessageFilterEx(window_handle, msgid, MSGFLT_ALLOW, nullptr);
        }
	}
    //We could return the success state of this, but the resulting behavior wouldn't change. Should usually not fail anyways.
}

UINT IPCManager::GetWin32MessageID(IPCMsgID IPC_id) const
{
	return m_RegisteredMessages[IPC_id];
}

IPCMsgID IPCManager::GetIPCMessageID(UINT win32_id) const
{
    //Primitive lookup, but should be good enough for the low amount of message IDs
    for (size_t i = 0; i < ipcmsg_MAX; ++i)
    {
        if (m_RegisteredMessages[i] == win32_id)
            return (IPCMsgID)i;
    }

    return ipcmsg_MAX;
}

bool IPCManager::GetAllMesssagesRegistered() const
{
	for (UINT msgid : m_RegisteredMessages)
	{
		if (msgid == 0)
			return false;
	}

	return true;
}

bool IPCManager::IsDashboardAppRunning()
{
    return (::FindWindow(g_WindowClassNameDashboardApp, nullptr) != 0);
}

bool IPCManager::IsUIAppRunning()
{
    return (::FindWindow(g_WindowClassNameUIApp, nullptr) != 0);
}

bool IPCManager::IsElevatedModeProcessRunning()
{
    return (::FindWindow(g_WindowClassNameElevatedMode, nullptr) != 0);
}

void IPCManager::PostMessageToDashboardApp(IPCMsgID IPC_id, WPARAM w_param, LPARAM l_param) const
{
	//We take the cost of finding the window for every message (which isn't frequent anyways) so we don't have to worry about the process going anywhere
	if (HWND window = ::FindWindow(g_WindowClassNameDashboardApp, nullptr))
	{
		::PostMessage(window, GetWin32MessageID(IPC_id), w_param, l_param);
	}
}

void IPCManager::PostMessageToUIApp(IPCMsgID IPC_id, WPARAM w_param, LPARAM l_param) const
{
	if (HWND window = ::FindWindow(g_WindowClassNameUIApp, nullptr))
	{
		::PostMessage(window, GetWin32MessageID(IPC_id), w_param, l_param);
	}
}

void IPCManager::PostMessageToElevatedModeProcess(IPCMsgID IPC_id, WPARAM w_param, LPARAM l_param) const
{
    if (HWND window = ::FindWindow(g_WindowClassNameElevatedMode, nullptr))
    {
        ::PostMessage(window, GetWin32MessageID(IPC_id), w_param, l_param);
    }
}

void IPCManager::SendStringToDashboardApp(ConfigID_String config_id, const std::string& str, HWND source_window) const
{
    if (HWND window = ::FindWindow(g_WindowClassNameDashboardApp, nullptr))
	{
        COPYDATASTRUCT cds;
        cds.dwData = config_id;
        cds.cbData = (DWORD)str.length();  //We do not include the NUL byte
        cds.lpData = (void*)str.c_str();
        ::SendMessage(window, WM_COPYDATA, (WPARAM)source_window, (LPARAM)(LPVOID)&cds);
	}
}

void IPCManager::SendStringToUIApp(ConfigID_String config_id, const std::string& str, HWND source_window) const
{
    if (HWND window = ::FindWindow(g_WindowClassNameUIApp, nullptr))
	{
        COPYDATASTRUCT cds;
        cds.dwData = config_id;
        cds.cbData = (DWORD)str.length();  //We do not include the NUL byte
        cds.lpData = (void*)str.c_str();
        ::SendMessage(window, WM_COPYDATA, (WPARAM)source_window, (LPARAM)(LPVOID)&cds);
	}
}

void IPCManager::SendStringToElevatedModeProcess(IPCElevatedStringID elevated_str_id, const std::string& str, HWND source_window) const
{
    if (HWND window = ::FindWindow(g_WindowClassNameElevatedMode, nullptr))
    {
        COPYDATASTRUCT cds;
        cds.dwData = elevated_str_id;
        cds.cbData = (DWORD)str.length();  //We do not include the NUL byte
        cds.lpData = (void*)str.c_str();
        ::SendMessage(window, WM_COPYDATA, (WPARAM)source_window, (LPARAM)(LPVOID)&cds);
    }
}
