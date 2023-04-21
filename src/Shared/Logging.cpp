#include "Logging.h"

#include <string>

#include "openvr.h"
#include "Util.h"

#include "DesktopPlusWinRT.h"

void DPLog_Init(const char* name)
{
    //Make file names from provided log name
    const std::string  filename          = std::string(name) + ".log";
    const std::wstring filename_u16      = WStringConvertFromUTF8(name) + L".log";
    const std::wstring filename_prev_u16 = WStringConvertFromUTF8(name) + L"_prev.log";

    //Check the creation time and rotate if it was created a week or longer ago
    HANDLE file_handle = ::CreateFileW(filename_u16.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

    FILETIME ftime_log_create;
    if (::GetFileTime(file_handle, &ftime_log_create, nullptr, nullptr))
    {
        ::CloseHandle(file_handle);

        FILETIME ftime_current;
        ::GetSystemTimeAsFileTime(&ftime_current);

        ULARGE_INTEGER time_current{ftime_current.dwLowDateTime, ftime_current.dwHighDateTime};
        ULARGE_INTEGER time_create{ftime_log_create.dwLowDateTime, ftime_log_create.dwHighDateTime};
        const ULONGLONG ftime_week = 7ULL * 24 * 60 * 60 * 10000000;

        if (time_create.QuadPart + ftime_week <= time_current.QuadPart)
        {
            ::DeleteFileW(filename_prev_u16.c_str());
            ::MoveFileW(filename_u16.c_str(), filename_prev_u16.c_str());
        }
    }
    else
    {
        ::CloseHandle(file_handle);
    }

    //We always append, but if we just rotated the log we set it to truncate to avoid the blank lines at the top
    const loguru::FileMode log_file_mode = (FileExists(filename_u16.c_str())) ? loguru::Append : loguru::Truncate;

    //Set some Loguru settings
    loguru::g_internal_verbosity = 1;
    loguru::g_colorlogtostderr   = false;
    loguru::g_preamble_uptime    = false;
    loguru::g_preamble_thread    = false;

    //Init Loguru
    loguru::init(__argc, __argv);
    loguru::add_file(filename.c_str(), log_file_mode, loguru::g_stderr_verbosity);

    LOG_F(INFO, "Launching %s", k_pch_DesktopPlusVersion);
}

void DPLog_SteamVR_SystemInfo()
{
    if (vr::VRSystem() == nullptr)
    {
        LOG_F(INFO, "SteamVR is not loaded");
        return;
    }
    
    //Get some info that might be useful for debugging...
    LOG_F(INFO, "SteamVR Runtime Version: %s", vr::VRSystem()->GetRuntimeVersion());
    LOG_F(INFO, "OpenVR API Version: %u.%u.%u", vr::k_nSteamVRVersionMajor, vr::k_nSteamVRVersionMinor, vr::k_nSteamVRVersionBuild);

    {
        char buffer[vr::k_unMaxPropertyStringSize];
        LOG_SCOPE_F(INFO, "HMD Info");

        vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, buffer, vr::k_unMaxPropertyStringSize);
        LOG_F(INFO, "Model: %s", buffer);
        vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, buffer, vr::k_unMaxPropertyStringSize);
        LOG_F(INFO, "Manufacturer: %s", buffer);
        vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, buffer, vr::k_unMaxPropertyStringSize);
        LOG_F(INFO, "Tracking System: %s", buffer);
    }
}

void DPLog_DPWinRT_SupportInfo()
{
    //Loguru doesn't support taking logging from other libraries, but we just make do with this instead for now
    if (DPWinRT_IsCaptureSupported())
    {
        LOG_SCOPE_F(INFO, "Graphics Capture Support");
        LOG_F(INFO, "Combined Desktop: %s", (DPWinRT_IsCaptureFromCombinedDesktopSupported())   ? "Yes" : "No");
        LOG_F(INFO, "Disabling Cursor: %s", (DPWinRT_IsCaptureCursorEnabledPropertySupported()) ? "Yes" : "No");
        LOG_F(INFO, "Disabling Border: %s", (DPWinRT_IsBorderRequiredPropertySupported())       ? "Yes" : "No");
    }
    else
    {
        LOG_F(INFO, "Graphics Capture is not supported");
    }

}
