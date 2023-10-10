#include "Actions.h"

#include <sstream>
#include <random>
#include <ctime>

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windowsx.h>
#include <ShlDisp.h>

#include "ConfigManager.h"
#include "InterprocessMessaging.h"
#include "OverlayManager.h"
#include "Util.h"
#include "Logging.h"
#include "Ini.h"

#include "DPBrowserAPIClient.h"

#ifndef DPLUS_UI
    #include "OutputManager.h"
#endif

#ifdef DPLUS_UI
    #include "ImGuiExt.h"
#endif

/*
Command type data
-
command_key: UIntID = keycode, UIntArg = ToggleKeys bool
command_mouse_pos: UIntID = X & Y (in low/high word order, signed)
command_string: StrMain = string
command_launch_app: StrMain = path, StrArg = arguments
command_show_keyboard: UIntArg = CommandToggleArg
command_crop_active_window: no data
command_show_overlay: StrMain = tags string, UIntID = UseTargetTags bool & UndoOnRelease bool (in low/high word order), UIntArg = CommandToggleArg
command_switch_task: no data
*/

const char* ActionCommand::s_CommandTypeNames[ActionCommand::command_MAX] = 
{
    "None",
    "Key",
    "MousePos",
    "String",
    "LaunchApp",
    "ShowKeyboard",
    "CropActiveWindow",
    "ShowOverlay",
    "SwitchTask",
    "Unknown"
};

std::string ActionCommand::Serialize() const
{
    std::stringstream ss(std::ios::out | std::ios::binary);
    size_t str_size = 0;

    ss.write((const char*)&Type,     sizeof(Type));
    ss.write((const char*)&UIntID,   sizeof(UIntID));
    ss.write((const char*)&UIntArg,  sizeof(UIntArg));

    str_size = StrMain.size();
    ss.write((const char*)&str_size, sizeof(str_size));
    ss.write(StrMain.data(),         str_size);

    str_size = StrArg.size();
    ss.write((const char*)&str_size, sizeof(str_size));
    ss.write(StrArg.data(),          str_size);

    return ss.str();
}

void ActionCommand::Deserialize(const std::string& str)
{
    std::stringstream ss(str, std::ios::in | std::ios::binary);

    ActionCommand new_command;
    size_t str_length = 0;

    ss.read((char*)&new_command.Type,    sizeof(Type));
    ss.read((char*)&new_command.UIntID,  sizeof(UIntID));
    ss.read((char*)&new_command.UIntArg, sizeof(UIntArg));

    ss.read((char*)&str_length, sizeof(str_length));
    str_length = std::min(str_length, (size_t)4096);    //Arbitrary size limit to avoid large allocations on garbage data
    new_command.StrMain.resize(str_length);
    ss.read(&new_command.StrMain[0], str_length);

    ss.read((char*)&str_length, sizeof(str_length));
    str_length = std::min(str_length, (size_t)4096);
    new_command.StrArg.resize(str_length);
    ss.read(&new_command.StrArg[0], str_length);

    //Replace all data with the read command if there were no stream errors
    if (ss.good())
        *this = new_command;
}


std::string Action::Serialize() const
{
    std::stringstream ss(std::ios::out | std::ios::binary);
    size_t str_size = 0;

    ss.write((const char*)&UID, sizeof(UID));

    str_size = Name.size();
    ss.write((const char*)&str_size, sizeof(str_size));
    ss.write(Name.data(),            str_size);

    str_size = Label.size();
    ss.write((const char*)&str_size, sizeof(str_size));
    ss.write(Label.data(),           str_size);

    size_t command_count = Commands.size();
    ss.write((const char*)&command_count, sizeof(command_count));

    for (const auto& command : Commands)
    {
        std::string command_serialized = command.Serialize();

        str_size = command_serialized.size();
        ss.write((const char*)&str_size,    sizeof(str_size));
        ss.write(command_serialized.data(), str_size);
    }

    ss.write((const char*)&TargetUseTags, sizeof(TargetUseTags));

    //Tags are still preserved even when not used
    str_size = TargetTags.size();
    ss.write((const char*)&str_size, sizeof(str_size));
    ss.write(TargetTags.data(),      str_size);

    str_size = IconFilename.size();
    ss.write((const char*)&str_size, sizeof(str_size));
    ss.write(IconFilename.data(),    str_size);

    return ss.str();
}

