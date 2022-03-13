#include "FloatingUI.h"

#include "UIManager.h"
#include "OverlayManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"

FloatingUI::FloatingUI() : m_OvrlHandleCurrentUITarget(vr::k_ulOverlayHandleInvalid),
                           m_OvrlIDCurrentUITarget(0),
                           m_Width(1.0f),
                           m_Alpha(0.0f),
                           m_Visible(false),
                           m_IsSwitchingTarget(false),
                           m_FadeOutDelayCount(0.0f),
                           m_AutoFitFrames(0)
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
            vr::VROverlay()->SetOverlayInputMethod(ovrl_handle_floating_ui, vr::VROverlayInputMethod_Mouse);

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
            m_Alpha += (m_Visible) ? 0.1f : -0.1f;

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
                m_Alpha += 0.1f;
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
    else if ( (m_Visible) && (has_pointer_device) && (ConfigManager::Get().IsLaserPointerTargetOverlay(ovrl_handle_floating_ui)) )  //Use as target Floating UI if it's hovered
    {
        ovrl_handle_hover_target = ovrl_handle_floating_ui;
    }

    //Don't show UI if ImGui popup is open (which blocks all input so just hide this)
    //ImGui::IsPopupOpen() doesn't just check for modals though so it could get in the way at some point
    if ( (ovrl_handle_hover_target == vr::k_ulOverlayHandleInvalid) && (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) && (!ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp)) )
    {
        if (has_pointer_device)
        {
            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().FindOverlayHandle(i);

                if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
                {
                    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                    if ( (data.ConfigBool[configid_bool_overlay_floatingui_enabled]) && (ConfigManager::Get().IsLaserPointerTargetOverlay(ovrl_handle)) )
                    {
                        ovrl_handle_hover_target = ovrl_handle;
                        ovrl_id_hover_target = i;

                        break;
                    }
                    else if ( (ovrl_id_primary_dashboard == k_ulOverlayID_None) && (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_dashboard) && 
                              (data.ConfigInt[configid_int_overlay_display_mode] != ovrl_dispmode_scene) )
                    {
                        ovrl_id_primary_dashboard = i;

                        //First dashboard origin with non-scene display mode is considered to be the primary dashboard overlay, but only really use it if enabled with FloatingUI on
                        if ( (data.ConfigBool[configid_bool_overlay_enabled]) && (data.ConfigBool[configid_bool_overlay_floatingui_enabled]) && (vr::VROverlay()->IsOverlayVisible(ovrl_handle)) )
                        {
                            ovrl_handle_primary_dashboard = ovrl_handle;
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
        return;
    }
    else if ( (ovrl_handle_hover_target != ovrl_handle_floating_ui) && (ovrl_handle_hover_target != vr::k_ulOverlayHandleInvalid) )
    {
        if ( (!m_Visible) && (m_Alpha == 0.0f) )
        {
            is_newly_visible = true;
        }

        m_OvrlHandleCurrentUITarget = ovrl_handle_hover_target;
        m_OvrlIDCurrentUITarget = ovrl_id_hover_target;
        m_Visible = true;
    }

    //If there is an active hover target overlay, position the floating UI
    if (m_OvrlHandleCurrentUITarget != vr::k_ulOverlayHandleInvalid)
    {
        Matrix4 matrix = OverlayManager::Get().GetOverlayCenterBottomTransform(m_OvrlIDCurrentUITarget, m_OvrlHandleCurrentUITarget);

        //If the Floating UI is just appearing, adjust overlay size based on the distance between HMD and overlay
        if (is_newly_visible)
        {
            //Use fixed size when using primary dashboard overlay fallback
            if (ovrl_handle_primary_dashboard == m_OvrlHandleCurrentUITarget)
            {
                m_Width = 1.2f;
                vr::VROverlay()->SetOverlayWidthInMeters(ovrl_handle_floating_ui, m_Width);
            }
            else
            {
                vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
                vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

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

        if ( (is_newly_visible) || (!UIManager::Get()->IsDummyOverlayTransformUnstable()) )
        {
            vr::HmdMatrix34_t hmd_matrix = matrix.toOpenVR34();
            vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle_floating_ui, vr::TrackingUniverseStanding, &hmd_matrix);
        }

        //Set floating UI curvature based on target overlay curvature
        float curvature;
        vr::VROverlay()->GetOverlayCurvature(m_OvrlHandleCurrentUITarget, &curvature);
        float overlay_width = OverlayManager::Get().GetConfigData(m_OvrlIDCurrentUITarget).ConfigFloat[configid_float_overlay_width];

        vr::VROverlay()->SetOverlayCurvature(ovrl_handle_floating_ui, curvature * (m_Width / overlay_width) );
    }

    if ( (ovrl_handle_hover_target == vr::k_ulOverlayHandleInvalid) || (m_OvrlHandleCurrentUITarget == vr::k_ulOverlayHandleInvalid) ) //If not even the UI itself is being hovered, fade out
    {
        if (m_Visible)
        {
            m_FadeOutDelayCount += ImGui::GetIO().DeltaTime;

            //Delay normal fade in order to not flicker when switching hover target between mirror overlay and floating UI
            if (m_FadeOutDelayCount > 0.8f)
            {
                //Hide
                m_Visible = false;
                m_FadeOutDelayCount = 0.0f;
            }
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
