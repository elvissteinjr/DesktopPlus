#include "WindowKeyboard.h"

#include "ImGuiExt.h"
#include "TextureManager.h"
#include "TranslationManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "UIManager.h"
#include "OverlayManager.h"

#include <sstream>

#include "imgui_internal.h"

WindowKeyboard::WindowKeyboard() : 
    m_WindowWidth(-1.0f),
    m_IsHovered(false),
    m_IsAnyButtonHovered(false),
    m_AssignedOverlayIDRoom(-1),
    m_AssignedOverlayIDDashboardTab(-1),
    m_IsIsoEnterDown(false),
    m_UnstickModifiersLater(false),
    m_SubLayoutOverride(kbdlayout_sub_base),
    m_LastSubLayout(kbdlayout_sub_base)
{
    m_WindowIcon = tmtex_icon_xsmall_keyboard;
    m_OvrlWidth = OVERLAY_WIDTH_METERS_KEYBOARD;

    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_keyboard);
    m_Size = {float(rect.GetWidth() - 4), float(rect.GetHeight() - 4)};

    m_Pos = {(float)rect.GetCenter().x, (float)rect.GetCenter().y};
    m_PosPivot = {0.5f, 0.5f};

    m_WindowFlags |= ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
    m_AllowRoomUnpinning = true;

    std::fill(std::begin(m_ManuallyStickingModifiers), std::end(m_ManuallyStickingModifiers), false);

    FloatingWindow::ResetTransformAll();
}

void WindowKeyboard::UpdateVisibility()
{
    //Set state depending on dashboard tab visibility
    if ( (!UIManager::Get()->IsInDesktopMode()) && (!m_IsTransitionFading) )
    {
        const bool is_using_dashboard_state = (m_OverlayStateCurrentID == floating_window_ovrl_state_dashboard_tab);

        if (is_using_dashboard_state != vr::VROverlay()->IsOverlayVisible(UIManager::Get()->GetOverlayHandleDPlusDashboard()))
        {
            OverlayStateSwitchCurrent(!is_using_dashboard_state);
        }
    }

    //Overlay position and visibility
    if (UIManager::Get()->IsOpenVRLoaded())
    {
        vr::VROverlayHandle_t ovrl_handle_assigned = vr::k_ulOverlayHandleInvalid;
        vr::VROverlayHandle_t overlay_handle = GetOverlayHandle();

        int assigned_overlay_id = GetAssignedOverlayID();

        if ( (m_OverlayStateCurrentID == floating_window_ovrl_state_room) && (assigned_overlay_id >= 0) )
        {
            ovrl_handle_assigned = OverlayManager::Get().GetConfigData((unsigned int)assigned_overlay_id).ConfigHandle[configid_handle_overlay_state_overlay_handle];

            if ( (ovrl_handle_assigned != vr::k_ulOverlayHandleInvalid) && (m_OverlayStateCurrent->IsVisible != vr::VROverlay()->IsOverlayVisible(ovrl_handle_assigned)) )
            {
                (m_OverlayStateCurrent->IsVisible) ? Hide() : Show();
            }
        }

        if ((!m_OvrlVisible) && (m_OverlayStateCurrent->IsVisible))
        {
            vr::VROverlay()->ShowOverlay(overlay_handle);
            m_OvrlVisible = true;

            ConfigManager::SetValue(configid_bool_state_keyboard_visible, true);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_state_keyboard_visible, true);
        }

        if ((m_OvrlVisible) && (!m_OverlayStateCurrent->IsVisible) && (m_Alpha == 0.0f))
        {
            vr::VROverlay()->HideOverlay(overlay_handle);
            m_OvrlVisible = false;

            ConfigManager::SetValue(configid_bool_state_keyboard_visible, false);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_state_keyboard_visible, false);
        }

        //Set position
        if ( (m_OverlayStateCurrent->IsVisible) && (!m_IsTransitionFading) && (!UIManager::Get()->GetOverlayDragger().IsDragActive()) && (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) )
        {
            if (ovrl_handle_assigned != vr::k_ulOverlayHandleInvalid)                       //Based on assigned overlay
            {
                if (!m_OverlayStateCurrent->IsPinned)
                {
                    Matrix4 matrix_m4 = OverlayManager::Get().GetOverlayCenterBottomTransform((unsigned int)assigned_overlay_id, ovrl_handle_assigned);
                    UIManager::Get()->GetOverlayDragger().ApplyDashboardScale(matrix_m4);
                    matrix_m4 *= m_OverlayStateCurrent->Transform;

                    vr::HmdMatrix34_t matrix_ovr = matrix_m4.toOpenVR34();
                    vr::VROverlay()->SetOverlayTransformAbsolute(GetOverlayHandle(), vr::TrackingUniverseStanding, &matrix_ovr);
                    m_OverlayStateCurrent->TransformAbs = matrix_m4;
                }
            }
            else if (m_OverlayStateCurrentID == floating_window_ovrl_state_dashboard_tab)   //Based on dashboard tab
            {
                if ((!m_OverlayStateCurrent->IsPinned) && ( (!UIManager::Get()->IsDummyOverlayTransformUnstable()) || (m_Alpha != 1.0f) ) )
                {
                    vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;
                    Matrix4 matrix_m4 = UIManager::Get()->GetOverlayDragger().GetBaseOffsetMatrix(ovrl_origin_dplus_tab) * m_OverlayStateCurrent->Transform;

                    vr::HmdMatrix34_t matrix_ovr = matrix_m4.toOpenVR34();
                    vr::VROverlay()->SetOverlayTransformAbsolute(overlay_handle, origin, &matrix_ovr);
                    m_OverlayStateCurrent->TransformAbs = matrix_m4;
                }
            }
            else if (!m_OverlayStateCurrent->IsPinned)                                      //Based on m_TransformUIOrigin (fallback when above not available and unpinned)
            {
                if (!vr::VROverlay()->IsOverlayVisible(UIManager::Get()->GetOverlayHandleDPlusDashboard()))
                {
                    Matrix4 matrix_m4 = m_TransformUIOrigin;
                    matrix_m4 *= m_OverlayStateCurrent->Transform;

                    vr::HmdMatrix34_t matrix_ovr = matrix_m4.toOpenVR34();
                    vr::VROverlay()->SetOverlayTransformAbsolute(GetOverlayHandle(), vr::TrackingUniverseStanding, &matrix_ovr);
                    m_OverlayStateCurrent->TransformAbs = matrix_m4;
                }
            }
        }
    }
}

