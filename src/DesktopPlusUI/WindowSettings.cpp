#include "WindowSettings.h"

#include <sstream>
#include <unordered_set>
#include <shlwapi.h>

#include "ImGuiExt.h"
#include "UIManager.h"
#include "TranslationManager.h"
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
    m_Column0Width(0.0f),
    m_WarningHeight(0.0f),
    m_ProfileOverlaySelectIsSaving(false),
    m_ActionPickerID(action_none)
{
    m_WindowTitleStrID = tstr_SettingsWindowTitle;
    m_WindowIcon = tmtex_icon_xsmall_settings;
    m_OvrlWidth = OVERLAY_WIDTH_METERS_SETTINGS;

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

void WindowSettings::ClearCachedTranslationStrings()
{
    m_WarningTextOverlayError.clear();
    m_WarningTextWinRTError.clear();
    m_WarningTextAppProfile.clear();
    m_BrowserMaxFPSValueText.clear();
    m_BrowserBlockListCountText.clear();
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
        m_PageAnimationProgress += ImGui::GetIO().DeltaTime * 3.0f;

        if (m_PageAnimationProgress >= 1.0f)
        {
            //Remove pages in the stack after finishing going back
            if (m_PageAnimationDir == 1)
            {
                while ((int)m_PageStack.size() > m_PageStackPosAnimation + 1)
                {
                    m_PageStack.pop_back();
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
    else if (m_IsWindowAppearing) //Set appearing value when the whole window appeared again
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

        //Disable items when the page isn't active
        const bool is_inactive_page = (child_id + 1 < stack_size);

        if (is_inactive_page)
        {
            ImGui::PushItemDisabledNoVisual();
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f)); //This prevents child bg color being visible if there's a widget before this (e.g. warnings)

        if ( (ImGui::BeginChild(child_str_id[child_id], child_size, false, ImGuiWindowFlags_NavFlattened)) || (m_PageAppearing == page_id) ) //Process page if currently appearing
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
                case wndsettings_page_color_picker:            UpdatePageColorPicker();             break;
                case wndsettings_page_profile_picker:          UpdatePageProfilePicker();           break;
                case wndsettings_page_action_picker:           UpdatePageActionPicker();            break;
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

        if (is_inactive_page)
        {
            ImGui::SameLine();
        }

        child_id++;
    }

    m_PageAppearing = wndsettings_page_none;

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
                if (ImGui::Selectable(TranslationManager::GetString(tstr_ActionSwitchTask)))
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_do, action_switch_task);
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

void WindowSettings::UpdatePageMain()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::BeginChild("SettingsMainContent", ImVec2(0.00f, 0.00f), false, ImGuiWindowFlags_NavFlattened);
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
    //Interface
    {
        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatInterface)); 
        ImGui::Columns(2, "ColumnInterface", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( TranslationManager::GetString(tstr_SettingsInterfaceLanguage) );
        ImGui::NextColumn();

        ImGui::PushItemWidth(-1);
        if (ImGui::BeginComboAnimated("##ComboLang", TranslationManager::Get().GetCurrentTranslationName() ))
        {
            static std::vector< std::pair<std::string, std::string> > list_langs; //filename, list name
            static int list_id = 0;

            if ( (ImGui::IsWindowAppearing()) && (list_langs.empty()) )
            {
                //Load language list
                list_id = 0;

                const std::string& current_filename = ConfigManager::GetValue(configid_str_interface_language_file);
                const std::wstring wpath = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "lang/*.ini").c_str() );
                WIN32_FIND_DATA find_data;
                HANDLE handle_find = ::FindFirstFileW(wpath.c_str(), &find_data);

                if (handle_find != INVALID_HANDLE_VALUE)
                {
                    do
                    {
                        const std::string filename_utf8 = StringConvertFromUTF16(find_data.cFileName);
                        const std::string name = TranslationManager::GetTranslationNameFromFile(filename_utf8);

                        //If name could be read, add to list
                        if (!name.empty())
                        {
                            list_langs.push_back( std::make_pair(filename_utf8, name) );

                            //Select matching entry when appearing
                            if (filename_utf8 == current_filename)
                            {
                                list_id = (int)list_langs.size() - 1;
                            }
                        }
                    }
                    while (::FindNextFileW(handle_find, &find_data) != 0);

                    ::FindClose(handle_find);
                }
            }

            int i = 0;
            for (const auto& item : list_langs)
            {
                if (ImGui::Selectable(item.second.c_str(), (list_id == i)))
                {
                    ConfigManager::SetValue(configid_str_interface_language_file, item.first);
                    TranslationManager::Get().LoadTranslationFromFile(item.first);
                    UIManager::Get()->OnTranslationChanged();

                    list_id = i;
                }

                i++;
            }

            ImGui::EndCombo();
        }

        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Indent();

        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsInterfaceAdvancedSettings), &ConfigManager::GetRef(configid_bool_interface_show_advanced_settings)))
        {
            UIManager::Get()->RepeatFrame();
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
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

        if (ImGui::ColorButton("##BackgroundColor", background_color_vec4, ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop))
        {
            PageGoForward(wndsettings_page_color_picker);
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

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
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsEnvironmentDimInterfaceTip));

        ImGui::Columns(1);
    }
}

