#pragma once

#define NOMINMAX
#include <windows.h>

#include "imgui.h"
#include "openvr.h"
#include "Matrices.h"

//Hosts and manages UI that is displayed on the DesktopPlusUIAux overlay
//Generally rarely and not parallel displayed things go here

//Order of UI ID also determines priority. A higher priority UI can force an active lower one to stop displaying
enum AuxUIID
{
    auxui_none,
    auxui_drag_hint,
    auxui_gazefade_auto_hint,
    auxui_window_select
};

class AuxUIWindow
{
    protected:
        const AuxUIID m_AuxUIID;
        ImVec2 m_Pos;                          //Used for overlay texture bounds, always top-left pivot, should already be offset for 2x2 pixel padding
        ImVec2 m_Size;                         //Used for overlay texture bounds, read as max size for ImGui window when using auto-sizing

        bool m_Visible;
        float m_Alpha;
        bool m_IsTransitionFading;             //If true, AuxUIWindow does not clear the active UI but instead shows the window again to transition to a different position with a fade
        int m_AutoSizeFrames;                  //Overlay setup may need to be delayed when the window uses auto-sizing. In that case m_Alpha stays 0.0f while m_AutoSizeFrames is not 0

        bool WindowUpdateBase();               //Handles alpha fading according to AuxUI state, but doesn't Begin() a window like FloatingWindow would. Returns true if window should be rendered

        virtual void SetUpTextureBounds();
        virtual void StartTransitionFade();
        virtual void ApplyPendingValues();     //Pending values when switching between states from a transition fade should be applied here, if any

    public:
        AuxUIWindow(AuxUIID ui_id);

        virtual bool Show();
        virtual void Hide();
        bool IsVisible();
};

class WindowDragHint : public AuxUIWindow
{
    private:
        vr::ETrackedControllerRole m_TargetHand;
        bool m_IsDockingHint;
        bool m_IsDockingHintPending;

        void SetUpOverlay();
        void UpdateOverlayPos();
        virtual void ApplyPendingValues();

    public:
        WindowDragHint();

        void Update();
        void SetHintType(vr::ETrackedControllerRole controller_role, bool is_docking);
};

class WindowGazeFadeAutoHint : public AuxUIWindow
{
    private:
        unsigned int m_TargetOverlay;
        int m_Countdown;
        double m_TickTime;
        std::string m_Label;

        void SetUpOverlay();
        void UpdateOverlayPos();

    public:
        WindowGazeFadeAutoHint();

        virtual bool Show();
        virtual void Hide();
        void Update();
        void SetTargetOverlay(unsigned int overlay_id);
};

class WindowCaptureWindowSelect : public AuxUIWindow
{
    private:
        ImVec2 m_PosCenter;
        Matrix4 m_Transform;
        HWND m_WindowLastClicked;
        ULONGLONG m_HoveredTickLast;

        void SetUpOverlay();
        virtual void ApplyPendingValues();

    public:
        WindowCaptureWindowSelect();

        virtual bool Show();
        void Update();
        void SetTransform(Matrix4 transform);
};

class AuxUI
{
    friend class AuxUIWindow;

    private:
        AuxUIID m_ActiveUIID;
        bool m_IsUIFadingOut;

        WindowDragHint m_WindowDragHint;
        WindowGazeFadeAutoHint m_WindowGazeFadeAutoHint;
        WindowCaptureWindowSelect m_WindowCaptureWindowSelect;

        bool SetActiveUI(AuxUIID new_ui_id);        //May return false if higher priority UI is active
        AuxUIID GetActiveUI() const;
        void ClearActiveUI(AuxUIID current_ui_id);  //Sets active UI to auxui_none, but current ID has to match active one

        bool IsUIFadingOut() const;
        void SetFadeOutFinished();

    public:
        AuxUI();
        void Update();
        bool IsActive() const;

        void HideTemporaryWindows();                //Called on certain OpenVR events to hide temporary windows when user interaction ended so they don't hang around needlessly

        WindowDragHint& GetDragHintWindow();
        WindowGazeFadeAutoHint& GetGazeFadeAutoHintWindow();
        WindowCaptureWindowSelect& GetCaptureWindowSelectWindow();
};