void Action::Deserialize(const std::string& str)
{
    std::stringstream ss(str, std::ios::in | std::ios::binary);

    Action new_action;
    size_t str_length = 0;

    ss.read((char*)&new_action.UID,    sizeof(UID));

    ss.read((char*)&str_length, sizeof(str_length));
    str_length = std::min(str_length, str.length());
    new_action.Name.resize(str_length);
    ss.read(&new_action.Name[0], str_length);

    ss.read((char*)&str_length, sizeof(str_length));
    str_length = std::min(str_length, str.length());
    new_action.Label.resize(str_length);
    ss.read(&new_action.Label[0], str_length);

    size_t command_count = 0;
    ss.read((char*)&command_count,     sizeof(command_count));

    for (size_t i = 0; i < command_count; ++i)
    {
        ss.read((char*)&str_length, sizeof(str_length));
        str_length = std::min(str_length, str.length());
        std::string command_serialized(str_length, '\0');
        ss.read(&command_serialized[0], str_length);

        ActionCommand new_command;
        new_command.Deserialize(command_serialized);

        new_action.Commands.push_back(new_command);
    }

    ss.read((char*)&new_action.TargetUseTags, sizeof(TargetUseTags));

    ss.read((char*)&str_length, sizeof(str_length));
    str_length = std::min(str_length, str.length());
    new_action.TargetTags.resize(str_length);
    ss.read(&new_action.TargetTags[0], str_length);

    ss.read((char*)&str_length, sizeof(str_length));
    str_length = std::min(str_length, str.length());
    new_action.IconFilename.resize(str_length);
    ss.read(&new_action.IconFilename[0], str_length);

    //Replace all data with the read action if there were no stream errors
    if (ss.good())
    {
        *this = new_action;

        #ifdef DPLUS_UI
            //Check for potential translation strings
            NameTranslationID  = ActionManager::GetTranslationIDForName(Name);
            LabelTranslationID = ActionManager::GetTranslationIDForName(Label);
        #endif
    }
}

ActionManager::ActionManager()
{
    //Set name for null action in case it does get displayed from stale references somewhere
    m_NullAction.Name = "tstr_ActionNone";

    #ifdef DPLUS_UI
        m_NullAction.NameTranslationID = tstr_ActionNone;
    #endif
}

#ifndef DPLUS_UI

void ActionManager::DoKeyCommand(const ActionCommand& command, OverlayIDList& overlay_targets, bool down) const
{
    bool has_pressed_for_desktop = false;   //Only do command once for inputs that end up on the desktop
    unsigned char keycode = command.UIntID;

    if (keycode == 0)
        return;

    for (unsigned int overlay_id : overlay_targets)
    {
        const Overlay& overlay = OverlayManager::Get().GetOverlay(overlay_id);
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

        if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser)
        {
            if (command.UIntArg == 1 /*ToggleKeys*/)
            {
                DPBrowserAPIClient::Get().DPBrowser_KeyboardToggleKey(overlay.GetHandle(), keycode);
            }
            else
            {
                DPBrowserAPIClient::Get().DPBrowser_KeyboardSetKeyState(overlay.GetHandle(), (down) ? dpbrowser_ipckbd_keystate_flag_key_down : (DPBrowserIPCKeyboardKeystateFlags)0, keycode);
            }
        }
        else if (!has_pressed_for_desktop)
        {
            if (OutputManager* outmgr = OutputManager::Get())
            {
                InputSimulator& input_sim = outmgr->GetInputSimulator();

                if (command.UIntArg == 1 /*ToggleKeys*/)
                {
                    input_sim.KeyboardToggleState(keycode);
                }
                else
                {
                    (down) ? input_sim.KeyboardSetDown(keycode) : input_sim.KeyboardSetUp(keycode);
                }
            }

            has_pressed_for_desktop = true;
        }
    }
}

void ActionManager::DoMousePosCommand(const ActionCommand& command, OverlayIDList& overlay_targets) const
{
    LPARAM pos_lparam = command.UIntID;
    int mouse_x = GET_X_LPARAM(pos_lparam);
    int mouse_y = GET_Y_LPARAM(pos_lparam);

    bool has_moved_for_desktop = false;   //Only do command once for inputs that end up on the desktop

    for (unsigned int overlay_id : overlay_targets)
    {
        const Overlay& overlay = OverlayManager::Get().GetOverlay(overlay_id);
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

        if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser)
        {
            DPBrowserAPIClient::Get().DPBrowser_MouseMove(overlay.GetHandle(), mouse_x, mouse_y);
        }
        else if (!has_moved_for_desktop)
        {
            if (OutputManager* outmgr = OutputManager::Get())
            {
                outmgr->GetInputSimulator().MouseMove(mouse_x, mouse_y);
            }
        }
    }
}

