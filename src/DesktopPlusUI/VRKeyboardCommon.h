#pragma once

#include "Actions.h"
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
    kbdlayout_key_action,
    kbdlayout_key_MAX
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
    std::string Author = "";
    std::string FileName;
    bool HasAltGr = false;                                  //Right Alt switches to AltGr sublayout when down
    bool HasCluster[kbdlayout_cluster_MAX] = {false};
};

struct KeyboardLayoutKey
{
    KeyboardLayoutCluster KeyCluster = kbdlayout_cluster_base;
    KeyboardLayoutKeyType KeyType = kbdlayout_key_blank_space;
    bool IsRowEnd = false;
    float Width   = 1.0f;
    float Height  = 1.0f;
    std::string Label;
    float LabelHAlignment = 0.5f;   //Computed at load-time
    bool IsLabelMultiline = false;  //Computed at load-time
    bool BlockModifiers   = false;
    bool NoRepeat         = false;
    unsigned char KeyCode = 0;
    std::string KeyString;
    KeyboardLayoutSubLayout KeySubLayoutToggle = kbdlayout_sub_base;
    ActionUID KeyActionUID = k_ActionUID_Invalid;
};

enum KeyboardInputTarget
{
    kbdtarget_desktop,
    kbdtarget_ui,
    kbdtarget_overlay
};
