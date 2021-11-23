#include "OverlayDragger.h"

#ifndef DPLUS_UI
    #include "OutputManager.h"
#endif

#include "OverlayManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"

OverlayDragger::OverlayDragger() : 
    m_DragModeDeviceID(-1),
    m_DragModeOverlayID(k_ulOverlayID_None),
    m_DragModeOverlayHandle(vr::k_ulOverlayHandleInvalid),
    m_DragModeOverlayOrigin(ovrl_origin_room),
    m_DragGestureActive(false),
    m_DragGestureScaleDistanceStart(0.0f),
    m_DragGestureScaleWidthStart(0.0f),
    m_DragGestureScaleDistanceLast(0.0f),
    m_AbsoluteModeActive(false),
    m_AbsoluteModeOffsetForward(0.0f),
    m_DashboardHMD_Y(-100.0f)
{

}

void OverlayDragger::DragStartBase(bool is_gesture_drag)
{
    if ( (IsDragActive()) || (IsDragGestureActive()) )
        return;

    //This is also used by DragGestureStart() (with is_gesture_drag = true), but only to convert between overlay origins.
    //Doesn't need calls to the other DragUpdate() or DragFinish() functions in that case
    vr::TrackedDeviceIndex_t device_index = ConfigManager::Get().GetPrimaryLaserPointerDevice();

    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

    //We have no dashboard device, but something still started a drag, eh? This happens when the dashboard is closed but the overlays are still interactive
    //There doesn't seem to be a way to get around this, so we guess by checking which of the two hand controllers are currently pointing at the overlay
    //Works for most cases at least
    if (device_index == vr::k_unTrackedDeviceIndexInvalid)
    {
        device_index = FindPointerDeviceForOverlay(m_DragModeOverlayHandle);

        //Still nothing, try the config hint
        if (device_index == vr::k_unTrackedDeviceIndexInvalid)
        {
            device_index = (vr::TrackedDeviceIndex_t)ConfigManager::GetValue(configid_int_state_laser_pointer_device_hint);
        }
    }

    if ( (device_index < vr::k_unMaxTrackedDeviceCount) && (poses[device_index].bPoseIsValid) )
    {
        if (!is_gesture_drag)
        {
            m_DragModeDeviceID = device_index;
        }

        m_DragModeMatrixSourceStart = poses[device_index].mDeviceToAbsoluteTracking;

        switch (m_DragModeOverlayOrigin)
        {
            case ovrl_origin_hmd:
            {
                if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
                {
                    vr::VROverlay()->SetOverlayTransformAbsolute(m_DragModeOverlayHandle, vr::TrackingUniverseStanding, &poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
                }
                break;
            }
            case ovrl_origin_right_hand:
            {
                vr::TrackedDeviceIndex_t index_right_hand = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

                if ( (index_right_hand != vr::k_unTrackedDeviceIndexInvalid) && (poses[index_right_hand].bPoseIsValid) )
                {
                    vr::VROverlay()->SetOverlayTransformAbsolute(m_DragModeOverlayHandle, vr::TrackingUniverseStanding, &poses[index_right_hand].mDeviceToAbsoluteTracking);
                }
                break;
            }
            case ovrl_origin_left_hand:
            {
                vr::TrackedDeviceIndex_t index_left_hand = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);

                if ( (index_left_hand != vr::k_unTrackedDeviceIndexInvalid) && (poses[index_left_hand].bPoseIsValid) )
                {
                    vr::VROverlay()->SetOverlayTransformAbsolute(m_DragModeOverlayHandle, vr::TrackingUniverseStanding, &poses[index_left_hand].mDeviceToAbsoluteTracking);
                }
                break;
            }
            case ovrl_origin_aux:
            {
                vr::TrackedDeviceIndex_t index_tracker = GetFirstVRTracker();

                if ( (index_tracker != vr::k_unTrackedDeviceIndexInvalid) && (poses[index_tracker].bPoseIsValid) )
                {
                    vr::VROverlay()->SetOverlayTransformAbsolute(m_DragModeOverlayHandle, vr::TrackingUniverseStanding, &poses[index_tracker].mDeviceToAbsoluteTracking);
                }
                break;
            }
        }

        vr::HmdMatrix34_t transform_target;
        vr::TrackingUniverseOrigin origin;
        vr::VROverlay()->GetOverlayTransformAbsolute(m_DragModeOverlayHandle, &origin, &transform_target);
        m_DragModeMatrixTargetStart   = transform_target;
        m_DragModeMatrixTargetCurrent = m_DragModeMatrixTargetStart;

        if ( (!is_gesture_drag) && (m_DragModeOverlayID != k_ulOverlayID_None) )
        {
            vr::VROverlay()->SetOverlayFlag(m_DragModeOverlayHandle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, false);
            vr::VROverlay()->SetOverlayFlag(m_DragModeOverlayHandle, vr::VROverlayFlags_SendVRSmoothScrollEvents,   true);
        }
    }
    else
    {
        //No drag started, reset state
        m_DragModeOverlayID     = k_ulOverlayID_None;
        m_DragModeOverlayHandle = vr::k_ulOverlayHandleInvalid;
    }
}

