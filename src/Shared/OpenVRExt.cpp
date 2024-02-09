#include "OpenVRExt.h"

#include <wrl/client.h>

#include "openvr.h"
#include "Matrices.h"

namespace vr
{
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
        vr::VROverlayIntersectionParams_t params = {0};

        if (GetOverlayIntersectionParamsForDevice(params, device_index, tracking_origin, use_tip_offset))
        {
            if (vr::VROverlay()->ComputeOverlayIntersection(overlay_handle, &params, results))
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

    EVROverlayError IVROverlayEx::SetSharedOverlayTexture(VROverlayHandle_t ovrl_handle_source, VROverlayHandle_t ovrl_handle_target, ID3D11Resource* device_texture_ref)
    {
        if (device_texture_ref == nullptr)
            return vr::VROverlayError_InvalidTexture;

        //Get overlay texture handle from OpenVR and set it as handle for the target overlay
        ID3D11ShaderResourceView* ovrl_shader_res = nullptr;
        uint32_t ovrl_width;
        uint32_t ovrl_height;
        uint32_t ovrl_native_format;
        vr::ETextureType ovrl_api_type;
        vr::EColorSpace ovrl_color_space;
        vr::VRTextureBounds_t ovrl_tex_bounds;

        vr::VROverlayError ovrl_error = vr::VROverlayError_None;
        ovrl_error = vr::VROverlay()->GetOverlayTexture(ovrl_handle_source, (void**)&ovrl_shader_res, device_texture_ref, &ovrl_width, &ovrl_height, &ovrl_native_format,
                                                        &ovrl_api_type, &ovrl_color_space, &ovrl_tex_bounds);

        if (ovrl_error == vr::VROverlayError_None)
        {
            {
                Microsoft::WRL::ComPtr<ID3D11Resource> ovrl_tex;
                Microsoft::WRL::ComPtr<IDXGIResource> ovrl_dxgi_resource;
                ovrl_shader_res->GetResource(&ovrl_tex);

                HRESULT hr = ovrl_tex.As(&ovrl_dxgi_resource);

                if (!FAILED(hr))
                {
                    HANDLE ovrl_tex_handle = nullptr;
                    ovrl_dxgi_resource->GetSharedHandle(&ovrl_tex_handle);

                    vr::Texture_t vrtex_target = {};
                    vrtex_target.eType = vr::TextureType_DXGISharedHandle;
                    vrtex_target.eColorSpace = vr::ColorSpace_Gamma;
                    vrtex_target.handle = ovrl_tex_handle;

                    vr::VROverlay()->SetOverlayTexture(ovrl_handle_target, &vrtex_target);
                }
            }

            vr::VROverlay()->ReleaseNativeOverlayHandle(ovrl_handle_source, (void*)ovrl_shader_res);
            ovrl_shader_res = nullptr;
        }

        return ovrl_error;
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
