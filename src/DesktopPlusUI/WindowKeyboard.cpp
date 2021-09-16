#include "WindowKeyboard.h"

#include "ImGuiExt.h"
#include "TextureManager.h"
#include "TranslationManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "UIManager.h"
#include "OverlayManager.h"

#include "Ini.h"

#include <sstream>

WindowKeyboard::WindowKeyboard() : 
    m_WindowWidth(-1.0f),
    m_IsHovered(false),
    m_HasHoveredNewItem(false),
    m_CurrentStringKeyDown(false),
    m_UnstickModifiersLater(false),
    m_SubLayoutOverride(kbdlayout_sub_base),
    m_LastSubLayout(kbdlayout_sub_base),
    m_ActiveKeyIndex(-1),
    m_ActiveKeyCode(0)
{
    m_WindowIcon = tmtex_icon_xsmall_keyboard;
    m_OvrlWidth = OVERLAY_WIDTH_METERS_KEYBOARD;

    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_keyboard);
    m_Size = {float(rect.GetWidth() - 4), float(rect.GetHeight() - 4)};

    m_Pos = {(float)rect.GetCenter().x, (float)rect.GetCenter().y};
    m_PosPivot = {0.5f, 0.5f};

    m_WindowFlags |= ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;

    std::fill(std::begin(m_ManuallyStickingModifiers), std::end(m_ManuallyStickingModifiers), false);
}

void WindowKeyboard::Show(bool skip_fade)
{
    FloatingWindow::Show(skip_fade);

    m_WindowTitleStrID = (UIManager::Get()->GetVRKeyboard().IsTargetUI()) ? tstr_KeyboardWindowTitleSettings : tstr_KeyboardWindowTitle;
}

void WindowKeyboard::Hide(bool skip_fade)
{
    FloatingWindow::Hide(skip_fade);

    UIManager::Get()->GetVRKeyboard().OnWindowHidden();
    m_HasHoveredNewItem = false;
}