void WindowSettings::UpdatePageMainCatActions()
{
    //Actions (strings are not translatable here since this is just temp stuff)
    {
        ImGui::Spacing();

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Actions");

        ImGui::Indent();

        if (ImGui::Button("Switch to Action Editor"))
        {
            UIManager::Get()->RestartIntoActionEditor();
        }

        ImGui::Unindent();
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
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
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
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
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
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsMouseShowCursorGCActiveWarning), "(!)");
        }
        else if (!DPWinRT_IsCaptureCursorEnabledPropertySupported())
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsMouseShowCursorGCUnsupported), "(!)");
        }

        bool& scroll_smooth = ConfigManager::GetRef(configid_bool_input_mouse_scroll_smooth);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsMouseScrollSmooth), &scroll_smooth))
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_input_mouse_scroll_smooth, scroll_smooth);
        }

        bool& pointer_override = ConfigManager::Get().GetRef(configid_bool_input_mouse_allow_pointer_override);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsMouseAllowLaserPointerOverride), &pointer_override))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_mouse_allow_pointer_override), pointer_override);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsMouseAllowLaserPointerOverrideTip));

        ImGui::Unindent();

        //Double-Click Assistant
        ImGui::Columns(2, "ColumnMouse", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsMouseDoubleClickAssist)); 
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
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

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_mouse_dbl_click_assist_duration_ms), assist_duration);
        }
        vr_keyboard.VRKeyboardInputEnd();

        ImGui::Columns(1);
    }

    //Laser Pointer
    {
        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatLaserPointer));
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
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
            bool& strict_matching = ConfigManager::GetRef(configid_bool_windows_winrt_window_matching_strict);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysStrictMatching), &strict_matching))
            {
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_windows_winrt_window_matching_strict, strict_matching);
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsWindowOverlaysStrictMatchingTip));
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
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsPerformanceRapidUpdatesTip));

            ImGui::NextColumn();
            ImGui::NextColumn();

            bool& single_desktop = ConfigManager::Get().GetRef(configid_bool_performance_single_desktop_mirroring);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsPerformanceSingleDesktopMirror), &single_desktop))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_performance_single_desktop_mirroring), single_desktop);
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsPerformanceSingleDesktopMirrorTip));

            ImGui::NextColumn();
            ImGui::Spacing();       //Only use additional spacing when this is visible since it looks odd with just 2 checkboxes
        }

        bool& show_fps = ConfigManager::Get().GetRef(configid_bool_performance_show_fps);
        ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsPerformanceShowFPS), &show_fps);

        ImGui::Columns(1);
    }
}

