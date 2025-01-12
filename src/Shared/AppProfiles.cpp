#include "AppProfiles.h"

#include <sstream>

#ifndef DPLUS_UI
    #include "OutputManager.h"
#endif

#include "ConfigManager.h"
#include "Util.h"
#include "Logging.h"
#include "Ini.h"

std::string AppProfileManager::GetCurrentSceneAppKey() const
{
    if (vr::VRApplications() != nullptr)
    {
        return GetProcessAppKey(vr::VRApplications()->GetCurrentSceneProcessId());
    }

    return "";
}

std::string AppProfileManager::GetProcessAppKey(uint32_t pid) const
{
    char app_key_buffer[vr::k_unMaxApplicationKeyLength] = "";

    if (vr::VRApplications() != nullptr)
    {
        vr::VRApplications()->GetApplicationKeyByProcessId(pid, app_key_buffer, vr::k_unMaxApplicationKeyLength);
    }

    return app_key_buffer;
}

bool AppProfileManager::LoadProfilesFromFile()
{
    m_Profiles.clear();

    std::wstring wpath = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "app_profiles.ini").c_str() );
    bool existed = FileExists(wpath.c_str());

    if (!existed)
        return false;

    Ini pfile(wpath.c_str());

    for (const auto& section_name : pfile.GetSectionList())
    {
        if (section_name.empty())
            continue;

        AppProfile profile;
        profile.IsEnabled =                    pfile.ReadBool(  section_name.c_str(), "Enabled");
        profile.LastApplicationName    =       pfile.ReadString(section_name.c_str(), "LastApplicationName");
        profile.OverlayProfileFileName =       pfile.ReadString(section_name.c_str(), "OverlayProfile");
        profile.ActionUIDEnter = std::strtoull(pfile.ReadString(section_name.c_str(), "ActionEnter", "0").c_str(), nullptr, 10);
        profile.ActionUIDLeave = std::strtoull(pfile.ReadString(section_name.c_str(), "ActionLeave", "0").c_str(), nullptr, 10);

        StoreProfile(section_name, profile);
    }

    return true;
}

void AppProfileManager::SaveProfilesToFile()
{
    std::wstring wpath = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "app_profiles.ini").c_str() );

    //Don't write if no profiles
    if (m_Profiles.empty())
    {
        //Delete application profile file instead of leaving an empty one behind
        if (FileExists(wpath.c_str()))
        {
            ::DeleteFileW(wpath.c_str());
        }

        return;
    }

    Ini pfile(wpath.c_str());

    char app_name_buffer[vr::k_unMaxPropertyStringSize]  = "";

    for (const auto& profile_pair : m_Profiles)
    {
        const std::string& app_key = profile_pair.first;
        const AppProfile& profile  = profile_pair.second;

        pfile.WriteBool(app_key.c_str(), "Enabled", profile.IsEnabled);

        //Write last known application name if SteamVR is running
        bool has_updated_app_name = false;
        if (vr::VRApplications() != nullptr)
        {
            vr::EVRApplicationError app_error = vr::VRApplicationError_None;
            vr::VRApplications()->GetApplicationPropertyString(app_key.c_str(), vr::VRApplicationProperty_Name_String, app_name_buffer, vr::k_unMaxPropertyStringSize, &app_error);

            if (app_error == vr::VRApplicationError_None)
            {
                pfile.WriteString(app_key.c_str(), "LastApplicationName", app_name_buffer);
                has_updated_app_name = true;
            }
        }

        //Write existing info from profile if we couldn't get a fresh one
        if (!has_updated_app_name)
        {
            pfile.WriteString(app_key.c_str(), "LastApplicationName", profile.LastApplicationName.c_str());
        }

        pfile.WriteString(app_key.c_str(), "OverlayProfile", profile.OverlayProfileFileName.c_str());
        pfile.WriteString(app_key.c_str(), "ActionEnter",    std::to_string(profile.ActionUIDEnter).c_str());
        pfile.WriteString(app_key.c_str(), "ActionLeave",    std::to_string(profile.ActionUIDLeave).c_str());
    }

    pfile.Save();
}

const AppProfile& AppProfileManager::GetProfile(const std::string& app_key)
{
    auto it = m_Profiles.find(app_key);

    return (it != m_Profiles.end()) ? it->second : m_NullProfile;
}

