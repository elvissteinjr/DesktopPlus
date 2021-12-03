#include "WindowSettings.h"

#include <sstream>
#include <shlwapi.h>

#include "ImGuiExt.h"
#include "UIManager.h"
#include "TranslationManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"

WindowSettingsNew::WindowSettingsNew() :
    m_PageStackPos(0),
    m_PageStackPosAnimation(0),
    m_PageAnimationDir(0),
    m_PageAnimationProgress(0.0f),
    m_PageAnimationStartPos(0.0f),
    m_PageAnimationOffset(0.0f),
    m_PageAppearing(wndsettings_page_none),
    m_IsScrolling(false),
    m_ScrollMainCurrent(0.0f),
    m_ScrollMainMaxPos(0.0f),
    m_ScrollProgress(0.0f),
    m_ScrollStartPos(0.0f),
    m_ScrollTargetPos(0.0f),
    m_Column0Width(0.0f),
    m_WarningHeight(0.0f),
    m_ProfileOverlaySelectIsSaving(false)
{
    m_WindowTitleStrID = tstr_SettingsWindowTitle;
    m_WindowIcon = tmtex_icon_xsmall_settings;
    m_OvrlWidth = OVERLAY_WIDTH_METERS_SETTINGS;

    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect rect = UITextureSpaces::Get().GetRect(ui_texspace_settings);
    m_Size = {float(rect.GetWidth() - 4), float(rect.GetHeight() - 4)};
    m_Pos =  {float(rect.GetTL().x + 2),  float(rect.GetTL().y + 2)};

    m_PageStack.push_back(wndsettings_page_main);
    std::fill(std::begin(m_ScrollMainCatPos), std::end(m_ScrollMainCatPos), -1.0f);

    ResetTransform();
}

void WindowSettingsNew::Hide(bool skip_fade)
{
    FloatingWindow::Hide();

    ConfigManager::Get().SaveConfigToFile();
}

void WindowSettingsNew::ResetTransform()
{
    m_Transform.identity();
    m_Transform.rotateY(-15.0f);
    m_Transform.translate_relative(OVERLAY_WIDTH_METERS_DASHBOARD_UI / 3.0f, 0.70f, 0.15f);
}

vr::VROverlayHandle_t WindowSettingsNew::GetOverlayHandle() const
{
    return UIManager::Get()->GetOverlayHandleSettings();
}

void WindowSettingsNew::ClearCachedTranslationStrings()
{
    m_WarningTextOverlayError = "";
    m_WarningTextWinRTError = "";
}

void WindowSettingsNew::WindowUpdate()
{
    ImGui::SetWindowSize(m_Size);

    ImGuiStyle& style = ImGui::GetStyle();

    if (m_Column0Width == 0.0f)
    {
        m_Column0Width = ImGui::GetFontSize() * 12.75f;
    }

    const float page_width = m_Size.x - style.WindowBorderSize - style.WindowPadding.x - style.WindowPadding.x;

    //Page animation
    if (m_PageAnimationDir != 0)
    {
        float target_x = (page_width + style.ItemSpacing.x) * -m_PageStackPosAnimation;
        m_PageAnimationProgress += ImGui::GetIO().DeltaTime * 3.0f;

        m_PageAnimationOffset = smoothstep(m_PageAnimationProgress, m_PageAnimationStartPos, target_x);

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

            m_PageAnimationOffset = target_x;
            m_PageAnimationDir    = 0;
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
    else if (ImGui::IsWindowAppearing()) //Set appearing value when the whole window appeared again
    {
        m_PageAppearing = m_PageStack.back();
    }

    UpdateWarnings();

    //Set up page offset and clipping
    ImGui::SetCursorPosX( ImGui::GetCursorPosX() + m_PageAnimationOffset);

    ImGui::PushClipRect({m_Pos.x + style.WindowBorderSize, 0.0f}, {m_Pos.x + m_Size.x - style.WindowBorderSize, FLT_MAX}, false);

    const char* const child_str_id[] {"SettingsPageMain", "SettingsPage1", "SettingsPage2", "SettingsPage3"}; //No point in generating these on the fly
    int child_id = 0;
    int stack_size = (int)m_PageStack.size();
    for (WindowSettingsPage page_id : m_PageStack)
    {
        if (child_id >= IM_ARRAYSIZE(child_str_id))
            break;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));

        if ( (ImGui::BeginChild(child_str_id[child_id], {page_width, ImGui::GetContentRegionAvail().y})) || (m_PageAppearing == page_id) ) //Process page if currently appearing
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg

            switch (page_id)
            {
                case wndsettings_page_main:                    UpdatePageMain();                    break;
                case wndsettings_page_keyboard:                UpdatePageKeyboardLayout();          break;
                case wndsettings_page_profiles:                UpdatePageProfiles();                break;
                case wndsettings_page_profiles_overlay_select: UpdatePageProfilesOverlaySelect();   break;
                case wndsettings_page_reset_confirm:           UpdatePageResetConfirm();            break;
                default: break;
            }
        }
        else
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg
        }

        ImGui::EndChild();

        child_id++;

        if (stack_size > child_id)
        {
            ImGui::SameLine();
        }
    }

    m_PageAppearing = wndsettings_page_none;

    ImGui::PopClipRect();
}

