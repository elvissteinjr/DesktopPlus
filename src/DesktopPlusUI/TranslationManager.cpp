#include "TranslationManager.h"

#include "ConfigManager.h"
#include "Ini.h"
#include "Util.h"

#include <clocale>

const char* TranslationManager::s_StringIDNames[tstr_MAX] =
{
    "tstr_SettingsWindowTitle",
    "tstr_SettingsJumpTo",
    "tstr_SettingsCatInterface",
    "tstr_SettingsCatEnvironment",
    "tstr_SettingsCatProfiles",
    "tstr_SettingsCatActions",
    "tstr_SettingsCatKeyboard",
    "tstr_SettingsCatMouse",
    "tstr_SettingsCatLaserPointer",
    "tstr_SettingsCatWindowOverlays",
    "tstr_SettingsCatBrowser", 
    "tstr_SettingsCatPerformance",
    "tstr_SettingsCatVersionInfo",
    "tstr_SettingsCatWarnings",
    "tstr_SettingsCatStartup",
    "tstr_SettingsCatTroubleshooting",
    "tstr_SettingsWarningPrefix",
    "tstr_SettingsWarningCompositorResolution",
    "tstr_SettingsWarningCompositorQuality",
    "tstr_SettingsWarningProcessElevated",
    "tstr_SettingsWarningElevatedMode",
    "tstr_SettingsWarningElevatedProcessFocus",
    "tstr_SettingsWarningBrowserMissing",
    "tstr_SettingsWarningBrowserMismatch",
    "tstr_SettingsWarningUIAccessLost",
    "tstr_SettingsWarningOverlayCreationErrorLimit",
    "tstr_SettingsWarningOverlayCreationErrorOther",
    "tstr_SettingsWarningGraphicsCaptureError",
    "tstr_SettingsWarningMenuDontShowAgain",
    "tstr_SettingsWarningMenuDismiss",
    "tstr_SettingsInterfaceLanguage",
    "tstr_SettingsInterfaceAdvancedSettings",
    "tstr_SettingsInterfaceAdvancedSettingsTip",
    "tstr_SettingsInterfaceBlankSpaceDrag",
    "tstr_SettingsInterfacePersistentUI",
    "tstr_SettingsInterfacePersistentUIManage",
    "tstr_SettingsInterfacePersistentUIHelp",
    "tstr_SettingsInterfacePersistentUIHelp2",
    "tstr_SettingsInterfacePersistentUIWindowsHeader",
    "tstr_SettingsInterfacePersistentUIWindowsSettings",
    "tstr_SettingsInterfacePersistentUIWindowsProperties",
    "tstr_SettingsInterfacePersistentUIWindowsKeyboard",
    "tstr_SettingsInterfacePersistentUIWindowsStateGlobal",
    "tstr_SettingsInterfacePersistentUIWindowsStateDashboardTab",
    "tstr_SettingsInterfacePersistentUIWindowsStateVisible",
    "tstr_SettingsInterfacePersistentUIWindowsStatePinned",
    "tstr_SettingsInterfacePersistentUIWindowsStatePosition",
    "tstr_SettingsInterfacePersistentUIWindowsStatePositionReset",
    "tstr_SettingsInterfacePersistentUIWindowsStateSize",
    "tstr_SettingsInterfacePersistentUIWindowsStateLaunchRestore",
    "tstr_SettingsEnvironmentBackgroundColor",
    "tstr_SettingsEnvironmentBackgroundColorDispModeNever",
    "tstr_SettingsEnvironmentBackgroundColorDispModeDPlusTab",
    "tstr_SettingsEnvironmentBackgroundColorDispModeAlways",
    "tstr_SettingsEnvironmentDimInterface",
    "tstr_SettingsEnvironmentDimInterfaceTip",
    "tstr_SettingsProfilesOverlays",
    "tstr_SettingsProfilesApps",
    "tstr_SettingsProfilesManage",
    "tstr_SettingsProfilesOverlaysHeader",
    "tstr_SettingsProfilesOverlaysNameDefault",
    "tstr_SettingsProfilesOverlaysNameNew",
    "tstr_SettingsProfilesOverlaysNameNewBase",
    "tstr_SettingsProfilesOverlaysProfileLoad",
    "tstr_SettingsProfilesOverlaysProfileAdd",
    "tstr_SettingsProfilesOverlaysProfileSave",
    "tstr_SettingsProfilesOverlaysProfileDelete",
    "tstr_SettingsProfilesOverlaysProfileDeleteConfirm",
    "tstr_SettingsProfilesOverlaysProfileFailedLoad",
    "tstr_SettingsProfilesOverlaysProfileFailedDelete",
    "tstr_SettingsProfilesOverlaysProfileAddSelectHeader",
    "tstr_SettingsProfilesOverlaysProfileAddSelectEmpty",
    "tstr_SettingsProfilesOverlaysProfileAddSelectDo",
    "tstr_SettingsProfilesOverlaysProfileAddSelectAll",
    "tstr_SettingsProfilesOverlaysProfileAddSelectNone",
    "tstr_SettingsProfilesOverlaysProfileSaveSelectHeader",
    "tstr_SettingsProfilesOverlaysProfileSaveSelectName",
    "tstr_SettingsProfilesOverlaysProfileSaveSelectNameErrorBlank",
    "tstr_SettingsProfilesOverlaysProfileSaveSelectNameErrorTaken",
    "tstr_SettingsProfilesOverlaysProfileSaveSelectHeaderList", 
    "tstr_SettingsProfilesOverlaysProfileSaveSelectDo",
    "tstr_SettingsProfilesOverlaysProfileSaveSelectDoFailed",
    "tstr_SettingsKeyboardLayout",
    "tstr_SettingsKeyboardSize",
    "tstr_SettingsKeyboardBehavior",
    "tstr_SettingsKeyboardStickyMod",
    "tstr_SettingsKeyboardKeyRepeat",
    "tstr_SettingsKeyboardAutoShow",
    "tstr_SettingsKeyboardAutoShowDesktopOnly",
    "tstr_SettingsKeyboardAutoShowDesktop",
    "tstr_SettingsKeyboardAutoShowDesktopTip",
    "tstr_SettingsKeyboardAutoShowBrowser",
    "tstr_SettingsKeyboardKeyClusters",
    "tstr_SettingsKeyboardKeyClusterFunction",
    "tstr_SettingsKeyboardKeyClusterNavigation",
    "tstr_SettingsKeyboardKeyClusterNumpad",
    "tstr_SettingsKeyboardKeyClusterExtra",
    "tstr_SettingsMouseShowCursor",
    "tstr_SettingsMouseShowCursorGCUnsupported",
    "tstr_SettingsMouseShowCursorGCActiveWarning",
    "tstr_SettingsMouseScrollSmooth",
    "tstr_SettingsMouseAllowLaserPointerOverride",
    "tstr_SettingsMouseAllowLaserPointerOverrideTip",
    "tstr_SettingsMouseDoubleClickAssist",
    "tstr_SettingsMouseDoubleClickAssistTip",
    "tstr_SettingsMouseDoubleClickAssistTipValueOff",
    "tstr_SettingsMouseDoubleClickAssistTipValueAuto",
    "tstr_SettingsLaserPointerTip",
    "tstr_SettingsLaserPointerBlockInput",
    "tstr_SettingsLaserPointerAutoToggleDistance",
    "tstr_SettingsLaserPointerAutoToggleDistanceValueOff",
    "tstr_SettingsWindowOverlaysAutoFocus",
    "tstr_SettingsWindowOverlaysKeepOnScreen",
    "tstr_SettingsWindowOverlaysKeepOnScreenTip",
    "tstr_SettingsWindowOverlaysAutoSizeOverlay",
    "tstr_SettingsWindowOverlaysFocusSceneApp",
    "tstr_SettingsWindowOverlaysStrictMatching",
    "tstr_SettingsWindowOverlaysStrictMatchingTip",
    "tstr_SettingsWindowOverlaysOnWindowDrag",
    "tstr_SettingsWindowOverlaysOnWindowDragDoNothing",
    "tstr_SettingsWindowOverlaysOnWindowDragBlock",
    "tstr_SettingsWindowOverlaysOnWindowDragOverlay",
    "tstr_SettingsWindowOverlaysOnCaptureLoss",
    "tstr_SettingsWindowOverlaysOnCaptureLossTip",
    "tstr_SettingsWindowOverlaysOnCaptureLossDoNothing",
    "tstr_SettingsWindowOverlaysOnCaptureLossHide",
    "tstr_SettingsWindowOverlaysOnCaptureLossRemove",
    "tstr_SettingsBrowserMaxFrameRate",
    "tstr_SettingsBrowserMaxFrameRateOverrideOff",
    "tstr_SettingsBrowserContentBlocker",
    "tstr_SettingsBrowserContentBlockerTip",
    "tstr_SettingsBrowserContentBlockerListCount",
    "tstr_SettingsBrowserContentBlockerListCountSingular",
    "tstr_SettingsPerformanceUpdateLimiter",
    "tstr_SettingsPerformanceUpdateLimiterMode",
    "tstr_SettingsPerformanceUpdateLimiterModeOff",
    "tstr_SettingsPerformanceUpdateLimiterModeMS",
    "tstr_SettingsPerformanceUpdateLimiterModeFPS",
    "tstr_SettingsPerformanceUpdateLimiterModeOffOverride",
    "tstr_SettingsPerformanceUpdateLimiterModeMSTip",
    "tstr_SettingsPerformanceUpdateLimiterFPSValue",
    "tstr_SettingsPerformanceUpdateLimiterOverride",
    "tstr_SettingsPerformanceUpdateLimiterOverrideTip",
    "tstr_SettingsPerformanceUpdateLimiterModeOverride",
    "tstr_SettingsPerformanceRapidUpdates",
    "tstr_SettingsPerformanceRapidUpdatesTip",
    "tstr_SettingsPerformanceSingleDesktopMirror",
    "tstr_SettingsPerformanceSingleDesktopMirrorTip",
    "tstr_SettingsPerformanceShowFPS",
    "tstr_SettingsWarningsHidden",
    "tstr_SettingsWarningsReset",
    "tstr_SettingsStartupAutoLaunch",
    "tstr_SettingsStartupSteamDisable",
    "tstr_SettingsStartupSteamDisableTip",
    "tstr_SettingsTroubleshootingRestart",
    "tstr_SettingsTroubleshootingRestartSteam",
    "tstr_SettingsTroubleshootingRestartDesktop",
    "tstr_SettingsTroubleshootingElevatedModeEnter",
    "tstr_SettingsTroubleshootingElevatedModeLeave",
    "tstr_SettingsTroubleshootingSettingsReset",
    "tstr_SettingsTroubleshootingSettingsResetConfirmDescription",
    "tstr_SettingsTroubleshootingSettingsResetConfirmButton",
    "tstr_KeyboardWindowTitle",
    "tstr_KeyboardWindowTitleSettings",
    "tstr_KeyboardWindowTitleOverlay",
    "tstr_KeyboardWindowTitleOverlayUnknown",
    "tstr_KeyboardShortcutsCut",
    "tstr_KeyboardShortcutsCopy",
    "tstr_KeyboardShortcutsPaste",
    "tstr_OvrlPropsCatPosition",
    "tstr_OvrlPropsCatAppearance",
    "tstr_OvrlPropsCatCapture",
    "tstr_OvrlPropsCatPerformanceMonitor",
    "tstr_OvrlPropsCatBrowser",
    "tstr_OvrlPropsCatAdvanced",
    "tstr_OvrlPropsCatPerformance",
    "tstr_OvrlPropsCatInterface",
    "tstr_OvrlPropsPositionOrigin",
    "tstr_OvrlPropsPositionOriginRoom",
    "tstr_OvrlPropsPositionOriginHMDXY",
    "tstr_OvrlPropsPositionOriginSeatedSpace",
    "tstr_OvrlPropsPositionOriginDashboard",
    "tstr_OvrlPropsPositionOriginHMD",
    "tstr_OvrlPropsPositionOriginControllerL",
    "tstr_OvrlPropsPositionOriginControllerR",
    "tstr_OvrlPropsPositionOriginTracker1",
    "tstr_OvrlPropsPositionDispMode",
    "tstr_OvrlPropsPositionDispModeAlways",
    "tstr_OvrlPropsPositionDispModeDashboard",
    "tstr_OvrlPropsPositionDispModeScene",
    "tstr_OvrlPropsPositionDispModeDPlus",
    "tstr_OvrlPropsPositionPos",
    "tstr_OvrlPropsPositionPosTip",
    "tstr_OvrlPropsPositionChange",
    "tstr_OvrlPropsPositionReset",
    "tstr_OvrlPropsPositionChangeHeader",
    "tstr_OvrlPropsPositionChangeHelp",
    "tstr_OvrlPropsPositionChangeHelpDesktop",
    "tstr_OvrlPropsPositionChangeManualAdjustment",
    "tstr_OvrlPropsPositionChangeMove",
    "tstr_OvrlPropsPositionChangeRotate",
    "tstr_OvrlPropsPositionChangeForward",
    "tstr_OvrlPropsPositionChangeBackward",
    "tstr_OvrlPropsPositionChangeRollCW",
    "tstr_OvrlPropsPositionChangeRollCCW",
    "tstr_OvrlPropsPositionChangeLookAt",
    "tstr_OvrlPropsPositionChangeDragButton",
    "tstr_OvrlPropsPositionChangeOffset",
    "tstr_OvrlPropsPositionChangeOffsetUpDown",
    "tstr_OvrlPropsPositionChangeOffsetRightLeft",
    "tstr_OvrlPropsPositionChangeOffsetForwardBackward",
    "tstr_OvrlPropsPositionChangeDragSettings",
    "tstr_OvrlPropsPositionChangeDragSettingsForceUpright",
    "tstr_OvrlPropsPositionChangeDragSettingsAutoDocking",
    "tstr_OvrlPropsAppearanceWidth",
    "tstr_OvrlPropsAppearanceCurve",
    "tstr_OvrlPropsAppearanceOpacity",
    "tstr_OvrlPropsAppearanceBrightness",
    "tstr_OvrlPropsAppearanceCrop",
    "tstr_OvrlPropsAppearanceCropValueMax",
    "tstr_OvrlPropsCrop",
    "tstr_OvrlPropsCropManualAdjust",
    "tstr_OvrlPropsCropInvalidTip",
    "tstr_OvrlPropsCropX",
    "tstr_OvrlPropsCropY",
    "tstr_OvrlPropsCropWidth",
    "tstr_OvrlPropsCropHeight",
    "tstr_OvrlPropsCropToWindow",
    "tstr_OvrlPropsCaptureMethod",
    "tstr_OvrlPropsCaptureMethodDup",
    "tstr_OvrlPropsCaptureMethodGC",
    "tstr_OvrlPropsCaptureMethodGCUnsupportedTip",
    "tstr_OvrlPropsCaptureMethodGCUnsupportedPartialTip",
    "tstr_OvrlPropsCaptureSource",
    "tstr_OvrlPropsCaptureGCSource",
    "tstr_OvrlPropsCaptureSourceUnknownWarning",
    "tstr_OvrlPropsPerfMonDesktopModeTip",
    "tstr_OvrlPropsPerfMonGlobalTip",
    "tstr_OvrlPropsPerfMonStyle",
    "tstr_OvrlPropsPerfMonStyleCompact",
    "tstr_OvrlPropsPerfMonStyleLarge",
    "tstr_OvrlPropsPerfMonShowCPU",
    "tstr_OvrlPropsPerfMonShowGPU",
    "tstr_OvrlPropsPerfMonShowGraphs",
    "tstr_OvrlPropsPerfMonShowFrameStats",
    "tstr_OvrlPropsPerfMonShowTime",
    "tstr_OvrlPropsPerfMonShowBattery",
    "tstr_OvrlPropsPerfMonShowTrackerBattery",
    "tstr_OvrlPropsPerfMonShowViveWirelessTemp",
    "tstr_OvrlPropsPerfMonDisableGPUCounter",
    "tstr_OvrlPropsPerfMonDisableGPUCounterTip",
    "tstr_OvrlPropsPerfMonResetValues",
    "tstr_OvrlPropsBrowserNotAvailableTip",
    "tstr_OvrlPropsBrowserCloned",
    "tstr_OvrlPropsBrowserClonedTip",
    "tstr_OvrlPropsBrowserClonedConvert",
    "tstr_OvrlPropsBrowserURL",
    "tstr_OvrlPropsBrowserURLHint",
    "tstr_OvrlPropsBrowserGo",
    "tstr_OvrlPropsBrowserRestore",
    "tstr_OvrlPropsBrowserWidth",
    "tstr_OvrlPropsBrowserHeight",
    "tstr_OvrlPropsBrowserZoom",
    "tstr_OvrlPropsBrowserAllowTransparency",
    "tstr_OvrlPropsBrowserAllowTransparencyTip",
    "tstr_OvrlPropsBrowserRecreateContext",
    "tstr_OvrlPropsBrowserRecreateContextTip",
    "tstr_OvrlPropsAdvanced3D",
    "tstr_OvrlPropsAdvancedHSBS",
    "tstr_OvrlPropsAdvancedSBS",
    "tstr_OvrlPropsAdvancedHOU",
    "tstr_OvrlPropsAdvancedOU",
    "tstr_OvrlPropsAdvanced3DSwap",
    "tstr_OvrlPropsAdvancedGazeFade",
    "tstr_OvrlPropsAdvancedGazeFadeAuto",
    "tstr_OvrlPropsAdvancedGazeFadeDistance",
    "tstr_OvrlPropsAdvancedGazeFadeDistanceValueInf",
    "tstr_OvrlPropsAdvancedGazeFadeSensitivity",
    "tstr_OvrlPropsAdvancedGazeFadeOpacity",
    "tstr_OvrlPropsAdvancedInput",
    "tstr_OvrlPropsAdvancedInputInGame",
    "tstr_OvrlPropsAdvancedInputFloatingUI",
    "tstr_OvrlPropsAdvancedOverlayGroup",
    "tstr_OvrlPropsAdvancedOverlayGroupTip",
    "tstr_OvrlPropsAdvancedOverlayGroupIDNone",
    "tstr_OvrlPropsAdvancedOverlayGroupID1",
    "tstr_OvrlPropsAdvancedOverlayGroupID2",
    "tstr_OvrlPropsAdvancedOverlayGroupID3",
    "tstr_OvrlPropsPerformanceInvisibleUpdate",
    "tstr_OvrlPropsPerformanceInvisibleUpdateTip",
    "tstr_OvrlPropsInterfaceOverlayName",
    "tstr_OvrlPropsInterfaceOverlayNameAuto",
    "tstr_OvrlPropsInterfaceActions",
    "tstr_OvrlPropsInterfaceActionsEdit",
    "tstr_OvrlPropsInterfaceDesktopButtons",
    "tstr_OverlayBarOvrlHide",
    "tstr_OverlayBarOvrlShow",
    "tstr_OverlayBarOvrlClone",
    "tstr_OverlayBarOvrlRemove",
    "tstr_OverlayBarOvrlRemoveConfirm",
    "tstr_OverlayBarOvrlProperties",
    "tstr_OverlayBarOvrlAddWindow",
    "tstr_OverlayBarTooltipOvrlAdd",
    "tstr_OverlayBarTooltipSettings",
    "tstr_OverlayBarTooltipResetHold",
    "tstr_FloatingUIHideOverlayTip",
    "tstr_FloatingUIDragModeEnableTip",
    "tstr_FloatingUIDragModeDisableTip",
    "tstr_FloatingUIWindowAddTip",
    "tstr_FloatingUIActionBarShowTip",
    "tstr_FloatingUIActionBarHideTip",
    "tstr_FloatingUIBrowserGoBackTip",
    "tstr_FloatingUIBrowserGoForwardTip",
    "tstr_FloatingUIBrowserRefreshTip",
    "tstr_FloatingUIBrowserStopTip",
    "tstr_ActionNone",
    "tstr_ActionKeyboardShow",
    "tstr_ActionKeyboardHide",
    "tstr_ActionWindowCrop",
    "tstr_ActionOverlayGroupToggle1",
    "tstr_ActionOverlayGroupToggle2",
    "tstr_ActionOverlayGroupToggle3",
    "tstr_ActionSwitchTask",
    "tstr_ActionButtonOverlayGroupToggle1",
    "tstr_ActionButtonOverlayGroupToggle2",
    "tstr_ActionButtonOverlayGroupToggle3",
    "tstr_DefActionMiddleMouse",
    "tstr_DefActionBackMouse",
    "tstr_DefActionReadMe",
    "tstr_PerformanceMonitorCPU",
    "tstr_PerformanceMonitorGPU",
    "tstr_PerformanceMonitorRAM",
    "tstr_PerformanceMonitorVRAM",
    "tstr_PerformanceMonitorFrameTime",
    "tstr_PerformanceMonitorLoad",
    "tstr_PerformanceMonitorFPS",
    "tstr_PerformanceMonitorFPSAverage",
    "tstr_PerformanceMonitorReprojectionRatio",
    "tstr_PerformanceMonitorDroppedFrames",
    "tstr_PerformanceMonitorBatteryLeft",
    "tstr_PerformanceMonitorBatteryRight",
    "tstr_PerformanceMonitorBatteryHMD",
    "tstr_PerformanceMonitorBatteryTracker",
    "tstr_PerformanceMonitorBatteryDisconnected",
    "tstr_PerformanceMonitorViveWirelessTempNotAvailable",
    "tstr_PerformanceMonitorCompactCPU",
    "tstr_PerformanceMonitorCompactGPU",
    "tstr_PerformanceMonitorCompactFPS",
    "tstr_PerformanceMonitorCompactFPSAverage",
    "tstr_PerformanceMonitorCompactReprojectionRatio",
    "tstr_PerformanceMonitorCompactDroppedFrames",
    "tstr_PerformanceMonitorCompactBattery",
    "tstr_PerformanceMonitorCompactBatteryLeft",
    "tstr_PerformanceMonitorCompactBatteryRight",
    "tstr_PerformanceMonitorCompactBatteryHMD",
    "tstr_PerformanceMonitorCompactBatteryTracker",
    "tstr_PerformanceMonitorCompactBatteryDisconnected",
    "tstr_PerformanceMonitorCompactViveWirelessTempNotAvailable",
    "tstr_PerformanceMonitorEmpty",
    "tstr_AuxUIDragHintDocking",
    "tstr_AuxUIDragHintUndocking",
    "tstr_AuxUIGazeFadeAutoHint",
    "tstr_AuxUIGazeFadeAutoHintSingular",
    "tstr_DialogOk",
    "tstr_DialogCancel",
    "tstr_DialogDone",
    "tstr_DialogColorPickerHeader",
    "tstr_DialogColorPickerCurrent",
    "tstr_DialogColorPickerOriginal",
    "tstr_SourceDesktopAll",
    "tstr_SourceDesktopID",
    "tstr_SourceWinRTNone",
    "tstr_SourceWinRTUnknown",
    "tstr_SourceWinRTClosed",
    "tstr_SourcePerformanceMonitor",
    "tstr_SourceBrowser",
    "tstr_SourceBrowserNoPage",
    "tstr_NotificationIconRestoreVR",
    "tstr_NotificationIconOpenOnDesktop",
    "tstr_NotificationIconQuit"
};

