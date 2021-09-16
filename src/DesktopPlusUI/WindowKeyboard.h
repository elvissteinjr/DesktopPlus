#pragma once

#include "FloatingWindow.h"
#include "ImGuiExt.h"

enum KeyboardLayoutSubLayout : unsigned char;

class WindowKeyboard : public FloatingWindow
{
    private:
        float m_WindowWidth;
        bool m_IsHovered;
        bool m_HasHoveredNewItem;

        bool m_CurrentStringKeyDown;
        bool m_UnstickModifiersLater;
        KeyboardLayoutSubLayout m_SubLayoutOverride;
        KeyboardLayoutSubLayout m_LastSubLayout;
        int m_ActiveKeyIndex;
        unsigned char m_ActiveKeyCode;
        bool m_ManuallyStickingModifiers[8];

        ImGui::ActiveWidgetStateStorage m_KeyboardWidgetState;

        virtual void WindowUpdate();

        void OnVirtualKeyDown(unsigned char keycode, bool block_modifiers = false);
        void OnVirtualKeyUp(unsigned char keycode, bool block_modifiers = false);
        void OnStringKeyDown(const std::string& keystring);
        void OnStringKeyUp(const std::string& keystring);
        void HandleUnstickyModifiers(unsigned char source_keycode = 0);

        void SetManualStickyModifierState(unsigned char keycode, bool is_down);
        static int GetModifierID(unsigned char keycode);

    public:
        WindowKeyboard();
        virtual void Show(bool skip_fade = false);
        virtual void Hide(bool skip_fade = false);
        virtual vr::VROverlayHandle_t GetOverlayHandle() const;
        virtual void ResetTransform();
        bool IsHovered() const;
        bool HasHoveredNewItem() const;
        void UpdateOverlaySize() const;
};