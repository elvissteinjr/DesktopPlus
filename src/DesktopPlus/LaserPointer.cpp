#include "LaserPointer.h"

#include "ConfigManager.h"
#include "OverlayManager.h"
#include "OutputManager.h"
#include "Util.h"

#define LASER_POINTER_OVERLAY_WIDTH 0.0025f
#define LASER_POINTER_DEFAULT_LENGTH 5.0f

LaserPointer::LaserPointer() : m_ActivationOrigin(dplp_activation_origin_none), 
                               m_HadPrimaryPointerDevice(false), 
                               m_DeviceMaxActiveID(0), 
                               m_IsForceTargetOverlayActive(false),
                               m_ForceTargetOverlayHandle(vr::k_ulOverlayHandleInvalid)
{
    //Not calling Update() here since the OutputManager typically needs to load the config and OpenVR first
}

LaserPointer::~LaserPointer()
{
    for (auto& lp_device : m_Devices)
    {
        if (lp_device.OvrlHandle != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->DestroyOverlay(lp_device.OvrlHandle);
        }
    }
}

void LaserPointer::CreateDeviceOverlay(vr::TrackedDeviceIndex_t device_index)
{
    if (device_index >= vr::k_unMaxTrackedDeviceCount)
        return;

    LaserPointerDevice& lp_device = m_Devices[device_index];

    std::string key = "elvissteinjr.DesktopPlusPointer" + std::to_string(device_index);
    vr::EVROverlayError ovrl_error = vr::VROverlay()->CreateOverlay(key.c_str(), "Desktop+ Laser Pointer", &lp_device.OvrlHandle);

    if (ovrl_error != vr::VROverlayError_None)
        return;

    //Set overlay as 2x2 half transparent blue-ish image
    uint32_t pixels[2 * 2];
    std::fill(std::begin(pixels), std::end(pixels), 0xFFBFA75F);      //Fairly close to SteamVR's pointer color (ABGR / little-endian order)

    vr::VROverlay()->SetOverlayRaw(lp_device.OvrlHandle, pixels, 2, 2, 4);

    vr::VROverlay()->SetOverlayWidthInMeters(lp_device.OvrlHandle, LASER_POINTER_OVERLAY_WIDTH);
    vr::VROverlay()->SetOverlaySortOrder(lp_device.OvrlHandle, 2);
}

void LaserPointer::UpdateDeviceOverlay(vr::TrackedDeviceIndex_t device_index)
{
    if (device_index >= vr::k_unMaxTrackedDeviceCount)
        return;

    LaserPointerDevice& lp_device = m_Devices[device_index];

    //Create overlay if it doesn't exist yet
    if (lp_device.OvrlHandle == vr::k_ulOverlayHandleInvalid)
    {
        CreateDeviceOverlay(device_index);
    }

    //Adjust visibility
    bool do_show_later = false;
    bool is_active = IsActive();
    if ( (!lp_device.IsVisible) && (is_active) )
    {
        lp_device.IsVisible = true;
        do_show_later = true;       //Postpone actually showing the overlay until after we set position and visuals
    }
    else if ( (lp_device.IsVisible) && (!is_active) )
    {
        vr::VROverlay()->HideOverlay(lp_device.OvrlHandle);
        lp_device.IsVisible = false;
    }

    //Skip rest if not visible
    if (!lp_device.IsVisible)
        return;

    //Position laser
    Matrix4 transform_tip, transform_offset;

    //Perform rotation locally and offset laser forward so it starts at the tip
    transform_offset.rotateX(-90.0f);
    transform_offset.translate_relative(0.0f, lp_device.LaserLength / 2.0f, 0.0f);

    //Use tip if there is one
    vr::ETrackedControllerRole controller_role = vr::VRSystem()->GetControllerRoleForTrackedDeviceIndex(device_index);
    if ( (controller_role == vr::TrackedControllerRole_LeftHand) || (controller_role == vr::TrackedControllerRole_RightHand) )
    {
        transform_tip = GetControllerTipMatrix( (controller_role == vr::TrackedControllerRole_RightHand) );
    }

    transform_tip = transform_tip * transform_offset;
    //A smart person could probably figure out how to also have the overlay spin towards the HMD so it doesn't appear flat

    vr::HmdMatrix34_t transform_openvr = transform_tip.toOpenVR34();
    vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(lp_device.OvrlHandle, device_index, &transform_openvr);

    //Adjust pointer alpha/brightness
    bool is_primary_device = (device_index == (vr::TrackedDeviceIndex_t)ConfigManager::Get().GetConfigInt(configid_int_state_dplus_laser_pointer_device));
    if (is_primary_device)
    {
        vr::VROverlay()->SetOverlayAlpha(lp_device.OvrlHandle, 1.0f);

        if (lp_device.OvrlHandleTargetLast != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->SetOverlayColor(lp_device.OvrlHandle, 1.0f, 1.0f, 1.0f);
        }
        else
        {
            vr::VROverlay()->SetOverlayColor(lp_device.OvrlHandle, 0.25f, 0.25f, 0.25f);
        }
    }
    else if (lp_device.IsActiveForMultiLaserInput)
    {
        vr::VROverlay()->SetOverlayAlpha(lp_device.OvrlHandle, 0.75f);
        vr::VROverlay()->SetOverlayColor(lp_device.OvrlHandle, 0.50f, 0.50f, 0.50f);
    }
    else
    {
        vr::VROverlay()->SetOverlayAlpha(lp_device.OvrlHandle, 0.125f);
        vr::VROverlay()->SetOverlayColor(lp_device.OvrlHandle, 0.25f, 0.25f, 0.25f);
    }

    if (do_show_later)
    {
        vr::VROverlay()->ShowOverlay(lp_device.OvrlHandle);
    }
}