static TranslationManager g_TranslationManager;

TranslationManager::TranslationManager()
{
    //Init strings with ID names in case even fallback English doesn't have them
    for (size_t i = 0; i < tstr_MAX; ++i)
    {
        m_Strings[i] = s_StringIDNames[i];
    }
}

TranslationManager& TranslationManager::Get()
{
    return g_TranslationManager;
}

const char* TranslationManager::GetString(TRMGRStrID str_id)
{
    return Get().m_Strings[str_id].c_str();
}

TRMGRStrID TranslationManager::GetStringID(const char* str)
{
    const auto it_begin = std::begin(s_StringIDNames), it_end = std::end(s_StringIDNames);
    const auto it = std::find_if(it_begin, it_end, [&](const char* str_id_name){ return (strcmp(str_id_name, str) == 0); });

    if (it != it_end)
        return (TRMGRStrID)std::distance(it_begin, it);

    return tstr_NONE;
}

std::string TranslationManager::GetTranslationNameFromFile(const std::string& filename)
{
    std::string name;
    std::string fullpath = ConfigManager::Get().GetApplicationPath() + "lang/" + filename;
    Ini lang_file( WStringConvertFromUTF8(fullpath.c_str()).c_str() );

    //Check if it's probably a translation/language file
    if (lang_file.SectionExists("TranslationInfo"))
    {
        name = lang_file.ReadString("TranslationInfo", "Name");
    }

    return name;
}

