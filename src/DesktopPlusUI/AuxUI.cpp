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
    return m_Visible;
}

void AuxUIWindow::Hide()
{
    m_Visible = false;
    m_IsTransitionFading = false;
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

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

    if (!m_Visible)
        flags |= ImGuiWindowFlags_NoInputs;

    ImGui::SetNextWindowPos(m_Pos, ImGuiCond_Always);
    ImGui::Begin("WindowDragHint", nullptr, flags);

    switch (m_HintType)
    {
        case WindowDragHint::hint_docking:                     ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIDragHintDocking));                  break;
        case WindowDragHint::hint_undocking:                   ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIDragHintUndocking));                break;
        case WindowDragHint::hint_ovrl_locked:                 ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIDragHintOvrlLocked));               break;
        case WindowDragHint::hint_ovrl_theater_screen_blocked: ImGui::TextUnformatted(TranslationManager::GetString(tstr_AuxUIDragHintOvrlTheaterScreenBlocked)); break;
    }

    m_Size = ImGui::GetWindowSize();
    ImGui::End();

    if ( (!UIManager::Get()->IsInDesktopMode()) && (!m_IsTransitionFading) )
    {
        UpdateOverlayPos();
    }

    if (m_AutoSizeFrames > 0)
    {
        m_AutoSizeFrames--;

        if (m_AutoSizeFrames == 0)
        {
            if (!UIManager::Get()->IsInDesktopMode())
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
    //Initial offset (somewhat centered inside controller model, for Index at least)
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

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

    if (!m_Visible)
        flags |= ImGuiWindowFlags_NoInputs;

    ImGui::SetNextWindowPos(m_Pos, ImGuiCond_Always);
    ImGui::Begin("WindowGazeFadeAutoHint", nullptr, flags);

    ImGui::TextUnformatted(m_Label.c_str());

    m_Size = ImGui::GetWindowSize();
    ImGui::End();

    if ( (!UIManager::Get()->IsInDesktopMode()) && (!m_IsTransitionFading) )
    {
        UpdateOverlayPos();
    }

    if (m_AutoSizeFrames > 0)
    {
        m_AutoSizeFrames--;

        if (m_AutoSizeFrames == 0)
        {
            if (!UIManager::Get()->IsInDesktopMode())
                SetUpOverlay();
        }
    }
}

void WindowGazeFadeAutoHint::SetTargetOverlay(unsigned int overlay_id)
{
    m_TargetOverlay = overlay_id;
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

        if (m_AutoSizeFrames == 0)
        {
            if (!UIManager::Get()->IsInDesktopMode())
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