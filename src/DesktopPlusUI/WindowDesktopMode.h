#pragma once

#include <vector>
#include "imgui.h"

#include "OverlayManager.h"

enum WindowDesktopModePage
{
    wnddesktopmode_page_none,
    wnddesktopmode_page_main,
    wnddesktopmode_page_settings,
    wnddesktopmode_page_profiles,
    wnddesktopmode_page_properties,
    wnddesktopmode_page_add_window_overlay
};

//Interop functions for windows that have their content hosted as a sub-page in desktop mode
class FloatingWindowDesktopModeInterop
{
    public:
        virtual const char* DesktopModeGetTitle() const = 0;
        virtual bool DesktopModeGetIconTextureInfo(ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const = 0;   //Returns false on no icon
        virtual float DesktopModeGetTitleIconAlpha() const       { return 1.0f; }
        virtual void DesktopModeOnTitleBarHover(bool is_hovered) {};
        virtual bool DesktopModeGoBack() = 0;                                                                 //Returns false if already on main page
};

class WindowDesktopMode
{
    private:
        std::vector<WindowDesktopModePage> m_PageStack;
        int m_PageStackPos = 0;
        int m_PageStackPosAnimation = 0;
        WindowDesktopModePage m_PageAppearing = wnddesktopmode_page_none; //Similar to ImGui::IsWindowAppearing(), equals the current page ID for a single frame if it or the window is newly appearing
        WindowDesktopModePage m_PageReturned  = wnddesktopmode_page_none; //Equals the previous page ID after PageGoBack() was called, ideally cleared after making use of its value

        ImVec4 m_TitleBarRect;

        int m_PageAnimationDir        = 0;
        float m_PageAnimationProgress = 0.0f;
        float m_PageAnimationStartPos = 0.0f;
        float m_PageAnimationOffset   = 0.0f;

        unsigned int m_OverlayListActiveMenuID = k_ulOverlayID_None;
        float m_MenuAlpha                      = 0.0f;
        bool m_IsOverlayAddMenuVisible         = false;
        bool m_IsMenuRemoveConfirmationVisible = false;
        bool m_IsDraggingOverlaySelectables    = false;

        void UpdateTitleBar();
        void UpdatePageMain();
        void UpdatePageMainOverlayList();
        void UpdatePageAddWindowOverlay();

        void MenuOverlayList(unsigned int overlay_id);
        void MenuAddOverlay();
        void HideMenus();

        void PageGoForward(WindowDesktopModePage new_page);
        void PageGoBack();

    public:
        WindowDesktopMode();
        void Update();

        ImVec4 GetTitleBarRect() const;
};