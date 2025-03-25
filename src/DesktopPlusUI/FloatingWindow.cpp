#include "FloatingWindow.h"

#include "UIManager.h"
#include "OverlayManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "OpenVRExt.h"
#include "ImGuiExt.h"
#include "imgui_internal.h"

FloatingWindow::FloatingWindow() : m_OvrlWidth(1.0f),
                                   m_OvrlWidthMax(FLT_MAX),
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
                                   m_TitleBarMinWidth(64.0f),
                                   m_TitleBarTitleMaxWidth(-1.0f),
                                   m_TitleBarTitleIconAlpha(1.0f),
                                   m_IsTitleBarHovered(false),
                                   m_IsTitleIconClicked(false),
                                   m_HasAppearedOnce(false),
                                   m_IsWindowAppearing(false),
                                   m_CompactTableHeaderHeight(0.0f)
{
    m_Pos.x = FLT_MIN;
    m_WindowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    m_WindowID = "###" + std::to_string((unsigned long long)this);

    m_OverlayStateDashboardTab.TransformAbs.zero();
    m_OverlayStateRoom.TransformAbs.zero();
}

void FloatingWindow::WindowUpdateBase()
{
    if ( ((!UIManager::Get()->GetRepeatFrame()) || (m_Alpha == 0.0f)) && ((m_Alpha != 0.0f) || (m_OverlayStateCurrent->IsVisible) || (m_IsTransitionFading)) )
    {
        const float alpha_prev = m_Alpha;
        const float alpha_step = ImGui::GetIO().DeltaTime * 6.0f;

        //Alpha fade animation
        m_Alpha += ((m_OverlayStateCurrent->IsVisible) && (!m_IsTransitionFading)) ? alpha_step : -alpha_step;

        if (m_Alpha > 1.0f)
            m_Alpha = 1.0f;
        else if (m_Alpha < 0.0f)
            m_Alpha = 0.0f;

        //Use overlay alpha when not in desktop mode for better blending
        if ( (!UIManager::Get()->IsInDesktopMode()) && (alpha_prev != m_Alpha) )
            vr::VROverlay()->SetOverlayAlpha(GetOverlayHandle(), m_Alpha);

        if (m_Alpha == 0.0f)
        {
            //Not the best spot, but it can be difficult to cancel an active overlay hightlight when disappearing since the window code isn't running anymore
            //So instead we always cancel the current highlight here, which shouldn't be problematic
            UIManager::Get()->HighlightOverlay(k_ulOverlayID_None);

            //Finish transition fade if one's active
            if (m_IsTransitionFading)
            {
                OverlayStateSwitchFinish();
            }
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
    ImGui::SetNextWindowSizeConstraints({m_TitleBarMinWidth, 4.0f}, m_Size);
    ImGui::SetNextWindowScroll({0.0f, -1.0f}); //Prevent real horizontal scrolling from happening

    ImGuiWindowFlags flags = m_WindowFlags;

    if (!m_OverlayStateCurrent->IsVisible)
        flags |= ImGuiWindowFlags_NoInputs;

    ImGui::Begin(m_WindowID.c_str(), nullptr, flags);

    m_IsTitleBarHovered = ((ImGui::IsItemHovered()) && (m_OverlayStateCurrent->IsVisible)); //Current item is the title bar (needs to be checked before BeginTitleBar())
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

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_TitleBarTitleIconAlpha);

    ImGui::Image(io.Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);
    m_IsTitleIconClicked = ImGui::IsItemClicked();

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    ImVec2 clip_end = ImGui::GetCursorScreenPos();
    clip_end.x += m_TitleBarTitleMaxWidth;
    clip_end.y += ImGui::GetFrameHeight();

    ImGui::PushClipRect(ImGui::GetCursorScreenPos(), clip_end, true);
    ImGui::TextUnformatted( (m_WindowTitleStrID == tstr_NONE) ? m_WindowTitle.c_str() : TranslationManager::GetString(m_WindowTitleStrID) );
    ImGui::PopClipRect();

    ImGui::PopStyleVar();

    float title_text_width = ImGui::GetItemRectSize().x;

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

    if (ImGui::ImageButton("PinButton", io.Fonts->TexID, img_size, img_uv_min, img_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
    {
        SetPinned(!IsPinned());
        OnWindowPinButtonPressed();
    }
    ImGui::SameLine(0.0f, 0.0f);

    if (disable_pinning)
        ImGui::PopItemDisabled();

    TextureManager::Get().GetTextureInfo(tmtex_icon_xxsmall_close, img_size, img_uv_min, img_uv_max);

    if (ImGui::ImageButton("CloseButton", io.Fonts->TexID, img_size, img_uv_min, img_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
    {
        Hide();
        OnWindowCloseButtonPressed();
    }

    ImGui::EndGroup();

    m_IsTitleBarHovered = ( (m_IsTitleBarHovered) && (!ImGui::IsItemHovered()) ); //Title was hovered and no title bar element is hovered
    b_width = ImGui::GetItemRectSize().x;

    ImGui::PopStyleColor();

    //Calculate title bar constraints
    m_TitleBarMinWidth = img_size_line_height.x + b_width + (style.ItemSpacing.x * 2.0f);
    m_TitleBarTitleMaxWidth = ImGui::GetWindowSize().x - m_TitleBarMinWidth;
    m_TitleBarMinWidth += style.ItemSpacing.x * 2.0f;

    //Shorten title bar string if it doesn't fit (this is destructive, but doesn't matter for the windows using this)
    if ((m_WindowTitleStrID == tstr_NONE) && (title_text_width > std::max(ImGui::GetFontSize(), m_TitleBarTitleMaxWidth) ))
    {
        //Don't attempt to shorten the string during repeat frames as the size can still be adjusting in corner cases and throw us into a loop
        if (!UIManager::Get()->GetRepeatFrame())
        {
            m_WindowTitle = ImGui::StringEllipsis(m_WindowTitle.c_str(), m_TitleBarTitleMaxWidth);

            //Repeat frame to not make title shortening visible
            UIManager::Get()->RepeatFrame();
        }
    }

    //Title bar dragging
    if ( (m_OverlayStateCurrent->IsVisible) && (m_IsTitleBarHovered) && (UIManager::Get()->IsOpenVRLoaded()) )
    {
        if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) && (!UIManager::Get()->GetOverlayDragger().IsDragActive()) && (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) )
        {
            UIManager::Get()->GetOverlayDragger().DragStart(GetOverlayHandle(), m_DragOrigin);
            UIManager::Get()->GetOverlayDragger().DragSetMaxWidth(m_OvrlWidthMax);
        }
        else if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) && (!UIManager::Get()->GetOverlayDragger().IsDragActive()) && (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) )
        {
            UIManager::Get()->GetOverlayDragger().DragGestureStart(GetOverlayHandle(), m_DragOrigin);
            UIManager::Get()->GetOverlayDragger().DragSetMaxWidth(m_OvrlWidthMax);
        }
    }

    ImGui::EndTitleBar();

    //To appease boundary extension error check
    ImGui::Dummy({0.0f, 0.0f});
    ImGui::SameLine(0.0f, 0.0f);

    //Window content
    WindowUpdate();

    //Hack to work around ImGui's auto-sizing quirks. Just checking for ImGui::IsWindowAppearing() and using alpha 0 then doesn't help on its own so this is the next best thing
    if (!m_HasAppearedOnce)
    {
        if (ImGui::IsWindowAppearing())
        {
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
            UIManager::Get()->GetOverlayDragger().DragSetMaxWidth(m_OvrlWidthMax);
        }
        else if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) && (!UIManager::Get()->GetOverlayDragger().IsDragActive()) && (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) )
        {
            UIManager::Get()->GetOverlayDragger().DragGestureStart(GetOverlayHandle(), m_DragOrigin);
            UIManager::Get()->GetOverlayDragger().DragSetMaxWidth(m_OvrlWidthMax);
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
    //If pin button pressed from dashboard tab, also make it visible for the room state so it can be used there
    if ( (m_OverlayStateCurrentID == floating_window_ovrl_state_dashboard_tab) && (m_OverlayStateDashboardTab.IsPinned) )
    {
        m_OverlayStateRoom.IsVisible = true;
    }
}

void FloatingWindow::OnWindowCloseButtonPressed()
{
    //Do nothing by default
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
                if (UIManager::Get()->IsInDesktopMode())
                {
                    last_y_offset = -m_Size.y + ImGui::GetWindowSize().y + UIManager::Get()->GetDesktopModeWindow().GetTitleBarRect().w;
                }
                else
                {
                    last_y_offset = -m_Size.y + ImGui::GetWindowSize().y + ImGui::GetFontSize() + style.FramePadding.y * 2.0f;
                }
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

void FloatingWindow::UpdateLimiterSetting(bool is_override) const
{
    const ImGuiStyle& style = ImGui::GetStyle();

    const ConfigID_Int   configid_mode = (is_override) ? configid_int_overlay_update_limit_override_mode : configid_int_performance_update_limit_mode;
    const ConfigID_Int   configid_fps  = (is_override) ? configid_int_overlay_update_limit_override_fps  : configid_int_performance_update_limit_fps;
    const ConfigID_Float configid_ms   = (is_override) ? configid_float_overlay_update_limit_override_ms : configid_float_performance_update_limit_ms;

    int& update_limit_mode = ConfigManager::GetRef(configid_mode);
    bool limit_updates = (update_limit_mode != update_limit_mode_off);

    if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings)) //Advanced view, choose limiter mode
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString((is_override) ? tstr_SettingsPerformanceUpdateLimiterModeOverride : tstr_SettingsPerformanceUpdateLimiterMode));

        if (update_limit_mode == update_limit_mode_ms)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsPerformanceUpdateLimiterModeMSTip));
        }
        else if (is_override)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsPerformanceUpdateLimiterOverrideTip));
        }

        ImGui::NextColumn();

        //Manually set up combo items here to support different first string for override setting
        const TRMGRStrID combo_strings[] = 
        {
            (is_override) ? tstr_SettingsPerformanceUpdateLimiterModeOffOverride : tstr_SettingsPerformanceUpdateLimiterModeOff,
            tstr_SettingsPerformanceUpdateLimiterModeMS,
            tstr_SettingsPerformanceUpdateLimiterModeFPS
        };

        update_limit_mode = clamp(update_limit_mode, 0, IM_ARRAYSIZE(combo_strings) - 1);   //Avoid accessing past limits with invalid values

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginComboAnimated("##ComboUpdateLimitMode", TranslationManager::GetString(combo_strings[update_limit_mode]) ))
        {
            int i = 0;
            for (const auto& item : combo_strings)
            {
                if (ImGui::Selectable(TranslationManager::GetString(item), (update_limit_mode == i)))
                {
                    update_limit_mode = i;

                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_mode, update_limit_mode);
                }

                ++i;
            }

            ImGui::EndCombo();
        }

        ImGui::NextColumn();
        ImGui::NextColumn();
    }
    else //Simple view, only switch between off and fps mode (still shows ms below if previously set active)
    {
        if (ImGui::Checkbox(TranslationManager::GetString((is_override) ? tstr_SettingsPerformanceUpdateLimiterOverride : tstr_SettingsPerformanceUpdateLimiter), &limit_updates))
        {
            update_limit_mode = (limit_updates) ? update_limit_mode_fps : update_limit_mode_off;
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_mode, update_limit_mode);
        }

        if (is_override)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            HelpMarker(TranslationManager::GetString(tstr_SettingsPerformanceUpdateLimiterOverrideTip));
        }

        ImGui::NextColumn();
    }

    if (update_limit_mode == update_limit_mode_ms)
    {
        VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

        float& update_limit_ms = ConfigManager::Get().GetRef(configid_ms);

        vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("UpdateLimitMS") );
        if (ImGui::SliderWithButtonsFloat("UpdateLimitMS", update_limit_ms, 0.5f, 0.05f, 0.0f, 100.0f, "%.2f ms", ImGuiSliderFlags_Logarithmic))
        {
            if (update_limit_ms < 0.0f)
                update_limit_ms = 0.0f;

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_ms, update_limit_ms);
        }
        vr_keyboard.VRKeyboardInputEnd();
    }
    else
    {
        if (!limit_updates)
            ImGui::PushItemDisabled();

        int& update_limit_fps = ConfigManager::Get().GetRef(configid_fps);
        const int update_limit_fps_max = 9;
        if (ImGui::SliderWithButtonsInt("UpdateLimitFPS", update_limit_fps, 1, 1, 0, update_limit_fps_max, "##%d", ImGuiSliderFlags_NoInput, nullptr, 
                                        TranslationManager::Get().GetFPSLimitString(update_limit_fps)))
        {
            update_limit_fps = clamp(update_limit_fps, 0, update_limit_fps_max);

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_fps, update_limit_fps);
        }

        if (!limit_updates)
            ImGui::PopItemDisabled();
    }

    ImGui::NextColumn();
}