void WindowSettingsNew::UpdateWarnings()
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
            if (ImGui::BeginPopup("DontShowAgain"))
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
            if (ImGui::BeginPopup("DontShowAgain2"))
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
            if (ImGui::BeginPopup("DontShowAgain3"))
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
            if (ImGui::BeginPopup("DontShowAgain4"))
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

    //Focused process elevation warning
    {
        if (  (ConfigManager::GetValue(configid_bool_state_window_focused_process_elevated)) && (!ConfigManager::GetValue(configid_bool_state_misc_process_elevated)) && 
             (!ConfigManager::GetValue(configid_bool_state_misc_elevated_mode_active))       && (!ConfigManager::GetValue(configid_bool_state_misc_uiaccess_enabled)) )
        {
            SelectableWarning("##WarningElevation2", "FocusedElevatedContext", TranslationManager::GetString(tstr_SettingsWarningElevatedProcessFocus));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);
            if (ImGui::BeginPopup("FocusedElevatedContext"))
            {
                if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsWarningMenuFocusTry)))
                {
                    UIManager::Get()->TryChangingWindowFocus();
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
            if (ImGui::BeginPopup("DontShowAgain6"))
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
            if (ImGui::BeginPopup("DismissWarning"))
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
            if (ImGui::BeginPopup("DismissWarning2"))
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

void WindowSettingsNew::UpdatePageMain()
{
    static int jumpto_item_id = 0;
    const TRMGRStrID jumpto_strings[wndsettings_cat_MAX] = 
    {
        tstr_SettingsCatInterface, 
        tstr_SettingsCatProfiles,
        tstr_SettingsCatActions,
        tstr_SettingsCatKeyboard,
        tstr_SettingsCatLaserPointer,
        tstr_SettingsCatWindowOverlays,
        tstr_SettingsCatVersionInfo,
        tstr_SettingsCatWarnings,
        tstr_SettingsCatStartup,
        tstr_SettingsCatTroubleshooting
    };

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted( TranslationManager::GetString(tstr_SettingsJumpTo) );
    ImGui::SameLine();

    const float combo_width = ImGui::GetFontSize() * 15.0f;
    ImGui::SetCursorPosX( ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - combo_width );
    ImGui::PushItemWidth(combo_width);
    if (ImGui::BeginComboAnimated("##JumpTo", TranslationManager::GetString(jumpto_strings[jumpto_item_id]) ))
    {
        int i = 0;
        for (const auto& item : jumpto_strings)
        {
            if (m_ScrollMainCatPos[i] != -1.0f) //Don't list if not visible (never set scroll position)
            {
                if (ImGui::Selectable(TranslationManager::GetString(item), (jumpto_item_id == i)))
                {
                    //Scroll to item
                    m_ScrollTargetPos = std::min(m_ScrollMainCatPos[i], m_ScrollMainMaxPos);
                    m_IsScrolling = true;
                    m_ScrollProgress = 0.0f;
                    m_ScrollStartPos = m_ScrollMainCurrent;

                    jumpto_item_id = i;
                }
            }
            i++;
        }

        ImGui::EndCombo();
    }

    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::BeginChild("SettingsMainContent");
    ImGui::PopStyleColor();

    //Page Content
    UpdatePageMainCatInterface();
    UpdatePageMainCatProfiles();
    UpdatePageMainCatActions();
    UpdatePageMainCatInput();
    UpdatePageMainCatWindows();
    UpdatePageMainCatMisc();

    //Scrolling
    m_ScrollMainCurrent = ImGui::GetScrollY();
    m_ScrollMainMaxPos  = ImGui::GetScrollMaxY();

    if (m_IsScrolling)
    {
        m_ScrollProgress += ImGui::GetIO().DeltaTime * 3.0f;

        if (m_ScrollProgress >= 1.0f)
        {
            m_ScrollProgress = 1.0f;
            m_IsScrolling = false;
        }

        float scroll = smoothstep(m_ScrollProgress, m_ScrollStartPos, m_ScrollTargetPos);
        ImGui::SetScrollY(scroll);
    }

    ImGui::EndChild();
}

void WindowSettingsNew::UpdatePageMainCatInterface()
{
    //Interface
    {
        m_ScrollMainCatPos[wndsettings_cat_interface] = ImGui::GetCursorPosY();

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
    }
}

void WindowSettingsNew::UpdatePageMainCatActions()
{
    //Actions (strings are not translatable here since this is just temp stuff)
    {
        ImGui::Spacing();
        m_ScrollMainCatPos[wndsettings_cat_actions] = ImGui::GetCursorPosY();

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Actions");

        ImGui::Indent();

        if (ImGui::Button("Switch to Action Editor"))
        {
            UIManager::Get()->RestartIntoActionEditor();
        }

        ImGui::Unindent();
    }
}

void WindowSettingsNew::UpdatePageMainCatProfiles()
{
    //Profiles
    {
        m_ScrollMainCatPos[wndsettings_cat_profiles] = ImGui::GetCursorPosY();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatProfiles)); 
        ImGui::Columns(2, "ColumnInterface", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsProfilesOverlays));
        ImGui::NextColumn();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesManage)))
        {
            PageGoForward(wndsettings_page_profiles);
        }

        ImGui::Columns(1);
    }
}

