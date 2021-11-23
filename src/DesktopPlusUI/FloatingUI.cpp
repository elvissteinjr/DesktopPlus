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
    vr::VROverlayHandle_t ovrl_handle_hover_target = (ConfigManager::Get().IsLaserPointerTargetOverlay(ovrl_handle_floating_ui)) ? ovrl_handle_floating_ui : vr::k_ulOverlayHandleInvalid;
    unsigned int ovrl_id_hover_target = k_ulOverlayID_None;

    //If previous target overlay is no longer visible
    if ( (m_OvrlHandleCurrentUITarget != vr::k_ulOverlayHandleInvalid) && (!vr::VROverlay()->IsOverlayVisible(m_OvrlHandleCurrentUITarget)) )
    {
        m_OvrlHandleCurrentUITarget = vr::k_ulOverlayHandleInvalid;
        ovrl_handle_hover_target = vr::k_ulOverlayHandleInvalid;
        m_Visible = false;
        m_FadeOutDelayCount = 100;
    }

    const bool has_pointer_device = (ConfigManager::Get().GetPrimaryLaserPointerDevice() != vr::k_unTrackedDeviceIndexInvalid);

    //Don't show UI if ImGui popup is open (which blocks all input so just hide this)
    //ImGui::IsPopupOpen() doesn't just check for modals though so it could get in the way at some point
    if ( (ovrl_handle_hover_target == vr::k_ulOverlayHandleInvalid) && (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) && (!ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp)) )
    {
        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
        {
            vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().FindOverlayHandle(i);

            if ( (ovrl_handle != vr::k_ulOverlayHandleInvalid) && (ConfigManager::Get().IsLaserPointerTargetOverlay(ovrl_handle)) )
            {
                const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                if ( (has_pointer_device) && (data.ConfigBool[configid_bool_overlay_floatingui_enabled]) )
                {
                    ovrl_handle_hover_target = ovrl_handle;
                    ovrl_id_hover_target = i;

                    break;
                }
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
        //Okay, this is a load of poop to be honest and took a while to get right
        //The gist is that GetTransformForOverlayCoordinates() is fundamentally broken if the overlay has non-default properties
        //UV min/max not 0.0/1.0? You still get coordinates as if they were
        //Pixel aspect not 1.0? That function doesn't care
        //Also doesn't care about curvature, but that's not a huge issue

        vr::HmdMatrix34_t matrix;
        vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;
        vr::VRTextureBounds_t bounds;

        vr::VROverlay()->GetOverlayTextureBounds(m_OvrlHandleCurrentUITarget, &bounds);


        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_OvrlIDCurrentUITarget);

        int ovrl_pixel_width, ovrl_pixel_height;

        if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication)
        {
            ui_manager.GetDesktopOverlayPixelSize(ovrl_pixel_width, ovrl_pixel_height);
        }
        else
        {
            vr::HmdVector2_t ovrl_mouse_scale;
            //Use mouse scale of overlay if possible as it can sometimes differ from the config size (and GetOverlayTextureSize() currently leaks GPU memory, oops)
            if (vr::VROverlay()->GetOverlayMouseScale(m_OvrlHandleCurrentUITarget, &ovrl_mouse_scale) == vr::VROverlayError_None)
            {
                ovrl_pixel_width  = (int)ovrl_mouse_scale.v[0];
                ovrl_pixel_height = (int)ovrl_mouse_scale.v[1];
            }
            else
            {
                ovrl_pixel_width  = data.ConfigInt[configid_int_overlay_state_content_width];
                ovrl_pixel_height = data.ConfigInt[configid_int_overlay_state_content_height];
            }
        }

        //Get 3D height factor
        float height_factor_3d = 1.0f;

        if (data.ConfigBool[configid_bool_overlay_3D_enabled])
        {
            if ((data.ConfigInt[configid_int_overlay_3D_mode] == ovrl_3Dmode_sbs) || (data.ConfigInt[configid_int_overlay_3D_mode] == ovrl_3Dmode_ou))
            {
                //Additionally check if the overlay is actually displaying 3D content right now (can be not the case when error texture is shown)
                uint32_t ovrl_flags = 0;
                vr::VROverlay()->GetOverlayFlags(m_OvrlHandleCurrentUITarget, &ovrl_flags);

                if ((ovrl_flags & vr::VROverlayFlags_SideBySide_Parallel) || (ovrl_flags & vr::VROverlayFlags_SideBySide_Crossed))
                {
                    height_factor_3d = (data.ConfigInt[configid_int_overlay_3D_mode] == ovrl_3Dmode_sbs) ? 2.0f : 0.5f;
                }
            }
        }

        //Attempt to calculate the correct offset to bottom, taking in account all the things GetTransformForOverlayCoordinates() does not
        float width = data.ConfigFloat[configid_float_overlay_width];
        float uv_width  = bounds.uMax - bounds.uMin;
        float uv_height = bounds.vMax - bounds.vMin;
        float cropped_width  = ovrl_pixel_width  * uv_width;
        float cropped_height = ovrl_pixel_height * uv_height * height_factor_3d;
        float aspect_ratio_orig = (float)ovrl_pixel_width / ovrl_pixel_height;
        float aspect_ratio_new = cropped_height / cropped_width;
        float height = (aspect_ratio_orig * width);
        float offset_to_bottom = -( (aspect_ratio_new * width) - (aspect_ratio_orig * width) ) / 2.0f;
        offset_to_bottom -= height / 2.0f;

        //Y-coordinate from this function is pretty much unpredictable if not pixel_height / 2
        vr::VROverlay()->GetTransformForOverlayCoordinates(m_OvrlHandleCurrentUITarget, origin, { (float)ovrl_pixel_width/2.0f, (float)ovrl_pixel_height/2.0f }, &matrix);

        //If the Floating UI is just appearing, adjust overlay size based on the distance between HMD and overlay
        if (is_newly_visible)
        {
            vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
            vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

            if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
            {
                Matrix4 mat_overlay(matrix);
                Matrix4 mat_hmd = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
                float distance = mat_overlay.getTranslation().distance(mat_hmd.getTranslation());

                m_Width = 0.66f + (0.5f * distance);
                vr::VROverlay()->SetOverlayWidthInMeters(ovrl_handle_floating_ui, m_Width);
            }
        }

        //When Performance Monitor, apply additional offset of the unused overlay space
        if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui)
        {
            float height_new = aspect_ratio_new * width; //height uses aspect_ratio_orig, so calculate new height
            offset_to_bottom += ( (cropped_height - UIManager::Get()->GetPerformanceWindow().GetSize().y) ) * ( (height_new / cropped_height) ) / 2.0f;
        }

        //Move to bottom, vertically centering the floating UI overlay on the bottom end of the target overlay (previous function already got the X centered)
        const DPRect& rect_floating_ui = UITextureSpaces::Get().GetRect(ui_texspace_floating_ui);
        const float floating_ui_height_m = m_Width * ((float)rect_floating_ui.GetHeight() / (float)rect_floating_ui.GetWidth());

        offset_to_bottom -= floating_ui_height_m / 3.0f;

        TransformOpenVR34TranslateRelative(matrix, 0.0f, offset_to_bottom, 0.0f);

        if ( (is_newly_visible) || (!UIManager::Get()->IsDummyOverlayTransformUnstable()) )
        {
            vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle_floating_ui, origin, &matrix);
        }

        //Set floating UI curvature based on target overlay curvature
        float curvature;
        vr::VROverlay()->GetOverlayCurvature(m_OvrlHandleCurrentUITarget, &curvature);

        vr::VROverlay()->SetOverlayCurvature(ovrl_handle_floating_ui, curvature * (m_Width / width) );
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

                vr::VROverlay()->SetOverlayFlag(ovrl_handle_floating_ui, vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, false);
            }
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
