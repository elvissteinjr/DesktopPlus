#pragma once

#include "imgui.h"

#include "ConfigManager.h"

//The entire settings interface
class WindowSettingsActionEdit
{
    private:
        ImVec2 m_Size;
        bool m_Visible;

        bool m_ActionEditIsNew;

        void UpdateWarnings();
        void UpdateCatActions();

        bool ButtonKeybind(unsigned char* key_code, bool no_mouse = false);
        bool ButtonAction(const char* str_id, ActionID& action_id);
        bool ButtonHotkey(unsigned int hotkey_id);
        void ActionOrderSetting(unsigned int overlay_id = UINT_MAX);
        bool ActionButtonRow(ActionID action_id, int list_pos, int& list_selected_pos, unsigned int overlay_id = UINT_MAX);

        void PopupActionEdit(CustomAction& action, int id);
        bool PopupIconSelect(std::string& filename);

    public:
        WindowSettingsActionEdit();
        void Show();
        void Update();

        bool IsShown() const;
        const ImVec2& GetSize() const;
};