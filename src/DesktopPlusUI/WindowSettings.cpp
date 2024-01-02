#include "WindowSettings.h"

#define NOMINMAX
#include <windows.h>
#include <dxgi1_2.h>
#include <sstream>

#include "imgui.h"
#include "DesktopPlusWinRT.h"

#include "UIManager.h"
#include "Ini.h"
#include "Util.h"
#include "ConfigManager.h"
#include "OverlayManager.h"
#include "InterprocessMessaging.h"
#include "ImGuiExt.h"

void WindowSettings::UpdateWarnings()
{
    bool warning_displayed = false;

    //Compositor resolution warning
    {
        bool& hide_compositor_res_warning = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_warning_compositor_res_hidden);

        if ((!hide_compositor_res_warning) && (UIManager::Get()->IsCompositorResolutionLow()))
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningCompRes"))
            {
                ImGui::OpenPopup("DontShowAgain");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: Compositor resolution is below 100%%! This affects overlay rendering quality.");

            if (ImGui::BeginPopup("DontShowAgain"))
            {
                if (ImGui::Selectable("Don't show this again"))
                {
                    hide_compositor_res_warning = true;
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Compositor quality warning
    {
        bool& hide_compositor_quality_warning = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_warning_compositor_quality_hidden);

        if ((!hide_compositor_quality_warning) && (UIManager::Get()->IsCompositorRenderQualityLow()))
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningCompQuality"))
            {
                ImGui::OpenPopup("DontShowAgain2");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: Overlay render quality is not set to high!");

            if (ImGui::BeginPopup("DontShowAgain2"))
            {
                if (ImGui::Selectable("Don't show this again"))
                {
                    hide_compositor_quality_warning = true;
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Dashboard app process elevation warning
    {
        bool& hide_process_elevation_warning = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_warning_process_elevation_hidden);

        if ((!hide_process_elevation_warning) && (ConfigManager::Get().GetConfigBool(configid_bool_state_misc_process_elevated)))
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningElevation"))
            {
                ImGui::OpenPopup("DontShowAgain3");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: Desktop+ is running with administrative privileges!");

            if (ImGui::BeginPopup("DontShowAgain3"))
            {
                if (ImGui::Selectable("Don't show this again"))
                {
                    hide_process_elevation_warning = true;
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Elevated mode warning (this is different from elevated dashboard process)
    {
        bool& hide_elevated_mode_warning = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_warning_elevated_mode_hidden);

        if ((!hide_elevated_mode_warning) && (ConfigManager::Get().GetConfigBool(configid_bool_state_misc_elevated_mode_active)))
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningElevatedMode"))
            {
                ImGui::OpenPopup("DontShowAgain4");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: Elevated mode is active!");

            if (ImGui::BeginPopup("DontShowAgain4"))
            {
                if (ImGui::Selectable("Don't show this again"))
                {
                    hide_elevated_mode_warning = true;
                }
                else if (ImGui::Selectable("Leave Elevated Mode"))
                {
                    UIManager::Get()->ElevatedModeLeave();
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Focused process elevation warning
    {
        if (  (ConfigManager::Get().GetConfigBool(configid_bool_state_window_focused_process_elevated)) && (!ConfigManager::Get().GetConfigBool(configid_bool_state_misc_process_elevated)) && 
             (!ConfigManager::Get().GetConfigBool(configid_bool_state_misc_elevated_mode_active))       && (!ConfigManager::Get().GetConfigBool(configid_bool_state_misc_uiaccess_enabled)) )
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningElevation2"))
            {
                ImGui::OpenPopup("FocusedElevatedContext");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: An elevated process has focus! Desktop+ is unable to simulate input right now.");

            if (ImGui::BeginPopup("FocusedElevatedContext"))
            {
                if (ImGui::Selectable("Switch Task"))
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_do, action_switch_task);
                    UIManager::Get()->RepeatFrame();
                }
                else if ((UIManager::Get()->IsElevatedTaskSetUp()) && ImGui::Selectable("Enter Elevated Mode"))
                {
                    UIManager::Get()->ElevatedModeEnter();
                    UIManager::Get()->RepeatFrame();
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    {
        if ((ConfigManager::Get().GetConfigBool(configid_bool_misc_uiaccess_was_enabled)) && (!ConfigManager::Get().GetConfigBool(configid_bool_state_misc_uiaccess_enabled)))
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningUIAccess"))
            {
                ImGui::OpenPopup("DontShowAgain6");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: Desktop+ is no longer running with UIAccess privileges!");

            if (ImGui::BeginPopup("DontShowAgain6"))
            {
                if (ImGui::Selectable("Don't show this again"))
                {
                    ConfigManager::Get().SetConfigBool(configid_bool_misc_uiaccess_was_enabled, false);
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Overlay error warning
    {
        vr::EVROverlayError overlay_error = UIManager::Get()->GetOverlayErrorLast();

        if ( (overlay_error != vr::VROverlayError_None) && (UIManager::Get()->IsOpenVRLoaded()) )
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningOverlayError"))
            {
                ImGui::OpenPopup("DismissWarning");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);

            if (overlay_error == vr::VROverlayError_OverlayLimitExceeded)
            {
                ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: An overlay creation failed! (Maximum Overlay limit exceeded)");
            }
            else
            {
                ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: An overlay creation failed! (%s)", vr::VROverlay()->GetOverlayErrorNameFromEnum(overlay_error));
            }

            if (ImGui::BeginPopup("DismissWarning"))
            {
                if (ImGui::Selectable("Dismiss"))
                {
                    UIManager::Get()->ResetOverlayErrorLast();
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //WinRT Capture error warning
    {
        HRESULT hr_error = UIManager::Get()->GetWinRTErrorLast();

        if ( (hr_error != S_OK) && (UIManager::Get()->IsOpenVRLoaded()) )
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningWinRTError"))
            {
                ImGui::OpenPopup("DismissWarning2");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);

            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: An unexpected error occurred in a Graphics Capture thread! (0x%x)", hr_error);

            if (ImGui::BeginPopup("DismissWarning2"))
            {
                if (ImGui::Selectable("Dismiss"))
                {
                    UIManager::Get()->ResetWinRTErrorLast();
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Welcome "warning"
    {
        bool& hide_welcome_warning = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_warning_welcome_hidden);

        if (!hide_welcome_warning)
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningWelcome"))
            {
                ImGui::OpenPopup("DontShowAgain5");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextNotification, "Welcome to Desktop+! Click here to see some tips on getting started with the application.");

            bool open_popup = false;

            if (ImGui::BeginPopup("DontShowAgain5"))
            {
                if (ImGui::Selectable("Show Quick Start Guide"))
                {
                    open_popup = true;
                }
                else if (ImGui::Selectable("Don't show this again"))
                {
                    hide_welcome_warning = true;
                }
                ImGui::EndPopup();
            }

            if (open_popup)
            {
                ImGui::OpenPopup("QuickStartGuidePopup");
            }

            PopupQuickStartGuide();

            warning_displayed = true;
        }
    }

    //Separate from the main content if a warning was actually displayed
    if (warning_displayed)
    {
        //Horizontal separator
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
        ImGui::BeginChild("hsepW", ImVec2(0.0f, 1.0f), true);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

void WindowSettings::UpdateCatOverlay()
{
    ImGui::Text("Overlay");
            
    //Horizontal separator (not using ImGui::Separator() since it looks slightly different to the vertical makeshift one with transparent colors)
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    const float column_width_0 = ImGui::GetFontSize() * 10.0f;

    //Overlay selector
    {
        static int overlay_window_icon_id = -1;
        static TMNGRTexID overlay_icon_texture_id = tmtex_icon_desktop;
        static char buffer_overlay_name[1024] = "";
        static float button_change_width = 0.0f;

        int& current_overlay = ConfigManager::Get().GetConfigIntRef(configid_int_interface_overlay_current_id);

        ImGui::Columns(2, "ColumnCurrentOverlay", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Current Overlay");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1 - button_change_width);

        bool buffer_changed = false;
        static bool is_combo_input_visible = false;
        static bool is_combo_input_activated = false;
        static bool is_combo_mouse_released_once = false;

        ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
        ImVec2 img_size, img_uv_min, img_uv_max;

        ImVec2 combo_pos = ImGui::GetCursorScreenPos();

        if (ImGui::BeginComboWithInputText("##ComboOverlaySelector", buffer_overlay_name, 1024, buffer_changed, is_combo_input_visible, is_combo_input_activated, is_combo_mouse_released_once, true))
        {
            if (ImGui::IsWindowAppearing())
            {
                UIManager::Get()->RepeatFrame();    //Dropdown scrollbar flickers when size of elements is unknown and arrow was used, so skip the frame
            }

            int index_hovered = -1;
            int selectable_window_icon_id = -1;
            TMNGRTexID selectable_icon_texture_id = tmtex_icon_desktop;

            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                bool current_overlay_enabled = data.ConfigBool[configid_bool_overlay_enabled];

                if (!current_overlay_enabled)
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

                ImGui::PushID(i);

                if (ImGui::Selectable("", (i == current_overlay)))
                {
                    current_overlay = i;
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_overlay_current_id), current_overlay);
                    OverlayManager::Get().SetCurrentOverlayID(current_overlay);

                    m_OverlayNameBufferNeedsUpdate = true;
                    UIManager::Get()->RepeatFrame();
                }

                if (ImGui::IsItemHovered())
                {
                    index_hovered = i;
                }

                ImGui::SameLine(0.0f, 0.0f);

                //Icon and text
                if (ImGui::IsRectVisible(img_size_line_height)) //Only lookup icon if it's gonna be visible
                {
                    selectable_window_icon_id = GetOverlayIcon(i, selectable_icon_texture_id);

                    if (selectable_window_icon_id != -1)
                        TextureManager::Get().GetWindowIconTextureInfo(selectable_window_icon_id, img_size, img_uv_min, img_uv_max);
                    else
                        TextureManager::Get().GetTextureInfo(selectable_icon_texture_id, img_size, img_uv_min, img_uv_max);

                    ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);
                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                }

                ImGui::Text(data.ConfigNameStr.c_str());

                ImGui::PopID();

                if (!current_overlay_enabled)
                    ImGui::PopStyleVar();
            }

            ImGui::EndCombo();

            HighlightOverlay(index_hovered);
        }

        ImGui::ComboWithInputTextActivationCheck(is_combo_input_visible);

        if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
        {
            if (ImGui::IsItemHovered())
            {
                HighlightOverlay(current_overlay);
            }
            else if (!ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_selectmode))
            {
                HighlightOverlay(-1);
            }
        }

        ImGui::SameLine();

        //Custom combo preview content (icon with text), unless input is visible
        if (!is_combo_input_visible)
        {
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec2 backup_pos = ImGui::GetCursorScreenPos();
            ImVec2 clip_end = ImGui::GetItemRectMax();
            clip_end.x -= ImGui::GetFrameHeight();

            ImGui::SetCursorScreenPos(ImVec2(combo_pos.x + style.FramePadding.x, combo_pos.y + style.FramePadding.y));

            ImGui::PushID(current_overlay);

            bool current_overlay_enabled = ConfigManager::Get().GetConfigBool(configid_bool_overlay_enabled);

            if (!current_overlay_enabled)
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

            if (overlay_window_icon_id != -1)
                TextureManager::Get().GetWindowIconTextureInfo(overlay_window_icon_id, img_size, img_uv_min, img_uv_max);
            else
                TextureManager::Get().GetTextureInfo(overlay_icon_texture_id, img_size, img_uv_min, img_uv_max);

            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

            ImGui::PopID();

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - style.FramePadding.y);

            ImGui::PushClipRect(ImGui::GetCursorPos(), clip_end, true);
            ImGui::Text(buffer_overlay_name);
            ImGui::PopClipRect();

            if (!current_overlay_enabled)
                ImGui::PopStyleVar();

            ImGui::SetCursorScreenPos(backup_pos);
        }

        OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

        if (buffer_changed)
        {
            //If name buffer is not empty, set name from user input, otherwise fall back to auto naming
            if (buffer_overlay_name[0] != '\0')
            {
                data.ConfigBool[configid_bool_overlay_name_custom] = true;
                data.ConfigNameStr = buffer_overlay_name;

                if (ImGui::StringContainsUnmappedCharacter(buffer_overlay_name))
                {
                    TextureManager::Get().ReloadAllTexturesLater();
                }
            }
            else
            {
                data.ConfigBool[configid_bool_overlay_name_custom] = false;
                OverlayManager::Get().SetCurrentOverlayNameAuto();
            }
        }
        else if ( (!is_combo_input_visible) && (!m_OverlayNameBufferNeedsUpdate) && (buffer_overlay_name[0] == '\0') ) //If editing is over and the buffer is still blank, update it so it has the auto name
        {
            //This can also trigger when the name was loaded blank, so set these again so it doesn't repeat this forever
            data.ConfigBool[configid_bool_overlay_name_custom] = false;
            OverlayManager::Get().SetCurrentOverlayNameAuto();

            m_OverlayNameBufferNeedsUpdate = true;
        }

        if (ImGui::Button("+##AddOverlay", {ImGui::GetFrameHeight(), ImGui::GetFrameHeight()}))
        {
            DuplicateCurrentOverlay();
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (ImGui::Button("Manage"))
        {
            ImGui::OpenPopup("CurrentOverlayManage");

            //Activate selection mode
            ConfigManager::Get().SetConfigBool(configid_bool_state_overlay_selectmode, true);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragselectmode_show_hidden), true);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_selectmode), true);
        }

        button_change_width = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x;

        if ( (PopupCurrentOverlayManage()) || (ImGui::IsWindowAppearing()) || (m_OverlayNameBufferNeedsUpdate) )
        {
            //Update buffer
            OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

            size_t copied_length = data.ConfigNameStr.copy(buffer_overlay_name, 1023);
            buffer_overlay_name[copied_length] = '\0';

            if (ImGui::StringContainsUnmappedCharacter(buffer_overlay_name))
            {
                if (TextureManager::Get().AddFontBuilderString(buffer_overlay_name)) //Doesn't need to be added to it actually, but helps with avoiding texture reloads
                {
                    TextureManager::Get().ReloadAllTexturesLater();
                }
            }

            //Update icon
            overlay_window_icon_id = GetOverlayIcon(current_overlay, overlay_icon_texture_id);

            //Deactivate selection mode
            ConfigManager::Get().SetConfigBool(configid_bool_state_overlay_selectmode, false);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragselectmode_show_hidden), false);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_selectmode), false);

            m_OverlayNameBufferNeedsUpdate = false;
        }

        ImGui::Columns(1);
    }

    ImGui::Spacing();

    ImGui::BeginTabBar("TabBarOverlay", ImGuiTabBarFlags_NoTooltip);

    if (ImGui::BeginTabItem("General"))
    {
        UpdateCatOverlayTabGeneral();
    }

    if (ConfigManager::Get().GetConfigInt(configid_int_overlay_capture_source) != ovrl_capsource_ui)
    {
        if (ImGui::BeginTabItem("Capture"))
        {
            UpdateCatOverlayTabCapture();
        }
    }

    if (ImGui::BeginTabItem("Advanced"))
    {
        UpdateCatOverlayTabAdvanced();
    }

    if (ImGui::BeginTabItem("Interface"))
    {
        UpdateCatOverlayTabInterface();
    }
    
    ImGui::EndTabBar();
}

void WindowSettings::UpdateCatOverlayTabGeneral()
{
    const float column_width_0 = ImGui::GetFontSize() * 10.0f;
    bool detached = ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached);

    ImGui::BeginChild("ViewOverlayTabGeneral");

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    //Profiles
    {
        static bool profile_selector_type_multi = false;

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Profiles");
        ImGui::Columns(2, "ColumnProfiles", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Type");
        ImGui::NextColumn();

        if (ImGui::RadioButton("Single-Overlay", !profile_selector_type_multi))
        {
            profile_selector_type_multi = false;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("Multi-Overlay", profile_selector_type_multi))
        {
            profile_selector_type_multi = true;
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Profile");
        ImGui::NextColumn();

        ProfileSelector(profile_selector_type_multi);

        ImGui::Columns(1);
    }

    //Appearance
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Appearance");
        ImGui::Columns(2, "ColumnAppearance", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& is_enabled = ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_enabled);

        if (ImGui::Checkbox("Enabled", &is_enabled))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_enabled), is_enabled);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Width");
        ImGui::NextColumn();

        float& width = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_width);
        float width_slider_max = 10.0f;

        if (detached) //Variable max slider ranges for detached
        {
            width_slider_max = (ConfigManager::Get().GetConfigIntRef(configid_int_overlay_detached_origin) >= ovrl_origin_right_hand) ? 1.5f : 10.0f;
        }

        if (ImGui::SliderWithButtonsFloat("OverlayWidth", width, 0.1f, 0.01f, 0.025f, width_slider_max, "%.2f m", ImGuiSliderFlags_Logarithmic))
        {
            if (width < 0.05f)
                width = 0.05f;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_width), *(LPARAM*)&width);
        }
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Curvature");
        ImGui::NextColumn();

        float& curve = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_curvature);

        if (ImGui::SliderWithButtonsFloatPercentage("OverlayCurvature", curve, 5, 1, 0, 35, "%d%%"))
        {
            curve = clamp(curve, 0.0f, 1.0f);

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_curvature), *(LPARAM*)&curve);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Opacity");
        ImGui::NextColumn();

        float& opacity = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_opacity);

        if (ImGui::SliderWithButtonsFloatPercentage("OverlayOpacity", opacity, 5, 1, 0, 100, "%d%%"))
        {
            opacity = clamp(opacity, 0.0f, 1.0f);

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_opacity), *(LPARAM*)&opacity);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Brightness");
        ImGui::NextColumn();

        float& brightness = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_brightness);

        if (ImGui::SliderWithButtonsFloatPercentage("OverlayBrightness", brightness, 5, 1, 0, 100, "%d%%"))
        {
            brightness = clamp(brightness, 0.0f, 1.0f);

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_brightness), *(LPARAM*)&brightness);
        }

        ImGui::Columns(1);
    }

    //Position (Dashboard)
    if (!detached)
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Position");

        //Show some beginner help if there's only the dashboard overlay
        if (OverlayManager::Get().GetOverlayCount() == 1)
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("To freely set position, origin or display mode, add an additional overlay");
        }

        ImGui::Columns(2, "ColumnPosition", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Up/Down Offset");
        ImGui::NextColumn();

        float& up = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_offset_up);

        if (ImGui::SliderWithButtonsFloat("OverlayOffsetUp", up, 0.1f, 0.01f, -5.0f, 5.0f, "%.2f m", ImGuiSliderFlags_Logarithmic))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_up), *(LPARAM*)&up);
        }
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Right/Left Offset");
        ImGui::NextColumn();

        float& right = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_offset_right);

        if (ImGui::SliderWithButtonsFloat("OverlayOffsetRight", right, 0.1f, 0.01f, -5.0f, 5.0f, "%.2f m", ImGuiSliderFlags_Logarithmic))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_right), *(LPARAM*)&right);
        }
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Forward/Backward Offset");
        ImGui::NextColumn();

        float& forward = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_offset_forward);

        if (ImGui::SliderWithButtonsFloat("OverlayOffsetForward", forward, 0.1f, 0.01f, -5.0f, 5.0f, "%.2f m", ImGuiSliderFlags_Logarithmic))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_forward), *(LPARAM*)&forward);
        }

        ImGui::Columns(1);
    }
    else //Position (Detached)
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Position");
        ImGui::Columns(2, "ColumnPositionDetached", false);
        ImGui::SetColumnWidth(0, column_width_0);

        static bool is_generic_tracker_connected = false;

        if ((ImGui::IsWindowAppearing()) && (UIManager::Get()->IsOpenVRLoaded()))
        {
            is_generic_tracker_connected = (GetFirstVRTracker() != vr::k_unTrackedDeviceIndexInvalid);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Display Mode");
        ImGui::NextColumn();

        int& mode_origin = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_detached_origin);

        if (mode_origin == ovrl_origin_hmd)
            ImGui::PushItemDisabled();

        ImGui::SetNextItemWidth(-1);

        int& mode_display = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_detached_display_mode);

        const char* items_display[] = {"Always", "Only in Dashboard", "Only in Scene", "Only in Desktop+ Tab"};
        if (ImGui::BeginCombo("##ComboDetachedDisplayMode", items_display[mode_display]))
        {
            int mode_display_old = mode_display;

            //Displays some only when origin isn't dashboard
            if (ImGui::Selectable(items_display[ovrl_dispmode_always], (mode_display == ovrl_dispmode_always)))
                mode_display = ovrl_dispmode_always;
            if (ImGui::Selectable(items_display[ovrl_dispmode_dashboard], (mode_display == ovrl_dispmode_dashboard)))
                mode_display = ovrl_dispmode_dashboard;
            if (ImGui::Selectable(items_display[ovrl_dispmode_scene], (mode_display == ovrl_dispmode_scene)))
                mode_display = ovrl_dispmode_scene;
            if (ImGui::Selectable(items_display[ovrl_dispmode_dplustab], (mode_display == ovrl_dispmode_dplustab)))
                mode_display = ovrl_dispmode_dplustab;

            if (mode_display != mode_display_old)
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_detached_display_mode), mode_display);

            ImGui::EndCombo();
        }

        if (mode_origin == ovrl_origin_hmd)
            ImGui::PopItemDisabled();

        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Position Origin");

        if (mode_origin == ovrl_origin_dashboard)
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Dashboard origin is only an approximation");
        }
        else if (mode_origin == ovrl_origin_seated_universe)
        {
            if ( (UIManager::Get()->IsOpenVRLoaded()) && (vr::VRCompositor()->GetTrackingSpace() != vr::TrackingUniverseSeated) )
            {
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::FixedHelpMarker("Current tracking space is not seated");
            }
        }

        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);

        const char* items_origin[] = {"Play Area", "HMD Floor Position", "Seated Position", "Dashboard", "HMD", "Right Controller", "Left Controller", "Tracker #1"};
        if (ImGui::BeginCombo("##ComboDetachedOrigin", items_origin[mode_origin]))
        {
            int mode_origin_old = mode_origin;

            //Displays tracker option only when one is connected
            if ( (ImGui::Selectable(items_origin[ovrl_origin_room],            (mode_origin == ovrl_origin_room))) )
                mode_origin = ovrl_origin_room;
            if ( (ImGui::Selectable(items_origin[ovrl_origin_hmd_floor],       (mode_origin == ovrl_origin_hmd_floor))) )
                mode_origin = ovrl_origin_hmd_floor;
            if ( (ImGui::Selectable(items_origin[ovrl_origin_seated_universe], (mode_origin == ovrl_origin_seated_universe))) )
                mode_origin = ovrl_origin_seated_universe;
            if ( (ImGui::Selectable(items_origin[ovrl_origin_dashboard],       (mode_origin == ovrl_origin_dashboard))) )
                mode_origin = ovrl_origin_dashboard;
            if ( (ImGui::Selectable(items_origin[ovrl_origin_hmd],             (mode_origin == ovrl_origin_hmd))) )
                mode_origin = ovrl_origin_hmd;
            if ( (ImGui::Selectable(items_origin[ovrl_origin_right_hand],      (mode_origin == ovrl_origin_right_hand))) )
                mode_origin = ovrl_origin_right_hand;
            if ( (ImGui::Selectable(items_origin[ovrl_origin_left_hand],       (mode_origin == ovrl_origin_left_hand))) )
                mode_origin = ovrl_origin_left_hand;
            if ( (is_generic_tracker_connected) && (ImGui::Selectable(items_origin[ovrl_origin_aux], (mode_origin == ovrl_origin_aux))) )
                mode_origin = ovrl_origin_aux;

            if (mode_origin != mode_origin_old)
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_detached_origin), mode_origin);

                if (mode_origin == ovrl_origin_dashboard)
                {
                    mode_display = ovrl_dispmode_dashboard;
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_detached_display_mode), mode_display);
                }
                else if (mode_origin == ovrl_origin_hmd)
                {
                    mode_display = ovrl_dispmode_scene;
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_detached_display_mode), mode_display);
                }

                //Automatically reset the matrix to a saner default if it still has the zero value
                if (ConfigManager::Get().GetOverlayDetachedTransform().isZero())
                {
                    //Limit initial width to 0.25m for controller origin. This only helps then first switching the origin, but better than not at all
                    if ((mode_origin == ovrl_origin_right_hand) || (mode_origin == ovrl_origin_left_hand))
                    {
                        float& width = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_width);

                        if (width > 0.25f)
                        {
                            width = 0.25f;

                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_width), *(LPARAM*)&width);
                        }
                    }

                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
                }
            }

            ImGui::EndCombo();
        }

        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Position");

        if (!UIManager::Get()->IsOpenVRLoaded())
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Position can only be changed or reset when Desktop+ is running");
        }

        ImGui::NextColumn();

        if (!UIManager::Get()->IsOpenVRLoaded())
            ImGui::PushItemDisabled();

        bool& is_changing_position = ConfigManager::Get().GetConfigBoolRef(configid_bool_state_overlay_dragmode);

        if (ImGui::Button("Change"))
        {
            ImGui::OpenPopup("OverlayChangePosPopup");

            //Dragging the overlay the UI is open on is pretty inconvenient to get out of when not sitting in front of a real mouse, so let's prevent this
            if (!UIManager::Get()->IsInDesktopMode())
            {
                //Automatically reset the matrix to a saner default if it still has the zero value
                if (ConfigManager::Get().GetOverlayDetachedTransform().isZero())
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
                }

                is_changing_position = true;
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragselectmode_show_hidden), is_changing_position);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragmode), is_changing_position);
            }
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (ImGui::Button("Reset"))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
        }

        if (!UIManager::Get()->IsOpenVRLoaded())
            ImGui::PopItemDisabled();

        ImGui::Columns(1);

        if (mode_origin == ovrl_origin_dashboard)
        {
            ImGui::Columns(2, "ColumnPositionWide", false);
            ImGui::SetColumnWidth(0, column_width_0 * 2.0f);

            bool& apply_steamvr2_offset = ConfigManager::Get().GetConfigBoolRef(configid_bool_misc_apply_steamvr2_dashboard_offset);
            if (ImGui::Checkbox("Apply SteamVR 2 Dashboard Offset", &apply_steamvr2_offset))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_misc_apply_steamvr2_dashboard_offset), apply_steamvr2_offset);
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Applies backward compatibility offset to dashboard origin overlays when the SteamVR 2 dashboard is detected.\nThis setting applies to all overlays.");
        }

        PopupOverlayDetachedPositionChange();
    }

    ImGui::EndChild();
    ImGui::EndTabItem();
}