void OverlayDragger::DragStart(unsigned int overlay_id)
{
    if ( (IsDragActive()) || (IsDragGestureActive()) )
        return;

    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

    m_DragModeDeviceID      = -1;
    m_DragModeOverlayID     = overlay_id;
    m_DragModeOverlayOrigin = (OverlayOrigin)data.ConfigInt[configid_int_overlay_origin];

    #ifndef DPLUS_UI
        m_DragModeOverlayHandle = OverlayManager::Get().GetOverlay(overlay_id).GetHandle();
    #else
        m_DragModeOverlayHandle = OverlayManager::Get().FindOverlayHandle(overlay_id);
    #endif

    DragStartBase(false);
}

void OverlayDragger::DragStart(vr::VROverlayHandle_t overlay_handle, OverlayOrigin overlay_origin)
{
    if ( (IsDragActive()) || (IsDragGestureActive()) )
        return;

    m_DragModeDeviceID      = -1;
    m_DragModeOverlayID     = k_ulOverlayID_None;
    m_DragModeOverlayHandle = overlay_handle;
    m_DragModeOverlayOrigin = overlay_origin;

    DragStartBase(false);
}

void OverlayDragger::DragUpdate()
{
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

    if (poses[m_DragModeDeviceID].bPoseIsValid)
    {
        if (m_AbsoluteModeActive)
        {
            //Get matrices
            Matrix4 mat_device = poses[m_DragModeDeviceID].mDeviceToAbsoluteTracking;

            //Apply tip offset if controller
            vr::TrackedDeviceIndex_t index_right = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
            vr::TrackedDeviceIndex_t index_left  = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
            if ( (m_DragModeDeviceID == index_left) || (m_DragModeDeviceID == index_right) ) 
            {
                mat_device = mat_device * GetControllerTipMatrix( (m_DragModeDeviceID == index_right) );
            }

            //Apply forward offset
            mat_device.translate_relative(0.0f, 0.0f, -m_AbsoluteModeOffsetForward);

            m_DragModeMatrixTargetCurrent = mat_device;

            //Set transform
            vr::HmdMatrix34_t vrmat = m_DragModeMatrixTargetCurrent.toOpenVR34();
            vr::VROverlay()->SetOverlayTransformAbsolute(m_DragModeOverlayHandle, vr::TrackingUniverseStanding, &vrmat);
        }
        else
        {
            Matrix4 matrix_source_current = poses[m_DragModeDeviceID].mDeviceToAbsoluteTracking;
            Matrix4 matrix_target_new = m_DragModeMatrixTargetStart;

            Matrix4 matrix_source_start_inverse = m_DragModeMatrixSourceStart;
            matrix_source_start_inverse.invert();

            matrix_source_current = matrix_source_current * matrix_source_start_inverse;

            m_DragModeMatrixTargetCurrent = matrix_source_current * matrix_target_new;

            //Do axis locking if managed overlay and setting enabled
            if ( (m_DragModeOverlayID != k_ulOverlayID_None) && (ConfigManager::GetValue(configid_bool_input_drag_force_upright)) )
            {
                TransformForceUpright(m_DragModeMatrixTargetCurrent);
            }

            vr::HmdMatrix34_t vrmat = m_DragModeMatrixTargetCurrent.toOpenVR34();
            vr::VROverlay()->SetOverlayTransformAbsolute(m_DragModeOverlayHandle, vr::TrackingUniverseStanding, &vrmat);
        }
    }
}

