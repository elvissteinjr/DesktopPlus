#include "DashboardUI.h"

#include "UIManager.h"
#include "OverlayManager.h"

DashboardUI::DashboardUI() : m_WindowMainBar(&m_WindowSettings)
{

}

void DashboardUI::Update()
{
    m_WindowSettings.Update();

    if (!UIManager::Get()->IsInDesktopMode())
    {
        m_WindowMainBar.Update(k_ulOverlayID_Dashboard);
    }
}

WindowSettings& DashboardUI::GetSettingsWindow()
{
    return m_WindowSettings;
}