void WindowSettings::UpdateCatOverlayTabCapture()
{
    const float column_width_0 = ImGui::GetFontSize() * 10.0f;
    int& capture_method = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_capture_source);
    int& winrt_selected_desktop = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_winrt_desktop_id);
    HWND winrt_selected_window = (HWND)ConfigManager::Get().GetConfigIntPtr(configid_intptr_overlay_state_winrt_hwnd);

    static HWND capture_window_list_selected_window = nullptr;
    static int capture_list_selected_desktop = -2;
    static std::string capture_list_selected_str;

    ImGui::BeginChild("ViewOverlayTabCapture");

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    //Capture Settings
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Capture Settings");
        ImGui::Columns(2, "ColumnCaptureSettings", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Capture Method");
        ImGui::NextColumn();

        if (ImGui::RadioButton("Desktop Duplication", (capture_method == ovrl_capsource_desktop_duplication)))
        {
            capture_method = ovrl_capsource_desktop_duplication;
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_capture_source), capture_method);

            OverlayManager::Get().SetCurrentOverlayNameAuto();
            m_OverlayNameBufferNeedsUpdate = true;

            UIManager::Get()->RepeatFrame();
        }

        ImGui::SameLine();

        if (!DPWinRT_IsCaptureSupported())
            ImGui::PushItemDisabled();

        if (ImGui::RadioButton("Graphics Capture", (capture_method == ovrl_capsource_winrt_capture)))
        {
            capture_method = ovrl_capsource_winrt_capture;
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_capture_source), capture_method);

            OverlayManager::Get().SetCurrentOverlayNameAuto();
            m_OverlayNameBufferNeedsUpdate = true;

            UIManager::Get()->RepeatFrame();
        }

        if (!DPWinRT_IsCaptureSupported())
        {
            ImGui::PopItemDisabled();

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Graphics Capture is not supported on this system", "(!)");
        }
        else if (!DPWinRT_IsCaptureFromCombinedDesktopSupported())
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Some Graphics Capture features are not supported on this system", "(!)");
        }

        //WinRT Capture settings
        if (capture_method == ovrl_capsource_winrt_capture)
        {
            if ((ImGui::IsWindowAppearing()) || (m_CaptureWindowList.empty()))
            {
                UpdateWindowList(capture_window_list_selected_window, capture_list_selected_str);
            }

            //Catch selection changes from other sources or from changing the current overlay (or window is just gone)
            if ((capture_window_list_selected_window != winrt_selected_window) || (capture_list_selected_desktop != winrt_selected_desktop) || (capture_list_selected_str.empty()) ||
                ( (winrt_selected_window != nullptr) && (!::IsWindow(winrt_selected_window)) ) )
            {
                if (winrt_selected_window == nullptr)
                {
                    switch (winrt_selected_desktop)
                    {
                        case -2: capture_list_selected_str = "[None]";     break;
                        case -1: capture_list_selected_str = "Combined Desktop"; break;
                        default:
                        {
                            char desktop_str[16];
                            snprintf(desktop_str, 16, "Desktop %d", winrt_selected_desktop + 1);
                            capture_list_selected_str = desktop_str;
                        }
                    }
                }
                else
                {
                    const auto it = std::find_if(m_CaptureWindowList.begin(), m_CaptureWindowList.end(), [&](const auto& window) { return (window.WindowHandle == winrt_selected_window); });

                    if ( (it != m_CaptureWindowList.end()) && (::IsWindow(winrt_selected_window)) )
                    {
                        capture_list_selected_str = it->ListTitle;
                    }
                    else
                    {
                        if (capture_list_selected_str.empty())
                        {
                            capture_list_selected_str = "[Unknown Window]";
                        }
                    }
                }

                capture_list_selected_desktop = winrt_selected_desktop;
                capture_window_list_selected_window = winrt_selected_window;

                OverlayManager::Get().SetCurrentOverlayNameAuto();
            }

            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Source");
            ImGui::NextColumn();

            int desktop_count = ConfigManager::Get().GetConfigInt(configid_int_state_interface_desktop_count);

            ImGui::SetNextItemWidth(-1);

            if (!DPWinRT_IsCaptureFromHandleSupported())
                ImGui::PushItemDisabled();

            //The two seperators add additional height not taken account in the Combo content height calculations
            //That makes for a little bit of awkward sizing, so we adjust the item spacing value to nudge it to be just right
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x, ImGui::GetStyle().ItemSpacing.y + (ImGui::GetStyle().ItemSpacing.y*2) });

            ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
            ImVec2 img_size, img_uv_min, img_uv_max;

            ImVec2 combo_pos = ImGui::GetCursorScreenPos();

            if (ImGui::BeginCombo("##ComboSource", ""))
            {
                ImGui::PopStyleVar();

                //Update list when dropdown was just opened as well
                if (ImGui::IsWindowAppearing())
                {
                    UpdateWindowList(capture_window_list_selected_window, capture_list_selected_str);
                    UIManager::Get()->RepeatFrame();
                }

                ImGui::PushID(-2);

                if (ImGui::Selectable("", ( (winrt_selected_desktop == -2) && (winrt_selected_window == nullptr) ) ))
                {
                    winrt_selected_desktop = -2;
                    winrt_selected_window = nullptr;
                    capture_list_selected_str = "[None]";
                    capture_window_list_selected_window = winrt_selected_window;
                    ConfigManager::Get().SetConfigIntPtr(configid_intptr_overlay_state_winrt_hwnd, 0);
                    ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_title,    "");
                    ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_exe_name, "");

                    OverlayManager::Get().SetCurrentOverlayNameAuto();
                    m_OverlayNameBufferNeedsUpdate = true;

                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id), winrt_selected_desktop);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_intptr_overlay_state_winrt_hwnd), 0);

                    UIManager::Get()->RepeatFrame();
                }

                ImGui::SameLine(0.0f, 0.0f);

                TextureManager::Get().GetTextureInfo(tmtex_icon_xsmall_desktop_none, img_size, img_uv_min, img_uv_max);
                ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::Text("[None]");

                ImGui::PopID();

                ImGui::Separator();

                //List Combined desktop, if supported
                if (DPWinRT_IsCaptureFromCombinedDesktopSupported())
                {
                    ImGui::PushID(-1);

                    if (ImGui::Selectable("", (winrt_selected_desktop == -1)))
                    {
                        winrt_selected_desktop = -1;
                        winrt_selected_window = nullptr;
                        capture_list_selected_str = "Combined Desktop";
                        capture_window_list_selected_window = winrt_selected_window;
                        ConfigManager::Get().SetConfigIntPtr(configid_intptr_overlay_state_winrt_hwnd, 0);
                        ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_title,    "");
                        ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_exe_name, "");

                        OverlayManager::Get().SetCurrentOverlayNameAuto();
                        m_OverlayNameBufferNeedsUpdate = true;

                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_intptr_overlay_state_winrt_hwnd), 0);
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id), winrt_selected_desktop);

                        UIManager::Get()->RepeatFrame();
                    }

                    ImGui::SameLine(0.0f, 0.0f);

                    TextureManager::Get().GetTextureInfo(tmtex_icon_xsmall_desktop_all, img_size, img_uv_min, img_uv_max);
                    ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                    ImGui::Text("Combined Desktop");

                    ImGui::PopID();
                }

                //List desktops
                for (int i = 0; i < desktop_count; ++i)
                {
                    ImGui::PushID(i);

                    char desktop_str[16];
                    snprintf(desktop_str, 16, "Desktop %d", i + 1);

                    if (ImGui::Selectable("", (winrt_selected_desktop == i)))
                    {
                        winrt_selected_desktop = i;
                        winrt_selected_window = nullptr;
                        capture_list_selected_str = desktop_str;
                        capture_window_list_selected_window = winrt_selected_window;
                        ConfigManager::Get().SetConfigIntPtr(configid_intptr_overlay_state_winrt_hwnd, 0);
                        ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_title,    "");
                        ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_exe_name, "");

                        OverlayManager::Get().SetCurrentOverlayNameAuto();
                        m_OverlayNameBufferNeedsUpdate = true;

                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_intptr_overlay_state_winrt_hwnd), 0);
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id), winrt_selected_desktop);

                        UIManager::Get()->RepeatFrame();
                    }

                    ImGui::SameLine(0.0f, 0.0f);

                    const TMNGRTexID texid = (tmtex_icon_desktop_1 + i <= tmtex_icon_desktop_6) ? (TMNGRTexID)(tmtex_icon_xsmall_desktop_1 + i) : tmtex_icon_xsmall_desktop;
                    TextureManager::Get().GetTextureInfo(texid, img_size, img_uv_min, img_uv_max);
                    ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                    ImGui::Text(desktop_str);

                    ImGui::PopID();
                }

                ImGui::Separator();

                //List windows
                for (WindowInfo& window : m_CaptureWindowList)
                {
                    ImGui::PushID(&window);

                    if (ImGui::Selectable("", (winrt_selected_window == window.WindowHandle)))
                    {
                        winrt_selected_desktop = -2;
                        winrt_selected_window = window.WindowHandle;
                        capture_list_selected_str = window.ListTitle;
                        capture_window_list_selected_window = winrt_selected_window;

                        ConfigManager::Get().SetConfigIntPtr(configid_intptr_overlay_state_winrt_hwnd, (intptr_t)winrt_selected_window);
                        ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_title,    StringConvertFromUTF16(window.Title.c_str())); //No need to sync these
                        ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_exe_name, window.ExeName);

                        OverlayManager::Get().SetCurrentOverlayNameAuto();
                        m_OverlayNameBufferNeedsUpdate = true;

                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id), -2);
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_intptr_overlay_state_winrt_hwnd), (LPARAM)winrt_selected_window);

                        UIManager::Get()->RepeatFrame();
                    }

                    ImGui::SameLine(0.0f, 0.0f);

                    int icon_id = TextureManager::Get().GetWindowIconCacheID(window.GetIcon());

                    if (icon_id != -1)
                    {
                        TextureManager::Get().GetWindowIconTextureInfo(icon_id, img_size, img_uv_min, img_uv_max);
                        ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

                        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                    }

                    ImGui::Text(window.ListTitle.c_str());

                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }
            else
            {
                ImGui::PopStyleVar();
            }

            //Undo effects of style adjustment as explained above BeginCombo()
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (ImGui::GetStyle().ItemSpacing.y*2));

            //Custom combo preview content (icon with text)
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec2 backup_pos = ImGui::GetCursorScreenPos();
            ImVec2 clip_end = ImGui::GetItemRectMax();
            clip_end.x -= ImGui::GetFrameHeight();

            int window_icon_id = -1;
            TMNGRTexID icon_texture_id = tmtex_icon_desktop;

            ImGui::SetCursorScreenPos(ImVec2(combo_pos.x + style.FramePadding.x, combo_pos.y + style.FramePadding.y));

            ImGui::PushID(OverlayManager::Get().GetCurrentOverlayID());

            window_icon_id = GetOverlayIcon(OverlayManager::Get().GetCurrentOverlayID(), icon_texture_id);

            if (window_icon_id != -1)
                TextureManager::Get().GetWindowIconTextureInfo(window_icon_id, img_size, img_uv_min, img_uv_max);
            else
                TextureManager::Get().GetTextureInfo(icon_texture_id, img_size, img_uv_min, img_uv_max);

            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

            ImGui::PopID();

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY());

            ImGui::PushClipRect(ImGui::GetCursorPos(), clip_end, true);
            ImGui::Text(capture_list_selected_str.c_str());
            ImGui::PopClipRect();

            ImGui::SetCursorScreenPos(backup_pos);

            ImGui::NextColumn();
            ImGui::NextColumn();

            if (ImGui::Button("Use Active Window"))
            {
                //Try to find the foreground window in the window list and only use it when found there
                UpdateWindowList(capture_window_list_selected_window, capture_list_selected_str);
                HWND foreground_window = ::GetForegroundWindow();
                const auto it = std::find_if(m_CaptureWindowList.begin(), m_CaptureWindowList.end(), [&](const auto& window){ return (window.WindowHandle == foreground_window); });

                if (it != m_CaptureWindowList.end())
                {
                    winrt_selected_desktop = -2;
                    winrt_selected_window = foreground_window;
                    capture_list_selected_str = it->ListTitle;
                    capture_window_list_selected_window = winrt_selected_window;

                    ConfigManager::Get().SetConfigIntPtr(configid_intptr_overlay_state_winrt_hwnd, (intptr_t)winrt_selected_window);
                    ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_title,    StringConvertFromUTF16(it->Title.c_str())); //No need to sync these
                    ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_exe_name, it->ExeName);

                    OverlayManager::Get().SetCurrentOverlayNameAuto();
                    m_OverlayNameBufferNeedsUpdate = true;

                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id), -2);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_intptr_overlay_state_winrt_hwnd), (LPARAM)winrt_selected_window);
                }
            }

            if (!DPWinRT_IsCaptureFromHandleSupported())
                ImGui::PopItemDisabled();

            //Only show this when there's still a point in using it
            if ( (!DPWinRT_IsCaptureFromCombinedDesktopSupported()) && (UIManager::Get()->IsOpenVRLoaded()) )
            {
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

                if (ImGui::Button("Use Picker Window"))
                {
                    m_CaptureWindowList.clear();    //Clear window list so it's refreshed next frame

                    //Have the dashboard app figure out how to do this as the UI doesn't have all data needed at hand
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_winrt_show_picker);
                }
            }
        }

        ImGui::Columns(1);
    }

    if (capture_method == ovrl_capsource_desktop_duplication)
    {
        //Desktop Cropping
        if (UIManager::Get()->IsOpenVRLoaded())
        {
            static int list_selected_desktop = 0;
            static unsigned int list_selected_desktop_overlay_id = k_ulOverlayID_Dashboard;

            if ((ImGui::IsWindowAppearing()) || (list_selected_desktop_overlay_id != OverlayManager::Get().GetCurrentOverlayID()))
            {
                //Reset state to current desktop for convenience
                list_selected_desktop = ConfigManager::Get().GetConfigInt(configid_int_overlay_desktop_id);
                list_selected_desktop_overlay_id = OverlayManager::Get().GetCurrentOverlayID();
            }

            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Desktop Cropping");
            ImGui::Columns(2, "ColumnCropDesktop", false);
            ImGui::SetColumnWidth(0, column_width_0);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Desktop");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1);

            int desktop_count = ConfigManager::Get().GetConfigInt(configid_int_state_interface_desktop_count);

            char desktop_str[16];
            snprintf(desktop_str, 16, "Desktop %d", list_selected_desktop + 1);

            if (ImGui::BeginCombo("##ComboDesktopCrop", (list_selected_desktop == -1) ? "Combined Desktop" : desktop_str))
            {
                if (ImGui::Selectable("Combined Desktop", (list_selected_desktop == -1)))
                {
                    list_selected_desktop = -1;
                }

                for (int i = 0; i < desktop_count; ++i)
                {
                    ImGui::PushID(i);

                    snprintf(desktop_str, 16, "Desktop %d", i + 1);

                    if (ImGui::Selectable(desktop_str, (list_selected_desktop == i)))
                    {
                        list_selected_desktop = i;
                    }

                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }

            ImGui::NextColumn();
            ImGui::NextColumn();

            if (ImGui::Button("Crop to Desktop"))
            {
                //This is the same as resetting, except the desktop ID can be changed
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_desktop_id), list_selected_desktop);
                ConfigManager::Get().SetConfigInt(configid_int_overlay_desktop_id, list_selected_desktop);

                OverlayManager::Get().SetCurrentOverlayNameAuto();
                m_OverlayNameBufferNeedsUpdate = true;
            }

            ImGui::Columns(1);
        }
    }

    //Cropping Area
    {
        int& crop_x      = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_x);
        int& crop_y      = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_y);
        int& crop_width  = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_width);
        int& crop_height = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_height);

        int ovrl_width, ovrl_height;

        if (ConfigManager::Get().GetConfigInt(configid_int_overlay_capture_source) == ovrl_capsource_desktop_duplication)
        {
            UIManager::Get()->GetDesktopOverlayPixelSize(ovrl_width, ovrl_height);
        }
        else //This would also work for desktop duplication except the above works without the dashboard app running while this doesn't
        {
            ovrl_width  = ConfigManager::Get().GetConfigInt(configid_int_overlay_state_content_width);
            ovrl_height = ConfigManager::Get().GetConfigInt(configid_int_overlay_state_content_height);

            //If overlay width and height are uninitialized, set them to the crop values at least
            if ((ovrl_width == -1) && (ovrl_height == -1))
            {
                ovrl_width  = crop_width  + crop_x;
                ovrl_height = crop_height + crop_y;

                ConfigManager::Get().SetConfigInt(configid_int_overlay_state_content_width,  ovrl_width);
                ConfigManager::Get().SetConfigInt(configid_int_overlay_state_content_height, ovrl_height);
            }
        }

        int crop_width_max  = ovrl_width  - crop_x;
        int crop_height_max = ovrl_height - crop_y;
        int crop_width_ui   = (crop_width  == -1) ? crop_width_max  + 1 : crop_width;
        int crop_height_ui  = (crop_height == -1) ? crop_height_max + 1 : crop_height;

        const bool disable_sliders = ((ovrl_width == -1) && (ovrl_height == -1));
        const bool is_crop_invalid = ((crop_x > ovrl_width - 1) || (crop_y > ovrl_height - 1) || (crop_width_max < 1) || (crop_height_max < 1));

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Cropping Rectangle");

        if ( (!disable_sliders) && (is_crop_invalid) )
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("The current cropping rectangle is invalid. The overlay may not be visible as a result.", "(!)");
        }

        ImGui::Columns(2, "ColumnCrop", false);
        ImGui::SetColumnWidth(0, column_width_0);

        if (disable_sliders)
            ImGui::PushItemDisabled();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("X");
        ImGui::NextColumn();

        if (ImGui::SliderWithButtonsInt("CropX", crop_x, 1, 1, 0, ovrl_width - 1, "%d px"))
        {
            //Note that we need to clamp the new value as neither the buttons nor the slider on direct input do so (they could, but this is in line with the rest of ImGui)
            crop_x = clamp(crop_x, 0, ovrl_width - 1);

            if (crop_x + crop_width > ovrl_width)
            {
                crop_width = ovrl_width - crop_x;
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_width), crop_width);
            }

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_x), crop_x);
        }
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Y");
        ImGui::NextColumn();

        if (ImGui::SliderWithButtonsInt("CropY", crop_y, 1, 1, 0, ovrl_height - 1, "%d px"))
        {
            crop_y = clamp(crop_y, 0, ovrl_height - 1);

            if (crop_y + crop_height > ovrl_height)
            {
                crop_height = ovrl_height - crop_y;
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);
            }

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_y), crop_y);
        }
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Width");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);

        //The way mapping max + 1 == -1 value into the slider is done is a bit convoluted, but it works
        if (ImGui::SliderWithButtonsInt("CropWidth", crop_width_ui, 1, 1, 1, crop_width_max + 1, (crop_width == -1) ? "Max" : "%d px"))
        {
            crop_width = clamp(crop_width_ui, 1, crop_width_max + 1);

            if (crop_width_ui > crop_width_max)
                crop_width = -1;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_width), crop_width);
        }
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Height");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);


        if (ImGui::SliderWithButtonsInt("CropHeight", crop_height_ui, 1, 1, 1, crop_height_max + 1, (crop_height == -1) ? "Max" : "%d px"))
        {
            crop_height = clamp(crop_height_ui, 1, crop_height_max + 1);

            if (crop_height_ui > crop_height_max)
                crop_height = -1;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();

        //Needs dashboard app to be running
        if ( (ConfigManager::Get().GetConfigInt(configid_int_overlay_capture_source) == ovrl_capsource_desktop_duplication) && (UIManager::Get()->IsOpenVRLoaded()) )
        {
            if (ImGui::Button("Crop to Active Window"))
            {
                //Have the dashboard app figure out how to do this as the UI doesn't have all data needed at hand
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_crop_to_active_window);

                OverlayManager::Get().SetCurrentOverlayNameAuto(::GetForegroundWindow());
                m_OverlayNameBufferNeedsUpdate = true;
            }

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }

        if (ImGui::Button("Reset##Crop"))
        {
            if ((ConfigManager::Get().GetConfigInt(configid_int_overlay_capture_source) != ovrl_capsource_desktop_duplication) || 
                (ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring)) || (!UIManager::Get()->IsOpenVRLoaded()))
            {
                crop_x = 0;
                crop_y = 0;
                crop_width = -1;
                crop_height = -1;

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_x), crop_x);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_y), crop_y);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_width), crop_width);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);
            }
            else
            {
                //Have the dashboard figure out the right crop by changing the desktop ID to the current value again
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_desktop_id),
                                                            ConfigManager::Get().GetConfigInt(configid_int_overlay_desktop_id));
            }

            OverlayManager::Get().SetCurrentOverlayNameAuto();
            m_OverlayNameBufferNeedsUpdate = true;
        }

        if (disable_sliders)
            ImGui::PopItemDisabled();

        ImGui::Columns(1);
    }

    ImGui::EndChild();
    ImGui::EndTabItem();
}

