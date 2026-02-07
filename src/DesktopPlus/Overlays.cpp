#include "Overlays.h"

#include <sstream>

#include "CommonTypes.h"
#include "OpenVRExt.h"
#include "OverlayManager.h"
#include "OutputManager.h"
#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"

Overlay::Overlay(unsigned int id) : m_ID(id),
                                    m_OvrlHandle(vr::k_ulOverlayHandleInvalid),
                                    m_Visible(false),
                                    m_Opacity(1.0f),
                                    m_TextureSource(ovrl_texsource_invalid)
{
    //Don't call InitOverlay when OpenVR isn't loaded yet. This happens during startup when loading the config and will be fixed up by OutputManager::InitOverlay() afterwards
    if (vr::VROverlay() != nullptr)
    {
        InitOverlay();
    }

    m_SmootherPos.SetDetectInterruptions(false);
    m_SmootherRot.SetDetectInterruptions(false);
}

Overlay::Overlay(Overlay&& b)
{
    m_OvrlHandle = vr::k_ulOverlayHandleInvalid; //This needs a valid value first
    *this = std::move(b);
}

Overlay& Overlay::operator=(Overlay&& b)
{
    if (this != &b)
    {
        if (m_OvrlHandle != vr::k_ulOverlayHandleInvalid)
        {
            if (m_TextureSource == ovrl_texsource_winrt_capture)
            {
                DPWinRT_StopCapture(m_OvrlHandle);
            }
            else if (m_TextureSource == ovrl_texsource_browser)
            {
                DPBrowserAPIClient::Get().DPBrowser_StopBrowser(m_OvrlHandle);
            }

            vr::VROverlayEx()->DestroyOverlayEx(m_OvrlHandle);
        }

        m_ID                = b.m_ID;
        m_OvrlHandle        = b.m_OvrlHandle;
        m_Visible           = b.m_Visible;
        m_Opacity           = b.m_Opacity;
        m_ValidatedCropRect = b.m_ValidatedCropRect;
        m_TextureSource     = b.m_TextureSource;
        //m_OUtoSBSConverter should just be left alone, it only holds cached state anyways

        b.m_OvrlHandle = vr::k_ulOverlayHandleInvalid;
    }

    return *this;
}

Overlay::~Overlay()
{
    if (m_OvrlHandle != vr::k_ulOverlayHandleInvalid)
    {
        if (m_TextureSource == ovrl_texsource_winrt_capture)
        {
            DPWinRT_StopCapture(m_OvrlHandle);
        }
        else if (m_TextureSource == ovrl_texsource_browser)
        {
            DPBrowserAPIClient::Get().DPBrowser_StopBrowser(m_OvrlHandle);
        }

        vr::VROverlayEx()->DestroyOverlayEx(m_OvrlHandle);
    }
}


void Overlay::InitOverlay()
{
    unsigned int id_offset = 0;
    std::string key;
    vr::VROverlayHandle_t overlay_handle_find;

    //Generate overlay key from ID and check if it's not used yet, add to it if it's not
    //Overlay keys & handles are fixed and don't change when overlays are re-ordered or deleted
    do
    {
        key = "elvissteinjr.DesktopPlus" + std::to_string(m_ID + id_offset);
        overlay_handle_find = vr::k_ulOverlayHandleInvalid;
        id_offset++;

        vr::VROverlay()->FindOverlay(key.c_str(), &overlay_handle_find);
    }
    while (overlay_handle_find != vr::k_ulOverlayHandleInvalid);

    vr::VROverlayError ovrl_error = vr::VROverlayError_None;
    ovrl_error = vr::VROverlay()->CreateOverlay(key.c_str(), "Desktop+", &m_OvrlHandle);

    if (ovrl_error == vr::VROverlayError_None)
    {
        vr::VROverlay()->SetOverlayAlpha(m_OvrlHandle, m_Opacity);
    } 
    else //Creation failed, send error to UI so the user at least knows (typically this only happens when the overlay limit is exceeded)
    {
        IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_overlay_creation_error, ovrl_error);
    }
}

void Overlay::AssignDesktopDuplicationTexture()
{
    if (m_OvrlHandle != vr::k_ulOverlayHandleInvalid)
    {
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_ID);

        if (data.ConfigInt[configid_int_overlay_capture_source] != ovrl_capsource_desktop_duplication)
            return;

        OutputManager* outmgr = OutputManager::Get();
        if (outmgr == nullptr)
            return;

        //Set content size to desktop duplication values
        int dwidth  = outmgr->GetDesktopWidth();
        int dheight = outmgr->GetDesktopHeight();

        //Avoid sending it over to UI if we can help it
        if ( (data.ConfigInt[configid_int_overlay_state_content_width] != dwidth) || (data.ConfigInt[configid_int_overlay_state_content_height] != dheight) )
        {
            data.ConfigInt[configid_int_overlay_state_content_width]  = dwidth;
            data.ConfigInt[configid_int_overlay_state_content_height] = dheight;
            UpdateValidatedCropRect();

            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, m_ID);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_width,  dwidth);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_height, dheight);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
        }

        //Exclude indirect desktop duplication sources, like converted Over-Under 3D
        if (m_TextureSource != ovrl_texsource_desktop_duplication)
            return;

        //Use desktop texture overlay as source for a shared overlay texture
        ID3D11Resource* device_texture_ref = (OutputManager::Get()->GetMultiGPUTargetTexture() != nullptr) ? OutputManager::Get()->GetMultiGPUTargetTexture() : OutputManager::Get()->GetOverlayTexture();
        vr::VROverlayEx()->SetSharedOverlayTexture(outmgr->GetDesktopTextureOverlay(), m_OvrlHandle, device_texture_ref);
    }
}

