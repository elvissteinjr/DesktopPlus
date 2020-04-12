#include "VRInput.h"
#include "VRInput.h"

#include <string>
#include <windows.h>

#include "ConfigManager.h"
#include "OutputManager.h"

VRInput::VRInput() : m_handle_actionset_shortcuts(vr::k_ulInvalidActionSetHandle),
                     m_handle_action_set_overlay_detached(vr::k_ulInvalidActionHandle),
                     m_handle_action_set_detached_interactive(vr::k_ulInvalidActionHandle),
                     m_handle_action_do_global_shortcut_01(vr::k_ulInvalidActionHandle),
                     m_handle_action_do_global_shortcut_02(vr::k_ulInvalidActionHandle),
                     m_handle_action_do_global_shortcut_03(vr::k_ulInvalidActionHandle)
{
}

bool VRInput::Init()
{
    bool ret = false;

    //Load manifest finally
    vr::EVRInputError input_error = vr::VRInput()->SetActionManifestPath( (ConfigManager::Get().GetApplicationPath() + "action_manifest.json").c_str() );
            
    if (input_error == vr::VRInputError_None)
    {
        input_error = vr::VRInput()->GetActionSetHandle("/actions/shortcuts", &m_handle_actionset_shortcuts);

        if (input_error != vr::VRInputError_None)
            return false;

        //Load actions (we assume that the files are not messed with and skip some error checking)
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/SetOverlayDetached",     &m_handle_action_set_overlay_detached);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/SetDetachedInteractive", &m_handle_action_set_detached_interactive);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut01",       &m_handle_action_do_global_shortcut_01);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut02",       &m_handle_action_do_global_shortcut_02);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut03",       &m_handle_action_do_global_shortcut_03);

        ret = true;
    }

    return ret;
}

void VRInput::Update()
{
    if (m_handle_actionset_shortcuts == vr::k_ulInvalidActionSetHandle)
        return;

    vr::VRActiveActionSet_t actionset_desc = { 0 };
    actionset_desc.ulActionSet = m_handle_actionset_shortcuts;
    actionset_desc.nPriority = 10;  //Random number, INT_MAX doesn't work

    vr::EVRInputError error = vr::VRInput()->UpdateActionState(&actionset_desc, sizeof(actionset_desc), 1);

    /*if (error != vr::VRInputError_None)
    {
        OutputDebugString(L"VRInput broke");
    }*/
}

void VRInput::HandleGlobalActionShortcuts(OutputManager& outmgr)
{
    vr::InputDigitalActionData_t data;
    vr::EVRInputError input_error = vr::VRInput()->GetDigitalActionData(m_handle_action_do_global_shortcut_01, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    if ((input_error == vr::VRInputError_None) && (data.bChanged))
    {
        if (data.bState)
        {
            outmgr.DoStartAction((ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut01_action_id));
        }
        else
        {
            outmgr.DoStopAction((ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut01_action_id));
        }
    }

    input_error = vr::VRInput()->GetDigitalActionData(m_handle_action_do_global_shortcut_02, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    if ((input_error == vr::VRInputError_None) && (data.bChanged))
    {
        if (data.bState)
        {
            outmgr.DoStartAction((ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut02_action_id));
        }
        else
        {
            outmgr.DoStopAction((ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut02_action_id));
        }
    }

    input_error = vr::VRInput()->GetDigitalActionData(m_handle_action_do_global_shortcut_03, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    if ((input_error == vr::VRInputError_None) && (data.bChanged))
    {
        if (data.bState)
        {
            outmgr.DoStartAction((ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut03_action_id));
        }
        else
        {
            outmgr.DoStopAction((ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut03_action_id));
        }
    }
}

bool VRInput::HandleSetOverlayDetachedShortcut()
{
    vr::InputDigitalActionData_t data;
    vr::EVRInputError input_error = vr::VRInput()->GetDigitalActionData(m_handle_action_set_overlay_detached, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    if ((input_error == vr::VRInputError_None) && (data.bChanged))
    {
        ConfigManager::Get().SetConfigBool(configid_bool_overlay_detached, data.bState);
        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_bool_overlay_detached), data.bState);

        return true;
    }

    return false;
}

bool VRInput::GetSetDetachedInteractiveDown()
{
    vr::InputDigitalActionData_t data;
    vr::EVRInputError input_error = vr::VRInput()->GetDigitalActionData(m_handle_action_set_detached_interactive, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    if (input_error == vr::VRInputError_None)
    {
        return data.bState;
    }

    return false;
}