void OverlayDragger::DragAddDistance(float distance)
{
    float overlay_width = 1.0f;

    if (m_DragModeOverlayID != k_ulOverlayID_None)
    {
        overlay_width = OverlayManager::Get().GetConfigData(m_DragModeOverlayID).ConfigFloat[configid_float_overlay_width];
    }
    else
    {
        vr::VROverlay()->GetOverlayWidthInMeters(m_DragModeOverlayHandle, &overlay_width);
    }

	//Scale distance to overlay width
    distance = clamp(distance * (overlay_width / 2.0f), -0.5f, 0.5f);

    if (m_AbsoluteModeActive)
    {
        m_AbsoluteModeOffsetForward += distance * 0.5f;
        m_AbsoluteModeOffsetForward = std::max(0.01f, m_AbsoluteModeOffsetForward);
    }
    else
    {
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

        if (poses[m_DragModeDeviceID].bPoseIsValid)
        {
            Matrix4 mat_drag_device = m_DragModeMatrixSourceStart;

            //Apply tip offset if possible (usually the case)
            vr::TrackedDeviceIndex_t index_right = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
            vr::TrackedDeviceIndex_t index_left  = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
            if ( (m_DragModeDeviceID == index_left) || (m_DragModeDeviceID == index_right) ) 
            {
                mat_drag_device = mat_drag_device * GetControllerTipMatrix( (m_DragModeDeviceID == index_right) );
            }

            //Take the drag device start orientation and the overlay's start translation and offset forward from there
            mat_drag_device.setTranslation(m_DragModeMatrixTargetStart.getTranslation());
            mat_drag_device.translate_relative(0.0f, 0.0f, distance * -0.5f);
            m_DragModeMatrixTargetStart.setTranslation(mat_drag_device.getTranslation());
        }
    }
}

float OverlayDragger::DragAddWidth(float width)
{
    if (!IsDragActive())
        return 0.0f;

    width = clamp(width, -0.25f, 0.25f) + 1.0f; //Expected range is smaller than for DragAddDistance()

    float overlay_width = 1.0f;

    if (m_DragModeOverlayID != k_ulOverlayID_None)
    {
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_DragModeOverlayID);

        overlay_width = data.ConfigFloat[configid_float_overlay_width] * width;

        if (overlay_width < 0.05f)
            overlay_width = 0.05f;

        vr::VROverlay()->SetOverlayWidthInMeters(m_DragModeOverlayHandle, overlay_width);
        data.ConfigFloat[configid_float_overlay_width] = overlay_width;

        #ifndef DPLUS_UI
            //Send adjusted width to the UI app
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)m_DragModeOverlayID);
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_float_overlay_width), pun_cast<LPARAM, float>(overlay_width));
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
        #endif
    }
    else
    {
        vr::VROverlay()->GetOverlayWidthInMeters(m_DragModeOverlayHandle, &overlay_width);

        overlay_width *= width;

        if (overlay_width < 0.50f) //Usually used with ImGui window UI overlays, so use higher minimum width
            overlay_width = 0.50f;

        vr::VROverlay()->SetOverlayWidthInMeters(m_DragModeOverlayHandle, overlay_width);
    }

    return overlay_width;
}

Matrix4 OverlayDragger::GetBaseOffsetMatrix()
{
    return GetBaseOffsetMatrix((OverlayOrigin)ConfigManager::GetValue(configid_int_overlay_origin));
}

