//Just log rotation/unified init plus some common info logs on top of Loguru

#pragma once

#include "loguru.hpp"

//Version string logged and displayed in the UI
//DPLUS_SHA is set externally for nightly builds to use the commit hash in version string
//DPLUS_SHA is also always defined but usually empty... so we detect this with some constexpr trickery instead of messing with the build system even more
constexpr const char* const k_pch_DesktopPlusVersion = (sizeof("" DPLUS_SHA) <= 1) ? "Desktop+ 3.1.1 Beta" : "Desktop+ " DPLUS_SHA;

//Version written to config file
//Only really increased when backward incompatible changes were made (we fall back to default values when things are missing usually)
static const int k_nDesktopPlusConfigVersion = 2;

void DPLog_Init(const char* name);
void DPLog_SteamVR_SystemInfo();
void DPLog_DPWinRT_SupportInfo();