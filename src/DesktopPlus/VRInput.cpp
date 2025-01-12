#include "VRInput.h"
#include "VRInput.h"

#define NOMINMAX
#include <string>
#include <sstream>
#include <windows.h>

#include "ConfigManager.h"
#include "OutputManager.h"
#include "OpenVRExt.h"

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
                     m_LaserPointerScrollMode(vrinput_scroll_none),
                     m_KeyboardDeviceInputValueHandle(vr::k_ulInvalidInputValueHandle),
                     m_GamepadDeviceInputValueHandle(vr::k_ulInvalidInputValueHandle),
                     m_KeyboardDeviceToggleState{0},
                     m_KeyboardDeviceIsToggleKeyDown(false),
                     m_KeyboardDeviceClickState{0},
                     m_KeyboardDeviceDragState{0}
{
}

void VRInput::UpdateKeyboardDeviceState()
{
    auto update_input_data = [](vr::InputDigitalActionData_t& input_data, int keycode)
    {
        if (keycode != 0)
        {
            if ((ConfigManager::GetValue(configid_bool_input_laser_pointer_hmd_device)) && (!vr::IVROverlayEx::IsSystemLaserPointerActive()))
            {
                input_data.bActive = true;
                input_data.bChanged = false;

                if (::GetAsyncKeyState(keycode) < 0)
                {
                    if (!input_data.bState)
                    {
                        input_data.bChanged = true;
                        input_data.bState = true;
                    }
                }
                else if (input_data.bState)
                {
                    input_data.bChanged = true;
                    input_data.bState = false;
                }
            }
            else
            {
                //Drop inputs if settings disabled or system laser pointer is active
                input_data.bActive  = input_data.bState;    //true for one frame before we set it inactive
                input_data.bChanged = input_data.bState;
                input_data.bState   = false;
            }
        }
        else
        {
            input_data.bActive  = false;
            input_data.bChanged = false;
            input_data.bState   = false;
        }
    };

    auto update_input_data_toggle = [](vr::InputDigitalActionData_t& input_data, int keycode, bool& is_key_down)
    {
        if (keycode != 0)
        {
            if ((ConfigManager::GetValue(configid_bool_input_laser_pointer_hmd_device)) && (!vr::IVROverlayEx::IsSystemLaserPointerActive()))
            {
                input_data.bActive  = true;
                input_data.bChanged = false;

                if (::GetAsyncKeyState(keycode) < 0)
                {
                    if (!is_key_down)
                    {
                        input_data.bChanged = true;
                        input_data.bState   = !input_data.bState;
                    }

                    is_key_down = true;
                }
                else
                {
                    is_key_down = false;
                }
            }
            else
            {
                //Drop inputs if settings disabled or system laser pointer is active (Desktop+ pointer won't be doing anything either way, but the toggle key should reset at least)
                input_data.bActive  = input_data.bState;    //true for one frame before we set it inactive
                input_data.bChanged = input_data.bState;
                input_data.bState   = false;
            }
        }
        else
        {
            input_data.bActive  = false;
            input_data.bChanged = false;
            input_data.bState   = false;
        }
    };

    //Toggle action state is always set up as a toggle binding
    update_input_data_toggle(m_KeyboardDeviceToggleState, ConfigManager::GetValue(configid_int_input_laser_pointer_hmd_device_keycode_toggle), m_KeyboardDeviceIsToggleKeyDown);

    update_input_data(m_KeyboardDeviceClickState[0], ConfigManager::GetValue(configid_int_input_laser_pointer_hmd_device_keycode_left));
    update_input_data(m_KeyboardDeviceClickState[1], ConfigManager::GetValue(configid_int_input_laser_pointer_hmd_device_keycode_right));
    update_input_data(m_KeyboardDeviceClickState[2], ConfigManager::GetValue(configid_int_input_laser_pointer_hmd_device_keycode_middle));
    //Aux01/02 are not configurable but fields exist for parity with the regular action data array (they can still be pressed via actions if really needed)

    update_input_data(m_KeyboardDeviceDragState, ConfigManager::GetValue(configid_int_input_laser_pointer_hmd_device_keycode_drag));
}

