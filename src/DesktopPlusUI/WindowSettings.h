#pragma once

#include "FloatingWindow.h"

enum WindowSettingsPage
{
    wndsettings_page_none,
    wndsettings_page_main,
    wndsettings_page_keyboard,
    wndsettings_page_profiles,
    wndsettings_page_profiles_overlay_select,
    wndsettings_page_reset_confirm
};

enum WindowSettingsMainCategory
{
    wndsettings_cat_interface,
    wndsettings_cat_profiles,
    wndsettings_cat_actions,
    wndsettings_cat_keyboard,
    wndsettings_cat_mouse,
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
        float m_WarningHeight;

        std::string m_WarningTextOverlayError;
        std::string m_WarningTextWinRTError;

        std::string m_ProfileSelectionName;
        bool m_ProfileOverlaySelectIsSaving;
        std::vector<std::string> m_ProfileList;

        virtual void WindowUpdate();

        void UpdateWarnings();

        void UpdatePageMain();
        void UpdatePageMainCatInterface();
        void UpdatePageMainCatActions();
        void UpdatePageMainCatProfiles();
        void UpdatePageMainCatInput();
        void UpdatePageMainCatWindows();
        void UpdatePageMainCatMisc();
        void UpdatePageKeyboardLayout();
        void UpdatePageProfiles();
        void UpdatePageProfilesOverlaySelect();
        void UpdatePageResetConfirm();

        void PageGoForward(WindowSettingsPage new_page);
        void PageGoBack();
        void PageGoBackInstantly();
        void PageGoHome();

        void SelectableWarning(const char* selectable_id, const char* popup_id, const char* text, bool show_warning_prefix = true, const ImVec4* text_color = nullptr);

    public:
        WindowSettingsNew();
        virtual void Hide(bool skip_fade = false);
        virtual void ResetTransform();
        virtual vr::VROverlayHandle_t GetOverlayHandle() const;

        void ClearCachedTranslationStrings();
};