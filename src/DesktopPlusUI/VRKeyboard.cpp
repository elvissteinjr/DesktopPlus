#include "VRKeyboard.h"

#include <sstream>
#include "UIManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "Ini.h"
#include "DPBrowserAPIClient.h"

#include "imgui_internal.h"
#include "imgui_impl_win32_openvr.h"

static const char* const g_KeyboardSublayoutNames[kbdlayout_sub_MAX]    = {"Base", "Shift", "AltGr", "Aux"};
static const char* const g_KeyboardClusterNames[kbdlayout_cluster_MAX]  = {"Base", "Function", "Navigation", "Numpad", "Extra"};
static const char* const g_KeyboardKeyTypeNames[kbdlayout_key_MAX]      = {"Blank", "VirtualKey", "VirtualKeyToggle", "VirtualKeyIsoEnter", "String", "SubLayoutToggle", "Action"};

VRKeyboard::VRKeyboard() : 
    m_InputTarget(kbdtarget_desktop),
    m_InputTargetOverlayID(k_ulOverlayID_None),
    m_CapsLockToggled(false),
    m_ActiveInputText(0),
    m_InputBeginWidgetID(0),
    m_ShortcutWindowDirHint(ImGuiDir_Down),
    m_ShortcutWindowYOffset(0.0f),
    m_ActiveInputTextIsMultiline(false),
    m_MouseLeftDownPrevCached(false),
    m_MouseLeftClickedPrevCached(false),
    m_KeyboardHiddenLastFrame(false)
{
    std::fill_n(m_KeyDown, IM_ARRAYSIZE(m_KeyDown), 0);
}

unsigned char VRKeyboard::GetModifierFlags() const
{
    unsigned char flags = 0;
    if (m_KeyDown[VK_LSHIFT])   { flags |= kbd_keystate_flag_lshift_down; }
    if (m_KeyDown[VK_RSHIFT])   { flags |= kbd_keystate_flag_rshift_down; }
    if (m_KeyDown[VK_LCONTROL]) { flags |= kbd_keystate_flag_lctrl_down;  }
    if (m_KeyDown[VK_RCONTROL]) { flags |= kbd_keystate_flag_rctrl_down;  }
    if (m_KeyDown[VK_LMENU])    { flags |= kbd_keystate_flag_lalt_down;   }
    if (m_KeyDown[VK_RMENU])    { flags |= (m_LayoutMetadata.HasAltGr) ? (kbd_keystate_flag_ralt_down | kbd_keystate_flag_lctrl_down) : kbd_keystate_flag_ralt_down; }
    if (m_CapsLockToggled)      { flags |= kbd_keystate_flag_capslock_toggled; }

    return flags;
}

vr::VROverlayHandle_t VRKeyboard::GetTargetOverlayHandle() const
{
    if (m_InputTargetOverlayID != k_ulOverlayID_None)
    {
        return OverlayManager::Get().GetConfigData(m_InputTargetOverlayID).ConfigHandle[configid_handle_overlay_state_overlay_handle];
    }

    return vr::k_ulOverlayHandleInvalid;
}

WindowKeyboard& VRKeyboard::GetWindow()
{
    return m_WindowKeyboard;
}

