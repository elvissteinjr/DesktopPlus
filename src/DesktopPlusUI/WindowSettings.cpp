#include "WindowSettings.h"

#include <sstream>
#include <unordered_set>
#include <shlwapi.h>

#include "ImGuiExt.h"
#include "UIManager.h"
#include "TranslationManager.h"
#include "WindowManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"

WindowSettings::WindowSettings() :
    m_PageStackPos(0),
    m_PageStackPosAnimation(0),
    m_PageAnimationDir(0),
    m_PageAnimationProgress(0.0f),
    m_PageAnimationStartPos(0.0f),
    m_PageAnimationOffset(0.0f),
    m_PageAppearing(wndsettings_page_none),
    m_PageReturned(wndsettings_page_none),
    m_PageCurrent(wndsettings_page_none),
    m_Column0Width(0.0f),
    m_WarningHeight(0.0f),
    m_ProfileOverlaySelectIsSaving(false),
    m_ActionSelectionUID(0),
    m_ActionOrderListEditForOverlayBar(false),
    m_ActionPickerUID(k_ActionUID_Invalid),
    m_KeyCodePickerID(0),
    m_KeyCodePickerHotkeyFlags(0),
    m_KeyCodePickerNoMouse(false),
    m_KeyCodePickerHotkeyMode(false),
    m_WindowPickerHWND(nullptr)
{
    m_WindowTitleStrID = tstr_SettingsWindowTitle;
    m_WindowIcon = tmtex_icon_xsmall_settings;
    m_OvrlWidth    = OVERLAY_WIDTH_METERS_SETTINGS;
    m_OvrlWidthMax = OVERLAY_WIDTH_METERS_SETTINGS * 3.0f;

    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect rect = UITextureSpaces::Get().GetRect(ui_texspace_settings);
    m_Size = {float(rect.GetWidth() - 4), float(rect.GetHeight() - 4)};
    m_SizeUnscaled = m_Size;

    m_Pos = {float(rect.GetTL().x + 2), float(rect.GetTL().y + 2)};

    m_PageStack.push_back(wndsettings_page_main);

    FloatingWindow::ResetTransformAll();
}

void WindowSettings::Hide(bool skip_fade)
{
    FloatingWindow::Hide();

    ConfigManager::Get().SaveConfigToFile();
}

void WindowSettings::ResetTransform(FloatingWindowOverlayStateID state_id)
{
    FloatingWindow::ResetTransform(state_id);

    FloatingWindowOverlayState& overlay_state = GetOverlayState(state_id);

    overlay_state.Transform.rotateY(-15.0f);
    overlay_state.Transform.translate_relative(OVERLAY_WIDTH_METERS_DASHBOARD_UI / 3.0f, 0.70f, 0.15f);
}

vr::VROverlayHandle_t WindowSettings::GetOverlayHandle() const
{
    return UIManager::Get()->GetOverlayHandleSettings();
}

void WindowSettings::ApplyUIScale()
{
    FloatingWindow::ApplyUIScale();

    m_CachedSizes = {};
}

void WindowSettings::UpdateDesktopMode()
{
    WindowUpdate();
}

void WindowSettings::UpdateDesktopModeWarnings()
{
    UpdateWarnings();
}

void WindowSettings::DesktopModeSetRootPage(WindowSettingsPage root_page)
{
    m_PageStack[0] = root_page;
    m_PageAppearing = root_page;
}

const char* WindowSettings::DesktopModeGetTitle() const
{
    if (m_PageStack[0] == wndsettings_page_profiles)
        return TranslationManager::GetString(tstr_SettingsProfilesOverlays);
    else if (m_PageStack[0] == wndsettings_page_app_profiles)
        return TranslationManager::GetString(tstr_SettingsProfilesApps);
    else if (m_PageStack[0] == wndsettings_page_actions)
        return TranslationManager::GetString(tstr_DesktopModeToolActions);
    else
        return TranslationManager::GetString(m_WindowTitleStrID);
}

bool WindowSettings::DesktopModeGetIconTextureInfo(ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const
{
    return TextureManager::Get().GetTextureInfo(m_WindowIcon, size, uv_min, uv_max);
}

bool WindowSettings::DesktopModeGoBack()
{
    if (m_PageStackPos != 0)
    {
        PageGoBack();
        return true;
    }

    return false;
}

float WindowSettings::DesktopModeGetWarningHeight() const
{
    return m_WarningHeight;
}

void WindowSettings::QuickStartGuideGoToPage(WindowSettingsPage new_page)
{
    //This is only meant to be used by the Quick Start Guide window so it's not very flexible
    const WindowSettingsPage current_top_page = m_PageStack[m_PageStackPos];

    if (current_top_page == new_page)
        return;

    switch (new_page)
    {
        case wndsettings_page_main:
        {
            PageGoHome();
            break;
        }
        case wndsettings_page_actions:
        {
            if (current_top_page == wndsettings_page_actions_edit)
            {
                PageGoBack();
            }
            else
            {
                PageGoForward(wndsettings_page_actions);
            }
            break;
        }
        case wndsettings_page_actions_edit:
        {
            if (current_top_page != wndsettings_page_actions)
            {
                PageGoForward(wndsettings_page_actions);
            }

            m_ActionSelectionUID = 0;
            PageGoForward(wndsettings_page_actions_edit);
            break;
        }
    }
}

void WindowSettings::ClearCachedTranslationStrings()
{
    m_WarningTextOverlayError.clear();
    m_WarningTextWinRTError.clear();
    m_WarningTextAppProfile.clear();
    m_TranslationAuthorLabel.clear();
    m_BrowserMaxFPSValueText.clear();
    m_BrowserBlockListCountText.clear();
    m_ActionButtonsDefaultLabel.clear();
    m_ActionButtonsOverlayBarLabel.clear();
    m_ActionGlobalShortcutLabels.clear();

    for (ConfigHotkey& hotkey : ConfigManager::Get().GetHotkeys())
    {
        hotkey.StateUIName.clear();
    }
}

void WindowSettings::WindowUpdate()
{
    ImGui::SetWindowSize(m_Size);

    ImGuiStyle& style = ImGui::GetStyle();

    m_Column0Width = ImGui::GetFontSize() * 12.75f;

    float page_width = m_Size.x - style.WindowBorderSize - style.WindowPadding.x - style.WindowPadding.x;

    //Compensate for the padding added in constructor and ignore border in desktop mode
    if (UIManager::Get()->IsInDesktopMode())
    {
        page_width += 2 + style.WindowBorderSize;
    }

    //Page animation
    if (m_PageAnimationDir != 0)
    {
        //Use the averaged framerate value instead of delta time for the first animation step
        //This is to smooth over increased frame deltas that can happen when a new page needs to do initial larger computations or save/load files
        const float progress_step = (m_PageAnimationProgress == 0.0f) ? (1.0f / ImGui::GetIO().Framerate) * 3.0f : ImGui::GetIO().DeltaTime * 3.0f;
        m_PageAnimationProgress += progress_step;

        if (m_PageAnimationProgress >= 1.0f)
        {
            //Remove pages in the stack after finishing going back
            if (m_PageAnimationDir == 1)
            {
                while ((int)m_PageStack.size() > m_PageStackPosAnimation + 1)
                {
                    m_PageStack.pop_back();
                }

                m_PageAnimationDir = 0;

                //Add pending pages now that we don't have an active animation
                while (!m_PageStackPending.empty())
                {
                    PageGoForward(m_PageStackPending[0]);
                    m_PageStackPending.erase(m_PageStackPending.begin());
                }
            }

            m_PageAnimationProgress = 1.0f;
            m_PageAnimationDir      = 0;
        }
    }
    else if (m_PageStackPosAnimation != m_PageStackPos) //Only start new animation if none is running
    {
        m_PageAnimationDir      = (m_PageStackPosAnimation < m_PageStackPos) ? -1 : 1;
        m_PageStackPosAnimation = m_PageStackPos;
        m_PageAnimationStartPos = m_PageAnimationOffset;
        m_PageAnimationProgress = 0.0f;

        //Set appearing value to top of stack when starting animation to it
        if (m_PageAnimationDir == -1)
        {
            m_PageAppearing = m_PageStack.back();
        }
    }
    else if ((m_PageStackPosAnimation == m_PageStackPos) && ((int)m_PageStack.size() > m_PageStackPos + 1))
    {
        //Remove pages that were added and left again while there was no chance to animate anything
        while ((int)m_PageStack.size() > m_PageStackPos + 1)
        {
            m_PageStack.pop_back();
        }
    }
    
    //Set appearing value when the whole window appeared again
    if ((m_PageAnimationDir == 0) && (m_IsWindowAppearing))
    {
        m_PageAppearing = m_PageStack.back();
    }

    const float target_x = (page_width + style.ItemSpacing.x) * -m_PageStackPosAnimation;
    m_PageAnimationOffset = smoothstep(m_PageAnimationProgress, m_PageAnimationStartPos, target_x);

    if (!UIManager::Get()->IsInDesktopMode())
    {
        UpdateWarnings();
    }

    //Set up page offset and clipping
    ImGui::SetCursorPosX( ImGui::GetCursorPosX() + m_PageAnimationOffset);

    ImGui::PushClipRect({m_Pos.x + style.WindowBorderSize, 0.0f}, {m_Pos.x + m_Size.x - style.WindowBorderSize, FLT_MAX}, false);

    const char* const child_str_id[] {"SettingsPageMain", "SettingsPage1", "SettingsPage2", "SettingsPage3"}; //No point in generating these on the fly
    const ImVec2 child_size = {page_width, ImGui::GetContentRegionAvail().y};
    int child_id = 0;
    int stack_size = (int)m_PageStack.size();
    for (WindowSettingsPage page_id : m_PageStack)
    {
        if (child_id >= IM_ARRAYSIZE(child_str_id))
            break;

        m_PageCurrent = page_id;

        //Disable items when the page isn't active
        const bool is_inactive_page = (child_id != m_PageStackPos);

        if (is_inactive_page)
        {
            ImGui::PushItemDisabledNoVisual();
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f)); //This prevents child bg color being visible if there's a widget before this (e.g. warnings)

        if ( (ImGui::BeginChild(child_str_id[child_id], child_size, ImGuiChildFlags_NavFlattened)) || (m_PageAppearing == page_id) ) //Process page if currently appearing
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg

            switch (page_id)
            {
                case wndsettings_page_main:                    UpdatePageMain();                    break;
                case wndsettings_page_persistent_ui:           UpdatePagePersistentUI();            break;
                case wndsettings_page_keyboard:                UpdatePageKeyboardLayout();          break;
                case wndsettings_page_profiles:                UpdatePageProfiles();                break;
                case wndsettings_page_profiles_overlay_select: UpdatePageProfilesOverlaySelect();   break;
                case wndsettings_page_app_profiles:            UpdatePageAppProfiles();             break;
                case wndsettings_page_actions:                 UpdatePageActions();                 break;
                case wndsettings_page_actions_edit:            UpdatePageActionsEdit();             break;
                case wndsettings_page_color_picker:            UpdatePageColorPicker();             break;
                case wndsettings_page_profile_picker:          UpdatePageProfilePicker();           break;
                case wndsettings_page_action_picker:           UpdatePageActionPicker();            break;
                case wndsettings_page_actions_order_add:       UpdatePageActionsOrderAdd();         break;
                case wndsettings_page_actions_order:           UpdatePageActionsOrder();            break;
                case wndsettings_page_keycode_picker:          UpdatePageKeyCodePicker();           break;
                case wndsettings_page_icon_picker:             UpdatePageIconPicker();              break;
                case wndsettings_page_window_picker:           UpdatePageWindowPicker();            break;
                case wndsettings_page_reset_confirm:           UpdatePageResetConfirm();            break;
                default: break;
            }
        }
        else
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg
        }

        if (is_inactive_page)
        {
            ImGui::PopItemDisabledNoVisual();
        }

        ImGui::EndChild();

        if (child_id + 1 < stack_size)
        {
            ImGui::SameLine();
        }

        child_id++;
    }

    m_PageAppearing = wndsettings_page_none;
    m_PageCurrent = wndsettings_page_none;

    ImGui::PopClipRect();
}

void WindowSettings::UpdateWarnings()
{
    if (!UIManager::Get()->IsAnyWarningDisplayed())
    {
        m_WarningHeight = 0.0f;
        return;
    }

    bool warning_displayed = false;
    bool popup_visible = false;

    static float popup_alpha = 0.0f;

    const float warning_height_start = ImGui::GetCursorPosY();

    //Compositor resolution warning
    {
        bool& hide_compositor_res_warning = ConfigManager::GetRef(configid_bool_interface_warning_compositor_res_hidden);

        if ( (!hide_compositor_res_warning) && (UIManager::Get()->IsCompositorResolutionLow()) )
        {
            SelectableWarning("##WarningCompRes", "DontShowAgain", TranslationManager::GetString(tstr_SettingsWarningCompositorResolution));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DontShowAgain", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDontShowAgain)))
                {
                    hide_compositor_res_warning = true;
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //Compositor quality warning
    {
        bool& hide_compositor_quality_warning = ConfigManager::GetRef(configid_bool_interface_warning_compositor_quality_hidden);

        if ( (!hide_compositor_quality_warning) && (UIManager::Get()->IsCompositorRenderQualityLow()) )
        {
            SelectableWarning("##WarningCompQuality", "DontShowAgain2", TranslationManager::GetString(tstr_SettingsWarningCompositorQuality));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DontShowAgain2", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDontShowAgain)))
                {
                    hide_compositor_quality_warning = true;
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //Dashboard app process elevation warning
    {
        bool& hide_process_elevation_warning = ConfigManager::GetRef(configid_bool_interface_warning_process_elevation_hidden);

        if ( (!hide_process_elevation_warning) && (ConfigManager::GetValue(configid_bool_state_misc_process_elevated)) )
        {
            SelectableWarning("##WarningElevation", "DontShowAgain3", TranslationManager::GetString(tstr_SettingsWarningProcessElevated));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DontShowAgain3", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDontShowAgain)))
                {
                    hide_process_elevation_warning = true;
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //Elevated mode warning (this is different from elevated dashboard process)
    {
        bool& hide_elevated_mode_warning = ConfigManager::GetRef(configid_bool_interface_warning_elevated_mode_hidden);

        if ( (!hide_elevated_mode_warning) && (ConfigManager::GetValue(configid_bool_state_misc_elevated_mode_active)) )
        {
            SelectableWarning("##WarningElevatedMode", "DontShowAgain4", TranslationManager::GetString(tstr_SettingsWarningElevatedMode));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DontShowAgain4", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDontShowAgain)))
                {
                    hide_elevated_mode_warning = true;
                }
                else if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsTroubleshootingElevatedModeLeave)))
                {
                    UIManager::Get()->ElevatedModeLeave();
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //Browser missing warning
    {
        bool& hide_browser_missing_warning = ConfigManager::GetRef(configid_bool_interface_warning_browser_missing_hidden);

        if ( (!hide_browser_missing_warning) && (ConfigManager::GetValue(configid_bool_state_misc_browser_used_but_missing)) )
        {
            SelectableWarning("##WarningBrowserMissing", "DontShowAgain5", TranslationManager::GetString(tstr_SettingsWarningBrowserMissing));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DontShowAgain5", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDontShowAgain)))
                {
                    hide_browser_missing_warning = true;
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //Browser mismatch warning
    {
        bool& hide_browser_version_mismatch_warning = ConfigManager::GetRef(configid_bool_interface_warning_browser_version_mismatch_hidden);

        if ( (!hide_browser_version_mismatch_warning) && (ConfigManager::GetValue(configid_bool_state_misc_browser_version_mismatch)) )
        {
            SelectableWarning("##WarningBrowserMismatch", "DontShowAgain6", TranslationManager::GetString(tstr_SettingsWarningBrowserMismatch));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DontShowAgain6", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDontShowAgain)))
                {
                    hide_browser_version_mismatch_warning = true;
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //Focused process elevation warning
    {
        if (  (ConfigManager::GetValue(configid_bool_state_window_focused_process_elevated)) && (!ConfigManager::GetValue(configid_bool_state_misc_process_elevated)) && 
             (!ConfigManager::GetValue(configid_bool_state_misc_elevated_mode_active))       && (!ConfigManager::GetValue(configid_bool_state_misc_uiaccess_enabled)) )
        {
            SelectableWarning("##WarningElevation2", "FocusedElevatedContext", TranslationManager::GetString(tstr_SettingsWarningElevatedProcessFocus));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("FocusedElevatedContext", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_DefActionSwitchTask)))
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_switch_task);
                    UIManager::Get()->RepeatFrame();
                }
                else if ((UIManager::Get()->IsElevatedTaskSetUp()) && ImGui::Selectable(TranslationManager::GetString(tstr_SettingsTroubleshootingElevatedModeEnter)))
                {
                    UIManager::Get()->ElevatedModeEnter();
                    UIManager::Get()->RepeatFrame();
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //UIAccess lost warning
    {
        if ( (ConfigManager::GetValue(configid_bool_misc_uiaccess_was_enabled)) && (!ConfigManager::GetValue(configid_bool_state_misc_uiaccess_enabled)) )
        {
            SelectableWarning("##WarningUIAccess", "DontShowAgain6", TranslationManager::GetString(tstr_SettingsWarningUIAccessLost));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DontShowAgain6", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDontShowAgain)))
                {
                    ConfigManager::SetValue(configid_bool_misc_uiaccess_was_enabled, false);
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //Overlay error warning
    {
        vr::EVROverlayError overlay_error = UIManager::Get()->GetOverlayErrorLast();

        if ( (overlay_error != vr::VROverlayError_None) && (UIManager::Get()->IsOpenVRLoaded()) )
        {
            if (overlay_error == vr::VROverlayError_OverlayLimitExceeded)
            {
                SelectableWarning("##WarningOverlayError", "DismissWarning", TranslationManager::GetString(tstr_SettingsWarningOverlayCreationErrorLimit));
            }
            else
            {
                static vr::EVROverlayError overlay_error_last = overlay_error;

                //Format error string into cached translated string
                if ( (m_WarningTextOverlayError.empty()) || (overlay_error != overlay_error_last) )
                {
                    m_WarningTextWinRTError = TranslationManager::GetString(tstr_SettingsWarningGraphicsCaptureError);
                    StringReplaceAll(m_WarningTextOverlayError, "%ERRORNAME%", vr::VROverlay()->GetOverlayErrorNameFromEnum(overlay_error));

                    overlay_error_last = overlay_error;
                }

                SelectableWarning("##WarningOverlayError", "DismissWarning", m_WarningTextOverlayError.c_str());
            }

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DismissWarning", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDismiss)))
                {
                    UIManager::Get()->ResetOverlayErrorLast();
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //WinRT Capture error warning
    {
        HRESULT hr_error = UIManager::Get()->GetWinRTErrorLast();

        if ( (hr_error != S_OK) && (UIManager::Get()->IsOpenVRLoaded()) )
        {
            static HRESULT hr_error_last = hr_error;

            //Format error code into cached translated string
            if ( (m_WarningTextWinRTError.empty()) || (hr_error != hr_error_last) )
            {
                std::stringstream ss;
                ss << "0x" << std::hex << hr_error;

                m_WarningTextWinRTError = TranslationManager::GetString(tstr_SettingsWarningGraphicsCaptureError);
                StringReplaceAll(m_WarningTextWinRTError, "%ERRORCODE%", ss.str());

                hr_error_last = hr_error;
            }

            SelectableWarning("##WarningWinRTError", "DismissWarning2", m_WarningTextWinRTError.c_str());

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DismissWarning2", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDismiss)))
                {
                    UIManager::Get()->ResetWinRTErrorLast();
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //App profile with overlay profile active warning
    {
        bool& hide_app_profile_active_warning = ConfigManager::GetRef(configid_bool_interface_warning_app_profile_active_hidden);

        if ( (!hide_app_profile_active_warning) && (ConfigManager::Get().GetAppProfileManager().IsActiveProfileWithOverlayProfile()) )
        {
            static std::string active_app_key_last;

            //Format app name into cached translated string
            if ( (m_WarningTextAppProfile.empty()) || (ConfigManager::Get().GetAppProfileManager().GetActiveProfileAppKey() != active_app_key_last) )
            {
                m_WarningTextAppProfile = TranslationManager::GetString(tstr_SettingsWarningAppProfileActive);
                StringReplaceAll(m_WarningTextAppProfile, "%APPNAME%", ConfigManager::Get().GetAppProfileManager().GetActiveProfileAppName());

                active_app_key_last = ConfigManager::Get().GetAppProfileManager().GetActiveProfileAppKey();
            }

            SelectableWarning("##WarningAppProfile", "DontShowAgain7", m_WarningTextAppProfile.c_str()); 

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DontShowAgain7", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDontShowAgain)))
                {
                    hide_app_profile_active_warning = true;
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //Config migrated in current session "warning"
    {
        if (ConfigManager::GetValue(configid_bool_state_misc_config_migrated))
        {
            SelectableWarning("##WarningConfigMigrated", "DismissWarning3", TranslationManager::GetString(tstr_SettingsWarningConfigMigrated), false, &Style_ImGuiCol_TextNotification);

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("DismissWarning3", ImGuiWindowFlags_NoMove))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuDismiss)))
                {
                    ConfigManager::SetValue(configid_bool_state_misc_config_migrated, false);
                }
                ImGui::EndPopup();

                popup_visible = true;
            }
            ImGui::PopStyleVar();

            warning_displayed = true;
        }
    }

    //Separate from the main content if a warning was actually displayed
    if (warning_displayed)
    {
        ImGui::Separator();
    }
    else //...no warning displayed but UIManager still thinks there is one, so update that state
    {
        UIManager::Get()->UpdateAnyWarningDisplayedState();
    }

    m_WarningHeight = ImGui::GetCursorPosY() - warning_height_start;

    //Animate fade-in if any of the popups is visible
    if (popup_visible)
    {
        popup_alpha += ImGui::GetIO().DeltaTime * 10.0f;

        if (popup_alpha > 1.0f)
            popup_alpha = 1.0f;
    }
    else
    {
        popup_alpha = 0.0f;
    }
}

#include "implot.h"

void WindowSettings::UpdatePageMain()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::BeginChild("SettingsMainContent", ImVec2(0.00f, 0.00f), ImGuiChildFlags_NavFlattened);
    ImGui::PopStyleColor();

    //Page Content
    UpdatePageMainCatInterface();
    UpdatePageMainCatProfiles();
    UpdatePageMainCatActions();
    UpdatePageMainCatInput();
    UpdatePageMainCatWindows();
    UpdatePageMainCatBrowser();
    UpdatePageMainCatPerformance();
    UpdatePageMainCatMisc();

    ImGui::EndChild();
}

void WindowSettings::UpdatePageMainCatInterface()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    //Interface
    {
        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatInterface)); 
        ImGui::Columns(2, "ColumnInterface", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( TranslationManager::GetString(tstr_SettingsInterfaceLanguage) );

        if (!TranslationManager::Get().IsCurrentTranslationComplete())
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsInterfaceLanguageIncompleteWarning), "(!)");
        }

        ImGui::NextColumn();

        ImGui::PushItemWidth(-1);
        if (ImGui::BeginComboAnimated("##ComboLang", TranslationManager::Get().GetCurrentTranslationName().c_str() ))
        {
            static std::vector<TranslationManager::ListEntry> list_langs;
            static int list_id = 0;

            //Load language list when dropdown is used for the first time
            if (ImGui::IsWindowAppearing())
            {
                if (list_langs.empty())
                {
                    list_id = 0;
                    list_langs = TranslationManager::GetTranslationList();
                }

                //Select matching entry and add unmapped characters if needed
                const std::string& current_filename = ConfigManager::GetValue(configid_str_interface_language_file);
                for (auto it = list_langs.cbegin(); it != list_langs.cend(); ++it)
                {
                    if (current_filename == it->FileName)
                    {
                        list_id = (int)std::distance(list_langs.cbegin(), it);
                    }

                    UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(it->ListName.c_str());
                }
            }

            int i = 0;
            for (const auto& item : list_langs)
            {
                if (ImGui::Selectable(item.ListName.c_str(), (list_id == i)))
                {
                    ConfigManager::SetValue(configid_str_interface_language_file, item.FileName);
                    TranslationManager::Get().LoadTranslationFromFile(item.FileName);
                    UIManager::Get()->OnTranslationChanged();

                    list_id = i;
                }

                i++;
            }

            ImGui::EndCombo();
        }

        ImGui::NextColumn();
        ImGui::NextColumn();

        if (!TranslationManager::Get().GetCurrentTranslationAuthor().empty())
        {
            if (m_TranslationAuthorLabel.empty())
            {
                m_TranslationAuthorLabel = TranslationManager::GetString(tstr_SettingsInterfaceLanguageCommunity);
                StringReplaceAll(m_TranslationAuthorLabel, "%AUTHOR%", TranslationManager::Get().GetCurrentTranslationAuthor());
            }

            ImGui::Indent(style.ItemInnerSpacing.x);    //Indent a bit since text lined up with the combo widget instead of the widget's text looks a bit odd
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted(m_TranslationAuthorLabel.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Unindent(style.ItemInnerSpacing.x);
        }

        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Indent();

        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsInterfaceAdvancedSettings), &ConfigManager::GetRef(configid_bool_interface_show_advanced_settings)))
        {
            UIManager::Get()->RepeatFrame();
        }
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsInterfaceAdvancedSettingsTip));

        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsInterfaceBlankSpaceDrag), &ConfigManager::GetRef(configid_bool_interface_blank_space_drag_enabled));
        }

        ImGui::Unindent();

        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            ImGui::Spacing();
            ImGui::Columns(2, "ColumnInterface2", false);
            ImGui::SetColumnWidth(0, m_Column0Width);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsInterfacePersistentUI));
            ImGui::NextColumn();

            ImGui::PushID(tstr_SettingsInterfacePersistentUI);  //Avoid ID conflict from common "Manage" label
            if (ImGui::Button(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIManage)))
            {
                PageGoForward(wndsettings_page_persistent_ui);
            }
            ImGui::PopID();

            ImGui::Columns(1);
        }

        ImGui::Spacing();
        ImGui::Columns(2, "ColumnInterface3", false);
        ImGui::SetColumnWidth(0, m_Column0Width);
        ImGui::AlignTextToFramePadding();
        ImGui::Text(TranslationManager::GetString(tstr_SettingsInterfaceDesktopButtons));
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);
        int button_style = clamp(ConfigManager::GetRef(configid_int_interface_desktop_listing_style), 0, (tstr_SettingsInterfaceDesktopButtonsCycle - tstr_SettingsInterfaceDesktopButtonsNone) - 1);
        if (TranslatedComboAnimated("##ComboButtonStyle", button_style, tstr_SettingsInterfaceDesktopButtonsNone, tstr_SettingsInterfaceDesktopButtonsCycle))
        {
            ConfigManager::SetValue(configid_int_interface_desktop_listing_style, button_style);
            UIManager::Get()->RepeatFrame();
        }

        ImGui::NextColumn();
        ImGui::NextColumn();

        bool& include_all = ConfigManager::GetRef(configid_bool_interface_desktop_buttons_include_combined);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsInterfaceDesktopButtonsAddCombined), &include_all))
        {
            UIManager::Get()->RepeatFrame();
        }

        ImGui::Columns(1);
    }

    //Environment (still Interface, but not really)
    {
        static ImVec4 background_color_vec4;

        if ( (m_PageAppearing == wndsettings_page_main) || (m_PageReturned == wndsettings_page_color_picker) )
        {
            background_color_vec4 = ImGui::ColorConvertU32ToFloat4(*(ImU32*)&ConfigManager::Get().GetRef(configid_int_interface_background_color));
            m_PageReturned = wndsettings_page_none;
        }

        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatEnvironment)); 
        ImGui::Columns(2, "ColumnEnvironment", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::Text(TranslationManager::GetString(tstr_SettingsEnvironmentBackgroundColor));
        ImGui::NextColumn();

        if (ImGui::ColorButton("##BackgroundColor", background_color_vec4, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop))
        {
            PageGoForward(wndsettings_page_color_picker);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        int& mode_display = ConfigManager::GetRef(configid_int_interface_background_color_display_mode);

        ImGui::SetNextItemWidth(-1);
        if (TranslatedComboAnimated("##ComboBackgroundDisplay", mode_display, tstr_SettingsEnvironmentBackgroundColorDispModeNever, tstr_SettingsEnvironmentBackgroundColorDispModeAlways))
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_interface_background_color_display_mode, mode_display);
        }

        ImGui::NextColumn();

        bool& dim_ui = ConfigManager::Get().GetRef(configid_bool_interface_dim_ui);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsEnvironmentDimInterface), &dim_ui))
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_interface_dim_ui, dim_ui);

            if (UIManager::Get()->IsOpenVRLoaded())
            {
                UIManager::Get()->UpdateOverlayDimming();
            }
        }
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsEnvironmentDimInterfaceTip));

        ImGui::Columns(1);
    }
}