Matrix4 OverlayDragger::GetBaseOffsetMatrix(OverlayOrigin overlay_origin)
{
    Matrix4 matrix; //Identity

    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

    switch (overlay_origin)
    {
        case ovrl_origin_room:
        {
            break;
        }
        case ovrl_origin_hmd_floor:
        {
            vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
            vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

            if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
            {
                Matrix4 mat_pose = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
                Vector3 pos_offset = mat_pose.getTranslation();

                pos_offset.y = 0.0f;
                matrix.setTranslation(pos_offset);
            }
            break;
        }
        case ovrl_origin_seated_universe:
        {
            matrix = vr::VRSystem()->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
            break;
        }
        case ovrl_origin_dashboard:
        {
            //This code is prone to break when Valve changes the entire dashboard once again
            vr::VROverlayHandle_t system_dashboard;
            vr::VROverlay()->FindOverlay("system.systemui", &system_dashboard);

            if (system_dashboard != vr::k_ulOverlayHandleInvalid)
            {
                vr::HmdMatrix34_t matrix_overlay_system;

                vr::HmdVector2_t overlay_system_size;
                vr::VROverlay()->GetOverlayMouseScale(system_dashboard, &overlay_system_size); //Coordinate size should be mouse scale

                vr::VROverlay()->GetTransformForOverlayCoordinates(system_dashboard, universe_origin, { overlay_system_size.v[0]/2.0f, 0.0f }, &matrix_overlay_system);
                matrix = matrix_overlay_system;

                if (m_DashboardHMD_Y == -100.0f)    //If Desktop+ was started with the dashboard open, the value will still be default, so set it now
                {
                    UpdateDashboardHMD_Y();
                }

                Vector3 pos_offset = matrix.getTranslation();
                pos_offset.y = m_DashboardHMD_Y;
                pos_offset.y -= 0.44f;              //Move 0.44m down for better dashboard overlay default pos (needs to fit Floating UI though)
                matrix.setTranslation(pos_offset);
            }

            break;
        }
        case ovrl_origin_hmd:
        case ovrl_origin_right_hand:
        case ovrl_origin_left_hand:
        case ovrl_origin_aux:
        {
            //This is used for the dragging only. In other cases the origin is identity, as it's attached to the controller via OpenVR
            vr::TrackedDeviceIndex_t device_index;

            switch (overlay_origin)
            {
                case ovrl_origin_hmd:        device_index = vr::k_unTrackedDeviceIndex_Hmd;                                                              break;
                case ovrl_origin_right_hand: device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand); break;
                case ovrl_origin_left_hand:  device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);  break;
                case ovrl_origin_aux:        device_index = GetFirstVRTracker();                                                                         break;
                default:                     device_index = vr::k_unTrackedDeviceIndexInvalid;
            }
             
            if (device_index != vr::k_unTrackedDeviceIndexInvalid)
            {
                vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
                vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

                if (poses[device_index].bPoseIsValid)
                {
                    matrix = poses[device_index].mDeviceToAbsoluteTracking;
                }
            }
            break;
        }
        case ovrl_origin_dplus_tab:
        {
            vr::VROverlayHandle_t ovrl_handle_dplus;
            vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &ovrl_handle_dplus);

            if (ovrl_handle_dplus != vr::k_ulOverlayHandleInvalid)
            {
                vr::HmdMatrix34_t matrix_dplus_tab;
                vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;

                vr::VROverlay()->GetTransformForOverlayCoordinates(ovrl_handle_dplus, origin, {0.5f, 0.0f}, &matrix_dplus_tab);
                
                matrix = matrix_dplus_tab;
            }
            break;
        }
    }

    return matrix;
}

