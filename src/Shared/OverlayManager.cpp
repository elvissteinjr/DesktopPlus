#include "OverlayManager.h"

#ifndef DPLUS_UI
    #include "OutputManager.h"
    #include "DesktopPlusWinRT.h"
    #include "DPBrowserAPIClient.h"
#else
    #include "UIManager.h"
    #include "TranslationManager.h"
#endif

#include "InterprocessMessaging.h"
#include "WindowManager.h"
#include "Util.h"
#include "OpenVRExt.h"
#include "Logging.h"

#include <sstream>
#include <unordered_set>

static OverlayManager g_OverlayManager;

OverlayManager& OverlayManager::Get()
{
    return g_OverlayManager;
}

#ifndef DPLUS_UI
    OverlayManager::OverlayManager() : m_CurrentOverlayID(0), m_OverlayNull(k_ulOverlayID_None), m_TheaterOverlayHandle(vr::k_ulOverlayHandleInvalid),
                                       m_TheaterOverlayReferenceHandle(vr::k_ulOverlayHandleInvalid), m_CurrentTheaterOverlayOrigHandle(vr::k_ulOverlayHandleInvalid), 
                                       m_CurrentTheaterOverlayID(k_ulOverlayID_None)
#else
    OverlayManager::OverlayManager() : m_CurrentOverlayID(0)
#endif
{

}


Matrix4 OverlayManager::GetOverlayTransformBase(vr::VROverlayHandle_t ovrl_handle, unsigned int id, bool add_bottom_offset) const
{
    //The gist is that GetTransformForOverlayCoordinates() is fundamentally broken if the overlay has non-default properties
    //UV min/max not 0.0/1.0? You still get coordinates as if they were
    //Pixel aspect not 1.0? That function doesn't care
    //Also doesn't care about curvature, but that's not a huge issue
    const OverlayConfigData& data = GetConfigData(id);

    int ovrl_pixel_width = 1, ovrl_pixel_height = 1;

    vr::HmdVector2_t ovrl_mouse_scale;
    //Use mouse scale of overlay if possible as it can sometimes differ from the config size (and GetOverlayTextureSize() currently leaks GPU memory, oops)
    if (vr::VROverlay()->GetOverlayMouseScale(ovrl_handle, &ovrl_mouse_scale) == vr::VROverlayError_None)
    {
        ovrl_pixel_width  = (int)ovrl_mouse_scale.v[0];
        ovrl_pixel_height = (int)ovrl_mouse_scale.v[1];
    }
    else
    {
        ovrl_pixel_width  = data.ConfigInt[configid_int_overlay_state_content_width];
        ovrl_pixel_height = data.ConfigInt[configid_int_overlay_state_content_height];

        //If we can't get mouse scale we still need to make the 3D adjustments ourselves here
        if (data.ConfigBool[configid_bool_overlay_3D_enabled])
        {
            switch (data.ConfigInt[configid_int_overlay_3D_mode])
            {
                case ovrl_3Dmode_hsbs:
                case ovrl_3Dmode_sbs:
                {
                    ovrl_pixel_width /= 2; 
                    break;
                }
                case ovrl_3Dmode_hou:
                case ovrl_3Dmode_ou:
                {
                    //OU converted to SBS will have texture size based on crop rect
                    ovrl_pixel_width  = data.ConfigInt[configid_int_overlay_crop_width];
                    ovrl_pixel_height = data.ConfigInt[configid_int_overlay_crop_height] / 2;
                }
            }
        }
    }

    //Y-coordinate from this function is pretty much unpredictable if not pixel_height / 2
    vr::HmdMatrix34_t matrix = {0};
    vr::VROverlay()->GetTransformForOverlayCoordinates(ovrl_handle, vr::TrackingUniverseStanding, { (float)ovrl_pixel_width/2.0f, (float)ovrl_pixel_height/2.0f }, &matrix);

    if (add_bottom_offset)
    {
        //Get texture bounds
        vr::VRTextureBounds_t bounds;
        vr::VROverlay()->GetOverlayTextureBounds(ovrl_handle, &bounds);

        //Get 3D height factor
        float height_factor_3d = 1.0f;

        if (data.ConfigBool[configid_bool_overlay_3D_enabled])
        {
            float texel_aspect = 1.0f;
            vr::VROverlay()->GetOverlayTexelAspect(ovrl_handle, &texel_aspect);

            height_factor_3d = 1.0f / texel_aspect;
        }

        //Attempt to calculate the correct offset to bottom, taking in account all the things GetTransformForOverlayCoordinates() does not
        float width = data.ConfigFloat[configid_float_overlay_width];
        float uv_width  = bounds.uMax - bounds.uMin;
        float uv_height = bounds.vMax - bounds.vMin;
        float cropped_width  = ovrl_pixel_width  * uv_width;
        float cropped_height = ovrl_pixel_height * uv_height * height_factor_3d;
        float aspect_ratio_orig = (float)ovrl_pixel_width / ovrl_pixel_height;
        float aspect_ratio_new = cropped_height / cropped_width;
        float height = (aspect_ratio_orig * width);
        float offset_to_bottom =  -( (aspect_ratio_new * width) - (aspect_ratio_orig * width) ) / 2.0f;
        offset_to_bottom -= height / 2.0f;

        //Theater Screen's width can't be queried, so we use a cursor overlay and use it as reference point instead
        if (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen)
        {
            #ifdef DPLUS_UI
                vr::VROverlayHandle_t ovrl_handle_reference = vr::k_ulOverlayHandleInvalid;
                vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusTheaterReference", &ovrl_handle_reference);
            #else
                vr::VROverlayHandle_t ovrl_handle_reference = m_TheaterOverlayReferenceHandle;
            #endif

            if (ovrl_handle_reference != vr::k_ulOverlayHandleInvalid)
            {
                //Put reference cursor overlay to bottom center of theater overlay
                vr::HmdVector2_t pos{ovrl_pixel_width/2.0f, 0.0f};
                vr::VROverlay()->SetOverlayCursorPositionOverride(ovrl_handle, &pos);

                vr::VROverlay()->SetOverlayAlpha(ovrl_handle_reference, 0.25f);

                //Grab its middle spot and return that
                vr::VROverlay()->GetTransformForOverlayCoordinates(ovrl_handle_reference, vr::TrackingUniverseStanding, {0.5f, 0.5f}, &matrix);

                return matrix;
            }
        }

        //When Performance Monitor, apply additional offset of the unused overlay space
        #ifdef DPLUS_UI
            if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui)
            {
                float height_new = aspect_ratio_new * width; //height uses aspect_ratio_orig, so calculate new height
                offset_to_bottom += ( (cropped_height - UIManager::Get()->GetPerformanceWindow().GetSize().y) ) * ( (height_new / cropped_height) ) / 2.0f;
            }
        #endif

        //Browser overlays are using flipped UVs, so flip offset_to_bottom to match
        if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser)
        {
            offset_to_bottom *= -1.0f;
        }

        //Move to bottom
        vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix, 0.0f, offset_to_bottom, 0.0f);
    }

    return matrix;
}