void WindowSettings::UpdatePageMainCatProfiles()
{
    //Profiles
    {
        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatProfiles)); 
        ImGui::Columns(2, "ColumnInterface", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesOverlays));
        ImGui::NextColumn();

        ImGui::PushID(tstr_SettingsProfilesManage);  //Avoid ID conflict from common "Manage" label
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesManage)))
        {
            PageGoForward(wndsettings_page_profiles);
        }
        ImGui::PopID();

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesApps));
        ImGui::NextColumn();

        ImGui::PushID(tstr_SettingsProfilesApps);  //Avoid ID conflict from common "Manage" label
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesManage)))
        {
            PageGoForward(wndsettings_page_app_profiles);
        }
        ImGui::PopID();

        ImGui::Columns(1);
    }
}

void WindowSettings::UpdatePageMainCatActions()
{
    //Actions
    {
        const ActionManager& action_manager = ConfigManager::Get().GetActionManager();
        ActionManager::ActionList& global_shortcut_list = ConfigManager::Get().GetGlobalShortcuts();
        ConfigHotkeyList& hotkey_list = ConfigManager::Get().GetHotkeys();
        const ImGuiStyle& style = ImGui::GetStyle();

        static ConfigID_Handle action_picker_config_id = configid_handle_MAX;
        static int action_picker_global_shortcut_id = -1;
        static int action_picker_hotkey_id = -1;
        static float button_binding_width = 0.0f;

        if (m_PageReturned == wndsettings_page_actions_order)
        {
            m_ActionButtonsDefaultLabel.clear();
            m_ActionButtonsOverlayBarLabel.clear();

            m_PageReturned = wndsettings_page_none;
        }
        else if (m_PageReturned == wndsettings_page_action_picker)
        {
            if (action_picker_config_id != configid_handle_MAX)
            {
                ConfigManager::SetValue(action_picker_config_id, m_ActionPickerUID);
                IPCManager::Get().PostConfigMessageToDashboardApp(action_picker_config_id, m_ActionPickerUID);
                action_picker_config_id = configid_handle_MAX;

                m_PageReturned = wndsettings_page_none;
            }
            else if (action_picker_global_shortcut_id != -1)
            {
                if ((action_picker_global_shortcut_id >= 0) && (action_picker_global_shortcut_id < (int)global_shortcut_list.size()))
                {
                    global_shortcut_list[action_picker_global_shortcut_id] = m_ActionPickerUID;

                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_handle_state_action_uid, m_ActionPickerUID);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_global_shortcut_set, action_picker_global_shortcut_id);
                }

                action_picker_global_shortcut_id = -1;
                m_PageReturned = wndsettings_page_none;
            }
            else if (action_picker_hotkey_id != -1)
            {
                if ((action_picker_hotkey_id >= 0) && (action_picker_hotkey_id < (int)hotkey_list.size()))
                {
                    ConfigHotkey& hotkey = hotkey_list[action_picker_hotkey_id];

                    hotkey.ActionUID = m_ActionPickerUID;

                    IPCManager::Get().SendStringToDashboardApp(configid_str_state_hotkey_data, hotkey.Serialize(), UIManager::Get()->GetWindowHandle());
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_hotkey_set, action_picker_hotkey_id);
                }

                action_picker_hotkey_id = -1;
                m_PageReturned = wndsettings_page_none;
            }
        }

        if (m_ActionButtonsDefaultLabel.empty())
        {
            const size_t action_count = ConfigManager::Get().GetActionManager().GetActionOrderListBarDefault().size();

            m_ActionButtonsDefaultLabel = TranslationManager::GetString( (action_count == 1) ? tstr_SettingsActionsOrderButtonLabelSingular : tstr_SettingsActionsOrderButtonLabel );
            StringReplaceAll(m_ActionButtonsDefaultLabel, "%COUNT%", std::to_string(action_count));
        }

        if (m_ActionButtonsOverlayBarLabel.empty())
        {
            const size_t action_count = ConfigManager::Get().GetActionManager().GetActionOrderListOverlayBar().size();

            m_ActionButtonsOverlayBarLabel = TranslationManager::GetString( (action_count == 1) ? tstr_SettingsActionsOrderButtonLabelSingular : tstr_SettingsActionsOrderButtonLabel );
            StringReplaceAll(m_ActionButtonsOverlayBarLabel, "%COUNT%", std::to_string(action_count));
        }

        if ((m_ActionGlobalShortcutLabels.empty()) || (m_ActionGlobalShortcutLabels.size() != global_shortcut_list.size()))
        {
            m_ActionGlobalShortcutLabels.clear();

            for (int i = 0; i < global_shortcut_list.size(); ++i)
            {
                std::string label = TranslationManager::GetString(tstr_SettingsActionsGlobalShortcutsEntry);
                StringReplaceAll(label, "%ID%", std::to_string(i + 1));
                m_ActionGlobalShortcutLabels.push_back(label);
            }
        }

        ImGui::Spacing();

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatActions));
        ImGui::Columns(2, "ColumnActions", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsManage));
        ImGui::NextColumn();

        ImGui::PushID(tstr_SettingsActionsManage);  //Avoid ID conflict from common "Manage" label
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsManageButton)))
        {
            PageGoForward(wndsettings_page_actions);
        }
        ImGui::PopID();

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsButtonsOrderDefault));
        ImGui::NextColumn();

        ImGui::PushID(tstr_SettingsActionsButtonsOrderDefault);
        if (ImGui::Button(m_ActionButtonsDefaultLabel.c_str()))
        {
            m_ActionOrderListEditForOverlayBar = false;
            PageGoForward(wndsettings_page_actions_order);
        }
        ImGui::PopID();

        ImGui::NextColumn();

        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsButtonsOrderOverlayBar));
            ImGui::NextColumn();

            ImGui::PushID(tstr_SettingsActionsButtonsOrderOverlayBar);
            if (ImGui::Button(m_ActionButtonsOverlayBarLabel.c_str()))
            {
                m_ActionOrderListEditForOverlayBar = true;
                PageGoForward(wndsettings_page_actions_order);
            }
            ImGui::PopID();

            ImGui::NextColumn();
        }
        ImGui::Spacing();

        ImGui::Columns(1);

        //Active Shortcuts
        if (UIManager::Get()->IsOpenVRLoaded())
            ImGui::Spacing();

        ImGui::Indent();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsActiveShortcuts));
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsActionsActiveShortcutsTip));

        ImGui::PushID("ActiveButtons");

        if (UIManager::Get()->IsOpenVRLoaded())
        {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - button_binding_width - 1.0f);
            if (ImGui::SmallButton(TranslationManager::GetString(tstr_SettingsActionsShowBindings)))
            {
                //OpenBindingUI does not use that app key argument it takes, it always opens the bindings of the calling application
                //To work around this, we pretend to be the app we want to open the bindings for during the call
                //Works and seems to not break anything
                vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), "openvr.component.vrcompositor");
                vr::VRInput()->OpenBindingUI("openvr.component.vrcompositor", vr::k_ulInvalidActionSetHandle, vr::k_ulInvalidInputValueHandle, UIManager::Get()->IsInDesktopMode());
                vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyUIApp);
            }
            button_binding_width = ImGui::GetItemRectSize().x;

            //For reasons unknown, if there's no item added in this spot and the Show Bindings button exist, ImGuiContext::HoveredIdDisabled will be true for the table rows (needs to be false for haptics)
            //This is despite item disabled last being set in CompactTableHeadersRow(), so I'm not sure how this transfers over, but this works, eh
            ImGui::SameLine();
            ImGui::Dummy({0.0f, 0.0f});
        }

        ImGui::Indent();

        const ImVec2 table_size(-style.IndentSpacing - 1.0f, 0.0f);                                     //Replicate padding from columns
        const float table_column_width = m_Column0Width - style.IndentSpacing - style.IndentSpacing;    //Align with width of other columns
        const float table_cell_height = ImGui::GetFontSize() + style.ItemInnerSpacing.y;

        if (BeginCompactTable("TableActiveButtons", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, table_size))
        {
            ImGui::TableSetupColumn(TranslationManager::GetString(tstr_SettingsActionsTableHeaderShortcut), 0, table_column_width);
            ImGui::TableSetupColumn(TranslationManager::GetString(tstr_SettingsActionsTableHeaderAction),   ImGuiTableColumnFlags_WidthStretch);
            CompactTableHeadersRow();

            for (int i = 0; i < 2; ++i)
            {
                ConfigID_Handle config_id = (i == 0) ? configid_handle_input_go_home_action_uid : configid_handle_input_go_back_action_uid;
                ActionUID uid = ConfigManager::GetValue(config_id);

                ImGui::PushID(i);

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString((i == 0) ? tstr_SettingsActionsActiveShortuctsHome : tstr_SettingsActionsActiveShortuctsBack));
                ImGui::SameLine();

                ImGui::TableNextColumn();

                if (ImGui::Selectable(action_manager.GetTranslatedName(uid)))
                {
                    m_ActionPickerUID = uid;
                    action_picker_config_id = config_id;
                    PageGoForward(wndsettings_page_action_picker);
                }

                ImGui::PopID();
            }

            EndCompactTable();
        }

        ImGui::PopID();

        ImGui::Unindent();

        //Global Shortcuts
        ImGui::Spacing();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsGlobalShortcuts));
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsActionsGlobalShortcutsTip));

        ImGui::PushID("GlobalShortcuts");

        if (UIManager::Get()->IsOpenVRLoaded())
        {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - button_binding_width - 1.0f);
            if (ImGui::SmallButton(TranslationManager::GetString(tstr_SettingsActionsShowBindings)))
            {
                //See comment on the active shortcuts
                vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyDashboardApp);
                vr::VRInput()->OpenBindingUI(g_AppKeyDashboardApp, vr::k_ulInvalidActionSetHandle, vr::k_ulInvalidInputValueHandle, UIManager::Get()->IsInDesktopMode());
                vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyUIApp);
            }

            //See Active Shortcuts comment
            ImGui::SameLine();
            ImGui::Dummy({0.0f, 0.0f});
        }

        ImGui::Indent();

        static float table_global_shortcuts_buttons_width = 0.0f;

        const int shortcuts_visible = (int)global_shortcut_list.size();
        const int shortcuts_max     = ConfigManager::GetValue(configid_int_input_global_shortcuts_max_count);

        if (BeginCompactTable("TableGlobalShortcuts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, table_size))
        {
            ImGui::TableSetupColumn(TranslationManager::GetString(tstr_SettingsActionsTableHeaderShortcut), 0, table_column_width);
            ImGui::TableSetupColumn(TranslationManager::GetString(tstr_SettingsActionsTableHeaderAction),   ImGuiTableColumnFlags_WidthStretch);
            CompactTableHeadersRow();

            IM_ASSERT(m_ActionGlobalShortcutLabels.size() == global_shortcut_list.size());

            int shortcut_id = 0;
            for (ActionUID uid : global_shortcut_list)
            {
                ImGui::PushID(shortcut_id);

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(m_ActionGlobalShortcutLabels[shortcut_id].c_str());
                ImGui::SameLine();

                ImGui::TableNextColumn();
                if (ImGui::Selectable(action_manager.GetTranslatedName(uid)))
                {
                    m_ActionPickerUID = uid;
                    action_picker_global_shortcut_id = shortcut_id;
                    PageGoForward(wndsettings_page_action_picker);
                }

                ImGui::PopID();

                ++shortcut_id;
            }

            EndCompactTable();
        }

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - table_global_shortcuts_buttons_width - 1.0f);

        ImGui::BeginGroup();

        if (shortcuts_visible >= shortcuts_max)
            ImGui::PushItemDisabled();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsGlobalShortcutsAdd)))
        {
            global_shortcut_list.push_back(k_ActionUID_Invalid);

            ImGui::SetScrollY(ImGui::GetScrollY() + table_cell_height);
        }

        if (shortcuts_visible >= shortcuts_max)
            ImGui::PopItemDisabled();

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (shortcuts_visible <= 1)
            ImGui::PushItemDisabled();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsGlobalShortcutsRemove)))
        {
            global_shortcut_list.pop_back();

            ImGui::SetScrollY(ImGui::GetScrollY() - table_cell_height);
        }

        if (shortcuts_visible <= 1)
            ImGui::PopItemDisabled();

        ImGui::EndGroup();

        table_global_shortcuts_buttons_width = ImGui::GetItemRectSize().x + style.IndentSpacing;

        ImGui::PopID();

        ImGui::Unindent();

        //Hotkeys
        static float table_hotkeys_max_column_width = 0.0f;
        static float table_hotkeys_remove_button_width = 0.0f;
        static float table_hotkeys_buttons_width = 0.0f;
        static int table_hotkeys_hovered_row = -1;

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsHotkeys));
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsActionsHotkeysTip));

        ImGui::Indent();

        ImGui::PushID("Hotkeys");

        if (BeginCompactTable("TableHotkeys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, table_size))
        {
            ImGui::TableSetupColumn(TranslationManager::GetString(tstr_SettingsActionsTableHeaderHotkey), 0, std::max(table_column_width, table_hotkeys_max_column_width));
            ImGui::TableSetupColumn(TranslationManager::GetString(tstr_SettingsActionsTableHeaderAction), ImGuiTableColumnFlags_WidthStretch);
            CompactTableHeadersRow();

            ImGui::TableNextColumn();

            float line_start_x = ImGui::GetCursorPosX();
            table_hotkeys_max_column_width = 0.0f;
            int hovered_row_new = -1;

            int hotkey_id = 0;
            for (ConfigHotkey& hotkey : hotkey_list)
            {
                ImGui::PushID(hotkey_id);

                ImGui::AlignTextToFramePadding();
                SelectableHotkey(hotkey, hotkey_id);
                ImGui::SameLine(0.0f, 0.0f);
                table_hotkeys_max_column_width = std::max(table_hotkeys_max_column_width, ImGui::GetCursorPosX() - line_start_x);

                ImGui::TableNextColumn();

                if (table_hotkeys_hovered_row == hotkey_id)
                {
                    //Use lower-alpha version of hovered header color while the remove button is hovered to increase the contrast with the button hover color (we don't really want to change it, however)
                    ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered);
                    col.w *= 0.75f;
                    ImGui::PushStyleColor(ImGuiCol_Header, col);
                }

                if (ImGui::Selectable(action_manager.GetTranslatedName(hotkey.ActionUID), (table_hotkeys_hovered_row == hotkey_id), ImGuiSelectableFlags_AllowOverlap))
                {
                    m_ActionPickerUID = hotkey.ActionUID;
                    action_picker_hotkey_id = hotkey_id;
                    PageGoForward(wndsettings_page_action_picker);
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                {
                    hovered_row_new = hotkey_id;
                }

                if (table_hotkeys_hovered_row == hotkey_id)
                {
                    ImGui::PopStyleColor();

                    //Show remove button only if there's more than one hotkey or the single existing hotkey has properties set (basically no button that doesn't do anything)
                    if ((hotkey_list.size() > 1) || (hotkey.KeyCode != 0) || (hotkey.ActionUID != k_ActionUID_Invalid))
                    {
                        ImGui::SetNextItemAllowOverlap();
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - table_hotkeys_remove_button_width);

                        if (ImGui::SmallButton(TranslationManager::GetString(tstr_SettingsActionsHotkeysRemove)))
                        {
                            if (hotkey_list.size() > 1)
                            {
                                hotkey_list.erase(hotkey_list.begin() + hotkey_id);
                                IPCManager::Get().SendStringToDashboardApp(configid_str_state_hotkey_data, "", UIManager::Get()->GetWindowHandle());
                            }
                            else //If there's only one entry, clear it instead to not have a weird looking table
                            {
                                hotkey = ConfigHotkey();
                                IPCManager::Get().SendStringToDashboardApp(configid_str_state_hotkey_data, hotkey.Serialize(), UIManager::Get()->GetWindowHandle());
                            }

                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_hotkey_set, hotkey_id);

                            //Keep scroll position constant
                            ImGui::SetScrollY(ImGui::GetScrollY() - table_cell_height);

                            //Erased something straight out of the list, get out of the loop and discard frame
                            UIManager::Get()->RepeatFrame();
                            ImGui::PopID();
                            break;
                        }

                        if (ImGui::IsItemHovered())
                        {
                            hovered_row_new = hotkey_id;
                        }

                        table_hotkeys_remove_button_width = ImGui::GetItemRectSize().x;
                    }
                }

                ImGui::TableNextColumn();

                ImGui::PopID();

                ++hotkey_id;
            }

            table_hotkeys_hovered_row = hovered_row_new;

            EndCompactTable();
        }

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - table_hotkeys_buttons_width - 1.0f);

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsHotkeysAdd)))
        {
            ConfigHotkey hotkey_new;
            hotkey_list.push_back(hotkey_new);

            IPCManager::Get().SendStringToDashboardApp(configid_str_state_hotkey_data, hotkey_new.Serialize(), UIManager::Get()->GetWindowHandle());
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_hotkey_set, hotkey_list.size() - 1);

            ImGui::SetScrollY(ImGui::GetScrollY() + table_cell_height);

            UIManager::Get()->RepeatFrame(3);   //Avoid some flicker from potentially hovering selectable + remove button appearing in the next few frames
        }

        table_hotkeys_buttons_width = ImGui::GetItemRectSize().x + style.IndentSpacing;

        ImGui::PopID();

        ImGui::Unindent();
        ImGui::Unindent();

        ImGui::Columns(1);
    }
}

