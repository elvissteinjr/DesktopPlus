#include "OverlayManager.h"

#ifndef DPLUS_UI
    #include "OutputManager.h"
    #include "DesktopPlusWinRT.h"
#endif

#include <sstream>

static OverlayManager g_OverlayManager;

OverlayManager& OverlayManager::Get()
{
    return g_OverlayManager;
}

OverlayManager::OverlayManager() : m_CurrentOverlayID(0)
{
    //Add a dashboard overlay placeholder so there's always one overlay set up
    AddOverlay(OverlayConfigData());
}

unsigned int OverlayManager::AddOverlay(const OverlayConfigData& data, bool is_based_on_dashboard)
{
    unsigned int id = (unsigned int)m_OverlayConfigData.size();

    #ifndef DPLUS_UI
        m_Overlays.emplace_back(id);
    #endif
    m_OverlayConfigData.push_back(data);

    //All overlays except the dashboard one are detached
    if (id != k_ulOverlayID_Dashboard)
    {
        m_OverlayConfigData.back().ConfigBool[configid_bool_overlay_detached] = true;

        if (is_based_on_dashboard)
        {
            //Desktop buttons should not be enabled by default on floating overlays
            m_OverlayConfigData.back().ConfigBool[configid_bool_overlay_floatingui_desktops_enabled] = false;
        }
    }

    return id;
}

#ifndef DPLUS_UI

Overlay& OverlayManager::GetOverlay(unsigned int id)
{
    if (id < m_Overlays.size())
        return m_Overlays[id];
    else
        return m_Overlays[k_ulOverlayID_Dashboard]; //Return dashboard overlay, which always exists, when out of range
}

Overlay& OverlayManager::GetCurrentOverlay()
{
    return GetOverlay(m_CurrentOverlayID);
}

unsigned int OverlayManager::FindOverlayID(vr::VROverlayHandle_t handle)
{
    const auto it = std::find_if(m_Overlays.begin(), m_Overlays.end(), [&](const auto& overlay){ return (overlay.GetHandle() == handle); });

    return (it != m_Overlays.end()) ? it->GetID() : k_ulOverlayID_None;
}

#endif

OverlayConfigData& OverlayManager::GetConfigData(unsigned int id)
{
    if (id < m_OverlayConfigData.size())
        return m_OverlayConfigData[id];
    else
        return m_OverlayConfigData[k_ulOverlayID_Dashboard]; //Return dashboard overlay data, which always exists, when out of range
}

OverlayConfigData& OverlayManager::GetCurrentConfigData()
{
    return GetConfigData(m_CurrentOverlayID);
}

unsigned int OverlayManager::GetCurrentOverlayID() const
{
    return m_CurrentOverlayID;
}

void OverlayManager::SetCurrentOverlayID(unsigned int id)
{
    m_CurrentOverlayID = id;
}

vr::VROverlayHandle_t OverlayManager::FindOverlayHandle(unsigned int id)
{
    vr::VROverlayHandle_t ret = vr::k_ulOverlayHandleInvalid;
    std::string key = "elvissteinjr.DesktopPlus" + std::to_string(id);

    vr::VROverlay()->FindOverlay(key.c_str(), &ret);

    return ret;
}

unsigned int OverlayManager::GetOverlayCount() const
{
    return (unsigned int)m_OverlayConfigData.size();
}

void OverlayManager::SwapOverlays(unsigned int id, unsigned int id2)
{
    if ( (id == id2) || (id == k_ulOverlayID_Dashboard) || (id2 == k_ulOverlayID_Dashboard) || (id >= m_OverlayConfigData.size()) || (id2 >= m_OverlayConfigData.size()) )
        return;

    std::iter_swap(m_OverlayConfigData.begin() + id, m_OverlayConfigData.begin() + id2);

    #ifndef DPLUS_UI
        Overlay& overlay   = GetOverlay(id);
        Overlay& overlay_2 = GetOverlay(id2);

        //If any if the swapped overlays are active WinRT capture targets, swap them there as well
        if ((overlay.GetTextureSource() == ovrl_texsource_winrt_capture) || (overlay_2.GetTextureSource() == ovrl_texsource_winrt_capture))
        {
            DPWinRT_SwapCaptureTargetOverlays(overlay.GetHandle(), overlay_2.GetHandle());

            //Despite below comment, set texture sources directly if needed to avoid interruption of capture from resetting the overlays later
            OverlayTextureSource source_temp = overlay.GetTextureSource();

            if (overlay_2.GetTextureSource() == ovrl_texsource_winrt_capture)
                overlay.SetTextureSource(ovrl_texsource_winrt_capture);
            else if (source_temp == ovrl_texsource_winrt_capture)
                overlay_2.SetTextureSource(ovrl_texsource_winrt_capture);

        }

        //We don't swap the overlays themselves, instead we reset the overlays with the swapped config data
        //The Overlay class is supposed to hold the state of the OpenVR overlay only, so this may break if it's not being kept like that
        if (OutputManager* outmgr = OutputManager::Get())
        {
            unsigned int current_overlay_old = GetCurrentOverlayID();
            SetCurrentOverlayID(id);
            outmgr->ResetCurrentOverlay();
            SetCurrentOverlayID(id2);
            outmgr->ResetCurrentOverlay();
            SetCurrentOverlayID(current_overlay_old);
        }
    #endif
}