vr::InputDigitalActionData_t VRInput::CombineDigitalActionData(vr::InputDigitalActionData_t data_a, vr::InputDigitalActionData_t data_b)
{
    //Trying to make sense of having multiple action data sources at once, with some bias towards data_a
    vr::InputDigitalActionData_t data_out = {0};

    data_out.bActive  = data_a.bActive  || data_b.bActive;
    data_out.bChanged = data_a.bChanged || data_b.bChanged;
    data_out.bState   = data_a.bState   || data_b.bState;
    data_out.activeOrigin = (data_a.bState == data_out.bState) ? data_a.activeOrigin : ((data_b.bState == data_out.bState) ? data_b.activeOrigin : data_a.activeOrigin);

    return data_out;
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
        vr::VRInput()->GetActionHandle("/actions/laserpointer/in/Drag",           &m_HandleActionLaserPointerDrag);
        vr::VRInput()->GetActionHandle("/actions/laserpointer/out/Haptic",        &m_HandleActionLaserPointerHaptic);

        vr::VRInput()->GetActionSetHandle("/actions/scroll_discrete",                &m_HandleActionsetScrollDiscrete);
        vr::VRInput()->GetActionHandle("/actions/scroll_discrete/in/ScrollDiscrete", &m_HandleActionLaserPointerScrollDiscrete);

        vr::VRInput()->GetActionSetHandle("/actions/scroll_smooth",                &m_HandleActionsetScrollSmooth);
        vr::VRInput()->GetActionHandle("/actions/scroll_smooth/in/ScrollSmooth",   &m_HandleActionLaserPointerScrollSmooth);

        //This mimics OpenXR device path pattern but isn't actually formally defined (and this is OpenVR anyhow)
        vr::VRInput()->GetInputSourceHandle("/user/keyboard", &m_KeyboardDeviceInputValueHandle);
        //Frequently used gamepad device path
        vr::VRInput()->GetInputSourceHandle("/user/gamepad", &m_GamepadDeviceInputValueHandle);

        m_KeyboardDeviceToggleState.activeOrigin = m_KeyboardDeviceInputValueHandle;
        for (auto& input_data : m_KeyboardDeviceClickState)
        {
            input_data.activeOrigin = m_KeyboardDeviceInputValueHandle;
        }
        m_KeyboardDeviceDragState.activeOrigin = m_KeyboardDeviceInputValueHandle;

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
    UpdateKeyboardDeviceState();

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
    if (restrict_to_device == m_KeyboardDeviceInputValueHandle)
        return;

    //There have been problems with rumble getting stuck indefinitely when calling TriggerHapticVibrationAction() on a gamepad device (even though haptics aren't even bound)
    //Unsure if this an isolated issue, we're just avoid calling this function on gamepads altogether... this could be a one-liner otherwise
    if (restrict_to_device == vr::k_ulInvalidInputValueHandle)
    {
        //All devices, but we're going to exclude the gamepad one and trigger manually for the rest
        //Asking for haptic action origin doesn't seem to work, but GetLaserPointerDevicesInfo() gets every device with left click bound, which is good enough
        //This function is usually not called with this value either way
        std::vector<vr::InputOriginInfo_t> lp_devices_info = GetLaserPointerDevicesInfo();

        for (vr::InputOriginInfo_t device_info : lp_devices_info)
        {
            if (device_info.devicePath != m_GamepadDeviceInputValueHandle)
            {
                vr::VRInput()->TriggerHapticVibrationAction(m_HandleActionLaserPointerHaptic, 0.0f, 0.0f, 1.0f, 0.16f, device_info.devicePath);
            }
        }
    }
    else
    {
        //Don't trigger the vibration if this was called for the gamepad
        vr::InputOriginInfo_t device_info = GetOriginTrackedDeviceInfoEx(restrict_to_device);

        if (device_info.devicePath != m_GamepadDeviceInputValueHandle)
        {
            vr::VRInput()->TriggerHapticVibrationAction(m_HandleActionLaserPointerHaptic, 0.0f, 0.0f, 1.0f, 0.16f, restrict_to_device);
        }
    }
}