void WindowSettings::UpdatePageMainCatInput()
{
    static bool is_any_gc_overlay_active = false;

    if (m_PageAppearing == wndsettings_page_main)
    {
        //Check if any Graphics Capture overlays are active
        is_any_gc_overlay_active = false;
        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
        {
            if (OverlayManager::Get().GetConfigData(i).ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture)
            {
                is_any_gc_overlay_active = true;
                break;
            }
        }
    }

    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();
    const ImGuiStyle& style = ImGui::GetStyle();

    //Keyboard
    {
        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatKeyboard)); 
        ImGui::Columns(2, "ColumnKeyboard", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsKeyboardLayout));
        ImGui::NextColumn();

        if (ImGui::Button( UIManager::Get()->GetVRKeyboard().GetLayoutMetadata().Name.c_str() ))
        {
            PageGoForward(wndsettings_page_keyboard);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsKeyboardSize));
        ImGui::NextColumn();

        //Keyboard size setting shows size of currently visible overlay state (usually dashboard tab) and applies it to all
        WindowKeyboard& window_keyboard = UIManager::Get()->GetVRKeyboard().GetWindow();
        float& size = window_keyboard.GetOverlayState(window_keyboard.GetOverlayStateCurrentID()).Size;

        vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("KeyboardSize") );
        if (ImGui::SliderWithButtonsFloatPercentage("KeyboardSize", size, 5, 1, 50, 200, "%d%%"))
        {
            if (size < 0.10f)
                size = 0.10f;

            window_keyboard.GetOverlayState(floating_window_ovrl_state_room).Size          = size;
            window_keyboard.GetOverlayState(floating_window_ovrl_state_dashboard_tab).Size = size;

            UIManager::Get()->GetVRKeyboard().GetWindow().ApplyCurrentOverlayState();
        }
        vr_keyboard.VRKeyboardInputEnd();

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsKeyboardBehavior));
        ImGui::NextColumn();

        ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardStickyMod), &ConfigManager::GetRef(configid_bool_input_keyboard_sticky_modifiers));

        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyRepeat), &ConfigManager::GetRef(configid_bool_input_keyboard_key_repeat));

        bool& auto_show_desktop = ConfigManager::Get().GetRef(configid_bool_input_keyboard_auto_show_desktop);
        bool& auto_show_browser = ConfigManager::Get().GetRef(configid_bool_input_keyboard_auto_show_browser);

        //Arrange the checkboxes in their own group if browser is available, otherwise just add a single one for desktop/window under the behavior group
        if (DPBrowserAPIClient::Get().IsBrowserAvailable())
        {
            ImGui::Spacing();
            ImGui::NextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsKeyboardAutoShow));
            ImGui::NextColumn();

            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardAutoShowDesktop), &auto_show_desktop))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_input_keyboard_auto_show_desktop, auto_show_desktop);
            }
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsKeyboardAutoShowDesktopTip));

            ImGui::NextColumn();
            ImGui::NextColumn();

            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardAutoShowBrowser), &auto_show_browser))
            {
                if (!auto_show_browser)
                {
                    //Hide currently auto-visible keyboard if it's shown for a browser overlay
                    int overlay_id = vr_keyboard.GetWindow().GetAssignedOverlayID();

                    if ( (overlay_id >= 0) && (OverlayManager::Get().GetConfigData((unsigned int)overlay_id).ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser) )
                    {
                        vr_keyboard.GetWindow().SetAutoVisibility(overlay_id, false);
                    }
                }
            }
        }
        else
        {
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardAutoShowDesktopOnly), &auto_show_desktop))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_input_keyboard_auto_show_desktop, auto_show_desktop);
            }
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsKeyboardAutoShowDesktopTip));
        }

        ImGui::Columns(1);
    }

    //Mouse
    {
        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatMouse)); 

        ImGui::Indent();

        bool& render_cursor = ConfigManager::Get().GetRef(configid_bool_input_mouse_render_cursor);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsMouseShowCursor), &render_cursor))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_mouse_render_cursor), render_cursor);
        }

        if ( (!render_cursor) && (is_any_gc_overlay_active) )
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsMouseShowCursorGCActiveWarning), "(!)");
        }
        else if (!DPWinRT_IsCaptureCursorEnabledPropertySupported())
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsMouseShowCursorGCUnsupported), "(!)");
        }

        bool& scroll_smooth = ConfigManager::GetRef(configid_bool_input_mouse_scroll_smooth);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsMouseScrollSmooth), &scroll_smooth))
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_input_mouse_scroll_smooth, scroll_smooth);
        }

        bool& simulate_pen = ConfigManager::GetRef(configid_bool_input_mouse_simulate_pen_input);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsMouseSimulatePen), &simulate_pen))
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_input_mouse_simulate_pen_input, simulate_pen);
        }
        if (!ConfigManager::GetValue(configid_bool_state_pen_simulation_supported))
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsMouseSimulatePenUnsupported), "(!)");
        }

        bool& pointer_override = ConfigManager::Get().GetRef(configid_bool_input_mouse_allow_pointer_override);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsMouseAllowLaserPointerOverride), &pointer_override))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_mouse_allow_pointer_override), pointer_override);
        }
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsMouseAllowLaserPointerOverrideTip));

        ImGui::Unindent();

        //Double-Click Assistant
        ImGui::Columns(2, "ColumnMouse", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsMouseDoubleClickAssist)); 
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsMouseDoubleClickAssistTip));

        ImGui::NextColumn();

        //The way mapping max + 1 == -1 value into the slider is done is a bit convoluted, but still works
        int& assist_duration = ConfigManager::Get().GetRef(configid_int_input_mouse_dbl_click_assist_duration_ms);
        const int assist_duration_max = 3000; //The "Auto" wrapping makes this the absolute maximum value even with manual input, but longer than 3 seconds is questionable either way
        int assist_duration_ui = (assist_duration == -1) ? assist_duration_max + 1 : assist_duration;

        const char* text_alt_assist = nullptr;
        if (assist_duration <= 0)
        {
            text_alt_assist = TranslationManager::GetString((assist_duration == -1) ? tstr_SettingsMouseDoubleClickAssistTipValueAuto : tstr_SettingsMouseDoubleClickAssistTipValueOff);
        }

        vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("DBLClickAssist") );
        if (ImGui::SliderWithButtonsInt("DBLClickAssist", assist_duration_ui, 25, 5, 0, assist_duration_max + 1, (text_alt_assist != nullptr) ? "" : "%d ms", 0, nullptr, text_alt_assist))
        {
            assist_duration = clamp(assist_duration_ui, 0, assist_duration_max + 1);

            if (assist_duration_ui > assist_duration_max)
                assist_duration = -1;

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_mouse_dbl_click_assist_duration_ms, assist_duration);
        }
        vr_keyboard.VRKeyboardInputEnd();

        ImGui::NextColumn();

        //Input Smoothing
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsMouseSmoothing));
        ImGui::NextColumn();

        int& input_smoothing_level = ConfigManager::Get().GetRef(configid_int_input_mouse_input_smoothing_level);
        const int input_smoothing_level_max = tstr_SettingsMouseSmoothingLevelVeryHigh - tstr_SettingsMouseSmoothingLevelNone;
        input_smoothing_level = clamp(input_smoothing_level, 0, input_smoothing_level_max);

        if (ImGui::SliderWithButtonsInt("SmoothingLevel", input_smoothing_level, 1, 1, 0, input_smoothing_level_max, "##%d", ImGuiSliderFlags_NoInput, nullptr, 
                                        TranslationManager::GetString( (TRMGRStrID)(tstr_SettingsMouseSmoothingLevelNone + input_smoothing_level) )))
        {
            input_smoothing_level = clamp(input_smoothing_level, 0, input_smoothing_level_max);

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_mouse_input_smoothing_level, input_smoothing_level);
        }

        ImGui::Columns(1);
    }

    //Laser Pointer
    {
        static ConfigID_Int edited_hmd_pointer_input_id = configid_int_MAX;

        //Write changes if we're returning from a picker
        if ((m_PageReturned == wndsettings_page_keycode_picker) && (edited_hmd_pointer_input_id != configid_int_MAX))
        {
            ConfigManager::SetValue(edited_hmd_pointer_input_id, m_KeyCodePickerID);
            IPCManager::Get().PostConfigMessageToDashboardApp(edited_hmd_pointer_input_id, m_KeyCodePickerID);

            m_PageReturned = wndsettings_page_none;
            edited_hmd_pointer_input_id = configid_int_MAX;
        }

        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatLaserPointer));
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsLaserPointerTip));

        ImGui::Indent();

        bool& block_input = ConfigManager::GetRef(configid_bool_input_laser_pointer_block_input);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsLaserPointerBlockInput), &block_input))
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_input_laser_pointer_block_input, block_input);
        }

        ImGui::Unindent();

        ImGui::Columns(2, "ColumnLaserPointer", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsLaserPointerAutoToggleDistance));
        ImGui::NextColumn();

        float& distance = ConfigManager::GetRef(configid_float_input_detached_interaction_max_distance);
        const char* alt_text = (distance < 0.01f) ? TranslationManager::GetString(tstr_SettingsLaserPointerAutoToggleDistanceValueOff) : nullptr;

        vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("LaserPointerMaxDistance") );
        if (ImGui::SliderWithButtonsFloat("LaserPointerMaxDistance", distance, 0.05f, 0.01f, 0.0f, 3.0f, (distance < 0.01f) ? "##%.2f" : "%.2f m", ImGuiSliderFlags_Logarithmic, nullptr, alt_text))
        {
            if (distance < 0.01f)
                distance = 0.0f;

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_float_input_detached_interaction_max_distance, distance);
        }
        vr_keyboard.VRKeyboardInputEnd();

        ImGui::Columns(1);
        ImGui::Spacing();

        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            ImGui::Indent();

            bool& hmd_pointer_enabled = ConfigManager::GetRef(configid_bool_input_laser_pointer_hmd_device);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsLaserPointerHMDPointer), &hmd_pointer_enabled))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_input_laser_pointer_hmd_device, hmd_pointer_enabled);
            }

            ImGui::Indent(ImGui::GetFrameHeightWithSpacing());

            if (!hmd_pointer_enabled)
                ImGui::PushItemDisabled();

            ImGui::PushID("HMDPointer");

            const ImVec2 table_size(-style.IndentSpacing - 1.0f, 0.0f);                                                    //Replicate padding from columns
            const float table_column_width = m_Column0Width - style.IndentSpacing - ImGui::GetFrameHeightWithSpacing();    //Align with width of other columns 

            if (BeginCompactTable("TableHMDPointerInput", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, table_size))
            {
                ImGui::TableSetupColumn(TranslationManager::GetString(tstr_SettingsLaserPointerHMDPointerTableHeaderInputAction), 0, table_column_width);
                ImGui::TableSetupColumn(TranslationManager::GetString(tstr_SettingsLaserPointerHMDPointerTableHeaderBinding), ImGuiTableColumnFlags_WidthStretch);
                CompactTableHeadersRow();

                const int key_count = configid_int_input_laser_pointer_hmd_device_keycode_drag - configid_int_input_laser_pointer_hmd_device_keycode_toggle;
                IM_ASSERT(configid_int_input_laser_pointer_hmd_device_keycode_toggle < configid_int_input_laser_pointer_hmd_device_keycode_drag);
                IM_ASSERT(tstr_SettingsLaserPointerHMDPointerTableBindingToggle + key_count < tstr_MAX);

                for (int i = 0; i < key_count + 1; ++i)
                {
                    ConfigID_Int config_id = (ConfigID_Int)(configid_int_input_laser_pointer_hmd_device_keycode_toggle + i);

                    ImGui::PushID(i);

                    ImGui::TableNextColumn();

                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(TranslationManager::GetString( (TRMGRStrID)(tstr_SettingsLaserPointerHMDPointerTableBindingToggle + i) ));
                    ImGui::SameLine();

                    ImGui::TableNextColumn();

                    if (ImGui::Selectable( GetStringForKeyCode(ConfigManager::GetValue(config_id)) ))
                    {
                        m_KeyCodePickerNoMouse    = true;
                        m_KeyCodePickerHotkeyMode = false;
                        m_KeyCodePickerID = ConfigManager::GetValue(config_id);
                        edited_hmd_pointer_input_id = config_id;

                        PageGoForward(wndsettings_page_keycode_picker);
                        m_PageReturned = wndsettings_page_none;
                    }

                    ImGui::PopID();
                }

                EndCompactTable();
            }

            ImGui::PopID();

            if (!hmd_pointer_enabled)
                ImGui::PopItemDisabled();

            ImGui::Unindent(ImGui::GetFrameHeightWithSpacing());
            ImGui::Unindent();
        }
    }
}

void WindowSettings::UpdatePageMainCatWindows()
{
    //Window Overlays
    {
        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatWindowOverlays)); 

        ImGui::Indent();

        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            bool& auto_focus = ConfigManager::GetRef(configid_bool_windows_winrt_auto_focus);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysAutoFocus), &auto_focus))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_windows_winrt_auto_focus, auto_focus);
            }

            bool& keep_on_screen = ConfigManager::GetRef(configid_bool_windows_winrt_keep_on_screen);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysKeepOnScreen), &keep_on_screen))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_windows_winrt_keep_on_screen, keep_on_screen);
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsWindowOverlaysKeepOnScreenTip));
        }

        bool& auto_size_overlay = ConfigManager::GetRef(configid_bool_windows_winrt_auto_size_overlay);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysAutoSizeOverlay), &auto_size_overlay))
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_windows_winrt_auto_size_overlay, auto_size_overlay);
        }

        bool& focus_scene_app = ConfigManager::GetRef(configid_bool_windows_winrt_auto_focus_scene_app);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysFocusSceneApp), &focus_scene_app))
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_windows_winrt_auto_focus_scene_app, focus_scene_app);
        }

        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            bool& auto_focus_scene_app_dashboard = ConfigManager::GetRef(configid_bool_windows_auto_focus_scene_app_dashboard);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysFocusSceneAppDashboard), &auto_focus_scene_app_dashboard))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_windows_auto_focus_scene_app_dashboard, auto_focus_scene_app_dashboard);
            }
        }

        ImGui::Unindent();

        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            ImGui::Spacing();
            ImGui::Columns(2, "ColumnWindows", false);
            ImGui::SetColumnWidth(0, m_Column0Width);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsWindowOverlaysOnWindowDrag));
            ImGui::NextColumn();

            int& mode_dragging = ConfigManager::GetRef(configid_int_windows_winrt_dragging_mode);

            ImGui::SetNextItemWidth(-1);
            if (TranslatedComboAnimated("##ComboWindowDrag", mode_dragging, tstr_SettingsWindowOverlaysOnWindowDragDoNothing, tstr_SettingsWindowOverlaysOnWindowDragOverlay))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_windows_winrt_dragging_mode, mode_dragging);
            }

            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsWindowOverlaysOnCaptureLoss));
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsWindowOverlaysOnCaptureLossTip));
            ImGui::NextColumn();

            int& behavior_capture_loss = ConfigManager::GetRef(configid_int_windows_winrt_capture_lost_behavior);

            ImGui::SetNextItemWidth(-1);
            if (TranslatedComboAnimated("##ComboCaptureLost", behavior_capture_loss, tstr_SettingsWindowOverlaysOnCaptureLossDoNothing, tstr_SettingsWindowOverlaysOnCaptureLossRemove))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_windows_winrt_capture_lost_behavior, behavior_capture_loss);
            }
        }

        ImGui::Columns(1);
    }
}

void WindowSettings::UpdatePageMainCatBrowser()
{
    //Browser
    if (DPBrowserAPIClient::Get().IsBrowserAvailable())
    {
        VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatBrowser));

        ImGui::Columns(2, "ColumnBrowser", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsBrowserMaxFrameRate));
        ImGui::NextColumn();

        int& max_fps = ConfigManager::Get().GetRef(configid_int_browser_max_fps);

        if (m_BrowserMaxFPSValueText.empty())
        {
            m_BrowserMaxFPSValueText =  TranslationManager::GetString(tstr_SettingsPerformanceUpdateLimiterFPSValue);
            StringReplaceAll(m_BrowserMaxFPSValueText, "%FPS%", std::to_string(max_fps));
        }

        vr_keyboard.VRKeyboardInputBegin(ImGui::SliderWithButtonsGetSliderID("MaxFPS"));
        if (ImGui::SliderWithButtonsInt("MaxFPS", max_fps, 5, 1, 1, 144, "##%d", 0, nullptr, m_BrowserMaxFPSValueText.c_str()))
        {
            if (max_fps < 1)
                max_fps = 1;

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_browser_max_fps, max_fps);

            m_BrowserMaxFPSValueText = "";
        }
        vr_keyboard.VRKeyboardInputEnd();

        ImGui::NextColumn();

        bool& content_blocker = ConfigManager::GetRef(configid_bool_browser_content_blocker);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsBrowserContentBlocker), &content_blocker))
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_browser_content_blocker, content_blocker);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsBrowserContentBlockerTip));

        ImGui::NextColumn();
        
        static int block_list_count_last = -1;

        if ( (m_BrowserBlockListCountText.empty()) || (ConfigManager::GetValue(configid_int_state_browser_content_blocker_list_count) != block_list_count_last) )
        {
            block_list_count_last = ConfigManager::GetValue(configid_int_state_browser_content_blocker_list_count);

            m_BrowserBlockListCountText = TranslationManager::GetString((block_list_count_last != 1) ? tstr_SettingsBrowserContentBlockerListCount : tstr_SettingsBrowserContentBlockerListCountSingular);
            StringReplaceAll(m_BrowserBlockListCountText, "%LISTCOUNT%", std::to_string(block_list_count_last) );
        }

        if ( (content_blocker) && (block_list_count_last != -1) )
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(m_BrowserBlockListCountText.c_str());
        }

        ImGui::Columns(1);
    }
}

void WindowSettings::UpdatePageMainCatPerformance()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    //Performance
    {
        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatPerformance)); 
        ImGui::Columns(2, "ColumnPerformance", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        UpdateLimiterSetting(false);

        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            ImGui::Spacing();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsCaptureMethodDup));
            ImGui::NextColumn();

            ImGui::Spacing();
            bool& rapid_updates = ConfigManager::Get().GetRef(configid_bool_performance_rapid_laser_pointer_updates);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsPerformanceRapidUpdates), &rapid_updates))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_performance_rapid_laser_pointer_updates), rapid_updates);
            }
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsPerformanceRapidUpdatesTip));

            ImGui::NextColumn();
            ImGui::NextColumn();

            bool& single_desktop = ConfigManager::Get().GetRef(configid_bool_performance_single_desktop_mirroring);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsPerformanceSingleDesktopMirror), &single_desktop))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_performance_single_desktop_mirroring), single_desktop);
            }
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsPerformanceSingleDesktopMirrorTip));

            ImGui::NextColumn();
            ImGui::Spacing();

            bool& use_hdr = ConfigManager::Get().GetRef(configid_bool_performance_hdr_mirroring);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsPerformanceUseHDR), &use_hdr))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_performance_hdr_mirroring), use_hdr);
            }
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsPerformanceUseHDRTip));

            ImGui::NextColumn();
            ImGui::NextColumn();
        }

        bool& show_fps = ConfigManager::Get().GetRef(configid_bool_performance_show_fps);
        ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsPerformanceShowFPS), &show_fps);

        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            bool& auto_throttle = ConfigManager::Get().GetRef(configid_bool_performance_ui_auto_throttle);
            ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsPerformanceUIAutoThrottle), &auto_throttle);
        }

        ImGui::Columns(1);
    }
}

void WindowSettings::UpdatePageMainCatMisc()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    static bool is_autolaunch_enabled = false;

    if ( (UIManager::Get()->IsOpenVRLoaded()) && (m_PageAppearing == wndsettings_page_main) )
    {
        is_autolaunch_enabled = vr::VRApplications()->GetApplicationAutoLaunch(g_AppKeyDashboardApp);
    }

    //Version Info
    {
        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatVersionInfo)); 
        ImGui::Indent();

        ImGui::TextUnformatted(k_pch_DesktopPlusVersion);

        ImGui::Unindent();
    }

    //Warnings
    {
        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatWarnings)); 
        ImGui::Columns(2, "ColumnResetWarnings", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        int warning_hidden_count = 0;

        if (ConfigManager::GetValue(configid_bool_interface_warning_compositor_quality_hidden))
            warning_hidden_count++;
        if (ConfigManager::GetValue(configid_bool_interface_warning_compositor_res_hidden))
            warning_hidden_count++;
        if (ConfigManager::GetValue(configid_bool_interface_warning_process_elevation_hidden))
            warning_hidden_count++;
        if (ConfigManager::GetValue(configid_bool_interface_warning_elevated_mode_hidden))
            warning_hidden_count++;
        if (ConfigManager::GetValue(configid_bool_interface_warning_browser_missing_hidden))
            warning_hidden_count++;
        if (ConfigManager::GetValue(configid_bool_interface_warning_browser_version_mismatch_hidden))
            warning_hidden_count++;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsWarningsHidden));
        ImGui::SameLine();
        ImGui::Text("%i", warning_hidden_count);

        ImGui::NextColumn();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsWarningsReset)))
        {
            ConfigManager::SetValue(configid_bool_interface_warning_compositor_quality_hidden,       false);
            ConfigManager::SetValue(configid_bool_interface_warning_compositor_res_hidden,           false);
            ConfigManager::SetValue(configid_bool_interface_warning_process_elevation_hidden,        false);
            ConfigManager::SetValue(configid_bool_interface_warning_elevated_mode_hidden,            false);
            ConfigManager::SetValue(configid_bool_interface_warning_browser_missing_hidden,          false);
            ConfigManager::SetValue(configid_bool_interface_warning_browser_version_mismatch_hidden, false);

            UIManager::Get()->UpdateAnyWarningDisplayedState();
        }

        ImGui::Columns(1);
    }

    //Startup
    bool& no_steam = ConfigManager::GetRef(configid_bool_misc_no_steam);

    if ( (ConfigManager::Get().IsSteamInstall()) || (UIManager::Get()->IsOpenVRLoaded()) ) //Only show if Steam install or we can access OpenVR settings
    {
        ImGui::Spacing();

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatStartup));
        ImGui::Indent();

        if (UIManager::Get()->IsOpenVRLoaded())
        {
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsStartupAutoLaunch), &is_autolaunch_enabled))
            {
                vr::VRApplications()->SetApplicationAutoLaunch(g_AppKeyDashboardApp, is_autolaunch_enabled);
            }
        }

        if (ConfigManager::Get().IsSteamInstall())
        {
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsStartupSteamDisable), &no_steam))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_misc_no_steam, no_steam);
            }
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsStartupSteamDisableTip));
        }

        ImGui::Unindent();
    }

    //Troubleshooting
    {
        ImGui::Spacing();

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatTroubleshooting));
        ImGui::Columns(2, "ColumnTroubleshooting", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        //All the restart buttons only start up new processes, but both UI and dashboard app get rid of the older instance when starting
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Desktop+");
        ImGui::NextColumn();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingRestart)))
        {
            UIManager::Get()->RestartDashboardApp();
        }

        bool has_restart_steam_button = ( (ConfigManager::Get().IsSteamInstall()) && (!ConfigManager::GetValue(configid_bool_state_misc_process_started_by_steam)) );

        if (has_restart_steam_button)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

            if (no_steam)
                ImGui::PushItemDisabled();

            if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingRestartSteam)))
            {
                UIManager::Get()->RestartDashboardApp(true);
            }

            if (no_steam)
                ImGui::PopItemDisabled();
        }

        if (UIManager::Get()->IsElevatedTaskSetUp())
        {
            static float elevated_task_button_width = 0.0f;

            //Put this button on a new line if it doesn't fit (typically happens with [Restart with Steam] button present in VR mode)
            if (ImGui::GetItemRectMax().x + style.ItemSpacing.x + elevated_task_button_width < ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x)
                ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

            const bool dashboard_app_running  = IPCManager::IsDashboardAppRunning();

            if (!dashboard_app_running)
                ImGui::PushItemDisabled();

            if (!ConfigManager::GetValue(configid_bool_state_misc_elevated_mode_active))
            {
                if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingElevatedModeEnter)))
                {
                    UIManager::Get()->ElevatedModeEnter();
                }
            }
            else
            {
                if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingElevatedModeLeave)))
                {
                    UIManager::Get()->ElevatedModeLeave();
                }
            }

            elevated_task_button_width = ImGui::GetItemRectSize().x;

            if (!dashboard_app_running)
                ImGui::PopItemDisabled();
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Desktop+ UI");
        ImGui::NextColumn();

        ImGui::PushID("##UI");

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingRestart)))
        {
            UIManager::Get()->Restart(false);
        }

        ImGui::PopID();

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingRestartDesktop)))
        {
            UIManager::Get()->Restart(true);
        }

        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Indent();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsReset)))
        {
            PageGoForward(wndsettings_page_reset_confirm);
        }

        ImGui::Unindent();
    }
}