KeyboardEditor& VRKeyboard::GetEditor()
{
    return m_KeyboardEditor;
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

        const bool keyboard_editor_mode = UIManager::Get()->IsInKeyboardEditorMode();   //Always load all clusters when in Keyboard Editor mode
        const bool cluster_enabled[kbdlayout_cluster_MAX] = 
        {
            true, 
            (ConfigManager::GetValue(configid_bool_input_keyboard_cluster_function_enabled)   || keyboard_editor_mode),
            (ConfigManager::GetValue(configid_bool_input_keyboard_cluster_navigation_enabled) || keyboard_editor_mode),
            (ConfigManager::GetValue(configid_bool_input_keyboard_cluster_numpad_enabled)     || keyboard_editor_mode),
            (ConfigManager::GetValue(configid_bool_input_keyboard_cluster_extra_enabled)      || keyboard_editor_mode)
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
            key_section << "Key_" << g_KeyboardSublayoutNames[sublayout_id] << "_Row_" << row_id << "_ID_" << key_id;

            if (layout_file.SectionExists(key_section.str().c_str()))
            {
                //Key cluster
                key_cluster_str = layout_file.ReadString(key_section.str().c_str(), "Cluster", "Base");

                //Match cluster string and skip if the cluster has been disabled
                bool skip_key = false;
                size_t key_cluster_i = 0;
                for (key_cluster_i = 0; key_cluster_i < kbdlayout_cluster_MAX; ++key_cluster_i)
                {
                    if (key_cluster_str == g_KeyboardClusterNames[key_cluster_i])
                    {
                        if (!cluster_enabled[key_cluster_i])
                        {
                            skip_key = true;
                        }
                        break;
                    }
                }

                if (skip_key)
                {
                    key_id++;
                    continue;
                }

                KeyboardLayoutKey key;

                //Key cluster
                key.KeyCluster = (KeyboardLayoutCluster)key_cluster_i;

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
                        if (key_sublayout_toggle_str == g_KeyboardSublayoutNames[i])
                        {
                            key.KeySubLayoutToggle = (KeyboardLayoutSubLayout)i;
                            break;
                        }
                    }
                }
                else if (key_type_str == "Action")
                {
                    key.KeyType = kbdlayout_key_action;
                    key.KeyActionUID = std::strtoull(layout_file.ReadString(key_section.str().c_str(), "ActionUID", "0").c_str(), nullptr, 10);
                }

                //General
                key.Width    = layout_file.ReadInt(key_section.str().c_str(), "Width",  100) / 100.0f;
                key.Height   = layout_file.ReadInt(key_section.str().c_str(), "Height", 100) / 100.0f;
                key.Label    = layout_file.ReadString(key_section.str().c_str(), "Label");
                key.NoRepeat = layout_file.ReadBool(key_section.str().c_str(), "NoRepeat", false);

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

        //Show keyboard again if it's visible to refresh title that may have been cut off before
        if (m_WindowKeyboard.IsVisible())
        {
            m_WindowKeyboard.Show();
        }

        UIManager::Get()->RepeatFrame();
    }
}

bool VRKeyboard::SaveCurrentLayoutToFile(const std::string& filename)
{
    m_LayoutMetadata.FileName = filename;

    std::string fullpath = ConfigManager::Get().GetApplicationPath() + "keyboards/" + filename;
    std::wstring wpath = WStringConvertFromUTF8(fullpath.c_str());

    //Update cluster availability metadata
    for (int i_sublayout = kbdlayout_sub_base; i_sublayout < kbdlayout_sub_MAX; ++i_sublayout)
    {
        KeyboardLayoutSubLayout sublayout = (KeyboardLayoutSubLayout)i_sublayout;

        for (const KeyboardLayoutKey& key : m_KeyboardKeys[sublayout])
        {
            m_LayoutMetadata.HasCluster[key.KeyCluster] = true;
        }
    }

    Ini layout_file(wpath.c_str(), true);

    //Write metadata
    layout_file.WriteString("LayoutInfo", "Name",                 m_LayoutMetadata.Name.c_str());
    layout_file.WriteString("LayoutInfo", "Author",               m_LayoutMetadata.Author.c_str());
    layout_file.WriteBool(  "LayoutInfo", "HasAltGr",             m_LayoutMetadata.HasAltGr);
    layout_file.WriteBool(  "LayoutInfo", "HasClusterFunction",   m_LayoutMetadata.HasCluster[kbdlayout_cluster_function]);
    layout_file.WriteBool(  "LayoutInfo", "HasClusterNavigation", m_LayoutMetadata.HasCluster[kbdlayout_cluster_navigation]);
    layout_file.WriteBool(  "LayoutInfo", "HasClusterNumpad",     m_LayoutMetadata.HasCluster[kbdlayout_cluster_numpad]);
    layout_file.WriteBool(  "LayoutInfo", "HasClusterExtra",      m_LayoutMetadata.HasCluster[kbdlayout_cluster_extra]);

    for (int i_sublayout = kbdlayout_sub_base; i_sublayout < kbdlayout_sub_MAX; ++i_sublayout)
    {
        KeyboardLayoutSubLayout sublayout = (KeyboardLayoutSubLayout)i_sublayout;
        unsigned int row_id = 0;
        unsigned int key_id = 0;

        for (const KeyboardLayoutKey& key : m_KeyboardKeys[sublayout])
        {
            std::stringstream key_section;
            key_section << "Key_" << g_KeyboardSublayoutNames[sublayout] << "_Row_" << row_id << "_ID_" << key_id;

            //Key type
            layout_file.WriteString(key_section.str().c_str(), "Type", g_KeyboardKeyTypeNames[key.KeyType]);

            //General
            if (key.Width != 1.0f)
            {
                layout_file.WriteInt(key_section.str().c_str(), "Width", int(key.Width * 100.0f));
            }

            if (key.Height != 1.0f)
            {
                layout_file.WriteInt(key_section.str().c_str(), "Height", int(key.Height * 100.0f));
            }

            if (key.KeyType != kbdlayout_key_blank_space)
            {
                std::string label_escaped = key.Label;
                StringReplaceAll(label_escaped, "\n", "\\n");

                layout_file.WriteString(key_section.str().c_str(), "Label", label_escaped.c_str());
            }

            if (key.KeyCluster != kbdlayout_cluster_base)
            {
                layout_file.WriteString(key_section.str().c_str(), "Cluster", g_KeyboardClusterNames[key.KeyCluster]);
            }

            //Key type specific values
            switch (key.KeyType)
            {
                case kbdlayout_key_virtual_key:
                case kbdlayout_key_virtual_key_toggle:
                case kbdlayout_key_virtual_key_iso_enter:
                {
                    layout_file.WriteInt(   key_section.str().c_str(), "KeyCode", key.KeyCode);

                    if ((key.KeyType == kbdlayout_key_virtual_key) && (key.BlockModifiers))
                    {
                        layout_file.ReadBool(key_section.str().c_str(), "BlockModifiers", true);
                    }
                    break;
                }
                case kbdlayout_key_string:
                {
                    layout_file.WriteString(key_section.str().c_str(), "String", key.KeyString.c_str());
                    break;
                }
                case kbdlayout_key_sublayout_toggle:
                {
                    layout_file.WriteString(key_section.str().c_str(), "SubLayout", g_KeyboardSublayoutNames[key.KeySubLayoutToggle]);
                    break;
                }
                case kbdlayout_key_action:
                {
                    layout_file.WriteString(key_section.str().c_str(), "ActionUID", std::to_string(key.KeyActionUID).c_str());
                    break;
                }
            }

            if (key.NoRepeat)
            {
                layout_file.WriteBool(key_section.str().c_str(), "NoRepeat", true);
            }

            if (key.IsRowEnd)
            {
                row_id++;
                key_id = 0;
            }
            else
            {
                key_id++;
            }
        }
    }

    return layout_file.Save();
}