void LaserPointer::UpdateIntersection(vr::TrackedDeviceIndex_t device_index)
{
    if (device_index >= vr::k_unMaxTrackedDeviceCount)
        return;

    LaserPointerDevice& lp_device = m_Devices[device_index];
    bool is_primary_device = (device_index == (vr::TrackedDeviceIndex_t)ConfigManager::Get().GetConfigInt(configid_int_state_dplus_laser_pointer_device));
    bool was_active_for_multilaser_input = lp_device.IsActiveForMultiLaserInput;
    bool skip_intersection_test = ( (vr::VROverlay()->IsDashboardVisible()) || (vr::VROverlay()->GetPrimaryDashboardDevice() != vr::k_unTrackedDeviceIndexInvalid) );
    bool skip_input = skip_intersection_test;

    //Set up intersection test
    bool hit_multilaser = false;
    vr::VROverlayIntersectionParams_t  params  = {0};
    vr::VROverlayIntersectionResults_t results = {0};

    if (!GetOverlayIntersectionParamsForDevice(params, device_index, vr::TrackingUniverseStanding))
    {
        skip_intersection_test = true; //Skip if pose isn't valid
    }

    //Find the nearest intersecting overlay
    vr::VROverlayHandle_t nearest_target_overlay = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayIntersectionResults_t nearest_results = {0};
    nearest_results.fDistance = FLT_MAX;

    //If requested via ForceTargetOverlay(), force a different target overlay
    if (m_IsForceTargetOverlayActive)
    {
        skip_intersection_test = true;
        nearest_target_overlay = m_ForceTargetOverlayHandle;
    }

    //If input is held down, do not switch overlays and act as if overlay space is extending past the surface when not hitting it
    if (!skip_intersection_test)
    {
        if (lp_device.InputDownCount != 0)
        {
            //Check if input is still enabled
            vr::VROverlayInputMethod input_method = vr::VROverlayInputMethod_None;
            vr::VROverlay()->GetOverlayInputMethod(lp_device.OvrlHandleTargetLast, &input_method);

            if (input_method == vr::VROverlayInputMethod_Mouse)
            {
                if (vr::VROverlay()->ComputeOverlayIntersection(lp_device.OvrlHandleTargetLast, &params, &results))
                {
                    nearest_target_overlay = lp_device.OvrlHandleTargetLast;
                }

                //These results are still valid even when not actually hitting
                nearest_results = results;
            }
        }
        else
        {
            //Desktop+ overlays
            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                const Overlay& overlay = OverlayManager::Get().GetOverlay(i);

                if (overlay.IsVisible())
                {
                    //Check if input is enabled right now (could differ from config setting)
                    vr::VROverlayInputMethod input_method = vr::VROverlayInputMethod_None;
                    vr::VROverlay()->GetOverlayInputMethod(overlay.GetHandle(), &input_method);

                    if ( (input_method == vr::VROverlayInputMethod_Mouse) && 
                         (vr::VROverlay()->ComputeOverlayIntersection(overlay.GetHandle(), &params, &results)) && (results.fDistance < nearest_results.fDistance) )
                    {
                        hit_multilaser         = false;
                        nearest_target_overlay = overlay.GetHandle();
                        nearest_results        = results;
                    }
                }
            }

            //Desktop+ UI overlays
            //
            //ComputeOverlayIntersection() does not take intersection masks into account. This has been reported under issue #1601 in the OpenVR repo.
            //As masks can change any frame, sending them over all the time seems tedious and inefficient. Not super hopeful, but I decided to wait if anything happens first.
            for (vr::VROverlayHandle_t overlay_handle : m_OverlayHandlesUI)
            {
                if (vr::VROverlay()->IsOverlayVisible(overlay_handle))
                {
                    //Check if input is enabled
                    vr::VROverlayInputMethod input_method = vr::VROverlayInputMethod_None;
                    vr::VROverlay()->GetOverlayInputMethod(overlay_handle, &input_method);

                    if ( (input_method == vr::VROverlayInputMethod_Mouse) && (vr::VROverlay()->ComputeOverlayIntersection(overlay_handle, &params, &results)) && (results.fDistance < nearest_results.fDistance) )
                    {
                        hit_multilaser         = false;
                        nearest_target_overlay = overlay_handle;
                        nearest_results        = results;
                    }
                }
            }

            //MultiLaser overlays (just keyboard right now)
            for (vr::VROverlayHandle_t overlay_handle : m_OverlayHandlesMultiLaser)
            {
                if (vr::VROverlay()->IsOverlayVisible(overlay_handle))
                {
                    //Check if input is enabled
                    vr::VROverlayInputMethod input_method = vr::VROverlayInputMethod_None;
                    vr::VROverlay()->GetOverlayInputMethod(overlay_handle, &input_method);

                    if ( (input_method == vr::VROverlayInputMethod_Mouse) && (vr::VROverlay()->ComputeOverlayIntersection(overlay_handle, &params, &results)) && (results.fDistance < nearest_results.fDistance) )
                    {
                        hit_multilaser         = true;
                        nearest_target_overlay = overlay_handle;
                        nearest_results        = results;
                    }
                }
            }

            //Non-MultiLaser hits are discared when not using the primary device
            if ( (!hit_multilaser) && (!is_primary_device) )
            {
                nearest_target_overlay = vr::k_ulOverlayHandleInvalid;
            }

            lp_device.IsActiveForMultiLaserInput = hit_multilaser;
        }
    }

    //If we hit a different overlay (or lack thereof)...
    if ( ( (lp_device.InputDownCount == 0) && (nearest_target_overlay != lp_device.OvrlHandleTargetLast) ) || (m_IsForceTargetOverlayActive) )
    {
        //...send focus leave event to last entered overlay
        if (lp_device.OvrlHandleTargetLast != vr::k_ulOverlayHandleInvalid)
        {
            vr::VREvent_t vr_event = {0};
            vr_event.trackedDeviceIndex = device_index;
            vr_event.eventType = vr::VREvent_FocusLeave;

            vr::VROverlayView()->PostOverlayEvent(lp_device.OvrlHandleTargetLast, &vr_event);

            //Clear overlay cursor if we were probably the last one to set it
            if ( (!was_active_for_multilaser_input) || (is_primary_device) )
            {
                vr::VROverlay()->ClearOverlayCursorPositionOverride(lp_device.OvrlHandleTargetLast);
            }
        }

        //...and enter to the new one, if any
        if (nearest_target_overlay != vr::k_ulOverlayHandleInvalid)
        {
            vr::VREvent_t vr_event = {0};
            vr_event.trackedDeviceIndex = device_index;
            vr_event.eventType = vr::VREvent_FocusEnter;

            vr::VROverlayView()->PostOverlayEvent(nearest_target_overlay, &vr_event);
        }
    }

    //Send mouse move event if we hit an overlay
    if (nearest_target_overlay != vr::k_ulOverlayHandleInvalid)
    {
        vr::HmdVector2_t mouse_scale;
        vr::VROverlay()->GetOverlayMouseScale(nearest_target_overlay, &mouse_scale);

        vr::VREvent_t vr_event = {0};
        vr_event.trackedDeviceIndex = device_index;
        vr_event.eventType = vr::VREvent_MouseMove;
        vr_event.data.mouse.x = nearest_results.vUVs.v[0] * mouse_scale.v[0];
        vr_event.data.mouse.y = nearest_results.vUVs.v[1] * mouse_scale.v[1];

        vr::VROverlayView()->PostOverlayEvent(nearest_target_overlay, &vr_event);

        //Set blob
        if (is_primary_device)
        {
            bool hide_intersection = false;
            vr::VROverlay()->GetOverlayFlag(nearest_target_overlay, vr::VROverlayFlags_HideLaserIntersection, &hide_intersection);

            if (!hide_intersection)
            {
                vr::VRTextureBounds_t tex_bounds;
                vr::VROverlay()->GetOverlayTextureBounds(nearest_target_overlay, &tex_bounds);
                int mapped_x      = (mouse_scale.v[0] * tex_bounds.uMin);
                int mapped_y      = (mouse_scale.v[1] * tex_bounds.vMin);
                int mapped_width  = (mouse_scale.v[0] * tex_bounds.uMax) - mapped_x;
                int mapped_height = (mouse_scale.v[1] * tex_bounds.vMax) - mapped_y;

                //Flip to normal 2D space
                float pointer_y = -round(vr_event.data.mouse.y) + mouse_scale.v[1];

                float new_u = (vr_event.data.mouse.x - mapped_x) / (mapped_width);
                float new_v = (pointer_y - mapped_y) / (mapped_height);

                //Flip it back
                new_v = -new_v + 1.0f;

                vr::HmdVector2_t pos = {new_u * mouse_scale.v[0], new_v * mouse_scale.v[1]};

                vr::VROverlay()->SetOverlayCursorPositionOverride(nearest_target_overlay, &pos);
            }
        }

        //Set pointer length to overlay distance
        lp_device.LaserLength = nearest_results.fDistance;
    }
    else if ( (!skip_input) && (lp_device.InputDownCount != 0) ) //Nothing hit, but input is down, extend
    {
        vr::HmdVector2_t mouse_scale;
        vr::VROverlay()->GetOverlayMouseScale(lp_device.OvrlHandleTargetLast, &mouse_scale);

        vr::VREvent_t vr_event = {0};
        vr_event.trackedDeviceIndex = device_index;
        vr_event.eventType = vr::VREvent_MouseMove;
        vr_event.data.mouse.x =   nearest_results.vUVs.v[0] * mouse_scale.v[0];
        vr_event.data.mouse.y = (-nearest_results.vUVs.v[1] + 1.0f) * mouse_scale.v[1]; //For some reason the V position is upside-down when the intersection did not happen

        vr::VROverlayView()->PostOverlayEvent(lp_device.OvrlHandleTargetLast, &vr_event);

        //Act as if the overlay was hit for the rest of this function
        nearest_target_overlay = lp_device.OvrlHandleTargetLast;
    }
    else
    {
        //Set pointer length to default
        lp_device.LaserLength = LASER_POINTER_DEFAULT_LENGTH;
    }

    //Input events
    if (nearest_target_overlay != vr::k_ulOverlayHandleInvalid)
    {
        VRInput& vr_input = OutputManager::Get()->GetVRInput();

        //Clicking
        auto click_state_array = vr_input.GetLaserPointerClickState(lp_device.InputValueHandle);
        uint32_t mouse_button_id = vr::VRMouseButton_Left;
        lp_device.InputDownCount = 0;

        for (const auto& input_data : click_state_array)
        {
            if (input_data.bActive)
            {
                if (input_data.bChanged)
                {
                    vr::VREvent_t vr_event = {0};
                    vr_event.trackedDeviceIndex = device_index;
                    vr_event.eventType = (input_data.bState) ? vr::VREvent_MouseButtonDown : vr::VREvent_MouseButtonUp;
                    vr_event.data.mouse.button = mouse_button_id;

                    vr::VROverlayView()->PostOverlayEvent(nearest_target_overlay, &vr_event);
                }

                if (input_data.bState)
                {
                    lp_device.InputDownCount++;
                }
            }

            mouse_button_id *= 2; //with click_state_array containing 5 elements, this goes over the OpenVR defined buttons to include our custom VRMouseButton_DP_Aux01/02
        }

        //Scrolling
        
        //Get scroll mode from overlay and set it for VRInput
        bool send_scroll_discrete = false, send_scroll_smooth = false;
        vr::VROverlay()->GetOverlayFlag(nearest_target_overlay, vr::VROverlayFlags_SendVRDiscreteScrollEvents, &send_scroll_discrete);
        vr::VROverlay()->GetOverlayFlag(nearest_target_overlay, vr::VROverlayFlags_SendVRSmoothScrollEvents,   &send_scroll_smooth);

        VRInputScrollMode scroll_mode = (send_scroll_smooth) ? vrinput_scroll_smooth : (send_scroll_discrete) ? vrinput_scroll_discrete : vrinput_scroll_none;
        vr_input.SetLaserPointerScrollMode(scroll_mode);

        switch (scroll_mode)
        {
            case vrinput_scroll_discrete:
            {
                vr::InputAnalogActionData_t input_data = vr_input.GetLaserPointerScrollDiscreteState();

                //If active and changed or not 0 on any axis
                if ( (input_data.bActive) && ( (input_data.x != 0.0f) || (input_data.y != 0.0f) || (input_data.deltaX != 0.0f) || (input_data.deltaY != 0.0f) ) )
                {
                    vr::VREvent_t vr_event = {0};
                    vr_event.trackedDeviceIndex = device_index;
                    vr_event.eventType = vr::VREvent_ScrollDiscrete;
                    vr_event.data.scroll.xdelta = input_data.x;
                    vr_event.data.scroll.ydelta = input_data.y;

                    vr::VROverlayView()->PostOverlayEvent(nearest_target_overlay, &vr_event);
                }

                break;
            }
            case vrinput_scroll_smooth:
            {
                vr::InputAnalogActionData_t input_data = vr_input.GetLaserPointerScrollSmoothState();

                //If active and changed or not 0 on any axis
                if ( (input_data.bActive) && ( (input_data.x != 0.0f) || (input_data.y != 0.0f) || (input_data.deltaX != 0.0f) || (input_data.deltaY != 0.0f) ) )
                {
                    vr::VREvent_t vr_event = {0};
                    vr_event.trackedDeviceIndex = device_index;
                    vr_event.eventType = vr::VREvent_ScrollSmooth;
                    vr_event.data.scroll.xdelta = input_data.x;
                    vr_event.data.scroll.ydelta = input_data.y;

                    vr::VROverlayView()->PostOverlayEvent(nearest_target_overlay, &vr_event);
                }

                break;
            }
            default: break;
        }
    }
    else
    {
        lp_device.InputDownCount = 0;
    }

    if (nearest_target_overlay != lp_device.OvrlHandleTargetLast)
    {
        lp_device.OvrlHandleTargetLast = nearest_target_overlay;

        if (is_primary_device)
        {
            ConfigManager::Get().SetConfigHandle(configid_handle_state_dplus_laser_pointer_target_overlay, lp_device.OvrlHandleTargetLast);
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_handle_state_dplus_laser_pointer_target_overlay), *(LPARAM*)&lp_device.OvrlHandleTargetLast);
        }
    }

    m_IsForceTargetOverlayActive = false;

    //Update overlay length
    vr::VRTextureBounds_t tex_bounds = {0};
    tex_bounds.uMax = 1.0f;
    tex_bounds.vMax = lp_device.LaserLength / LASER_POINTER_OVERLAY_WIDTH;

    vr::VROverlay()->SetOverlayTextureBounds(lp_device.OvrlHandle, &tex_bounds);
}