void WindowSettings::UpdatePagePersistentUI()
{
    static float tab_item_height = 0.0f;
    //We only use half the size for column 0 on this page since we can really use the space for other things
    const float column_0_width = m_Column0Width / 2.0f;
    const float column_1_width = (ImGui::GetContentRegionAvail().x - column_0_width) / 2.0f;

    auto window_state_tab_item = [&](const char* label, FloatingWindow& window, bool& config_var_state_restore) 
    {
        if (ImGui::BeginTabItem(label, 0, ImGuiTabItemFlags_NoPushId))  //NoPushId avoids flickering from size calculations and is not problematic two controls are never active at the same time
        {
            if (ImGui::IsWindowAppearing())
            {
                UIManager::Get()->RepeatFrame();
            }

            ImGuiStyle& style = ImGui::GetStyle();
            FloatingWindowOverlayState& state_room      = window.GetOverlayState(floating_window_ovrl_state_room);
            FloatingWindowOverlayState& state_dplus_tab = window.GetOverlayState(floating_window_ovrl_state_dashboard_tab);

            const bool use_lazy_resize = (&window == &UIManager::Get()->GetSettingsWindow()); //Lazily resize for settings window since the input is done through it

            //Draw background and border manually to merge with the tab-bar
            ImVec2 border_min = ImGui::GetCursorScreenPos();
            ImVec2 border_max = border_min;
            border_min.y -= style.ItemSpacing.y + 1;
            border_max.x += ImGui::GetContentRegionAvail().x;
            border_max.y += tab_item_height;
            ImGui::GetWindowDrawList()->AddRectFilled(border_min, border_max, ImGui::GetColorU32(ImGuiCol_ChildBg));
            ImGui::GetWindowDrawList()->AddRect(      border_min, border_max, ImGui::GetColorU32(ImGuiCol_Border ));

            //Window state table
            ImGui::Columns(3, "ColumnPersistWindowState", false);
            ImGui::SetColumnWidth(0, column_0_width);
            ImGui::SetColumnWidth(1, column_1_width);
            ImGui::SetColumnWidth(2, column_1_width);

            //Headers
            ImGui::NextColumn();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsStateGlobal));
    
            ImGui::NextColumn();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsStateDashboardTab));
            ImGui::NextColumn();

            //Visible
            ImGui::Spacing();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsStateVisible));
            ImGui::NextColumn();

            ImGui::Spacing();
            if (ImGui::Checkbox("##CheckVisibleRoom", &state_room.IsVisible))
            {
                //Pin automatically if in room state and unpinned is not allowed
                if ( (!window.CanUnpinRoom()) && (!state_room.IsPinned) )
                {
                    //Pin and unpin (if needed) current state to have it apply to the room state. This is only needed when there's no pinned transform yet
                    const bool current_pinned = window.IsPinned();

                    window.SetPinned(true);

                    if (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_dashboard_tab)
                    {
                        window.SetPinned(current_pinned, true);
                    }
                }

                if (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_room)
                {
                    window.ApplyCurrentOverlayState();
                }

                //When hiding keyboard, the assigned overlay also needs to be cleared so it doesn't automatically reappear
                if ( (!state_room.IsVisible) && (&window == &UIManager::Get()->GetVRKeyboard().GetWindow()) )
                {
                    UIManager::Get()->GetVRKeyboard().GetWindow().SetAssignedOverlayID(-1, floating_window_ovrl_state_room);
                }
            }
            ImGui::NextColumn();

            ImGui::Spacing();
            if (ImGui::Checkbox("##CheckVisibleDPlusTab", &state_dplus_tab.IsVisible))
            {
                if (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_dashboard_tab)
                {
                    window.ApplyCurrentOverlayState();
                }
            }
            ImGui::NextColumn();

            //Pinned
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsStatePinned));
            ImGui::NextColumn();

            if (!window.CanUnpinRoom())
                ImGui::PushItemDisabled();

            if (ImGui::Checkbox("##CheckPinnedRoom", &state_room.IsPinned))
            {
                if (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_room)
                {
                    window.SetPinned(state_room.IsPinned, true);
                }
                else
                {
                    //Pin and unpin current state to have it apply to the room state
                    const bool current_pinned = window.IsPinned();

                    window.SetPinned(state_room.IsPinned);
                    window.SetPinned(current_pinned, true);
                }
            }

            if (!window.CanUnpinRoom())
                ImGui::PopItemDisabled();

            ImGui::NextColumn();

            if (ImGui::Checkbox("##CheckPinnedDPlusTab", &state_dplus_tab.IsPinned))
            {
                if (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_dashboard_tab)
                {
                    window.SetPinned(state_dplus_tab.IsPinned, true);
                }
            }
            ImGui::NextColumn();

            //Position
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsStatePosition));
            ImGui::NextColumn();

            ImGui::PushID(floating_window_ovrl_state_room);
            if (ImGui::Button(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsStatePositionReset)))
            {
                window.ResetTransform(floating_window_ovrl_state_room);

                if (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_room)
                {
                    window.ApplyCurrentOverlayState();
                }
            }
            ImGui::PopID();
            ImGui::NextColumn();

            ImGui::PushID(floating_window_ovrl_state_dashboard_tab);
            if (ImGui::Button(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsStatePositionReset)))
            {
                if (state_dplus_tab.IsPinned)
                {
                    if (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_dashboard_tab)
                    {
                        window.SetPinned(false, true);
                    }
                }

                window.ResetTransform(floating_window_ovrl_state_dashboard_tab);
            }
            ImGui::PopID();
            ImGui::NextColumn();

            //Size
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsStateSize));
            ImGui::NextColumn();

            if (ImGui::SliderWithButtonsFloatPercentage("SizeRoom", state_room.Size, 5, 1, 50, 200, "%d%%"))
            {
                if (state_dplus_tab.Size < 0.10f)
                {
                    state_dplus_tab.Size = 0.10f;
                }

                if ( (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_room) && (!use_lazy_resize) )
                {
                    window.ApplyCurrentOverlayState();
                }
            }
            
            if ( (ImGui::IsItemDeactivated()) && (use_lazy_resize) && (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_room) )
            {
                window.ApplyCurrentOverlayState();
            }
            ImGui::NextColumn();

            if (ImGui::SliderWithButtonsFloatPercentage("SizeDPlusTab", state_dplus_tab.Size, 5, 1, 50, 200, "%d%%"))
            {
                if (state_dplus_tab.Size < 0.10f)
                {
                    state_dplus_tab.Size = 0.10f;
                }

                if ( (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_dashboard_tab) && (!use_lazy_resize) )
                {
                    window.ApplyCurrentOverlayState();
                }
            }
            
            if ( (ImGui::IsItemDeactivated()) && (use_lazy_resize) && (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_dashboard_tab) )
            {
                window.ApplyCurrentOverlayState();
            }

            ImGui::Columns(1);

            ImGui::Indent();
            ImGui::Spacing();
            ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsStateLaunchRestore), &config_var_state_restore);
            ImGui::Unindent();

            ImGui::EndTabItem();

            tab_item_height = ImGui::GetCursorScreenPos().y - border_min.y;
        }
    };

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsInterfacePersistentUI));

    ImGui::Indent();

    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIHelp));
    ImGui::Spacing();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIHelp2));
    ImGui::PopTextWrapPos();

    ImGui::Unindent();

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsHeader));

    ImGui::Indent();

    if (ImGui::BeginTabBar("TabBarPersist"))
    {
        window_state_tab_item(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsSettings),  *this, 
                              ConfigManager::GetRef(configid_bool_interface_window_settings_restore_state));
        window_state_tab_item(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsProperties), UIManager::Get()->GetOverlayPropertiesWindow(), 
                              ConfigManager::GetRef(configid_bool_interface_window_properties_restore_state));
        window_state_tab_item(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsKeyboard),   UIManager::Get()->GetVRKeyboard().GetWindow(),  
                              ConfigManager::GetRef(configid_bool_interface_window_keyboard_restore_state));

        ImGui::EndTabBar();
    }

    ImGui::Unindent();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogDone))) 
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageKeyboardLayout(bool only_restore_settings)
{
    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    static int list_id = -1;
    static std::vector<KeyboardLayoutMetadata> list_layouts;
    static std::vector<std::string> str_list_authors; 
    static bool cluster_enabled_prev[kbdlayout_cluster_MAX] = {false};

    if (only_restore_settings)
    {
        //Restore previous cluster settings
        ConfigManager::SetValue(configid_bool_input_keyboard_cluster_function_enabled,   cluster_enabled_prev[kbdlayout_cluster_function]);
        ConfigManager::SetValue(configid_bool_input_keyboard_cluster_navigation_enabled, cluster_enabled_prev[kbdlayout_cluster_navigation]);
        ConfigManager::SetValue(configid_bool_input_keyboard_cluster_numpad_enabled,     cluster_enabled_prev[kbdlayout_cluster_numpad]);
        ConfigManager::SetValue(configid_bool_input_keyboard_cluster_extra_enabled,      cluster_enabled_prev[kbdlayout_cluster_extra]);

        vr_keyboard.LoadCurrentLayout();
        return;
    }

    if (m_PageAppearing == wndsettings_page_keyboard)
    {
        //Show the keyboard since that's probably useful
        vr_keyboard.GetWindow().Show();

        //Load layout list
        list_id = -1;
        list_layouts = VRKeyboard::GetKeyboardLayoutList();

        //Generate cached author list strings
        str_list_authors.clear();
        for (const auto& metadata: list_layouts)
        {
            if (!metadata.Author.empty())
            {
                std::string author_str = TranslationManager::GetString(tstr_SettingsKeyboardLayoutAuthor);
                StringReplaceAll(author_str, "%AUTHOR%", metadata.Author);
                str_list_authors.push_back(author_str);
            }
            else
            {
                str_list_authors.emplace_back();
            }
        }

        //Select matching entry
        const std::string& current_filename = ConfigManager::GetValue(configid_str_input_keyboard_layout_file);
        auto it = std::find_if(list_layouts.begin(), list_layouts.end(), [&current_filename](const auto& list_entry){ return (current_filename == list_entry.FileName); });

        if (it != list_layouts.end())
        {
            list_id = (int)std::distance(list_layouts.begin(), it);
        }

        //Clusters
        cluster_enabled_prev[kbdlayout_cluster_function]   = ConfigManager::GetValue(configid_bool_input_keyboard_cluster_function_enabled);
        cluster_enabled_prev[kbdlayout_cluster_navigation] = ConfigManager::GetValue(configid_bool_input_keyboard_cluster_navigation_enabled);
        cluster_enabled_prev[kbdlayout_cluster_numpad]     = ConfigManager::GetValue(configid_bool_input_keyboard_cluster_numpad_enabled);
        cluster_enabled_prev[kbdlayout_cluster_extra]      = ConfigManager::GetValue(configid_bool_input_keyboard_cluster_extra_enabled);

        //Reload current layout in case there's a previous pending selection still loaded
        vr_keyboard.LoadCurrentLayout();

        UIManager::Get()->RepeatFrame();
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsKeyboardLayout) ); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 19.0f : 15.0f;
    ImGui::BeginChild("LayoutList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //List layouts
    int index = 0;
    for (const auto& metadata: list_layouts)
    {
        ImGui::PushID(index);

        if (ImGui::Selectable(metadata.Name.c_str(), (index == list_id)))
        {
            list_id = index;
            vr_keyboard.LoadLayoutFromFile(metadata.FileName);
        }

        if (!metadata.Author.empty())
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
            ImGui::TextRightUnformatted(0.0f, str_list_authors[index].c_str());
            ImGui::PopStyleColor();
        }

        ImGui::PopID();

        index++;
    }

    ImGui::EndChild();

    ImGui::Unindent();

    //Key Clusters
    ImGui::Spacing();
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsKeyboardKeyClusters) ); 
    ImGui::Indent();

    bool reload_layout = false;
    bool& function_enabled   = ConfigManager::GetRef(configid_bool_input_keyboard_cluster_function_enabled);
    bool& navigation_enabled = ConfigManager::GetRef(configid_bool_input_keyboard_cluster_navigation_enabled);
    bool& numpad_enabled     = ConfigManager::GetRef(configid_bool_input_keyboard_cluster_numpad_enabled);
    bool& extra_enabled      = ConfigManager::GetRef(configid_bool_input_keyboard_cluster_extra_enabled);

    //Keep unavailable options as enabled but show the check boxes as unticked to avoid confusion
    bool cluster_available[kbdlayout_cluster_MAX] = {false};

    if (list_id != -1)
    {
        const KeyboardLayoutMetadata& current_metadata = list_layouts[list_id];
        std::copy_n(current_metadata.HasCluster, kbdlayout_cluster_MAX, cluster_available);
    }

    bool function_enabled_visual   = (cluster_available[kbdlayout_cluster_function])   ? function_enabled   : false;
    bool navigation_enabled_visual = (cluster_available[kbdlayout_cluster_navigation]) ? navigation_enabled : false;
    bool numpad_enabled_visual     = (cluster_available[kbdlayout_cluster_numpad])     ? numpad_enabled     : false;
    bool extra_enabled_visual      = (cluster_available[kbdlayout_cluster_extra])      ? extra_enabled      : false;

    //Use bigger spacing for the checkbox listing
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {style.ItemSpacing.x * 2.0f, style.ItemSpacing.y});

    if (!cluster_available[kbdlayout_cluster_function])
        ImGui::PushItemDisabled();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyClusterFunction), &function_enabled_visual))
    {
        function_enabled = function_enabled_visual;
        reload_layout = true;
    }

    if (!cluster_available[kbdlayout_cluster_function])
        ImGui::PopItemDisabled();

    ImGui::SameLine();

    if (!cluster_available[kbdlayout_cluster_navigation])
        ImGui::PushItemDisabled();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyClusterNavigation), &navigation_enabled_visual))
    {
        navigation_enabled = navigation_enabled_visual;
        reload_layout = true;
    }

    if (!cluster_available[kbdlayout_cluster_navigation])
        ImGui::PopItemDisabled();

    ImGui::SameLine();

    if (!cluster_available[kbdlayout_cluster_numpad])
        ImGui::PushItemDisabled();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyClusterNumpad), &numpad_enabled_visual))
    {
        numpad_enabled = numpad_enabled_visual;
        reload_layout = true;
    }

    if (!cluster_available[kbdlayout_cluster_numpad])
        ImGui::PopItemDisabled();

    ImGui::SameLine();

    if (!cluster_available[kbdlayout_cluster_extra])
        ImGui::PushItemDisabled();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyClusterExtra), &extra_enabled_visual))
    {
        extra_enabled = extra_enabled_visual;
        reload_layout = true;
    }

    if (!cluster_available[kbdlayout_cluster_extra])
        ImGui::PopItemDisabled();

    ImGui::PopStyleVar(); //ImGuiStyleVar_ItemSpacing

    if ( (reload_layout) && (list_id != -1) )
    {
        vr_keyboard.LoadLayoutFromFile(list_layouts[list_id].FileName);
    }

    ImGui::Unindent();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons
    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogOk))) 
    {
        //Prevent them from being reset
        cluster_enabled_prev[kbdlayout_cluster_function]   = function_enabled;
        cluster_enabled_prev[kbdlayout_cluster_navigation] = navigation_enabled;
        cluster_enabled_prev[kbdlayout_cluster_numpad]     = numpad_enabled;
        cluster_enabled_prev[kbdlayout_cluster_extra]      = extra_enabled;

        if (list_id != -1)
        {
            ConfigManager::SetValue(configid_str_input_keyboard_layout_file, list_layouts[list_id].FileName);
        }

        PageGoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }
    
    if (UIManager::Get()->IsInDesktopMode())
    {
        ImGui::SameLine();

        static float keyboard_editor_button_width = 0.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - keyboard_editor_button_width);

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsKeyboardSwitchToEditor)))
        {
            UIManager::Get()->RestartIntoKeyboardEditor();
        }

        keyboard_editor_button_width = ImGui::GetItemRectSize().x;
    }
}

void WindowSettings::UpdatePageProfiles()
{
    ImGuiStyle& style = ImGui::GetStyle();

    static int list_id = -1;
    static bool scroll_to_selection        = false;
    static bool used_profile_save_new_page = false;
    static bool delete_confirm_state       = false;
    static bool has_loading_failed         = false;
    static bool has_deletion_failed        = false;
    static float list_buttons_width        = 0.0f;
    const bool is_root_page = (m_PageStack[0] == wndsettings_page_profiles);

    if (m_PageAppearing == wndsettings_page_profiles)
    {
        //Load profile list
        m_ProfileList = ConfigManager::Get().GetOverlayProfileList();
        list_id = -1;
        m_ProfileSelectionName = "";
        delete_confirm_state = false;
        has_deletion_failed = false;

        UIManager::Get()->RepeatFrame();
    }
    else if ( (used_profile_save_new_page) && (m_PageStack[m_PageStackPos] == wndsettings_page_profiles) ) //Find m_ProfileSelectionName profile after returning from saving profile
    {
        const auto it = std::find(m_ProfileList.begin(), m_ProfileList.end(), m_ProfileSelectionName);

        if (it != m_ProfileList.end())
        {
            list_id = (int)std::distance(m_ProfileList.begin(), it);
        }
        else
        {
            list_id = (int)m_ProfileList.size() - 1;
            m_ProfileSelectionName = "";
        }

        scroll_to_selection = true;
        used_profile_save_new_page = false;
    }

    if ((m_PageAppearing == wndsettings_page_profiles) || (m_CachedSizes.Profiles_ButtonDeleteSize.x == 0.0f))
    {
        //Figure out size for delete button. We need it to stay the same but also consider the case of the confirm text being longer in some languages
        ImVec2 text_size_delete  = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDelete),        nullptr, true);
        ImVec2 text_size_confirm = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDeleteConfirm), nullptr, true);
        m_CachedSizes.Profiles_ButtonDeleteSize = (text_size_delete.x > text_size_confirm.x) ? text_size_delete : text_size_confirm;

        m_CachedSizes.Profiles_ButtonDeleteSize.x += style.FramePadding.x * 2.0f;
        m_CachedSizes.Profiles_ButtonDeleteSize.y += style.FramePadding.y * 2.0f;

        UIManager::Get()->RepeatFrame();
    }

    bool focus_add_button = false;

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsProfilesOverlaysHeader) );

    //Show errors up here when used as root page since there's no space elsewhere
    if ( (is_root_page) && ( (has_loading_failed) || (has_deletion_failed) ) )
    {
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextError);
        ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString((has_loading_failed) ? tstr_SettingsProfilesOverlaysProfileFailedLoad : tstr_SettingsProfilesOverlaysProfileFailedDelete));
        ImGui::PopStyleColor();
    }

    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? ( (is_root_page) ? 22.0f : 20.0f ) : 16.0f;
    ImGui::BeginChild("ProfileList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //List profiles
    int index = 0;
    for (const auto& name : m_ProfileList)
    {
        ImGui::PushID(index);
        if (ImGui::Selectable(name.c_str(), (index == list_id)))
        {
            list_id = index;
            m_ProfileSelectionName = (list_id == m_ProfileList.size() - 1) ? "" : m_ProfileList[list_id];

            delete_confirm_state = false;
            focus_add_button = ImGui::GetIO().NavVisible;
        }
        ImGui::PopID();

        if ( (scroll_to_selection) && (index == list_id) )
        {
            ImGui::SetScrollHereY();

            if (ImGui::IsItemVisible())
            {
                scroll_to_selection = false;
            }
        }

        index++;
    }

    ImGui::EndChild();

    const bool is_none  = (list_id == -1);
    const bool is_first = (list_id == 0);
    const bool is_last  = (list_id == m_ProfileList.size() - 1);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - list_buttons_width);

    ImGui::BeginGroup();

    if ( (is_last) || (is_none) )
        ImGui::PushItemDisabled();

    if ( (is_first) && (focus_add_button) )
        ImGui::SetKeyboardFocusHere();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileLoad)))
    {
        has_loading_failed = false;

        if (list_id == 0)
        {
            ConfigManager::Get().LoadOverlayProfileDefault(true);

            //Tell dashboard app to load the profile as well
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_profile_name_load, m_ProfileSelectionName, UIManager::Get()->GetWindowHandle());
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_profile_load, MAKELPARAM(ipcactv_ovrl_profile_multi, -2));
        }
        else
        {
            has_loading_failed = !ConfigManager::Get().LoadMultiOverlayProfileFromFile(m_ProfileSelectionName + ".ini");

            if (!has_loading_failed)
            {
                //Tell dashboard app to load the profile as well
                IPCManager::Get().SendStringToDashboardApp(configid_str_state_profile_name_load, m_ProfileSelectionName, UIManager::Get()->GetWindowHandle());
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_profile_load, ipcactv_ovrl_profile_multi);
            }
        }

        if (!has_loading_failed)
        {
            UIManager::Get()->OnProfileLoaded();
        }

        delete_confirm_state = false;
        has_deletion_failed  = false;
    }

    ImGui::SameLine();

    if ( (is_last) || (is_none) )
        ImGui::PopItemDisabled();

    if ( (is_first) || (is_last) || (is_none) )
        ImGui::PushItemDisabled();

    if ( (!is_first) && (focus_add_button) )
        ImGui::SetKeyboardFocusHere();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileAdd)))
    {
        m_ProfileOverlaySelectIsSaving = false;
        PageGoForward(wndsettings_page_profiles_overlay_select);

        delete_confirm_state = false;
        has_loading_failed   = false;
        has_deletion_failed  = false;
    }

    ImGui::SameLine();

    if ( (is_first) || (is_last) || (is_none) )
        ImGui::PopItemDisabled();

    if ( (is_first) || (OverlayManager::Get().GetOverlayCount() == 0) )
        ImGui::PushItemDisabled();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileSave)))
    {
        m_ProfileOverlaySelectIsSaving = true;
        PageGoForward(wndsettings_page_profiles_overlay_select);

        if (is_last)
        {
            used_profile_save_new_page = true; //Refresh list ID when returning to this page later
        }

        delete_confirm_state = false;
        has_loading_failed   = false;
        has_deletion_failed  = false;
    }

    if ( (is_first) || (OverlayManager::Get().GetOverlayCount() == 0) )
        ImGui::PopItemDisabled();

    ImGui::SameLine();

    if ( (is_first) || (is_last) || (is_none) )
        ImGui::PushItemDisabled();

    if (delete_confirm_state)
    {
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDeleteConfirm), m_CachedSizes.Profiles_ButtonDeleteSize))
        {
            has_deletion_failed = !ConfigManager::Get().DeleteOverlayProfile(m_ProfileSelectionName + ".ini");

            if (!has_deletion_failed)
            {
                m_ProfileList = ConfigManager::Get().GetOverlayProfileList();
                list_id--;
                m_ProfileSelectionName = m_ProfileList[list_id];

                UIManager::Get()->RepeatFrame();
            }

            has_loading_failed   = false;
            delete_confirm_state = false;
        }
    }
    else
    {
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDelete), m_CachedSizes.Profiles_ButtonDeleteSize))
        {
            delete_confirm_state = true;
        }
    }

    if ( (is_first) || (is_last) || (is_none) )
        ImGui::PopItemDisabled();

    ImGui::EndGroup();

    list_buttons_width = ImGui::GetItemRectSize().x + style.IndentSpacing;

    ImGui::Unindent();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons (don't show when used as root page)
    if (!is_root_page)
    {
        ImGui::Separator();

        if (ImGui::Button(TranslationManager::GetString(tstr_DialogDone))) 
        {
            PageGoBack();
        }

        if ( (has_loading_failed) || (has_deletion_failed) )
        {
            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextError);
            ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString((has_loading_failed) ? tstr_SettingsProfilesOverlaysProfileFailedLoad : tstr_SettingsProfilesOverlaysProfileFailedDelete));
            ImGui::PopStyleColor();
        }
    }
}

