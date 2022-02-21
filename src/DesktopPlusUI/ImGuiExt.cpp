#include "ImGuiExt.h"

#include <string>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
    #define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"
#include "UIManager.h"
#include "Util.h"

ImVec4 Style_ImGuiCol_TextNotification;
ImVec4 Style_ImGuiCol_TextWarning;
ImVec4 Style_ImGuiCol_TextError;
ImVec4 Style_ImGuiCol_ButtonPassiveToggled;
ImVec4 Style_ImGuiCol_SteamVRCursor;
ImVec4 Style_ImGuiCol_SteamVRCursorBorder;

namespace ImGui
{
    //Like InputFloat()'s buttons but with a slider instead. Not quite as flexible, though. Always takes as much space as available.
    bool SliderWithButtonsFloat(const char* str_id, float& value, float step, float step_small, float min, float max, const char* format, ImGuiSliderFlags flags, bool* used_button, const char* text_alt)
    {
        //Hacky solution to make right mouse enable text input on the slider while not touching ImGui code or generalizing it as ctrl press
        ImGuiIO& io = ImGui::GetIO();
        const bool mouse_left_clicked_old = io.MouseClicked[ImGuiMouseButton_Left];
        const bool key_ctrl_old = io.KeyCtrl;

        if (io.MouseDown[ImGuiMouseButton_Right])
        {
            io.MouseClicked[ImGuiMouseButton_Left] = true;
            io.KeyCtrl = true;
            io.KeyMods |= ImGuiKeyModFlags_Ctrl; //KeyMods needs to stay consistent with KeyCtrl
        }

        //Use small step value when shift is down
        if (io.KeyShift)
        {
            step = step_small;
        }

        ImGuiStyle& style = ImGui::GetStyle();

        const float value_old = value;
        const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        ImGui::BeginGroup();

        ImGui::PushID(str_id);
        ImGui::PushButtonRepeat(true);

        //Calulate slider width (GetContentRegionAvail() returns 1 more than when using -1 width to fill)
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2) - 1.0f);
        ImGui::SliderFloat("##Slider", &value, min, max, format, flags);

        if (text_alt != nullptr)
        {
            ImGui::RenderTextClipped(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), text_alt, nullptr, nullptr, ImVec2(0.5f, 0.5f));
        }

        bool has_slider_deactivated = false;
        if (ImGui::IsItemDeactivated())
        {
            has_slider_deactivated = true;
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("-", button_size))
        {
            //Round to the step value while we're at it. This may not be the most expected thing at first, but it helps a lot to get the usually preferred even values
            int step_count = (int)ceilf(value / step);

            value = step_count * step;
            value -= step;

            if ( int( ceilf(value / step) ) >= step_count ) //Welcome to floating point math, this can happen
                value -= step / 10000.f;                    //This works for what we need, but not quite elegant indeed

            if (used_button)
                *used_button = true;
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("+", button_size))
        {
            int step_count = int(value / step);

            value = step_count * step;
            value += step;

            if ( int(value / step) <= step_count ) //See above
                value += step / 10000.f;

            if (used_button)
                *used_button = true;
        }

        ImGui::PopButtonRepeat();
        ImGui::PopID();

        ImGui::EndGroup();

        //Deactivated flag for the slider gets swallowed up somewhere, but we really need it for the VR keyboard, so we tape it back on here
        if (has_slider_deactivated)
        {
            ImGui::GetCurrentContext()->LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDeactivated | ImGuiItemStatusFlags_Deactivated;
        }

        //We generally don't want -0.0 to be a thing, so prevent it
        if (value == -0.0f)
            value = 0.0f;

        //Restore hack
        io.MouseClicked[ImGuiMouseButton_Left] = mouse_left_clicked_old;
        io.KeyCtrl = key_ctrl_old;
        if (!io.KeyCtrl)
        {
            io.KeyMods &= ~ImGuiKeyModFlags_Ctrl;
        }

        return (value != value_old);
    }

    bool SliderWithButtonsInt(const char* str_id, int& value, int step, int step_small, int min, int max, const char* format, ImGuiSliderFlags flags, bool* used_button, const char* text_alt)
    {
        //Hacky solution to make right mouse enable text input on the slider while not touching ImGui code or generalizing it as ctrl press
        ImGuiIO& io = ImGui::GetIO();
        const bool mouse_left_clicked_old = io.MouseClicked[ImGuiMouseButton_Left];
        const bool key_ctrl_old = io.KeyCtrl;

        if (io.MouseDown[ImGuiMouseButton_Right])
        {
            io.MouseClicked[ImGuiMouseButton_Left] = true;
            io.KeyCtrl = true;
            io.KeyMods |= ImGuiKeyModFlags_Ctrl; //KeyMods needs to stay consistent with KeyCtrl
        }

        //Use small step value when shift is down
        if (io.KeyShift)
        {
            step = step_small;
        }

        ImGuiStyle& style = ImGui::GetStyle();

        const int value_old = value;
        const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        ImGui::BeginGroup();

        ImGui::PushID(str_id);
        ImGui::PushButtonRepeat(true);

        //Calulate slider width (GetContentRegionAvail() returns 1 more than when using -1 width to fill)
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2) - 1.0f);
        ImGui::SliderInt("##Slider", &value, min, max, format, flags);

        if (text_alt != nullptr)
        {
            ImGui::RenderTextClipped(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), text_alt, nullptr, nullptr, ImVec2(0.5f, 0.5f));
        }

        bool has_slider_deactivated = false;
        if (ImGui::IsItemDeactivated())
        {
            has_slider_deactivated = true;
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("-", button_size))
        {
            //Also rounding to steps here
            value = (int)ceilf((float)value / step) * step;
            value -= step;

            if (used_button)
                *used_button = true;
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("+", button_size))
        {
            value = (value / step) * step;
            value += step;

            if (used_button)
                *used_button = true;
        }

        ImGui::PopButtonRepeat();
        ImGui::PopID();

        ImGui::EndGroup();

        //Deactivated flag for the slider gets swallowed up somewhere, but we really need it for the VR keyboard, so we tape it back on here
        if (has_slider_deactivated)
        {
            ImGui::GetCurrentContext()->LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDeactivated | ImGuiItemStatusFlags_Deactivated;
        }

        //Restore hack
        io.MouseClicked[ImGuiMouseButton_Left] = mouse_left_clicked_old;
        io.KeyCtrl = key_ctrl_old;
        if (!io.KeyCtrl)
        {
            io.KeyMods &= ~ImGuiKeyModFlags_Ctrl;
        }

        return (value != value_old);
    }

    bool SliderWithButtonsFloatPercentage(const char* str_id, float& value, int step, int step_small, int min, int max, const char* format, ImGuiSliderFlags flags, bool* used_button, const char* text_alt)
    {
        int value_ui = int(value * 100.0f);

        if (ImGui::SliderWithButtonsInt(str_id, value_ui, step, step_small, min, max, format, flags, used_button, text_alt))
        {
            value = value_ui / 100.0f;

            //Floating point hell hacky fix (slider can get stuck when using + button otherwise)
            int intvalue = int(value * 100.0f), intvalue_prev = intvalue;
            while (intvalue < value_ui) 
            {
                value += step / 10000.f;

                intvalue_prev = intvalue;
                intvalue = int(value * 100.0f);

                if (intvalue == intvalue_prev) //Sanity check to avoid endless loop at big value + small step combinations
                {
                    break;
                }
            }

            return true;
        }

        return false;
    }

    ImGuiID SliderWithButtonsGetSliderID(const char* str_id)
    {
        ImGui::PushID(str_id);
        ImGuiID id = ImGui::GetID("##Slider");
        ImGui::PopID();

        return id;
    }

    //Like imgui_demo's HelpMarker, but with a fixed position tooltip
    void FixedHelpMarker(const char* desc, const char* marker_str)
    {
        ImGui::TextDisabled(marker_str);
        if (ImGui::IsItemHovered())
        {
            static float last_y_offset = FLT_MIN;       //Try to avoid getting the tooltip off-screen... the way it's done here is a bit messy to be fair

            float pos_y = ImGui::GetItemRectMin().y;
            bool is_invisible = false;

            if (last_y_offset == FLT_MIN) //Same as IsWindowAppearing except the former doesn't work before beginning the window which is too late for the position...
            {
                //We need to create the tooltip window for size calculations to happen but also don't want to see it... so alpha 0, even if wasteful
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
                is_invisible = true;
            }
            else
            {
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemInnerSpacing.x, ImGui::GetItemRectMin().y + last_y_offset));
            }

            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::PushTextWrapPos(ImGui::GetIO().DisplaySize.x - ImGui::GetCursorScreenPos().x);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::PopTextWrapPos();

            if (ImGui::IsWindowAppearing()) //New tooltip, reset offset
            {
                //The window size isn't available in this frame yet, so we'll have to skip the having it visible for one more frame and then act on it
                last_y_offset = FLT_MIN;
            }
            else
            {
                if (pos_y + ImGui::GetWindowSize().y > ImGui::GetIO().DisplaySize.y) //If it would be partially off-screen
                {
                    last_y_offset = ImGui::GetIO().DisplaySize.y - (pos_y + ImGui::GetWindowSize().y);
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

    bool ButtonWithWrappedLabel(const char* label, const ImVec2& size)
    {
        //This could probably be solved in a cleaner way, but it's not /that/ dirty
        ImGui::PushID(label);
        ImGui::BeginGroup();

        ImGuiWindow* window = GetCurrentWindow();
        const ImGuiStyle& style = ImGui::GetStyle();

        bool ret = ImGui::Button("", ImVec2(size.x + (style.FramePadding.x * 2.0f), size.y + (style.FramePadding.y * 2.0f)) );
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - size.x - style.FramePadding.x);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.FramePadding.y);
        
        //Prevent child window moving line pos
        float cursor_max_pos_prev_y = window->DC.CursorMaxPos.y;

        ImGui::BeginChild("ButtonLabel", ImVec2(size.x + style.FramePadding.x, size.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);

        ImVec2 text_size = ImGui::CalcTextSize(label, nullptr, false, size.x);
        ImGui::SetCursorPosX( (size.x / 2.0f) - int(text_size.x / 2.0f) );
        ImGui::SetCursorPosY( (size.x / 2.0f) - int(text_size.y / 2.0f) );

        ImGui::PushTextWrapPos(size.x);
        ImGui::TextUnformatted(label);
        ImGui::PopTextWrapPos();

        ImGui::EndChild();

        window->DC.CursorMaxPos.y = cursor_max_pos_prev_y;

        ImGui::EndGroup();
        ImGui::PopID();

        return ret;
    }

    bool BeginComboWithInputText(const char* str_id, char* str_buffer, size_t buffer_size, bool& out_buffer_changed, bool& persist_input_visible,
                                 bool& persist_input_activated, bool& persist_mouse_released_once, bool no_preview_text)
    {
        ImGuiContext& g = *GImGui;

        out_buffer_changed = false;

        if (persist_input_visible)
        {
            ImGui::PushID("InputText");

            g.NextItemData.Width  = ImGui::CalcItemWidth();
            g.NextItemData.Width -= ImGui::GetFrameHeight();

            if ((ImGui::InputText(str_id, str_buffer, buffer_size)))
            {
                out_buffer_changed = true;
            }

            ImGuiID input_text_id = ImGui::GetItemID();

            if ( (persist_input_activated) && (persist_mouse_released_once) && (ImGui::PopupContextMenuInputText(str_id, str_buffer, buffer_size)) )
            {
                out_buffer_changed = true;
            }

            if (!persist_input_activated)
            {
                ImGui::ActivateItem(ImGui::GetItemID());
                persist_input_activated = true;
            }
            else if ( (!ImGui::IsPopupOpen(str_id)) && ( (ImGui::IsItemDeactivated()) || (g.ActiveId != input_text_id) ) )
            {
                persist_input_visible = false;
                persist_input_activated = false;
                persist_mouse_released_once = false;
                UIManager::Get()->RepeatFrame();
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            {
                persist_mouse_released_once = true;
            }

            ImGui::SameLine(0.0f, 0.0f);

            ImGui::PopID();
        }

        return (ImGui::BeginCombo(str_id, (no_preview_text) ? "" : str_buffer, (persist_input_visible) ? (ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft) : ImGuiComboFlags_None));
    }

    void ComboWithInputTextActivationCheck(bool& persist_input_visible)
    {
        ImGuiContext& g = *GImGui;

        //Right-click or Ctrl+Left-click to edit
        if ( (ImGui::IsItemClicked(ImGuiMouseButton_Right)) || ((ImGui::IsItemClicked(ImGuiMouseButton_Left)) && g.IO.KeyCtrl) )
        {
            persist_input_visible = true;
        }
    }

    static float CalcMaxPopupHeightFromItemCount(int items_count)
    {
        ImGuiContext& g = *GImGui;
        if (items_count <= 0)
            return FLT_MAX;
        return (g.FontSize + g.Style.ItemSpacing.y) * items_count - g.Style.ItemSpacing.y + (g.Style.WindowPadding.y * 2);
    }

    bool BeginComboAnimated(const char* label, const char* preview_value, ImGuiComboFlags flags)
    {
        //-
        //Custom code sections marked with //-
        //Otherwise exactly the same as ImGui::BeginCombo() of ImGui v1.82
        static float animation_progress = 0.0f;
        static float scrollbar_alpha = 0.0f;
        float popup_height = 0.0f;
        //-

        // Always consume the SetNextWindowSizeConstraint() call in our early return paths
        ImGuiContext& g = *GImGui;
        bool has_window_size_constraint = (g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint) != 0;
        g.NextWindowData.Flags &= ~ImGuiNextWindowDataFlags_HasSizeConstraint;

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together

        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const float arrow_size = (flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        const float expected_w = CalcItemWidth();
        const float w = (flags & ImGuiComboFlags_NoPreview) ? arrow_size : expected_w;
        const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
        const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id, &frame_bb))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(frame_bb, id, &hovered, &held);
        bool popup_open = IsPopupOpen(id, ImGuiPopupFlags_None);

        const ImU32 frame_col = GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
        const float value_x2 = ImMax(frame_bb.Min.x, frame_bb.Max.x - arrow_size);
        RenderNavHighlight(frame_bb, id);
        if (!(flags & ImGuiComboFlags_NoPreview))
            window->DrawList->AddRectFilled(frame_bb.Min, ImVec2(value_x2, frame_bb.Max.y), frame_col, style.FrameRounding, (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
        if (!(flags & ImGuiComboFlags_NoArrowButton))
        {
            ImU32 bg_col = GetColorU32((popup_open || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
            ImU32 text_col = GetColorU32(ImGuiCol_Text);
            window->DrawList->AddRectFilled(ImVec2(value_x2, frame_bb.Min.y), frame_bb.Max, bg_col, style.FrameRounding, (w <= arrow_size) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersRight);
            if (value_x2 + arrow_size - style.FramePadding.x <= frame_bb.Max.x)
                RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, frame_bb.Min.y + style.FramePadding.y), text_col, ImGuiDir_Down, 1.0f);
        }
        RenderFrameBorder(frame_bb.Min, frame_bb.Max, style.FrameRounding);
        if (preview_value != NULL && !(flags & ImGuiComboFlags_NoPreview))
        {
            ImVec2 preview_pos = frame_bb.Min + style.FramePadding;
            if (g.LogEnabled)
                LogSetNextTextDecoration("{", "}");
            RenderTextClipped(preview_pos, ImVec2(value_x2, frame_bb.Max.y), preview_value, NULL, NULL, ImVec2(0.0f, 0.0f));
        }
        if (label_size.x > 0)
            RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

        if ((pressed || g.NavActivateId == id) && !popup_open)
        {
            if (window->DC.NavLayerCurrent == 0)
                window->NavLastIds[0] = id;
            OpenPopupEx(id, ImGuiPopupFlags_None);
            popup_open = true;

            //-
            //Reset animation
            animation_progress = 0.0f;
            scrollbar_alpha = 0.0f;
            //-
        }

        if (!popup_open)
            return false;

        //-
        ImVec2 clip_min = ImGui::GetCursorScreenPos();
        clip_min.y -= style.FramePadding.y + style.PopupBorderSize;
        //-

        if (has_window_size_constraint)
        {
            g.NextWindowData.Flags |= ImGuiNextWindowDataFlags_HasSizeConstraint;
            g.NextWindowData.SizeConstraintRect.Min.x = ImMax(g.NextWindowData.SizeConstraintRect.Min.x, w);
        }
        else
        {
            if ((flags & ImGuiComboFlags_HeightMask_) == 0)
                flags |= ImGuiComboFlags_HeightRegular;
            IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiComboFlags_HeightMask_));    // Only one
            int popup_max_height_in_items = -1;
            if (flags & ImGuiComboFlags_HeightRegular)     popup_max_height_in_items = 8;
            else if (flags & ImGuiComboFlags_HeightSmall)  popup_max_height_in_items = 4;
            else if (flags & ImGuiComboFlags_HeightLarge)  popup_max_height_in_items = 20;
            SetNextWindowSizeConstraints(ImVec2(w, 0.0f), ImVec2(FLT_MAX, CalcMaxPopupHeightFromItemCount(popup_max_height_in_items)));
        }

        char name[16];
        ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g.BeginPopupStack.Size); // Recycle windows based on depth

        // Position the window given a custom constraint (peak into expected window size so we can position it)
        // This might be easier to express with an hypothetical SetNextWindowPosConstraints() function.
        if (ImGuiWindow* popup_window = FindWindowByName(name))
            if (popup_window->WasActive)
            {
                // Always override 'AutoPosLastDirection' to not leave a chance for a past value to affect us.
                ImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
                if (flags & ImGuiComboFlags_PopupAlignLeft)
                    popup_window->AutoPosLastDirection = ImGuiDir_Left; // "Below, Toward Left"
                else
                    popup_window->AutoPosLastDirection = ImGuiDir_Down; // "Below, Toward Right (default)"
                ImRect r_outer = GetPopupAllowedExtentRect(popup_window);
                ImVec2 pos = FindBestWindowPosForPopupEx(frame_bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, frame_bb, ImGuiPopupPositionPolicy_ComboBox);


                //-
                //Animated popup scrolling out of the widget
                if (animation_progress < 1.0f)
                {
                    animation_progress += ImGui::GetIO().DeltaTime * 5.0f;
                }
                else
                {
                    animation_progress = 1.0f;
                    scrollbar_alpha = std::min(scrollbar_alpha + (ImGui::GetIO().DeltaTime * 5.0f), 1.0f);
                }

                popup_height = IM_ROUND(smoothstep(animation_progress, 0.0f, size_expected.y)); //popup_height grows to full size in animation

                //Offset position by animation progress
                pos.y += (popup_window->AutoPosLastDirection == ImGuiDir_Down) ? -size_expected.y + popup_height : size_expected.y - popup_height;
                //-

                SetNextWindowPos(pos);
            }

        //-
        //Hide background and border while animating since they don't get clipped (they're drawn manually below)
        if (animation_progress != 1.0f)
        {
            ImGui::PushStyleColor(ImGuiCol_Border, 0);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, 0);
        }

        //Fade-in scrollbar colors
        ImVec4 col;
        for (int i = ImGuiCol_ScrollbarBg; i <= ImGuiCol_ScrollbarGrabActive; ++i)
        {
            col = style.Colors[i];
            col.w *= scrollbar_alpha;
            ImGui::PushStyleColor(i, col);
        }
        //-

        // We don't use BeginPopupEx() solely because we have a custom name string, which we could make an argument to BeginPopupEx()
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;

        //-
        if (scrollbar_alpha == 0.0f)
        {
            window_flags |= ImGuiWindowFlags_NoScrollbar; //Disable scrollbar when not visible so it can't be clicked accidentally
        }
        //-

        // Horizontally align ourselves with the framed text
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.FramePadding.x, style.WindowPadding.y));
        bool ret = Begin(name, NULL, window_flags);
        PopStyleVar();
        if (!ret)
        {
            EndPopup();
            IM_ASSERT(0);   // This should never happen as we tested for IsPopupOpen() above
            return false;
        }

        //-
        ImGui::PopStyleColor(4); //Scrollbar colors

        if (animation_progress != 1.0f)
        {
            ImGui::PopStyleColor(2); //Border and PopupBg

            ImGuiWindow* popup_window = GetCurrentWindow();

            clip_min.x = popup_window->Pos.x;
            ImVec2 clip_max = clip_min;
            clip_max.x += popup_window->Size.x;

            //Popup open direction
            if (popup_window->AutoPosLastDirection == ImGuiDir_Down)
            {
                clip_max.y += popup_height + style.PopupBorderSize;

                ImGui::PushClipRect(clip_min, clip_max, false);

                clip_max.y -= style.PopupBorderSize;
                popup_window->DrawList->AddRectFilled(clip_min, clip_max, GetColorU32(ImGuiCol_PopupBg));
                popup_window->DrawList->AddRect(clip_min, clip_max, GetColorU32(ImGuiCol_Border));

                clip_min.y += 1;
            }
            else //ImGuiDir_Up
            {
                clip_min.y -= frame_bb.GetHeight() + popup_height + style.PopupBorderSize;
                clip_max.y -= frame_bb.GetHeight();

                ImGui::PushClipRect(clip_min, clip_max, false);

                clip_min.y += style.PopupBorderSize;
                popup_window->DrawList->AddRectFilled(clip_min, clip_max, GetColorU32(ImGuiCol_PopupBg));
                popup_window->DrawList->AddRect(clip_min, clip_max, GetColorU32(ImGuiCol_Border));
            }

            ImGui::PopClipRect();

            //Push the clip rect for the window content... this doesn't get popped anywhere, but that seems to be harmless
            ImGui::PushClipRect(GetCurrentWindow()->InnerClipRect.Min, GetCurrentWindow()->InnerClipRect.Max, false);
            ImGui::PushClipRect(clip_min, clip_max, true);
        }
        //-

        return true;
    }

    void TextRight(float offset_x, const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        TextRightV(offset_x, fmt, args);
        va_end(args);
    }

    void TextRightV(float offset_x, const char* fmt, va_list args)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImGuiContext& g = *GImGui;
        const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

        ImVec2 size = ImGui::CalcTextSize(g.TempBuffer, text_end);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - size.x - offset_x));

        TextEx(g.TempBuffer, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
    }

    void TextRightUnformatted(float offset_x, const char* text, const char* text_end)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImVec2 size = ImGui::CalcTextSize(text, text_end);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - size.x - offset_x));

        TextEx(text, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
    }

    void TextColoredUnformatted(const ImVec4& col, const char* text, const char* text_end)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(text, text_end); 
        ImGui::PopStyleColor();
    }

    bool ColorEdit4Simple(const char* label, float col[4], ImGuiColorEditFlags flags)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        //Hacky solution to make right mouse enable text input on the slider while not touching ImGui code or generalizing it as ctrl press
        ImGuiIO& io = ImGui::GetIO();
        const bool mouse_left_clicked_old = io.MouseClicked[ImGuiMouseButton_Left];
        const bool key_ctrl_old = io.KeyCtrl;

        if (io.MouseDown[ImGuiMouseButton_Right])
        {
            io.MouseClicked[ImGuiMouseButton_Left] = true;
            io.KeyCtrl = true;
            io.KeyMods |= ImGuiKeyModFlags_Ctrl; //KeyMods needs to stay consistent with KeyCtrl
        }

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const float square_sz = GetFrameHeight();
        const float w_full = CalcItemWidth();
        const float w_button = (flags & ImGuiColorEditFlags_NoSmallPreview) ? 0.0f : (square_sz + style.ItemInnerSpacing.x);
        const float w_inputs = w_full - w_button;
        const char* label_display_end = FindRenderedTextEnd(label);
        g.NextItemData.ClearFlags();

        BeginGroup();
        PushID(label);

        const bool alpha = (flags & ImGuiColorEditFlags_NoAlpha) == 0;

        bool value_changed = false;
        bool value_changed_as_float = false;

        const ImVec2 pos = window->DC.CursorPos;
        const float inputs_offset_x = (style.ColorButtonPosition == ImGuiDir_Left) ? w_button : 0.0f;
        window->DC.CursorPos.x = pos.x + inputs_offset_x;

        ImGuiWindow* picker_active_window = NULL;
        if (!(flags & ImGuiColorEditFlags_NoSmallPreview))
        {
            window->DC.CursorPos = ImVec2(pos.x, pos.y);

            const ImVec4 col_v4(col[0], col[1], col[2], alpha ? col[3] : 1.0f);
            if (ColorButton("##ColorButton", col_v4, flags))
            {
                if (!(flags & ImGuiColorEditFlags_NoPicker))
                {
                    // Store current color and open a picker
                    g.ColorPickerRef = col_v4;
                    OpenPopup("picker");
                    SetNextWindowPos(g.LastItemData.Rect.GetBL() + ImVec2(-1, style.ItemSpacing.y));
                }
            }

            //Centered and no move is better for the VR use-case
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ImGui::GetStyle().WindowRounding);

            ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (BeginPopup("picker", ImGuiWindowFlags_NoMove))
            {
                picker_active_window = g.CurrentWindow;
                if (label != label_display_end)
                {
                    TextEx(label, label_display_end);
                    Spacing();
                }
                ImGuiColorEditFlags picker_flags_to_forward = ImGuiColorEditFlags_DataTypeMask_ | ImGuiColorEditFlags_PickerMask_ | ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop;
                ImGuiColorEditFlags picker_flags = (flags & picker_flags_to_forward) | ImGuiColorEditFlags_DisplayMask_ | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreview;
                SetNextItemWidth(square_sz * 12.0f); // Use 256 + bar sizes?
                value_changed |= ColorPicker4("##picker", col, picker_flags, &g.ColorPickerRef.x);

                //Hack: Catch active drag&drop and cancel in to disable that feature
                //ImGuiColorEditFlags_NoDragDrop does not get forwarded in ColorPicker4() but we don't want to copy the whole function for a small change
                if (g.DragDropActive)
                {
                    ImGui::ClearDragDrop();
                    ImGui::ClearActiveID();
                }

                EndPopup();
            }

            ImGui::PopStyleVar();
        }

        if (label != label_display_end && !(flags & ImGuiColorEditFlags_NoLabel))
        {
            const float text_offset_x = (flags & ImGuiColorEditFlags_NoInputs) ? w_button : w_full + style.ItemInnerSpacing.x;
            window->DC.CursorPos = ImVec2(pos.x + text_offset_x, pos.y + style.FramePadding.y);
            TextEx(label, label_display_end);
        }

        PopID();
        EndGroup();

        // When picker is being actively used, use its active id so IsItemActive() will function on ColorEdit4().
        if (picker_active_window && g.ActiveId != 0 && g.ActiveIdWindow == picker_active_window)
            g.LastItemData.ID = g.ActiveId;

        if (value_changed)
            MarkItemEdited(g.LastItemData.ID);

        //Restore hack
        io.MouseClicked[ImGuiMouseButton_Left] = mouse_left_clicked_old;
        io.KeyCtrl = key_ctrl_old;
        if (!io.KeyCtrl)
        {
            io.KeyMods &= ~ImGuiKeyModFlags_Ctrl;
        }

        return value_changed;
    }

    void PushItemDisabled()
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * style.DisabledAlpha);
    }

    void PopItemDisabled()
    {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }

    void PushItemDisabledNoVisual()
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    }

    void PopItemDisabledNoVisual()
    {
        ImGui::PopItemFlag();
    }

    bool PopupContextMenuInputText(const char* str_id, char* str_buffer, size_t buffer_size, bool paste_remove_newlines)
    {
        bool ret = false;

        if (ImGui::BeginPopupContextItem(str_id))
        {
            if (ImGui::MenuItem("Copy all"))
            {
                ImGui::SetClipboardText(str_buffer);
            }

            if (ImGui::MenuItem("Replace with Clipboard", nullptr, false, (ImGui::GetClipboardText() != nullptr) ))
            {
                std::string str(ImGui::GetClipboardText());

                if (paste_remove_newlines)
                {
                    //Remove newlines (all kinds, since you never know what might be in a clipboard)
                    std::string newlines[] = {"\r\n", "\n", "\r"};

                    for (auto& nline : newlines)
                    {
                        size_t start_pos = 0;
                        while ((start_pos = str.find(nline, start_pos)) != std::string::npos)
                        {
                            str.replace(start_pos, nline.length(), " ");
                        }
                    }
                }

                //Copy to buffer
                size_t copied_length = str.copy(str_buffer, buffer_size - 1);
                str_buffer[copied_length] = '\0';

                ret = true;
            }

            ImGui::EndPopup();
        }

        return ret;
    }

    float ImGui::GetPreviousLineHeight()
    {
        return GImGui->CurrentWindow->DC.PrevLineSize.y;
    }

    void SetPreviousLineHeight(float height)
    {
        GImGui->CurrentWindow->DC.PrevLineSize.y = height;
    }

    bool HasHoveredNewItem()
    {
        const ImGuiContext& g = *GImGui;
        bool blocked_by_active_item = (g.ActiveId != 0 && !g.ActiveIdAllowOverlap);

        return ( (g.HoveredId != g.HoveredIdPreviousFrame) && (g.HoveredId != 0) && (!g.HoveredIdDisabled) && (!blocked_by_active_item) );
    }

    bool IsAnyItemActiveOrDeactivated()
    {
        const ImGuiContext& g = *GImGui;
        return ( (g.ActiveId != 0) || (g.ActiveIdPreviousFrame != 0) );
    }

    bool IsAnyInputTextActive()
    {
        const ImGuiContext& g = *GImGui;
        return ( (g.ActiveId != 0) && (g.ActiveId == g.InputTextState.ID) );
    }

    bool IsAnyMouseClicked()
    {
        const ImGuiContext& g = *GImGui;
        for (int n = 0; n < IM_ARRAYSIZE(g.IO.MouseDown); n++)
            if (g.IO.MouseClicked[n])
                return true;
        return false;
    }

    void HScrollWindowFromMouseWheelV()
    {
        ImGuiContext& g = *GImGui;

        //Don't do anything is real hscroll is active
        if ( (g.IO.MouseWheelH != 0.0f && !g.IO.KeyShift) || (g.IO.MouseWheel != 0.0f && g.IO.KeyShift) )
            return;

        ImGuiWindow* window = ImGui::GetCurrentWindow();

        if (GImGui->WheelingWindow != window)
            return;

        const float wheel_x = GImGui->IO.MouseWheel;
        float max_step = window->InnerRect.GetWidth() * 0.67f;
        float scroll_step = ImFloor(ImMin(2 * window->CalcFontSize(), max_step));

        ImGui::SetScrollX(window, window->Scroll.x - wheel_x * scroll_step);
    }

    bool IsAnyScrollBarVisible()
    {
        ImGuiWindow* window = ImGui::GetCurrentWindowRead();
        return ((window->ScrollbarX) || (window->ScrollbarY));
    }

    ImVec4 BeginTitleBar()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        ImRect rect = ImGui::GetCurrentWindowRead()->TitleBarRect();
        ImGui::SetCursorScreenPos({rect.Min.x + style.WindowBorderSize + style.FramePadding.x, rect.Min.y + style.FramePadding.y});

        ImGui::PushClipRect(rect.Min, rect.Max, false);

        return ImVec4(rect.Min.x, rect.Min.y, rect.Max.x, rect.Max.y);
    }

    void EndTitleBar()
    {
        ImGui::PopClipRect();

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGui::SetCursorScreenPos(window->ContentRegionRect.Min);
        window->DC.CursorMaxPos.x = 0;                              //Title bar does *not* affect overall window width as it messes with auto-resize
    }

    bool StringContainsUnmappedCharacter(const char* str)
    {
        if (GImGui == nullptr)
            return true;

        //Use current style font if possible, otherwise fall back to default font
        ImFont* font = (GImGui->FontStack.empty()) ? ImGui::GetDefaultFont() : GImGui->FontStack.back();

        if (font == nullptr)
            return true;

        const char* str_end = str + strlen(str);
        ImWchar32 c;
        int decoded_length;

        while (str < str_end)
        {
            decoded_length = ImTextCharFromUtf8(&c, str, str_end);

            if ( (c >= ' ') && (font->FindGlyphNoFallback((ImWchar)c) == nullptr) ) //Don't return true on unprintable characters being unmapped
            {
                return true;
            }

            str += decoded_length;
        }

        return false;
    }

    std::string StringEllipsis(const char* str, float width_max)
    {
        //Naive approach, but we're not supposed to use RenderTextEllipsis(), so this will do
        const char* str_begin = str;
        const char* str_end   = str + strlen(str);

        //Handle corner case of text fitting without ellipsis, but not with
        if (ImGui::CalcTextSize(str, str_end).x <= width_max)
        {
            return str;
        }

        float ellipsis_width = ImGui::CalcTextSize("...").x;
        bool is_full_string = true;
        ImWchar32 c;
        int decoded_length = 0;

        while (str < str_end)
        {
            if (ImGui::CalcTextSize(str_begin, str).x + ellipsis_width >= width_max)
            {
                str -= decoded_length;
                is_full_string = false;
                break;
            }

            decoded_length = ImTextCharFromUtf8(&c, str, str_end);

            str += decoded_length;
        }

        std::string str_out(str_begin, str - str_begin);

        if (!is_full_string)
        {
            str_out.append("...");
        }

        return str_out;
    }


    ImGuiMouseState::ImGuiMouseState()
    {
        //Set everything to zero
        memset(this, 0, sizeof(*this));
        MousePos = MousePosPrev = ImVec2(-FLT_MAX,-FLT_MAX);
    }

    void ImGuiMouseState::SetFromGlobalState()
    {
        const ImGuiIO& io = ImGui::GetIO();

        MousePos     = io.MousePos;
        std::copy(std::begin(io.MouseDown), std::end(io.MouseDown), MouseDown);
        MouseWheel   = io.MouseWheel;
        MouseWheelH  = io.MouseWheelH;
        MouseDelta   = io.MouseDelta;
        MousePosPrev = io.MousePosPrev;
        std::copy(std::begin(io.MouseClickedPos),                std::end(io.MouseClickedPos),                MouseClickedPos);
        std::copy(std::begin(io.MouseClickedTime),               std::end(io.MouseClickedTime),               MouseClickedTime);
        std::copy(std::begin(io.MouseClicked),                   std::end(io.MouseClicked),                   MouseClicked);
        std::copy(std::begin(io.MouseDoubleClicked),             std::end(io.MouseDoubleClicked),             MouseDoubleClicked);
        std::copy(std::begin(io.MouseReleased),                  std::end(io.MouseReleased),                  MouseReleased);
        std::copy(std::begin(io.MouseDownOwned),                 std::end(io.MouseDownOwned),                 MouseDownOwned);
        std::copy(std::begin(io.MouseDownOwnedUnlessPopupClose), std::end(io.MouseDownOwnedUnlessPopupClose), MouseDownOwnedUnlessPopupClose);
        std::copy(std::begin(io.MouseDownWasDoubleClick),        std::end(io.MouseDownWasDoubleClick),        MouseDownWasDoubleClick);
        std::copy(std::begin(io.MouseDownDuration),              std::end(io.MouseDownDuration),              MouseDownDuration);
        std::copy(std::begin(io.MouseDownDurationPrev),          std::end(io.MouseDownDurationPrev),          MouseDownDurationPrev);
        std::copy(std::begin(io.MouseDragMaxDistanceAbs),        std::end(io.MouseDragMaxDistanceAbs),        MouseDragMaxDistanceAbs);
        std::copy(std::begin(io.MouseDragMaxDistanceSqr),        std::end(io.MouseDragMaxDistanceSqr),        MouseDragMaxDistanceSqr);
    }

    void ImGuiMouseState::ApplyToGlobalState()
    {
        ImGuiIO& io = ImGui::GetIO();

        io.MousePos     = MousePos;
        std::copy(std::begin(MouseDown), std::end(MouseDown), io.MouseDown);
        io.MouseWheel   = MouseWheel;
        io.MouseWheelH  = MouseWheelH;
        io.MouseDelta   = MouseDelta;
        io.MousePosPrev = MousePosPrev;
        std::copy(std::begin(MouseClickedPos),                std::end(MouseClickedPos),                io.MouseClickedPos);
        std::copy(std::begin(MouseClickedTime),               std::end(MouseClickedTime),               io.MouseClickedTime);
        std::copy(std::begin(MouseClicked),                   std::end(MouseClicked),                   io.MouseClicked);
        std::copy(std::begin(MouseDoubleClicked),             std::end(MouseDoubleClicked),             io.MouseDoubleClicked);
        std::copy(std::begin(MouseReleased),                  std::end(MouseReleased),                  io.MouseReleased);
        std::copy(std::begin(MouseDownOwned),                 std::end(MouseDownOwned),                 io.MouseDownOwned);
        std::copy(std::begin(MouseDownOwnedUnlessPopupClose), std::end(MouseDownOwnedUnlessPopupClose), io.MouseDownOwnedUnlessPopupClose);
        std::copy(std::begin(MouseDownWasDoubleClick),        std::end(MouseDownWasDoubleClick),        io.MouseDownWasDoubleClick);
        std::copy(std::begin(MouseDownDuration),              std::end(MouseDownDuration),              io.MouseDownDuration);
        std::copy(std::begin(MouseDownDurationPrev),          std::end(MouseDownDurationPrev),          io.MouseDownDurationPrev);
        std::copy(std::begin(MouseDragMaxDistanceAbs),        std::end(MouseDragMaxDistanceAbs),        io.MouseDragMaxDistanceAbs);
        std::copy(std::begin(MouseDragMaxDistanceSqr),        std::end(MouseDragMaxDistanceSqr),        io.MouseDragMaxDistanceSqr);
    }

    void ImGuiMouseState::Advance()
    {
        //This is almost just ImGui::UpdateMouseInputs(), but we don't touch any global state here
        const ImGuiContext& g = *ImGui::GetCurrentContext();

        // Round mouse position to avoid spreading non-rounded position (e.g. UpdateManualResize doesn't support them well)
        if (IsMousePosValid(&MousePos))
            MousePos = ImFloor(MousePos);

        // If mouse just appeared or disappeared (usually denoted by -FLT_MAX components) we cancel out movement in MouseDelta
        if (IsMousePosValid(&MousePos) && IsMousePosValid(&MousePosPrev))
            MouseDelta = MousePos - MousePosPrev;
        else
            MouseDelta = ImVec2(0.0f, 0.0f);

        MousePosPrev = MousePos;
        for (int i = 0; i < IM_ARRAYSIZE(MouseDown); i++)
        {
            MouseClicked[i] = MouseDown[i] && MouseDownDuration[i] < 0.0f;
            MouseReleased[i] = !MouseDown[i] && MouseDownDuration[i] >= 0.0f;
            MouseDownDurationPrev[i] = MouseDownDuration[i];
            MouseDownDuration[i] = MouseDown[i] ? (MouseDownDuration[i] < 0.0f ? 0.0f : MouseDownDuration[i] + g.IO.DeltaTime) : -1.0f;
            MouseDoubleClicked[i] = false;
            if (MouseClicked[i])
            {
                if ((float)(g.Time - MouseClickedTime[i]) < g.IO.MouseDoubleClickTime)
                {
                    ImVec2 delta_from_click_pos = IsMousePosValid(&MousePos) ? (MousePos - MouseClickedPos[i]) : ImVec2(0.0f, 0.0f);
                    if (ImLengthSqr(delta_from_click_pos) < g.IO.MouseDoubleClickMaxDist * g.IO.MouseDoubleClickMaxDist)
                        MouseDoubleClicked[i] = true;
                    MouseClickedTime[i] = -g.IO.MouseDoubleClickTime * 2.0f; // Mark as "old enough" so the third click isn't turned into a double-click
                }
                else
                {
                    MouseClickedTime[i] = g.Time;
                }
                MouseClickedPos[i] = MousePos;
                MouseDownWasDoubleClick[i] = MouseDoubleClicked[i];
                MouseDragMaxDistanceAbs[i] = ImVec2(0.0f, 0.0f);
                MouseDragMaxDistanceSqr[i] = 0.0f;
            }
            else if (MouseDown[i])
            {
                // Maintain the maximum distance we reaching from the initial click position, which is used with dragging threshold
                ImVec2 delta_from_click_pos = IsMousePosValid(&MousePos) ? (MousePos - MouseClickedPos[i]) : ImVec2(0.0f, 0.0f);
                MouseDragMaxDistanceSqr[i] = ImMax(MouseDragMaxDistanceSqr[i], ImLengthSqr(delta_from_click_pos));
                MouseDragMaxDistanceAbs[i].x = ImMax(MouseDragMaxDistanceAbs[i].x, delta_from_click_pos.x < 0.0f ? -delta_from_click_pos.x : delta_from_click_pos.x);
                MouseDragMaxDistanceAbs[i].y = ImMax(MouseDragMaxDistanceAbs[i].y, delta_from_click_pos.y < 0.0f ? -delta_from_click_pos.y : delta_from_click_pos.y);
            }
            if (!MouseDown[i] && !MouseReleased[i])
                MouseDownWasDoubleClick[i] = false;
        }
    }


    ActiveWidgetStateStorage::ActiveWidgetStateStorage()
    {
        IsInitialized                            = false;
        HoveredId                                = 0;
        HoveredIdPreviousFrame                   = 0;
        HoveredIdAllowOverlap                    = false;
        HoveredIdUsingMouseWheel                 = false;
        HoveredIdPreviousFrameUsingMouseWheel    = false;
        HoveredIdDisabled                        = false;
        HoveredIdTimer                           = 0.0f;
        HoveredIdNotActiveTimer                  = 0.0f;
        ActiveId                                 = 0;
        ActiveIdIsAlive                          = 0;
        ActiveIdTimer                            = 0.0f;
        ActiveIdIsJustActivated                  = false;
        ActiveIdAllowOverlap                     = false;
        ActiveIdNoClearOnFocusLoss               = false;
        ActiveIdHasBeenPressedBefore             = false;
        ActiveIdHasBeenEditedBefore              = false;
        ActiveIdHasBeenEditedThisFrame           = false;
        ActiveIdUsingMouseWheel                  = false;
        ActiveIdUsingNavDirMask                  = 0x00;
        ActiveIdUsingNavInputMask                = 0x00;
        ActiveIdUsingKeyInputMask                = 0x00;
        ActiveIdClickOffset                      = ImVec2(-1, -1);
        ActiveIdWindow                           = nullptr;
        ActiveIdSource                           = ImGuiInputSource_None;
        ActiveIdMouseButton                      = -1;
        ActiveIdPreviousFrame                    = 0;
        ActiveIdPreviousFrameIsAlive             = false;
        ActiveIdPreviousFrameHasBeenEditedBefore = false;
        ActiveIdPreviousFrameWindow              = nullptr;
        LastActiveId                             = 0;
        LastActiveIdTimer                        = 0.0f;
    }

    void ActiveWidgetStateStorage::StoreCurrentState()
    {
        IsInitialized = true;

        ImGuiContext& g = *ImGui::GetCurrentContext();

        HoveredId                                = g.HoveredId;
        HoveredIdPreviousFrame                   = g.HoveredIdPreviousFrame;
        HoveredIdAllowOverlap                    = g.HoveredIdAllowOverlap;
        HoveredIdUsingMouseWheel                 = g.HoveredIdUsingMouseWheel;
        HoveredIdPreviousFrameUsingMouseWheel    = g.HoveredIdPreviousFrameUsingMouseWheel;
        HoveredIdDisabled                        = g.HoveredIdDisabled;
        HoveredIdTimer                           = g.HoveredIdTimer;
        HoveredIdNotActiveTimer                  = g.HoveredIdNotActiveTimer;
        ActiveId                                 = g.ActiveId;
        ActiveIdIsAlive                          = g.ActiveIdIsAlive;
        ActiveIdTimer                            = g.ActiveIdTimer;
        ActiveIdIsJustActivated                  = g.ActiveIdIsJustActivated;
        ActiveIdAllowOverlap                     = g.ActiveIdAllowOverlap;
        ActiveIdNoClearOnFocusLoss               = g.ActiveIdNoClearOnFocusLoss;
        ActiveIdHasBeenPressedBefore             = g.ActiveIdHasBeenPressedBefore;
        ActiveIdHasBeenEditedBefore              = g.ActiveIdHasBeenEditedBefore;
        ActiveIdHasBeenEditedThisFrame           = g.ActiveIdHasBeenEditedThisFrame;
        ActiveIdUsingMouseWheel                  = g.ActiveIdUsingMouseWheel;
        ActiveIdUsingNavDirMask                  = g.ActiveIdUsingNavDirMask;
        ActiveIdUsingNavInputMask                = g.ActiveIdUsingNavInputMask;
        ActiveIdUsingKeyInputMask                = g.ActiveIdUsingKeyInputMask;
        ActiveIdClickOffset                      = g.ActiveIdClickOffset;
        ActiveIdWindow                           = g.ActiveIdWindow;
        ActiveIdSource                           = g.ActiveIdSource;
        ActiveIdMouseButton                      = g.ActiveIdMouseButton;
        ActiveIdPreviousFrame                    = g.ActiveIdPreviousFrame;
        ActiveIdPreviousFrameIsAlive             = g.ActiveIdPreviousFrameIsAlive;
        ActiveIdPreviousFrameHasBeenEditedBefore = g.ActiveIdPreviousFrameHasBeenEditedBefore;
        ActiveIdPreviousFrameWindow              = g.ActiveIdPreviousFrameWindow;
        LastActiveId                             = g.LastActiveId;
        LastActiveIdTimer                        = g.LastActiveIdTimer;
    }

    void ActiveWidgetStateStorage::ApplyState()
    {
        //Do nothing if not initialized. Calls to this are typically followed by storing the state afterwards, initializing it correctly then
        if (!IsInitialized)
            return;

        ImGuiContext& g = *ImGui::GetCurrentContext();

        g.HoveredId                                = HoveredId;
        g.HoveredIdPreviousFrame                   = HoveredIdPreviousFrame;
        g.HoveredIdAllowOverlap                    = HoveredIdAllowOverlap;
        g.HoveredIdUsingMouseWheel                 = HoveredIdUsingMouseWheel;
        g.HoveredIdPreviousFrameUsingMouseWheel    = HoveredIdPreviousFrameUsingMouseWheel;
        g.HoveredIdDisabled                        = HoveredIdDisabled;
        g.HoveredIdTimer                           = HoveredIdTimer;
        g.HoveredIdNotActiveTimer                  = HoveredIdNotActiveTimer;
        g.ActiveId                                 = ActiveId;
        g.ActiveIdIsAlive                          = ActiveIdIsAlive;
        g.ActiveIdTimer                            = ActiveIdTimer;
        g.ActiveIdIsJustActivated                  = ActiveIdIsJustActivated;
        g.ActiveIdAllowOverlap                     = ActiveIdAllowOverlap;
        g.ActiveIdNoClearOnFocusLoss               = ActiveIdNoClearOnFocusLoss;
        g.ActiveIdHasBeenPressedBefore             = ActiveIdHasBeenPressedBefore;
        g.ActiveIdHasBeenEditedBefore              = ActiveIdHasBeenEditedBefore;
        g.ActiveIdHasBeenEditedThisFrame           = ActiveIdHasBeenEditedThisFrame;
        g.ActiveIdUsingMouseWheel                  = ActiveIdUsingMouseWheel;
        g.ActiveIdUsingNavDirMask                  = ActiveIdUsingNavDirMask;
        g.ActiveIdUsingNavInputMask                = ActiveIdUsingNavInputMask;
        g.ActiveIdUsingKeyInputMask                = ActiveIdUsingKeyInputMask;
        g.ActiveIdClickOffset                      = ActiveIdClickOffset;
        g.ActiveIdWindow                           = (ImGuiWindow*)ActiveIdWindow;
        g.ActiveIdSource                           = (ImGuiInputSource)ActiveIdSource;
        g.ActiveIdMouseButton                      = ActiveIdMouseButton;
        g.ActiveIdPreviousFrame                    = ActiveIdPreviousFrame;
        g.ActiveIdPreviousFrameIsAlive             = ActiveIdPreviousFrameIsAlive;
        g.ActiveIdPreviousFrameHasBeenEditedBefore = ActiveIdPreviousFrameHasBeenEditedBefore;
        g.ActiveIdPreviousFrameWindow              = (ImGuiWindow*)ActiveIdPreviousFrameWindow;
        g.LastActiveId                             = LastActiveId;
        g.LastActiveIdTimer                        = LastActiveIdTimer;
    }

    void ActiveWidgetStateStorage::AdvanceState()
    {
        if (!IsInitialized)
            return;

        const ImGuiIO& io = ImGui::GetIO();

        // Update HoveredId data
        if (!HoveredIdPreviousFrame)
            HoveredIdTimer = 0.0f;
        if (!HoveredIdPreviousFrame || (HoveredId && ActiveId == HoveredId))
            HoveredIdNotActiveTimer = 0.0f;
        if (HoveredId)
            HoveredIdTimer += io.DeltaTime;
        if (HoveredId && ActiveId != HoveredId)
            HoveredIdNotActiveTimer += io.DeltaTime;
        HoveredIdPreviousFrame = HoveredId;
        HoveredIdPreviousFrameUsingMouseWheel = HoveredIdUsingMouseWheel;
        HoveredId = 0;
        HoveredIdAllowOverlap = false;
        HoveredIdUsingMouseWheel = false;
        HoveredIdDisabled = false;

        // Update ActiveId data (clear reference to active widget if the widget isn't alive anymore)
        if (ActiveIdIsAlive != ActiveId && ActiveIdPreviousFrame == ActiveId && ActiveId != 0)
        {
            ActiveIdIsJustActivated = true;

            if (ActiveIdIsJustActivated)
            {
                ActiveIdTimer                = 0.0f;
                ActiveIdHasBeenPressedBefore = false;
                ActiveIdHasBeenEditedBefore  = false;
                ActiveIdMouseButton          = -1;
            }

            ActiveId                       = 0;
            ActiveIdAllowOverlap           = false;
            ActiveIdNoClearOnFocusLoss     = false;
            ActiveIdWindow                 = nullptr;
            ActiveIdHasBeenEditedThisFrame = false;
        }

        if (ActiveId)
            ActiveIdTimer += io.DeltaTime;

        LastActiveIdTimer                       += io.DeltaTime;
        ActiveIdPreviousFrame                    = ActiveId;
        ActiveIdPreviousFrameWindow              = ActiveIdWindow;
        ActiveIdPreviousFrameHasBeenEditedBefore = ActiveIdHasBeenEditedBefore;
        ActiveIdIsAlive                          = 0;
        ActiveIdHasBeenEditedThisFrame           = false;
        ActiveIdPreviousFrameIsAlive             = false;
        ActiveIdIsJustActivated                  = false;

        if (ActiveId == 0)
        {
            ActiveIdUsingNavDirMask   = 0x00;
            ActiveIdUsingNavInputMask = 0x00;
            ActiveIdUsingKeyInputMask = 0x00;
        }
    }
}