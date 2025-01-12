#include "OpenVRExt.h"

#include <wrl/client.h>

#include "openvr.h"
#include "Matrices.h"

namespace vr
{
    ID3D11ShaderResourceView* IVROverlayEx::GetOverlayTextureExInternal(VROverlayHandle_t overlay_handle, ID3D11Resource* device_texture_ref)
    {
        //m_SharedOverlayTexuresMutex is assumed to be locked already
        if (device_texture_ref == nullptr)
            return nullptr;

        auto it = m_SharedOverlayTextures.find(overlay_handle);

        if (it == m_SharedOverlayTextures.end())
            return nullptr;

        SharedOverlayTexture& shared_tex = it->second;

        //Get overlay texture from OpenVR if we don't have it cached yet
        if (shared_tex.ShaderResourceView == nullptr)
        {
            uint32_t ovrl_width;
            uint32_t ovrl_height;
            uint32_t ovrl_native_format;
            ETextureType ovrl_api_type;
            EColorSpace ovrl_color_space;
            VRTextureBounds_t ovrl_tex_bounds;

            VROverlayError ovrl_error = vr::VROverlayError_None;
            ovrl_error = VROverlay()->GetOverlayTexture(overlay_handle, (void**)&shared_tex.ShaderResourceView, device_texture_ref, &ovrl_width, &ovrl_height, &ovrl_native_format,
                                                        &ovrl_api_type, &ovrl_color_space, &ovrl_tex_bounds);

            //Shader Resource View set despite returning an error might not ever happen, but call release if it does
            if ((ovrl_error != VROverlayError_None) && (shared_tex.ShaderResourceView != nullptr))
            {
                VROverlay()->ReleaseNativeOverlayHandle(overlay_handle, it->second.ShaderResourceView);
                shared_tex.ShaderResourceView = nullptr;
            }
        }

        return shared_tex.ShaderResourceView;
    }

    bool IVROverlayEx::GetOverlayIntersectionParamsForDevice(VROverlayIntersectionParams_t& params, TrackedDeviceIndex_t device_index, ETrackingUniverseOrigin tracking_origin, bool use_tip_offset)
    {
        if (device_index >= k_unMaxTrackedDeviceCount)
            return false;

        TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount];
        VRSystem()->GetDeviceToAbsoluteTrackingPose(tracking_origin, IVRSystemEx::GetTimeNowToPhotons(), poses, k_unMaxTrackedDeviceCount);

        if (!poses[device_index].bPoseIsValid)
            return false;

        Matrix4 mat_device = poses[device_index].mDeviceToAbsoluteTracking;

        if (use_tip_offset)
        {
            mat_device = mat_device * IVRSystemEx::GetControllerTipMatrix( VRSystem()->GetControllerRoleForTrackedDeviceIndex(device_index) );
        } 

        //Set up intersection test
        Vector3 v_pos = mat_device.getTranslation();
        Vector3 forward = {mat_device[8], mat_device[9], mat_device[10]};
        forward *= -1.0f;

        params.eOrigin    = tracking_origin;
        params.vSource    = {v_pos.x, v_pos.y, v_pos.z};
        params.vDirection = {forward.x, forward.y, forward.z};

