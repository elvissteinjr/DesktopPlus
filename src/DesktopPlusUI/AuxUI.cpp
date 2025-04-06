#include "AuxUI.h"

#include "UIManager.h"
#include "InterprocessMessaging.h"
#include "WindowManager.h"
#include "OpenVRExt.h"

AuxUIWindow::AuxUIWindow(AuxUIID ui_id) : m_AuxUIID(ui_id), m_Visible(false), m_Alpha(0.0f), m_IsTransitionFading(false), m_AutoSizeFrames(-1)
{
    //Nothing
}

bool AuxUIWindow::WindowUpdateBase()
{
    AuxUI& aux_ui = UIManager::Get()->GetAuxUI();

    if (m_Visible)
    {
        if (aux_ui.GetActiveUI() != m_AuxUIID)
        {
            Hide();
            return true;
        }

        //Wait for previous fade out to be done before continuing
        if (aux_ui.IsUIFadingOut())
            return false;

        if ( (m_Alpha == 0.0f) && (m_AutoSizeFrames == -1) )
        {
            m_AutoSizeFrames = 2;
            ApplyPendingValues();
        }

        float alpha_prev = m_Alpha;

        //Alpha fade animation
        if (m_AutoSizeFrames <= 0)
        {
            m_AutoSizeFrames = -1;

            m_Alpha += ImGui::GetIO().DeltaTime * 7.5f;

            if (m_Alpha > 1.0f)
                m_Alpha = 1.0f;
        }

        //Set overlay alpha when not in desktop mode
        if ( (!UIManager::Get()->IsInDesktopMode()) && (alpha_prev != m_Alpha) )
        {
            vr::VROverlay()->SetOverlayAlpha(UIManager::Get()->GetOverlayHandleAuxUI(), m_Alpha);
        }
    }
    else if (m_Alpha != 0.0f)
    {
        float alpha_prev = m_Alpha;

        //Alpha fade animation
        m_Alpha -= ImGui::GetIO().DeltaTime * 7.5f;

        if (m_Alpha < 0.0f)
            m_Alpha = 0.0f;

        //Set overlay alpha when not in desktop mode
        if ( (!UIManager::Get()->IsInDesktopMode()) && (alpha_prev != m_Alpha) )
        {
            vr::VROverlay()->SetOverlayAlpha(UIManager::Get()->GetOverlayHandleAuxUI(), m_Alpha);
        }

        if (m_Alpha == 0.0f)
        {
            if (m_IsTransitionFading)
            {
                m_IsTransitionFading = false;
                Show();
            }
            else
            {
                aux_ui.SetFadeOutFinished();
                aux_ui.ClearActiveUI(m_AuxUIID);
            }

            if (!UIManager::Get()->IsInDesktopMode())
                vr::VROverlay()->HideOverlay(UIManager::Get()->GetOverlayHandleAuxUI());
        }
    }
    else
    {
        return false;
    }

    return true;
}

void AuxUIWindow::DrawFullDimmedRectBehindWindow()
{
    //Based on ImGui::RenderDimmedBackgrounds()
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (draw_list->CmdBuffer.Size == 0)
        draw_list->AddDrawCmd();

    draw_list->PushClipRect({0.0f, 0.0f},  ImGui::GetIO().DisplaySize, false); //ImGui FIXME: Need to stricty ensure ImDrawCmd are not merged (ElemCount==6 checks below will verify that)
    draw_list->AddRectFilled({0.0f, 0.0f}, ImGui::GetIO().DisplaySize, ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 0.0f, m_Alpha * 0.5f}));
    ImDrawCmd cmd = draw_list->CmdBuffer.back();
    //IM_ASSERT(cmd.ElemCount == 6);    //This seems to block in the way we use this function, but also appears fine without for now?
    draw_list->CmdBuffer.pop_back();
    draw_list->CmdBuffer.push_front(cmd);
    draw_list->AddDrawCmd(); //ImGui: We need to create a command as CmdBuffer.back().IdxOffset won't be correct if we append to same command.
    draw_list->PopClipRect();
}

void AuxUIWindow::SetUpTextureBounds()
{
    //Set overlay texture bounds based on m_Pos and m_Size (with padding), clamped to available texture space
    vr::VRTextureBounds_t bounds = {};

    const DPRect& rect_total = UITextureSpaces::Get().GetRect(ui_texspace_total);
    float tex_width  = (float)rect_total.GetWidth();
    float tex_height = (float)rect_total.GetHeight();

    const DPRect& rect_aux_ui = UITextureSpaces::Get().GetRect(ui_texspace_aux_ui);
    ImVec2 pos_ovrl = {m_Pos.x - rect_aux_ui.GetTL().x, m_Pos.y - rect_aux_ui.GetTL().y};

    bounds.uMin = clamp(int(m_Pos.x - 2),            rect_aux_ui.GetTL().x, rect_aux_ui.GetBR().x) / tex_width;
    bounds.vMin = clamp(int(m_Pos.y - 2),            rect_aux_ui.GetTL().y, rect_aux_ui.GetBR().y) / tex_height;
    bounds.uMax = clamp(int(m_Pos.x + m_Size.x + 2), rect_aux_ui.GetTL().x, rect_aux_ui.GetBR().x) / tex_width;
    bounds.vMax = clamp(int(m_Pos.y + m_Size.y + 2), rect_aux_ui.GetTL().y, rect_aux_ui.GetBR().y) / tex_height;

    vr::VROverlay()->SetOverlayTextureBounds(UIManager::Get()->GetOverlayHandleAuxUI(), &bounds);
}

