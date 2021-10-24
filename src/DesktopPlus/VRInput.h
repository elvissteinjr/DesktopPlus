#ifndef _VRINPUT_H_
#define _VRINPUT_H_

#include "openvr.h"

#include <array>
#include <vector>

class OutputManager;

//Additional VRMouseButton values to get full state auxiliary click events
//Normal implementations shouldn't have issues with these, but they're only sent to Desktop+ overlays anyways
#define VRMouseButton_DP_Aux01 0x0008
#define VRMouseButton_DP_Aux02 0x0010

enum VRInputScrollMode
{
    vrinput_scroll_none,
    vrinput_scroll_discrete,
    vrinput_scroll_smooth,
};

//Can't be used with open dashboard, but handles global shortcuts and Desktop+ laser pointer input instead.
class VRInput
{
    private:
        vr::VRActionSetHandle_t m_HandleActionsetShortcuts;
        vr::VRActionSetHandle_t m_HandleActionsetLaserPointer;
        vr::VRActionSetHandle_t m_HandleActionsetScrollDiscrete;
        vr::VRActionSetHandle_t m_HandleActionsetScrollSmooth;

        vr::VRActionHandle_t m_HandleActionEnableGlobalLaserPointer;
        vr::VRActionHandle_t m_HandleActionDoGlobalShortcut01;
        vr::VRActionHandle_t m_HandleActionDoGlobalShortcut02;
        vr::VRActionHandle_t m_HandleActionDoGlobalShortcut03;
        vr::VRActionHandle_t m_HandleActionToggleOverlayGroupEnabled01;
        vr::VRActionHandle_t m_HandleActionToggleOverlayGroupEnabled02;
        vr::VRActionHandle_t m_HandleActionToggleOverlayGroupEnabled03;

        vr::VRActionHandle_t m_HandleActionLaserPointerLeftClick;
        vr::VRActionHandle_t m_HandleActionLaserPointerRightClick;
        vr::VRActionHandle_t m_HandleActionLaserPointerMiddleClick;
        vr::VRActionHandle_t m_HandleActionLaserPointerAux01Click;
        vr::VRActionHandle_t m_HandleActionLaserPointerAux02Click;
        vr::VRActionHandle_t m_HandleActionLaserPointerScrollDiscrete;
        vr::VRActionHandle_t m_HandleActionLaserPointerScrollSmooth;
        vr::VRActionHandle_t m_HandleActionLaserPointerHaptic;

        bool m_IsAnyGlobalActionBound;            //"Bound" meaning assigned and the device is actually active
        bool m_IsAnyGlobalActionBoundStateValid;

        bool m_IsLaserPointerInputActive;
        VRInputScrollMode m_LaserPointerScrollMode;

    public:
        VRInput();
        bool Init();
        void Update();
        void RefreshAnyGlobalActionBound();
        void HandleGlobalActionShortcuts(OutputManager& outmgr);
        void HandleGlobalOverlayGroupShortcuts(OutputManager& outmgr);
        void TriggerLaserPointerHaptics(vr::VRInputValueHandle_t restrict_to_device = vr::k_ulInvalidInputValueHandle) const;

        bool GetSetDetachedInteractiveDown() const;
        vr::InputDigitalActionData_t GetEnableGlobalLaserPointerState() const;

        std::vector<vr::InputOriginInfo_t> GetLaserPointerDevicesInfo() const;
        vr::InputDigitalActionData_t GetLaserPointerLeftClickState(vr::VRInputValueHandle_t restrict_to_device = vr::k_ulInvalidInputValueHandle)  const;
        std::array<vr::InputDigitalActionData_t, 5> GetLaserPointerClickState(vr::VRInputValueHandle_t restrict_to_device = vr::k_ulInvalidInputValueHandle)  const;
        vr::InputAnalogActionData_t GetLaserPointerScrollDiscreteState()         const;
        vr::InputAnalogActionData_t GetLaserPointerScrollSmoothState()           const;

        void SetLaserPointerActive(bool is_active);
        void SetLaserPointerScrollMode(VRInputScrollMode scroll_mode);
        VRInputScrollMode GetLaserPointerScrollMode() const;

        bool IsAnyGlobalActionBound() const;
};

#endif