//Manages the UI state and stuffs
//Since this whole thing is the UI, it's the default place for things
//Basic ImGui and rendering backend implementation is left out of it, though

#pragma once

#define NOMINMAX
#include <windows.h>

#include "openvr.h"
#include "Matrices.h"

#include "DashboardUI.h"
#include "FloatingUI.h"

//Screen/Overlay definitions used to cram the several overlays on the same render space
#define TEXSPACE_VERTICAL_SPACING 2             //Space kept blank between render spaces so interpolation doesn't bleed in pixels
#define TEXSPACE_DASHBOARD_UI_HEIGHT 1080
#define TEXSPACE_FLOATING_UI_HEIGHT 440         //This will break when changing the icon size, but whatever
#define TEXSPACE_KEYBOARD_HELPER_HEIGHT 128
#define TEXSPACE_KEYBOARD_HELPER_SCALE 0.35f
#define TEXSPACE_TOTAL_WIDTH 1920
#define TEXSPACE_TOTAL_HEIGHT (TEXSPACE_DASHBOARD_UI_HEIGHT + TEXSPACE_VERTICAL_SPACING + TEXSPACE_FLOATING_UI_HEIGHT + TEXSPACE_VERTICAL_SPACING + TEXSPACE_KEYBOARD_HELPER_HEIGHT)
#define OVERLAY_WIDTH_METERS_DASHBOARD_UI 2.75f

class WindowKeyboardHelper;

class UIManager
{
    private:
        DashboardUI m_DashboardUI;
        FloatingUI m_FloatingUI;

        HWND m_WindowHandle;
        int m_RepeatFrame;

        bool m_DesktopMode;
        bool m_OpenVRLoaded;         //Desktop mode can run with or without OpenVR and we want to avoid needlessly starting up SteamVR
        bool m_NoRestartOnExit;      //Prevent auto-restart when closing from desktop mode while dashboard app is running (i.e. when using troubleshooting buttons)

        float m_UIScale;
        ImFont* m_FontCompact;
        ImFont* m_FontLarge;         //Only loaded when the UI scale is set to large

        bool m_LowCompositorRes;     //Set when compositor's resolution is set below 100% during init. The user is warned about this as it affects overlay rendering
        bool m_LowCompositorQuality; //Set when the compositor's quality setting to set to something other than High or Auto
        vr::EVROverlayError m_OverlayErrorLast; //Last encountered error when adding an overlay (usually just overlay limit exceeded)
        HRESULT m_WinRTErrorLast;    //Last encountered error when a Graphics Capture thread crashed (ideally never happens)

        bool m_ElevatedTaskSetUp;   

        vr::VROverlayHandle_t m_OvrlHandle;
        vr::VROverlayHandle_t m_OvrlHandleFloatingUI;
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

        DashboardUI& GetDashboardUI();
        FloatingUI& GetFloatingUI();

        void SetWindowHandle(HWND handle);
        HWND GetWindowHandle() const;

        vr::VROverlayHandle_t GetOverlayHandle() const;
        vr::VROverlayHandle_t GetOverlayHandleFloatingUI() const;
        vr::VROverlayHandle_t GetOverlayHandleKeyboardHelper() const;

        //This can be called by functions knowingly making changes which will cause visible layout re-alignment due to ImGui's nature of intermediate UI
        //This will cause 2 extra frames to be calculated but thrown away instantly to be more pleasing to the eye.
        void RepeatFrame(); 
        bool GetRepeatFrame() const;
        void DecreaseRepeatFrameCount();

        bool IsInDesktopMode() const;
        bool IsOpenVRLoaded() const;
        void DisableRestartOnExit();

        void Restart(bool desktop_mode);
        void RestartDashboardApp(bool force_steam = false);
        void ElevatedModeEnter();
        void ElevatedModeLeave();

        void SetUIScale(float scale);
        float GetUIScale() const;
        void SetFonts(ImFont* font_compact, ImFont* font_large);
        ImFont* GetFontCompact() const;
        ImFont* GetFontLarge() const;               //May return nullptr

        bool IsCompositorResolutionLow() const;
        bool IsCompositorRenderQualityLow() const;
        void UpdateCompositorRenderQualityLow();
        vr::EVROverlayError GetOverlayErrorLast() const;
        HRESULT GetWinRTErrorLast() const;
        void ResetOverlayErrorLast();
        void ResetWinRTErrorLast();
        bool IsElevatedTaskSetUp() const;
        static void TryChangingWindowFocus();

        bool IsOverlayVisible() const;
        bool IsOverlayKeyboardHelperVisible() const;

        void GetDesktopOverlayPixelSize(int& width, int& height) const;
        void UpdateDesktopOverlayPixelSize();

        void PositionOverlay(WindowKeyboardHelper& window_kdbhelper);
};