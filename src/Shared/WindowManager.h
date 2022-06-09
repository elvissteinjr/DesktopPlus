#pragma once

#define NOMINMAX
#include <windows.h>

#include <vector>
#include <mutex>

class WindowInfo
{
    private:
        HWND m_WindowHandle;
        mutable HICON m_Icon;              //Is nullptr until requested by calling GetIcon() for the first time
        std::wstring m_Title;
        std::wstring m_ClassName;
        mutable std::string m_ExeName;     //Is empty until requested by calling GetExeName() for the first time
        mutable std::string m_ListTitle;   //Is empty until requested by calling GetListTitle() for the first time

    public: 
        WindowInfo(HWND window_handle);
        bool operator==(const WindowInfo& info) { return (m_WindowHandle == info.m_WindowHandle); }
        bool operator!=(const WindowInfo& info) { return !(*this == info); }

        HWND GetWindowHandle() const;
        HICON GetIcon() const;
        const std::wstring& GetTitle() const;
        const std::wstring& GetWindowClassName() const;
        const std::string& GetExeName() const;
        const std::string& GetListTitle() const;

        bool UpdateWindowTitle();          //Returns if the title has changed

        static std::string GetExeName(HWND window_handle);
        static HICON GetIcon(HWND window_handle);
        static HWND FindClosestWindowForTitle(const std::string& title_str, const std::string& class_str, const std::string& exe_str, const std::vector<WindowInfo>& window_list);
};

struct WindowManagerThreadData
{
    bool BlockDrag = false;
    bool DoOverlayDrag = false;
    bool KeepOnScreen = false;
    HWND TargetWindow = nullptr;
    unsigned int TargetOverlayID = UINT_MAX;

    bool operator==(const WindowManagerThreadData b)
    {
        return ( (BlockDrag       == b.BlockDrag)     &&
                 (DoOverlayDrag   == b.DoOverlayDrag) &&
                 (KeepOnScreen    == b.KeepOnScreen)  &&
                 (TargetWindow    == b.TargetWindow)  &&
                 (TargetOverlayID == b.TargetOverlayID) );
    }
    bool operator!=(const WindowManagerThreadData b)
    {
        return !(*this == b);
    }
};

class InputSimulator;

//WindowManager uses a separate thread for win event hook callbacks in order to be able to react as soon as possible (needed for window drag blocking)
//For the UI process it only really tracks the window list
class WindowManager
{
    public:
        static WindowManager& Get();

        //- Only called by main thread
        void UpdateConfigState();                                                                //Updates config state from ConfigManager for the WindowManager thread
        void SetTargetWindow(HWND window, unsigned int overlay_id = UINT_MAX);                   //Sets target window and overlay id for the WindowManager thread
        HWND GetTargetWindow() const;
        void SetActive(bool is_active);                                                          //Set active state for the window manager. Threads are destroyed when it's inactive
        bool IsActive() const;
        void SetOverlayActive(bool is_active);                                                   //Set overlay active state. Overlay interactive config states are forced to false when inactive
        bool IsOverlayActive() const;

        const WindowInfo& WindowListAdd(HWND window);                                            //Returns reference to new or already existing window
        std::wstring WindowListRemove(HWND window);                                              //Returns title of removed window or blank wstring
        WindowInfo const* WindowListUpdateTitle(HWND window, bool* has_title_changed = nullptr); //Returns pointer to updated window (may be nullptr)
        const std::vector<WindowInfo>& WindowListGet() const;
        WindowInfo const* WindowListFindWindow(HWND window) const;

        bool WouldDragMaximizedTitleBar(HWND window, int prev_cursor_x, int prev_cursor_y, int new_cursor_x, int new_cursor_y);
        bool IsHoveringCapturableTitleBar(HWND window, int cursor_x, int cursor_y);
        void RaiseAndFocusWindow(HWND window, InputSimulator* input_sim_ptr = nullptr);          //input_sim_ptr is optional but passing it increases chance of success
        bool SetTempTopMostWindow(HWND window);
        bool ClearTempTopMostWindow();
        void FocusActiveVRSceneApp(InputSimulator* input_sim_ptr = nullptr);
        static void MoveWindowIntoWorkArea(HWND window);

    private:
        //- Only accessed in main thread
        HANDLE m_ThreadHandle = nullptr;
        DWORD m_ThreadID = 0;
        HWND m_TargetWindow = nullptr;
        unsigned int m_TargetOverlayID = UINT_MAX;

        bool m_IsActive = false;
        bool m_IsOverlayActive = false;

        HWND m_LastFocusFailedWindow = nullptr;
        ULONGLONG m_LastFocusFailedTick = 0;
        HWND m_TempTopMostWindow = nullptr;

        std::vector<WindowInfo> m_WindowList;

        //- Protected by m_ThreadMutex
        std::mutex m_ThreadMutex;
        WindowManagerThreadData m_ThreadData;

        //- Synchronization variables for UpdateConfigState(). m_TargetWindow is not something we can afford to have updated slightly delayed, so that function blocks
        std::condition_variable m_UpdateDoneCV;
        std::mutex m_UpdateDoneMutex;
        bool m_UpdateDoneFlag = false;

        //- Only accessed in WindowManager thread
        WindowManagerThreadData m_ThreadLocalData;
        POINT m_DragStartMousePos {0, 0};
        RECT m_DragStartWindowRect {0, 0, 0, 0};
        HWND m_DragWindow = nullptr;
        bool m_DragOverlayMsgSent = false;
        DWORD m_FocusLastProcess = 0;

        //- Only called by main thread
        void WindowListInit();

        //- Only called by WindowManager thread
        void HandleWinEvent(DWORD win_event, HWND hwnd, LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time);
        static void CALLBACK WinEventProc(HWINEVENTHOOK event_hook_handle, DWORD win_event, HWND hwnd, LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time);
        void ManageEventHooks(HWINEVENTHOOK& hook_handle_move_size, HWINEVENTHOOK& hook_handle_location_change, HWINEVENTHOOK& hook_handle_foreground, HWINEVENTHOOK& hook_handle_destroy_show);

        static DWORD WindowManagerThreadEntry(void* param);
};