void TranslationManager::LoadTranslationFromFile(const std::string& filename)
{
    //When filename empty (called from empty config value), figure out the user's language to default to that
    if (filename.empty())
    {
        wchar_t buffer[16] = {0};
        ::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME, buffer, sizeof(buffer) / sizeof(wchar_t));

        //ISO 639 code ideally matches the file names used, so this works as auto-detection
        std::string lang_filename = StringConvertFromUTF16(buffer) + ".ini";
        ConfigManager::SetValue(configid_str_interface_language_file, lang_filename);
        LoadTranslationFromFile(lang_filename);

        return;
    }

    //Load English first as a fallback
    if (filename != "en.ini")
    {
        LoadTranslationFromFile("en.ini");
    }

    std::string fullpath = ConfigManager::Get().GetApplicationPath() + "lang/" + filename;
    Ini lang_file( WStringConvertFromUTF8(fullpath.c_str()).c_str() );

    //Check if it's probably a translation/language file
    if (lang_file.SectionExists("TranslationInfo"))
    {
        m_CurrentTranslationName = lang_file.ReadString("TranslationInfo", "Name", "Unknown");
        m_IsCurrentTranslationComplete = true;

        //Clear precomputed strings to regenerate them with new translation later
        m_StringsDesktopID.clear();
        m_StringsFPSLimit.clear();

        //Try loading all possible strings
        for (size_t i = 0; i < tstr_MAX; ++i)
        {
            if (lang_file.KeyExists("Strings", s_StringIDNames[i]))
            {
                m_Strings[i] = lang_file.ReadString("Strings", s_StringIDNames[i]);
                StringReplaceAll(m_Strings[i], "\\n", "\n");    //Replace new line placeholder
            }
            else
            {
                m_IsCurrentTranslationComplete = false;
            }
        }

        //Set locale or reset it if it's not in the file
        //It's debatable if we should set the locale based on the language as they're separate things... but I personally like seeing separators and formats matching the typical language pattern.
        //It's always possible to edit the locale key to blank to get the user one here
        char* new_locale = setlocale(LC_ALL, lang_file.ReadString("TranslationInfo", "Locale", "C").c_str());

        if (new_locale == nullptr) //Reset to "C" if it failed
        {
            setlocale(LC_ALL, "C");
        }
    }
}