void VRKeyboard::LoadCurrentLayout()
{
    LoadLayoutFromFile(ConfigManager::GetValue(configid_str_input_keyboard_layout_file));
}

std::vector<KeyboardLayoutMetadata> VRKeyboard::GetKeyboardLayoutList()
{
    std::vector<KeyboardLayoutMetadata> layout_list;

    const std::wstring wpath = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "keyboards/*.ini").c_str() );
    WIN32_FIND_DATA find_data;
    HANDLE handle_find = ::FindFirstFileW(wpath.c_str(), &find_data);

    if (handle_find != INVALID_HANDLE_VALUE)
    {
        do
        {
            KeyboardLayoutMetadata metadata = LoadLayoutMetadataFromFile( StringConvertFromUTF16(find_data.cFileName) );

            //If base cluster exists, layout is probably valid, add to list
            if (metadata.HasCluster[kbdlayout_cluster_base])
            {
                layout_list.push_back(metadata);
            }
        }
        while (::FindNextFileW(handle_find, &find_data) != 0);

        ::FindClose(handle_find);
    }

    return layout_list;
}

const KeyboardLayoutMetadata& VRKeyboard::GetLayoutMetadata() const
{
    return m_LayoutMetadata;
}

void VRKeyboard::SetLayoutMetadata(const KeyboardLayoutMetadata& metadata)
{
    m_LayoutMetadata = metadata;
}

std::vector<KeyboardLayoutKey>& VRKeyboard::GetLayout(KeyboardLayoutSubLayout sublayout)
{
    return m_KeyboardKeys[sublayout];
}

void VRKeyboard::SetLayout(KeyboardLayoutSubLayout sublayout, std::vector<KeyboardLayoutKey>& keys)
{
    m_KeyboardKeys[sublayout] = keys;
}

const std::string& VRKeyboard::GetKeyLabelsString() const
{
    return m_KeyLabels;
}

KeyboardInputTarget VRKeyboard::GetInputTarget() const
{
    return m_InputTarget;
}

