#include "WindowOverlayBar.h"

#include "ImGuiExt.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "UIManager.h"
#include "OverlayManager.h"
#include "DesktopPlusWinRT.h"


WindowOverlayBar::WindowOverlayBar() : m_Visible(true),
                                       m_Alpha(1.0f), 
                                       m_IsScrollBarVisible(false),
                                       m_OverlayButtonActiveMenuID(k_ulOverlayID_None),
                                       m_IsAddOverlayButtonActive(false),
                                       m_MenuAlpha(0.0f),
                                       m_IsMenuRemoveConfirmationVisible(false)
{
    m_Size.x = 32.0f;
}

void WindowOverlayBar::DisplayTooltipIfHovered(const char* text, unsigned int overlay_id)
{
    //Blank name is not allowed by ImGui and doesn't much sense to display anyways
    if ((text == nullptr) || (text[0] == '\0'))
        return;

    if (ImGui::IsItemHovered())
    {
        static ImVec2 button_pos_last; //Remember last position and use it when posible. This avoids flicker when the same tooltip string is used in different places
        ImVec2 pos = ImGui::GetItemRectMin();
        pos.y = ImGui::GetIO().DisplaySize.y;
        float button_width = ImGui::GetItemRectSize().x;

        //Default tooltips are not suited for this as there's too much trouble with resize flickering and stuff
        ImGui::Begin(text, nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

        ImGui::BeginGroup();

        //Display icon for overlay origin if the tooltip is for a overlay
        if (overlay_id != k_ulOverlayID_None)
        {
            const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

            ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
            ImVec2 img_size, img_uv_min, img_uv_max;

            TextureManager::Get().GetTextureInfo((TMNGRTexID)(tmtex_icon_xsmall_origin_room + data.ConfigInt[configid_int_overlay_origin]), img_size, img_uv_min, img_uv_max);
            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }

        ImGui::TextUnformatted(text);
        ImGui::EndGroup();

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
        pos.y -= window_size.y;// + ImGui::GetStyle().WindowPadding.y;

        pos.x = clamp(pos.x, 0.0f, ImGui::GetIO().DisplaySize.x - window_size.x);   //Clamp right side to texture end

        ImGui::SetWindowPos(pos);

        ImGui::End();
    }
}

void WindowOverlayBar::UpdateOverlayButtons()
{
    ImGuiIO& io = ImGui::GetIO();

    //List of unique IDs for overlays so ImGui can identify the same list entries after reordering or list expansion (needed for drag reordering)
    static std::vector<int> list_unique_ids;
    static unsigned int drag_last_hovered_button = k_ulOverlayID_None;
    static bool drag_done_since_last_mouse_down = false;

    const int overlay_count = (int)OverlayManager::Get().GetOverlayCount();
    unsigned int properties_active_overlay = (UIManager::Get()->GetOverlayPropertiesWindow().IsVisible()) ? (UIManager::Get()->GetOverlayPropertiesWindow().GetActiveOverlayID()) : k_ulOverlayID_None;

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

    ImGui::PushID("OverlayButtons");

    bool b_window_icon_available = false;
    TMNGRTexID b_icon_texture_id = tmtex_icon_desktop;
    ImVec2 b_size, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max); //Get settings icons dimensions for uniform button size
    ImVec2 b_size_default = b_size;

    static unsigned int hovered_overlay_id_last = k_ulOverlayID_None;
    unsigned int hovered_overlay_id = k_ulOverlayID_None;
    const unsigned int u_overlay_count = OverlayManager::Get().GetOverlayCount();

    for (unsigned int i = 0; i < u_overlay_count; ++i)
    {
        ImGui::PushID(list_unique_ids[i]);

        bool is_active = ( (m_OverlayButtonActiveMenuID == i) || (properties_active_overlay == i) );

        if (is_active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

        //Get icon texture ID
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);
        TextureManager::Get().GetOverlayIconTextureInfo(data, b_size, b_uv_min, b_uv_max, false, &b_window_icon_available);

        const ImVec4 tint_color = ImVec4(1.0f, 1.0f, 1.0f, data.ConfigBool[configid_bool_overlay_enabled] ? 1.0f : 0.5f); //Transparent when hidden

        if (ImGui::ImageButton(io.Fonts->TexID, b_size_default, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint_color))
        {
            if (io.MouseDownDurationPrev[ImGuiMouseButton_Left] < 3.0f) //Don't do normal button behavior after reset was just triggered
            {
                if ((m_OverlayButtonActiveMenuID != i) && (!drag_done_since_last_mouse_down))
                {
                    HideMenus();
                    m_OverlayButtonActiveMenuID = i;
                }
                else
                {
                    HideMenus();
                }
            }
        }

        if (is_active)
            ImGui::PopStyleColor();

        //Additional button behavior
        bool button_active = ImGui::IsItemActive();
        ImVec2 pos = ImGui::GetItemRectMin();
        float width = ImGui::GetItemRectSize().x;

        //Reset transform when holding the button for 3 or more seconds
        bool show_hold_message = false;

        if ( (button_active) && (!drag_done_since_last_mouse_down) )
        {
            if (io.MouseDownDuration[ImGuiMouseButton_Left] > 3.0f)
            {
                FloatingWindow& overlay_properties = UIManager::Get()->GetOverlayPropertiesWindow();
                overlay_properties.SetPinned(false);
                overlay_properties.ResetTransformAll();
                io.MouseDown[ImGuiMouseButton_Left] = false;    //Release mouse button so transform changes don't get blocked
            }
            else if (io.MouseDownDurationPrev[ImGuiMouseButton_Left] > 0.5f)
            {
                show_hold_message = true;
            }
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            drag_last_hovered_button = i;
            hovered_overlay_id = i;
        }

        if ((ImGui::IsItemActive()) && (!ImGui::IsItemHovered())) //Drag reordering
        {
            int index_swap = i + ((ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x < 0.0f) ? -1 : 1);
            if ((drag_last_hovered_button != i) && (index_swap >= 0) && (index_swap < overlay_count))
            {
                OverlayManager::Get().SwapOverlays(i, index_swap);
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, i);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_swap, index_swap);
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

                std::iter_swap(list_unique_ids.begin() + i, list_unique_ids.begin() + index_swap);

                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                drag_done_since_last_mouse_down = true;

                //Also adjust the active properties window if we just swapped that
                if (properties_active_overlay == i)
                {
                    UIManager::Get()->GetOverlayPropertiesWindow().SetActiveOverlayID((unsigned int)index_swap, true);
                }
                else if (properties_active_overlay == (unsigned int)index_swap)
                {
                    UIManager::Get()->GetOverlayPropertiesWindow().SetActiveOverlayID(i, true);
                }

                m_OverlayButtonActiveMenuID = k_ulOverlayID_None;
            }
        }

        if (show_hold_message)
        {
            DisplayTooltipIfHovered(TranslationManager::GetString(tstr_OverlayBarTooltipResetHold));
        }
        else
        {
            DisplayTooltipIfHovered(OverlayManager::Get().GetConfigData(i).ConfigNameStr.c_str(), i);
        }
        

        float dist = width / 2.0f;
        float menu_y = m_Pos.y + ImGui::GetStyle().WindowBorderSize + dist - (dist * m_MenuAlpha);

        if (m_OverlayButtonActiveMenuID == i)
        {
            MenuOverlayButton(i, {pos.x + width / 2.0f, menu_y}, button_active);

            //Check if menu modified overlay count and bail then
            if (OverlayManager::Get().GetOverlayCount() != u_overlay_count)
            {
                ImGui::PopID();
                UIManager::Get()->RepeatFrame();
                break;
            }
        }

        //Draw window icon on top
        if (b_window_icon_available)
        {
            TextureManager::Get().GetOverlayIconTextureInfo(data, b_size, b_uv_min, b_uv_max, true);

            ImVec2 p_min = {pos.x + (width / 2.0f) - (b_size.x / 2.0f), pos.y + (width / 2.0f) - (b_size.y / 2.0f)};
            ImVec2 p_max = p_min;
            p_max.x += b_size.x;
            p_max.y += b_size.y;

            ImGui::GetWindowDrawList()->AddImage(io.Fonts->TexID, p_min, p_max, b_uv_min, b_uv_max, ImGui::ColorConvertFloat4ToU32(tint_color));
        }

        ImGui::SameLine();

        ImGui::PopID();
    }

    //Don't change overlay highlight while mouse down as it won't be correct while dragging and flicker just before it
    if ( (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) && (hovered_overlay_id_last != hovered_overlay_id) )
    {
        UIManager::Get()->HighlightOverlay(hovered_overlay_id);
        hovered_overlay_id_last = hovered_overlay_id;
    }

    ImGui::PopID();

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        //If we did a swap, finalize all swapping changes
        if (drag_done_since_last_mouse_down)
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_swap_finish);
            OverlayManager::Get().SwapOverlaysFinish();
            drag_done_since_last_mouse_down = false;
        }
    }
}

