#include "ImGuiExt.h"

#include <string>

#include "imgui_internal.h"
#include "UIManager.h"

ImVec4 Style_ImGuiCol_TextWarning;
ImVec4 Style_ImGuiCol_TextError;

namespace ImGui
{
    //Like InputFloat()'s buttons but with a slider instead. Not quite as flexible, though. Always takes as much space as available.
    bool SliderWithButtonsFloat(const char* str_id, float& value, float step, float min, float max, const char* format, float power, bool* used_button)
    {
        //Hacky solution to make right mouse enable text input on the slider while not touching ImGui code or generalizing it as ctrl press
        ImGuiIO& io = ImGui::GetIO();
        const bool mouse_left_clicked_old = io.MouseClicked[0];
        const bool key_ctrl_old = io.KeyCtrl;

        if (io.MouseClicked[1])
        {
            io.MouseClicked[0] = true;
            io.KeyCtrl = true;
        }

        ImGuiStyle& style = ImGui::GetStyle();

        const float value_old = value;
        const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        ImGui::PushID(str_id);
        ImGui::PushButtonRepeat(true);

        //Calulate slider width (GetContentRegionAvail() returns 1 more than when using -1 width to fill)
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2) - 1.0f);
        ImGui::SliderFloat("##Slider", &value, min, max, format, power);

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
        io.MouseClicked[0] = mouse_left_clicked_old;
        io.KeyCtrl = key_ctrl_old;

        return (value != value_old);
    }

    bool SliderWithButtonsInt(const char* str_id, int& value, int step, int min, int max, const char* format, bool* used_button)
    {
        //Hacky solution to make right mouse enable text input on the slider while not touching ImGui code or generalizing it as ctrl press
        ImGuiIO& io = ImGui::GetIO();
        const bool mouse_left_clicked_old = io.MouseClicked[0];
        const bool key_ctrl_old = io.KeyCtrl;

        if (io.MouseDown[1])
        {
            io.MouseClicked[0] = true;
            io.KeyCtrl = true;
        }


        ImGuiStyle& style = ImGui::GetStyle();

        const int value_old = value;
        const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        ImGui::PushID(str_id);
        ImGui::PushButtonRepeat(true);

        //Calulate slider width (GetContentRegionAvail() returns 1 more than when using -1 width to fill)
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2) - 1.0f);
        ImGui::SliderInt("##Slider", &value, min, max, format);

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
        io.MouseClicked[0] = mouse_left_clicked_old;
        io.KeyCtrl = key_ctrl_old;

        return (value != value_old);
    }

    bool SliderWithButtonsFloatPercentage(const char* str_id, float& value, int step, int min, int max, const char* format, bool* used_button)
    {
        int value_ui = int(value * 100.0f);

        if (ImGui::SliderWithButtonsInt(str_id, value_ui, step, min, max, format, used_button))
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

    bool SliderWithButtonsEnum(const char* str_id, int& value, int min, int max, const char* format, bool* used_button)
    {
        //Hacky solution to block ctrl + left click entering edit mode on the slider
        ImGuiIO& io = ImGui::GetIO();
        const bool key_ctrl_old = io.KeyCtrl;

        io.KeyCtrl = false;


        ImGuiStyle& style = ImGui::GetStyle();

        const int value_old = value;
        const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        ImGui::PushID(str_id);
        ImGui::PushButtonRepeat(true);
        ImGui::PushAllowKeyboardFocus(false);

        //Calulate slider width (GetContentRegionAvail() returns 1 more than when using -1 width to fill)
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2) - 1.0f);
        ImGui::SliderInt("##Slider", &value, min, max, format);

        ImGui::PopAllowKeyboardFocus();

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("-", button_size))
        {
            value--;

            if (used_button)
                *used_button = true;
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("+", button_size))
        {
            value++;

            if (used_button)
                *used_button = true;
        }

        ImGui::PopButtonRepeat();
        ImGui::PopID();

        //Restore hack
        io.KeyCtrl = key_ctrl_old;

        return (value != value_old);
    }


    //Like imgui_demo's HelpMarker, but with a fixed position tooltip
    void FixedHelpMarker(const char* desc)
    {
        ImGui::TextDisabled("(?)");
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
            ImGui::TextUnformatted(desc);
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
        float cursor_pos_prev_line_y = window->DC.CursorPosPrevLine.y;
        ImGui::BeginChild("ButtonLabel", ImVec2(size.x + style.FramePadding.x, size.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);

        ImVec2 text_size = ImGui::CalcTextSize(label, nullptr, false, size.x);
        ImGui::SetCursorPosX( (size.x / 2.0f) - int(text_size.x / 2.0f) );
        ImGui::SetCursorPosY( (size.x / 2.0f) - int(text_size.y / 2.0f) );

        ImGui::PushTextWrapPos(size.x);
        ImGui::TextUnformatted(label);
        ImGui::PopTextWrapPos();

        ImGui::EndChild();
        ImGui::EndGroup();
        ImGui::PopID();
        window->DC.CursorPosPrevLine.y = cursor_pos_prev_line_y;

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

    bool PopupContextMenuInputText(const char* str_id, char* str_buffer, size_t buffer_size)
    {
        bool ret = false;

        if (ImGui::BeginPopupContextItem(str_id))
        {
            if (ImGui::MenuItem("Copy all"))
            {
                ImGui::SetClipboardText(str_buffer);
            }

            if (ImGui::MenuItem("Replace with Clipboard"))
            {
                std::string str(ImGui::GetClipboardText());
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
}