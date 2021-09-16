#pragma once

#include <string>
#include <vector>

#include "imgui.h"

enum TRMGRStrID
{
    tstr_SettingsWindowTitle,
    tstr_SettingsJumpTo,
    tstr_SettingsCatInterface,
    tstr_SettingsCatKeyboard,
    tstr_SettingsCatWindowOverlays,
    tstr_SettingsCatVersionInfo,
    tstr_SettingsCatWarnings,
    tstr_SettingsCatStartup,
    tstr_SettingsCatTroubleshooting,
    tstr_SettingsInterfaceLanguage,
    tstr_SettingsInterfaceAdvancedSettings,
    tstr_SettingsInterfaceAdvancedSettingsTip,
    tstr_SettingsInterfaceBlankSpaceDrag,
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
    tstr_OvrlPropsPositionOriginDashboard,
    tstr_OvrlPropsPositionOriginHMD,
    tstr_OvrlPropsPositionOriginControllerR,
    tstr_OvrlPropsPositionOriginControllerL,
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
    tstr_OvrlPropsAdvancedSBS,
    tstr_OvrlPropsAdvancedHSBS,
    tstr_OvrlPropsAdvancedOU,
    tstr_OvrlPropsAdvancedHOU,
    tstr_OvrlPropsAdvanced3DSwap,
    tstr_OvrlPropsAdvancedGazeFade,
    tstr_OvrlPropsAdvancedGazeFadeAuto,
    tstr_OvrlPropsAdvancedGazeFadeDist,
    tstr_OvrlPropsAdvancedGazeFadeSens,
    tstr_OvrlPropsAdvancedInput,
    tstr_OvrlPropsAdvancedInputAutoToggle,
    tstr_OvrlPropsAdvancedInputFloatingUI,
    tstr_OvrlPropsInterfaceActions,
    tstr_OvrlPropsInterfaceActionsEdit,
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
    tstr_DialogOk,
    tstr_DialogCancel,
    tstr_DialogDone,
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
        static std::string GetTranslationNameFromFile(const std::string& filename);

        void LoadTranslationFromFile(const std::string& filename);

        const char* GetCurrentTranslationName() const;
        bool IsCurrentTranslationComplete() const;
        void AddStringsToFontBuilder(ImFontGlyphRangesBuilder& builder) const;

        const char* GetDesktopIDString(int desktop_id);
};