void WindowSettings::UpdateCatOverlayTabAdvanced()
{
    const float column_width_0 = ImGui::GetFontSize() * 10.0f;
    bool detached = ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached);
    int capture_source = ConfigManager::Get().GetConfigInt(configid_int_overlay_capture_source);

    ImGui::BeginChild("ViewOverlayTabAdvanced");

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    //3D
    if (capture_source != ovrl_capsource_ui)
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "3D");
        ImGui::Columns(2, "Column3D", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("3D Mode");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);
        const char* items[] = { "Off", "Half Side-by-Side", "Side-by-Side", "Half Over-Under", "Over-Under" };
        int mode_3D = clamp(ConfigManager::Get().GetConfigIntRef(configid_int_overlay_3D_mode), 0, IM_ARRAYSIZE(items) - 1);
        if (ImGui::Combo("##Combo3DMode", &mode_3D, items, IM_ARRAYSIZE(items)))
        {
            ConfigManager::Get().SetConfigInt(configid_int_overlay_3D_mode, mode_3D);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_3D_mode), mode_3D);
        }

        ImGui::NextColumn();

        bool& swapped_3D = ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_3D_swapped);
        if (ImGui::Checkbox("Swap Left/Right Eye", &swapped_3D))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_3D_swapped), swapped_3D);
        }

        ImGui::Columns(1);
    }

    //Gaze Fade
    if (detached) //Gaze Fade only works with detached overlays (not like anyone needs it on dashboard ones)
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Gaze Fade");
        ImGui::Columns(2, "ColumnGazeFade", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& gazefade_enabled = ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_gazefade_enabled);
        if (ImGui::Checkbox("Gaze Fade Active", &gazefade_enabled))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_gazefade_enabled), gazefade_enabled);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();

        if (!gazefade_enabled)
            ImGui::PushItemDisabled();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Gaze Distance");
        ImGui::NextColumn();

        float& distance = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_gazefade_distance);

        //Note about the "##%.2f": ImGui sliders read the precision for rounding out of the format string. This leads to weird behavior when switching to labels without it
        //Fortunately, ImGui has string ID notations which don't get rendered so we can abuse this here
        if (ImGui::SliderWithButtonsFloat("OverlayFadeGazeDistance", distance, 0.05f, 0.01f, 0.0f, 1.5f, (distance < 0.01f) ? "Infinite##%.2f" : "%.2f m"))
        {
            if (distance < 0.01f)
                distance = 0.0f;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_gazefade_distance), *(LPARAM*)&distance);
        }
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Fade Rate");
        ImGui::NextColumn();

        float& rate = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_gazefade_rate);

        if (ImGui::SliderWithButtonsFloat("OverlayFadeGazeRate", rate, 0.1f, 0.025f, 0.4f, 3.0f, "%.2fx", ImGuiSliderFlags_Logarithmic))
        {
            if (rate < 0.0f)
                rate = 0.0f;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_gazefade_rate), *(LPARAM*)&rate);
        }
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Target Opacity");
        ImGui::NextColumn();

        float& target_opacity = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_gazefade_opacity);

        if (ImGui::SliderWithButtonsFloatPercentage("OverlayFadeGazeOpacity", target_opacity, 5, 1, 0, 100, "%d%%"))
        {
            target_opacity = clamp(target_opacity, 0.0f, 1.0f);

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_gazefade_opacity), *(LPARAM*)&target_opacity);
        }
        ImGui::NextColumn();

        if (!gazefade_enabled)
            ImGui::PopItemDisabled();

        if (UIManager::Get()->IsOpenVRLoaded())
        {
            bool is_overlay_enabled = ConfigManager::Get().GetConfigBool(configid_bool_overlay_enabled);

            //At least disable the button when the overlay surely can't be visible. Doesn't catch other cases though
            if (!is_overlay_enabled)
                ImGui::PushItemDisabled();

            if (ImGui::Button("Set from Gaze"))
            {
                ImGui::OpenPopup("PopupGazeFadeAutoConfigure");

                //Deactivate gaze fade during the popup so the overlay is visible to the user
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_gazefade_enabled), false);
            }

            if (!is_overlay_enabled)
                ImGui::PopItemDisabled();

            ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("PopupGazeFadeAutoConfigure", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
            {
                static double start_time = 0;

                if (ImGui::IsWindowAppearing())
                {
                    start_time = ImGui::GetTime();
                }

                ImGui::Text("Look at the center of the overlay and wait for 3 seconds...");

                if ( (start_time + 3.0 < ImGui::GetTime()) || (ImGui::IsMouseClicked(ImGuiPopupFlags_MouseButtonLeft, false)) ) //Also allows to click to skip the wait
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_gaze_fade_auto);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_gazefade_enabled), true);
                    gazefade_enabled = true;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        ImGui::Columns(1);
    }

    //Update Limiter Override
    if (capture_source != ovrl_capsource_ui)
    {
        UpdateLimiterSetting(column_width_0, true);
    }

    //Input
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Input");
        ImGui::Columns(2, "ColumnOverlayInput", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& enable_input = ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_input_enabled);
        if (ImGui::Checkbox("Enable Input", &enable_input))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_input_enabled), enable_input);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Overlay Group");

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Overlay groups are used to target overlays in actions and input bindings");

        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);
        const char* items[] = { "None", "Group 1", "Group 2", "Group 3" };
        int group_id = clamp(ConfigManager::Get().GetConfigIntRef(configid_int_overlay_group_id), 0, IM_ARRAYSIZE(items) - 1);
        if (ImGui::Combo("##ComboGroupID", &group_id, items, IM_ARRAYSIZE(items)))
        {
            ConfigManager::Get().SetConfigInt(configid_int_overlay_group_id, group_id);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_group_id), group_id);
        }

        ImGui::Columns(1);
    }

    //Performance
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Performance");
    ImGui::Columns(2, "ColumnPerformance", false);
    ImGui::SetColumnWidth(0, column_width_0 * 2.0f);

    bool& always_update = ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_update_invisible);
    if (ImGui::Checkbox("Update when Invisible", &always_update))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_update_invisible), always_update);
    }
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::FixedHelpMarker("Update overlay even when invisible from Opacity setting or Gaze Fade.\nHelps with third-party applications accessing the overlay's contents. Not recommended otherwise.\nUpdates are still suspended if the overlay is disabled or hidden by Display Mode setting.");

    ImGui::Columns(1);

    ImGui::EndChild();
    ImGui::EndTabItem();
}

void WindowSettings::UpdateCatOverlayTabInterface()
{
    const float column_width_0 = ImGui::GetFontSize() * 20.0f;
    bool detached = ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached);

    ImGui::BeginChild("ViewOverlayTabInterface");

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    //Floating UI
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Floating UI");
        ImGui::Columns(2, "ColumnFloatingUI", false);
        ImGui::SetColumnWidth(0, column_width_0);

        //Pure UI states, no need to sync
        if (detached)
        {
            ImGui::Checkbox("Show Floating UI", &ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_floatingui_enabled));
        }
        else //Dashboard's UI is always visible, even though it's technically not the floating UI
        {
            ImGui::PushItemDisabled();
            ImGui::Checkbox("Show Floating UI", &ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_floatingui_enabled));
            ImGui::PopItemDisabled();
        }

        if (ImGui::Checkbox("Show Desktop Buttons", &ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_floatingui_desktops_enabled)))
        {
            UIManager::Get()->RepeatFrame();
        }

        if (!DPWinRT_IsCaptureFromHandleSupported())
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Desktop switching is not supported for Graphics Capture overlays on this system", "(!)");
        }
        else if (!DPWinRT_IsCaptureFromCombinedDesktopSupported())
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Switching to combined desktop is not supported for Graphics Capture overlays on this system", "(!)");
        }

        ImGui::Columns(1);
    }

    //Action Order
    {
        ActionOrderSetting(OverlayManager::Get().GetCurrentOverlayID());
    }

    ImGui::EndChild();
    ImGui::EndTabItem();
}

void WindowSettings::UpdateCatInterface()
{
    ImGui::Text("Interface");
            
    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewInterfaceSettings");

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Most interface options don't need to be sent to the dashboard overlay application

    //Settings Interface
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Settings Interface");
        ImGui::Columns(2, "ColumnInterfaceSettingsUI", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Display Scale");

        if (UIManager::Get()->IsInDesktopMode())
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Does not apply in desktop mode");
        }
        ImGui::NextColumn();

        bool& use_large_style = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_large_style);

        if (ImGui::RadioButton("Compact", !use_large_style))
        {
            use_large_style = false;
            TextureManager::Get().ReloadAllTexturesLater();
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("Large", use_large_style))
        {
            use_large_style = true;
            TextureManager::Get().ReloadAllTexturesLater();
        }

        ImGui::Columns(1);
    }

    //Desktop Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Desktop Buttons");
        ImGui::Columns(2, "ColumnDesktopButtons", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Listing Style");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);
        const char* items[] = { "None", "Individual Desktops", "Cycle Buttons" };
        int button_style = clamp(ConfigManager::Get().GetConfigIntRef(configid_int_interface_mainbar_desktop_listing), 0, IM_ARRAYSIZE(items) - 1);
        if (ImGui::Combo("##ComboButtonStyle", &button_style, items, IM_ARRAYSIZE(items)))
        {
            ConfigManager::Get().SetConfigInt(configid_int_interface_mainbar_desktop_listing, button_style);
            UIManager::Get()->RepeatFrame();
        }

        ImGui::NextColumn();

        bool& include_all = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_mainbar_desktop_include_all);
        if (ImGui::Checkbox("Add Combined Desktop", &include_all))
        {
            UIManager::Get()->RepeatFrame();
        }

        ImGui::Columns(1);
    }

    //Action Buttons
    {
        ActionOrderSetting();
    }

    //Environment
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Environment");

        ImGui::Columns(2, "ColumnEnvironment", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Background Color");
        ImGui::NextColumn();

        static ImVec4 background_color_vec4;

        if (ImGui::IsWindowAppearing())
        {
            //Unpack color value from config
            unsigned int rgba = *(unsigned int*)&ConfigManager::Get().GetConfigIntRef(configid_int_interface_background_color);
            background_color_vec4.x = ((rgba & 0xFF000000) >> 24) / 255.0f; //R
            background_color_vec4.y = ((rgba & 0x00FF0000) >> 16) / 255.0f; //B
            background_color_vec4.z = ((rgba & 0x0000FF00) >> 8)  / 255.0f; //G
            background_color_vec4.w =  (rgba & 0x000000FF)        / 255.0f; //A
        }

        const ImGuiColorEditFlags flags = ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview |
                                          ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop;
        if (ImGui::ColorEdit4Simple("##BackgroundColor", (float*)&background_color_vec4, flags))
        {
            //Pack color values back to config (this is doing float -> int conversion the same way the ImGui picker does internally)
            unsigned int r = (unsigned int)(background_color_vec4.x * 255.0f + 0.5f);
            unsigned int g = (unsigned int)(background_color_vec4.y * 255.0f + 0.5f);
            unsigned int b = (unsigned int)(background_color_vec4.z * 255.0f + 0.5f);
            unsigned int a = (unsigned int)(background_color_vec4.w * 255.0f + 0.5f);
            unsigned int rgba = (r << 24) | (g << 16) | (b << 8) | a;

            ConfigManager::Get().SetConfigInt(configid_int_interface_background_color, *(int*)&rgba);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_background_color), *(int*)&rgba);
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        ImGui::SetNextItemWidth(-1);
        const char* items[] = { "Never Visible", "Only Visible in Desktop+ Tab", "Always Visible" };
        int display_mode = clamp(ConfigManager::Get().GetConfigInt(configid_int_interface_background_color_display_mode), 0, IM_ARRAYSIZE(items) - 1);
        if (ImGui::Combo("##ComboBackgroundDisplay", &display_mode, items, IM_ARRAYSIZE(items)))
        {
            ConfigManager::Get().SetConfigInt(configid_int_interface_background_color_display_mode, display_mode);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_background_color_display_mode), display_mode);
        }

        ImGui::NextColumn();

        bool& dim_ui = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_dim_ui);
        if (ImGui::Checkbox("Dim Interface", &dim_ui))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_interface_dim_ui), dim_ui);

            if (UIManager::Get()->IsOpenVRLoaded())
            {
                UIManager::Get()->UpdateOverlayDimming();
            }
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Dims the SteamVR dashboard and Desktop+ UI while the Desktop+ dashboard tab is open");
    }

    //Windows Mixed Reality
    {
        //This stuff is only shown to WMR systems
        //Assume it's WMR if these settings were changed, this way these option will be available in desktop mode if required
        if (ConfigManager::Get().GetConfigInt(configid_int_interface_wmr_ignore_vscreens) != -1)
        {
            bool ignore_vscreens = (ConfigManager::Get().GetConfigInt(configid_int_interface_wmr_ignore_vscreens) == 1);

            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Windows Mixed Reality");
            ImGui::Columns(2, "ColumnInterfaceWMR", false);
            ImGui::SetColumnWidth(0, column_width_0 * 2.0f);

            if (ImGui::Checkbox("Ignore WMR Virtual Desktops", &ignore_vscreens))
            {
                ConfigManager::Get().SetConfigInt(configid_int_interface_wmr_ignore_vscreens, ignore_vscreens);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_wmr_ignore_vscreens), ignore_vscreens);
            }

            ImGui::Columns(1);
        }
    }

    ImGui::EndChild();
}

