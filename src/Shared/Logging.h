//Just log rotation/unified init plus some common info logs on top of Loguru

#pragma once

#include "loguru.hpp"

//Version string logged and displayed in the UI
static const char* const k_pch_DesktopPlusVersion = "Desktop+ NewUI Preview 13";

void DPLog_Init(const char* name);
void DPLog_SteamVR_SystemInfo();
void DPLog_DPWinRT_SupportInfo();