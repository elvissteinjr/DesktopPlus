#include "VRInput.h"
#include "VRInput.h"

#define NOMINMAX
#include <string>
#include <sstream>
#include <windows.h>

#include "ConfigManager.h"
#include "OutputManager.h"

VRInput::VRInput() : m_HandleActionsetShortcuts(vr::k_ulInvalidActionSetHandle),
                     m_HandleActionsetLaserPointer(vr::k_ulInvalidActionSetHandle),
                     m_HandleActionsetScrollDiscrete(vr::k_ulInvalidActionSetHandle),
                     m_HandleActionsetScrollSmooth(vr::k_ulInvalidActionSetHandle),
                     m_HandleActionEnableGlobalLaserPointer(vr::k_ulInvalidActionHandle),
                     m_HandleActionLaserPointerLeftClick(vr::k_ulInvalidActionHandle),
                     m_HandleActionLaserPointerRightClick(vr::k_ulInvalidActionHandle),
                     m_HandleActionLaserPointerMiddleClick(vr::k_ulInvalidActionHandle),
                     m_HandleActionLaserPointerAux01Click(vr::k_ulInvalidActionHandle),
                     m_HandleActionLaserPointerAux02Click(vr::k_ulInvalidActionHandle),
                     m_HandleActionLaserPointerScrollDiscrete(vr::k_ulInvalidActionHandle),
                     m_HandleActionLaserPointerScrollSmooth(vr::k_ulInvalidActionHandle),
                     m_HandleActionLaserPointerHaptic(vr::k_ulInvalidActionHandle),
                     m_IsAnyGlobalActionBound(false),
                     m_IsAnyGlobalActionBoundStateValid(false),
                     m_IsLaserPointerInputActive(false),
                     m_LaserPointerScrollMode(vrinput_scroll_none)
{
}

bool VRInput::Init()
{
    //Load manifest, this will fail with VRInputError_MismatchedActionManifest when a Steam configured manifest is already associated with the app key, but we can just ignore that
    vr::EVRInputError input_error = vr::VRInput()->SetActionManifestPath( (ConfigManager::Get().GetApplicationPath() + "action_manifest.json").c_str() );

    if ( (input_error == vr::VRInputError_None) || (input_error == vr::VRInputError_MismatchedActionManifest) )
    {
        input_error = vr::VRInput()->GetActionSetHandle("/actions/shortcuts", &m_HandleActionsetShortcuts);

        if (input_error != vr::VRInputError_None)
            return false;

        //Load actions (we assume that the files are not messed with and skip some error checking)
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/EnableGlobalLaserPointer", &m_HandleActionEnableGlobalLaserPointer);

        //Load as many global shortcut input actions as we can find. Up to configid_int_input_global_shortcuts_max_count at least.
        //This allows for extended amounts via end-user modification, though the Steam manifest takes priority if present
        m_HandleActionDoGlobalShortcuts.clear();
        const int shortcut_max = ConfigManager::GetValue(configid_int_input_global_shortcuts_max_count);
        for (int i = 0; i < shortcut_max; ++i)
        {
            vr::VRActionHandle_t handle_global_shortcut = vr::k_ulInvalidActionHandle;

            std::stringstream ss;
            ss << "/actions/shortcuts/in/GlobalShortcut" << std::setfill('0') << std::setw(2) << i + 1;

            vr::VRInput()->GetActionHandle(ss.str().c_str(), &handle_global_shortcut);

            //We do check if we got a handle here, but as of writing this, GetActionHandle() will always return a valid handle, regardless of presence in the manifest.
            if (handle_global_shortcut != vr::k_ulInvalidActionHandle)
            {
                m_HandleActionDoGlobalShortcuts.push_back(handle_global_shortcut);
            }
            else
            {
                break;
            }
        }

        vr::VRInput()->GetActionSetHandle("/actions/laserpointer",                &m_HandleActionsetLaserPointer);
        vr::VRInput()->GetActionHandle("/actions/laserpointer/in/LeftClick",      &m_HandleActionLaserPointerLeftClick);
        vr::VRInput()->GetActionHandle("/actions/laserpointer/in/RightClick" ,    &m_HandleActionLaserPointerRightClick);
        vr::VRInput()->GetActionHandle("/actions/laserpointer/in/MiddleClick",    &m_HandleActionLaserPointerMiddleClick);
        vr::VRInput()->GetActionHandle("/actions/laserpointer/in/Aux01Click",     &m_HandleActionLaserPointerAux01Click);
        vr::VRInput()->GetActionHandle("/actions/laserpointer/in/Aux02Click",     &m_HandleActionLaserPointerAux02Click);
        vr::VRInput()->GetActionHandle("/actions/laserpointer/out/Haptic",        &m_HandleActionLaserPointerHaptic);

        vr::VRInput()->GetActionSetHandle("/actions/scroll_discrete",                &m_HandleActionsetScrollDiscrete);
        vr::VRInput()->GetActionHandle("/actions/scroll_discrete/in/ScrollDiscrete", &m_HandleActionLaserPointerScrollDiscrete);

        vr::VRInput()->GetActionSetHandle("/actions/scroll_smooth",                &m_HandleActionsetScrollSmooth);
        vr::VRInput()->GetActionHandle("/actions/scroll_smooth/in/ScrollSmooth",   &m_HandleActionLaserPointerScrollSmooth);

        return true;
    }

    return false;
}