void WindowOverlayBar::MenuOverlayButton(unsigned int overlay_id, ImVec2 pos, bool is_item_active)
{
    m_MenuAlpha += ImGui::GetIO().DeltaTime * 12.0f;

    if (m_MenuAlpha > 1.0f)
        m_MenuAlpha = 1.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_MenuAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    if (ImGui::Begin("OverlayButtonMenu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | 
                                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        if ( (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) && (ImGui::IsAnyMouseClicked()) && (!is_item_active) )
        {
            HideMenus();
        }

        bool& is_enabled = OverlayManager::Get().GetConfigData(overlay_id).ConfigBool[configid_bool_overlay_enabled];
        WindowOverlayProperties& properties_window = UIManager::Get()->GetOverlayPropertiesWindow();

        if (ImGui::Selectable(TranslationManager::GetString((is_enabled) ? tstr_OverlayBarOvrlHide : tstr_OverlayBarOvrlShow), false))
        {
            is_enabled = !is_enabled;

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, (int)overlay_id);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_overlay_enabled, is_enabled);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

            HideMenus();
        }

        if (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlClone), false))
        {
            //Copy data of overlay and add a new one based on it
            OverlayManager::Get().DuplicateOverlay(OverlayManager::Get().GetConfigData(overlay_id));
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_duplicate, (int)overlay_id);

            HideMenus();
        }

        if (m_IsMenuRemoveConfirmationVisible)
        {
            if (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlRemoveConfirm), false))
            {
                OverlayManager::Get().RemoveOverlay(overlay_id);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_remove, overlay_id);

                //Hide properties window if it's open for this overlay
                if (properties_window.GetActiveOverlayID() == overlay_id)
                {
                    properties_window.SetActiveOverlayID(k_ulOverlayID_None, true);
                    properties_window.Hide();
                }
                else if (properties_window.GetActiveOverlayID() > overlay_id) //Adjust properties window active overlay ID if it's open for an overlay that had its ID shifted
                {
                    properties_window.SetActiveOverlayID(properties_window.GetActiveOverlayID() - 1, true);
                }

                HideMenus();
            }
        }
        else
        {
            if (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlRemove), false))
            {
                m_IsMenuRemoveConfirmationVisible = true;
            }
        }


        if (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlProperties), false))
        {
            //Hide window instead if it's already open for this overlay
            if ((properties_window.IsVisible()) && (properties_window.GetActiveOverlayID() == overlay_id))
            {
                properties_window.Hide();
            }
            else
            {
                properties_window.SetActiveOverlayID(overlay_id);
                properties_window.Show();
            }

            HideMenus();
        }

        //Position window while clamping to overlay bar size
        ImVec2 window_size = ImGui::GetWindowSize();
        pos.x = clamp(pos.x - (window_size.x / 2.0f), m_Pos.x, m_Pos.x + m_Size.x - window_size.x);
        pos.y -= window_size.y;

        ImGui::SetWindowPos(pos);

        if (ImGui::IsWindowAppearing())
        {
            //We need valid window size for positioning (can't use ImGui::SetNextWindowPos() because of clamping), so reset things and repeat frame if don't have it yet
            UIManager::Get()->RepeatFrame();
            m_MenuAlpha = 0.0f;
        }

    }

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    ImGui::End();
}