void WindowSettings::UpdateCatActions()
{
    //We want to show binding callouts further below, but as of writing this code, showing the callouts via the API only appears to work if they've been previously displayed in the session.
    //Displaying them from the controller settings screen works. And after that, the API calls work as well (we use a hacky way here, but this applies to regular ones as well afaik).
    //When it works, the binding callout overlay exists, so we can use that info to not offer a broken menu item at least. Still not so great, but maybe it'll get fixed.
    static bool is_binding_callout_overlay_available = false;

    ImGui::Text("Actions");

    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewActionsSettings");

    if (ImGui::IsWindowAppearing())
    {
        UIManager::Get()->RepeatFrame();

        //Check if the binding callout overlay exists
        if (UIManager::Get()->IsOpenVRLoaded())
        {
            vr::VROverlayHandle_t ovrl_handle = vr::k_ulOverlayHandleInvalid;
            vr::VROverlay()->FindOverlay("system.vrwebhelper.bindingcallouts", &ovrl_handle);
            is_binding_callout_overlay_available = (ovrl_handle != vr::k_ulOverlayHandleInvalid);
        }
    }

    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Active Controller Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Active Controller Buttons");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (UIManager::Get()->IsOpenVRLoaded())
        {
            ImGui::FixedHelpMarker("Controller bindings when pointing at the overlay.\nClick here to show or configure the VR Dashboard controller bindings and change which buttons these are.");

            //Somewhat hidden, but still convenient shortcut to the controller binding page
            if ((UIManager::Get()->IsOpenVRLoaded()) && (ImGui::IsItemClicked()))
            {
                ImGui::OpenPopup("PopupOpenControllerBindingsCompositor");  //OpenPopupOnItemClick() doesn't work with this btw
            }

            if (ImGui::BeginPopup("PopupOpenControllerBindingsCompositor"))
            {
                if ( (is_binding_callout_overlay_available) && (ImGui::Selectable("Show VR Dashboard Controller Bindings")) )
                {
                    //Pretend to be the VR Dashboard app for a split second to get the action set handle and show the bindings for it
                    //Unfortunately there are no APIs to read binding data of other applications... or even the own when not actively using it as far as I can tell
                    vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), "openvr.component.vrcompositor");

                    vr::VRActionSetHandle_t handle_actionset = vr::k_ulInvalidActionSetHandle;

                    if (vr::VRInput()->GetActionSetHandle("/actions/lasermouse", &handle_actionset) == vr::VRInputError_None)
                    {
                        vr::VRActiveActionSet_t actionset_desc = {0};
                        actionset_desc.ulActionSet = handle_actionset;

                        vr::VRInput()->ShowBindingsForActionSet(&actionset_desc, sizeof(vr::VRActiveActionSet_t), 1, vr::k_ulInvalidInputValueHandle);
                    }

                    vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyUIApp);
                }

                if (ImGui::Selectable("Configure VR Dashboard Controller Bindings"))
                {
                    //OpenBindingUI does not use that app key argument it takes, it always opens the bindings of the calling application
                    //To work around this, we pretend to be the app we want to open the bindings for during the call
                    //Works and seems to not break anything
                    vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), "openvr.component.vrcompositor");
                    vr::VRInput()->OpenBindingUI("openvr.component.vrcompositor", vr::k_ulInvalidActionSetHandle, vr::k_ulInvalidInputValueHandle, UIManager::Get()->IsInDesktopMode());
                    vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyUIApp);
                }
                ImGui::EndPopup();
            }
        }
        else
        {
            ImGui::FixedHelpMarker("Controller bindings when pointing at the overlay.\nConfigure the VR Dashboard controller bindings to change which buttons these are.");
        }

        ActionID actionid_home = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_go_home_action_id);
        ActionID actionid_back = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_go_back_action_id);

        ImGui::Columns(2, "ColumnControllerButtonActions", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Go Home\" Action");
        ImGui::NextColumn();

        if (ButtonAction("ActionGoHome", actionid_home))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_go_home_action_id, actionid_home);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_go_home_action_id), actionid_home);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Go Back\" Action");
        ImGui::NextColumn();

        if (ButtonAction("ActionGoBack", actionid_back))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_go_back_action_id, actionid_back);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_go_back_action_id), actionid_back);
        }

        ImGui::Columns(1);
    }

    //Global Controller Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Global Controller Buttons");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (UIManager::Get()->IsOpenVRLoaded())
        {
            ImGui::FixedHelpMarker("Controller bindings when the dashboard is closed and not pointing at an overlay.\nClick here to show or configure the Desktop+ controller bindings and change which buttons these are.");

            //Somewhat hidden, but still convenient shortcut to the controller binding page
            if ((UIManager::Get()->IsOpenVRLoaded()) && (ImGui::IsItemClicked()))
            {
                ImGui::OpenPopup("PopupOpenControllerBindingsDesktopPlus");  //OpenPopupOnItemClick() doesn't work with this btw
            }

            if (ImGui::BeginPopup("PopupOpenControllerBindingsDesktopPlus"))
            {
                if ( (is_binding_callout_overlay_available) && (ImGui::Selectable("Show Desktop+ Controller Bindings")) )
                {
                    //See comment on the active controller buttons
                    vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyDashboardApp);

                    vr::VRActionSetHandle_t handle_actionset = vr::k_ulInvalidActionSetHandle;

                    if (vr::VRInput()->GetActionSetHandle("/actions/shortcuts", &handle_actionset) == vr::VRInputError_None)
                    {
                        vr::VRActiveActionSet_t actionset_desc = {0};
                        actionset_desc.ulActionSet = handle_actionset;

                        vr::VRInput()->ShowBindingsForActionSet(&actionset_desc, sizeof(vr::VRActiveActionSet_t), 1, vr::k_ulInvalidInputValueHandle);
                    }

                    vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyUIApp);
                }

                if (ImGui::Selectable("Configure Desktop+ Controller Bindings"))
                {
                    //See comment on the active controller buttons
                    vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyDashboardApp);
                    vr::VRInput()->OpenBindingUI(g_AppKeyDashboardApp, vr::k_ulInvalidActionSetHandle, vr::k_ulInvalidInputValueHandle, UIManager::Get()->IsInDesktopMode());
                    vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyUIApp);
                }

                ImGui::EndPopup();
            }
        }
        else
        {
            ImGui::FixedHelpMarker("Controller bindings when the dashboard is closed and not pointing at an overlay.\nConfigure the Desktop+ controller bindings to change which buttons these are.");
        }

        ActionID actionid_global_01 = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut01_action_id);
        ActionID actionid_global_02 = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut02_action_id);
        ActionID actionid_global_03 = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut03_action_id);

        ImGui::Columns(2, "ColumnControllerButtonGlobalActions", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Global Shortcut 1\" Action");
        ImGui::NextColumn();

        if (ButtonAction("ActionGlobalShortcut1", actionid_global_01))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_shortcut01_action_id, actionid_global_01);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_shortcut01_action_id), actionid_global_01);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Global Shortcut 2\" Action");
        ImGui::NextColumn();

        if (ButtonAction("ActionGlobalShortcut2", actionid_global_02))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_shortcut02_action_id, actionid_global_02);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_shortcut02_action_id), actionid_global_02);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Global Shortcut 3\" Action");
        ImGui::NextColumn();

        if (ButtonAction("ActionGlobalShortcut3", actionid_global_03))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_shortcut03_action_id, actionid_global_03);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_shortcut03_action_id), actionid_global_03);
        }

        ImGui::Columns(1);
    }

    //Global Hotkeys
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Global Hotkeys");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("System-wide keyboard shortcuts.\nHotkeys block other applications from receiving that input and may not work if the same combination has already been registered elsewhere.");

        ActionID actionid_hotkey_01 = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_hotkey01_action_id);
        ActionID actionid_hotkey_02 = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_hotkey02_action_id);
        ActionID actionid_hotkey_03 = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_hotkey03_action_id);

        //Adjust column width automatically if there's a stupidly long hotkey button/name
        static float hotkey_button_width = 0.0f;

        ImGui::Columns(2, "ColumnHotkeyActions", false);
        ImGui::SetColumnWidth(0, std::max(column_width_0, hotkey_button_width));

        float hotkey_button_width_temp = 0.0f;  //Collect longest hotkey button width first

        //Hotkey 1
        ImGui::AlignTextToFramePadding();
        ButtonHotkey(0);

        ImGui::SameLine();
        ImGui::Text("Action");
        ImGui::SameLine();

        if (ImGui::GetCursorPosX() > hotkey_button_width_temp)
            hotkey_button_width_temp = ImGui::GetCursorPosX();

        ImGui::NextColumn();

        if (ButtonAction("ActionHotkey1", actionid_hotkey_01))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_hotkey01_action_id, actionid_hotkey_01);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_hotkey01_action_id), actionid_hotkey_01);
        }

        ImGui::NextColumn();

        //Hotkey 2
        ImGui::AlignTextToFramePadding();
        ButtonHotkey(1);

        ImGui::SameLine();
        ImGui::Text("Action");
        ImGui::SameLine();

        if (ImGui::GetCursorPosX() > hotkey_button_width_temp)
            hotkey_button_width_temp = ImGui::GetCursorPosX();

        ImGui::NextColumn();

        if (ButtonAction("ActionHotkey2", actionid_hotkey_02))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_hotkey02_action_id, actionid_hotkey_02);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_hotkey02_action_id), actionid_hotkey_02);
        }

        ImGui::NextColumn();

        //Hotkey 3
        ImGui::AlignTextToFramePadding();
        ButtonHotkey(2);

        ImGui::SameLine();
        ImGui::Text("Action");
        ImGui::SameLine();

        if (ImGui::GetCursorPosX() > hotkey_button_width_temp)
            hotkey_button_width_temp = ImGui::GetCursorPosX();

        ImGui::NextColumn();

        if (ButtonAction("ActionHotkey3", actionid_hotkey_03))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_hotkey03_action_id, actionid_hotkey_03);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_hotkey03_action_id), actionid_hotkey_03);
        }

        ImGui::NextColumn();

        hotkey_button_width = hotkey_button_width_temp;

        ImGui::Columns(1);
    }

    //Custom Actions
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Custom Actions");

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);
        ImGui::BeginChild("ViewCustomActions", ImVec2(-ImGui::GetStyle().ItemSpacing.y, ImGui::GetFrameHeight() * 7.0f), true);

        static int list_selected_index = -1;
        static bool delete_confirm_state = false; //Simple uninstrusive extra confirmation step for deleting actions 

        if (ImGui::IsWindowAppearing())
        {
            //Reset state after switching panes or hiding the window
            delete_confirm_state = false;
        }

        std::vector<CustomAction>& actions = ConfigManager::Get().GetCustomActions();
        int act_index = 0;
        for (CustomAction& action : actions)
        {
            ImGui::PushID(&action);

            if (ImGui::Selectable(action.Name.c_str(), (list_selected_index == act_index)))
            {
                list_selected_index = act_index;
                delete_confirm_state = false;
            }

            ImGui::PopID();

            act_index++;
        }

        if (m_ActionEditIsNew) //Make newly created action visible
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button("New Action"))
        {
            list_selected_index = (int)actions.size();
            delete_confirm_state = false;
                
            CustomAction act;
            act.Name = "New Action";

            actions.push_back(act);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current), actions.size() - 1);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 1);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_value_int), (int)act.FunctionType);

            ConfigManager::Get().GetActionMainBarOrder().push_back({ (ActionID)(actions.size() - 1 + action_custom), false });

            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                auto& action_order = OverlayManager::Get().GetConfigData(i).ConfigActionBarOrder;

                action_order.push_back({ (ActionID)(actions.size() - 1 + action_custom), false });
            }

            m_ActionEditIsNew = true;

            ImGui::OpenPopup("ActionEditPopup");
        }

        ImGui::SameLine();

        bool buttons_disabled = (list_selected_index == -1); //State can change in-between

        if (buttons_disabled)
            ImGui::PushItemDisabled();

        if (UIManager::Get()->IsOpenVRLoaded())
        {
            if (ImGui::Button("Do"))
            {
                if (actions[list_selected_index].FunctionType != caction_press_keys) //Press and release of action keys is handled below instead
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_do, action_custom + list_selected_index);
                }
                delete_confirm_state = false;
            }

            //Enable press and release of action keys based on button press
            if (ImGui::IsItemActivated())
            {
                if (actions[list_selected_index].FunctionType == caction_press_keys)
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_start, action_custom + list_selected_index);
                }
            }

            if (ImGui::IsItemDeactivated())
            {
                if (actions[list_selected_index].FunctionType == caction_press_keys)
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_stop, action_custom + list_selected_index);
                }
            }

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }


        if (ImGui::Button("Edit"))
        {
            m_ActionEditIsNew = false;
            ImGui::OpenPopup("ActionEditPopup");
            delete_confirm_state = false;
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (delete_confirm_state)
        {
            if (ImGui::Button("Really?"))
            {
                ActionManager::Get().EraseCustomAction(list_selected_index);
                UIManager::Get()->RepeatFrame();

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_delete, list_selected_index);

                list_selected_index = -1;

                delete_confirm_state = false;
            }
        }
        else
        {
            if (ImGui::Button("Delete"))
            {
                delete_confirm_state = true;
            }
        }

        if (buttons_disabled)
            ImGui::PopItemDisabled();

        if ( (list_selected_index != -1) && (actions.size() > list_selected_index) ) //If actually exists
        {
            PopupActionEdit(actions[list_selected_index], list_selected_index);

            if (actions.size() <= list_selected_index) //New Action got deleted by the popup, reset selection
            {
                list_selected_index = -1;
            }
        }
    }

    ImGui::EndChild();
}

void WindowSettings::UpdateCatInput()
{
    ImGui::Text("Input");

    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewInputSettings");

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Mouse
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Mouse");
        ImGui::Columns(2, "ColumnMouse", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& render_cursor = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_mouse_render_cursor);
        if (ImGui::Checkbox("Render Cursor", &render_cursor))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_mouse_render_cursor), render_cursor);
        }

        if (!DPWinRT_IsCaptureCursorEnabledPropertySupported())
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Disabling the cursor for Graphics Capture overlays is not supported on this system", "(!)");
        }

        bool& render_blob = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_mouse_render_intersection_blob);
        if (ImGui::Checkbox("Render Intersection Blob", &render_blob))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_mouse_render_intersection_blob), render_blob);
        }

        bool& pointer_override = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_mouse_hmd_pointer_override);
        if (ImGui::Checkbox("Allow HMD-Pointer Override", &pointer_override))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_mouse_hmd_pointer_override), pointer_override);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Disables the laser pointer when the physical mouse is moved rapidly after the dashboard was opened with the HMD button.\nRe-open or click the overlay to get the laser pointer back.");

        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Double-Click Assistant"); 
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Freezes the mouse cursor for the set duration to ease the input of double-clicks");

        ImGui::NextColumn();

        //The way mapping max + 1 == -1 value into the slider is done is a bit convoluted again, but still works
        int& assist_duration = ConfigManager::Get().GetConfigIntRef(configid_int_input_mouse_dbl_click_assist_duration_ms);
        int assist_duration_max = 3000; //The "Auto" wrapping makes this the absolute maximum value even with manual input, but longer than 3 seconds is questionable either way
        int assist_duration_ui = (assist_duration == -1) ? assist_duration_max + 1 : assist_duration;

        if (ImGui::SliderWithButtonsInt("DBLClickAssist", assist_duration_ui, 25, 5, 0, assist_duration_max + 1, (assist_duration == -1) ? "Auto" : (assist_duration == 0) ? "Off" : "%d ms"))
        {
            assist_duration = clamp(assist_duration_ui, 0, assist_duration_max + 1);

            if (assist_duration_ui > assist_duration_max)
                assist_duration = -1;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_mouse_dbl_click_assist_duration_ms), assist_duration);
        }

        ImGui::Columns(1);
    }

    //Keyboard
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Keyboard");
        ImGui::Columns(2, "ColumnKeyboard", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& enable_keyboard_helper = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_keyboard_helper_enabled);
        if (ImGui::Checkbox("Enable Keyboard Extension", &enable_keyboard_helper))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_keyboard_helper_enabled), enable_keyboard_helper);
        }

        ImGui::Columns(1);
    }

    //Floating Overlay
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Floating Overlay");
        ImGui::Columns(2, "ColumnDetachedOverlay", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Interaction Auto-Toggle Max Distance");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Maximum allowed distance between overlay and pointing controller to automatically toggle interaction while the dashboard is closed.\nSet this to \"Off\" when using the global controller binding toggle.");

        ImGui::NextColumn();

        float& distance = ConfigManager::Get().GetConfigFloatRef(configid_float_input_detached_interaction_max_distance);
        if (ImGui::SliderWithButtonsFloat("LaserPointerMaxDistance", distance, 0.05f, 0.01f, 0.0f, 3.0f, (distance < 0.01f) ? "Off##%.2f" : "%.2f m", ImGuiSliderFlags_Logarithmic))
        {
            if (distance < 0.01f)
                distance = 0.0f;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_input_detached_interaction_max_distance), *(LPARAM*)&distance);
        }

        ImGui::NextColumn();

        bool& global_pointer = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_global_hmd_pointer);
        if (ImGui::Checkbox("Global HMD-Pointer", &global_pointer))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_global_hmd_pointer), global_pointer);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Enables using HMD gaze to point at Desktop+ overlays while the dashboard is closed");

        ImGui::NextColumn();
        ImGui::NextColumn();

        if (!global_pointer)
            ImGui::PushItemDisabled();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Global HMD-Pointer Max Distance");

        ImGui::NextColumn();

        float& hmd_distance = ConfigManager::Get().GetConfigFloatRef(configid_float_input_global_hmd_pointer_max_distance);
        if (ImGui::SliderWithButtonsFloat("HMDPointerMaxDistance", hmd_distance, 0.05f, 0.01f, 0.0f, 3.0f, (hmd_distance < 0.01f) ? "Infinite##%.2f" : "%.2f m", ImGuiSliderFlags_Logarithmic))
        {
            if (hmd_distance < 0.01f)
                hmd_distance = 0.0f;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_input_global_hmd_pointer_max_distance), *(LPARAM*)&hmd_distance);
        }

        if (!global_pointer)
            ImGui::PopItemDisabled();

        ImGui::Columns(1);
    }

    ImGui::EndChild();
}

void WindowSettings::UpdateCatWindows()
{
    ImGui::Text("Windows");

    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewWindowsSettings");

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    const float column_width_0 = ImGui::GetFontSize() * 25.0f;

    //General
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "General");

        ImGui::Columns(2, "ColumnWindowsGeneral", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& focus_scene_app = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_auto_focus_scene_app_dashboard);
        if (ImGui::Checkbox("Focus Scene-App on Dashboard Deactivation", &focus_scene_app))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_auto_focus_scene_app_dashboard), focus_scene_app);
        }

        ImGui::Columns(1);
    }

    //Graphics Capture
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Graphics Capture");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("These settings only apply to Graphics Capture overlays with a source window");

        ImGui::Columns(2, "ColumnWindowsGraphicsCapture", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& auto_focus = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_winrt_auto_focus);
        if (ImGui::Checkbox("Focus Window when Pointing at Overlay", &auto_focus))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_winrt_auto_focus), auto_focus);
        }

        bool& keep_on_screen = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_winrt_keep_on_screen);
        if (ImGui::Checkbox("Keep Window on Screen", &keep_on_screen))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_winrt_keep_on_screen), keep_on_screen);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Automatically move source window inside the screen's work area if its bounds lie outside of it");

        bool& auto_size_overlay = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_winrt_auto_size_overlay);
        if (ImGui::Checkbox("Adjust Overlay Size when Window Resizes", &auto_size_overlay))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_winrt_auto_size_overlay), auto_size_overlay);
        }

        bool& focus_scene_app = ConfigManager::Get().GetConfigBoolRef(configid_bool_windows_winrt_auto_focus_scene_app);
        if (ImGui::Checkbox("Focus Scene-App when Laser Pointer leaves Overlay", &focus_scene_app))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_windows_winrt_auto_focus_scene_app), focus_scene_app);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("On Window Drag");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);
        const char* items[] = { "Do Nothing", "Block Drag", "Drag Overlay" };
        int mode_dragging = clamp(ConfigManager::Get().GetConfigInt(configid_int_windows_winrt_dragging_mode), 0, IM_ARRAYSIZE(items) - 1);
        if (ImGui::Combo("##ComboLimitMode", &mode_dragging, items, IM_ARRAYSIZE(items)))
        {
            ConfigManager::Get().SetConfigInt(configid_int_windows_winrt_dragging_mode, mode_dragging);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_windows_winrt_dragging_mode), mode_dragging);
        }

        ImGui::NextColumn();
        ImGui::Columns(1);
    }

    ImGui::EndChild();
}

void WindowSettings::UpdateCatPerformance()
{
    ImGui::Text("Performance");
            
    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewPerformanceSettings");

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Update Limiter
    {
        UpdateLimiterSetting(column_width_0);
    }

    //Desktop Duplication
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Desktop Duplication");

        ImGui::Columns(2, "ColumnPerformanceDesktopDuplication", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& rapid_updates = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_rapid_laser_pointer_updates);
        if (ImGui::Checkbox("Rapid Laser Pointer Updates", &rapid_updates))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_performance_rapid_laser_pointer_updates), rapid_updates);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Burn additional CPU cycles to make the laser pointer cursor as accurate as possible.\nOnly affects CPU load when pointing at the overlay.");

        bool& single_desktop = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_single_desktop_mirroring);
        if (ImGui::Checkbox("Single Desktop Mirroring", &single_desktop))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_performance_single_desktop_mirroring), single_desktop);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Mirror individual desktops when switching to them instead of cropping from the combined desktop.\nWhen this is active, all overlays will be showing the same desktop.");

        ImGui::Columns(1);
    }

    //Performance Monitor
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Performance Monitor");
        if ( (UIManager::Get()->IsOpenVRLoaded()) && (UIManager::Get()->IsInDesktopMode()) )
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Performance Monitor overlays do not update in desktop mode");
        }

        ImGui::Columns(2, "ColumnPerformancePerformanceMonitor", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Style");
        ImGui::NextColumn();

        bool& use_large_style = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_large_style);

        if (ImGui::RadioButton("Compact", !use_large_style))
        {
            use_large_style = false;
            UIManager::Get()->RepeatFrame();
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("Large", use_large_style))
        {
            use_large_style = true;
            UIManager::Get()->RepeatFrame();
        }

        //Monitor items
        ImGui::Columns(1);
        ImGui::Columns( (m_IsStyleScaled) ? 2 : 3, "ColumnPerformancePerformanceMonitorItems", false);    //Use 2 columns when using large UI since 3 won't fit
        ImGui::SetColumnWidth(0, column_width_0);
        ImGui::SetColumnWidth(1, column_width_0);

        bool& show_cpu             = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_show_cpu);
        bool& show_gpu             = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_show_gpu);
        bool& show_graphs          = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_show_graphs);
        bool& show_fps             = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_show_fps);
        bool& show_battery         = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_show_battery);
        bool& show_time            = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_show_time);
        bool& show_trackers        = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_show_trackers);
        bool& show_vive_wireless   = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_show_vive_wireless);
        bool& disable_gpu_counters = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_monitor_disable_gpu_counters);

        //Keep unavailable options as enabled but show the check boxes as unticked to avoid confusion
        bool show_graphs_visual        = ( (use_large_style) && ((!show_cpu) && (!show_gpu)) ) ? false : show_graphs;
        bool show_time_visual          = ( ((!show_fps) && (!show_battery)) || (!use_large_style) ) ? false : show_time;
        bool show_trackers_visual      = (!show_battery) ? false : show_trackers;
        bool show_vive_wireless_visual = (!show_battery) ? false : show_vive_wireless;

        if (ImGui::Checkbox("Show CPU Stats", &show_cpu))
        {
            UIManager::Get()->RepeatFrame();
        }

        ImGui::NextColumn();

        if (ImGui::Checkbox("Show GPU Stats", &show_gpu))
        {
            UIManager::Get()->RepeatFrame();
        }

        ImGui::NextColumn();

        if ( (use_large_style) && ((!show_cpu) && (!show_gpu)) )
            ImGui::PushItemDisabled();

        if (ImGui::Checkbox("Show Graphs", &show_graphs_visual))
        {
            show_graphs = show_graphs_visual;
            UIManager::Get()->RepeatFrame();
        }

        if ( (use_large_style) && ((!show_cpu) && (!show_gpu)) )
            ImGui::PopItemDisabled();

        ImGui::NextColumn();

        if (ImGui::Checkbox("Show Frame Stats", &show_fps))
        {
            UIManager::Get()->RepeatFrame();
        }

        ImGui::NextColumn();

        if ( ((!show_fps) && (!show_battery)) || (!use_large_style) )
            ImGui::PushItemDisabled();

        if (ImGui::Checkbox("Show Time", &show_time_visual))
        {
            show_time = show_time_visual;
            UIManager::Get()->RepeatFrame();
        }

        if ( ((!show_fps) && (!show_battery)) || (!use_large_style) )
            ImGui::PopItemDisabled();

        ImGui::NextColumn();

        if (!m_IsStyleScaled)
            ImGui::NextColumn();

        if (ImGui::Checkbox("Show Battery Stats", &show_battery))
        {
            UIManager::Get()->RepeatFrame();
        }

        ImGui::NextColumn();

        if (!show_battery)
            ImGui::PushItemDisabled();

        if (ImGui::Checkbox("Show Tracker Battery Levels", &show_trackers_visual))
        {
            show_trackers = show_trackers_visual;
            UIManager::Get()->RepeatFrame();
        }

        ImGui::NextColumn();

        if (UIManager::Get()->GetPerformanceWindow().IsViveWirelessInstalled())
        {
            if (ImGui::Checkbox("Show Vive Wireless Temperature", &show_vive_wireless_visual))
            {
                show_vive_wireless = show_vive_wireless_visual;
                UIManager::Get()->RepeatFrame();
            }
        }

        if (!show_battery)
            ImGui::PopItemDisabled();

        ImGui::NextColumn();

        if (ImGui::Checkbox("Disable GPU Performance Counters", &disable_gpu_counters))
        {
            //Update active performance counter state if the window is currently visible
            if (UIManager::Get()->GetPerformanceWindow().IsVisible())
            {
                if (disable_gpu_counters)
                {
                    UIManager::Get()->GetPerformanceWindow().GetPerformanceData().DisableGPUCounters();
                }
                else
                {
                    UIManager::Get()->GetPerformanceWindow().GetPerformanceData().EnableCounters(true);
                }
            }

            UIManager::Get()->RepeatFrame();
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Disables display of GPU load % and VRAM usage.\nThis prevents GPU hardware monitoring related stutter with recent NVIDIA drivers.");

        ImGui::Columns(1);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

        if (UIManager::Get()->IsOpenVRLoaded()) //Only show when OpenVR is loaded since many of the monitor items don't work at all
        {
            if (ImGui::Button("View as Pop-Up"))
            {
                ImGui::OpenPopup("PopupPerformanceMonitor");
            }

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }


        if (ImGui::Button("Add as Overlay"))
        {
            unsigned int new_id = OverlayManager::Get().AddUIOverlay();
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_new_ui);

            OverlayManager::Get().SetCurrentOverlayID(new_id);
            OverlayManager::Get().SetCurrentOverlayNameAuto();
            ConfigManager::Get().SetConfigInt(configid_int_interface_overlay_current_id, (int)new_id);

            UIManager::Get()->GetPerformanceWindow().ScheduleOverlaySharedTextureUpdate();
            m_OverlayNameBufferNeedsUpdate = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Reset Cumulative Values"))
        {
            UIManager::Get()->GetPerformanceWindow().ResetCumulativeValues();
        }

    }

    if (ImGui::IsPopupOpen("PopupPerformanceMonitor"))
    {
        //PerformanceMonitor does not work with the large interface scale, so pop it temporarily
        PopInterfaceScale();
        UIManager::Get()->GetPerformanceWindow().SetPopupOpen(true);
        UIManager::Get()->GetPerformanceWindow().Update(true);
        PushInterfaceScale();
    }
    else
    {
        UIManager::Get()->GetPerformanceWindow().SetPopupOpen(false);
    }

    //Stats
    if (UIManager::Get()->IsOpenVRLoaded())
    {
        //Get compositor timing from OpenVR
        vr::Compositor_FrameTiming frame_timing;
        frame_timing.m_nSize = sizeof(vr::Compositor_FrameTiming);
        bool frame_timing_valid = vr::VRCompositor()->GetFrameTiming(&frame_timing, 0);

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Statistics");

        ImGui::Columns(2, "ColumnPerformanceStats", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::Text("VR Compositor CPU Frame Time: ");
        ImGui::NextColumn();

        if (frame_timing_valid)
            ImGui::Text("%.2f ms", frame_timing.m_flCompositorRenderCpuMs);
        else
            ImGui::Text("?");


        ImGui::NextColumn();
        ImGui::Text("VR Compositor GPU Frame Time: ");
        ImGui::NextColumn();

        if (frame_timing_valid)
            ImGui::Text("%.2f ms", frame_timing.m_flCompositorRenderGpuMs);
        else
            ImGui::Text("?");

        ImGui::NextColumn();
        ImGui::Text("Desktop Duplication Update Rate: ");

        ImGui::NextColumn();

        ImGui::Text("%d fps", ConfigManager::Get().GetConfigInt(configid_int_state_performance_duplication_fps));
        ImGui::NextColumn();

        ImGui::Text("Cross-GPU Copy Active: ");
        ImGui::NextColumn();

        ImGui::Text((ConfigManager::Get().GetConfigBool(configid_bool_state_performance_gpu_copy_active)) ? "Yes" : "No");
        ImGui::NextColumn();
    }

    ImGui::EndChild();
}

