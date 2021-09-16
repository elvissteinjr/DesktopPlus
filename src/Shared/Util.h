//Some misc utility functions shared between both applications

#pragma once

#include <algorithm>
#include <string>
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>

#include "Matrices.h"

//String conversion functions using win32's conversion. Return empty string on failure
std::string StringConvertFromUTF16(LPCWSTR str);
std::wstring WStringConvertFromUTF8(const char* str);
std::wstring WStringConvertFromLocalEncoding(const char* str);

//VR helpers
void TransformOpenVR34TranslateRelative(vr::HmdMatrix34_t& matrix, float offset_right, float offset_up, float offset_forward);
void TransformLookAt(Matrix4& matrix, const Vector3 pos_target, const Vector3 up = {0.0f, 1.0f, 0.0f});

bool ComputeOverlayIntersectionForDevice(vr::VROverlayHandle_t overlay_handle, vr::TrackedDeviceIndex_t device_index, vr::ETrackingUniverseOrigin tracking_origin, vr::VROverlayIntersectionResults_t* results, 
                                         bool use_tip_offset = true);

vr::TrackedDeviceIndex_t FindPointerDeviceForOverlay(vr::VROverlayHandle_t overlay_handle);
vr::TrackedDeviceIndex_t GetFirstVRTracker();
Matrix4 GetControllerTipMatrix(bool right_hand = true);
float GetTimeNowToPhotons();
void SetConfigForWMR(int& wmr_ignore_vscreens);
vr::EVROverlayError SetSharedOverlayTexture(vr::VROverlayHandle_t ovrl_handle_source, vr::VROverlayHandle_t ovrl_handle_target, ID3D11Resource* device_texture_ref);

//Algorithm helpers
template <typename T> T clamp(const T& value, const T& value_min, const T& value_max) 
{
    return std::max(value_min, std::min(value, value_max));
}

template <typename T> int sgn(T val)
{ 
    return (T(0) < val) - (val < T(0));
}

inline float smoothstep(float step, float value_min, float value_max)
{
    return ((step) * (step) * (3 - 2 * (step))) * (value_max - value_min) + value_min;
}

//Display stuff
DEVMODE GetDevmodeForDisplayID(int display_id, HMONITOR* hmon = nullptr); //DEVMODE.dmSize != 0 on success
int GetMonitorRefreshRate(int display_id);
void CenterRectToMonitor(LPRECT prc);
void CenterWindowToMonitor(HWND hwnd, bool use_cursor_pos = false);
void ForceScreenRefresh();

//Misc
bool IsProcessElevated();
bool IsProcessElevated(DWORD process_id);
bool FileExists(LPCTSTR path);
bool DirectoryExists(LPCTSTR path);
void StopProcessByWindowClass(LPCTSTR class_name); //Used to stop the previous instance of the application
HWND FindMainWindow(DWORD pid);
unsigned int GetKeyboardModifierState();
void StringReplaceAll(std::string& source, const std::string& from, const std::string& to);
void WStringReplaceAll(std::wstring& source, const std::wstring& from, const std::wstring& to);

//Virtual Keycode string mapping
const char* GetStringForKeyCode(unsigned char keycode);
unsigned char GetKeyCodeForListID(unsigned char list_id);  //Used for a more natural sort when doing direct listing