void WindowKeyboard::Show(bool skip_fade)
{
    m_WindowTitleStrID = (UIManager::Get()->GetVRKeyboard().IsTargetUI()) ? tstr_KeyboardWindowTitleSettings : tstr_KeyboardWindowTitle;

    //Update UI origin transform when newly visible, used when there's no overlay assignment to base the position off of
    if ( (m_Alpha == 0.0f) && (UIManager::Get()->IsOpenVRLoaded()) )
    {
        //Get dashboard-similar transform and adjust it down a bit
        Matrix4 matrix_facing = ComputeHMDFacingTransform(1.15f);
        matrix_facing.translate_relative(0.0f, -0.50f, 0.0f);

        //dplus_tab origin contains dashboard scale, so apply it to this transform to stay consistent in size
        UIManager::Get()->GetOverlayDragger().ApplyDashboardScale(matrix_facing);

        m_TransformUIOrigin = matrix_facing;
    }

    ApplyCurrentOverlayState();

    FloatingWindow::Show(skip_fade);
}

void WindowKeyboard::Hide(bool skip_fade)
{
    //Refuse to hide if window is currently being dragged or hovered and remove assignment
    if ( (UIManager::Get()->GetOverlayDragger().IsDragActive()) && (UIManager::Get()->GetOverlayDragger().GetDragOverlayHandle() == GetOverlayHandle()) )
    {
        SetAssignedOverlayID(-1);

        return;
    }

    FloatingWindow::Hide(skip_fade);

    UIManager::Get()->GetVRKeyboard().OnWindowHidden();
}