bool FloatingWindow::InputOverlayTags(const char* str_id, char* buffer_tags, size_t buffer_tags_size, FloatingWindowInputOverlayTagsState& state, int clip_parent_depth, bool show_auto_tags)
{
    static const int single_tag_buffer_size = IM_ARRAYSIZE(state.TagEditBuffer);

    ImGuiStyle& style = ImGui::GetStyle();

    float widget_width = 0.0f;
    bool is_widget_hovered = false;

    float child_height = ImGui::GetTextLineHeight() + style.FramePadding.y * 2.0f;
    child_height += ImGui::GetTextLineHeightWithSpacing() * (state.ChildHeightLines - 1.0f);

    ImGui::PushID(str_id);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
    if (ImGui::BeginChild("InputOverlayTags", {-style.ChildBorderSize, child_height}, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened))
    {
        ImGui::PopStyleVar();

        state.ChildHeightLines = 1.0f;
        widget_width = ImGui::GetWindowWidth();
        is_widget_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        //Split input string into individual tags and show a small button for each
        const char* str = buffer_tags;
        const char* str_end = str + strlen(str);
        const char* tag_start = str;
        const char* tag_end = nullptr;
        char buffer_single_tag[single_tag_buffer_size] = "";
        int tag_id = 0;

        while (tag_start < str_end)
        {
            tag_end = (const char*)memchr(tag_start, ' ', str_end - tag_start);

            if (tag_end == nullptr)
                tag_end = str_end;

            size_t length = tag_end - tag_start;

            //Break if tag doesn't fit (would be comically long though)
            if (length >= single_tag_buffer_size)
                break;

            if (length > 0)
            {
                memcpy(buffer_single_tag, tag_start, length);
                buffer_single_tag[length] = '\0';

                if (tag_id != 0)
                {
                    ImVec2 text_size = ImGui::CalcTextSize(buffer_single_tag);
                    text_size.x += style.ItemSpacing.x;

                    if (text_size.x > ImGui::GetContentRegionAvail().x)
                    {
                        ImGui::NewLine();
                        state.ChildHeightLines += 1.0f;
                    }
                }

                ImGui::PushID(tag_id);

                if (ImGui::SmallButton(buffer_single_tag))
                {
                    //Copy tag into edit buffer
                    memcpy(state.TagEditBuffer, buffer_single_tag, single_tag_buffer_size);

                    //Also keep a copy to put back when canceling
                    state.TagEditOrigStr = state.TagEditBuffer;

                    //Remove tag from full string
                    std::string str_tags = buffer_tags;
                    str_tags.erase(tag_start - str, length);
                    StringReplaceAll(str_tags, "  ", " ");               //Clean up double whitespace separators, no matter where they came from

                    //Cleanup stray space at the beginning too
                    if ((!str_tags.empty()) && (str_tags[0] == ' '))
                    {
                        str_tags.erase(0, 1);
                    }

                    //Copy back to buffer
                    size_t copied_length = str_tags.copy(buffer_tags, buffer_tags_size - 1);
                    buffer_tags[copied_length] = '\0';

                    //Open popup to allow editing
                    state.PopupShow = true;
                    state.FocusTextInput = true;

                    //We modified the buffer we're looping over, get out of here
                    UIManager::Get()->RepeatFrame(5);   //Prevent hover flicker from the button that take this one's place
                    ImGui::PopID();
                    ImGui::EndChild();
                    ImGui::PopID();

                    return true;
                }

                ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

                ImGui::PopID();
                tag_id++;
            }

            tag_start = tag_end + 1;
        }

        if (tag_id != 0)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

            ImVec2 text_size = ImGui::CalcTextSize("+");
            text_size.x += style.ItemSpacing.x;

            if (text_size.x > ImGui::GetContentRegionAvail().x)
            {
                ImGui::NewLine();
                state.ChildHeightLines += 1.0f;
            }
        }

        //There are two plus-shaped buttons on the screen when the popup is up.
        //This one will just clear the text input then and might be accidentally pressed instead of the bigger one. Disable it to prevent accidents
        //Similar thing applies to the tag buttons themselves, but that behavior is at least useful for editing there
        const bool disable_add = state.PopupShow;
        if (disable_add)
        {
            ImGui::PushItemDisabledNoVisual();
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
    
        //Add some frame padding in VR mode only to have a more square-ish button that is also easier to hit
        //Due to how we do style-scaling this isn't necessary in desktop mode
        if (!UIManager::Get()->IsInDesktopMode())
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {style.FramePadding.x * 1.5f, 0.0f});

        if (ImGui::SmallButton("+##AddTag"))
        {
            state.TagEditBuffer[0] = '\0';
            state.TagEditOrigStr = state.TagEditBuffer;

            state.PopupShow = true;
            state.FocusTextInput = true;
        }

        if (!UIManager::Get()->IsInDesktopMode())
            ImGui::PopStyleVar();

        if (disable_add)
        {
            ImGui::PopStyleColor();
            ImGui::PopItemDisabledNoVisual();
        }
    }
    else
    {
        ImGui::PopStyleVar();   //ImGuiStyleVar_WindowPadding
    }

    //Avoid scrollbar flicker while figuring out child window size
    if ((ImGui::IsAnyScrollBarVisible()) && (state.ChildHeightLines <= 3.0f))
    {
        UIManager::Get()->RepeatFrame();
    }

    //Show up to 3 lines before adding a scrollbar
    state.ChildHeightLines = std::min(state.ChildHeightLines, 3.0f);

    ImGui::EndChild();

    //--Popup Window
    if (!state.PopupShow)
    {
        ImGui::PopID();
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    const float pos_y      = ImGui::GetItemRectMin().y - style.ItemSpacing.y;
    const float pos_y_down = ImGui::GetItemRectMax().y + style.ItemInnerSpacing.y;
    const float pos_y_up   = ImGui::GetItemRectMin().y - style.ItemSpacing.y - state.PopupHeight;

    bool update_filter = false;
    bool ret = false;

    //Wait for window height to be known and stable before setting pos or animating fade/pos
    if ((state.PopupHeight != FLT_MIN) && (state.PopupHeight == state.PopupHeightPrev))
    {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, smoothstep(state.PosAnimationProgress, pos_y_down, pos_y_up) ));

        const float time_step = ImGui::GetIO().DeltaTime * 6.0f;

        state.PopupAlpha += (!state.IsFadingOut) ? time_step : -time_step;

        if (state.PopupAlpha > 1.0f)
            state.PopupAlpha = 1.0f;
        else if (state.PopupAlpha < 0.0f)
            state.PopupAlpha = 0.0f;

        state.PosAnimationProgress += (state.PosDir == ImGuiDir_Up) ? time_step : -time_step;

        if (state.PosAnimationProgress > 1.0f)
            state.PosAnimationProgress = 1.0f;
        else if (state.PosAnimationProgress < 0.0f)
            state.PosAnimationProgress = 0.0f;
    }
    else if (state.PopupHeight == FLT_MIN)   //Popup is appearing
    {
        state.KnownTagsList = OverlayManager::Get().GetKnownOverlayTagList();
        update_filter = true;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, state.PopupAlpha);

    //Look up parent window (optionally digging deeper to avoid child windows) and get its clipping rect
    ImGuiWindow* window_parent = ImGui::GetCurrentWindow();
    ImGuiWindow* window_parent_lookup = window_parent;

    while (clip_parent_depth > 0)
    {
        window_parent_lookup = window_parent_lookup->ParentWindow;
        clip_parent_depth--;

        if (window_parent_lookup != nullptr)
        {
            window_parent = window_parent_lookup;
        }
        else
        {
            break;
        }
    }

    ImRect clip_rect = window_parent->ClipRect;
    clip_rect.Max.y = window_parent->Size.y + clip_rect.Min.y;   //Set Max.y from window content size as FLT_MAX values break drawlist commands for some reason

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | 
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground;

    ImGui::SetNextWindowSize(ImVec2(widget_width, ImGui::GetTextLineHeightWithSpacing() * 11.5f));
    ImGui::Begin("##WindowAddTags", nullptr, flags);


    //Transfer scroll input to parent window (which isn't a real parent window but just the one in the stack), so this window doesn't block scrolling
    ImGui::ScrollBeginStackParentWindow();

    //Use clipping rect of parent window
    ImGui::PushClipRect(clip_rect.Min, clip_rect.Max, false);

    //Draw background + border manually so it can be clipped properly
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImRect window_rect = window->Rect();
    window->DrawList->AddRectFilled(window_rect.Min, window_rect.Max, ImGui::GetColorU32(ImGuiCol_PopupBg), window->WindowRounding);
    window->DrawList->AddRect(window_rect.Min, window_rect.Max, ImGui::GetColorU32(ImGuiCol_Border), window->WindowRounding, 0, window->WindowBorderSize);

    //Disable inputs when fading out
    const bool disable_items = state.IsFadingOut;
    if (disable_items)
        ImGui::PushItemDisabledNoVisual();

    //-Window contents
    static float buttons_width = 0.0f;
    const bool is_input_text_active = ImGui::IsAnyInputTextActive(); //Need to get this before InputText is canceled in the same frame
    bool add_current_input = false;

    if (state.FocusTextInput)
    {
        ImGui::SetKeyboardFocusHere();
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttons_width - style.ItemInnerSpacing.x);

    //Set up shortcut window so it does not block the tag listing itself either. It's a little bit awkward looking like this, but better than blocking the buttons
    vr_keyboard.SetShortcutWindowDirectionHint(ImGuiDir_Up, (state.PosDir == ImGuiDir_Down) ? -child_height - style.ItemSpacing.y - style.ItemSpacing.y : -style.ItemInnerSpacing.y);
    vr_keyboard.VRKeyboardInputBegin("##InputTagEdit");
    if (ImGui::InputTextWithHint("##InputTagEdit", TranslationManager::GetString(tstr_DialogInputTagsHint), state.TagEditBuffer, single_tag_buffer_size, 
                                 ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        add_current_input = !state.IsTagAlreadyInBuffer;
    }
    vr_keyboard.VRKeyboardInputEnd();

    //Wait until the actually have focus before turning the flag off
    if (ImGui::IsItemActive())
    {
        state.FocusTextInput = false;
    }

    //Check if tag would be a duplicate and disable adding in that case
    if (ImGui::IsItemEdited())
    {
        state.IsTagAlreadyInBuffer = OverlayManager::MatchOverlayTagSingle(buffer_tags, state.TagEditBuffer);
        update_filter = true;

        UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(state.TagEditBuffer);
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    ImGui::BeginGroup();

    ImVec2 b_size, b_uv_min, b_uv_max;
    ImVec2 b_size_real = ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight());
    TextureManager::Get().GetTextureInfo(tmtex_icon_add, b_size, b_uv_min, b_uv_max);

    if (state.IsTagAlreadyInBuffer)
        ImGui::PushItemDisabled();

    if (ImGui::ImageButton("AddButton", io.Fonts->TexID, b_size_real, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
    {
        add_current_input = true;
    }

    if (state.IsTagAlreadyInBuffer)
        ImGui::PopItemDisabled();

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    TextureManager::Get().GetTextureInfo(tmtex_icon_small_close, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton("DeleteButton", io.Fonts->TexID, b_size_real, b_uv_min, b_uv_max, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
    {
        state.TagEditBuffer[0] = '\0';
        state.TagEditOrigStr = "";
        state.IsTagAlreadyInBuffer = true;
        update_filter = true;
    }

    ImGui::EndGroup();

    buttons_width = ImGui::GetItemRectSize().x;

    ImGui::BeginChild("ChildKnownTags", ImVec2(0.0f, 0.0f), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);

    for (const auto& list_entry : state.KnownTagsList)
    {
        if (state.KnownTagsFilter.PassFilter(list_entry.Tag.c_str()))
        {
            if ((list_entry.IsAutoTag) && (!show_auto_tags))
                continue;

            if (list_entry.IsAutoTag)
                ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextNotification);

            if (ImGui::Selectable(list_entry.Tag.c_str()))
            {
                size_t copied_length = list_entry.Tag.copy(state.TagEditBuffer, buffer_tags_size - 1);
                state.TagEditBuffer[copied_length] = '\0';

                add_current_input = true;
                state.TagEditOrigStr = "";
            }

            if (list_entry.IsAutoTag)
                ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();

    if (disable_items)
        ImGui::PopItemDisabledNoVisual();

    //Abandoned popup while editing existing tag, put it back
    if ((state.IsFadingOut) && (!state.TagEditOrigStr.empty()))
    {
        size_t copied_length = state.TagEditOrigStr.copy(state.TagEditBuffer, buffer_tags_size - 1);
        state.TagEditBuffer[copied_length] = '\0';

        state.TagEditOrigStr = "";
        add_current_input = true;
        update_filter = true;
    }

    if (add_current_input)
    {
        //Check if tag is already in the string first and just don't add it then
        if (!OverlayManager::MatchOverlayTagSingle(buffer_tags, state.TagEditBuffer))
        {
            std::string str_tags = buffer_tags;

            if (!str_tags.empty())
            {
                str_tags += " ";
            }

            str_tags += state.TagEditBuffer;

            size_t copied_length = str_tags.copy(buffer_tags, buffer_tags_size - 1);
            buffer_tags[copied_length] = '\0';
        }

        state.TagEditOrigStr = "";

        ret = true;
        state.IsFadingOut = true;
        UIManager::Get()->RepeatFrame();
    }

    if (update_filter)
    {
        //Update filter manually (we don't use its buffer directly as its size is fixed to 256)
        size_t length = strlen(state.TagEditBuffer);
        if (length < (size_t)IM_ARRAYSIZE(state.KnownTagsFilter.InputBuf))
        {
            memcpy(state.KnownTagsFilter.InputBuf, state.TagEditBuffer, length);
            state.KnownTagsFilter.InputBuf[length] = '\0';

            state.KnownTagsFilter.Build();
        }
    }

    //Switch directions if there's no space in the default direction
    if (state.PosDirDefault == ImGuiDir_Down)
    {
        state.PosDir = (pos_y_down + ImGui::GetWindowSize().y > clip_rect.Max.y) ? ImGuiDir_Up : ImGuiDir_Down;
    }
    else
    {
        //Not using pos_y_up here as it's not valid yet (state.PopupHeight not set) 
        state.PosDir = (pos_y - ImGui::GetWindowSize().y < clip_rect.Min.y) ? ImGuiDir_Down : ImGuiDir_Up;
    }

    if (state.PopupAlpha == 0.0f)
    {
        state.PosAnimationProgress = (state.PosDir == ImGuiDir_Down) ? 0.0f : 1.0f;
    }

    //Fade out on focus loss/cancel input
    if ( ((!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) && (!ImGui::IsAnyItemActive())) ||
         ((!is_input_text_active) && (ImGui::IsNavInputPressed(ImGuiNavInput_Cancel))) )
    {
        state.IsFadingOut = true;
    }

    //Cache window height so it's available on the next frame before beginning the window
    state.PopupHeightPrev = state.PopupHeight;
    state.PopupHeight = ImGui::GetWindowSize().y;

    ImGui::End();

    ImGui::PopStyleVar(); //ImGuiStyleVar_Alpha

    //Reset when fade-out is done
    if ( (state.IsFadingOut) && (state.PopupAlpha == 0.0f) )
    {
        state.IsFadingOut = false;
        state.PopupHeight = FLT_MIN;
        state.PosDir = state.PosDirDefault;
        state.PosAnimationProgress = (state.PosDirDefault == ImGuiDir_Down) ? 0.0f : 1.0f;
        state.PopupShow = false;
    }

    ImGui::PopID();

    return ret;
}

bool FloatingWindow::ActionOrderList(ActionManager::ActionList& list_actions_target, bool is_appearing, bool is_returning, FloatingWindowActionOrderListState& state, 
                                     bool& go_add_actions, float height_offset)
{
    static float list_buttons_width = 0.0f;
    static ImVec2 no_actions_text_size;

    const ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();
    ActionManager& action_manager = ConfigManager::Get().GetActionManager();

    if ((is_appearing) || (is_returning))
    {
        state.HasSavedChanges = false;
        state.SelectedIndex = -1;

        state.ActionsList.clear();
        for (ActionUID uid : list_actions_target)
        {
            state.ActionsList.push_back({uid, action_manager.GetTranslatedName(uid)});
        }
    }

    if (is_appearing)
    {
        state.ActionListOrig = list_actions_target;
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsActionsOrderHeader)); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFrameHeight() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 16.0f : 14.0f;

    ImGui::BeginChild("ActionList", ImVec2(0.0f, (item_height * item_count) + inner_padding + height_offset), true);

    if ((is_appearing) || (is_returning))
    {
        ImGui::SetScrollY(0.0f);
    }

    //Display error if there are no actions
    if (state.ActionsList.size() == 0)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x / 2.0f - (no_actions_text_size.x / 2.0f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y / 2.0f - (no_actions_text_size.y / 2.0f));

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SettingsActionsOrderNoActions));
        no_actions_text_size = ImGui::GetItemRectSize();
    }
    else
    {
        ActionUID hovered_action_prev = state.HoveredAction;

        //List actions
        int index = 0;
        for (const auto& entry : state.ActionsList)
        {
            ImGui::PushID((void*)entry.UID);

            //Set focus for nav if we previously re-ordered overlays via keyboard
            if (state.KeyboardSwappedIndex == index)
            {
                ImGui::SetKeyboardFocusHere();

                //Nav works against us here, so keep setting focus until ctrl isn't down anymore
                if ((!io.KeyCtrl) || (!io.NavVisible))
                {
                    state.KeyboardSwappedIndex = -1;
                }
            }

            ImGui::SetNextItemAllowOverlap();
            if (ImGui::Selectable(entry.Name.c_str(), (index == state.SelectedIndex), ImGuiSelectableFlags_AllowOverlap))
            {
                state.SelectedIndex = index;
            }

            if ( (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenOverlappedByItem)) || ((io.NavVisible) && (ImGui::IsItemFocused())) )
            {
                state.HoveredAction = entry.UID;
            }

            if (ImGui::IsItemVisible())
            {
                //Drag reordering
                if ((ImGui::IsItemActive()) && (!ImGui::IsItemHovered()))
                {
                    int index_swap = index + ((ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y < 0.0f) ? -1 : 1);
                    if ((state.HoveredAction != entry.UID) && (index_swap >= 0) && (index_swap < state.ActionsList.size()))
                    {
                        std::iter_swap(state.ActionsList.begin()   + index, state.ActionsList.begin()   + index_swap);
                        std::iter_swap(list_actions_target.begin() + index, list_actions_target.begin() + index_swap);
                        state.SelectedIndex = index_swap;

                        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                    }
                }

                //Keyboard reordering
                if ((io.NavVisible) && (io.KeyCtrl) && (state.HoveredAction == entry.UID))
                {
                    int index_swap = index + ((ImGui::IsNavInputPressed(ImGuiNavInput_DpadDown, true)) ? 1 : (ImGui::IsNavInputPressed(ImGuiNavInput_DpadUp, true)) ? -1 : 0);
                    if ((index != index_swap) && (index_swap >= 0) && (index_swap < state.ActionsList.size()))
                    {
                        std::iter_swap(state.ActionsList.begin()   + index, state.ActionsList.begin()   + index_swap);
                        std::iter_swap(list_actions_target.begin() + index, list_actions_target.begin() + index_swap);

                        //Skip the rest of this frame to avoid double-swaps
                        state.KeyboardSwappedIndex = index_swap;
                        ImGui::PopID();
                        UIManager::Get()->RepeatFrame();
                        break;
                    }
                }
            }

            ImGui::PopID();

            index++;
        }

        //Reduce flicker from dragging and hovering
        if (state.HoveredAction != hovered_action_prev)
        {
            UIManager::Get()->RepeatFrame();
        }
    }

    ImGui::EndChild();
    ImGui::Unindent();

    const bool is_none  = (state.SelectedIndex == -1);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - list_buttons_width);

    ImGui::BeginGroup();

    go_add_actions = ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsOrderAdd));

    ImGui::SameLine();

    if (is_none)
        ImGui::PushItemDisabled();

    if ((ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsOrderRemove))) || (ImGui::IsKeyPressed(ImGuiKey_Delete)))
    {
        if ((state.SelectedIndex >= 0) && (state.SelectedIndex < state.ActionsList.size()))
        {
            state.ActionsList.erase(state.ActionsList.begin()     + state.SelectedIndex);
            list_actions_target.erase(list_actions_target.begin() + state.SelectedIndex);

            if (state.SelectedIndex >= (int)list_actions_target.size())
            {
                state.SelectedIndex--;
            }
        }
    }

    if (is_none)
        ImGui::PopItemDisabled();

    ImGui::EndGroup();

    list_buttons_width = ImGui::GetItemRectSize().x + style.IndentSpacing;

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons
    ImGui::Separator();

    bool ret = false;
    if (ImGui::Button(TranslationManager::GetString(tstr_DialogOk))) 
    {
        ret = true;
        state.HasSavedChanges = true;
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        ret = true;
        list_actions_target = state.ActionListOrig;
    }

    return ret;
}

