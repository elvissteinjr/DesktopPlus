#include "VRKeyboard.h"

#include <sstream>
#include "UIManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"

#include "imgui_internal.h"

VRKeyboard::VRKeyboard() : 
    m_TargetIsUI(true),
    m_CapsLockToggled(false),
    m_ActiveInputText(0),
    m_InputBeginWidgetID(0),
    m_MouseLeftStateOldCached(false),
    m_KeyboardHiddenLastFrame(false)
{
    std::fill_n(m_KeyDown, IM_ARRAYSIZE(m_KeyDown), 0);
}

unsigned char VRKeyboard::GetModifierFlags() const
{
    unsigned char flags = 0;
    if (m_KeyDown[VK_LSHIFT])   { flags |= kbd_keystate_flag_lshift_down;      }
    if (m_KeyDown[VK_RSHIFT])   { flags |= kbd_keystate_flag_rshift_down;      }
    if (m_KeyDown[VK_LCONTROL]) { flags |= kbd_keystate_flag_lctrl_down;       }
    if (m_KeyDown[VK_RCONTROL]) { flags |= kbd_keystate_flag_rctrl_down;       }
    if (m_KeyDown[VK_LMENU])    { flags |= kbd_keystate_flag_lalt_down;        }
    if (m_KeyDown[VK_RMENU])    { flags |= kbd_keystate_flag_ralt_down;        }
    if (m_CapsLockToggled)      { flags |= kbd_keystate_flag_capslock_toggled; }

    return flags;
}

WindowKeyboard& VRKeyboard::GetWindow()
{
    return m_WindowKeyboard;
}

