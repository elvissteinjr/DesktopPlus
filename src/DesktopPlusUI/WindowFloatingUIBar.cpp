#include "WindowFloatingUIBar.h"

#include "ImGuiExt.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "UIManager.h"
#include "OverlayManager.h"
#include "WindowManager.h"
#include "DesktopPlusWinRT.h"

//-WindowFloatingUIMainBar
WindowFloatingUIMainBar::WindowFloatingUIMainBar() : m_IsCurrentWindowCapturable(-1), m_AnimationProgress(0.0f)
{

}

void WindowFloatingUIMainBar::DisplayTooltipIfHovered(const char* text)
{
    if ( ((m_AnimationProgress == 0.0f) || (m_AnimationProgress == 1.0f)) && (ImGui::IsItemHovered()) ) //Also hide while animating
    {
        static ImVec2 button_pos_last; //Remember last position and use it when posible. This avoids flicker when the same tooltip string is used in different places
        ImVec2 pos = ImGui::GetItemRectMin();
        float button_width = ImGui::GetItemRectSize().x;

        //Default tooltips are not suited for this as there's too much trouble with resize flickering and stuff
        ImGui::Begin(text, nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

        ImGui::TextUnformatted(text);

        //Not using GetWindowSize() here since it's delayed and plays odd when switching between buttons with the same label
        ImVec2 window_size = ImGui::GetItemRectSize();
        window_size.x += ImGui::GetStyle().WindowPadding.x * 2.0f;
        window_size.y += ImGui::GetStyle().WindowPadding.y * 2.0f;

        //Repeat frame when the window is appearing as it will not have the right position (either from being first time or still having old pos)
        if ( (ImGui::IsWindowAppearing()) || (pos.x != button_pos_last.x) )
        {
            UIManager::Get()->RepeatFrame();
        }

        button_pos_last = pos;

        pos.x += (button_width / 2.0f) - (window_size.x / 2.0f);
        pos.y += button_width + ImGui::GetStyle().WindowPadding.y - 2.0f;

        //Clamp x so the tooltip does not get cut off
        pos.x = clamp(pos.x, 0.0f, ImGui::GetIO().DisplaySize.x - window_size.x);   //Clamp right side to texture end

        ImGui::SetWindowPos(pos);

        ImGui::End();
    }
}

void WindowFloatingUIMainBar::Update(float actionbar_height, unsigned int overlay_id)
{
    OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData(overlay_id);

    ImGuiIO& io = ImGui::GetIO();

    ImVec2 b_size, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
    const ImGuiStyle& style = ImGui::GetStyle();

    //Put window near the bottom of the overlay with space for the tooltips + padding (touching action-bar when visible)
    const float offset_base = ImGui::GetFontSize() + style.FramePadding.y + (style.WindowPadding.y * 2.0f) + 3.0f;
    const float offset_y = smoothstep(m_AnimationProgress, offset_base + actionbar_height /* Action-Bar not visible */, offset_base /* Action-Bar visible */);

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2.0f, io.DisplaySize.y - offset_y), 0, ImVec2(0.5f, 1.0f));
    ImGui::Begin("WindowFloatingUIMainBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
                                                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    //Show Action-Bar Button (this is a per-overlay state)
    bool& is_actionbar_enabled = overlay_data.ConfigBool[configid_bool_overlay_actionbar_enabled];
    bool actionbar_was_enabled = is_actionbar_enabled;

    if (actionbar_was_enabled)
        ImGui::PushStyleColor(ImGuiCol_Button, Style_ImGuiCol_ButtonPassiveToggled);

    ImGui::PushID(tmtex_icon_small_actionbar);
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_actionbar, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        is_actionbar_enabled = !is_actionbar_enabled;
        //This is an UI state so no need to sync
    }

    if (actionbar_was_enabled)
        ImGui::PopStyleColor();

    DisplayTooltipIfHovered(TranslationManager::GetString( (actionbar_was_enabled) ? tstr_FloatingUIActionBarHideTip : tstr_FloatingUIActionBarShowTip ));

    ImGui::PopID();
    //

    ImGui::SameLine();

    //Add current window as overlay (only show if desktop duplication or non-window WinRT capture)
    if (  (overlay_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication) ||
         ((overlay_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && (overlay_data.ConfigIntPtr[configid_intptr_overlay_state_winrt_hwnd] == 0)) )
    {
        //If marked to need update, refresh actual state
        if (m_IsCurrentWindowCapturable == -1)
        {
            m_IsCurrentWindowCapturable = (WindowManager::Get().WindowListFindWindow(::GetForegroundWindow()) != nullptr);
        }

        if (m_IsCurrentWindowCapturable != 1)
            ImGui::PushItemDisabled();

        ImGui::PushID(tmtex_icon_small_add_window);
        TextureManager::Get().GetTextureInfo(tmtex_icon_small_add_window, b_size, b_uv_min, b_uv_max);
        ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max); //This one's activated on mouse down

        if (ImGui::IsItemActivated())
        {
            vr::TrackedDeviceIndex_t device_index = vr::VROverlay()->GetPrimaryDashboardDevice();

            //If no dashboard device, try finding one
            if (device_index == vr::k_unTrackedDeviceIndexInvalid)
            {
                device_index = FindPointerDeviceForOverlay(UIManager::Get()->GetOverlayHandleFloatingUI());
            }

            //Try to get the pointer distance
            float source_distance = 1.0f;
            vr::VROverlayIntersectionResults_t results;

            if (ComputeOverlayIntersectionForDevice(UIManager::Get()->GetOverlayHandleFloatingUI(), device_index, vr::TrackingUniverseStanding, &results))
            {
                source_distance = results.fDistance;
            }

            //Set pointer hint in case dashboard app needs it
            ConfigManager::Get().SetConfigInt(configid_int_state_laser_pointer_device_hint, (int)device_index);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_laser_pointer_device_hint), (int)device_index);

            //Add overlay
            HWND current_window = ::GetForegroundWindow();
            OverlayManager::Get().AddOverlay(ovrl_capsource_winrt_capture, -2, current_window);

            //Send to dashboard app
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_intptr_state_arg_hwnd), (LPARAM)current_window);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_new_drag, MAKELPARAM(-2, (source_distance * 100.0f)));
        }

        if (m_IsCurrentWindowCapturable != 1)
            ImGui::PopItemDisabled();

        DisplayTooltipIfHovered(TranslationManager::GetString(tstr_FloatingUIWindowAddTip));

        ImGui::PopID();

        ImGui::SameLine();
    }
    //

    ImGui::SameLine();

    //Drag-Mode Toggle Button (this is a global state)
    bool& is_dragmode_enabled = ConfigManager::Get().GetConfigBoolRef(configid_bool_state_overlay_dragmode);
    bool dragmode_was_enabled = is_dragmode_enabled;

    if (dragmode_was_enabled)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    ImGui::PushID(tmtex_icon_small_move);
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_move, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        is_dragmode_enabled = !is_dragmode_enabled;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragselectmode_show_hidden), false);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragmode), is_dragmode_enabled);
    }

    if (dragmode_was_enabled)
        ImGui::PopStyleColor();

    DisplayTooltipIfHovered(TranslationManager::GetString( (dragmode_was_enabled) ? tstr_FloatingUIDragModeDisableTip : tstr_FloatingUIDragModeEnableTip ));

    ImGui::PopID();
    //

    ImGui::SameLine();

    //Close/Disable Button
    ImGui::PushID(tmtex_icon_small_close);
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_close, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        overlay_data.ConfigBool[configid_bool_overlay_enabled] = false;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)overlay_id);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_bool_overlay_enabled), false);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
    }

    DisplayTooltipIfHovered(TranslationManager::GetString(tstr_FloatingUIHideOverlayTip));

    ImGui::PopID();
    //

    ImGui::PopStyleColor(); //ImGuiCol_Button
    ImGui::PopStyleVar();   //ImGuiStyleVar_FrameRounding

    m_Pos  = ImGui::GetWindowPos();
    m_Size = ImGui::GetWindowSize();

    ImGui::End();

    //Sliding animation when action-bar state changes
    if (UIManager::Get()->GetFloatingUI().GetAlpha() != 0.0f)
    {
        m_AnimationProgress += (is_actionbar_enabled) ? ImGui::GetIO().DeltaTime * 3.0f : ImGui::GetIO().DeltaTime * -3.0f;
        m_AnimationProgress = clamp(m_AnimationProgress, 0.0f, 1.0f);
    }
    else //Skip animation when Floating UI is just fading in
    {
        m_AnimationProgress = (is_actionbar_enabled) ? 1.0f : 0.0f;
    }
}

