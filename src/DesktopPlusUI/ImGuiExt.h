//Some extra functions extending ImGui
//Hopefully nothing here breaks when updating ImGui

#pragma once

#include "imgui.h"

//More colors
extern ImVec4 Style_ImGuiCol_TextWarning;
extern ImVec4 Style_ImGuiCol_TextError;
extern ImVec4 Style_ImGuiCol_ButtonPassiveToggled; //Toggled state for a button indicating a passive state, rather a full-on eye-catching active state

namespace ImGui
{
    //Like InputFloat()'s buttons but with a slider instead. Not quite as flexible, though. Always takes as much space as available.
    bool SliderWithButtonsFloat(const char* str_id, float& value, float step, float min, float max, const char* format, ImGuiSliderFlags flags = 0, bool* used_button = nullptr);
    bool SliderWithButtonsInt(const char* str_id, int& value, int step, int min, int max, const char* format, ImGuiSliderFlags flags = 0, bool* used_button = nullptr);
    // format is for int
    bool SliderWithButtonsFloatPercentage(const char* str_id, float& value, int step, int min, int max, const char* format, ImGuiSliderFlags flags = 0, bool* used_button = nullptr);

    //Like imgui_demo's HelpMarker, but with a fixed position tooltip
    void FixedHelpMarker(const char* desc, const char* marker_str = "(?)");

    //Button, but with wrapped, cropped and center aligned label
    bool ButtonWithWrappedLabel(const char* label, const ImVec2& size);

    //BeginCombo() but with the option of turning the Combo into an Input text like the sliders. Little bit awkward with the state variables but works otherwise
    //Uses PopupContextMenuInputText() with the InputText
    //Use normal EndCombo() to finish
    bool BeginComboWithInputText(const char* str_id, char* str_buffer, size_t buffer_size, bool& out_buffer_changed,
                                 bool& persist_input_visible, bool& persist_input_activated, bool& persist_mouse_released_once);
    void ComboWithInputTextActivationCheck(bool& persist_input_visible); //Always call after BeginComboWithInputText(), outside the if statement
    bool ChoiceTest();

    //ImGuiItemFlags_Disabled is not exposed public API yet and has no styling, so here's something that does the job
    void PushItemDisabled();
    void PopItemDisabled();

    //Something loosely resembling an InputText context menu, except it doesn't operate on the current cursor position or selection at all. Returns if buffer was modified
    bool PopupContextMenuInputText(const char* str_id, char* str_buffer, size_t buffer_size, bool paste_remove_newlines = true);

    //Returns true if the hovered item has changed to a different one
    bool HasHoveredNewItem();

    //Returns true if a character in the string is mapped in the active font
    bool StringContainsUnmappedCharacter(const char* str);
}