Matrix4 OverlayDragger::DragFinish()
{
    DragUpdate();

    //Allow managed overlay origin to change after drag (used for auto-docking)
    if (m_DragModeOverlayID != k_ulOverlayID_None)
    {
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_DragModeOverlayID);
        m_DragModeOverlayOrigin = (OverlayOrigin)data.ConfigInt[configid_int_overlay_origin];
    }

    vr::HmdMatrix34_t transform_target;
    vr::TrackingUniverseOrigin origin;

    vr::VROverlay()->GetOverlayTransformAbsolute(m_DragModeOverlayHandle, &origin, &transform_target);
    Matrix4 matrix_target_finish = transform_target;

    Matrix4 matrix_target_base = GetBaseOffsetMatrix(m_DragModeOverlayOrigin);
    matrix_target_base.invert();

    Matrix4 matrix_target_relative = matrix_target_base * matrix_target_finish;

    //Apply to managed overlay if drag was with ID
    if (m_DragModeOverlayID != k_ulOverlayID_None)
    {
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_DragModeOverlayID);

        vr::VROverlay()->SetOverlayFlag(m_DragModeOverlayHandle, vr::VROverlayFlags_SendVRSmoothScrollEvents, false);

        if (data.ConfigBool[configid_bool_overlay_input_enabled])
        {
            vr::VROverlay()->SetOverlayFlag(m_DragModeOverlayHandle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);
        }

        //Counteract additonal offset that might've been present on the transform
        matrix_target_relative.translate_relative(-data.ConfigFloat[configid_float_overlay_offset_right],
                                                  -data.ConfigFloat[configid_float_overlay_offset_up],
                                                  -data.ConfigFloat[configid_float_overlay_offset_forward]);

        //Counteract origin offset for dashboard origin overlays
        #ifndef DPLUS_UI
            if (m_DragModeOverlayOrigin == ovrl_origin_dashboard)
            {
                if (OutputManager* outmgr = OutputManager::Get())
                {
                    float height = outmgr->GetOverlayHeight(m_DragModeOverlayID);
                    matrix_target_relative.translate_relative(0.0f, height / -2.0f, 0.0f);
                }
            }
        #endif

        data.ConfigTransform = matrix_target_relative;
    }

    //Reset state
    m_DragModeDeviceID      = -1;
    m_DragModeOverlayID     = k_ulOverlayID_None;
    m_DragModeOverlayHandle = vr::k_ulOverlayHandleInvalid;
    m_AbsoluteModeActive    = false;

    return matrix_target_relative;
}

void OverlayDragger::DragCancel()
{
    //Reset state
    m_DragModeDeviceID      = -1;
    m_DragModeOverlayID     = k_ulOverlayID_None;
    m_DragModeOverlayHandle = vr::k_ulOverlayHandleInvalid;
    m_AbsoluteModeActive    = false;
}

void OverlayDragger::DragGestureStartBase()
{
    if ( (IsDragActive()) || (IsDragGestureActive()) )
        return;

    DragStartBase(true); //Call the other drag start function to convert the overlay transform to absolute. This doesn't actually start the normal drag

    DragGestureUpdate();

    m_DragGestureScaleDistanceStart = m_DragGestureScaleDistanceLast;

    if (m_DragModeOverlayID != k_ulOverlayID_None)
    {
        m_DragGestureScaleWidthStart = OverlayManager::Get().GetConfigData(m_DragModeOverlayID).ConfigFloat[configid_float_overlay_width];
    }
    else
    {
        vr::VROverlay()->GetOverlayWidthInMeters(m_DragModeOverlayHandle, &m_DragGestureScaleWidthStart);
    }

    m_DragGestureActive = true;
}

void OverlayDragger::TransformForceUpright(Matrix4& transform) const
{
    //Based off of ComputeHMDFacingTransform()... might not be the best way to do it, but it works.
    static const Vector3 up = {0.0f, 1.0f, 0.0f};

    Matrix4 matrix_temp  = transform;
    Vector3 ovrl_start   = matrix_temp.translate_relative(0.0f, 0.0f, -0.001f).getTranslation();
    Vector3 forward_temp = (ovrl_start - transform.getTranslation()).normalize();
    Vector3 right        = forward_temp.cross(up).normalize();
    Vector3 forward      = up.cross(right).normalize();

    Matrix4 mat_upright(right, up, forward * -1.0f);
    mat_upright.setTranslation(ovrl_start);

    transform = mat_upright;
}

void OverlayDragger::DragGestureStart(unsigned int overlay_id)
{
    if ( (IsDragActive()) || (IsDragGestureActive()) )
        return;

    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

    m_DragModeDeviceID      = -1;
    m_DragModeOverlayID     = overlay_id;
    m_DragModeOverlayOrigin = (OverlayOrigin)data.ConfigInt[configid_int_overlay_origin];

    #ifndef DPLUS_UI
        m_DragModeOverlayHandle = OverlayManager::Get().GetOverlay(overlay_id).GetHandle();
    #else
        m_DragModeOverlayHandle = OverlayManager::Get().FindOverlayHandle(overlay_id);
    #endif

    DragGestureStartBase();
}

