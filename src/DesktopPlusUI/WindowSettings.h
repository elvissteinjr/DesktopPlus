#pragma once

#include <vector>
#include <string>

#include "imgui.h"

#include "ConfigManager.h"
#include "WindowList.h"

//The entire settings interface
class WindowSettings
{
    private:
        ImVec2 m_Size;
        bool m_Visible;
        float m_Alpha;

        bool m_ActionEditIsNew;
        bool m_OverlayNameBufferNeedsUpdate;
        ImGuiStyle m_StyleOrig;
        bool m_IsStyleScaled;

        void UpdateWarnings();
        void UpdateCatOverlay();
        void UpdateCatOverlayTabGeneral();
        void UpdateCatOverlayTabCapture();
        void UpdateCatOverlayTabAdvanced();
        void UpdateCatOverlayTabInterface();
        void UpdateCatInterface();
        void UpdateCatInput();
        void UpdateCatWindows();
        void UpdateCatPerformance();
        void UpdateCatMisc();

        void PushInterfaceScale();
        void PopInterfaceScale();

        bool ButtonKeybind(unsigned char* key_code);
        bool ButtonAction(ActionID& action_id);
        void ProfileSelector(bool multi_overlay);
        void UpdateWindowList(std::vector<WindowInfo>& window_list, HWND selected_window, std::string& selected_window_str);
        void ActionOrderSetting(unsigned int overlay_id = UINT_MAX);
        void UpdateLimiterSetting(float column_width_0, bool is_override = false);
        bool ActionButtonRow(ActionID action_id, int list_pos, int& list_selected_pos, unsigned int overlay_id = UINT_MAX);
        bool PopupCurrentOverlayManage();
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