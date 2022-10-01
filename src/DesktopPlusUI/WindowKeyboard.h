#pragma once

#include "FloatingWindow.h"
#include "ImGuiExt.h"
#include "openvr.h"

struct LaserInputState
{
    vr::TrackedDeviceIndex_t DeviceIndex = vr::k_unTrackedDeviceIndexInvalid;
    ImGui::ImGuiMouseState MouseState;
};

struct ButtonLaserState
{
    vr::TrackedDeviceIndex_t DeviceIndexHeld = vr::k_unTrackedDeviceIndexInvalid;
    bool IsDown         = false;
    bool IsHeld         = false;
    bool IsHovered      = false;
    bool IsActivated    = false;
    bool IsDeactivated  = false;
    bool IsRightClicked = false;
};

enum KeyboardLayoutSubLayout : unsigned char;

class WindowKeyboard : public FloatingWindow
{
    private:
        float m_WindowWidth;
        bool m_IsAutoVisible;
        bool m_IsHovered;
        bool m_IsAnyButtonHovered;
        Matrix4 m_TransformUIOrigin;

        int m_AssignedOverlayIDRoom;            //-1 = None/Global, -2 = UI (only if newly shown for it)
        int m_AssignedOverlayIDDashboardTab;    //^

        bool m_IsIsoEnterDown;
        bool m_UnstickModifiersLater;
        KeyboardLayoutSubLayout m_SubLayoutOverride;
        KeyboardLayoutSubLayout m_LastSubLayout;
        bool m_ManuallyStickingModifiers[8];

        ImGui::ActiveWidgetStateStorage m_KeyboardWidgetState;
        std::vector<ButtonLaserState> m_ButtonStates;
        std::vector<LaserInputState> m_LaserInputStates;

        virtual void WindowUpdate();

        virtual void OnWindowCloseButtonPressed();
        virtual bool IsVirtualWindowItemHovered() const;

        void OnVirtualKeyDown(unsigned char keycode, bool block_modifiers = false);
        void OnVirtualKeyUp(unsigned char keycode, bool block_modifiers = false);
        void OnStringKeyDown(const std::string& keystring);
        void OnStringKeyUp(const std::string& keystring);
        void HandleUnstickyModifiers(unsigned char source_keycode = 0);

        void SetManualStickyModifierState(unsigned char keycode, bool is_down);
        static int GetModifierID(unsigned char keycode);
        int FindSameKeyInNewSubLayout(int key_index, KeyboardLayoutSubLayout sublayout_id_current, KeyboardLayoutSubLayout sublayout_id_new);

        LaserInputState& GetLaserInputState(vr::TrackedDeviceIndex_t device_index);
        bool ButtonLaser(const char* label, const ImVec2& size_arg, ButtonLaserState& button_state);
        void ButtonVisual(const char* label, const ImVec2& size_arg);

    public:
        WindowKeyboard();
        virtual void UpdateVisibility();

        virtual void Show(bool skip_fade = false);
        virtual void Hide(bool skip_fade = false);
        bool SetAutoVisibility(unsigned int overlay_id, bool show);                         //Returns true on success
        virtual vr::VROverlayHandle_t GetOverlayHandle() const;
        virtual void RebaseTransform();
        virtual void ResetTransform(FloatingWindowOverlayStateID state_id);

        void SetAssignedOverlayID(int assigned_id);                                         //Sets for current state
        void SetAssignedOverlayID(int assigned_id, FloatingWindowOverlayStateID state_id);
        int GetAssignedOverlayID() const;                                                   //Gets from current state
        int GetAssignedOverlayID(FloatingWindowOverlayStateID state_id) const;

        bool IsHovered() const;
        void ResetButtonState();    //Called by VRKeyboard when resetting the keyboard state

        bool HandleOverlayEvent(const vr::VREvent_t& vr_event);
        void LaserSetMousePos(vr::TrackedDeviceIndex_t device_index, ImVec2 pos);
        void LaserSetMouseButton(vr::TrackedDeviceIndex_t device_index, ImGuiMouseButton button_index, bool is_down);
};

class WindowKeyboardShortcuts
{
    private:
        enum ButtonAction {btn_act_none = -1, btn_act_cut, btn_act_copy, btn_act_paste};

        bool m_IsHovered   = false;
        bool m_IsFadingOut = false;
        float m_Alpha      = 0.0f;

        ImGuiID m_ActiveWidget        = 0;
        ImGuiID m_ActiveWidgetPending = 0;

        ImGui::ActiveWidgetStateStorage m_WindowWidgetState;

        float m_WindowHeight              = FLT_MIN;
        float m_WindowHeightPrev          = FLT_MIN;
        ImGuiDir m_PosDir                 = ImGuiDir_Down;
        ImGuiDir m_PosDirDefault          = ImGuiDir_Down;
        float m_PosAnimationProgress      = 0.0f;

        ButtonAction m_ActiveButtonAction = btn_act_none;
        bool m_IsAnyButtonDown            = false;

    public:
        void SetActiveWidget(ImGuiID widget_id);
        void SetDefaultPositionDirection(ImGuiDir pos_dir);
        void Update(ImGuiID window_id);

        bool IsHovered() const;
        bool IsAnyButtonDown() const;
};