void WindowOverlayBar::MenuAddOverlayButton(ImVec2 pos, bool is_item_active)
{
    m_MenuAlpha += ImGui::GetIO().DeltaTime * 12.0f;

    if (m_MenuAlpha > 1.0f)
        m_MenuAlpha = 1.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_MenuAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::SetNextWindowSizeConstraints({-1.0f, 0.0f}, {-1.0f, (ImGui::GetTextLineHeightWithSpacing() * 6.0f) + (ImGui::GetStyle().WindowPadding.y * 2.0f) });
    if (ImGui::Begin("AddOverlayButtonMenu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | 
                                                      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        if ( (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) && (ImGui::IsAnyMouseClicked()) && (!is_item_active) )
        {
            HideMenus();
        }

        int desktop_count = ConfigManager::GetValue(configid_int_state_interface_desktop_count);

        int new_overlay_desktop_id = -255;

        for (int i = 0; i < desktop_count; ++i)
        {
            ImGui::PushID(i);

            ImGui::Selectable(TranslationManager::Get().GetDesktopIDString(i));

            if (ImGui::IsItemActivated())
            {
                new_overlay_desktop_id = i;
            }

            ImGui::PopID();
        }

        if ( (DPWinRT_IsCaptureSupported()) && (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlAddWindow))) )
        {
            //Get current pointer transform and set window transform from it
            if (UIManager::Get()->IsOpenVRLoaded())
            {
                vr::TrackedDeviceIndex_t device_index = ConfigManager::Get().GetPrimaryLaserPointerDevice();

                //If no dashboard device, try finding one
                if (device_index == vr::k_unTrackedDeviceIndexInvalid)
                {
                    device_index = FindPointerDeviceForOverlay(UIManager::Get()->GetOverlayHandleOverlayBar());
                }

                Matrix4 overlay_transform;
                vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

                vr::VROverlayIntersectionResults_t results;

                if (ComputeOverlayIntersectionForDevice(UIManager::Get()->GetOverlayHandleOverlayBar(), device_index, vr::TrackingUniverseStanding, &results))
                {
                    overlay_transform.setTranslation(results.vPoint);
                }
                else //Shouldn't happen, but have some fallback
                {
                    vr::HmdMatrix34_t transform;
                    vr::VROverlay()->GetOverlayTransformAbsolute(UIManager::Get()->GetOverlayHandleOverlayBar(), &universe_origin, &transform);

                    overlay_transform = transform;
                }

                //Get devices poses
                vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
                vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

                if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
                {
                    //Take the average between HMD and controller position (at controller's height) and rotate towards that
                    Matrix4 mat_hmd(poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
                    Vector3 pos = mat_hmd.getTranslation();
                    
                    if ( (device_index < vr::k_unMaxTrackedDeviceCount) && (poses[device_index].bPoseIsValid) ) //If pointer doesn't have a pose, it falls back to rotating to HMD
                    {
                        Matrix4 mat_controller(poses[device_index].mDeviceToAbsoluteTracking);
                        pos = mat_controller.getTranslation();
                        pos.x += mat_hmd.getTranslation().x;
                        pos.x /= 2.0f;
                        pos.z += mat_hmd.getTranslation().z;
                        pos.z /= 2.0f;
                    }

                    TransformLookAt(overlay_transform, pos);
                }

                UIManager::Get()->GetAuxUI().GetCaptureWindowSelectWindow().SetTransform(overlay_transform);
            }

            UIManager::Get()->GetAuxUI().GetCaptureWindowSelectWindow().Show();
            HideMenus();
        }

        ImGui::Selectable(TranslationManager::GetString(tstr_SourcePerformanceMonitor));

        if (ImGui::IsItemActivated())
        {
            new_overlay_desktop_id = -3;
        }

        //Create new overlay if desktop or UI selectables were triggered
        if (new_overlay_desktop_id != -255)
        {
            vr::TrackedDeviceIndex_t device_index = ConfigManager::Get().GetPrimaryLaserPointerDevice();

            //If no dashboard device, try finding one
            if (device_index == vr::k_unTrackedDeviceIndexInvalid)
            {
                device_index = FindPointerDeviceForOverlay(UIManager::Get()->GetOverlayHandleOverlayBar());
            }

            float pointer_distance = 0.5f;

            if (UIManager::Get()->IsOpenVRLoaded())
            {
                vr::VROverlayIntersectionResults_t results;

                if (ComputeOverlayIntersectionForDevice(UIManager::Get()->GetOverlayHandleOverlayBar(), device_index, vr::TrackingUniverseStanding, &results))
                {
                    pointer_distance = results.fDistance;
                }
            }

            //Set pointer hint in case dashboard app needs it
            ConfigManager::SetValue(configid_int_state_laser_pointer_device_hint, (int)device_index);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_laser_pointer_device_hint, (int)device_index);

            //Add overlay and sent to dashboard app
            OverlayManager::Get().AddOverlay((new_overlay_desktop_id == -3) ? ovrl_capsource_ui : ovrl_capsource_desktop_duplication, new_overlay_desktop_id);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_new_drag, MAKELPARAM(new_overlay_desktop_id, int(pointer_distance * 100.0f) ));

            HideMenus();
        }

        //Position window while clamping to overlay bar size
        ImVec2 window_size = ImGui::GetWindowSize();
        pos.x = clamp(pos.x - (window_size.x / 2.0f), m_Pos.x, m_Pos.x + m_Size.x - window_size.x);
        pos.y -= window_size.y;

        ImGui::SetWindowPos(pos);

        if (ImGui::IsWindowAppearing())
        {
            //We need valid window size for positioning (can't use ImGui::SetNextWindowPos() because of clamping), so reset things and repeat frame if don't have it yet
            UIManager::Get()->RepeatFrame();
            m_MenuAlpha = 0.0f;
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    ImGui::End();
}

