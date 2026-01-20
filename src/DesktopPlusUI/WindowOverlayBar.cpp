#include "WindowOverlayBar.h"

#include "ImGuiExt.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "OpenVRExt.h"
#include "UIManager.h"
#include "OverlayManager.h"
#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"


WindowOverlayBar::WindowOverlayBar() : m_Visible(true),
                                       m_Alpha(1.0f), 
                                       m_IsScrollBarVisible(false),
                                       m_OverlayButtonActiveMenuID(k_ulOverlayID_None),
                                       m_IsAddOverlayButtonActive(false),
                                       m_MenuAlpha(0.0f),
                                       m_IsMenuRemoveConfirmationVisible(false),
                                       m_IsDraggingOverlayButtons(false)
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
        const ImGuiStyle& style = ImGui::GetStyle();

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

            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        }

        ImGui::TextUnformatted(text);
        ImGui::EndGroup();

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
        pos.y -= window_size.y;

        pos.x = clamp(pos.x, 0.0f, ImGui::GetIO().DisplaySize.x - window_size.x);   //Clamp right side to texture end

        ImGui::SetWindowPos(pos);

        ImGui::End();
    }
}

void WindowOverlayBar::UpdateOverlayButtons()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    struct OverlayButtonState
    {
        float starting_x = -1.0f;
        float animation_progress = 0.0f;
    };

    //List of unique IDs for overlays so ImGui can identify the same list entries after reordering or list expansion (needed for drag reordering)
    static std::vector<int> list_unique_ids;
    static std::vector<OverlayButtonState> list_unique_data;
    static unsigned int drag_button_id  = k_ulOverlayID_None;
    static unsigned int drag_overlay_id = k_ulOverlayID_None;
    static float drag_mouse_offset = 0.0f;

    const int overlay_count = (int)OverlayManager::Get().GetOverlayCount();
    const unsigned int properties_active_overlay = (UIManager::Get()->GetOverlayPropertiesWindow().IsVisible()) ? (UIManager::Get()->GetOverlayPropertiesWindow().GetActiveOverlayID()) : k_ulOverlayID_None;

    //Reset unique IDs & widget data when appearing
    if (ImGui::IsWindowAppearing())
    {
        list_unique_ids.clear();
        list_unique_data.clear();
    }

    //Expand unique id lists if overlays were added (also does initialization since it's empty then)
    while (list_unique_ids.size() < OverlayManager::Get().GetOverlayCount())
    {
        list_unique_ids.push_back((int)list_unique_ids.size());
    }

    while (list_unique_data.size() < OverlayManager::Get().GetOverlayCount())
    {
        list_unique_data.emplace_back();
    }

    //Get settings icons dimensions for uniform button size
    ImVec2 b_size, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
    ImVec2 b_size_default = b_size;

    static unsigned int left_down_overlay_id  = k_ulOverlayID_None;
    static unsigned int right_down_overlay_id = k_ulOverlayID_None;
    static unsigned int hovered_overlay_id_last = k_ulOverlayID_None;
    unsigned int hovered_overlay_id = k_ulOverlayID_None;

    //Lambda for an overlay button widget. Interactions are implemented separately where needed
    auto overlay_button = [&](unsigned int overlay_id)
    {
        bool is_active = ( (m_OverlayButtonActiveMenuID == overlay_id) || (drag_overlay_id == overlay_id) || (properties_active_overlay == overlay_id) || (left_down_overlay_id == overlay_id) || 
                           (right_down_overlay_id == overlay_id) );

        if (is_active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }

        //Get icon texture ID
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
        bool b_window_icon_available;
        TextureManager::Get().GetOverlayIconTextureInfo(data, b_size, b_uv_min, b_uv_max, false, &b_window_icon_available);

        const ImVec4 tint_color = ImVec4(1.0f, 1.0f, 1.0f, data.ConfigBool[configid_bool_overlay_enabled] ? 1.0f : 0.5f); //Half-transparent when hidden

        bool ret = ImGui::ImageButton("OverlayButton", io.Fonts->TexID, b_size_default, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint_color);

        if (is_active)
        {
            ImGui::PopStyleColor(2);
        }

        //Additional button behavior
        const ImVec2 pos  = ImGui::GetItemRectMin();
        const float width = ImGui::GetItemRectSize().x;

        //Draw window icon on top
        if (b_window_icon_available)
        {
            TextureManager::Get().GetOverlayIconTextureInfo(data, b_size, b_uv_min, b_uv_max, true);

            //Downscale oversized icons
            float icon_scale = (b_size_default.x * 0.5f) / std::max(b_size.x, b_size.y);

            if (icon_scale < 1.0f)
            {
                b_size *= icon_scale;
            }

            ImVec2 p_min = {pos.x + (width / 2.0f) - (b_size.x / 2.0f), pos.y + (width / 2.0f) - (b_size.y / 2.0f)};
            ImVec2 p_max = p_min;
            p_max.x += b_size.x;
            p_max.y += b_size.y;

            ImGui::GetWindowDrawList()->AddImage(io.Fonts->TexID, p_min, p_max, b_uv_min, b_uv_max, ImGui::ColorConvertFloat4ToU32(tint_color));
        }

        return ret;
    };

    //List overlays
    const float button_width_base = b_size_default.x + (style.ItemSpacing.x * 3.0f);
    const ImVec2 cursor_pos_first = ImGui::GetCursorPos();
    const ImVec2 cursor_screen_pos_first = ImGui::GetCursorScreenPos();

    ImGui::PushID("OverlayButtons");

    const unsigned int u_overlay_count = OverlayManager::Get().GetOverlayCount();
    for (unsigned int i = 0; i < u_overlay_count; ++i)
    {
        const unsigned int button_id = list_unique_ids[i];

        if (drag_button_id != button_id)
        {
            ImGui::PushID(button_id);

            //Button positioning
            OverlayButtonState& button_state = list_unique_data[button_id];
            const float target_x = cursor_pos_first.x + (button_width_base * i);

            if (button_state.starting_x != -1.0f)
            {
                ImGui::SetCursorPosX(smoothstep(button_state.animation_progress, button_state.starting_x, target_x));

                const float animation_step = ImGui::GetIO().DeltaTime * 6.0f;
                button_state.animation_progress = clamp(button_state.animation_progress + animation_step, 0.0f, 1.0f);

                if (button_state.animation_progress == 1.0f)
                {
                    button_state.starting_x = -1.0f;
                    button_state.animation_progress = 0.0f;
                }
            }
            else
            {
                ImGui::SetCursorPosX(target_x);
            }

            //Overlay button widget
            if (overlay_button(i))
            {
                if (io.MouseDownDurationPrev[ImGuiMouseButton_Left] < 3.0f) //Don't do normal button behavior after reset was just triggered
                {
                    if ((m_OverlayButtonActiveMenuID != i) && (!m_IsDraggingOverlayButtons))
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

            //-Additional button behavior
            bool button_active = ImGui::IsItemActive();
            ImVec2 pos         = ImGui::GetItemRectMin();
            float width        = ImGui::GetItemRectSize().x;

            //Reset transform when holding the button for 3 or more seconds
            bool show_hold_message = false;

            if ( (button_active) && (!m_IsDraggingOverlayButtons) )
            {
                if (io.MouseDownDuration[ImGuiMouseButton_Left] > 3.0f)
                {
                    FloatingWindow& overlay_properties = UIManager::Get()->GetOverlayPropertiesWindow();
                    overlay_properties.SetPinned(false);
                    overlay_properties.ResetTransformAll();
                    io.MouseDown[ImGuiMouseButton_Left] = false;    //Release mouse button so transform changes don't get blocked
                }
                else if (io.MouseDownDuration[ImGuiMouseButton_Left] > 0.5f)
                {
                    show_hold_message = true;
                }
            }

            if (ImGui::IsItemHovered())
            {
                //Quick toggle visibility via double-click
                const int click_count = ImGui::GetMouseClickedCount(ImGuiMouseButton_Left);
                if ((click_count > 1) && (click_count % 2 == 0))     //ImGui keeps counting up, so fast double-clicks in a row don't get detected as such with ImGui::IsMouseDoubleClicked()
                {
                    OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);
                    bool& is_enabled = data.ConfigBool[configid_bool_overlay_enabled];

                    is_enabled = !is_enabled;

                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, i);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_overlay_enabled, is_enabled);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);
                }

                //Quick switch properties via right-click
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    right_down_overlay_id = i;
                }

                if ((ImGui::IsMouseReleased(ImGuiMouseButton_Right)) && (right_down_overlay_id == i))
                {
                    WindowOverlayProperties& properties_window = UIManager::Get()->GetOverlayPropertiesWindow();

                    //Hide window instead if it's already open for this overlay
                    if ((properties_window.IsVisible()) && (properties_window.GetActiveOverlayID() == i))
                    {
                        properties_window.Hide();
                    }
                    else
                    {
                        properties_window.SetActiveOverlayID(i);
                        properties_window.Show();
                    }

                    HideMenus();
                }
            }

            //Remember hovered overlay for highlighting
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            {
                hovered_overlay_id = i;
            }

            //Tooltip, but don't show while dragging or animating position
            if ((!m_IsDraggingOverlayButtons) && (button_state.starting_x == -1.0f))
            {
                (show_hold_message) ? DisplayTooltipIfHovered(TranslationManager::GetString(tstr_OverlayBarTooltipResetHold)) : 
                                      DisplayTooltipIfHovered(OverlayManager::Get().GetConfigData(i).ConfigNameStr.c_str(), i);
            }

            //Button menu
            if (m_OverlayButtonActiveMenuID == i)
            {
                float dist = width / 2.0f;
                float menu_y = m_Pos.y + ImGui::GetStyle().WindowBorderSize + dist - (dist * m_MenuAlpha);

                MenuOverlayButton(i, {pos.x + width / 2.0f, menu_y}, button_active);

                //Check if menu modified overlay count and bail then
                if (OverlayManager::Get().GetOverlayCount() != u_overlay_count)
                {
                    ImGui::PopID();
                    UIManager::Get()->RepeatFrame();
                    break;
                }
            }

            //Dragging start
            if (button_active)
            {
                if (!m_IsDraggingOverlayButtons)
                {
                    //Record cursor offset at the time of clicking as we require a large drag delta to avoid mis-inputs when trying to just press the button with a shaky pointer
                    //The button will be animated to snap to this offset
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        drag_mouse_offset = -(ImGui::GetMousePos().x - ImGui::GetItemRectMin().x);
                    }

                    //Not using ImGui::IsMouseDragging() here to only check for horizontal dragging
                    if (fabs(ImGui::GetMouseDragDelta().x) > width / 2.0f)
                    {
                        drag_button_id = button_id;
                        drag_overlay_id = i;
                        m_IsDraggingOverlayButtons = true;

                        //Set starting drag state for animating the button snapping to the mouse offset posiiton
                        OverlayButtonState& drag_button_state = list_unique_data[list_unique_ids[drag_overlay_id]];
                        const float target_x = ImGui::GetMousePos().x + drag_mouse_offset - ImGui::GetMouseDragDelta().x;
                        if (drag_button_state.starting_x != -1.0f)   //account for ongoing animation even if it's really quick
                        {
                            drag_button_state.starting_x = smoothstep(drag_button_state.animation_progress, drag_button_state.starting_x, target_x);
                        }
                        else
                        {
                            drag_button_state.starting_x = target_x;
                        }

                        //This state is only used to avoid flickering when dragging right at the button edge since in Dear ImGui a held-down button doesn't stay visually down if the cursor leaves it
                        //It's going to highlight the wrong buttons if it stays set during the drag however, so we already clear it here
                        left_down_overlay_id = k_ulOverlayID_None;

                        UIManager::Get()->RepeatFrame();
                    }
                    else
                    {
                        left_down_overlay_id = i;
                    }
                }
            }

            ImGui::SameLine();
            ImGui::PopID();
        }
    }

    if (m_IsDraggingOverlayButtons)
    {
        ImGui::PushID(drag_button_id);

        //Add active dragged button separately so it's always on top of the other ones
        OverlayButtonState& drag_button_state = list_unique_data[list_unique_ids[drag_overlay_id]];
        const float target_x_drag = ImGui::GetMousePos().x + drag_mouse_offset;

        //Animate the button snapping from its initial positon into the mouse position with drag offset
        float drag_button_x = -1.0f;
        if (drag_button_state.starting_x != -1.0f)
        {
            drag_button_x = smoothstep(drag_button_state.animation_progress, drag_button_state.starting_x, target_x_drag);

            const float animation_step = ImGui::GetIO().DeltaTime * 8.0f;
            drag_button_state.animation_progress = clamp(drag_button_state.animation_progress + animation_step, 0.0f, 1.0f);

            if (drag_button_state.animation_progress == 1.0f)
            {
                drag_button_state.starting_x = -1.0f;
                drag_button_state.animation_progress = 0.0f;
            }
        }
        else
        {
            drag_button_x = target_x_drag;
        }

        drag_button_x = clamp(drag_button_x, cursor_screen_pos_first.x, cursor_screen_pos_first.x + (button_width_base * (u_overlay_count - 1) ));

        //Overlay button widget
        ImGui::SetCursorScreenPos({drag_button_x, ImGui::GetCursorScreenPos().y});
        ImGui::SetNextItemAllowOverlap();

        overlay_button(drag_overlay_id);
        ImGui::SameLine();

        //Button & overlay swapping
        unsigned int index_swap = (unsigned int)( ((drag_button_x - cursor_screen_pos_first.x) + (button_width_base / 2.0f)) / button_width_base );
        if (drag_overlay_id != index_swap)
        {
            //Animate swap
            OverlayButtonState& button_state_swap = list_unique_data[list_unique_ids[index_swap]];
            const float target_x = cursor_pos_first.x + (button_width_base * index_swap);
            if (button_state_swap.starting_x != -1.0f)   //account for ongoing animation even if it's really quick
            {
                button_state_swap.starting_x = smoothstep(button_state_swap.animation_progress, button_state_swap.starting_x, target_x);
            }
            else
            {
                button_state_swap.starting_x = target_x;
            }
            button_state_swap.animation_progress = 0.0f;

            //Actually swap the overlay
            OverlayManager::Get().SwapOverlays(drag_overlay_id, index_swap);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, drag_overlay_id);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_swap, index_swap);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

            std::iter_swap(list_unique_ids.begin() + drag_overlay_id, list_unique_ids.begin() + index_swap);

            //Also adjust the active properties window if we just swapped that
            if (properties_active_overlay == drag_overlay_id)
            {
                UIManager::Get()->GetOverlayPropertiesWindow().SetActiveOverlayID(index_swap, true);
            }
            else if (properties_active_overlay == index_swap)
            {
                UIManager::Get()->GetOverlayPropertiesWindow().SetActiveOverlayID(drag_overlay_id, true);
            }

            drag_overlay_id = index_swap;
            m_OverlayButtonActiveMenuID = k_ulOverlayID_None;

            UIManager::Get()->RepeatFrame();
        }

        //Dragging release
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            drag_button_state.starting_x = drag_button_x - ImGui::GetWindowPos().x + ImGui::GetScrollX();
            drag_button_state.animation_progress = 0.0f;

            drag_button_id = k_ulOverlayID_None;
            drag_overlay_id = k_ulOverlayID_None;
            m_IsDraggingOverlayButtons = false;
        }

        ImGui::PopID();
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        left_down_overlay_id = k_ulOverlayID_None;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
    {
        right_down_overlay_id = k_ulOverlayID_None;
    }

    //Don't change overlay highlight while mouse down as it won't be correct while dragging and flicker just before it
    if ( (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) && ((hovered_overlay_id_last != k_ulOverlayID_None) || (hovered_overlay_id != k_ulOverlayID_None)) )
    {
        UIManager::Get()->HighlightOverlay(hovered_overlay_id);
        hovered_overlay_id_last = hovered_overlay_id;
    }

    //Always leave cursor where the last unmoved button would've left it (leaves blank space during during drags if necessary)
    ImGui::SetCursorPosX(cursor_pos_first.x + (button_width_base * u_overlay_count));

    ImGui::PopID();
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
            OverlayManager::Get().DuplicateOverlay(OverlayManager::Get().GetConfigData(overlay_id), overlay_id);
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
                    properties_window.HideAll();
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
                    device_index = vr::IVROverlayEx::FindPointerDeviceForOverlay(UIManager::Get()->GetOverlayHandleOverlayBar());
                }

                Matrix4 overlay_transform;
                vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

                vr::VROverlayIntersectionResults_t results;

                if (vr::IVROverlayEx::ComputeOverlayIntersectionForDevice(UIManager::Get()->GetOverlayHandleOverlayBar(), device_index, vr::TrackingUniverseStanding, &results))
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
                vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, vr::IVRSystemEx::GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

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

                    vr::IVRSystemEx::TransformLookAt(overlay_transform, pos);
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

        if (DPBrowserAPIClient::Get().IsBrowserAvailable())
        {
            ImGui::Selectable(TranslationManager::GetString(tstr_SourceBrowser));

            if (ImGui::IsItemActivated())
            {
                new_overlay_desktop_id = -4;
            }
        }

        //Create new overlay if desktop or UI/Browser selectables were triggered
        if (new_overlay_desktop_id != -255)
        {
            vr::TrackedDeviceIndex_t device_index = ConfigManager::Get().GetPrimaryLaserPointerDevice();

            //If no dashboard device, try finding one
            if (device_index == vr::k_unTrackedDeviceIndexInvalid)
            {
                device_index = vr::IVROverlayEx::FindPointerDeviceForOverlay(UIManager::Get()->GetOverlayHandleOverlayBar());
            }

            float pointer_distance = 0.5f;

            if (UIManager::Get()->IsOpenVRLoaded())
            {
                vr::VROverlayIntersectionResults_t results;

                if (vr::IVROverlayEx::ComputeOverlayIntersectionForDevice(UIManager::Get()->GetOverlayHandleOverlayBar(), device_index, vr::TrackingUniverseStanding, &results))
                {
                    pointer_distance = results.fDistance;
                }
            }

            //Set pointer hint in case dashboard app needs it
            ConfigManager::SetValue(configid_int_state_laser_pointer_device_hint, (int)device_index);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_laser_pointer_device_hint, (int)device_index);

            //Choose capture source
            OverlayCaptureSource capsource;

            switch (new_overlay_desktop_id)
            {
                case -3: capsource = ovrl_capsource_ui;      break;
                case -4: capsource = ovrl_capsource_browser; break;
                default: capsource = ovrl_capsource_desktop_duplication;
            }

            //Add overlay and sent to dashboard app
            OverlayManager::Get().AddOverlay(capsource, new_overlay_desktop_id);
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
        const float alpha_step = ImGui::GetIO().DeltaTime * 6.0f;

        //Alpha fade animation
        m_Alpha += (m_Visible) ? alpha_step : -alpha_step;

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
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
    //Default button size for custom actions to be the same as the settings icon so the user is able to provide oversized images without messing up the layout
    //as well as still providing a way to change the size of text buttons by editing the settings icon's dimensions
    ImVec2 b_size_default = b_size;

    float tooltip_padding = ImGui::GetTextLineHeightWithSpacing() + (style.WindowPadding.y * 2.0f);
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {style.ItemSpacing.y, style.ItemSpacing.y});
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

        TextureManager::Get().GetTextureInfo(tmtex_icon_add, b_size, b_uv_min, b_uv_max);
        if (ImGui::ImageButton("Add Overlay", io.Fonts->TexID, b_size_default, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
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
            float menu_y = m_Pos.y + style.WindowBorderSize + dist - (dist * m_MenuAlpha);

            MenuAddOverlayButton({pos.x + width / 2.0f, menu_y}, button_active);
        }

        if (!UIManager::Get()->IsOpenVRLoaded())
            ImGui::PopItemDisabled();
    }

    ImGui::SameLine();

    //Action Buttons, if any
    if (!ConfigManager::Get().GetActionManager().GetActionOrderListOverlayBar().empty())
    {
        UIManager::Get()->GetFloatingUI().GetActionBarWindow().UpdateActionButtons(k_ulOverlayID_None);
    }

    //Settings Button
    bool settings_shown = UIManager::Get()->GetSettingsWindow().IsVisible();
    if (settings_shown)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton("Settings", io.Fonts->TexID, b_size, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
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
        else if (io.MouseDownDuration[ImGuiMouseButton_Left] > 0.5f)
        {
            show_hold_message = true;
        }
    }

    if (settings_shown)
        ImGui::PopStyleColor(); //ImGuiCol_Button

    //Warning/Error marker
    if (UIManager::Get()->IsAnyWarningDisplayed())
    {
        ImVec2 p_max = {ImGui::GetItemRectMax().x - style.ItemInnerSpacing.x, ImGui::GetItemRectMin().y + style.ItemInnerSpacing.y};
        ImVec2 p_min = p_max;
        p_min.x -= ImGui::CalcTextSize(k_pch_bold_exclamation_mark).x;
        p_max.y += ImGui::GetTextLineHeight();

        ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, ImGui::GetColorU32(Style_ImGuiCol_TextError), style.WindowRounding);
        ImGui::GetWindowDrawList()->AddText(p_min, ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)), k_pch_bold_exclamation_mark);
    }

    right_buttons_width = (ImGui::GetItemRectSize().x * 2.0f) + style.ItemSpacing.x;

    DisplayTooltipIfHovered( TranslationManager::GetString((show_hold_message) ? tstr_OverlayBarTooltipResetHold : tstr_OverlayBarTooltipSettings) );

    ImGui::PopStyleColor(); //ImGuiCol_Button
    ImGui::PopStyleVar();   //ImGuiStyleVar_ItemSpacing
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

bool WindowOverlayBar::IsVisibleOrFading() const
{
    return ( (m_Visible) || (m_Alpha != 0.0f) );
}

bool WindowOverlayBar::IsAnyMenuVisible() const
{
    return (m_MenuAlpha != 0.0f);
}

bool WindowOverlayBar::IsScrollBarVisible() const
{
    return m_IsScrollBarVisible;
}

bool WindowOverlayBar::IsDraggingOverlayButtons() const
{
    return m_IsDraggingOverlayButtons;
}

float WindowOverlayBar::GetAlpha() const
{
    return m_Alpha;
}