void VRKeyboard::LoadLayoutFromFile(const std::string& filename)
{
    std::string fullpath = ConfigManager::Get().GetApplicationPath() + "keyboards/" + filename;
    Ini layout_file( WStringConvertFromUTF8(fullpath.c_str()).c_str() );

    //Check if it's probably a keyboard layout file
    if (layout_file.SectionExists("LayoutInfo"))
    {
        //Clear old layout data
        ResetState();

        for (auto& sublayout : m_KeyboardKeys)
        {
            sublayout.clear();
        }

        m_KeyLabels = "";

        //Load new layout
        m_LayoutMetadata = LoadLayoutMetadataFromFile(filename);

        const char* const sublayout_names[kbdlayout_sub_MAX]   = {"Base", "Shift", "AltGr", "Aux"};
        const char* const cluster_names[kbdlayout_cluster_MAX] = {"Base", "Function", "Navigation", "Numpad", "Extra"};
        const bool cluster_enabled[kbdlayout_cluster_MAX] = 
        {
            true, 
            ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_cluster_function_enabled),
            ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_cluster_navigation_enabled),
            ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_cluster_numpad_enabled),
            ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_cluster_extra_enabled)
        };

        unsigned int sublayout_id = kbdlayout_sub_base;
        unsigned int row_id = 0;
        unsigned int key_id = 0;

        std::string key_type_str;
        std::string key_cluster_str;
        std::string key_sublayout_toggle_str;
        m_KeyLabels = "";

        while (true)
        {
            std::stringstream key_section;
            key_section << "Key_" << sublayout_names[sublayout_id] << "_Row_" << row_id << "_ID_" << key_id;

            if (layout_file.SectionExists(key_section.str().c_str()))
            {
                //Key cluster
                key_cluster_str = layout_file.ReadString(key_section.str().c_str(), "Cluster", "Base");

                //Match cluster string and skip if the cluster has been disabled
                bool skip_key = false;
                for (size_t i = 0; i < kbdlayout_cluster_MAX; ++i)
                {
                    if ( (key_cluster_str == cluster_names[i]) && (!cluster_enabled[i]) )
                    {
                        skip_key = true;
                        break;
                    }
                }

                if (skip_key)
                {
                    key_id++;
                    continue;
                }

                KeyboardLayoutKey key;

                //Key type
                key_type_str = layout_file.ReadString(key_section.str().c_str(), "Type", "Blank");

                if (key_type_str == "Blank")
                {
                    key.KeyType = kbdlayout_key_blank_space;
                }
                else if (key_type_str == "VirtualKey")
                {
                    key.KeyType = kbdlayout_key_virtual_key;
                    key.KeyCode = layout_file.ReadInt(key_section.str().c_str(), "KeyCode", 0);
                    key.BlockModifiers = layout_file.ReadBool(key_section.str().c_str(), "BlockModifiers", false);
                }
                else if (key_type_str == "VirtualKeyToggle")
                {
                    key.KeyType = kbdlayout_key_virtual_key_toggle;
                    key.KeyCode = layout_file.ReadInt(key_section.str().c_str(), "KeyCode", 0);
                }
                else if (key_type_str == "VirtualKeyIsoEnter")
                {
                    key.KeyType = kbdlayout_key_virtual_key_iso_enter;
                    key.KeyCode = layout_file.ReadInt(key_section.str().c_str(), "KeyCode", 0);
                }
                else if (key_type_str == "String")
                {
                    key.KeyType   = kbdlayout_key_string;
                    key.KeyString = layout_file.ReadString(key_section.str().c_str(), "String");
                }
                else if (key_type_str == "SubLayoutToggle")
                {
                    key.KeyType = kbdlayout_key_sublayout_toggle;

                    key_sublayout_toggle_str = layout_file.ReadString(key_section.str().c_str(), "SubLayout", "Base");

                    //Match sublayout string
                    for (size_t i = 0; i < kbdlayout_sub_MAX; ++i)
                    {
                        if (key_sublayout_toggle_str == sublayout_names[i])
                        {
                            key.KeySubLayoutToggle = (KeyboardLayoutSubLayout)i;
                            break;
                        }
                    }
                }
                else if (key_type_str == "Action")
                {
                    key.KeyType = kbdlayout_key_action;
                    key.KeyActionID = (ActionID)layout_file.ReadInt(key_section.str().c_str(), "ActionID", action_none);
                }

                //General
                key.Width  = layout_file.ReadInt(key_section.str().c_str(), "Width",  100) / 100.0f;
                key.Height = layout_file.ReadInt(key_section.str().c_str(), "Height", 100) / 100.0f;
                key.Label  = layout_file.ReadString(key_section.str().c_str(), "Label");
                StringReplaceAll(key.Label, "\\n", "\n");
                m_KeyLabels += key.Label;

                m_KeyboardKeys[sublayout_id].push_back(key);

                key_id++;
            }
            else if (key_id == 0) //No more keys in sublayout
            {
                sublayout_id++; //Try next sublayout
                row_id = 0;

                if (sublayout_id == kbdlayout_sub_MAX)
                {
                    break; //No more possible sublayouts, we're done here
                }
            }
            else //Try next row
            {
                //Mark last key as end of row
                if (!m_KeyboardKeys[sublayout_id].empty())
                {
                    m_KeyboardKeys[sublayout_id].back().IsRowEnd = true;
                }

                row_id++;
                key_id = 0;
            }
        }

        //Reload texture to update font atlas with keyboard chars
        if (ImGui::StringContainsUnmappedCharacter(m_KeyLabels.c_str()))
        {
            TextureManager::Get().ReloadAllTexturesLater();
            UIManager::Get()->RepeatFrame();
        }

        UIManager::Get()->RepeatFrame();
    }
}

void VRKeyboard::LoadCurrentLayout()
{
    LoadLayoutFromFile(ConfigManager::Get().GetConfigString(configid_str_input_keyboard_layout_file));
}

const KeyboardLayoutMetadata& VRKeyboard::GetLayoutMetadata() const
{
    return m_LayoutMetadata;
}

std::vector<KeyboardLayoutKey>& VRKeyboard::GetLayout(KeyboardLayoutSubLayout sublayout)
{
    return m_KeyboardKeys[sublayout];
}

const std::string& VRKeyboard::GetKeyLabelsString() const
{
    return m_KeyLabels;
}

bool VRKeyboard::IsTargetUI() const
{
    return m_TargetIsUI;
}

