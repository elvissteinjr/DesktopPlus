#pragma once

#include "imgui.h"

class WindowSettings;

//The main bar visible below the desktop mirror in the dashboard, containing desktop buttons and user-defined action buttons
//Also used as action bar by the floating UI
class WindowMainBar
{
    private:
        ImVec2 m_Pos;
        ImVec2 m_Size;
        bool m_Visible; //This and m_Alpha default to visible state, meaning the dashboard UI, where it's always visible doesn't need to call Show()
        float m_Alpha;
        WindowSettings* m_WndSettingsPtr;

        void DisplayTooltipIfHovered(const char* text);
        void UpdateDesktopButtons();
        void UpdateActionButtons();

    public:
        WindowMainBar(WindowSettings* settings_window = nullptr);

        void Show(bool skip_fade = false);
        void Hide(bool skip_fade = false);
        void Update();
        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;
        bool IsVisible() const;
        float GetAlpha() const;
};