void AuxUIWindow::StartTransitionFade()
{
    if (m_Alpha != 0.0f)
    {
        m_Visible = false;
        m_IsTransitionFading = true;
    }
    else //Just skip transition if the window is already invisible
    {
        m_Visible = true;
    }
}

void AuxUIWindow::ApplyPendingValues()
{
    //Does nothing by default
}

bool AuxUIWindow::Show()
{
    if (m_IsTransitionFading)
        return true;

    m_Visible = UIManager::Get()->GetAuxUI().SetActiveUI(m_AuxUIID);
    UIManager::Get()->GetIdleState().AddActiveTime();

    return m_Visible;
}

void AuxUIWindow::Hide()
{
    m_Visible = false;
    m_IsTransitionFading = false;

    //Clear right away if this window never had the chance to fade in
    if (m_Alpha == 0.0f)
    {
        AuxUI& aux_ui = UIManager::Get()->GetAuxUI();
        aux_ui.ClearActiveUI(m_AuxUIID);
        aux_ui.SetFadeOutFinished();
    }

    UIManager::Get()->GetIdleState().AddActiveTime();
}

bool AuxUIWindow::IsVisible()
{
    return m_Visible;
}


//--WindowDragHint
WindowDragHint::WindowDragHint() : AuxUIWindow(auxui_drag_hint), m_TargetDevice(vr::k_unTrackedDeviceIndexInvalid), m_HintType(hint_docking), m_HintTypePending(hint_docking)
{
    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_aux_ui);
    m_Pos  = {float(rect.GetTL().x + 2), float(rect.GetTL().y + 2)};
    m_Size = {-1.0f, -1.0f};
}

void WindowDragHint::SetUpOverlay()
{
    vr::VROverlayHandle_t overlay_handle = UIManager::Get()->GetOverlayHandleAuxUI();

    const float overlay_width = OVERLAY_WIDTH_METERS_AUXUI_DRAG_HINT * (m_Size.x / 200.0f);   //Scale width based on window width for consistent sizing

    vr::VROverlay()->SetOverlayWidthInMeters(overlay_handle, overlay_width);
    vr::VROverlay()->SetOverlaySortOrder(overlay_handle, 100);
    vr::VROverlay()->SetOverlayAlpha(overlay_handle, 0.0f);
    vr::VROverlay()->SetOverlayInputMethod(overlay_handle, vr::VROverlayInputMethod_None);

    SetUpTextureBounds();

    vr::VROverlay()->ShowOverlay(overlay_handle);
}

void WindowDragHint::UpdateOverlayPos()
{
    //Check for never tracked device and show for HMD instead then
    const bool is_device_never_tracked = vr::VRSystem()->GetBoolTrackedDeviceProperty(m_TargetDevice, vr::Prop_NeverTracked_Bool);
    vr::TrackedDeviceIndex_t device_index = (is_device_never_tracked) ? vr::k_unTrackedDeviceIndex_Hmd : m_TargetDevice;

    Matrix4 mat;

    if (device_index == vr::k_unTrackedDeviceIndex_Hmd) //Show in front of HMD
    {
        mat.scale(2.0f);                            //Scale on the transform itself for simplicity
        mat.translate_relative(0.0f, 0.0f, -0.5f);  //Means this is -1m though
    }
    else //Show on controller
    {
        //Initial offset (somewhat centered inside controller model, for Index at least)
        mat.translate_relative(0.0f, 0.0f, 0.10f);
        Vector3 pos = mat.getTranslation();

        //Get poses
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, vr::IVRSystemEx::GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

        if ( (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) && (device_index < vr::k_unMaxTrackedDeviceCount)  && (poses[device_index].bPoseIsValid) )
        {
            //Rotate towards HMD position
            Matrix4 mat_hmd(poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
            Matrix4 mat_controller(poses[device_index].mDeviceToAbsoluteTracking);
            mat = mat_controller;

            vr::IVRSystemEx::TransformLookAt(mat, mat_hmd.getTranslation());

            //Apply rotation difference between controller used as origin and lookat angle
            mat.setTranslation({0.0f, 0.0f, 0.0f});
            mat_controller.setTranslation({0.0f, 0.0f, 0.0f});
            mat_controller.invert();
            mat = mat_controller * mat;

            //Restore position
            mat.setTranslation(pos);
        }

        //Additional offset (move away from controller to avoid clipping, again value fits mostly for Index)
        mat.translate_relative(0.0f, 0.0f, 0.10f);
    }

    //Set transform
    vr::HmdMatrix34_t mat_ovr = mat.toOpenVR34();
    vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(UIManager::Get()->GetOverlayHandleAuxUI(), device_index, &mat_ovr);
}

void WindowDragHint::ApplyPendingValues()
{
    m_HintType = m_HintTypePending;
}

void WindowDragHint::Update()
{
    bool render_window = WindowUpdateBase() || m_Size.x == -1.0f;

    if (!render_window)
        return;

    const bool desktop_mode = UIManager::Get()->IsInDesktopMode();

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

    if (!m_Visible)
        flags |= ImGuiWindowFlags_NoInputs;

    //Center on screen in desktop mode
    if (desktop_mode)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Alpha);
        ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x / 2.0f, ImGui::GetIO().DisplaySize.y / 2.0f}, ImGuiCond_Always, {0.5f, 0.5f});
    }
    else
    {
        ImGui::SetNextWindowPos(m_Pos, ImGuiCond_Always);
    }

    ImGui::Begin("WindowDragHint", nullptr, flags);

    switch (m_HintType)
    {
        case WindowDragHint::hint_docking:                     ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIDragHintDocking));                  break;
        case WindowDragHint::hint_undocking:                   ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIDragHintUndocking));                break;
        case WindowDragHint::hint_ovrl_locked:                 ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIDragHintOvrlLocked));               break;
        case WindowDragHint::hint_ovrl_theater_screen_blocked: ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIDragHintOvrlTheaterScreenBlocked)); break;
    }

    if (desktop_mode)
        DrawFullDimmedRectBehindWindow();

    m_Size = ImGui::GetWindowSize();
    ImGui::End();

    if (desktop_mode)
        ImGui::PopStyleVar();

    if ((!desktop_mode) && (!m_IsTransitionFading))
    {
        UpdateOverlayPos();
    }

    if (m_AutoSizeFrames > 0)
    {
        m_AutoSizeFrames--;

        if ((m_AutoSizeFrames == 0) && (!desktop_mode))
        {
            SetUpOverlay();
        }
    }
}