void WindowSettings::UpdateCatMisc()
{
    ImGui::Text("Misc");
            
    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewMiscSettings");

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Version Info
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Version Info");

        ImGui::Columns(2, "ColumnVersionInfo", false);
        ImGui::SetColumnWidth(0, column_width_0 * 2.0f);

        ImGui::Text("Desktop+ Version 2.8.5");

        ImGui::Columns(1);
    }

    //Warnings
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Warnings");

        ImGui::Columns(2, "ColumnResetWarnings", false);
        ImGui::SetColumnWidth(0, column_width_0);

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

        ImGui::Text("Warnings/Notifications Hidden: %i", warning_hidden_count);

        if (ImGui::Button("Reset Hidden Warnings"))
        {
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_compositor_quality_hidden, false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_compositor_res_hidden,     false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_process_elevation_hidden,  false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_elevated_mode_hidden,      false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_welcome_hidden,            false);
        }

        ImGui::Columns(1);
    }

    //Steam
    bool& no_steam = ConfigManager::Get().GetConfigBoolRef(configid_bool_misc_no_steam);
    if (ConfigManager::Get().IsSteamInstall())
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Steam");

        ImGui::Columns(2, "ColumnMiscSteam", false);
        ImGui::SetColumnWidth(0, column_width_0);

        if (ImGui::Checkbox("Disable Steam Integration", &no_steam))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_misc_no_steam), no_steam);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Restarts Desktop+ without Steam when it was launched by it.\nThis disables the permanent in-app status, usage time statistics and other Steam features.");

        ImGui::Columns(1);
    }

    //Troubleshooting
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Troubleshooting");

        ImGui::Columns(2, "ColumnTroubleshooting", false);
        ImGui::SetColumnWidth(0, column_width_0);

        //All the restart buttons only start up new processes, but both UI and dashboard app get rid of the older instance when starting

        ImGui::Text("Desktop+");
        ImGui::NextColumn();

        if (ImGui::Button("Restart"))
        {
            UIManager::Get()->RestartDashboardApp();
        }

        if ( (ConfigManager::Get().IsSteamInstall()) && (!ConfigManager::Get().GetConfigBool(configid_bool_state_misc_process_started_by_steam)) )
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

            if (no_steam)
                ImGui::PushItemDisabled();

            if (ImGui::Button("Restart with Steam"))
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

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

            if (!ConfigManager::Get().GetConfigBool(configid_bool_state_misc_elevated_mode_active))
            {
                if (ImGui::Button("Enter Elevated Mode"))
                {
                    UIManager::Get()->ElevatedModeEnter();
                }
            }
            else
            {
                if (ImGui::Button("Leave Elevated Mode"))
                {
                    UIManager::Get()->ElevatedModeLeave();
                }
            }

            if (!dashboard_app_running)
                ImGui::PopItemDisabled();
        }

        ImGui::NextColumn();

        ImGui::Text("Desktop+ UI");
        ImGui::NextColumn();

        if (ImGui::Button("Restart##UI"))
        {
            UIManager::Get()->Restart(false);
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (ImGui::Button("Restart in Desktop Mode"))
        {
            UIManager::Get()->Restart(true);
        }

        ImGui::Columns(1);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

        if (ImGui::Button("Restore Default Settings"))
        {
            ImGui::OpenPopup("SettingsResetPopup");
        }

        PopupSettingsReset();
    }

    ImGui::EndChild();
}

void WindowSettings::PushInterfaceScale()
{
    if ( (ConfigManager::Get().GetConfigBool(configid_bool_interface_large_style)) && (!UIManager::Get()->IsInDesktopMode()) )
    {
        ImGui::PushFont(UIManager::Get()->GetFontLarge());

        //Backup original style so it can be restored on pop
        m_StyleOrig = ImGui::GetStyle();
        ImGui::GetStyle().ScaleAllSizes(1.5f);

        m_IsStyleScaled = true; //configid_bool_interface_large_style may change between push and pop calls
    }
}

void WindowSettings::PopInterfaceScale()
{
    if (m_IsStyleScaled)
    {
        ImGui::PopFont();

        //Restore original style
        float window_bg_alpha = ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w;  //Preserve changes to window bg alpha made by UIManager between the interface scale calls

        ImGui::GetStyle() = m_StyleOrig;

        ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = window_bg_alpha;

        m_IsStyleScaled = false;
    }
}

bool WindowSettings::ButtonKeybind(unsigned char* key_code, bool no_mouse)
{
    //ID hierarchy prevents properly opening the popups directly from within the button popup, so this is a workaround
    static bool open_bind_popup = false, open_list_popup = false;
 
    ImGui::PushID(key_code);

    ImGui::PushID("KeycodeBindButton");
    if (ImGui::Button(GetStringForKeyCode(*key_code)))
    {
        ImGui::PopID();

        if (UIManager::Get()->IsInDesktopMode())
        {
            ImGui::OpenPopup("KeycodeButtonPopup");
        }
        else
        {
            open_list_popup = true;
        }
    }
    else
    {
        ImGui::PopID();
    }

    if (ImGui::BeginPopup("KeycodeButtonPopup"))
    {
        if (ImGui::Selectable("Set from Input..."))
        {
            open_bind_popup = true;
        }

        if (ImGui::Selectable("Set from List..."))
        {
            open_list_popup = true;
        }

        ImGui::EndPopup();
    }
        
    if (open_bind_popup)
    {
        ImGui::OpenPopup("Bind Key");
        open_bind_popup = false;
    }

    if (open_list_popup)
    {
        ImGui::OpenPopup("Select Key");
        open_list_popup = false;
    }

    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Bind Key", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::Text((no_mouse) ? "Press any key..." : "Press any key or mouse button...");

        ImGuiIO& io = ImGui::GetIO();

        if (!no_mouse)
        {
            for (int i = 0; i < 5; ++i)
            {
                if (ImGui::IsMouseClicked(i, false)) //Checking io.MouseClicked would do the same, but let's use the thing that is not marked [Internal] here
                {
                    switch (i) //Virtual key code for mouse is unfortunately not just VK_LBUTTON + i
                    {
                        case 0: *key_code = VK_LBUTTON;  break;
                        case 1: *key_code = VK_RBUTTON;  break;
                        case 2: *key_code = VK_MBUTTON;  break;
                        case 3: *key_code = VK_XBUTTON1; break;
                        case 4: *key_code = VK_XBUTTON2; break;
                    }
                    ImGui::CloseCurrentPopup();
                    break;
                }
            }
        }

        for (int i = 0; i < IM_ARRAYSIZE(io.KeysDown); ++i)
        {
            if ( (io.KeysDown[i]) && (io.KeysDownDuration[i] == 0.0f) )
            {
                *key_code = i;
                ImGui::CloseCurrentPopup();
                break;
            }
        }

        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Select Key", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::SetWindowSize(ImVec2(GetSize().x * 0.5f, GetSize().y * 0.75f));

        static ImGuiTextFilter filter;
        static int list_id = 0;

        if (ImGui::IsWindowAppearing())
        {
            for (int i = 0; i < 256; i++)
            {
                //Not the smartest, but most straight forward way
                if (GetKeyCodeForListID(i) == *key_code)
                {
                    list_id = i;
                    break;
                }
            }
        }

        ImGui::Text("Select Key Code");

        ImGui::SetNextItemWidth(-1.0f);

        if (ImGui::InputTextWithHint("", "Filter List", filter.InputBuf, IM_ARRAYSIZE(filter.InputBuf)))
        {
            filter.Build();
        }

        ImGui::BeginChild("KeyList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        unsigned char list_keycode;
        for (int i = 0; i < 256; i++)
        {
            list_keycode = GetKeyCodeForListID(i);
            if (filter.PassFilter( GetStringForKeyCode(list_keycode) ))
            {
                if ( (no_mouse) && (list_keycode >= VK_LBUTTON) && (list_keycode <= VK_XBUTTON2) && (list_keycode != VK_CANCEL) )    //Skip mouse buttons if turned off
                    continue;

                if (ImGui::Selectable( GetStringForKeyCode(list_keycode), (i == list_id)))
                {
                    list_id = i;
                }
            }
        }

        ImGui::EndChild();

        if (ImGui::Button("Ok")) 
        {
            *key_code = GetKeyCodeForListID(list_id);
            ImGui::CloseCurrentPopup();
        }
            
        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopID();

    return false;
}

bool WindowSettings::ButtonAction(const char* str_id, ActionID& action_id)
{
    bool result = false;

    ImGui::PushID(str_id);

    if (ImGui::Button(ActionManager::Get().GetActionName(action_id)))
    {
        ImGui::OpenPopup("Select Action");
    }

    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Select Action", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::SetWindowSize(ImVec2(GetSize().x * 0.5f, GetSize().y * 0.75f));

        static ActionID list_id = action_none;

        if (ImGui::IsWindowAppearing())
        {
            list_id = action_id;
            UIManager::Get()->RepeatFrame();
        }

        ImGui::Text("Select Action");

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::BeginChild("ActionList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), true);

        //List default actions
        for (int i = 0; i < action_built_in_MAX; ++i)
        {
            if (ImGui::Selectable(ActionManager::Get().GetActionName((ActionID)i), (i == list_id)))
            {
                list_id = (ActionID)i;
            }
        }

        //List custom actions
        int act_index = 0;
        for (CustomAction& action : ConfigManager::Get().GetCustomActions())
        {
            ImGui::PushID(&action);
            if (ImGui::Selectable(action.Name.c_str(), (act_index + action_custom == list_id)))
            {
                list_id = (ActionID)(act_index + action_custom);
            }
            ImGui::PopID();

            act_index++;
        }

        ImGui::EndChild();

        if (ImGui::Button("Ok")) 
        {
            action_id = list_id;
            UIManager::Get()->RepeatFrame();
            ImGui::CloseCurrentPopup();
            result = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopID();

    return result;
}

bool WindowSettings::ButtonHotkey(unsigned int hotkey_id)
{
    static std::string hotkey_name[3];

    hotkey_id = std::min(hotkey_id, 2u);

    unsigned int  flags   = 0;
    unsigned char keycode = 0;

    switch (hotkey_id)
    {
        case 0:
        {
            flags   = (unsigned int) ConfigManager::Get().GetConfigInt(configid_int_input_hotkey01_modifiers);
            keycode = (unsigned char)ConfigManager::Get().GetConfigInt(configid_int_input_hotkey01_keycode);
            break;
        }
        case 1:
        {
            flags   = (unsigned int) ConfigManager::Get().GetConfigInt(configid_int_input_hotkey02_modifiers);
            keycode = (unsigned char)ConfigManager::Get().GetConfigInt(configid_int_input_hotkey02_keycode);
            break;
        }
        case 2:
        {
            flags   = (unsigned int) ConfigManager::Get().GetConfigInt(configid_int_input_hotkey03_modifiers);
            keycode = (unsigned char)ConfigManager::Get().GetConfigInt(configid_int_input_hotkey03_keycode);
            break;
        }
    }

    //Update cached hotkey name if window is just appearing or the name is empty
    if ( (ImGui::IsWindowAppearing()) || (hotkey_name[hotkey_id].empty()) )
    {
        hotkey_name[hotkey_id] = "";

        if (keycode != 0)
        {
            if (flags & MOD_CONTROL)
                hotkey_name[hotkey_id] += "Ctrl+";
            if (flags & MOD_ALT)
                hotkey_name[hotkey_id] += "Alt+";
            if (flags & MOD_SHIFT)
                hotkey_name[hotkey_id] += "Shift+";
            if (flags & MOD_WIN)
                hotkey_name[hotkey_id] += "Win+";
        }

        hotkey_name[hotkey_id] += GetStringForKeyCode(keycode);
    }


    bool result = false;

    ImGui::PushID(hotkey_id);

    if (ImGui::Button(hotkey_name[hotkey_id].c_str()))
    {
        ImGui::OpenPopup("HotkeyEditPopup");
    }

    float scale_mul = (m_IsStyleScaled) ? 1.25f : 1.0f;
    ImGui::SetNextWindowSizeConstraints(ImVec2(GetSize().x * 0.5f * scale_mul, -1),  ImVec2(GetSize().x * 0.5f * scale_mul, -1));
    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("HotkeyEditPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        static bool mod_ctrl  = false;
        static bool mod_alt   = false;
        static bool mod_shift = false;
        static bool mod_win   = false;
        static unsigned char keycode_edit = 0;

        if (ImGui::IsWindowAppearing())
        {
            mod_ctrl  = (flags & MOD_CONTROL);
            mod_alt   = (flags & MOD_ALT);
            mod_shift = (flags & MOD_SHIFT);
            mod_win   = (flags & MOD_WIN);
            keycode_edit = keycode;
        }

        bool do_save = false;

        const float column_width_0 = ImGui::GetFontSize() * 10.0f;

        ImGui::Columns(2, "ColumnHotkey", false);
        ImGui::SetColumnWidth(0, column_width_0);


        ImGui::AlignTextToFramePadding();
        ImGui::Text("Modifiers");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1.0f);

        ImGui::Checkbox("Ctrl",  &mod_ctrl);
        ImGui::SameLine();
        ImGui::Checkbox("Alt",   &mod_alt);
        ImGui::SameLine();
        ImGui::Checkbox("Shift", &mod_shift);
        ImGui::SameLine();
        ImGui::Checkbox("Win",   &mod_win);

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Key Code");
        ImGui::NextColumn();

        ButtonKeybind(&keycode_edit, true);
        ImGui::NextColumn();

        ImGui::Columns(1);

        ImGui::Separator();

        if (ImGui::Button("Ok"))
        {
            do_save = true;
        }

        if (do_save)
        {
            flags = 0;

            if (mod_ctrl)
                flags |= MOD_CONTROL;
            if (mod_alt)
                flags |= MOD_ALT;
            if (mod_shift)
                flags |= MOD_SHIFT;
            if (mod_win)
                flags |= MOD_WIN;

            //Set cached hotkey name to blank so it'll get updated next frame
            hotkey_name[hotkey_id] = "";

            //Store hotkey modifier and keycode and send it over to the dashboard app
            switch (hotkey_id)
            {
                case 0: 
                {
                    ConfigManager::Get().SetConfigInt(configid_int_input_hotkey01_modifiers, (int)flags);
                    ConfigManager::Get().SetConfigInt(configid_int_input_hotkey01_keycode,   keycode_edit);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_hotkey01_modifiers), (int)flags);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_hotkey01_keycode),   keycode_edit);
                    break;
                }
                case 1: 
                {
                    ConfigManager::Get().SetConfigInt(configid_int_input_hotkey02_modifiers, (int)flags);
                    ConfigManager::Get().SetConfigInt(configid_int_input_hotkey02_keycode, keycode_edit);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_hotkey02_modifiers), (int)flags);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_hotkey02_keycode),   keycode_edit);
                    break;
                }
                case 2: 
                {
                    ConfigManager::Get().SetConfigInt(configid_int_input_hotkey03_modifiers, (int)flags);
                    ConfigManager::Get().SetConfigInt(configid_int_input_hotkey03_keycode, keycode_edit);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_hotkey03_modifiers), (int)flags);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_hotkey03_keycode),   keycode_edit);
                    break;
                }
            }

            UIManager::Get()->RepeatFrame();
            ImGui::CloseCurrentPopup();

            result = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopID();

    return result;
}

