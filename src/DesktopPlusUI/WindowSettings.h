#pragma once

#include "FloatingWindow.h"

enum WindowSettingsPage
{
    wndsettings_page_none,
    wndsettings_page_main,
    wndsettings_page_keyboard
};

enum WindowSettingsMainCategory
{
    wndsettings_cat_interface,
    wndsettings_cat_actions,
    wndsettings_cat_keyboard,
    wndsettings_cat_laser_pointer,
    wndsettings_cat_window_overlays,
    wndsettings_cat_version_info,
    wndsettings_cat_warnings,
    wndsettings_cat_startup,
    wndsettings_cat_troubleshooting,
    wndsettings_cat_MAX
};

class WindowSettingsNew : public FloatingWindow
{
    private:
        std::vector<WindowSettingsPage> m_PageStack;
        int m_PageStackPos;
        int m_PageStackPosAnimation;
        WindowSettingsPage m_PageAppearing; //Similar to ImGui::IsWindowAppearing(), equals the current page ID for a single frame if it or the window is newly appearing

        int m_PageAnimationDir;
        float m_PageAnimationProgress;
        float m_PageAnimationStartPos;
        float m_PageAnimationOffset;

        bool m_IsScrolling;
        float m_ScrollMainCatPos[wndsettings_cat_MAX];
        float m_ScrollMainCurrent;
        float m_ScrollMainMaxPos;
        float m_ScrollProgress;
        float m_ScrollStartPos;
        float m_ScrollTargetPos;

        float m_Column0Width;

        virtual void WindowUpdate();

        void UpdatePageMain();
        void UpdatePageMainCatInterface();
        void UpdatePageMainCatActions();
        void UpdatePageMainCatInput();
        void UpdatePageMainCatWindows();
        void UpdatePageMainCatMisc();
        void UpdatePageKeyboardLayout();

        void PageGoForward(WindowSettingsPage new_page);
        void PageGoBack();
        void PageGoHome();

    public:
        WindowSettingsNew();
        virtual void Hide(bool skip_fade = false);
        virtual void ResetTransform();
        virtual vr::VROverlayHandle_t GetOverlayHandle() const;
};