void WindowSettings::UpdatePageMainCatMisc()
{
    static bool is_autolaunch_enabled = false;

    if ( (UIManager::Get()->IsOpenVRLoaded()) && (m_PageAppearing == wndsettings_page_main) )
    {
        is_autolaunch_enabled = vr::VRApplications()->GetApplicationAutoLaunch(g_AppKeyDashboardApp);
    }

    //Version Info
    {
        ImGui::Spacing();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatVersionInfo)); 
        ImGui::Columns(2, "ColumnVersion", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::TextUnformatted("Desktop+ NewUI Preview 9");

        ImGui::Columns(1);
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
        if (ConfigManager::GetValue(configid_bool_interface_warning_welcome_hidden))
            warning_hidden_count++;

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
            ConfigManager::SetValue(configid_bool_interface_warning_welcome_hidden,                  false);

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
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
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
        ImGui::TextUnformatted("Desktop+");
        ImGui::NextColumn();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingRestart)))
        {
            UIManager::Get()->RestartDashboardApp();
        }

        bool has_restart_steam_button = ( (ConfigManager::Get().IsSteamInstall()) && (!ConfigManager::GetValue(configid_bool_state_misc_process_started_by_steam)) );

        if (has_restart_steam_button)
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

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
            const bool dashboard_app_running  = IPCManager::IsDashboardAppRunning();

            if (!dashboard_app_running)
                ImGui::PushItemDisabled();

            if (!has_restart_steam_button)
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

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

            if (!dashboard_app_running)
                ImGui::PopItemDisabled();
        }

        ImGui::NextColumn();

        ImGui::TextUnformatted("Desktop+ UI");
        ImGui::NextColumn();

        ImGui::PushID("##UI");

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingRestart)))
        {
            UIManager::Get()->Restart(false);
        }

        ImGui::PopID();

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

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
            border_min.y -= ImGui::GetStyle().ItemSpacing.y + 1;
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
            else if ( (ImGui::IsItemDeactivated()) && (use_lazy_resize) && (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_room) )
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
            else if ( (ImGui::IsItemDeactivated()) && (use_lazy_resize) && (window.GetOverlayStateCurrentID() == floating_window_ovrl_state_dashboard_tab) )
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

void WindowSettings::UpdatePageKeyboardLayout()
{
    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    static int list_id = -1;
    static std::vector<KeyboardLayoutMetadata> list_layouts;
    static bool cluster_enabled_prev[kbdlayout_cluster_MAX] = {false};
    
    if (m_PageAppearing == wndsettings_page_keyboard)
    {
        //Show the keyboard since that's probably useful
        vr_keyboard.GetWindow().Show();

        //Load layout list
        list_layouts.clear();
        list_id = -1;

        const std::string& current_filename = ConfigManager::GetValue(configid_str_input_keyboard_layout_file);
        const std::wstring wpath = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "keyboards/*.ini").c_str() );
        WIN32_FIND_DATA find_data;
        HANDLE handle_find = ::FindFirstFileW(wpath.c_str(), &find_data);

        if (handle_find != INVALID_HANDLE_VALUE)
        {
            do
            {
                KeyboardLayoutMetadata metadata = VRKeyboard::LoadLayoutMetadataFromFile( StringConvertFromUTF16(find_data.cFileName) );

                //If base cluster exists, layout is probably valid, add to list
                if (metadata.HasCluster[kbdlayout_cluster_base])
                {
                    list_layouts.push_back(metadata);

                    //Select matching entry when appearing
                    if (list_layouts.back().FileName == current_filename)
                    {
                        list_id = (int)list_layouts.size() - 1;
                    }
                }
            }
            while (::FindNextFileW(handle_find, &find_data) != 0);

            ::FindClose(handle_find);
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
    ImGui::BeginChild("LayoutList", ImVec2(0.0f, (item_height * 13.0f) + inner_padding - m_WarningHeight), true);

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
        if (list_id != -1)
        {
            ConfigManager::SetValue(configid_str_input_keyboard_layout_file, list_layouts[list_id].FileName);
        }

        PageGoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        //Restore previous cluster settings
        ConfigManager::SetValue(configid_bool_input_keyboard_cluster_function_enabled,   cluster_enabled_prev[kbdlayout_cluster_function]);
        ConfigManager::SetValue(configid_bool_input_keyboard_cluster_navigation_enabled, cluster_enabled_prev[kbdlayout_cluster_navigation]);
        ConfigManager::SetValue(configid_bool_input_keyboard_cluster_numpad_enabled,     cluster_enabled_prev[kbdlayout_cluster_numpad]);
        ConfigManager::SetValue(configid_bool_input_keyboard_cluster_extra_enabled,      cluster_enabled_prev[kbdlayout_cluster_extra]);

        vr_keyboard.LoadCurrentLayout();

        PageGoBack();
    }
}

