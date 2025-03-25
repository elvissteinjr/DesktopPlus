#include "WindowKeyboard.h"

#include "ImGuiExt.h"
#include "TextureManager.h"
#include "TranslationManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "OpenVRExt.h"
#include "UIManager.h"
#include "OverlayManager.h"

#include <sstream>

#include "imgui_internal.h"

//-WindowKeyboard
WindowKeyboard::WindowKeyboard() : 
    m_WindowWidth(-1.0f),
    m_IsAutoVisible(false),
    m_IsHovered(false),
    m_IsAnyButtonHovered(false),
    m_AssignedOverlayIDRoom(-1),
    m_AssignedOverlayIDDashboardTab(-1),
    m_IsIsoEnterDown(false),
    m_IsDashboardPointerActiveLast(false),
    m_UnstickModifiersLater(false),
    m_SubLayoutOverride(kbdlayout_sub_base),
    m_LastSubLayout(kbdlayout_sub_base)
{
    m_WindowIcon = tmtex_icon_xsmall_keyboard;
    m_OvrlWidth    = OVERLAY_WIDTH_METERS_KEYBOARD;
    m_OvrlWidthMax = OVERLAY_WIDTH_METERS_KEYBOARD * 5.0f;

    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_keyboard);
    m_Size = {float(rect.GetWidth() - 4), float(rect.GetHeight() - 4)};
    m_SizeUnscaled = m_Size;

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
            //Auto-visible keyboards don't persist between overlay state switches
            if (m_IsAutoVisible)
            {
                SetAssignedOverlayID(-1);
                Hide();
            }

            OverlayStateSwitchCurrent(!is_using_dashboard_state);
        }
    }

    //Overlay position and visibility
    if (UIManager::Get()->IsOpenVRLoaded())
    {
        vr::VROverlayHandle_t ovrl_handle_assigned = vr::k_ulOverlayHandleInvalid;
        vr::VROverlayHandle_t overlay_handle = GetOverlayHandle();

        int assigned_overlay_id = GetAssignedOverlayID();
        bool assigned_overlay_use_fallback_origin = false;

        if ( (m_OverlayStateCurrentID == floating_window_ovrl_state_room) && (assigned_overlay_id >= 0) )
        {
            const OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)assigned_overlay_id);
            ovrl_handle_assigned = data.ConfigHandle[configid_handle_overlay_state_overlay_handle];
            assigned_overlay_use_fallback_origin = (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen); //Theater Screen uses fallback origin if not pinned

            if (ovrl_handle_assigned != vr::k_ulOverlayHandleInvalid)
            {
                if (m_OverlayStateCurrent->IsVisible != vr::VROverlay()->IsOverlayVisible(ovrl_handle_assigned))
                {
                    (m_OverlayStateCurrent->IsVisible) ? Hide() : Show();
                }
            }
            else  //Overlay doesn't exist anymore, remove assignment
            {
                SetAssignedOverlayID(-1);
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
            if ( (ovrl_handle_assigned != vr::k_ulOverlayHandleInvalid) && (!assigned_overlay_use_fallback_origin) )    //Based on assigned overlay
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
    switch (UIManager::Get()->GetVRKeyboard().GetInputTarget())
    {
        case kbdtarget_desktop: m_WindowTitleStrID = tstr_KeyboardWindowTitle;         break;
        case kbdtarget_ui:      m_WindowTitleStrID = tstr_KeyboardWindowTitleSettings; break;
        case kbdtarget_overlay:
        {
            m_WindowTitleStrID = tstr_NONE;
            m_WindowTitle = TranslationManager::Get().GetString(tstr_KeyboardWindowTitleOverlay);

            const unsigned int target_id = UIManager::Get()->GetVRKeyboard().GetInputTargetOverlayID();

            if (target_id < OverlayManager::Get().GetOverlayCount())
            {
                StringReplaceAll(m_WindowTitle, "%OVERLAYNAME%", OverlayManager::Get().GetConfigData(target_id).ConfigNameStr);
            }
            else
            {
                StringReplaceAll(m_WindowTitle, "%OVERLAYNAME%", TranslationManager::Get().GetString(tstr_KeyboardWindowTitleOverlayUnknown));
            }
        }
    }

    //Update UI origin transform when newly visible, used when there's no overlay assignment to base the position off of
    if ( (m_Alpha == 0.0f) && (UIManager::Get()->IsOpenVRLoaded()) )
    {
        //Get dashboard-similar transform and adjust it down a bit
        Matrix4 matrix_facing = vr::IVRSystemEx::ComputeHMDFacingTransform(1.15f);
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
    m_IsAutoVisible = false;

    //Refuse to hide if window is currently being dragged and remove assignment
    if ( (UIManager::Get()->GetOverlayDragger().IsDragActive()) && (UIManager::Get()->GetOverlayDragger().GetDragOverlayHandle() == GetOverlayHandle()) )
    {
        SetAssignedOverlayID(-1);

        return;
    }

    FloatingWindow::Hide(skip_fade);

    UIManager::Get()->GetVRKeyboard().OnWindowHidden();
}

bool WindowKeyboard::SetAutoVisibility(int assigned_overlay_id, bool show)
{
    if (show)
    {
        if (!IsVisible())
        {
            SetAssignedOverlayID(assigned_overlay_id);
            m_IsAutoVisible = true;

            //This will not have a smooth transition if there was another auto-visible keyboard right before this, but let's skip the effort for that for now
            Show();

            return true;
        }
    }
    else if ( (m_IsAutoVisible) && (GetAssignedOverlayID() == assigned_overlay_id) )
    {
        //Don't auto-hide while any buttons are being hovered, mostly relevant for the title bar buttons
        if ((assigned_overlay_id == -2) && (m_IsAnyButtonHovered))
        {
            return false;
        }

        SetAssignedOverlayID(-1);
        Hide();

        return true;
    }

    return false;
}

void WindowKeyboard::WindowUpdate()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
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

    if (!UIManager::Get()->IsInDesktopMode())
    {
        vr::TrackedDeviceIndex_t primary_device = ConfigManager::Get().GetPrimaryLaserPointerDevice();
        bool dashboard_device_exists = vr::IVROverlayEx::IsSystemLaserPointerActive();

        //Overlay leave events are not processed when a dashboard pointer exists changes active state, so make sure to get the pointers out of the way anyways
        if (dashboard_device_exists != m_IsDashboardPointerActiveLast)
        {
            for (auto& state : m_LaserInputStates)
            {
                //Pointer left the overlay
                state.MouseState.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

                //Release mouse buttons if they're held down
                std::fill(std::begin(state.MouseState.MouseDown), std::end(state.MouseState.MouseDown), false);
            }

            state_real.MouseState.ApplyToGlobalState();

            m_IsDashboardPointerActiveLast = dashboard_device_exists;
        }

        //Render pointer blobs for any input that isn't already controlled by the mouse
        //Use pos/size from ImGui since our stored members describe center pivoted max window space
        const ImVec2 cur_pos  = ImGui::GetWindowPos();
        const ImVec2 cur_size = ImGui::GetWindowSize();
        const ImRect window_bb(cur_pos.x, cur_pos.y, cur_pos.x + cur_size.x, cur_pos.y + cur_size.y);

        for (auto& state : m_LaserInputStates)
        {
            if ( (state.DeviceIndex != vr::k_unTrackedDeviceIndexOther) && (state.DeviceIndex != primary_device) )
            {
                //Only render if inside window
                if ((!dashboard_device_exists) && (window_bb.Contains(state.MouseState.MousePos)))
                {
                    ImGui::GetForegroundDrawList()->AddCircleFilled(state.MouseState.MousePos, 7.0f, ImGui::GetColorU32(Style_ImGuiCol_SteamVRCursorBorder));
                    ImGui::GetForegroundDrawList()->AddCircleFilled(state.MouseState.MousePos, 5.0f, ImGui::GetColorU32(Style_ImGuiCol_SteamVRCursor));
                }

                //Also advance the mouse state as if it was managed by ImGui
                state.MouseState.Advance();
            }
        }
    }

    //Increased key repeat delay and rate for the VR keyboard buttons
    const float key_repeat_delay_old  = io.KeyRepeatDelay;
    const float key_repeat_delay_rate = io.KeyRepeatRate;
    io.KeyRepeatDelay = 0.5f;
    io.KeyRepeatRate  = 0.025f;

    //Exclude title bar buttons from hovered state to get regular widget focus behavior on them
    const bool is_hovering_titlebar_buttons = ((ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenBlockedByPopup)) && 
                                               (io.MousePos.y < ImGui::GetCursorScreenPos().y - style.WindowPadding.y) && (!m_IsTitleBarHovered));

    m_IsHovered = ((ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenBlockedByPopup)) && (!is_hovering_titlebar_buttons));

    //Keyboard buttons use their own window-local active widget state in order to not interrupt active InputText() widgets. This is a dirty hack, but works for now
    ImGui::ActiveWidgetStateStorage widget_state_back;
    widget_state_back.StoreCurrentState();

    m_KeyboardWidgetState.AdvanceState();
    m_KeyboardWidgetState.ApplyState();

    //Enable button repeat if setting is enabled
    const bool use_key_repeat_global = ConfigManager::GetValue(configid_bool_input_keyboard_key_repeat);
    if (use_key_repeat_global)
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

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

        const bool sticky_modifiers_enabled = ConfigManager::GetValue(configid_bool_input_keyboard_sticky_modifiers);

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
                            vr_keyboard.SetActionDown(key.KeyActionUID, false);
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

    //Default to title bar button hover state as m_IsAnyButtonHovered is checked for blocking auto-hiding
    m_IsAnyButtonHovered = is_hovering_titlebar_buttons;

    int key_index = 0;
    ImVec2 cursor_pos = ImGui::GetCursorPos();
    for (const auto& key : vr_keyboard.GetLayout(current_sublayout))
    {
        ButtonLaserState& button_state = m_ButtonStates[key_index];
        ImGui::PushID(key_index);

        //Keep cursor pos on integer values
        cursor_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos({ceilf(cursor_pos.x), ceilf(cursor_pos.y)});

        //This accounts for the spacing that is missing with wider keys so rows still line up and also forces integer values
        const float key_width_f = base_width * key.Width  + (style.ItemInnerSpacing.x * (key.Width  - 1.0f));
        const float key_height  = (float)(int)( base_width * key.Height + (style.ItemInnerSpacing.y * (key.Height - 1.0f)) );
        float key_width         = (float)(int)( key_width_f );

        //Add an extra pixel of width if the untruncated values would push it further
        //There might be a smarter way to do this (simple rounding doesn't seem to be it), but this helps the keys align while rendering on full pixels only
        if (cursor_pos.x + key_width_f > ImGui::GetCursorPosX() + key_width)
        {
            key_width += 1.0f;
        }

        //Disable button repeat if the individual key has its key repeat disabled (avoid doing it when it's already off though)
        const bool use_key_repeat = ((use_key_repeat_global) && (!key.NoRepeat));

        if ((use_key_repeat_global) && (!use_key_repeat))
            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, false);

        switch (key.KeyType)
        {
            case kbdlayout_key_blank_space:
            {
                ImGui::Dummy({key_width, key_height});
                break;
            }
            case kbdlayout_key_virtual_key:
            {
                const bool is_down = vr_keyboard.GetKeyDown(key.KeyCode);
                const bool push_color = ((is_down) && (button_state.IsDown));  //Prevent flickering when repeat is active and there are multiple keys with the same key code

                if (push_color)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                //We don't want key repeat on backspace for ImGui since it does that itself
                const bool repeat_on_backspace = ((vr_keyboard.GetInputTarget() != kbdtarget_ui) || (key.KeyCode != VK_BACK));

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
                    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, false);

                const bool is_down = vr_keyboard.GetKeyDown(key.KeyCode);
                const bool push_color = ((is_down) && (button_state.IsDown));

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
                    ImGui::PopItemFlag();

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
                    offset_y = style.ItemSpacing.y + base_width;
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
                    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

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
                    ButtonVisual("##IsoEnterTop", {iso_enter_top_width - style.FrameRounding, base_width});

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
                const bool is_down = button_state.IsDown;

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
                    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, false);

                const bool is_down = (m_SubLayoutOverride == key.KeySubLayoutToggle);

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
                    ImGui::PopItemFlag();

                break;
            }
            case kbdlayout_key_action:
            {
                const bool is_down = button_state.IsDown;

                if (is_down)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                if ( (ButtonLaser(key.Label.c_str(), {key_width, key_height}, button_state)) && (use_key_repeat) )
                {
                    vr_keyboard.SetActionDown(key.KeyActionUID, !button_state.IsDown);
                    button_state.IsDown = !button_state.IsDown;
                }

                if (button_state.IsActivated)
                {
                    vr_keyboard.SetActionDown(key.KeyActionUID, true);
                    button_state.IsDown = true;
                }
                else if (button_state.IsDeactivated)
                {
                    vr_keyboard.SetActionDown(key.KeyActionUID, false);
                    button_state.IsDown = false;
                }

                //Right click to toggle key
                if (button_state.IsRightClicked)
                {
                    vr_keyboard.SetActionDown(key.KeyActionUID, !button_state.IsDown);
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
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            cursor_pos.x += key_width_f;
        }

        //Undo disabled button repeat if necessary
        if ((use_key_repeat_global) && (!use_key_repeat))
            ImGui::PopItemFlag();

        ImGui::PopID();

        key_index++;
    }

    m_LastSubLayout = current_sublayout;

    if (use_key_repeat_global)
        ImGui::PopItemFlag();

    m_KeyboardWidgetState.StoreCurrentState();
    widget_state_back.ApplyState();

    io.KeyRepeatDelay = key_repeat_delay_old;
    io.KeyRepeatRate  = key_repeat_delay_rate;
}