bool AppProfileManager::ProfileExists(const std::string& app_key) const
{
    return (m_Profiles.find(app_key) != m_Profiles.end());
}

bool AppProfileManager::StoreProfile(const std::string& app_key, const AppProfile& profile)
{
    AppProfile profile_prev = GetProfile(app_key);
    m_Profiles[app_key] = profile;

    //Adjust active profile state from change if needed
    if (m_AppKeyActiveProfile == app_key)
    {
        if (profile.IsEnabled)
        {
            //App profile is current active one & enabled and the overlay profile changed, re-activate
            //(avoid doing unecessary activations as they'd override any temporary overlay changes)
            if (profile_prev.OverlayProfileFileName != profile.OverlayProfileFileName)
            {
                return ActivateProfile(app_key);
            }
        }
        else //App profile is current active one and has been disabled, activate blank app profile
        {
            return ActivateProfile("");
        }
    }
    else if ( (profile.IsEnabled) && (m_AppKeyActiveProfile.empty()) )
    {
        //Profile is currently not active but app key belongs to the current scene app, activate if newly enabled
        if (app_key == GetCurrentSceneAppKey())
        {
            return ActivateProfile(app_key);
        }
    }

    return false;
}

bool AppProfileManager::RemoveProfile(const std::string& app_key)
{
    LOG_F(INFO, "Removing app profile \"%s\"...", app_key.c_str());

    m_Profiles.erase(app_key);

    //If deleting active profile, activate blank app profile
    if (m_AppKeyActiveProfile == app_key)
    {
        return ActivateProfile("");
    }

    return false;
}

void AppProfileManager::RemoveAllProfiles()
{
    LOG_F(INFO, "Removing all app profiles...");

    m_Profiles.clear();

    //If a profile was active, activate blank app profile
    if (!m_AppKeyActiveProfile.empty())
    {
        ActivateProfile("");
    }
}

bool AppProfileManager::ActivateProfile(const std::string& app_key)
{
    bool loaded_overlay_profile = false;
    const bool is_reloading_profile = (app_key == m_AppKeyActiveProfile);   //Reloading same profile (i.e. from changing profile data), don't trigger enter/leave actions

    //Execute profile leave action if an app profile is already active
    #ifndef DPLUS_UI
        if ( (!is_reloading_profile) && (!m_AppKeyActiveProfile.empty()) )
        {
            const AppProfile& profile_prev = GetProfile(m_AppKeyActiveProfile);

            if ((profile_prev.IsEnabled) && (profile_prev.ActionUIDLeave != k_ActionUID_Invalid))
            {
                if (OutputManager* outmgr = OutputManager::Get())
                {
                    VLOG_F(1, "Executing profile exit action %llu for app profile \"%s\"...", profile_prev.ActionUIDLeave, m_AppKeyActiveProfile.c_str());

                    ConfigManager::Get().GetActionManager().DoAction(profile_prev.ActionUIDLeave);
                }
            }
        }
    #endif

    const AppProfile& profile = GetProfile(app_key);    //This will be m_NullProfile on missing or blank app_key

    LOG_IF_F(INFO, (!is_reloading_profile) && (&profile != &m_NullProfile), "Activating app profile \"%s\"...", app_key.c_str());
    LOG_IF_F(INFO,  (is_reloading_profile) && (&profile != &m_NullProfile), "Reloading app profile \"%s\"...", app_key.c_str());

    //Look up and cache app name
    if (!is_reloading_profile)
    {
        if (profile.IsEnabled)
        {
            m_AppNameActiveProfile = app_key;

            if (vr::VRApplications() != nullptr)
            {
                char app_name_buffer[vr::k_unMaxPropertyStringSize] = "";
                vr::EVRApplicationError app_error = vr::VRApplicationError_None;
                vr::VRApplications()->GetApplicationPropertyString(app_key.c_str(), vr::VRApplicationProperty_Name_String, app_name_buffer, vr::k_unMaxPropertyStringSize, &app_error);

                if (app_error == vr::VRApplicationError_None)
                {
                    m_AppNameActiveProfile = app_name_buffer;
                }
            }
        }
        else
        {
            m_AppNameActiveProfile.clear();
        }
    }

    //Load app profile overlay config
    if ((profile.IsEnabled) && (!profile.OverlayProfileFileName.empty()))
    {
        #ifdef DPLUS_UI
            //If there's no app profile overlay config already active, make sure to save the current normal config first so we can restore it properly later
            if (!m_IsProfileActiveWithOverlayProfile)
            {
                ConfigManager::Get().SaveConfigToFile();
            }
        #endif

        if (ConfigManager::Get().LoadMultiOverlayProfileFromFile(profile.OverlayProfileFileName + ".ini"))
        {
            loaded_overlay_profile = true;
            m_IsProfileActiveWithOverlayProfile = true;
        }
    }

    if ((!loaded_overlay_profile) && (m_IsProfileActiveWithOverlayProfile))  //Restore normal overlay config if previous profile loaded an overlay profile
    {
        ConfigManager::Get().LoadMultiOverlayProfileFromFile("../config.ini");
        loaded_overlay_profile = true;

        m_IsProfileActiveWithOverlayProfile = false;
    }

    m_AppKeyActiveProfile = (profile.IsEnabled) ? app_key : "";

    //Execute profile enter action
    #ifndef DPLUS_UI
        if ((!is_reloading_profile) && (profile.IsEnabled) && (profile.ActionUIDEnter != k_ActionUID_Invalid))
        {
            if (OutputManager* outmgr = OutputManager::Get())
            {
                VLOG_F(1, "Executing profile enter action %llu for app profile \"%s\"...", profile.ActionUIDEnter, m_AppKeyActiveProfile.c_str());
                ConfigManager::Get().GetActionManager().DoAction(profile.ActionUIDEnter);
            }
        }
    #endif

    return loaded_overlay_profile;
}