void WindowKeyboard::WindowUpdate()
{
    ImGuiIO& io = ImGui::GetIO();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    //Set input state from the real mouse for ButtonLaser
    LaserInputState& state_real = GetLaserInputState(vr::k_unTrackedDeviceIndexOther);
    state_real.MouseState.SetFromGlobalState();

    /*
    //--LaserInputState desktop mode debug testing code 
    LaserInputState& state = GetLaserInputState(1);

    if (!ImGui::IsMousePosValid(&state.MouseState.MousePos))
    {
        state.MouseState.MousePos = m_Pos;
    }

    if (ImGui::IsKeyDown(VK_NUMPAD4))
    {
        state.MouseState.MousePos.x -= 5;
    }
    if (ImGui::IsKeyDown(VK_NUMPAD6))
    {
        state.MouseState.MousePos.x += 5;
    }
    if (ImGui::IsKeyDown(VK_NUMPAD8))
    {
        state.MouseState.MousePos.y -= 5;
    }
    if (ImGui::IsKeyDown(VK_NUMPAD2))
    {
        state.MouseState.MousePos.y += 5;
    }

    state.MouseState.MouseDown[0] = ImGui::IsKeyDown(VK_NUMPAD7);
    state.MouseState.MouseDown[1] = ImGui::IsKeyDown(VK_NUMPAD9);
    //--
    */

    //Render pointer blobs for any input that isn't already controller the mouse
    if (!UIManager::Get()->IsInDesktopMode())
    {
        vr::TrackedDeviceIndex_t primary_device = ConfigManager::Get().GetPrimaryLaserPointerDevice();
        bool dashboard_device_exists = (vr::VROverlay()->GetPrimaryDashboardDevice() != vr::k_unTrackedDeviceIndexInvalid);

        //Use pos/size from ImGui since our stored members describe center pivoted max window space
        const ImVec2 cur_pos  = ImGui::GetWindowPos();
        const ImVec2 cur_size = ImGui::GetWindowSize();
        const ImRect window_bb(cur_pos.x, cur_pos.y, cur_pos.x + cur_size.x, cur_pos.y + cur_size.y);

        for (auto& state : m_LaserInputStates)
        {
            if ( (state.DeviceIndex != vr::k_unTrackedDeviceIndexOther) && (state.DeviceIndex != primary_device) )
            {
                //Overlay leave events are not processed when a dashboard device exists (dashboard pointer doesn't set device index), so make sure to get the pointers out of the way anyways
                if (dashboard_device_exists)
                {
                    //Pointer left the overlay
                    state.MouseState.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

                    //Release mouse buttons if they're still down (not the case in most situations)
                    std::fill(std::begin(state.MouseState.MouseDown), std::end(state.MouseState.MouseDown), false);
                }

                //Only render if inside window
                if (window_bb.Contains(state.MouseState.MousePos))
                {
                    ImGui::GetForegroundDrawList()->AddCircleFilled(state.MouseState.MousePos, 7.0f, ImGui::GetColorU32(Style_ImGuiCol_SteamVRCursorBorder));
                    ImGui::GetForegroundDrawList()->AddCircleFilled(state.MouseState.MousePos, 5.0f, ImGui::GetColorU32(Style_ImGuiCol_SteamVRCursor));
                }

                //Also advance the mouse state as if it was managed by ImGui
                state.MouseState.Advance();
            }
        }
    }

    //Increased key repeat delay for the VR keyboard buttons
    float key_repeat_delay_old = io.KeyRepeatDelay;
    io.KeyRepeatDelay = 0.5f;

    m_IsHovered = ((ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenBlockedByPopup)) && 
                   (io.MousePos.y >= ImGui::GetCursorScreenPos().y - ImGui::GetStyle().WindowPadding.y));

    //Keyboard buttons use their own window-local active widget state in order to not interrupt active InputText() widgets. This is a dirty hack, but works for now
    ImGui::ActiveWidgetStateStorage widget_state_back;
    widget_state_back.StoreCurrentState();

    m_KeyboardWidgetState.AdvanceState();
    m_KeyboardWidgetState.ApplyState();

    //Enable button repeat if setting is enabled
    bool use_key_repeat = ConfigManager::GetValue(configid_bool_input_keyboard_key_repeat);
    if (use_key_repeat)
        ImGui::PushButtonRepeat(true);

    const float base_width = (float)int(ImGui::GetTextLineHeightWithSpacing() * 2.225f);

    //Used for kbdlayout_key_virtual_key_iso_enter key
    ImVec2 iso_enter_top_pos(-1.0f, -1.0f);
    float  iso_enter_top_width    = -1.0f;
    int    iso_enter_top_index    = -1;
    int    iso_enter_button_state = 0;
    int    iso_enter_click_state  = 0;
    bool   iso_enter_hovered      = false;

    KeyboardLayoutSubLayout current_sublayout = kbdlayout_sub_base;

    //Select sublayout based on current state
    if (m_SubLayoutOverride != kbdlayout_sub_base)
    {
        current_sublayout = m_SubLayoutOverride;
    }
    else if ((vr_keyboard.GetLayoutMetadata().HasAltGr) && (vr_keyboard.GetKeyDown(VK_RMENU)) && (!vr_keyboard.GetLayout(kbdlayout_sub_altgr).empty()))
    {
        current_sublayout = kbdlayout_sub_altgr;
    }
    else if ((vr_keyboard.GetKeyDown(VK_SHIFT) != vr_keyboard.IsCapsLockToggled()))
    {
        current_sublayout = kbdlayout_sub_shift;
    }

    //Resize button state storage if necessary
    if (vr_keyboard.GetLayout(current_sublayout).size() > m_ButtonStates.size())
    {
        m_ButtonStates.resize(vr_keyboard.GetLayout(current_sublayout).size());
    }

    //Release keys that may have been held down when the sublayout switched (buttons won't fire release when they just disappear from that)
    if (current_sublayout != m_LastSubLayout)
    {
        //Reset button state, but try to keep the same keys pressed if they exist in the new layout
        std::vector<ButtonLaserState> button_states_prev = m_ButtonStates;
        std::fill(m_ButtonStates.begin(), m_ButtonStates.end(), ButtonLaserState());

        bool sticky_modifiers_enabled = ConfigManager::GetValue(configid_bool_input_keyboard_sticky_modifiers);

        int key_index = 0;
        for (const auto& key : vr_keyboard.GetLayout(m_LastSubLayout))
        {
            ButtonLaserState& button_state = button_states_prev[key_index];

            //If down, try to find same keys in both layouts
            if (button_state.IsDown)
            {
                int key_index_new = FindSameKeyInNewSubLayout(key_index, m_LastSubLayout, current_sublayout);

                if (key_index_new != -1)
                {
                    m_ButtonStates[key_index_new] = button_state;
                }
                else
                {
                    switch (key.KeyType)
                    {
                        case kbdlayout_key_virtual_key:
                        {
                            OnVirtualKeyUp(key.KeyCode);
                            break;
                        }
                        case kbdlayout_key_virtual_key_iso_enter:
                        {
                            OnVirtualKeyUp(key.KeyCode);
                            m_IsIsoEnterDown = false;
                            break;
                        }
                        case kbdlayout_key_string:
                        {
                            OnStringKeyUp(key.KeyString);
                            break;
                        }
                        case kbdlayout_key_action:
                        {
                            vr_keyboard.SetActionDown(key.KeyActionID, false);
                            break;
                        }
                        default: break;
                    }
                }
            }

            key_index++;
        }

        ImGui::ClearActiveID();
    }

    //Handle scheduled modifier releases
    if (m_UnstickModifiersLater)
    {
        const unsigned char keycodes[] = {VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN};

        //Release modifier keys unless they were manually sticky-ed by right-clicking them
        for (auto keycode : keycodes)
        {
            if (!m_ManuallyStickingModifiers[GetModifierID(keycode)])
            {
                vr_keyboard.SetKeyDown(keycode, false);

                //Look up correct button state to remove IsDown state
                int key_index = 0;
                for (const auto& key : vr_keyboard.GetLayout(current_sublayout))
                {
                    if (key.KeyCode == keycode)
                    {
                        m_ButtonStates[key_index].IsDown = false;
                        break;
                    }

                    key_index++;
                }
            }
        }

        m_UnstickModifiersLater = false;
    }

    m_IsAnyButtonHovered = false;

    int key_index = 0;
    for (const auto& key : vr_keyboard.GetLayout(current_sublayout))
    {
        ButtonLaserState& button_state = m_ButtonStates[key_index];
        ImGui::PushID(key_index);

        //This accounts for the spacing that is missing with wider keys so rows still line up and also forces integer values
        float key_width  = (float)int( base_width * key.Width  + (ImGui::GetStyle().ItemInnerSpacing.x * (key.Width  - 1.0f)) );
        float key_height = (float)int( base_width * key.Height + (ImGui::GetStyle().ItemInnerSpacing.y * (key.Height - 1.0f)) );

        switch (key.KeyType)
        {
            case kbdlayout_key_blank_space:
            {
                ImGui::Dummy({key_width, key_height});
                break;
            }
            case kbdlayout_key_virtual_key:
            {
                bool is_down = vr_keyboard.GetKeyDown(key.KeyCode);
                bool push_color = ((is_down) && (button_state.IsDown));  //Prevent flickering when repeat is active and there are multiple keys with the same key code

                if (push_color)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                //We don't want key repeat on backspace for ImGui since it does that itself
                bool repeat_on_backspace = ((!vr_keyboard.IsTargetUI()) || (key.KeyCode != VK_BACK));

                if ( (ButtonLaser(key.Label.c_str(), {key_width, key_height}, button_state)) && (use_key_repeat) && (repeat_on_backspace) )
                {
                    (is_down) ? OnVirtualKeyUp(key.KeyCode, key.BlockModifiers) : OnVirtualKeyDown(key.KeyCode, key.BlockModifiers);
                    button_state.IsDown = !button_state.IsDown;
                }

                if (button_state.IsActivated)
                {
                    OnVirtualKeyDown(key.KeyCode, key.BlockModifiers);
                    button_state.IsDown = true;
                }
                else if (button_state.IsDeactivated)
                {
                    OnVirtualKeyUp(key.KeyCode, key.BlockModifiers);
                    button_state.IsDown = false;
                }

                //Right click to toggle key
                if (button_state.IsRightClicked)
                {
                    (is_down) ? OnVirtualKeyUp(key.KeyCode, key.BlockModifiers) : OnVirtualKeyDown(key.KeyCode, key.BlockModifiers);
                    button_state.IsDown = !button_state.IsDown;

                    SetManualStickyModifierState(key.KeyCode, !is_down);
                }

                if (push_color)
                    ImGui::PopStyleColor();

                break;
            }
            case kbdlayout_key_virtual_key_toggle:
            {
                if (use_key_repeat)
                    ImGui::PushButtonRepeat(false);

                bool is_down = vr_keyboard.GetKeyDown(key.KeyCode);
                bool push_color = ((is_down) && (button_state.IsDown));

                if (push_color)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                //Allow right click too to be consistent with toggling normal virtual keys
                if ( (ButtonLaser(key.Label.c_str(), {key_width, key_height}, button_state)) || (button_state.IsRightClicked) )
                {
                    (is_down) ? OnVirtualKeyUp(key.KeyCode) : OnVirtualKeyDown(key.KeyCode);
                    button_state.IsDown = !button_state.IsDown;

                    if (button_state.IsRightClicked)
                    {
                        SetManualStickyModifierState(key.KeyCode, !is_down);
                    }
                }

                if (push_color)
                    ImGui::PopStyleColor();

                if (use_key_repeat)
                    ImGui::PopButtonRepeat();

                break;
            }
            case kbdlayout_key_virtual_key_iso_enter:
            {
                //This one's a bit of a mess, but builds an ISO-Enter shaped button out of two key entries
                //First step is the top "key". Its label is unused, but the width is.
                //Second step is the bottom "key". It stretches itself over the row above it and hosts the label.
                //First and second step used invisible buttons first to check item state and have it synced up,
                //then in the second step there are two visual-only buttons used with style color adjusted to the state of the invisible buttons
                //
                //...now that a custom button implementation is used anyways this could be solved more cleanly... but it still works, so eh.

                const bool is_bottom_key = (iso_enter_top_index != -1);
                const ImVec2 cursor_pos = ImGui::GetCursorPos();
                float offset_y = 0.0f;

                //If second ISO-enter key, offset cursor to the previous row and stretch the button down to the end of the current row
                if (is_bottom_key)
                {
                    offset_y = ImGui::GetStyle().ItemSpacing.y + base_width;
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - offset_y);
                }
                else //else remember the width of the top part for later
                {
                    iso_enter_top_pos   = cursor_pos;
                    iso_enter_top_width = key_width;
                    iso_enter_top_index = key_index;
                }

                //Use an invisible button to collect active state from either (ImGui::InivisbleButton() does not pass button repeat flag, so can't be used here)
                ImGui::PushStyleColor(ImGuiCol_Button,        {0.0f, 0.0f, 0.0f, 0.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.0f, 0.0f, 0.0f, 0.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.0f, 0.0f, 0.0f, 0.0f});

                if ( (ButtonLaser("##IsoEnterDummy", {key_width, base_width + offset_y}, button_state)) && (use_key_repeat) )
                {
                    iso_enter_click_state = 3;
                }

                ImGui::PopStyleColor(3);

                if (button_state.IsHeld)
                {
                    iso_enter_button_state = 2;
                }

                if (button_state.IsHovered)
                {
                    if (button_state.IsRightClicked)
                    {
                        iso_enter_click_state = 4;
                    }

                    iso_enter_button_state = std::max(1, iso_enter_button_state);
                    iso_enter_hovered = true;
                }

                if (button_state.IsActivated)
                {
                    iso_enter_click_state = 1;
                }
                else if (button_state.IsDeactivated)
                {
                    iso_enter_click_state = 2;
                }

                //If second ISO-enter key, create two visible buttons that match the active state collected from the invisible buttons (but otherwise don't do anything)
                if (is_bottom_key)
                {
                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

                    bool push_color = ((vr_keyboard.GetKeyDown(key.KeyCode)) && (m_IsIsoEnterDown));

                    //Adjust button state depending on whether any invisible button was hovered to match ImGui behavior
                    if ( (iso_enter_button_state == 0) && (iso_enter_hovered) )
                        iso_enter_button_state = 1;
                    else if (!iso_enter_hovered)
                        iso_enter_button_state = 0;

                    if (iso_enter_button_state == 1)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
                    else if ( (iso_enter_button_state >= 2) || (push_color) )
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                    //Upper part without label
                    ImGui::SetCursorPos(iso_enter_top_pos);

                    //Shorten width by rounding so it doesn't stack corner AA
                    ButtonVisual("##IsoEnterTop", {iso_enter_top_width - ImGui::GetStyle().FrameRounding, base_width});

                    //Lower part with label
                    ImGui::SetCursorPos({cursor_pos.x, cursor_pos.y - offset_y});

                    ButtonVisual(key.Label.c_str(), {key_width, base_width + offset_y});

                    //React to button click state
                    if ( ( (iso_enter_click_state == 3) /*button clicked*/ && (use_key_repeat) ) || (iso_enter_click_state == 4) /*button right-clicked*/)
                    {
                        bool is_down = vr_keyboard.GetKeyDown(key.KeyCode);
                        (is_down) ? OnVirtualKeyUp(key.KeyCode) : OnVirtualKeyDown(key.KeyCode);

                        if (iso_enter_click_state == 4) //button right-clicked
                        {
                            SetManualStickyModifierState(key.KeyCode, !is_down);
                            m_IsIsoEnterDown = !m_IsIsoEnterDown;
                        }
                    }

                    if (iso_enter_click_state == 1)      //button pressed
                    {
                        OnVirtualKeyDown(key.KeyCode);
                        m_IsIsoEnterDown = true;
                    }
                    else if (iso_enter_click_state == 2) //button released
                    {
                        OnVirtualKeyUp(key.KeyCode);
                        m_IsIsoEnterDown = false;
                    }

                    if ( (iso_enter_button_state != 0) || (push_color) )
                        ImGui::PopStyleColor();

                    //Sync up button hover state for haptics
                    m_ButtonStates[iso_enter_top_index].IsHovered = iso_enter_hovered;
                    button_state.IsHovered = iso_enter_hovered;

                    //Restore cursor to normal row position
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::SetCursorPosY( (key.IsRowEnd) ? ImGui::GetCursorPosY() : ImGui::GetCursorPosY() + offset_y);
                    ImGui::Dummy({0.0f, base_width});
                    ImGui::SetPreviousLineHeight(ImGui::GetPreviousLineHeight() - offset_y);
                }
                break;
            }
            case kbdlayout_key_string:
            {
                bool is_down = button_state.IsDown;

                if (is_down)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                if ( (ButtonLaser(key.Label.c_str(), {key_width, key_height}, button_state)) && (use_key_repeat) )
                {
                    (button_state.IsDown) ? OnStringKeyUp(key.KeyString) : OnStringKeyDown(key.KeyString);
                    button_state.IsDown = !button_state.IsDown;
                }

                if (button_state.IsActivated)
                {
                    OnStringKeyDown(key.KeyString);
                    button_state.IsDown = true;
                }
                else if (button_state.IsDeactivated)
                {
                    OnStringKeyUp(key.KeyString);
                    button_state.IsDown = false;
                }

                //Right click to toggle key, only works properly when string maps to virtual key
                if (button_state.IsRightClicked)
                {
                    (button_state.IsDown) ? OnStringKeyUp(key.KeyString) : OnStringKeyDown(key.KeyString);
                    button_state.IsDown = !button_state.IsDown;
                }

                if (is_down)
                    ImGui::PopStyleColor();

                break;
            }
            case kbdlayout_key_sublayout_toggle:
            {
                if (use_key_repeat)
                    ImGui::PushButtonRepeat(false);

                bool is_down = (m_SubLayoutOverride == key.KeySubLayoutToggle);

                if (is_down)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                //Allow right click too to be consistent with toggling normal virtual keys
                if ( (ButtonLaser(key.Label.c_str(), {key_width, key_height}, button_state)) || (button_state.IsRightClicked) )
                {
                    m_SubLayoutOverride = (is_down) ? kbdlayout_sub_base : key.KeySubLayoutToggle;
                }

                if (is_down)
                    ImGui::PopStyleColor();

                if (use_key_repeat)
                    ImGui::PopButtonRepeat();

                break;
            }
            case kbdlayout_key_action:
            {
                bool is_down = button_state.IsDown;

                if (is_down)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                if ( (ButtonLaser(key.Label.c_str(), {key_width, key_height}, button_state)) && (use_key_repeat) )
                {
                    vr_keyboard.SetActionDown(key.KeyActionID, !button_state.IsDown);
                    button_state.IsDown = !button_state.IsDown;
                }

                if (button_state.IsActivated)
                {
                    vr_keyboard.SetActionDown(key.KeyActionID, true);
                    button_state.IsDown = true;
                }
                else if (button_state.IsDeactivated)
                {
                    vr_keyboard.SetActionDown(key.KeyActionID, false);
                    button_state.IsDown = false;
                }

                //Right click to toggle key
                if (button_state.IsRightClicked)
                {
                    vr_keyboard.SetActionDown(key.KeyActionID, !button_state.IsDown);
                    button_state.IsDown = !button_state.IsDown;
                }

                if (is_down)
                    ImGui::PopStyleColor();

                break;
            }
            default: break;
        }

        //Return to normal row position if the key was taller than 100%
        if (key.Height > 1.0f)
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - key_height + base_width);
            ImGui::SetPreviousLineHeight(ImGui::GetPreviousLineHeight() - key_height + base_width);
        }

        if (!key.IsRowEnd)
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }

        ImGui::PopID();

        key_index++;
    }

    m_LastSubLayout = current_sublayout;

    if (use_key_repeat)
        ImGui::PopButtonRepeat();

    m_KeyboardWidgetState.StoreCurrentState();
    widget_state_back.ApplyState();

    io.KeyRepeatDelay = key_repeat_delay_old;
}