#ifndef DPLUS_UI

void OverlayManager::TheaterOverlayForwardCapture(const Overlay& ovrl_source)
{
    if (ovrl_source.GetTextureSource() == ovrl_texsource_winrt_capture)
    {
        DPWinRT_StartCaptureFromOverlay(m_TheaterOverlayHandle, m_CurrentTheaterOverlayOrigHandle);
        DPWinRT_PauseCapture(m_TheaterOverlayHandle, !ovrl_source.IsVisible());
        DPWinRT_PauseCapture(m_CurrentTheaterOverlayOrigHandle, true);
    }
    else if (ovrl_source.GetTextureSource() == ovrl_texsource_ui)
    {
        vr::VROverlay()->SetOverlayRenderingPid(m_TheaterOverlayHandle, IPCManager::GetUIAppProcessID());
        IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_overlays_ui_reset);
    }
    else if (ovrl_source.GetTextureSource() == ovrl_texsource_browser)
    {
        DPBrowserAPIClient::Get().DPBrowser_DuplicateBrowserOutput(m_CurrentTheaterOverlayOrigHandle, m_TheaterOverlayHandle);
        DPBrowserAPIClient::Get().DPBrowser_PauseBrowser(m_TheaterOverlayHandle, !ovrl_source.IsVisible());
        DPBrowserAPIClient::Get().DPBrowser_PauseBrowser(m_CurrentTheaterOverlayOrigHandle, true);

        vr::VROverlay()->SetOverlayRenderingPid(m_TheaterOverlayHandle, DPBrowserAPIClient::Get().GetServerAppProcessID());
    }
}

void OverlayManager::TheaterOverlayReturnCapture(const Overlay& ovrl_source)
{
    if (ovrl_source.GetTextureSource() == ovrl_texsource_winrt_capture)
    {
        DPWinRT_StopCapture(m_TheaterOverlayHandle);
    }
    else if (ovrl_source.GetTextureSource() == ovrl_texsource_ui)
    {
        vr::VROverlay()->SetOverlayRenderingPid(m_TheaterOverlayHandle, ::GetCurrentProcessId());
        IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_overlays_ui_reset);
    }
    else if (ovrl_source.GetTextureSource() == ovrl_texsource_browser)
    {
        DPBrowserAPIClient::Get().DPBrowser_StopBrowser(m_TheaterOverlayHandle);

        vr::VROverlay()->SetOverlayRenderingPid(m_TheaterOverlayHandle, ::GetCurrentProcessId());
    }
}

#endif //ifndef DPLUS_UI

unsigned int OverlayManager::DuplicateOverlay(const OverlayConfigData& data, unsigned int source_id)
{
    unsigned int id = (unsigned int)m_OverlayConfigData.size();

    #ifndef DPLUS_UI
        m_Overlays.emplace_back(id);
    #endif
    m_OverlayConfigData.push_back(data);

    OverlayConfigData& new_data = m_OverlayConfigData.back();

    #ifdef DPLUS_UI
        if (new_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui)
        {
            UIManager::Get()->GetPerformanceWindow().ScheduleOverlaySharedTextureUpdate();
        }
    #endif

    //Store duplication ID for browser overlays
    if ( (source_id != k_ulOverlayID_None) && (new_data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser) )
    {
        //If this is duplicated from an already duplicating overlay, keep the ID
        if (new_data.ConfigInt[configid_int_overlay_duplication_id] == -1)
        {
            new_data.ConfigInt[configid_int_overlay_duplication_id] = (int)source_id;
        }

        //Also clear strings which won't match after any navigation has been done on the source overlay but would hang around forever in profiles
        new_data.ConfigStr[configid_str_overlay_browser_url] = "";
        new_data.ConfigStr[configid_str_overlay_browser_url_user_last] = "";
        new_data.ConfigStr[configid_str_overlay_browser_title] = "";
    }

    //Send handle over to UI
    #ifndef DPLUS_UI
        new_data.ConfigHandle[configid_handle_overlay_state_overlay_handle] = m_Overlays.back().GetHandle();

        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)id);
        IPCManager::Get().PostConfigMessageToUIApp(configid_handle_overlay_state_overlay_handle, new_data.ConfigHandle[configid_handle_overlay_state_overlay_handle]);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
    #endif

    return id;
}

unsigned int OverlayManager::AddOverlay(OverlayCaptureSource capture_source, int desktop_id, HWND window_handle)
{
    unsigned int id = (unsigned int)m_OverlayConfigData.size();

    #ifndef DPLUS_UI
    m_Overlays.emplace_back(id);
    #endif
    m_OverlayConfigData.push_back(OverlayConfigData());

    OverlayConfigData& data = m_OverlayConfigData.back();

    //Load general default values from the default profile
    unsigned int current_id_old = m_CurrentOverlayID;
    m_CurrentOverlayID = id;
    ConfigManager::Get().LoadOverlayProfileDefault();
    m_CurrentOverlayID = current_id_old;

    //Apply additional defaults
    data.ConfigInt[configid_int_overlay_capture_source]                        = capture_source;
    data.ConfigFloat[configid_float_overlay_width]                             = 0.3f;
    data.ConfigFloat[configid_float_overlay_curvature]                         = 0.0f;
    data.ConfigFloat[configid_float_overlay_state_brightness_extra_multiplier] = 1.0f;

    switch (capture_source)
    {
        case ovrl_capsource_desktop_duplication:
        {
            data.ConfigInt[configid_int_overlay_desktop_id] = desktop_id;
            break;
        }
        case ovrl_capsource_winrt_capture:
        {
            data.ConfigInt[configid_int_overlay_winrt_desktop_id]       = desktop_id; //This just so happens to be -2 (unset) if called from ipcact_overlay_new_drag with HWND
            data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] = (intptr_t)window_handle;
           
            #ifdef DPLUS_UI
                const WindowInfo* window_info = WindowManager::Get().WindowListFindWindow(window_handle);

                if (window_info != nullptr)
                {
                    data.ConfigStr[configid_str_overlay_winrt_last_window_title]    = StringConvertFromUTF16(window_info->GetTitle().c_str());
                    data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name] = window_info->GetExeName();
                }
            #endif
            break;
        }
        case ovrl_capsource_ui:
        {
            #ifdef DPLUS_UI
                if ((UIManager::Get()) && (UIManager::Get()->IsOpenVRLoaded()))
                {
                    UIManager::Get()->GetPerformanceWindow().ScheduleOverlaySharedTextureUpdate();
                }
            #endif
            break;
        }
    }

    #ifdef DPLUS_UI
        SetOverlayNameAuto(id);
    #endif

    //Send handle over to UI
    #ifndef DPLUS_UI
        data.ConfigHandle[configid_handle_overlay_state_overlay_handle] = m_Overlays.back().GetHandle();

        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)id);
        IPCManager::Get().PostConfigMessageToUIApp(configid_handle_overlay_state_overlay_handle, data.ConfigHandle[configid_handle_overlay_state_overlay_handle]);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
    #endif

    return id;
}

