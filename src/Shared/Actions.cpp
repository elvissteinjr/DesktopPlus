#include "Actions.h"

#include "ConfigManager.h"
#include "OverlayManager.h"
#include "InterprocessMessaging.h"

#ifdef DPLUS_UI

static const TRMGRStrID g_ActionNameIDs[] =
{
    tstr_ActionNone,
    tstr_ActionKeyboardShow,
    tstr_ActionWindowCrop,
    tstr_ActionOverlayGroupToggle1,
    tstr_ActionOverlayGroupToggle2,
    tstr_ActionOverlayGroupToggle3,
    tstr_ActionSwitchTask
};

static const TRMGRStrID g_ActionButtonLabelIDs[] =
{
    tstr_ActionNone,
    tstr_ActionKeyboardShow,
    tstr_ActionWindowCrop,
    tstr_ActionButtonOverlayGroupToggle1,
    tstr_ActionButtonOverlayGroupToggle2,
    tstr_ActionButtonOverlayGroupToggle3,
    tstr_ActionSwitchTask
};

#endif


void CustomAction::ApplyIntFromConfig()
{
    const int sub = ConfigManager::GetValue(configid_int_state_action_current_sub);
    const int value = ConfigManager::GetValue(configid_int_state_action_value_int);

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
                else if (sub == 5)
                {
                    IntID = value;
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
    const int sub = ConfigManager::GetValue(configid_int_state_action_current_sub);
    const std::string& value = ConfigManager::GetValue(configid_str_state_action_value_string);

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

                #ifdef DPLUS_UI
                    UpdateNameTranslationID();
                #endif
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

#ifdef DPLUS_UI

void CustomAction::UpdateNameTranslationID()
{
    //If the name starts with the translation string prefix, try finding the ID for it
    if (Name.find("tstr_") == 0)
    {
        NameTranslationID = TranslationManager::Get().GetStringID(Name.c_str());
    }
    else
    {
        NameTranslationID = tstr_NONE;
    }
}

#endif

void CustomAction::SendUpdateToDashboardApp(int id, HWND window_handle) const
{
    //Send changes over to dashboard application
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current, id);
    //Sendings strings stalls the application and the dashboard app doesn't use the name, so don't send it
    /*IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 0);
    IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_value_string, Name, window_handle);*/
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 1);
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_value_int, FunctionType);

    switch (FunctionType)
    {
        case caction_press_keys:
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 2);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_value_int, KeyCodes[0]);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 3);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_value_int, KeyCodes[1]);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 4);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_value_int, KeyCodes[2]);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 5);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_value_int, IntID);

            break;
        }
        case caction_type_string:
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 2);
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_value_string, StrMain, window_handle);

            break;
        }
        case caction_launch_application:
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 2);
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_value_string, StrMain, window_handle);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 3);
            IPCManager::Get().SendStringToDashboardApp(configid_str_state_action_value_string, StrArg, window_handle);

            break;
        }
        case caction_toggle_overlay_enabled_state:
        {
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_current_sub, 2);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_action_value_int, IntID);
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
        int custom_id = action_id - action_custom;
        return (m_CustomActions.size() > custom_id);
    }
    else
    {
        return (action_id < action_built_in_MAX);
    }
}

#ifdef DPLUS_UI

const char* ActionManager::GetActionName(ActionID action_id) const
{
    if (action_id >= action_custom)
    {
        int custom_id = action_id - action_custom;

        if (m_CustomActions.size() > custom_id)
        {
            //Use translation string if the action has one
            TRMGRStrID name_str_id = m_CustomActions[custom_id].NameTranslationID;
            return (name_str_id == tstr_NONE) ? m_CustomActions[custom_id].Name.c_str() : TranslationManager::GetString(name_str_id);
        }
        else //Custom action actually doesn't exist... shouldn't normally happen
        {
            return TranslationManager::GetString(tstr_ActionNone);
        }
    }
    else if ( (action_id >= action_none) && (action_id < action_built_in_MAX) )
    {
        return TranslationManager::GetString(g_ActionNameIDs[action_id]);
    }
    else
    {
        return TranslationManager::GetString(tstr_ActionNone);
    }
}

const char* ActionManager::GetActionButtonLabel(ActionID action_id) const
{
    if (action_id >= action_custom)
    {
        return GetActionName(action_id);
    }
    else if ( (action_id >= action_none) && (action_id < action_built_in_MAX) )
    {
        return TranslationManager::GetString(g_ActionButtonLabelIDs[action_id]);
    }
    else
    {
        return TranslationManager::GetString(tstr_ActionNone);
    }
}

#endif //ifdef DPLUS_UI

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
        if (ConfigManager::GetValue(configid_int_input_go_home_action_id) == action_id)
        {
            ConfigManager::SetValue(configid_int_input_go_home_action_id, action_none);
        }
        if (ConfigManager::GetValue(configid_int_input_go_back_action_id) == action_id)
        {
            ConfigManager::SetValue(configid_int_input_go_back_action_id, action_none);
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