void WindowDragHint::SetHintType(vr::TrackedDeviceIndex_t device_index, WindowDragHint::HintType hint_type)
{
    if ((m_TargetDevice == device_index) && (m_HintType == hint_type))
        return;

    m_TargetDevice    = device_index;
    m_HintTypePending = hint_type;    //Actual value set after transition

    if (m_Visible)
        StartTransitionFade();
}


//--WindowGazeFadeAutoHint
WindowGazeFadeAutoHint::WindowGazeFadeAutoHint() : AuxUIWindow(auxui_gazefade_auto_hint), m_TargetOverlay(k_ulOverlayID_None), m_Countdown(3), m_TickTime(0.0)
{
    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_aux_ui);
    m_Pos  = {float(rect.GetTL().x + 2), float(rect.GetTL().y + 2)};
    m_Size = {-1.0f, -1.0f};
}

void WindowGazeFadeAutoHint::SetUpOverlay()
{
    vr::VROverlayHandle_t overlay_handle = UIManager::Get()->GetOverlayHandleAuxUI();

    vr::VROverlay()->SetOverlayWidthInMeters(overlay_handle, OVERLAY_WIDTH_METERS_AUXUI_GAZEFADE_AUTO_HINT);
    vr::VROverlay()->SetOverlaySortOrder(overlay_handle, 100);
    vr::VROverlay()->SetOverlayAlpha(overlay_handle, 0.0f);
    vr::VROverlay()->SetOverlayInputMethod(overlay_handle, vr::VROverlayInputMethod_None);

    SetUpTextureBounds();

    vr::VROverlay()->ShowOverlay(overlay_handle);
}

void WindowGazeFadeAutoHint::UpdateOverlayPos()
{
    //Initial offset
    Matrix4 mat;
    mat.translate_relative(0.0f, 0.0f, -1.00f);

    //Set transform
    vr::HmdMatrix34_t mat_ovr = mat.toOpenVR34();
    vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(UIManager::Get()->GetOverlayHandleAuxUI(), vr::k_unTrackedDeviceIndex_Hmd, &mat_ovr);
}

bool WindowGazeFadeAutoHint::Show()
{
    m_Countdown = 3;
    m_TickTime = 0.0;         //Triggers label update right away on Update()

    UIManager::Get()->GetIdleState().AddActiveTime(5000);

    //Deactivate GazeFade during the countdown so the overlay is visible to the user
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, (int)m_TargetOverlay);
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_overlay_gazefade_enabled, false);
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

    return AuxUIWindow::Show();
}

void WindowGazeFadeAutoHint::Hide()
{
    //Trigger GazeFade auto-configure
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, (int)m_TargetOverlay);
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_gaze_fade_auto);
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_overlay_gazefade_enabled, true);
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

    //Enable gaze fade if it isn't yet
    OverlayManager::Get().GetConfigData(m_TargetOverlay).ConfigBool[configid_bool_overlay_gazefade_enabled] = true;

    AuxUIWindow::Hide();
}

void WindowGazeFadeAutoHint::Update()
{
    bool render_window = WindowUpdateBase() || m_Size.x == -1.0f;

    if (!render_window)
        return;

    if (m_TickTime + 1.0 < ImGui::GetTime())
    {
        if (m_Countdown == 0)
        {
            //Hide and keep the old label during fade-out
            Hide();
        }
        else //Update label
        {
            m_Label = TranslationManager::GetString((m_Countdown != 1) ? tstr_AuxUIGazeFadeAutoHint : tstr_AuxUIGazeFadeAutoHintSingular);
            StringReplaceAll(m_Label, "%SECONDS%", std::to_string(m_Countdown));

            m_Countdown--;
            m_TickTime = ImGui::GetTime();
        }
    }

    const bool desktop_mode = UIManager::Get()->IsInDesktopMode();

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

    if (!m_Visible)
        flags |= ImGuiWindowFlags_NoInputs;

    //Center on screen in desktop mode
    if (desktop_mode)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Alpha);
        ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x / 2.0f, ImGui::GetIO().DisplaySize.y / 2.0f}, ImGuiCond_Always, {0.5f, 0.5f});
    }
    else
    {
        ImGui::SetNextWindowPos(m_Pos, ImGuiCond_Always);
    }
    
    ImGui::Begin("WindowGazeFadeAutoHint", nullptr, flags);

    ImGui::TextUnformatted(m_Label.c_str());

    if (desktop_mode)
        DrawFullDimmedRectBehindWindow();

    m_Size = ImGui::GetWindowSize();
    ImGui::End();

    if (desktop_mode)
        ImGui::PopStyleVar();

    if ((!desktop_mode) && (!m_IsTransitionFading))
    {
        UpdateOverlayPos();
    }

    if (m_AutoSizeFrames > 0)
    {
        m_AutoSizeFrames--;

        if ((m_AutoSizeFrames == 0) && (!desktop_mode))
        {
            SetUpOverlay();
        }
    }
}