unsigned int OverlayManager::AddUIOverlay()
{
    unsigned int id = DuplicateOverlay(OverlayConfigData());

    //Load general default values from the default profile
    unsigned int current_id_old = m_CurrentOverlayID;
    m_CurrentOverlayID = id;
    ConfigManager::Get().LoadOverlayProfileDefault();
    m_CurrentOverlayID = current_id_old;

    //Apply additional defaults
    OverlayConfigData& data = m_OverlayConfigData.back();
    data.ConfigInt[configid_int_overlay_origin]          = ovrl_origin_left_hand;
    data.ConfigInt[configid_int_overlay_capture_source]  = ovrl_capsource_ui;
    data.ConfigFloat[configid_float_overlay_width]       = 0.3f;
    data.ConfigFloat[configid_float_overlay_curvature]   = 0.0f;

    //Put the overlay on the left hand controller if possible, otherwise on right or just in the room
    #ifdef DPLUS_UI
    if ( (UIManager::Get()) && (UIManager::Get()->IsOpenVRLoaded()) )
    {
    #endif
        if (vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand) != vr::k_unTrackedDeviceIndexInvalid)
        {
            data.ConfigInt[configid_int_overlay_origin] = ovrl_origin_left_hand;
        }
        else if (vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) != vr::k_unTrackedDeviceIndexInvalid)
        {
            data.ConfigInt[configid_int_overlay_origin] = ovrl_origin_right_hand;
        }
        else
        {
            data.ConfigInt[configid_int_overlay_origin] = ovrl_origin_room;
            data.ConfigFloat[configid_float_overlay_width] = 0.7f;
        }
    #ifdef DPLUS_UI
    }
    #endif

    return id;
}

#ifndef DPLUS_UI

Overlay& OverlayManager::GetOverlay(unsigned int id)
{
    if (id < m_Overlays.size())
        return m_Overlays[id];
    else
        return m_OverlayNull; //Return null overlay when out of range
}

const Overlay& OverlayManager::GetOverlay(unsigned int id) const
{
    if (id < m_Overlays.size())
        return m_Overlays[id];
    else
        return m_OverlayNull; //Return null overlay when out of range
}

Overlay& OverlayManager::GetCurrentOverlay()
{
    return GetOverlay(m_CurrentOverlayID);
}

Overlay& OverlayManager::GetPrimaryDashboardOverlay()
{
    auto it = std::find_if(m_Overlays.begin(), m_Overlays.end(), [&](auto& overlay)
                           { return ( (GetConfigData(overlay.GetID()).ConfigInt[configid_int_overlay_origin] == ovrl_origin_dashboard) && (overlay.IsVisible()) ); });

    return (it != m_Overlays.end()) ? *it : m_OverlayNull;
}

vr::VROverlayHandle_t OverlayManager::GetTheaterOverlayHandle() const
{
    return m_TheaterOverlayHandle;
}

unsigned int OverlayManager::GetTheaterOverlayID() const
{
    return m_CurrentTheaterOverlayID;
}

void OverlayManager::SetTheaterOverlayID(unsigned int id)
{
    if (id >= m_OverlayConfigData.size())
    {
        ClearTheaterOverlay();
        return;
    }

    Overlay& ovrl_source = m_Overlays[id];

    if (m_CurrentTheaterOverlayID == k_ulOverlayID_None)
    {
        vr::VROverlayError ovrl_error = vr::VROverlayError_None;
        vr::VROverlayHandle_t ovrl_handle;
        vr::VROverlayHandle_t icon_unused;

        ovrl_error = vr::VROverlay()->CreateDashboardOverlay(g_AppKeyTheaterScreen, "Desktop+ Theater Screen", &ovrl_handle, &icon_unused);

        if (ovrl_error == vr::VROverlayError_None)
        {
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_NoDashboardTab,        true);
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_EnableControlBar,      true);
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_EnableControlBarClose, true);

            m_TheaterOverlayHandle = ovrl_handle;

            //Since the Theater Screen's width can't be queried, getting anything except the middle transform is difficult
            //We work around this restriction by using a cursor overlay as transform reference as it can be placed relative on the overlay and still be queried for its transform
            ovrl_error = vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusTheaterReference", "Desktop+ Theater Screen Reference", &ovrl_handle);

            if (ovrl_error == vr::VROverlayError_None)
            {
                //Empty overlay is probably fine, but give it something to work with just to be sure
                unsigned char bytes[2 * 2 * 4] = {0}; //2x2 transparent RGBA
                vr::VROverlay()->SetOverlayRaw(ovrl_handle, bytes, 2, 2, 4);

                //Apply initial cursor state, though position gets adjusted for every query as it needs to match overlay mouse scale
                vr::HmdVector2_t hotspot{0.5f, 0.5f};
                vr::HmdVector2_t pos{0.0f, 0.0f};
                vr::VROverlay()->SetOverlayCursor(m_TheaterOverlayHandle, ovrl_handle);
                vr::VROverlay()->SetOverlayTransformCursor(ovrl_handle, &hotspot);
                vr::VROverlay()->SetOverlayCursorPositionOverride(m_TheaterOverlayHandle, &pos);

                m_TheaterOverlayReferenceHandle = ovrl_handle;
            }
            else
            {
                LOG_F(WARNING, "Failed to create Theater Mode reference overlay: %s", vr::VROverlay()->GetOverlayErrorNameFromEnum(ovrl_error));
            }
        }
        else
        {
            LOG_F(WARNING, "Failed to create Theater Mode overlay: %s", vr::VROverlay()->GetOverlayErrorNameFromEnum(ovrl_error));
        }
    }
    else if (m_CurrentTheaterOverlayID < m_OverlayConfigData.size())
    {
        //Return previous source overlay to its own handle
        Overlay& ovrl_source_prev = m_Overlays[m_CurrentTheaterOverlayID];
        ovrl_source_prev.SetHandle(m_CurrentTheaterOverlayOrigHandle);
        vr::VROverlay()->SetOverlayAlpha(m_CurrentTheaterOverlayOrigHandle, ovrl_source.GetOpacity());  //Match opacity as its only set on changes
        ovrl_source_prev.SetVisible(false);                                                             //Mark it as invisible and have OutputManager reset it later

        m_OverlayConfigData[m_CurrentTheaterOverlayID].ConfigHandle[configid_handle_overlay_state_overlay_handle] = m_CurrentTheaterOverlayOrigHandle;
        ConfigManager::SetValue(configid_handle_state_theater_orig_overlay_handle, vr::k_ulOverlayHandleInvalid);

        //Send handle change over to UI
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)m_CurrentTheaterOverlayID);
        IPCManager::Get().PostConfigMessageToUIApp(configid_handle_overlay_state_overlay_handle, m_CurrentTheaterOverlayOrigHandle);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);

        IPCManager::Get().PostConfigMessageToUIApp(configid_handle_state_theater_orig_overlay_handle, vr::k_ulOverlayHandleInvalid);

        TheaterOverlayReturnCapture(ovrl_source_prev);
    }

    m_CurrentTheaterOverlayOrigHandle = ovrl_source.GetHandle();
    vr::VROverlay()->HideOverlay(m_CurrentTheaterOverlayOrigHandle);                     //Hide original overlay
    vr::VROverlay()->SetOverlayAlpha(m_TheaterOverlayHandle, ovrl_source.GetOpacity());  //Match opacity as its only set on changes
    ovrl_source.SetHandle(m_TheaterOverlayHandle);

    m_CurrentTheaterOverlayID = id;

    if (m_CurrentTheaterOverlayID < m_OverlayConfigData.size())
    {
        m_OverlayConfigData[m_CurrentTheaterOverlayID].ConfigHandle[configid_handle_overlay_state_overlay_handle] = m_TheaterOverlayHandle;
        ConfigManager::SetValue(configid_handle_state_theater_orig_overlay_handle, m_CurrentTheaterOverlayOrigHandle);

        //Send handle change over to UI
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)m_CurrentTheaterOverlayID);
        IPCManager::Get().PostConfigMessageToUIApp(configid_handle_overlay_state_overlay_handle, m_TheaterOverlayHandle);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);

        IPCManager::Get().PostConfigMessageToUIApp(configid_handle_state_theater_orig_overlay_handle, m_CurrentTheaterOverlayOrigHandle);

        TheaterOverlayForwardCapture(ovrl_source);
    }

    //Keep active overlay count correct
    if (OutputManager* outmgr = OutputManager::Get())
    {
        outmgr->ResetOverlayActiveCount();
    }
}