void ActionManager::DoStringCommand(const ActionCommand& command, OverlayIDList& overlay_targets) const
{
    bool has_typed_for_desktop = false;   //Only do command once for inputs that end up on the desktop

    for (unsigned int overlay_id : overlay_targets)
    {
        const Overlay& overlay = OverlayManager::Get().GetOverlay(overlay_id);
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

        if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser)
        {
            DPBrowserAPIClient::Get().DPBrowser_KeyboardTypeString(overlay.GetHandle(), command.StrMain);
        }
        else if (!has_typed_for_desktop)
        {
            if (OutputManager* outmgr = OutputManager::Get())
            {
                InputSimulator& input_sim = outmgr->GetInputSimulator();

                input_sim.KeyboardText(command.StrMain.c_str(), true);
                input_sim.KeyboardTextFinish();
            }
        }
    }
}

void ActionManager::DoLaunchAppCommand(const ActionCommand& command, OverlayIDList& /*overlay_targets*/) const
{
    const std::string& path_utf8 = command.StrMain;
    const std::string& arg_utf8  = command.StrArg;

    if (ConfigManager::GetValue(configid_bool_state_misc_elevated_mode_active))
    {
        HWND source_window = nullptr;
        if (OutputManager* outmgr = OutputManager::Get())
        {
            source_window = OutputManager::Get()->GetWindowHandle();
        }

        IPCManager::Get().SendStringToElevatedModeProcess(ipcestrid_launch_application_path, path_utf8, source_window);

        if (!arg_utf8.empty())
        {
            IPCManager::Get().SendStringToElevatedModeProcess(ipcestrid_launch_application_arg, arg_utf8, source_window);
        }

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_launch_application);
        return;
    }

    //Convert path and arg to utf16
    std::wstring path_wstr = WStringConvertFromUTF8(path_utf8.c_str());
    std::wstring arg_wstr  = WStringConvertFromUTF8(arg_utf8.c_str());

    if (!path_wstr.empty())
    {
        if (OutputManager* outmgr = OutputManager::Get())
        {
            outmgr->InitComIfNeeded();

            ::ShellExecute(nullptr, nullptr, path_wstr.c_str(), arg_wstr.c_str(), nullptr, SW_SHOWNORMAL);
        }
    }
}

void ActionManager::DoShowKeyboardCommand(const ActionCommand& command, OverlayIDList& overlay_targets) const
{
    //We cannot show the keyboard for multiple overlays, so use the first target if one is provided
    unsigned int overlay_source_id = (!overlay_targets.empty()) ? overlay_targets[0] : k_ulOverlayID_None;

    if (ConfigManager::GetValue(configid_bool_state_keyboard_visible))
    {
        if (command.UIntArg != ActionCommand::command_arg_always_show)  //Don't do anything if it's set to always show
        {
            IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_keyboard_show, -1);
        }
    }
    else if (command.UIntArg != ActionCommand::command_arg_always_hide)
    {
        //Tell UI to show keyboard assigned to overlay
        IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_keyboard_show, (overlay_source_id != k_ulOverlayID_None) ? (int)overlay_source_id : -2);

        //Set focused ID to source overlay ID if there is one
        if (overlay_source_id != k_ulOverlayID_None)
        {
            ConfigManager::Get().SetValue(configid_int_state_overlay_focused_id, (int)overlay_source_id);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_focused_id, (int)overlay_source_id);
        }
    }
}

void ActionManager::DoCropActiveWindowCommand(const ActionCommand& command, OverlayIDList& overlay_targets) const
{
    if (OutputManager* outmgr = OutputManager::Get())
    {
        for (unsigned int overlay_id : overlay_targets)
        {
            outmgr->CropToActiveWindowToggle(overlay_id);
        }
    }
}

