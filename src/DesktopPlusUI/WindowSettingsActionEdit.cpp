#include "WindowSettingsActionEdit.h"

#include "imgui.h"

#include "UIManager.h"
#include "InterprocessMessaging.h"

void WindowSettingsActionEdit::UpdateWarnings()
{
    bool warning_displayed = false;

    static bool warning_temp_notice_hidden = false;

    //Compositor resolution warning
    {
        if (!warning_temp_notice_hidden)
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningTempNotice"))
            {
                ImGui::OpenPopup("Dismiss");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "This is just a stop-gap solution to allow editing actions until the new interface includes these options in a later build");

            if (ImGui::BeginPopup("Dismiss"))
            {
                if (ImGui::Selectable("Dismiss"))
                {
                    warning_temp_notice_hidden = true;
                    UIManager::Get()->RepeatFrame();
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Separate from the main content if a warning was actually displayed
    if (warning_displayed)
    {
        ImGui::Separator();
    }
}

void WindowSettingsActionEdit::UpdateCatActions()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, 0);

    ImGui::BeginChild("ViewActionsSettings");

    ImGui::PopStyleColor();

    if (ImGui::IsWindowAppearing())
        UIManager::Get()->RepeatFrame();

    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Active Controller Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Active Controller Buttons");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (UIManager::Get()->IsOpenVRLoaded())
        {
            ImGui::FixedHelpMarker("Controller bindings when pointing at the overlay.\nClick here to configure the VR Dashboard controller bindings and change which buttons these are.");

            //Somewhat hidden, but still convenient shortcut to the controller binding page
            if ((UIManager::Get()->IsOpenVRLoaded()) && (ImGui::IsItemClicked()))
            {
                ImGui::OpenPopup("PopupOpenControllerBindingsCompositor");  //OpenPopupOnItemClick() doesn't work with this btw
            }

            if (ImGui::BeginPopup("PopupOpenControllerBindingsCompositor"))
            {
                if (ImGui::Selectable("Open VR Dashboard Controller Bindings"))
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

        ActionID actionid_home = (ActionID)ConfigManager::GetValue(configid_int_input_go_home_action_id);
        ActionID actionid_back = (ActionID)ConfigManager::GetValue(configid_int_input_go_back_action_id);

        ImGui::Columns(2, "ColumnControllerButtonActions", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Go Home\" Action");
        ImGui::NextColumn();

        if (ButtonAction("ActionGoHome", actionid_home))
        {
            ConfigManager::SetValue(configid_int_input_go_home_action_id, actionid_home);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_go_home_action_id, actionid_home);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Go Back\" Action");
        ImGui::NextColumn();
            
        if (ButtonAction("ActionGoBack", actionid_back))
        {
            ConfigManager::SetValue(configid_int_input_go_back_action_id, actionid_back);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_go_back_action_id, actionid_back);
        }

        ImGui::Columns(1);
    }

    //Global Controller Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Global Controller Buttons");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (UIManager::Get()->IsOpenVRLoaded())
        {
            ImGui::FixedHelpMarker("Controller bindings when the dashboard is closed and not pointing at an overlay.\nClick here to configure the Desktop+ controller bindings and change which buttons these are.");

            //Somewhat hidden, but still convenient shortcut to the controller binding page
            if ((UIManager::Get()->IsOpenVRLoaded()) && (ImGui::IsItemClicked()))
            {
                ImGui::OpenPopup("PopupOpenControllerBindingsDesktopPlus");  //OpenPopupOnItemClick() doesn't work with this btw
            }

            if (ImGui::BeginPopup("PopupOpenControllerBindingsDesktopPlus"))
            {
                if (ImGui::Selectable("Open Desktop+ Controller Bindings"))
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

        ActionID actionid_global_01 = (ActionID)ConfigManager::GetValue(configid_int_input_shortcut01_action_id);
        ActionID actionid_global_02 = (ActionID)ConfigManager::GetValue(configid_int_input_shortcut02_action_id);
        ActionID actionid_global_03 = (ActionID)ConfigManager::GetValue(configid_int_input_shortcut03_action_id);

        ImGui::Columns(2, "ColumnControllerButtonGlobalActions", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Global Shortcut 1\" Action");
        ImGui::NextColumn();

        if (ButtonAction("ActionGlobalShortcut1", actionid_global_01))
        {
            ConfigManager::SetValue(configid_int_input_shortcut01_action_id, actionid_global_01);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_shortcut01_action_id, actionid_global_01);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Global Shortcut 2\" Action");
        ImGui::NextColumn();

        if (ButtonAction("ActionGlobalShortcut2", actionid_global_02))
        {
            ConfigManager::SetValue(configid_int_input_shortcut02_action_id, actionid_global_02);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_shortcut02_action_id, actionid_global_02);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Global Shortcut 3\" Action");
        ImGui::NextColumn();

        if (ButtonAction("ActionGlobalShortcut3", actionid_global_03))
        {
            ConfigManager::SetValue(configid_int_input_shortcut03_action_id, actionid_global_03);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_shortcut03_action_id, actionid_global_03);
        }

        ImGui::Columns(1);
    }

    //Global Hotkeys
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Global Hotkeys");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("System-wide keyboard shortcuts.\nHotkeys block other applications from receiving that input and may not work if the same combination has already been registered elsewhere.");

        ActionID actionid_hotkey_01 = (ActionID)ConfigManager::GetValue(configid_int_input_hotkey01_action_id);
        ActionID actionid_hotkey_02 = (ActionID)ConfigManager::GetValue(configid_int_input_hotkey02_action_id);
        ActionID actionid_hotkey_03 = (ActionID)ConfigManager::GetValue(configid_int_input_hotkey03_action_id);

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
            ConfigManager::SetValue(configid_int_input_hotkey01_action_id, actionid_hotkey_01);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_hotkey01_action_id, actionid_hotkey_01);
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
            ConfigManager::SetValue(configid_int_input_hotkey02_action_id, actionid_hotkey_02);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_hotkey02_action_id, actionid_hotkey_02);
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
            ConfigManager::SetValue(configid_int_input_hotkey03_action_id, actionid_hotkey_03);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_hotkey03_action_id, actionid_hotkey_03);
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
            ActionID action_id = (ActionID)(act_index + action_custom);

            ImGui::PushID(&action);
            if (ImGui::Selectable(ActionManager::Get().GetActionName(action_id), (list_selected_index == act_index) ))
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
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current, actions.size() - 1);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 1);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_value_int, (int)act.FunctionType);

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

    //Desktop Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Floating UI Desktop Buttons");
        ImGui::Columns(2, "ColumnDesktopButtons", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Listing Style");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);
        const char* items[] = { "None", "Individual Desktops", "Cycle Buttons" };
        int button_style = clamp(ConfigManager::GetRef(configid_int_interface_mainbar_desktop_listing), 0, IM_ARRAYSIZE(items) - 1);
        if (ImGui::Combo("##ComboButtonStyle", &button_style, items, IM_ARRAYSIZE(items)))
        {
            ConfigManager::SetValue(configid_int_interface_mainbar_desktop_listing, button_style);
            UIManager::Get()->RepeatFrame();
        }

        ImGui::NextColumn();

        bool& include_all = ConfigManager::GetRef(configid_bool_interface_mainbar_desktop_include_all);
        if (ImGui::Checkbox("Add Combined Desktop", &include_all))
        {
            UIManager::Get()->RepeatFrame();
        }

        ImGui::Columns(1);
    }

    //Action Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Floating UI/Overlay Bar Action Buttons");

        ImGui::Indent();

        if (ImGui::BeginTabBar("TabBarActionOrder"))
        {
            if (ImGui::BeginTabItem("Floating UI", 0, ImGuiTabItemFlags_NoPushId))
            {
                ActionOrderSetting();
            }

            if (ImGui::BeginTabItem("Overlay Bar", 0, ImGuiTabItemFlags_NoPushId))
            {
                ActionOrderSetting(k_ulOverlayID_None - 1);
            }

            ImGui::EndTabBar();
        }

        ImGui::Unindent();
    }

    ImGui::EndChild();
}