void OverlayManager::ClearTheaterOverlay(bool no_ui_update)
{
    if (m_CurrentTheaterOverlayID < m_OverlayConfigData.size())
    {
        //Return previous source overlay to its own handle
        Overlay& ovrl_source_prev = m_Overlays[m_CurrentTheaterOverlayID];
        ovrl_source_prev.SetHandle(m_CurrentTheaterOverlayOrigHandle);
        vr::VROverlay()->SetOverlayAlpha(m_CurrentTheaterOverlayOrigHandle, ovrl_source_prev.GetOpacity());  //Match opacity as its only set on changes
        ovrl_source_prev.SetVisible(false);                                                                  //Mark it as invisible and have OutputManager reset it later

        m_OverlayConfigData[m_CurrentTheaterOverlayID].ConfigHandle[configid_handle_overlay_state_overlay_handle] = m_CurrentTheaterOverlayOrigHandle;
        ConfigManager::SetValue(configid_handle_state_theater_orig_overlay_handle, vr::k_ulOverlayHandleInvalid);

        //Send handle change over to UI (this should be skipped when the current theater overlay is in the process of being removed)
        if (!no_ui_update)
        {
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)m_CurrentTheaterOverlayID);
            IPCManager::Get().PostConfigMessageToUIApp(configid_handle_overlay_state_overlay_handle, m_CurrentTheaterOverlayOrigHandle);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);

            IPCManager::Get().PostConfigMessageToUIApp(configid_handle_state_theater_orig_overlay_handle, vr::k_ulOverlayHandleInvalid);
        }

        TheaterOverlayReturnCapture(ovrl_source_prev);
    }

    if (m_TheaterOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->DestroyOverlay(m_TheaterOverlayHandle);

        if (m_TheaterOverlayReferenceHandle != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->DestroyOverlay(m_TheaterOverlayReferenceHandle);
        }
    }

    m_CurrentTheaterOverlayID         = k_ulOverlayID_None;
    m_CurrentTheaterOverlayOrigHandle = vr::k_ulOverlayHandleInvalid;
    m_TheaterOverlayHandle            = vr::k_ulOverlayHandleInvalid;
    m_TheaterOverlayReferenceHandle   = vr::k_ulOverlayHandleInvalid;

    //Keep active overlay count correct
    if (OutputManager* outmgr = OutputManager::Get())
    {
        outmgr->ResetOverlayActiveCount();
    }
}

#endif

OverlayConfigData& OverlayManager::GetConfigData(unsigned int id)
{
    if (id < m_OverlayConfigData.size())
        return m_OverlayConfigData[id];
    else
        return m_OverlayConfigDataNull; //Return null overlay data when out of range
}

const OverlayConfigData& OverlayManager::GetConfigData(unsigned int id) const
{
    if (id < m_OverlayConfigData.size())
        return m_OverlayConfigData[id];
    else
        return m_OverlayConfigDataNull; //Return null overlay data when out of range
}

OverlayConfigData& OverlayManager::GetCurrentConfigData()
{
    return GetConfigData(m_CurrentOverlayID);
}

OverlayOriginConfig OverlayManager::GetOriginConfigFromData(const OverlayConfigData& data) const
{
    OverlayOriginConfig origin_config;
    origin_config.HMDFloorUseTurning = data.ConfigBool[configid_bool_overlay_origin_hmd_floor_use_turning];

    return origin_config;
}

unsigned int OverlayManager::GetCurrentOverlayID() const
{
    return m_CurrentOverlayID;
}

void OverlayManager::SetCurrentOverlayID(unsigned int id)
{
    m_CurrentOverlayID = id;
}


#ifndef DPLUS_UI

unsigned int OverlayManager::FindOverlayID(vr::VROverlayHandle_t handle) const
{
    const auto it = std::find_if(m_Overlays.cbegin(), m_Overlays.cend(), [&](const auto& overlay){ return (overlay.GetHandle() == handle); });

    if (it != m_Overlays.cend())
    {
        return it->GetID();
    }
    else if (handle == m_CurrentTheaterOverlayOrigHandle)
    {
        return m_CurrentTheaterOverlayID;
    }

    return k_ulOverlayID_None;
}

unsigned int OverlayManager::FindTheaterOverlayID() const
{
    return m_CurrentTheaterOverlayID;
}

