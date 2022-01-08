#include "FloatingWindow.h"

#include "UIManager.h"
#include "OverlayManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "ImGuiExt.h"

FloatingWindow::FloatingWindow() : m_OvrlWidth(1.0f),
                                   m_Alpha(0.0f),
                                   m_OvrlVisible(false),
                                   m_IsTransitionFading(false),
                                   m_OverlayStateCurrentID(floating_window_ovrl_state_dashboard_tab),
                                   m_OverlayStateCurrent(&m_OverlayStateDashboardTab),
                                   m_OverlayStatePending(&m_OverlayStateFading),
                                   m_WindowTitleStrID(tstr_NONE),
                                   m_WindowIcon(tmtex_icon_xsmall_desktop_none),
                                   m_WindowIconWin32IconCacheID(-1),
                                   m_AllowRoomUnpinning(false),
                                   m_DragOrigin(ovrl_origin_dplus_tab),
                                   m_TitleBarWidth(64.0f),
                                   m_TitleBarTitleMaxWidth(-1.0f),
                                   m_HasAppearedOnce(false),
                                   m_IsWindowAppearing(false)
{
    m_Pos.x = FLT_MIN;
    m_WindowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    m_WindowID = "###" + std::to_string((unsigned long long)this);
}

void FloatingWindow::WindowUpdateBase()
{
    if ( ((!UIManager::Get()->GetRepeatFrame()) || (m_Alpha == 0.0f)) && ((m_Alpha != 0.0f) || (m_OverlayStateCurrent->IsVisible) || (m_IsTransitionFading)) )
    {
        float alpha_prev = m_Alpha;

        //Alpha fade animation
        m_Alpha += ((m_OverlayStateCurrent->IsVisible) && (!m_IsTransitionFading)) ? 0.1f : -0.1f;

        if (m_Alpha > 1.0f)
            m_Alpha = 1.0f;
        else if (m_Alpha < 0.0f)
            m_Alpha = 0.0f;

        //Use overlay alpha when not in desktop mode for better blending
        if ( (!UIManager::Get()->IsInDesktopMode()) && (alpha_prev != m_Alpha) )
            vr::VROverlay()->SetOverlayAlpha(GetOverlayHandle(), m_Alpha);

        //Finish transition fade if one's active
        if ((m_Alpha == 0.0f) && (m_IsTransitionFading))
        {
            OverlayStateSwitchFinish();
        }
    }

    if (m_Alpha == 0.0f)
        return;

    if (UIManager::Get()->IsInDesktopMode())
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Alpha);

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    if (m_Pos.x == FLT_MIN)
    {
        //Expects m_Size to be padded 2 pixels around actual texture space, so -4 on each axis 
        m_Pos = {2.0f, (io.DisplaySize.y - m_Size.y) - 2.0f};
    }

    ImGui::SetNextWindowPos(m_Pos, ImGuiCond_Always, m_PosPivot);
    ImGui::SetNextWindowSizeConstraints({m_TitleBarWidth, 4.0f}, m_Size);
    ImGui::SetNextWindowScroll({0.0f, -1.0f}); //Prevent real horizontal scrolling from happening

    ImGuiWindowFlags flags = m_WindowFlags;

    if (!m_OverlayStateCurrent->IsVisible)
        flags |= ImGuiWindowFlags_NoInputs;

    ImGui::Begin(m_WindowID.c_str(), nullptr, flags);

    bool title_hover = ImGui::IsItemHovered(); //Current item is the title bar (needs to be checked before BeginTitleBar())
    m_IsWindowAppearing = ImGui::IsWindowAppearing();

    //Title bar
    ImVec4 title_rect = ImGui::BeginTitleBar();

    //Icon and title text
    ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
    ImVec2 img_size, img_uv_min, img_uv_max;

    if (m_WindowIconWin32IconCacheID == -1)
    {
        TextureManager::Get().GetTextureInfo(m_WindowIcon, img_size, img_uv_min, img_uv_max);
    }
    else
    {
        TextureManager::Get().GetWindowIconTextureInfo(m_WindowIconWin32IconCacheID, img_size, img_uv_min, img_uv_max);
    }

    ImGui::Image(io.Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::TextUnformatted( (m_WindowTitleStrID == tstr_NONE) ? m_WindowTitle.c_str() : TranslationManager::GetString(m_WindowTitleStrID) );
    float title_text_width = ImGui::GetItemRectSize().x;
    
    m_TitleBarWidth = img_size_line_height.x  + style.ItemSpacing.x + ImGui::GetItemRectSize().x;
    m_TitleBarTitleMaxWidth = img_size_line_height.x + style.ItemSpacing.x;

    //Right end of title bar
    ImGui::PushStyleColor(ImGuiCol_Button, 0);
    static float b_width = 0.0f;

    ImGui::SetCursorScreenPos({title_rect.z - b_width - style.ItemInnerSpacing.x, title_rect.y + style.FramePadding.y});

    //Buttons
    TextureManager::Get().GetTextureInfo((m_OverlayStateCurrent->IsPinned) ? tmtex_icon_xxsmall_unpin : tmtex_icon_xxsmall_pin, img_size, img_uv_min, img_uv_max);

    ImGui::BeginGroup();

    const bool disable_pinning = ( (!m_AllowRoomUnpinning) && (m_OverlayStateCurrentID == floating_window_ovrl_state_room) );

    if (disable_pinning)
        ImGui::PushItemDisabled();

    ImGui::PushID("PinButton");
    if (ImGui::ImageButton(io.Fonts->TexID, img_size, img_uv_min, img_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
    {
        SetPinned(!IsPinned());
        OnWindowPinButtonPressed();
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, 0.0f);

    if (disable_pinning)
        ImGui::PopItemDisabled();

    TextureManager::Get().GetTextureInfo(tmtex_icon_xxsmall_close, img_size, img_uv_min, img_uv_max);

    ImGui::PushID("CloseButton");
    if (ImGui::ImageButton(io.Fonts->TexID, img_size, img_uv_min, img_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
    {
        Hide();
    }
    ImGui::PopID();

    ImGui::EndGroup();

    title_hover = ( (title_hover) && (!ImGui::IsItemHovered()) ); //Title was hovered and no title bar element is hovered

    b_width = ImGui::GetItemRectSize().x;
    m_TitleBarWidth += b_width + style.ItemSpacing.x;
    m_TitleBarTitleMaxWidth += b_width + style.ItemSpacing.x;

    ImGui::PopStyleColor();

    m_TitleBarWidth = std::min(m_TitleBarWidth, m_Size.x);
    m_TitleBarTitleMaxWidth = std::max(ImGui::GetWindowSize().x, m_TitleBarWidth) - m_TitleBarTitleMaxWidth;

    //Shorten title bar string if it doesn't fit (this is destructive, but doesn't matter for the windows using this)
    if ((m_WindowTitleStrID == tstr_NONE) && (title_text_width > m_TitleBarTitleMaxWidth))
    {
        m_WindowTitle = ImGui::StringEllipsis(m_WindowTitle.c_str(), m_TitleBarTitleMaxWidth);

        //Repeat frame to not make title shortening visible
        if (m_Alpha != 1.0f)
        {
            m_Alpha -= 0.1f; //Also adjust alpha to keep fade smooth
        }
        UIManager::Get()->RepeatFrame();
    }

    //Title bar dragging
    if ( (m_OverlayStateCurrent->IsVisible) && (title_hover) && (UIManager::Get()->IsOpenVRLoaded()) )
    {
        if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) && (!UIManager::Get()->GetOverlayDragger().IsDragActive()) && (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) )
        {
            UIManager::Get()->GetOverlayDragger().DragStart(GetOverlayHandle(), m_DragOrigin);
        }
        else if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) && (!UIManager::Get()->GetOverlayDragger().IsDragActive()) && (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) )
        {
            UIManager::Get()->GetOverlayDragger().DragGestureStart(GetOverlayHandle(), m_DragOrigin);
        }
    }

    ImGui::EndTitleBar();

    //Window content
    WindowUpdate();

    //Hack to work around ImGui's auto-sizing quirks. Just checking for ImGui::IsWindowAppearing() and using alpha 0 then doesn't help on its own so this is the next best thing
    if (!m_HasAppearedOnce)
    {
        if (ImGui::IsWindowAppearing())
        {
            m_Alpha -= 0.1f;
            UIManager::Get()->RepeatFrame();
        }
        else
        {
            m_HasAppearedOnce = true;
        }
    }

    //Blank space drag
    if ( (ConfigManager::GetValue(configid_bool_interface_blank_space_drag_enabled)) && (m_OverlayStateCurrent->IsVisible) && (UIManager::Get()->IsOpenVRLoaded()) && (!ImGui::IsAnyItemHovered()) &&
         (!IsVirtualWindowItemHovered()) && (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) )
    {
        if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) && (!UIManager::Get()->GetOverlayDragger().IsDragActive()) && (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) )
        {
            UIManager::Get()->GetOverlayDragger().DragStart(GetOverlayHandle(), m_DragOrigin);
        }
        else if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) && (!UIManager::Get()->GetOverlayDragger().IsDragActive()) && (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) )
        {
            UIManager::Get()->GetOverlayDragger().DragGestureStart(GetOverlayHandle(), m_DragOrigin);
        }
    }

    ImGui::End();

    if (UIManager::Get()->IsInDesktopMode())
        ImGui::PopStyleVar(); //ImGuiStyleVar_Alpha
}

