//Some misc utility functions shared between both applications

#pragma once

#include <algorithm>
#include <string>

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>

#include "Matrices.h"

//String conversion functions using win32's conversion. Return empty string on failure
std::string StringConvertFromUTF16(LPCWSTR str);
std::wstring WStringConvertFromUTF8(const char* str);
std::wstring WStringConvertFromLocalEncoding(const char* str);

//Algorithm helpers
template <typename T> T clamp(const T& value, const T& value_min, const T& value_max) 
{
    return std::max(value_min, std::min(value, value_max));
}

template <typename T> int sgn(T val)
{ 
    return (T(0) < val) - (val < T(0));
}

template <typename T_out, typename T_in> inline typename std::enable_if_t<std::is_trivially_copyable_v<T_out> && std::is_trivially_copyable_v<T_in>, T_out> pun_cast(const T_in& value)
{
    //Do type punning, but actually legal
    T_out value_out = T_out(0);
    std::memcpy(&value_out, &value, (sizeof(value_out) < sizeof(value)) ? sizeof(value_out) : sizeof(value) );
    return value_out;
}

inline float smoothstep(float step, float value_min, float value_max)
{
    return ((step) * (step) * (3 - 2 * (step))) * (value_max - value_min) + value_min;
}

inline float lin2log(float value_normalized)
{
    return value_normalized * (logf(value_normalized + 1.0f) / logf(2.0f));
}

//64-bit packed value helpers for window messages (we don't support building for 32-bit so this is fine)
#define MAKEQWORD(a, b)     ((DWORD64)(((DWORD)(((DWORD64)(a)) & 0xffffffff)) | ((DWORD64)((DWORD)(((DWORD64)(b)) & 0xffffffff))) << 32))
#define LODWORD(qw)         ((DWORD)(qw))
#define HIDWORD(qw)         ((DWORD)(((qw) >> 32) & 0xffffffff))

//Display stuff
DEVMODE GetDevmodeForDisplayID(int display_id, bool wmr_ignore_vscreens, HMONITOR* hmon = nullptr); //DEVMODE.dmSize != 0 on success
int GetDisplayIDFromHMonitor(HMONITOR monitor_handle, bool wmr_ignore_vscreens);                    //Returns -1 on failure
int GetMonitorRefreshRate(int display_id, bool wmr_ignore_vscreens);
void CenterRectToMonitor(LPRECT prc);
void CenterWindowToMonitor(HWND hwnd, bool use_cursor_pos = false);
void ForceScreenRefresh();

//Misc
bool IsProcessElevated();
bool IsProcessElevated(DWORD process_id);
bool ShellExecuteUnelevated(LPCWSTR lpFile, LPCWSTR lpParameters = nullptr, LPCWSTR lpDirectory = nullptr, LPCWSTR lpOperation = nullptr, INT nShowCmd = SW_SHOWNORMAL);
bool FileExists(LPCTSTR path);
bool DirectoryExists(LPCTSTR path);
void StopProcessByWindowClass(LPCTSTR class_name); //Used to stop the previous instance of the application
HWND FindMainWindow(DWORD pid);
unsigned int GetKeyboardModifierState();
void StringReplaceAll(std::string& source, const std::string& from, const std::string& to);
void WStringReplaceAll(std::wstring& source, const std::wstring& from, const std::wstring& to);
bool IsWCharInvalidForFileName(wchar_t wchar);
void SanitizeFileNameWString(std::wstring& str);
bool WStringCompareNatural(std::wstring& str1, std::wstring& str2);

//Virtual Keycode string mapping
const char* GetStringForKeyCode(unsigned char keycode);
unsigned char GetKeyCodeForListID(unsigned char list_id);  //Used for a more natural sort when doing direct listing