#else

unsigned int OverlayManager::FindOverlayID(vr::VROverlayHandle_t handle) const
{
    for (unsigned int i = 0; i < m_OverlayConfigData.size(); ++i)
    {
        if (m_OverlayConfigData[i].ConfigHandle[configid_handle_overlay_state_overlay_handle] == handle)
        {
            return i;
        }
    }

    //Theater overlay handle switcheroo is mostly transparent to the UI but in some instances it still needs to find the overlay based on its original handle while used in theater screen
    if (handle == ConfigManager::GetValue(configid_handle_state_theater_orig_overlay_handle))
    {
        return FindTheaterOverlayID();
    }

    return k_ulOverlayID_None;
}

unsigned int OverlayManager::FindTheaterOverlayID() const
{
    for (unsigned int i = 0; i < m_OverlayConfigData.size(); ++i)
    {
        const OverlayConfigData& data = m_OverlayConfigData[i];
        if ((data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen) && (data.ConfigBool[configid_bool_overlay_enabled]))
        {
            return i;
        }
    }

    return k_ulOverlayID_None;
}

#endif

unsigned int OverlayManager::GetOverlayCount() const
{
    return (unsigned int)m_OverlayConfigData.size();
}

void OverlayManager::SwapOverlays(unsigned int id, unsigned int id2)
{
    if ( (id == id2) || (id >= m_OverlayConfigData.size()) || (id2 >= m_OverlayConfigData.size()) )
        return;

    //Swap config data
    std::iter_swap(m_OverlayConfigData.begin() + id, m_OverlayConfigData.begin() + id2);

    #ifndef DPLUS_UI
        //Swap overlays and fix IDs
        std::iter_swap(m_Overlays.begin() + id, m_Overlays.begin() + id2);
        m_Overlays[id].SetID(id);
        m_Overlays[id2].SetID(id2);

        //Fixup theater overlay ID if needed
        m_CurrentTheaterOverlayID = (m_CurrentTheaterOverlayID == id) ? id2 : (m_CurrentTheaterOverlayID == id2) ? id : m_CurrentTheaterOverlayID;
    #endif

    #ifdef DPLUS_UI
        //Swap assigned keyboard overlay ID as well if it is one of the affected ones
        WindowKeyboard& window_keyboard = UIManager::Get()->GetVRKeyboard().GetWindow();
        int assigned_id = -1;

        assigned_id = window_keyboard.GetAssignedOverlayID(floating_window_ovrl_state_dashboard_tab);
        window_keyboard.SetAssignedOverlayID( (assigned_id == id) ? id2 : (assigned_id == id2) ? id : assigned_id, floating_window_ovrl_state_dashboard_tab);
        assigned_id = window_keyboard.GetAssignedOverlayID(floating_window_ovrl_state_room);
        window_keyboard.SetAssignedOverlayID( (assigned_id == id) ? id2 : (assigned_id == id2) ? id : assigned_id, floating_window_ovrl_state_room);
    #endif

    //Find and swap overlay duplication IDs
    for (auto& data : m_OverlayConfigData)
    {
        if (data.ConfigInt[configid_int_overlay_duplication_id] == (int)id)
            data.ConfigInt[configid_int_overlay_duplication_id] = (int)id2;
        else if (data.ConfigInt[configid_int_overlay_duplication_id] == (int)id2)
            data.ConfigInt[configid_int_overlay_duplication_id] = (int)id;
    }

    //Fixup focused overlay if needed
    int& focused_overlay_id = ConfigManager::Get().GetRef(configid_int_state_overlay_focused_id);
    focused_overlay_id = (focused_overlay_id == id) ? id2 : (focused_overlay_id == id2) ? id : focused_overlay_id;
}

void OverlayManager::RemoveOverlay(unsigned int id)
{
    if (id < m_OverlayConfigData.size())
    {
        //Find and fix overlays referencing the to-be-deleted overlay with their duplication ID first
        unsigned int replacement_id = k_ulOverlayID_None;
        for (unsigned int i = 0; i < m_OverlayConfigData.size(); ++i)
        {
            OverlayConfigData& data = m_OverlayConfigData[i];
            int& duplication_id = data.ConfigInt[configid_int_overlay_duplication_id];

            if (duplication_id == (int)id)
            {
                //Take the first of the duplicating overlays and convert it to standalone as the new source
                if (replacement_id == k_ulOverlayID_None)
                {
                    ConvertDuplicatedOverlayToStandalone(i, true);
                    replacement_id = i;
                }
                else    //We already have one, so just change the ID
                {
                    duplication_id = (int)replacement_id;
                }
            }

            //Also fix overlay duplication IDs reference overlays that had an higher ID than the removed one
            if ( (duplication_id != -1) && (duplication_id > (int)id) )
            {
                duplication_id--;
            }
        }

        #ifndef DPLUS_UI
            //Clear Theater overlay if it's currently used by this to ensure proper cleanup
            if (m_CurrentTheaterOverlayID == id)
            {
                ClearTheaterOverlay(true);
            }
        #endif

        //Then delete the config
        m_OverlayConfigData.erase(m_OverlayConfigData.begin() + id);

        #ifndef DPLUS_UI
            m_Overlays.erase(m_Overlays.begin() + id);

            //Fixup IDs for overlays past it if the overlay wasn't the last one
            if (id != m_Overlays.size())
            {
                for (auto& overlay : m_Overlays)
                {
                    if (overlay.GetID() > id)
                    {
                        overlay.SetID(overlay.GetID() - 1);
                    }
                }
            }

            if (m_CurrentTheaterOverlayID > id)
            {
                m_CurrentTheaterOverlayID--;
            }

            //Keep active count correct
            if (OutputManager* outmgr = OutputManager::Get())
            {
                outmgr->ResetOverlayActiveCount();
            }
        #else
            //Remove assigned keyboard overlay ID or fix it up if it was higher than the removed one
            WindowKeyboard& window_keyboard = UIManager::Get()->GetVRKeyboard().GetWindow();
            int assigned_id = -1;

            assigned_id = window_keyboard.GetAssignedOverlayID(floating_window_ovrl_state_dashboard_tab);
            if (assigned_id == (int)id)
            {
                window_keyboard.SetAssignedOverlayID(-1, floating_window_ovrl_state_dashboard_tab);
                window_keyboard.GetOverlayState(floating_window_ovrl_state_dashboard_tab).IsVisible = false;
            }
            else if (assigned_id > (int)id)
            {
                window_keyboard.SetAssignedOverlayID(assigned_id - 1, floating_window_ovrl_state_dashboard_tab);
            }

            assigned_id = window_keyboard.GetAssignedOverlayID(floating_window_ovrl_state_room);
            if (assigned_id == (int)id)
            {
                window_keyboard.SetAssignedOverlayID(-1, floating_window_ovrl_state_room);
                window_keyboard.GetOverlayState(floating_window_ovrl_state_room).IsVisible = false;
            }
            else if (assigned_id > (int)id)
            {
                window_keyboard.SetAssignedOverlayID(assigned_id - 1, floating_window_ovrl_state_room);
            }
        #endif

        //Fixup current overlay if needed
        if (m_CurrentOverlayID >= m_OverlayConfigData.size())
        {
            m_CurrentOverlayID--;
        }

        //Fixup focused overlay if needed
        int& focused_overlay_id = ConfigManager::Get().GetRef(configid_int_state_overlay_focused_id);

        if (focused_overlay_id == (int)id)
        {
            focused_overlay_id = -1;
        }
        else if (focused_overlay_id > (int)id)
        {
            focused_overlay_id--;
        }
    }
}