void OverlayManager::RemoveOverlay(unsigned int id)
{
    if (id < m_OverlayConfigData.size())
    {
        #ifndef DPLUS_UI
            //Hide overlay before removal to keep active count correct
            if (OutputManager* outmgr = OutputManager::Get())
            {
                outmgr->HideOverlay(id);
            }
        #endif

        m_OverlayConfigData.erase(m_OverlayConfigData.begin() + id);

        #ifndef DPLUS_UI
            //If the overlay isn't the last one we set its handle to invalid so it won't get destroyed and can be reused below
            if (id + 1 != m_Overlays.size())
            {
                if (m_Overlays[id].GetTextureSource() == ovrl_texsource_winrt_capture)
                {
                    //Manually stop the capture if there is one since the destructor won't be able to do it
                    DPWinRT_StopCapture(m_Overlays[id].GetHandle());
                }

                m_Overlays[id].SetHandle(vr::k_ulOverlayHandleInvalid);
                m_Overlays.erase(m_Overlays.begin() + id);

                //Fixup IDs for overlays past it
                for (auto& overlay : m_Overlays)
                {
                    if (overlay.GetID() > id)
                    {
                        overlay.SetID(overlay.GetID() - 1);

                        //OpenVR overlay keys can't be renamed, yet we want the ID to match the overlay key.
                        //It's kind of awkward, but we swap around overlay handles here to make it work regardless
                        vr::VROverlayHandle_t ovrl_handle = FindOverlayHandle(overlay.GetID());

                        if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
                        {
                            if (overlay.GetTextureSource() == ovrl_texsource_winrt_capture)
                            {
                                DPWinRT_SwapCaptureTargetOverlays(overlay.GetHandle(), ovrl_handle);
                            }

                            overlay.SetHandle(ovrl_handle);

                            //If this overlay got the handle of the removed overlay, set it to not visible since we hid that handle earlier
                            if (overlay.GetID() == id)
                            {
                                overlay.SetVisible(false);
                            }
                        }
                    }
                }

                //After swapping around overlay handles, the previously highest ID has been abandonned, so get rid of it manually
                vr::VROverlay()->DestroyOverlay(FindOverlayHandle(m_Overlays.size()));

                //After swapping, the states also need to be applied again to the new handles
                if (OutputManager* outmgr = OutputManager::Get())
                {
                    outmgr->ResetOverlays();
                }
            }
            else //It's the last overlay so it can just be straight up erased and everything will be fine
            {
                m_Overlays.erase(m_Overlays.begin() + id);
            }
        #endif

        //Fixup current overlay if needed
        if (m_CurrentOverlayID >= m_OverlayConfigData.size())
        {
            m_CurrentOverlayID--;
        }
    }
}

void OverlayManager::RemoveAllOverlays()
{
    //Remove all overlays except dashboard with minimal overhead and refreshes
    while (m_OverlayConfigData.size() > 1)
    {
        m_OverlayConfigData.erase(m_OverlayConfigData.begin() + m_OverlayConfigData.size() - 1);

        #ifndef DPLUS_UI
            m_Overlays.erase(m_Overlays.begin() + m_Overlays.size() - 1);
        #endif
    }

    m_CurrentOverlayID = 0;

    #ifndef DPLUS_UI
        //Fixup active overlay counts after we just removed everything that might've been considered active
        if (OutputManager* outmgr = OutputManager::Get())
        {
            outmgr->ResetOverlayActiveCount();
        }
    #endif
}