bool FloatingWindow::ActionAddSelector(ActionManager::ActionList& list_actions_target, bool is_appearing, FloatingWindowActionAddSelectorState& state, float height_offset)
{
    static float list_buttons_width = 0.0f;
    static ImVec2 no_actions_text_size;

    const ImGuiStyle& style = ImGui::GetStyle();
    ActionManager& action_manager = ConfigManager::Get().GetActionManager();

    if (is_appearing)
    {
        state.ActionsList = action_manager.GetActionNameList();

        //Remove actions already in the existing list
        auto it = std::remove_if(state.ActionsList.begin(), state.ActionsList.end(), 
                                 [&](const auto& entry) { return (std::find(list_actions_target.begin(), list_actions_target.end(), entry.UID) != list_actions_target.end()); } );

        state.ActionsList.erase(it, state.ActionsList.end());

        state.ActionsTickedList.resize(state.ActionsList.size(), 0);
        std::fill(state.ActionsTickedList.begin(), state.ActionsTickedList.end(), 0);
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_SettingsActionsAddSelectorHeader)); 
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    const float item_height = ImGui::GetFrameHeight() + style.ItemSpacing.y;
    const float inner_padding = style.FramePadding.y + style.ItemInnerSpacing.y;
    const float item_count = (UIManager::Get()->IsInDesktopMode()) ? 16.0f : 14.0f;

    ImGui::BeginChild("ActionSelector", ImVec2(0.0f, (item_height * item_count) + inner_padding + height_offset), true);

    //Reset scroll when appearing
    if (is_appearing)
    {
        ImGui::SetScrollY(0.0f);
    }

    //Display error if there are no actions
    if (state.ActionsList.size() == 0)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x / 2.0f - (no_actions_text_size.x / 2.0f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y / 2.0f - (no_actions_text_size.y / 2.0f));

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_DialogActionPickerEmpty));
        no_actions_text_size = ImGui::GetItemRectSize();
    }
    else
    {
        //List actions
        const float cursor_x_past_checkbox = ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing();
        
        int index = 0;
        for (const auto& entry : state.ActionsList)
        {
            ImGui::PushID(index);

            //We're using a trick here to extend the checkbox interaction space to the end of the child window
            //Checkbox() uses the item inner spacing if the label is not blank, so we increase that and use a space label
            //Below we render a custom label after adjusting the cursor position to where it normally would be
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {ImGui::GetContentRegionAvail().x, style.ItemInnerSpacing.y});

            if (ImGui::Checkbox(" ", (bool*)&state.ActionsTickedList[index]))
            {
                //Update any ticked status
                state.IsAnyActionTicked = false;
                for (auto is_ticked : state.ActionsTickedList)
                {
                    if (is_ticked != 0)
                    {
                        state.IsAnyActionTicked = true;
                        break;
                    }
                }
            }

            ImGui::PopStyleVar();

            if (ImGui::IsItemVisible())
            {
                //Adjust cursor position to be after the checkbox
                ImGui::SameLine();
                float text_y = ImGui::GetCursorPosY();
                ImGui::SetCursorPos({cursor_x_past_checkbox, text_y});

                ImGui::TextUnformatted(entry.Name.c_str());
            }

            ImGui::PopID();

            index++;
        }
    }

    ImGui::EndChild();
    ImGui::Unindent();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - list_buttons_width);

    ImGui::BeginGroup();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileAddSelectAll)))
    {
        std::fill(state.ActionsTickedList.begin(), state.ActionsTickedList.end(), 1);
        state.IsAnyActionTicked = true;
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsProfilesOverlaysProfileAddSelectNone)))
    {
        std::fill(state.ActionsTickedList.begin(), state.ActionsTickedList.end(), 0);
        state.IsAnyActionTicked = false;
    }
    ImGui::EndGroup();

    list_buttons_width = ImGui::GetItemRectSize().x + style.IndentSpacing;

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //Confirmation buttons
    ImGui::Separator();

    bool ret = false;
    if (ImGui::Button(TranslationManager::GetString(tstr_SettingsActionsAddSelectorAdd))) 
    {
        //Add ticked actions to the existing list
        int index = 0;
        for (const auto& entry : state.ActionsList)
        {
            if (state.ActionsTickedList[index])
            {
                list_actions_target.push_back(entry.UID);
            }

            index++;
        }

        ret = true;
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        ret = true;
    }

    return ret;
}

