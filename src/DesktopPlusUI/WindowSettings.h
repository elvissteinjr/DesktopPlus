#pragma once

#include <vector>
#include <string>

#include "imgui.h"

#include "ConfigManager.h"

//The entire settings interface
class WindowSettings
{
    private:
        ImVec2 m_Size;
        bool m_Visible;
        float m_Alpha;

        bool m_ActionEditIsNew;
        bool m_OverlayNameBufferNeedsUpdate;

        void UpdateWarnings();
        void UpdateCatOverlay();
        void UpdateCatInterface();
        void UpdateCatInput();
        void UpdateCatPerformance();
        void UpdateCatMisc();

        bool ButtonKeybind(unsigned char* key_code);
        bool ButtonAction(ActionID& action_id);
        void ProfileSelector(bool multi_overlay);
        void UpdateLimiterSetting(float column_width_0, bool is_override = false);
        bool ActionButtonRow(ActionID action_id, int list_pos, int& list_selected_pos);
        bool PopupCurrentOverlayChange();
        void PopupNewOverlayProfile(std::vector<std::string>& overlay_profile_list, int& overlay_profile_selected_id, bool multi_overlay);
        void PopupActionEdit(CustomAction& action, int id);
        void PopupOverlayDetachedPositionChange();
        bool PopupIconSelect(std::string& filename);

        void HighlightOverlay(int overlay_id);

    public:
        WindowSettings();
        void Show();
        void Hide();
        void Update();

        bool IsShown() const;
        const ImVec2& GetSize() const;
};