#include "WindowFloatingUIBar.h"

#include "ImGuiExt.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "OpenVRExt.h"
#include "UIManager.h"
#include "OverlayManager.h"
#include "WindowManager.h"
#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"

//-WindowFloatingUIMainBar
WindowFloatingUIMainBar::WindowFloatingUIMainBar() : m_IsCurrentWindowCapturable(-1), m_AnimationProgress(0.0f)
{

}

void WindowFloatingUIMainBar::DisplayTooltipIfHovered(const char* text)
{
    if ( ((m_AnimationProgress == 0.0f) || (m_AnimationProgress == 1.0f)) && (ImGui::IsItemHovered()) ) //Also hide while animating
    {
        const ImGuiStyle& style = ImGui::GetStyle();

        static ImVec2 button_pos_last; //Remember last position and use it when posible. This avoids flicker when the same tooltip string is used in different places
        ImVec2 pos = ImGui::GetItemRectMin();
        float button_width = ImGui::GetItemRectSize().x;

        //Default tooltips are not suited for this as there's too much trouble with resize flickering and stuff
        ImGui::Begin(text, nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

        ImGui::TextUnformatted(text);

        //Not using GetWindowSize() here since it's delayed and plays odd when switching between buttons with the same label
        ImVec2 window_size = ImGui::GetItemRectSize();
        window_size.x += style.WindowPadding.x * 2.0f;
        window_size.y += style.WindowPadding.y * 2.0f;

        //Repeat frame when the window is appearing as it will not have the right position (either from being first time or still having old pos)
        if ( (ImGui::IsWindowAppearing()) || (pos.x != button_pos_last.x) )
        {
            UIManager::Get()->RepeatFrame();
        }

        button_pos_last = pos;

        pos.x += (button_width / 2.0f) - (window_size.x / 2.0f);
        pos.y += button_width + style.WindowPadding.y;

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
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_close, b_size, b_uv_min, b_uv_max);
    const ImGuiStyle& style = ImGui::GetStyle();

    //Put window near the bottom of the overlay with space for the tooltips + padding (touching action-bar when visible)
    const float offset_base = ImGui::GetFontSize() + style.FramePadding.y + (style.WindowPadding.y * 2.0f) + 3.0f;
    const float offset_y = smoothstep(m_AnimationProgress, offset_base + actionbar_height /* Action-Bar not visible */, offset_base /* Action-Bar visible */);

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2.0f, io.DisplaySize.y - offset_y), 0, ImVec2(0.5f, 1.0f));
    ImGui::Begin("WindowFloatingUIMainBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
                                                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {style.ItemSpacing.y, style.ItemSpacing.y});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    //Show Action-Bar Button (this is a per-overlay state)
    bool& is_actionbar_enabled = overlay_data.ConfigBool[configid_bool_overlay_actionbar_enabled];
    bool actionbar_was_enabled = is_actionbar_enabled;

    if (actionbar_was_enabled)
        ImGui::PushStyleColor(ImGuiCol_Button, Style_ImGuiCol_ButtonPassiveToggled);

    TextureManager::Get().GetTextureInfo(tmtex_icon_small_actionbar, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton("ToggleActionBar", io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        is_actionbar_enabled = !is_actionbar_enabled;
        //This is an UI state so no need to sync
    }

    if (actionbar_was_enabled)
        ImGui::PopStyleColor();

    DisplayTooltipIfHovered(TranslationManager::GetString( (actionbar_was_enabled) ? tstr_FloatingUIActionBarHideTip : tstr_FloatingUIActionBarShowTip ));
    //

    ImGui::SameLine();

    //Extra buttons
    if (overlay_data.ConfigBool[configid_bool_overlay_floatingui_extras_enabled])
    {
        //Add current window as overlay (only show if desktop duplication or non-window WinRT capture)
        if (  (overlay_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication) ||
             ((overlay_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && (overlay_data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] == 0)) )
        {
            //If marked to need update, refresh actual state
            if (m_IsCurrentWindowCapturable == -1)
            {
                m_IsCurrentWindowCapturable = (WindowManager::Get().WindowListFindWindow(::GetForegroundWindow()) != nullptr);
            }

            if (m_IsCurrentWindowCapturable != 1)
                ImGui::PushItemDisabled();

            TextureManager::Get().GetTextureInfo(tmtex_icon_small_add_window, b_size, b_uv_min, b_uv_max);
            ImGui::ImageButton("AddWindow", io.Fonts->TexID, b_size, b_uv_min, b_uv_max); //This one's activated on mouse down

            if (ImGui::IsItemActivated())
            {
                vr::TrackedDeviceIndex_t device_index = ConfigManager::Get().GetPrimaryLaserPointerDevice();

                //If no dashboard device, try finding one
                if (device_index == vr::k_unTrackedDeviceIndexInvalid)
                {
                    device_index = vr::IVROverlayEx::FindPointerDeviceForOverlay(UIManager::Get()->GetOverlayHandleFloatingUI());
                }

                //Try to get the pointer distance
                float source_distance = 1.0f;
                vr::VROverlayIntersectionResults_t results;

                if (vr::IVROverlayEx::ComputeOverlayIntersectionForDevice(UIManager::Get()->GetOverlayHandleFloatingUI(), device_index, vr::TrackingUniverseStanding, &results))
                {
                    source_distance = results.fDistance;
                }

                //Set pointer hint in case dashboard app needs it
                ConfigManager::SetValue(configid_int_state_laser_pointer_device_hint, (int)device_index);
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_laser_pointer_device_hint, (int)device_index);

                //Add overlay
                HWND current_window = ::GetForegroundWindow();
                OverlayManager::Get().AddOverlay(ovrl_capsource_winrt_capture, -2, current_window);

                //Send to dashboard app
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_handle_state_arg_hwnd, (LPARAM)current_window);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_new_drag, MAKELPARAM(-2, (source_distance * 100.0f)));
            }

            if (m_IsCurrentWindowCapturable != 1)
                ImGui::PopItemDisabled();

            DisplayTooltipIfHovered(TranslationManager::GetString(tstr_FloatingUIWindowAddTip));

            ImGui::SameLine();
        }
        //

        if (overlay_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui) //Performance Monitor reset button (only show if UI overlay)
        {
            UpdatePerformanceMonitorButtons();
        }
        else if (overlay_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser) //Browser navigation/reload buttons (only show if browser overlay)
        {
            UpdateBrowserButtons(overlay_id);
        }
    }
    //

    //Drag-Mode Toggle Button (this is a global state)
    bool& is_dragmode_enabled = ConfigManager::GetRef(configid_bool_state_overlay_dragmode);
    bool dragmode_was_enabled = is_dragmode_enabled;
    bool& is_overlay_transform_locked = overlay_data.ConfigBool[configid_bool_overlay_transform_locked];

    if (dragmode_was_enabled)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    TextureManager::Get().GetTextureInfo((is_overlay_transform_locked) ? tmtex_icon_small_move_locked : tmtex_icon_small_move, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton("DragMode", io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        if (io.MouseDownDurationPrev[ImGuiMouseButton_Left] < 1.5f) //Don't do normal button behavior after lock toggle was triggered
        {
            is_dragmode_enabled = !is_dragmode_enabled;
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_state_overlay_dragselectmode_show_hidden, false);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_state_overlay_dragmode, is_dragmode_enabled);

            //Update temporary standing position if dragmode has been activated and dashboard tab isn't active
            if ((is_dragmode_enabled) && (!UIManager::Get()->IsOverlayBarOverlayVisible()))
            {
                UIManager::Get()->GetOverlayDragger().UpdateTempStandingPosition();
            }
        }
    }

    //Toggle transform lock when holding for 1.5 seconds
    bool show_hold_message_lock = false;

    if (ImGui::IsItemActive())
    {
        if (io.MouseDownDuration[ImGuiMouseButton_Left] > 1.5f)
        {
            is_overlay_transform_locked = !is_overlay_transform_locked;

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, (int)overlay_id);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_overlay_transform_locked, is_overlay_transform_locked);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

            io.MouseDown[ImGuiMouseButton_Left] = false;    //Release mouse button so actual button won't get toggled
        }
        else if (io.MouseDownDuration[ImGuiMouseButton_Left] > 0.5f)
        {
            show_hold_message_lock = true;
        }
    }

    if (dragmode_was_enabled)
        ImGui::PopStyleColor();

    if (show_hold_message_lock)
    {
        DisplayTooltipIfHovered(TranslationManager::GetString((is_overlay_transform_locked) ? tstr_FloatingUIDragModeHoldUnlockTip: tstr_FloatingUIDragModeHoldLockTip));
    }
    else
    {
        DisplayTooltipIfHovered(TranslationManager::GetString((dragmode_was_enabled) ? tstr_FloatingUIDragModeDisableTip : tstr_FloatingUIDragModeEnableTip));
    }
    //

    ImGui::SameLine();

    //Close/Disable Button
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_close, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton("Close", io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        if (io.MouseDownDurationPrev[ImGuiMouseButton_Left] < 2.5f) //Don't do normal button behavior after lock toggle was triggered
        {
            overlay_data.ConfigBool[configid_bool_overlay_enabled] = false;

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, (int)overlay_id);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_overlay_enabled, false);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);
        }
    }

    //Toggle transform lock when holding for 2.5 seconds
    bool show_hold_message_remove = false;

    if (ImGui::IsItemActive())
    {
        if (io.MouseDownDuration[ImGuiMouseButton_Left] > 2.5f) //Longer delay compared to other holds because this is pretty destructive (but still less than rare transform resets)
        {
            OverlayManager::Get().RemoveOverlay(overlay_id);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_remove, overlay_id);

            //Hide properties window if it's open for this overlay
            WindowOverlayProperties& properties_window = UIManager::Get()->GetOverlayPropertiesWindow();

            if (properties_window.GetActiveOverlayID() == overlay_id)
            {
                properties_window.SetActiveOverlayID(k_ulOverlayID_None, true);
                properties_window.HideAll();
            }
            else if (properties_window.GetActiveOverlayID() > overlay_id) //Adjust properties window active overlay ID if it's open for an overlay that had its ID shifted
            {
                properties_window.SetActiveOverlayID(properties_window.GetActiveOverlayID() - 1, true);
            }

            io.MouseDown[ImGuiMouseButton_Left] = false;    //Release mouse button so actual button won't get toggled
        }
        else if (io.MouseDownDuration[ImGuiMouseButton_Left] > 0.5f)
        {
            show_hold_message_remove = true;
        }
    }

    if (show_hold_message_remove)
    {
        DisplayTooltipIfHovered(TranslationManager::GetString(tstr_FloatingUIHideOverlayHoldTip));
    }
    else
    {
        DisplayTooltipIfHovered(TranslationManager::GetString(tstr_FloatingUIHideOverlayTip));
    }
    //

    ImGui::PopStyleColor(); //ImGuiCol_Button
    ImGui::PopStyleVar();   //ImGuiStyleVar_ItemSpacing
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

