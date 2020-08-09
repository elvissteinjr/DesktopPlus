#include "OverlayManager.h"

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

unsigned int OverlayManager::AddOverlay(const OverlayConfigData& data)
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

    std::stringstream ss;
    ss << "elvissteinjr.DesktopPlus" << id;

    vr::VROverlay()->FindOverlay(ss.str().c_str(), &ret);

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
        std::iter_swap(m_Overlays.begin() + id, m_Overlays.begin() + id2);
    #endif
}

void OverlayManager::RemoveOverlay(unsigned int id)
{
    if (id < m_OverlayConfigData.size())
    {
        m_OverlayConfigData.erase(m_OverlayConfigData.begin() + id);

        #ifndef DPLUS_UI
            m_Overlays.erase(m_Overlays.begin() + id);

            //Fixup IDs for overlays past it
            for (auto& overlay : m_Overlays)
            {
                if (overlay.GetID() > id)
                {
                    overlay.SetID(overlay.GetID() - 1);
                }
            }
        #endif

        //Fixup current overlay
        if (m_CurrentOverlayID >= id)
        {
            m_CurrentOverlayID--;
        }
    }
}