void ActionManager::DoShowOverlayCommand(const ActionCommand& command, OverlayIDList& overlay_targets, bool undo) const
{
    if (OutputManager* outmgr = OutputManager::Get())
    {
        OverlayIDList overlay_targets_command;
        ActionCommand::CommandToggleArg command_arg = (ActionCommand::CommandToggleArg)command.UIntArg;
        const bool use_command_tags = (LOWORD(command.UIntID) == 1);
        const bool do_undo_command  = (HIWORD(command.UIntID) == 1);

        if (use_command_tags)
        {
            overlay_targets_command = OverlayManager::Get().FindOverlaysWithTags(command.StrMain.c_str());
        }

        if ((do_undo_command) && (undo))
        {
            //Swap show and hide command arguments when undoing
            switch (command.UIntArg)
            {
                case ActionCommand::command_arg_always_show: command_arg = ActionCommand::command_arg_always_hide; break;
                case ActionCommand::command_arg_always_hide: command_arg = ActionCommand::command_arg_always_show; break;
            }
        }
        else if (undo)
        {
            return; //Don't do anything on undo call when action isn't set to undo
        }

        for (unsigned int overlay_id : (use_command_tags) ? overlay_targets_command : overlay_targets)
        {
            const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
            
            bool is_enabled = true;
            switch (command_arg)
            {
                case ActionCommand::command_arg_toggle:      is_enabled = !data.ConfigBool[configid_bool_overlay_enabled]; break;
                case ActionCommand::command_arg_always_show: is_enabled = true;                                            break;
                case ActionCommand::command_arg_always_hide: is_enabled = false;                                           break;
            }
            
            outmgr->SetOverlayEnabled(overlay_id, is_enabled);
        }
    }
}

void ActionManager::DoSwitchTaskCommand(const ActionCommand& /*command*/, OverlayIDList& /*overlay_targets*/) const
{
    if (OutputManager* outmgr = OutputManager::Get())
    {
        outmgr->ShowWindowSwitcher();
    }
}

#endif //ifdef DPLUS_UI

#ifdef DPLUS_UI

void ActionManager::UpdateActionOrderListUI()
{
    if (m_Actions.size() == m_ActionOrderUI.size())
        return;

    //Start fresh, put everything in and sort by name (this shouldn't happen unless default config is blank)
    if (m_ActionOrderUI.empty())
    {
        LOG_F(INFO, "Action order list is empty, recreating...");

        for (const auto& action_pair : m_Actions)
        {
            const Action& action = action_pair.second;

            m_ActionOrderUI.push_back(action.UID);
        }

        std::sort(m_ActionOrderUI.begin(), m_ActionOrderUI.end(), 
                      [this](const auto& uid_a, const auto& uid_b)
                      { 
                          return (strcmp(GetTranslatedName(uid_a), GetTranslatedName(uid_b)) < 0); 
                      } 
                 );

        return;
    }

    //Remove any actions that don't exist first
    ValidateActionOrderList(m_ActionOrderUI);

    //Look for whatever is missing and add it (not fast but this doesn't happen in normal execution)
    LOG_F(INFO, "Action order list is missing entries, adding missing actions...");

    for (const auto& action_pair : m_Actions)
    {
        const Action& action = action_pair.second;

        auto it = std::find_if(m_ActionOrderUI.begin(), m_ActionOrderUI.end(), [&](const auto& uid) { return (uid == action.UID); } );

        if (it == m_ActionOrderUI.end())
        {
            m_ActionOrderUI.push_back(action.UID);

            LOG_F(INFO, "Added action %llu to action order list", action.UID);
        }
    }
}

void ActionManager::ValidateActionOrderList(ActionList& ui_order) const
{
    //Remove any actions that don't exist anymore
    auto it = std::remove_if(ui_order.begin(), ui_order.end(), [&](const auto& uid) { return !ActionExists(uid); } );
    ui_order.erase(it, ui_order.end());
}

#endif //DPLUS_UI

