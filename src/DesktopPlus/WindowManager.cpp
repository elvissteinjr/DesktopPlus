#include "WindowManager.h"

#include <algorithm>
#include <iostream>

#include <dwmapi.h>

#include "ConfigManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"

WindowManager g_WindowManager;

#define WM_WINDOWMANAGER_UPDATE_DATA WM_APP //Sent to WindowManager thread to update the local thread data

WindowManager& WindowManager::Get()
{
    return g_WindowManager;
}

void WindowManager::UpdateConfigState()
{
	WindowManagerThreadData thread_data_new;
	thread_data_new.BlockDrag       = (ConfigManager::Get().GetConfigInt(configid_int_windows_winrt_dragging_mode) != window_dragging_none);
	thread_data_new.DoOverlayDrag   = (ConfigManager::Get().GetConfigInt(configid_int_windows_winrt_dragging_mode) == window_dragging_overlay);
	thread_data_new.KeepOnScreen    = ConfigManager::Get().GetConfigBool(configid_bool_windows_winrt_keep_on_screen);
	thread_data_new.TargetWindow    = m_TargetWindow;
	thread_data_new.TargetOverlayID = m_TargetOverlayID;

    if (m_IsActive)
    {
		//Create WindowManager thread if there is none
        if (m_ThreadHandle == nullptr)
        {
			m_ThreadData = thread_data_new; //No need to lock since no other thread exists to race with
			m_ThreadHandle = ::CreateThread(nullptr, 0, WindowManagerThreadEntry, nullptr, 0, &m_ThreadID);
        }
		else if (m_ThreadData != thread_data_new) //If just data has changed, update existing thread
		{
			{
				std::lock_guard<std::mutex> lock(m_ThreadMutex);
				m_ThreadData = thread_data_new;
			}

			//Reset done flag before sending update
			{
				std::lock_guard<std::mutex> lock(m_UpdateDoneMutex);
				m_UpdateDoneFlag = false;
			}

			::PostThreadMessage(m_ThreadID, WM_WINDOWMANAGER_UPDATE_DATA, 0, 0);

			//Wait for thread to be done with update before continuing (for 500ms in case the window message goes poof... shouldn't happen though)
			{
				std::unique_lock<std::mutex> lock(m_UpdateDoneMutex);
				m_UpdateDoneCV.wait_for(lock, std::chrono::milliseconds(500), [&]{ return m_UpdateDoneFlag; });
			}
		}
    }
	else //If thread is no longer needed, remove it
	{
		if (m_ThreadHandle != nullptr)
		{
			::PostThreadMessage(m_ThreadID, WM_QUIT, 0, 0);

			::CloseHandle(m_ThreadHandle);
			m_ThreadHandle = nullptr;
			m_ThreadID	   = 0;
		}
	}
}

void WindowManager::SetTargetWindow(HWND window, unsigned int overlay_id)
{
	if (window != m_TargetWindow)
		m_DragOverlayMsgSent = false;

	m_TargetWindow    = window;
	m_TargetOverlayID = overlay_id;
	UpdateConfigState();
}

HWND WindowManager::GetTargetWindow() const
{
	return m_TargetWindow;
}

void WindowManager::SetActive(bool is_active)
{
	m_IsActive = is_active;
	UpdateConfigState();
}

bool WindowManager::WouldDragMaximizedTitleBar(HWND window, int prev_cursor_x, int prev_cursor_y, int new_cursor_x, int new_cursor_y)
{
	//If the target window matches (simulated left mouse down), the window is maximized and the cursor position changed
	if ( (window == m_TargetWindow) && (::IsZoomed(window)) && ( (prev_cursor_x != new_cursor_x) || (prev_cursor_y != new_cursor_y) ) )
	{
		//Return true if previous cursor position was on the title bar
		return (::SendMessage(window, WM_NCHITTEST, 0, MAKELPARAM(prev_cursor_x, prev_cursor_y)) == HTCAPTION);
	}

	return false;
}

