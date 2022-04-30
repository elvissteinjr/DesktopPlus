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

//Returns false if the device has no valid pose
bool GetOverlayIntersectionParamsForDevice(vr::VROverlayIntersectionParams_t& params, vr::TrackedDeviceIndex_t device_index, vr::ETrackingUniverseOrigin tracking_origin, bool use_tip_offset = true);
//Returns if intersection happened
bool ComputeOverlayIntersectionForDevice(vr::VROverlayHandle_t overlay_handle, vr::TrackedDeviceIndex_t device_index, vr::ETrackingUniverseOrigin tracking_origin, vr::VROverlayIntersectionResults_t* results, 
                                         bool use_tip_offset = true);

//Returns transform similar to the dashboard transform (not a perfect match, though)
Matrix4 ComputeHMDFacingTransform(float distance);

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
bool IsWCharInvalidForFileName(wchar_t wchar);
void SanitizeFileNameWString(std::wstring& str);

//Virtual Keycode string mapping
const char* GetStringForKeyCode(unsigned char keycode);
unsigned char GetKeyCodeForListID(unsigned char list_id);  //Used for a more natural sort when doing direct listing