void LaserPointer::Update()
{
    //Refresh/Init handles in any case if they're empty
    if (m_OverlayHandlesUI.empty())
    {
        RefreshCachedOverlayHandles();
    }

    vr::TrackedDeviceIndex_t primary_pointer_device = (vr::TrackedDeviceIndex_t)ConfigManager::Get().GetConfigInt(configid_int_state_dplus_laser_pointer_device);

    if ( (primary_pointer_device == vr::k_unTrackedDeviceIndexInvalid) && (!m_HadPrimaryPointerDevice) )
        return;

    VRInput& vr_input = OutputManager::Get()->GetVRInput();

    //Primary device switching
    if (vr::VROverlay()->GetPrimaryDashboardDevice() == vr::k_unTrackedDeviceIndexInvalid)
    {
        vr::InputOriginInfo_t origin_info = {0};
        vr::InputDigitalActionData_t left_click_state = vr_input.GetLaserPointerLeftClickState();

        if ( (left_click_state.bChanged) && (left_click_state.bState) &&
             (vr::VRInput()->GetOriginTrackedDeviceInfo(left_click_state.activeOrigin, &origin_info, sizeof(vr::InputOriginInfo_t)) == vr::VRInputError_None))
        {
            if ( (origin_info.trackedDeviceIndex < vr::k_unMaxTrackedDeviceCount) && (origin_info.trackedDeviceIndex != primary_pointer_device) &&
                 (!m_Devices[origin_info.trackedDeviceIndex].IsActiveForMultiLaserInput))
            {
                //Set device path here since we have it ready anyways
                m_Devices[origin_info.trackedDeviceIndex].InputValueHandle = origin_info.devicePath;

                SetActiveDevice(origin_info.trackedDeviceIndex);
            }
        }
    }

    //Don't do anything if a dashboard pointer is active or we have no primary pointer ourselves
    bool should_pointer_be_active = ( (vr::VROverlay()->GetPrimaryDashboardDevice() == vr::k_unTrackedDeviceIndexInvalid) && (primary_pointer_device != vr::k_unTrackedDeviceIndexInvalid) );
    if ( (m_HadPrimaryPointerDevice) || (should_pointer_be_active) )
    {
        for (vr::TrackedDeviceIndex_t i = 0; i <= m_DeviceMaxActiveID; ++i)
        {
            if (m_Devices[i].OvrlHandle != vr::k_ulOverlayHandleInvalid)
            {
                UpdateIntersection(i);
            }
        }

        //Store if we were active the previous frame so we can sent overlay leave events in this iteration and clean up state before not doing this when not needed afterwards
        m_HadPrimaryPointerDevice = should_pointer_be_active;
    }

    for (vr::TrackedDeviceIndex_t i = 0; i <= m_DeviceMaxActiveID; ++i)
    {
        if (m_Devices[i].OvrlHandle != vr::k_ulOverlayHandleInvalid)
        {
            UpdateDeviceOverlay(i);
        }
    }

    vr_input.SetLaserPointerActive(should_pointer_be_active);
}

