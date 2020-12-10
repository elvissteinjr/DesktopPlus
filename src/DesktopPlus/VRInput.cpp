#include "VRInput.h"
#include "VRInput.h"

#define NOMINMAX
#include <string>
#include <windows.h>

#include "ConfigManager.h"
#include "OutputManager.h"

VRInput::VRInput() : m_HandleActionsetShortcuts(vr::k_ulInvalidActionSetHandle),
                     m_HandleActionSetDetachedInteractive(vr::k_ulInvalidActionHandle),
                     m_HandleActionDoGlobalShortcut01(vr::k_ulInvalidActionHandle),
                     m_HandleActionDoGlobalShortcut02(vr::k_ulInvalidActionHandle),
                     m_HandleActionDoGlobalShortcut03(vr::k_ulInvalidActionHandle),
                     m_IsAnyActionBound(false),
                     m_IsAnyActionBoundStateValid(false)
{
}

bool VRInput::Init()
{
    bool ret = false;

    //Load manifest, this will fail with VRInputError_MismatchedActionManifest when a Steam configured manifest is already associated with the app key, but we can just ignore that
    vr::EVRInputError input_error = vr::VRInput()->SetActionManifestPath( (ConfigManager::Get().GetApplicationPath() + "action_manifest.json").c_str() );

    if ( (input_error == vr::VRInputError_None) || (input_error == vr::VRInputError_MismatchedActionManifest) )
    {
        input_error = vr::VRInput()->GetActionSetHandle("/actions/shortcuts", &m_HandleActionsetShortcuts);

        if (input_error != vr::VRInputError_None)
            return false;

        //Load actions (we assume that the files are not messed with and skip some error checking)
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/SetDetachedInteractive", &m_HandleActionSetDetachedInteractive);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut01",       &m_HandleActionDoGlobalShortcut01);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut02",       &m_HandleActionDoGlobalShortcut02);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut03",       &m_HandleActionDoGlobalShortcut03);

        ret = true;
    }

    return ret;
}

void VRInput::Update()
{
    if (m_HandleActionsetShortcuts == vr::k_ulInvalidActionSetHandle)
        return;

    vr::VRActiveActionSet_t actionset_desc = { 0 };
    actionset_desc.ulActionSet = m_HandleActionsetShortcuts;
    actionset_desc.nPriority = 10;  //Random number, INT_MAX doesn't work

    vr::EVRInputError error = vr::VRInput()->UpdateActionState(&actionset_desc, sizeof(actionset_desc), 1);

    //SteamVR Input is incredibly weird with the initial action state. The first couple attempts at getting any action state will fail. Probably some async loading stuff
    //However, SteamVR also does not send any events once the initial state goes valid (it does for binding state changes after this)
    //As we don't want to needlessly refresh the any-action-bound state on every update, we poll it until it succeeds once and then rely on events afterwards
    if (!m_IsAnyActionBoundStateValid)
    {
        RefreshAnyActionBound();
    }
}

void VRInput::RefreshAnyActionBound()
{
    //Doesn't trigger on app start since it's actions are not valid yet
    vr::VRActionHandle_t action_handles[] = 
    {
        m_HandleActionSetDetachedInteractive,
        m_HandleActionDoGlobalShortcut01,
        m_HandleActionDoGlobalShortcut02,
        m_HandleActionDoGlobalShortcut03
    };

    vr::VRInputValueHandle_t action_origin = vr::k_ulInvalidInputValueHandle;
    m_IsAnyActionBound = false;

    for (auto handle : action_handles)
    {
        vr::EVRInputError error = vr::VRInput()->GetActionOrigins(m_HandleActionsetShortcuts, handle, &action_origin, 1);

        if (action_origin != vr::k_ulInvalidInputValueHandle) 
        { 
            m_IsAnyActionBoundStateValid = true;

            vr::InputOriginInfo_t action_origin_info = {0};
            vr::EVRInputError error = vr::VRInput()->GetOriginTrackedDeviceInfo(action_origin, &action_origin_info, sizeof(vr::InputOriginInfo_t));

            if ( (error == vr::VRInputError_None) && (vr::VRSystem()->IsTrackedDeviceConnected(action_origin_info.trackedDeviceIndex)) )
            {
                m_IsAnyActionBound = true;
                return;
            }
        }
        else
        {
            m_IsAnyActionBoundStateValid = false;
        }
    }
}

void VRInput::HandleGlobalActionShortcuts(OutputManager& outmgr)
{
    vr::InputDigitalActionData_t data;
    vr::EVRInputError input_error = vr::VRInput()->GetDigitalActionData(m_HandleActionDoGlobalShortcut01, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

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

    input_error = vr::VRInput()->GetDigitalActionData(m_HandleActionDoGlobalShortcut02, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

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

    input_error = vr::VRInput()->GetDigitalActionData(m_HandleActionDoGlobalShortcut03, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

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

bool VRInput::GetSetDetachedInteractiveDown() const
{
    vr::InputDigitalActionData_t data;
    vr::EVRInputError input_error = vr::VRInput()->GetDigitalActionData(m_HandleActionSetDetachedInteractive, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    if (input_error == vr::VRInputError_None)
    {
        return data.bState;
    }

    return false;
}

bool VRInput::IsAnyActionBound() const
{
    return m_IsAnyActionBound;
}