bool VRKeyboard::GetKeyDown(unsigned char keycode) const
{
    return m_KeyDown[keycode];
}

void VRKeyboard::SetKeyDown(unsigned char keycode, bool down, bool block_modifiers)
{
    m_KeyDown[keycode] = down;

    //Update combined modifier key state if relevant
    if ((keycode == VK_LSHIFT) || (keycode == VK_RSHIFT))
    {
        m_KeyDown[VK_SHIFT] = (m_KeyDown[VK_LSHIFT] || m_KeyDown[VK_RSHIFT]);

        if (m_TargetIsUI)
        {
            ImGui::GetIO().KeysDown[VK_SHIFT] = m_KeyDown[VK_SHIFT];
        }
    }
    else if ((keycode == VK_LCONTROL) || (keycode == VK_RCONTROL))
    {
        m_KeyDown[VK_CONTROL] = (m_KeyDown[VK_LCONTROL] || m_KeyDown[VK_RCONTROL]);

        if (m_TargetIsUI)
        {
            ImGui::GetIO().KeysDown[VK_CONTROL] = m_KeyDown[VK_CONTROL];
        }
    }
    else if ((keycode == VK_LMENU) || (keycode == VK_RMENU))
    {
        m_KeyDown[VK_MENU] = (m_KeyDown[VK_LMENU] || m_KeyDown[VK_RMENU]);

        if (m_TargetIsUI)
        {
            ImGui::GetIO().KeysDown[VK_MENU] = m_KeyDown[VK_MENU];
        }
    }
    else if ( (down) && (keycode == VK_CAPITAL) ) //For caps lock, update toggled state
    {
        m_CapsLockToggled = !m_CapsLockToggled;
    }

    if (m_TargetIsUI)
    {
        ImGui::GetIO().KeysDown[keycode] = down;

        //Get text output for current state and key
        if ((down) && (keycode != VK_BACK) && (keycode != VK_TAB))
        {
            //Win32 keyboard state for ToUnicode()
            BYTE keyboard_state[256] = {0};
            keyboard_state[VK_SHIFT]   = (m_KeyDown[VK_SHIFT])   ? -1 : 0;
            keyboard_state[VK_CONTROL] = (m_KeyDown[VK_CONTROL]) ? -1 : 0;
            keyboard_state[VK_MENU]    = (m_KeyDown[VK_MENU])    ? -1 : 0;
            keyboard_state[VK_CAPITAL] = (m_CapsLockToggled)     ?  1 : 0;    //Key toggle state is high-order bit
            keyboard_state[keycode]    = -1;


            WCHAR key_wchars[16] = {0};

            int wchars_written = ::ToUnicode(keycode, ::MapVirtualKey(keycode, MAPVK_VK_TO_VSC), keyboard_state, key_wchars, 15, 0);

            //Documentation of ToUnicode() suggests there may be invalid characters in the buffer past the written ones if the return value is higher than 1
            //One would like to believe the function NUL-terminates at that point, but there's no guarantee for that
            if (wchars_written > 1)
            {
                key_wchars[std::min(wchars_written, 15)] = '\0';
            }

            if (wchars_written > 0)
            {
                AddTextToStringQueue(StringConvertFromUTF16(key_wchars));
            }
        }
    }
    else
    {
        if (block_modifiers)
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_vkey, MAKELPARAM((down) ? kbd_keystate_flag_key_down : 0, 0));   //Press key without modifiers
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_vkey, MAKELPARAM(GetModifierFlags(), 0));                        //Restore old modifier state
        }
        else
        {
            unsigned char flags = (down) ? GetModifierFlags() | kbd_keystate_flag_key_down : GetModifierFlags();
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_vkey, MAKELPARAM(flags, keycode));
        }

        
    }
}