void WindowKeyboard::OnWindowCloseButtonPressed()
{
    //Remove assignment on close button press to not just have to pop up again from overlay visbility tracking (but only when in room state)
    if (m_OverlayStateCurrentID == floating_window_ovrl_state_room)
    {
        SetAssignedOverlayID(-1);
    }
}

bool WindowKeyboard::IsVirtualWindowItemHovered() const
{
    return m_IsAnyButtonHovered;
}

void WindowKeyboard::OnVirtualKeyDown(unsigned char keycode, bool block_modifiers)
{
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    vr_keyboard.SetKeyDown(keycode, true, block_modifiers);

    if ((keycode != VK_BACK) && (keycode != VK_TAB))
    {
        HandleUnstickyModifiers(keycode);
    }
}

void WindowKeyboard::OnVirtualKeyUp(unsigned char keycode, bool block_modifiers)
{
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    vr_keyboard.SetKeyDown(keycode, false, block_modifiers);
}

void WindowKeyboard::OnStringKeyDown(const std::string& keystring)
{
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();
    HandleUnstickyModifiers();
    vr_keyboard.SetStringDown(keystring, true);
}

void WindowKeyboard::OnStringKeyUp(const std::string& keystring)
{
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();
    vr_keyboard.SetStringDown(keystring, false);
}

