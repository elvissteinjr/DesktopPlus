#include "DashboardUI.h"

#include "UIManager.h"

DashboardUI::DashboardUI() : m_WindowMainBar(&m_WindowSettings)
{

}

void DashboardUI::Update()
{
    m_WindowSettings.Update();

    if (!UIManager::Get()->IsInDesktopMode())
    {
        m_WindowMainBar.Update();
    }
}

WindowSettings& DashboardUI::GetSettingsWindow()
{
    return m_WindowSettings;
}