void VRKeyboard::SetStringDown(const std::string text, bool down)
{
    std::wstring wstr = WStringConvertFromUTF8(text.c_str());

    if (m_TargetIsUI)
    {
        if (down)
        {
            AddTextToStringQueue(text);
        }
    }
    else
    {
        //If it's a single character, check if it can be pressed on the current windows keyboard layout
        if (wstr.length() == 1)
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_wchar, MAKELPARAM(wstr[0], down));

            if (!down)
            {
                RestoreDesktopModifierState();
            }
        }
        else if (!down) //Isn't a single character or wasn't a valid key for the current layout, send as string input
        {
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_keyboard_string, text, UIManager::Get()->GetWindowHandle());
        }
    }
}

void VRKeyboard::SetActionDown(ActionID action_id, bool down)
{
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, (down) ? ipcact_action_start : ipcact_action_stop, action_id); //_action_start will fall back to _action_do for non input actions
}

bool VRKeyboard::IsCapsLockToggled() const
{
    return m_CapsLockToggled;
}

void VRKeyboard::ResetState()
{
    //Release any held down keys
    for (int i = 0; i < 256; ++i) 
    {
        if (m_KeyDown[i])
        {
            SetKeyDown(i, false);
        }
    }

    if (m_CapsLockToggled)
    {
        SetKeyDown(m_CapsLockToggled, true);
        SetKeyDown(m_CapsLockToggled, false);
        m_CapsLockToggled = false;
    }

    m_StringQueue = {}; //Clear

    m_WindowKeyboard.ResetButtonState();
}

void VRKeyboard::VRKeyboardInputBegin(const char* str_id)
{
    VRKeyboardInputBegin(ImGui::GetID(str_id));
}

void VRKeyboard::VRKeyboardInputBegin(ImGuiID widget_id)
{
    ImGuiContext& g = *GImGui;
    ImGuiIO& io = ImGui::GetIO();

    if (m_ActiveInputText == widget_id)
    {
        if ((io.InputQueueCharacters.empty()) && (!m_StringQueue.empty()))
        {
            io.AddInputCharactersUTF8(m_StringQueue.front().c_str());
            m_StringQueue.pop();
        }
    }

    m_MouseLeftStateOldCached = io.MouseDown[ImGuiMouseButton_Left];

    if (m_WindowKeyboard.IsHovered())
        io.MouseDown[ImGuiMouseButton_Left] = false;

    m_InputBeginWidgetID = widget_id;
}

void VRKeyboard::VRKeyboardInputEnd()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiID widget_id = m_InputBeginWidgetID;

    //Restore mouse down in case modified in VRKeyboardInputBegin()
    io.MouseDown[ImGuiMouseButton_Left] = m_MouseLeftStateOldCached;

    if (ImGui::IsItemActivated())
    {
        m_ActiveInputText = widget_id;
    }
    else if ((m_ActiveInputText == widget_id) && (ImGui::IsItemDeactivated()))
    {
        m_ActiveInputText = 0;
    }

    if ( (m_ActiveInputText == widget_id) && 
         ( (ImGui::IsKeyPressedMap(ImGuiKey_Tab)) || (ImGui::IsKeyPressedMap(ImGuiKey_Escape)) || (ImGui::IsKeyPressedMap(ImGuiKey_Enter)) ) )
    {
        ImGui::ClearActiveID();
        m_ActiveInputText = 0;
        UIManager::Get()->RepeatFrame();
    }

    if ( (m_ActiveInputText == widget_id) && (!ImGui::IsItemHovered()) && (ImGui::IsItemActive()) && (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) && (!m_WindowKeyboard.IsHovered()) )
    {
        ImGui::ClearActiveID();
        m_ActiveInputText = 0;
        io.MouseDownDuration[ImGuiMouseButton_Left] = -1.0f;        //Reset mouse down duration so the click counts as newly pressed again in the next frame
        UIManager::Get()->RepeatFrame();
    }

    if (m_ActiveInputText == widget_id)
    {
        if (m_WindowKeyboard.IsHovered())
        {
            ImGui::SetKeyboardFocusHere(-1);
        }
    }

    m_InputBeginWidgetID = 0;
}