const ImVec2& WindowFloatingUIMainBar::GetPos() const
{
    return m_Pos;
}

const ImVec2& WindowFloatingUIMainBar::GetSize() const
{
    return m_Size;
}

void WindowFloatingUIMainBar::MarkCurrentWindowCapturableStateOutdated()
{
    //Mark state as outdated. We don't do the update here as the current window can change a lot while the UI isn't even displaying... no need to bother then.
    m_IsCurrentWindowCapturable = -1;
}


//-WindowFloatingUIActionBar
void WindowFloatingUIActionBar::DisplayTooltipIfHovered(const char* text)
{
    if (ImGui::IsItemHovered())
    {
        static ImVec2 button_pos_last; //Remember last position and use it when posible. This avoids flicker when the same tooltip string is used in different places
        ImVec2 pos = ImGui::GetItemRectMin();
        float button_width = ImGui::GetItemRectSize().x;

        //Default tooltips are not suited for this as there's too much trouble with resize flickering and stuff
        ImGui::Begin(text, nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

        ImGui::TextUnformatted(text);

        //Not using GetWindowSize() here since it's delayed and plays odd when switching between buttons with the same label
        ImVec2 window_size = ImGui::GetItemRectSize();
        window_size.x += ImGui::GetStyle().WindowPadding.x * 2.0f;
        window_size.y += ImGui::GetStyle().WindowPadding.y * 2.0f;

        //Repeat frame when the window is appearing as it will not have the right position (either from being first time or still having old pos)
        if ( (ImGui::IsWindowAppearing()) || (pos.x != button_pos_last.x) )
        {
            UIManager::Get()->RepeatFrame();
        }

        button_pos_last = pos;

        pos.x += (button_width / 2.0f) - (window_size.x / 2.0f);
        pos.y -= window_size.y + ImGui::GetStyle().WindowPadding.y;

        //Clamp x so the tooltip does not get cut off
        pos.x = clamp(pos.x, 0.0f, ImGui::GetIO().DisplaySize.x - window_size.x);   //Clamp right side to texture end

        ImGui::SetWindowPos(pos);

        ImGui::End();
    }
}

void WindowFloatingUIActionBar::UpdateDesktopButtons(unsigned int overlay_id)
{
    ImGui::PushID("DesktopButtons");

    ImGuiIO& io = ImGui::GetIO();

    ImVec2 b_size, b_uv_min, b_uv_max;
    OverlayConfigData& overlay_config = OverlayManager::Get().GetConfigData(overlay_id);
    ConfigID_Int current_configid = configid_int_overlay_desktop_id;
    bool disable_normal = false;
    bool disable_combined = false;
        
    if (overlay_config.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture)
    {
        current_configid = configid_int_overlay_winrt_desktop_id;
        //Check if buttons would be usable for WinRT capture
        disable_normal   = !DPWinRT_IsCaptureFromHandleSupported();
        disable_combined = !DPWinRT_IsCaptureFromCombinedDesktopSupported();
    }

    int& current_desktop = overlay_config.ConfigInt[current_configid];
    int current_desktop_new = current_desktop;

    if (ConfigManager::Get().GetConfigBool(configid_bool_interface_mainbar_desktop_include_all))
    {
        if (disable_combined)
            ImGui::PushItemDisabled();

        if (current_desktop == -1)
            ImGui::PushStyleColor(ImGuiCol_Button, Style_ImGuiCol_ButtonPassiveToggled);

        ImGui::PushID(tmtex_icon_desktop_all);
        TextureManager::Get().GetTextureInfo(tmtex_icon_desktop_all, b_size, b_uv_min, b_uv_max);
        if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
        {
            //Don't change to same value to avoid flicker from mirror reset
            if ( (current_desktop != -1) || (!ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring)) )
            {
                current_desktop_new = -1;
            }
        }
        DisplayTooltipIfHovered("Combined Desktop");
        ImGui::PopID();
        ImGui::SameLine();

        if (current_desktop == -1)
            ImGui::PopStyleColor();

        if (disable_combined)
            ImGui::PopItemDisabled();
    }

    int desktop_count = ConfigManager::Get().GetConfigInt(configid_int_state_interface_desktop_count);

    if (disable_normal)
        ImGui::PushItemDisabled();

    switch (ConfigManager::Get().GetConfigInt(configid_int_interface_mainbar_desktop_listing))
    {
        case mainbar_desktop_listing_individual:
        {
            for (int i = 0; i < desktop_count; ++i)
            {
                ImGui::PushID(tmtex_icon_desktop_1 + i);

                //We have icons for up to 6 desktops, the rest (if it even exists) gets blank ones)
                //Numbering on the icons starts with 1. 
                //While there is no guarantee this corresponds to the same display as in the Windows settings, it is more familiar than the 0-based ID used internally
                //Why icon bitmaps? No need to create/load an icon font, cleaner rendered number and easily customizable icon per desktop for the end-user.
                if (tmtex_icon_desktop_1 + i <= tmtex_icon_desktop_6)
                    TextureManager::Get().GetTextureInfo((TMNGRTexID)(tmtex_icon_desktop_1 + i), b_size, b_uv_min, b_uv_max);
                else
                    TextureManager::Get().GetTextureInfo(tmtex_icon_desktop, b_size, b_uv_min, b_uv_max);

                if (i == current_desktop)
                    ImGui::PushStyleColor(ImGuiCol_Button, Style_ImGuiCol_ButtonPassiveToggled);

                if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
                {
                    //Don't change to same value to avoid flicker from mirror reset
                    if ( (i != current_desktop) || (!ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring)) )
                    {
                        current_desktop_new = i;
                    }
                }

                if (i == current_desktop)
                    ImGui::PopStyleColor();

                DisplayTooltipIfHovered(TranslationManager::Get().GetDesktopIDString(i));

                ImGui::PopID();
                ImGui::SameLine();
            }
            break;
        }
        case mainbar_desktop_listing_cycle:
        {
            ImGui::PushID(tmtex_icon_desktop_prev);
            TextureManager::Get().GetTextureInfo(tmtex_icon_desktop_prev, b_size, b_uv_min, b_uv_max);
            if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
            {
                current_desktop_new--;

                if (current_desktop_new <= -1)
                    current_desktop_new = desktop_count - 1;
            }
            DisplayTooltipIfHovered("Previous Desktop");
            ImGui::PopID();
            ImGui::SameLine();

            ImGui::PushID(tmtex_icon_desktop_next);
            TextureManager::Get().GetTextureInfo(tmtex_icon_desktop_next, b_size, b_uv_min, b_uv_max);
            if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
            {
                current_desktop_new++;

                if (current_desktop_new == desktop_count)
                    current_desktop_new = 0;
            }
            DisplayTooltipIfHovered("Next Desktop");
            ImGui::PopID();
            ImGui::SameLine();
            break;
        }
    }

    if (disable_normal)
        ImGui::PopItemDisabled();

    if (current_desktop_new != current_desktop)
    {
        current_desktop = current_desktop_new;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)overlay_id);

        //Reset window selection when switching to a desktop
        if (current_configid == configid_int_overlay_winrt_desktop_id)
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_intptr_overlay_state_winrt_hwnd), 0);
            overlay_config.ConfigIntPtr[configid_intptr_overlay_state_winrt_hwnd] = 0;
        }

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(current_configid), current_desktop);

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);

        //Update overlay name
        OverlayManager::Get().SetOverlayNameAuto(overlay_id);

        //Update overlay properties title if this is the current overlay
        if (UIManager::Get()->GetOverlayPropertiesWindow().GetActiveOverlayID() == overlay_id)
        {
            UIManager::Get()->GetOverlayPropertiesWindow().SetActiveOverlayID(overlay_id, true);
        }
    }

    ImGui::PopID();
}