void FloatingWindow::OverlayStateSwitchCurrent(bool use_dashboard_tab)
{
    if (m_OverlayStateCurrent == &m_OverlayStateFading)
        return;

    //Use transition fade if the overlay position will change
    m_OverlayStatePending = (use_dashboard_tab) ? &m_OverlayStateDashboardTab : &m_OverlayStateRoom;

    if ( (m_OverlayStateCurrent->TransformAbs != m_OverlayStatePending->TransformAbs) || (!m_OverlayStatePending->IsVisible) )
    {
        m_IsTransitionFading = true;
        m_OverlayStateFading = *m_OverlayStateCurrent;
        m_OverlayStateCurrent = &m_OverlayStateFading;
    }
    else
    {
        OverlayStateSwitchFinish();
    }
}

void FloatingWindow::OverlayStateSwitchFinish()
{
    m_IsTransitionFading = false;
    m_OverlayStateCurrent = m_OverlayStatePending;
    m_OverlayStateCurrentID = (m_OverlayStateCurrent == &m_OverlayStateDashboardTab) ? floating_window_ovrl_state_dashboard_tab : floating_window_ovrl_state_room;

    ApplyCurrentOverlayState();

    if (m_OverlayStateCurrent->IsVisible)
    {
        Show();
    }
}

