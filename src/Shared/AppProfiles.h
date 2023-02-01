#pragma once

#include <unordered_map>
#include <vector>
#include "Actions.h"

struct AppProfile
{
    bool IsEnabled = false;
    std::string LastApplicationName;            //Used when SteamVR isn't running or can't find the application from the app key
    std::string OverlayProfileFileName;
    ActionID ActionIDEnter = action_none;
    ActionID ActionIDLeave = action_none;

    std::string Serialize() const;              //Serializes into binary data stored as string (contains NUL bytes), not suitable for storage
    void Deserialize(const std::string& str);   //Deserializes from strings created by above function
};

class AppProfileManager
{
    private:
        std::unordered_map<std::string, AppProfile> m_Profiles;
        AppProfile m_NullProfile;

        std::string m_AppKeyActiveProfile;
        std::string m_AppNameActiveProfile;
        bool m_IsProfileActiveWithOverlayProfile = false;

        std::string GetCurrentSceneAppKey() const;
        std::string GetProcessAppKey(uint32_t pid) const;

    public:
        bool LoadProfilesFromFile();
        void SaveProfilesToFile();

        const AppProfile& GetProfile(const std::string& app_key);
        bool ProfileExists(const std::string& app_key) const;
        bool StoreProfile(const std::string& app_key, const AppProfile& profile);   //Returns true if a new overlay profile was loaded after change of active profile
        bool RemoveProfile(const std::string& app_key);                             //Returns true if a new overlay profile was loaded after removal of active profile

        bool ActivateProfile(const std::string& app_key);                           //Returns true if a new overlay profile was loaded
        bool ActivateProfileForCurrentSceneApp();                                   //^
        bool ActivateProfileForProcess(uint32_t pid);                               //^
        const std::string& GetActiveProfileAppKey();
        const std::string& GetActiveProfileAppName();
        bool IsActiveProfileWithOverlayProfile() const;                             //Returns if the active app profile loaded an overlay profile

        std::vector<std::string> GetProfileAppKeyList() const;
};