unsigned int Overlay::GetID() const
{
    return m_ID;
}

void Overlay::SetID(unsigned int id)
{
    m_ID = id;
}

vr::VROverlayHandle_t Overlay::GetHandle() const
{
    return m_OvrlHandle;
}

void Overlay::SetHandle(vr::VROverlayHandle_t handle)
{
    m_OvrlHandle = handle;
}

void Overlay::SetOpacity(float opacity)
{
    if (opacity == m_Opacity)
        return;

    OutputManager* outmgr = OutputManager::Get();
    if (outmgr == nullptr)
        return;

    vr::VROverlay()->SetOverlayAlpha(m_OvrlHandle, opacity);

    if (m_Opacity == 0.0f) //If it was previously 0%, show if needed
    {
        m_Opacity = opacity; //ShouldBeVisible() depends on this being correct, so set it here

        if ( (!m_Visible) && (ShouldBeVisible()) )
        {
            outmgr->ShowOverlay(m_ID);
        }
    }
    else if (opacity == 0.0f) //If it's 0% now, hide if it shouldn't be visible (Update when Invisble setting can make ShouldBeVisible() return true still)
    {
        m_Opacity = opacity;

        if (!ShouldBeVisible())
        {
            outmgr->HideOverlay(m_ID);
        }
    }

    m_Opacity = opacity;
}

float Overlay::GetOpacity() const
{
    return m_Opacity;
}

void Overlay::SetVisible(bool visible)
{
    m_Visible = visible;
    (visible) ? vr::VROverlay()->ShowOverlay(m_OvrlHandle) : vr::VROverlay()->HideOverlay(m_OvrlHandle);

    m_SmootherPos.ResetLastPos();
    m_SmootherRot.ResetLastPos();
}

bool Overlay::IsVisible() const
{
    return m_Visible;
}

bool Overlay::ShouldBeVisible() const
{
    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_ID);
    
    if ( (m_Opacity == 0.0f) && (!data.ConfigBool[configid_bool_overlay_update_invisible]) )
        return false;

    bool should_be_visible = false;

    if (!data.ConfigBool[configid_bool_overlay_enabled])
        return false;

    //Enabled theater mode is always visible
    if (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen)
        return true;

    OutputManager* outmgr = OutputManager::Get();
    if (outmgr == nullptr)
        return false;

    switch (data.ConfigInt[configid_int_overlay_display_mode])
    {
        case ovrl_dispmode_always:
        {
            should_be_visible = true;
            break;
        }
        case ovrl_dispmode_dashboard:
        {
            //Our method for getting the dashboard transform only works after it has been manually been brought up once OR the Desktop+ tab has been shown
            //In practice this means we won't be showing dashboard display mode overlays on the initial SteamVR dashboard that is active when booting up
            should_be_visible = ( (outmgr->HasDashboardBeenActivatedOnce()) && (vr::VROverlay()->IsDashboardVisible()) );
            break;
        }
        case ovrl_dispmode_scene:
        {
            should_be_visible = !vr::VROverlay()->IsDashboardVisible();
            break;
        }
        case ovrl_dispmode_dplustab:
        {
            should_be_visible = (outmgr->IsDashboardTabActive());
            break;
        }
    }

    //Also apply above when origin is dashboard
    if ( (should_be_visible) && (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_dashboard) )
    {
        should_be_visible = outmgr->HasDashboardBeenActivatedOnce();
    }

    return should_be_visible;
}

