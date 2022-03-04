#include "BackgroundOverlay.h"

#include "ConfigManager.h"
#include "OutputManager.h"

BackgroundOverlay::BackgroundOverlay() : m_OvrlHandle(vr::k_ulOverlayHandleInvalid)
{
    //Not calling Update() here since the OutputManager typically needs to load the config and OpenVR first
}

BackgroundOverlay::~BackgroundOverlay()
{
    if (m_OvrlHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->DestroyOverlay(m_OvrlHandle);
    }
}

void BackgroundOverlay::Update()
{
    InterfaceBGColorDisplayMode display_mode = (InterfaceBGColorDisplayMode)ConfigManager::GetValue(configid_int_interface_background_color_display_mode);
    if (display_mode == ui_bgcolor_dispmode_never)
    {
        //Don't keep the overlay around if it's absolutely not needed (which is the case most of the time)
        if (m_OvrlHandle != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->DestroyOverlay(m_OvrlHandle);
            m_OvrlHandle = vr::k_ulOverlayHandleInvalid;
        }
    }
    else
    {
        //Create overlay if it doesn't exist yet
        if (m_OvrlHandle == vr::k_ulOverlayHandleInvalid)
        {
            vr::EVROverlayError ovrl_error = vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusBackground", "Desktop+ Background", &m_OvrlHandle);

            if (ovrl_error != vr::VROverlayError_None)
                return;

            //Fill overlay with some pixels
            unsigned char bytes[2 * 2 * 4];
            std::fill(std::begin(bytes), std::end(bytes), 255); //Full white RGBA

            vr::VROverlay()->SetOverlayRaw(m_OvrlHandle, bytes, 2, 2, 4);

            //The trick to this overlay is to set it as a panorama attached to the HMD
            //Panorama overlays are weird in that they essentially poke a hole into the rendering in the area they'd cover normally
            //By doing this we ensure to always cover the field of view. The actual panorama is rendered in front of scene content, but positioned behind all overlays
            //IVRCompositor::FadeToColor() can achieve a similar effect, but the colors are washed out. Impossible to achieve full black with it.
            vr::VROverlay()->SetOverlayFlag(m_OvrlHandle, vr::VROverlayFlags_Panorama, true);
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandle, 100.0f);

            Matrix4 transform;
            transform.setTranslation({0.0f, 0.0f, -10.0f});
            vr::HmdMatrix34_t transform_openvr = transform.toOpenVR34();
            vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(m_OvrlHandle, vr::k_unTrackedDeviceIndex_Hmd, &transform_openvr);
        }

        bool display_overlay = true; //ui_bgcolor_dispmode_always

        if (display_mode == ui_bgcolor_dispmode_dplustab)
        {
            display_overlay = ((OutputManager::Get()) && (OutputManager::Get()->IsDashboardTabActive()));
        }

        if (display_overlay)
        {
            //Unpack color value
            unsigned int rgba = pun_cast<unsigned int, int>(ConfigManager::GetValue(configid_int_interface_background_color));
            float r =  (rgba & 0x000000FF)        / 255.0f;
            float g = ((rgba & 0x0000FF00) >> 8)  / 255.0f;
            float b = ((rgba & 0x00FF0000) >> 16) / 255.0f;
            float a = ((rgba & 0xFF000000) >> 24) / 255.0f;

            vr::VROverlay()->SetOverlayColor(m_OvrlHandle, r, g, b);
            vr::VROverlay()->SetOverlayAlpha(m_OvrlHandle, a);

            if (!vr::VROverlay()->IsOverlayVisible(m_OvrlHandle))
            {
                vr::VROverlay()->ShowOverlay(m_OvrlHandle);
            }
        }
        else if (vr::VROverlay()->IsOverlayVisible(m_OvrlHandle))
        {
            vr::VROverlay()->HideOverlay(m_OvrlHandle);
        }
    }
}
