#pragma once

#define NOMINMAX
#include <windows.h>

#include <mutex>

#include "OverlayManager.h"

struct WindowManagerThreadData
{
    bool BlockDrag = false;
    bool DoOverlayDrag = false;
    bool KeepOnScreen = false;
    HWND TargetWindow = nullptr;
    unsigned int TargetOverlayID = k_ulOverlayID_Dashboard;

    bool operator==(const WindowManagerThreadData b)
    {
        return ( (BlockDrag == b.BlockDrag) &&
                 (DoOverlayDrag == b.DoOverlayDrag) &&
                 (KeepOnScreen == b.KeepOnScreen) &&
                 (TargetWindow == b.TargetWindow) &&
                 (TargetOverlayID == b.TargetOverlayID) );
    }
    bool operator!=(const WindowManagerThreadData b)
    {
        return !(*this == b);
    }
};

//WindowManager uses a separate thread for win event hook callbacks in order to be able to react as soon as possible (needed for window drag blocking)
class WindowManager
{
    public:
        static WindowManager& Get();

        //- Only called by main thread
        void UpdateConfigState();                                                              //Updates config state from ConfigManager for the WindowManager thread
        void SetTargetWindow(HWND window, unsigned int overlay_id = k_ulOverlayID_Dashboard);  //Sets target window and overlay id for the WindowManager thread
        HWND GetTargetWindow() const;
        void SetActive(bool is_active);                                                        //Set active state for the window manager. Threads are destroyed when it's inactive

        bool WouldDragMaximizedTitleBar(HWND window, int prev_cursor_x, int prev_cursor_y, int new_cursor_x, int new_cursor_y);
        void RaiseAndFocusWindow(HWND window);
        static void MoveWindowIntoWorkArea(HWND window);
        static void FocusActiveVRSceneApp();

    private:
        //- Only accessed in main thread
        HANDLE m_ThreadHandle = nullptr;
        DWORD m_ThreadID = 0;
        HWND m_TargetWindow = nullptr;
        unsigned int m_TargetOverlayID = k_ulOverlayID_Dashboard;

        bool m_IsActive = false;

        HWND m_LastFocusFailedWindow = nullptr;
        ULONGLONG m_LastFocusFailedTick = 0;

        //- Protected by m_ThreadMutex
        std::mutex m_ThreadMutex;
        WindowManagerThreadData m_ThreadData;

        //- Synchronization variables for UpdateConfigState(). m_TargetWindow is not something we can afford to have updated slightly delayed, so that function blocks
        std::condition_variable m_UpdateDoneCV;
        std::mutex m_UpdateDoneMutex;
        bool m_UpdateDoneFlag;

        //- Only accessed in WindowManager thread
        WindowManagerThreadData m_ThreadLocalData;
        POINT m_DragStartMousePos {0, 0};
        RECT m_DragStartWindowRect {0, 0, 0, 0};
        HWND m_DragWindow = nullptr;
        bool m_DragOverlayMsgSent = false;
        DWORD m_FocusLastProcess = 0;

        //- Only called by WindowManager thread
        void HandleWinEvent(DWORD win_event, HWND hwnd, LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time);
        static void CALLBACK WinEventProc(HWINEVENTHOOK event_hook_handle, DWORD win_event, HWND hwnd, LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time);
        void ManageEventHooks(HWINEVENTHOOK& hook_handle_move_size, HWINEVENTHOOK& hook_handle_location_change, HWINEVENTHOOK& hook_handle_focus_change);

        static DWORD WindowManagerThreadEntry(void* param);
};