void WindowSettingsNew::UpdatePageMainCatInput()
{
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    //Keyboard
    {
        ImGui::Spacing();
        m_ScrollMainCatPos[wndsettings_cat_keyboard] = ImGui::GetCursorPosY();

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

        float& size = ConfigManager::GetRef(configid_float_input_keyboard_detached_size);

        vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("KeyboardSize") );
        if (ImGui::SliderWithButtonsFloatPercentage("KeyboardSize", size, 5, 1, 50, 200, "%d%%"))
        {
            if (size < 0.10f)
                size = 0.10f;

            UIManager::Get()->GetVRKeyboard().GetWindow().UpdateOverlaySize();
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

        ImGui::Columns(1);
    }

    //Laser Pointer
    {
        ImGui::Spacing();
        m_ScrollMainCatPos[wndsettings_cat_laser_pointer] = ImGui::GetCursorPosY();

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

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_float_input_detached_interaction_max_distance, pun_cast<LPARAM, float>(distance));
        }
        vr_keyboard.VRKeyboardInputEnd();

        ImGui::Columns(1);
    }
}

void WindowSettingsNew::UpdatePageMainCatWindows()
{
    //Window Overlays
    {
        ImGui::Spacing();
        m_ScrollMainCatPos[wndsettings_cat_window_overlays] = ImGui::GetCursorPosY();

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

void WindowSettingsNew::UpdatePageMainCatMisc()
{
    static bool is_autolaunch_enabled = false;

    if ( (UIManager::Get()->IsOpenVRLoaded()) && (m_PageAppearing == wndsettings_page_main) )
    {
        is_autolaunch_enabled = vr::VRApplications()->GetApplicationAutoLaunch(g_AppKeyDashboardApp);
    }

    //Version Info
    {
        ImGui::Spacing();
        m_ScrollMainCatPos[wndsettings_cat_version_info] = ImGui::GetCursorPosY();

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsCatVersionInfo)); 
        ImGui::Columns(2, "ColumnVersion", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::TextUnformatted("Desktop+ NewUI Preview 3");

        ImGui::Columns(1);
    }

    //Warnings
    {
        ImGui::Spacing();
        m_ScrollMainCatPos[wndsettings_cat_warnings] = ImGui::GetCursorPosY();

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
        if (ConfigManager::GetValue(configid_bool_interface_warning_welcome_hidden))
            warning_hidden_count++;

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsWarningsHidden));
        ImGui::SameLine();
        ImGui::Text("%i", warning_hidden_count);

        ImGui::NextColumn();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsWarningsReset)))
        {
            ConfigManager::SetValue(configid_bool_interface_warning_compositor_quality_hidden, false);
            ConfigManager::SetValue(configid_bool_interface_warning_compositor_res_hidden,     false);
            ConfigManager::SetValue(configid_bool_interface_warning_process_elevation_hidden,  false);
            ConfigManager::SetValue(configid_bool_interface_warning_elevated_mode_hidden,      false);
            ConfigManager::SetValue(configid_bool_interface_warning_welcome_hidden,            false);

            UIManager::Get()->UpdateAnyWarningDisplayedState();
        }

        ImGui::Columns(1);
    }

    //Startup
    bool& no_steam = ConfigManager::GetRef(configid_bool_misc_no_steam);

    if ( (ConfigManager::Get().IsSteamInstall()) || (UIManager::Get()->IsOpenVRLoaded()) ) //Only show if Steam install or we can access OpenVR settings
    {
        ImGui::Spacing();
        m_ScrollMainCatPos[wndsettings_cat_startup] = ImGui::GetCursorPosY();

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
        m_ScrollMainCatPos[wndsettings_cat_troubleshooting] = ImGui::GetCursorPosY();

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

void WindowSettingsNew::UpdatePageKeyboardLayout()
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

void WindowSettingsNew::UpdatePageProfiles()
{
    ImGuiStyle& style = ImGui::GetStyle();

    bool scroll_to_selection = false;
    static int list_id = -1;
    static bool used_profile_save_new_page = false;
    static bool delete_confirm_state       = false;
    static bool has_loading_failed         = false;
    static bool has_deletion_failed        = false;
    static float list_buttons_width        = 0.0f;
    static ImVec2 button_delete_size;
    
    if (m_PageAppearing == wndsettings_page_profiles)
    {
        //Load profile list
        m_ProfileList = ConfigManager::Get().GetOverlayProfileList();
        list_id = -1;
        delete_confirm_state = false;
        has_deletion_failed = false;

        //Figure out size for delete button. We need it to stay the same but also consider the case of the confirm text being longer in some languages
        ImVec2 text_size_delete  = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDelete));
        ImVec2 text_size_confirm = ImGui::CalcTextSize(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDeleteConfirm));
        button_delete_size = (text_size_delete.x > text_size_confirm.x) ? text_size_delete : text_size_confirm;
        
        button_delete_size.x += style.FramePadding.x * 2.0f;
        button_delete_size.y += style.FramePadding.y * 2.0f;

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

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsProfilesOverlaysHeader) ); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFontSize() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.FramePadding.y + style.ItemInnerSpacing.y;
    ImGui::BeginChild("LayoutList", ImVec2(0.0f, (item_height * 15.0f) + inner_padding - m_WarningHeight), true);

    //List layouts
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
            //Adjust current overlay ID for UI since this may have made the old selection invalid
            int& current_overlay = ConfigManager::Get().GetRef(configid_int_interface_overlay_current_id);
            current_overlay = clamp(current_overlay, 0, (int)OverlayManager::Get().GetOverlayCount() - 1);

            //Adjust overlay properties window
            WindowOverlayProperties& properties_window = UIManager::Get()->GetOverlayPropertiesWindow();

            //Hide window if overlay ID no longer in range
            if (properties_window.GetActiveOverlayID() >= OverlayManager::Get().GetOverlayCount())
            {
                properties_window.SetActiveOverlayID(k_ulOverlayID_None, true);
                properties_window.Hide();
            }
            else //Just adjust switch if it is still is
            {
                properties_window.SetActiveOverlayID(properties_window.GetActiveOverlayID(), true);
            }

            UIManager::Get()->RepeatFrame();
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
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDeleteConfirm), button_delete_size))
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
        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileDelete), button_delete_size))
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

    //Confirmation buttons
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