void WindowFloatingUIMainBar::UpdatePerformanceMonitorButtons()
{
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 b_size, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_close, b_size, b_uv_min, b_uv_max);
    const ImGuiStyle& style = ImGui::GetStyle();

    //Reset Cumulative Values Button
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_performance_monitor_reset, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton("Perfmon Reset", io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        UIManager::Get()->GetPerformanceWindow().ResetCumulativeValues();
    }

    DisplayTooltipIfHovered(TranslationManager::GetString(tstr_OvrlPropsPerfMonResetValues));

    ImGui::SameLine();
}

void WindowFloatingUIMainBar::UpdateBrowserButtons(unsigned int overlay_id)
{
    //Use overlay data of duplication ID if one is set
    int duplication_id = OverlayManager::Get().GetConfigData(overlay_id).ConfigInt[configid_int_overlay_duplication_id];
    OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData((duplication_id == -1) ? overlay_id : (unsigned int)duplication_id);

    ImGuiIO& io = ImGui::GetIO();

    ImVec2 b_size, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_close, b_size, b_uv_min, b_uv_max);
    const ImGuiStyle& style = ImGui::GetStyle();

    //Go Back Button
    if (!overlay_data.ConfigBool[configid_bool_overlay_state_browser_nav_can_go_back])
        ImGui::PushItemDisabled();

    TextureManager::Get().GetTextureInfo(tmtex_icon_small_browser_back, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton("GoBack", io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        DPBrowserAPIClient::Get().DPBrowser_GoBack(overlay_data.ConfigHandle[configid_handle_overlay_state_overlay_handle]);
    }

    if (!overlay_data.ConfigBool[configid_bool_overlay_state_browser_nav_can_go_back])
        ImGui::PopItemDisabled();

    DisplayTooltipIfHovered(TranslationManager::GetString(tstr_FloatingUIBrowserGoBackTip));
    //

    ImGui::SameLine();

    //Go Forward Button
    if (!overlay_data.ConfigBool[configid_bool_overlay_state_browser_nav_can_go_forward])
        ImGui::PushItemDisabled();

    TextureManager::Get().GetTextureInfo(tmtex_icon_small_browser_forward, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton("GoForward", io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        DPBrowserAPIClient::Get().DPBrowser_GoForward(overlay_data.ConfigHandle[configid_handle_overlay_state_overlay_handle]);
    }

    if (!overlay_data.ConfigBool[configid_bool_overlay_state_browser_nav_can_go_forward])
        ImGui::PopItemDisabled();

    DisplayTooltipIfHovered(TranslationManager::GetString(tstr_FloatingUIBrowserGoForwardTip));
    //

    ImGui::SameLine();

    //Refresh Button
    const bool is_loading = overlay_data.ConfigBool[configid_bool_overlay_state_browser_nav_is_loading];
    TextureManager::Get().GetTextureInfo((is_loading) ? tmtex_icon_small_browser_stop : tmtex_icon_small_browser_refresh, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton("Refresh", io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        DPBrowserAPIClient::Get().DPBrowser_Refresh(overlay_data.ConfigHandle[configid_handle_overlay_state_overlay_handle]);
    }

    DisplayTooltipIfHovered(TranslationManager::GetString((is_loading) ? tstr_FloatingUIBrowserStopTip : tstr_FloatingUIBrowserRefreshTip));
    //

    ImGui::SameLine();
}

const ImVec2& WindowFloatingUIMainBar::GetPos() const
{
    return m_Pos;
}

const ImVec2& WindowFloatingUIMainBar::GetSize() const
{
    return m_Size;
}

float WindowFloatingUIMainBar::GetAnimationProgress() const
{
    return m_AnimationProgress;
}

void WindowFloatingUIMainBar::MarkCurrentWindowCapturableStateOutdated()
{
    //Mark state as outdated. We don't do the update here as the current window can change a lot while the UI isn't even displaying... no need to bother then.
    m_IsCurrentWindowCapturable = -1;

    if (UIManager::Get()->GetFloatingUI().IsVisible())
    {
        UIManager::Get()->GetIdleState().AddActiveTime(50);
    }
}


//-WindowFloatingUIActionBar
WindowFloatingUIActionBar::WindowFloatingUIActionBar() : m_Visible(false), m_Alpha(0.0f), m_LastDesktopSwitchTime(0.0), m_TooltipPositionForOverlayBar(false)
{
    m_Size.x = 32.0f;
}

void WindowFloatingUIActionBar::DisplayTooltipIfHovered(const char* text)
{
    if (ImGui::IsItemHovered())
    {
        const ImGuiStyle& style = ImGui::GetStyle();

        static ImVec2 button_pos_last; //Remember last position and use it when posible. This avoids flicker when the same tooltip string is used in different places
        ImVec2 pos = ImGui::GetItemRectMin();
        float button_width = ImGui::GetItemRectSize().x;

        //Default tooltips are not suited for this as there's too much trouble with resize flickering and stuff
        ImGui::Begin(text, nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

        ImGui::TextUnformatted(text);

        //Not using GetWindowSize() here since it's delayed and plays odd when switching between buttons with the same label
        ImVec2 window_size = ImGui::GetItemRectSize();
        window_size.x += style.WindowPadding.x * 2.0f;
        window_size.y += style.WindowPadding.y * 2.0f;

        //Repeat frame when the window is appearing as it will not have the right position (either from being first time or still having old pos)
        if ( (ImGui::IsWindowAppearing()) || (pos.x != button_pos_last.x) )
        {
            UIManager::Get()->RepeatFrame();
        }

        button_pos_last = pos;

        pos.x += (button_width / 2.0f) - (window_size.x / 2.0f);

        if (m_TooltipPositionForOverlayBar)
        {
            pos.y = ImGui::GetIO().DisplaySize.y;
            pos.y -= window_size.y;
        }
        else
        {
            pos.y -= window_size.y + style.WindowPadding.y + 2.0f;
        }

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

    if (ConfigManager::GetValue(configid_bool_interface_desktop_buttons_include_combined))
    {
        if (disable_combined)
            ImGui::PushItemDisabled();

        if (current_desktop == -1)
            ImGui::PushStyleColor(ImGuiCol_Button, Style_ImGuiCol_ButtonPassiveToggled);

        TextureManager::Get().GetTextureInfo(tmtex_icon_desktop_all, b_size, b_uv_min, b_uv_max);
        if (ImGui::ImageButton("Combined Desktop", io.Fonts->TexID, b_size, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
        {
            //Don't change to same value to avoid flicker from mirror reset
            if ( (current_desktop != -1) || (!ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring)) )
            {
                current_desktop_new = -1;
            }
        }
        DisplayTooltipIfHovered(TranslationManager::GetString(tstr_SourceDesktopAll));
        ImGui::SameLine();

        if (current_desktop == -1)
            ImGui::PopStyleColor();

        if (disable_combined)
            ImGui::PopItemDisabled();
    }

    int desktop_count = ConfigManager::GetValue(configid_int_state_interface_desktop_count);

    if (disable_normal)
        ImGui::PushItemDisabled();

    switch (ConfigManager::GetValue(configid_int_interface_desktop_listing_style))
    {
        case desktop_listing_style_individual:
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

                if (ImGui::ImageButton("", io.Fonts->TexID, b_size, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
                {
                    //Don't change to same value to avoid flicker from mirror reset
                    if ( (i != current_desktop) || (!ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring)) )
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
        case desktop_listing_style_cycle:
        {
            TextureManager::Get().GetTextureInfo(tmtex_icon_desktop_prev, b_size, b_uv_min, b_uv_max);
            if (ImGui::ImageButton("Previous Desktop", io.Fonts->TexID, b_size, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
            {
                current_desktop_new--;

                if (current_desktop_new <= -1)
                    current_desktop_new = desktop_count - 1;
            }
            DisplayTooltipIfHovered(TranslationManager::GetString(tstr_FloatingUIActionBarDesktopPrev));
            ImGui::SameLine();

            TextureManager::Get().GetTextureInfo(tmtex_icon_desktop_next, b_size, b_uv_min, b_uv_max);
            if (ImGui::ImageButton("Next Destkop", io.Fonts->TexID, b_size, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
            {
                current_desktop_new++;

                if (current_desktop_new == desktop_count)
                    current_desktop_new = 0;
            }
            DisplayTooltipIfHovered(TranslationManager::GetString(tstr_FloatingUIActionBarDesktopNext));
            ImGui::SameLine();
            break;
        }
    }

    if (disable_normal)
        ImGui::PopItemDisabled();

    if (current_desktop_new != current_desktop)
    {
        current_desktop = current_desktop_new;

        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, (int)overlay_id);

        //Reset window selection when switching to a desktop
        if (current_configid == configid_int_overlay_winrt_desktop_id)
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_handle_overlay_state_winrt_hwnd, 0);
            overlay_config.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] = 0;
        }

        IPCManager::Get().PostConfigMessageToDashboardApp(current_configid, current_desktop);

        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

        //Update overlay name
        OverlayManager::Get().SetOverlayNameAuto(overlay_id);
        UIManager::Get()->OnOverlayNameChanged();

        //Store last switch time for the anti-flicker code (only needed for dashboard origin)
        if (overlay_config.ConfigInt[configid_int_overlay_origin] == ovrl_origin_dashboard)
        {
            m_LastDesktopSwitchTime = ImGui::GetTime();
        }
    }

    ImGui::PopID();
}

void WindowFloatingUIActionBar::ButtonActionKeyboard(const Action& action, const ImVec2& size, unsigned int overlay_id)
{
    FloatingWindow& keyboard_window = UIManager::Get()->GetVRKeyboard().GetWindow();
    const ActionManager& action_manager = ConfigManager::Get().GetActionManager();
    ImGuiIO& io = ImGui::GetIO();
    const bool is_keyboard_visible = keyboard_window.IsVisible();

    if (is_keyboard_visible)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    if (ButtonAction(action, size))
    {
        if (io.MouseDownDurationPrev[ImGuiMouseButton_Left] < 3.0f) //Don't do normal button behavior after reset was just triggered
        {
            action_manager.DoAction(action.UID);    //We only support DoAction here since we want holding down the button to not trigger the action already
        }
    }

    if (is_keyboard_visible)
        ImGui::PopStyleColor();

    //Reset tranform when holding the button for 3 or more seconds
    TRMGRStrID tooltip_strid = (is_keyboard_visible) ? tstr_ActionKeyboardHide : tstr_ActionKeyboardShow;

    if (ImGui::IsItemActive())
    {
        if (io.MouseDownDuration[ImGuiMouseButton_Left] > 3.0f)
        {
            //Unpin if dashboard overlay is available
            if ( (UIManager::Get()->IsOpenVRLoaded()) && (vr::VROverlay()->IsOverlayVisible(UIManager::Get()->GetOverlayHandleDPlusDashboard())) )
            {
                keyboard_window.SetPinned(false);
            }

            keyboard_window.ResetTransformAll();
            io.MouseDown[ImGuiMouseButton_Left] = false;    //Release mouse button so transform changes don't get blocked
        }
        else if (io.MouseDownDuration[ImGuiMouseButton_Left] > 0.5f)
        {
            tooltip_strid = tstr_OverlayBarTooltipResetHold;
        }
    }

    //Show tooltip depending on current keyboard state
    DisplayTooltipIfHovered( TranslationManager::GetString(tooltip_strid) );
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
        const float alpha_step = ImGui::GetIO().DeltaTime * 6.0f;

        //Alpha fade animation
        m_Alpha += (m_Visible) ? alpha_step : -alpha_step;

        if (m_Alpha > 1.0f)
            m_Alpha = 1.0f;
        else if (m_Alpha < 0.0f)
            m_Alpha = 0.0f;
    }

    //We need to not skip on alpha 0.0 at least twice to get the real height of the bar. 32.0f (and sometimes 16.0f) is the placeholder width ImGui seems to use until then
    if ( (m_Alpha == 0.0f) && (m_Size.x > 32.0f) )
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
                                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    //Set focused ID when clicking anywhere on the window
    if ((ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) && (ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
    {
        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_focused_id, (int)overlay_id);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_focused_id,        (int)overlay_id);  //Sending to self to trigger change behavior
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {style.ItemSpacing.y, style.ItemSpacing.y});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    const ImVec2 cursor_pos = ImGui::GetCursorPos();

    if (OverlayManager::Get().GetConfigData(overlay_id).ConfigBool[configid_bool_overlay_floatingui_desktops_enabled])
    {
        UpdateDesktopButtons(overlay_id);
    }

    UpdateActionButtons(overlay_id);

    //Check if there any buttons added by above functions and if not, display placeholder text so the window doesn't look weird
    const ImVec2 cursor_pos_orig = ImGui::GetCursorPos();
    if (cursor_pos_orig.x == cursor_pos.x)
    {
        //Match height of what a normal button would take up and middle-align the text
        ImVec2 b_size, b_uv_min, b_uv_max;
        TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
        b_size.x += style.FramePadding.x * 2.0f;
        b_size.y += style.FramePadding.y * 2.0f;

        ImGui::Dummy({0.0f, b_size.y});
        ImGui::SetCursorPosY(cursor_pos.y + (b_size.y / 2.0f) - (ImGui::GetFontSize() / 2.0f));
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_FloatingUIActionBarEmpty));
    }

    ImGui::PopStyleColor(); //ImGuiCol_Button
    ImGui::PopStyleVar();   //ImGuiStyleVar_ItemSpacing
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

double WindowFloatingUIActionBar::GetLastDesktopSwitchTime() const
{
    return m_LastDesktopSwitchTime;
}

void WindowFloatingUIActionBar::UpdateActionButtons(unsigned int overlay_id)
{
    //This function can be called by WindowOverlayBar to optionally put some action buttons in there too
    //It's a little bit hacky, but the only real difference is where to pull the action order from and how to display the tooltip
    bool is_overlay_bar = (overlay_id == k_ulOverlayID_None);
    m_TooltipPositionForOverlayBar = is_overlay_bar;

    ImGui::PushID("ActionButtons");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 b_size, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
    //Default button size for custom actions to be the same as the settings icon so the user is able to provide oversized images without messing up the layout
    //as well as still providing a way to change the size of text buttons by editing the settings icon's dimensions
    ImVec2 b_size_default = b_size;

    const ActionManager& action_manager = ConfigManager::Get().GetActionManager();
    OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
    const auto& action_order = (is_overlay_bar) ? action_manager.GetActionOrderListOverlayBar() :
                               (data.ConfigBool[configid_bool_overlay_actionbar_order_use_global]) ? action_manager.GetActionOrderListBarDefault() : data.ConfigActionBarOrder;

    int list_id = 0;
    for (ActionUID uid : action_order)
    {
        const Action action = action_manager.GetAction(uid);

        //Detect normal Show Keyboard action by translation ID and provide special behavior
        if (action.NameTranslationID == tstr_DefActionShowKeyboard)
        {
            ButtonActionKeyboard(action, b_size, overlay_id);
        }
        else    //Every other action
        {
            ButtonAction(action, b_size);

            if (ImGui::IsItemActivated())
            {
                action_manager.StartAction(uid);
            }
            else if (ImGui::IsItemDeactivated())
            {
                action_manager.StopAction(uid);
            }

            //Only display tooltip if the label isn't already on the button (is here since we need ImGui::IsItem*() to work on the button)
            if (action.Label.empty())
            {
                DisplayTooltipIfHovered(action_manager.GetTranslatedName(uid));
            }
        }

        ImGui::SameLine();
    }

    ImGui::PopStyleColor(); //ImGuiCol_ChildBg
    ImGui::PopID();

    m_TooltipPositionForOverlayBar = false;
}

bool WindowFloatingUIActionBar::ButtonAction(const Action& action, const ImVec2& size, bool use_temp_icon)
{
    bool ret = false;

    ImGui::PushID((void*)action.UID);
    ImGui::BeginGroup();

    const ImGuiStyle& style = ImGui::GetStyle();

    if ((action.IconImGuiRectID != -1) || ((use_temp_icon) && (!action.IconFilename.empty())))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 b_size, b_uv_min, b_uv_max;

        if (use_temp_icon)
        {
            TextureManager::Get().GetTextureInfo(tmtex_icon_temp, b_size, b_uv_min, b_uv_max);
        }
        else
        {
            TextureManager::Get().GetTextureInfo(action, b_size, b_uv_min, b_uv_max);
        }

        ret = ImGui::ImageButton("##ActionButtonImg", io.Fonts->TexID, size, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    }
    else
    {
        ret = ImGui::Button("##ActionButton", ImVec2(size.x + (style.FramePadding.x * 2.0f), size.y + (style.FramePadding.y * 2.0f)) );
    }

    if (!action.Label.empty())
    {
        ImGui::SameLine(0.0f, 0.0f);

        const char* label = (action.LabelTranslationID == tstr_NONE) ? action.Label.c_str() : TranslationManager::GetString(action.LabelTranslationID);
        const char* label_end = label + strlen(label);
        const char* line_start = label;
        const char* line_end = nullptr;
        const ImVec2 label_size = ImGui::CalcTextSize(label, label_end);
        const ImU32 col = ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));

        const float pos_x = ImGui::GetCursorScreenPos().x - size.x - style.FramePadding.x;
        float pos_y = ImGui::GetCursorScreenPos().y + int( (size.y / 2.0f) - (label_size.y / 2.0f) );

        while (line_start < label_end)
        {
            line_end = (const char*)memchr(line_start, '\n', label_end - line_start);

            if (line_end == nullptr)
                line_end = label_end;

            ImVec2 line_size = ImGui::CalcTextSize(line_start, line_end);
            float line_xscale = (size.x + style.FramePadding.x / 2.0f) / line_size.x;

            if (line_xscale > 1.0f)
                line_xscale = 1.0f;

            ImGui::BeginStretched();
            ImGui::GetWindowDrawList()->AddText({pos_x + int( ((size.x / 2.0f) - (line_size.x * line_xscale) / 2.0f) ), pos_y}, col, line_start, line_end);
            ImGui::EndStretched(line_xscale);

            line_start = line_end + 1;
            pos_y += ImGui::GetTextLineHeight();
        }
    }

    ImGui::EndGroup();
    ImGui::PopID();

    return ret;
}


//-WindowFloatingUIOverlayStats
ImVec2 WindowFloatingUIOverlayStats::CalcPos(const WindowFloatingUIMainBar& mainbar, const WindowFloatingUIActionBar& actionbar, float& window_width) const
{
    const ImGuiStyle& style = ImGui::GetStyle();

    //Limit width to what we have left in the texture space
    const float max_width = (mainbar.GetPos().x - UITextureSpaces::Get().GetRect(ui_texspace_floating_ui).GetBL().x) - style.ItemSpacing.x;
    window_width = std::min(window_width, max_width);

    //Position the window left to either action- or main-bar, depending on visibility of action-bar (switching is animated alongside mainbar)
    //Additionally moves the window down if the action-bar occupies the horizontal space needed
    const ImVec2& actionbar_pos  = actionbar.GetPos();
    const ImVec2& actionbar_size = actionbar.GetSize();
    const ImVec2& mainbar_pos    = mainbar.GetPos();
    const bool is_actionbar_blocking = (actionbar_pos.x < window_width + style.ItemSpacing.x);

    float max_x = actionbar_pos.x;

    if (is_actionbar_blocking)
    {
        const float space_width = mainbar.GetPos().x - actionbar_pos.x + style.ItemSpacing.x;  //Space between left side of action-bar and left side of main-bar

        if (window_width > space_width)
        {
            max_x = mainbar_pos.x;
        }
        else
        {
            max_x = actionbar_pos.x + window_width + style.ItemSpacing.x;
        }
    }

    ImVec2 pos;
    pos.x  = smoothstep(mainbar.GetAnimationProgress(), mainbar.GetPos().x, max_x);
    pos.x -= style.ItemSpacing.x;
    pos.y  = smoothstep(mainbar.GetAnimationProgress(), actionbar_pos.y, (is_actionbar_blocking) ? actionbar_pos.y + actionbar_size.y + style.ItemSpacing.y : actionbar_pos.y);

    return pos;
}

void WindowFloatingUIOverlayStats::Update(const WindowFloatingUIMainBar& mainbar, const WindowFloatingUIActionBar& actionbar, unsigned int overlay_id)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing;

    const OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData(overlay_id);
    const bool show_fps = !overlay_data.ConfigBool[configid_bool_overlay_state_no_output];

    if (show_fps)
    {
        //Don't display if disabled
        if (!ConfigManager::Get().GetValue(configid_bool_performance_show_fps))
            return;

        //Use overlay data of duplication ID if one is set
        int duplication_id = OverlayManager::Get().GetConfigData(overlay_id).ConfigInt[configid_int_overlay_duplication_id];
        const OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData((duplication_id == -1) ? overlay_id : (unsigned int)duplication_id);

        int fps = -1;
        if (overlay_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication)
        {
            fps = ConfigManager::GetValue(configid_int_state_performance_duplication_fps);
        }
        else if (overlay_data.ConfigInt[configid_int_overlay_capture_source] != ovrl_capsource_ui)
        {
            fps = overlay_data.ConfigInt[configid_int_overlay_state_fps];
        }

        //Don't display window if no fps value available
        if (fps == -1)
            return;

        //This is a fixed size window, which seems like a bad idea for localization, but since it uses performance monitor strings it already uses size constrained text
        //(and "FPS" will be untranslated in practice)
        float window_width = ImGui::GetFontSize() * 3.5f;
        ImVec2 pos = CalcPos(mainbar, actionbar, window_width);

        //Actual window
        ImGui::SetNextWindowPos(pos, 0, ImVec2(1.0f, 0.0f));    
        ImGui::SetNextWindowSize(ImVec2(window_width, -1.0f));
        ImGui::Begin("WindowFloatingUIStats", nullptr, window_flags);

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorFPS));
        ImGui::SameLine();
        ImGui::TextRight(0.0f, "%d", fps);

        ImGui::End();
    }
    else  //Show name
    {
        float window_width = ImGui::CalcTextSize(overlay_data.ConfigNameStr.c_str()).x + style.ItemSpacing.x + style.ItemSpacing.x;
        ImVec2 pos = CalcPos(mainbar, actionbar, window_width);

        //Actual window
        ImGui::SetNextWindowPos(pos, 0, ImVec2(1.0f, 0.0f));    
        ImGui::SetNextWindowSize(ImVec2(window_width, -1.0f));
        ImGui::Begin("WindowFloatingUIStats", nullptr, window_flags);

        ImGui::TextUnformatted(overlay_data.ConfigNameStr.c_str());

        ImGui::End();
    }
}