void WindowKeyboard::HandleUnstickyModifiers(unsigned char source_keycode)
{
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    //Skip if modifiers are set to be sticky
    if (ConfigManager::GetValue(configid_bool_input_keyboard_sticky_modifiers))
        return;

    //Schedule releasing modifier keys next frame if this wasn't called for any of them
    switch (source_keycode)
    {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
        {
            break;
        }

        default:
        {
            m_UnstickModifiersLater = true;
        }
    }
}

void WindowKeyboard::SetManualStickyModifierState(unsigned char keycode, bool is_down)
{
    int modifier_id = GetModifierID(keycode);
    if (modifier_id != -1)
    {
        m_ManuallyStickingModifiers[modifier_id] = is_down;
    }
}

int WindowKeyboard::GetModifierID(unsigned char keycode)
{
    switch (keycode)
    {
        case VK_SHIFT:
        {
            return 0;
            break;
        }
        case VK_CONTROL:
        {
            return VK_LCONTROL - VK_LSHIFT;
            break;
        }
        case VK_MENU:
        {
            return VK_LMENU - VK_LSHIFT;
            break;
        }
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
        {
            return keycode - VK_LSHIFT;
        }
        case VK_LWIN:
        case VK_RWIN:
        {
            return (keycode - VK_LWIN) + 7;
            break;
        }
        default:
        {
            break;
        }
    }

    return -1;
}