void WindowSettingsNew::UpdatePageProfilesOverlaySelect()
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

        list_overlays_ticked.clear();
        list_overlays_ticked.resize(list_overlays.size(), 1);
        is_any_ticked = !list_overlays.empty();
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

        if ( (pending_input_focus) && (ImGui::IsItemVisible()) )
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

void WindowSettingsNew::UpdatePageResetConfirm()
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

void WindowSettingsNew::PageGoForward(WindowSettingsPage new_page)
{
    m_PageStack.push_back(new_page);
    m_PageStackPos++;
}

void WindowSettingsNew::PageGoBack()
{
    if (m_PageStackPos != 0)
        m_PageStackPos--;
}

void WindowSettingsNew::PageGoBackInstantly()
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

void WindowSettingsNew::PageGoHome()
{
    m_PageStackPos = 0;
}

void WindowSettingsNew::SelectableWarning(const char* selectable_id, const char* popup_id, const char* text, bool show_warning_prefix, const ImVec4* text_color)
{
    float* selectable_height = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID(selectable_id), 1.0f);

    //Use selectable stretching over the text area to make it clickable
    if (ImGui::Selectable(selectable_id, ImGui::IsPopupOpen(popup_id), 0, {0.0f, *selectable_height}))
    {
        ImGui::OpenPopup(popup_id);
    }
    ImGui::SameLine(0.0f, 0.0f);

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

    //Store height for the selectable for next time if window is being hovered (could get bogus value otherwise)
    if (ImGui::IsWindowHovered())
    {
        *selectable_height = ImGui::GetItemRectSize().y;
    }
}
