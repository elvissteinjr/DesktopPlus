//Some extra functions extending ImGui
//Hopefully nothing here breaks when updating ImGui

#pragma once

#include "imgui.h"
#include <string>

//More colors
extern ImVec4 Style_ImGuiCol_TextNotification;
extern ImVec4 Style_ImGuiCol_TextWarning;
extern ImVec4 Style_ImGuiCol_TextError;
extern ImVec4 Style_ImGuiCol_ButtonPassiveToggled; //Toggled state for a button indicating a passive state, rather a full-on eye-catching active state
extern ImVec4 Style_ImGuiCol_SteamVRCursor;        //Inner color used to mimic a SteamVR overlay cursor
extern ImVec4 Style_ImGuiCol_SteamVRCursorBorder;  //Border color used to mimic a SteamVR overlay cursor

namespace ImGui
{
    //Like InputFloat()'s buttons but with a slider instead. Not quite as flexible, though. Always takes as much space as available.
    //alt_text is unformatted text rendered on top if set (for unsanitized translation strings). format shouldn't render anything then (use ##)
    bool SliderWithButtonsFloat(const char* str_id, float& value, float step, float step_small, float min, float max, const char* format, ImGuiSliderFlags flags = 0, bool* used_button = nullptr, 
                                const char* text_alt = nullptr);
    bool SliderWithButtonsInt(const char* str_id, int& value, int step, int step_small, int min, int max, const char* format, ImGuiSliderFlags flags = 0, bool* used_button = nullptr, 
                              const char* text_alt = nullptr);
    // format is for int
    bool SliderWithButtonsFloatPercentage(const char* str_id, float& value, int step, int step_small, int min, int max, const char* format, ImGuiSliderFlags flags = 0, bool* used_button = nullptr, 
                                          const char* text_alt = nullptr);
    ImGuiID SliderWithButtonsGetSliderID(const char* str_id);

    //Like imgui_demo's HelpMarker, but with a fixed position tooltip
    void FixedHelpMarker(const char* desc, const char* marker_str = "(?)");

    //Button, but with wrapped, cropped and center aligned label
    bool ButtonWithWrappedLabel(const char* label, const ImVec2& size);

    //BeginCombo() but with the option of turning the Combo into an Input text like the sliders. Little bit awkward with the state variables but works otherwise
    //Uses PopupContextMenuInputText() with the InputText
    //Use normal EndCombo() to finish
    bool BeginComboWithInputText(const char* str_id, char* str_buffer, size_t buffer_size, bool& out_buffer_changed,
                                 bool& persist_input_visible, bool& persist_input_activated, bool& persist_mouse_released_once, bool no_preview_text = false);
    void ComboWithInputTextActivationCheck(bool& persist_input_visible); //Always call after BeginComboWithInputText(), outside the if statement

    //BeginCombo(), but opening it is animated
    bool BeginComboAnimated(const char* label, const char* preview_value, ImGuiComboFlags flags = 0);

    //Right-alinged Text(). Use offset_x if it's not supposed to take all of the available space. Note that text may not always be pixel-perfectly aligned with this
    void TextRight(float offset_x, const char* fmt, ...)           IM_FMTARGS(2);
    void TextRightV(float offset_x, const char* fmt, va_list args) IM_FMTLIST(2);
    void TextRightUnformatted(float offset_x, const char* text, const char* text_end = nullptr);

    //Shortcut for unformatted colored text
    void TextColoredUnformatted(const ImVec4& col, const char* text, const char* text_end = nullptr);

    //ColorPicker simplified for embedded widget use instead having of popups + translation support
    bool ColorPicker4Simple(const char* str_id, float col[4], float ref_col[4], const char* label_color_current = nullptr, const char* label_color_original = nullptr);

    //ImGuiItemFlags_Disabled is not exposed public API yet and has no styling, so here's something that does the job
    //BeginDisabled()/EndDisabled() exists now, but behaves slightly differently and styling may change, so let's keep this for the time being
    void PushItemDisabled();
    void PopItemDisabled();    
    void PushItemDisabledNoVisual();
    void PopItemDisabledNoVisual();

    //Just straight up brought into the public API, use -1.0f on one axis to leave as-is
    void SetNextWindowScroll(const ImVec2& scroll);

    //Brought into the public API since we need only that one sometimes
    void ClearActiveID();

    //Get and set previous line height. Useful on complex layouts where a widget may take more height while not being supposed to push the next line further down
    float GetPreviousLineHeight();
    void SetPreviousLineHeight(float height);

    //Something loosely resembling an InputText context menu, except it doesn't operate on the current cursor position or selection at all. Returns if buffer was modified
    bool PopupContextMenuInputText(const char* str_id, char* str_buffer, size_t buffer_size, bool paste_remove_newlines = true);