void LaserPointer::SetActiveDevice(vr::TrackedDeviceIndex_t device_index, LaserPointerActivationOrigin activation_origin)
{
    if (device_index >= vr::k_unMaxTrackedDeviceCount)
        return;

    vr::TrackedDeviceIndex_t previous_active_device = (vr::TrackedDeviceIndex_t)ConfigManager::Get().GetConfigInt(configid_int_state_dplus_laser_pointer_device);

    if (previous_active_device == device_index)
        return;

    //Clear last overlay cursor override
    if (previous_active_device < vr::k_unMaxTrackedDeviceCount)
    {
        LaserPointerDevice& lp_device_prev = m_Devices[previous_active_device];

        if (lp_device_prev.OvrlHandleTargetLast != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->ClearOverlayCursorPositionOverride(lp_device_prev.OvrlHandleTargetLast);
        }
    }

    LaserPointerDevice& lp_device = m_Devices[device_index];

    //Create overlay if it doesn't exist yet
    if (lp_device.OvrlHandle == vr::k_ulOverlayHandleInvalid)
    {
        CreateDeviceOverlay(device_index);
    }

    //Try finding the input value handle for this device if it's not set yet
    if (lp_device.InputValueHandle == vr::k_ulInvalidInputValueHandle)
    {
        std::vector<vr::InputOriginInfo_t> devices_info = OutputManager::Get()->GetVRInput().GetLaserPointerDevicesInfo();
        const auto it = std::find_if(devices_info.begin(), devices_info.end(), [&](const auto& input_origin_info){ return (input_origin_info.trackedDeviceIndex == device_index); });

        if (it != devices_info.end())
        {
            lp_device.InputValueHandle = it->devicePath;
        }
    }

    //Adjust max active ID if it's smaller
    if (m_DeviceMaxActiveID < device_index)
    {
        m_DeviceMaxActiveID = device_index;
    }

    //Vibrate if this activates the Desktop+ pointer (not just switching)
    if (previous_active_device == vr::k_unTrackedDeviceIndexInvalid)
    {
        TriggerLaserPointerHaptics(device_index);
    }

    m_ActivationOrigin = activation_origin;

    ConfigManager::Get().SetConfigInt(configid_int_state_dplus_laser_pointer_device, device_index);
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_dplus_laser_pointer_device), device_index);

    ConfigManager::Get().SetConfigHandle(configid_handle_state_dplus_laser_pointer_target_overlay, lp_device.OvrlHandleTargetLast);
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_handle_state_dplus_laser_pointer_target_overlay), *(LPARAM*)&lp_device.OvrlHandleTargetLast);
}

