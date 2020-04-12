//Manages the UI state and stuffs
//Since this whole thing is the UI, it's the default place for things
//Basic ImGui and rendering backend implementation is left out of it, though

#pragma once

#define NOMINMAX
#include <windows.h>

#include "openvr.h"
#include "Matrices.h"

//Screen/Overlay definitions used to cram the keyboard helper on the same render space as the UI
#define MAIN_SURFACE_HEIGHT 1080
#define KEYBOARD_HELPER_HEIGHT 128
#define KEYBOARD_HELPER_SCALE 0.35f
#define OVERLAY_WIDTH 1920
#define OVERLAY_HEIGHT (MAIN_SURFACE_HEIGHT + KEYBOARD_HELPER_HEIGHT)

class WindowKeyboardHelper;

class UIManager
{
    private:
        HWND m_WindowHandle;
        int m_RepeatFrame;

        bool m_DesktopMode;
        bool m_OpenVRLoaded;         //Desktop mode can run with or without OpenVR and we want to avoid needlessly starting up SteamVR

        float m_UIScale;
        bool m_LowCompositorRes;     //Set when compositor's resolution is set below 100% during init. The user is warned about this as it affects overlay rendering
        bool m_LowCompositorQuality; //Set when the compositor's quality setting to set to something other than High or Auto

        bool m_ElevatedTaskSetUp;   

        vr::VROverlayHandle_t m_OvrlHandle;
        vr::VROverlayHandle_t m_OvrlHandleKeyboardHelper;
        bool m_OvrlVisible;
        bool m_OvrlVisibleKeyboardHelper;
        //Dimensions of the mirror texture, updated from OpenVR when opening up settings or receiving a resolution update message from the overlay application
        int m_OvrlPixelWidth;
        int m_OvrlPixelHeight;

        void DisplayDashboardAppError(const std::string& str);

    public:
        static UIManager* Get();

        UIManager(bool desktop_mode);
        ~UIManager();

        vr::EVRInitError InitOverlay();
        void HandleIPCMessage(const MSG& msg);
        void OnExit();

        void SetWindowHandle(HWND handle);
        HWND GetWindowHandle() const;

        vr::VROverlayHandle_t GetOverlayHandle() const;
        vr::VROverlayHandle_t GetOverlayHandleKeyboardHelper() const;

        //This can be called by functions knowingly making changes which will cause visible layout re-alignment due to ImGui's nature of intermediate UI
        //This will cause 2 extra frames to be calculated but thrown away instantly to be more pleasing to the eye.
        void RepeatFrame(); 
        bool GetRepeatFrame() const;
        void DecreaseRepeatFrameCount();

        bool IsInDesktopMode() const;
        bool IsOpenVRLoaded() const;

        void SetUIScale(float scale);
        float GetUIScale() const;

        bool IsCompositorResolutionLow() const;
        bool IsCompositorRenderQualityLow() const;
        void UpdateCompositorRenderQualityLow();
        bool IsElevatedTaskSetUp() const;

        bool IsOverlayVisible() const;
        bool IsOverlayKeyboardHelperVisible() const;

        void GetOverlayPixelSize(int& width, int& height) const;
        void UpdateOverlayPixelSize();

        void PositionOverlay(WindowKeyboardHelper& window_kdbhelper);
};