void WindowManager::RaiseAndFocusWindow(HWND window)
{
	bool focus_success = true;
	if ( ( (m_LastFocusFailedWindow != window) || (m_LastFocusFailedTick + 3000 >= ::GetTickCount64()) ) && (::GetForegroundWindow() != window) )
	{
		if (::IsIconic(window))
		{
			focus_success = ::OpenIcon(window); //Also focuses
		}
		else
		{
			focus_success = ::SetForegroundWindow(window);
		}

		if (focus_success)
		{
			m_LastFocusFailedWindow = nullptr;
		}
	}

	if (!focus_success)
	{
		//If a focusing attempt failed we either wait until it succeeded on another window or 3 seconds in order to not spam attempts when it's blocked for some reason
		m_LastFocusFailedWindow = window;
		m_LastFocusFailedTick = ::GetTickCount64();
	}
}

void WindowManager::MoveWindowIntoWorkArea(HWND window)
{
	if ( (::IsIconic(window)) || ((::IsZoomed(window))) )
		return;

	//Get position of the window and DWM frame bounds
	int offset_x = 0, offset_y = 0;
	RECT window_rect, dwm_rect;
	::GetWindowRect(window, &window_rect);
	
	if (::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &dwm_rect, sizeof(dwm_rect)) == S_OK)
	{
		//We take the frame bounds as well since the window rect contains the window shadows, but we don't want those to to take up any space in the calculations
		offset_x = window_rect.left - dwm_rect.left;
		offset_y = window_rect.top  - dwm_rect.top;

		//Replace the window rect with the DWM one. We apply the stored offset later when setting the window position
		window_rect = dwm_rect;	
	}

	//Get the nearest monitor to the window
	HMONITOR hmon = ::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);

	//Get monitor info to access the work area rect
	MONITORINFO monitor_info = {0};
	monitor_info.cbSize = sizeof(monitor_info);
	::GetMonitorInfo(hmon, &monitor_info);

	int width  = window_rect.right  - window_rect.left;
	int height = window_rect.bottom - window_rect.top;

	//Clamp position to the work area. This does move windows even if they'd be fully accessible across multiple screens, but whatever
	window_rect.left   = std::max(monitor_info.rcWork.left, std::min(monitor_info.rcWork.right  - width,  window_rect.left));
	window_rect.top    = std::max(monitor_info.rcWork.top,  std::min(monitor_info.rcWork.bottom - height, window_rect.top));
	window_rect.right  = window_rect.left + width;
	window_rect.bottom = window_rect.top  + height;

	::SetWindowPos(window, nullptr,  window_rect.left + offset_x, window_rect.top + offset_y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

void WindowManager::FocusActiveVRSceneApp()
{
	//Try finding and focusing the window of the current scene application
	uint32_t pid = vr::VRApplications()->GetCurrentSceneProcessId();

	if (pid != 0)
	{
		HWND scene_process_window = FindMainWindow(pid);

		if (scene_process_window != nullptr)
		{
			::SetForegroundWindow(scene_process_window);
		}
	}
}

void WindowManager::HandleWinEvent(DWORD win_event, HWND hwnd, LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time)
{
	if (win_event == EVENT_OBJECT_FOCUS)
	{
		//Check if focus is from a different process than last time
		DWORD process_id;
		::GetWindowThreadProcessId(hwnd, &process_id);
		if (process_id != m_FocusLastProcess)
		{
			m_FocusLastProcess = process_id;

			//Send updated process elevation state to UI
			IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_bool_state_window_focused_process_elevated), IsProcessElevated(process_id));
		}

		return;
	}

	//Limit the rest to visible top-level window events
    if ( (hwnd == nullptr) || (id_object != OBJID_WINDOW) || (id_child != CHILDID_SELF) || (GetWindowTextLength(hwnd) == 0) || 
		 (!::IsWindowVisible(hwnd)) || (::GetAncestor(hwnd, GA_ROOT) != hwnd) )
	{
        return;
	}

	if ( (m_ThreadLocalData.BlockDrag) || (m_ThreadLocalData.KeepOnScreen) )
	{
		if (win_event == EVENT_SYSTEM_MOVESIZESTART)
		{
			if (hwnd == m_ThreadLocalData.TargetWindow)
			{
				m_DragWindow = hwnd;
				m_DragOverlayMsgSent = false;
			}
			return;
		}
		else if (win_event == EVENT_OBJECT_LOCATIONCHANGE)
		{
			if (hwnd == m_DragWindow)
			{
				if (m_ThreadLocalData.BlockDrag)
				{
					RECT window_rect;
					::GetWindowRect(hwnd, &window_rect);
					SIZE start_size = {m_DragStartWindowRect.right - m_DragStartWindowRect.left, m_DragStartWindowRect.bottom - m_DragStartWindowRect.top};
					SIZE current_size = {window_rect.right - window_rect.left,window_rect.bottom - window_rect.top};

					//Only block/start overlay drag if the position changed, but not the size (allows in-place resizing)
					if ( ((window_rect.left != m_DragStartWindowRect.left) || (window_rect.top != m_DragStartWindowRect.top)) && (start_size.cx == current_size.cx) && 
						 (start_size.cy == current_size.cy) )
					{
						::SetCursorPos(m_DragStartMousePos.x, m_DragStartMousePos.y);
						::SetWindowPos(hwnd, nullptr, m_DragStartWindowRect.left, m_DragStartWindowRect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

						if (!m_DragOverlayMsgSent)
						{
							//Start the overlay drag or send overlay ID 0 to have the mouse button be released (so the desktop drag stops)
							IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_winmanager_drag_start, 
																		(m_ThreadLocalData.DoOverlayDrag) ? m_ThreadLocalData.TargetOverlayID : 0);
							m_DragOverlayMsgSent = true;
						}
					}
					else if ((start_size.cx != current_size.cx) || (start_size.cy != current_size.cy))
					{
						//Adapt to size change in case this happened from a normal drag (when dragging from a maximized window for example)
						::GetWindowRect(hwnd, &m_DragStartWindowRect);
					}
				}
				else if (m_ThreadLocalData.KeepOnScreen)
				{
					MoveWindowIntoWorkArea(hwnd);
				}
			}
			else if (hwnd == m_ThreadLocalData.TargetWindow)
			{
				//Fallback for windows that do their own dragging logic.
				//This has the chance of picking up a programmatic position change, 
				//but the time window for that is very small since the target window is only set while the simulated mouse is down

				//Check if the current cursor is a sizing cursor, though. There are edge cases we want to avoid if that's the case
				bool is_sizing_cursor = false;
				CURSORINFO cinfo;
				cinfo.cbSize = sizeof(CURSORINFO);

				if (::GetCursorInfo(&cinfo))
				{
					is_sizing_cursor = ( (cinfo.hCursor == ::LoadCursor(nullptr, IDC_SIZENESW)) ||
										 (cinfo.hCursor == ::LoadCursor(nullptr, IDC_SIZENS))   ||
										 (cinfo.hCursor == ::LoadCursor(nullptr, IDC_SIZENWSE)) ||
										 (cinfo.hCursor == ::LoadCursor(nullptr, IDC_SIZEWE)) );
				}


				if (!is_sizing_cursor)
				{
					m_DragWindow = hwnd;
				}

				m_DragOverlayMsgSent = false;
			}
		}
		else if (win_event == EVENT_SYSTEM_MOVESIZEEND)
		{
			if (m_DragWindow != nullptr)
			{
				::SetCursorPos(m_DragStartMousePos.x, m_DragStartMousePos.y);
				::SetWindowPos(hwnd, nullptr, m_DragStartWindowRect.left, m_DragStartWindowRect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

				if (m_ThreadLocalData.KeepOnScreen)
				{
					MoveWindowIntoWorkArea(hwnd);
				}
			}

			m_DragWindow = nullptr;
		}
	}

}

