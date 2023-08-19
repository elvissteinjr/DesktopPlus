#include "VRInput.h"
#include "VRInput.h"

#define NOMINMAX
#include <string>
#include <windows.h>

#include "ConfigManager.h"
#include "OutputManager.h"

VRInput::VRInput() : m_HandleActionsetShortcuts(vr::k_ulInvalidActionSetHandle),
                     m_HandleActionsetLaserPointer(vr::k_ulInvalidActionSetHandle),
                     m_HandleActionsetScrollDiscrete(vr::k_ulInvalidActionSetHandle),
                     m_HandleActionsetScrollSmooth(vr::k_ulInvalidActionSetHandle),
                     m_HandleActionEnableGlobalLaserPointer(vr::k_ulInvalidActionHandle),
                     m_HandleActionDoGlobalShortcut01(vr::k_ulInvalidActionHandle),
                     m_HandleActionDoGlobalShortcut02(vr::k_ulInvalidActionHandle),
                     m_HandleActionDoGlobalShortcut03(vr::k_ulInvalidActionHandle),
                     m_HandleActionDoGlobalShortcut04(vr::k_ulInvalidActionHandle),
                     m_HandleActionDoGlobalShortcut05(vr::k_ulInvalidActionHandle),
                     m_HandleActionDoGlobalShortcut06(vr::k_ulInvalidActionHandle),
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
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/EnableGlobalLaserPointer",    &m_HandleActionEnableGlobalLaserPointer);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut01",            &m_HandleActionDoGlobalShortcut01);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut02",            &m_HandleActionDoGlobalShortcut02);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut03",            &m_HandleActionDoGlobalShortcut03);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut04",            &m_HandleActionDoGlobalShortcut04);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut05",            &m_HandleActionDoGlobalShortcut05);
        vr::VRInput()->GetActionHandle("/actions/shortcuts/in/GlobalShortcut06",            &m_HandleActionDoGlobalShortcut06);

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
    //Doesn't trigger on app start since its actions are not valid yet
    vr::VRActionHandle_t action_handles[] = 
    {
        m_HandleActionEnableGlobalLaserPointer,
        m_HandleActionDoGlobalShortcut01,
        m_HandleActionDoGlobalShortcut02,
        m_HandleActionDoGlobalShortcut03,
        m_HandleActionDoGlobalShortcut04,
        m_HandleActionDoGlobalShortcut05,
        m_HandleActionDoGlobalShortcut06,
    };

    vr::VRInputValueHandle_t action_origin = vr::k_ulInvalidInputValueHandle;
    m_IsAnyGlobalActionBound = false;

    for (auto handle : action_handles)
    {
        vr::EVRInputError error = vr::VRInput()->GetActionOrigins(m_HandleActionsetShortcuts, handle, &action_origin, 1);

        if (action_origin != vr::k_ulInvalidInputValueHandle) 
        { 
            m_IsAnyGlobalActionBoundStateValid = true;

            vr::InputOriginInfo_t action_origin_info = {0};
            vr::EVRInputError error = vr::VRInput()->GetOriginTrackedDeviceInfo(action_origin, &action_origin_info, sizeof(vr::InputOriginInfo_t));

            if ( (error == vr::VRInputError_None) && (vr::VRSystem()->IsTrackedDeviceConnected(action_origin_info.trackedDeviceIndex)) )
            {
                m_IsAnyGlobalActionBound = true;
                return;
            }
        }
        else
        {
            m_IsAnyGlobalActionBoundStateValid = false;
        }
    }
}

void VRInput::HandleGlobalActionShortcuts(OutputManager& outmgr)
{
    vr::InputDigitalActionData_t data;
    const std::pair<vr::VRActionHandle_t, ConfigID_Handle> shortcuts[] = 
    {
        {m_HandleActionDoGlobalShortcut01, configid_handle_input_shortcut01_action_uid}, 
        {m_HandleActionDoGlobalShortcut02, configid_handle_input_shortcut02_action_uid}, 
        {m_HandleActionDoGlobalShortcut03, configid_handle_input_shortcut03_action_uid},
        {m_HandleActionDoGlobalShortcut04, configid_handle_input_shortcut04_action_uid},
        {m_HandleActionDoGlobalShortcut05, configid_handle_input_shortcut05_action_uid},
        {m_HandleActionDoGlobalShortcut06, configid_handle_input_shortcut06_action_uid}
    };

    for (const auto& shortcut_pair : shortcuts)
    {
        vr::EVRInputError input_error = vr::VRInput()->GetDigitalActionData(shortcut_pair.first, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

        if ((input_error == vr::VRInputError_None) && (data.bChanged))
        {
            if (data.bState)
            {
                ConfigManager::Get().GetActionManager().StartAction( ConfigManager::GetValue(shortcut_pair.second) );
            }
            else
            {
                ConfigManager::Get().GetActionManager().StopAction( ConfigManager::GetValue(shortcut_pair.second) );
            }
        }
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