#pragma once

#include <string>
#include <vector>

#include "imgui.h"

enum TRMGRStrID
{
    tstr_SettingsWindowTitle,
    tstr_SettingsJumpTo,
    tstr_SettingsCatInterface,
    tstr_SettingsCatEnvironment,
    tstr_SettingsCatProfiles,
    tstr_SettingsCatActions,
    tstr_SettingsCatKeyboard,
    tstr_SettingsCatMouse,
    tstr_SettingsCatLaserPointer,
    tstr_SettingsCatWindowOverlays,
    tstr_SettingsCatVersionInfo,
    tstr_SettingsCatWarnings,
    tstr_SettingsCatStartup,
    tstr_SettingsCatTroubleshooting,
    tstr_SettingsWarningPrefix,
    tstr_SettingsWarningCompositorResolution,
    tstr_SettingsWarningCompositorQuality,
    tstr_SettingsWarningProcessElevated,
    tstr_SettingsWarningElevatedMode,
    tstr_SettingsWarningElevatedProcessFocus,
    tstr_SettingsWarningUIAccessLost,
    tstr_SettingsWarningOverlayCreationErrorLimit,
    tstr_SettingsWarningOverlayCreationErrorOther,   //%ERRORNAME% == VROverlay::GetOverlayErrorNameFromEnum()
    tstr_SettingsWarningGraphicsCaptureError,        //%ERRORCODE% == WinRT HRESULT in hex notation
    tstr_SettingsWarningMenuDontShowAgain,
    tstr_SettingsWarningMenuDismiss,
    tstr_SettingsInterfaceLanguage,
    tstr_SettingsInterfaceAdvancedSettings,
    tstr_SettingsInterfaceAdvancedSettingsTip,
    tstr_SettingsInterfaceBlankSpaceDrag,
    tstr_SettingsInterfacePersistentUI,
    tstr_SettingsInterfacePersistentUIManage,
    tstr_SettingsInterfacePersistentUIHelp,
    tstr_SettingsInterfacePersistentUIHelp2,
    tstr_SettingsInterfacePersistentUIWindowsHeader,
    tstr_SettingsInterfacePersistentUIWindowsSettings,
    tstr_SettingsInterfacePersistentUIWindowsProperties,
    tstr_SettingsInterfacePersistentUIWindowsKeyboard,
    tstr_SettingsInterfacePersistentUIWindowsStateGlobal,
    tstr_SettingsInterfacePersistentUIWindowsStateDashboardTab,
    tstr_SettingsInterfacePersistentUIWindowsStateVisible,
    tstr_SettingsInterfacePersistentUIWindowsStatePinned,
    tstr_SettingsInterfacePersistentUIWindowsStatePosition,
    tstr_SettingsInterfacePersistentUIWindowsStatePositionReset,
    tstr_SettingsInterfacePersistentUIWindowsStateSize,
    tstr_SettingsInterfacePersistentUIWindowsStateLaunchRestore,
    tstr_SettingsEnvironmentBackgroundColor,
    tstr_SettingsEnvironmentBackgroundColorDispModeNever,
    tstr_SettingsEnvironmentBackgroundColorDispModeDPlusTab,
    tstr_SettingsEnvironmentBackgroundColorDispModeAlways,
    tstr_SettingsEnvironmentDimInterface,
    tstr_SettingsEnvironmentDimInterfaceTip,
    tstr_SettingsProfilesOverlays,
    tstr_SettingsProfilesApps,
    tstr_SettingsProfilesManage,
    tstr_SettingsProfilesOverlaysHeader,
    tstr_SettingsProfilesOverlaysNameDefault,
    tstr_SettingsProfilesOverlaysNameNew,
    tstr_SettingsProfilesOverlaysNameNewBase,        //%ID% == ID of profile, increased if previous number is already taken
    tstr_SettingsProfilesOverlaysProfileLoad,
    tstr_SettingsProfilesOverlaysProfileAdd,
    tstr_SettingsProfilesOverlaysProfileSave,
    tstr_SettingsProfilesOverlaysProfileDelete,
    tstr_SettingsProfilesOverlaysProfileDeleteConfirm,
    tstr_SettingsProfilesOverlaysProfileFailedLoad,
    tstr_SettingsProfilesOverlaysProfileFailedDelete,
    tstr_SettingsProfilesOverlaysProfileAddSelectHeader,
    tstr_SettingsProfilesOverlaysProfileAddSelectEmpty,
    tstr_SettingsProfilesOverlaysProfileAddSelectDo,
    tstr_SettingsProfilesOverlaysProfileAddSelectAll,
    tstr_SettingsProfilesOverlaysProfileAddSelectNone,
    tstr_SettingsProfilesOverlaysProfileSaveSelectHeader,
    tstr_SettingsProfilesOverlaysProfileSaveSelectName,
    tstr_SettingsProfilesOverlaysProfileSaveSelectNameErrorBlank,
    tstr_SettingsProfilesOverlaysProfileSaveSelectNameErrorTaken,
    tstr_SettingsProfilesOverlaysProfileSaveSelectHeaderList, 
    tstr_SettingsProfilesOverlaysProfileSaveSelectDo,
    tstr_SettingsProfilesOverlaysProfileSaveSelectDoFailed,
    tstr_SettingsKeyboardLayout,
    tstr_SettingsKeyboardSize,
    tstr_SettingsKeyboardBehavior,
    tstr_SettingsKeyboardStickyMod,
    tstr_SettingsKeyboardKeyRepeat,
    tstr_SettingsKeyboardKeyClusters,
    tstr_SettingsKeyboardKeyClusterFunction,
    tstr_SettingsKeyboardKeyClusterNavigation,
    tstr_SettingsKeyboardKeyClusterNumpad,
    tstr_SettingsKeyboardKeyClusterExtra,
    tstr_SettingsMouseShowCursor,
    tstr_SettingsMouseShowCursorGCUnsupported,
    tstr_SettingsMouseShowCursorGCActiveWarning,
    tstr_SettingsMouseScrollSmooth,
    tstr_SettingsMouseAllowLaserPointerOverride,
    tstr_SettingsMouseAllowLaserPointerOverrideTip,
    tstr_SettingsMouseDoubleClickAssist,
    tstr_SettingsMouseDoubleClickAssistTip,
    tstr_SettingsMouseDoubleClickAssistTipValueOff,
    tstr_SettingsMouseDoubleClickAssistTipValueAuto,
    tstr_SettingsLaserPointerTip,
    tstr_SettingsLaserPointerBlockInput,
    tstr_SettingsLaserPointerAutoToggleDistance,
    tstr_SettingsLaserPointerAutoToggleDistanceValueOff,
    tstr_SettingsWindowOverlaysAutoFocus,
    tstr_SettingsWindowOverlaysKeepOnScreen,
    tstr_SettingsWindowOverlaysKeepOnScreenTip,
    tstr_SettingsWindowOverlaysAutoSizeOverlay,
    tstr_SettingsWindowOverlaysFocusSceneApp,
    tstr_SettingsWindowOverlaysStrictMatching,
    tstr_SettingsWindowOverlaysStrictMatchingTip,
    tstr_SettingsWindowOverlaysOnWindowDrag,
    tstr_SettingsWindowOverlaysOnWindowDragDoNothing,
    tstr_SettingsWindowOverlaysOnWindowDragBlock,
    tstr_SettingsWindowOverlaysOnWindowDragOverlay,
    tstr_SettingsWindowOverlaysOnCaptureLoss,
    tstr_SettingsWindowOverlaysOnCaptureLossTip,
    tstr_SettingsWindowOverlaysOnCaptureLossDoNothing,
    tstr_SettingsWindowOverlaysOnCaptureLossHide,
    tstr_SettingsWindowOverlaysOnCaptureLossRemove,
    tstr_SettingsWarningsHidden,
    tstr_SettingsWarningsReset,
    tstr_SettingsStartupAutoLaunch,
    tstr_SettingsStartupSteamDisable,
    tstr_SettingsStartupSteamDisableTip,
    tstr_SettingsTroubleshootingRestart,
    tstr_SettingsTroubleshootingRestartSteam,
    tstr_SettingsTroubleshootingRestartDesktop,
    tstr_SettingsTroubleshootingElevatedModeEnter,
    tstr_SettingsTroubleshootingElevatedModeLeave,
    tstr_SettingsTroubleshootingSettingsReset,
    tstr_SettingsTroubleshootingSettingsResetConfirmDescription,
    tstr_SettingsTroubleshootingSettingsResetConfirmButton,
    tstr_KeyboardWindowTitle,
    tstr_KeyboardWindowTitleSettings,
    tstr_OvrlPropsCatPosition,
    tstr_OvrlPropsCatAppearance,
    tstr_OvrlPropsCatCapture,
    tstr_OvrlPropsCatPerformanceMonitor,
    tstr_OvrlPropsCatAdvanced,
    tstr_OvrlPropsCatInterface,
    tstr_OvrlPropsPositionOrigin,
    tstr_OvrlPropsPositionOriginRoom,
    tstr_OvrlPropsPositionOriginHMDXY,
    tstr_OvrlPropsPositionOriginSeatedSpace,
    tstr_OvrlPropsPositionOriginDashboard,
    tstr_OvrlPropsPositionOriginHMD,
    tstr_OvrlPropsPositionOriginControllerL,
    tstr_OvrlPropsPositionOriginControllerR,
    tstr_OvrlPropsPositionOriginTracker1,
    tstr_OvrlPropsPositionDispMode,
    tstr_OvrlPropsPositionDispModeAlways,
    tstr_OvrlPropsPositionDispModeDashboard,
    tstr_OvrlPropsPositionDispModeScene,
    tstr_OvrlPropsPositionDispModeDPlus,
    tstr_OvrlPropsPositionPos,
    tstr_OvrlPropsPositionPosTip,
    tstr_OvrlPropsPositionChange,
    tstr_OvrlPropsPositionReset,
    tstr_OvrlPropsPositionChangeHeader,
    tstr_OvrlPropsPositionChangeHelp,
    tstr_OvrlPropsPositionChangeHelpDesktop,
    tstr_OvrlPropsPositionChangeManualAdjustment,
    tstr_OvrlPropsPositionChangeMove,
    tstr_OvrlPropsPositionChangeRotate,
    tstr_OvrlPropsPositionChangeForward,
    tstr_OvrlPropsPositionChangeBackward,
    tstr_OvrlPropsPositionChangeRollCW,
    tstr_OvrlPropsPositionChangeRollCCW,
    tstr_OvrlPropsPositionChangeLookAt,
    tstr_OvrlPropsPositionChangeDragButton,
    tstr_OvrlPropsPositionChangeOffset,
    tstr_OvrlPropsPositionChangeOffsetUpDown,
    tstr_OvrlPropsPositionChangeOffsetRightLeft,
    tstr_OvrlPropsPositionChangeOffsetForwardBackward,
    tstr_OvrlPropsPositionChangeDragSettings,
    tstr_OvrlPropsPositionChangeDragSettingsForceUpright,
    tstr_OvrlPropsAppearanceWidth,
    tstr_OvrlPropsAppearanceCurve,
    tstr_OvrlPropsAppearanceOpacity,
    tstr_OvrlPropsAppearanceCrop,
    tstr_OvrlPropsAppearanceCropValueMax,
    tstr_OvrlPropsCrop,
    tstr_OvrlPropsCropManualAdjust,
    tstr_OvrlPropsCropInvalidTip,
    tstr_OvrlPropsCropX,
    tstr_OvrlPropsCropY,
    tstr_OvrlPropsCropWidth,
    tstr_OvrlPropsCropHeight,
    tstr_OvrlPropsCropToWindow,
    tstr_OvrlPropsCaptureMethod,
    tstr_OvrlPropsCaptureMethodDup,
    tstr_OvrlPropsCaptureMethodGC,
    tstr_OvrlPropsCaptureMethodGCUnsupportedTip,
    tstr_OvrlPropsCaptureMethodGCUnsupportedPartialTip,
    tstr_OvrlPropsCaptureSource,
    tstr_OvrlPropsCaptureGCSource,
    tstr_OvrlPropsCaptureFPSLimit,
    tstr_OvrlPropsCaptureFPSUnit,
    tstr_OvrlPropsCaptureInvUpdate,
    tstr_OvrlPropsPerfMonDesktopModeTip,
    tstr_OvrlPropsPerfMonGlobalTip,
    tstr_OvrlPropsPerfMonStyle,
    tstr_OvrlPropsPerfMonStyleCompact,
    tstr_OvrlPropsPerfMonStyleLarge,
    tstr_OvrlPropsPerfMonShowCPU,
    tstr_OvrlPropsPerfMonShowGPU,
    tstr_OvrlPropsPerfMonShowGraphs,
    tstr_OvrlPropsPerfMonShowFrameStats,
    tstr_OvrlPropsPerfMonShowTime,
    tstr_OvrlPropsPerfMonShowBattery,
    tstr_OvrlPropsPerfMonShowTrackerBattery,
    tstr_OvrlPropsPerfMonShowViveWirelessTemp,
    tstr_OvrlPropsPerfMonDisableGPUCounter,
    tstr_OvrlPropsPerfMonDisableGPUCounterTip,
    tstr_OvrlPropsPerfMonResetValues,
    tstr_OvrlPropsAdvanced3D,
    tstr_OvrlPropsAdvancedHSBS,
    tstr_OvrlPropsAdvancedSBS,
    tstr_OvrlPropsAdvancedHOU,
    tstr_OvrlPropsAdvancedOU,
    tstr_OvrlPropsAdvanced3DSwap,
    tstr_OvrlPropsAdvancedGazeFade,
    tstr_OvrlPropsAdvancedGazeFadeAuto,
    tstr_OvrlPropsAdvancedGazeFadeDistance,
    tstr_OvrlPropsAdvancedGazeFadeDistanceValueInf,
    tstr_OvrlPropsAdvancedGazeFadeSensitivity,
    tstr_OvrlPropsAdvancedGazeFadeOpacity,
    tstr_OvrlPropsAdvancedInput,
    tstr_OvrlPropsAdvancedInputInGame,
    tstr_OvrlPropsAdvancedInputFloatingUI,
    tstr_OvrlPropsInterfaceOverlayName,
    tstr_OvrlPropsInterfaceOverlayNameAuto,
    tstr_OvrlPropsInterfaceActions,
    tstr_OvrlPropsInterfaceActionsEdit,
    tstr_OvrlPropsInterfaceDesktopButtons,
    tstr_OverlayBarOvrlHide,
    tstr_OverlayBarOvrlShow,
    tstr_OverlayBarOvrlClone,
    tstr_OverlayBarOvrlRemove,
    tstr_OverlayBarOvrlRemoveConfirm,
    tstr_OverlayBarOvrlProperties,
    tstr_OverlayBarOvrlAddWindow,
    tstr_OverlayBarTooltipOvrlAdd,
    tstr_OverlayBarTooltipSettings,
    tstr_OverlayBarTooltipResetHold,
    tstr_FloatingUIHideOverlayTip,
    tstr_FloatingUIDragModeEnableTip,
    tstr_FloatingUIDragModeDisableTip,
    tstr_FloatingUIWindowAddTip,
    tstr_FloatingUIActionBarShowTip,
    tstr_FloatingUIActionBarHideTip,
    tstr_ActionNone,
    tstr_ActionKeyboardShow,
    tstr_ActionKeyboardHide,
    tstr_ActionWindowCrop,
    tstr_ActionOverlayGroupToggle1,
    tstr_ActionOverlayGroupToggle2,
    tstr_ActionOverlayGroupToggle3,
    tstr_ActionSwitchTask,
    tstr_ActionButtonOverlayGroupToggle1,
    tstr_ActionButtonOverlayGroupToggle2,
    tstr_ActionButtonOverlayGroupToggle3,
    tstr_DefActionMiddleMouse,
    tstr_DefActionBackMouse,
    tstr_DefActionReadMe,
    tstr_PerformanceMonitorCPU,
    tstr_PerformanceMonitorGPU,
    tstr_PerformanceMonitorRAM,
    tstr_PerformanceMonitorVRAM,
    tstr_PerformanceMonitorFrameTime,
    tstr_PerformanceMonitorLoad,
    tstr_PerformanceMonitorFPS,
    tstr_PerformanceMonitorFPSAverage,
    tstr_PerformanceMonitorReprojectionRatio,
    tstr_PerformanceMonitorDroppedFrames,
    tstr_PerformanceMonitorBatteryLeft,
    tstr_PerformanceMonitorBatteryRight,
    tstr_PerformanceMonitorBatteryHMD,
    tstr_PerformanceMonitorBatteryTracker,
    tstr_PerformanceMonitorBatteryDisconnected,
    tstr_PerformanceMonitorViveWirelessTempNotAvailable,
    tstr_PerformanceMonitorCompactCPU,
    tstr_PerformanceMonitorCompactGPU,
    tstr_PerformanceMonitorCompactFPS,
    tstr_PerformanceMonitorCompactFPSAverage,
    tstr_PerformanceMonitorCompactReprojectionRatio,
    tstr_PerformanceMonitorCompactDroppedFrames,
    tstr_PerformanceMonitorCompactBattery,
    tstr_PerformanceMonitorCompactBatteryLeft,
    tstr_PerformanceMonitorCompactBatteryRight,
    tstr_PerformanceMonitorCompactBatteryHMD,
    tstr_PerformanceMonitorCompactBatteryTracker,
    tstr_PerformanceMonitorCompactBatteryDisconnected,
    tstr_PerformanceMonitorCompactViveWirelessTempNotAvailable,
    tstr_PerformanceMonitorEmpty,
    tstr_AuxUIDragHintDocking,
    tstr_AuxUIDragHintUndocking,
    tstr_AuxUIGazeFadeAutoHint,           //%SECONDS% == Countdown seconds
    tstr_AuxUIGazeFadeAutoHintSingular,   //^
    tstr_DialogOk,
    tstr_DialogCancel,
    tstr_DialogDone,
    tstr_DialogColorPickerHeader,
    tstr_DialogColorPickerCurrent,
    tstr_DialogColorPickerOriginal,
    tstr_SourceDesktopAll,
    tstr_SourceDesktopID,   //%ID% == Desktop ID
    tstr_SourceWinRTNone,
    tstr_SourceWinRTUnknown,
    tstr_SourceWinRTClosed,
    tstr_SourcePerformanceMonitor,
    tstr_NotificationIconRestoreVR,
    tstr_NotificationIconOpenOnDesktop,
    tstr_NotificationIconQuit,
    tstr_MAX,
    tstr_NONE = tstr_MAX //Don't pass this into GetString()
};

class TranslationManager
{
    private:
        static const char* s_StringIDNames[tstr_MAX];
        std::string m_Strings[tstr_MAX];
        std::vector<std::string> m_StringsDesktopID;

        std::string m_CurrentTranslationName;
        bool m_IsCurrentTranslationComplete;

    public:
        TranslationManager();
        static TranslationManager& Get();
        static const char* GetString(TRMGRStrID str_id);      //Strings are not sanitized in any way, so probably a bad idea to feed straight into ImGui functions calling printf
        static TRMGRStrID GetStringID(const char* str);       //May return tstr_NONE
        static std::string GetTranslationNameFromFile(const std::string& filename);

        void LoadTranslationFromFile(const std::string& filename);

        const char* GetCurrentTranslationName() const;
        bool IsCurrentTranslationComplete() const;
        void AddStringsToFontBuilder(ImFontGlyphRangesBuilder& builder) const;

        const char* GetDesktopIDString(int desktop_id);
};