void OverlayManager::RemoveAllOverlays()
{
    #ifndef DPLUS_UI
        ClearTheaterOverlay(true);
    #endif

    //Remove all overlays with minimal overhead and refreshes
    while (m_OverlayConfigData.size() > 0)
    {
        m_OverlayConfigData.erase(m_OverlayConfigData.begin() + m_OverlayConfigData.size() - 1);

        #ifndef DPLUS_UI
            m_Overlays.erase(m_Overlays.begin() + m_Overlays.size() - 1);
        #endif
    }

    m_CurrentOverlayID = k_ulOverlayID_None;

    #ifndef DPLUS_UI
        //Fixup active overlay counts after we just removed everything that might've been considered active
        if (OutputManager* outmgr = OutputManager::Get())
        {
            outmgr->ResetOverlayActiveCount();
        }
    #endif
}

#ifndef DPLUS_UI

OverlayIDList OverlayManager::FindInactiveOverlaysForWindow(const WindowInfo& window_info) const
{
    OverlayIDList matching_overlay_ids;
    OverlayIDList candidate_overlay_ids;

    //Check if there are any candidates before comparing anything
    for (unsigned int i = 0; i < m_OverlayConfigData.size(); ++i)
    {
        const OverlayConfigData& data = m_OverlayConfigData[i];

        if ( (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && (data.ConfigInt[configid_int_overlay_winrt_desktop_id] == -2) &&
             (m_Overlays[i].GetTextureSource() == ovrl_texsource_none) )
        {
            candidate_overlay_ids.push_back(i);
        }
    }

    //If no candidates, stop here
    if (candidate_overlay_ids.empty())
        return matching_overlay_ids;

    //Do the basically the same as in WindowManager::FindClosestWindowForTitle(), but matching overlays to one window instead
    const std::string title_str = StringConvertFromUTF16(window_info.GetTitle().c_str());

    //Precompute UTF-16 window class names of candidate overlays (using total m_OverlayConfigData array size for direct indexing), as we're using WindowInfo's matching function
    std::vector<std::wstring> overlay_class_name_wstr;
    overlay_class_name_wstr.resize(m_OverlayConfigData.size());

    for (auto i : candidate_overlay_ids)
    {
        overlay_class_name_wstr[i] = WStringConvertFromUTF8(m_OverlayConfigData[i].ConfigStr[configid_str_overlay_winrt_last_window_class_name].c_str());
    }

    //Look for a complete match first 
    for (auto i : candidate_overlay_ids)
    {
        const OverlayConfigData& data = m_OverlayConfigData[i];

        if ( (window_info.IsClassNameMatching(overlay_class_name_wstr[i])) && (data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name] == window_info.GetExeName()) && 
             (data.ConfigStr[configid_str_overlay_winrt_last_window_title] == title_str) )
        {
            matching_overlay_ids.push_back(i);
        }
    }

    //Stop here if we already had at least a single match
    if (!matching_overlay_ids.empty())
    {
        return matching_overlay_ids;
    }

    //Cut off document part of title if it there is one
    std::string title_search = title_str;
    std::string app_name;
    size_t search_pos = title_str.rfind(" - ");

    if (search_pos != std::string::npos)
    {
        app_name = title_str.substr(search_pos);
    }

    //Try to find a partial match by removing the last word from the title string and appending the application name
    while ((search_pos != 0) && (search_pos != std::string::npos))
    {
        search_pos--;
        search_pos = title_str.find_last_of(L' ', search_pos);

        if (search_pos != std::string::npos)
        {
            title_search = title_str.substr(0, search_pos) + app_name;
        }
        else if (!app_name.empty()) //Last attempt, just the app-name
        {
            title_search = app_name;
        }
        else
        {
            break;
        }

        for (auto i : candidate_overlay_ids)
        {
            const OverlayConfigData& data = m_OverlayConfigData[i];

            //Check if class/exe name matches and search title can be found in the last stored window title (but just skip if overlay uses strict matching)
            if ( (!data.ConfigBool[configid_bool_overlay_winrt_window_matching_strict]) &&
                 (window_info.IsClassNameMatching(overlay_class_name_wstr[i])) && (data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name] == window_info.GetExeName()) &&
                 (data.ConfigStr[configid_str_overlay_winrt_last_window_title].find(title_search) != std::string::npos) )
            {
                matching_overlay_ids.push_back(i);
            }
        }

        //Don't reduce the title any further if we had at least a single match
        if (!matching_overlay_ids.empty())
        {
            break;
        }
    }

    //Nothing found, try to get a match with just the same class and exe name at least
    if (matching_overlay_ids.empty())
    {
        for (auto i : candidate_overlay_ids)
        {
            const OverlayConfigData& data = m_OverlayConfigData[i];

            if ( (!data.ConfigBool[configid_bool_overlay_winrt_window_matching_strict]) &&
                 (window_info.IsClassNameMatching(overlay_class_name_wstr[i])) && (data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name] == window_info.GetExeName()) )
            {
                matching_overlay_ids.push_back(i);
            }
        }
    }

    return matching_overlay_ids;
}

#endif //ifndef DPLUS_UI

OverlayIDList OverlayManager::FindDuplicatedOverlaysForOverlay(unsigned int source_id) const
{
    OverlayIDList matching_overlay_ids;

    for (unsigned int i = 0; i < m_OverlayConfigData.size(); ++i)
    {
        const OverlayConfigData& data = m_OverlayConfigData[i];

        if (data.ConfigInt[configid_int_overlay_duplication_id] == source_id)
        {
            matching_overlay_ids.push_back(i);
        }
    }

    return matching_overlay_ids;
}

