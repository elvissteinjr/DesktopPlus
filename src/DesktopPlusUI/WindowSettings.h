#pragma once

#include "FloatingWindow.h"
#include "WindowDesktopMode.h"

enum WindowSettingsPage
{
    wndsettings_page_none,
    wndsettings_page_main,
    wndsettings_page_persistent_ui,
    wndsettings_page_keyboard,
    wndsettings_page_profiles,
    wndsettings_page_profiles_overlay_select,
    wndsettings_page_color_picker,
    wndsettings_page_reset_confirm
};

class WindowSettings : public FloatingWindow, public FloatingWindowDesktopModeInterop
{
    private:
        std::vector<WindowSettingsPage> m_PageStack;
        int m_PageStackPos;
        int m_PageStackPosAnimation;
        WindowSettingsPage m_PageAppearing; //Similar to ImGui::IsWindowAppearing(), equals the current page ID for a single frame if it or the window is newly appearing
        WindowSettingsPage m_PageReturned;  //Equals the previous page ID after PageGoBack() was called, ideally cleared after making use of its value

        int m_PageAnimationDir;
        float m_PageAnimationProgress;
        float m_PageAnimationStartPos;
        float m_PageAnimationOffset;

        float m_Column0Width;
        float m_WarningHeight;

        std::string m_WarningTextOverlayError;
        std::string m_WarningTextWinRTError;
        std::string m_BrowserMaxFPSValueText;
        std::string m_BrowserBlockListCountText;

        std::string m_ProfileSelectionName;
        bool m_ProfileOverlaySelectIsSaving;
        std::vector<std::string> m_ProfileList;

        //Struct of cached sizes which may change at any time on translation or DPI switching (only the ones that aren't updated unconditionally)
        struct
        {
            ImVec2 Profiles_ButtonDeleteSize;
        } 
        m_CachedSizes;

        virtual void WindowUpdate();

        void UpdateWarnings();

        void UpdatePageMain();
        void UpdatePageMainCatInterface();
        void UpdatePageMainCatActions();
        void UpdatePageMainCatProfiles();
        void UpdatePageMainCatInput();
        void UpdatePageMainCatWindows();
        void UpdatePageMainCatBrowser();
        void UpdatePageMainCatPerformance();
        void UpdatePageMainCatMisc();
        void UpdatePagePersistentUI();
        void UpdatePageKeyboardLayout();
        void UpdatePageProfiles();
        void UpdatePageProfilesOverlaySelect();
        void UpdatePageColorPicker();
        void UpdatePageResetConfirm();

        void PageGoForward(WindowSettingsPage new_page);
        void PageGoBack();
        void PageGoBackInstantly();
        void PageGoHome();

        void SelectableWarning(const char* selectable_id, const char* popup_id, const char* text, bool show_warning_prefix = true, const ImVec4* text_color = nullptr);

    public:
        WindowSettings();
        virtual void Hide(bool skip_fade = false);
        virtual void ResetTransform(FloatingWindowOverlayStateID state_id);
        virtual vr::VROverlayHandle_t GetOverlayHandle() const;

        virtual void ApplyUIScale();

        void UpdateDesktopMode();
        void UpdateDesktopModeWarnings();
        void DesktopModeSetRootPage(WindowSettingsPage root_page);
        virtual const char* DesktopModeGetTitle();
        virtual bool DesktopModeGetIconTextureInfo(ImVec2& size, ImVec2& uv_min, ImVec2& uv_max);
        virtual bool DesktopModeGoBack();

        void ClearCachedTranslationStrings();
};