void WindowManager::WindowManager::WinEventProc(HWINEVENTHOOK /*event_hook_handle*/, DWORD win_event, HWND hwnd, LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time)
{
    Get().HandleWinEvent(win_event, hwnd, id_object, id_child, event_thread, event_time);
}

void WindowManager::ManageEventHooks(HWINEVENTHOOK& hook_handle_move_size, HWINEVENTHOOK& hook_handle_location_change, HWINEVENTHOOK& hook_handle_focus_change)
{
	if ( (m_ThreadLocalData.BlockDrag) && (hook_handle_move_size == nullptr) )
	{
		hook_handle_move_size       = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND, nullptr, WindowManager::WinEventProc, 0, 0, 
													  WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
		hook_handle_location_change = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, WindowManager::WinEventProc, 0, 0,
													  WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
	}
	else if ( (!m_ThreadLocalData.BlockDrag) && (hook_handle_move_size != nullptr) )
	{
		UnhookWinEvent(hook_handle_move_size);
		UnhookWinEvent(hook_handle_location_change);

		hook_handle_move_size		= nullptr;
		hook_handle_location_change = nullptr;
	}

	if (hook_handle_focus_change == nullptr)
	{
		hook_handle_focus_change = SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, nullptr, WindowManager::WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
	}

	//Reset drag window state when target window is nullptr
	if (m_ThreadLocalData.TargetWindow == nullptr)
	{
		m_DragWindow = nullptr;
		m_DragOverlayMsgSent = false;
	}
	else //Store drag start mouse position and window rect now, since doing on actual drag start will be too late
	{
		::GetCursorPos(&m_DragStartMousePos);
		::GetWindowRect(m_ThreadLocalData.TargetWindow, &m_DragStartWindowRect);
	}
}

