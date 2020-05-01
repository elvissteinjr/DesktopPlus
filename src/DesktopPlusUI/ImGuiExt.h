//Some extra functions extending ImGui
//Hopefully nothing here breaks when updating ImGui

#pragma once

#include "imgui.h"

//More colors
extern ImVec4 Style_ImGuiCol_TextWarning;
extern ImVec4 Style_ImGuiCol_TextError;

namespace ImGui
{
    //Like InputFloat()'s buttons but with a slider instead. Not quite as flexible, though. Always takes as much space as available.
    bool SliderWithButtonsFloat(const char* str_id, float& value, float step, float min, float max, const char* format, bool* used_button = nullptr);
    bool SliderWithButtonsInt(const char* str_id, int& value, int step, int min, int max, const char* format, bool* used_button = nullptr);
    bool SliderWithButtonsFloatPercentage(const char* str_id, float& value, int step, int min, int max, const char* format, bool* used_button = nullptr); //format is for int

    //Like imgui_demo's HelpMarker, but with a fixed position tooltip
    void FixedHelpMarker(const char* desc);

    //Button, but with wrapped, cropped and center aligned label
    bool ButtonWithWrappedLabel(const char* label, const ImVec2& size);

    //ImGuiItemFlags_Disabled is not exposed public API yet and has no styling, so here's something that does the job
    void PushItemDisabled();
    void PopItemDisabled();

    //Something loosely resembling an InputText context menu, except it doesn't operate on the current cursor position or selection at all. Returns if buffer was modified
    bool PopupContextMenuInputText(const char* str_id, char* str_buffer, size_t buffer_size);

    //Returns true if the hovered item has changed to a different one
    bool HasHoveredNewItem();
}