void WindowSettings::ProfileSelector(bool multi_overlay)
{
    static std::vector<std::string> single_overlay_profile_list;
    static int single_overlay_profile_selected_id = 0;
    static bool single_overwrite_confirm_state = false;
    static bool single_delete_confirm_state = false;

    static std::vector<std::string> multi_overlay_profile_list;
    static int multi_overlay_profile_selected_id = 0;
    static bool multi_overwrite_confirm_state = false;
    static bool multi_delete_confirm_state = false;

    //Reset confirm states when the other selector isn't visible
    if (multi_overlay)
    {
        single_overwrite_confirm_state = false;
        single_delete_confirm_state = false;
    }
    else
    {
        multi_overwrite_confirm_state = false;
        multi_delete_confirm_state = false;
    }

    //May look a bit convoluted, but set up conditional references for both selector types
    std::vector<std::string>& overlay_profile_list = (multi_overlay) ? multi_overlay_profile_list : single_overlay_profile_list;
    int& overlay_profile_selected_id = (multi_overlay) ? multi_overlay_profile_selected_id : single_overlay_profile_selected_id;
    bool& overwrite_confirm_state = (multi_overlay) ? multi_overwrite_confirm_state : single_overwrite_confirm_state;
    bool& delete_confirm_state = (multi_overlay) ? multi_delete_confirm_state : single_delete_confirm_state;

    if ( (ImGui::IsWindowAppearing()) || (overlay_profile_list.empty()) )
    {
        overlay_profile_list = ConfigManager::Get().GetOverlayProfileList(multi_overlay);
        overwrite_confirm_state = false;
        delete_confirm_state = false;
    }

    ImGui::PushID(multi_overlay);
    ImGui::SetNextItemWidth(-1);
    int index = 0;

    if (ImGui::BeginCombo("##OverlayProfileCombo", overlay_profile_list[overlay_profile_selected_id].c_str()))
    {
        for (const auto& str : overlay_profile_list)
        {
            if (ImGui::Selectable(str.c_str(), (index == overlay_profile_selected_id)))
            {
                overlay_profile_selected_id = index;

                overwrite_confirm_state = false;
                delete_confirm_state = false;
            }

            index++;
        }
        ImGui::EndCombo();
    }

    ImGui::NextColumn();
    ImGui::NextColumn();

    const bool is_first = (overlay_profile_selected_id == 0);
    const bool is_last  = (overlay_profile_selected_id == overlay_profile_list.size() - 1);

    if (is_last)
        ImGui::PushItemDisabled();

    if (ImGui::Button("Load"))
    {
        if (overlay_profile_selected_id == 0)
        {
            ConfigManager::Get().LoadOverlayProfileDefault(multi_overlay);
        }
        else
        {
            if (multi_overlay)
            {
                ConfigManager::Get().LoadMultiOverlayProfileFromFile(overlay_profile_list[overlay_profile_selected_id] + ".ini");
            }
            else
            {
                ConfigManager::Get().LoadOverlayProfileFromFile(overlay_profile_list[overlay_profile_selected_id] + ".ini");
            }
        }

        //Adjust current overlay ID for UI since this may have made the old selection invalid
        int& current_overlay = ConfigManager::Get().GetConfigIntRef(configid_int_interface_overlay_current_id);
        current_overlay = clamp(current_overlay, 0, (int)OverlayManager::Get().GetOverlayCount() - 1);

        //Tell dashboard app to load the profile as well
        IPCManager::Get().SendStringToDashboardApp(configid_str_state_profile_name_load, overlay_profile_list[overlay_profile_selected_id], UIManager::Get()->GetWindowHandle());
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_profile_load, (multi_overlay) ? ipcactv_ovrl_profile_multi : ipcactv_ovrl_profile_single);

        UIManager::Get()->RepeatFrame();

        overwrite_confirm_state = false;
        delete_confirm_state = false;

        m_OverlayNameBufferNeedsUpdate = true;
    }

    if (multi_overlay)
    {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (is_first)
            ImGui::PushItemDisabled();

        if (ImGui::Button("Add"))
        {
            if (overlay_profile_selected_id == 0)
            {
                ConfigManager::Get().LoadOverlayProfileDefault();
            }
            else
            {
                ConfigManager::Get().LoadMultiOverlayProfileFromFile(overlay_profile_list[overlay_profile_selected_id] + ".ini", false);
            }

            //Tell dashboard app to load the profile as well
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_profile_name_load, overlay_profile_list[overlay_profile_selected_id], UIManager::Get()->GetWindowHandle());
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_profile_load, ipcactv_ovrl_profile_multi_add);

            UIManager::Get()->RepeatFrame();

            overwrite_confirm_state = false;
            delete_confirm_state = false;
        }

        if (is_first)
            ImGui::PopItemDisabled();
    }

    if (is_last)
        ImGui::PopItemDisabled();

    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

    if (is_first)
        ImGui::PushItemDisabled();

    if (overwrite_confirm_state)
    {
        if (ImGui::Button("Overwrite?"))
        {
            if (multi_overlay)
            {
                ConfigManager::Get().SaveMultiOverlayProfileToFile(overlay_profile_list[overlay_profile_selected_id] + ".ini");
            }
            else
            {
                ConfigManager::Get().SaveOverlayProfileToFile(overlay_profile_list[overlay_profile_selected_id] + ".ini");
            }

            overwrite_confirm_state = false;
            delete_confirm_state = false;
        }
    }
    else
    {
        if (ImGui::Button("Save"))
        {
            if (!is_last)
            {
                overwrite_confirm_state = true;
            }
            else
            {
                ImGui::OpenPopup("NewOverlayProfilePopup");
            }

            delete_confirm_state = false;
        }
    }

    if (is_first)
        ImGui::PopItemDisabled();

    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

    if ( (is_first) || (is_last) )
        ImGui::PushItemDisabled();

    if (delete_confirm_state)
    {
        if (ImGui::Button("Really?"))
        {
            if (ConfigManager::Get().DeleteOverlayProfile(overlay_profile_list[overlay_profile_selected_id] + ".ini", multi_overlay))
            {
                overlay_profile_list = ConfigManager::Get().GetOverlayProfileList(multi_overlay);
                overlay_profile_selected_id = 0;
            }

            delete_confirm_state = false;
        }
    }
    else
    {
        if (ImGui::Button("Delete"))
        {
            delete_confirm_state = true;
            overwrite_confirm_state = false;
        }
    }

    ImGui::NextColumn();

    if ( (is_first) || (is_last) )
        ImGui::PopItemDisabled();

    PopupNewOverlayProfile(overlay_profile_list, overlay_profile_selected_id, multi_overlay);

    ImGui::PopID();
}

void WindowSettings::UpdateWindowList(HWND selected_window, std::string& selected_window_str)
{
    m_CaptureWindowList = WindowInfo::CreateCapturableWindowList();

    //Check if any of the titles contain unmapped characters and add them to the font builder list if they do
    //Also update string of selected window in case it changed
    for (WindowInfo& window : m_CaptureWindowList)
    {
        if (ImGui::StringContainsUnmappedCharacter(window.ListTitle.c_str()))
        {
            if (TextureManager::Get().AddFontBuilderString(window.ListTitle.c_str()))
            {
                TextureManager::Get().ReloadAllTexturesLater();
            }
        }

        if (window.WindowHandle == selected_window)
        {
            selected_window_str = window.ListTitle;

            //Update last window title and auto name on current overlay
            ConfigManager::Get().SetConfigString(configid_str_overlay_winrt_last_window_title, StringConvertFromUTF16(window.Title.c_str()));
            OverlayManager::Get().SetCurrentOverlayNameAuto();
            m_OverlayNameBufferNeedsUpdate = true;
        }
    }
}

void WindowSettings::ActionOrderSetting(unsigned int overlay_id)
{
    static int list_selected_pos = -1;
    bool use_global_order = false;

    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Action Buttons");

    if (overlay_id != UINT_MAX)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

        if (ImGui::Checkbox("Use Global Setting", &ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_actionbar_order_use_global)))
        {
            UIManager::Get()->RepeatFrame();
        }

        use_global_order = ConfigManager::Get().GetConfigBool(configid_bool_overlay_actionbar_order_use_global);
    }

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

    float arrows_width       = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    float column_0_width     = ImGui::GetContentRegionAvail().x - arrows_width;
    float viewbuttons_height = (ImGui::GetFrameHeightWithSpacing() * 6.0f) + ImGui::GetFrameHeight() + (ImGui::GetStyle().WindowPadding.y * 2.0f);

    ImGui::Columns(2, "ColumnActionButtons", false);
    ImGui::SetColumnWidth(0, column_0_width);
    ImGui::SetColumnWidth(1, arrows_width);

    if (use_global_order)
    {
        list_selected_pos = -1;
        ImGui::PushItemDisabled();
    }

    //ActionButton list
    ImGui::BeginChild("ViewActionButtons", ImVec2(0.0f, viewbuttons_height), true);

    auto& actions = ConfigManager::Get().GetCustomActions();
    auto& action_order = (overlay_id == UINT_MAX) ? ConfigManager::Get().GetActionMainBarOrder() : OverlayManager::Get().GetConfigData(overlay_id).ConfigActionBarOrder;
    int list_id = 0;
    for (auto& order_data : action_order)
    {
        ActionButtonRow((ActionID)order_data.action_id, list_id, list_selected_pos, overlay_id);

        //Drag reordering
        if ( (ImGui::IsItemActive()) && (!ImGui::IsItemHovered()) && (fabs(ImGui::GetMouseDragDelta(0).y) > ImGui::GetFrameHeight() / 2.0f) )
        {
            int list_id_swap = list_id + ((ImGui::GetMouseDragDelta(0).y < 0.0f) ? -1 : 1);
            if ( (list_id_swap >= 0) && (list_id_swap < action_order.size()) )
            {
                std::iter_swap(action_order.begin() + list_id, action_order.begin() + list_id_swap);
                list_selected_pos = list_id_swap;
                ImGui::ResetMouseDragDelta();
            }
        }

        list_id++;
    }

    ImGui::EndChild();

    //Reduce horizontal spacing a bit so the arrows are closer to the list
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x / 3.0f, ImGui::GetStyle().ItemSpacing.y});

    ImGui::NextColumn();


    //This is a bit of a mess, but centers the buttons vertically, yeah.
    ImGui::Dummy(ImVec2(0.0f, (viewbuttons_height / 2.0f) - ( (ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight()) / 2.0f ) - ImGui::GetStyle().ItemSpacing.y));
            
    int list_selected_pos_pre = list_selected_pos;

    //Up
    if (list_selected_pos_pre <= 0)
        ImGui::PushItemDisabled();

    if (ImGui::ArrowButton("MoveUp", ImGuiDir_Up))
    {
        std::iter_swap(action_order.begin() + list_selected_pos, action_order.begin() + list_selected_pos - 1);
        list_selected_pos--;
    }

    if (list_selected_pos_pre <= 0)
        ImGui::PopItemDisabled();

    //Down
    if ( (list_selected_pos_pre < 0) || (list_selected_pos_pre + 1 == action_order.size()) )
        ImGui::PushItemDisabled();

    if (ImGui::ArrowButton("MoveDown", ImGuiDir_Down))
    {
        std::iter_swap(action_order.begin() + list_selected_pos, action_order.begin() + list_selected_pos + 1);
        list_selected_pos++;
    }

    if ( (list_selected_pos_pre < 0) || (list_selected_pos_pre + 1 == action_order.size()) )
        ImGui::PopItemDisabled();

    ImGui::PopStyleVar();

    ImGui::Columns(1);

    if (use_global_order)
        ImGui::PopItemDisabled();
}

void WindowSettings::UpdateLimiterSetting(float column_width_0, bool is_override)
{
    const ConfigID_Int configid_mode = (is_override) ? configid_int_overlay_update_limit_override_mode : configid_int_performance_update_limit_mode;
    const ConfigID_Int configid_fps  = (is_override) ? configid_int_overlay_update_limit_override_fps  : configid_int_performance_update_limit_fps;
    const ConfigID_Float configid_ms = (is_override) ? configid_float_overlay_update_limit_override_ms : configid_float_performance_update_limit_ms;


    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), (is_override) ? "Update Limiter Override" : "Update Limiter");

    ImGui::Columns(2, "ColumnUpdateLimiter", false);

    if (is_override)
    {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("When multiple overrides are active, the one resulting in the highest update rate is used");
    }

    ImGui::SetColumnWidth(0, column_width_0);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Limiter Mode");
    ImGui::NextColumn();

    ImGui::SetNextItemWidth(-1);
    const char* items[] = { "Off", "Frame Time", "Frame Rate" };
    const char* items_override[] = { "Use Global Setting", "Frame Time", "Frame Rate" };
    int mode_limit = clamp(ConfigManager::Get().GetConfigIntRef(configid_mode), 0, IM_ARRAYSIZE(items) - 1);
    if (ImGui::Combo("##ComboLimitMode", &mode_limit, (is_override) ? items_override : items, IM_ARRAYSIZE(items)))
    {
        ConfigManager::Get().SetConfigInt(configid_mode, mode_limit);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_mode), mode_limit);
    }

    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Limit");

    if (mode_limit == update_limit_mode_fps)
    {
        ImGui::NextColumn();

        const char* fps_enum_names[] = { "1 fps", "2 fps", "5 fps", "10 fps", "15 fps", "20 fps", "25 fps", "30 fps", "40 fps", "50 fps" };

        int& update_limit_fps = ConfigManager::Get().GetConfigIntRef(configid_fps);
        const char* update_limit_fps_display = (update_limit_fps >= 0 && update_limit_fps < IM_ARRAYSIZE(fps_enum_names)) ? fps_enum_names[update_limit_fps] : "?";

        if (ImGui::SliderWithButtonsInt("UpdateLimitFPS", update_limit_fps, 1, 1, 0, IM_ARRAYSIZE(fps_enum_names) - 1, update_limit_fps_display, ImGuiSliderFlags_NoInput))
        {
            update_limit_fps = clamp(update_limit_fps, 0, IM_ARRAYSIZE(fps_enum_names) - 1);

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_fps), update_limit_fps);
        }
    }
    else //This still shows when off, but as disabled
    {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Minimum time between overlay updates");
        ImGui::NextColumn();

        if (mode_limit == update_limit_mode_off)
            ImGui::PushItemDisabled();

        float& update_limit_ms = ConfigManager::Get().GetConfigFloatRef(configid_ms);

        if (ImGui::SliderWithButtonsFloat("UpdateLimitMS", update_limit_ms, 0.5f, 0.05f, 0.0f, 100.0f, "%.2f ms", ImGuiSliderFlags_Logarithmic))
        {
            if (update_limit_ms < 0.0f)
                update_limit_ms = 0.0f;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_ms), *(LPARAM*)&update_limit_ms);
        }

        if (mode_limit == update_limit_mode_off)
            ImGui::PopItemDisabled();
    }

    ImGui::Columns(1);
}

bool WindowSettings::ActionButtonRow(ActionID action_id, int list_pos, int& list_selected_pos, unsigned int overlay_id)
{
    auto& action_order = (overlay_id == UINT_MAX) ? ConfigManager::Get().GetActionMainBarOrder() : OverlayManager::Get().GetConfigData(overlay_id).ConfigActionBarOrder;
    bool delete_pressed = false;

    static float column_width_1 = 0.0f;
    const float column_width_0 = ImGui::GetStyle().ItemSpacing.x + ImGui::GetContentRegionAvail().x - column_width_1;

    ImGui::PushID(action_id);
    ImGui::PushID(ActionManager::Get().GetActionName(action_id));

    ImGui::Columns(2, "ColumnActionRow", false);

    ImGui::SetColumnWidth(0, column_width_0);
    ImGui::SetColumnWidth(1, column_width_1);

    column_width_1 = ImGui::GetStyle().ItemInnerSpacing.x;

    ImGui::AlignTextToFramePadding();
    if (ImGui::Checkbox("##VisibleCheckbox", &action_order[list_pos].visible))
    {
        UIManager::Get()->RepeatFrame();
    }
    ImGui::SameLine();

    if (ImGui::Selectable(ActionManager::Get().GetActionName(action_id), (list_selected_pos == list_pos)))
    {
        list_selected_pos = list_pos;
    }

    ImGui::PopID();
    ImGui::PopID();

    ImGui::Columns(1);

    return delete_pressed;
}

void WindowSettings::PopupQuickStartGuide()
{
    static int current_page = 0;

    //Set popup rounding to the same as a normal window
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ImGui::GetStyle().WindowRounding);

    //Use larger window with large interface style
    float size_mul = (ConfigManager::Get().GetConfigBool(configid_bool_interface_large_style)) ? 1.25f : 1.0f;

    ImGui::SetWindowSize(ImVec2(GetSize().x * 0.5f * size_mul, -1));
    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopup("QuickStartGuidePopup", ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::BeginChild("ChildPageContent", ImVec2(GetSize().x * 0.5f * size_mul, ImGui::GetIO().DisplaySize.y * 0.5f * size_mul));

        //This would've used highlighting and such if it didn't break with wrapped text... oh well.
        switch (current_page)
        {
            case 0:
            {
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Welcome to Desktop+!");

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f) );

                ImGui::TextWrapped("This short guide will introduce you to the very basics of the application.\n"
                                   "For more detailed information see the ReadMe and User Guide.");

                break;

            }
            case 1:
            {
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Overlays");

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f) );

                ImGui::TextWrapped("Desktop+ allows you to create overlays mirroring your desktops or individual windows.\n"
                                   "Each created overlay can be customized in the Overlays settings page.\n"
                                   "\n"
                                   "The first overlay is special. It's only visible in and fixed to the Desktop+ dashboard tab.\n"
                                   "If you wish to bring an desktop into the game world for example, you need to add another overlay.\n");

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Additional overlays can be created by clicking on ");
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::Button("+");
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::Text(".");

                ImGui::AlignTextToFramePadding();
                ImGui::Text("You also can manage and remove them again by clicking on ");
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::Button("Manage");
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::Text(".\n\n");

                ImGui::TextWrapped("Individual overlays or complete layouts can be saved to profiles. Desktop+ comes with a few sample profiles you can check out.\n"
                                   "The current overlay setup will automatically be remembered between sessions.");
                break;
            }
            case 2:
            {
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Actions");

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f) );

                ImGui::TextWrapped("Actions in Desktop+ are functions which can be bound to controller inputs, added to the Action Bar at the bottom as buttons and more.\n\n"
                                   "There are built-in and user-defined custom actions. Custom actions can do things like pressing keyboard shortcuts, text input, execute applications, and control overlay state.\n"
                                   "\n"
                                   "\"Open ReadMe\" and \"Middle/Back Mouse Button\" are examples of custom actions.\n"
                                   "You may change or even remove them entirely if you want to.");
                break;
            }
            case 3:
            {
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Further Reading");

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f) );

                ImGui::TextWrapped("This should already be enough to get you started with Desktop+. There are a lot of options, but don't be afraid to just try them out.\n"
                                   "If you run into any trouble or get stuck, make sure to check out the ReadMe as well.\n"
                                   "The button in the Action Bar will open it for you.\n"
                                   "\n"
                                   "Detailed explanations of each option and step-by-step guides for common usage scenarios can be found the in the User Guide linked in the ReadMe.\n"
                                   "\n"
                                   "To remove the welcome notification, click on it and choose \"Do not show this again\".");
                break;
            }
        }

        ImGui::PopStyleVar(); //ImGuiStyleVar_ItemSpacing

        ImGui::EndChild();

        ImGui::Separator();

        int current_page_prev = current_page;

        if (current_page_prev == 0)
            ImGui::PushItemDisabled();

        if (ImGui::Button("Previous Page")) 
        {
            current_page--;
            UIManager::Get()->RepeatFrame();
        }

        if (current_page_prev == 0)
            ImGui::PopItemDisabled();

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (current_page_prev == 3)
            ImGui::PushItemDisabled();

        if (ImGui::Button("Next Page")) 
        {
            current_page++;
            UIManager::Get()->RepeatFrame();
        }

        if (current_page_prev == 3)
            ImGui::PopItemDisabled();

        ImGui::SameLine();

        if (ImGui::Button("Close")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(); //ImGuiStyleVar_PopupRounding
}

