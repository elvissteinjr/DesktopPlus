#include "Actions.h"

#include "ConfigManager.h"
#include "OverlayManager.h"
#include "InterprocessMessaging.h"

const char* g_ActionNames[] =
{
    "[None]",
    "Show Keyboard",
    "Crop to Active Window",
    //Custom
};


void CustomAction::ApplyIntFromConfig()
{
    const int sub = ConfigManager::Get().GetConfigInt(configid_int_state_action_current_sub);
    const int value = ConfigManager::Get().GetConfigInt(configid_int_state_action_value_int);

    if (sub == 1)
    {
        FunctionType = (CustomActionFunctionID)value;
    }
    else if (sub != 0)
    {
        switch (FunctionType)
        {
            case caction_press_keys:
            {
                if (sub < 5)
                {
                    KeyCodes[sub - 2] = (unsigned char)value;
                }
                break;
            }
            case caction_toggle_overlay_enabled_state:
            {
                IntID = value;
                break;
            }
            default: break;
        }
    }
}

void CustomAction::ApplyStringFromConfig()
{
    const int sub = ConfigManager::Get().GetConfigInt(configid_int_state_action_current_sub);
    const std::string& value = ConfigManager::Get().GetConfigString(configid_str_state_action_value_string);

    if (sub == 0)
    {
        Name = value;
    }
    else if (sub != 1)
    {
        switch (FunctionType)
        {
            case caction_type_string:
            {
                StrMain = value;
                break;
            }
            case caction_launch_application:
            {
                if (sub == 2)
                    StrMain = value;
                else
                    StrArg = value;

                break;
            }
            default: break;
        }
    }
}

void CustomAction::SendUpdateToDashboardApp(int id, HWND window_handle) const
{
    //Send changes over to dashboard application
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current), id);
    //Sendings strings stalls the application and the dashboard app doesn't use the name, so don't send it
    /*IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 0);
    IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_value_string, Name, window_handle);*/
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 1);
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_value_int), FunctionType);

    switch (FunctionType)
    {
        case caction_press_keys:
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 2);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_value_int), KeyCodes[0]);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 3);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_value_int), KeyCodes[1]);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 4);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_value_int), KeyCodes[2]);

            break;
        }
        case caction_type_string:
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 2);
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_value_string, StrMain, window_handle);

            break;
        }
        case caction_launch_application:
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 2);
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_value_string, StrMain, window_handle);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 3);
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_value_string, StrArg, window_handle);

            break;
        }
        case caction_toggle_overlay_enabled_state:
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 2);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_value_int), IntID);
        }
        default: break;
    }

}

std::vector<CustomAction>& ActionManager::GetCustomActions()
{
    return m_CustomActions;
}

std::vector<ActionMainBarOrderData>& ActionManager::GetActionMainBarOrder()
{
    return m_ActionMainBarOrder;
}

bool ActionManager::IsActionIDValid(ActionID action_id) const
{
    if (action_id >= action_custom)
    {
        return (ConfigManager::Get().GetCustomActions().size() > action_id - action_custom);
    }
    else
    {
        return (action_id < action_built_in_MAX);
    }
}

const char* ActionManager::GetActionName(ActionID action_id)
{
    if (action_id >= action_custom)
    {
        int custom_id = action_id - action_custom;

        if (m_CustomActions.size() > custom_id)
        {
            return m_CustomActions[custom_id].Name.c_str();
        }
        else //Custom action actually doesn't exist... shouldn't normally happen
        {
            return g_ActionNames[action_none];
        }
    }
    else if (action_id < action_built_in_MAX)
    {
        return g_ActionNames[action_id];
    }
    else
    {
        return g_ActionNames[action_none];
    }
}

void ActionManager::EraseCustomAction(int custom_action_id)
{
    if (m_CustomActions.size() > custom_action_id) //Actually exists
    {
        m_CustomActions.erase(m_CustomActions.begin() + custom_action_id);

        ActionID action_id = (ActionID)(action_custom + custom_action_id);

        //Fixup IDs in the mainbar order list and remove the entry for the deleted action
        auto it_del = m_ActionMainBarOrder.end();
        for (auto it = m_ActionMainBarOrder.begin(); it != m_ActionMainBarOrder.end(); ++it)
        {
            if (it->action_id > action_id)
            {
                it->action_id = (ActionID)(it->action_id - 1);
            }
            else if (it->action_id == action_id)
            {
                it_del = it; //Delete it after we're done iterating through this
            }
        }

        if (it_del != m_ActionMainBarOrder.end())
        {
            m_ActionMainBarOrder.erase(it_del);
        }

        //Do the same for every overlay
        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
        {
            auto& action_order = OverlayManager::Get().GetConfigData(i).ConfigActionBarOrder;

            auto it_del = action_order.end();
            for (auto it = action_order.begin(); it != action_order.end(); ++it)
            {
                if (it->action_id > action_id)
                {
                    it->action_id = (ActionID)(it->action_id - 1);
                }
                else if (it->action_id == action_id)
                {
                    it_del = it; //Delete it after we're done iterating through this
                }
            }

            if (it_del != action_order.end())
            {
                action_order.erase(it_del);
            }
        }

        //Set button binding to none if it was bound before
        if (ConfigManager::Get().GetConfigInt(configid_int_input_go_home_action_id) == action_id)
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_go_home_action_id, action_none);
        }
        if (ConfigManager::Get().GetConfigInt(configid_int_input_go_back_action_id) == action_id)
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_go_back_action_id, action_none);
        }
    }
}

CustomActionFunctionID ActionManager::ParseCustomActionFunctionString(const std::string& str)
{
    if (str == "PressKeys")
        return caction_press_keys;
    else if (str == "TypeString")
        return caction_type_string;
    else if (str == "LaunchApplication")
        return caction_launch_application;
    else if (str == "ToggleOverlayEnabledState")
        return caction_toggle_overlay_enabled_state;

    return caction_press_keys;
}

const char* ActionManager::CustomActionFunctionToString(CustomActionFunctionID function_id)
{
    switch (function_id)
    {
        case caction_press_keys:                   return "PressKeys";
        case caction_type_string:                  return "TypeString";
        case caction_launch_application:           return "LaunchApplication";
        case caction_toggle_overlay_enabled_state: return "ToggleOverlayEnabledState";
        default:                                   return "UnknownFunction";
    }
}

ActionManager& ActionManager::Get()
{
    return ConfigManager::Get().GetActionManager(); //Kinda a roundabout, but nice for readability
}