bool ActionManager::LoadActionsFromFile(const char* filename)
{
    m_Actions.clear();

    const std::string filename_str = (filename == nullptr) ? "actions.ini" : filename;
    const std::wstring wpath = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + filename_str).c_str() );
    const bool existed = FileExists(wpath.c_str());

    if (!existed)
        return false;

    Ini afile(wpath.c_str());

    for (const auto& uid_str : afile.GetSectionList())
    {
        if (uid_str.empty())
            continue;

        Action action;
        action.UID = std::strtoull(uid_str.c_str(), nullptr, 10);
        action.Name = afile.ReadString(uid_str.c_str(), "Name");

        action.Label = afile.ReadString(uid_str.c_str(), "Label");
        StringReplaceAll(action.Label, "\\n", "\n");    //Unescape newlines

        #ifdef DPLUS_UI
            action.NameTranslationID  = GetTranslationIDForName(action.Name);
            action.LabelTranslationID = GetTranslationIDForName(action.Label);
        #endif
        
        int command_count = afile.ReadInt(uid_str.c_str(), "CommandCount", 0);

        for (int i = 0; i < command_count; ++i)
        {
            ActionCommand command;
            std::string command_key = "Command" + std::to_string(i);

            //Get command type ID from string
            command.Type = ActionCommand::command_unknown;  //Unknown when no match found
            std::string str_type = afile.ReadString(uid_str.c_str(), (command_key + "Type").c_str(), ActionCommand::s_CommandTypeNames[ActionCommand::command_none]);

            for (size_t i = ActionCommand::command_none; i < ActionCommand::command_MAX; ++i)
            {
                if (str_type == ActionCommand::s_CommandTypeNames[i])
                {
                    command.Type = (ActionCommand::CommandType)i;
                    break;
                }
            }

            command.UIntID  = afile.ReadInt(uid_str.c_str(),    (command_key + "UIntID").c_str(),  0);
            command.UIntArg = afile.ReadInt(uid_str.c_str(),    (command_key + "UIntArg").c_str(), 0);
            command.StrMain = afile.ReadString(uid_str.c_str(), (command_key + "StrMain").c_str());
            command.StrArg  = afile.ReadString(uid_str.c_str(), (command_key + "StrArg").c_str());

            action.Commands.push_back(command);
        }

        action.TargetUseTags = afile.ReadBool(  uid_str.c_str(), "TargetUseTags", false);
        action.TargetTags    = afile.ReadString(uid_str.c_str(), "TargetTags");
        action.IconFilename  = afile.ReadString(uid_str.c_str(), "IconFilename");

        StoreAction(action);
    }

    return existed;
}

void ActionManager::SaveActionsToFile()
{
    std::wstring wpath = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "actions.ini").c_str() );

    //Don't write if no actions
    if (m_Actions.empty())
    {
        //Delete actions file instead of leaving an empty one behind
        if (FileExists(wpath.c_str()))
        {
            ::DeleteFileW(wpath.c_str());
        }

        return;
    }

    Ini afile(wpath.c_str(), true);

    for (const auto& action_pair : m_Actions)
    {
        const Action& action = action_pair.second;

        LOG_IF_F(ERROR, action_pair.first != action.UID, "Action UID values don't match! (%llu & %llu)", action_pair.first, action.UID);

        std::string uid_str = std::to_string(action.UID);

        afile.WriteString(uid_str.c_str(), "Name", action.Name.c_str());

        std::string label_escaped = action.Label;
        StringReplaceAll(label_escaped, "\n", "\\n");   //Escape newlines so they don't break the ini layout
        afile.WriteString(uid_str.c_str(), "Label", label_escaped.c_str());

        afile.WriteInt(uid_str.c_str(), "CommandCount", (int)action.Commands.size());

        int i = 0;
        for (const auto& command : action.Commands)
        {
            if ((command.Type == ActionCommand::command_none) || (command.Type >= ActionCommand::command_MAX))
                continue;

            std::string command_key = "Command" + std::to_string(i);

            afile.WriteString(uid_str.c_str(), (command_key + "Type").c_str(),    ActionCommand::s_CommandTypeNames[command.Type]);
            afile.WriteInt(uid_str.c_str(),    (command_key + "UIntID").c_str(),  (int)command.UIntID);
            afile.WriteInt(uid_str.c_str(),    (command_key + "UIntArg").c_str(), (int)command.UIntArg);
            afile.WriteString(uid_str.c_str(), (command_key + "StrMain").c_str(), command.StrMain.c_str());
            afile.WriteString(uid_str.c_str(), (command_key + "StrArg").c_str(),  command.StrArg.c_str());

            ++i;
        }

        afile.WriteBool(  uid_str.c_str(), "TargetUseTags", action.TargetUseTags);
        afile.WriteString(uid_str.c_str(), "TargetTags",    action.TargetTags.c_str());
        afile.WriteString(uid_str.c_str(), "IconFilename",  action.IconFilename.c_str());
    }

    afile.Save();
}

void ActionManager::RestoreActionsFromDefault()
{
    #ifdef DPLUS_UI
        m_ActionOrderUI.clear();
        m_ActionOrderBarDefault.clear();
        m_ActionOrderOverlayBar.clear();
    #endif
    LoadActionsFromFile("actions_default.ini");
}

const Action& ActionManager::GetAction(ActionUID action_uid) const
{
    auto it = m_Actions.find(action_uid);

    return (it != m_Actions.end()) ? it->second : m_NullAction;
}

