#pragma once

#include <string>
#include <vector>

#define NOMINMAX
#include <windows.h>

struct WindowInfo
{
    HWND WindowHandle;
    HICON Icon;             //Is nullptr until requested by calling GetIcon() at least once
    std::wstring Title;
    std::wstring ClassName;
    std::string ExeName;
    std::string ListTitle;

    WindowInfo(HWND window_handle);

    bool operator==(const WindowInfo& info) { return WindowHandle == info.WindowHandle; }
    bool operator!=(const WindowInfo& info) { return !(*this == info); }

    HICON GetIcon();

    static std::vector<WindowInfo> CreateCapturableWindowList();
    static std::string GetExeName(HWND window_handle);
    static HICON GetIcon(HWND window_handle);
    static HWND FindClosestWindowForTitle(const std::string title_str, const std::string exe_str);
};