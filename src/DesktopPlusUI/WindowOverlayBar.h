#pragma once

#include "imgui.h"
#include "OverlayManager.h"

class WindowSettings;

//The bar visible below the dashboard, containing overlay buttons and access to the settings window
class WindowOverlayBar
{
    private:
        ImVec2 m_Pos;
        ImVec2 m_Size;
        bool m_Visible; //This and m_Alpha default to visible state, meaning the dashboard UI, where it's always visible doesn't need to call Show()
        float m_Alpha;
        bool m_IsScrollBarVisible;

        unsigned int m_OverlayButtonActiveMenuID;
        bool m_IsAddOverlayButtonActive;
        float m_MenuAlpha;
        bool m_IsMenuRemoveConfirmationVisible;
        bool m_IsDraggingOverlayButtons;

        void DisplayTooltipIfHovered(const char* text, unsigned int overlay_id = k_ulOverlayID_None);
        void UpdateOverlayButtons();
        void MenuOverlayButton(unsigned int overlay_id, ImVec2 pos, bool is_item_active);
        void MenuAddOverlayButton(ImVec2 pos, bool is_item_active);

    public:
        WindowOverlayBar();

        void Show(bool skip_fade = false);
        void Hide(bool skip_fade = false);
        void HideMenus();
        void Update();

        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;

        bool IsVisible() const;
        bool IsVisibleOrFading() const;
        bool IsAnyMenuVisible() const;
        bool IsScrollBarVisible() const;
        bool IsDraggingOverlayButtons() const;

        float GetAlpha() const;
};