bool ActionManager::ActionExists(ActionUID action_uid) const
{
    return (m_Actions.find(action_uid) != m_Actions.end());
}

void ActionManager::StoreAction(const Action& action)
{
    size_t action_count_prev = m_Actions.size();

    m_Actions[action.UID] = action;

    #ifdef DPLUS_UI
        //Add to UI order if this added a new action
        if (action_count_prev != m_Actions.size())
        {
            m_ActionOrderUI.push_back(action.UID);
        }
    #endif
}

void ActionManager::RemoveAction(ActionUID action_uid)
{
    m_Actions.erase(action_uid);

    #ifdef DPLUS_UI
        //Find in UI order and remove
        auto it = std::find_if(m_ActionOrderUI.begin(), m_ActionOrderUI.end(), [&](const auto& uid) { return (uid == action_uid); } );

        if (it != m_ActionOrderUI.end())
        {
            m_ActionOrderUI.erase(it);
        }
    #endif
}

void ActionManager::StartAction(ActionUID action_uid, unsigned int overlay_source_id) const
{
    #ifdef DPLUS_UI
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_start, action_uid);
    #else
        const Action& action = GetAction(action_uid);

        OverlayIDList overlay_targets;

        if (action.TargetUseTags)
        {
            overlay_targets = OverlayManager::Get().FindOverlaysWithTags(action.TargetTags.c_str());
        }
        else
        {
            //Use focused ID if there is no source ID
            if ( (overlay_source_id == k_ulOverlayID_None) && (ConfigManager::GetValue(configid_int_state_overlay_focused_id) != -1) )
            {
                overlay_source_id = (unsigned int)ConfigManager::GetValue(configid_int_state_overlay_focused_id);
            }

            overlay_targets.push_back(overlay_source_id);
        }

        for (const ActionCommand& command : action.Commands)
        {
            switch (command.Type)
            {
                case ActionCommand::command_key:                DoKeyCommand(             command, overlay_targets, true);  break;
                case ActionCommand::command_mouse_pos:          DoMousePosCommand(        command, overlay_targets);        break;
                case ActionCommand::command_string:             DoStringCommand(          command, overlay_targets);        break;
                case ActionCommand::command_launch_app:         DoLaunchAppCommand(       command, overlay_targets);        break;
                case ActionCommand::command_show_keyboard:      DoShowKeyboardCommand(    command, overlay_targets);        break;
                case ActionCommand::command_crop_active_window: DoCropActiveWindowCommand(command, overlay_targets);        break;
                case ActionCommand::command_show_overlay:       DoShowOverlayCommand(     command, overlay_targets, false); break;
                case ActionCommand::command_switch_task:        DoSwitchTaskCommand(      command, overlay_targets);        break;
                default:                                        break;
            }
        }
    #endif
}

void ActionManager::StopAction(ActionUID action_uid, unsigned int overlay_source_id) const
{
    #ifdef DPLUS_UI
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_stop, action_uid);
    #else
        const Action& action = GetAction(action_uid);

        OverlayIDList overlay_targets;

        if (action.TargetUseTags)
        {
            overlay_targets = OverlayManager::Get().FindOverlaysWithTags(action.TargetTags.c_str());
        }
        else
        {
            //Use focused ID if there is no source ID
            if ( (overlay_source_id == k_ulOverlayID_None) && (ConfigManager::GetValue(configid_int_state_overlay_focused_id) != -1) )
            {
                overlay_source_id = (unsigned int)ConfigManager::GetValue(configid_int_state_overlay_focused_id);
            }

            overlay_targets.push_back(overlay_source_id);
        }

        //This function only needs to release keys previously pressed by StartAction() for now, in reverse order to avoid anything depending on that
        for (auto it = action.Commands.crbegin(); it != action.Commands.crend(); ++it)
        {
            const ActionCommand& command = *it;

            switch (command.Type)
            {
                case ActionCommand::command_key:          DoKeyCommand(        command, overlay_targets, false); break;
                case ActionCommand::command_show_overlay: DoShowOverlayCommand(command, overlay_targets, true);  break;
                default:                                  break;
            }
        }
    #endif
}

void ActionManager::DoAction(ActionUID action_uid, unsigned int overlay_source_id) const
{
    StartAction(action_uid, overlay_source_id);
    StopAction(action_uid, overlay_source_id);
}