void LaserPointer::ClearActiveDevice()
{
    //Clear last overlay cursor override
    vr::TrackedDeviceIndex_t previous_active_device = (vr::TrackedDeviceIndex_t)ConfigManager::Get().GetConfigInt(configid_int_state_dplus_laser_pointer_device);

    if (previous_active_device < vr::k_unMaxTrackedDeviceCount)
    {
        LaserPointerDevice& lp_device_prev = m_Devices[previous_active_device];

        if (lp_device_prev.OvrlHandleTargetLast != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->ClearOverlayCursorPositionOverride(lp_device_prev.OvrlHandleTargetLast);
        }
    }

    m_ActivationOrigin = dplp_activation_origin_none;

    ConfigManager::Get().SetConfigInt(configid_int_state_dplus_laser_pointer_device, vr::k_unTrackedDeviceIndexInvalid);
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_dplus_laser_pointer_device), vr::k_unTrackedDeviceIndexInvalid);

    ConfigManager::Get().SetConfigHandle(configid_handle_state_dplus_laser_pointer_target_overlay, vr::k_ulOverlayHandleInvalid);
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_handle_state_dplus_laser_pointer_target_overlay), vr::k_ulOverlayHandleInvalid);
}

void LaserPointer::RemoveDevice(vr::TrackedDeviceIndex_t device_index)
{
    if (device_index >= vr::k_unMaxTrackedDeviceCount)
        return;

    LaserPointerDevice& lp_device = m_Devices[device_index];

    if (lp_device.OvrlHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->DestroyOverlay(lp_device.OvrlHandle);
    }

    //Send focus leave event to last entered overlay if there is any
    if (lp_device.OvrlHandleTargetLast != vr::k_ulOverlayHandleInvalid)
    {
        vr::VREvent_t vr_event = {0};
        vr_event.trackedDeviceIndex = device_index;
        vr_event.eventType = vr::VREvent_FocusLeave;

        vr::VROverlayView()->PostOverlayEvent(lp_device.OvrlHandleTargetLast, &vr_event);

        //Also remove pointer override in case there is any
        bool is_primary_device = (device_index == (vr::TrackedDeviceIndex_t)ConfigManager::Get().GetConfigInt(configid_int_state_dplus_laser_pointer_device));
        if ( (!lp_device.IsActiveForMultiLaserInput) || (is_primary_device) )
        {
            vr::VROverlay()->ClearOverlayCursorPositionOverride(lp_device.OvrlHandleTargetLast);
        }
    }

    lp_device = LaserPointerDevice();
}

