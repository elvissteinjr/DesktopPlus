//Extra functions extending/wrapping OpenVR APIs
//Interfaces are supposed to be thread-safe
//Assumes OpenVR is initialized

#pragma once

#include <map>
#include <mutex>

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
        private:
            struct SharedOverlayTexture
            {
                ID3D11ShaderResourceView* ShaderResourceView = nullptr;
                Vector2Int TextureSize = {-1, -1};
                EColorSpace TextureColorSpace = ColorSpace_Auto;
            };

            std::mutex m_SharedOverlayTexuresMutex;
            std::map<VROverlayHandle_t, SharedOverlayTexture> m_SharedOverlayTextures;

            ID3D11ShaderResourceView* GetOverlayTextureExInternal(VROverlayHandle_t overlay_handle, ID3D11Resource* device_texture_ref);

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

            //-OverlayTextureEx functions
            //These functions wrap around base OpenVR functions to keep track texture sizes and shared texture handles (when used once)
            //This is under the assumption that nothing except these functions invalidate existing shared texture handles
            //Size difference is assumed to trigger backing texture change
            //These functions should only be called on application-owned overlays with textures previously set with SetOverlayTextureEx()

            //Calls IVROverlay::SetOverlayTextureEx() and adds overlay book-keeping data
            //If out_shared_texture_invalidated_ptr is non-null, it is set to true if the shared texture would be invalidated (even if there never was any, so can be used as refresh flag)
            EVROverlayError SetOverlayTextureEx(VROverlayHandle_t overlay_handle, const Texture_t* texture_ptr, Vector2Int texture_size, bool* out_shared_texture_invalidated_ptr = nullptr);

            //Calls IVROverlay::SetOverlayFromFile() and adds overlay book-keeping data
            //This always invalidates the cached shared texture if there is any
            EVROverlayError SetOverlayFromFileEx(VROverlayHandle_t overlay_handle, const char* file_path);

            //Takes the shared texture of the source overlay and sets it as texture on the target overlay. This only works as long as the backing texture exists
            EVROverlayError SetSharedOverlayTexture(VROverlayHandle_t overlay_handle_source, VROverlayHandle_t overlay_handle_target, ID3D11Resource* device_texture_ref);

            //Releases the shared texture of the overlay, but doesn't touch the overlay in any other way. Use when letting another process/module take over rendering to it
            EVROverlayError ReleaseSharedOverlayTexture(VROverlayHandle_t overlay_handle);

            //Calls IVROverlay::ClearOverlayTexture(), frees the shared texture if needed, and removes the overlay book-keeping data
            EVROverlayError ClearOverlayTextureEx(VROverlayHandle_t overlay_handle);

            //Returns the shared texture after requesting it from OpenVR if it's not already cached (or nullptr if that fails)
            ID3D11ShaderResourceView* GetOverlayTextureEx(VROverlayHandle_t overlay_handle, ID3D11Resource* device_texture_ref);

            //Returns stored texture size. Doesn't call into OpenVR. Returns {-1, -1} if overlay is unknown
            Vector2Int GetOverlayTextureSizeEx(VROverlayHandle_t overlay_handle);

            //Calls IVROverlay::DestroyOverlay(), frees the shared texture if needed, and removes the overlay book-keeping data
            EVROverlayError DestroyOverlayEx(VROverlayHandle_t overlay_handle);

    };

    IVRSystemEx* VRSystemEx();
    IVROverlayEx* VROverlayEx();
}