bool FloatingWindow::BeginCompactTable(const char* str_id, int column, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width)
{
    const ImGuiStyle style = ImGui::GetStyle();

    //There's minor breakage at certain fractional scales, but the ones we care about (100%, 160% (VR), 200%) work fine with this
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,      {style.CellPadding.x,     -1.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {style.ItemInnerSpacing.x, 0.0f});

    flags = flags & (~ImGuiTableFlags_BordersOuter);    //Remove border flag, we draw our own later
    flags |= ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX;
    bool ret = ImGui::BeginTable(str_id, column, flags, outer_size, inner_width);

    if (ret)
    {
        m_CompactTableHeaderHeight = ImGui::GetCursorPosY();
    }
    else
    {
        ImGui::PopStyleVar(2);
    }

    return ret;
}

void FloatingWindow::CompactTableHeadersRow()
{
    ImGui::PushItemDisabledNoVisual();
    ImGui::TableHeadersRow();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
    ImGui::Dummy({0.0f, 0.0f});         //To appease parent boundary extension error check
    ImGui::PopItemDisabledNoVisual();

    m_CompactTableHeaderHeight = ImGui::GetCursorPosY() - m_CompactTableHeaderHeight - ImGui::GetStyle().ItemSpacing.y;
}

void FloatingWindow::EndCompactTable()
{
    const ImGuiStyle style = ImGui::GetStyle();

    ImGui::EndTable();
    ImGui::PopStyleVar(2);

    //Selectables cover parts of the default table border and the bottom border would be one pixel inside the last row, so we draw our own header and table border on top instead
    ImVec2 rect_min = ImGui::GetItemRectMin(), rect_max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRect(rect_min, {rect_max.x, rect_min.y + ceilf(m_CompactTableHeaderHeight - 1.0f)}, ImGui::GetColorU32(ImGuiCol_Border), 0.0f, 0, style.WindowBorderSize);
    ImGui::GetWindowDrawList()->AddRect(rect_min, {rect_max.x, rect_max.y + 1.0f},                                     ImGui::GetColorU32(ImGuiCol_Border), 0.0f, 0, style.WindowBorderSize);
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
             (!UIManager::Get()->GetOverlayDragger().IsDragGestureActive()) )
        {
            //We don't update position when the dummy transform is unstable to avoid flicker, but we absolutely need to update it when the overlay is about to appear
            if ( (!m_OvrlVisible) || (!UIManager::Get()->IsDummyOverlayTransformUnstable()) )
            { 
                vr::TrackingUniverseOrigin origin = vr::TrackingUniverseStanding;
                Matrix4 matrix_m4 = UIManager::Get()->GetOverlayDragger().GetBaseOffsetMatrix(ovrl_origin_dplus_tab) * m_OverlayStateCurrent->Transform;
                vr::HmdMatrix34_t matrix_ovr = matrix_m4.toOpenVR34();

                vr::VROverlay()->SetOverlayTransformAbsolute(overlay_handle, origin, &matrix_ovr);

                m_OverlayStateCurrent->TransformAbs = matrix_m4;
            }
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

    UIManager::Get()->GetIdleState().AddActiveTime();
}