void WindowKeyboard::OnWindowPinButtonPressed()
{
    FloatingWindow::OnWindowPinButtonPressed();

    //Disable auto-hiding that may happen when the keyboard is no longer hovered after pressing this
    m_IsAutoVisible = false;
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
    const int modifier_id = GetModifierID(keycode);
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
                        if (key.KeyActionUID == key_ptr_current->KeyActionUID)
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

    bool do_repeat = (g.LastItemData.ItemFlags & ImGuiItemFlags_ButtonRepeat);

    //Check all laser input states for potential input on the button
    LaserInputState orig_state;
    orig_state.MouseState.SetFromGlobalState();

    const ImGuiMouseButton mouse_button       = ImGuiMouseButton_Left;
    const ImGuiMouseButton mouse_button_right = ImGuiMouseButton_Right;

    bool is_global_mouse_state_modified = false;
    bool is_any_hovering = false, is_pressed = false, is_held = button_state.IsHeld, is_released = false;

    button_state.IsRightClicked = false;

    if ( (!UIManager::Get()->GetOverlayDragger().IsDragActive()) && (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) ) //Skip if an overlay drag is active
    {
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
    ImGui::RenderNavCursor(bb, id);
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
        bool assigned_overlay_use_fallback_origin = false;

        if (assigned_id >= 0)
        {
            const OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)assigned_id);
            assigned_overlay_use_fallback_origin = (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen); //Theater Screen uses fallback origin if not pinned
        }

        if ((!assigned_overlay_use_fallback_origin) && (assigned_id >= 0))
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
    const float overlay_height_m = (m_Size.y / m_Size.x) * OVERLAY_WIDTH_METERS_KEYBOARD;
    const float offset_up        = -0.55f - ((overlay_height_m * overlay_state.Size) / 2.0f);

    overlay_state.Transform.rotateX(-45);
    overlay_state.Transform.translate_relative(0.0f, offset_up, 0.00f);

    //If visible, pinned and dplus dashboard overlay not available, reset to transform useful outside of the dashboard
    if ( (state_id == m_OverlayStateCurrentID) && (overlay_state.IsVisible) && (overlay_state.IsPinned) && (UIManager::Get()->IsOpenVRLoaded()) && 
         (!vr::VROverlay()->IsOverlayVisible(UIManager::Get()->GetOverlayHandleDPlusDashboard())) )
    {
        //Get dashboard-similar transform and adjust it down a bit
        Matrix4 matrix_facing = vr::IVRSystemEx::ComputeHMDFacingTransform(1.15f);
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
    //Notify dashboard app for focus enter/leave in case there's a reason to react to it
    switch (vr_event.eventType)
    {
        case vr::VREvent_FocusEnter:
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_ovrl_focus_enter);
            break;
        }
        case vr::VREvent_FocusLeave:
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_ovrl_focus_leave);
            break;
        }
    }

    if (ImGui::GetCurrentContext() == nullptr)
        return false;

    vr::TrackedDeviceIndex_t device_index = vr_event.trackedDeviceIndex;

    //System laser pointer expresses a secondary pointer device as a cursor index instead of providing the device index as our custom laser pointer does
    uint32_t cursor_index = 0;
    switch (vr_event.eventType)
    {
        case vr::VREvent_MouseMove:
        case vr::VREvent_MouseButtonDown:
        case vr::VREvent_MouseButtonUp:
        {
            cursor_index = vr_event.data.mouse.cursorIndex;
            break;
        }
        case vr::VREvent_FocusEnter:
        case vr::VREvent_FocusLeave:
        {
            cursor_index = vr_event.data.overlay.cursorIndex;
            break;
        }
        case vr::VREvent_ScrollDiscrete:
        case vr::VREvent_ScrollSmooth:
        {
            cursor_index = vr_event.data.scroll.cursorIndex;
            break;
        }
        default: break;
    }

    //See if we already identified the device index for this cursor index if it's not the primary laser pointer
    if (cursor_index != 0)
    {
        auto it = std::find_if(m_LaserInputStates.begin(), m_LaserInputStates.end(), [&](const auto& state){ return (state.CursorIndexLast == cursor_index); });

        if (it != m_LaserInputStates.end())
        {
            device_index = it->DeviceIndex;
        }
    }

    //Patch up mouse and button state when device is primary device and there's still a separate input state
    if ((device_index == ConfigManager::Get().GetPrimaryLaserPointerDevice()) && (device_index != vr::k_unTrackedDeviceIndexInvalid))
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

    //Skip if no valid device index or it's the system pointer (sends as device 0 (HMD) or invalid)
    //For mouse move events with cursor indices above 0 we allow invalid devices as they're used to guess the correct one
    if ( ( (device_index >= vr::k_unMaxTrackedDeviceCount) || (device_index == vr::k_unTrackedDeviceIndex_Hmd) || 
          ((cursor_index == 0) && (vr::IVROverlayEx::IsSystemLaserPointerActive())) ) && ((cursor_index == 0) || (vr_event.eventType != vr::VREvent_MouseMove)) )
    {
        return (cursor_index != 0); //Still return true for non-0 index as we don't want it to be treated as ImGui mouse input
    }

    switch (vr_event.eventType)
    {
        case vr::VREvent_MouseMove:
        {
            //If we haven't seen this device on this cursor index before (or device index is invalid), guess which one it likely is based on the cursor position
            if (GetLaserInputState(device_index).CursorIndexLast != cursor_index)
            {
                Vector2 uv_pos(vr_event.data.mouse.x / ImGui::GetIO().DisplaySize.x, vr_event.data.mouse.y / ImGui::GetIO().DisplaySize.y);
                device_index = vr::IVROverlayEx::FindPointerDeviceForOverlay(UIManager::Get()->GetOverlayHandleKeyboard(), uv_pos);
            }

            if (device_index != vr::k_unTrackedDeviceIndexInvalid)
            {
                LaserInputState& state = GetLaserInputState(device_index);
                state.MouseState.MousePos = ImVec2(vr_event.data.mouse.x, -vr_event.data.mouse.y + ImGui::GetIO().DisplaySize.y);
                state.CursorIndexLast = cursor_index;
            }

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
            state.CursorIndexLast = 0;

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

    return (cursor_index != 0);
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


//-WindowKeyboardShortcuts
void WindowKeyboardShortcuts::SetActiveWidget(ImGuiID widget_id)
{
    //Set active widget ID if none is set, otherwise fade the previous one out first
    if (m_ActiveWidget == 0)
    {
        m_ActiveWidget = widget_id;
        m_ActiveWidgetPending = 0;
    }
    else if (m_ActiveWidget != widget_id)
    {
        m_ActiveWidgetPending = widget_id;
        m_IsFadingOut = true;
    }
}

void WindowKeyboardShortcuts::SetDefaultPositionDirection(ImGuiDir pos_dir, float y_offset)
{
    if (!m_IsFadingOut)
    {
        m_PosDirDefault = pos_dir;
        m_YOffsetDefault = y_offset;
    }
}

void WindowKeyboardShortcuts::Update(ImGuiID widget_id)
{
    if (widget_id != m_ActiveWidget)
        return;

    ImGuiIO& io = ImGui::GetIO();

    //Release previously held keys by button actions
    switch (m_ActiveButtonAction)
    {
        case btn_act_cut:
        {
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            io.AddKeyEvent(ImGuiKey_X, false);

            m_ActiveButtonAction = btn_act_none;
            break;
        }
        case btn_act_copy:
        {
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            io.AddKeyEvent(ImGuiKey_C, false);

            m_ActiveButtonAction = btn_act_none;
            break;
        }
        case btn_act_paste:
        {
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            io.AddKeyEvent(ImGuiKey_V, false);

            m_ActiveButtonAction = btn_act_none;
            break;
        }
        default: break;
    }

    const float offset_down = (m_PosDirDefault == ImGuiDir_Down) ? m_YOffsetDefault : 0.0f;
    const float offset_up   = (m_PosDirDefault == ImGuiDir_Up)   ? m_YOffsetDefault : 0.0f;

    const float pos_y      = ImGui::GetItemRectMin().y - ImGui::GetStyle().ItemSpacing.y + m_YOffsetDefault;
    const float pos_y_down = ImGui::GetItemRectMax().y + ImGui::GetStyle().ItemInnerSpacing.y + offset_down;
    const float pos_y_up   = ImGui::GetItemRectMin().y - ImGui::GetStyle().ItemSpacing.y - m_WindowHeight + offset_up;

    //Wait for window height to be known and stable before setting pos or animating fade/pos
    if ((m_WindowHeight != FLT_MIN) && (m_WindowHeight == m_WindowHeightPrev))
    {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, smoothstep(m_PosAnimationProgress, pos_y_down, pos_y_up) ));

        const float time_step = ImGui::GetIO().DeltaTime * 6.0f;

        m_Alpha += (!m_IsFadingOut) ? time_step : -time_step;

        if (m_Alpha > 1.0f)
            m_Alpha = 1.0f;
        else if (m_Alpha < 0.0f)
            m_Alpha = 0.0f;

        m_PosAnimationProgress += (m_PosDir == ImGuiDir_Up) ? time_step : -time_step;

        if (m_PosAnimationProgress > 1.0f)
            m_PosAnimationProgress = 1.0f;
        else if (m_PosAnimationProgress < 0.0f)
            m_PosAnimationProgress = 0.0f;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Alpha);

    //Get clipping rect of parent window
    ImGuiWindow* window_parent = ImGui::GetCurrentWindow();
    ImRect clip_rect = window_parent->ClipRect;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | 
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##VRKeyboardShortcuts", nullptr, flags);

    //Force this window to be in front. For this to be not a flicker-fest, the parent should have ImGuiWindowFlags_NoBringToFrontOnFocus set (default for FloatingWindow)
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!ImGui::IsWindowAbove(window, window_parent))
    {
        ImGui::BringWindowToDisplayFront(window);
    }

    //Transfer scroll input to parent window (which isn't a real parent window but just the one in the stack), so this window doesn't block scrolling
    ImGui::ScrollBeginStackParentWindow();

    //Use clipping rect of parent window
    ImGui::PushClipRect(clip_rect.Min, clip_rect.Max, false);

    //Draw background + border manually so it can be clipped properly
    ImRect window_rect = window->Rect();
    window->DrawList->AddRectFilled(window_rect.Min, window_rect.Max, ImGui::GetColorU32(ImGuiCol_PopupBg), window->WindowRounding);
    window->DrawList->AddRect(window_rect.Min, window_rect.Max, ImGui::GetColorU32(ImGuiCol_Border), window->WindowRounding, 0, window->WindowBorderSize);

    //Disable inputs when fading out
    if (m_IsFadingOut)
        ImGui::PushItemDisabledNoVisual();

    //Since we don't want to lose input focus, we use a separate widget state for this window
    ImGui::ActiveWidgetStateStorage prev_widget_state;
    prev_widget_state.StoreCurrentState();

    m_WindowWidgetState.AdvanceState();
    m_WindowWidgetState.ApplyState();


    //-Window buttons
    m_IsAnyButtonDown = false;

    if (ImGui::Button(TranslationManager::GetString(tstr_KeyboardShortcutsCut)))
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_X, true);

        m_ActiveButtonAction = btn_act_cut;
    }

    if (ImGui::IsItemActive())
    {
        m_IsAnyButtonDown = true;
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_KeyboardShortcutsCopy)))
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_C, true);

        m_ActiveButtonAction = btn_act_copy;
    }

    if (ImGui::IsItemActive())
    {
        m_IsAnyButtonDown = true;
    }

    ImGui::SameLine();

    //Check if we have clipboard text to disable pasting if we don't
    const char* clipboard_text = ImGui::GetClipboardText();
    const bool has_clipboard_text = ((clipboard_text != nullptr) && (clipboard_text[0] != '\0'));

    if (!has_clipboard_text)
        ImGui::PushItemDisabled();

    if (ImGui::Button(TranslationManager::GetString(tstr_KeyboardShortcutsPaste)))
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_V, true);

        m_ActiveButtonAction = btn_act_paste;
    }

    if (ImGui::IsItemActive())
    {
        m_IsAnyButtonDown = true;
    }

    if (!has_clipboard_text)
        ImGui::PopItemDisabled();

    m_IsHovered = ImGui::IsWindowHovered();

    //Restore global widget state
    m_WindowWidgetState.StoreCurrentState();
    prev_widget_state.ApplyState();

    if (m_IsFadingOut)
        ImGui::PopItemDisabledNoVisual();

    //Switch directions if there's no space in the default direction
    if (m_PosDirDefault == ImGuiDir_Down)
    {
        m_PosDir = (pos_y_down + ImGui::GetWindowSize().y > clip_rect.Max.y) ? ImGuiDir_Up : ImGuiDir_Down;
    }
    else
    {
        //Not using pos_y_up here as it's not valid yet (m_WindowHeight not set) 
        m_PosDir = (pos_y - ImGui::GetWindowSize().y < clip_rect.Min.y) ? ImGuiDir_Down : ImGuiDir_Up;
    }

    if (m_Alpha == 0.0f)
    {
        m_PosAnimationProgress = (m_PosDir == ImGuiDir_Down) ? 0.0f : 1.0f;
    }

    //Cache window height so it's available on the next frame before beginning the window
    m_WindowHeightPrev = m_WindowHeight;
    m_WindowHeight = ImGui::GetWindowSize().y;

    ImGui::End();

    ImGui::PopStyleVar(); //ImGuiStyleVar_Alpha

    //Reset when fade-out is done
    if ( (m_IsFadingOut) && (m_Alpha == 0.0f) )
    {
        m_ActiveWidget = m_ActiveWidgetPending;
        m_ActiveWidgetPending = 0;

        m_IsFadingOut = false;
        m_WindowHeight = FLT_MIN;
        m_PosDir = m_PosDirDefault;
        m_PosAnimationProgress = (m_PosDirDefault == ImGuiDir_Down) ? 0.0f : 1.0f;
    }
}

bool WindowKeyboardShortcuts::IsHovered() const
{
    return m_IsHovered;
}

bool WindowKeyboardShortcuts::IsAnyButtonDown() const
{
    return m_IsAnyButtonDown;
}