void Overlay::UpdateValidatedCropRect()
{
    OutputManager* outmgr = OutputManager::Get();
    if (outmgr == nullptr)
        return;

    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_ID);

    int x, y, width, height;

    if (data.ConfigBool[configid_bool_overlay_crop_enabled])
    {
        x      = std::min( std::max(0, data.ConfigInt[configid_int_overlay_crop_x]), data.ConfigInt[configid_int_overlay_state_content_width]);
        y      = std::min( std::max(0, data.ConfigInt[configid_int_overlay_crop_y]), data.ConfigInt[configid_int_overlay_state_content_height]);
        width  = data.ConfigInt[configid_int_overlay_crop_width];
        height = data.ConfigInt[configid_int_overlay_crop_height];
    }
    else //Fall back to default crop when cropping is disabled
    {
        //Current desktop cropping values for desktop duplication
        if ((m_TextureSource == ovrl_texsource_desktop_duplication) || (m_TextureSource == ovrl_texsource_desktop_duplication_3dou_converted))
        {
            outmgr->CropToDisplay(data.ConfigInt[configid_int_overlay_desktop_id], x, y, width, height);
        }
        else //Content size for everything else
        {
            x = 0;
            y = 0;
            width  = data.ConfigInt[configid_int_overlay_state_content_width];
            height = data.ConfigInt[configid_int_overlay_state_content_height];
        }
    }

    int width_max  = std::max(data.ConfigInt[configid_int_overlay_state_content_width]  - x, 1);
    int height_max = std::max(data.ConfigInt[configid_int_overlay_state_content_height] - y, 1);

    if (width == -1)
        width = width_max;
    else
        width = std::min(width, width_max);

    if (height == -1)
        height = height_max;
    else
        height = std::min(height, height_max);

    m_ValidatedCropRect = DPRect(x, y, x + width, y + height);
}

const DPRect& Overlay::GetValidatedCropRect() const
{
    return m_ValidatedCropRect;
}

void Overlay::SetTextureSource(OverlayTextureSource tex_source)
{
    //Skip if nothing changed (except texsource_ui/browser which are always re-applied)
    if ( (m_TextureSource == tex_source) && (tex_source != ovrl_texsource_ui) && (tex_source != ovrl_texsource_browser) )
        return;

    //Cleanup old sources if needed
    switch (m_TextureSource)
    {
        case ovrl_texsource_desktop_duplication_3dou_converted: m_OUtoSBSConverter.CleanRefs();    break;
        case ovrl_texsource_winrt_capture:                      DPWinRT_StopCapture(m_OvrlHandle); break;
        case ovrl_texsource_ui:
        {
            if (tex_source != ovrl_texsource_ui)
            {
                vr::VROverlay()->SetOverlayRenderingPid(m_OvrlHandle, ::GetCurrentProcessId());
                vr::VROverlay()->SetOverlayIntersectionMask(m_OvrlHandle, nullptr, 0);
            }
            break;
        }
        case ovrl_texsource_browser:
        {
            if (tex_source != ovrl_texsource_browser)
            {
                DPBrowserAPIClient::Get().DPBrowser_StopBrowser(m_OvrlHandle);
                vr::VROverlay()->SetOverlayRenderingPid(m_OvrlHandle, ::GetCurrentProcessId());
            }
            break;
        }
        default: break;
    }

    //If this overlay is the theater overlay, hide it so there's always proper release from the old texture source happening
    if ( (m_TextureSource != tex_source) && (OverlayManager::Get().GetTheaterOverlayID() == m_ID) )
    {
        if (OutputManager* outmgr = OutputManager::Get())
        {
            outmgr->HideOverlay(m_ID);
        }
    }

    m_TextureSource = tex_source;

    switch (m_TextureSource)
    {
        case ovrl_texsource_none:
        {
            if (OutputManager* outmgr = OutputManager::Get())
            {
                outmgr->SetOutputErrorTexture(m_OvrlHandle);
            }
            break;
        }
        case ovrl_texsource_desktop_duplication:                /*fallthrough*/
        case ovrl_texsource_desktop_duplication_3dou_converted: AssignDesktopDuplicationTexture();                                                                        break;
        case ovrl_texsource_ui:                                 vr::VROverlay()->SetOverlayRenderingPid(m_OvrlHandle, IPCManager::GetUIAppProcessID());                   break;
        case ovrl_texsource_browser:                            vr::VROverlay()->SetOverlayRenderingPid(m_OvrlHandle, DPBrowserAPIClient::Get().GetServerAppProcessID()); break;
        default: break;
    }

    //Set output error config state
    {
        const bool has_no_output = (m_TextureSource == ovrl_texsource_none);
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_ID);

        if (data.ConfigBool[configid_bool_overlay_state_no_output] != has_no_output)
        {
            data.ConfigBool[configid_bool_overlay_state_no_output] = has_no_output;

            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, m_ID);
            IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_state_no_output, has_no_output);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
        }
    }
}

OverlayTextureSource Overlay::GetTextureSource() const
{
    return m_TextureSource;
}

void Overlay::OnDesktopDuplicationUpdate()
{
    if ( (m_Visible) && (m_TextureSource == ovrl_texsource_desktop_duplication_3dou_converted) )
    {
        OutputManager::Get()->ConvertOUtoSBS(*this, m_OUtoSBSConverter);
    }
}

RadialFollowCore& Overlay::GetSmootherPos()
{
    return m_SmootherPos;
}

RadialFollowCore& Overlay::GetSmootherRot()
{
    return m_SmootherRot;
}
