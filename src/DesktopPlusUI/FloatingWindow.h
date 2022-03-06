#pragma once

#include "OverlayDragger.h"
#include "TextureManager.h"
#include "TranslationManager.h"
#include <string>

enum FloatingWindowOverlayStateID
{
    floating_window_ovrl_state_room,
    floating_window_ovrl_state_dashboard_tab
};

struct FloatingWindowOverlayState
{
    bool IsVisible = false;
    bool IsPinned  = false;
    float Size = 1.0f;
    Matrix4 Transform;
    Matrix4 TransformAbs;
};

//Base class for drag-able floating overlay windows, such as the Settings, Overlay Properties and Keyboard windows
class FloatingWindow
{
    protected:
        float m_OvrlWidth;
        float m_Alpha;
        bool m_OvrlVisible;
        bool m_IsTransitionFading;

        FloatingWindowOverlayStateID m_OverlayStateCurrentID;
        FloatingWindowOverlayState m_OverlayStateRoom;
        FloatingWindowOverlayState m_OverlayStateDashboardTab;
        FloatingWindowOverlayState m_OverlayStateFading;
        FloatingWindowOverlayState* m_OverlayStateCurrent;
        FloatingWindowOverlayState* m_OverlayStatePending;

        std::string m_WindowTitle;
        std::string m_WindowID;
        TRMGRStrID m_WindowTitleStrID;
        TMNGRTexID m_WindowIcon;
        int m_WindowIconWin32IconCacheID; //TextureManager Icon cache ID when using a Win32 window icon as the ImGui window icon

        ImVec2 m_Pos;
        ImVec2 m_PosPivot;
        ImVec2 m_Size;                   //Set in derived constructor, 2 pixel-wide padding around actual texture space expected
        ImGuiWindowFlags m_WindowFlags;
        bool m_AllowRoomUnpinning;       //Set to enable pin button while room overlay state is active
        OverlayOrigin m_DragOrigin;      //Origin passed to OverlayDragger for window drags, doesn't affect overlay positioning (override relevant functions instead)

        float m_TitleBarWidth;
        float m_TitleBarTitleMaxWidth;   //Width available for the title string without icon and buttons
        bool m_HasAppearedOnce;
        bool m_IsWindowAppearing;

        void WindowUpdateBase();         //Sets up ImGui window with custom title bar, pinning and overlay-based dragging
        virtual void WindowUpdate() = 0; //Window content, called within an ImGui Begin()'d window

        void OverlayStateSwitchCurrent(bool use_dashboard_tab);
        void OverlayStateSwitchFinish();

        virtual void OnWindowPinButtonPressed();         //Called when the pin button is pressed, after updating overlay state
        virtual void OnWindowCloseButtonPressed();       //Called when the close button is pressed, after updating overlay state
        virtual bool IsVirtualWindowItemHovered() const; //Returns false by default, can be overridden to signal hover state of widgets that don't touch global ImGui state (used for blank space drag)

        void HelpMarker(const char* desc, const char* marker_str = "(?)") const;    //Help marker, but tooltip is fixed to top or bottom of the window
        bool TranslatedComboAnimated(const char* label, int& value, TRMGRStrID trstr_min, TRMGRStrID trstr_max) const;

    public:
        FloatingWindow();
        virtual ~FloatingWindow() = default;
        void Update();                   //Not called when idling (no windows visible)
        virtual void UpdateVisibility(); //Only called in VR mode

        virtual void Show(bool skip_fade = false);
        virtual void Hide(bool skip_fade = false);
        bool IsVisible() const;
        bool IsVisibleOrFading() const;  //Returns true if m_Visible is true *or* m_Alpha isn't 0 yet
        float GetAlpha() const;

        bool CanUnpinRoom() const;
        bool IsPinned() const;
        void SetPinned(bool is_pinned, bool no_state_copy = false);  //no_state_copy = don't copy dashboard state to room (default behavior for pin button)

        FloatingWindowOverlayState& GetOverlayState(FloatingWindowOverlayStateID id);
        FloatingWindowOverlayStateID GetOverlayStateCurrentID();

        Matrix4& GetTransform();
        void SetTransform(const Matrix4& transform);
        virtual void ApplyCurrentOverlayState(); //Applies current absolute transform to the overlay if pinned and sets the width
        virtual void RebaseTransform();
        virtual void ResetTransformAll();
        virtual void ResetTransform(FloatingWindowOverlayStateID state_id);

        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;
        virtual vr::VROverlayHandle_t GetOverlayHandle() const = 0;
};