uint64_t ActionManager::GenerateUID() const
{
    //32-bit timestamp, but at least unsigned (good till 2106) and nothing really bad happens after that either
    uint32_t timestamp = (uint32_t)std::time(nullptr);

    std::random_device seed;
    std::mt19937 generator(seed());
    std::uniform_int_distribution<uint32_t> distribute(0, UINT_MAX);
    
    //Create UID out of random number and timestamp until there's no ID conflict
    uint64_t uid = 0;
    do
    {
        uint64_t rnd = distribute(generator);
        uid = (rnd << 32) | timestamp;
    }
    while (ActionExists(uid)); //Very unlikely, to be fair

    return uid;
}

std::string ActionManager::ActionOrderListToString(const ActionList& action_order)
{
    std::stringstream ss;

    for (const ActionUID uid : action_order)
    {
        ss << uid << ';';
    }

    return ss.str();
}

ActionManager::ActionList ActionManager::ActionOrderListFromString(const std::string& str)
{
    ActionList action_order;

    std::stringstream ss(str);
    ActionUID uid;
    char sep;

    for (;;)
    {
        ss >> uid >> sep;

        if (ss.fail())
            break;

        action_order.push_back(uid);
    }

    return action_order;
}

#ifdef DPLUS_UI

ActionUID ActionManager::DuplicateAction(const Action& action)
{
    Action action_dup = action;
    action_dup.UID = GenerateUID();

    //Replace translation string with a normal one, so we can attach a copy marker to the string
    if (action_dup.NameTranslationID != tstr_NONE)
    {
        action_dup.Name = TranslationManager::GetString(action_dup.NameTranslationID);
        action_dup.NameTranslationID = tstr_NONE;
    }

    action_dup.Name += " (Copy)";

    m_Actions[action_dup.UID] = action_dup;

    //Put it below the source action in the order list
    auto it = std::find(m_ActionOrderUI.begin(), m_ActionOrderUI.end(), action.UID);

    if (it != m_ActionOrderUI.end())
    {
        m_ActionOrderUI.insert(++it, action_dup.UID);
    }

    return action_dup.UID;
}

void ActionManager::ClearIconData()
{
    for (auto& action_pair : m_Actions)
    {
        Action& action = action_pair.second;

        action.IconImGuiRectID = -1;
        action.IconAtlasSize   = ImVec2();
        action.IconAtlasUV     = ImVec4();
    }
}

const ActionManager::ActionList& ActionManager::GetActionOrderListUI() const
{
    return m_ActionOrderUI;
}

void ActionManager::SetActionOrderListUI(const ActionList& ui_order)
{
    m_ActionOrderUI = ui_order;
    UpdateActionOrderListUI();
}

ActionManager::ActionList& ActionManager::GetActionOrderListBarDefault()
{
    return m_ActionOrderBarDefault;
}

const ActionManager::ActionList& ActionManager::GetActionOrderListBarDefault() const
{
    return m_ActionOrderBarDefault;
}

void ActionManager::SetActionOrderListBarDefault(const ActionList& ui_order)
{
    m_ActionOrderBarDefault = ui_order;
    ValidateActionOrderList(m_ActionOrderBarDefault);
}

ActionManager::ActionList& ActionManager::GetActionOrderListOverlayBar()
{
    return m_ActionOrderOverlayBar;
}

const ActionManager::ActionList& ActionManager::GetActionOrderListOverlayBar() const
{
    return m_ActionOrderOverlayBar;
}

void ActionManager::SetActionOrderListOverlayBar(const ActionList& ui_order)
{
    m_ActionOrderOverlayBar = ui_order;
    ValidateActionOrderList(m_ActionOrderOverlayBar);
}

const char* ActionManager::GetTranslatedName(ActionUID action_uid) const
{
    if (action_uid == k_ActionUID_Invalid)
        return TranslationManager::GetString(tstr_ActionNone);

    const Action& action = GetAction(action_uid);

    return (action.NameTranslationID == tstr_NONE) ? action.Name.c_str() : TranslationManager::GetString(action.NameTranslationID);
}

const char* ActionManager::GetTranslatedLabel(ActionUID action_uid) const
{
    const Action& action = GetAction(action_uid);

    return (action.LabelTranslationID == tstr_NONE) ? action.Label.c_str() : TranslationManager::GetString(action.LabelTranslationID);
}

