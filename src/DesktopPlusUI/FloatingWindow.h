#pragma once

#include "OverlayDragger.h"
#include "TextureManager.h"
#include "TranslationManager.h"
#include <string>

//Base class for drag-able floating overlay windows, such as the Settings, Overlay Properties and Keyboard windows
class FloatingWindow
{
    protected:
        float m_OvrlWidth;
        float m_Alpha;
        bool m_Visible;
        bool m_OvrlVisible;
        bool m_IsPinned;

        std::string m_WindowTitle;
        std::string m_WindowID;
        TRMGRStrID m_WindowTitleStrID;
        TMNGRTexID m_WindowIcon;
        int m_WindowIconWin32IconCacheID; //TextureManager Icon cache ID when using a Win32 window icon as the ImGui window icon
        Matrix4 m_Transform;

        ImVec2 m_Pos;
        ImVec2 m_PosPivot;
        ImVec2 m_Size;                   //Set in derived constructor, 2 pixel-wide padding around actual texture space expected
        ImGuiWindowFlags m_WindowFlags;
        float m_TitleBarWidth;
        float m_TitleBarTitleMaxWidth;   //Width available for the title string without icon and buttons
        bool m_HasAppearedOnce;

        void WindowUpdateBase();         //Sets up ImGui window with custom title bar, pinning and overlay-based dragging
        virtual void WindowUpdate() = 0; //Window content, called within an ImGui Begin()'d window

        virtual void OnWindowPinButtonPressed();         //Called when the pin button is pressed, after updating m_IsPinned
        virtual bool IsVirtualWindowItemHovered() const; //Returns false by default, can be overridden to signal hover state of widgets that don't touch global ImGui state (used for blank space drag)

        void HelpMarker(const char* desc, const char* marker_str = "(?)") const;    //Help marker, but tooltip is fixed to top or bottom of the window
        bool TranslatedComboAnimated(const char* label, int& value, TRMGRStrID trstr_min, TRMGRStrID trstr_max) const;

    public:
        FloatingWindow();
        virtual ~FloatingWindow() = default;
        void Update();                   //Not called when idling (no windows visible)
        void UpdateVisibility();         //Only called in VR mode

        virtual void Show(bool skip_fade = false);
        virtual void Hide(bool skip_fade = false);
        bool IsVisible() const;
        bool IsVisibleOrFading() const;  //Returns true if m_Visible is true *or* m_Alpha isn't 0 yet
        float GetAlpha() const;

        bool IsPinned() const;
        void SetPinned(bool is_pinned);

        Matrix4& GetTransform();
        void SetTransform(Matrix4& transform);
        virtual void ResetTransform();
        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;
        virtual vr::VROverlayHandle_t GetOverlayHandle() const = 0;
};