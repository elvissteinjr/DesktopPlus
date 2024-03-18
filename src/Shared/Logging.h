//Just log rotation/unified init plus some common info logs on top of Loguru

#pragma once

#include "loguru.hpp"

//Version string logged and displayed in the UI
//DPLUS_SHA is set externally for nightly builds to use the commit hash in version string
#ifdef DPLUS_SHA
	static const char* const k_pch_DesktopPlusVersion = "Desktop+ NewUI " DPLUS_SHA;
#else
	static const char* const k_pch_DesktopPlusVersion = "Desktop+ NewUI Preview 14";
#endif

void DPLog_Init(const char* name);
void DPLog_SteamVR_SystemInfo();
void DPLog_DPWinRT_SupportInfo();