void LaserPointer::RefreshCachedOverlayHandles()
{
    m_OverlayHandlesUI.clear();
    m_OverlayHandlesMultiLaser.clear();

    vr::VROverlayHandle_t overlay_handle;

    //Find UI overlays (except keyboard)
    const char* ui_overlay_keys[] = 
    {
        "elvissteinjr.DesktopPlusUI",
        "elvissteinjr.DesktopPlusUIFloating",
        "elvissteinjr.DesktopPlusUISettings",
        "elvissteinjr.DesktopPlusUIOverlayProperties",
        "elvissteinjr.DesktopPlusUIAux"
    };

    for (const char* key : ui_overlay_keys)
    {
        vr::VROverlay()->FindOverlay(key, &overlay_handle);

        if (overlay_handle != vr::k_ulOverlayHandleInvalid)
        {
            m_OverlayHandlesUI.push_back(overlay_handle);
        }
    }

    //Find MultiLaser overlays (which is just keyboard right now)
    vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusUIKeyboard", &overlay_handle);

    if (overlay_handle != vr::k_ulOverlayHandleInvalid)
    {
        m_OverlayHandlesMultiLaser.push_back(overlay_handle);
    }
}

void LaserPointer::TriggerLaserPointerHaptics(vr::TrackedDeviceIndex_t device_index)
{
    OutputManager::Get()->GetVRInput().TriggerLaserPointerHaptics((device_index < vr::k_unMaxTrackedDeviceCount) ? m_Devices[device_index].InputValueHandle : vr::k_ulInvalidInputValueHandle);
}