void WindowSettings::UpdatePageProfilesOverlaySelect()
{
    ImGuiStyle& style = ImGui::GetStyle();

    bool skip_selection = false;
    static std::vector<std::wstring> list_profiles_w;
    static std::vector< std::pair<std::string, OverlayOrigin> > list_overlays;
    static std::vector<char> list_overlays_ticked;
    static char buffer_profile_name[256] = "";
    static bool pending_input_focus      = false;
    static int appearing_framecount      = 0;
    static bool is_any_ticked            = true;
    static bool is_name_readonly         = true;
    static bool is_name_taken            = false;
    static bool is_name_blank            = false;
    static bool has_saving_failed        = false;
    static float list_buttons_width      = 0.0f;

    auto check_profile_name_taken = [](const wchar_t* profile_name) 
                                    { 
                                       auto it = std::find_if(list_profiles_w.begin(), list_profiles_w.end(), 
                                                              [&profile_name](const auto& list_entry){ return (::StrCmpIW(profile_name, list_entry.c_str()) == 0); });
                                    
                                       return (it != list_profiles_w.end());
                                    };
    
    if (m_PageAppearing == wndsettings_page_profiles_overlay_select)
    {
        appearing_framecount = ImGui::GetFrameCount();

        //Load overlay list
        if (m_ProfileOverlaySelectIsSaving)
        {
            //We also keep a list of profile names as wide strings to allow for case-insensitive comparisions through the WinAPI
            list_profiles_w.clear();
            for (const auto& name : m_ProfileList)
            {
                list_profiles_w.push_back(WStringConvertFromUTF8(name.c_str()));
            }

            list_overlays.clear();
            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);
                list_overlays.push_back( std::make_pair(data.ConfigNameStr, (OverlayOrigin)data.ConfigInt[configid_int_overlay_origin]) );
            }

            //Only do this part if the page is appearing from a page change, not window appearing again
            if (!m_IsWindowAppearing)
            {
                is_name_readonly = !m_ProfileSelectionName.empty();

                //Generate name if empty
                if (m_ProfileSelectionName.empty())
                {
                    std::wstring new_profile_name_w_base = WStringConvertFromUTF8(TranslationManager::GetString(tstr_SettingsProfilesOverlaysNameNewBase));
                    std::wstring new_profile_name_w;
                    int i = 0;

                    //Sanity check translation string for ID placeholder to avoid infinite loop
                    if (new_profile_name_w_base.find(L"%ID%") == std::wstring::npos)
                    {
                        new_profile_name_w_base = L"Profile %ID%";
                    }

                    //Default to higher profile number if normal is already taken
                    auto it = list_profiles_w.end();

                    do
                    {
                        ++i;
                        new_profile_name_w = new_profile_name_w_base;
                        WStringReplaceAll(new_profile_name_w, L"%ID%", std::to_wstring(i));
                    }
                    while ( check_profile_name_taken(new_profile_name_w.c_str()) );

                    m_ProfileSelectionName = StringConvertFromUTF16(new_profile_name_w.c_str());
                }

                //Set profile name buffer
                size_t copied_length = m_ProfileSelectionName.copy(buffer_profile_name, IM_ARRAYSIZE(buffer_profile_name) - 1);
                buffer_profile_name[copied_length] = '\0';

                //Focus InputText once as soon as we can (needs to be not clipped)
                pending_input_focus = !is_name_readonly;
            }
        }
        else
        {
            list_overlays = ConfigManager::Get().GetOverlayProfileOverlayNameList(m_ProfileSelectionName + ".ini");

            //Skip selection if there's only one overlay to choose from anyways
            if (list_overlays.size() == 1)
            {
                skip_selection = true;
            }
            else
            {
                //Check overlay list for names with unmapped characters
                for (const auto& pair : list_overlays)
                {
                    UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(pair.first.c_str());
                }
            }
        }

        //Only clear if page changed
        if (!m_IsWindowAppearing)
        {
            list_overlays_ticked.clear();
        }

        list_overlays_ticked.resize(list_overlays.size(), 1);

        //If we're preserving the selections we need to check ticked state
        if (m_IsWindowAppearing)
        {
            is_any_ticked = false;
            for (auto is_ticked : list_overlays_ticked)
            {
                if (is_ticked != 0)
                {
                    is_any_ticked = true;
                    break;
                }
            }
        }
        else
        {
            is_any_ticked = !list_overlays.empty();
        }

        is_name_taken = false;
        is_name_blank = false;
        has_saving_failed = false;

        UIManager::Get()->RepeatFrame();
    }

    if (m_ProfileOverlaySelectIsSaving)
    {
        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileSaveSelectHeader));
        ImGui::Columns(2, "ColumnProfileName", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileSaveSelectName));

        if (is_name_taken)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileSaveSelectNameErrorTaken), "(!)");
        }
        else if (is_name_blank)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileSaveSelectNameErrorBlank), "(!)");
        }

        ImGui::NextColumn();

        VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

        if (is_name_readonly)
            ImGui::PushItemDisabled();

        ImGui::PushItemWidth(-1.0f);
        ImGui::PushID(appearing_framecount);  //The idea is to have ImGui treat this as a new widget every time the page is opened, so the cursor position isn't remembered between page switches
        vr_keyboard.VRKeyboardInputBegin("##InputProfileName");
        if (ImGui::InputText("##InputProfileName", buffer_profile_name, IM_ARRAYSIZE(buffer_profile_name), ImGuiInputTextFlags_CallbackCharFilter,
                                                                                                           [](ImGuiInputTextCallbackData* data)
                                                                                                           {
                                                                                                               return (int)IsWCharInvalidForFileName(data->EventChar);
                                                                                                           }
           ))
        {
            std::wstring wstr = WStringConvertFromUTF8(buffer_profile_name);
            is_name_taken = check_profile_name_taken(wstr.c_str());
            is_name_blank = wstr.empty();

            //Check input for unmapped character
            //This isn't ideal as it'll collect builder strings over time, but assuming there won't be too many of those in a session it's alright
            UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(buffer_profile_name);
        }
        vr_keyboard.VRKeyboardInputEnd();
        ImGui::PopID();

        if ( (pending_input_focus) && (ImGui::IsItemVisible()) && (m_PageAnimationDir == 0) )
        {
            ImGui::SetKeyboardFocusHere(-1);
            pending_input_focus = false;
        }

        if (is_name_readonly)
            ImGui::PopItemDisabled();

        ImGui::Columns(1);
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), 
                                  TranslationManager::GetString((m_ProfileOverlaySelectIsSaving) ? tstr_SettingsProfilesOverlaysProfileSaveSelectHeaderList : 
                                                                                                   tstr_SettingsProfilesOverlaysProfileAddSelectHeader       ) ); 
    ImGui::Indent();

    //Shouldn't really happen, but display error if there are no overlays
    if (list_overlays.size() == 0)
    {
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileAddSelectEmpty));
    }
    else
    {
        ImGui::SetNextItemWidth(-1.0f);
        const float item_height = ImGui::GetFrameHeight() + style.ItemSpacing.y;
        const float inner_padding = style.FramePadding.y + style.ItemInnerSpacing.y;
        float item_count = (m_ProfileOverlaySelectIsSaving) ? 12.0f : 14.0f;

        if (UIManager::Get()->IsInDesktopMode())
        {
            item_count +=  2.0f;
        }

        ImGui::BeginChild("OverlayList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

        //Reset scroll when appearing
        if (m_PageAppearing == wndsettings_page_profiles_overlay_select)
        {
            ImGui::SetScrollY(0.0f);
        }

        //List overlays
        const float cursor_x_past_checkbox = ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing();
        ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
        ImVec2 img_size, img_uv_min, img_uv_max;

        static unsigned int hovered_overlay_id_last = k_ulOverlayID_None;
        unsigned int hovered_overlay_id = k_ulOverlayID_None;
        
        int index = 0;
        for (const auto& pair : list_overlays)
        {
            ImGui::PushID(index);

            //We're using a trick here to extend the checkbox interaction space to the end of the child window
            //Checkbox() uses the item inner spacing if the label is not blank, so we increase that and use a space label
            //Below we render a custom label with icon in front of it after adjusting the cursor position to where it normally would be
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {ImGui::GetContentRegionAvail().x, style.ItemInnerSpacing.y});

            if (ImGui::Checkbox(" ", (bool*)&list_overlays_ticked[index]))
            {
                //Update any ticked status
                is_any_ticked = false;
                for (auto is_ticked : list_overlays_ticked)
                {
                    if (is_ticked != 0)
                    {
                        is_any_ticked = true;
                        break;
                    }
                }
            }

            ImGui::PopStyleVar();

            if (ImGui::IsItemVisible())
            {
                if ( (m_ProfileOverlaySelectIsSaving) && (ImGui::IsItemHovered()) )
                {
                    hovered_overlay_id = (unsigned int)index;
                }

                //Adjust cursor position to be after the checkbox + offset for icon
                ImGui::SameLine();
                float text_y = ImGui::GetCursorPosY();
                ImGui::SetCursorPos({cursor_x_past_checkbox, text_y + style.FramePadding.y});

                //Origin icon
                TextureManager::Get().GetTextureInfo((TMNGRTexID)(tmtex_icon_xsmall_origin_room + pair.second), img_size, img_uv_min, img_uv_max);
                ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

                //Checkbox label
                ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                ImGui::SetCursorPosY(text_y);

                ImGui::TextUnformatted(pair.first.c_str());
            }

            ImGui::PopID();

            index++;
        }

        ImGui::EndChild();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - list_buttons_width);

        ImGui::BeginGroup();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileAddSelectAll)))
        {
            std::fill(list_overlays_ticked.begin(), list_overlays_ticked.end(), 1);
            is_any_ticked = true;
        }

        ImGui::SameLine();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileAddSelectNone)))
        {
            std::fill(list_overlays_ticked.begin(), list_overlays_ticked.end(), 0);
            is_any_ticked = false;
        }
        ImGui::EndGroup();

        list_buttons_width = ImGui::GetItemRectSize().x + style.IndentSpacing;

        //Set overlay highlight if saving
        if ( (m_ProfileOverlaySelectIsSaving) && (hovered_overlay_id_last != hovered_overlay_id) )
        {
            UIManager::Get()->HighlightOverlay(hovered_overlay_id);
            hovered_overlay_id_last = hovered_overlay_id;
        }
    }

    ImGui::Unindent();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons
    ImGui::Separator();

    const bool disable_confirm_button = ((!is_any_ticked) || (is_name_taken) || (is_name_blank));

    if (disable_confirm_button)
        ImGui::PushItemDisabled();

    if (m_ProfileOverlaySelectIsSaving)
    {
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileSaveSelectDo)))   //Save profile
        {
            m_ProfileSelectionName = buffer_profile_name;
            has_saving_failed = !ConfigManager::Get().SaveMultiOverlayProfileToFile(m_ProfileSelectionName + ".ini", &list_overlays_ticked);

            if (!has_saving_failed)
            {
                m_ProfileList = ConfigManager::Get().GetOverlayProfileList();
                PageGoBack();
            }
        }
    }
    else
    {
        if ( (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileAddSelectDo))) || (skip_selection) )  //Add selected overlays from profile
        {
            ConfigManager::Get().LoadMultiOverlayProfileFromFile(m_ProfileSelectionName + ".ini", false, &list_overlays_ticked);

            //Tell dashboard app to load the profile as well
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_profile_name_load, m_ProfileSelectionName, UIManager::Get()->GetWindowHandle());

            for (int i = 0; i < list_overlays_ticked.size(); ++i)  //Send each ticked overlay's ID to queue it up
            {
                if (list_overlays_ticked[i] == 1)
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_profile_load, MAKELPARAM(ipcactv_ovrl_profile_multi_add, i) );
                }
            }

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_profile_load, MAKELPARAM(ipcactv_ovrl_profile_multi_add, -1) );       //Add queued overlays

            (skip_selection) ? PageGoBackInstantly() : PageGoBack();
            UIManager::Get()->RepeatFrame();
        }
    }

    if (disable_confirm_button)
        ImGui::PopItemDisabled();

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }

    if (has_saving_failed)
    {
        ImGui::SameLine();

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextError);
        ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileSaveSelectDoFailed));
        ImGui::PopStyleColor();
    }
}

void WindowSettings::UpdatePageAppProfiles()
{
    ImGuiStyle& style = ImGui::GetStyle();
    AppProfileManager& app_profiles = ConfigManager::Get().GetAppProfileManager();

    static int list_id = 0;
    static AppProfile app_profile_selected_edit;        //Temporary copy of the selected profile for editing
    static bool is_action_picker_for_leave = false;
    static bool delete_confirm_state       = false;
    static bool delete_disabled            = false;     //No actually reason to do this but to avoid user confusion from allowing to delete the same profile over and over
    static float delete_button_width       = 0.0f;
    static float active_header_text_width  = 0.0f;
    static ImVec2 no_apps_text_size;
    const bool is_root_page = (m_PageStack[0] == wndsettings_page_app_profiles);

    if (m_PageAppearing == wndsettings_page_app_profiles)
    {
        RefreshAppList();

        list_id = 0;
        app_profile_selected_edit = app_profiles.GetProfile((m_AppList.empty()) ? "" : m_AppList[0].first);
        delete_confirm_state = false;
        delete_disabled = false;

        UIManager::Get()->RepeatFrame();
    }
    
    if (list_id >= m_AppList.size())
    {
        list_id = (m_AppList.empty()) ? -1 : 0;
        app_profile_selected_edit = {};
    }

    if ((m_PageAppearing == wndsettings_page_app_profiles) || (m_CachedSizes.Profiles_ButtonDeleteSize.x == 0.0f))
    {
        //Figure out size for delete button. We need it to stay the same but also consider the case of the confirm text being longer in some languages
        ImVec2 text_size_delete  = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDelete),        nullptr, true);
        ImVec2 text_size_confirm = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDeleteConfirm), nullptr, true);
        m_CachedSizes.Profiles_ButtonDeleteSize = (text_size_delete.x > text_size_confirm.x) ? text_size_delete : text_size_confirm;

        m_CachedSizes.Profiles_ButtonDeleteSize.x += style.FramePadding.x * 2.0f;
        m_CachedSizes.Profiles_ButtonDeleteSize.y += style.FramePadding.y * 2.0f;

        UIManager::Get()->RepeatFrame();
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsProfilesAppsHeader) ); 

    if (!UIManager::Get()->IsOpenVRLoaded())
    {
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsProfilesAppsHeaderNoVRTip));
    }

    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? ( (is_root_page) ? 16.0f : 15.0f ) : 11.0f;
    ImGui::BeginChild("AppList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //Reset scroll when appearing
    if (m_PageAppearing == wndsettings_page_app_profiles)
    {
        ImGui::SetScrollY(0.0f);
    }

    bool focus_app_section = false;

    //List applications
    const bool is_any_app_profile_active = !app_profiles.GetActiveProfileAppKey().empty();
    const AppProfile& app_profile_active = app_profiles.GetProfile(app_profiles.GetActiveProfileAppKey());

    //Use list clipper to minimize GetProfile() lookups
    ImGuiListClipper list_clipper;
    list_clipper.Begin((int)m_AppList.size());
    while (list_clipper.Step())
    {
        for (int i = list_clipper.DisplayStart; i < list_clipper.DisplayEnd; ++i)
        {
            const AppProfile& app_profile = app_profiles.GetProfile(m_AppList[i].first);
            const bool is_active_profile = ((is_any_app_profile_active) && (&app_profile == &app_profile_active));

            if (!app_profile.IsEnabled)
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

            if (is_active_profile)
                ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextNotification);

            ImGui::PushID(i);
            if (ImGui::Selectable(m_AppList[i].second.c_str(), (i == list_id)))
            {
                list_id = i;
                app_profile_selected_edit = app_profile;

                delete_confirm_state = false;
                delete_disabled = !app_profiles.ProfileExists(m_AppList[i].first);
                focus_app_section = ImGui::GetIO().NavVisible;
            }
            ImGui::PopID();

            if (is_active_profile)
                ImGui::PopStyleColor();

            if (!app_profile.IsEnabled)
                ImGui::PopStyleVar();
        }
    }

    if (m_AppList.empty())
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x / 2.0f - (no_apps_text_size.x / 2.0f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y / 2.0f - (no_apps_text_size.y / 2.0f));

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesAppsListEmpty));
        no_apps_text_size = ImGui::GetItemRectSize();
    }

    ImGui::EndChild();
    ImGui::Unindent();

    const bool is_none  = (list_id == -1); //Would be rare, as we don't allow no selection unless the app list is really empty, but still handle that
    const bool is_first = (list_id == 0);
    const bool is_last  = (list_id == m_AppList.size() - 1);

    if (!is_none)
    {
        const std::string& app_key_selected  = m_AppList[list_id].first;
        const std::string& app_name_selected = m_AppList[list_id].second;
        const bool is_active_profile = ((is_any_app_profile_active) && (app_key_selected == app_profiles.GetActiveProfileAppKey()));

        static bool store_profile_changes = false;

        if (m_PageReturned == wndsettings_page_profile_picker)
        {
            store_profile_changes = (app_profile_selected_edit.OverlayProfileFileName != m_ProfilePickerName);
            app_profile_selected_edit.OverlayProfileFileName = m_ProfilePickerName;

            m_PageReturned = wndsettings_page_none;
        }
        else if (m_PageReturned == wndsettings_page_action_picker)
        {
            if (is_action_picker_for_leave)
            {
                store_profile_changes = (app_profile_selected_edit.ActionUIDLeave != m_ActionPickerUID);
                app_profile_selected_edit.ActionUIDLeave = m_ActionPickerUID;
            }
            else
            {
                store_profile_changes = (app_profile_selected_edit.ActionUIDEnter != m_ActionPickerUID);
                app_profile_selected_edit.ActionUIDEnter = m_ActionPickerUID;
            }

            m_PageReturned = wndsettings_page_none;
        }

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), app_name_selected.c_str());

        if (is_active_profile)
        {
            ImGui::SameLine();
            ImGui::TextColoredUnformatted(Style_ImGuiCol_TextNotification, TranslationManager::GetString(tstr_SettingsProfilesAppsProfileHeaderActive));

            active_header_text_width = ImGui::GetItemRectSize().x;
        }

        if (focus_app_section)
        {
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::Indent();
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsProfilesAppsProfileEnabled), &app_profile_selected_edit.IsEnabled))
        {
            store_profile_changes = true;
        }
        ImGui::Unindent();

        ImGui::Columns(2, "ColumnAppProfile", false);
        ImGui::SetColumnWidth(0, m_Column0Width);
        
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesAppsProfileOverlayProfile));
        ImGui::NextColumn();

        const char* overlay_profile_button_label = (app_profile_selected_edit.OverlayProfileFileName.empty()) ? TranslationManager::GetString(tstr_DialogProfilePickerNone) : 
                                                                                                                app_profile_selected_edit.OverlayProfileFileName.c_str();

        ImGui::PushID("ButtonOverlayProfile");
        if (ImGui::Button(overlay_profile_button_label))
        {
            m_ProfilePickerName = app_profile_selected_edit.OverlayProfileFileName;
            PageGoForward(wndsettings_page_profile_picker);
        }
        ImGui::PopID();

        ImGui::NextColumn();

        const ActionManager& action_manager = ConfigManager::Get().GetActionManager();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesAppsProfileActionEnter));
        ImGui::NextColumn();

        ImGui::PushID("ButtonActionEnter");
        if (ImGui::Button(action_manager.GetTranslatedName(app_profile_selected_edit.ActionUIDEnter)))
        {
            m_ActionPickerUID = app_profile_selected_edit.ActionUIDEnter;
            is_action_picker_for_leave = false;
            PageGoForward(wndsettings_page_action_picker);
        }
        ImGui::PopID();

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesAppsProfileActionLeave));
        ImGui::NextColumn();

        ImGui::PushID("ButtonActionLeave");
        if (ImGui::Button(action_manager.GetTranslatedName(app_profile_selected_edit.ActionUIDLeave)))
        {
            m_ActionPickerUID = app_profile_selected_edit.ActionUIDLeave;
            is_action_picker_for_leave = true;
            PageGoForward(wndsettings_page_action_picker);
        }
        ImGui::PopID();

        ImGui::Columns(1);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - delete_button_width);

        const bool delete_disabled_prev = delete_disabled;
        if (delete_disabled_prev)
            ImGui::PushItemDisabled();

        if (delete_confirm_state)
        {
            if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDeleteConfirm), m_CachedSizes.Profiles_ButtonDeleteSize))
            {
                IPCManager::Get().SendStringToDashboardApp(configid_str_state_app_profile_key, app_key_selected, UIManager::Get()->GetWindowHandle());
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_app_profile_remove);

                app_profiles.RemoveProfile(app_key_selected);
                app_profile_selected_edit = {};

                delete_confirm_state = false;
                delete_disabled = true;
            }
        }
        else
        {
            if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDelete), m_CachedSizes.Profiles_ButtonDeleteSize))
            {
                delete_confirm_state = true;
            }
        }

        if (delete_disabled_prev)
            ImGui::PopItemDisabled();

        delete_button_width = ImGui::GetItemRectSize().x + style.IndentSpacing;

        //Store and sync the profile if flag was set
        if (store_profile_changes)
        {
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_app_profile_key,  app_key_selected,                      UIManager::Get()->GetWindowHandle());
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_app_profile_data, app_profile_selected_edit.Serialize(), UIManager::Get()->GetWindowHandle());

            const bool loaded_overlay_profile = app_profiles.StoreProfile(app_key_selected, app_profile_selected_edit);

            if (loaded_overlay_profile)
            {
                UIManager::Get()->OnProfileLoaded();
            }

            store_profile_changes = false;
            delete_disabled = false;
        }
    }

    //Confirmation buttons (don't show when used as root page)
    if (!is_root_page)
    {
        ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );
        ImGui::Separator();

        if (ImGui::Button(TranslationManager::GetString(tstr_DialogDone))) 
        {
            PageGoBack();
        }
    }
}

void WindowSettings::UpdatePageActions()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();
    ActionManager& action_manager = ConfigManager::Get().GetActionManager();

    static int list_id = -1;
    static bool delete_confirm_state  = false;
    static bool scroll_to_selection   = false;
    static int keyboard_swapped_index = -1;
    static ActionUID hovered_action   = k_ActionUID_Invalid;
    static float list_buttons_width   = 0.0f;
    static ImVec2 no_actions_text_size;

    const bool is_root_page = (m_PageStack[0] == wndsettings_page_actions);

    if (m_PageAppearing == wndsettings_page_actions)
    {
        //Load action list
        m_ActionList = ConfigManager::Get().GetActionManager().GetActionNameList();
        list_id = -1;
        delete_confirm_state = false;

        UIManager::Get()->RepeatFrame();
    }
    else if (m_PageReturned == wndsettings_page_actions_edit)
    {
        m_ActionList = ConfigManager::Get().GetActionManager().GetActionNameList();

        //Find potentially new action in the list and select it
        auto it = std::find_if(m_ActionList.begin(), m_ActionList.end(), [&](const auto& list_entry) { return (list_entry.UID == m_ActionSelectionUID); } );
        list_id = (it != m_ActionList.end()) ? (int)std::distance(m_ActionList.begin(), it) : -1;

        m_PageReturned = wndsettings_page_none;
    }

    if ((m_PageAppearing == wndsettings_page_actions) || (m_CachedSizes.Actions_ButtonDeleteSize.x == 0.0f))
    {
        //Figure out size for delete button. We need it to stay the same but also consider the case of the confirm text being longer in some languages
        ImVec2 text_size_delete  = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsActionsManageDelete),        nullptr, true);
        ImVec2 text_size_confirm = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsActionsManageDeleteConfirm), nullptr, true);
        m_CachedSizes.Actions_ButtonDeleteSize = (text_size_delete.x > text_size_confirm.x) ? text_size_delete : text_size_confirm;

        m_CachedSizes.Actions_ButtonDeleteSize.x += style.FramePadding.x * 2.0f;
        m_CachedSizes.Actions_ButtonDeleteSize.y += style.FramePadding.y * 2.0f;

        UIManager::Get()->RepeatFrame();
    }

    bool focus_edit_button = false;

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsActionsManageHeader)); 
    ImGui::Indent();

    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? ( (is_root_page) ? 22.0f : 20.0f ) : 16.0f;
    ImGui::BeginChild("ActionList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //Display error if there are no actions
    if (m_ActionList.empty())
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x / 2.0f - (no_actions_text_size.x / 2.0f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y / 2.0f - (no_actions_text_size.y / 2.0f));

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_DialogActionPickerEmpty));
        no_actions_text_size = ImGui::GetItemRectSize();
    }
    else
    {
        //List actions
        int index = 0;
        for (const auto& entry : m_ActionList)
        {
            ImGui::PushID((void*)entry.UID);

            //Set focus for nav if we previously re-ordered overlays via keyboard
            if (keyboard_swapped_index == index)
            {
                ImGui::SetKeyboardFocusHere();

                //Nav works against us here, so keep setting focus until ctrl isn't down anymore
                if ((!io.KeyCtrl) || (!io.NavVisible))
                {
                    keyboard_swapped_index = -1;
                }
            }

            if (ImGui::Selectable(entry.Name.c_str(), (index == list_id)))
            {
                list_id = index;
                m_ActionSelectionUID = entry.UID;

                delete_confirm_state = false;
                focus_edit_button = io.NavVisible;
            }

            if ( (scroll_to_selection) && (index == list_id) )
            {
                ImGui::SetScrollHereY();

                if (ImGui::IsItemVisible())
                {
                    scroll_to_selection = false;
                }
            }

            if (ImGui::IsItemHovered())
            {
                hovered_action = entry.UID;
            }

            if (ImGui::IsItemVisible())
            {
                //Additional selectable behavior
                bool selectable_active = ImGui::IsItemActive();

                if ( (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) || ((io.NavVisible) && (ImGui::IsItemFocused())) )
                {
                    hovered_action = entry.UID;
                }

                if ((ImGui::IsItemClicked()) && (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)))
                {
                    PageGoForward(wndsettings_page_actions_edit);

                    delete_confirm_state = false;
                }

                //Drag reordering
                if ((ImGui::IsItemActive()) && (!ImGui::IsItemHovered()))
                {
                    int index_swap = index + ((ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y < 0.0f) ? -1 : 1);
                    if ((hovered_action != entry.UID) && (index_swap >= 0) && (index_swap < m_ActionList.size()))
                    {
                        std::iter_swap(m_ActionList.begin() + index, m_ActionList.begin() + index_swap);

                        ActionManager::ActionList ui_order = action_manager.GetActionOrderListUI();
                        std::iter_swap(ui_order.begin() + index, ui_order.begin() + index_swap);
                        action_manager.SetActionOrderListUI(ui_order);

                        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                    }
                }

                //Keyboard reordering
                if ((io.NavVisible) && (io.KeyCtrl) && (hovered_action == entry.UID))
                {
                    int index_swap = index + ((ImGui::IsNavInputPressed(ImGuiNavInput_DpadDown, true)) ? 1 : (ImGui::IsNavInputPressed(ImGuiNavInput_DpadUp, true)) ? -1 : 0);
                    if ((index != index_swap) && (index_swap >= 0) && (index_swap < m_ActionList.size()))
                    {
                        std::iter_swap(m_ActionList.begin() + index, m_ActionList.begin() + index_swap);

                        ActionManager::ActionList ui_order = action_manager.GetActionOrderListUI();
                        std::iter_swap(ui_order.begin() + index, ui_order.begin() + index_swap);
                        action_manager.SetActionOrderListUI(ui_order);

                        //Skip the rest of this frame to avoid double-swaps
                        keyboard_swapped_index = index_swap;
                        ImGui::PopID();
                        UIManager::Get()->RepeatFrame();
                        break;
                    }
                }
            }

            ImGui::PopID();

            index++;
        }
    }

    ImGui::EndChild();

    const bool is_none  = (list_id == -1);
    const bool is_first = (list_id == 0);
    const bool is_last  = (list_id == m_ProfileList.size() - 1);

    ImGui::Indent();

    if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
    {
        if (is_none)
            ImGui::PushItemDisabled();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsManageCopyUID)))
        {
            ImGui::SetClipboardText(std::to_string(m_ActionList[list_id].UID).c_str());
            delete_confirm_state = false;
        }

        if (is_none)
            ImGui::PopItemDisabled();

        ImGui::SameLine();
    }

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - list_buttons_width);

    ImGui::BeginGroup();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsManageNew)))
    {
        m_ActionSelectionUID = 0;
        PageGoForward(wndsettings_page_actions_edit);

        delete_confirm_state = false;
    }

    ImGui::SameLine();

    if (is_none)
        ImGui::PushItemDisabled();

    if (focus_edit_button)
        ImGui::SetKeyboardFocusHere();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsManageEdit)))
    {
        PageGoForward(wndsettings_page_actions_edit);

        delete_confirm_state = false;
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsManageDuplicate)))
    {
        ActionUID dup_uid = action_manager.DuplicateAction(action_manager.GetAction(m_ActionSelectionUID));

        //Sync with dashboard app
        IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_data, action_manager.GetAction(dup_uid).Serialize(), UIManager::Get()->GetWindowHandle());

        //Refresh list
        m_ActionList = action_manager.GetActionNameList();
        list_id++;
        m_ActionSelectionUID = m_ActionList[list_id].UID;

        UIManager::Get()->RepeatFrame();

        delete_confirm_state = false;
    }

    ImGui::SameLine();

    if (delete_confirm_state)
    {
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsManageDeleteConfirm), m_CachedSizes.Actions_ButtonDeleteSize))
        {
            action_manager.RemoveAction(m_ActionSelectionUID);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_delete, m_ActionSelectionUID);

            m_ActionList = action_manager.GetActionNameList();
            list_id--;
            m_ActionSelectionUID = (list_id != -1) ? m_ActionList[list_id].UID : k_ActionUID_Invalid;

            UIManager::Get()->RepeatFrame();

            delete_confirm_state = false;
        }
    }
    else
    {
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsManageDelete), m_CachedSizes.Actions_ButtonDeleteSize))
        {
            delete_confirm_state = true;
        }
    }

    if (is_none)
        ImGui::PopItemDisabled();

    ImGui::EndGroup();

    list_buttons_width = ImGui::GetItemRectSize().x + style.IndentSpacing;

    ImGui::Unindent();
    ImGui::Unindent();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons (don't show when used as root page)
    if (!is_root_page)
    {
        ImGui::Separator();

        if (ImGui::Button(TranslationManager::GetString(tstr_DialogDone))) 
        {
            PageGoBack();
        }
    }
}