std::vector<ActionManager::ActionNameListEntry> ActionManager::GetActionNameList()
{
    UpdateActionOrderListUI();

    std::vector<ActionManager::ActionNameListEntry> vec;

    for (const auto& uid : m_ActionOrderUI)
    {
        const Action& action = GetAction(uid);

        vec.push_back({action.UID, GetTranslatedName(action.UID)});
    }

    return vec;
}

std::vector<std::string> ActionManager::GetIconFileList()
{
    std::vector<std::string> file_list;

    const std::wstring wpath = WStringConvertFromUTF8(std::string(ConfigManager::Get().GetApplicationPath() + "images/icons/*.png").c_str());
    WIN32_FIND_DATA find_data;
    HANDLE handle_find = ::FindFirstFileW(wpath.c_str(), &find_data);

    if (handle_find != INVALID_HANDLE_VALUE)
    {
        do
        {
            file_list.push_back(StringConvertFromUTF16(find_data.cFileName));
        }
        while (::FindNextFileW(handle_find, &find_data) != 0);

        ::FindClose(handle_find);
    }

    return file_list;
}

TRMGRStrID ActionManager::GetTranslationIDForName(const std::string& str)
{
    //If the name starts with the translation string prefix, try finding the ID for it
    if (str.find("tstr_") == 0)
    {
        return TranslationManager::Get().GetStringID(str.c_str());
    }

    return tstr_NONE;
}

std::string ActionManager::GetCommandDescription(const ActionCommand& command, float max_width)
{
    std::string str;

    switch (command.Type)
    {
        case ActionCommand::command_none: str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescNone); break;
        case ActionCommand::command_key:
        {
            str = TranslationManager::GetString((command.UIntArg == 1) ? tstr_SettingsActionsEditCommandDescKeyToggle : tstr_SettingsActionsEditCommandDescKey);
            StringReplaceAll(str, "%KEYNAME%", GetStringForKeyCode(command.UIntID));
            break;
        }
        case ActionCommand::command_mouse_pos:
        {
            str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescMousePos);
            StringReplaceAll(str, "%X%", std::to_string( GET_X_LPARAM(command.UIntID) ));
            StringReplaceAll(str, "%Y%", std::to_string( GET_Y_LPARAM(command.UIntID) ));
            break;
        }
        case ActionCommand::command_string:
        {
            str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescString);

            //Remove any kind of newline character
            std::string strmain_single_line = command.StrMain;
            StringReplaceAll(strmain_single_line, "\r\n", " ");
            StringReplaceAll(strmain_single_line, "\n",   " ");
            StringReplaceAll(strmain_single_line, "\r",   " ");

            StringReplaceAll(str, "%STRING%", strmain_single_line);
            break;
        }
        case ActionCommand::command_launch_app:
        {
            str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescLaunchApp);
            StringReplaceAll(str, "%APP%",  command.StrMain);
            StringReplaceAll(str, "%ARGSOPT%", (!command.StrArg.empty()) ? TranslationManager::GetString(tstr_SettingsActionsEditCommandDescLaunchAppArgsOpt) : "");
            StringReplaceAll(str, "%ARGS%", command.StrArg);
            break;
        }
        case ActionCommand::command_show_keyboard:
        {
            switch (command.UIntArg)
            {
                case ActionCommand::command_arg_toggle:      str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescKeyboardToggle); break;
                case ActionCommand::command_arg_always_show: str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescKeyboardShow);   break;
                case ActionCommand::command_arg_always_hide: str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescKeyboardHide);   break;
                default:                                     str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescUnknown);        break;
            }
            break;
        }
        case ActionCommand::command_crop_active_window:
        {
            str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescCropWindow);
            break;
        }
        case ActionCommand::command_show_overlay:
        {
            switch (command.UIntArg)
            {
                case ActionCommand::command_arg_toggle:      str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescOverlayToggle); break;
                case ActionCommand::command_arg_always_show: str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescOverlayShow);   break;
                case ActionCommand::command_arg_always_hide: str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescOverlayHide);   break;
                default:                                     str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescUnknown);       break;
            }
            
            StringReplaceAll(str, "%TAGS%", (LOWORD(command.UIntID) == 1) ? command.StrMain : TranslationManager::GetString(tstr_SettingsActionsEditCommandDescOverlayTargetDefault));

            break;
        }
        case ActionCommand::command_switch_task:
        {
            str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescSwitchTask);
            break;
        }
        default: str = TranslationManager::GetString(tstr_SettingsActionsEditCommandDescUnknown); break;
    }

    str = ImGui::StringEllipsis(str.c_str(), max_width);

    return str;
}

#endif //DPLUS_UI