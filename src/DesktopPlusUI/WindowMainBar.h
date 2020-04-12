#pragma once

#include "imgui.h"

class WindowSettings;

//The main bar visible below the desktop mirror in the dashboard, containing desktop buttons and user-defined action buttons
class WindowMainBar
{
    private:
        ImVec2 m_Pos;
        ImVec2 m_Size;
        WindowSettings* m_WndSettingsPtr;

        void DisplayTooltipIfHovered(const char* text);
        void UpdateDesktopButtons();
        void UpdateActionButtons();

    public:
        WindowMainBar(WindowSettings* settings_window);

        void Update();
        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;
};