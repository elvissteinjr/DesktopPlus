#include "WindowSettings.h"

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
    m_Column0Width(0.0f)
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
                case wndsettings_page_main:      UpdatePageMain();            break;
                case wndsettings_page_keyboard:  UpdatePageKeyboardLayout();  break;
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

void WindowSettingsNew::UpdatePageMain()
{
    static int jumpto_item_id = 0;
    const TRMGRStrID jumpto_strings[wndsettings_cat_MAX] = 
    {
        tstr_SettingsCatInterface, 
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

                const std::string& current_filename = ConfigManager::Get().GetConfigString(configid_str_interface_language_file);
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
                    ConfigManager::Get().SetConfigString(configid_str_interface_language_file, item.first);
                    TranslationManager::Get().LoadTranslationFromFile(item.first);
                    TextureManager::Get().ReloadAllTexturesLater();
                    UIManager::Get()->GetNotificationIcon().RefreshPopupMenu();
                    UIManager::Get()->GetOverlayPropertiesWindow().SetActiveOverlayID(UIManager::Get()->GetOverlayPropertiesWindow().GetActiveOverlayID(), true);
                    UIManager::Get()->RepeatFrame();

                    list_id = i;
                }

                i++;
            }

            ImGui::EndCombo();
        }

        ImGui::Columns(1);

        ImGui::Indent();

        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsInterfaceAdvancedSettings), &ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_show_advanced_settings)))
        {
            UIManager::Get()->RepeatFrame();
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_SettingsInterfaceAdvancedSettingsTip));

        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_show_advanced_settings))
        {
            ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsInterfaceBlankSpaceDrag), &ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_blank_space_drag_enabled));
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

        float& size = ConfigManager::Get().GetConfigFloatRef(configid_float_input_keyboard_detached_size);

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

        ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardStickyMod), &ConfigManager::Get().GetConfigBoolRef(configid_bool_input_keyboard_sticky_modifiers));

        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyRepeat), &ConfigManager::Get().GetConfigBoolRef(configid_bool_input_keyboard_key_repeat));

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

        bool& block_input = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_laser_pointer_block_input);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsLaserPointerBlockInput), &block_input))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_laser_pointer_block_input), block_input);
        }

        ImGui::Unindent();

        ImGui::Columns(2, "ColumnLaserPointer", false);
        ImGui::SetColumnWidth(0, m_Column0Width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsLaserPointerAutoToggleDistance));
        ImGui::NextColumn();

        float& distance = ConfigManager::Get().GetConfigFloatRef(configid_float_input_detached_interaction_max_distance);
        const char* alt_text = (distance < 0.01f) ? TranslationManager::GetString(tstr_SettingsLaserPointerAutoToggleDistanceValueOff) : nullptr;

        vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("LaserPointerMaxDistance") );
        if (ImGui::SliderWithButtonsFloat("LaserPointerMaxDistance", distance, 0.05f, 0.01f, 0.0f, 3.0f, (distance < 0.01f) ? "##%.2f" : "%.2f m", ImGuiSliderFlags_Logarithmic, nullptr, alt_text))
        {
            if (distance < 0.01f)
                distance = 0.0f;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_input_detached_interaction_max_distance), pun_cast<LPARAM, float>(distance));
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

        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_show_advanced_settings))
        {
            bool& auto_focus = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_winrt_auto_focus);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysAutoFocus), &auto_focus))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_winrt_auto_focus), auto_focus);
            }

            bool& keep_on_screen = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_winrt_keep_on_screen);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysKeepOnScreen), &keep_on_screen))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_winrt_keep_on_screen), keep_on_screen);
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsWindowOverlaysKeepOnScreenTip));
        }

        bool& auto_size_overlay = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_winrt_auto_size_overlay);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysAutoSizeOverlay), &auto_size_overlay))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_winrt_auto_size_overlay), auto_size_overlay);
        }

        bool& focus_scene_app = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_winrt_auto_focus_scene_app);
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysFocusSceneApp), &focus_scene_app))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_winrt_auto_focus_scene_app), focus_scene_app);
        }

        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_show_advanced_settings))
        {
            bool& strict_matching = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_winrt_window_matching_strict);
            if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsWindowOverlaysStrictMatching), &strict_matching))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_winrt_window_matching_strict), strict_matching);
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsWindowOverlaysStrictMatchingTip));
        }

        ImGui::Unindent();

        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_show_advanced_settings))
        {
            ImGui::Columns(2, "ColumnWindows", false);
            ImGui::SetColumnWidth(0, m_Column0Width);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsWindowOverlaysOnWindowDrag));
            ImGui::NextColumn();

            int& mode_dragging = ConfigManager::Get().GetConfigIntRef(configid_int_windows_winrt_dragging_mode);

            ImGui::SetNextItemWidth(-1);
            if (TranslatedComboAnimated("##ComboWindowDrag", mode_dragging, tstr_SettingsWindowOverlaysOnWindowDragDoNothing, tstr_SettingsWindowOverlaysOnWindowDragOverlay))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_windows_winrt_dragging_mode), mode_dragging);
            }

            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsWindowOverlaysOnCaptureLoss));
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsWindowOverlaysOnCaptureLossTip));
            ImGui::NextColumn();

            int& behavior_capture_loss = ConfigManager::Get().GetConfigIntRef(configid_int_windows_winrt_capture_lost_behavior);

            ImGui::SetNextItemWidth(-1);
            if (TranslatedComboAnimated("##ComboCaptureLost", behavior_capture_loss, tstr_SettingsWindowOverlaysOnCaptureLossDoNothing, tstr_SettingsWindowOverlaysOnCaptureLossRemove))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_windows_winrt_capture_lost_behavior), behavior_capture_loss);
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

        ImGui::TextUnformatted("Desktop+ NewUI Preview 2");

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

        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_warning_compositor_quality_hidden))
            warning_hidden_count++;
        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_warning_compositor_res_hidden))
            warning_hidden_count++;
        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_warning_process_elevation_hidden))
            warning_hidden_count++;
        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_warning_elevated_mode_hidden))
            warning_hidden_count++;
        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_warning_welcome_hidden))
            warning_hidden_count++;

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsWarningsHidden));
        ImGui::SameLine();
        ImGui::Text("%i", warning_hidden_count);

        ImGui::NextColumn();

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsWarningsReset)))
        {
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_compositor_quality_hidden, false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_compositor_res_hidden,     false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_process_elevation_hidden,  false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_elevated_mode_hidden,      false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_welcome_hidden,            false);
        }

        ImGui::Columns(1);
    }

    //Startup
    bool& no_steam = ConfigManager::Get().GetConfigBoolRef(configid_bool_misc_no_steam);

    if ( (ConfigManager::Get().IsSteamInstall()) || (UIManager::Get()->IsOpenVRLoaded()) ) //Only show Steam install or we can access OpenVR settings
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
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_misc_no_steam), no_steam);
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker(TranslationManager::GetString(tstr_SettingsStartupSteamDisableTip));
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

        bool has_restart_steam_button = ( (ConfigManager::Get().IsSteamInstall()) && (!ConfigManager::Get().GetConfigBool(configid_bool_state_misc_process_started_by_steam)) );

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

            if (!ConfigManager::Get().GetConfigBool(configid_bool_state_misc_elevated_mode_active))
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

        /*ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

        if (ImGui::Button(TranslationManager::GetString(tstr_SettingsTroubleshootingSettingsReset)))
        {
            ImGui::OpenPopup("SettingsResetPopup");
        }*/
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

        const std::string& current_filename = ConfigManager::Get().GetConfigString(configid_str_input_keyboard_layout_file);
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
        cluster_enabled_prev[kbdlayout_cluster_function]   = ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_cluster_function_enabled);
        cluster_enabled_prev[kbdlayout_cluster_navigation] = ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_cluster_navigation_enabled);
        cluster_enabled_prev[kbdlayout_cluster_numpad]     = ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_cluster_numpad_enabled);
        cluster_enabled_prev[kbdlayout_cluster_extra]      = ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_cluster_extra_enabled);

        UIManager::Get()->RepeatFrame();
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsKeyboardLayout) ); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::BeginChild("LayoutList", ImVec2(0.0f, ImGui::GetFontSize() * 15.0f), true);

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
    bool& function_enabled   = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_keyboard_cluster_function_enabled);
    bool& navigation_enabled = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_keyboard_cluster_navigation_enabled);
    bool& numpad_enabled     = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_keyboard_cluster_numpad_enabled);
    bool& extra_enabled      = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_keyboard_cluster_extra_enabled);

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
            ConfigManager::Get().SetConfigString(configid_str_input_keyboard_layout_file, list_layouts[list_id].FileName);
        }

        PageGoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        //Restore previous cluster settings
        ConfigManager::Get().SetConfigBool(configid_bool_input_keyboard_cluster_function_enabled,   cluster_enabled_prev[kbdlayout_cluster_function]);
        ConfigManager::Get().SetConfigBool(configid_bool_input_keyboard_cluster_navigation_enabled, cluster_enabled_prev[kbdlayout_cluster_navigation]);
        ConfigManager::Get().SetConfigBool(configid_bool_input_keyboard_cluster_numpad_enabled,     cluster_enabled_prev[kbdlayout_cluster_numpad]);
        ConfigManager::Get().SetConfigBool(configid_bool_input_keyboard_cluster_extra_enabled,      cluster_enabled_prev[kbdlayout_cluster_extra]);

        vr_keyboard.LoadCurrentLayout();

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

void WindowSettingsNew::PageGoHome()
{
    m_PageStackPos = 0;
}
