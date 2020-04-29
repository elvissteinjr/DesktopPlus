#ifndef _VRINPUT_H_
#define _VRINPUT_H_

#include "openvr.h"

class OutputManager;

//Can't be used with open overlay, but handles global shortcuts instead.
class VRInput
{
    private:
        vr::VRActionSetHandle_t m_handle_actionset_shortcuts;
        vr::VRActionHandle_t m_handle_action_set_overlay_detached;
        vr::VRActionHandle_t m_handle_action_set_detached_interactive;
        vr::VRActionHandle_t m_handle_action_do_global_shortcut_01;
        vr::VRActionHandle_t m_handle_action_do_global_shortcut_02;
        vr::VRActionHandle_t m_handle_action_do_global_shortcut_03;

    public:
        VRInput();
        bool Init();
        void Update();
        void HandleGlobalActionShortcuts(OutputManager& outmgr);
        bool HandleSetOverlayDetachedShortcut(bool is_detached_interactive);    //Returns true when it changed

        bool GetSetDetachedInteractiveDown();
};

#endif