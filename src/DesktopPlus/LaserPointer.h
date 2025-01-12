#pragma once

#include "DPRect.h"
#include "Overlays.h"
#include "openvr.h"

#include <vector>

struct LaserPointerDevice
{
    vr::VROverlayHandle_t OvrlHandle = vr::k_ulOverlayHandleInvalid;
    vr::VRInputValueHandle_t InputValueHandle = vr::k_ulInvalidInputValueHandle;
    bool UseHMDAsOrigin = false;
    bool IsVisible = false;
    float LaserLength = 0.0f;

    vr::VROverlayHandle_t OvrlHandleTargetLast = vr::k_ulOverlayHandleInvalid;
    OverlayTextureSource OvrlHandleTargetLastTextureSource = ovrl_texsource_none;
    bool IsActiveForMultiLaserInput = false;
    int InputDownCount = 0;
    bool IsDragDown = false;
};

//Optional origin that can be passed to SetActiveDevice() in order to keep track what activated the laser pointer (and thus should be responsible for deactivating)
enum LaserPointerActivationOrigin
{
    dplp_activation_origin_none,
    dplp_activation_origin_auto_toggle,
    dplp_activation_origin_input_binding
};

//Custom laser pointer implementation for use outside of the dashboard
//Operates on Desktop+ and Desktop+ UI overlays, sending overlay events matching SteamVR laser input behavior as closely as possible
//Actually sends middle mouse events when bound
//Supports use of any tracked device that has the corrects inputs bound, though IsAnyOverlayHovered() only checks handed controllers for auto interaction toggle
//Paralell/Multi-Laser input is supported for certain overlays that can handle it (currently only the VR keyboard)
class LaserPointer
{
    private:
        LaserPointerDevice m_Devices[vr::k_unMaxTrackedDeviceCount];
        std::vector<vr::VROverlayHandle_t> m_OverlayHandlesUI;
        std::vector<vr::VROverlayHandle_t> m_OverlayHandlesMultiLaser;

        LaserPointerActivationOrigin m_ActivationOrigin;
        bool m_HadPrimaryPointerDevice;
        vr::TrackedDeviceIndex_t m_DeviceMaxActiveID;
        ULONGLONG m_LastPrimaryDeviceSwitchTick;
        ULONGLONG m_LastScrollTick;
        vr::TrackedDeviceIndex_t m_DeviceHapticPending;

        //State set by ForceTargetOverlay()
        bool m_IsForceTargetOverlayActive;
        vr::VROverlayHandle_t m_ForceTargetOverlayHandle;

        Vector2Int m_UIMouseScale;
        std::vector<DPRect> m_UIIntersectionMaskRects;
        std::vector<DPRect> m_UIIntersectionMaskRectsPending;

        void CreateDeviceOverlay(vr::TrackedDeviceIndex_t device_index);
        void UpdateDeviceOverlay(vr::TrackedDeviceIndex_t device_index);
        void UpdateIntersection(vr::TrackedDeviceIndex_t device_index);

        void SendDirectDragCommand(vr::VROverlayHandle_t overlay_handle_target, bool do_start_drag);

    public:
        LaserPointer();
        ~LaserPointer();

        void Update();

        void SetActiveDevice(vr::TrackedDeviceIndex_t device_index, LaserPointerActivationOrigin activation_origin = dplp_activation_origin_none);
        void ClearActiveDevice();
        void RemoveDevice(vr::TrackedDeviceIndex_t device_index);           //Clears device entry, called on device disconnect

        void RefreshCachedOverlayHandles();
        void TriggerLaserPointerHaptics(vr::TrackedDeviceIndex_t device_index) const;
        void ForceTargetOverlay(vr::VROverlayHandle_t overlay_handle);      //Forces a different overlay to be current pointer target (only if there's currently one)

        //ComputeOverlayIntersection() does not take intersection masks into account. It's a bit cumbersome, but we track the DDP/UI ones ourselves to get around that.
        bool IntersectionMaskHitTest(OverlayTextureSource texsource, vr::HmdVector2_t& uv) const;
        void UIIntersectionMaskAddRect(DPRect& rect);
        void UIIntersectionMaskFinish();

        LaserPointerActivationOrigin GetActivationOrigin() const;
        bool IsActive() const;
        vr::TrackedDeviceIndex_t IsAnyOverlayHovered(float max_distance) const; //Returns hovering device_index (or invalid if none). Only checks for LaserPointer supported overlays
        bool IsScrolling() const;
};