bool TranslationManager::IsCurrentTranslationComplete() const
{
    return m_IsCurrentTranslationComplete;
}

const char* TranslationManager::GetCurrentTranslationName() const
{
    return m_CurrentTranslationName.c_str();
}

void TranslationManager::AddStringsToFontBuilder(ImFontGlyphRangesBuilder& builder) const
{
    for (const auto& str : m_Strings)
    {
        builder.AddText(str.c_str());
    }
}

const char* TranslationManager::GetDesktopIDString(int desktop_id)
{
    if (desktop_id < 0)
    {
        return GetString(tstr_SourceDesktopAll);
    }

    int desktop_count = ConfigManager::GetValue(configid_int_state_interface_desktop_count);

    desktop_id = clamp(desktop_id, 0, desktop_count - 1);

    //Generate desktop ID strings based on current translation
    while ((int)m_StringsDesktopID.size() < desktop_count)
    {
        std::string str = GetString(tstr_SourceDesktopID);
        StringReplaceAll(str, "%ID%", std::to_string(m_StringsDesktopID.size() + 1));

        m_StringsDesktopID.push_back(str);
    }

    return m_StringsDesktopID[desktop_id].c_str();
}

const char* TranslationManager::GetFPSLimitString(int fps_limit_id)
{
    const int fps_limits[10] = { 1, 2, 5, 10, 15, 20, 25, 30, 40, 50 };

    //Set out of range value to ID 10
    if ( (fps_limit_id < 0) || (fps_limit_id > 9) )
    {
        fps_limit_id = 10;
    }

    //Generate fps limit strings based on current translation if needed
    if (m_StringsFPSLimit.empty())
    {
        for (int limit : fps_limits)
        {
            std::string str = GetString(tstr_SettingsPerformanceUpdateLimiterFPSValue);
            StringReplaceAll(str, "%FPS%", std::to_string(limit));

            m_StringsFPSLimit.push_back(str);
        }

        //Out of range string
        std::string str = GetString(tstr_SettingsPerformanceUpdateLimiterFPSValue);
        StringReplaceAll(str, "%FPS%", "?");

        m_StringsFPSLimit.push_back(str);
    }

    return m_StringsFPSLimit[fps_limit_id].c_str();
}
