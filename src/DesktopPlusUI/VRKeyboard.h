#pragma once

#include "WindowKeyboard.h"

#include <vector>
#include <queue>
#include <string>

enum KeyboardLayoutSubLayout : unsigned char
{
    kbdlayout_sub_base,
    kbdlayout_sub_shift,
    kbdlayout_sub_altgr,
    kbdlayout_sub_aux,
    kbdlayout_sub_MAX
};

enum KeyboardLayoutKeyType
{
    kbdlayout_key_blank_space,
    kbdlayout_key_virtual_key,
    kbdlayout_key_virtual_key_toggle,
    kbdlayout_key_virtual_key_iso_enter,
    kbdlayout_key_string,
    kbdlayout_key_sublayout_toggle,
    kbdlayout_key_action
};

enum KeyboardLayoutCluster
{
    kbdlayout_cluster_base,
    kbdlayout_cluster_function,
    kbdlayout_cluster_navigation,
    kbdlayout_cluster_numpad,
    kbdlayout_cluster_extra,
    kbdlayout_cluster_MAX
};

struct KeyboardLayoutMetadata
{
    std::string Name = "Unknown";
    std::string FileName;
    bool HasAltGr = false;                                  //Right Alt switches to AltGr sublayout when down
    bool HasCluster[kbdlayout_cluster_MAX] = {false};
};

struct KeyboardLayoutKey
{
    KeyboardLayoutKeyType KeyType = kbdlayout_key_blank_space;
    bool IsRowEnd = false;
    float Width   = 1.0f;
    float Height  = 1.0f;
    std::string Label;
    bool BlockModifiers   = false;
    unsigned char KeyCode = 0;
    std::string KeyString;
    KeyboardLayoutSubLayout KeySubLayoutToggle = kbdlayout_sub_base;
    ActionID KeyActionID = action_none;
};

enum KeyboardInputTarget
{
    kbdtarget_desktop,
    kbdtarget_ui,
    kbdtarget_overlay
};

class VRKeyboard
{
    private:
        WindowKeyboard m_WindowKeyboard;
        WindowKeyboardShortcuts m_WindowKeyboardShortcuts;

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
        bool m_MouseLeftDownPrevCached;
        bool m_MouseLeftClickedPrevCached;
        bool m_KeyboardHiddenLastFrame;

        unsigned char GetModifierFlags() const;
        vr::VROverlayHandle_t GetTargetOverlayHandle() const;

    public:
        VRKeyboard();
        WindowKeyboard& GetWindow();

        void LoadLayoutFromFile(const std::string& filename);
        void LoadCurrentLayout();

        const KeyboardLayoutMetadata& GetLayoutMetadata() const;
        std::vector<KeyboardLayoutKey>& GetLayout(KeyboardLayoutSubLayout sublayout);
        const std::string& GetKeyLabelsString() const;

        KeyboardInputTarget GetInputTarget() const;
        unsigned int GetInputTargetOverlayID() const;

        bool GetKeyDown(unsigned char keycode) const;
        void SetKeyDown(unsigned char keycode, bool down, bool block_modifiers = false);
        void SetStringDown(const std::string text, bool down);
        void SetActionDown(ActionID action_id, bool down);
        bool IsCapsLockToggled() const;
        void ResetState();

        void VRKeyboardInputBegin(const char* str_id);
        void VRKeyboardInputBegin(ImGuiID widget_id);
        void VRKeyboardInputEnd();

        void OnImGuiNewFrame();
        void OnWindowHidden();

        void AddTextToStringQueue(const std::string text);

        void UpdateImGuiModifierState() const;
        void RestoreDesktopModifierState() const;

        static KeyboardLayoutMetadata LoadLayoutMetadataFromFile(const std::string& filename);
};