void FloatingWindow::Hide(bool skip_fade)
{
    m_OverlayStateCurrent->IsVisible = false;

    if (skip_fade)
    {
        m_Alpha = 0.0f;
    }

    UIManager::Get()->GetIdleState().AddActiveTime();
}

void FloatingWindow::HideAll(bool skip_fade)
{
    Hide(skip_fade);

    m_OverlayStateRoom.IsVisible         = false;
    m_OverlayStateDashboardTab.IsVisible = false;
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

void FloatingWindow::ApplyUIScale()
{
    m_Size.x = m_SizeUnscaled.x * UIManager::Get()->GetUIScale();
    m_Size.y = m_SizeUnscaled.y * UIManager::Get()->GetUIScale();
}

bool FloatingWindow::CanUnpinRoom() const
{
    return m_AllowRoomUnpinning;
}

bool FloatingWindow::IsPinned() const
{
    return m_OverlayStateCurrent->IsPinned;
}

void FloatingWindow::SetPinned(bool is_pinned, bool no_state_copy)
{
    m_OverlayStateCurrent->IsPinned = is_pinned;

    if (!UIManager::Get()->IsOpenVRLoaded())
        return;

    if (!is_pinned)
    {
        RebaseTransform();
    }
    else if (!no_state_copy)
    {
        if (m_OverlayStateCurrentID == floating_window_ovrl_state_dashboard_tab)
        {
            m_OverlayStateRoom.IsPinned     = m_OverlayStateDashboardTab.IsPinned;
            m_OverlayStateRoom.Transform    = m_OverlayStateDashboardTab.Transform;
            m_OverlayStateRoom.TransformAbs = m_OverlayStateDashboardTab.TransformAbs;
        }

        //Reset transform if TransformAbs doesn't have anything of value yet
        if (m_OverlayStateRoom.TransformAbs.isZero())
        {
            ResetTransform(floating_window_ovrl_state_room);
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

void FloatingWindow::ResetTransformAll()
{
    ResetTransform(floating_window_ovrl_state_dashboard_tab);
    ResetTransform(floating_window_ovrl_state_room);

    m_OverlayStateRoom.IsVisible = false;
    m_OverlayStateRoom.IsPinned  = false;
}

void FloatingWindow::ResetTransform(FloatingWindowOverlayStateID state_id)
{
    GetOverlayState(state_id).Transform.identity();

    //Set absolute transform to the dashboard tab one (as if pinning)
    if (state_id == floating_window_ovrl_state_room)
    {
        if (!m_OverlayStateDashboardTab.TransformAbs.isZero())
        {
            m_OverlayStateRoom.TransformAbs = m_OverlayStateDashboardTab.TransformAbs;
        }
        else if ((UIManager::Get() != nullptr) && (UIManager::Get()->IsOpenVRLoaded())) //If the dashboard tab transform is still zero, generate a HMD facing transform instead (needs startup to be done though)
        {
            m_OverlayStateRoom.TransformAbs = vr::IVRSystemEx::ComputeHMDFacingTransform(1.25f);
            UIManager::Get()->GetOverlayDragger().ApplyDashboardScale(m_OverlayStateRoom.TransformAbs);
        }
    }
}

void FloatingWindow::StartDrag()
{
    if (UIManager::Get()->IsOpenVRLoaded())
    {
        OverlayDragger& overlay_dragger = UIManager::Get()->GetOverlayDragger();

        if ( (!overlay_dragger.IsDragActive()) && (!overlay_dragger.IsDragGestureActive()) )
        {
            overlay_dragger.DragStart(GetOverlayHandle(), m_DragOrigin);
            overlay_dragger.DragSetMaxWidth(m_OvrlWidthMax);
        }
    }
}

const ImVec2& FloatingWindow::GetPos() const
{
    return m_Pos;
}

const ImVec2& FloatingWindow::GetSize() const
{
    return m_Size;
}

bool FloatingWindow::TranslatedComboAnimated(const char* label, int& value, TRMGRStrID trstr_min, TRMGRStrID trstr_max)
{
    bool ret = false;
    const char* preview_value = ((trstr_min + value >= trstr_min) && (trstr_min + value <= trstr_max)) ? TranslationManager::GetString( (TRMGRStrID)(trstr_min + value) ) : "???";

    if (ImGui::BeginComboAnimated(label, preview_value))
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