bool WindowSettings::PopupCurrentOverlayManage()
{
    static bool popup_was_open = false;

    //Set popup rounding to the same as a normal window
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ImGui::GetStyle().WindowRounding);
    bool is_popup_rounding_pushed = true;

    //Center popup
    ImGui::SetNextWindowSize(ImVec2(GetSize().x * 0.5f, GetSize().y * 0.75f));
    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopup("CurrentOverlayManage", ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        popup_was_open = true;

        ImGui::PopStyleVar(); //ImGuiStyleVar_PopupRounding
        is_popup_rounding_pushed = false;

        //List of unique IDs for overlays so ImGui can identify the same list entries after reordering or list expansion (needed for drag reordering)
        static std::vector<int> list_unique_ids; 

        //Reset unique IDs when popup was opened
        if (ImGui::IsWindowAppearing())
        {
            list_unique_ids.clear();
        }

        //Expand unique id lists if overlays were added (also does initialization since it's empty then)
        while (list_unique_ids.size() < OverlayManager::Get().GetOverlayCount())
        {
            list_unique_ids.push_back((int)list_unique_ids.size());
        }

        if (UIManager::Get()->IsOpenVRLoaded())
        {
            ImGui::Text("Click on an overlay or choose one from the list");
        }
        else
        {
            ImGui::Text("Choose an overlay from the list");
        }

        int& current_overlay = ConfigManager::Get().GetConfigIntRef(configid_int_interface_overlay_current_id);

        float arrows_width       = ImGui::GetFrameHeightWithSpacing();
        float column_0_width     = ImGui::GetContentRegionAvail().x - arrows_width + ImGui::GetStyle().ItemSpacing.x;
        float viewbuttons_height = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();
        static float extra_buttons_width = 0.0f;

        ImGui::Columns(2, "ColumnOverlayList", false);
        ImGui::SetColumnWidth(0, column_0_width);
        ImGui::SetColumnWidth(1, arrows_width + ImGui::GetStyle().ItemSpacing.x);

        //Overlay list
        ImGui::BeginChild("ViewOverlayList", ImVec2(0.0f, viewbuttons_height), true);

        //List overlays
        const int overlay_count = (int)OverlayManager::Get().GetOverlayCount();
        int index_hovered = -1;
        ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
        ImVec2 img_size, img_uv_min, img_uv_max;
        int selectable_window_icon_id = -1;
        TMNGRTexID selectable_icon_texture_id = tmtex_icon_desktop;

        for (int index = 0; index < overlay_count; ++index)
        {
            const OverlayConfigData& data = OverlayManager::Get().GetConfigData(index);
            bool current_overlay_enabled = data.ConfigBool[configid_bool_overlay_enabled];

            ImGui::PushID(list_unique_ids[index]);

            if (ImGui::Selectable("", (index == current_overlay)))
            {
                current_overlay = index;
                m_OverlayNameBufferNeedsUpdate = true;

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_overlay_current_id), current_overlay);
                OverlayManager::Get().SetCurrentOverlayID(current_overlay);
                UIManager::Get()->RepeatFrame();
            }

            if (ImGui::IsItemHovered())
            {
                index_hovered = index;
            }
            else if ( (ImGui::IsItemActive()) && (!ImGui::IsItemHovered()) ) //Drag reordering
            {
                int index_swap = index + ((ImGui::GetMouseDragDelta(0).y < 0.0f) ? -1 : 1);
                if ( (index > 0) && (index_swap > 0) && (index_swap < overlay_count) )
                {
                    OverlayManager::Get().SwapOverlays(index, index_swap);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_swap, index_swap);
                    current_overlay = index_swap;
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_overlay_current_id), current_overlay);
                    OverlayManager::Get().SetCurrentOverlayID(current_overlay);

                    std::iter_swap(list_unique_ids.begin() + index, list_unique_ids.begin() + index_swap);

                    ImGui::ResetMouseDragDelta();
                }
            }

            ImGui::SameLine(0.0f, 0.0f);

            if (!current_overlay_enabled)
                ImGui::PushItemDisabled();

            ImGui::Text("#%d -", index);
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

            //Icon and text
            if (ImGui::IsRectVisible(img_size_line_height)) //Only lookup icon if it's gonna be visible
            {
                selectable_window_icon_id = GetOverlayIcon(index, selectable_icon_texture_id);

                if (selectable_window_icon_id != -1)
                    TextureManager::Get().GetWindowIconTextureInfo(selectable_window_icon_id, img_size, img_uv_min, img_uv_max);
                else
                    TextureManager::Get().GetTextureInfo(selectable_icon_texture_id, img_size, img_uv_min, img_uv_max);

                ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            }

            ImGui::Text(OverlayManager::Get().GetConfigData(index).ConfigNameStr.c_str());

            ImGui::PopID();

            if (!current_overlay_enabled)
                ImGui::PopItemDisabled();
        }

        ImGui::EndChild();

        //Check if any actual overlay is being hovered
        if (UIManager::Get()->IsOpenVRLoaded())
        {
            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().FindOverlayHandle(i);

                if ((ovrl_handle != vr::k_ulOverlayHandleInvalid) && (vr::VROverlay()->IsHoverTargetOverlay(ovrl_handle)))
                {
                    index_hovered = i;
                    break;
                }
            }
        }

        //Highlight hovered or active entry if nothing is hovered
        HighlightOverlay((index_hovered != -1) ? index_hovered : current_overlay);

        //Remove horizontal spacing so the arrows are closer to the list
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, ImGui::GetStyle().ItemSpacing.y});

        ImGui::NextColumn();


        //This is a bit of a mess, but centers the buttons vertically, yeah.
        ImGui::Dummy(ImVec2(0.0f, (viewbuttons_height / 2.0f) - ( (ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight()) / 2.0f ) - ImGui::GetStyle().ItemSpacing.y));
            
        int current_overlay_prev = current_overlay;

        //Up
        if (current_overlay_prev <= 1) //Don't allow moving dashboard overlay
            ImGui::PushItemDisabled();

        if (ImGui::ArrowButton("MoveUp", ImGuiDir_Up))
        {
            OverlayManager::Get().SwapOverlays(current_overlay, current_overlay - 1);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_swap, current_overlay - 1);
            std::iter_swap(list_unique_ids.begin() + current_overlay, list_unique_ids.begin() + current_overlay - 1);
            current_overlay--;
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_overlay_current_id), current_overlay);
            OverlayManager::Get().SetCurrentOverlayID(current_overlay);
        }

        if (current_overlay_prev <= 1)
            ImGui::PopItemDisabled();

        //Down
        if ( (current_overlay_prev < 1) || (current_overlay_prev + 1 == overlay_count) )
            ImGui::PushItemDisabled();

        if (ImGui::ArrowButton("MoveDown", ImGuiDir_Down))
        {
            OverlayManager::Get().SwapOverlays(current_overlay, current_overlay + 1);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_swap, current_overlay + 1);
            std::iter_swap(list_unique_ids.begin() + current_overlay, list_unique_ids.begin() + current_overlay + 1);
            current_overlay++;
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_overlay_current_id), current_overlay);
            OverlayManager::Get().SetCurrentOverlayID(current_overlay);
        }

        if ( (current_overlay_prev < 1) || (current_overlay_prev + 1 == overlay_count) )
            ImGui::PopItemDisabled();

        ImGui::PopStyleVar();

        ImGui::Columns(1);

        //Bottom buttons
        if (ImGui::Button("Done")) 
        {
            UIManager::Get()->RepeatFrame();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine(column_0_width - ImGui::GetItemRectSize().x - extra_buttons_width + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().ItemSpacing.y);
        ImGui::SetCursorPosX(column_0_width - extra_buttons_width - ImGui::GetStyle().ItemSpacing.x - ImGui::GetStyle().ItemInnerSpacing.x);

        if (ImGui::Button("Add")) 
        {
            DuplicateCurrentOverlay();
        }

        extra_buttons_width = ImGui::GetItemRectSize().x;

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (ImGui::Button("Rename"))
        {
            ImGui::OpenPopup("RenameOverlayPopup");
        }

        extra_buttons_width += ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemInnerSpacing.x;

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        bool current_overlay_is_dashboard = (current_overlay == k_ulOverlayID_Dashboard);

        if (current_overlay_is_dashboard)
            ImGui::PushItemDisabled();

        if (ImGui::Button("Remove")) 
        {
            OverlayManager::Get().RemoveOverlay(current_overlay);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_remove, current_overlay);

            current_overlay = (int)OverlayManager::Get().GetCurrentOverlayID(); //RemoveOverlay() will change the current ID
            //No need to sync current overlay here
        }

        if (current_overlay_is_dashboard)
            ImGui::PopItemDisabled();

        extra_buttons_width += ImGui::GetItemRectSize().x;

        PopupCurrentOverlayRename();

        ImGui::EndPopup();
    }

    //This has to be popped early to prevent affecting other popups but we can't forget about it when the popup is closed either
    if (is_popup_rounding_pushed)
    {
        ImGui::PopStyleVar(); //ImGuiStyleVar_PopupRounding
    }

    //Detect if the popup was closed, which can happen at any time from clicking outside of it
    if ((popup_was_open) && (!ImGui::IsPopupOpen("CurrentOverlayManage")))
    {
        popup_was_open = false;
        return true;
    }

    return false;
}

bool WindowSettings::PopupCurrentOverlayRename()
{
    bool ret = false;

    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("RenameOverlayPopup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::SetWindowSize(ImVec2(GetSize().x * 0.45f, -1.0f));

        static char buf_name[1024] = "";
        static int popup_framecount = 0;

        if (ImGui::IsWindowAppearing())
        {
            popup_framecount = ImGui::GetFrameCount();

            size_t copied_length = OverlayManager::Get().GetCurrentConfigData().ConfigNameStr.copy(buf_name, 1023);
            buf_name[copied_length] = '\0';
        }

        ImGui::Text("Enter new Overlay Name");

        bool do_save = false;
        bool buffer_changed = false;

        ImGui::SetNextItemWidth(-1.0f);
        //The idea is to have ImGui treat this as a new widget every time the popup is open, so the cursor position isn't remembered between popups
        ImGui::PushID(popup_framecount);
        if (ImGui::InputText("", buf_name, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            do_save = true;
        }
        ImGui::PopID();

        //Focus text input when the window is appearing
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::IsItemEdited())
        {
            buffer_changed = true;
        }

        if (ImGui::PopupContextMenuInputText(nullptr, buf_name, 1024))
        {
            buffer_changed = true;
        }

        if (buffer_changed)
        {
            if (ImGui::StringContainsUnmappedCharacter(buf_name))
            {
                if (TextureManager::Get().AddFontBuilderString(buf_name))
                {
                    TextureManager::Get().ReloadAllTexturesLater();
                }
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Ok")) 
        {
            do_save = true;
        }

        if (do_save)
        {
            OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

            //If name buffer is not empty, set name from user input, otherwise fall back to auto naming
            if (buf_name[0] != '\0')
            {
                data.ConfigBool[configid_bool_overlay_name_custom] = true;
                data.ConfigNameStr = buf_name;
            }
            else
            {
                data.ConfigBool[configid_bool_overlay_name_custom] = false;
                OverlayManager::Get().SetCurrentOverlayNameAuto();
            }

            m_OverlayNameBufferNeedsUpdate = true;
            ret = true;

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return ret;
}

void WindowSettings::PopupNewOverlayProfile(std::vector<std::string>& overlay_profile_list, int& overlay_profile_selected_id, bool multi_overlay)
{
    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("NewOverlayProfilePopup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::SetWindowSize(ImVec2(GetSize().x * 0.5f, -1.0f));

        static char buf_name[1024] = "";
        static bool is_name_taken = false;
        static bool is_name_invalid = false;
        static int list_id = 0;
        static int popup_framecount = 0;

        if (ImGui::IsWindowAppearing())
        {
            popup_framecount = ImGui::GetFrameCount();

            std::string default_name("Profile 1");
            int i = 1;

            //Default to higher profile number if normal is already taken
            while (std::find(overlay_profile_list.begin(), overlay_profile_list.end(), default_name) != overlay_profile_list.end())
            {
                ++i;
                default_name = "Profile " + std::to_string(i);
            }

            size_t copied_length = default_name.copy(buf_name, 1023);
            buf_name[copied_length] = '\0';
        }

        ImGui::Text("Enter new Profile Name");

        if (is_name_taken)
        {
            ImGui::SameLine(0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextError, "(a profile with this name already exists)");
        }

        bool do_save = false;
        bool buffer_changed = false;

        ImGui::SetNextItemWidth(-1.0f);
        //The idea is to have ImGui treat this as a new widget every time the popup is open, so the cursor position isn't remembered between popups
        ImGui::PushID(popup_framecount);
        if (ImGui::InputText("", buf_name, 1024, ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue,
                                                 [](ImGuiInputTextCallbackData* data)
                                                 {
                                                     //Filter forbidden characters, doesn't guarantee the name will work but should catch most cases
                                                     if ( (data->EventChar < 256) && (data->EventChar >= 32) && (strchr("\\/\"<>|*?", (char)data->EventChar)) )
                                                         return 1;
                                                     return 0;
                                                 }
                            ))
        {
            do_save = true;
        }
        ImGui::PopID();

        //Focus text input when the window is appearing
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::IsItemEdited())
        {
            buffer_changed = true;
        }

        if (ImGui::PopupContextMenuInputText(nullptr, buf_name, 1024))
        {
            //Filter forbidden characters from popup paste
            std::wstring wstr = WStringConvertFromUTF8(buf_name);

            wstr.erase(std::remove_if(wstr.begin(), wstr.end(), 
                                                    [](wchar_t& w_char)
                                                    {
                                                        return ( (w_char < 256) && (w_char >= 32) && (strchr("\\/\"<>|*?", (char)w_char)) );
                                                    }),
                                                    wstr.end());                               

            //Write back into buffer
            size_t copied_length = StringConvertFromUTF16(wstr.c_str()).copy(buf_name, 1023);
            buf_name[copied_length] = '\0';

            buffer_changed = true;
        }

        if (buffer_changed)
        {
            if (strnlen(buf_name, 1024) == 0)
            {
                is_name_invalid = true;
                is_name_taken = false;
            }
            else if (std::find(overlay_profile_list.begin(), overlay_profile_list.end(), std::string(buf_name) ) != overlay_profile_list.end())
            {
                is_name_invalid = true;
                is_name_taken = true;
            }
            else
            {
                is_name_invalid = false;
                is_name_taken = false;
            }
        }

        ImGui::Separator();

        if (is_name_invalid)
            ImGui::PushItemDisabled();

        if (ImGui::Button("Ok")) 
        {
            do_save = true;
        }

        if (do_save)
        {
            std::string name_str(buf_name);

            if (multi_overlay)
            {
                ConfigManager::Get().SaveMultiOverlayProfileToFile(name_str + ".ini");
            }
            else
            {
                ConfigManager::Get().SaveOverlayProfileToFile(name_str + ".ini"); //Yes, we don't check for success. A lot of other things would go wrong as well if this did so...
            }

            //Update list
            overlay_profile_list = ConfigManager::Get().GetOverlayProfileList(multi_overlay);

            //Find and select new profile in the list
            auto it = std::find(overlay_profile_list.begin(), overlay_profile_list.end(), name_str);

            if (it != overlay_profile_list.end())
            {
                overlay_profile_selected_id = (int)std::distance(overlay_profile_list.begin(), it);
            }

            ImGui::CloseCurrentPopup();
        }

        if (is_name_invalid)
            ImGui::PopItemDisabled();

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void WindowSettings::PopupActionEdit(CustomAction& action, int id)
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(GetSize().x * 0.5f, -1),  ImVec2(GetSize().x * 0.5f, -1));
    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("ActionEditPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        //Working with fixed sized char arrays for input fields makes this a bit simpler
        static char buf_name[1024] = "";
        static std::string str_icon_file;
        static bool use_action_icon = true;   //Icon to use for the preview button. Switches to tmtex_icon_temp when the icon was changed
        static int action_function  = caction_press_keys;
        static unsigned char keycodes[3] = {0};
        static bool bool_id = false;          //Loaded from and written to int_id when saving
        static int int_id   = 0;
        static char buf_type_str[1024] = "";
        static char buf_exe_path[1024] = "";
        static char buf_exe_arg[1024]  = "";

        if (ImGui::IsWindowAppearing())
        {
            //Load data from action
            size_t copied_length = action.Name.copy(buf_name, 1023);
            buf_name[copied_length] = '\0';
            action_function = action.FunctionType;

            keycodes[0] = 0;
            keycodes[1] = 0;
            keycodes[2] = 0;
            bool_id = false;
            int_id  = 0;
            buf_type_str[0] = '\0';
            buf_exe_path[0] = '\0';
            buf_exe_arg[0]  = '\0';

            switch (action_function)
            {
                case caction_press_keys:
                {
                    keycodes[0] = action.KeyCodes[0];
                    keycodes[1] = action.KeyCodes[1];
                    keycodes[2] = action.KeyCodes[2];
                    bool_id     = (action.IntID == 1);
                    break;
                }
                case caction_type_string:
                {
                    copied_length = action.StrMain.copy(buf_type_str, 1023);
                    buf_type_str[copied_length] = '\0';
                    break;
                }
                case caction_launch_application:
                {
                    copied_length = action.StrMain.copy(buf_exe_path, 1023);
                    buf_exe_path[copied_length] = '\0';
                    copied_length = action.StrArg.copy(buf_exe_arg, 1023);
                    buf_exe_arg[copied_length] = '\0';
                    break;
                }
                case caction_toggle_overlay_enabled_state:
                {
                    int_id = action.IntID;
                    break;
                }
            }

            str_icon_file = action.IconFilename;
            use_action_icon = true;
        }

        bool do_save = false;

        const float column_width_0 = ImGui::GetFontSize() * 10.0f;

        ImGui::Columns(2, "ColumnCustomAction", false);
        ImGui::SetColumnWidth(0, column_width_0);


        ImGui::AlignTextToFramePadding();
        ImGui::Text("Name");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1.0f);

        if (ImGui::InputText("##Name", buf_name, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            do_save = true;
        }

        ImGui::PopupContextMenuInputText(nullptr, buf_name, 1024);

        ImGui::NextColumn();

        //Button Appearance stuff
        ImVec2 b_size, b_uv_min, b_uv_max;
        TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
        //Default button size for custom actions
        ImVec2 b_size_default = b_size;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Button Appearance");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1.0f);

        bool use_icon = !str_icon_file.empty();

        if (use_icon)
        {
            if (use_action_icon)
            {
                use_icon = TextureManager::Get().GetTextureInfo(action, b_size, b_uv_min, b_uv_max); //Loading may have failed, which falls back to no icon
            }
            else
            {
                TextureManager::Get().GetTextureInfo(tmtex_icon_temp, b_size, b_uv_min, b_uv_max);
            }
        }

        if (use_icon)
        {
            if (ImGui::ImageButton(ImGui::GetIO().Fonts->TexID, b_size_default, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
            {
                ImGui::OpenPopup("Select Icon");
            }
        }
        else
        {
            //Adapt to the last known scale used in VR so the text alignment matches what's seen in the headset later
            if (UIManager::Get()->IsInDesktopMode())
            {
                b_size_default.x *= UIManager::Get()->GetUIScale();
                b_size_default.y *= UIManager::Get()->GetUIScale();
                b_size_default.x *= ConfigManager::Get().GetConfigFloat(configid_float_interface_last_vr_ui_scale);
                b_size_default.y *= ConfigManager::Get().GetConfigFloat(configid_float_interface_last_vr_ui_scale);
            }
            else if (m_IsStyleScaled) //Scale the button size up if the large display scale is active
            {
                b_size_default.x *= 1.5f;
                b_size_default.y *= 1.5f;
            }

            if (ImGui::ButtonWithWrappedLabel(buf_name, b_size_default))
            {
                ImGui::OpenPopup("Select Icon");
            }
        }

        if (PopupIconSelect(str_icon_file)) //True if icon was changed
        {
            if (!str_icon_file.empty())
            {
                TextureManager::Get().SetTextureFilenameIconTemp(WStringConvertFromUTF8(str_icon_file.c_str()).c_str());
                TextureManager::Get().ReloadAllTexturesLater(); //Might be considering excessive, but the loading is pretty fast
                use_action_icon = false;
            }
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Function");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1.0f);

        const char* f_items[] = {"Press Keys", "Type String", "Launch Application", "Toggle Overlay Enabled State"};
        ImGui::Combo("##ComboFunction", &action_function, f_items, IM_ARRAYSIZE(f_items));

        ImGui::Columns(1);
        ImGui::Separator();

        ImGui::Columns(2, "ColumnCustomActionF", false);
        ImGui::SetColumnWidth(0, column_width_0);

        if (action_function == caction_press_keys)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Key Code 1");
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Desktop+ uses virtual key codes to simulate input.\nThe meaning of some of them depend on the used keyboard layout.\nWhen Desktop+UI is launched in desktop mode, the key code can also be directly set from user input.");
            ImGui::NextColumn();

            ButtonKeybind(&keycodes[0]);
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Key Code 2");
            ImGui::NextColumn();

            ButtonKeybind(&keycodes[1]);
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Key Code 3");
            ImGui::NextColumn();

            ButtonKeybind(&keycodes[2]);
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Checkbox("Toggle Keys", &bool_id);
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("The keys' pressed states are inverted when the action is executed.\nDesktop+ will not release the keys until the action is executed again.");
        }
        else if (action_function == caction_type_string)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Typed String");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1.0f);
            
            if (ImGui::InputText("##TypeString", buf_type_str, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                do_save = true;
            }

            ImGui::PopupContextMenuInputText(nullptr, buf_type_str, 1024);
        }
        else if (action_function == caction_launch_application)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Executable Path");
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("This can also be a normal file or URL");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1.0f);

            if (ImGui::InputText("##ExePath", buf_exe_path, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                do_save = true;
            }

            ImGui::PopupContextMenuInputText(nullptr, buf_exe_path, 1024);

            ImGui::NextColumn();
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Application Arguments");
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("These are passed to the launched application.\nIf unsure, leave this blank.");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1.0f);

            if (ImGui::InputText("##ExeArg", buf_exe_arg, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                do_save = true;
            }

            ImGui::PopupContextMenuInputText(nullptr, buf_exe_arg, 1024);
        }
        else if (action_function == caction_toggle_overlay_enabled_state)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Overlay ID");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1.0f);

            if (ImGui::InputInt("##IntID", &int_id, 1, 2))
            {
                int_id = clamp(int_id, 0, (int)vr::k_unMaxOverlayCount - 1); //Though it's impossible to max out the overlay limit with desktop overlays either way
            }

            if ( (ImGui::IsItemDeactivated()) && (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) ) //Enter deactivates the item before we can catch it
            {
                do_save = true;
            }
        }

        ImGui::Columns(1);

        ImGui::Separator();

        if (ImGui::Button("Ok"))
        {
            do_save = true;
        }

        if (do_save)
        {
            std::string name_new = buf_name;

            //We clear unrelated fields to avoid old data to appear when editing at another time
            action = CustomAction();
            action.FunctionType = (CustomActionFunctionID)action_function;
            action.Name = buf_name;

            switch (action_function)
            {
                case caction_press_keys:
                {
                    action.KeyCodes[0] = keycodes[0];
                    action.KeyCodes[1] = keycodes[1];
                    action.KeyCodes[2] = keycodes[2];
                    action.IntID       = bool_id;
                    break;
                }
                case caction_type_string:
                {
                    action.StrMain = buf_type_str;
                    break;
                }
                case caction_launch_application:
                {
                    action.StrMain = buf_exe_path;
                    action.StrArg = buf_exe_arg;
                    break;
                }
                case caction_toggle_overlay_enabled_state:
                {
                    action.IntID = int_id;
                    break;
                }
            }

            action.IconFilename = str_icon_file;

            //Reload textures later in case the icon has changed or a previously unloaded character is part of the name now
            TextureManager::Get().ReloadAllTexturesLater();

            action.SendUpdateToDashboardApp(id, UIManager::Get()->GetWindowHandle());

            m_ActionEditIsNew = false;

            ImGui::CloseCurrentPopup();
        }
            
        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            if (m_ActionEditIsNew) //Canceling a new Action should remove it again
            {
                ActionManager::Get().EraseCustomAction(id);
                UIManager::Get()->RepeatFrame();

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_delete, id);

                m_ActionEditIsNew = false;
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void WindowSettings::PopupOverlayDetachedPositionChange()
{
    ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

    static bool popup_was_open = false;
    static float button_backward_width = 0.0f;

    const float column_width_0 = ImGui::GetFontSize() * 3.0f;
    const float column_width_1 = ImGui::GetFrameHeightWithSpacing() * 3.0f + style.ItemInnerSpacing.x;
    const float column_width_2 = button_backward_width + (style.ItemInnerSpacing.x * 2.0f);
    const float column_width_3 = column_width_0;
    const float popup_width = column_width_0 + (column_width_1 * 2.0f) + (column_width_2 * 2.0f) + column_width_3 + (ImGui::GetStyle().ItemSpacing.x * 1.0f) - 1.0f;

    //Set popup rounding to the same as a normal window
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ImGui::GetStyle().WindowRounding);
    bool is_popup_rounding_pushed = true;

    //Center popup
    ImGui::SetNextWindowSizeConstraints(ImVec2(popup_width, -1),  ImVec2(popup_width, -1));
    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopup("OverlayChangePosPopup", ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        popup_was_open = true;

        ImGui::PopStyleVar(); //ImGuiStyleVar_PopupRounding
        is_popup_rounding_pushed = false;

        static int active_capture_type = 0; //0 = off, 1 = Move, 2 = Rotate
        static ImVec2 active_capture_pos;

        if (ImGui::IsWindowAppearing())
        {
            UIManager::Get()->RepeatFrame();
        }

        if (!UIManager::Get()->IsInDesktopMode())
        {
            ImGui::Text("Drag the overlay around to change its position.");
            ImGui::Text("Hold right-click for two-handed gesture transform.");
        }
        else
        {
            ImGui::TextWrapped("Hold down the drag buttons (\"D\") to move or rotate the overlay with the mouse.");
        }

        ImGui::Separator();

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Manual Adjustment");

        if (ImGui::IsItemHovered())
        {
            HighlightOverlay(ConfigManager::Get().GetConfigIntRef(configid_int_interface_overlay_current_id));
        }
        else
        {
            HighlightOverlay(-1);
        }

        ImGui::Columns(6, "ColumnManualAdjust", false);

        ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
        ImGui::SetColumnWidth(0, column_width_0);
        ImGui::SetColumnWidth(1, column_width_1);
        ImGui::SetColumnWidth(2, column_width_2);
        ImGui::SetColumnWidth(3, column_width_3);
        ImGui::SetColumnWidth(4, column_width_1);
        ImGui::SetColumnWidth(5, column_width_2);

        ImGui::PushButtonRepeat(true);

        //Row 1
        ImGui::NextColumn();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing());

        if (ImGui::ArrowButton("MoveUp", ImGuiDir_Up))
        {
            //Do some packing
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;  //Increase bit
            packed_value |= ipcactv_ovrl_pos_adjust_updown;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::NextColumn();

        if (ImGui::Button("Forward", {button_backward_width, 0.0f}))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_forwback;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing());

        if (ImGui::ArrowButton("RotUp", ImGuiDir_Up))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_rotx);
        }

        ImGui::NextColumn();

        if (ImGui::Button("Roll CW", {button_backward_width, 0.0f}))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_rotz);
        }

        ImGui::NextColumn();

        //Row 2
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Move");
        ImGui::NextColumn();

        if (ImGui::ArrowButton("MoveLeft", ImGuiDir_Left))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_rightleft);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (UIManager::Get()->IsInDesktopMode())
        {
            bool is_active = (active_capture_type == 1);

            if (is_active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);

            ImGui::Button("D##Move", button_size);

            //Activate on mouse down instead of normal button behavior, which is on mouse up
            if ((active_capture_type == 0) && (ImGui::IsItemHovered()) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                active_capture_type = 1;
                active_capture_pos = ImGui::GetIO().MousePos;
            }

            if (is_active)
                ImGui::PopStyleColor();
        }
        else
        {
            ImGui::Dummy(button_size);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::ArrowButton("MoveRight", ImGuiDir_Right))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_rightleft;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::NextColumn();

        if (ImGui::Button("Backward"))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_forwback);
        }
        button_backward_width = ImGui::GetItemRectSize().x;

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Rotate");
        ImGui::NextColumn();

        if (ImGui::ArrowButton("RotLeft", ImGuiDir_Left))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_roty);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (UIManager::Get()->IsInDesktopMode())
        {
            bool is_active = (active_capture_type == 2);

            if (is_active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);

            ImGui::Button("D##Rot", button_size);

            //Activate on mouse down instead of normal button behavior, which is on mouse up
            if ((active_capture_type == 0) && (ImGui::IsItemHovered()) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                active_capture_type = 2;
                active_capture_pos = ImGui::GetIO().MousePos;
            }

            if (is_active)
                ImGui::PopStyleColor();
        }
        else
        {
            ImGui::Dummy(button_size);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::ArrowButton("RotRight", ImGuiDir_Right))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_roty;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::NextColumn();

        if (ImGui::Button("Roll CCW", {button_backward_width, 0.0f}))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_rotz;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::NextColumn();

        //Row 3
        ImGui::NextColumn();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing());

        if (ImGui::ArrowButton("MoveDown", ImGuiDir_Down))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_updown);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing());

        if (ImGui::ArrowButton("RotDown", ImGuiDir_Down))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_rotx;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::PopButtonRepeat();

        ImGui::NextColumn();

        if (ImGui::Button("To HMD", {button_backward_width, 0.0f}))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_lookat);
        }

        ImGui::Columns(1);

        ImGui::Separator();

        if (ImGui::Button("Done"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        static float button_reset_width = 0.0f;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - button_reset_width);

        if (ImGui::Button("Reset"))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
        }

        button_reset_width = ImGui::GetItemRectSize().x;

        //Mouse Dragging
        if (active_capture_type != 0)
        {
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                active_capture_type = 0;
                ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            }
            else
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_None);

                const float delta_step = 5.0f;
                ImVec2 mouse_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

                if (active_capture_type == 1)
                {
                    //X -> Right/Left
                    if (fabs(mouse_delta.x) > delta_step)
                    {
                        unsigned int packed_value = (mouse_delta.x > 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                        packed_value |= ipcactv_ovrl_pos_adjust_rightleft;

                        //Using the existing position adjust message a few times might be cheap, but it also results in actually useful grid-snapped adjustments
                        int steps = (int)(fabs(mouse_delta.x) / delta_step);
                        for (int i = 0; i < steps; ++i)
                        {
                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                        }
                    }

                    //Y -> Up/Down
                    if (fabs(mouse_delta.y) > delta_step)
                    {
                        unsigned int packed_value = (mouse_delta.y < 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                        packed_value |= ipcactv_ovrl_pos_adjust_updown;

                        int steps = (int)(fabs(mouse_delta.y) / delta_step);
                        for (int i = 0; i < steps; ++i)
                        {
                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                        }
                    }

                    //Wheel -> Forward/Backward
                    if (fabs(ImGui::GetIO().MouseWheel) > 0.0f)
                    {
                        unsigned int packed_value = (ImGui::GetIO().MouseWheel < 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                        packed_value |= ipcactv_ovrl_pos_adjust_forwback;

                        int steps = (int)(fabs(ImGui::GetIO().MouseWheel) * delta_step);
                        for (int i = 0; i < steps; ++i)
                        {
                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                        }
                    }
                }
                else //active_capture_type == 2
                {
                    //X -> Rotate Y+-
                    if (fabs(mouse_delta.x) > delta_step)
                    {
                        unsigned int packed_value = (mouse_delta.x > 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                        packed_value |= ipcactv_ovrl_pos_adjust_roty;

                        int steps = (int)(fabs(mouse_delta.x) / delta_step);
                        for (int i = 0; i < steps; ++i)
                        {
                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                        }
                    }

                    //Y -> Rotate X+-
                    if (fabs(mouse_delta.y) > delta_step)
                    {
                        unsigned int packed_value = (mouse_delta.y > 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                        packed_value |= ipcactv_ovrl_pos_adjust_rotx;

                        int steps = (int)(fabs(mouse_delta.y) / delta_step);
                        for (int i = 0; i < steps; ++i)
                        {
                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                        }
                    }

                    //Wheel -> Rotate Z+-
                    if (fabs(ImGui::GetIO().MouseWheel) > 0.0f)
                    {
                        unsigned int packed_value = (ImGui::GetIO().MouseWheel > 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                        packed_value |= ipcactv_ovrl_pos_adjust_rotz;

                        int steps = (int)(fabs(ImGui::GetIO().MouseWheel) * delta_step);
                        for (int i = 0; i < steps; ++i)
                        {
                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                        }
                    }
                }

                //Reset mouse cursor if needed
                ImGuiIO& io = ImGui::GetIO();

                if (fabs(mouse_delta.x) > delta_step)
                {
                    io.WantSetMousePos = true;
                    io.MousePos.x = active_capture_pos.x;
                    io.MouseClickedPos[0].x = io.MousePos.x;    //for drag delta
                }

                if (fabs(mouse_delta.y) > delta_step)
                {
                    io.WantSetMousePos = true;
                    io.MousePos.y = active_capture_pos.y;
                    io.MouseClickedPos[0].y = io.MousePos.y;
                }
            }
        }

        ImGui::EndPopup();
    }

    //This has to be popped early to prevent affecting other popups but we can't forget about it when the popup is closed either
    if (is_popup_rounding_pushed)
    {
        ImGui::PopStyleVar(); //ImGuiStyleVar_PopupRounding
    }

    //Detect if the popup was closed, which can happen at any time from clicking outside of it
    if ((popup_was_open) && (!ImGui::IsPopupOpen("OverlayChangePosPopup")))
    {
        popup_was_open = false;

        bool& is_changing_position = ConfigManager::Get().GetConfigBoolRef(configid_bool_state_overlay_dragmode);

        is_changing_position = false;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragselectmode_show_hidden), is_changing_position);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragmode), is_changing_position);
    }
}