void VRInput::Update()
{
    if (m_HandleActionsetShortcuts == vr::k_ulInvalidActionSetHandle)
        return;

    vr::VRActiveActionSet_t actionset_desc[3] = { 0 };
    int actionset_active_count = 1;
    actionset_desc[0].ulActionSet = m_HandleActionsetShortcuts;
    actionset_desc[0].nPriority = 100; //Arbitrary number, but probably higher than the scene application's... if that even matters

    if (m_IsLaserPointerInputActive)
    {
        actionset_active_count = 2;

        actionset_desc[1].ulActionSet = m_HandleActionsetLaserPointer;
        //+2 when blocking since OVRAS uses vr::k_nActionSetOverlayGlobalPriorityMin + 1 as priority for global input
        //When not blocking laser pointer inputs should have priority over global shortcuts
        actionset_desc[1].nPriority = ConfigManager::GetValue(configid_bool_input_laser_pointer_block_input) ? vr::k_nActionSetOverlayGlobalPriorityMin + 2 : 101;

        if (m_LaserPointerScrollMode != vrinput_scroll_none)
        {
            actionset_active_count = 3;

            actionset_desc[2].ulActionSet = (m_LaserPointerScrollMode == vrinput_scroll_discrete) ? m_HandleActionsetScrollDiscrete : m_HandleActionsetScrollSmooth;
            actionset_desc[2].nPriority = actionset_desc[1].nPriority;
        }
    }

    vr::VRInput()->UpdateActionState(actionset_desc, sizeof(vr::VRActiveActionSet_t), actionset_active_count);

    //SteamVR Input is incredibly weird with the initial action state. The first couple attempts at getting any action state will fail. Probably some async loading stuff
    //However, SteamVR also does not send any events once the initial state goes valid (it does for binding state changes after this)
    //As we don't want to needlessly refresh the any-action-bound state on every update, we poll it until it succeeds once and then rely on events afterwards
    if (!m_IsAnyGlobalActionBoundStateValid)
    {
        RefreshAnyGlobalActionBound();
    }
}

void VRInput::RefreshAnyGlobalActionBound()
{
    auto is_action_bound = [&](vr::VRActionHandle_t action_handle)
    {
        vr::VRInputValueHandle_t action_origin = vr::k_ulInvalidInputValueHandle;
        vr::EVRInputError error = vr::VRInput()->GetActionOrigins(m_HandleActionsetShortcuts, action_handle, &action_origin, 1);

        if (action_origin != vr::k_ulInvalidInputValueHandle) 
        { 
            m_IsAnyGlobalActionBoundStateValid = true;

            vr::InputOriginInfo_t action_origin_info = {0};
            vr::EVRInputError error = vr::VRInput()->GetOriginTrackedDeviceInfo(action_origin, &action_origin_info, sizeof(vr::InputOriginInfo_t));

            if ( (error == vr::VRInputError_None) && (vr::VRSystem()->IsTrackedDeviceConnected(action_origin_info.trackedDeviceIndex)) )
            {
                return true;
            }
        }
        
        return false;
    };

    //Doesn't trigger on app start since its actions are not valid yet
    m_IsAnyGlobalActionBound = false;

    if (is_action_bound(m_HandleActionEnableGlobalLaserPointer))
    {
        m_IsAnyGlobalActionBound = true;
        return;
    }

    for (auto handle : m_HandleActionDoGlobalShortcuts)
    {
        if (is_action_bound(handle))
        {
            m_IsAnyGlobalActionBound = true;
            return;
        }
    }
}

