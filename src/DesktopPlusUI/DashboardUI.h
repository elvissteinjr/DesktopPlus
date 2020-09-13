#pragma once

#include "WindowMainBar.h"
#include "WindowSettings.h"

class DashboardUI
{
    private:
        WindowMainBar m_WindowMainBar;
        WindowSettings m_WindowSettings;

    public:
        DashboardUI();

        void Update();
        WindowSettings& GetSettingsWindow();
};