void WindowSettings::UpdatePageProfiles()
{
    ImGuiStyle& style = ImGui::GetStyle();

    bool scroll_to_selection = false;
    static int list_id = -1;
    static bool used_profile_save_new_page = false;
    static bool delete_confirm_state       = false;
    static bool has_loading_failed         = false;
    static bool has_deletion_failed        = false;
    static float list_buttons_width        = 0.0f;

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
        ImVec2 text_size_delete  = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDelete));
        ImVec2 text_size_confirm = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDeleteConfirm));
        m_CachedSizes.Profiles_ButtonDeleteSize = (text_size_delete.x > text_size_confirm.x) ? text_size_delete : text_size_confirm;

        m_CachedSizes.Profiles_ButtonDeleteSize.x += style.FramePadding.x * 2.0f;
        m_CachedSizes.Profiles_ButtonDeleteSize.y += style.FramePadding.y * 2.0f;

        UIManager::Get()->RepeatFrame();
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsProfilesOverlaysHeader) ); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    ImGui::BeginChild("ProfileList", ImVec2(0.0f, (item_height * 15.0f) + inner_padding - m_WarningHeight), true);

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
        }
        ImGui::PopID();

        if ( (scroll_to_selection) && (index == list_id) )
        {
            ImGui::SetScrollHereY();
        }

        index++;
    }

    ImGui::EndChild();
    ImGui::Spacing();

    const bool is_none  = (list_id == -1);
    const bool is_first = (list_id == 0);
    const bool is_last  = (list_id == m_ProfileList.size() - 1);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - list_buttons_width);

    ImGui::BeginGroup();

    if ( (is_last) || (is_none) )
        ImGui::PushItemDisabled();

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
                m_ProfileSelectionName = m_ProfileList[list_id];
                list_id--;

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
    if (m_PageStack[0] != wndsettings_page_profiles)
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
                    if (ImGui::StringContainsUnmappedCharacter(pair.first.c_str()))
                    {
                        if (TextureManager::Get().AddFontBuilderString(pair.first))
                        {
                            TextureManager::Get().ReloadAllTexturesLater();
                            UIManager::Get()->RepeatFrame();
                        }
                    }
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
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileSaveSelectNameErrorTaken), "(!)");
        }
        else if (is_name_blank)
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
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
            if (ImGui::StringContainsUnmappedCharacter(buffer_profile_name))
            {
                if (TextureManager::Get().AddFontBuilderString(buffer_profile_name))
                {
                    TextureManager::Get().ReloadAllTexturesLater();
                    UIManager::Get()->RepeatFrame();
                }
            }
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
        ImGui::BeginChild("OverlayList", ImVec2(0.0f, (item_height * ((m_ProfileOverlaySelectIsSaving) ? 12.0f : 13.0f) ) + inner_padding - m_WarningHeight), true);

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
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::SetCursorPosY(text_y);

                ImGui::TextUnformatted(pair.first.c_str());
            }

            ImGui::PopID();

            index++;
        }

        ImGui::EndChild();
        ImGui::Spacing();

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
    static float delete_button_width       = 0.0f;
    static float active_header_text_width  = 0.0f;
    static ImVec2 no_apps_text_size;

    if (m_PageAppearing == wndsettings_page_app_profiles)
    {
        RefreshAppList();

        list_id = 0;
        app_profile_selected_edit = app_profiles.GetProfile((m_AppList.empty()) ? "" : m_AppList[0].first);
        delete_confirm_state = false;

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
        ImVec2 text_size_delete  = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDelete));
        ImVec2 text_size_confirm = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDeleteConfirm));
        m_CachedSizes.Profiles_ButtonDeleteSize = (text_size_delete.x > text_size_confirm.x) ? text_size_delete : text_size_confirm;

        m_CachedSizes.Profiles_ButtonDeleteSize.x += style.FramePadding.x * 2.0f;
        m_CachedSizes.Profiles_ButtonDeleteSize.y += style.FramePadding.y * 2.0f;

        UIManager::Get()->RepeatFrame();
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsProfilesAppsHeader) ); 

    if (!UIManager::Get()->IsOpenVRLoaded())
    {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsProfilesAppsHeaderNoVRTip));
    }

    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 15.0f : 11.0f;
    ImGui::BeginChild("AppList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //Reset scroll when appearing
    if (m_PageAppearing == wndsettings_page_app_profiles)
    {
        ImGui::SetScrollY(0.0f);
    }

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
            app_profile_selected_edit.OverlayProfileFileName = m_ProfilePickerName;

            store_profile_changes = true;
            m_PageReturned = wndsettings_page_none;
        }
        else if (m_PageReturned == wndsettings_page_action_picker)
        {
            if (is_action_picker_for_leave)
            {
                app_profile_selected_edit.ActionIDLeave = m_ActionPickerID;
            }
            else
            {
                app_profile_selected_edit.ActionIDEnter = m_ActionPickerID;
            }

            store_profile_changes = true;
            m_PageReturned = wndsettings_page_none;
        }

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), app_name_selected.c_str());

        if (is_active_profile)
        {
            ImGui::SameLine();
            ImGui::TextColoredUnformatted(Style_ImGuiCol_TextNotification, TranslationManager::GetString(tstr_SettingsProfilesAppsProfileHeaderActive));

            active_header_text_width = ImGui::GetItemRectSize().x;
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

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesAppsProfileActionEnter));
        ImGui::NextColumn();

        ImGui::PushID("ButtonActionEnter");
        if (ImGui::Button(ConfigManager::Get().GetActionManager().GetActionName(app_profile_selected_edit.ActionIDEnter)))
        {
            m_ActionPickerID = app_profile_selected_edit.ActionIDEnter;
            is_action_picker_for_leave = false;
            PageGoForward(wndsettings_page_action_picker);
        }
        ImGui::PopID();

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesAppsProfileActionLeave));
        ImGui::NextColumn();

        ImGui::PushID("ButtonActionLeave");
        if (ImGui::Button(ConfigManager::Get().GetActionManager().GetActionName(app_profile_selected_edit.ActionIDLeave)))
        {
            m_ActionPickerID = app_profile_selected_edit.ActionIDLeave;
            is_action_picker_for_leave = true;
            PageGoForward(wndsettings_page_action_picker);
        }
        ImGui::PopID();

        ImGui::Columns(1);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - delete_button_width);

        if (delete_confirm_state)
        {
            if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDeleteConfirm), m_CachedSizes.Profiles_ButtonDeleteSize))
            {
                IPCManager::Get().SendStringToDashboardApp(configid_str_state_app_profile_key, app_key_selected, UIManager::Get()->GetWindowHandle());
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_app_profile_remove);

                app_profiles.RemoveProfile(app_key_selected);
                app_profile_selected_edit = {};

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
        }
    }

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons (don't show when used as root page)
    if (m_PageStack[0] != wndsettings_page_app_profiles)
    {
        ImGui::Separator();

        if (ImGui::Button(TranslationManager::GetString(tstr_DialogDone))) 
        {
            PageGoBack();
        }
    }
}

