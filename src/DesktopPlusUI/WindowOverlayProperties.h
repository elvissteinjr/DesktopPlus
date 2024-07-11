#pragma once

#include "FloatingWindow.h"
#include "WindowDesktopMode.h"

enum WindowOverlayPropertiesPage
{
    wndovrlprop_page_none,
    wndovrlprop_page_main,
    wndovrlprop_page_position_change,
    wndovrlprop_page_crop_change,
    wndovrlprop_page_graphics_capture_source,
    wndovrlprop_page_actions_order,
    wndovrlprop_page_actions_order_add,
};

class WindowOverlayProperties : public FloatingWindow, public FloatingWindowDesktopModeInterop
{
    private:
        std::vector<WindowOverlayPropertiesPage> m_PageStack;
        std::vector<WindowOverlayPropertiesPage> m_PageStackPending; //Stores pending page forward calls that they were requested during an active backwards animation
        int m_PageStackPos;
        int m_PageStackPosAnimation;
        WindowOverlayPropertiesPage m_PageAppearing; //Similar to ImGui::IsWindowAppearing(), equals the current page ID for a single frame if it or the window is newly appearing
        WindowOverlayPropertiesPage m_PageReturned;  //Equals the previous page ID after PageGoBack() was called, ideally cleared after making use of its value

        int m_PageAnimationDir;
        float m_PageAnimationProgress;
        float m_PageAnimationStartPos;
        float m_PageAnimationOffset;
        int m_PageFadeDir;
        float m_PageFadeAlpha;
        unsigned int m_PageFadeTargetOverlayID;

        unsigned int m_ActiveOverlayID;
        float m_Column0Width;

        OverlayConfigData m_ConfigDataTemp;          //Stores config data used for restoring when a page is canceled
        bool m_IsConfigDataModified;                 //Set to true on page enter and cleared when a page is saving data

        float m_OriginHMDFloorSettingsAnimationProgress;
        float m_OriginTheaterScreenSettingsAnimationProgress;
        std::string m_CropButtonLabel;
        std::string m_WinRTSourceButtonLabel;
        std::string m_ActionButtonsLabel;
        std::string m_BrowserMaxFPSValueText;
        char m_BufferOverlayName[1024];
        char m_BufferOverlayTags[1024];
        bool m_IsBrowserURLChanged;

        //Struct of cached sizes which may change at any time on translation or DPI switching (only the ones that aren't updated unconditionally)
        struct
        {
            float MainCatCapture_WinRTSourceLabelWidth = 0.0f;
            float PositionChange_Column0Width          = 0.0f;
            float PositionChange_ButtonWidth           = 0.0f;
        } 
        m_CachedSizes;

        virtual void WindowUpdate();
        void OverlayPositionReset();

        void UpdatePageMain();
        void UpdatePageMainCatPosition();
        void UpdatePageMainCatAppearance();
        void UpdatePageMainCatCapture();
        void UpdatePageMainCatPerformanceMonitor();
        void UpdatePageMainCatBrowser();
        void UpdatePageMainCatAdvanced();
        void UpdatePageMainCatPerformance();
        void UpdatePageMainCatInterface();

        void UpdatePagePositionChange(bool only_restore_settings = false);
        void UpdatePageCropChange(bool only_restore_settings = false);
        void UpdatePageGraphicsCaptureSource(bool only_restore_settings = false);
        void UpdatePageActionsOrder(bool only_restore_settings = false);
        void UpdatePageActionsOrderAdd();

        std::string GetStringForWinRTSource(HWND source_window, int source_desktop);

        void PageGoForward(WindowOverlayPropertiesPage new_page);
        void PageGoBack();
        void PageGoHome(bool skip_animation = false);
        void PageFadeStart(unsigned int overlay_id);

        void OnPageLeaving(WindowOverlayPropertiesPage previous_page); //Called from PageGoBack() and PageGoHome() to allow for page-specific cleanup if necessary

    public:
        WindowOverlayProperties();
        virtual void Show(bool skip_fade = false);
        virtual void Hide(bool skip_fade = false);
        virtual void ResetTransform(FloatingWindowOverlayStateID state_id);
        virtual vr::VROverlayHandle_t GetOverlayHandle() const;

        virtual void ApplyUIScale();

        unsigned int GetActiveOverlayID() const;
        void SetActiveOverlayID(unsigned int overlay_id, bool skip_fade = false);              //Call with same as active ID to refresh window title and icon

        void UpdateDesktopMode();
        virtual const char* DesktopModeGetTitle() const;
        virtual bool DesktopModeGetIconTextureInfo(ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const;
        virtual float DesktopModeGetTitleIconAlpha() const;
        virtual void DesktopModeOnTitleIconClick();
        virtual void DesktopModeOnTitleBarHover(bool is_hovered);
        virtual bool DesktopModeGoBack();

        void MarkBrowserURLChanged();
};