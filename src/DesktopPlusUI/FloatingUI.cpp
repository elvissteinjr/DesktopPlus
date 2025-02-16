#include "FloatingUI.h"

#include "UIManager.h"
#include "OverlayManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "OpenVRExt.h"

FloatingUI::FloatingUI() : m_OvrlHandleCurrentUITarget(vr::k_ulOverlayHandleInvalid),
                           m_OvrlIDCurrentUITarget(0),
                           m_Width(1.0f),
                           m_Alpha(0.0f),
                           m_Visible(false),
                           m_IsSwitchingTarget(false),
                           m_FadeOutDelayCount(0.0f),
                           m_AutoFitFrames(0),
                           m_TheaterOffsetAnimationProgress(0.0f)
{

}

void FloatingUI::Update()
{
    if ( (m_Alpha != 0.0f) || (m_Visible) )
    {
        vr::VROverlayHandle_t ovrl_handle_floating_ui = UIManager::Get()->GetOverlayHandleFloatingUI();

        if ( (m_Alpha == 0.0f) && (m_AutoFitFrames == 0) ) //Overlay was hidden before
        {
            vr::VROverlay()->ShowOverlay(ovrl_handle_floating_ui);

            OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData(m_OvrlIDCurrentUITarget);

            //Instantly adjust action bar visibility to overlay state before fading in
            if (overlay_data.ConfigBool[configid_bool_overlay_actionbar_enabled])
            {
                m_WindowActionBar.Show(true);
            }
            else
            {
                m_WindowActionBar.Hide(true);
            }

            //Give ImGui its two auto-fit frames it typically needs to rearrange widgets properly should they have changed from the last time FloatingUI was visible
            m_AutoFitFrames = 2;
        }

        //Alpha fade animation
        if ( (!UIManager::Get()->GetRepeatFrame()) && (m_AutoFitFrames == 0) )
        {
            const float alpha_step = ImGui::GetIO().DeltaTime * 6.0f;

            m_Alpha += (m_Visible) ? alpha_step : -alpha_step;
        }

        if (m_Alpha > 1.0f)
            m_Alpha = 1.0f;
        else if (m_Alpha < 0.0f)
            m_Alpha = 0.0f;

        vr::VROverlay()->SetOverlayAlpha(ovrl_handle_floating_ui, m_Alpha);

        if ( (m_Alpha == 0.0f) && (m_AutoFitFrames == 0) ) //Overlay was visible before
        {
            vr::VROverlay()->HideOverlay(ovrl_handle_floating_ui);
            m_WindowActionBar.Hide(true);
            //In case we were switching targets, reset switching state and target overlay
            m_IsSwitchingTarget = false;
            m_OvrlHandleCurrentUITarget = vr::k_ulOverlayHandleInvalid;
            m_OvrlIDCurrentUITarget = 0;

            //Request sync if drag-mode is still active while the UI is disappearing
            if (ConfigManager::GetValue(configid_bool_state_overlay_dragmode))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_transform_sync, -1);
            }
        }
        else if (m_Visible) //Make sure the input method is always set when visible, even after partial fade-out
        {
            if (!ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp))
            {
                vr::VROverlay()->SetOverlayInputMethod(ovrl_handle_floating_ui, vr::VROverlayInputMethod_Mouse);
            }
        }
    }

    if ( (m_Alpha != 0.0f) || (m_AutoFitFrames != 0) )
    {
        OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData(m_OvrlIDCurrentUITarget);

        //If action bar state was changed
        if (overlay_data.ConfigBool[configid_bool_overlay_actionbar_enabled])
        {
            m_WindowActionBar.Show();
        }
        else
        {
            m_WindowActionBar.Hide();
        }

        m_WindowActionBar.Update(m_OvrlIDCurrentUITarget);
        m_WindowMainBar.Update(m_WindowActionBar.GetSize().y, m_OvrlIDCurrentUITarget);
        m_WindowOverlayStats.Update(m_WindowMainBar, m_WindowActionBar, m_OvrlIDCurrentUITarget);

        if (m_AutoFitFrames > 0)
        {
            m_AutoFitFrames--;
            UIManager::Get()->RepeatFrame();

            if (m_AutoFitFrames == 0)
            {
                m_Alpha += ImGui::GetIO().DeltaTime * 6.0f;
            }
        }
    }
}