void WindowFloatingUIActionBar::UpdateActionButtons(unsigned int overlay_id)
{
    ImGui::PushID("ActionButtons");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 b_size, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
    //Default button size for custom actions to be the same as the settings icon so the user is able to provide oversized images without messing up the layout
    //as well as still providing a way to change the size of text buttons by editing the settings icon's dimensions
    ImVec2 b_size_default = b_size;

    OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
    auto& custom_actions = ConfigManager::Get().GetCustomActions();
    auto& action_order = (data.ConfigBool[configid_bool_overlay_actionbar_order_use_global]) ? ConfigManager::Get().GetActionMainBarOrder() : data.ConfigActionBarOrder;
    int list_id = 0;
    for (auto& order_data : action_order)
    {
        if (order_data.visible)
        {
            ImGui::PushID(list_id);

            if (order_data.action_id < action_built_in_MAX) //Built-in action
            {
                bool has_icon = true;

                //Button action is always the same but we want to use icons if available
                switch (order_data.action_id)
                {
                    case action_show_keyboard: TextureManager::Get().GetTextureInfo(tmtex_icon_keyboard, b_size, b_uv_min, b_uv_max); break;                     
                    default:                   has_icon = false;
                }

                if (has_icon)
                {
                    if (ImGui::ImageButton(io.Fonts->TexID, b_size_default, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)overlay_id);
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_do, order_data.action_id);
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
                    }
                    DisplayTooltipIfHovered(ActionManager::Get().GetActionName(order_data.action_id));
                }
                else
                {
                    if (ImGui::ButtonWithWrappedLabel(ActionManager::Get().GetActionButtonLabel(order_data.action_id), b_size_default))
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)overlay_id);
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_do, order_data.action_id);
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
                    }
                }
            }
            else if (order_data.action_id >= action_custom) //Custom action
            {
                const CustomAction& custom_action = custom_actions[order_data.action_id - action_custom];
                if (custom_action.IconImGuiRectID != -1)
                {
                    TextureManager::Get().GetTextureInfo(custom_action, b_size, b_uv_min, b_uv_max);
                    if (ImGui::ImageButton(io.Fonts->TexID, b_size_default, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
                    {
                        if (custom_action.FunctionType != caction_press_keys)   //Press and release of action keys is handled below instead
                        {
                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_do, order_data.action_id);
                        }
                    }
                }
                else
                {
                    if (ImGui::ButtonWithWrappedLabel(ActionManager::Get().GetActionName(order_data.action_id), b_size_default))
                    {
                        if (custom_action.FunctionType != caction_press_keys)
                        {
                            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_do, order_data.action_id);
                        }
                    }
                }

                //Enable press and release of action keys based on button press
                if (custom_action.FunctionType == caction_press_keys)
                {
                    if (ImGui::IsItemActivated())
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_start, order_data.action_id);
                    }

                    if (ImGui::IsItemDeactivated())
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_stop, order_data.action_id);
                    }
                }

                //Only display tooltip if the label isn't already on the button (is here since we need ImGui::IsItem*() to work on the button)
                if (custom_action.IconImGuiRectID != -1)
                {
                    DisplayTooltipIfHovered(ActionManager::Get().GetActionName(order_data.action_id));
                }
            }
            ImGui::SameLine();

            ImGui::PopID();
        }
        list_id++;
    }

    ImGui::PopStyleColor(); //ImGuiCol_ChildBg
    ImGui::PopID();
}