vr::InputOriginInfo_t VRInput::GetOriginTrackedDeviceInfoEx(vr::VRInputValueHandle_t origin) const
{
    vr::InputOriginInfo_t origin_info = {0};
    origin_info.trackedDeviceIndex = vr::k_unTrackedDeviceIndexInvalid;

    if (origin == m_KeyboardDeviceInputValueHandle)
    {
        origin_info.trackedDeviceIndex = vr::k_unTrackedDeviceIndex_Hmd;
        origin_info.devicePath = origin;
    }
    else
    {
        vr::VRInput()->GetOriginTrackedDeviceInfo(origin, &origin_info, sizeof(vr::InputOriginInfo_t));
    }

    return origin_info;
}

vr::InputDigitalActionData_t VRInput::GetEnableGlobalLaserPointerState() const
{
    vr::InputDigitalActionData_t data;
    vr::VRInput()->GetDigitalActionData(m_HandleActionEnableGlobalLaserPointer, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    if (ConfigManager::GetValue(configid_bool_input_laser_pointer_hmd_device))
    {
        data = CombineDigitalActionData(data, m_KeyboardDeviceToggleState);
    }

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

    if (ConfigManager::GetValue(configid_bool_input_laser_pointer_hmd_device))
    {
        vr::InputOriginInfo_t origin_info = {0};
        origin_info.trackedDeviceIndex = vr::k_unTrackedDeviceIndex_Hmd;    //Simulated Keyboard device is used for HMD interaction only so we use that
        origin_info.devicePath = m_KeyboardDeviceInputValueHandle;

        devices_info.push_back(origin_info);
    }

    return devices_info;
}

vr::InputDigitalActionData_t VRInput::GetLaserPointerLeftClickState(vr::VRInputValueHandle_t restrict_to_device) const
{
    vr::InputDigitalActionData_t data = {0};
    vr::VRInput()->GetDigitalActionData(m_HandleActionLaserPointerLeftClick, &data, sizeof(data), restrict_to_device);

    if (ConfigManager::GetValue(configid_bool_input_laser_pointer_hmd_device))
    {
        if ((restrict_to_device == vr::k_ulInvalidInputValueHandle) || (restrict_to_device == m_KeyboardDeviceInputValueHandle))
        {
            data = CombineDigitalActionData(data, m_KeyboardDeviceClickState[0]);
        }
    }

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

    if (ConfigManager::GetValue(configid_bool_input_laser_pointer_hmd_device))
    {
        if ((restrict_to_device == vr::k_ulInvalidInputValueHandle) || (restrict_to_device == m_KeyboardDeviceInputValueHandle))
        {
            for (int i = 0; i < data.size(); ++i)
            {
                data[i] = CombineDigitalActionData(data[i], m_KeyboardDeviceClickState[i]);
            }
        }
    }

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

vr::InputDigitalActionData_t VRInput::GetLaserPointerDragState(vr::VRInputValueHandle_t restrict_to_device) const
{
    vr::InputDigitalActionData_t data = {0};
    vr::VRInput()->GetDigitalActionData(m_HandleActionLaserPointerDrag, &data, sizeof(data), restrict_to_device);

    if (ConfigManager::GetValue(configid_bool_input_laser_pointer_hmd_device))
    {
        if ((restrict_to_device == vr::k_ulInvalidInputValueHandle) || (restrict_to_device == m_KeyboardDeviceInputValueHandle))
        {
            data = CombineDigitalActionData(data, m_KeyboardDeviceDragState);
        }
    }

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

vr::VRInputValueHandle_t VRInput::GetKeyboardDeviceInputValueHandle() const
{
    return m_KeyboardDeviceInputValueHandle;
}
