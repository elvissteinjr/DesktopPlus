#pragma once

#include "imgui.h"


//The main bar visible in the floating UI, containing buttons to hide the overlay, toggle drag-mode and show the Action-Bar
class WindowFloatingUIMainBar
{
    private:
        ImVec2 m_Pos;
        ImVec2 m_Size;

        int m_IsCurrentWindowCapturable;        //-1 = needs update, otherwise bool
        float m_AnimationProgress;

        void DisplayTooltipIfHovered(const char* text);

    public:
        WindowFloatingUIMainBar();

        void Update(float mainbar_height, unsigned int overlay_id);

        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;
        float GetAnimationProgress() const;

        void MarkCurrentWindowCapturableStateOutdated();
};

//Action-Bar visible in the floating UI, containing desktop buttons and user-defined action buttons
class WindowFloatingUIActionBar
{
    private:
        ImVec2 m_Pos;
        ImVec2 m_Size;
        bool m_Visible;
        float m_Alpha;

        double m_LastDesktopSwitchTime;

        void DisplayTooltipIfHovered(const char* text);
        void UpdateDesktopButtons(unsigned int overlay_id);
        void UpdateActionButtons(unsigned int overlay_id);

        void ButtonActionKeyboard(unsigned int overlay_id, ImVec2& b_size_default);

    public:
        WindowFloatingUIActionBar();

        void Show(bool skip_fade = false);
        void Hide(bool skip_fade = false);
        void Update(unsigned int overlay_id);
        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;
        bool IsVisible() const;
        float GetAlpha() const;

        double GetLastDesktopSwitchTime() const;
};

//Extra window currently only showing capture fps of the overlay if enabled
class WindowFloatingUIOverlayStats
{
    public:
        void Update(const WindowFloatingUIMainBar& mainbar, const WindowFloatingUIActionBar& actionbar, unsigned int overlay_id);
};