unsigned int VRKeyboard::GetInputTargetOverlayID() const
{
    return m_InputTargetOverlayID;
}

bool VRKeyboard::GetKeyDown(unsigned char keycode) const
{
    return m_KeyDown[keycode];
}

void VRKeyboard::SetKeyDown(unsigned char keycode, bool down, bool block_modifiers)
{
    //Don't do anything if the key state didn't change
    if (m_KeyDown[keycode] == down)
        return;

    m_KeyDown[keycode] = down;

    //Update combined modifier key state if relevant
    if ((keycode == VK_LSHIFT) || (keycode == VK_RSHIFT))
    {
        m_KeyDown[VK_SHIFT] = (m_KeyDown[VK_LSHIFT] || m_KeyDown[VK_RSHIFT]);

        if (m_InputTarget == kbdtarget_ui)
        {
            ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, m_KeyDown[VK_SHIFT]);
        }
    }
    else if ((keycode == VK_LCONTROL) || (keycode == VK_RCONTROL))
    {
        m_KeyDown[VK_CONTROL] = (m_KeyDown[VK_LCONTROL] || m_KeyDown[VK_RCONTROL]);

        if (m_InputTarget == kbdtarget_ui)
        {
            ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, m_KeyDown[VK_CONTROL]);
        }
    }
    else if ((keycode == VK_LMENU) || (keycode == VK_RMENU))
    {
        m_KeyDown[VK_MENU] = (m_KeyDown[VK_LMENU] || m_KeyDown[VK_RMENU]);

        if (m_InputTarget == kbdtarget_ui)
        {
            ImGui::GetIO().AddKeyEvent(ImGuiMod_Alt, m_KeyDown[VK_MENU]);
        }
    }
    else if ( (down) && (keycode == VK_CAPITAL) ) //For caps lock, update toggled state
    {
        m_CapsLockToggled = !m_CapsLockToggled;
    }

    if (m_InputTarget == kbdtarget_overlay)
    {
        //Send keystate message (additional wchar message is sent below)
        unsigned char flags = (down) ? GetModifierFlags() | kbd_keystate_flag_key_down : GetModifierFlags();
        DPBrowserAPIClient::Get().DPBrowser_KeyboardSetKeyState(GetTargetOverlayHandle(), (DPBrowserIPCKeyboardKeystateFlags)flags, keycode);
    }

    if (m_InputTarget != kbdtarget_desktop)
    {
        ImGui::GetIO().AddKeyEvent(ImGui_ImplWin32_KeyEventToImGuiKey(keycode, 0), down);

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

            if (wchars_written > 0)
            {
                //Documentation of ToUnicode() suggests there may be invalid characters in the buffer past the written ones if the return value is higher than 1
                //One would like to believe the function NUL-terminates at that point, but there's no guarantee for that
                if (wchars_written > 1)
                {
                    key_wchars[std::min(wchars_written, 15)] = '\0';
                }

                if (m_InputTarget == kbdtarget_ui)
                {
                    AddTextToStringQueue(StringConvertFromUTF16(key_wchars));
                }
                else if (m_InputTarget == kbdtarget_overlay)
                {
                    DPBrowserAPIClient::Get().DPBrowser_KeyboardTypeString(GetTargetOverlayHandle(), StringConvertFromUTF16(key_wchars));
                }
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
    if (m_InputTarget == kbdtarget_ui)
    {
        if (down)
        {
            AddTextToStringQueue(text);
        }
        return;
    }
    
    std::wstring wstr = WStringConvertFromUTF8(text.c_str());
    
    if (m_InputTarget == kbdtarget_desktop)
    {
        //If it's a single character, send as wchar so it checks if it can be pressed on the current windows keyboard layout
        if (wstr.length() == 1)
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_wchar, MAKELPARAM(wstr[0], down));

            if (!down)
            {
                RestoreDesktopModifierState();
            }
        }
        else if (!down) //Otherwise, send as string input
        {
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_keyboard_string, text, UIManager::Get()->GetWindowHandle());
        }
    }
    else if (m_InputTarget == kbdtarget_overlay)
    {
        //If it's a single character, send as wchar so it checks if it can be pressed on the current windows keyboard layout
        if (wstr.length() == 1)
        {
            DPBrowserAPIClient::Get().DPBrowser_KeyboardTypeWChar(GetTargetOverlayHandle(), wstr[0], down);
        }
        else if (!down) //Otherwise, send as string input
        {
            DPBrowserAPIClient::Get().DPBrowser_KeyboardTypeString(GetTargetOverlayHandle(), StringConvertFromUTF16(wstr.c_str()));
        }
    }
}