void WindowSettings::UpdatePageActionsEdit(bool only_restore_settings)
{
    struct CommandUIState
    {
        float header_animation_progress = 0.0f;
        float header_2_animation_progress = 0.0f;
        std::string header_label;
        char buffer_str_main[1024] = "";
        char buffer_str_arg[1024]  = "";
        int temp_int_1 = 0;
        int temp_int_2 = 0;
        std::string temp_window_button_title;
        FloatingWindowInputOverlayTagsState input_tags_state;
    };

    auto filter_newline_limit = [](ImGuiInputTextCallbackData* data)
    {
        static int newline_count = 0;
        const int newline_max = 2;

        if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
        {
            //Iterate over buffer and count newlines
            newline_count = 0;
            for (char* ibuf = data->Buf, *ibuf_end = data->Buf + data->BufTextLen; ibuf != ibuf_end; ++ibuf)
            {
                if (*ibuf == '\n')
                {
                    //Replace newlines that occur after counting to max with space character
                    if (newline_count >= newline_max)
                    {
                        *ibuf = ' ';
                        data->BufDirty = true;
                    }
                    else
                    {
                        ++newline_count;
                    }
                }
            }
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) //Filter newlines during input if over limit
        {
            return (int)((data->EventChar == '\n') && (newline_count >= newline_max));
        }

        return 0;
    };

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();
    ActionManager& action_manager = ConfigManager::Get().GetActionManager();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    static Action action_edit;                           //Temporary copy of the selected action for editing
    static FloatingWindowInputOverlayTagsState input_tags_state;
    static std::vector<CommandUIState> command_ui_states;
    static char buffer_action_name[256]         = "";
    static char buffer_action_label[1024]       = "";
    static char buffer_action_target_tags[1024] = "";
    static bool label_matches_name              = true;
    static int appearing_framecount             = 0;
    static float area_tags_animation_progress   = 0.0f;
    static float tags_widget_height             = 0.0f;
    static bool action_test_was_used            = false;
    static bool delete_confirm_state            = false;
    static float delete_button_width            = 0.0f;

    if (only_restore_settings)
    {
        //Restore action to old state for dashboard app if it was sent over for testing
        if (action_test_was_used)
        {
            if (action_manager.ActionExists(action_edit.UID))
            {
                IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_data, action_edit.Serialize(), UIManager::Get()->GetWindowHandle());
            }
            else    //It shouldn't exist, so delete it on the other end
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_delete, action_edit.UID);
            }
        }

        return;
    }

    bool reload_icon = false;

    if (m_PageAppearing == wndsettings_page_actions_edit)
    {
        appearing_framecount = ImGui::GetFrameCount();
        command_ui_states.clear();
        action_test_was_used = false;
        delete_confirm_state = false;
        reload_icon = true;
        input_tags_state.PopupShow = false;

        //Make sure profile list is ready
        m_ProfileList = ConfigManager::Get().GetOverlayProfileList();

        //Create from scratch if selected UID is 0
        if (m_ActionSelectionUID == k_ActionUID_Invalid)
        {
            action_edit = Action();
            action_edit.UID = action_manager.GenerateUID();
            action_edit.Name  = TranslationManager::GetString(tstr_SettingsActionsEditNameNew);
            action_edit.Label = action_edit.Name;

            m_ActionSelectionUID = action_edit.UID;
        }
        else
        {
            action_edit = action_manager.GetAction(m_ActionSelectionUID);
        }

        //Fill buffers from action data
        size_t copied_length = action_edit.Name.copy(buffer_action_name, IM_ARRAYSIZE(buffer_action_name) - 1);
        buffer_action_name[copied_length] = '\0';
        copied_length = action_edit.Label.copy(buffer_action_label, IM_ARRAYSIZE(buffer_action_label) - 1);
        buffer_action_label[copied_length] = '\0';
        copied_length = action_edit.TargetTags.copy(buffer_action_target_tags, IM_ARRAYSIZE(buffer_action_target_tags) - 1);
        buffer_action_target_tags[copied_length] = '\0';

        label_matches_name = (action_edit.Label == action_edit.Name);
        area_tags_animation_progress = (action_edit.TargetUseTags) ? 1.0f : 0.0f;

        UIManager::Get()->RepeatFrame();
    }

    if ((m_PageAppearing == wndsettings_page_actions_edit) || (m_CachedSizes.ActionEdit_ButtonDeleteSize.x == 0.0f))
    {
        //Figure out size for delete button. We need it to stay the same but also consider the case of the confirm text being longer in some languages
        ImVec2 text_size_delete  = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsActionsEditCommandDelete),        nullptr, true);
        ImVec2 text_size_confirm = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsActionsEditCommandDeleteConfirm), nullptr, true);
        m_CachedSizes.ActionEdit_ButtonDeleteSize = (text_size_delete.x > text_size_confirm.x) ? text_size_delete : text_size_confirm;

        m_CachedSizes.ActionEdit_ButtonDeleteSize.x += style.FramePadding.x * 2.0f;
        m_CachedSizes.ActionEdit_ButtonDeleteSize.y += style.FramePadding.y * 2.0f;

        UIManager::Get()->RepeatFrame();
    }

    if (m_PageReturned == wndsettings_page_icon_picker)
    {
        action_edit.IconFilename = m_IconPickerFile;
        reload_icon = true;

        m_PageReturned = wndsettings_page_none;
    }

    if ((reload_icon) && (!action_edit.IconFilename.empty()))
    {
        std::string icon_path = "images/icons/" + action_edit.IconFilename;
        std::wstring icon_path_wstr = WStringConvertFromUTF8(icon_path.c_str());

        //Avoid reloading if the path is already the same
        if (wcscmp(icon_path_wstr.c_str(), TextureManager::Get().GetTextureFilename(tmtex_icon_temp)) != 0)
        {
            TextureManager::Get().SetTextureFilenameIconTemp(icon_path_wstr.c_str());
            TextureManager::Get().ReloadAllTexturesLater();
        }

        UIManager::Get()->RepeatFrame();
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsActionsEditHeader)); 

    ImGui::Columns(2, "ColumnActionEditBase", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditName));

    //Add a tooltip when translation ID is used to minimize confusion about the name not matching what's displayed outside this page
    if (action_edit.NameTranslationID != tstr_NONE)
    {
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsActionsEditNameTranslatedTip));
    }

    ImGui::NextColumn();

    ImGui::PushItemWidth(-1.0f);
    ImGui::PushID(appearing_framecount);  //The idea is to have ImGui treat this as a new widget every time the page is opened, so the cursor position isn't remembered between page switches
    vr_keyboard.VRKeyboardInputBegin("##InputActionName");
    if (ImGui::InputText("##InputActionName", buffer_action_name, IM_ARRAYSIZE(buffer_action_name)))
    {
        UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(buffer_action_name);

        action_edit.Name = buffer_action_name;

        //As long as label matches name, adjust label alongside it
        if (label_matches_name)
        {
            action_edit.Label = action_edit.Name;

            size_t copied_length = action_edit.Label.copy(buffer_action_label, IM_ARRAYSIZE(buffer_action_label) - 1);
            buffer_action_label[copied_length] = '\0';

            action_edit.LabelTranslationID = action_manager.GetTranslationIDForName(action_edit.Label);
        }

        //Check for potential translation string
        action_edit.NameTranslationID = action_manager.GetTranslationIDForName(action_edit.Name);
    }
    vr_keyboard.VRKeyboardInputEnd();
    ImGui::PopID();

    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditTarget));
    ImGui::NextColumn();

    if (ImGui::RadioButton(TranslationManager::GetString(tstr_SettingsActionsEditTargetDefault), !action_edit.TargetUseTags))
    {
        action_edit.TargetUseTags = false;
    }
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    HelpMarker(TranslationManager::GetString(tstr_SettingsActionsEditTargetDefaultTip));

    ImGui::SameLine();

    if (ImGui::RadioButton(TranslationManager::GetString(tstr_SettingsActionsEditTargetUseTags), action_edit.TargetUseTags))
    {
        action_edit.TargetUseTags = true;
    }

    tags_widget_height = ImGui::GetCursorPosY();


    ImGui::BeginCollapsingArea("AreaTags", action_edit.TargetUseTags, area_tags_animation_progress);

    if (InputOverlayTags("TargetTags", buffer_action_target_tags, IM_ARRAYSIZE(buffer_action_target_tags), input_tags_state))
    {
        action_edit.TargetTags = buffer_action_target_tags;
    }

    ImGui::EndCollapsingArea();

    ImGui::Columns(1);

    tags_widget_height = ImGui::GetCursorPosY() - tags_widget_height;

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsActionsEditHeaderAppearance));

    //Use settings icon as default button size reference
    ImVec2 b_size_default, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size_default, b_uv_min, b_uv_max);

    //Adapt to the last known scale used in VR so the text alignment matches what's seen in the headset later
    if (UIManager::Get()->IsInDesktopMode())
    {
        b_size_default.x *= UIManager::Get()->GetUIScale();
        b_size_default.y *= UIManager::Get()->GetUIScale();
        b_size_default.x *= ConfigManager::GetValue(configid_float_interface_last_vr_ui_scale);
        b_size_default.y *= ConfigManager::GetValue(configid_float_interface_last_vr_ui_scale);
    }

    //It's a bit hacky, but we add the preview button last to avoid it changing line heights and shift the position of widgets around to leave space for it instead
    ImVec2 button_pos = ImGui::GetCursorPos();
    const float line_x = ImGui::GetCursorPosX() + b_size_default.x + style.IndentSpacing + style.IndentSpacing + style.ItemSpacing.x + style.ItemSpacing.x;

    ImGui::Columns(2, "ColumnActionEditAppearance", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    ImGui::SetCursorPosX(line_x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditIcon));
    ImGui::NextColumn();

    ImGui::PushID("##FileName");
    if (ImGui::Button((!action_edit.IconFilename.empty()) ? action_edit.IconFilename.c_str() : TranslationManager::GetString(tstr_DialogIconPickerNone)))
    {
        PageGoForward(wndsettings_page_icon_picker);
    }
    ImGui::PopID();

    ImGui::NextColumn();

    ImGui::SetCursorPosX(line_x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditLabel));

    if (action_edit.LabelTranslationID != tstr_NONE)
    {
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsActionsEditLabelTranslatedTip));
    }

    ImGui::NextColumn();

    ImGui::PushItemWidth(-1.0f);
    ImGui::PushID(appearing_framecount);
    vr_keyboard.VRKeyboardInputBegin("##InputActionLabel", true);
    ImVec2 multiline_input_size(-1, (ImGui::GetTextLineHeight() * 3.0f) + (style.FramePadding.y * 2.0f));
    if (ImGui::InputTextMultiline("##InputActionLabel", buffer_action_label, IM_ARRAYSIZE(buffer_action_label), multiline_input_size, 
                                  ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackEdit, filter_newline_limit))
    {
        UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(buffer_action_label);

        action_edit.Label = buffer_action_label;
        label_matches_name = (action_edit.Label == action_edit.Name);

        //Check for potential translation string
        action_edit.LabelTranslationID = action_manager.GetTranslationIDForName(action_edit.Label);
    }
    vr_keyboard.VRKeyboardInputEnd();
    ImGui::PopID();

    ImGui::Columns(1);

    const ImVec2 command_header_pos = ImGui::GetCursorPos();

    //Vertically center button
    button_pos.x += style.IndentSpacing;
    button_pos.y += (command_header_pos.y - button_pos.y) / 2.0f - (b_size_default.y / 2.0f);
    ImGui::SetCursorPos(button_pos);

    if (!UIManager::Get()->IsOpenVRLoaded())    //Disable when dashboard app isn't available
        ImGui::PushItemDisabledNoVisual();

    WindowFloatingUIActionBar::ButtonAction(action_edit, b_size_default, true);

    if (ImGui::IsItemActivated())
    {
        //Send action over so it can be used for testing
        IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_data, action_edit.Serialize(), UIManager::Get()->GetWindowHandle());

        action_manager.StartAction(action_edit.UID);

        //Make sure to either revert or get rid of the action if canceling the edit later
        action_test_was_used = true;
    }
    else if (ImGui::IsItemDeactivated())
    {
        action_manager.StopAction(action_edit.UID);
    }

    if (!UIManager::Get()->IsOpenVRLoaded())
        ImGui::PopItemDisabledNoVisual();

    ImGui::SetCursorPos(command_header_pos);

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsActionsEditHeaderCommands));

    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 13.5f : 9.5f;

    ImGui::BeginChild("CommandList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight - tags_widget_height), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    static int header_open = -1;
    static float header_new_appearance_progress = 1.0f;     //We animate newly created command headers appearing as pop-in is a bit grating and it's simple, but do nothing for removal
    static std::vector<int> list_unique_ids;
    static unsigned int drag_last_hovered_header = 0;
    static int keyboard_swapped_index            = -1;
    static bool is_dragging_header = false;

    command_ui_states.resize(action_edit.Commands.size());
    const float header_text_max_width = ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeightWithSpacing() - style.FramePadding.x;

    //Reset unique IDs when appearing
    if (m_PageAppearing == wndsettings_page_actions_edit)
    {
        list_unique_ids.clear();
    }

    //Expand unique id lists if commands were added (also does initialization since it's empty then)
    while (list_unique_ids.size() < action_edit.Commands.size())
    {
        list_unique_ids.push_back((int)list_unique_ids.size());
    }

    int command_id = 0;
    for (auto& command : action_edit.Commands)
    {
        CommandUIState& ui_state = command_ui_states[command_id];
        const bool animate_appearing = (header_new_appearance_progress < 1.0f) && (command_id == action_edit.Commands.size() - 1);

        if (ui_state.header_label.empty())
        {
            ui_state.header_label = ActionManager::GetCommandDescription(command, header_text_max_width) + "###CommandHeader";

            size_t copied_length = command.StrMain.copy(ui_state.buffer_str_main, IM_ARRAYSIZE(ui_state.buffer_str_main) - 1);
            ui_state.buffer_str_main[copied_length] = '\0';
            copied_length = command.StrArg.copy(ui_state.buffer_str_arg, IM_ARRAYSIZE(ui_state.buffer_str_arg) - 1);
            ui_state.buffer_str_arg[copied_length] = '\0';

            //Skip header animation if command has the value set already
            ui_state.header_2_animation_progress = ((command.Type == ActionCommand::command_show_overlay) && (LOWORD(command.UIntID) == 1)) ? 1.0f : 0.0f;
        }

        ImGui::PushID(list_unique_ids[command_id]);

        //Set focus for nav if we previously re-ordered overlays via keyboard
        if (keyboard_swapped_index == command_id)
        {
            ImGui::SetKeyboardFocusHere();

            //Nav works against us here, so keep setting focus until ctrl isn't down anymore
            if ((!io.KeyCtrl) || (!io.NavVisible))
            {
                keyboard_swapped_index = -1;
            }
        }

        if (animate_appearing)
            ImGui::BeginCollapsingArea("CollapsingAreaNewAppear", true, header_new_appearance_progress);

        ImGui::SetNextItemOpen((header_open == command_id));
        if (ImGui::CollapsingHeaderPadded(ui_state.header_label.c_str()))
        {
            if (!is_dragging_header)
            {
                header_open = command_id;
            }
            else if (header_open != command_id)
            {
                UIManager::Get()->RepeatFrame();    //Collapsing header's arrow flickers if we deny the change, so skip that frame
            }
        }
        else if (header_open == command_id)
        {
            if (!is_dragging_header)
            {
                header_open = -1;
                delete_confirm_state = false;
            }
            else
            {
                UIManager::Get()->RepeatFrame();
            }
        }

        if (animate_appearing)
        {
            ImGui::EndCollapsingArea();
            ui_state.header_animation_progress = header_new_appearance_progress * 0.50f; //The collapsing areas are appearing independently, but slow the lower one down a bit to make it less jarring
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            drag_last_hovered_header = command_id;
        }

        //Drag reordering
        if ((ImGui::IsItemActive()) && (!ImGui::IsItemHovered()))
        {
            int index_swap = command_id + ((ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y < 0.0f) ? -1 : 1);
            if ((drag_last_hovered_header != command_id) && (index_swap >= 0) && (index_swap < action_edit.Commands.size()))
            {
                std::iter_swap(action_edit.Commands.begin() + command_id, action_edit.Commands.begin() + index_swap);
                std::iter_swap(command_ui_states.begin()    + command_id, command_ui_states.begin()    + index_swap);
                std::iter_swap(list_unique_ids.begin()      + command_id, list_unique_ids.begin()      + index_swap);

                if (header_open == command_id)
                {
                    header_open = index_swap;
                }
                else if (header_open == index_swap)
                {
                    header_open = command_id;
                }

                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                is_dragging_header = true;
                UIManager::Get()->RepeatFrame();
            }
        }

        //Keyboard reordering
        if ((io.NavVisible) && (io.KeyCtrl) && (drag_last_hovered_header == command_id))
        {
            int index_swap = command_id + ((ImGui::IsNavInputPressed(ImGuiNavInput_DpadDown, true)) ? 1 : (ImGui::IsNavInputPressed(ImGuiNavInput_DpadUp, true)) ? -1 : 0);
            if ((command_id != index_swap) && (index_swap >= 0) && (index_swap < action_edit.Commands.size()))
            {
                std::iter_swap(action_edit.Commands.begin() + command_id, action_edit.Commands.begin() + index_swap);
                std::iter_swap(command_ui_states.begin()    + command_id, command_ui_states.begin()    + index_swap);
                std::iter_swap(list_unique_ids.begin()      + command_id, list_unique_ids.begin()      + index_swap);

                if (header_open == command_id)
                {
                    header_open = index_swap;
                }
                else if (header_open == index_swap)
                {
                    header_open = command_id;
                }

                //Skip the rest of this frame to avoid double-swaps
                keyboard_swapped_index = index_swap;
                ImGui::PopID();
                UIManager::Get()->RepeatFrame();
                break;
            }
        }

        ImGui::BeginCollapsingArea("CollapsingArea", (header_open == command_id), ui_state.header_animation_progress);
        ImGui::Indent();
        ImGui::Spacing();

        ImGui::Columns(2, "ColumnCommand", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandType));
        ImGui::NextColumn();

        bool has_value_changed = false;

        int command_type_temp = command.Type;
        ImGui::SetNextItemWidth(-1);
        if (TranslatedComboAnimated("##ComboCommandType", command_type_temp, tstr_SettingsActionsEditCommandTypeNone, tstr_SettingsActionsEditCommandTypeLoadOverlayProfile))
        {
            //Reset command values, then set type
            command = ActionCommand();
            command.Type = (ActionCommand::CommandType)command_type_temp;

            ui_state.buffer_str_main[0] = '\0';
            ui_state.buffer_str_arg[0]  = '\0';
            ui_state.temp_int_1 = 0;
            ui_state.temp_int_2 = 0;

            //Set command specific default values
            if (command.Type == ActionCommand::command_load_overlay_profile)
            {
                command.UIntID = 1; //Clear existing overlays: true
            }

            has_value_changed = true;
        }
        ImGui::Spacing();
        ImGui::NextColumn();

        const float cursor_y_prev = ImGui::GetCursorPosY();

        switch (command.Type)
        {
            case ActionCommand::command_none: break;
            case ActionCommand::command_key:
            {
                if ((m_PageReturned == wndsettings_page_keycode_picker) && (header_open == command_id))
                {
                    command.UIntID = m_KeyCodePickerID;
                    has_value_changed = true;

                    m_PageReturned = wndsettings_page_none;
                }

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandKeyCode));
                ImGui::NextColumn();

                if (ImGui::Button( (command.UIntID == 0) ? TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCodeNone) : GetStringForKeyCode(command.UIntID) ))
                {
                    m_KeyCodePickerNoMouse    = false;
                    m_KeyCodePickerHotkeyMode = false;
                    m_KeyCodePickerID = (unsigned int)command.UIntID;
                    PageGoForward(wndsettings_page_keycode_picker);
                }
                ImGui::NextColumn();

                bool temp_bool = (command.UIntArg == 1);
                if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsActionsEditCommandKeyToggle), &temp_bool))
                {
                    command.UIntArg = temp_bool;
                    has_value_changed = true;
                }

                break;
            }
            case ActionCommand::command_mouse_pos:
            {
                const float input_width = ImGui::GetFontSize() * 6.0f;

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandMouseX));
                ImGui::NextColumn();

                ImGui::SetNextItemWidth(input_width);
                vr_keyboard.VRKeyboardInputBegin("##X");
                if (ImGui::InputInt("##X", &ui_state.temp_int_1, 1, 25))
                {
                    has_value_changed = true;
                }
                vr_keyboard.VRKeyboardInputEnd();
                ImGui::NextColumn();

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandMouseY));
                ImGui::NextColumn();

                ImGui::SetNextItemWidth(input_width);
                vr_keyboard.VRKeyboardInputBegin("##Y");
                if (ImGui::InputInt("##Y", &ui_state.temp_int_2, 1, 25))
                {
                    has_value_changed = true;
                }
                vr_keyboard.VRKeyboardInputEnd();
                ImGui::NextColumn();
                ImGui::NextColumn();

                if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsEditCommandMouseUseCurrent)))
                {
                    POINT mouse_pos = {0};
                    ::GetCursorPos(&mouse_pos); 

                    ui_state.temp_int_1 = mouse_pos.x;
                    ui_state.temp_int_2 = mouse_pos.y;

                    has_value_changed = true;
                }

                if (has_value_changed)
                {
                    command.UIntID = MAKELPARAM(ui_state.temp_int_1, ui_state.temp_int_2);
                }

                break;
            }
            case ActionCommand::command_string:
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandString));
                ImGui::NextColumn();

                vr_keyboard.VRKeyboardInputBegin("##InputCommandString");
                if (ImGui::InputTextMultiline("##InputCommandString", ui_state.buffer_str_main, IM_ARRAYSIZE(ui_state.buffer_str_main), multiline_input_size))
                {
                    //Check input for unmapped characters
                    if (ImGui::StringContainsUnmappedCharacter(ui_state.buffer_str_main))
                    {
                        if (TextureManager::Get().AddFontBuilderString(ui_state.buffer_str_main))
                        {
                            TextureManager::Get().ReloadAllTexturesLater();
                            UIManager::Get()->RepeatFrame();
                        }
                    }

                    command.StrMain = ui_state.buffer_str_main;
                    has_value_changed = true;
                }
                vr_keyboard.VRKeyboardInputEnd();

                break;
            }
            case ActionCommand::command_launch_app:
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandPath));
                ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                HelpMarker(TranslationManager::GetString(tstr_SettingsActionsEditCommandPathTip));

                ImGui::NextColumn();

                ImGui::SetNextItemWidth(-1.0f);
                vr_keyboard.VRKeyboardInputBegin("##InputCommandPath");
                if (ImGui::InputText("##InputCommandPath", ui_state.buffer_str_main, IM_ARRAYSIZE(ui_state.buffer_str_main)))
                {
                    UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(ui_state.buffer_str_main);

                    command.StrMain = ui_state.buffer_str_main;
                    has_value_changed = true;
                }
                vr_keyboard.VRKeyboardInputEnd();
                ImGui::NextColumn();

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandArgs));
                ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                HelpMarker(TranslationManager::GetString(tstr_SettingsActionsEditCommandArgsTip));

                ImGui::NextColumn();

                ImGui::SetNextItemWidth(-1.0f);
                vr_keyboard.VRKeyboardInputBegin("##InputCommandArg");
                if (ImGui::InputText("##InputCommandArg", ui_state.buffer_str_arg, IM_ARRAYSIZE(ui_state.buffer_str_arg)))
                {
                    UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(ui_state.buffer_str_arg);

                    command.StrArg = ui_state.buffer_str_arg;
                    has_value_changed = true;
                }
                vr_keyboard.VRKeyboardInputEnd();

                break;
            }
            case ActionCommand::command_show_keyboard:
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandVisibility));
                ImGui::NextColumn();

                int command_arg_temp = command.UIntArg;
                ImGui::SetNextItemWidth(-1);
                if (TranslatedComboAnimated("##ComboCommandToggleArg", command_arg_temp, tstr_SettingsActionsEditCommandVisibilityToggle, tstr_SettingsActionsEditCommandVisibilityHide))
                {
                    command.UIntArg = command_arg_temp;
                    has_value_changed = true;
                }

                break;
            }
            case ActionCommand::command_crop_active_window: break;
            case ActionCommand::command_show_overlay:
            {
                bool use_command_tags = (LOWORD(command.UIntID) == 1);
                bool do_undo_command  = (HIWORD(command.UIntID) == 1);

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandVisibility));
                ImGui::NextColumn();

                int command_arg_temp = command.UIntArg;
                ImGui::SetNextItemWidth(-1);
                if (TranslatedComboAnimated("##ComboCommandToggleArg", command_arg_temp, tstr_SettingsActionsEditCommandVisibilityToggle, tstr_SettingsActionsEditCommandVisibilityHide))
                {
                    command.UIntArg = command_arg_temp;
                    has_value_changed = true;
                }
                ImGui::NextColumn();

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditTarget));
                ImGui::NextColumn();

                if (ImGui::RadioButton(TranslationManager::GetString(tstr_SettingsActionsEditTargetActionTarget), !use_command_tags))
                {
                    command.UIntID = MAKELPARAM(false, do_undo_command);
                    has_value_changed = true;
                }

                ImGui::SameLine();

                if (ImGui::RadioButton(TranslationManager::GetString(tstr_SettingsActionsEditTargetUseTags), use_command_tags))
                {
                    command.UIntID = MAKELPARAM(true, do_undo_command);
                    has_value_changed = true;
                }

                ImGui::BeginCollapsingArea("CommandAreaTags", use_command_tags, ui_state.header_2_animation_progress);

                if (InputOverlayTags("CommandTargetTags", ui_state.buffer_str_main, IM_ARRAYSIZE(ui_state.buffer_str_main), ui_state.input_tags_state, 1))
                {
                    command.StrMain = ui_state.buffer_str_main;
                    has_value_changed = true;
                }

                ImGui::NextColumn();

                if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsActionsEditCommandUndo), &do_undo_command))
                {
                    command.UIntID = MAKELPARAM(use_command_tags, do_undo_command);
                    has_value_changed = true;
                }

                ImGui::EndCollapsingArea();

                break;
            }
            case ActionCommand::command_switch_task:
            {
                bool use_strict_matching = (LOWORD(command.UIntArg) == 1);
                bool warp_cursor         = (HIWORD(command.UIntArg) == 1);

                //If window title needs to be refreshed or returned from widnow picker
                if ((ui_state.temp_window_button_title.empty()) || ((m_PageReturned == wndsettings_page_window_picker) && (header_open == command_id)))
                {
                    const auto& window_list = WindowManager::Get().WindowListGet();
                    const bool returned_from_picker = (m_WindowPickerHWND != nullptr);

                    //If not returned from window picker, find HWND for the target window title if possible
                    if (!returned_from_picker)
                    {
                        //exe & class names are packed into StrArg, seperated by |, which isn't allowed to appear in file names so it makes a decent separator here
                        std::string exe_name;
                        std::string class_name;
                        size_t search_pos = command.StrArg.find("|");

                        if (search_pos != std::string::npos)
                        {
                            exe_name   = command.StrArg.substr(0, search_pos);
                            class_name = command.StrArg.substr(search_pos + 1);
                        }

                        m_WindowPickerHWND = WindowInfo::FindClosestWindowForTitle(command.StrMain, class_name, exe_name, window_list, use_strict_matching);
                    }

                    const auto it = std::find_if(window_list.begin(), window_list.end(), [&](const auto& window) { return (window.GetWindowHandle() == m_WindowPickerHWND); });

                    if (it != window_list.end())
                    {
                        command.StrMain = StringConvertFromUTF16(it->GetTitle().c_str());
                        command.StrArg  = it->GetExeName() + "|" + StringConvertFromUTF16(it->GetWindowClassName().c_str());

                        ui_state.temp_window_button_title = it->GetListTitle();
                    }
                    else if (!command.StrMain.empty())  //Referenced window doesn't exist right now
                    {
                        ui_state.temp_window_button_title = TranslationManager::GetString(tstr_SourceWinRTClosed) + std::string(" " + command.StrMain);
                    }
                    else  //Couldn't find any open window
                    {
                        command.StrArg.clear();

                        ui_state.temp_window_button_title = TranslationManager::GetString(tstr_SettingsActionsEditCommandWindowNone);
                    }

                    has_value_changed = true;

                    m_PageReturned = wndsettings_page_none;
                    m_WindowPickerHWND = nullptr;
                }

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandSwitchingMethod));
                ImGui::NextColumn();

                if (ImGui::RadioButton(TranslationManager::GetString(tstr_SettingsActionsEditCommandSwitchingMethodSwitcher), (command.UIntID == 0)))
                {
                    command.UIntID = 0;
                    has_value_changed = true;
                }

                ImGui::SameLine();

                if (ImGui::RadioButton(TranslationManager::GetString(tstr_SettingsActionsEditCommandSwitchingMethodFocus), (command.UIntID == 1)))
                {
                    command.UIntID = 1;
                    has_value_changed = true;
                }

                ImGui::NextColumn();

                if (command.UIntID == 0)
                    ImGui::PushItemDisabled();

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandWindow));
                ImGui::NextColumn();

                //Window picker button
                ImVec2 button_size(0.0f, 0.0f);

                if (ImGui::CalcTextSize(ui_state.temp_window_button_title.c_str()).x > ImGui::GetContentRegionAvail().x - style.FramePadding.x * 2.0f)
                {
                    button_size.x = ImGui::GetContentRegionAvail().x - 1.0f;
                }

                if (ImGui::Button(ui_state.temp_window_button_title.c_str(), button_size))
                {
                    std::string exe_name;
                    std::string class_name;
                    size_t search_pos = command.StrArg.find("|");

                    if (search_pos != std::string::npos)
                    {
                        exe_name   = command.StrArg.substr(0, search_pos);
                        class_name = command.StrArg.substr(search_pos + 1);
                    }

                    HWND hwnd_current = WindowInfo::FindClosestWindowForTitle(command.StrMain, class_name, exe_name, WindowManager::Get().WindowListGet(), use_strict_matching);

                    m_WindowPickerHWND = hwnd_current;
                    PageGoForward(wndsettings_page_window_picker);
                }

                ImGui::NextColumn();
                ImGui::NextColumn();

                if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsCaptureGCStrictMatching), &use_strict_matching))
                {
                    command.UIntArg = MAKELPARAM(use_strict_matching, warp_cursor);
                    has_value_changed = true;
                }
                ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                HelpMarker(TranslationManager::GetString(tstr_SettingsActionsEditCommandWindowStrictMatchingTip));

                ImGui::NextColumn();

                if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsActionsEditCommandCursorWarp), &warp_cursor))
                {
                    command.UIntArg = MAKELPARAM(use_strict_matching, warp_cursor);
                    has_value_changed = true;
                }

                if (command.UIntID == 0)
                    ImGui::PopItemDisabled();

                break;
            }
            case ActionCommand::command_load_overlay_profile:
            {
                bool clear_existing = (command.UIntID == 1);

                int& list_id = ui_state.temp_int_1;

                //Find current selection index if needed
                if ((list_id == 0) && (!command.StrMain.empty()))
                {
                    const auto it = std::find(m_ProfileList.begin(), m_ProfileList.end(), command.StrMain);
                    list_id = (it != m_ProfileList.end()) ? (int)std::distance(m_ProfileList.begin(), it) : -1;
                }

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandProfile));

                ImGui::NextColumn();

                ImGui::PushItemWidth(-1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginComboAnimated("##ComboLang", (!command.StrMain.empty()) ? command.StrMain.c_str() : TranslationManager::GetString(tstr_SettingsProfilesOverlaysNameDefault) ))
                {
                    int index = 0;
                    for (const auto& name : m_ProfileList)
                    {
                        //Skip [New Profile] which is always at the end of the list
                        if (index == m_ProfileList.size() - 1)
                        {
                            break;
                        }

                        ImGui::PushID(index);
                        if (ImGui::Selectable(name.c_str(), (index == list_id)))
                        {
                            list_id = index;

                            if (list_id == 0)
                            {
                                command.StrMain = "";
                                command.UIntID  = 1;
                            }
                            else
                            {
                                command.StrMain = m_ProfileList[list_id];
                            }

                            has_value_changed = true;
                        }
                        ImGui::PopID();

                        index++;
                    }

                    ImGui::EndCombo();
                }

                ImGui::NextColumn();

                //"Default Profile" is always clearing overlays
                if (list_id == 0)
                    ImGui::PushItemDisabled();

                if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsActionsEditCommandProfileClear), &clear_existing))
                {
                    command.UIntID = clear_existing;
                    has_value_changed = true;
                }

                if (list_id == 0)
                    ImGui::PopItemDisabled();

                break;
            }
            default: break;
        }

        if (has_value_changed)
        {
            ui_state.header_label = ActionManager::GetCommandDescription(command, header_text_max_width);
        }

        ImGui::Columns(1);

        //Delete button
        bool command_was_deleted = false;

        if (cursor_y_prev != ImGui::GetCursorPosY())    //Only add spacing if the command had any settings
            ImGui::Spacing();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - delete_button_width);

        if (delete_confirm_state)
        {
            if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsEditCommandDeleteConfirm), m_CachedSizes.ActionEdit_ButtonDeleteSize))
            {
                delete_confirm_state = false;

                action_edit.Commands.erase(action_edit.Commands.begin() + command_id);
                command_ui_states.erase(command_ui_states.begin() + command_id);

                header_open = -1;
                command_was_deleted = true;
            }
        }
        else
        {
            if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsEditCommandDelete), m_CachedSizes.ActionEdit_ButtonDeleteSize))
            {
                delete_confirm_state = true;
            }
        }

        delete_button_width = ImGui::GetItemRectSize().x + style.IndentSpacing;

        ImGui::Spacing();
        ImGui::Unindent();
        ImGui::EndCollapsingArea();

        ImGui::PopID();

        if (command_was_deleted)
        {
            UIManager::Get()->RepeatFrame();
            break;
        }

        ++command_id;
    }

    if (!action_edit.Commands.empty())
    {
        ImGui::Separator();
    }

    //Use empty label here. Icon and actual label are manually created further down
    if (ImGui::Selectable("##AddCommand", false))
    {
        ActionCommand command;

        action_edit.Commands.push_back(command);

        //Animate appearance of new command and open its header
        header_new_appearance_progress = 0.0f;
        header_open = (int)action_edit.Commands.size() - 1;
        UIManager::Get()->RepeatFrame(3);                           //3 frames to avoid scrollbar flickering from sizing calculations
    }

    if (ImGui::IsItemVisible())
    {
        //Custom render the selectable label with icon
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
        ImVec2 img_size, img_uv_min, img_uv_max;
        TextureManager::Get().GetTextureInfo(tmtex_icon_add, img_size, img_uv_min, img_uv_max);
        ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

        //Label
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsEditCommandAdd));
    }

    ImGui::EndChild();

    ImGui::Unindent();

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        is_dragging_header = false;
    }

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()) );

    //Confirmation buttons
    if (ImGui::Button(TranslationManager::GetString(tstr_DialogOk))) 
    {
        action_manager.StoreAction(action_edit);

        //Reload texture to apply icon if there is any
        if (!action_edit.IconFilename.empty())
        {
            TextureManager::Get().ReloadAllTexturesLater();
        }

        //Send action over
        IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_data, action_edit.Serialize(), UIManager::Get()->GetWindowHandle());

        PageGoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageActionsOrder(bool only_restore_settings)
{
    static FloatingWindowActionOrderListState page_state;

    ActionManager& action_manager = ConfigManager::Get().GetActionManager();
    ActionManager::ActionList& action_list = (m_ActionOrderListEditForOverlayBar) ? action_manager.GetActionOrderListOverlayBar() : action_manager.GetActionOrderListBarDefault();

    if (only_restore_settings)
    {
        if (!page_state.HasSavedChanges)
        {
            action_list = page_state.ActionListOrig;
        }
        return;
    }

    bool go_add_actions = false;
    bool go_back = ActionOrderList(action_list, (m_PageAppearing == wndsettings_page_actions_order), (m_PageReturned == wndsettings_page_actions_order_add), page_state, go_add_actions, -m_WarningHeight);

    if (m_PageReturned == wndsettings_page_actions_order_add)
    {
        m_PageReturned = wndsettings_page_none;
    }

    if (go_add_actions)
    {
        PageGoForward(wndsettings_page_actions_order_add);
    }
    else if (go_back)
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageActionsOrderAdd()
{
    static FloatingWindowActionAddSelectorState page_state;

    ActionManager& action_manager = ConfigManager::Get().GetActionManager();
    ActionManager::ActionList& action_list = (m_ActionOrderListEditForOverlayBar) ? action_manager.GetActionOrderListOverlayBar() : action_manager.GetActionOrderListBarDefault();

    bool go_back = ActionAddSelector(action_list, (m_PageAppearing == wndsettings_page_actions_order_add), page_state, -m_WarningHeight);

    if (go_back)
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageColorPicker(bool only_restore_settings)
{
    static ImVec4 color_current;
    static ImVec4 color_original;
    static bool do_restore_color = true;
    const ConfigID_Int config_id = configid_int_interface_background_color; //Currently only need this one, so we keep it simple

    if (only_restore_settings)
    {
        if (do_restore_color)
        {
            ImU32 rgba = ImGui::ColorConvertFloat4ToU32(color_original);

            ConfigManager::Get().SetValue(config_id, *(int*)&rgba);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(config_id), *(int*)&rgba);
        }

        return;
    }

    if (m_PageAppearing == wndsettings_page_color_picker)
    {
        color_current  = ImGui::ColorConvertU32ToFloat4( pun_cast<ImU32, int>(ConfigManager::Get().GetValue(config_id)) );
        color_original = color_current;
        do_restore_color = true;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    //Make page scrollable since we can't easily adjust the picker to space taken by warnings
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::BeginChild("SettingsColorPicker", ImVec2(0.00f, -ImGui::GetFrameHeightWithSpacing() - style.ItemSpacing.y), ImGuiChildFlags_NavFlattened);
    ImGui::PopStyleColor();

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogColorPickerHeader)); 
    ImGui::Indent();

    vr_keyboard.VRKeyboardInputBegin("#ColorPicker");
    if (ImGui::ColorPicker4Simple("#ColorPicker", &color_current.x, &color_original.x, 
                                  TranslationManager::GetString(tstr_DialogColorPickerCurrent), TranslationManager::GetString(tstr_DialogColorPickerOriginal),
                                  UIManager::Get()->IsInDesktopMode() ? 1.25f : 1.00f))
    {
        int rgba = pun_cast<int, ImU32>( ImGui::ColorConvertFloat4ToU32(color_current) );

        ConfigManager::Get().SetValue(config_id, rgba);
        IPCManager::Get().PostConfigMessageToDashboardApp(config_id, rgba);
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::Unindent();
    ImGui::EndChild();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons
    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogOk))) 
    {
        //Prevent restore settings code from overwriting it later
        do_restore_color = false;

        PageGoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageProfilePicker()
{
    ImGuiStyle& style = ImGui::GetStyle();

    static bool is_nav_focus_entry_pending = false;    //Focus has to be delayed until after the page animation is done
    static bool scroll_to_selection = false;
    static int list_id = -1;

    if (m_PageAppearing == wndsettings_page_profile_picker)
    {
        //Load profile list
        m_ProfileList = ConfigManager::Get().GetOverlayProfileList();
        list_id = -1;

        //Adjust entries from profile list for picker use
        m_ProfileList[0] = TranslationManager::GetString(tstr_DialogProfilePickerNone);
        m_ProfileList.erase(m_ProfileList.end() - 1);

        //Pre-select previous selection if it can be found
        if (m_ProfilePickerName.empty())
        {
            list_id = 0;
            scroll_to_selection = true;
            is_nav_focus_entry_pending = ImGui::GetIO().NavVisible;
        }
        else
        {
            const auto it = std::find(m_ProfileList.begin(), m_ProfileList.end(), m_ProfilePickerName);

            if (it != m_ProfileList.end())
            {
                list_id = (int)std::distance(m_ProfileList.begin(), it);
                scroll_to_selection = true;
                is_nav_focus_entry_pending = ImGui::GetIO().NavVisible;
            }
        }
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogProfilePickerHeader)); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 22.0f : 15.0f;
    ImGui::BeginChild("ProfilePickerList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //List profiles
    int index = 0;
    for (const auto& name : m_ProfileList)
    {
        if ( (is_nav_focus_entry_pending) && (m_PageAnimationDir == 0) && (index == list_id) )
        {
            ImGui::SetKeyboardFocusHere();
            is_nav_focus_entry_pending = false;
        }

        ImGui::PushID(index);
        if (ImGui::Selectable(name.c_str(), (index == list_id)))
        {
            list_id = index;
            m_ProfilePickerName = (list_id == 0) ? "" : m_ProfileList[list_id];

            PageGoBack();
        }
        ImGui::PopID();

        if ( (scroll_to_selection) && (index == list_id) )
        {
            ImGui::SetScrollHereY();

            if (ImGui::IsItemVisible())
            {
                scroll_to_selection = false;
            }
        }

        index++;
    }

    ImGui::EndChild();
    ImGui::Unindent();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()) );

    //Cancel button
    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageActionPicker()
{
    ImGuiStyle& style = ImGui::GetStyle();

    static ActionUID list_uid = k_ActionUID_Invalid;
    static bool is_nav_focus_entry_pending = false;    //Focus has to be delayed until after the page animation is done
    static bool scroll_to_selection = false;

    if (m_PageAppearing == wndsettings_page_action_picker)
    {
        //Load action list
        m_ActionList = ConfigManager::Get().GetActionManager().GetActionNameList();

        list_uid = m_ActionPickerUID;
        scroll_to_selection = true;
        is_nav_focus_entry_pending = ImGui::GetIO().NavVisible;

        //Set to invalid if selection doesn't exist
        if (!ConfigManager::Get().GetActionManager().ActionExists(list_uid))
        {
            list_uid = k_ActionUID_Invalid;
        }
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogActionPickerHeader)); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 22.0f : 15.0f;
    ImGui::BeginChild("ActionPickerList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //No Action entry
    {
        ImGui::PushID(0);

        if ( (is_nav_focus_entry_pending) && (m_PageAnimationDir == 0) && (list_uid == k_ActionUID_Invalid) )
        {
            ImGui::SetKeyboardFocusHere();
            is_nav_focus_entry_pending = false;
        }

        if (ImGui::Selectable(TranslationManager::GetString(tstr_ActionNone), (list_uid == k_ActionUID_Invalid) ))
        {
            list_uid = k_ActionUID_Invalid;
            m_ActionPickerUID = k_ActionUID_Invalid;

            PageGoBack();
        }

        if ( (scroll_to_selection) && (list_uid == k_ActionUID_Invalid) )
        {
            ImGui::SetScrollHereY();

            if (ImGui::IsItemVisible())
            {
                scroll_to_selection = false;
            }
        }

        ImGui::PopID();
    }

    //List actions
    for (const auto& entry : m_ActionList)
    {
        ImGui::PushID((void*)entry.UID);

        if ( (is_nav_focus_entry_pending) && (m_PageAnimationDir == 0) && (entry.UID == list_uid) )
        {
            ImGui::SetKeyboardFocusHere();
            is_nav_focus_entry_pending = false;
        }

        if (ImGui::Selectable(entry.Name.c_str(), (entry.UID == list_uid) ))
        {
            list_uid = entry.UID;
            m_ActionPickerUID = entry.UID;

            PageGoBack();
        }

        if ( (scroll_to_selection) && (entry.UID == list_uid) )
        {
            ImGui::SetScrollHereY();

            if (ImGui::IsItemVisible())
            {
                scroll_to_selection = false;
            }
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::Unindent();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()) );

    //Cancel button
    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageKeyCodePicker(bool only_restore_settings)
{
    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    static ImGuiTextFilter filter;
    static int list_id = 0;
    static bool is_nav_focus_entry_pending = false;    //Focus has to be delayed until after the page animation is done
    static bool scroll_to_selection = false;

    static unsigned char key_code_prev = 0;
    static bool mod_ctrl  = false;
    static bool mod_alt   = false;
    static bool mod_shift = false;
    static bool mod_win   = false;

    if (only_restore_settings)
    {
        //Only really need to reset this in hotkey mode
        m_KeyCodePickerID = key_code_prev;
        return;
    }

    if (m_PageAppearing == wndsettings_page_keycode_picker)
    {
        list_id = m_KeyCodePickerID;
        key_code_prev = m_KeyCodePickerID;
        scroll_to_selection = true;
        is_nav_focus_entry_pending = ImGui::GetIO().NavVisible;

        for (int i = 0; i < 256; i++)
        {
            //Not the smartest, but most straight forward way
            if (GetKeyCodeForListID(i) == m_KeyCodePickerID)
            {
                list_id = i;

                //Clear filter if it wouldn't show the current selection
                if (!filter.PassFilter( (m_KeyCodePickerID == 0) ? TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCodeNone) : GetStringForKeyCode(m_KeyCodePickerID) ))
                {
                    filter.Clear();
                }

                break;
            }
        }

        if (m_KeyCodePickerHotkeyMode)
        {
            mod_ctrl  = (m_KeyCodePickerHotkeyFlags & MOD_CONTROL);
            mod_alt   = (m_KeyCodePickerHotkeyFlags & MOD_ALT);
            mod_shift = (m_KeyCodePickerHotkeyFlags & MOD_SHIFT);
            mod_win   = (m_KeyCodePickerHotkeyFlags & MOD_WIN);
        }
    }

    //Modifier flags if this displayed to set a hotkey
    if (m_KeyCodePickerHotkeyMode)
    {
        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogKeyCodePickerHeaderHotkey)); 
        ImGui::Indent();

        static float checkbox_width = 0.0f;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_DialogKeyCodePickerModifiers));
        ImGui::SameLine();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - checkbox_width);

        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {style.ItemSpacing.x * 2.0f, style.ItemSpacing.y});

        ImGui::Checkbox("Ctrl",  &mod_ctrl);    //These could be translated, but the whole key list isn't, so no point right now
        ImGui::SameLine();
        ImGui::Checkbox("Alt",   &mod_alt);
        ImGui::SameLine();
        ImGui::Checkbox("Shift", &mod_shift);
        ImGui::SameLine();
        ImGui::Checkbox("Win",   &mod_win);

        ImGui::PopStyleVar();
        ImGui::EndGroup();

        checkbox_width = ImGui::GetItemRectSize().x + style.ItemSpacing.x;

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCode)); 
        ImGui::Spacing();
    }
    else //Normal header
    {
        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogKeyCodePickerHeader)); 
    }

    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    vr_keyboard.SetShortcutWindowDirectionHint(ImGuiDir_Up);
    vr_keyboard.VRKeyboardInputBegin("##FilterList");
    if (ImGui::InputTextWithHint("##FilterList", TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCodeHint), filter.InputBuf, IM_ARRAYSIZE(filter.InputBuf)))
    {
        UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(filter.InputBuf);

        filter.Build();
    }
    vr_keyboard.VRKeyboardInputEnd();

    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count_offset = (m_KeyCodePickerHotkeyMode) ? ((UIManager::Get()->IsInDesktopMode()) ? -2.5f : -2.0f) : 0.0f;
    const float item_count = ((UIManager::Get()->IsInDesktopMode()) ? 21.0f : 16.0f) + item_count_offset;
    ImGui::BeginChild("KeyCodePickerList", ImVec2(-1.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    unsigned char list_keycode;
    const char* list_keycode_str = nullptr;
    for (int i = 0; i < 256; i++)
    {
        list_keycode = GetKeyCodeForListID(i);
        list_keycode_str = (list_keycode == 0) ? TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCodeNone) : GetStringForKeyCode(list_keycode);
        if (filter.PassFilter(list_keycode_str))
        {
            if ( (m_KeyCodePickerNoMouse) && (list_keycode >= VK_LBUTTON) && (list_keycode <= VK_XBUTTON2) && (list_keycode != VK_CANCEL) )    //Skip mouse buttons if turned off
                continue;

            if ( (is_nav_focus_entry_pending) && (m_PageAnimationDir == 0) && (i == list_id) )
            {
                ImGui::SetKeyboardFocusHere();
                is_nav_focus_entry_pending = false;
            }

            if (ImGui::Selectable(list_keycode_str, (i == list_id)))
            {
                list_id = i;
                m_KeyCodePickerID = list_keycode;

                if (!m_KeyCodePickerHotkeyMode)
                {
                    key_code_prev = m_KeyCodePickerID; //Prevent it from being reset
                    PageGoBack();
                }
            }

            if ((ImGui::IsItemClicked()) && (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)))
            {
                key_code_prev = m_KeyCodePickerID; //Prevent it from being reset
                PageGoBack();
            }

            if ( (scroll_to_selection) && (i == list_id) )
            {
                ImGui::SetScrollHereY();

                if (ImGui::IsItemVisible())
                {
                    scroll_to_selection = false;
                }
            }
        }
    }

    ImGui::EndChild();
    ImGui::Unindent();

    if (m_KeyCodePickerHotkeyMode)
    {
        ImGui::Unindent();
    }

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()) );

    //Confirmation buttons
    if (m_KeyCodePickerHotkeyMode)
    {
        if (ImGui::Button(TranslationManager::GetString(tstr_DialogOk)))
        {
            m_KeyCodePickerHotkeyFlags = 0;

            if (mod_ctrl)
                m_KeyCodePickerHotkeyFlags |= MOD_CONTROL;
            if (mod_alt)
                m_KeyCodePickerHotkeyFlags |= MOD_ALT;
            if (mod_shift)
                m_KeyCodePickerHotkeyFlags |= MOD_SHIFT;
            if (mod_win)
                m_KeyCodePickerHotkeyFlags |= MOD_WIN;

            //Prevent restore settings code from overwriting it later
            key_code_prev = m_KeyCodePickerID;

            PageGoBack();
        }

        ImGui::SameLine();
    }

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }

    //"From Input..." button
    if (UIManager::Get()->IsInDesktopMode())
    {
        ImGui::SameLine();

        static float list_buttons_width = 0.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - list_buttons_width);

        if (ImGui::Button(TranslationManager::GetString(tstr_DialogKeyCodePickerFromInput)))
        {
            ImGui::OpenPopup("Bind Key");
        }

        list_buttons_width = ImGui::GetItemRectSize().x;

        ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Bind Key", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNavInputs))
        {
            ImGui::Text(TranslationManager::GetString( (m_KeyCodePickerNoMouse) ? tstr_DialogKeyCodePickerFromInputPopupNoMouse : tstr_DialogKeyCodePickerFromInputPopup ));

            ImGuiIO& io = ImGui::GetIO();

            //We can no longer use ImGui's keyboard state to query all possible keyboard keys, so we do it manually via GetAsyncKeyState()
            //To avoid issues with keys that are already down to begin with, we store the state in the moment of the popup appearing and only act on changes to that
            static bool keyboard_state_initial[255] = {0};
            static bool wait_for_key_release = false; 

            if (ImGui::IsWindowAppearing())
            {
                for (int i = 0; i < IM_ARRAYSIZE(keyboard_state_initial); ++i)
                {
                    keyboard_state_initial[i] = (::GetAsyncKeyState(i) < 0);
                }

                wait_for_key_release = false;
            }

            for (int i = 0; i < IM_ARRAYSIZE(keyboard_state_initial); ++i)
            {
                if ((::GetAsyncKeyState(i) < 0) != keyboard_state_initial[i])
                {
                    //Key was up before, so it's a key press
                    if (!keyboard_state_initial[i])
                    {
                        //Ignore mouse buttons if they're disabled
                        bool skip_key = false;
                        if (m_KeyCodePickerNoMouse)
                        {
                            switch (i)
                            {
                                case VK_LBUTTON:
                                case VK_RBUTTON:
                                case VK_MBUTTON:
                                case VK_XBUTTON1:
                                case VK_XBUTTON2: skip_key = true;
                            }
                        }

                        if (!skip_key)
                        {
                            m_KeyCodePickerID = i;

                            if (!m_KeyCodePickerHotkeyMode)
                            {
                                key_code_prev = m_KeyCodePickerID; //Prevent it from being reset
                            }

                            for (int i = 0; i < 256; i++)
                            {
                                if (GetKeyCodeForListID(i) == m_KeyCodePickerID)
                                {
                                    list_id = i;
                                    break;
                                }
                            }

                            scroll_to_selection = true;

                            //Wait for the key to be released to avoid inputs triggering other things
                            wait_for_key_release = true;
                            keyboard_state_initial[i] = true;
                        }
                        break;
                    }
                    else   //Key was down before, so it's a key release. Update the initial state so it can be pressed again and registered as such
                    {
                        keyboard_state_initial[i] = false;

                        //Close popup here if we are waiting for this key to be released
                        if ((wait_for_key_release) && (i == m_KeyCodePickerID))
                        {
                            ImGui::CloseCurrentPopup();
                            io.ClearInputKeys();

                            if (!m_KeyCodePickerHotkeyMode)
                            {
                                PageGoBack();
                            }
                        }
                    }
                }
            }

            ImGui::EndPopup();
        }
    }
}