int WindowKeyboard::FindSameKeyInNewSubLayout(int key_index, KeyboardLayoutSubLayout sublayout_id_current, KeyboardLayoutSubLayout sublayout_id_new)
{
    //Return index of key at the position as sublayout_current's key_index key exists and has the same function (i.e. the same key, but possibly with different label and index)
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();
    const auto& sublayout_current = vr_keyboard.GetLayout(sublayout_id_current);
    const auto& sublayout_new     = vr_keyboard.GetLayout(sublayout_id_new);

    //Skip if index doesn't exist in layout for some reason
    if (sublayout_current.size() <= key_index)
        return -1;

    //Get key position in current layout
    int i = 0;
    ImVec2 key_current_pos;
    const KeyboardLayoutKey* key_ptr_current = nullptr;

    for (const auto& key : sublayout_current)
    {
        if (i == key_index)
        {
            key_ptr_current = &key;
            break;
        }

        key_current_pos.x += key.Width;

        if (key.IsRowEnd)
        {
            key_current_pos.x  = 0.0f;
            key_current_pos.y += 1.0f; //Key height doesn't matter, advance a row
        }

        i++;
    }

    if (key_ptr_current == nullptr)
        return -1;

    //Find key at position in new layout
    i = 0;
    ImVec2 key_new_pos;

    for (const auto& key : sublayout_new)
    {
        if ( (key_new_pos.y == key_current_pos.y) && (key_new_pos.x == key_current_pos.x) )
        {
            //Key exists at the same position, check if it's same function
            if (key_ptr_current->KeyType == key.KeyType)
            {
                switch (key.KeyType)
                {
                    case kbdlayout_key_virtual_key:
                    case kbdlayout_key_virtual_key_toggle:
                    case kbdlayout_key_virtual_key_iso_enter:
                    {
                        if (key.KeyCode == key_ptr_current->KeyCode)
                        {
                            return i;
                        }
                        break;
                    }
                    case kbdlayout_key_string:
                    {
                        if (key.KeyString == key_ptr_current->KeyString)
                        {
                            return i;
                        }
                        break;
                    }
                    case kbdlayout_key_sublayout_toggle:
                    {
                        if (key.KeySubLayoutToggle == key_ptr_current->KeySubLayoutToggle) //This one seems a bit useless
                        {
                            return i;
                        }
                        break;
                    }
                    case kbdlayout_key_action:
                    {
                        if (key.KeyActionID == key_ptr_current->KeyActionID)
                        {
                            return i;
                        }
                        break;
                    }
                }
            }

            return -1;
        }
        else if (key_new_pos.y > key_current_pos.y) //We went past, break
        {
            break;
        }

        key_new_pos.x += key.Width;

        if (key.IsRowEnd)
        {
            key_new_pos.x  = 0.0f;
            key_new_pos.y += 1.0f;
        }

        i++;
    }

    return -1;
}

LaserInputState& WindowKeyboard::GetLaserInputState(vr::TrackedDeviceIndex_t device_index)
{
    auto it = std::find_if(m_LaserInputStates.begin(), m_LaserInputStates.end(), [&](const auto& state){ return (state.DeviceIndex == device_index); });

    //Add if device isn't in the list yet
    if (it == m_LaserInputStates.end())
    {
        m_LaserInputStates.push_back(LaserInputState());
        it = m_LaserInputStates.end() - 1;
        it->DeviceIndex = device_index;
    }

    return *it;
}