void VRKeyboard::SetActionDown(ActionUID action_uid, bool down)
{
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, (down) ? ipcact_action_start : ipcact_action_stop, action_uid);
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
        SetKeyDown(VK_CAPITAL, true);
        SetKeyDown(VK_CAPITAL, false);
    }

    m_StringQueue = {}; //Clear

    m_WindowKeyboard.ResetButtonState();
}

void VRKeyboard::VRKeyboardInputBegin(const char* str_id, bool is_multiline)
{
    VRKeyboardInputBegin(ImGui::GetID(str_id), is_multiline);
}

void VRKeyboard::VRKeyboardInputBegin(ImGuiID widget_id, bool is_multiline)
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

        m_ActiveInputTextIsMultiline = is_multiline;
    }

    m_MouseLeftDownPrevCached    = io.MouseDown[ImGuiMouseButton_Left];
    m_MouseLeftClickedPrevCached = io.MouseClicked[ImGuiMouseButton_Left];

    if ( (m_WindowKeyboard.IsHovered()) || (m_WindowKeyboardShortcuts.IsHovered()) )
    {
        io.MouseDown[ImGuiMouseButton_Left] = false;
        io.MouseClicked[ImGuiMouseButton_Left] = false;
    }

    //Set mouse delta to 0 when keyboard shortcut window buttons are down in order to prevent the InputText's selection to change from cursor movement
    if (m_WindowKeyboardShortcuts.IsAnyButtonDown())
    {
        io.MouseDelta.x = 0.0f;
        io.MouseDelta.y = 0.0f;
    }

    m_InputBeginWidgetID = widget_id;
}

void VRKeyboard::VRKeyboardInputEnd()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiID widget_id = m_InputBeginWidgetID;

    if (ImGui::IsItemActivated())
    {
        m_ActiveInputText = widget_id;
    }
    else if ((m_ActiveInputText == widget_id) && (ImGui::IsItemDeactivated()))
    {
        m_ActiveInputText = 0;
    }

    if ( (m_ActiveInputText == widget_id) && 
         ( (ImGui::IsKeyPressed(ImGuiKey_Tab)) || (ImGui::IsKeyPressed(ImGuiKey_Escape)) || ((!m_ActiveInputTextIsMultiline) && (ImGui::IsKeyPressed(ImGuiKey_Enter))) ) )
    {
        ImGui::ClearActiveID();
        m_ActiveInputText = 0;
        UIManager::Get()->RepeatFrame();
    }

    if ( (m_ActiveInputText == widget_id) && (!ImGui::IsItemHovered()) && (ImGui::IsItemActive()) && (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) && 
         (!m_WindowKeyboard.IsHovered()) && (!m_WindowKeyboardShortcuts.IsHovered()) )
    {
        ImGui::ClearActiveID();
        m_ActiveInputText = 0;
        io.MouseDownDuration[ImGuiMouseButton_Left] = -1.0f;        //Reset mouse down duration so the click counts as newly pressed again in the next frame
        UIManager::Get()->RepeatFrame();
    }

    //Restore mouse down in case modified in VRKeyboardInputBegin()
    io.MouseDown[ImGuiMouseButton_Left]    = m_MouseLeftDownPrevCached;
    io.MouseClicked[ImGuiMouseButton_Left] = m_MouseLeftClickedPrevCached;

    if (!UIManager::Get()->IsInDesktopMode())
    {
        //Set active widget for keyboard shortcuts window if there's actually active text input (m_ActiveInputText may be a slider for example)
        if ((m_ActiveInputText == widget_id) && (io.WantTextInput))
        {
            m_WindowKeyboardShortcuts.SetActiveWidget(m_ActiveInputText);
            m_WindowKeyboardShortcuts.SetDefaultPositionDirection(m_ShortcutWindowDirHint, m_ShortcutWindowYOffset);
        }
        else if (m_ActiveInputText == 0)
        {
            m_WindowKeyboardShortcuts.SetActiveWidget(0);
        }

        m_WindowKeyboardShortcuts.Update(widget_id);    //This is called regardless of the widget being active in case a fade-out is still happening
    }

    m_InputBeginWidgetID = 0;
    m_ShortcutWindowDirHint = ImGuiDir_Down;
    m_ShortcutWindowYOffset = 0.0f;
}

