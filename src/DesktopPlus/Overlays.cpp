#include "Overlays.h"

#include <sstream>

#include "CommonTypes.h"
#include "OverlayManager.h"
#include "OutputManager.h"
#include "DesktopPlusWinRT.h"

Overlay::Overlay(unsigned int id) : m_ID(id),
                                    m_OvrlHandle(vr::k_ulOverlayHandleInvalid),
                                    m_Visible(false),
                                    m_Opacity(1.0f),
                                    m_GlobalInteractive(false),
                                    m_TextureSource(ovrl_texsource_desktop_duplication)
{
    //Don't call InitOverlay when OpenVR isn't loaded yet. This happens during startup when loading the config and will be fixed up by OutputManager::InitOverlay() afterwards
    if (vr::VROverlay() != nullptr)
    {
        InitOverlay();
    }
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

            vr::VROverlay()->DestroyOverlay(m_OvrlHandle);
        }

        m_ID = b.m_ID;
        m_OvrlHandle = b.m_OvrlHandle;
        m_Visible = b.m_Visible;
        m_Opacity = b.m_Opacity;
        m_ValidatedCropRect = b.m_ValidatedCropRect;
        m_GlobalInteractive = b.m_GlobalInteractive;
        m_TextureSource = b.m_TextureSource;
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

        vr::VROverlay()->DestroyOverlay(m_OvrlHandle);
    }
}


void Overlay::InitOverlay()
{
    std::string key = "elvissteinjr.DesktopPlus" + std::to_string(m_ID);

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

            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), m_ID);
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_overlay_state_content_width),  dwidth);
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_overlay_state_content_height), dheight);
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
        }

        //Exclude indirect desktop duplication sources, like converted Over-Under 3D
        if (m_TextureSource != ovrl_texsource_desktop_duplication)
            return;

        //Use desktop texture overlay as source for a shared overlay texture
        ID3D11Resource* device_texture_ref = (OutputManager::Get()->GetMultiGPUTargetTexture() != nullptr) ? OutputManager::Get()->GetMultiGPUTargetTexture() : OutputManager::Get()->GetOverlayTexture();
        SetSharedOverlayTexture(outmgr->GetDesktopTextureOverlay(), m_OvrlHandle, device_texture_ref);
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
    else if ( (opacity == 0.0f) && (m_Visible) ) //If it's 0% now, hide if currently visible
    {
        outmgr->HideOverlay(m_ID);
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
}

bool Overlay::IsVisible() const
{
    return m_Visible;
}

bool Overlay::ShouldBeVisible() const
{
    if (m_Opacity == 0.0f)
        return false;

    bool should_be_visible = false;

    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_ID);

    if (!data.ConfigBool[configid_bool_overlay_enabled])
        return false;

    if (data.ConfigBool[configid_bool_overlay_detached])
    {
        OutputManager* outmgr = OutputManager::Get();
        if (outmgr == nullptr)
            return false;

        switch (data.ConfigInt[configid_int_overlay_detached_display_mode])
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
    }
    else
    {
        should_be_visible = ((OutputManager::Get() != nullptr) && (OutputManager::Get()->IsDashboardTabActive()));
    }

    return should_be_visible;
}

void Overlay::SetGlobalInteractiveFlag(bool interactive)
{
    if (m_GlobalInteractive != interactive) //Avoid spamming flag changes
    {
        vr::VROverlay()->SetOverlayFlag(m_OvrlHandle, vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, interactive);
        m_GlobalInteractive = interactive;
    }
}

bool Overlay::GetGlobalInteractiveFlag()
{
    return m_GlobalInteractive;
}

void Overlay::UpdateValidatedCropRect()
{
    OutputManager* outmgr = OutputManager::Get();
    if (outmgr == nullptr)
        return;

    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_ID);

    int x, y, width, height;

    x              = std::min( std::max(0, data.ConfigInt[configid_int_overlay_crop_x]), data.ConfigInt[configid_int_overlay_state_content_width]);
    y              = std::min( std::max(0, data.ConfigInt[configid_int_overlay_crop_y]), data.ConfigInt[configid_int_overlay_state_content_height]);
    width          = data.ConfigInt[configid_int_overlay_crop_width];
    height         = data.ConfigInt[configid_int_overlay_crop_height];
    int width_max  = data.ConfigInt[configid_int_overlay_state_content_width]  - x;
    int height_max = data.ConfigInt[configid_int_overlay_state_content_height] - y;

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
    //Skip if nothing changed
    if (m_TextureSource == tex_source)
        return;

    //Cleanup old sources if needed
    switch (m_TextureSource)
    {
        case ovrl_texsource_desktop_duplication_3dou_converted: m_OUtoSBSConverter.CleanRefs();    break;
        case ovrl_texsource_winrt_capture:                      DPWinRT_StopCapture(m_OvrlHandle); break;
        default: break;
    }

    m_TextureSource = tex_source;

    if (m_TextureSource == ovrl_texsource_none)
    {
        if (OutputManager* outmgr = OutputManager::Get())
        {
            outmgr->SetOutputErrorTexture(m_OvrlHandle);
        }
    }
    else if (m_TextureSource == ovrl_texsource_desktop_duplication)
    {
        AssignDesktopDuplicationTexture();
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