OverlayIDList OverlayManager::FindOverlaysWithTags(const char* str_tags) const
{
    OverlayIDList matching_overlay_ids;

    for (unsigned int i = 0; i < m_OverlayConfigData.size(); ++i)
    {
        const OverlayConfigData& data = m_OverlayConfigData[i];

        //This isn't exactly the fastest way to do it, but this is called rarely and saves us from keeping a cache up to date and stuff
        if (MatchOverlayTags(str_tags, data.ConfigStr[configid_str_overlay_tags].c_str(), &data))
        {
            matching_overlay_ids.push_back(i);
        }
    }

    return matching_overlay_ids;
}

void OverlayManager::ConvertDuplicatedOverlayToStandalone(unsigned int id, bool no_reset)
{
    if (id < m_OverlayConfigData.size())
    {
        OverlayConfigData& data = m_OverlayConfigData[id];

        //Nothing to do if there's no valid duplication source ID
        if ( (data.ConfigInt[configid_int_overlay_duplication_id] == -1) && ((unsigned int)data.ConfigInt[configid_int_overlay_duplication_id] >= m_OverlayConfigData.size()) )
            return;

        OverlayConfigData& data_source = m_OverlayConfigData[data.ConfigInt[configid_int_overlay_duplication_id]];

        //Currently only works and used on browser overlays
        if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser)
        {
            //Copy relevant properties over
            data.ConfigStr[configid_str_overlay_browser_url]           = data_source.ConfigStr[configid_str_overlay_browser_url];
            data.ConfigStr[configid_str_overlay_browser_url_user_last] = data_source.ConfigStr[configid_str_overlay_browser_url_user_last];
            data.ConfigInt[configid_int_overlay_user_width]            = data_source.ConfigInt[configid_int_overlay_user_width];
            data.ConfigInt[configid_int_overlay_user_height]           = data_source.ConfigInt[configid_int_overlay_user_height];
            data.ConfigFloat[configid_float_overlay_browser_zoom]      = data_source.ConfigFloat[configid_float_overlay_browser_zoom];
        }

        data.ConfigInt[configid_int_overlay_state_content_width]  = data_source.ConfigInt[configid_int_overlay_state_content_width];
        data.ConfigInt[configid_int_overlay_state_content_height] = data_source.ConfigInt[configid_int_overlay_state_content_height];

        //Remove duplication ID and reset the overlay
        data.ConfigInt[configid_int_overlay_duplication_id] = -1;

        #ifndef DPLUS_UI
            if (!no_reset)
            {
                if (OutputManager* outmgr = OutputManager::Get())
                {
                    unsigned int current_overlay_old = m_CurrentOverlayID;
                    m_CurrentOverlayID = id;

                    GetOverlay(id).SetTextureSource(ovrl_texsource_invalid);    //Invalidate capture source so browser source is re-initialized with a new context

                    outmgr->ResetCurrentOverlay();

                    m_CurrentOverlayID = current_overlay_old;
                }
            }
        #endif
    }
}

#ifdef DPLUS_UI

std::vector<OverlayManager::TagListEntry> OverlayManager::GetKnownOverlayTagList()
{
    auto add_tags_from_tags_string = [](const std::string& str, std::vector<OverlayManager::TagListEntry>& list, std::unordered_set<std::string>& list_unique_tags)
    {
        std::string single_tag_str;
        std::stringstream ss(str);
        while (std::getline(ss, single_tag_str, ' '))
        {
            //Only add if not already in list
            if ((!single_tag_str.empty()) && (list_unique_tags.find(single_tag_str) == list_unique_tags.end()))
            {
                list.push_back({single_tag_str, false});
                list_unique_tags.insert(single_tag_str);
            }
        }
    };

    //List of built-in, automatically matched tags
    const char* auto_tags[] = {"Ovrl_All", "Ovrl_Visible", "Ovrl_Hidden", "Ovrl_Desktop", "Ovrl_Window", "Ovrl_Browser", "Ovrl_PerfMon"};

    std::vector<OverlayManager::TagListEntry> list;
    std::unordered_set<std::string> list_unique_tags;

    //Add auto tags
    for (const auto& tag_str : auto_tags)
    {
        list.push_back({tag_str, true});
        list_unique_tags.insert(tag_str);
    }

    //Add tags referenced by actions
    ActionManager& action_manager = ConfigManager::Get().GetActionManager();
    for (ActionUID uid : action_manager.GetActionOrderListUI())
    {
        const Action& action = action_manager.GetAction(uid);

        add_tags_from_tags_string(action.TargetTags, list, list_unique_tags);

        for (const auto& command : action.Commands)
        {
            //For Show Overlay, StrMain contains overlay tags
            if (command.Type == ActionCommand::command_show_overlay)
            {
                add_tags_from_tags_string(command.StrMain, list, list_unique_tags);
            }
        }
    }

    //Add tags referenced by overlays
    for (const auto& data : m_OverlayConfigData)
    {
        add_tags_from_tags_string(data.ConfigStr[configid_str_overlay_tags], list, list_unique_tags);
    }

    return list;
}

void OverlayManager::SetCurrentOverlayNameAuto(const WindowInfo* window_info)
{
    SetOverlayNameAuto(m_CurrentOverlayID, window_info);
}

void OverlayManager::SetOverlayNameAuto(unsigned int id, const WindowInfo* window_info)
{
    if (id < m_OverlayConfigData.size())
    {
        OverlayConfigData& data = m_OverlayConfigData[id];

        //Call is just silently ignored when overlay name is set to custom/user set already
        if (data.ConfigBool[configid_bool_overlay_name_custom])
            return;

        data.ConfigNameStr = "";

        //If override window info passed, use that
        if (window_info != nullptr)
        {
            data.ConfigNameStr += StringConvertFromUTF16(window_info->GetTitle().c_str());
            return;
        }

        switch (data.ConfigInt[configid_int_overlay_capture_source])
        {
            case ovrl_capsource_desktop_duplication:
            {
                data.ConfigNameStr += TranslationManager::Get().GetDesktopIDString(data.ConfigInt[configid_int_overlay_desktop_id]);
                break;
            }
            case ovrl_capsource_winrt_capture:
            {
                if (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0)
                {
                    data.ConfigNameStr += data.ConfigStr[configid_str_overlay_winrt_last_window_title];
                }
                else if (data.ConfigInt[configid_int_overlay_winrt_desktop_id] != -2)
                {
                    data.ConfigNameStr += TranslationManager::Get().GetDesktopIDString(data.ConfigInt[configid_int_overlay_winrt_desktop_id]);
                }
                else
                {
                    data.ConfigNameStr += TranslationManager::GetString(tstr_SourceWinRTNone);
                }
                break;
            }
            case ovrl_capsource_ui:
            {
                data.ConfigNameStr += TranslationManager::GetString(tstr_SourcePerformanceMonitor); //So far all UI overlays are just that
                break;
            }
            case ovrl_capsource_browser:
            {
                //Use duplication IDs' title if any is set
                const int duplication_id = data.ConfigInt[configid_int_overlay_duplication_id];
                const std::string& title = (duplication_id == -1) ? data.ConfigStr[configid_str_overlay_browser_title] : 
                                                                    GetConfigData((unsigned int)duplication_id).ConfigStr[configid_str_overlay_browser_title];

                if (!title.empty())
                {
                    data.ConfigNameStr += title;
                }
                else
                {
                    data.ConfigNameStr += TranslationManager::GetString(tstr_SourceBrowserNoPage);
                }
                break;
            }
        }
    }
}

