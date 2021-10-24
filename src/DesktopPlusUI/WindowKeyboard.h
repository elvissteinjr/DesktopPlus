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
        bool m_IsHovered;

        bool m_IsIsoEnterDown;
        bool m_UnstickModifiersLater;
        KeyboardLayoutSubLayout m_SubLayoutOverride;
        KeyboardLayoutSubLayout m_LastSubLayout;
        bool m_ManuallyStickingModifiers[8];

        ImGui::ActiveWidgetStateStorage m_KeyboardWidgetState;
        std::vector<ButtonLaserState> m_ButtonStates;
        std::vector<LaserInputState> m_LaserInputStates;

        virtual void WindowUpdate();

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
        virtual void Show(bool skip_fade = false);
        virtual void Hide(bool skip_fade = false);
        virtual vr::VROverlayHandle_t GetOverlayHandle() const;
        virtual void ResetTransform();

        bool IsHovered() const;
        void UpdateOverlaySize() const;
        void ResetButtonState();    //Called by VRKeyboard when resetting the keyboard state

        bool HandleOverlayEvent(const vr::VREvent_t& vr_event);
        void LaserSetMousePos(vr::TrackedDeviceIndex_t device_index, ImVec2 pos);
        void LaserSetMouseButton(vr::TrackedDeviceIndex_t device_index, ImGuiMouseButton button_index, bool is_down);
};