void VRKeyboard::OnImGuiNewFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    //Show keyboard for UI if needed
    if (io.WantTextInput)
    {
        if (!m_KeyboardHiddenLastFrame)
        {
            m_TargetIsUI = true;
            m_WindowKeyboard.Show();

            //Assign keyboard to UI if it's not assigned to any overlay yet
            if (ConfigManager::Get().GetConfigInt(configid_int_state_keyboard_visible_for_overlay_id) == -1)
            {
                ConfigManager::Get().SetConfigInt(configid_int_state_keyboard_visible_for_overlay_id, -2);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_keyboard_visible_for_overlay_id), -2);
            }
        }
    }
    else if (m_WindowKeyboard.IsVisible())
    {
        //If keyboard is visible for an overlay, just disable UI target
        //if (ConfigManager::Get().GetConfigInt(configid_int_state_keyboard_visible_for_overlay_id) != -1)
        {
            if (m_TargetIsUI)
            {
                ResetState();
                m_TargetIsUI = false;
                m_WindowKeyboard.Show(); //Show() updates window title
            }
        }
        /*else //Auto hide is problematic, maybe find a solution later
        {
            m_WindowKeyboard.Hide();
        }*/
        if (ConfigManager::Get().GetConfigInt(configid_int_state_keyboard_visible_for_overlay_id) == -2)
        {
            m_WindowKeyboard.Hide();
        }
    }

    m_KeyboardHiddenLastFrame = false;

    if (m_TargetIsUI)
    {
        UpdateImGuiModifierState();
    }
}

void VRKeyboard::OnWindowHidden()
{
    ImGuiIO& io = ImGui::GetIO();

    if (io.WantTextInput)
    {
        ImGui::ClearActiveID();
        io.WantTextInput = false;
    }

    //Remove overlay assignment, if any
    ConfigManager::Get().SetConfigInt(configid_int_state_keyboard_visible_for_overlay_id, -1);
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_keyboard_visible_for_overlay_id), -1);

    m_KeyboardHiddenLastFrame = true; //Widgets will request the keyboard for the next frame anyways, so we prevent showing it again right away with this flag
}

void VRKeyboard::AddTextToStringQueue(const std::string text)
{
    //Only queue up if an InputText is focused
    if (m_ActiveInputText != 0)
    {
        m_StringQueue.push(text);
    }
}

void VRKeyboard::UpdateImGuiModifierState() const
{
    ImGuiIO& io = ImGui::GetIO();

    io.KeyCtrl  =  m_KeyDown[VK_CONTROL];
    io.KeyShift =  m_KeyDown[VK_SHIFT];
    io.KeyAlt   =  m_KeyDown[VK_MENU];
    io.KeySuper = (m_KeyDown[VK_LWIN] ||  m_KeyDown[VK_RWIN]);
}

void VRKeyboard::RestoreDesktopModifierState() const
{
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_vkey, MAKELPARAM(GetModifierFlags(), 0));
}

KeyboardLayoutMetadata VRKeyboard::LoadLayoutMetadataFromFile(const std::string& filename)
{
    KeyboardLayoutMetadata metadata;

    std::string fullpath = ConfigManager::Get().GetApplicationPath() + "keyboards/" + filename;
    Ini layout_file( WStringConvertFromUTF8(fullpath.c_str()).c_str() );

    if (layout_file.SectionExists("LayoutInfo"))
    {
        metadata.Name     = layout_file.ReadString("LayoutInfo", "Name");
        metadata.FileName = filename;
        metadata.HasCluster[kbdlayout_cluster_base]       = true;   //Always true for valid layouts
        metadata.HasCluster[kbdlayout_cluster_function]   = layout_file.ReadBool("LayoutInfo", "HasClusterFunction");
        metadata.HasCluster[kbdlayout_cluster_navigation] = layout_file.ReadBool("LayoutInfo", "HasClusterNavigation");
        metadata.HasCluster[kbdlayout_cluster_numpad]     = layout_file.ReadBool("LayoutInfo", "HasClusterNumpad");
        metadata.HasCluster[kbdlayout_cluster_extra]      = layout_file.ReadBool("LayoutInfo", "HasClusterExtra");
        metadata.HasAltGr                                 = layout_file.ReadBool("LayoutInfo", "HasAltGr");
    }

    return metadata;
}