bool WindowKeyboard::ButtonLaser(const char* label, const ImVec2& size_arg, ButtonLaserState& button_state)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g         = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id        = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    const ImVec2 half_spacing(style.ItemSpacing.x / 2.0f, style.ItemSpacing.y / 2.0f);

    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos.x, pos.y, pos.x + size.x, pos.y + size.y);
    const ImRect bb_with_spacing(bb.Min.x - half_spacing.x, bb.Min.y - half_spacing.y, bb.Max.x + half_spacing.x, bb.Max.y + half_spacing.y);

    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool do_repeat = (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat);

    //Check all laser input states for potential input on the button
    LaserInputState orig_state;
    orig_state.MouseState.SetFromGlobalState();

    const ImGuiMouseButton mouse_button       = ImGuiMouseButton_Left;
    const ImGuiMouseButton mouse_button_right = ImGuiMouseButton_Right;

    bool is_global_mouse_state_modified = false;
    bool is_any_hovering = false, is_pressed = false, is_held = button_state.IsHeld, is_released = false;

    button_state.IsRightClicked = false;

    for (auto& state : m_LaserInputStates)
    {
        bool hovered_spacing = bb_with_spacing.Contains(state.MouseState.MousePos);
        bool hovered = (hovered_spacing) ? bb.Contains(state.MouseState.MousePos) : false;

        if (hovered)
        {
            if (!button_state.IsHeld)
            {
                if (state.MouseState.MouseClicked[mouse_button])
                {
                    is_held = true;
                    is_pressed = !do_repeat;    //Only set pressed when not repeating (IsActivated/IsDeactivated is used instead then)
                    button_state.DeviceIndexHeld = state.DeviceIndex;
                }
                else if (state.MouseState.MouseClicked[mouse_button_right]) //Right-click is only a simple click state
                {
                    button_state.IsRightClicked = true;
                }
            }

            //Newly hovered, trigger haptics
            if (!button_state.IsHovered)
            {
                if (state.DeviceIndex == vr::k_unTrackedDeviceIndexOther) //other == mouse == current primary pointer device, don't use index
                {
                    UIManager::Get()->TriggerLaserPointerHaptics(GetOverlayHandle());
                }
                else
                {
                    UIManager::Get()->TriggerLaserPointerHaptics(GetOverlayHandle(), state.DeviceIndex);
                }
            }
        }

        if ( (button_state.IsHeld) && (button_state.DeviceIndexHeld == state.DeviceIndex) )
        {
            if (state.MouseState.MouseReleased[mouse_button])
            {
                button_state.DeviceIndexHeld = vr::k_unTrackedDeviceIndexInvalid;
                is_released = true;
            }
            else if ((do_repeat) && (state.MouseState.MouseDownDuration[mouse_button] > 0.0f))
            {
                //In order to access the button repeat behavior, we have to apply this mouse state to the global input state
                //As it is avoidable in all other cases we only do it here, which is a rarely hit code path
                state.MouseState.ApplyToGlobalState();
                is_global_mouse_state_modified = true;

                if (ImGui::IsMouseClicked(mouse_button, true))
                {
                    is_pressed = true;
                }
            }
        }

        if ( (state.DeviceIndex == vr::k_unTrackedDeviceIndexOther) && ( (hovered_spacing) || (button_state.IsHeld) ) )
        {
            m_IsAnyButtonHovered = true;    //This state is only used for blank space dragging. We include the spacing area in order to not activate drags from clicking in-between key spacing
        }

        if (hovered)
            is_any_hovering = hovered;
    }

    //Adjust button state
    button_state.IsActivated   = ((!button_state.IsHeld) && (is_held));
    button_state.IsDeactivated = ((button_state.IsHeld) && (is_released));
    button_state.IsHovered     = ((is_any_hovering) || (is_held));
    button_state.IsHeld        = (button_state.DeviceIndexHeld != vr::k_unTrackedDeviceIndexInvalid);
    //button_state.IsDown is not set by this function

    if (is_global_mouse_state_modified)
    {
        orig_state.MouseState.ApplyToGlobalState();
    }

    //Render button
    const ImU32 col = ImGui::GetColorU32((button_state.IsHeld) ? ImGuiCol_ButtonActive : (is_any_hovering) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    ImGui::RenderNavHighlight(bb, id);
    ImGui::RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

    const ImVec2 pos_min(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y);
    const ImVec2 pos_max(bb.Max.x - style.FramePadding.x, bb.Max.y - style.FramePadding.y);
    ImGui::RenderTextClipped(pos_min, pos_max, label, nullptr, &label_size, style.ButtonTextAlign, &bb);

    return is_pressed;
}

void WindowKeyboard::ButtonVisual(const char* label, const ImVec2& size_arg)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);
    const ImRect bb(pos.x, pos.y, pos.x + size.x, pos.y + size.y);

    //Render button
    const ImU32 col = ImGui::GetColorU32(ImGuiCol_Button);
    ImGui::RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

    const ImVec2 pos_min(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y);
    const ImVec2 pos_max(bb.Max.x - style.FramePadding.x, bb.Max.y - style.FramePadding.y);
    ImGui::RenderTextClipped(pos_min, pos_max, label, nullptr, &label_size, style.ButtonTextAlign, &bb);
}

vr::VROverlayHandle_t WindowKeyboard::GetOverlayHandle() const
{
    return UIManager::Get()->GetOverlayHandleKeyboard();
}

void WindowKeyboard::RebaseTransform()
{
    if ( (m_OverlayStateCurrentID == floating_window_ovrl_state_room) && (!m_OverlayStateCurrent->IsPinned) )
    {
        vr::HmdMatrix34_t hmd_mat = {0};
        vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

        vr::VROverlay()->GetOverlayTransformAbsolute(GetOverlayHandle(), &universe_origin, &hmd_mat);
        Matrix4 mat_abs = hmd_mat;
        Matrix4 mat_origin_inverse;

        int assigned_id = GetAssignedOverlayID();

        if (assigned_id >= 0)
        {
            mat_origin_inverse = OverlayManager::Get().GetOverlayCenterBottomTransform((unsigned int)assigned_id);
            UIManager::Get()->GetOverlayDragger().ApplyDashboardScale(mat_origin_inverse);
        }
        else
        {
            mat_origin_inverse = m_TransformUIOrigin;
        }

        mat_origin_inverse.invert();
        m_OverlayStateCurrent->Transform = mat_origin_inverse * mat_abs;
    }
    else
    {
        FloatingWindow::RebaseTransform();
    }
}