bool WindowSettingsActionEdit::ButtonKeybind(unsigned char* key_code, bool no_mouse)
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

bool WindowSettingsActionEdit::ButtonAction(const char* str_id, ActionID& action_id)
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
            ActionID action_id = (ActionID)(act_index + action_custom);

            ImGui::PushID(&action);
            if (ImGui::Selectable(ActionManager::Get().GetActionName(action_id), (action_id == list_id) ))
            {
                list_id = action_id;
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

bool WindowSettingsActionEdit::ButtonHotkey(unsigned int hotkey_id)
{
    static std::string hotkey_name[3];

    hotkey_id = std::min(hotkey_id, 2u);

    unsigned int  flags   = 0;
    unsigned char keycode = 0;

    switch (hotkey_id)
    {
        case 0:
        {
            flags   = (unsigned int) ConfigManager::GetValue(configid_int_input_hotkey01_modifiers);
            keycode = (unsigned char)ConfigManager::GetValue(configid_int_input_hotkey01_keycode);
            break;
        }
        case 1:
        {
            flags   = (unsigned int) ConfigManager::GetValue(configid_int_input_hotkey02_modifiers);
            keycode = (unsigned char)ConfigManager::GetValue(configid_int_input_hotkey02_keycode);
            break;
        }
        case 2:
        {
            flags   = (unsigned int) ConfigManager::GetValue(configid_int_input_hotkey03_modifiers);
            keycode = (unsigned char)ConfigManager::GetValue(configid_int_input_hotkey03_keycode);
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

    float scale_mul = 1.0f;
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
                    ConfigManager::SetValue(configid_int_input_hotkey01_modifiers, (int)flags);
                    ConfigManager::SetValue(configid_int_input_hotkey01_keycode,   keycode_edit);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_hotkey01_modifiers, (int)flags);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_hotkey01_keycode,   keycode_edit);
                    break;
                }
                case 1: 
                {
                    ConfigManager::SetValue(configid_int_input_hotkey02_modifiers, (int)flags);
                    ConfigManager::SetValue(configid_int_input_hotkey02_keycode, keycode_edit);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_hotkey02_modifiers, (int)flags);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_hotkey02_keycode,   keycode_edit);
                    break;
                }
                case 2: 
                {
                    ConfigManager::SetValue(configid_int_input_hotkey03_modifiers, (int)flags);
                    ConfigManager::SetValue(configid_int_input_hotkey03_keycode, keycode_edit);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_hotkey03_modifiers, (int)flags);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_input_hotkey03_keycode,   keycode_edit);
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

