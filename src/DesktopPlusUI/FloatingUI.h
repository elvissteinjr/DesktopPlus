#pragma once

#include "WindowMainBar.h"
#include "WindowSideBar.h"

#include "openvr.h"

class FloatingUI
{
    private:
        WindowMainBar m_WindowActionBar;
        WindowSideBar m_WindowSideBar;

        vr::VROverlayHandle_t m_OvrlHandleCurrentUITarget;
        unsigned int m_OvrlIDCurrentUITarget;

        float m_Alpha;
        bool m_Visible;
        bool m_IsSwitchingTarget;
        int m_FadeOutDelayCount;

        void UpdateUITargetState();

    public:
        FloatingUI();
        void Update();
        bool IsVisible() const;
};