void WindowGazeFadeAutoHint::SetTargetOverlay(unsigned int overlay_id)
{
    m_TargetOverlay = overlay_id;
}


//--WindowQuickStart
WindowQuickStart::WindowQuickStart() : AuxUIWindow(auxui_window_quickstart), m_CurrentPage(0)
{
    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_aux_ui);
    m_Pos  = {float(rect.GetTL().x + 2), float(rect.GetTL().y + 2)};
    m_Size = {-1.0f, -1.0f};

    m_Transform.zero();
}

void WindowQuickStart::SetUpOverlay()
{
    vr::VROverlayHandle_t overlay_handle = UIManager::Get()->GetOverlayHandleAuxUI();

    vr::VROverlay()->SetOverlayWidthInMeters(overlay_handle, OVERLAY_WIDTH_METERS_AUXUI_WINDOW_QUICKSTART);
    vr::VROverlay()->SetOverlaySortOrder(overlay_handle, 2);
    vr::VROverlay()->SetOverlayAlpha(overlay_handle, 0.0f);
    vr::VROverlay()->SetOverlayInputMethod(overlay_handle, vr::VROverlayInputMethod_Mouse);

    SetUpTextureBounds();
    UpdateOverlayPos();

    vr::VROverlay()->ShowOverlay(overlay_handle);
}