void WindowSettings::UpdatePageIconPicker()
{
    ImGuiStyle& style = ImGui::GetStyle();

    static std::vector<std::string> icon_file_list;
    static int list_id = 0;
    static bool is_nav_focus_entry_pending = false;    //Focus has to be delayed until after the page animation is done
    static bool scroll_to_selection = false;

    if (m_PageAppearing == wndsettings_page_icon_picker)
    {
        list_id = 0;
        scroll_to_selection = true;
        is_nav_focus_entry_pending = ImGui::GetIO().NavVisible;

        icon_file_list = ActionManager::GetIconFileList();
        icon_file_list.insert(icon_file_list.begin(), TranslationManager::GetString(tstr_DialogIconPickerNone));

        //Select matching entry
        auto it = std::find_if(icon_file_list.begin(), icon_file_list.end(), [&](const auto& list_entry){ return (m_IconPickerFile == list_entry); });

        if (it != icon_file_list.end())
        {
            list_id = (int)std::distance(icon_file_list.begin(), it);
        }
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogIconPickerHeader));
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    HelpMarker(TranslationManager::GetString(tstr_DialogIconPickerHeaderTip));

    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 22.0f : 18.0f;
    ImGui::BeginChild("IconPickerList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    int i = 0;
    for (const auto& file_entry : icon_file_list)
    {
        if ( (is_nav_focus_entry_pending) && (m_PageAnimationDir == 0) && (i == list_id) )
        {
            ImGui::SetKeyboardFocusHere();
            is_nav_focus_entry_pending = false;
        }

        if (ImGui::Selectable( file_entry.c_str(), (i == list_id) ))
        {
            list_id = i;

            std::string icon_path = "images/icons/" + file_entry;
            TextureManager::Get().SetTextureFilenameIconTemp(WStringConvertFromUTF8(icon_path.c_str()).c_str());
            TextureManager::Get().ReloadAllTexturesLater(); //Reloading everything on changing one icon path? Seems excessive, but works fine for now.

            UIManager::Get()->RepeatFrame();
        }

        if ((ImGui::IsItemClicked()) && (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)))
        {
            m_IconPickerFile = (list_id != 0) ? icon_file_list[list_id] : "";

            PageGoBack();
        }

        if ( (scroll_to_selection) && (i == list_id) )
        {
            ImGui::SetScrollHereY();

            if (ImGui::IsItemVisible())
            {
                scroll_to_selection = false;
            }
        }

        ++i;
    }

    const bool has_scrollbar = ImGui::IsAnyScrollBarVisible();

    ImGui::EndChild();
    ImGui::Unindent();

    //Icon Preview
    if (list_id != 0)
    {
        ImVec2 i_size, i_uv_min, i_uv_max;
        TextureManager::Get().GetTextureInfo(tmtex_icon_settings, i_size, i_uv_min, i_uv_max);
        //Default image size for custom actions
        ImVec2 i_size_default = i_size;

        //Draw at bottom right corner of the child window, but don't cover scroll bar if there is any
        ImVec2 preview_max = ImGui::GetItemRectMax();

        if (has_scrollbar)
            preview_max.x -= style.ScrollbarSize;

        preview_max.x -= style.ItemInnerSpacing.x;
        preview_max.y -= style.ItemInnerSpacing.y;

        ImVec2 preview_pos = preview_max;
        preview_pos.x -= i_size_default.x;
        preview_pos.y -= i_size_default.y;

        TextureManager::Get().GetTextureInfo(tmtex_icon_temp, i_size, i_uv_min, i_uv_max);
        ImGui::GetForegroundDrawList()->AddImage(ImGui::GetIO().Fonts->TexID, preview_pos, preview_max, i_uv_min, i_uv_max);
    }

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()) );

    //Confirmation buttons
    if (ImGui::Button(TranslationManager::GetString(tstr_DialogOk))) 
    {
        m_IconPickerFile = (list_id != 0) ? icon_file_list[list_id] : "";

        PageGoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageWindowPicker()
{
    ImGuiStyle& style = ImGui::GetStyle();

    static HWND list_hwnd = nullptr;
    static bool is_nav_focus_entry_pending = false;    //Focus has to be delayed until after the page animation is done
    static bool scroll_to_selection = false;
    static ImVec2 no_actions_text_size;

    if (m_PageAppearing == wndsettings_page_action_picker)
    {
        is_nav_focus_entry_pending = m_WindowPickerHWND;
        scroll_to_selection = true;
        is_nav_focus_entry_pending = ImGui::GetIO().NavVisible;
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogWindowPickerHeader)); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 22.0f : 15.0f;
    ImGui::BeginChild("WindowPickerList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //List windows
    ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
    ImVec2 img_size, img_uv_min, img_uv_max;
    const ImVec2 combo_pos = ImGui::GetCursorScreenPos();

    for (const auto& window_info : WindowManager::Get().WindowListGet())
    {
        ImGui::PushID(window_info.GetWindowHandle());
        if (ImGui::Selectable("", (list_hwnd == window_info.GetWindowHandle()) ))
        {
            list_hwnd = window_info.GetWindowHandle();
            m_WindowPickerHWND = list_hwnd;

            PageGoBack();
        }

        if ( (scroll_to_selection) && (list_hwnd == window_info.GetWindowHandle()) )
        {
            ImGui::SetScrollHereY();

            if (ImGui::IsItemVisible())
            {
                scroll_to_selection = false;
            }
        }

        ImGui::SameLine(0.0f, 0.0f);

        int icon_id = TextureManager::Get().GetWindowIconCacheID(window_info.GetIcon());

        if (icon_id != -1)
        {
            TextureManager::Get().GetWindowIconTextureInfo(icon_id, img_size, img_uv_min, img_uv_max);
            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        }

        ImGui::TextUnformatted(window_info.GetListTitle().c_str());

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::Unindent();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()) );

    //Cancel button
    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageResetConfirm()
{
    //Check if it makes sense to show the checkbox for deleting legacy files
    static bool show_delete_legacy_check = false;

    if (m_PageAppearing == m_PageCurrent)
    {
        const std::wstring wpath_config          = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "config_legacy.ini"       ).c_str() );
        const std::wstring wpath_profiles_single = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "profiles/overlays/"      ).c_str() );
        const std::wstring wpath_profiles_multi  = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "profiles/multi-overlays/").c_str() );

        show_delete_legacy_check = ( (FileExists(wpath_config.c_str())) || (DirectoryExists(wpath_profiles_single.c_str())) || (DirectoryExists(wpath_profiles_multi.c_str())) );
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsReset) ); 
    ImGui::Indent();

    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsResetConfirmDescription));
    ImGui::PopTextWrapPos();

    static bool reset_settings = true, reset_current_profile = true, reset_profile_overlays = false, reset_profile_apps = false, reset_actions = false, delete_legacy = false;

    //This uses existing translation strings to avoid duplication of strings that should reasonably stay the same for this context
    //Might come back to bite for some language but we'll see about that
    ImGui::Indent();
    ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsInterfacePersistentUIWindowsSettings), &reset_settings);
    ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsResetConfirmElementOverlays), &reset_current_profile);
    ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsProfilesOverlays), &reset_profile_overlays);
    ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsProfilesApps), &reset_profile_apps);
    ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsCatActions), &reset_actions);

    if (show_delete_legacy_check)
    {
        ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsResetConfirmElementLegacyFiles), &delete_legacy);
    }

    ImGui::Unindent();

    ImGui::Unindent();
    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons
    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsResetConfirmButton))) 
    {
        //Do the reset
        if (reset_settings)
        {
            //If resetting current profile isn't on, store it separately for a moment so we can get it back after resetting the whole config file
            if (!reset_current_profile)
            {
                ConfigManager::Get().SaveMultiOverlayProfileToFile("../overlays_temp.ini");
            }

            ConfigManager::Get().RestoreConfigFromDefault();

            if (!reset_current_profile)
            {
                ConfigManager::Get().LoadMultiOverlayProfileFromFile("../overlays_temp.ini");
                ConfigManager::Get().DeleteOverlayProfile("../overlays_temp.ini");
            }
        }
        else if (reset_current_profile)
        {
            ConfigManager::Get().LoadOverlayProfileDefault(true);
        }

        if (reset_profile_overlays)
        {
            ConfigManager::Get().DeleteAllOverlayProfiles();
        }

        if (reset_profile_apps)
        {
            ConfigManager::Get().GetAppProfileManager().RemoveAllProfiles();
        }

        if (reset_actions)
        {
            ConfigManager::Get().GetActionManager().RestoreActionsFromDefault();
        }

        if (delete_legacy)
        {
            const std::wstring wpath_config = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "config_legacy.ini").c_str() );
            //The folder paths are double-NUL terminated for SHFileOperationW()
            const std::wstring wpath_profiles_single = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "profiles/overlays/"      ).c_str() ) + L'\0';
            const std::wstring wpath_profiles_multi  = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "profiles/multi-overlays/").c_str() ) + L'\0';

            //Delete config_legacy.ini
            ::DeleteFileW(wpath_config.c_str());

            //Delete folders recursively with contained files
            SHFILEOPSTRUCTW fileop = {0};
            fileop.wFunc  = FO_DELETE;
            fileop.fFlags = FOF_NO_UI;

            fileop.pFrom = wpath_profiles_single.c_str();
            ::SHFileOperationW(&fileop);

            fileop.pFrom = wpath_profiles_multi.c_str();
            ::SHFileOperationW(&fileop);
        }

        UIManager::Get()->Restart(UIManager::Get()->IsInDesktopMode());

        //We restart this after the UI since the new UI process needs to detect the dashboard app running first so it doesn't launch in desktop mode
        if (IPCManager::IsDashboardAppRunning())
        {
            UIManager::Get()->RestartDashboardApp();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }

    //Show Quick-Start Guide
    if (!UIManager::Get()->IsInDesktopMode())
    {
        static float button_quick_start_width = -1.0f;
        const bool button_quick_start_enabled = ConfigManager::GetValue(configid_bool_interface_quick_start_hidden);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - button_quick_start_width);

        if (!button_quick_start_enabled)
            ImGui::PushItemDisabled();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsResetShowQuickStart))) 
        {
            UIManager::Get()->GetAuxUI().GetQuickStartWindow().Reset();
        }
        button_quick_start_width = ImGui::GetItemRectSize().x;

        if (!button_quick_start_enabled)
            ImGui::PopItemDisabled();
    }
}