void FloatingWindow::OnWindowPinButtonPressed()
{
    //No-op
}

bool FloatingWindow::IsVirtualWindowItemHovered() const
{
    return false;
}

void FloatingWindow::HelpMarker(const char* desc, const char* marker_str) const
{
    ImGui::TextDisabled(marker_str);
    if (ImGui::IsItemHovered())
    {
        static float last_y_offset = FLT_MIN;       //Try to avoid getting having the tooltip cover the marker... the way it's done here is a bit messy to be fair

        const ImGuiStyle& style = ImGui::GetStyle();
        float pos_y = ImGui::GetItemRectMax().y + style.ItemSpacing.y;
        bool is_invisible = false;

        if (last_y_offset == FLT_MIN) //Same as IsWindowAppearing except the former doesn't work before beginning the window which is too late for the position...
        {
            //We need to create the tooltip window for size calculations to happen but also don't want to see it... so alpha 0, even if wasteful
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
            is_invisible = true;
        }
        else
        {
            ImGui::SetNextWindowPos(ImVec2(m_Pos.x, m_Pos.y + m_Size.y + last_y_offset), 0, {0.0f, 1.0f});
            ImGui::SetNextWindowSize(ImVec2(m_Size.x, -1.0f));
        }

        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(m_Size.x - style.WindowPadding.x);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();

        if (ImGui::IsWindowAppearing()) //New tooltip, reset offset
        {
            //The window size isn't available in this frame yet, so we'll have to skip the having it visible for one more frame and then act on it
            last_y_offset = FLT_MIN;
        }
        else
        {
            if (pos_y + ImGui::GetWindowSize().y > m_Pos.y + m_Size.y) //If it would cover the marker
            {
                last_y_offset = -m_Size.y + ImGui::GetWindowSize().y + ImGui::GetFontSize() + style.FramePadding.y * 2.0f;
            }
            else //Use normal pos
            {
                last_y_offset = 0.0f;
            }
        }

        ImGui::EndTooltip();

        if (is_invisible)
            ImGui::PopStyleVar(); //ImGuiStyleVar_Alpha

    }
}