void OverlayDragger::DragGestureStart(vr::VROverlayHandle_t overlay_handle, OverlayOrigin overlay_origin)
{
    if ( (IsDragActive()) || (IsDragGestureActive()) )
        return;

    m_DragModeDeviceID      = -1;
    m_DragModeOverlayID     = k_ulOverlayID_None;
    m_DragModeOverlayHandle = overlay_handle;
    m_DragModeOverlayOrigin = overlay_origin;

    DragGestureStartBase();
}

void OverlayDragger::DragGestureUpdate()
{
    vr::TrackedDeviceIndex_t index_right = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
    vr::TrackedDeviceIndex_t index_left  = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);

    if ( (index_right != vr::k_unTrackedDeviceIndexInvalid) && (index_left != vr::k_unTrackedDeviceIndexInvalid) )
    {
        vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

        if ( (poses[index_right].bPoseIsValid) && (poses[index_left].bPoseIsValid) )
        {
            Matrix4 mat_right = poses[index_right].mDeviceToAbsoluteTracking;
            Matrix4 mat_left  = poses[index_left].mDeviceToAbsoluteTracking;

            //Gesture Scale
            m_DragGestureScaleDistanceLast = mat_right.getTranslation().distance(mat_left.getTranslation());

            if (m_DragGestureActive)
            {
                //Scale is just the start scale multiplied by the factor of changed controller distance
                float width = m_DragGestureScaleWidthStart * (m_DragGestureScaleDistanceLast / m_DragGestureScaleDistanceStart);
                vr::VROverlay()->SetOverlayWidthInMeters(m_DragModeOverlayHandle, width);

                if (m_DragModeOverlayID != k_ulOverlayID_None)
                {
                    OverlayManager::Get().GetConfigData(m_DragModeOverlayID).ConfigFloat[configid_float_overlay_width] = width;

                    #ifndef DPLUS_UI
                    //Send adjusted width to the UI app
                    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)m_DragModeOverlayID);
                    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_float_overlay_width), pun_cast<LPARAM, float>(width));
                    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
                    #endif
                }
            }

            //Gesture Rotate
            Matrix4 matrix_rotate_current = mat_left;
            //Use up-vector multiplied by rotation matrix to avoid locking at near-up transforms
            Vector3 up = m_DragGestureRotateMatLast * Vector3(0.0f, 1.0f, 0.0f);
            up.normalize();
            //Rotation motion is taken from the differences between left controller lookat(right controller) results
            TransformLookAt(matrix_rotate_current, mat_right.getTranslation(), up);

            if (m_DragGestureActive)
            {
                //Get difference of last drag frame
                Matrix4 matrix_rotate_last_inverse = m_DragGestureRotateMatLast;
                matrix_rotate_last_inverse.setTranslation({0.0f, 0.0f, 0.0f});
                matrix_rotate_last_inverse.invert();

                Matrix4 matrix_rotate_current_at_origin = matrix_rotate_current;
                matrix_rotate_current_at_origin.setTranslation({0.0f, 0.0f, 0.0f});

                Matrix4 matrix_rotate_diff = matrix_rotate_current_at_origin * matrix_rotate_last_inverse;

                //Do axis locking if managed overlay and setting enabled
                if ( (m_DragModeOverlayID != k_ulOverlayID_None) && (ConfigManager::GetValue(configid_bool_input_drag_force_upright)) )
                {
                    TransformForceUpright(matrix_rotate_diff);
                }

                //Apply difference
                Matrix4& mat_overlay = m_DragModeMatrixTargetStart;
                Vector3 pos = mat_overlay.getTranslation();
                mat_overlay.setTranslation({0.0f, 0.0f, 0.0f});
                mat_overlay = matrix_rotate_diff * mat_overlay;
                mat_overlay.setTranslation(pos);

                vr::HmdMatrix34_t vrmat = mat_overlay.toOpenVR34();
                vr::VROverlay()->SetOverlayTransformAbsolute(m_DragModeOverlayHandle, vr::TrackingUniverseStanding, &vrmat);
            }

            m_DragGestureRotateMatLast = matrix_rotate_current;
        }
    }
}