void WindowSettings::PageGoForward(WindowSettingsPage new_page)
{
    //We can't just mess with the stack while a backwards animation is going, so we save this for later
    if (m_PageAnimationDir == 1)
    {
        m_PageStackPending.push_back(new_page);
        return;
    }

    m_PageStack.push_back(new_page);
    m_PageStackPos++;
}

void WindowSettings::PageGoBack()
{
    if (m_PageStackPos != 0)
    {
        OnPageLeaving(m_PageStack[m_PageStackPos]);
        m_PageStackPos--;
        m_PageReturned = m_PageStack.back();
    }
}

void WindowSettings::PageGoBackInstantly()
{
    //Go back while skipping any active animations
    PageGoBack();

    while ((int)m_PageStack.size() > m_PageStackPosAnimation)
    {
        m_PageStack.pop_back();
    }

    m_PageAnimationDir      = 0;
    m_PageStackPosAnimation = m_PageStackPos;
    m_PageAnimationOffset   = m_PageAnimationStartPos;
    m_PageAnimationProgress = 0.0f;
}

void WindowSettings::PageGoHome()
{
    while (m_PageStackPos != 0)
    {
        OnPageLeaving(m_PageStack[m_PageStackPos]);
        m_PageStackPos--;
    }
}

void WindowSettings::OnPageLeaving(WindowSettingsPage previous_page)
{
    switch (previous_page)
    {
        case wndsettings_page_keyboard:
        {
            UpdatePageKeyboardLayout(true); //Call to reset settings
            break;
        }
        case wndsettings_page_actions_edit:
        {
            UpdatePageActionsEdit(true); //Call to reset settings
            break;
        }
        case wndsettings_page_actions_order:
        {
            UpdatePageActionsOrder(true); //Call to reset settings
            break;
        }
        case wndsettings_page_color_picker:
        {
            UpdatePageColorPicker(true); //Call to reset settings
            break;
        }
        case wndsettings_page_keycode_picker:
        {
            UpdatePageKeyCodePicker(true); //Call to reset settings
            break;
        }
        default: break;
    }
}

void WindowSettings::SelectableWarning(const char* selectable_id, const char* popup_id, const char* text, bool show_warning_prefix, const ImVec4* text_color)
{
    float* selectable_height = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID(selectable_id), 1.0f);
    const bool is_active = ImGui::IsPopupOpen(popup_id);

    //Force active header color when a menu is active for consistency with other context menus in the app
    if (is_active)
    {
        ImGui::PushStyleColor(ImGuiCol_Header,        ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
    }

    //Use selectable stretching over the text area to make it clickable
    if (ImGui::Selectable(selectable_id, ImGui::IsPopupOpen(popup_id), 0, {0.0f, *selectable_height}))
    {
        ImGui::OpenPopup(popup_id);
        UIManager::Get()->RepeatFrame();    //Avoid flicker from IsPopupOpen() not being true right away
    }
    ImGui::SameLine(0.0f, 0.0f);

    const bool is_selectable_focused = ImGui::IsItemFocused();

    //Render text (with wrapping for the actual warning text)
    ImGui::PushStyleColor(ImGuiCol_Text, (text_color != nullptr) ? *text_color : Style_ImGuiCol_TextWarning);

    if (show_warning_prefix)
    {
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsWarningPrefix));
        ImGui::SameLine();
    }

    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();

    ImGui::PopStyleColor();

    if (is_active)
    {
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    }

    //Store height for the selectable for next time if window is being hovered or selectable focused (could get bogus value otherwise)
    if ( (ImGui::IsWindowHovered()) || (is_selectable_focused) )
    {
        *selectable_height = ImGui::GetItemRectSize().y;
    }
}

void WindowSettings::SelectableHotkey(ConfigHotkey& hotkey, int id)
{
    static int edited_id = -1;

    //Write changes if we're returning from a picker
    if ((m_PageReturned == wndsettings_page_keycode_picker) && (edited_id == id))
    {
        hotkey.KeyCode   = m_KeyCodePickerID;
        hotkey.Modifiers = m_KeyCodePickerHotkeyFlags;

        IPCManager::Get().SendStringToDashboardApp(configid_str_state_hotkey_data, hotkey.Serialize(), UIManager::Get()->GetWindowHandle());
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_hotkey_set, id);

        m_PageReturned = wndsettings_page_none;
        hotkey.StateUIName = "";
        edited_id = -1;
    }

    //Update cached hotkey name if window is just appearing or the name is empty
    if ( (m_PageAppearing == m_PageCurrent) || (hotkey.StateUIName.empty()) )
    {
        hotkey.StateUIName = "";

        if (hotkey.KeyCode != 0)
        {
            if (hotkey.Modifiers & MOD_CONTROL)
                hotkey.StateUIName += "Ctrl+";
            if (hotkey.Modifiers & MOD_ALT)
                hotkey.StateUIName += "Alt+";
            if (hotkey.Modifiers & MOD_SHIFT)
                hotkey.StateUIName += "Shift+";
            if (hotkey.Modifiers & MOD_WIN)
                hotkey.StateUIName += "Win+";
        }

        hotkey.StateUIName += (hotkey.KeyCode == 0) ? TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCodeNone) : GetStringForKeyCode(hotkey.KeyCode);
    }

    ImGui::PushID("HotkeySelectable");
    if (ImGui::Selectable(hotkey.StateUIName.c_str()))
    {
        m_KeyCodePickerNoMouse    = false;
        m_KeyCodePickerHotkeyMode = true;
        m_KeyCodePickerHotkeyFlags = hotkey.Modifiers;
        m_KeyCodePickerID = hotkey.KeyCode;
        edited_id = id;

        PageGoForward(wndsettings_page_keycode_picker);
        m_PageReturned = wndsettings_page_none;
    }
    ImGui::PopID();
}

void WindowSettings::RefreshAppList()
{
    m_AppList.clear();

    //Each section is sorted alphabetically before being appended to the app list (via Win32, so UTF-16 needed)
    struct app_sublist_entry
    {
        std::string app_key;
        std::string app_name_utf8;
        std::wstring app_name_utf16;
    };
    std::vector<app_sublist_entry> app_sublist;
    auto app_sublist_compare = [](app_sublist_entry& a, app_sublist_entry& b) { return WStringCompareNatural(a.app_name_utf16, b.app_name_utf16); };

    std::unordered_set<std::string> unique_app_keys;
    char app_key_buffer[vr::k_unMaxApplicationKeyLength] = "";
    char app_prop_buffer[vr::k_unMaxPropertyStringSize]  = "";
    vr::EVRApplicationError app_error = vr::VRApplicationError_None;

    //Have some keys in the unique app key set by default as a way to never show them in the actual list
    unique_app_keys.insert("steam.app.250820");                         //"SteamVR"
    unique_app_keys.insert("openvr.tool.steamvr_tutorial");
    unique_app_keys.insert("openvr.tool.steamvr_room_setup");
    unique_app_keys.insert("openvr.tool.steamvr_environments_tools");

    //Add active app first
    if (UIManager::Get()->IsOpenVRLoaded())
    {
        app_error = vr::VRApplications()->GetApplicationKeyByProcessId(vr::VRApplications()->GetCurrentSceneProcessId(), app_key_buffer, vr::k_unMaxApplicationKeyLength);

        if (app_error == vr::VRApplicationError_None)
        {
            std::string app_key  = app_key_buffer;
            std::string app_name = app_key;

            vr::VRApplications()->GetApplicationPropertyString(app_key_buffer, vr::VRApplicationProperty_Name_String, app_prop_buffer, vr::k_unMaxPropertyStringSize, &app_error);

            if (app_error == vr::VRApplicationError_None)
            {
                app_name = app_prop_buffer;
            }

            unique_app_keys.insert(app_key);
            m_AppList.emplace_back(app_key, app_name);
        }
    }

    //Add apps with profiles before listing all eligble apps
    for (const auto& app_key : ConfigManager::Get().GetAppProfileManager().GetProfileAppKeyList())
    {
        std::string app_name;

        //Only add if not already in list
        if (unique_app_keys.find(app_key) == unique_app_keys.end())
        {
            if (UIManager::Get()->IsOpenVRLoaded())
            {
                vr::VRApplications()->GetApplicationPropertyString(app_key.c_str(), vr::VRApplicationProperty_Name_String, app_prop_buffer, vr::k_unMaxPropertyStringSize, &app_error);

                if (app_error == vr::VRApplicationError_None)
                {
                    app_name = app_prop_buffer;
                }
            }
            
            //Fall back to last known application name if we can't get it from SteamVR
            if (app_name.empty())
            {
                app_name = ConfigManager::Get().GetAppProfileManager().GetProfile(app_key).LastApplicationName;

                //If that's still empty for some reason (shouldn't happen), fall back to app key
                if (app_name.empty())
                {
                    app_name = app_key;
                }
            }

            unique_app_keys.insert(app_key);
            app_sublist.push_back( {app_key, app_name, WStringConvertFromUTF8(app_name.c_str())} );
        }
    }

    //Sort this list before adding it to the rest
    std::sort(app_sublist.begin(), app_sublist.end(), app_sublist_compare);

    for (const auto& app : app_sublist)
    {
        m_AppList.emplace_back(app.app_key, app.app_name_utf8);
    }

    app_sublist.clear();

    //List registered apps
    if (UIManager::Get()->IsOpenVRLoaded())
    {
        uint32_t app_count = vr::VRApplications()->GetApplicationCount();

        for (uint32_t i = 0; i < app_count; ++i)
        {
            app_error = vr::VRApplications()->GetApplicationKeyByIndex(i, app_key_buffer, vr::k_unMaxApplicationKeyLength);

            if (app_error == vr::VRApplicationError_None)
            {
                const bool is_installed = vr::VRApplications()->IsApplicationInstalled(app_key_buffer);
                const bool is_overlay   = vr::VRApplications()->GetApplicationPropertyBool(app_key_buffer, vr::VRApplicationProperty_IsDashboardOverlay_Bool);
                const bool is_internal  = vr::VRApplications()->GetApplicationPropertyBool(app_key_buffer, vr::VRApplicationProperty_IsInternal_Bool);
                //Application manifests and their properties aren't really documented but these two properties don't seem to matter for us
                //const bool is_template  = vr::VRApplications()->GetApplicationPropertyBool(app_key_buffer, vr::VRApplicationProperty_IsTemplate_Bool);
                //const bool is_instanced = vr::VRApplications()->GetApplicationPropertyBool(app_key_buffer, vr::VRApplicationProperty_IsInstanced_Bool);

                if ( (is_installed) && (!is_overlay) && (!is_internal) )
                {
                    std::string app_key  = app_key_buffer;
                    std::string app_name = app_key;

                    //Only add if not already in list
                    if (unique_app_keys.find(app_key) == unique_app_keys.end())
                    {
                        vr::VRApplications()->GetApplicationPropertyString(app_key_buffer, vr::VRApplicationProperty_Name_String, app_prop_buffer, vr::k_unMaxPropertyStringSize, &app_error);

                        if (app_error == vr::VRApplicationError_None)
                        {
                            app_name = app_prop_buffer;
                        }

                        unique_app_keys.insert(app_key);
                        app_sublist.push_back( {app_key, app_name, WStringConvertFromUTF8(app_name.c_str())} );
                    }
                }
            }
        }

        //Sort this list before adding it to the rest
        std::sort(app_sublist.begin(), app_sublist.end(), app_sublist_compare);

        for (const auto& app : app_sublist)
        {
            m_AppList.emplace_back(app.app_key, app.app_name_utf8);
        }
    }
}