void VRInput::HandleGlobalActionShortcuts(OutputManager& outmgr)
{
    const ActionManager::ActionList& shortcut_actions = ConfigManager::Get().GetGlobalShortcuts();
    vr::InputDigitalActionData_t data;

    size_t shortcut_id = 0;
    for (const auto shortcut_handle : m_HandleActionDoGlobalShortcuts)
    {
        vr::EVRInputError input_error = vr::VRInput()->GetDigitalActionData(shortcut_handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

        if ((shortcut_id < shortcut_actions.size()) && (input_error == vr::VRInputError_None) && (data.bChanged))
        {
            if (data.bState)
            {
                ConfigManager::Get().GetActionManager().StartAction(shortcut_actions[shortcut_id]);
            }
            else
            {
                ConfigManager::Get().GetActionManager().StopAction(shortcut_actions[shortcut_id]);
            }
        }

        ++shortcut_id;
    }
}

void VRInput::TriggerLaserPointerHaptics(vr::VRInputValueHandle_t restrict_to_device) const
{
    vr::VRInput()->TriggerHapticVibrationAction(m_HandleActionLaserPointerHaptic, 0.0f, 0.0f, 1.0f, 0.16f, restrict_to_device);
}

bool VRInput::GetSetDetachedInteractiveDown() const
{
    vr::InputDigitalActionData_t data;
    vr::EVRInputError input_error = vr::VRInput()->GetDigitalActionData(m_HandleActionEnableGlobalLaserPointer, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    if (input_error == vr::VRInputError_None)
    {
        return data.bState;
    }

    return false;
}

vr::InputDigitalActionData_t VRInput::GetEnableGlobalLaserPointerState() const
{
    vr::InputDigitalActionData_t data;
    vr::VRInput()->GetDigitalActionData(m_HandleActionEnableGlobalLaserPointer, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    return data;
}

std::vector<vr::InputOriginInfo_t> VRInput::GetLaserPointerDevicesInfo() const
{
    std::vector<vr::InputOriginInfo_t> devices_info;
    vr::VRInputValueHandle_t input_value_handles[vr::k_unMaxTrackedDeviceCount];
    vr::EVRInputError err = vr::VRInput()->GetActionOrigins(m_HandleActionsetLaserPointer, m_HandleActionLaserPointerLeftClick, input_value_handles, vr::k_unMaxTrackedDeviceCount);

    vr::InputOriginInfo_t origin_info = {0};
    for (auto input_value_handle : input_value_handles)
    {
        if ( (input_value_handle != vr::k_ulInvalidInputValueHandle) && (vr::VRInput()->GetOriginTrackedDeviceInfo(input_value_handle, &origin_info, sizeof(vr::InputOriginInfo_t)) == vr::VRInputError_None) )
        {
            devices_info.push_back(origin_info);
        }
    }

    //If GetActionOrigins() did not return anything useful, try at least getting origins for left and right hand controllers
    if (devices_info.empty())
    {
        for (int controller_role = vr::TrackedControllerRole_LeftHand; controller_role <= vr::TrackedControllerRole_RightHand; ++controller_role)
        {
            origin_info = {0};

            origin_info.trackedDeviceIndex = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole((vr::ETrackedControllerRole)controller_role);

            if (origin_info.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid)
            {
                vr::VRInputValueHandle_t input_value = vr::k_ulInvalidInputValueHandle;
                vr::VRInput()->GetInputSourceHandle((controller_role == vr::TrackedControllerRole_LeftHand) ? "/user/hand/left" : "/user/hand/right", &origin_info.devicePath);

                devices_info.push_back(origin_info);
            }
        }
    }

    return devices_info;
}

vr::InputDigitalActionData_t VRInput::GetLaserPointerLeftClickState(vr::VRInputValueHandle_t restrict_to_device) const
{
    vr::InputDigitalActionData_t data;
    vr::VRInput()->GetDigitalActionData(m_HandleActionLaserPointerLeftClick, &data, sizeof(data), restrict_to_device);

    return data;
}

std::array<vr::InputDigitalActionData_t, 5> VRInput::GetLaserPointerClickState(vr::VRInputValueHandle_t restrict_to_device) const
{
    std::array<vr::InputDigitalActionData_t, 5> data = {0};

    vr::VRInput()->GetDigitalActionData(m_HandleActionLaserPointerLeftClick,   &data[0], sizeof(vr::InputDigitalActionData_t), restrict_to_device);
    vr::VRInput()->GetDigitalActionData(m_HandleActionLaserPointerRightClick,  &data[1], sizeof(vr::InputDigitalActionData_t), restrict_to_device);
    vr::VRInput()->GetDigitalActionData(m_HandleActionLaserPointerMiddleClick, &data[2], sizeof(vr::InputDigitalActionData_t), restrict_to_device);
    vr::VRInput()->GetDigitalActionData(m_HandleActionLaserPointerAux01Click,  &data[3], sizeof(vr::InputDigitalActionData_t), restrict_to_device);
    vr::VRInput()->GetDigitalActionData(m_HandleActionLaserPointerAux02Click,  &data[4], sizeof(vr::InputDigitalActionData_t), restrict_to_device);

    return data;
}

vr::InputAnalogActionData_t VRInput::GetLaserPointerScrollDiscreteState() const
{
    vr::InputAnalogActionData_t data = {0};
    vr::VRInput()->GetAnalogActionData(m_HandleActionLaserPointerScrollDiscrete, &data, sizeof(vr::InputAnalogActionData_t), vr::k_ulInvalidInputValueHandle);

    return data;
}

vr::InputAnalogActionData_t VRInput::GetLaserPointerScrollSmoothState() const
{
    vr::InputAnalogActionData_t data = {0};
    vr::VRInput()->GetAnalogActionData(m_HandleActionLaserPointerScrollSmooth, &data, sizeof(vr::InputAnalogActionData_t), vr::k_ulInvalidInputValueHandle);

    return data;
}

void VRInput::SetLaserPointerActive(bool is_active)
{
    m_IsLaserPointerInputActive = is_active;
}

void VRInput::SetLaserPointerScrollMode(VRInputScrollMode scroll_mode)
{
    m_LaserPointerScrollMode = scroll_mode;
}

VRInputScrollMode VRInput::GetLaserPointerScrollMode() const
{
    return m_LaserPointerScrollMode;
}

bool VRInput::IsAnyGlobalActionBound() const
{
    return m_IsAnyGlobalActionBound;
}