void WindowQuickStart::UpdateOverlayPos()
{
    const float auxui_ovrl_width  = OVERLAY_WIDTH_METERS_AUXUI_WINDOW_QUICKSTART;
    const float auxui_ovrl_height = OVERLAY_WIDTH_METERS_AUXUI_WINDOW_QUICKSTART * (m_Size.y / m_Size.x);

    //Set initial transform to zoom in from the first time this is called while visible
    if ((m_Visible) && (m_Transform.isZero()))
    {
        m_Transform = vr::IVRSystemEx::ComputeHMDFacingTransform(1.0);

        Vector3 pos = m_Transform.getTranslation();
        m_Transform.setTranslation({0.0f, 0.0f, 0.0f});
        m_Transform.scale(0.0f);
        m_Transform.setTranslation(pos);

        return;
    }

    //Set target transform based on page (may update while animating)
    switch (m_CurrentPage)
    {
        case pageid_Welcome:
        {
            m_TransformAnimationEnd = vr::IVRSystemEx::ComputeHMDFacingTransform(1.0f);
            break;
        }
        case pageid_Overlays:
        {
            //Aim for top right side of overlay bar (doesn't obscure the overlay menu mentioned on the page)
            const auto& window = UIManager::Get()->GetOverlayBarWindow();
            const DPRect& rect_tex = UITextureSpaces::Get().GetRect(ui_texspace_overlay_bar);

            //Default to top left of window relative to texspace (note that AuxUI overlay is centered on that spot)
            Vector2 point_2d(window.GetPos().x - rect_tex.GetTL().x, window.GetPos().y - rect_tex.GetTL().y);
            point_2d.x += window.GetSize().x;

            m_TransformAnimationEnd = UIManager::Get()->GetOverlay2DPointTransform(point_2d, UIManager::Get()->GetOverlayHandleOverlayBar());
            m_TransformAnimationEnd.translate_relative(0.0f, auxui_ovrl_height / 2.0f, 0.0f);

            Vector3 pos = m_TransformAnimationEnd.getTranslation();
            m_TransformAnimationEnd.setTranslation({0.0f, 0.0f, 0.0f});
            m_TransformAnimationEnd.rotateY(-10.0f);
            m_TransformAnimationEnd.setTranslation(pos);

            m_TransformAnimationEnd.translate_relative(0.0f, 0.0f, 0.05f);
            break;
        }
        case pageid_Overlays_2:
        {
            //Aim for top left side of overlay bar (doesn't obscure the add overlay menu mentioned on the page)
            const auto& window = UIManager::Get()->GetOverlayBarWindow();
            const DPRect& rect_tex = UITextureSpaces::Get().GetRect(ui_texspace_overlay_bar);

            //Default to top left of window relative to texspace (note that AuxUI overlay is centered on that spot)
            Vector2 point_2d(window.GetPos().x - rect_tex.GetTL().x, window.GetPos().y - rect_tex.GetTL().y);

            m_TransformAnimationEnd = UIManager::Get()->GetOverlay2DPointTransform(point_2d, UIManager::Get()->GetOverlayHandleOverlayBar());
            m_TransformAnimationEnd.translate_relative(0.0f, auxui_ovrl_height / 2.0f, 0.0f);

            Vector3 pos = m_TransformAnimationEnd.getTranslation();
            m_TransformAnimationEnd.setTranslation({0.0f, 0.0f, 0.0f});
            m_TransformAnimationEnd.rotateY(12.5f);
            m_TransformAnimationEnd.setTranslation(pos);

            m_TransformAnimationEnd.translate_relative(0.0f, 0.0f, 0.05f);
            break;
        }
        case pageid_OverlayProperties:
        case pageid_OverlayProperties_2:
        {
            const auto& window = UIManager::Get()->GetOverlayPropertiesWindow();
            const DPRect& rect_tex = UITextureSpaces::Get().GetRect(ui_texspace_overlay_properties);

            //Top left of window relative to texspace
            Vector2 point_2d(window.GetPos().x - rect_tex.GetTL().x, window.GetPos().y - rect_tex.GetTL().y);
            point_2d.y += window.GetSize().y;

            m_TransformAnimationEnd = UIManager::Get()->GetOverlay2DPointTransform(point_2d, window.GetOverlayHandle());
            m_TransformAnimationEnd.translate_relative(auxui_ovrl_width * 1.52f, auxui_ovrl_height / 4.0f, 0.05f);

            Vector3 pos = m_TransformAnimationEnd.getTranslation();
            m_TransformAnimationEnd.setTranslation({0.0f, 0.0f, 0.0f});
            m_TransformAnimationEnd.rotateY(-25.0f);
            m_TransformAnimationEnd.setTranslation(pos);

            m_TransformAnimationEnd.translate_relative(0.0f, 0.0f, 0.125f);
            break;
        }
        case pageid_Settings:
        case pageid_Profiles:
        case pageid_Actions:
        case pageid_Actions_2:
        case pageid_OverlayTags:
        case pageid_Settings_End:
        {
            const auto& window = UIManager::Get()->GetSettingsWindow();
            const DPRect& rect_tex = UITextureSpaces::Get().GetRect(ui_texspace_settings);

            //Top left of window relative to texspace
            Vector2 point_2d(window.GetPos().x - rect_tex.GetTL().x, window.GetPos().y - rect_tex.GetTL().y);
            point_2d.y += window.GetSize().y;

            m_TransformAnimationEnd = UIManager::Get()->GetOverlay2DPointTransform(point_2d, UIManager::Get()->GetOverlayHandleSettings());
            m_TransformAnimationEnd.translate_relative(auxui_ovrl_width * -1.02f, auxui_ovrl_height / 4.0f, 0.05f);

            Vector3 pos = m_TransformAnimationEnd.getTranslation();
            m_TransformAnimationEnd.setTranslation({0.0f, 0.0f, 0.0f});
            m_TransformAnimationEnd.rotateY(25.0f);
            m_TransformAnimationEnd.setTranslation(pos);

            m_TransformAnimationEnd.translate_relative(0.0f, 0.0f, 0.125f);
            break;
        }
        case pageid_FloatingUI:
        {
            const auto& window = UIManager::Get()->GetFloatingUI().GetActionBarWindow();
            const DPRect& rect_tex = UITextureSpaces::Get().GetRect(ui_texspace_floating_ui);

            //Top left of window relative to texspace
            Vector2 point_2d(window.GetPos().x - rect_tex.GetTL().x, window.GetPos().y - rect_tex.GetTL().y);
            point_2d.x += window.GetSize().x / 2.0f;

            m_TransformAnimationEnd = UIManager::Get()->GetOverlay2DPointTransform(point_2d, UIManager::Get()->GetOverlayHandleFloatingUI());
            m_TransformAnimationEnd.translate_relative(0.0f, (auxui_ovrl_height / 2.0f) + 0.05f, 0.0f);

            m_TransformAnimationEnd.translate_relative(0.0f, 0.0f, 0.05f);
            break;
        }
        case pageid_DesktopMode:
        case pageid_ReadMe:
        {
            m_TransformAnimationEnd = vr::IVRSystemEx::ComputeHMDFacingTransform(1.0f);
            break;
        }
    }

    //Set start point if this animation is just starting
    if (m_TransformAnimationProgress == 0.0f)
    {
        m_TransformAnimationStart = m_Transform;
    }

    //Smoothstep over each matrix component and set the result
    float mat_array[16] = {};
    for (int i = 0; i < 16; ++i)
    {
        mat_array[i] = smoothstep(m_TransformAnimationProgress, m_TransformAnimationStart.get()[i], m_TransformAnimationEnd.get()[i]);
    }

    m_Transform.set(mat_array);

    //Progress animation step
    const float time_step = ImGui::GetIO().DeltaTime * 3.0f;
    m_TransformAnimationProgress += time_step;

    if (m_TransformAnimationProgress > 1.0f)
    {
        m_TransformAnimationProgress = 1.0f;
    }

    //Set transform
    vr::HmdMatrix34_t mat_ovr = m_Transform.toOpenVR34();
    vr::VROverlay()->SetOverlayTransformAbsolute(UIManager::Get()->GetOverlayHandleAuxUI(), vr::TrackingUniverseStanding, &mat_ovr);
}

void WindowQuickStart::OnPageChange(int page_id)
{
    //Set window visibility and scrolls as they should be for the page
    WindowSettings& window_settings = UIManager::Get()->GetSettingsWindow();
    WindowOverlayProperties& window_overlay_properties = UIManager::Get()->GetOverlayPropertiesWindow();

    switch (page_id)
    {
        case pageid_Welcome:
        case pageid_Overlays:
        case pageid_Overlays_2:
        {
            window_settings.Hide();
            window_overlay_properties.Hide();
            break;
        }
        case pageid_OverlayProperties:
        case pageid_OverlayProperties_2:
        {
            window_settings.Hide();
            window_overlay_properties.SetActiveOverlayID(0);
            window_overlay_properties.Show();
            break;
        }
        case pageid_Settings:
        case pageid_Profiles:
        {
            window_settings.Show();
            window_settings.QuickStartGuideGoToPage(wndsettings_page_main);
            window_overlay_properties.Hide();
            break;
        }
        case pageid_Actions:
        {
            window_settings.Show();
            window_settings.QuickStartGuideGoToPage(wndsettings_page_actions);
            window_overlay_properties.Hide();
            break;
        }
        case pageid_Actions_2:
        case pageid_OverlayTags:
        {
            window_settings.Show();
            window_settings.QuickStartGuideGoToPage(wndsettings_page_actions_edit);
            window_overlay_properties.Hide();
            break;
        }
        case pageid_Settings_End:
        {
            window_settings.Show();
            window_settings.QuickStartGuideGoToPage(wndsettings_page_main);
            window_overlay_properties.Hide();
            break;
        }
        case pageid_FloatingUI:
        case pageid_DesktopMode:
        case pageid_ReadMe:
        {
            window_settings.Hide();
            window_overlay_properties.Hide();
            break;
        }
    }

    m_TransformAnimationProgress = 0.0f;
}