void LaserPointer::ForceTargetOverlay(vr::VROverlayHandle_t overlay_handle)
{
    vr::TrackedDeviceIndex_t primary_pointer_device = (vr::TrackedDeviceIndex_t)ConfigManager::Get().GetConfigInt(configid_int_state_dplus_laser_pointer_device);

    if (primary_pointer_device >= vr::k_unMaxTrackedDeviceCount)
        return;

    LaserPointerDevice& lp_device = m_Devices[primary_pointer_device];

    //Don't do anything if there is no target overlay to switch from or if it would be a no-op
    if ( (lp_device.OvrlHandleTargetLast == vr::k_ulOverlayHandleInvalid) || (lp_device.OvrlHandleTargetLast == overlay_handle) )
        return;

    //Run UpdateIntersection() with last target removed so overlay leave events are sent
    m_IsForceTargetOverlayActive = true;
    m_ForceTargetOverlayHandle = vr::k_ulOverlayHandleInvalid;
    UpdateIntersection(primary_pointer_device);

    //Set force variables again so they apply next time UpdateIntersection() us called
    m_IsForceTargetOverlayActive = true;
    m_ForceTargetOverlayHandle = overlay_handle;
}

LaserPointerActivationOrigin LaserPointer::GetActivationOrigin() const
{
    return m_ActivationOrigin;
}