bool AppProfileManager::ActivateProfileForCurrentSceneApp()
{
    return ActivateProfile(GetCurrentSceneAppKey());
}

bool AppProfileManager::ActivateProfileForProcess(uint32_t pid)
{
    return ActivateProfile(GetProcessAppKey(pid));
}

const std::string& AppProfileManager::GetActiveProfileAppKey()
{
    return m_AppKeyActiveProfile;
}

const std::string& AppProfileManager::GetActiveProfileAppName()
{
    return m_AppNameActiveProfile;
}

bool AppProfileManager::IsActiveProfileWithOverlayProfile() const
{
    return m_IsProfileActiveWithOverlayProfile;
}

std::vector<std::string> AppProfileManager::GetProfileAppKeyList() const
{
    std::vector<std::string> app_keys;

    for (const auto& profile_pair : m_Profiles)
    {
        app_keys.push_back(profile_pair.first);
    }

    return app_keys;
}

std::string AppProfile::Serialize() const
{
    std::stringstream ss(std::ios::out | std::ios::binary);
    size_t str_size = 0;

    ss.write((const char*)&IsEnabled, sizeof(IsEnabled));

    str_size = LastApplicationName.size();
    ss.write((const char*)&str_size,        sizeof(str_size));
    ss.write(LastApplicationName.data(),    str_size);

    str_size = OverlayProfileFileName.size();
    ss.write((const char*)&str_size,        sizeof(str_size));
    ss.write(OverlayProfileFileName.data(), str_size);

    ss.write((const char*)&ActionUIDEnter, sizeof(ActionUIDEnter));
    ss.write((const char*)&ActionUIDLeave, sizeof(ActionUIDLeave));

    return ss.str();
}

void AppProfile::Deserialize(const std::string& str)
{
    std::stringstream ss(str, std::ios::in | std::ios::binary);

    AppProfile new_profile;
    size_t str_length = 0;

    ss.read((char*)&new_profile.IsEnabled, sizeof(IsEnabled));

    ss.read((char*)&str_length, sizeof(str_length));
    str_length = std::min(str_length, (size_t)4096);    //Arbitrary size limit to avoid large allocations on garbage data
    new_profile.LastApplicationName.resize(str_length);
    ss.read(&new_profile.LastApplicationName[0], str_length);

    ss.read((char*)&str_length, sizeof(str_length));
    str_length = std::min(str_length, (size_t)4096);
    new_profile.OverlayProfileFileName.resize(str_length);
    ss.read(&new_profile.OverlayProfileFileName[0], str_length);

    ss.read((char*)&new_profile.ActionUIDEnter, sizeof(ActionUIDEnter));
    ss.read((char*)&new_profile.ActionUIDLeave, sizeof(ActionUIDLeave));

    //Replace all data with the read profile if there were no stream errors
    if (ss.good())
        *this = new_profile;
}