void WindowSettingsActionEdit::ActionOrderSetting(unsigned int overlay_id)
{
    static int list_selected_pos = -1;
    bool use_global_order = false;
    bool is_overlay_bar = (overlay_id == k_ulOverlayID_None -1);    //This isn't consistent with what the other UI code does, but this is hacked in for now

    /*if (overlay_id != UINT_MAX)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

        if (ImGui::Checkbox("Use Global Setting", &ConfigManager::GetRef(configid_bool_overlay_actionbar_order_use_global)))
        {
            UIManager::Get()->RepeatFrame();
        }

        use_global_order = ConfigManager::GetValue(configid_bool_overlay_actionbar_order_use_global);
    }*/

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

    auto& action_order = (is_overlay_bar) ? ActionManager::Get().GetActionOverlayBarOrder() : 
                         (overlay_id == UINT_MAX) ? ConfigManager::Get().GetActionMainBarOrder() : OverlayManager::Get().GetConfigData(overlay_id).ConfigActionBarOrder;

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

bool WindowSettingsActionEdit::ActionButtonRow(ActionID action_id, int list_pos, int& list_selected_pos, unsigned int overlay_id)
{
    bool is_overlay_bar = (overlay_id == k_ulOverlayID_None -1);

    auto& action_order = (is_overlay_bar) ? ActionManager::Get().GetActionOverlayBarOrder() : 
                         (overlay_id == UINT_MAX) ? ConfigManager::Get().GetActionMainBarOrder() : OverlayManager::Get().GetConfigData(overlay_id).ConfigActionBarOrder;

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

void WindowSettingsActionEdit::PopupActionEdit(CustomAction& action, int id)
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

        //Add a tooltip when translation ID is used to minimize confusion about the name not matching what's displayed outside this popup
        if (action.NameTranslationID != tstr_NONE)
        {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("This action currently uses a translation string ID as a name to automatically match the chosen application language");
        }

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
                b_size_default.x *= ConfigManager::GetValue(configid_float_interface_last_vr_ui_scale);
                b_size_default.y *= ConfigManager::GetValue(configid_float_interface_last_vr_ui_scale);
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, 0);

            if (ImGui::ButtonWithWrappedLabel(buf_name, b_size_default))
            {
                ImGui::OpenPopup("Select Icon");
            }

            ImGui::PopStyleColor();;
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
            action.UpdateNameTranslationID();

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

bool WindowSettingsActionEdit::PopupIconSelect(std::string& filename)
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

WindowSettingsActionEdit::WindowSettingsActionEdit() : m_Visible(false), m_ActionEditIsNew(false)
{

}

void WindowSettingsActionEdit::Show()
{
    if (m_Size.x == 0.0f)
    {
        m_Size.x = UITextureSpaces::Get().GetRect(ui_texspace_total).GetWidth() * UIManager::Get()->GetUIScale();

        if (UIManager::Get()->IsInDesktopMode())    //Act as a "fullscreen" window if in desktop mode
            m_Size.y = ImGui::GetIO().DisplaySize.y;
        else
            m_Size.y = ImGui::GetIO().DisplaySize.y * 0.84f;
    }

    m_Visible = true;
}

void WindowSettingsActionEdit::Update()
{  
    //In desktop mode it's the only thing displayed, so no transition
    if (!m_Visible)
    {
        Show();
    }

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

    ImGui::Begin("WindowSettingsActionEdit", nullptr, flags);

    //Early pop as we have popups which should have a normal border
    if (UIManager::Get()->IsInDesktopMode())
    {
        ImGui::PopStyleVar(); //ImGuiStyleVar_WindowBorderSize
        ImGui::PopStyleVar(); //ImGuiStyleVar_WindowRounding
    }

    //Right
    ImGui::BeginGroup();

    UpdateWarnings();
    UpdateCatActions();

    ImGui::EndGroup();

    ImGui::End();
}

bool WindowSettingsActionEdit::IsShown() const
{
    return m_Visible;
}

const ImVec2& WindowSettingsActionEdit::GetSize() const
{
    return m_Size;
}
