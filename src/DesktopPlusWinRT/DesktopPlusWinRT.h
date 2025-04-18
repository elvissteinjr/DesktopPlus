#pragma once

//This is the part of Desktop+ using Windows Runtime functions, separated from rest of the codebase as a DLL
//Windows SDK 10.0.19041.0 or newer is required to build this, 10.0.20348.0 or newer recommended to allow removing the capture border
//Windows SDK 10.0.26100.0 or newer is required for Windows 11 24H2 features
//
//If you wish to build Desktop+ without support for the functionality provided by this library, define DPLUSWINRT_STUB for the project,
//remove the package references and adjust the project's Windows SDK version if needed

//Based on the Win32CaptureSample by Robert Mikhayelyan: https://github.com/robmikh/Win32CaptureSample

//For Graphics Capture, all overlay texture handling is handed off to this library. Everything else is still handled by OutputManager as usual, however.

//As a general rule, the callee of the library functions is responsible to check for support first, otherwise it may throw or crash
//In release builds, capture thread exceptions are caught and handled as unexpected errors, trying to just stop the thread. Ideally it never comes to that, of course.
//The library relies on delay loading CoreMessaging and D3D11 in order to support running on Windows 8, so keep that in mind if ever using a different compiler.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "openvr.h"

#ifdef DPLUSWINRT_EXPORTS
    #define DPLUSWINRT_API __declspec(dllexport)
#else
    #define DPLUSWINRT_API __declspec(dllimport)
#endif

//Thread message IDs
#define WM_DPLUSWINRT               WM_APP           //Base ID, to allow changing it easily later
#define WM_DPLUSWINRT_SIZE          WM_DPLUSWINRT    //Sent to main thread on size change. wParam = overlay handle, lParam = width & height (in low/high word order, signed)
#define WM_DPLUSWINRT_UPDATE_DATA   WM_DPLUSWINRT+1  //Sent to capture thread to update its local data
#define WM_DPLUSWINRT_CAPTURE_PAUSE WM_DPLUSWINRT+2  //Sent to capture thread to pause/resume capture. wParam = overlay handle, lParam = pause bool
#define WM_DPLUSWINRT_CAPTURE_LOST  WM_DPLUSWINRT+3  //Sent to main thread when capture item was closed, should call StopCapture() in response. wParam = overlay handle
#define WM_DPLUSWINRT_ENABLE_CURSOR WM_DPLUSWINRT+4  //Sent to capture thread to change cursor enabled state, wParam = cursor enabled bool
#define WM_DPLUSWINRT_ENABLE_HDR    WM_DPLUSWINRT+5  //Sent to capture thread to change HDR enabled state, wParam = HDR enabled bool
#define WM_DPLUSWINRT_THREAD_QUIT   WM_DPLUSWINRT+6  //Sent to capture thread to quit when no overlays are left to capture
#define WM_DPLUSWINRT_THREAD_ERROR  WM_DPLUSWINRT+7  //Sent to main thread when an unexpected error occured in the capture thread. wParam = thread ID, lParam = hresult
#define WM_DPLUSWINRT_THREAD_ACK    WM_DPLUSWINRT+8  //Sent to main thread to acknowledge thread messages from StopCapture() (main thread is blocked until this is received)
#define WM_DPLUSWINRT_FPS           WM_DPLUSWINRT+9  //Sent to main thread when fps count has changed. wParam = overlay handle, lParam = frames per second

#ifdef __cplusplus
extern "C" {
#endif

DPLUSWINRT_API void DPWinRT_Init();

DPLUSWINRT_API bool DPWinRT_IsCaptureSupported();                         //Build 1903 (1803 in theory, but picker is no longer supported by this library)
DPLUSWINRT_API bool DPWinRT_IsCaptureFromHandleSupported();               //Build 1903
DPLUSWINRT_API bool DPWinRT_IsCaptureFromCombinedDesktopSupported();      //Build 2004
DPLUSWINRT_API bool DPWinRT_IsCaptureCursorEnabledPropertySupported();    //Build 2004
DPLUSWINRT_API bool DPWinRT_IsBorderRequiredPropertySupported();          //Windows 11
DPLUSWINRT_API bool DPWinRT_IsIncludeSecondaryWindowsPropertySupported(); //Windows 11 24H2
DPLUSWINRT_API bool DPWinRT_IsMinUpdateIntervalPropertySupported();       //Windows 11 24H2

DPLUSWINRT_API bool DPWinRT_StartCaptureFromHWND(vr::VROverlayHandle_t overlay_handle, HWND handle);
DPLUSWINRT_API bool DPWinRT_StartCaptureFromDesktop(vr::VROverlayHandle_t overlay_handle, int desktop_id); //-1 is combined desktop, as usual
DPLUSWINRT_API bool DPWinRT_StartCaptureFromOverlay(vr::VROverlayHandle_t overlay_handle, vr::VROverlayHandle_t overlay_handle_source);
DPLUSWINRT_API bool DPWinRT_PauseCapture(vr::VROverlayHandle_t overlay_handle, bool pause);
DPLUSWINRT_API bool DPWinRT_StopCapture(vr::VROverlayHandle_t overlay_handle);

DPLUSWINRT_API bool DPWinRT_SetOverlayUpdateLimitDelay(vr::VROverlayHandle_t overlay_handle, LONGLONG delay_quadpart);
DPLUSWINRT_API bool DPWinRT_SetOverlayOverUnder3D(vr::VROverlayHandle_t overlay_handle, bool is_over_under_3D, int crop_x, int crop_y, int crop_width, int crop_height);
DPLUSWINRT_API void DPWinRT_SetCaptureCursorEnabled(bool is_cursor_enabled);
DPLUSWINRT_API void DPWinRT_SetHDREnabled(bool is_hdr_enabled);
DPLUSWINRT_API void DPWinRT_SetDesktopEnumerationFlags(bool ignore_wmr_screens);


#ifdef __cplusplus
}
#endif