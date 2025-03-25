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
        const bool  mouse_left_clicked_old       = io.MouseClicked[ImGuiMouseButton_Left];
        const bool  mouse_left_down_old          = io.MouseDown[ImGuiMouseButton_Left];
        const float mouse_left_down_duration_old = io.MouseDownDuration[ImGuiMouseButton_Left];
        const bool key_ctrl_old = io.KeyCtrl;

        if (io.MouseClicked[ImGuiMouseButton_Right])
        {
            io.MouseClicked[ImGuiMouseButton_Left]      = true;
            io.MouseDown[ImGuiMouseButton_Left]         = true;
            io.MouseDownDuration[ImGuiMouseButton_Left] = 0.0f;
            io.KeyCtrl = true;
            io.KeyMods |= ImGuiMod_Ctrl; //KeyMods needs to stay consistent with KeyCtrl
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
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

        //Calulate slider width (GetContentRegionAvail() returns 1 more than when using -1 width to fill)
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2) - 1.0f);
        ImGui::SliderFloat("##Slider", &value, min, max, format, flags);

        if ( (text_alt != nullptr) && (ImGui::GetCurrentContext()->TempInputId != ImGui::GetID("##Slider")) )
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

        ImGui::PopItemFlag();   //ImGuiItemFlag_ButtonRepeat
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
        io.MouseClicked[ImGuiMouseButton_Left]      = mouse_left_clicked_old;
        io.MouseDown[ImGuiMouseButton_Left]         = mouse_left_down_old;
        io.MouseDownDuration[ImGuiMouseButton_Left] = mouse_left_down_duration_old;
        io.KeyCtrl = key_ctrl_old;
        if (!io.KeyCtrl)
        {
            io.KeyMods &= ~ImGuiMod_Ctrl;
        }

        return (value != value_old);
    }

    bool SliderWithButtonsInt(const char* str_id, int& value, int step, int step_small, int min, int max, const char* format, ImGuiSliderFlags flags, bool* used_button, const char* text_alt)
    {
        //Hacky solution to make right mouse enable text input on the slider while not touching ImGui code or generalizing it as ctrl press
        ImGuiIO& io = ImGui::GetIO();
        const bool  mouse_left_clicked_old       = io.MouseClicked[ImGuiMouseButton_Left];
        const bool  mouse_left_down_old          = io.MouseDown[ImGuiMouseButton_Left];
        const float mouse_left_down_duration_old = io.MouseDownDuration[ImGuiMouseButton_Left];
        const bool key_ctrl_old = io.KeyCtrl;

        if (io.MouseClicked[ImGuiMouseButton_Right])
        {
            io.MouseClicked[ImGuiMouseButton_Left]      = true;
            io.MouseDown[ImGuiMouseButton_Left]         = true;
            io.MouseDownDuration[ImGuiMouseButton_Left] = 0.0f;
            io.KeyCtrl = true;
            io.KeyMods |= ImGuiMod_Ctrl; //KeyMods needs to stay consistent with KeyCtrl
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
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

        //Calulate slider width (GetContentRegionAvail() returns 1 more than when using -1 width to fill)
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2) - 1.0f);
        ImGui::SliderInt("##Slider", &value, min, max, format, flags);

        if ( (text_alt != nullptr) && (ImGui::GetCurrentContext()->TempInputId != ImGui::GetID("##Slider")) )
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

        ImGui::PopItemFlag();   //ImGuiItemFlag_ButtonRepeat
        ImGui::PopID();

        ImGui::EndGroup();

        //Deactivated flag for the slider gets swallowed up somewhere, but we really need it for the VR keyboard, so we tape it back on here
        if (has_slider_deactivated)
        {
            ImGui::GetCurrentContext()->LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDeactivated | ImGuiItemStatusFlags_Deactivated;
        }

        //Restore hack
        io.MouseClicked[ImGuiMouseButton_Left]      = mouse_left_clicked_old;
        io.MouseDown[ImGuiMouseButton_Left]         = mouse_left_down_old;
        io.MouseDownDuration[ImGuiMouseButton_Left] = mouse_left_down_duration_old;
        io.KeyCtrl = key_ctrl_old;
        if (!io.KeyCtrl)
        {
            io.KeyMods &= ~ImGuiMod_Ctrl;
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
                ImGui::ActivateItemByID(ImGui::GetItemID());
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
        //Otherwise exactly the same as ImGui::BeginCombo() of ImGui v1.82 (with compat patches)
        static float animation_progress = 0.0f;
        static float scrollbar_alpha = 0.0f;
        float popup_expected_width = FLT_MAX;
        float popup_height = 0.0f;
        //-

        // Always consume the SetNextWindowSizeConstraint() call in our early return paths
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindow();

        ImGuiNextWindowDataFlags backup_next_window_data_flags = g.NextWindowData.HasFlags;
        g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
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
        RenderNavCursor(frame_bb, id);
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

        g.NextWindowData.HasFlags = backup_next_window_data_flags;

        // Set popup size
        if (g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)
        {
            g.NextWindowData.SizeConstraintRect.Min.x = ImMax(g.NextWindowData.SizeConstraintRect.Min.x, w);
        }
        else
        {
            if ((flags & ImGuiComboFlags_HeightMask_) == 0)
                flags |= ImGuiComboFlags_HeightRegular;
            IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiComboFlags_HeightMask_)); // Only one
            int popup_max_height_in_items = -1;
            if (flags & ImGuiComboFlags_HeightRegular)     popup_max_height_in_items = 8;
            else if (flags & ImGuiComboFlags_HeightSmall)  popup_max_height_in_items = 4;
            else if (flags & ImGuiComboFlags_HeightLarge)  popup_max_height_in_items = 20;
            ImVec2 constraint_min(0.0f, 0.0f), constraint_max(FLT_MAX, FLT_MAX);
            if ((g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSize) == 0 || g.NextWindowData.SizeVal.x <= 0.0f) // Don't apply constraints if user specified a size
                constraint_min.x = w;
            if ((g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSize) == 0 || g.NextWindowData.SizeVal.y <= 0.0f)
                constraint_max.y = CalcMaxPopupHeightFromItemCount(popup_max_height_in_items);
            SetNextWindowSizeConstraints(constraint_min, constraint_max);
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
                const bool pos_goes_down = (popup_window->AutoPosLastDirection == ImGuiDir_Down) || (popup_window->AutoPosLastDirection == ImGuiDir_Left);
                pos.y += (pos_goes_down) ? -size_expected.y + popup_height : size_expected.y - popup_height;

                popup_expected_width = size_expected.x;
                //-

                SetNextWindowPos(pos);
            }
            //-
            else
            {
                //Expected width is not calculated in the first frame, having it default to larger than w is avoids flicker if it is the next frame. Kinda hacky to be fair
                popup_expected_width = FLT_MAX;
            }
            //-

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
            if (popup_expected_width <= w)  //With popups wider than the widget, we run into resize issues when toggling the scrollbar. So we skip it there as it's the lesser evil
            {
                window_flags |= ImGuiWindowFlags_NoScrollbar; //Disable scrollbar when not visible so it can't be clicked accidentally
            }
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
            if ((popup_window->AutoPosLastDirection == ImGuiDir_Down) || (popup_window->AutoPosLastDirection == ImGuiDir_Left)) //ImGuiDir_Left is left aligned but still down
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
        const char* text, *text_end;
        ImFormatStringToTempBufferV(&text, &text_end, fmt, args);

        ImVec2 size = ImGui::CalcTextSize(text, text_end);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - size.x - offset_x));

        TextEx(text, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
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

    int g_Stretched_BeginIndex = 0;
    float g_Stretched_BaseX = 0.0f;
    void BeginStretched()
    {
        g_Stretched_BeginIndex = ImGui::GetWindowDrawList()->VtxBuffer.Size;

        const auto& buffer = ImGui::GetWindowDrawList()->VtxBuffer;
        g_Stretched_BaseX = (buffer.empty()) ? 0.0f : buffer[g_Stretched_BeginIndex - 1].pos.x;
    }

    void EndStretched(float scale_x)
    {
        auto& buffer = ImGui::GetWindowDrawList()->VtxBuffer;

        g_Stretched_BaseX = (buffer.size() > g_Stretched_BeginIndex) ? buffer[g_Stretched_BeginIndex].pos.x : 0.0f;
        for (int i = g_Stretched_BeginIndex; i < buffer.Size; ++i)
        {
            buffer[i].pos.x = ((buffer[i].pos.x - g_Stretched_BaseX) * scale_x) + g_Stretched_BaseX;
        }
    }

    //Takes a nicely adjustable function and bolts it down to the options we need after making a few modifications
    bool ColorPicker4Simple(const char* str_id, float col[4], float ref_col[4], const char* label_color_current, const char* label_color_original, float scale)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        //Hacky solution to make right mouse enable text input on the slider while not touching ImGui code or generalizing it as ctrl press
        ImGuiIO& io = ImGui::GetIO();
        const bool mouse_left_clicked_old = io.MouseClicked[ImGuiMouseButton_Left];
        const float mouse_left_down_duration_old = io.MouseDownDuration[ImGuiMouseButton_Left];
        const bool key_ctrl_old = io.KeyCtrl;

        if (io.MouseClicked[ImGuiMouseButton_Right])
        {
            io.MouseDown[ImGuiMouseButton_Left]         = true;
            io.MouseClicked[ImGuiMouseButton_Left]      = true;
            io.MouseDownDuration[ImGuiMouseButton_Left] = 0.0f;
            io.KeyCtrl = true;
            io.KeyMods |= ImGuiMod_Ctrl; //Mods needs to stay consistent with KeyCtrl
        }

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;

        //Fixed flags, but we still kept some checks below in case we need to adjust later
        const ImGuiColorEditFlags flags = ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview | 
                                          ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop;

        const float start_y           = ImGui::GetCursorScreenPos().y;
        const float square_sz         = ImGui::GetFrameHeight() * scale;
        const float picker_width      = square_sz * 15.5f;
        const float picker_bar_height = picker_width - (2.0f * (square_sz + style.ItemInnerSpacing.x));
        g.NextItemData.ClearFlags();

        ImGui::BeginGroup();
        ImGui::PushID(str_id);

        bool value_changed = false;

        ImGuiColorEditFlags picker_flags_to_forward = ImGuiColorEditFlags_DataTypeMask_ | ImGuiColorEditFlags_PickerMask_ | ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_HDR | 
                                                      ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop;

        ImGuiColorEditFlags picker_flags = (flags & picker_flags_to_forward) | ImGuiColorEditFlags_DisplayMask_ | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview | 
                                           ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoOptions;

        ImGui::SetNextItemWidth(picker_width);
        value_changed |= ImGui::ColorPicker4("##picker", col, picker_flags, &g.ColorPickerRef.x);

        //Restore hack
        io.MouseDown[ImGuiMouseButton_Left]         = mouse_left_clicked_old;
        io.MouseClicked[ImGuiMouseButton_Left]      = mouse_left_clicked_old;
        io.MouseDownDuration[ImGuiMouseButton_Left] = mouse_left_down_duration_old;
        io.KeyCtrl = key_ctrl_old;
        if (!io.KeyCtrl)
        {
            io.KeyMods &= ~ImGuiMod_Ctrl;
        }

        //Picker style switching without popup
        if ( (ImGui::IsItemClicked(ImGuiMouseButton_Right)) && (ImGui::GetMousePos().y <= start_y + picker_bar_height) ) //(don't react to text input clicks)
        {
            ImGuiColorEditFlags picker_flags = (g.ColorEditOptions & ImGuiColorEditFlags_PickerHueBar) ? ImGuiColorEditFlags_PickerHueWheel : ImGuiColorEditFlags_PickerHueBar;

            g.ColorEditOptions = (g.ColorEditOptions & ~ImGuiColorEditFlags_PickerMask_) | (picker_flags & ImGuiColorEditFlags_PickerMask_);
        }

        //Side previews, but translatable
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        window->DC.CursorPos.y -= style.ItemSpacing.y;

        ImGui::BeginGroup();

        ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
        ImVec4 col_v4(col[0], col[1], col[2], (flags & ImGuiColorEditFlags_NoAlpha) ? 1.0f : col[3]);
        if ((flags & ImGuiColorEditFlags_NoLabel))
            ImGui::TextUnformatted(label_color_current);

        ImGuiColorEditFlags sub_flags_to_forward = ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip | 
                                                   ImGuiColorEditFlags_NoDragDrop;

        ImGui::ColorButton("##current", col_v4, (flags & sub_flags_to_forward), ImVec2(square_sz * 3, square_sz * 2));
        if (ref_col != nullptr)
        {
            ImGui::TextUnformatted(label_color_original);

            ImVec4 ref_col_v4(ref_col[0], ref_col[1], ref_col[2], (flags & ImGuiColorEditFlags_NoAlpha) ? 1.0f : ref_col[3]);
            if (ImGui::ColorButton("##original", ref_col_v4, (flags & sub_flags_to_forward), ImVec2(square_sz * 3, square_sz * 2)))
            {
                memcpy(col, ref_col, ((flags & ImGuiColorEditFlags_NoAlpha) ? 3 : 4) * sizeof(float));
                value_changed = true;
            }
        }

        ImGui::PopItemFlag();   //ImGuiItemFlags_NoNavDefaultFocus
        ImGui::EndGroup();

        ImGui::PopID();
        ImGui::EndGroup();

        if (value_changed)
            ImGui::MarkItemEdited(g.LastItemData.ID);

        return value_changed;
    }

    bool CollapsingHeaderPadded(const char* label, ImGuiTreeNodeFlags flags)
    {
        ImGuiWindow* window = GetCurrentWindow();
        //Move back a pixel to fix CollapsingHeader()'s position being off by one for some reason
        window->DC.CursorPos.x -= 1.0f;

        //Temporarily modify window padding to fool CollapsingHeader to size differently
        const float padding_x = ImGui::GetStyle().ItemInnerSpacing.x / 2.0f;
        window->WindowPadding.x -= padding_x;
        bool ret = ImGui::CollapsingHeader(label, flags);
        window->WindowPadding.x += padding_x;

        return ret;
    }

    struct CollapsingAreaState
    {
        ImGuiID WidgetID    = 0;
        float WidgetBeginY  = 0.0f;
        bool PushedClipRect = false;
    };

    ImVector<CollapsingAreaState> g_CollapsingArea_Stack;

    void BeginCollapsingArea(const char* str_id, bool show_content, float& persist_animation_progress)
    {
        g_CollapsingArea_Stack.push_back(CollapsingAreaState());
        CollapsingAreaState& state = g_CollapsingArea_Stack.back();

        //Animate when changing between show_content state
        const float animation_step = ImGui::GetIO().DeltaTime * 3.0f;
        persist_animation_progress = clamp(persist_animation_progress + ((show_content) ? animation_step : -animation_step), 0.0f, 1.0f);

        //Set clipping
        state.WidgetID = ImGui::GetID(str_id);

        //Don't push clip rect on full progress to allow for popups and such
        if (persist_animation_progress != 1.0f)
        {
            const float content_height = ImGui::GetStateStorage()->GetFloat(state.WidgetID, 0.0f);
            const float clip_height = smoothstep(persist_animation_progress, 0.0f, content_height);
            ImVec2 clip_begin = ImGui::GetCursorScreenPos();
            ImVec2 clip_end(clip_begin.x + ImGui::GetContentRegionAvail().x, clip_begin.y + clip_height);

            ImGui::PushClipRect(clip_begin, clip_end, true);
            state.PushedClipRect = true;

            //Pull cursor position further up to make widgets scroll down as they animate, keep start Y-pos in global to use in the next EndCollapsingArea() call
            state.WidgetBeginY = (ImGui::GetCursorPosY() - content_height) + clip_height;
            ImGui::SetCursorPosY(state.WidgetBeginY);
        }
    }

    void EndCollapsingArea()
    {
        IM_ASSERT(!g_CollapsingArea_Stack.empty() && "Called EndCollapsingArea() before BeginCollapsingArea()");

        CollapsingAreaState& state = g_CollapsingArea_Stack.back();

        float& content_height = *ImGui::GetStateStorage()->GetFloatRef(state.WidgetID);
        content_height = ImGui::GetCursorPosY() - state.WidgetBeginY;

        if (state.PushedClipRect)
        {
            ImGui::PopClipRect();
        }

        g_CollapsingArea_Stack.pop_back();
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

    void ConfigDisableCtrlTab()
    {
        GImGui->ConfigNavWindowingKeyNext = ImGuiKey_None;
        GImGui->ConfigNavWindowingKeyPrev = ImGuiKey_None;
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

    //ImGui got rid of direct nav input access, but we keep it around as an abstraction layer
    ImGuiKey MapNavToKey(ImGuiNavInput nav_input, ImGuiInputSource input_source)
    {
        ImGuiKey imgui_key = ImGuiKey_None;

        if (input_source == ImGuiInputSource_Keyboard)
        {
            switch (nav_input)
            {
                case ImGuiNavInput_Activate:  imgui_key = ImGuiKey_Space;      break;
                case ImGuiNavInput_Cancel:    imgui_key = ImGuiKey_Escape;     break;
                case ImGuiNavInput_Input:     imgui_key = ImGuiKey_Enter;      break;
                case ImGuiNavInput_Menu:      imgui_key = ImGuiMod_Alt;        break;
                case ImGuiNavInput_DpadLeft:  imgui_key = ImGuiKey_LeftArrow;  break;
                case ImGuiNavInput_DpadRight: imgui_key = ImGuiKey_RightArrow; break;
                case ImGuiNavInput_DpadUp:    imgui_key = ImGuiKey_UpArrow;    break;
                case ImGuiNavInput_DpadDown:  imgui_key = ImGuiKey_DownArrow;  break;
                case ImGuiNavInput_TweakSlow: imgui_key = ImGuiMod_Ctrl;       break;
                case ImGuiNavInput_TweakFast: imgui_key = ImGuiMod_Shift;      break;
            }
        }
        else if (input_source == ImGuiInputSource_Gamepad)
        {
            switch (nav_input)
            {
                case ImGuiNavInput_Activate:  imgui_key = ImGuiKey_GamepadFaceDown;  break;
                case ImGuiNavInput_Cancel:    imgui_key = ImGuiKey_GamepadFaceRight; break;
                case ImGuiNavInput_Input:     imgui_key = ImGuiKey_GamepadFaceLeft;  break;
                case ImGuiNavInput_Menu:      imgui_key = ImGuiKey_GamepadFaceUp;    break;
                case ImGuiNavInput_DpadLeft:  imgui_key = ImGuiKey_GamepadDpadLeft;  break;
                case ImGuiNavInput_DpadRight: imgui_key = ImGuiKey_GamepadDpadRight; break;
                case ImGuiNavInput_DpadUp:    imgui_key = ImGuiKey_GamepadDpadUp;    break;
                case ImGuiNavInput_DpadDown:  imgui_key = ImGuiKey_GamepadDpadDown;  break;
                case ImGuiNavInput_TweakSlow: imgui_key = ImGuiKey_GamepadL1;        break;
                case ImGuiNavInput_TweakFast: imgui_key = ImGuiKey_GamepadR1;        break;
            }
        }

        return imgui_key;
    }

    bool IsNavInputDown(ImGuiNavInput nav_input)
    {
        return ImGui::IsKeyDown(MapNavToKey(nav_input, GImGui->NavInputSource));
    }

    bool ImGui::IsNavInputPressed(ImGuiNavInput nav_input, bool repeat)
    {
        return ImGui::IsKeyPressed(MapNavToKey(nav_input, GImGui->NavInputSource), repeat);
    }

    bool ImGui::IsNavInputReleased(ImGuiNavInput nav_input)
    {
        return ImGui::IsKeyReleased(MapNavToKey(nav_input, GImGui->NavInputSource));
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

        return ( (g.HoveredId != g.HoveredIdPreviousFrame) && (g.HoveredId != 0) && (!g.HoveredIdIsDisabled) && (!blocked_by_active_item) );
    }

    bool IsAnyItemActiveOrDeactivated()
    {
        const ImGuiContext& g = *GImGui;
        return ( (g.ActiveId != 0) || (g.ActiveIdPreviousFrame != 0) );
    }

    bool IsAnyItemDeactivated()
    {
        const ImGuiContext& g = *GImGui;
        return ( (g.ActiveIdPreviousFrame != 0) && (g.ActiveId != g.ActiveIdPreviousFrame) );
    }

    bool IsAnyInputTextActive()
    {
        const ImGuiContext& g = *GImGui;
        return ( (g.ActiveId != 0) && (g.ActiveId == g.InputTextState.ID) );
    }

    bool IsAnyTempInputTextActive()
    {
        const ImGuiContext& g = *GImGui;
        return ( (g.ActiveId != 0) && (g.ActiveId == g.InputTextState.ID) && (g.TempInputId == g.InputTextState.ID) );
    }

    bool IsAnyMouseClicked()
    {
        const ImGuiContext& g = *GImGui;
        for (int n = 0; n < IM_ARRAYSIZE(g.IO.MouseDown); n++)
            if (g.IO.MouseClicked[n])
                return true;
        return false;
    }

    void BlockWidgetInput()
    {
        ImGuiContext& g = *GImGui;

        ImGui::SetActiveID(ImGui::GetID("ImGuiExtInputBlock"), nullptr);
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelX, g.ActiveId);
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, g.ActiveId);
        g.WheelingWindow = nullptr;
        g.WheelingWindowReleaseTimer = 0.0f;
    }

    void HScrollWindowFromMouseWheelV()
    {
        ImGuiContext& g = *GImGui;

        //Don't do anything is real hscroll is active
        if ( (g.IO.MouseWheelH != 0.0f && !g.IO.KeyShift) || (g.IO.MouseWheel != 0.0f && g.IO.KeyShift) )
            return;

        ImGuiWindow* window = ImGui::GetCurrentWindow();

        if (g.WheelingWindow != window)
            return;

        const float wheel_x = g.IO.MouseWheel;
        float max_step = window->InnerRect.GetWidth() * 0.67f;
        float scroll_step = ImTrunc(ImMin(2.0f * window->CalcFontSize(), max_step));

        ImGui::SetScrollX(window, window->Scroll.x - wheel_x * scroll_step);
    }

    void ScrollBeginStackParentWindow()
    {
        ImGuiContext& g = *GImGui;

        ImGuiWindow* window = ImGui::GetCurrentWindowRead();

        if (g.WheelingWindow != window)
            return;

        ImGuiWindow* window_target = window->ParentWindowInBeginStack;

        if (window_target == nullptr)
            return;

        const float wheel_x = g.IO.MouseWheelH;
        const float wheel_y = g.IO.MouseWheel;

        //HScroll
        float max_step = window_target->InnerRect.GetWidth() * 0.67f;
        float scroll_step = ImTrunc(ImMin(2.0f * window_target->CalcFontSize(), max_step));

        ImGui::SetScrollX(window_target, window_target->Scroll.x - wheel_x * scroll_step);

        //VScroll
        max_step = window_target->InnerRect.GetHeight() * 0.67f;
        scroll_step = ImTrunc(ImMin(5.0f * window_target->CalcFontSize(), max_step));

        ImGui::SetScrollY(window_target, window_target->Scroll.y - wheel_y * scroll_step);
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

    bool DraggableRectArea(const char* str_id, const ImVec2& area_size, ImGuiDraggableRectAreaState& state)
    {
        ImGuiIO& io = ImGui::GetIO();

        ImVec2& offset       = state.RectPos;
        ImVec2& size         = state.RectSize;
        ImVec2& offset_start = state.RectPosDragStart;
        ImVec2& size_start   = state.RectSizeDragStart;
        ImVec2 offset_prev   = offset;
        ImVec2 size_prev     = size;

        ImGuiMouseButton& drag_mouse_button = state.DragMouseButton;
        ImGuiDir& edge_drag_h_dir           = state.EdgeDragHDir;
        ImGuiDir& edge_drag_v_dir           = state.EdgeDragVDir;
        bool& is_edge_drag_active           = state.EdgeDragActive;
        bool& highlight_edge                = state.EdgeDragHighlightVisible;

        bool is_drag_active = false;

        //This widget is mouse-only and interacts weirdly with the ImGui navigation.
        //I've been unable to get ImGui to skip over it or bend it to work in sane manner, so I've resorted to disabling the items when nav is visible
        const bool was_disabled_initially = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled);
        const bool disable_for_nav = (io.NavVisible && !ImGui::IsMouseClicked(ImGuiMouseButton_Left));

        if (disable_for_nav)
            ImGui::PushItemDisabledNoVisual();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
        if (ImGui::BeginChild(str_id, area_size, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            const float drag_threshold_prev = io.MouseDragThreshold;
            io.MouseDragThreshold = 0.0f;

            ImVec2 pos_area = ImGui::GetCursorScreenPos();
            ImVec2 pos_button = pos_area;
            pos_button.x += roundf(offset.x);
            pos_button.y += roundf(offset.y);
            const float drag_margin = ImGui::GetFontSize() / 2.0f;

            //Draw rectangle manually
            ImVec4 col_button = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
            ImVec4 col_fill = col_button;
            col_fill.w = 0.25f;

            ImGui::GetWindowDrawList()->AddRectFilled(pos_button, {roundf(pos_button.x + size.x), roundf(pos_button.y + size.y)}, ImGui::GetColorU32(col_fill));
            ImGui::GetWindowDrawList()->AddRect(pos_button,       {roundf(pos_button.x + size.x), roundf(pos_button.y + size.y)}, ImGui::GetColorU32(col_button));

            if (highlight_edge)
            {
                if ((size.x > drag_margin * 2.0f) && (size.y > drag_margin * 2.0f))
                {
                    col_fill.w = 0.10f;

                    //This highlights the inner part actually, but still does the trick
                    ImGui::GetWindowDrawList()->AddRectFilled({pos_button.x + drag_margin, pos_button.y + drag_margin},
                                                              {roundf(pos_button.x + size.x - drag_margin), roundf(pos_button.y + size.y - drag_margin)},
                                                              ImGui::GetColorU32(col_fill));
                }
            }

            highlight_edge = false;

            //Invisible button spanning the entire area to catch right clicks anywhere for relative drag
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::InvisibleButton("DraggableRectArea", area_size, ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_PressedOnClick))
            {
                offset_start = offset;
                size_start   = size;
                drag_mouse_button = ImGuiMouseButton_Right;

                edge_drag_h_dir = ImGuiDir_None;
                edge_drag_v_dir = ImGuiDir_None;
                is_edge_drag_active = false;
            }
            else if (ImGui::IsItemActive())
            {
                is_drag_active = true;
            }

            //Pad the button size/pos to allow for off-edge grabs
            ImVec2 pos_button_padded = pos_button;
            pos_button_padded.x -= drag_margin / 2.0f;
            pos_button_padded.y -= drag_margin / 2.0f;

            ImVec2 size_button_padded = size;
            size_button_padded.x += drag_margin;
            size_button_padded.y += drag_margin;

            ImGui::SetCursorScreenPos(pos_button_padded);
            if (ImGui::InvisibleButton("DraggableRect", size_button_padded, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_AllowOverlap))
            {
                offset_start = offset;
                size_start   = size;
                drag_mouse_button = ImGuiMouseButton_Left;

                const ImVec2 pos(io.MouseClickedPos[drag_mouse_button].x - pos_area.x, io.MouseClickedPos[drag_mouse_button].y - pos_area.y);

                edge_drag_h_dir = ImGuiDir_None;
                edge_drag_v_dir = ImGuiDir_None;

                if (pos.x < offset.x + drag_margin)
                    edge_drag_h_dir = ImGuiDir_Left;
                else if (pos.x > offset.x + size.x - drag_margin)
                    edge_drag_h_dir = ImGuiDir_Right;

                if (pos.y < offset.y + drag_margin)
                    edge_drag_v_dir = ImGuiDir_Up;
                else if (pos.y > offset.y + size.y - drag_margin)
                    edge_drag_v_dir = ImGuiDir_Down;

                is_edge_drag_active = ((edge_drag_h_dir != ImGuiDir_None) || (edge_drag_v_dir != ImGuiDir_None));
            }
            else if (ImGui::IsItemActive())
            {
                is_drag_active = true;
            }

            if (is_drag_active)
            {
                if (is_edge_drag_active)
                {
                    if (edge_drag_h_dir == ImGuiDir_Left)
                    {
                        size.x = size_start.x - ImGui::GetMouseDragDelta(drag_mouse_button).x;
                        offset.x = offset_start.x - (size.x - size_start.x);

                        if (offset.x < 0.0f)
                        {
                            size.x += offset.x;
                            offset.x = 0.0f;
                        }
                        else if (offset.x > area_size.x)   //When dragged to opposite edge (width is negative)
                        {
                            size.x -= area_size.x - offset.x;
                            offset.x = area_size.x;
                        }
                    }
                    else if (edge_drag_h_dir == ImGuiDir_Right)
                    {
                        size.x = size_start.x + ImGui::GetMouseDragDelta(drag_mouse_button).x;
                        size.x = std::min(size.x, area_size.x - offset.x);
                        offset.x = offset_start.x;

                        if (size.x + offset.x < 0.0f) //When dragged to opposite edge (width is negative)
                        {
                            size.x -= size.x + offset.x;
                        }
                    }

                    if (edge_drag_v_dir == ImGuiDir_Up)
                    {
                        size.y = size_start.y - ImGui::GetMouseDragDelta(drag_mouse_button).y;
                        offset.y = offset_start.y - (size.y - size_start.y);

                        if (offset.y < 0.0f)
                        {
                            size.y += offset.y;
                            offset.y = 0.0f;
                        }
                        else if (offset.y > area_size.y)   //When dragged to opposite edge (height is negative)
                        {
                            size.y -= area_size.y - offset.y;
                            offset.y = area_size.y;
                        }
                    }
                    else if (edge_drag_v_dir == ImGuiDir_Down)
                    {
                        size.y = size_start.y + ImGui::GetMouseDragDelta(drag_mouse_button).y;
                        size.y = std::min(size.y, area_size.y - offset.y);
                        offset.y = offset_start.y;

                        if (size.y + offset.y < 0.0f) //When dragged to opposite edge (height is negative)
                        {
                            size.y -= size.y + offset.y;
                        }
                    }
                }
                else  //Normal drag
                {
                    //Alt key scroll swapping is usually already done by ImGui but seems to be blocked by something down the line so we do it ourselves, whatever
                    if (io.KeyAlt)
                    {
                        io.MouseWheelH = -io.MouseWheel;
                        io.MouseWheel = 0.0f;
                    }

                    //Adjust size from wheel input (with deadzone for smooth scrolling input)
                    if (fabs(io.MouseWheelH) > 0.05f)
                    {
                        float size_diff = size.x;
                        size.x *= 1.0f + (io.MouseWheelH / -10.0f);
                        size_diff = size.x - size_diff;

                        offset_start.x -= size_diff / 2.0f;
                    }

                    if (fabs(io.MouseWheel) > 0.05f)
                    {
                        float size_diff = size.y;
                        size.y *= 1.0f + (io.MouseWheel / 10.0f);
                        size_diff = size.y - size_diff;

                        offset_start.y -= size_diff / 2.0f;
                    }

                    offset.x = offset_start.x + ImGui::GetMouseDragDelta(drag_mouse_button).x;
                    offset.y = offset_start.y + ImGui::GetMouseDragDelta(drag_mouse_button).y;
                }

                //Correct inverted rectangle
                if (size.x < 0.0f)
                {
                    offset.x += size.x;
                    size.x *= -1.0f;
                }

                if (size.y < 0.0f)
                {
                    offset.y += size.y;
                    size.y *= -1.0f;
                }

                //Clamp to area
                offset.x = clamp(offset.x, 0.0f, area_size.x - size.x);
                offset.y = clamp(offset.y, 0.0f, area_size.y - size.y);

                size.x = clamp(size.x, 1.0f, area_size.x);
                size.y = clamp(size.y, 1.0f, area_size.y);
            }
            else
            {
                highlight_edge = ImGui::IsItemHovered( (was_disabled_initially) ? 0 : ImGuiHoveredFlags_AllowWhenDisabled );
            }

            io.MouseDragThreshold = drag_threshold_prev;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        if (disable_for_nav)
            ImGui::PopItemDisabledNoVisual();

        return ( (is_drag_active) && ((offset.x != offset_prev.x) || (offset.y != offset_prev.y) || 
                                      (size.x   != size_prev.x)   || (size.y   != size_prev.y)) );
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
        std::copy(std::begin(io.MouseClickedCount),              std::end(io.MouseClickedCount),              MouseClickedCount);
        std::copy(std::begin(io.MouseClickedLastCount),          std::end(io.MouseClickedLastCount),          MouseClickedLastCount);
        std::copy(std::begin(io.MouseReleased),                  std::end(io.MouseReleased),                  MouseReleased);
        std::copy(std::begin(io.MouseDownOwned),                 std::end(io.MouseDownOwned),                 MouseDownOwned);
        std::copy(std::begin(io.MouseDownOwnedUnlessPopupClose), std::end(io.MouseDownOwnedUnlessPopupClose), MouseDownOwnedUnlessPopupClose);
        std::copy(std::begin(io.MouseDownDuration),              std::end(io.MouseDownDuration),              MouseDownDuration);
        std::copy(std::begin(io.MouseDownDurationPrev),          std::end(io.MouseDownDurationPrev),          MouseDownDurationPrev);
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
        std::copy(std::begin(MouseClickedCount),              std::end(MouseClickedCount),              io.MouseClickedCount);
        std::copy(std::begin(MouseClickedLastCount),          std::end(MouseClickedLastCount),          io.MouseClickedLastCount);
        std::copy(std::begin(MouseReleased),                  std::end(MouseReleased),                  io.MouseReleased);
        std::copy(std::begin(MouseDownOwned),                 std::end(MouseDownOwned),                 io.MouseDownOwned);
        std::copy(std::begin(MouseDownOwnedUnlessPopupClose), std::end(MouseDownOwnedUnlessPopupClose), io.MouseDownOwnedUnlessPopupClose);
        std::copy(std::begin(MouseDownDuration),              std::end(MouseDownDuration),              io.MouseDownDuration);
        std::copy(std::begin(MouseDownDurationPrev),          std::end(MouseDownDurationPrev),          io.MouseDownDurationPrev);
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
            MouseClickedCount[i] = 0; // Will be filled below
            MouseReleased[i] = !MouseDown[i] && MouseDownDuration[i] >= 0.0f;
            MouseDownDurationPrev[i] = MouseDownDuration[i];
            MouseDownDuration[i] = MouseDown[i] ? (MouseDownDuration[i] < 0.0f ? 0.0f : MouseDownDuration[i] + g.IO.DeltaTime) : -1.0f;
            if (MouseClicked[i])
            {
                bool is_repeated_click = false;
                if ((float)(g.Time - MouseClickedTime[i]) < g.IO.MouseDoubleClickTime)
                {
                    ImVec2 delta_from_click_pos = IsMousePosValid(&MousePos) ? (MousePos - MouseClickedPos[i]) : ImVec2(0.0f, 0.0f);
                    if (ImLengthSqr(delta_from_click_pos) < g.IO.MouseDoubleClickMaxDist * g.IO.MouseDoubleClickMaxDist)
                        is_repeated_click = true;
                }
                if (is_repeated_click)
                    MouseClickedLastCount[i]++;
                else
                    MouseClickedLastCount[i] = 1;

                MouseClickedTime[i] = g.Time;

                MouseClickedPos[i] = MousePos;
                MouseClickedCount[i] = MouseClickedLastCount[i];
                MouseDragMaxDistanceSqr[i] = 0.0f;
            }
            else if (MouseDown[i])
            {
                // Maintain the maximum distance we reaching from the initial click position, which is used with dragging threshold
                float delta_sqr_click_pos = IsMousePosValid(&MousePos) ? ImLengthSqr(MousePos - MouseClickedPos[i]) : 0.0f;
                MouseDragMaxDistanceSqr[i] = ImMax(MouseDragMaxDistanceSqr[i], delta_sqr_click_pos);
            }

            // We provide io.MouseDoubleClicked[] as a legacy service
            MouseDoubleClicked[i] = (MouseClickedCount[i] == 2);
        }
    }


    ActiveWidgetStateStorage::ActiveWidgetStateStorage()
    {
        //These need to be the same or things will explode
        IM_ASSERT(sizeof(ImGuiDeactivatedItemDataInternal) == sizeof(DeactivatedItemData));
        IM_ASSERT(sizeof(ActiveIdValueOnActivation)        == sizeof(ActiveIdValueOnActivation));

        IsInitialized                            = false;

        HoveredId                                = 0;
        HoveredIdPreviousFrame                   = 0;
        HoveredIdPreviousFrameItemCount          = 0;
        HoveredIdTimer                           = 0.0f;
        HoveredIdNotActiveTimer                  = 0.0f;
        HoveredIdAllowOverlap                    = false;
        HoveredIdIsDisabled                      = false;
        ItemUnclipByLog                          = false;
        ActiveId                                 = 0;
        ActiveIdIsAlive                          = 0;
        ActiveIdTimer                            = 0.0f;
        ActiveIdIsJustActivated                  = false;
        ActiveIdAllowOverlap                     = false;
        ActiveIdNoClearOnFocusLoss               = false;
        ActiveIdHasBeenPressedBefore             = false;
        ActiveIdHasBeenEditedBefore              = false;
        ActiveIdHasBeenEditedThisFrame           = false;
        ActiveIdFromShortcut                     = false;
        ActiveIdUsingNavDirMask                  = 0x00;
        ActiveIdClickOffset                      = ImVec2(-1, -1);
        ActiveIdWindow                           = nullptr;
        ActiveIdSource                           = ImGuiInputSource_None;
        ActiveIdMouseButton                      = -1;
        ActiveIdPreviousFrame                    = 0;
        memset(&DeactivatedItemData,       0, sizeof(DeactivatedItemData));
        memset(&ActiveIdValueOnActivation, 0, sizeof(ActiveIdValueOnActivation));
        LastActiveId                             = 0;
        LastActiveIdTimer                        = 0.0f;

        ActiveIdUsingNavDirMask                  = 0;
        ActiveIdUsingAllKeyboardKeys             = false;
    }

    void ActiveWidgetStateStorage::StoreCurrentState()
    {
        IsInitialized = true;

        ImGuiContext& g = *ImGui::GetCurrentContext();

        HoveredId                                = g.HoveredId;
        HoveredIdPreviousFrame                   = g.HoveredIdPreviousFrame;
        HoveredIdPreviousFrameItemCount          = g.HoveredIdPreviousFrameItemCount;
        HoveredIdTimer                           = g.HoveredIdTimer;
        HoveredIdNotActiveTimer                  = g.HoveredIdNotActiveTimer;
        HoveredIdAllowOverlap                    = g.HoveredIdAllowOverlap;
        HoveredIdIsDisabled                      = g.HoveredIdIsDisabled;
        ItemUnclipByLog                          = g.ItemUnclipByLog;
        ActiveId                                 = g.ActiveId;
        ActiveIdIsAlive                          = g.ActiveIdIsAlive;
        ActiveIdTimer                            = g.ActiveIdTimer;
        ActiveIdIsJustActivated                  = g.ActiveIdIsJustActivated;
        ActiveIdAllowOverlap                     = g.ActiveIdAllowOverlap;
        ActiveIdNoClearOnFocusLoss               = g.ActiveIdNoClearOnFocusLoss;
        ActiveIdHasBeenPressedBefore             = g.ActiveIdHasBeenPressedBefore;
        ActiveIdHasBeenEditedBefore              = g.ActiveIdHasBeenEditedBefore;
        ActiveIdHasBeenEditedThisFrame           = g.ActiveIdHasBeenEditedThisFrame;
        ActiveIdFromShortcut                     = g.ActiveIdFromShortcut;
        ActiveIdMouseButton                      = g.ActiveIdMouseButton;
        ActiveIdClickOffset                      = g.ActiveIdClickOffset;
        ActiveIdWindow                           = g.ActiveIdWindow;
        ActiveIdSource                           = g.ActiveIdSource;
        ActiveIdPreviousFrame                    = g.ActiveIdPreviousFrame;
        DeactivatedItemData                      = *(ImGuiDeactivatedItemDataInternal*)&g.DeactivatedItemData;
        ActiveIdValueOnActivation                = *(ImGuiDataTypeStorageInternal*)&g.ActiveIdValueOnActivation;
        LastActiveId                             = g.LastActiveId;
        LastActiveIdTimer                        = g.LastActiveIdTimer;

        ActiveIdUsingNavDirMask                  = g.ActiveIdUsingNavDirMask;
        ActiveIdUsingAllKeyboardKeys             = g.ActiveIdUsingAllKeyboardKeys;
    }

    void ActiveWidgetStateStorage::ApplyState()
    {
        //Do nothing if not initialized. Calls to this are typically followed by storing the state afterwards, initializing it correctly then
        if (!IsInitialized)
            return;

        ImGuiContext& g = *ImGui::GetCurrentContext();

        g.HoveredId                                = HoveredId;
        g.HoveredIdPreviousFrame                   = HoveredIdPreviousFrame;
        g.HoveredIdPreviousFrameItemCount          = HoveredIdPreviousFrameItemCount;
        g.HoveredIdTimer                           = HoveredIdTimer;
        g.HoveredIdNotActiveTimer                  = HoveredIdNotActiveTimer;
        g.HoveredIdAllowOverlap                    = HoveredIdAllowOverlap;
        g.HoveredIdIsDisabled                      = HoveredIdIsDisabled;
        g.ItemUnclipByLog                          = ItemUnclipByLog;
        g.ActiveId                                 = ActiveId;
        g.ActiveIdIsAlive                          = ActiveIdIsAlive;
        g.ActiveIdTimer                            = ActiveIdTimer;
        g.ActiveIdIsJustActivated                  = ActiveIdIsJustActivated;
        g.ActiveIdAllowOverlap                     = ActiveIdAllowOverlap;
        g.ActiveIdNoClearOnFocusLoss               = ActiveIdNoClearOnFocusLoss;
        g.ActiveIdHasBeenPressedBefore             = ActiveIdHasBeenPressedBefore;
        g.ActiveIdHasBeenEditedBefore              = ActiveIdHasBeenEditedBefore;
        g.ActiveIdHasBeenEditedThisFrame           = ActiveIdHasBeenEditedThisFrame;
        g.ActiveIdFromShortcut                     = ActiveIdFromShortcut;
        g.ActiveIdMouseButton                      = ActiveIdMouseButton;
        g.ActiveIdClickOffset                      = ActiveIdClickOffset;
        g.ActiveIdWindow                           = (ImGuiWindow*)ActiveIdWindow;
        g.ActiveIdSource                           = (ImGuiInputSource)ActiveIdSource;
        g.ActiveIdPreviousFrame                    = ActiveIdPreviousFrame;
        g.DeactivatedItemData                      = *(ImGuiDeactivatedItemData*)&DeactivatedItemData;
        g.ActiveIdValueOnActivation                = *(ImGuiDataTypeStorage*)&ActiveIdValueOnActivation;
        g.LastActiveId                             = LastActiveId;
        g.LastActiveIdTimer                        = LastActiveIdTimer;

        g.ActiveIdUsingNavDirMask                  = ActiveIdUsingNavDirMask;
        g.ActiveIdUsingAllKeyboardKeys             = ActiveIdUsingAllKeyboardKeys;
    }

    void ActiveWidgetStateStorage::AdvanceState()
    {
        if (!IsInitialized)
            return;

        const ImGuiIO& io = ImGui::GetIO();
        ImGuiContext& g = *ImGui::GetCurrentContext();

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
        HoveredId = 0;
        HoveredIdAllowOverlap = false;
        HoveredIdIsDisabled = false;

        // Clear ActiveID if the item is not alive anymore.
        // In 1.87, the common most call to KeepAliveID() was moved from GetID() to ItemAdd().
        // As a result, custom widget using ButtonBehavior() _without_ ItemAdd() need to call KeepAliveID() themselves.
        if (ActiveId != 0 && ActiveIdIsAlive != ActiveId && ActiveIdPreviousFrame == ActiveId)
        {
            //ClearActiveID
            ImGuiDeactivatedItemDataInternal* deactivated_data = &DeactivatedItemData;
            deactivated_data->ID = ActiveId;
            deactivated_data->ElapseFrame = (g.LastItemData.ID == ActiveId) ? g.FrameCount : g.FrameCount + 1;
            deactivated_data->HasBeenEditedBefore = ActiveIdHasBeenEditedBefore;
            deactivated_data->IsAlive = (ActiveIdIsAlive == ActiveId);

            ActiveIdIsJustActivated        = true;
            ActiveIdTimer                  = 0.0f;
            ActiveIdHasBeenPressedBefore   = false;
            ActiveIdHasBeenEditedBefore    = false;
            ActiveIdHasBeenEditedThisFrame = false;
            ActiveIdFromShortcut           = false;
            ActiveIdMouseButton            = -1;
            ActiveId                       = 0;
            ActiveIdAllowOverlap           = false;
            ActiveIdNoClearOnFocusLoss     = false;
            ActiveIdWindow                 = nullptr;
            ActiveIdHasBeenEditedThisFrame = false;
        }

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
        ActiveIdIsAlive                          = 0;
        ActiveIdHasBeenEditedThisFrame           = false;
        ActiveIdIsJustActivated                  = false;

        if (ActiveId == 0)
        {
            ActiveIdUsingNavDirMask      = 0x00;
            ActiveIdUsingAllKeyboardKeys = false;
        }

        if (DeactivatedItemData.ElapseFrame < g.FrameCount)
            DeactivatedItemData.ID = 0;
        DeactivatedItemData.IsAlive = false;
    }
}