void OverlayManager::SetOverlayNamesAutoForWindow(const WindowInfo& window_info)
{
    for (unsigned int i = 0; i < m_OverlayConfigData.size(); ++i)
    {
        const OverlayConfigData& data = m_OverlayConfigData[i];

        if ( (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && ((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] == window_info.GetWindowHandle()) )
        {
            SetOverlayNameAuto(i, &window_info);
        }
    }
}

#endif //ifdef DPLUS_UI

Matrix4 OverlayManager::GetOverlayMiddleTransform(unsigned int id, vr::VROverlayHandle_t ovrl_handle) const
{
    //Get handle if none was given
    if (ovrl_handle == vr::k_ulOverlayHandleInvalid)
    {
        #ifdef DPLUS_UI
            ovrl_handle = GetConfigData(id).ConfigHandle[configid_handle_overlay_state_overlay_handle];
        #else
            ovrl_handle = GetOverlay(id).GetHandle();
        #endif

        //Couldn't find overlay, return identity matrix
        if (ovrl_handle == vr::k_ulOverlayHandleInvalid)
        {
            return Matrix4();
        }
    }

    return GetOverlayTransformBase(ovrl_handle, id, false);
}

Matrix4 OverlayManager::GetOverlayCenterBottomTransform(unsigned int id, vr::VROverlayHandle_t ovrl_handle) const
{
    //Get handle if none was given
    if (ovrl_handle == vr::k_ulOverlayHandleInvalid)
    {
        #ifdef DPLUS_UI
            ovrl_handle = GetConfigData(id).ConfigHandle[configid_handle_overlay_state_overlay_handle];
        #else
            ovrl_handle = GetOverlay(id).GetHandle();
        #endif

        //Couldn't find overlay, return identity matrix
        if (ovrl_handle == vr::k_ulOverlayHandleInvalid)
        {
            return Matrix4();
        }
    }

    return GetOverlayTransformBase(ovrl_handle, id, true);
}

bool OverlayManager::MatchOverlayTagSingle(const char* str_tags, const char* str_single_tag)
{
    return MatchOverlayTagSingle(str_tags, str_tags + strlen(str_tags), str_single_tag, strlen(str_single_tag));
}

bool OverlayManager::MatchOverlayTagSingle(const char* str_tags, const char* str_tags_end, const char* str_single_tag, size_t str_single_tag_length)
{
    //Split input string into individual tags and compare each to the single tag
    const char* tag_start = str_tags;
    const char* tag_end   = nullptr;

    while (tag_start < str_tags_end)
    {
        tag_end = (const char*)memchr(tag_start, ' ', str_tags_end - tag_start);

        if (tag_end == nullptr)
            tag_end = str_tags_end;

        size_t tag_length = tag_end - tag_start;

        if ((tag_length == str_single_tag_length) && (memcmp(tag_start, str_single_tag, tag_length) == 0))
        {
            return true;
        }

        tag_start = tag_end + 1;
    }

    return false;
}

bool OverlayManager::MatchOverlayTags(const char* str_tags_a, const char* str_tags_b, const OverlayConfigData* data_b)
{
    //Split string a into individual tags and compare each to string b
    const char* str_tags_a_end = str_tags_a + strlen(str_tags_a);
    const char* tag_a_start    = str_tags_a;
    const char* tag_a_end      = nullptr;
    const char* str_tags_b_end = str_tags_b + strlen(str_tags_b);

    while (tag_a_start < str_tags_a_end)
    {
        tag_a_end = (const char*)memchr(tag_a_start, ' ', str_tags_a_end - tag_a_start);

        if (tag_a_end == nullptr)
            tag_a_end = str_tags_a_end;

        size_t tag_a_length = tag_a_end - tag_a_start;

        //Handle built-in auto tags
        if (data_b != nullptr)
        {
            //tag_a_length doesn't count the NUL terminator, but sizeof does, so we add 1 when comparing lengths but only compare actual tag length
            if ((tag_a_length + 1 == sizeof("Ovrl_All")) && (memcmp(tag_a_start, "Ovrl_All", tag_a_length) == 0))
            {
                return true;
            }
            else if ((tag_a_length + 1 == sizeof("Ovrl_Visible")) && (memcmp(tag_a_start, "Ovrl_Visible", tag_a_length) == 0))
            {
                if (data_b->ConfigBool[configid_bool_overlay_enabled])
                {
                    return true;
                }
            }
            else if ((tag_a_length + 1 == sizeof("Ovrl_Hidden")) && (memcmp(tag_a_start, "Ovrl_Hidden", tag_a_length) == 0))
            {
                if (!data_b->ConfigBool[configid_bool_overlay_enabled])
                {
                    return true;
                }
            }
            else if ((tag_a_length + 1 == sizeof("Ovrl_Desktop")) && (memcmp(tag_a_start, "Ovrl_Desktop", tag_a_length) == 0))
            {
                if (  (data_b->ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication) || 
                    ( (data_b->ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && (data_b->ConfigInt[configid_int_overlay_winrt_desktop_id] != -2)) )
                {
                    return true;
                }
            }
            else if ((tag_a_length + 1 == sizeof("Ovrl_Window")) && (memcmp(tag_a_start, "Ovrl_Window", tag_a_length) == 0))
            {
                if ( (data_b->ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && (data_b->ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0) )
                {
                    return true;
                }
            }
            else if ((tag_a_length + 1 == sizeof("Ovrl_Browser")) && (memcmp(tag_a_start, "Ovrl_Browser", tag_a_length) == 0))
            {
                if (data_b->ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser)
                {
                    return true;
                }
            }
            else if ((tag_a_length + 1 == sizeof("Ovrl_PerfMon")) && (memcmp(tag_a_start, "Ovrl_PerfMon", tag_a_length) == 0))
            {
                if (data_b->ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui)
                {
                    return true;
                }
            }
        }

        if (MatchOverlayTagSingle(str_tags_b, str_tags_b_end, tag_a_start, tag_a_length))
        {
            return true;
        }

        tag_a_start = tag_a_end + 1;
    }

    return false;
}