void FloatingUI::UpdateUITargetState()
{
    UIManager& ui_manager = *UIManager::Get();
    vr::VROverlayHandle_t ovrl_handle_floating_ui = ui_manager.GetOverlayHandleFloatingUI();

    //Find which overlay is being hovered
    vr::VROverlayHandle_t ovrl_handle_hover_target = vr::k_ulOverlayHandleInvalid;
    unsigned int ovrl_id_hover_target = k_ulOverlayID_None;

    //Also find primary dashboard overlay as fallback to always display when available
    vr::VROverlayHandle_t ovrl_handle_primary_dashboard = vr::k_ulOverlayHandleInvalid;
    unsigned int ovrl_id_primary_dashboard = k_ulOverlayID_None;

    const bool has_pointer_device = (ConfigManager::Get().GetPrimaryLaserPointerDevice() != vr::k_unTrackedDeviceIndexInvalid);

    //If previous target overlay is no longer visible
    if ( (m_OvrlHandleCurrentUITarget != vr::k_ulOverlayHandleInvalid) && (!vr::VROverlay()->IsOverlayVisible(m_OvrlHandleCurrentUITarget)) )
    {
        m_FadeOutDelayCount = 100.0f;

        //Disable input so the pointer will no longer hit the UI window
        vr::VROverlay()->SetOverlayInputMethod(ovrl_handle_floating_ui, vr::VROverlayInputMethod_None);
    }
    else if ( (has_pointer_device) && (ConfigManager::Get().IsLaserPointerTargetOverlay(ovrl_handle_floating_ui)) )  //Use as target Floating UI if it's hovered
    {
        ovrl_handle_hover_target = ovrl_handle_floating_ui;
    }

    //Don't show while reordering overlays since config changes may cause the UI to jump around while doing that
    if ( (ovrl_handle_hover_target == vr::k_ulOverlayHandleInvalid) && (!ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp)) &&
         (!UIManager::Get()->GetOverlayBarWindow().IsDraggingOverlayButtons()) )
    {
        if (has_pointer_device)
        {
            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);
                vr::VROverlayHandle_t ovrl_handle = data.ConfigHandle[configid_handle_overlay_state_overlay_handle];

                if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
                {
                    //Check if this is the primary dashboard overlay
                    if ( (ovrl_id_primary_dashboard == k_ulOverlayID_None) && (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_dashboard) && 
                         (data.ConfigInt[configid_int_overlay_display_mode] != ovrl_dispmode_scene) )
                    {
                        //First dashboard origin with non-scene display mode is considered to be the primary dashboard overlay, but only really use it if enabled with FloatingUI on and in dashboard
                        if ( (data.ConfigBool[configid_bool_overlay_enabled]) && (data.ConfigBool[configid_bool_overlay_floatingui_enabled]) && (UIManager::Get()->IsOverlayBarOverlayVisible()) && 
                            (vr::VROverlay()->IsOverlayVisible(ovrl_handle)) )
                        {
                            ovrl_handle_primary_dashboard = ovrl_handle;
                            ovrl_id_primary_dashboard = i;
                        }
                    }

                    //Check if this the hover target overlay
                    if ( (data.ConfigBool[configid_bool_overlay_floatingui_enabled]) && (ConfigManager::Get().IsLaserPointerTargetOverlay(ovrl_handle)) )
                    {
                        ovrl_handle_hover_target = ovrl_handle;
                        ovrl_id_hover_target = i;

                        //Break here unless we still need to find the primary dashboard overlay
                        if (ovrl_id_primary_dashboard != k_ulOverlayID_None)
                        {
                            break;
                        }
                    }
                }
            }

            //Use fallback primary dashboard overlay if no hover target was found
            if ( (ovrl_handle_hover_target == vr::k_ulOverlayHandleInvalid) && (m_FadeOutDelayCount == 0) &&
                 ((m_OvrlHandleCurrentUITarget == vr::k_ulOverlayHandleInvalid) || (m_OvrlHandleCurrentUITarget == ovrl_handle_primary_dashboard)) )
            {
                ovrl_handle_hover_target = ovrl_handle_primary_dashboard;
                ovrl_id_hover_target = ovrl_id_primary_dashboard;
            }
        }
    }

    bool is_newly_visible = false;

    //Check if we're switching from another active overlay hover target, in which case we want to fade out completely first
    if ( (m_OvrlHandleCurrentUITarget != vr::k_ulOverlayHandleInvalid) && (ovrl_handle_hover_target != vr::k_ulOverlayHandleInvalid) && (ovrl_handle_hover_target != ovrl_handle_floating_ui) && 
         (ovrl_handle_hover_target != m_OvrlHandleCurrentUITarget) )
    {
        m_IsSwitchingTarget = true;
        m_Visible = false;

        UIManager::Get()->GetIdleState().AddActiveTime();
        return;
    }
    else if ( (ovrl_handle_hover_target != ovrl_handle_floating_ui) && (ovrl_handle_hover_target != vr::k_ulOverlayHandleInvalid) )
    {
        if ( (!m_Visible) && (m_Alpha == 0.0f) )
        {
            is_newly_visible = true;
            UIManager::Get()->GetIdleState().AddActiveTime();
        }

        m_OvrlHandleCurrentUITarget = ovrl_handle_hover_target;
        m_OvrlIDCurrentUITarget = ovrl_id_hover_target;
        m_Visible = true;
    }

    //If there is an active hover target overlay, position the floating UI
    if (m_OvrlHandleCurrentUITarget != vr::k_ulOverlayHandleInvalid)
    {
        OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData(m_OvrlIDCurrentUITarget);
        Matrix4 matrix = OverlayManager::Get().GetOverlayCenterBottomTransform(m_OvrlIDCurrentUITarget, m_OvrlHandleCurrentUITarget);

        //If the Floating UI is just appearing, adjust overlay size based on the distance between HMD and overlay
        if (is_newly_visible)
        {
            bool use_fixed_size = false;

            //Use fixed size when using primary dashboard overlay fallback and distance to dashboard is lower than 0.25m
            if (ovrl_handle_primary_dashboard == m_OvrlHandleCurrentUITarget)
            {
                //Use relative transform data here as dashboard transform can be unreliable during launch
                const float distance = overlay_data.ConfigTransform.getTranslation().distance({0.0f, 0.0f, 0.0f});
                use_fixed_size = (distance < 0.25f);

                m_Width = 1.2f;
                vr::VROverlay()->SetOverlayWidthInMeters(ovrl_handle_floating_ui, m_Width);
            }
            else if (overlay_data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen) //Fixed size for theater screen too
            {
                m_Width = 3.0f;
                vr::VROverlay()->SetOverlayWidthInMeters(ovrl_handle_floating_ui, m_Width);
            }
            else
            {
                vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
                vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, vr::IVRSystemEx::GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

                if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
                {
                    Matrix4 mat_hmd = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
                    float distance = matrix.getTranslation().distance(mat_hmd.getTranslation());

                    m_Width = 0.66f + (0.5f * distance);
                    vr::VROverlay()->SetOverlayWidthInMeters(ovrl_handle_floating_ui, m_Width);
                }
            }
        }

        //Move down by Floating UI height
        const DPRect& rect_floating_ui = UITextureSpaces::Get().GetRect(ui_texspace_floating_ui);
        const float floating_ui_height_m = m_Width * ((float)rect_floating_ui.GetHeight() / (float)rect_floating_ui.GetWidth());

        matrix.translate_relative(0.0f, -floating_ui_height_m / 3.0f, 0.0f);

        //Additional offset for theater screen
        if (overlay_data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen) 
        {
            //SteamVR's control bar is only shown while system laser pointer is active, so only add offset if that's the case
            const bool add_offset = vr::IVROverlayEx::IsSystemLaserPointerActive();

            if (is_newly_visible)
            {
                m_TheaterOffsetAnimationProgress = (add_offset) ? 1.0f : 0.0f;
            }
            else //Also animate this
            {
                const float time_step = ImGui::GetIO().DeltaTime * 6.0f;
                m_TheaterOffsetAnimationProgress += (add_offset) ? time_step : -time_step;

                if (m_TheaterOffsetAnimationProgress > 1.0f)
                    m_TheaterOffsetAnimationProgress = 1.0f;
                else if (m_TheaterOffsetAnimationProgress < 0.0f)
                    m_TheaterOffsetAnimationProgress = 0.0f;
            }

            matrix.translate_relative(0.0f, smoothstep(m_TheaterOffsetAnimationProgress, 0.0f, -0.29f), 0.0f);
        }

        //Don't update position if dummy transform is unstable unless it's target is not primary dashboard overlay or we're newly appearing
        if ( (is_newly_visible) || (!UIManager::Get()->IsDummyOverlayTransformUnstable()) || (ovrl_id_hover_target != ovrl_id_primary_dashboard) )
        {
            //Only set transform and add active time if we actually need to move the overlay
            if (matrix != m_TransformLast)
            {
                vr::HmdMatrix34_t hmd_matrix = matrix.toOpenVR34();
                vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle_floating_ui, vr::TrackingUniverseStanding, &hmd_matrix);

                UIManager::Get()->GetIdleState().AddActiveTime(100);
                m_TransformLast = matrix;
            }
        }

        //Set floating UI curvature based on target overlay curvature
        if (overlay_data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen)
        {
            //Set curvature to 0 in this case, as theater screen curve is controlled by SteamVR and not query-able)
            vr::VROverlay()->SetOverlayCurvature(ovrl_handle_floating_ui, 0.0f);
        }
        else
        {
            float curvature = 0.0f;
            vr::VROverlay()->GetOverlayCurvature(m_OvrlHandleCurrentUITarget, &curvature);
            float overlay_width = overlay_data.ConfigFloat[configid_float_overlay_width];

            vr::VROverlay()->SetOverlayCurvature(ovrl_handle_floating_ui, curvature * (m_Width / overlay_width) );
        }
    }

    if ( (ovrl_handle_hover_target == vr::k_ulOverlayHandleInvalid) || (m_OvrlHandleCurrentUITarget == vr::k_ulOverlayHandleInvalid) ) //If not even the UI itself is being hovered, fade out
    {
        if (m_Visible)
        {
            //Don't fade out if this is the theater screen overlay and the systemui is hovered
            //This does have false positives when really trying (systemui is used for many things), but it's better not to fade out when SteamVR's overlay controls are hovered
            bool blocked_by_systemui = false;
            if (m_OvrlHandleCurrentUITarget != vr::k_ulOverlayHandleInvalid)
            {
                vr::VROverlayHandle_t ovrl_handle_systemui;
                vr::VROverlay()->FindOverlay("system.systemui", &ovrl_handle_systemui);

                if (ovrl_handle_systemui != vr::k_ulOverlayHandleInvalid)
                {
                    const OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData(m_OvrlIDCurrentUITarget);
                    if (overlay_data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen)
                    {
                        blocked_by_systemui = ConfigManager::Get().IsLaserPointerTargetOverlay(ovrl_handle_systemui);
                    }
                }
            }

            if (!blocked_by_systemui)
            {
                m_FadeOutDelayCount += ImGui::GetIO().DeltaTime;

                //Delay normal fade in order to not flicker when switching hover target between mirror overlay and floating UI (or don't while reordering overlays)
                if ((m_FadeOutDelayCount > 0.8f) || (UIManager::Get()->GetOverlayBarWindow().IsDraggingOverlayButtons()))
                {
                    //Hide
                    m_Visible = false;
                    m_FadeOutDelayCount = 0.0f;
                }
            }

            UIManager::Get()->GetIdleState().AddActiveTime();
        }
        else if (m_Alpha == 0.0f)
        {
            m_FadeOutDelayCount = 0.0f;
        }
    }
    else
    {
        m_Visible = true;
    }

    //Update config state if it changed. This state is only set if the Floating UI itself is the hover target
    const int target_overlay_id_new = ((m_OvrlIDCurrentUITarget != k_ulOverlayID_None) && (ovrl_handle_hover_target == ovrl_handle_floating_ui)) ? (int)m_OvrlIDCurrentUITarget : -1;
    int& target_overlay_id_config = ConfigManager::GetRef(configid_int_state_interface_floating_ui_hovered_id);
    if (target_overlay_id_config != target_overlay_id_new)
    {
        target_overlay_id_config = target_overlay_id_new;
        IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_interface_floating_ui_hovered_id, target_overlay_id_config);
    }
}

bool FloatingUI::IsVisible() const
{
    return ((m_Visible) || (m_Alpha != 0.0f));
}

float FloatingUI::GetAlpha() const
{
    return m_Alpha;
}

WindowFloatingUIMainBar& FloatingUI::GetMainBarWindow()
{
    return m_WindowMainBar;
}

WindowFloatingUIActionBar& FloatingUI::GetActionBarWindow()
{
    return m_WindowActionBar;
}