void WindowKeyboard::ResetTransform(FloatingWindowOverlayStateID state_id)
{
    FloatingWindow::ResetTransform(state_id);

    FloatingWindowOverlayState& overlay_state = GetOverlayState(state_id);

    //Offset keyboard position depending on the scaled size
    float overlay_height_m = (m_Size.y / m_Size.x) * OVERLAY_WIDTH_METERS_KEYBOARD;
    float offset_up        = -0.55f - ((overlay_height_m * overlay_state.Size) / 2.0f);

    overlay_state.Transform.rotateX(-45);
    overlay_state.Transform.translate_relative(0.0f, offset_up, 0.00f);

    //If visible, pinned and dplus dashboard overlay not available, reset to transform useful outside of the dashboard
    if ( (state_id == m_OverlayStateCurrentID) && (overlay_state.IsVisible) && (overlay_state.IsPinned) && (UIManager::Get()->IsOpenVRLoaded()) && 
         (!vr::VROverlay()->IsOverlayVisible(UIManager::Get()->GetOverlayHandleDPlusDashboard())) )
    {
        //Get dashboard-similar transform and adjust it down a bit
        Matrix4 matrix_facing = ComputeHMDFacingTransform(1.15f);
        matrix_facing.translate_relative(0.0f, -0.50f, 0.0f);

        //dplus_tab origin contains dashboard scale, so get that scale and apply it to this transform to stay consistent in size
        Matrix4 mat_origin = UIManager::Get()->GetOverlayDragger().GetBaseOffsetMatrix(ovrl_origin_dplus_tab);
        Vector3 row_1(mat_origin[0], mat_origin[1], mat_origin[2]);

        Vector3 translation = matrix_facing.getTranslation();
        matrix_facing.setTranslation({0.0f, 0.0f, 0.0f});
        matrix_facing.scale(row_1.length());
        matrix_facing.setTranslation(translation);

        //Apply facing transform to normal keyboard position
        overlay_state.Transform = matrix_facing * overlay_state.Transform;

        //Set transform directly as it may not be updated automatically
        vr::HmdMatrix34_t matrix_ovr = overlay_state.Transform.toOpenVR34();
        vr::VROverlay()->SetOverlayTransformAbsolute(GetOverlayHandle(), vr::TrackingUniverseStanding, &matrix_ovr);
    }
}

void WindowKeyboard::SetAssignedOverlayID(int assigned_id)
{
    (m_OverlayStateCurrentID == floating_window_ovrl_state_dashboard_tab) ? m_AssignedOverlayIDDashboardTab = assigned_id : m_AssignedOverlayIDRoom = assigned_id;
}

void WindowKeyboard::SetAssignedOverlayID(int assigned_id, FloatingWindowOverlayStateID state_id)
{
    (state_id == floating_window_ovrl_state_dashboard_tab) ? m_AssignedOverlayIDDashboardTab = assigned_id : m_AssignedOverlayIDRoom = assigned_id;
}

int WindowKeyboard::GetAssignedOverlayID() const
{
    return (m_OverlayStateCurrentID == floating_window_ovrl_state_dashboard_tab) ? m_AssignedOverlayIDDashboardTab : m_AssignedOverlayIDRoom;
}

int WindowKeyboard::GetAssignedOverlayID(FloatingWindowOverlayStateID state_id) const
{
    return (state_id == floating_window_ovrl_state_dashboard_tab) ? m_AssignedOverlayIDDashboardTab : m_AssignedOverlayIDRoom;
}

bool WindowKeyboard::IsHovered() const
{
    return m_IsHovered;
}

void WindowKeyboard::ResetButtonState()
{
    m_ButtonStates.clear();
    m_IsIsoEnterDown = false;
}

bool WindowKeyboard::HandleOverlayEvent(const vr::VREvent_t& vr_event)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;

    vr::TrackedDeviceIndex_t device_index = vr_event.trackedDeviceIndex;

    //Patch up mouse and button state when device is primary device and there's still a separate input state
    if (device_index == ConfigManager::Get().GetPrimaryLaserPointerDevice())
    {
        auto it = std::find_if(m_LaserInputStates.begin(), m_LaserInputStates.end(), [&](const auto& state){ return (state.DeviceIndex == device_index); });

        if (it != m_LaserInputStates.end())
        {
            for (ButtonLaserState& button_state : m_ButtonStates)
            {
                if (button_state.DeviceIndexHeld == device_index)
                {
                    button_state.DeviceIndexHeld = vr::k_unTrackedDeviceIndexOther; //Other == global mouse
                }
            }

            it->MouseState.ApplyToGlobalState();

            m_LaserInputStates.erase(it);
        }

        return false;
    }

    //Skip if no valid device index or it's the system pointer (sends as device 0, HMD)
    if ( (device_index >= vr::k_unMaxTrackedDeviceCount) || (vr::VROverlay()->GetPrimaryDashboardDevice() != vr::k_unTrackedDeviceIndexInvalid) )
        return false;

    switch (vr_event.eventType)
    {
        case vr::VREvent_MouseMove:
        {
            LaserInputState& state = GetLaserInputState(device_index);
            state.MouseState.MousePos = ImVec2(vr_event.data.mouse.x, -vr_event.data.mouse.y + ImGui::GetIO().DisplaySize.y);
            return true;
        }
        case vr::VREvent_FocusEnter:
        {
            return true;
        }
        case vr::VREvent_FocusLeave:
        {
            LaserInputState& state = GetLaserInputState(device_index);

            //Pointer left the overlay
            state.MouseState.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

            //Release mouse buttons if they're still down (not the case in most situations)
            std::fill(std::begin(state.MouseState.MouseDown), std::end(state.MouseState.MouseDown), false);
            return true;
        }
        case vr::VREvent_MouseButtonDown:
        case vr::VREvent_MouseButtonUp:
        {
            LaserInputState& state = GetLaserInputState(device_index);
            ImGuiMouseButton button = ImGuiMouseButton_Left;

            switch (vr_event.data.mouse.button)
            {
                case vr::VRMouseButton_Left:    button = ImGuiMouseButton_Left;   break;
                case vr::VRMouseButton_Right:   button = ImGuiMouseButton_Right;  break;
                case vr::VRMouseButton_Middle:  button = ImGuiMouseButton_Middle; break;
            }

            state.MouseState.MouseDown[button] = (vr_event.eventType == vr::VREvent_MouseButtonDown);
            return true;
        }
        case vr::VREvent_ScrollDiscrete:
        case vr::VREvent_ScrollSmooth:
        {
            //Unhandled, but still consume them
            return true;
        }
    }

    return false;
}

void WindowKeyboard::LaserSetMousePos(vr::TrackedDeviceIndex_t device_index, ImVec2 pos)
{
    LaserInputState& state = GetLaserInputState(device_index);
    state.MouseState.MousePos = pos;
}

void WindowKeyboard::LaserSetMouseButton(vr::TrackedDeviceIndex_t device_index, ImGuiMouseButton button_index, bool is_down)
{
    LaserInputState& state = GetLaserInputState(device_index);
    state.MouseState.MouseDown[button_index] = is_down;
}