bool WindowQuickStart::Show()
{
    return AuxUIWindow::Show();
}

void WindowQuickStart::Hide()
{
    AuxUIWindow::Hide();
}

void WindowQuickStart::Update()
{
    //Temporarily hide when leaving dashboard tab
    if (!ConfigManager::GetValue(configid_bool_interface_quick_start_hidden))
    {
        if (UIManager::Get()->IsOverlayBarOverlayVisible())
        {
            if (!m_Visible)
            {
                //We need to wait a bit or else we might get old overlay transform data when calling UpdateOverlayPos() after this
                //Waiting two frames seems to be sufficient and isn't much of an issue
                vr::VROverlay()->WaitFrameSync(100);
                vr::VROverlay()->WaitFrameSync(100);

                Show();
                m_TransformAnimationProgress = 0.0f;
            }
        }
        else if (m_Visible)
        {
            Hide();
        }
    }

    bool render_window = WindowUpdateBase() || m_Size.x == -1.0f;

    if (!render_window)
        return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    if (!m_Visible)
        flags |= ImGuiWindowFlags_NoInputs;

    ImGuiStyle& style = ImGui::GetStyle();
    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_aux_ui);

    ImGui::SetNextWindowPos(m_Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({(float)rect.GetWidth() - 4.0f, (float)rect.GetHeight() * 0.85f});
    ImGui::Begin("WindowQuickStart", nullptr, flags);

    const float page_width = m_Size.x - style.WindowBorderSize - style.WindowPadding.x - style.WindowPadding.x;

    //Page animation
    if (m_PageAnimationDir != 0)
    {
        //Use the averaged framerate value instead of delta time for the first animation step
        //This is to smooth over increased frame deltas that can happen when a new page needs to do initial larger computations or save/load files
        const float progress_step = (m_PageAnimationProgress == 0.0f) ? (1.0f / ImGui::GetIO().Framerate) * 3.0f : ImGui::GetIO().DeltaTime * 3.0f;
        m_PageAnimationProgress += progress_step;

        if (m_PageAnimationProgress >= 1.0f)
        {
            m_PageAnimationProgress   = 1.0f;
            m_PageAnimationDir        = 0;
            m_CurrentPageAnimationMax = m_CurrentPageAnimation;
        }
    }
    else if (m_CurrentPageAnimation != m_CurrentPage) //Only start new animation if none is running
    {
        m_PageAnimationDir        = (m_CurrentPageAnimation < m_CurrentPage) ? -1 : 1;
        m_CurrentPageAnimation    = m_CurrentPage;
        m_PageAnimationStartPos   = m_PageAnimationOffset;
        m_PageAnimationProgress   = 0.0f;
        m_CurrentPageAnimationMax = std::max(m_CurrentPage, m_CurrentPageAnimationMax);
    }

    const float target_x = (page_width + style.ItemSpacing.x) * -m_CurrentPageAnimation;
    m_PageAnimationOffset = smoothstep(m_PageAnimationProgress, m_PageAnimationStartPos, target_x);

    //Set up page offset and clipping
    ImGui::SetCursorPosX( (ImGui::GetCursorPosX() + m_PageAnimationOffset) );

    const ImVec2 child_size = {page_width, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - style.ItemSpacing.y};
    const int active_page_count = m_CurrentPageAnimationMax + 1;
    for (int i = 0; i < active_page_count; ++i)
    {
        //Disable items when the page isn't active
        const bool is_inactive_page = (i != m_CurrentPage);

        if (is_inactive_page)
        {
            ImGui::PushItemDisabledNoVisual();
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));

        if ( (ImGui::BeginChild(ImGui::GetID((void*)(intptr_t)i), child_size, ImGuiChildFlags_NavFlattened)) )
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg

            ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);

            switch (i)
            {
                case pageid_Welcome:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartWelcomeHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartWelcomeBody));
                    break;
                }
                case pageid_Overlays:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartOverlaysHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartOverlaysBody));
                    break;
                }
                case pageid_Overlays_2:
                {

                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartOverlaysHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartOverlaysBody2));
                    break;
                }
                case pageid_OverlayProperties:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartOverlayPropertiesHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartOverlayPropertiesBody));
                    break;
                }
                case pageid_OverlayProperties_2:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartOverlayPropertiesHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartOverlayPropertiesBody2));
                    break;
                }
                case pageid_Settings:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartSettingsHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartSettingsBody));
                    break;
                }
                case pageid_Profiles:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartProfilesHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartProfilesBody));
                    break;
                }
                case pageid_Actions:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartActionsHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartActionsBody));
                    break;
                }
                case pageid_Actions_2:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartActionsHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartActionsBody2));
                    break;
                }
                case pageid_OverlayTags:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartOverlayTagsHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartOverlayTagsBody));
                    break;
                }
                case pageid_Settings_End:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartSettingsHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartSettingsEndBody));
                    break;
                }
                case pageid_FloatingUI:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartFloatingUIHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartFloatingUIBody));
                    break;
                }
                case pageid_DesktopMode:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartDesktopModeHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartDesktopModeBody));
                    break;
                }
                case pageid_ReadMe:
                {
                    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_AuxUIQuickStartEndHeader));
                    ImGui::Indent();
                    ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIQuickStartEndBody));
                    break;
                }
            }

            ImGui::Unindent();
            ImGui::PopTextWrapPos();
        }
        else
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg
        }

        if (is_inactive_page)
        {
            ImGui::PopItemDisabledNoVisual();
        }

        ImGui::EndChild();

        if (i + 1 < active_page_count)
        {
            ImGui::SameLine();
        }
    }

    //Bottom buttons
    ImGui::Separator();

    int current_page_prev = m_CurrentPage;

    if (current_page_prev == 0)
        ImGui::PushItemDisabled();

    if (ImGui::Button(TranslationManager::GetString(tstr_AuxUIQuickStartButtonPrev)))
    {
        m_CurrentPage--;
        OnPageChange(m_CurrentPage);

        UIManager::Get()->RepeatFrame();
    }

    if (current_page_prev == 0)
        ImGui::PopItemDisabled();

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    if (current_page_prev == pageid_MAX)
        ImGui::PushItemDisabled();

    if (ImGui::Button(TranslationManager::GetString(tstr_AuxUIQuickStartButtonNext))) 
    {
        m_CurrentPage++;
        OnPageChange(m_CurrentPage);

        UIManager::Get()->RepeatFrame();
    }

    if (current_page_prev == pageid_MAX)
        ImGui::PopItemDisabled();

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_AuxUIQuickStartButtonClose))) 
    {
        ConfigManager::SetValue(configid_bool_interface_quick_start_hidden, true);
        Hide();
    }

    m_Size = ImGui::GetWindowSize();
    ImGui::End();

    if ( (!UIManager::Get()->IsInDesktopMode()) && (m_Visible) && (!m_IsTransitionFading) && (m_TransformAnimationProgress != 1.0f) )
    {
        UpdateOverlayPos();
    }

    if (m_AutoSizeFrames > 0)
    {
        m_AutoSizeFrames--;

        if ((m_AutoSizeFrames == 0) && (!UIManager::Get()->IsInDesktopMode()))
        {
            SetUpOverlay();
        }
    }
}

