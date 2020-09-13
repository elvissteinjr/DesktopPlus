#ifndef _VRINPUT_H_
#define _VRINPUT_H_

#include "openvr.h"

class OutputManager;

//Can't be used with open overlay, but handles global shortcuts instead.
class VRInput
{
    private:
        vr::VRActionSetHandle_t m_HandleActionsetShortcuts;
        vr::VRActionHandle_t m_HandleActionSetDetachedInteractive;
        vr::VRActionHandle_t m_HandleActionDoGlobalShortcut01;
        vr::VRActionHandle_t m_HandleActionDoGlobalShortcut02;
        vr::VRActionHandle_t m_HandleActionDoGlobalShortcut03;

        bool m_IsAnyActionBound;            //"Bound" meaning assigned and the device is actually active
        bool m_IsAnyActionBoundStateValid;

    public:
        VRInput();
        bool Init();
        void Update();
        void RefreshAnyActionBound();
        void HandleGlobalActionShortcuts(OutputManager& outmgr);

        bool GetSetDetachedInteractiveDown() const;
        bool IsAnyActionBound() const;
};

#endif