    //Returns true if the hovered item has changed to a different one
    bool HasHoveredNewItem();

    //Returns true if any item is or was active in the previous frame
    bool IsAnyItemActiveOrDeactivated();

    //Returns true if any InputText is active
    //Use IsAnyTempInputTextActive() to handle widgets creating temp InputTexts instead as the text state ID doesn't get cleared and it can't tell normal use and text input apart
    bool IsAnyInputTextActive();

    //Returns true if any temp InputText (created for sliders and such) is active
    bool IsAnyTempInputTextActive();

    //Returns true if any mouse button is clicked
    bool IsAnyMouseClicked();

    //Takes active input focus and prevents scrolling on any ImGui widget while leaving input state untouched otherwise
    void BlockWidgetInput();

    //Scroll window horizontally from vertical mouse wheel input
    void HScrollWindowFromMouseWheelV();

    //Scroll the parent window in begin stack from current mouse wheel input (real child windows can scroll their parents by default already)
    void ScrollBeginStackParentWindow();

    //Returns true if either scroll bar is visible
    bool IsAnyScrollBarVisible();

    //Adjusts cursor pos and clipping rect to enable custom title bar content (assumes default left-aligned title). Returns title bar screen rect as X, Y, X2, Y2 coordinates
    ImVec4 BeginTitleBar();
    void EndTitleBar();

    //Returns true if a character in the string is mapped in the active font
    bool StringContainsUnmappedCharacter(const char* str);

    //Returns string shortened to fit width_max in the active font
    std::string StringEllipsis(const char* str, float width_max);

    //Stores an ImGui mouse state or something that would like to be one and can set from or apply to global state if needed. 
    //Somewhat hacky, but pretty unproblematic when used correctly.
    class ImGuiMouseState
    {
        public:
            ImVec2 MousePos;
            bool   MouseDown[5];
            float  MouseWheel;
            float  MouseWheelH;
            ImVec2 MouseDelta;
            ImVec2 MousePosPrev;
            ImVec2 MouseClickedPos[5];
            double MouseClickedTime[5];
            bool   MouseClicked[5];
            bool   MouseDoubleClicked[5];
            ImU16  MouseClickedCount[5];
            ImU16  MouseClickedLastCount[5];
            bool   MouseReleased[5];
            bool   MouseDownOwned[5];
            bool   MouseDownOwnedUnlessPopupClose[5];
            float  MouseDownDuration[5];
            float  MouseDownDurationPrev[5];
            float  MouseDragMaxDistanceSqr[5];

            ImGuiMouseState();
            void SetFromGlobalState();
            void ApplyToGlobalState();
            void Advance();        //Advance state similar to what happens in ImGui::NewFrame();
    };

    //Very dirty hack. Allows storing and restoring the active widget state. Works fine for a bunch of buttons not interrupting an InputText(), but likely breaks for anything complex.
    //A separate ImGuiContext would be the sane option, but that comes with its own complications.
    class ActiveWidgetStateStorage
    {
        private:
            bool    IsInitialized;

            ImGuiID HoveredId;
            ImGuiID HoveredIdPreviousFrame;
            bool    HoveredIdAllowOverlap;
            bool    HoveredIdUsingMouseWheel;
            bool    HoveredIdPreviousFrameUsingMouseWheel;
            bool    HoveredIdDisabled;
            float   HoveredIdTimer;
            float   HoveredIdNotActiveTimer; 
            ImGuiID ActiveId;
            ImGuiID ActiveIdIsAlive;
            float   ActiveIdTimer;
            bool    ActiveIdIsJustActivated;
            bool    ActiveIdAllowOverlap;
            bool    ActiveIdNoClearOnFocusLoss;
            bool    ActiveIdHasBeenPressedBefore;
            bool    ActiveIdHasBeenEditedBefore;
            bool    ActiveIdHasBeenEditedThisFrame;
            bool    ActiveIdUsingMouseWheel;
            ImU32   ActiveIdUsingNavDirMask;
            ImU32   ActiveIdUsingNavInputMask;
            ImU32   ActiveIdUsingKeyInputMask[5];
            ImVec2  ActiveIdClickOffset;
            void*   ActiveIdWindow;
            int     ActiveIdSource;
            int     ActiveIdMouseButton;
            ImGuiID ActiveIdPreviousFrame;
            bool    ActiveIdPreviousFrameIsAlive;
            bool    ActiveIdPreviousFrameHasBeenEditedBefore;
            void*   ActiveIdPreviousFrameWindow;
            ImGuiID LastActiveId;
            float   LastActiveIdTimer;

        public:
            ActiveWidgetStateStorage();
            void StoreCurrentState();
            void ApplyState();
            void AdvanceState();        //Advance state similar to what happens in ImGui::NewFrame();
    };
}