        return true;
    }

    bool IVROverlayEx::ComputeOverlayIntersectionForDevice(VROverlayHandle_t overlay_handle, TrackedDeviceIndex_t device_index, ETrackingUniverseOrigin tracking_origin, 
                                                           VROverlayIntersectionResults_t* results, bool use_tip_offset, bool front_face_only)
    {
        VROverlayIntersectionParams_t params = {0};

        if (GetOverlayIntersectionParamsForDevice(params, device_index, tracking_origin, use_tip_offset))
        {
            if (VROverlay()->ComputeOverlayIntersection(overlay_handle, &params, results))
            {
                return ( (!front_face_only) || (IsOverlayIntersectionHitFrontFacing(params, *results)) );
            }
        }

        return false;
    }

    bool IVROverlayEx::IsOverlayIntersectionHitFrontFacing(const VROverlayIntersectionParams_t& params, const VROverlayIntersectionResults_t& results)
    {
        Vector3 intersect_src_pos       = params.vSource;
        Vector3 intersect_target_pos    = results.vPoint;
        Vector3 intersect_target_normal = results.vNormal;
        intersect_target_normal.normalize();

        return (intersect_target_normal.dot(intersect_src_pos - intersect_target_pos) >= 0.0f);
    }

    TrackedDeviceIndex_t IVROverlayEx::FindPointerDeviceForOverlay(VROverlayHandle_t overlay_handle)
    {
        TrackedDeviceIndex_t device_index = k_unTrackedDeviceIndexInvalid;

        //Check left and right hand controller
        for (int controller_role = TrackedControllerRole_LeftHand; controller_role <= TrackedControllerRole_RightHand; ++controller_role)
        {
            TrackedDeviceIndex_t device_index_intersection = VRSystem()->GetTrackedDeviceIndexForControllerRole((ETrackedControllerRole)controller_role);
            VROverlayIntersectionResults_t results;

            if (ComputeOverlayIntersectionForDevice(overlay_handle, device_index_intersection, TrackingUniverseStanding, &results))
            {
                device_index = device_index_intersection;
            }
        }

        return device_index;
    }

    TrackedDeviceIndex_t IVROverlayEx::FindPointerDeviceForOverlay(VROverlayHandle_t overlay_handle, Vector2 pos_uv)
    {
        TrackedDeviceIndex_t device_index = k_unTrackedDeviceIndexInvalid;
        float nearest_uv_distance = FLT_MAX;

        //Check left and right hand controller
        for (int controller_role = TrackedControllerRole_LeftHand; controller_role <= TrackedControllerRole_RightHand; ++controller_role)
        {
            TrackedDeviceIndex_t device_index_intersection = VRSystem()->GetTrackedDeviceIndexForControllerRole((ETrackedControllerRole)controller_role);
            VROverlayIntersectionResults_t results;

            if (ComputeOverlayIntersectionForDevice(overlay_handle, device_index_intersection, TrackingUniverseStanding, &results))
            {
                const Vector2 uv_intesection(results.vUVs.v[0], results.vUVs.v[1]);
                const float distance = pos_uv.distance(uv_intesection);

                if (distance < nearest_uv_distance)
                {
                    device_index = device_index_intersection;
                    nearest_uv_distance = distance;
                }
            }
        }

        return device_index;
    }

    bool IVROverlayEx::IsSystemLaserPointerActive()
    {
        //IsSteamVRDrawingControllers() appears to only return true while the laser pointer is active even if SteamVR is drawing controllers from no scene app running or similar
        return (VROverlay()->IsDashboardVisible() || VRSystem()->IsSteamVRDrawingControllers());
    }

    EVROverlayError IVROverlayEx::SetOverlayTextureEx(VROverlayHandle_t overlay_handle, const Texture_t* texture_ptr, Vector2Int texture_size, bool* out_shared_texture_invalidated_ptr)
    {
        if (texture_ptr == nullptr)
            return VROverlayError_InvalidTexture;

        bool shared_texture_invalidated = true;

        {
            const std::lock_guard<std::mutex> textures_lock(m_SharedOverlayTexuresMutex);

            //Check if we already keep track of this overlay and update data & release shared handles if needed
            auto it = m_SharedOverlayTextures.find(overlay_handle);
            if (it != m_SharedOverlayTextures.end())
            {
                //Release shared texture handle if SetOverlayTexture will invalidate it
                SharedOverlayTexture& shared_tex = it->second;
                shared_texture_invalidated = (texture_size != shared_tex.TextureSize);

                if ((shared_texture_invalidated) && (shared_tex.ShaderResourceView != nullptr))
                {
                    VROverlay()->ReleaseNativeOverlayHandle(overlay_handle, shared_tex.ShaderResourceView);
                    shared_tex.ShaderResourceView = nullptr;
                }

                shared_tex.TextureSize = texture_size;
                shared_tex.TextureColorSpace = texture_ptr->eColorSpace;
            }
            else    //Add new shared overlay texture data
            {
                SharedOverlayTexture shared_tex;
                shared_tex.TextureSize = texture_size;
                shared_tex.TextureColorSpace = texture_ptr->eColorSpace;

                m_SharedOverlayTextures.emplace(overlay_handle, shared_tex);
            }
        }

        if (out_shared_texture_invalidated_ptr != nullptr)
        {
            *out_shared_texture_invalidated_ptr = shared_texture_invalidated;
        }

        return VROverlay()->SetOverlayTexture(overlay_handle, texture_ptr);
    }

    EVROverlayError IVROverlayEx::SetOverlayFromFileEx(VROverlayHandle_t overlay_handle, const char* file_path)
    {
        {
            const std::lock_guard<std::mutex> textures_lock(m_SharedOverlayTexuresMutex);

            //Check if we already keep track of this overlay and update data & release shared handles if needed
            auto it = m_SharedOverlayTextures.find(overlay_handle);
            if (it != m_SharedOverlayTextures.end())
            {
                //Always release shared texture handle (we assume that file loads aren't used for frequent updates and usually don't know the dimensions head of time)
                SharedOverlayTexture& shared_tex = it->second;

                if (shared_tex.ShaderResourceView != nullptr)
                {
                    VROverlay()->ReleaseNativeOverlayHandle(overlay_handle, shared_tex.ShaderResourceView);
                    shared_tex.ShaderResourceView = nullptr;
                }

                shared_tex.TextureSize = {-1, -1};
                shared_tex.TextureColorSpace = ColorSpace_Auto;
            }
            else    //Add new shared overlay texture data
            {
                SharedOverlayTexture shared_tex;
                shared_tex.TextureSize = {-1, -1};
                shared_tex.TextureColorSpace = ColorSpace_Auto;

                m_SharedOverlayTextures.emplace(overlay_handle, shared_tex);
            }
        }

        return VROverlay()->SetOverlayFromFile(overlay_handle, file_path);
    }

    EVROverlayError IVROverlayEx::SetSharedOverlayTexture(VROverlayHandle_t overlay_handle_source, VROverlayHandle_t overlay_handle_target, ID3D11Resource* device_texture_ref)
    {
        const std::lock_guard<std::mutex> textures_lock(m_SharedOverlayTexuresMutex);

        ID3D11ShaderResourceView* shader_resource_view = GetOverlayTextureExInternal(overlay_handle_source, device_texture_ref);

        if (shader_resource_view != nullptr)
        {
            Microsoft::WRL::ComPtr<ID3D11Resource> ovrl_tex;
            Microsoft::WRL::ComPtr<IDXGIResource> ovrl_dxgi_resource;
            shader_resource_view->GetResource(&ovrl_tex);

            HRESULT hr = ovrl_tex.As(&ovrl_dxgi_resource);

            if (!FAILED(hr))
            {
                HANDLE ovrl_tex_handle = nullptr;
                ovrl_dxgi_resource->GetSharedHandle(&ovrl_tex_handle);

                Texture_t vrtex_target = {};
                vrtex_target.eType = TextureType_DXGISharedHandle;
                vrtex_target.eColorSpace = m_SharedOverlayTextures[overlay_handle_source].TextureColorSpace;
                vrtex_target.handle = ovrl_tex_handle;

                return VROverlay()->SetOverlayTexture(overlay_handle_target, &vrtex_target);
            }
        }

        return VROverlayError_InvalidTexture;
    }

    EVROverlayError IVROverlayEx::ReleaseSharedOverlayTexture(VROverlayHandle_t overlay_handle)
    {
        const std::lock_guard<std::mutex> textures_lock(m_SharedOverlayTexuresMutex);

        EVROverlayError overlay_error = VROverlayError_None;

        auto it = m_SharedOverlayTextures.find(overlay_handle);
        if (it != m_SharedOverlayTextures.end())
        {
            if (it->second.ShaderResourceView != nullptr)
            {
                overlay_error = VROverlay()->ReleaseNativeOverlayHandle(overlay_handle, it->second.ShaderResourceView);
            }

            m_SharedOverlayTextures.erase(it);
        }

        return overlay_error;
    }

    EVROverlayError IVROverlayEx::ClearOverlayTextureEx(VROverlayHandle_t overlay_handle)
    {
        {
            const std::lock_guard<std::mutex> textures_lock(m_SharedOverlayTexuresMutex);

            auto it = m_SharedOverlayTextures.find(overlay_handle);
            if (it != m_SharedOverlayTextures.end())
            {
                if (it->second.ShaderResourceView != nullptr)
                {
                    VROverlay()->ReleaseNativeOverlayHandle(overlay_handle, it->second.ShaderResourceView);
                }

                m_SharedOverlayTextures.erase(it);
            }
        }

        return VROverlay()->ClearOverlayTexture(overlay_handle);
    }

    ID3D11ShaderResourceView* IVROverlayEx::GetOverlayTextureEx(VROverlayHandle_t overlay_handle, ID3D11Resource* device_texture_ref)
    {
        if (device_texture_ref == nullptr)
            return nullptr;

        const std::lock_guard<std::mutex> textures_lock(m_SharedOverlayTexuresMutex);

        return GetOverlayTextureExInternal(overlay_handle, device_texture_ref);
    }

    Vector2Int IVROverlayEx::GetOverlayTextureSizeEx(VROverlayHandle_t overlay_handle)
    {
        {
            const std::lock_guard<std::mutex> textures_lock(m_SharedOverlayTexuresMutex);
            auto it = m_SharedOverlayTextures.find(overlay_handle);

            if (it != m_SharedOverlayTextures.end())
            {
                return it->second.TextureSize;
            }
        }

        return {-1, -1};
    }

    EVROverlayError IVROverlayEx::DestroyOverlayEx(VROverlayHandle_t overlay_handle)
    {
        {
            const std::lock_guard<std::mutex> textures_lock(m_SharedOverlayTexuresMutex);

            auto it = m_SharedOverlayTextures.find(overlay_handle);

            if (it != m_SharedOverlayTextures.end())
            {
                if (it->second.ShaderResourceView != nullptr)
                {
                    VROverlay()->ReleaseNativeOverlayHandle(overlay_handle, it->second.ShaderResourceView);
                }

                m_SharedOverlayTextures.erase(it);
            }
        }

        return VROverlay()->DestroyOverlay(overlay_handle);
    }


    void IVRSystemEx::TransformOpenVR34TranslateRelative(HmdMatrix34_t& matrix, float offset_right, float offset_up, float offset_forward)
    {
        matrix.m[0][3] += offset_right * matrix.m[0][0];
        matrix.m[1][3] += offset_right * matrix.m[1][0];
        matrix.m[2][3] += offset_right * matrix.m[2][0];

        matrix.m[0][3] += offset_up * matrix.m[0][1];
        matrix.m[1][3] += offset_up * matrix.m[1][1];
        matrix.m[2][3] += offset_up * matrix.m[2][1];

        matrix.m[0][3] += offset_forward * matrix.m[0][2];
        matrix.m[1][3] += offset_forward * matrix.m[1][2];
        matrix.m[2][3] += offset_forward * matrix.m[2][2];
    }

    void IVRSystemEx::TransformLookAt(Matrix4& matrix, const Vector3 pos_target, const Vector3 up)
    {
        const Vector3 pos(matrix.getTranslation());

        Vector3 z_axis = pos_target - pos;
        z_axis.normalize();
        Vector3 x_axis = up.cross(z_axis);
        x_axis.normalize();
        Vector3 y_axis = z_axis.cross(x_axis);

        matrix = { x_axis.x, x_axis.y, x_axis.z, 0.0f,
                   y_axis.x, y_axis.y, y_axis.z, 0.0f,
                   z_axis.x, z_axis.y, z_axis.z, 0.0f,
                   pos.x,    pos.y,    pos.z,    1.0f };
    }

    Matrix4 IVRSystemEx::ComputeHMDFacingTransform(float distance)
    {
        //This is based on dashboard positioning code posted by Valve on the OpenVR GitHub
        static const Vector3 up = {0.0f, 1.0f, 0.0f};

        TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount];
        VRSystem()->GetDeviceToAbsoluteTrackingPose(TrackingUniverseStanding, 0.0f /*don't predict anything here*/, poses, k_unMaxTrackedDeviceCount);

        Matrix4 mat_hmd(poses[k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
        mat_hmd.translate_relative(0.0f, 0.0f, 0.10f);
        Matrix4 mat_hmd_temp = mat_hmd;

        Vector3 dashboard_start = mat_hmd_temp.translate_relative(0.0f, 0.0f, -distance).getTranslation();
        Vector3 forward_temp    = (dashboard_start - mat_hmd.getTranslation()).normalize();
        Vector3 right           = forward_temp.cross(up).normalize();
        Vector3 forward         = up.cross(right).normalize();

        dashboard_start = mat_hmd.getTranslation() + (distance * forward);

        Matrix4 mat_dashboard(right, up, forward * -1.0f);
        mat_dashboard.setTranslation(dashboard_start);

        return mat_dashboard;
    }

    float IVRSystemEx::GetTimeNowToPhotons()
    {
        float seconds_since_last_vsync;
        VRSystem()->GetTimeSinceLastVsync(&seconds_since_last_vsync, nullptr);

        const float vsync_to_photons  = VRSystem()->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, Prop_SecondsFromVsyncToPhotons_Float);
        const float display_frequency = VRSystem()->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, Prop_DisplayFrequency_Float);

        return (1.0f / display_frequency) - seconds_since_last_vsync + vsync_to_photons;
    }

    Matrix4 IVRSystemEx::GetControllerTipMatrix(ETrackedControllerRole controller_role)
    {
        if ( (controller_role != TrackedControllerRole_LeftHand) && (controller_role != TrackedControllerRole_RightHand) )
            return Matrix4();

        char buffer[k_unMaxPropertyStringSize];
        VRInputValueHandle_t input_value = k_ulInvalidInputValueHandle;

        VRSystem()->GetStringTrackedDeviceProperty(VRSystem()->GetTrackedDeviceIndexForControllerRole(controller_role), Prop_RenderModelName_String, buffer, k_unMaxPropertyStringSize);
        VRInput()->GetInputSourceHandle((controller_role == TrackedControllerRole_RightHand) ? "/user/hand/right" : "/user/hand/left", &input_value);

        RenderModel_ControllerMode_State_t controller_state = {0};
        RenderModel_ComponentState_t component_state = {0};

        if (VRRenderModels()->GetComponentStateForDevicePath(buffer, k_pch_Controller_Component_Tip, input_value, &controller_state, &component_state))
        {
            return component_state.mTrackingToComponentLocal;
        }

        return Matrix4();
    }

    TrackedDeviceIndex_t IVRSystemEx::GetFirstVRTracker()
    {
        for (int i = 0; i < k_unMaxTrackedDeviceCount; ++i)
        {
            if (VRSystem()->GetTrackedDeviceClass(i) == TrackedDeviceClass_GenericTracker)
            {
                return i;
            }
        }

        return k_unTrackedDeviceIndexInvalid;
    }

    static IVRSystemEx  g_IVRSystemEx;
    static IVROverlayEx g_IVROverlayEx;

    IVRSystemEx* VRSystemEx()
    {
        return &g_IVRSystemEx;
    }

    IVROverlayEx* VROverlayEx()
    {
        return &g_IVROverlayEx;
    }
}