bool WindowSettings::PopupIconSelect(std::string& filename)
{
    bool ret = false;

    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Select Icon", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::SetWindowSize(ImVec2(GetSize().x * 0.5f, GetSize().y * 0.75f));

        static int list_id = 0;
        static std::vector<std::string> list_files;

        if (ImGui::IsWindowAppearing())
        {
            //Get current filename without subfolders
            size_t filename_compare_start = filename.find_last_of('/');
            const std::string filename_compare = filename.substr( (filename_compare_start != std::string::npos) ? filename_compare_start + 1 : 0);

            list_files.clear();
            list_files.emplace_back("[Text Label]");

            const std::wstring wpath = WStringConvertFromUTF8(std::string(ConfigManager::Get().GetApplicationPath() + "images/icons/*.png").c_str());
            WIN32_FIND_DATA find_data;
            HANDLE handle_find = ::FindFirstFileW(wpath.c_str(), &find_data);

            if (handle_find != INVALID_HANDLE_VALUE)
            {
                do
                {
                    list_files.push_back(StringConvertFromUTF16(find_data.cFileName));

                    //Select matching entry when appearing
                    if (list_files.back() == filename_compare)
                    {
                        list_id = (int)list_files.size() - 1;
                    }
                }
                while (::FindNextFileW(handle_find, &find_data) != 0);

                ::FindClose(handle_find);
            }

            UIManager::Get()->RepeatFrame();
        }

        ImGui::Text("Select Icon");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Custom icons can be added as PNG files in the \"images\\icons\" directory");

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::BeginChild("IconList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), true);

        //List files
        int index = 0;
        for (const auto& str: list_files)
        {
            ImGui::PushID(&str);
            if (ImGui::Selectable(str.c_str(), (index == list_id)))
            {
                list_id = index;
            }
            ImGui::PopID();

            index++;
        }

        ImGui::EndChild();

        if (ImGui::Button("Ok")) 
        {
            if (list_id == 0)
            {
                filename = "";
            }
            else
            {
                filename = "images/icons/" + list_files[list_id];
            }

            UIManager::Get()->RepeatFrame();
            ImGui::CloseCurrentPopup();

            ret = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return ret;
}

void WindowSettings::PopupSettingsReset()
{
    //Set popup rounding to the same as a normal window
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ImGui::GetStyle().WindowRounding);

    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopup("SettingsResetPopup", ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::SetWindowSize(ImVec2(GetSize().x * 0.5f, -1.0f));

        ImGui::Text("This will reset all settings to their default values, including the current overlay setup.\nSaved profiles are not affected. Continue?");

        ImGui::Separator();

        if (ImGui::Button("Yes, Restore Default Settings")) 
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

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(); //ImGuiStyleVar_PopupRounding
}

void WindowSettings::HighlightOverlay(int overlay_id)
{
    //Indicate the current overlay by tinting it when hovering the overlay selector
    if (UIManager::Get()->IsOpenVRLoaded())
    {
        static int colored_id = -1;

        //Tint overlay if no other overlay is currently tinted (adds one frame delay on switching but it doesn't really matter)
        if ( (overlay_id != -1) && (colored_id == -1) )
        {
            vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().FindOverlayHandle((unsigned int)overlay_id);

            if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
            {
                const OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)overlay_id);
                float brightness = lin2log(data.ConfigFloat[configid_float_overlay_brightness]);
                ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);

                vr::VROverlay()->SetOverlayColor(ovrl_handle, col.x * brightness, col.y * brightness, col.z * brightness);

                colored_id = overlay_id;
            }
        }
        else if ( (colored_id != -1) && (colored_id != overlay_id) ) //Remove tint if overlay id is different or -1
        {
            vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().FindOverlayHandle((unsigned int)colored_id);

            if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
            {
                const OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)colored_id);
                float brightness = lin2log(data.ConfigFloat[configid_float_overlay_brightness]);

                vr::VROverlay()->SetOverlayColor(ovrl_handle, brightness, brightness, brightness);
            }

            colored_id = -1;
        }
    }
}

int WindowSettings::GetOverlayIcon(unsigned int overlay_id, TMNGRTexID& texture_id)
{
    if (overlay_id < OverlayManager::Get().GetOverlayCount())
    {
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
        int desktop_id = -2;

        switch (data.ConfigInt[configid_int_overlay_capture_source])
        {
            case ovrl_capsource_desktop_duplication:
            {
                desktop_id = data.ConfigInt[configid_int_overlay_desktop_id];
                break;
            }
            case ovrl_capsource_winrt_capture:
            {
                if (data.ConfigIntPtr[configid_intptr_overlay_state_winrt_hwnd] != 0)
                {
                    //Find HWND in the capturable window list to use cached HICON if possible
                    auto it = std::find_if(m_CaptureWindowList.begin(), m_CaptureWindowList.end(), 
                                           [&](const auto& window) { return (window.WindowHandle == (HWND)data.ConfigIntPtr[configid_intptr_overlay_state_winrt_hwnd]); });

                    if (it != m_CaptureWindowList.end())
                    {
                        return TextureManager::Get().GetWindowIconCacheID(it->GetIcon());
                    }

                    //If not get icon directly
                    return TextureManager::Get().GetWindowIconCacheID(WindowInfo::GetIcon( (HWND)data.ConfigIntPtr[configid_intptr_overlay_state_winrt_hwnd] ));
                }
                else if (data.ConfigInt[configid_int_overlay_winrt_desktop_id] != -2)
                {
                    desktop_id = data.ConfigInt[configid_int_overlay_winrt_desktop_id];
                }
                else
                {
                    texture_id = tmtex_icon_xsmall_desktop_none;
                    return -1;
                }
                break;
            }
            case ovrl_capsource_ui:
            {
                texture_id = tmtex_icon_xsmall_performance_monitor;
                return -1;
            }
        }

        if (desktop_id == -2)
        {
            desktop_id = 0;
        }

        if (desktop_id != -1)
        {
            texture_id = (tmtex_icon_desktop_1 + desktop_id <= tmtex_icon_desktop_6) ? (TMNGRTexID)(tmtex_icon_xsmall_desktop_1 + desktop_id) : tmtex_icon_xsmall_desktop;
        }
        else
        {
            texture_id = tmtex_icon_xsmall_desktop_all;
        }
    }

    return -1;
}

void WindowSettings::DuplicateCurrentOverlay()
{
    int& current_overlay = ConfigManager::Get().GetConfigIntRef(configid_int_interface_overlay_current_id);

    //Copy data of current overlay
    OverlayConfigData data = OverlayManager::Get().GetCurrentConfigData();

    OverlayManager::Get().AddOverlay(data, (current_overlay == k_ulOverlayID_Dashboard));
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_new, current_overlay);

    current_overlay = (int)OverlayManager::Get().GetOverlayCount() - 1;
    OverlayManager::Get().SetCurrentOverlayID(current_overlay);
    //No need to sync current overlay here

    if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui)
    {
        UIManager::Get()->GetPerformanceWindow().ScheduleOverlaySharedTextureUpdate();
    }

    m_OverlayNameBufferNeedsUpdate = true;
    UIManager::Get()->RepeatFrame();
}

WindowSettings::WindowSettings() : m_Visible(false), m_Alpha(0.0f), m_ActionEditIsNew(false), m_OverlayNameBufferNeedsUpdate(true), m_IsStyleScaled(false)
{

}

void WindowSettings::Show()
{
    if (m_Size.x == 0.0f)
    {
        m_Size.x = TEXSPACE_TOTAL_WIDTH * UIManager::Get()->GetUIScale();

        if (UIManager::Get()->IsInDesktopMode())    //Act as a "fullscreen" window if in desktop mode
            m_Size.y = ImGui::GetIO().DisplaySize.y;
        else
            m_Size.y = ImGui::GetIO().DisplaySize.y * 0.84f;
    }

    m_Visible = true;
    UIManager::Get()->UpdateDesktopOverlayPixelSize(); //Make sure we still have the correct size to work with
    UIManager::Get()->UpdateCompositorRenderQualityLow();

    //Adjust sort order when settings window is visible. This will still result in weird visuals with other overlays when active, but at least not constantly.
    //It's a compromise, really. The other reliable method would be about 1m distance between the two overlays, which is not happening
    if (!UIManager::Get()->IsInDesktopMode())
    {
        vr::VROverlay()->SetOverlaySortOrder(UIManager::Get()->GetOverlayHandle(), 1);
    }
}

void WindowSettings::Hide()
{
    m_Visible = false;
    ConfigManager::Get().SaveConfigToFile();
}

void WindowSettings::Update()
{  
    if (UIManager::Get()->IsInDesktopMode())    //In desktop mode it's the only thing displayed, so no transition
    {
        if (!m_Visible)
        {
            Show();
        }

        m_Alpha = 1.0f;
    }
    else if ( (m_Alpha != 0.0f) || (m_Visible) )
    {
        //Alpha fade animation
        m_Alpha += (m_Visible) ? 0.1f : -0.1f;

        if (m_Alpha > 1.0f)
            m_Alpha = 1.0f;
        else if (m_Alpha < 0.0f)
            m_Alpha = 0.0f;

        //Undo sort order once the settings window fully disappeared, unless the overlay is being hovered still
        if ( (!m_Visible) && (m_Alpha == 0.0f) && (!vr::VROverlay()->IsHoverTargetOverlay(UIManager::Get()->GetOverlayHandle())) )
        {
            vr::VROverlay()->SetOverlaySortOrder(UIManager::Get()->GetOverlayHandle(), 0);
        }
    }

    if (m_Alpha == 0.0f)
        return;

    PushInterfaceScale();

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Alpha);

    if (UIManager::Get()->IsInDesktopMode())    //Act as a "fullscreen" window if in desktop mode
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    }

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(m_Size);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    if (!m_Visible)
        flags |= ImGuiWindowFlags_NoInputs;

    ImGui::Begin("WindowSettings", nullptr, flags);

    //Left
    static int selected = 0;
    const float pane_left_width = ImGui::GetFontSize() * 5.0f;

    //Interesting clip rect and style pushing is in order to get ImGui to ignore the window and widget padding on this side of the window
    ImGui::PushClipRect({ImGui::GetStyle().WindowBorderSize, 0.0f}, {FLT_MAX, FLT_MAX}, false);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetStyle().WindowPadding.x);

    //Early pop as we have popups which should have a normal border
    if (UIManager::Get()->IsInDesktopMode())
    {
        ImGui::PopStyleVar(); //ImGuiStyleVar_WindowBorderSize
        ImGui::PopStyleVar(); //ImGuiStyleVar_WindowRounding
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, ImGui::GetStyle().ItemSpacing.y});

    ImGui::BeginChild("LeftPane", {pane_left_width, 0.0f}, false);

        static float selectable_height = 0.0f;

        //Dummy sets pane width and pushes the selectables down so they're middle aligned
        ImGui::Dummy({ pane_left_width, ((ImGui::GetWindowSize().y - ImGui::GetStyle().ItemSpacing.y) / 2.0f) - (selectable_height * 3.5f) });

        ImGui::PushClipRect({0.0f, 0.0f}, {FLT_MAX, FLT_MAX}, true); //Push another clip rect as BeginChild() adds its own in-between

        if (ImGui::Selectable("  Overlay",     selected == 0))
            selected = 0;
        if (ImGui::Selectable("  Interface",   selected == 1))
            selected = 1;
        if (ImGui::Selectable("  Actions",     selected == 2))
            selected = 2;
        if (ImGui::Selectable("  Input",       selected == 3))
            selected = 3;
        if (ImGui::Selectable("  Windows",     selected == 4))
            selected = 4;
        if (ImGui::Selectable("  Performance", selected == 5))
            selected = 5;
        if (ImGui::Selectable("  Misc",        selected == 6))
            selected = 6;

        selectable_height = ImGui::GetItemRectSize().y;
        ImGui::PopClipRect();

    ImGui::EndChild();

    ImGui::PopClipRect();

    ImGui::SameLine();

    //Vertical separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("vsep", ImVec2(1.0f, 0.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PopStyleVar(); //ImGuiStyleVar_ItemSpacing

    ImGui::SameLine();

    //Right
    ImGui::BeginGroup();

        UpdateWarnings();

        switch (selected)
        {
            case 0: UpdateCatOverlay();     break;
            case 1: UpdateCatInterface();   break;
            case 2: UpdateCatActions();     break;
            case 3: UpdateCatInput();       break;
            case 4: UpdateCatWindows();     break;
            case 5: UpdateCatPerformance(); break;
            case 6: UpdateCatMisc();        break;
        }

    ImGui::EndGroup();

    ImGui::End();
    ImGui::PopStyleVar(); //ImGuiStyleVar_Alpha

    PopInterfaceScale();

    //Toggle performance stats based on the active page
    bool& performance_stats_active = ConfigManager::Get().GetConfigBoolRef(configid_bool_state_performance_stats_active);
    if ((selected == 5) && (m_Visible) && (!performance_stats_active))
    {
        performance_stats_active = true;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_performance_stats_active), true);
    }
    else if (( (selected != 5) || (!m_Visible) ) && (performance_stats_active))
    {
        performance_stats_active = false;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_performance_stats_active), false);
    }
}

bool WindowSettings::IsShown() const
{
    return m_Visible;
}

const ImVec2& WindowSettings::GetSize() const
{
    return m_Size;
}

void WindowSettings::RefreshCurrentOverlayNameBuffer()
{
    m_OverlayNameBufferNeedsUpdate = true;
}