void WindowKeyboard::WindowUpdate()
{
    ImGuiIO& io = ImGui::GetIO();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

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
    bool use_key_repeat = ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_key_repeat);
    if (use_key_repeat)
        ImGui::PushButtonRepeat(true);

    const float base_width = (float)int(ImGui::GetTextLineHeightWithSpacing() * 2.225f);

    //Used for kbdlayout_key_virtual_key_iso_enter key
    ImVec2 iso_enter_top_pos(-1.0f, -1.0f);
    float  iso_enter_top_width    = -1.0f;
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

    //Release keys that may have been held down when the sublayout switched (buttons won't fire release when they just disappear from that)
    if ((current_sublayout != m_LastSubLayout) && (m_ActiveKeyIndex != -1))
    {
        const auto& sublayout = vr_keyboard.GetLayout(m_LastSubLayout);

        if (sublayout.size() > m_ActiveKeyIndex)
        {
            const auto& key = sublayout[m_ActiveKeyIndex];

            switch (key.KeyType)
            {
                case kbdlayout_key_virtual_key:
                case kbdlayout_key_virtual_key_iso_enter:
                {
                    if (vr_keyboard.GetKeyDown(key.KeyCode))
                    {
                        OnVirtualKeyUp(key.KeyCode);
                    }
                    break;
                }
                case kbdlayout_key_string:
                {
                    OnStringKeyUp(key.KeyString);
                    break;
                }
                default: break;
            }
        }

        m_ActiveKeyIndex = -1;
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
            }
        }

        m_UnstickModifiersLater = false;
    }

    int key_index = 0;
    for (const auto& key : vr_keyboard.GetLayout(current_sublayout))
    {
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
                bool push_color = ((is_down) && (m_ActiveKeyCode != key.KeyCode));  //Prevent flickering when repeat is active and there are multiple keys with the same key code

                if (push_color)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                //We don't want key repeat on backspace for ImGui since it does that itself
                bool repeat_on_backspace = ((!vr_keyboard.IsTargetUI()) || (key.KeyCode != VK_BACK));

                if ( (ImGui::Button(key.Label.c_str(), {key_width, key_height})) && (use_key_repeat) && (repeat_on_backspace) )
                {
                    (is_down) ? OnVirtualKeyUp(key.KeyCode, key.BlockModifiers) : OnVirtualKeyDown(key.KeyCode, key.BlockModifiers);
                }

                if (ImGui::IsItemActivated())
                {
                    OnVirtualKeyDown(key.KeyCode, key.BlockModifiers);
                    m_ActiveKeyIndex = key_index;
                    m_ActiveKeyCode = key.KeyCode;
                }
                else if (ImGui::IsItemDeactivated())
                {
                    OnVirtualKeyUp(key.KeyCode, key.BlockModifiers);
                    m_ActiveKeyIndex = -1;
                    m_ActiveKeyCode = 0;
                }

                //Right click to toggle key (yes this doesn't work with string keys even if they'd map to a key code)
                if ( (ImGui::IsItemHovered()) && (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) )
                {
                    (is_down) ? OnVirtualKeyUp(key.KeyCode, key.BlockModifiers) : OnVirtualKeyDown(key.KeyCode, key.BlockModifiers);
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
                bool push_color = ((is_down) && (m_ActiveKeyCode != key.KeyCode));

                if (push_color)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                //Allow right click too to be consistent with toggling normal virtual keys
                if ( (ImGui::Button(key.Label.c_str(), {key_width, key_height})) || ( (ImGui::IsItemHovered()) && (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) ) )
                {
                    (is_down) ? OnVirtualKeyUp(key.KeyCode) : OnVirtualKeyDown(key.KeyCode);

                    if ( (ImGui::IsItemHovered()) && (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) )
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
                //then in the second step there are two normal buttons used with style color adjusted to the state of the invisible buttons

                const bool is_bottom_key = (iso_enter_top_width != -1.0f);
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
                }

                //Use an invisible button to collect active state from either (ImGui::InivisbleButton() does not pass button repeat flag, so can't be used here)
                ImGui::PushStyleColor(ImGuiCol_Button,        {0.0f, 0.0f, 0.0f, 0.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.0f, 0.0f, 0.0f, 0.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.0f, 0.0f, 0.0f, 0.0f});

                if ( (ImGui::Button("##IsoEnterDummy", {key_width, base_width + offset_y})) && (use_key_repeat) )
                {
                    iso_enter_click_state = 3;
                }

                ImGui::PopStyleColor(3);

                if (ImGui::IsItemActive())
                {
                    iso_enter_button_state = 2;
                }
                    
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenOverlapped))
                {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    {
                        iso_enter_click_state = 4;
                    }

                    iso_enter_button_state = std::max(1, iso_enter_button_state);
                    iso_enter_hovered = true;
                }

                if (ImGui::IsItemActivated())
                {
                    iso_enter_click_state = 1;
                }
                else if (ImGui::IsItemDeactivated())
                {
                    iso_enter_click_state = 2;
                }

                //If second ISO-enter key, create two visible buttons that match the active state collected from the invisible buttons (but otherwise don't do anything)
                if (is_bottom_key)
                {
                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

                    bool push_color = ((vr_keyboard.GetKeyDown(key.KeyCode)) && (m_ActiveKeyCode != key.KeyCode));

                    //Adjust button state depending on whether any invisible button was hovered to match ImGui behavior
                    if ( (iso_enter_button_state == 0) && (iso_enter_hovered) )
                        iso_enter_button_state = 1;
                    else if (!iso_enter_hovered)
                        iso_enter_button_state = 0;

                    if (iso_enter_button_state == 1)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
                    else if ( (iso_enter_button_state >= 2) || (push_color) )
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_Button));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_Button));

                    //Upper part without label
                    ImGui::SetCursorPos(iso_enter_top_pos);

                    //Shorten width by rounding so it doesn't stack corner AA
                    ImGui::Button("##IsoEnterTop", {iso_enter_top_width - ImGui::GetStyle().FrameRounding, base_width});

                    //Lower part with label
                    ImGui::SetCursorPos({cursor_pos.x, cursor_pos.y - offset_y});

                    ImGui::Button(key.Label.c_str(), {key_width, base_width + offset_y});

                    //React to button click state
                    if ( ( (iso_enter_click_state == 3) /*button clicked*/ && (use_key_repeat) ) || (iso_enter_click_state == 4) /*button right-clicked*/)
                    {
                        bool is_down = vr_keyboard.GetKeyDown(key.KeyCode);
                        (is_down) ? OnVirtualKeyUp(key.KeyCode) : OnVirtualKeyDown(key.KeyCode);

                        if (iso_enter_click_state == 4) //button right-clicked
                        {
                            SetManualStickyModifierState(key.KeyCode, !is_down);
                        }
                    }

                    if (iso_enter_click_state == 1)      //button pressed
                    {
                        OnVirtualKeyDown(key.KeyCode);
                        m_ActiveKeyIndex = key_index;
                        m_ActiveKeyCode = key.KeyCode;
                    }
                    else if (iso_enter_click_state == 2) //button released
                    {
                        OnVirtualKeyUp(key.KeyCode);
                        m_ActiveKeyIndex = -1;
                        m_ActiveKeyCode = 0;
                    }

                    if ( (iso_enter_button_state != 0) || (push_color) )
                        ImGui::PopStyleColor();

                    ImGui::PopStyleColor(2);

                    //Restore cursor to normal row position
                    if (!key.IsRowEnd)
                    {
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset_y);
                        ImGui::Dummy({0.0f, base_width});
                        ImGui::SetPreviousLineHeight(ImGui::GetPreviousLineHeight() - offset_y);
                    }
                }
                break;
            }
            case kbdlayout_key_string:
            {
                if ( (ImGui::Button(key.Label.c_str(), {key_width, key_height})) && (use_key_repeat) )
                {
                    (m_CurrentStringKeyDown) ? OnStringKeyUp(key.KeyString) : OnStringKeyDown(key.KeyString);
                }

                if (ImGui::IsItemActivated())
                {
                    OnStringKeyDown(key.KeyString);
                    m_ActiveKeyIndex = key_index;
                }
                else if (ImGui::IsItemDeactivated())
                {
                    OnStringKeyUp(key.KeyString);
                    m_ActiveKeyIndex = -1;
                }
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
                if ( (ImGui::Button(key.Label.c_str(), {key_width, key_height})) || ( (ImGui::IsItemHovered()) && (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) ) )
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
                //No toggling or special duplicate key efforts here... this kind of key isn't in any stock layouts anyways
                bool is_down = (m_ActiveKeyIndex == key_index);

                if ( (ImGui::Button(key.Label.c_str(), {key_width, key_height})) && (use_key_repeat) )
                {
                    vr_keyboard.SetActionDown(key.KeyActionID, !is_down);
                }

                if (ImGui::IsItemActivated())
                {
                    vr_keyboard.SetActionDown(key.KeyActionID, true);
                    m_ActiveKeyIndex = key_index;
                }
                else if (ImGui::IsItemDeactivated())
                {
                    vr_keyboard.SetActionDown(key.KeyActionID, false);
                    m_ActiveKeyIndex = -1;
                }
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

    m_HasHoveredNewItem = ((m_IsHovered) && (ImGui::HasHoveredNewItem())); //Widget state hacks don't play well with that state, so we store it here instead

    m_KeyboardWidgetState.StoreCurrentState();
    widget_state_back.ApplyState();

    io.KeyRepeatDelay = key_repeat_delay_old;
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
    ImGui::GetIO().KeysDown[keycode] = false;
}