void WindowSettings::UpdatePageColorPicker()
{
    static ImVec4 color_current;
    static ImVec4 color_original;
    const ConfigID_Int config_id = configid_int_interface_background_color; //Currently only need this one, so we keep it simple

    if (m_PageAppearing == wndsettings_page_color_picker)
    {
        color_current  = ImGui::ColorConvertU32ToFloat4( pun_cast<ImU32, int>(ConfigManager::Get().GetValue(config_id)) );
        color_original = color_current;
    }

    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogColorPickerHeader)); 
    ImGui::Indent();

    vr_keyboard.VRKeyboardInputBegin("#ColorPicker");
    if (ImGui::ColorPicker4Simple("#ColorPicker", &color_current.x, &color_original.x, 
                                  TranslationManager::GetString(tstr_DialogColorPickerCurrent), TranslationManager::GetString(tstr_DialogColorPickerOriginal)))
    {
        int rgba = pun_cast<int, ImU32>( ImGui::ColorConvertFloat4ToU32(color_current) );

        ConfigManager::Get().SetValue(config_id, rgba);
        IPCManager::Get().PostConfigMessageToDashboardApp(config_id, rgba);
    }
    vr_keyboard.VRKeyboardInputEnd();
    
    ImGui::Unindent();
    
    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons
    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogOk))) 
    {
        PageGoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        //Restore previous setting
        ImU32 rgba = ImGui::ColorConvertFloat4ToU32(color_original);

        ConfigManager::Get().SetValue(config_id, *(int*)&rgba);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(config_id), *(int*)&rgba);

        PageGoBack();
    }
}