DWORD WindowManager::WindowManagerThreadEntry(void* /*param*/)
{
	//Copy thread data for lock-free reads later
	{
		WindowManager& wman = Get();
		std::lock_guard<std::mutex> lock(wman.m_ThreadMutex);

		wman.m_ThreadLocalData = wman.m_ThreadData;
	}

	//Create event hooks
	HWINEVENTHOOK hook_handle_move_size		  = nullptr;
	HWINEVENTHOOK hook_handle_location_change = nullptr;
	HWINEVENTHOOK hook_handle_focus_change	  = nullptr;
	
	Get().ManageEventHooks(hook_handle_move_size, hook_handle_location_change, hook_handle_focus_change);

	//Wait for callbacks, update or quit message
	MSG msg;
	while (::GetMessage(&msg, 0, 0, 0))
	{
		if (msg.message == WM_WINDOWMANAGER_UPDATE_DATA)
		{
			WindowManager& wman = Get();

			//Copy new thread data
			{
				std::lock_guard<std::mutex> lock(wman.m_ThreadMutex);

				if (wman.m_ThreadData.TargetWindow == nullptr)
				{
					//Give a potentially dragged window's process a little bit of time to realize the mouse release
					::Sleep(20);
				}

				//Process all pending messages/callbacks before continuing
				while (::PeekMessage(&msg, 0, 0, WM_APP, PM_REMOVE));

				wman.m_ThreadLocalData = wman.m_ThreadData;
			}

			wman.ManageEventHooks(hook_handle_move_size, hook_handle_location_change, hook_handle_focus_change);

			//Notify main thread we're done
			{
				std::lock_guard<std::mutex> lock(wman.m_UpdateDoneMutex);
				wman.m_UpdateDoneFlag = true;
			}
			wman.m_UpdateDoneCV.notify_one();
		}
	}

	UnhookWinEvent(hook_handle_move_size);
	UnhookWinEvent(hook_handle_location_change);
	UnhookWinEvent(hook_handle_focus_change);

	return 0;
}