void VRKeyboard::OnImGuiNewFrame()
{
    if (UIManager::Get()->IsInDesktopMode())
        return;

    ImGuiIO& io = ImGui::GetIO();

    //Show keyboard for UI if needed
    if (io.WantTextInput)
    {
        if ( (!m_KeyboardHiddenLastFrame) && ( (m_InputTarget != kbdtarget_ui) || (!m_WindowKeyboard.IsVisible()) ) )
        {
            m_InputTarget = kbdtarget_ui;

            bool do_assign_to_ui = false;
            int assigned_id = m_WindowKeyboard.GetAssignedOverlayID();

            //Assign keyboard to UI if it's not assigned to any overlay yet
            if (assigned_id == -1)
            {
                do_assign_to_ui = true;
            }
            else if (assigned_id >= 0)  //else do it if the assigned overlay is invisible
            {
                vr::VROverlayHandle_t ovrl_handle_assigned = OverlayManager::Get().GetConfigData((unsigned int)assigned_id).ConfigHandle[configid_handle_overlay_state_overlay_handle];
                do_assign_to_ui = !vr::VROverlay()->IsOverlayVisible(ovrl_handle_assigned);
            }

            if (do_assign_to_ui)
            {
                m_WindowKeyboard.SetAutoVisibility(-2, true);
            }

            m_WindowKeyboard.Show();
        }
    }
    else if (m_WindowKeyboard.IsVisible())
    {
        int assigned_id = m_WindowKeyboard.GetAssignedOverlayID();

        //Disable UI target if it's active
        if (m_InputTarget == kbdtarget_ui)
        {
            ResetState();
            m_InputTarget = kbdtarget_desktop;
            m_InputTargetOverlayID = k_ulOverlayID_None;
            m_WindowKeyboard.Show(); //Show() updates window title
        }

        //If keyboard is visible for the UI, turn off auto-visibility
        if (assigned_id == -2)
        {
            m_WindowKeyboard.SetAutoVisibility(-2, false);
        }

        //Check if overlay target should be used
        int focused_overlay_id = ConfigManager::Get().GetValue(configid_int_state_overlay_focused_id);
        OverlayCaptureSource capsource = ovrl_capsource_desktop_duplication;

        if (focused_overlay_id >= 0)
        {
            capsource = (OverlayCaptureSource)OverlayManager::Get().GetConfigData((unsigned int)focused_overlay_id).ConfigInt[configid_int_overlay_capture_source];
        }

        if ( (m_InputTarget == kbdtarget_overlay) && (capsource != ovrl_capsource_browser) )
        {
            ResetState();
            m_InputTarget = kbdtarget_desktop;
            m_InputTargetOverlayID = k_ulOverlayID_None;

            m_WindowKeyboard.Show(); //Show() updates window title
        }
        else if ( (m_InputTargetOverlayID != (unsigned int)focused_overlay_id) && (capsource == ovrl_capsource_browser) )  //Also resets state when overlay ID changes
        {
            ResetState();
            m_InputTarget = kbdtarget_overlay;
            m_InputTargetOverlayID = (unsigned int)focused_overlay_id;
            m_WindowKeyboard.Show(); //Show() updates window title
        }
    }

    m_KeyboardHiddenLastFrame = false;

    if (m_InputTarget == kbdtarget_ui)
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
        m_ActiveInputText = 0;
        io.WantTextInput = false;
    }

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

void VRKeyboard::SetShortcutWindowDirectionHint(ImGuiDir dir_hint, float y_offset)
{
    m_ShortcutWindowDirHint = dir_hint;
    m_ShortcutWindowYOffset = y_offset;
}

KeyboardLayoutMetadata VRKeyboard::LoadLayoutMetadataFromFile(const std::string& filename)
{
    KeyboardLayoutMetadata metadata;

    std::string fullpath = ConfigManager::Get().GetApplicationPath() + "keyboards/" + filename;
    Ini layout_file( WStringConvertFromUTF8(fullpath.c_str()).c_str() );

    if (layout_file.SectionExists("LayoutInfo"))
    {
        metadata.Name     = layout_file.ReadString("LayoutInfo", "Name");
        metadata.Author   = layout_file.ReadString("LayoutInfo", "Author");
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
