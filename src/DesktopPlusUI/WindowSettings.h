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
    wndsettings_page_app_profiles,
    wndsettings_page_actions,
    wndsettings_page_actions_edit,
    wndsettings_page_actions_order,
    wndsettings_page_actions_order_add,
    wndsettings_page_color_picker,
    wndsettings_page_profile_picker,
    wndsettings_page_action_picker,
    wndsettings_page_keycode_picker,
    wndsettings_page_icon_picker,
    wndsettings_page_window_picker,
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
        WindowSettingsPage m_PageCurrent;   //Equals the current page ID during the function call

        int m_PageAnimationDir;
        float m_PageAnimationProgress;
        float m_PageAnimationStartPos;
        float m_PageAnimationOffset;

        float m_Column0Width;
        float m_WarningHeight;

        std::string m_WarningTextOverlayError;
        std::string m_WarningTextWinRTError;
        std::string m_WarningTextAppProfile;
        std::string m_TranslationAuthorLabel;
        std::string m_ActionButtonsDefaultLabel;
        std::string m_ActionButtonsOverlayBarLabel;
        std::vector<std::string> m_ActionGlobalShortcutLabels;
        std::string m_BrowserMaxFPSValueText;
        std::string m_BrowserBlockListCountText;

        std::string m_ProfileSelectionName;
        bool m_ProfileOverlaySelectIsSaving;
        std::vector<std::string> m_ProfileList;

        ActionUID m_ActionSelectionUID;
        std::vector<ActionManager::ActionNameListEntry> m_ActionList;
        bool m_ActionOrderListEditForOverlayBar;

        std::string m_ProfilePickerName;
        ActionUID m_ActionPickerUID;
        unsigned char m_KeyCodePickerID;
        unsigned int m_KeyCodePickerHotkeyFlags;
        bool m_KeyCodePickerNoMouse;
        bool m_KeyCodePickerHotkeyMode;
        std::string m_IconPickerFile;
        HWND m_WindowPickerHWND;

        std::vector< std::pair<std::string, std::string> > m_AppList;   //app key, app name

        //Struct of cached sizes which may change at any time on translation or DPI switching (only the ones that aren't updated unconditionally)
        struct
        {
            ImVec2 Profiles_ButtonDeleteSize;
            ImVec2 Actions_ButtonDeleteSize;
            ImVec2 ActionEdit_ButtonDeleteSize;
        } 
        m_CachedSizes;

        virtual void WindowUpdate();

        void UpdateWarnings();

        void UpdatePageMain();
        void UpdatePageMainCatInterface();
        void UpdatePageMainCatProfiles();
        void UpdatePageMainCatActions();
        void UpdatePageMainCatInput();
        void UpdatePageMainCatWindows();
        void UpdatePageMainCatBrowser();
        void UpdatePageMainCatPerformance();
        void UpdatePageMainCatMisc();
        void UpdatePagePersistentUI();
        void UpdatePageKeyboardLayout(bool only_restore_settings = false);
        void UpdatePageProfiles();
        void UpdatePageProfilesOverlaySelect();
        void UpdatePageAppProfiles();
        void UpdatePageActions();
        void UpdatePageActionsEdit(bool only_restore_settings = false);
        void UpdatePageActionsOrder(bool only_restore_settings = false);
        void UpdatePageActionsOrderAdd();
        void UpdatePageColorPicker(bool only_restore_settings = false);
        void UpdatePageProfilePicker();
        void UpdatePageActionPicker();
        void UpdatePageKeyCodePicker(bool only_restore_settings = false);
        void UpdatePageIconPicker();
        void UpdatePageWindowPicker();
        void UpdatePageResetConfirm();

        void PageGoForward(WindowSettingsPage new_page);
        void PageGoBack();
        void PageGoBackInstantly();
        void PageGoHome();

        void OnPageLeaving(WindowSettingsPage previous_page); //Called from PageGoBack() and PageGoHome() to allow for page-specific cleanup if necessary

        void SelectableWarning(const char* selectable_id, const char* popup_id, const char* text, bool show_warning_prefix = true, const ImVec4* text_color = nullptr);
        void SelectableHotkey(ConfigHotkey& hotkey, int id);

        void RefreshAppList();

    public:
        WindowSettings();
        virtual void Hide(bool skip_fade = false);
        virtual void ResetTransform(FloatingWindowOverlayStateID state_id);
        virtual vr::VROverlayHandle_t GetOverlayHandle() const;

        virtual void ApplyUIScale();

        void UpdateDesktopMode();
        void UpdateDesktopModeWarnings();
        void DesktopModeSetRootPage(WindowSettingsPage root_page);
        virtual const char* DesktopModeGetTitle() const;
        virtual bool DesktopModeGetIconTextureInfo(ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const;
        virtual bool DesktopModeGoBack();

        void ClearCachedTranslationStrings();
};