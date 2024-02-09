//Extra functions extending/wrapping OpenVR APIs

#pragma once

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <d3d11.h>

#include "Matrices.h"

namespace vr
{
    class IVRSystemEx
    {
        public:
            //Translate the matrix relative to its own orientation
            static void TransformOpenVR34TranslateRelative(HmdMatrix34_t& matrix, float offset_right, float offset_up, float offset_forward);

            //Rotate the matrix towards the given target position
            static void TransformLookAt(Matrix4& matrix, const Vector3 pos_target, const Vector3 up = {0.0f, 1.0f, 0.0f});

            //Returns transform similar to the dashboard transform (not a perfect match, though)
            static Matrix4 ComputeHMDFacingTransform(float distance);

            //Returns the time to the next photon/physical display update, in seconds
            static float GetTimeNowToPhotons();

            //Returns tip matrix of the tracked device with the given controller role (handed roles only)
            static Matrix4 GetControllerTipMatrix(ETrackedControllerRole controller_role = TrackedControllerRole_RightHand);

            //Returns the first generic tracker device
            static TrackedDeviceIndex_t GetFirstVRTracker();
    };

    class IVROverlayEx
    {
        public:
            //Returns false if the device has no valid pose
            static bool GetOverlayIntersectionParamsForDevice(VROverlayIntersectionParams_t& params, TrackedDeviceIndex_t device_index, ETrackingUniverseOrigin tracking_origin, 
                                                              bool use_tip_offset = true);

            //Returns true if intersection happened
            static bool ComputeOverlayIntersectionForDevice(VROverlayHandle_t overlay_handle, TrackedDeviceIndex_t device_index, ETrackingUniverseOrigin tracking_origin, 
                                                            VROverlayIntersectionResults_t* results, bool use_tip_offset = true, bool front_face_only = true);

            //Returns true if intersection hit the front side of the overlay
            static bool IsOverlayIntersectionHitFrontFacing(const VROverlayIntersectionParams_t& params, const VROverlayIntersectionResults_t& results);

            //Returns which handed controller is currently pointing at the given overlay, if any
            static TrackedDeviceIndex_t FindPointerDeviceForOverlay(VROverlayHandle_t overlay_handle);

            //Returns the device pointing closest to the given position if there are multiple
            static TrackedDeviceIndex_t FindPointerDeviceForOverlay(VROverlayHandle_t overlay_handle, Vector2 pos_uv);

            //Returns true if the system laser pointer is likely to be active. There may be edge-cases with this depending on SteamVR behavior
            static bool IsSystemLaserPointerActive();

            //Takes the shared texture of the source overlay and sets it as texture on the target overlay. This only works as long as the backing texture exists
            static EVROverlayError SetSharedOverlayTexture(VROverlayHandle_t ovrl_handle_source, VROverlayHandle_t ovrl_handle_target, ID3D11Resource* device_texture_ref);
    };

    IVRSystemEx* VRSystemEx();
    IVROverlayEx* VROverlayEx();
}