void WindowQuickStart::Reset()
{
    Hide();
    ConfigManager::SetValue(configid_bool_interface_quick_start_hidden, false);
    UIManager::Get()->GetSettingsWindow().QuickStartGuideGoToPage(wndsettings_page_main);
    m_Transform.zero();

    m_CurrentPage = 0;
    OnPageChange(m_CurrentPage);

    if (m_Alpha == 0.0f)
    {
        m_PageAnimationProgress = 1.0f;
    }

    UIManager::Get()->RepeatFrame();
}


//--WindowCaptureWindowSelect
WindowCaptureWindowSelect::WindowCaptureWindowSelect() : AuxUIWindow(auxui_window_select), m_WindowLastClicked(nullptr), m_HoveredTickLast(0)
{
    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_aux_ui);
    m_Size = {float(rect.GetWidth() - 4), float(rect.GetHeight() - 4)};
    m_Pos  = {float(rect.GetTL().x  + 2), float(rect.GetTL().y   + 2)};
    m_PosCenter = {(float)rect.GetCenter().x, (float)rect.GetCenter().y};
}

void WindowCaptureWindowSelect::SetUpOverlay()
{
    vr::VROverlayHandle_t overlay_handle = UIManager::Get()->GetOverlayHandleAuxUI();

    vr::VROverlay()->SetOverlayWidthInMeters(overlay_handle, OVERLAY_WIDTH_METERS_AUXUI_WINDOW_SELECT);
    vr::VROverlay()->SetOverlaySortOrder(overlay_handle, 2);
    vr::VROverlay()->SetOverlayAlpha(overlay_handle, 0.0f);
    vr::VROverlay()->SetOverlayInputMethod(overlay_handle, vr::VROverlayInputMethod_Mouse);

    //Take transform set by Overlay Bar and offset it a bit forward
    Matrix4 mat = m_Transform;
    mat.translate_relative(0.0f, 0.0f, 0.1f);

    vr::HmdMatrix34_t mat_ovr = mat.toOpenVR34();
    vr::VROverlay()->SetOverlayTransformAbsolute(overlay_handle, vr::TrackingUniverseStanding, &mat_ovr);

    SetUpTextureBounds();

    vr::VROverlay()->ShowOverlay(overlay_handle);
}

void WindowCaptureWindowSelect::ApplyPendingValues()
{
    m_WindowLastClicked = nullptr;
}

bool WindowCaptureWindowSelect::Show()
{
    //Limit window height to perfectly fit 14 list entries (not done in constructor since we need ImGui context)
    m_Size.y = std::min( ((ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y) * 14.0f) + (ImGui::GetStyle().WindowPadding.y * 2.0f), m_Size.y);
    m_HoveredTickLast = ::GetTickCount64();

    return AuxUIWindow::Show();
}

