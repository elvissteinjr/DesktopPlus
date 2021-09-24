#include "OverlayManager.h"

#ifndef DPLUS_UI
    #include "OutputManager.h"
    #include "DesktopPlusWinRT.h"
#else
    #include "UIManager.h"
#endif

#include "WindowManager.h"
#include "Util.h"

#include <sstream>

static OverlayManager g_OverlayManager;

OverlayManager& OverlayManager::Get()
{
    return g_OverlayManager;
}

#ifndef DPLUS_UI
    OverlayManager::OverlayManager() : m_CurrentOverlayID(0), m_OverlayNull(k_ulOverlayID_None)
#else
    OverlayManager::OverlayManager() : m_CurrentOverlayID(0)
#endif
{

}

unsigned int OverlayManager::DuplicateOverlay(const OverlayConfigData& data)
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
    data.ConfigInt[configid_int_overlay_capture_source]  = capture_source;
    data.ConfigFloat[configid_float_overlay_width]       = 0.3f;
    data.ConfigFloat[configid_float_overlay_curvature]   = 0.0f;

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
        return m_OverlayConfigDataNull; //Return null overlay data when out of range
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
    if ( (id == id2) || (id >= m_OverlayConfigData.size()) || (id2 >= m_OverlayConfigData.size()) )
        return;

    std::iter_swap(m_OverlayConfigData.begin() + id, m_OverlayConfigData.begin() + id2);

    //We don't swap the overlays themselves, instead we reset the overlays with the swapped config data
    //To prevent most flickering from this, we collect all swaps and do the resetting and related actions once the UI app is done with swapping drag
    m_PendingSwaps.push_back({id, id2});
}

void OverlayManager::SwapOverlaysFinish()
{
    #ifndef DPLUS_UI
        for (const auto& id_pair : m_PendingSwaps)
        {
            Overlay& overlay   = GetOverlay(id_pair.first);
            Overlay& overlay_2 = GetOverlay(id_pair.second);

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
        }

        if (OutputManager* outmgr = OutputManager::Get())
        {
            outmgr->ResetOverlays();
        }
    #else
        for (const auto& id_pair : m_PendingSwaps)
        {
            const OverlayConfigData& data   = GetConfigData(id_pair.first);
            const OverlayConfigData& data_2 = GetConfigData(id_pair.second);

            if ((data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui) || (data_2.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui))
            {
                UIManager::Get()->GetPerformanceWindow().ScheduleOverlaySharedTextureUpdate();
            }
        }
    #endif

    m_PendingSwaps.clear();
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

                            //Update cached visibility to new handle's state
                            overlay.SetVisible(vr::VROverlay()->IsOverlayVisible(ovrl_handle));
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
std::vector<unsigned int> OverlayManager::FindInactiveOverlaysForWindow(const WindowInfo& window_info) const
{
    std::vector<unsigned int> matching_overlay_ids;
    std::vector<unsigned int> candidate_overlay_ids;

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
    const std::string class_str = StringConvertFromUTF16(window_info.GetWindowClassName().c_str());

    //Just straight look for a complete match when strict matching is enabled
    if (ConfigManager::Get().GetConfigBool(configid_bool_windows_winrt_window_matching_strict))
    {
        for (auto i : candidate_overlay_ids)
        {
            const OverlayConfigData& data = m_OverlayConfigData[i];

            if ( (data.ConfigStr[configid_str_overlay_winrt_last_window_class_name] == class_str) && (data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name] == window_info.GetExeName()) && 
                 (data.ConfigStr[configid_str_overlay_winrt_last_window_title] == title_str) )
            {
                matching_overlay_ids.push_back(i);
            }
        }

        return matching_overlay_ids;
    }

    std::string title_search = title_str;
    std::string app_name;
    size_t search_pos = title_str.find_last_of(" - ");

    if (search_pos != std::string::npos)
    {
        app_name = title_str.substr(search_pos - 2);
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

            //Check if class/exe name matches and search title can be found in the last stored window title
            if ( (data.ConfigStr[configid_str_overlay_winrt_last_window_class_name] == class_str) && (data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name] == window_info.GetExeName()) &&
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

            if ( (data.ConfigStr[configid_str_overlay_winrt_last_window_class_name] == class_str) && (data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name] == window_info.GetExeName()) )
            {
                matching_overlay_ids.push_back(i);
            }
        }
    }

    return matching_overlay_ids;
}

#endif

void OverlayManager::SetCurrentOverlayNameAuto(HWND window_handle)
{
    SetOverlayNameAuto(m_CurrentOverlayID, window_handle);
}

void OverlayManager::SetOverlayNameAuto(unsigned int id, HWND window_handle)
{
    if (id < m_OverlayConfigData.size())
    {
        OverlayConfigData& data = m_OverlayConfigData[id];

        //Call is just silently ignored when overlay name is set to custom/user set already
        if (data.ConfigBool[configid_bool_overlay_name_custom])
            return;

        data.ConfigNameStr = "";

        //If override window handle passed, try to use that
        if (window_handle != nullptr)
        {
            auto title_length = ::GetWindowTextLengthW(window_handle);
            if (title_length > 0)
            {
                title_length++;

                std::unique_ptr<WCHAR[]> title_buffer = std::unique_ptr<WCHAR[]>{ new WCHAR[title_length] };

                if (::GetWindowTextW(window_handle, title_buffer.get(), title_length) != 0)
                {
                    data.ConfigNameStr += StringConvertFromUTF16(title_buffer.get());

                    return;
                }
            }
        }

        switch (data.ConfigInt[configid_int_overlay_capture_source])
        {
            case ovrl_capsource_desktop_duplication:
            {
                int desktop_id = data.ConfigInt[configid_int_overlay_desktop_id];

                if (desktop_id == -2) //Default value for desktop 0 that has yet to initialize cropping values
                {
                    desktop_id = 0;
                }

                data.ConfigNameStr += (data.ConfigInt[configid_int_overlay_desktop_id] == -1) ? "Combined Desktop" : "Desktop " + std::to_string(desktop_id + 1);
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
                    data.ConfigNameStr += (data.ConfigInt[configid_int_overlay_winrt_desktop_id] == -1) ? "Combined Desktop" : 
                                                                                                          "Desktop " + std::to_string(data.ConfigInt[configid_int_overlay_winrt_desktop_id] + 1);
                }
                else
                {
                    data.ConfigNameStr += "[No Capture Target]";
                }
                break;
            }
            case ovrl_capsource_ui:
            {
                data.ConfigNameStr += "Performance Monitor"; //So far all UI overlays are just that
                break;
            }
        }
    }
}

void OverlayManager::SetOverlayNamesAutoForWindow(HWND window_handle)
{
    for (unsigned int i = 0; i < m_OverlayConfigData.size(); ++i)
    {
        const OverlayConfigData& data = m_OverlayConfigData[i];

        if ( (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && ((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] == window_handle) )
        {
            SetOverlayNameAuto(i, window_handle);
        }
    }
}