WindowFloatingUIActionBar::WindowFloatingUIActionBar() : m_Visible(false), m_Alpha(0.0f)
{
    m_Size.x = 32.0f;
}

void WindowFloatingUIActionBar::Show(bool skip_fade)
{
    m_Visible = true;

    if (skip_fade)
    {
        m_Alpha = 1.0f;
    }
}

void WindowFloatingUIActionBar::Hide(bool skip_fade)
{
    m_Visible = false;

    if (skip_fade)
    {
        m_Alpha = 0.0f;
    }
}

void WindowFloatingUIActionBar::Update(unsigned int overlay_id)
{
    if ( (m_Alpha != 0.0f) || (m_Visible) )
    {
        //Alpha fade animation
        m_Alpha += (m_Visible) ? 0.1f : -0.1f;

        if (m_Alpha > 1.0f)
            m_Alpha = 1.0f;
        else if (m_Alpha < 0.0f)
            m_Alpha = 0.0f;
    }

    //We need to not skip on alpha 0.0 at least twice to get the real height of the bar. 32.0f is the placeholder width ImGui seems to use until then
    if ( (m_Alpha == 0.0f) && (m_Size.x != 32.0f) )
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Alpha);

    ImGuiIO& io = ImGui::GetIO();
    const ImGuiStyle& style = ImGui::GetStyle();

    ImVec2 b_size, b_uv_min, b_uv_max;

    //Put window near the top of the overlay with space for the tooltips + padding
    const DPRect& rect_floating_ui = UITextureSpaces::Get().GetRect(ui_texspace_floating_ui);
    const float tooltip_height = ImGui::GetFontSize() + (style.WindowPadding.y * 2.0f);
    const float offset_y = rect_floating_ui.GetTL().y + tooltip_height + (style.FramePadding.y * 2.0f) + style.WindowPadding.y;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2.0f, offset_y), 0, ImVec2(0.5f, 0.0f));

    ImGui::Begin("WindowFloatingUIActionBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
                                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    if (OverlayManager::Get().GetConfigData(overlay_id).ConfigBool[configid_bool_overlay_floatingui_desktops_enabled])
    {
        UpdateDesktopButtons(overlay_id);
    }

    UpdateActionButtons(overlay_id);

    ImGui::PopStyleColor(); //ImGuiCol_Button
    ImGui::PopStyleVar();   //ImGuiStyleVar_FrameRounding

    m_Pos  = ImGui::GetWindowPos();
    m_Size = ImGui::GetWindowSize();

    ImGui::End();
    ImGui::PopStyleVar(); //ImGuiStyleVar_Alpha
}

const ImVec2& WindowFloatingUIActionBar::GetPos() const
{
    return m_Pos;
}

const ImVec2& WindowFloatingUIActionBar::GetSize() const
{
    return m_Size;
}

bool WindowFloatingUIActionBar::IsVisible() const
{
    return m_Visible;
}

float WindowFloatingUIActionBar::GetAlpha() const
{
    return m_Alpha;
}