void WindowCaptureWindowSelect::Update()
{
    bool render_window = WindowUpdateBase() || m_Size.x == -1.0f;

    if (!render_window)
        return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

    if (!m_Visible)
        flags |= ImGuiWindowFlags_NoInputs;


    ImGui::SetNextWindowSizeConstraints({4.0f, 4.0f}, m_Size);
    ImGui::SetNextWindowPos(m_PosCenter, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowScroll({0.0f, -1.0f});                               //Prevent horizontal scrolling from happening on overflow
    ImGui::Begin("WindowCaptureWindowSelect", nullptr, flags);

    //Hide if clicked outside or not hovered for 3 seconds
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
    {
        if ((ImGui::IsAnyMouseClicked()) || (m_HoveredTickLast + 3000 < ::GetTickCount64()))
        {
           Hide();
        }
    }
    else
    {
        m_HoveredTickLast = ::GetTickCount64();
    }

    //Adjust selectable colors to avoid flicker when fading the window out after activating
    ImGui::PushStyleColor(ImGuiCol_Header,       ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));

    //List windows
    ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
    ImVec2 img_size, img_uv_min, img_uv_max;
    for (const auto& window_info : WindowManager::Get().WindowListGet())
    {
        ImGui::PushID(window_info.GetWindowHandle());
        ImGui::Selectable("", (window_info.GetWindowHandle() == m_WindowLastClicked));

        if (ImGui::IsItemActivated())
        {
            if (UIManager::Get()->IsOpenVRLoaded())
            {
                vr::TrackedDeviceIndex_t device_index = ConfigManager::Get().GetPrimaryLaserPointerDevice();

                //If no dashboard device, try finding one
                if (device_index == vr::k_unTrackedDeviceIndexInvalid)
                {
                    device_index = vr::IVROverlayEx::FindPointerDeviceForOverlay(UIManager::Get()->GetOverlayHandleAuxUI());
                }

                //Try to get the pointer distance
                float source_distance = 1.0f;
                vr::VROverlayIntersectionResults_t results;

                if (vr::IVROverlayEx::ComputeOverlayIntersectionForDevice(UIManager::Get()->GetOverlayHandleAuxUI(), device_index, vr::TrackingUniverseStanding, &results))
                {
                    source_distance = results.fDistance;
                }

                //Set pointer hint in case dashboard app needs it
                ConfigManager::SetValue(configid_int_state_laser_pointer_device_hint, (int)device_index);
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_laser_pointer_device_hint, (int)device_index);

                //Add overlay
                HWND window_handle = window_info.GetWindowHandle();
                OverlayManager::Get().AddOverlay(ovrl_capsource_winrt_capture, -2, window_handle);

                //Send to dashboard app
                IPCManager::Get().PostConfigMessageToDashboardApp(configid_handle_state_arg_hwnd, (LPARAM)window_handle);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_new_drag, MAKELPARAM(-2, (source_distance * 100.0f)));
            }

            //Store last clicked window to avoid Selectable flicker on fade out
            m_WindowLastClicked = window_info.GetWindowHandle();

            //We're done here, hide this window
            Hide();
        }

        ImGui::SameLine(0.0f, 0.0f);

        //Window icon and title
        int icon_id = TextureManager::Get().GetWindowIconCacheID(window_info.GetIcon());

        if (icon_id != -1)
        {
            TextureManager::Get().GetWindowIconTextureInfo(icon_id, img_size, img_uv_min, img_uv_max);
            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }

        ImGui::TextUnformatted(window_info.GetListTitle().c_str());

        ImGui::PopID();
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleColor();

    ImGui::End();

    //Handle auto-size frames
    if (m_AutoSizeFrames > 0)
    {
        m_AutoSizeFrames--;

        if ((m_AutoSizeFrames == 0) && (!UIManager::Get()->IsInDesktopMode()))
        {
            SetUpOverlay();
        }
    }
}

void WindowCaptureWindowSelect::SetTransform(Matrix4 transform)
{
    m_Transform = transform;

    if (m_Visible)
    {
        StartTransitionFade();
    }
}


//--AuxUI
AuxUI::AuxUI() : m_ActiveUIID(auxui_none), m_IsUIFadingOut(false)
{
    //Nothing
}

void AuxUI::Update()
{
    //All Aux UI windows are updated even when not active
    m_WindowDragHint.Update();
    m_WindowGazeFadeAutoHint.Update();
    m_WindowCaptureWindowSelect.Update();
    m_WindowQuickStart.Update();
}

bool AuxUI::IsActive() const
{
    return (m_ActiveUIID != auxui_none);
}

void AuxUI::HideTemporaryWindows()
{
    switch (m_ActiveUIID)
    {
        case auxui_window_select:
        {
            ClearActiveUI(m_ActiveUIID);
            break;
        }
        default: break;
    }
}

bool AuxUI::SetActiveUI(AuxUIID new_ui_id)
{
    if (new_ui_id >= m_ActiveUIID)
    {
        if ( (m_ActiveUIID != auxui_none) && (new_ui_id != m_ActiveUIID) )
        {
            m_IsUIFadingOut = true;
        }

        m_ActiveUIID = new_ui_id;

        return true;
    }

    return false;
}

AuxUIID AuxUI::GetActiveUI() const
{
    return m_ActiveUIID;
}

void AuxUI::ClearActiveUI(AuxUIID current_ui_id)
{
    if (m_ActiveUIID == current_ui_id)
    {
        m_ActiveUIID = auxui_none;
    }
}

bool AuxUI::IsUIFadingOut() const
{
    return m_IsUIFadingOut;
}

void AuxUI::SetFadeOutFinished()
{
    m_IsUIFadingOut = false;
}

WindowDragHint& AuxUI::GetDragHintWindow()
{
    return m_WindowDragHint;
}

WindowGazeFadeAutoHint& AuxUI::GetGazeFadeAutoHintWindow()
{
    return m_WindowGazeFadeAutoHint;
}

WindowCaptureWindowSelect& AuxUI::GetCaptureWindowSelectWindow()
{
    return m_WindowCaptureWindowSelect;
}

WindowQuickStart& AuxUI::GetQuickStartWindow()
{
    return m_WindowQuickStart;
}