bool LaserPointer::IsActive() const
{
    vr::TrackedDeviceIndex_t primary_pointer_device = (vr::TrackedDeviceIndex_t)ConfigManager::Get().GetConfigInt(configid_int_state_dplus_laser_pointer_device);
    return ( (vr::VROverlay()->GetPrimaryDashboardDevice() == vr::k_unTrackedDeviceIndexInvalid) && (primary_pointer_device != vr::k_unTrackedDeviceIndexInvalid) );
}

vr::TrackedDeviceIndex_t LaserPointer::IsAnyOverlayHovered(float max_distance) const
{
    //If active, just check if the primary pointer has a last target overlay
    if (IsActive())
    {
        vr::TrackedDeviceIndex_t primary_pointer_device = (vr::TrackedDeviceIndex_t)ConfigManager::Get().GetConfigInt(configid_int_state_dplus_laser_pointer_device);

        if (primary_pointer_device >= vr::k_unMaxTrackedDeviceCount)
            return vr::k_unTrackedDeviceIndexInvalid;

        const LaserPointerDevice& lp_device = m_Devices[primary_pointer_device];

        if ( (lp_device.OvrlHandleTargetLast != vr::k_ulOverlayHandleInvalid) && (lp_device.LaserLength <= max_distance) )
        {
            return primary_pointer_device;
        }
    }
    else //...otherwise check all possible overlays
    {
        //Check left and right hand controller
        for (int i = vr::TrackedControllerRole_LeftHand; i <= vr::TrackedControllerRole_RightHand; ++i)
        {
            vr::ETrackedControllerRole controller_role = (vr::ETrackedControllerRole)i;
            vr::TrackedDeviceIndex_t device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(controller_role);
            OverlayOrigin origin_avoid = (OverlayOrigin)(ovrl_origin_left_hand + i - vr::TrackedControllerRole_LeftHand);       //Role_*Hand matches origin_*_hand

            //Set up intersection test
            vr::VROverlayIntersectionParams_t  params  = {0};
            vr::VROverlayIntersectionResults_t results = {0};

            if (!GetOverlayIntersectionParamsForDevice(params, device_index, vr::TrackingUniverseStanding))
            {
                continue; //Skip if pose isn't valid
            }

            //Desktop+ overlays
            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                const Overlay& overlay        = OverlayManager::Get().GetOverlay(i);
                const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                if ( (data.ConfigInt[configid_int_overlay_origin] != origin_avoid) && (overlay.IsVisible()) )
                {
                    //Check if input is enabled right now (could differ from config setting)
                    vr::VROverlayInputMethod input_method = vr::VROverlayInputMethod_None;
                    vr::VROverlay()->GetOverlayInputMethod(overlay.GetHandle(), &input_method);

                    if ( (input_method == vr::VROverlayInputMethod_Mouse) && 
                         (vr::VROverlay()->ComputeOverlayIntersection(overlay.GetHandle(), &params, &results)) && (results.fDistance <= max_distance) )
                    {
                        return device_index;
                    }
                }
            }

            //Desktop+ UI overlays
            //
            //See UpdateIntersection() for current issues
            for (vr::VROverlayHandle_t overlay_handle : m_OverlayHandlesUI)
            {
                if (vr::VROverlay()->IsOverlayVisible(overlay_handle))
                {
                    if ( (vr::VROverlay()->ComputeOverlayIntersection(overlay_handle, &params, &results)) && (results.fDistance <= max_distance) )
                    {
                        return device_index;
                    }
                }
            }

            //MultiLaser overlays (just keyboard right now)
            for (vr::VROverlayHandle_t overlay_handle : m_OverlayHandlesMultiLaser)
            {
                if (vr::VROverlay()->IsOverlayVisible(overlay_handle))
                {
                    if ( (vr::VROverlay()->ComputeOverlayIntersection(overlay_handle, &params, &results)) && (results.fDistance <= max_distance) )
                    {
                        return device_index;
                    }
                }
            }
        }
    }

    return vr::k_unTrackedDeviceIndexInvalid;
}