void WindowOverlayBar::Show(bool skip_fade)
{
    m_Visible = true;

    if (skip_fade)
    {
        m_Alpha = 1.0f;
    }
}

void WindowOverlayBar::Hide(bool skip_fade)
{
    m_Visible = false;

    if (skip_fade)
    {
        m_Alpha = 0.0f;
    }
}

void WindowOverlayBar::HideMenus()
{
    m_MenuAlpha = 0.0f;
    m_OverlayButtonActiveMenuID = k_ulOverlayID_None;
    m_IsAddOverlayButtonActive = false;
    m_IsMenuRemoveConfirmationVisible = false;

    UIManager::Get()->RepeatFrame();

    //Reset sort order if the overlay already isn't hovered anymore
    if ( (UIManager::Get()->IsOpenVRLoaded()) && (!ConfigManager::Get().IsLaserPointerTargetOverlay(UIManager::Get()->GetOverlayHandleOverlayBar())) )
    {
        vr::VROverlay()->SetOverlaySortOrder(UIManager::Get()->GetOverlayHandleOverlayBar(), 0);
    }
}

void WindowOverlayBar::Update()
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

    ImVec2 b_size, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
    //Default button size for custom actions to be the same as the settings icon so the user is able to provide oversized images without messing up the layout
    //as well as still providing a way to change the size of text buttons by editing the settings icon's dimensions
    ImVec2 b_size_default = b_size;

    float tooltip_padding = ImGui::GetTextLineHeightWithSpacing() + (ImGui::GetStyle().WindowPadding.y * 2.0f);
    float min_width = io.DisplaySize.x * 0.50f;
    ImGui::SetNextWindowSizeConstraints({min_width, -1.0f}, {io.DisplaySize.x * 0.95f, -1.0f});
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2.0f, io.DisplaySize.y - tooltip_padding), 0, ImVec2(0.5f, 1.0f));  //Center window at bottom of the overlay with space for tooltips

    ImGui::Begin("WindowOverlayBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
                                              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::HScrollWindowFromMouseWheelV();

    //Scrollbar visible state can flicker for one frame when an overlay is added or removed while the bar is actually visible... no idea why, but work around it by repeating a frame
    bool scrollbar_visible = ImGui::IsAnyScrollBarVisible();
    if (scrollbar_visible != m_IsScrollBarVisible)
    {
        UIManager::Get()->RepeatFrame();
        m_IsScrollBarVisible = ImGui::IsAnyScrollBarVisible();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    UpdateOverlayButtons();

    static float right_buttons_width = 0.0f;

    float free_width = min_width - ImGui::GetCursorPosX() - right_buttons_width;
    if (free_width > 0)
    {
        ImGui::Dummy({free_width, 0.0f});
        ImGui::SameLine(0.0f, 0.0f);
    }

    //Add Overlay Button
    {
        if (!UIManager::Get()->IsOpenVRLoaded())        //Add Overlay stuff doesn't work without OpenVR loaded, so disable it
            ImGui::PushItemDisabled();

        bool is_add_overlay_active = m_IsAddOverlayButtonActive;
        if (is_add_overlay_active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

        ImGui::PushID(tmtex_icon_small_close);
        TextureManager::Get().GetTextureInfo(tmtex_icon_add, b_size, b_uv_min, b_uv_max);
        if (ImGui::ImageButton(io.Fonts->TexID, b_size_default, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
        {
            if (!m_IsAddOverlayButtonActive)
            {
                HideMenus();
                m_IsAddOverlayButtonActive = true;
            }
            else
            {
                HideMenus();
            }
        }

        if (is_add_overlay_active)
            ImGui::PopStyleColor(); //ImGuiCol_Button

        bool button_active = ImGui::IsItemActive();
        ImVec2 pos = ImGui::GetItemRectMin();
        float width = ImGui::GetItemRectSize().x;

        DisplayTooltipIfHovered(TranslationManager::GetString(tstr_OverlayBarTooltipOvrlAdd));

        if (m_IsAddOverlayButtonActive)
        {
            float dist   = width / 2.0f;
            float menu_y = m_Pos.y + ImGui::GetStyle().WindowBorderSize + dist - (dist * m_MenuAlpha);

            MenuAddOverlayButton({pos.x + width / 2.0f, menu_y}, button_active);
        }

        ImGui::PopID();

        if (!UIManager::Get()->IsOpenVRLoaded())
            ImGui::PopItemDisabled();
    }

    ImGui::SameLine();

    //Settings Button
    bool settings_shown = UIManager::Get()->GetSettingsWindow().IsVisible();
    if (settings_shown)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    ImGui::PushID(tmtex_icon_settings);
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
    {
        if (io.MouseDownDurationPrev[ImGuiMouseButton_Left] < 3.0f) //Don't do normal button behavior after reset was just triggered
        {
            FloatingWindow& floating_settings = UIManager::Get()->GetSettingsWindow();
            (floating_settings.IsVisible()) ? floating_settings.Hide() : floating_settings.Show();
        }
    }

    //Reset tranform when holding the button for 3 or more seconds
    bool show_hold_message = false;

    if (ImGui::IsItemActive())  
    {
        if (io.MouseDownDuration[ImGuiMouseButton_Left] > 3.0f)
        {
            FloatingWindow& floating_settings = UIManager::Get()->GetSettingsWindow();
            floating_settings.SetPinned(false);
            floating_settings.ResetTransformAll();
            io.MouseDown[ImGuiMouseButton_Left] = false;    //Release mouse button so transform changes don't get blocked
        }
        else if (io.MouseDownDurationPrev[ImGuiMouseButton_Left] > 0.5f)
        {
            show_hold_message = true;
        }
    }

    if (settings_shown)
        ImGui::PopStyleColor(); //ImGuiCol_Button

    //Warning/Error marker
    if (UIManager::Get()->IsAnyWarningDisplayed())
    {
        ImVec2 p_max = {ImGui::GetItemRectMax().x - ImGui::GetStyle().ItemInnerSpacing.x, ImGui::GetItemRectMin().y + ImGui::GetStyle().ItemInnerSpacing.y};
        ImVec2 p_min = p_max;
        p_min.x -= ImGui::CalcTextSize(k_pch_bold_exclamation_mark).x;
        p_max.y += ImGui::GetTextLineHeight();

        ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, ImGui::GetColorU32(Style_ImGuiCol_TextError), ImGui::GetStyle().WindowRounding);
        ImGui::GetWindowDrawList()->AddText(p_min, ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)), k_pch_bold_exclamation_mark);
    }

    right_buttons_width = (ImGui::GetItemRectSize().x * 2.0f) + ImGui::GetStyle().ItemSpacing.x;

    DisplayTooltipIfHovered( TranslationManager::GetString((show_hold_message) ? tstr_OverlayBarTooltipResetHold : tstr_OverlayBarTooltipSettings) );

    ImGui::PopID();

    ImGui::PopStyleColor(); //ImGuiCol_Button
    ImGui::PopStyleVar();   //ImGuiStyleVar_FrameRounding

    m_Pos  = ImGui::GetWindowPos();
    m_Size = ImGui::GetWindowSize();

    ImGui::End();
    ImGui::PopStyleVar(); //ImGuiStyleVar_Alpha
}

const ImVec2 & WindowOverlayBar::GetPos() const
{
    return m_Pos;
}

const ImVec2 & WindowOverlayBar::GetSize() const
{
    return m_Size;
}

bool WindowOverlayBar::IsVisible() const
{
    return m_Visible;
}

bool WindowOverlayBar::IsAnyMenuVisible() const
{
    return (m_MenuAlpha != 0.0f);
}

bool WindowOverlayBar::IsScrollBarVisible() const
{
    return m_IsScrollBarVisible;
}

float WindowOverlayBar::GetAlpha() const
{
    return m_Alpha;
}
