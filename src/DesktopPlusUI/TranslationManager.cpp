#include "TranslationManager.h"

#include "ConfigManager.h"
#include "Ini.h"
#include "Util.h"
#include "Logging.h"

#include <clocale>

const char* TranslationManager::s_StringIDNames[tstr_MAX] =
{
    "tstr_SettingsWindowTitle",
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
    "tstr_SettingsWarningAppProfileActive",
    "tstr_SettingsWarningConfigMigrated",
    "tstr_SettingsWarningMenuDontShowAgain",
    "tstr_SettingsWarningMenuDismiss",
    "tstr_SettingsInterfaceLanguage",
    "tstr_SettingsInterfaceLanguageCommunity",
    "tstr_SettingsInterfaceLanguageIncompleteWarning",
    "tstr_SettingsInterfaceAdvancedSettings",
    "tstr_SettingsInterfaceAdvancedSettingsTip",
    "tstr_SettingsInterfaceBlankSpaceDrag",
    "tstr_SettingsInterfacePersistentUI",
    "tstr_SettingsInterfacePersistentUIManage",
    "tstr_SettingsInterfaceDesktopButtons",
    "tstr_SettingsInterfaceDesktopButtonsNone",
    "tstr_SettingsInterfaceDesktopButtonsIndividual",
    "tstr_SettingsInterfaceDesktopButtonsCycle",
    "tstr_SettingsInterfaceDesktopButtonsAddCombined",
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
    "tstr_SettingsProfilesAppsHeader",
    "tstr_SettingsProfilesAppsHeaderNoVRTip",
    "tstr_SettingsProfilesAppsListEmpty",
    "tstr_SettingsProfilesAppsProfileHeaderActive",
    "tstr_SettingsProfilesAppsProfileEnabled",
    "tstr_SettingsProfilesAppsProfileOverlayProfile",
    "tstr_SettingsProfilesAppsProfileActionEnter",
    "tstr_SettingsProfilesAppsProfileActionLeave",
    "tstr_SettingsActionsManage",
    "tstr_SettingsActionsManageButton",
    "tstr_SettingsActionsButtonsOrderDefault",
    "tstr_SettingsActionsButtonsOrderOverlayBar",
    "tstr_SettingsActionsShowBindings",
    "tstr_SettingsActionsActiveShortcuts",
    "tstr_SettingsActionsActiveShortcutsTip",
    "tstr_SettingsActionsActiveShortuctsHome",
    "tstr_SettingsActionsActiveShortuctsBack",
    "tstr_SettingsActionsGlobalShortcuts",
    "tstr_SettingsActionsGlobalShortcutsTip",
    "tstr_SettingsActionsGlobalShortcutsEntry",
    "tstr_SettingsActionsGlobalShortcutsAdd",
    "tstr_SettingsActionsGlobalShortcutsRemove",
    "tstr_SettingsActionsHotkeys",
    "tstr_SettingsActionsHotkeysTip",
    "tstr_SettingsActionsHotkeysAdd",
    "tstr_SettingsActionsHotkeysRemove",
    "tstr_SettingsActionsTableHeaderAction",
    "tstr_SettingsActionsTableHeaderShortcut",
    "tstr_SettingsActionsTableHeaderHotkey",
    "tstr_SettingsActionsManageHeader",
    "tstr_SettingsActionsManageCopyUID",
    "tstr_SettingsActionsManageNew",
    "tstr_SettingsActionsManageEdit",
    "tstr_SettingsActionsManageDuplicate",
    "tstr_SettingsActionsManageDelete",
    "tstr_SettingsActionsManageDeleteConfirm",
    "tstr_SettingsActionsManageDuplicatedName",
    "tstr_SettingsActionsEditHeader",
    "tstr_SettingsActionsEditName",
    "tstr_SettingsActionsEditNameTranslatedTip",
    "tstr_SettingsActionsEditTarget",
    "tstr_SettingsActionsEditTargetDefault",
    "tstr_SettingsActionsEditTargetDefaultTip",
    "tstr_SettingsActionsEditTargetUseTags",
    "tstr_SettingsActionsEditTargetActionTarget",
    "tstr_SettingsActionsEditHeaderAppearance",
    "tstr_SettingsActionsEditIcon",
    "tstr_SettingsActionsEditLabel",
    "tstr_SettingsActionsEditLabelTranslatedTip",
    "tstr_SettingsActionsEditHeaderCommands",
    "tstr_SettingsActionsEditNameNew",
    "tstr_SettingsActionsEditCommandAdd",
    "tstr_SettingsActionsEditCommandDelete",
    "tstr_SettingsActionsEditCommandDeleteConfirm",
    "tstr_SettingsActionsEditCommandType",
    "tstr_SettingsActionsEditCommandTypeNone",
    "tstr_SettingsActionsEditCommandTypeKey",
    "tstr_SettingsActionsEditCommandTypeMousePos",
    "tstr_SettingsActionsEditCommandTypeString",
    "tstr_SettingsActionsEditCommandTypeLaunchApp",
    "tstr_SettingsActionsEditCommandTypeShowKeyboard",
    "tstr_SettingsActionsEditCommandTypeCropActiveWindow",
    "tstr_SettingsActionsEditCommandTypeShowOverlay",
    "tstr_SettingsActionsEditCommandTypeSwitchTask",
    "tstr_SettingsActionsEditCommandTypeLoadOverlayProfile",
    "tstr_SettingsActionsEditCommandTypeUnknown",
    "tstr_SettingsActionsEditCommandVisibilityToggle",
    "tstr_SettingsActionsEditCommandVisibilityShow",
    "tstr_SettingsActionsEditCommandVisibilityHide",
    "tstr_SettingsActionsEditCommandUndo",
    "tstr_SettingsActionsEditCommandKeyCode",
    "tstr_SettingsActionsEditCommandKeyToggle",
    "tstr_SettingsActionsEditCommandMouseX",
    "tstr_SettingsActionsEditCommandMouseY",
    "tstr_SettingsActionsEditCommandMouseUseCurrent",
    "tstr_SettingsActionsEditCommandString",
    "tstr_SettingsActionsEditCommandPath",
    "tstr_SettingsActionsEditCommandPathTip",
    "tstr_SettingsActionsEditCommandArgs",
    "tstr_SettingsActionsEditCommandArgsTip",
    "tstr_SettingsActionsEditCommandVisibility",
    "tstr_SettingsActionsEditCommandSwitchingMethod",
    "tstr_SettingsActionsEditCommandSwitchingMethodSwitcher",
    "tstr_SettingsActionsEditCommandSwitchingMethodFocus",
    "tstr_SettingsActionsEditCommandWindow",
    "tstr_SettingsActionsEditCommandWindowNone",
    "tstr_SettingsActionsEditCommandWindowStrictMatchingTip",
    "tstr_SettingsActionsEditCommandCursorWarp",
    "tstr_SettingsActionsEditCommandProfile",
    "tstr_SettingsActionsEditCommandProfileClear",
    "tstr_SettingsActionsEditCommandDescNone",
    "tstr_SettingsActionsEditCommandDescKey",
    "tstr_SettingsActionsEditCommandDescKeyToggle",
    "tstr_SettingsActionsEditCommandDescMousePos",
    "tstr_SettingsActionsEditCommandDescString",
    "tstr_SettingsActionsEditCommandDescLaunchApp",
    "tstr_SettingsActionsEditCommandDescLaunchAppArgsOpt",
    "tstr_SettingsActionsEditCommandDescKeyboardToggle",
    "tstr_SettingsActionsEditCommandDescKeyboardShow",
    "tstr_SettingsActionsEditCommandDescKeyboardHide",
    "tstr_SettingsActionsEditCommandDescCropWindow",
    "tstr_SettingsActionsEditCommandDescOverlayToggle",
    "tstr_SettingsActionsEditCommandDescOverlayShow",
    "tstr_SettingsActionsEditCommandDescOverlayHide",
    "tstr_SettingsActionsEditCommandDescOverlayTargetDefault",
    "tstr_SettingsActionsEditCommandDescSwitchTask",
    "tstr_SettingsActionsEditCommandDescSwitchTaskWindow",
    "tstr_SettingsActionsEditCommandDescLoadOverlayProfile",
    "tstr_SettingsActionsEditCommandDescLoadOverlayProfileAdd",
    "tstr_SettingsActionsEditCommandDescUnknown",
    "tstr_SettingsActionsOrderHeader",
    "tstr_SettingsActionsOrderButtonLabel",
    "tstr_SettingsActionsOrderButtonLabelSingular",
    "tstr_SettingsActionsOrderNoActions",
    "tstr_SettingsActionsOrderAdd",
    "tstr_SettingsActionsOrderRemove",
    "tstr_SettingsActionsAddSelectorHeader",
    "tstr_SettingsActionsAddSelectorAdd",
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
    "tstr_SettingsKeyboardLayoutAuthor",
    "tstr_SettingsKeyboardKeyClusters",
    "tstr_SettingsKeyboardKeyClusterBase",
    "tstr_SettingsKeyboardKeyClusterFunction",
    "tstr_SettingsKeyboardKeyClusterNavigation",
    "tstr_SettingsKeyboardKeyClusterNumpad",
    "tstr_SettingsKeyboardKeyClusterExtra",
    "tstr_SettingsKeyboardSwitchToEditor",
    "tstr_SettingsMouseShowCursor",
    "tstr_SettingsMouseShowCursorGCUnsupported",
    "tstr_SettingsMouseShowCursorGCActiveWarning",
    "tstr_SettingsMouseScrollSmooth",
    "tstr_SettingsMouseSimulatePen",
    "tstr_SettingsMouseSimulatePenUnsupported",
    "tstr_SettingsMouseAllowLaserPointerOverride",
    "tstr_SettingsMouseAllowLaserPointerOverrideTip",
    "tstr_SettingsMouseDoubleClickAssist",
    "tstr_SettingsMouseDoubleClickAssistTip",
    "tstr_SettingsMouseDoubleClickAssistTipValueOff",
    "tstr_SettingsMouseDoubleClickAssistTipValueAuto",
    "tstr_SettingsMouseSmoothing",
    "tstr_SettingsMouseSmoothingLevelNone",
    "tstr_SettingsMouseSmoothingLevelVeryLow",
    "tstr_SettingsMouseSmoothingLevelLow",
    "tstr_SettingsMouseSmoothingLevelMedium",
    "tstr_SettingsMouseSmoothingLevelHigh",
    "tstr_SettingsMouseSmoothingLevelVeryHigh",
    "tstr_SettingsLaserPointerTip",
    "tstr_SettingsLaserPointerBlockInput",
    "tstr_SettingsLaserPointerAutoToggleDistance",
    "tstr_SettingsLaserPointerAutoToggleDistanceValueOff",
    "tstr_SettingsLaserPointerHMDPointer",
    "tstr_SettingsLaserPointerHMDPointerTableHeaderInputAction",
    "tstr_SettingsLaserPointerHMDPointerTableHeaderBinding",
    "tstr_SettingsLaserPointerHMDPointerTableBindingToggle",
    "tstr_SettingsLaserPointerHMDPointerTableBindingLeft",
    "tstr_SettingsLaserPointerHMDPointerTableBindingRight",
    "tstr_SettingsLaserPointerHMDPointerTableBindingMiddle",
    "tstr_SettingsLaserPointerHMDPointerTableBindingDrag",
    "tstr_SettingsWindowOverlaysAutoFocus",
    "tstr_SettingsWindowOverlaysKeepOnScreen",
    "tstr_SettingsWindowOverlaysKeepOnScreenTip",
    "tstr_SettingsWindowOverlaysAutoSizeOverlay",
    "tstr_SettingsWindowOverlaysFocusSceneApp",
    "tstr_SettingsWindowOverlaysFocusSceneAppDashboard",
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
    "tstr_SettingsPerformanceUseHDR",
    "tstr_SettingsPerformanceUseHDRTip",
    "tstr_SettingsPerformanceShowFPS",
    "tstr_SettingsPerformanceUIAutoThrottle",
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
    "tstr_SettingsTroubleshootingSettingsResetConfirmElementOverlays",
    "tstr_SettingsTroubleshootingSettingsResetConfirmElementLegacyFiles",
    "tstr_SettingsTroubleshootingSettingsResetShowQuickStart",
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
    "tstr_OvrlPropsPositionOriginTheaterScreen",
    "tstr_OvrlPropsPositionOriginConfigHMDXYTurning",
    "tstr_OvrlPropsPositionOriginConfigTheaterScreenEnter",
    "tstr_OvrlPropsPositionOriginConfigTheaterScreenLeave",
    "tstr_OvrlPropsPositionOriginTheaterScreenTip",
    "tstr_OvrlPropsPositionDispMode",
    "tstr_OvrlPropsPositionDispModeAlways",
    "tstr_OvrlPropsPositionDispModeDashboard",
    "tstr_OvrlPropsPositionDispModeScene",
    "tstr_OvrlPropsPositionDispModeDPlus",
    "tstr_OvrlPropsPositionPos",
    "tstr_OvrlPropsPositionPosTip",
    "tstr_OvrlPropsPositionChange",
    "tstr_OvrlPropsPositionReset",
    "tstr_OvrlPropsPositionLock",
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
    "tstr_OvrlPropsPositionChangeDragSettingsForceDistance",
    "tstr_OvrlPropsPositionChangeDragSettingsForceDistanceShape",
    "tstr_OvrlPropsPositionChangeDragSettingsForceDistanceShapeSphere",
    "tstr_OvrlPropsPositionChangeDragSettingsForceDistanceShapeCylinder",
    "tstr_OvrlPropsPositionChangeDragSettingsForceDistanceAutoCurve",
    "tstr_OvrlPropsPositionChangeDragSettingsForceDistanceAutoTilt",
    "tstr_OvrlPropsPositionChangeDragSettingsSnapPosition",
    "tstr_OvrlPropsAppearanceWidth",
    "tstr_OvrlPropsAppearanceCurve",
    "tstr_OvrlPropsAppearanceOpacity",
    "tstr_OvrlPropsAppearanceBrightness",
    "tstr_OvrlPropsAppearanceCrop",
    "tstr_OvrlPropsAppearanceCropValueMax",
    "tstr_OvrlPropsCrop",
    "tstr_OvrlPropsCropHelp",
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
    "tstr_OvrlPropsCaptureGCStrictMatching",
    "tstr_OvrlPropsCaptureGCStrictMatchingTip",
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
    "tstr_OvrlPropsAdvancedOverlayTags",
    "tstr_OvrlPropsAdvancedOverlayTagsTip",
    "tstr_OvrlPropsPerformanceInvisibleUpdate",
    "tstr_OvrlPropsPerformanceInvisibleUpdateTip",
    "tstr_OvrlPropsInterfaceOverlayName",
    "tstr_OvrlPropsInterfaceOverlayNameAuto",
    "tstr_OvrlPropsInterfaceActionOrderCustom",
    "tstr_OvrlPropsInterfaceDesktopButtons",
    "tstr_OvrlPropsInterfaceExtraButtons",
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
    "tstr_FloatingUIHideOverlayHoldTip",
    "tstr_FloatingUIDragModeEnableTip",
    "tstr_FloatingUIDragModeDisableTip",
    "tstr_FloatingUIDragModeHoldLockTip",
    "tstr_FloatingUIDragModeHoldUnlockTip",
    "tstr_FloatingUIWindowAddTip",
    "tstr_FloatingUIActionBarShowTip",
    "tstr_FloatingUIActionBarHideTip",
    "tstr_FloatingUIBrowserGoBackTip",
    "tstr_FloatingUIBrowserGoForwardTip",
    "tstr_FloatingUIBrowserRefreshTip",
    "tstr_FloatingUIBrowserStopTip",
    "tstr_FloatingUIActionBarDesktopPrev",
    "tstr_FloatingUIActionBarDesktopNext",
    "tstr_FloatingUIActionBarEmpty",
    "tstr_ActionNone",
    "tstr_ActionKeyboardShow",
    "tstr_ActionKeyboardHide",
    "tstr_DefActionShowKeyboard",
    "tstr_DefActionActiveWindowCrop",
    "tstr_DefActionActiveWindowCropLabel",
    "tstr_DefActionSwitchTask",
    "tstr_DefActionToggleOverlays",
    "tstr_DefActionToggleOverlaysLabel",
    "tstr_DefActionMiddleMouse",
    "tstr_DefActionMiddleMouseLabel",
    "tstr_DefActionBackMouse",
    "tstr_DefActionBackMouseLabel",
    "tstr_DefActionReadMe",
    "tstr_DefActionReadMeLabel",
    "tstr_DefActionDashboardToggle",
    "tstr_DefActionDashboardToggleLabel",
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
    "tstr_AuxUIDragHintOvrlLocked",
    "tstr_AuxUIDragHintOvrlTheaterScreenBlocked",
    "tstr_AuxUIGazeFadeAutoHint",
    "tstr_AuxUIGazeFadeAutoHintSingular",
    "tstr_AuxUIQuickStartWelcomeHeader",
    "tstr_AuxUIQuickStartWelcomeBody",
    "tstr_AuxUIQuickStartOverlaysHeader",
    "tstr_AuxUIQuickStartOverlaysBody",
    "tstr_AuxUIQuickStartOverlaysBody2",
    "tstr_AuxUIQuickStartOverlayPropertiesHeader",
    "tstr_AuxUIQuickStartOverlayPropertiesBody",
    "tstr_AuxUIQuickStartOverlayPropertiesBody2",
    "tstr_AuxUIQuickStartSettingsHeader",
    "tstr_AuxUIQuickStartSettingsBody",
    "tstr_AuxUIQuickStartProfilesHeader",
    "tstr_AuxUIQuickStartProfilesBody",
    "tstr_AuxUIQuickStartActionsHeader",
    "tstr_AuxUIQuickStartActionsBody",
    "tstr_AuxUIQuickStartActionsBody2",
    "tstr_AuxUIQuickStartOverlayTagsHeader",
    "tstr_AuxUIQuickStartOverlayTagsBody",
    "tstr_AuxUIQuickStartSettingsEndBody",
    "tstr_AuxUIQuickStartFloatingUIHeader",
    "tstr_AuxUIQuickStartFloatingUIBody",
    "tstr_AuxUIQuickStartDesktopModeHeader",
    "tstr_AuxUIQuickStartDesktopModeBody",
    "tstr_AuxUIQuickStartEndHeader",
    "tstr_AuxUIQuickStartEndBody",
    "tstr_AuxUIQuickStartButtonNext",
    "tstr_AuxUIQuickStartButtonPrev",
    "tstr_AuxUIQuickStartButtonClose",
    "tstr_DesktopModeCatTools",
    "tstr_DesktopModeCatOverlays",
    "tstr_DesktopModeToolSettings",
    "tstr_DesktopModeToolActions",
    "tstr_DesktopModeOverlayListAdd",
    "tstr_DesktopModePageAddWindowOverlayTitle",
    "tstr_DesktopModePageAddWindowOverlayHeader",
    "tstr_KeyboardEditorKeyListTitle",
    "tstr_KeyboardEditorKeyListTabContextReplace",
    "tstr_KeyboardEditorKeyListTabContextClear",
    "tstr_KeyboardEditorKeyListRow",
    "tstr_KeyboardEditorKeyListSpacing",
    "tstr_KeyboardEditorKeyListKeyAdd",
    "tstr_KeyboardEditorKeyListKeyDuplicate",
    "tstr_KeyboardEditorKeyListKeyRemove",
    "tstr_KeyboardEditorKeyPropertiesTitle",
    "tstr_KeyboardEditorKeyPropertiesNoSelection",
    "tstr_KeyboardEditorKeyPropertiesType",
    "tstr_KeyboardEditorKeyPropertiesTypeBlank",
    "tstr_KeyboardEditorKeyPropertiesTypeVirtualKey",
    "tstr_KeyboardEditorKeyPropertiesTypeVirtualKeyToggle",
    "tstr_KeyboardEditorKeyPropertiesTypeVirtualKeyIsoEnter",
    "tstr_KeyboardEditorKeyPropertiesTypeString",
    "tstr_KeyboardEditorKeyPropertiesTypeSublayoutToggle",
    "tstr_KeyboardEditorKeyPropertiesTypeAction",
    "tstr_KeyboardEditorKeyPropertiesTypeVirtualKeyIsoEnterTip",
    "tstr_KeyboardEditorKeyPropertiesTypeStringTip",
    "tstr_KeyboardEditorKeyPropertiesSize",
    "tstr_KeyboardEditorKeyPropertiesLabel",
    "tstr_KeyboardEditorKeyPropertiesKeyCode",
    "tstr_KeyboardEditorKeyPropertiesString",
    "tstr_KeyboardEditorKeyPropertiesSublayout",
    "tstr_KeyboardEditorKeyPropertiesAction",
    "tstr_KeyboardEditorKeyPropertiesCluster",
    "tstr_KeyboardEditorKeyPropertiesClusterTip",
    "tstr_KeyboardEditorKeyPropertiesBlockModifiers",
    "tstr_KeyboardEditorKeyPropertiesBlockModifiersTip",
    "tstr_KeyboardEditorKeyPropertiesNoRepeat",
    "tstr_KeyboardEditorKeyPropertiesNoRepeatTip",
    "tstr_KeyboardEditorMetadataTitle",
    "tstr_KeyboardEditorMetadataName",
    "tstr_KeyboardEditorMetadataAuthor",
    "tstr_KeyboardEditorMetadataHasAltGr",
    "tstr_KeyboardEditorMetadataHasAltGrTip",
    "tstr_KeyboardEditorMetadataClusterPreview",
    "tstr_KeyboardEditorMetadataSave",
    "tstr_KeyboardEditorMetadataLoad",
    "tstr_KeyboardEditorMetadataSavePopupTitle",
    "tstr_KeyboardEditorMetadataSavePopupFilename",
    "tstr_KeyboardEditorMetadataSavePopupFilenameBlankTip",
    "tstr_KeyboardEditorMetadataSavePopupConfirm",
    "tstr_KeyboardEditorMetadataSavePopupConfirmError",
    "tstr_KeyboardEditorMetadataLoadPopupTitle",
    "tstr_KeyboardEditorMetadataLoadPopupConfirm",
    "tstr_KeyboardEditorPreviewTitle",
    "tstr_KeyboardEditorSublayoutBase",
    "tstr_KeyboardEditorSublayoutShift",
    "tstr_KeyboardEditorSublayoutAltGr",
    "tstr_KeyboardEditorSublayoutAux",
    "tstr_DialogOk",
    "tstr_DialogCancel",
    "tstr_DialogDone",
    "tstr_DialogUndo",
    "tstr_DialogRedo",
    "tstr_DialogColorPickerHeader",
    "tstr_DialogColorPickerCurrent",
    "tstr_DialogColorPickerOriginal",
    "tstr_DialogProfilePickerHeader",
    "tstr_DialogProfilePickerNone",
    "tstr_DialogActionPickerHeader",
    "tstr_DialogActionPickerEmpty",
    "tstr_DialogIconPickerHeader",
    "tstr_DialogIconPickerHeaderTip",
    "tstr_DialogIconPickerNone",
    "tstr_DialogKeyCodePickerHeader",
    "tstr_DialogKeyCodePickerHeaderHotkey",
    "tstr_DialogKeyCodePickerModifiers",
    "tstr_DialogKeyCodePickerKeyCode",
    "tstr_DialogKeyCodePickerKeyCodeHint",
    "tstr_DialogKeyCodePickerKeyCodeNone",
    "tstr_DialogKeyCodePickerFromInput",
    "tstr_DialogKeyCodePickerFromInputPopup",
    "tstr_DialogKeyCodePickerFromInputPopupNoMouse",
    "tstr_DialogWindowPickerHeader",
    "tstr_DialogInputTagsHint",
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
    "tstr_NotificationIconQuit",
    "tstr_NotificationInitialStartupTitleVR",
    "tstr_NotificationInitialStartupTitleDesktop",
    "tstr_NotificationInitialStartupMessage",
    "tstr_BrowserErrorPageTitle",
    "tstr_BrowserErrorPageHeading",
    "tstr_BrowserErrorPageMessage",
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

std::vector<TranslationManager::ListEntry> TranslationManager::GetTranslationList()
{
    std::vector<TranslationManager::ListEntry> lang_list;

    const std::wstring wpath = WStringConvertFromUTF8( std::string(ConfigManager::Get().GetApplicationPath() + "lang/*.ini").c_str() );
    WIN32_FIND_DATA find_data;
    HANDLE handle_find = ::FindFirstFileW(wpath.c_str(), &find_data);

    if (handle_find != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string filename_utf8 = StringConvertFromUTF16(find_data.cFileName);
            const std::string name = GetTranslationNameFromFile(filename_utf8);

            //If name could be read, add to list
            if (!name.empty())
            {
                lang_list.push_back({filename_utf8, name});
            }
        }
        while (::FindNextFileW(handle_find, &find_data) != 0);

        ::FindClose(handle_find);
    }

    return lang_list;
}