void WindowKeyboard::OnStringKeyDown(const std::string& keystring)
{
    if (!m_CurrentStringKeyDown)
    {
        VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();
        m_CurrentStringKeyDown = true;

        HandleUnstickyModifiers();

        vr_keyboard.SetStringDown(keystring, true);
    }
}

void WindowKeyboard::OnStringKeyUp(const std::string& keystring)
{
    if (m_CurrentStringKeyDown)
    {
        VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();
        m_CurrentStringKeyDown = false;

        vr_keyboard.SetStringDown(keystring, false);
    }
}

void WindowKeyboard::HandleUnstickyModifiers(unsigned char source_keycode)
{
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    //Skip if modifiers are set to be sticky
    if (ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_sticky_modifiers))
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

vr::VROverlayHandle_t WindowKeyboard::GetOverlayHandle() const
{
    return UIManager::Get()->GetOverlayHandleKeyboard();
}

void WindowKeyboard::ResetTransform()
{
    //Offset keyboard position depending on the scaled size
    float overlay_height_m = (m_Size.y / m_Size.x) * OVERLAY_WIDTH_METERS_KEYBOARD;
    float offset_up = -0.55f - ((overlay_height_m * ConfigManager::Get().GetConfigFloat(configid_float_input_keyboard_detached_size)) / 2.0f);

    m_Transform.identity();
    m_Transform.rotateX(-45);
    m_Transform.translate_relative(0.0f, offset_up, 0.00f);
}

bool WindowKeyboard::IsHovered() const
{
    return m_IsHovered;
}

bool WindowKeyboard::HasHoveredNewItem() const
{
    return m_HasHoveredNewItem;
}

void WindowKeyboard::UpdateOverlaySize() const
{
    if (!UIManager::Get()->IsOpenVRLoaded())
        return;

    if (GetOverlayHandle() != vr::k_ulOverlayHandleInvalid)
        vr::VROverlay()->SetOverlayWidthInMeters(GetOverlayHandle(), OVERLAY_WIDTH_METERS_KEYBOARD * ConfigManager::Get().GetConfigFloat(configid_float_input_keyboard_detached_size));
}