Matrix4 OverlayDragger::DragGestureFinish()
{
    Matrix4 matrix_target_base = GetBaseOffsetMatrix(m_DragModeOverlayOrigin);
    matrix_target_base.invert();

    Matrix4 matrix_target_relative = matrix_target_base * m_DragModeMatrixTargetStart;

    //Apply to managed overlay if drag was with ID
    if (m_DragModeOverlayID != k_ulOverlayID_None)
    {
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_DragModeOverlayID);

        //Counteract additonal offset that might've been present on the transform
        matrix_target_relative.translate_relative(-data.ConfigFloat[configid_float_overlay_offset_right],
                                                  -data.ConfigFloat[configid_float_overlay_offset_up],
                                                  -data.ConfigFloat[configid_float_overlay_offset_forward]);

        //Counteract origin offset for dashboard origin overlays
        #ifndef DPLUS_UI
        if (m_DragModeOverlayOrigin == ovrl_origin_dashboard)
        {
            if (OutputManager* outmgr = OutputManager::Get())
            {
                float height = outmgr->GetOverlayHeight(m_DragModeOverlayID);
                matrix_target_relative.translate_relative(0.0f, height / -2.0f, 0.0f);
            }
        }
        #endif

        data.ConfigTransform = matrix_target_relative;
    }

    //Reset state
    m_DragGestureActive     = false;
    m_DragModeOverlayID     = k_ulOverlayID_None;
    m_DragModeOverlayHandle = vr::k_ulOverlayHandleInvalid;

    return matrix_target_relative;
}

void OverlayDragger::AbsoluteModeSet(bool is_active, float offset_forward)
{
    m_AbsoluteModeActive = is_active;
    m_AbsoluteModeOffsetForward = offset_forward;
}

void OverlayDragger::UpdateDashboardHMD_Y()
{
    vr::VROverlayHandle_t ovrl_handle_dplus;
    vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &ovrl_handle_dplus);

    //Use dashboard dummy if available and visible. It provides a way more reliable reference point
    if ( (ovrl_handle_dplus != vr::k_ulOverlayHandleInvalid) && (vr::VROverlay()->IsOverlayVisible(ovrl_handle_dplus)) )
    {
        vr::HmdMatrix34_t matrix_dplus_tab;
        vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;
        vr::VROverlay()->GetTransformForOverlayCoordinates(ovrl_handle_dplus, origin, {0.5f, 0.0f}, &matrix_dplus_tab);

        m_DashboardHMD_Y = matrix_dplus_tab.m[1][3] + 0.575283f; //Rough height difference between dashboard dummy reference point and SystemUI reference point
    }
    else //Otherwise use current headset pose. This works decently when looking straight, but drifts sligthly when not
    {
        vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;
        vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, 0 /*don't predict anything here*/, poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

        if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
        {
            Matrix4 mat_pose = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;

            //Offset pose 0.10 m forward to the actual center of the HMD pose. This is still pretty hacky, but minimizes deviation from not looking straight
            mat_pose.translate_relative(0.0f, 0.0f, 0.10f);

            m_DashboardHMD_Y = mat_pose.getTranslation().y;
        }
    }
}

bool OverlayDragger::IsDragActive() const
{
    return (m_DragModeDeviceID != -1);
}

bool OverlayDragger::IsDragGestureActive() const
{
    return m_DragGestureActive;
}

int OverlayDragger::GetDragDeviceID() const
{
    return m_DragModeDeviceID;
}

unsigned int OverlayDragger::GetDragOverlayID() const
{
    return m_DragModeOverlayID;
}

vr::VROverlayHandle_t OverlayDragger::GetDragOverlayHandle() const
{
    return m_DragModeOverlayHandle;
}

const Matrix4& OverlayDragger::GetDragOverlayMatrix() const
{
    return m_DragModeMatrixTargetCurrent;
}
