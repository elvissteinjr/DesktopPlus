#pragma once

#include "VRKeyboardCommon.h"
#include "WindowKeyboard.h"
#include "WindowKeyboardEditor.h"

#include <vector>
#include <queue>
#include <string>

class VRKeyboard
{
    private:
        WindowKeyboard m_WindowKeyboard;
        WindowKeyboardShortcuts m_WindowKeyboardShortcuts;
        KeyboardEditor m_KeyboardEditor;

        KeyboardLayoutMetadata m_LayoutMetadata;
        std::vector<KeyboardLayoutKey> m_KeyboardKeys[kbdlayout_sub_MAX];
        std::string m_KeyLabels;                                          //String containing all labels, so their characters are always loaded from the font

        KeyboardInputTarget m_InputTarget;
        unsigned int m_InputTargetOverlayID;
        bool m_KeyDown[256];
        bool m_CapsLockToggled;
        std::queue<std::string> m_StringQueue;

        ImGuiID m_ActiveInputText;
        ImGuiID m_InputBeginWidgetID;
        ImGuiDir m_ShortcutWindowDirHint;
        float m_ShortcutWindowYOffset;
        bool m_ActiveInputTextIsMultiline;
        bool m_MouseLeftDownPrevCached;
        bool m_MouseLeftClickedPrevCached;
        bool m_KeyboardHiddenLastFrame;

        unsigned char GetModifierFlags() const;
        vr::VROverlayHandle_t GetTargetOverlayHandle() const;

    public:
        VRKeyboard();
        WindowKeyboard& GetWindow();
        KeyboardEditor& GetEditor();

        void LoadLayoutFromFile(const std::string& filename);
        bool SaveCurrentLayoutToFile(const std::string& filename);
        void LoadCurrentLayout();
        static std::vector<KeyboardLayoutMetadata> GetKeyboardLayoutList();

        const KeyboardLayoutMetadata& GetLayoutMetadata() const;
        void SetLayoutMetadata(const KeyboardLayoutMetadata& metadata);
        std::vector<KeyboardLayoutKey>& GetLayout(KeyboardLayoutSubLayout sublayout);
        void SetLayout(KeyboardLayoutSubLayout sublayout, std::vector<KeyboardLayoutKey>& keys);
        const std::string& GetKeyLabelsString() const;

        KeyboardInputTarget GetInputTarget() const;
        unsigned int GetInputTargetOverlayID() const;

        bool GetKeyDown(unsigned char keycode) const;
        void SetKeyDown(unsigned char keycode, bool down, bool block_modifiers = false);
        void SetStringDown(const std::string text, bool down);
        void SetActionDown(ActionUID action_uid, bool down);
        bool IsCapsLockToggled() const;
        void ResetState();

        void VRKeyboardInputBegin(const char* str_id, bool is_multiline = false);
        void VRKeyboardInputBegin(ImGuiID widget_id, bool is_multiline = false);
        void VRKeyboardInputEnd();

        void OnImGuiNewFrame();
        void OnWindowHidden();

        void AddTextToStringQueue(const std::string text);

        void UpdateImGuiModifierState() const;
        void RestoreDesktopModifierState() const;

        //Positioning hint used by the shortcut window. Set when the default down direction would cover related widgets. Offset only used with hint dir. Resets automatically
        void SetShortcutWindowDirectionHint(ImGuiDir dir_hint, float y_offset = 0.0f);

        static KeyboardLayoutMetadata LoadLayoutMetadataFromFile(const std::string& filename);
};