void TranslationManager::LoadTranslationFromFile(const std::string& filename)
{
    //When filename empty (called from empty config value), figure out the user's language to default to that
    if (filename.empty())
    {
        wchar_t buffer[16] = {0};
        ::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME, buffer, sizeof(buffer) / sizeof(wchar_t));
        std::string lang = StringConvertFromUTF16(buffer);
        ::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, buffer, sizeof(buffer) / sizeof(wchar_t));
        std::string ctry = StringConvertFromUTF16(buffer);

        // ISO 639 code ideally matches the file names used, so this works as auto-detection
        // try to detect the language file with the country code first (e.g. en_US.ini)
        std::string lang_filename = lang + "_" + ctry + ".ini";
        if (!FileExists(WStringConvertFromUTF8((ConfigManager::Get().GetApplicationPath() + "lang/" + lang_filename).c_str()).c_str()))
        {
            // if the country code file does not exist, try to load the language file without the country code (e.g. en.ini)
            lang_filename = lang + ".ini";
        }
        
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
        m_CurrentTranslationName     = lang_file.ReadString("TranslationInfo", "Name", "Unknown");
        m_CurrentTranslationAuthor   = lang_file.ReadString("TranslationInfo", "Author");
        m_CurrentTranslationFontName = lang_file.ReadString("TranslationInfo", "PreferredFont");
        m_IsCurrentTranslationComplete = true;

        LOG_IF_F(INFO, (filename != "en.ini"), "Loading translation \"%s\", from \"%s\"...", m_CurrentTranslationName.c_str(), filename.c_str());

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

                VLOG_F(1, "Missing translation string %s", s_StringIDNames[i]);
            }
        }

        LOG_IF_F(WARNING, ((!m_IsCurrentTranslationComplete) && (loguru::current_verbosity_cutoff() < 1)), "Translation has missing strings. Set logging verbosity to 1 or higher for more details");

        //Append fixed IDs to strings that need it
        m_Strings[tstr_OverlayBarOvrlRemove]                         += "###OverlayRemove";
        m_Strings[tstr_OverlayBarOvrlRemoveConfirm]                  += "###OverlayRemove";
        m_Strings[tstr_SettingsProfilesOverlaysProfileDelete]        += "###ProfileDelete";
        m_Strings[tstr_SettingsProfilesOverlaysProfileDeleteConfirm] += "###ProfileDelete";
        m_Strings[tstr_SettingsActionsManageDelete]                  += "###ActionDelete";
        m_Strings[tstr_SettingsActionsManageDeleteConfirm]           += "###ActionDelete";
        m_Strings[tstr_SettingsActionsEditCommandDelete]             += "###CommandDelete";
        m_Strings[tstr_SettingsActionsEditCommandDeleteConfirm]      += "###CommandDelete";

        //Set locale or reset it if it's not in the file
        //It's debatable if we should set the locale based on the language as they're separate things... but I personally like seeing separators and formats matching the typical language pattern.
        //It's always possible to edit the locale key to blank to get the user one here
        char* new_locale = setlocale(LC_ALL, lang_file.ReadString("TranslationInfo", "Locale", "C").c_str());

        if (new_locale == nullptr) //Reset to "C" if it failed
        {
            setlocale(LC_ALL, "C");
        }
    }
    else
    {
        LOG_F(WARNING, "Tried to load translation, but \"%s\" is not a valid translation file", filename.c_str());
    }
}

bool TranslationManager::IsCurrentTranslationComplete() const
{
    return m_IsCurrentTranslationComplete;
}

const std::string& TranslationManager::GetCurrentTranslationName() const
{
    return m_CurrentTranslationName;
}

const std::string& TranslationManager::GetCurrentTranslationAuthor() const
{
    return m_CurrentTranslationAuthor;
}

const std::string& TranslationManager::GetCurrentTranslationFontName() const
{
    return m_CurrentTranslationFontName;
}

void TranslationManager::AddStringsToFontBuilder(ImFontGlyphRangesBuilder& builder) const
{
    builder.AddText(m_CurrentTranslationName.c_str());
    builder.AddText(m_CurrentTranslationAuthor.c_str());

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
