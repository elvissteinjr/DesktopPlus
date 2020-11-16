#include "ImGuiExt.h"

#include <string>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
    #define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"
#include "UIManager.h"

ImVec4 Style_ImGuiCol_TextWarning;
ImVec4 Style_ImGuiCol_TextError;
ImVec4 Style_ImGuiCol_ButtonPassiveToggled;

namespace ImGui
{
    //Like InputFloat()'s buttons but with a slider instead. Not quite as flexible, though. Always takes as much space as available.
    bool SliderWithButtonsFloat(const char* str_id, float& value, float step, float min, float max, const char* format, ImGuiSliderFlags flags, bool* used_button)
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

        ImGuiStyle& style = ImGui::GetStyle();

        const float value_old = value;
        const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        ImGui::PushID(str_id);
        ImGui::PushButtonRepeat(true);

        //Calulate slider width (GetContentRegionAvail() returns 1 more than when using -1 width to fill)
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2) - 1.0f);
        ImGui::SliderFloat("##Slider", &value, min, max, format, flags);

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

    bool SliderWithButtonsInt(const char* str_id, int& value, int step, int min, int max, const char* format, ImGuiSliderFlags flags, bool* used_button)
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

        ImGuiStyle& style = ImGui::GetStyle();

        const int value_old = value;
        const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        ImGui::PushID(str_id);
        ImGui::PushButtonRepeat(true);

        //Calulate slider width (GetContentRegionAvail() returns 1 more than when using -1 width to fill)
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2) - 1.0f);
        ImGui::SliderInt("##Slider", &value, min, max, format, flags);

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

        //Restore hack
        io.MouseClicked[ImGuiMouseButton_Left] = mouse_left_clicked_old;
        io.KeyCtrl = key_ctrl_old;
        if (!io.KeyCtrl)
        {
            io.KeyMods &= ~ImGuiKeyModFlags_Ctrl;
        }

        return (value != value_old);
    }

    bool SliderWithButtonsFloatPercentage(const char* str_id, float& value, int step, int min, int max, const char* format, ImGuiSliderFlags flags, bool* used_button)
    {
        int value_ui = int(value * 100.0f);

        if (ImGui::SliderWithButtonsInt(str_id, value_ui, step, min, max, format, flags, used_button))
        {
            value = value_ui / 100.0f;

            while (int(value * 100.0f) < value_ui) //Floating point hell hacky fix (slider can get stuck when using + button otherwise)
            {
                value += step / 10000.f;
            }

            return true;
        }

        return false;
    }

    //Like imgui_demo's HelpMarker, but with a fixed position tooltip
    void FixedHelpMarker(const char* desc, const char* marker_str)
    {
        ImGui::TextDisabled(marker_str);
        if (ImGui::IsItemHovered())
        {
            static float last_y_offset = FLT_MIN;       //Try to avoid getting the tooltip off-screen... the way it's done here is a bit messy to be fair

            float pos_y = ImGui::GetItemRectMin().y;

            if (last_y_offset == FLT_MIN) //Same as IsWindowAppearing except the former doesn't work before beginning the window which is too late for the position...
            {
                //We need to create the tooltip window for size calculations to happen but also don't want to see it... is there really not a better way?
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y));
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

    bool BeginComboWithInputText(const char* str_id, char* str_buffer, size_t buffer_size, bool& out_buffer_changed, bool& persist_input_visible, bool& persist_input_activated, bool& persist_mouse_released_once)
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

        return (ImGui::BeginCombo(str_id, str_buffer, (persist_input_visible) ? (ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft) : ImGuiComboFlags_None));
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

    bool ChoiceTest()
    {
        ImGuiContext& g = *GImGui;

        static char buffer_overlay_name[1024];
        static int current_overlay = 0;

        static bool is_input_text_visible = false;
        static bool is_input_text_activated = false;
        static bool mouse_released_once = false;

        bool ret = false;

        if (is_input_text_visible)
        {
            g.NextItemData.Width -= ImGui::GetFrameHeight();

            if ((ImGui::InputText("##InputOverlayName", buffer_overlay_name, 1024)))
            {
                ret = true;
            }

            ImGuiID input_text_id = ImGui::GetItemID();

            if ( (is_input_text_activated) && (mouse_released_once) && (ImGui::PopupContextMenuInputText("##InputOverlayName", buffer_overlay_name, 1024)) )
            {
                ret = true;
            }

           
            //ImGui::SetFocusID(, ImGui::GetCurrentWindow());
            if (!is_input_text_activated)
            {
                ImGui::ActivateItem(ImGui::GetItemID());
                is_input_text_activated = true;
            }
            else if ( (!ImGui::IsPopupOpen("##InputOverlayName")) && ( (ImGui::IsItemDeactivated()) || (g.ActiveId != input_text_id) ) )
            {
                is_input_text_visible = false;
                is_input_text_activated = false;
                mouse_released_once = false;
                UIManager::Get()->RepeatFrame();
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            {
                mouse_released_once = true;
            }

            ImGui::SameLine(0.0f, 0.0f);
        }

        {

            if (ImGui::BeginCombo("##ComboOverlaySelector", buffer_overlay_name, (is_input_text_visible) ? ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft : ImGuiComboFlags_None ))
            {
                int index_hovered = -1;

                for (unsigned int i = 0; i < 10; ++i)
                {
                    if (ImGui::Selectable("A", (i == current_overlay)))
                    {
                        current_overlay = i;
                    }

                    if (ImGui::IsItemHovered())
                    {
                        index_hovered = i;
                    }
                }

                ImGui::EndCombo();
            }
        }

        //Right-click or Ctrl+Left-click to edit
        if ( (ImGui::IsItemClicked(ImGuiMouseButton_Right)) || ((ImGui::IsItemClicked(ImGuiMouseButton_Left)) && g.IO.KeyCtrl) )
        {
            is_input_text_visible = true;
        }

        return ret;
    }

    //ImGuiItemFlags_Disabled is not exposed public API yet and has no styling, so here's something that does the job
    void PushItemDisabled()
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    void PopItemDisabled()
    {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
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

    bool HasHoveredNewItem()
    {
        ImGuiContext& g = *GImGui;
        bool blocked_by_active_item = (g.ActiveId != 0 && !g.ActiveIdAllowOverlap);

        return ( (g.HoveredId != g.HoveredIdPreviousFrame) && (g.HoveredId != 0) && (!blocked_by_active_item) );
    }

    bool StringContainsUnmappedCharacter(const char* str)
    {
        const char* str_end = str + strlen(str);
        ImWchar32 c;
        int decoded_length;

        while (str < str_end)
        {
            decoded_length = ImTextCharFromUtf8(&c, str, str_end);

            if (ImGui::GetFont()->FindGlyphNoFallback((ImWchar)c) == nullptr)
            {
                return true;
            }

            str += decoded_length;
        }

        return false;
    }
}