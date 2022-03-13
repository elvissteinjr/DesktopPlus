#pragma once

#include "WindowFloatingUIBar.h"

#include "openvr.h"

class FloatingUI
{
    private:
        WindowFloatingUIMainBar m_WindowMainBar;
        WindowFloatingUIActionBar m_WindowActionBar;
        WindowFloatingUIOverlayStats m_WindowOverlayStats;

        vr::VROverlayHandle_t m_OvrlHandleCurrentUITarget;
        unsigned int m_OvrlIDCurrentUITarget;

        float m_Width;
        float m_Alpha;
        bool m_Visible;
        bool m_IsSwitchingTarget;
        float m_FadeOutDelayCount;
        int m_AutoFitFrames;

    public:
        FloatingUI();
        void Update();
        void UpdateUITargetState();
        bool IsVisible() const;
        float GetAlpha() const;

        WindowFloatingUIMainBar& GetMainBarWindow();
        WindowFloatingUIActionBar& GetActionBarWindow();
};