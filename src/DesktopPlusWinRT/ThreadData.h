#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <vector>
#include "openvr.h"

struct DPWinRTOverlayData
{
    vr::VROverlayHandle_t Handle = vr::k_ulOverlayHandleInvalid;
    bool IsPaused = false;
    LARGE_INTEGER UpdateLimiterDelay = {0};
    bool IsOverUnder3D = false;
    int OU3D_crop_x = 0;
    int OU3D_crop_y = 0;
    int OU3D_crop_width  = 1;
    int OU3D_crop_height = 1;
};

struct DPWinRTThreadData
{
    HANDLE ThreadHandle = nullptr;
    DWORD ThreadID = 0;
    std::vector<DPWinRTOverlayData> Overlays;
    HWND SourceWindow = nullptr;
    int DesktopID = -2;
    bool UsePicker = false;
};