void WindowSettings::UpdatePageProfilePicker()
{
    ImGuiStyle& style = ImGui::GetStyle();

    bool scroll_to_selection = false;
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
        const auto it = std::find(m_ProfileList.begin(), m_ProfileList.end(), m_ProfilePickerName);

        if (it != m_ProfileList.end())
        {
            list_id = (int)std::distance(m_ProfileList.begin(), it);
            scroll_to_selection = true;
        }
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogProfilePickerHeader)); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 19.0f : 15.0f;
    ImGui::BeginChild("ProfilePickerList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //List profiles
    int index = 0;
    for (const auto& name : m_ProfileList)
    {
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
        }

        index++;
    }

    ImGui::EndChild();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Cancel button
    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageActionPicker()
{
    ImGuiStyle& style = ImGui::GetStyle();

    bool scroll_to_selection = false;
    static ActionID list_id = action_none;

    if (m_PageAppearing == wndsettings_page_action_picker)
    {
        list_id = m_ActionPickerID;
        scroll_to_selection = true;
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DialogActionPickerHeader)); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 19.0f : 15.0f;
    ImGui::BeginChild("ActionPickerList", ImVec2(0.0f, (item_height * item_count) + inner_padding - m_WarningHeight), true);

    //List default actions
    for (int i = 0; i < action_built_in_MAX; ++i)
    {
        if (ImGui::Selectable(ActionManager::Get().GetActionName((ActionID)i), (i == list_id)))
        {
            list_id = (ActionID)i;
            m_ActionPickerID = list_id;

            PageGoBack();
        }

        if ( (scroll_to_selection) && ((ActionID)i == list_id) )
        {
            ImGui::SetScrollHereY();
        }
    }

    //List custom actions
    int act_index = 0;
    for (CustomAction& action : ConfigManager::Get().GetCustomActions())
    {
        ActionID action_id = (ActionID)(act_index + action_custom);

        ImGui::PushID(&action);
        if (ImGui::Selectable(ActionManager::Get().GetActionName(action_id), (action_id == list_id) ))
        {
            list_id = action_id;
            m_ActionPickerID = action_id;

            PageGoBack();
        }
        ImGui::PopID();

        if ( (scroll_to_selection) && (action_id == list_id) )
        {
            ImGui::SetScrollHereY();
        }

        act_index++;
    }

    ImGui::EndChild();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Cancel button
    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }
}

void WindowSettings::UpdatePageResetConfirm()
{
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsReset) ); 
    ImGui::Indent();

    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsResetConfirmDescription));
    ImGui::PopTextWrapPos();

    ImGui::Unindent();

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons
    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsResetConfirmButton))) 
    {
        //Do the reset
        ConfigManager::Get().RestoreConfigFromDefault();

        UIManager::Get()->Restart(UIManager::Get()->IsInDesktopMode()); //This shouldn't be necessary, but let's still do it to really ensure clean state

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
}

void WindowSettings::PageGoForward(WindowSettingsPage new_page)
{
    m_PageStack.push_back(new_page);
    m_PageStackPos++;
}

void WindowSettings::PageGoBack()
{
    if (m_PageStackPos != 0)
    {
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
    m_PageStackPos = 0;
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