bool FloatingWindow::TranslatedComboAnimated(const char* label, int& value, TRMGRStrID trstr_min, TRMGRStrID trstr_max) const
{
    bool ret = false;

    if (ImGui::BeginComboAnimated(label, TranslationManager::GetString( (TRMGRStrID)(trstr_min + value) ) ))
    {
        //Make use of the fact values and translation string IDs are laid out sequentially and shorten this to a nice loop
        const int value_max = (trstr_max - trstr_min) + 1;
        for (int i = 0; i < value_max; ++i)
        {
            ImGui::PushID(i);

            if (ImGui::Selectable(TranslationManager::GetString( (TRMGRStrID)(trstr_min + i) ), (value == i)))
            {
                value = i;
                ret = true;
            }

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    return ret;
}

void FloatingWindow::Update()
{
    WindowUpdateBase();
}

void FloatingWindow::UpdateVisibility()
{
    //Set state depending on dashboard tab visibility
    if ( (!UIManager::Get()->IsInDesktopMode()) && (!m_IsTransitionFading) )
    {
        const bool is_using_dashboard_state = (m_OverlayStateCurrentID == floating_window_ovrl_state_dashboard_tab);

        if (is_using_dashboard_state != vr::VROverlay()->IsOverlayVisible(UIManager::Get()->GetOverlayHandleDPlusDashboard()))
        {
            OverlayStateSwitchCurrent(!is_using_dashboard_state);
        }
    }

    //Overlay position and visibility
    if (UIManager::Get()->IsOpenVRLoaded())
    {
        vr::VROverlayHandle_t overlay_handle = GetOverlayHandle();

        if ( (m_OverlayStateCurrent->IsVisible) && (!m_OverlayStateCurrent->IsPinned) && (!UIManager::Get()->GetOverlayDragger().IsDragActive()) &&
             (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) && (!UIManager::Get()->IsDummyOverlayTransformUnstable()) )
        {
            vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;
            Matrix4 matrix_m4 = UIManager::Get()->GetOverlayDragger().GetBaseOffsetMatrix(ovrl_origin_dplus_tab) * m_OverlayStateCurrent->Transform;
            vr::HmdMatrix34_t matrix_ovr = matrix_m4.toOpenVR34();

            vr::VROverlay()->SetOverlayTransformAbsolute(overlay_handle, origin, &matrix_ovr);

            m_OverlayStateCurrent->TransformAbs = matrix_m4;
        }

        if ((!m_OvrlVisible) && (m_OverlayStateCurrent->IsVisible))
        {
            vr::VROverlay()->ShowOverlay(overlay_handle);
            m_OvrlVisible = true;
        }
        else if ((m_OvrlVisible) && (!m_OverlayStateCurrent->IsVisible) && (m_Alpha == 0.0f))
        {
            vr::VROverlay()->HideOverlay(overlay_handle);
            m_OvrlVisible = false;
        }
    }
}

void FloatingWindow::Show(bool skip_fade)
{
    m_OverlayStateCurrent->IsVisible = true;

    if (skip_fade)
    {
        m_Alpha = 1.0f;
    }
}

void FloatingWindow::Hide(bool skip_fade)
{
    m_OverlayStateCurrent->IsVisible = false;

    if (skip_fade)
    {
        m_Alpha = 0.0f;
    }
}

bool FloatingWindow::IsVisible() const
{
    return m_OverlayStateCurrent->IsVisible;
}

bool FloatingWindow::IsVisibleOrFading() const
{
    return ( (m_OverlayStateCurrent->IsVisible) || (m_Alpha != 0.0f) || (m_IsTransitionFading) );
}

float FloatingWindow::GetAlpha() const
{
    return m_Alpha;
}

bool FloatingWindow::IsPinned() const
{
    return m_OverlayStateCurrent->IsPinned;
}

void FloatingWindow::SetPinned(bool is_pinned)
{
    m_OverlayStateCurrent->IsPinned = is_pinned;

    if (!UIManager::Get()->IsOpenVRLoaded())
        return;

    if (!is_pinned)
    {
        RebaseTransform();
    }
    else
    {
        if (m_OverlayStateCurrentID == floating_window_ovrl_state_dashboard_tab)
        {
            m_OverlayStateRoom = m_OverlayStateDashboardTab;
        }
    }
}

FloatingWindowOverlayState& FloatingWindow::GetOverlayState(FloatingWindowOverlayStateID id)
{
    switch (id)
    {
        case floating_window_ovrl_state_room:          return m_OverlayStateRoom;
        case floating_window_ovrl_state_dashboard_tab: return m_OverlayStateDashboardTab;
    }

    return m_OverlayStateRoom;
}

FloatingWindowOverlayStateID FloatingWindow::GetOverlayStateCurrentID()
{
    return m_OverlayStateCurrentID;
}

Matrix4& FloatingWindow::GetTransform()
{
    return m_OverlayStateCurrent->Transform;
}

void FloatingWindow::SetTransform(const Matrix4& transform)
{
    m_OverlayStateCurrent->Transform = transform;

    //Store last absolute transform
    vr::HmdMatrix34_t hmd_mat;
    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

    vr::VROverlay()->GetOverlayTransformAbsolute(GetOverlayHandle(), &universe_origin, &hmd_mat);
    m_OverlayStateCurrent->TransformAbs = hmd_mat;

    //Store size multiplier
    float current_width = m_OvrlWidth;
    
    if (vr::VROverlay()->GetOverlayWidthInMeters(GetOverlayHandle(), &current_width) == vr::VROverlayError_None)
    {
        m_OverlayStateCurrent->Size = current_width / m_OvrlWidth;
    }
}

void FloatingWindow::ApplyCurrentOverlayState()
{
    if (!UIManager::Get()->IsOpenVRLoaded())
        return;

    if (m_OverlayStateCurrent->IsPinned)
    {
        vr::HmdMatrix34_t matrix_ovr = m_OverlayStateCurrent->TransformAbs.toOpenVR34();
        vr::VROverlay()->SetOverlayTransformAbsolute(GetOverlayHandle(), vr::TrackingUniverseStanding, &matrix_ovr);
    }

    vr::VROverlay()->SetOverlayWidthInMeters(GetOverlayHandle(), m_OvrlWidth * m_OverlayStateCurrent->Size);
}

void FloatingWindow::RebaseTransform()
{
    vr::HmdMatrix34_t hmd_mat;
    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

    vr::VROverlay()->GetOverlayTransformAbsolute(GetOverlayHandle(), &universe_origin, &hmd_mat);
    Matrix4 mat_abs = hmd_mat;
    Matrix4 mat_origin_inverse = UIManager::Get()->GetOverlayDragger().GetBaseOffsetMatrix(ovrl_origin_dplus_tab);

    mat_origin_inverse.invert();
    m_OverlayStateCurrent->Transform = mat_origin_inverse * mat_abs;
}

void FloatingWindow::ResetTransform()
{
    m_OverlayStateDashboardTab.Transform.identity();
    m_OverlayStateRoom.Transform.identity();

    m_OverlayStateRoom.IsVisible = false;
    m_OverlayStateRoom.IsPinned  = false;
}

const ImVec2& FloatingWindow::GetPos() const
{
    return m_Pos;
}

const ImVec2& FloatingWindow::GetSize() const
{
    return m_Size;
}
