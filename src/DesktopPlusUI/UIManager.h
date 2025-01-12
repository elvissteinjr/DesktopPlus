//Manages the UI state and stuffs
//Since this whole thing is the UI, it's the default place for things
//Basic ImGui and rendering backend implementation is left out of it, though

#pragma once

#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <array>

#include "openvr.h"
#include "Matrices.h"
#include "DPRect.h"

#include "Logging.h"
#include "NotificationIcon.h"
#include "OverlayDragger.h"
#include "FloatingUI.h"
#include "AuxUI.h"
#include "VRKeyboard.h"
#include "WindowOverlayBar.h"
#include "WindowSettings.h"
#include "WindowOverlayProperties.h"
#include "WindowPerformance.h"

//Overlay width definitions
#define OVERLAY_WIDTH_METERS_DASHBOARD_UI 2.75f
#define OVERLAY_WIDTH_METERS_SETTINGS 1.5f
#define OVERLAY_WIDTH_METERS_KEYBOARD 2.75f
#define OVERLAY_WIDTH_METERS_AUXUI_DRAG_HINT 0.12f
#define OVERLAY_WIDTH_METERS_AUXUI_GAZEFADE_AUTO_HINT 0.75f
#define OVERLAY_WIDTH_METERS_AUXUI_WINDOW_SELECT 0.75f
#define OVERLAY_WIDTH_METERS_AUXUI_WINDOW_QUICKSTART 0.75f

enum UITexspaceID
{
    ui_texspace_total,
    ui_texspace_overlay_bar,
    ui_texspace_floating_ui,
    ui_texspace_settings,
    ui_texspace_overlay_properties,
    ui_texspace_keyboard,
    ui_texspace_performance_monitor,
    ui_texspace_aux_ui,
    ui_texspace_MAX
};

class UITextureSpaces
{
    private:
        DPRect m_TexspaceRects[ui_texspace_MAX];

    public:
        static UITextureSpaces& Get();

        void Init(bool desktop_mode, bool keyboard_editor_mode);
        const DPRect& GetRect(UITexspaceID texspace_id) const;
        ImVec4 GetRectAsVec4(UITexspaceID texspace_id) const;
};

static const char* const k_pch_bold_exclamation_mark = "\xE2\x9D\x97";

class UIManager
{
    private:
        FloatingUI m_FloatingUI;
        VRKeyboard m_VRKeyboard;
        AuxUI m_AuxUI;
        WindowOverlayBar m_WindowOverlayBar;
        WindowSettings m_WindowSettings;
        WindowOverlayProperties m_WindowOverlayProperties;
        WindowPerformance m_WindowPerformance;
        WindowDesktopMode m_WindowDesktopMode;

        HWND m_WindowHandle;
        NotificationIcon m_NotificationIcon;
        OverlayDragger m_OverlayDragger;
        Microsoft::WRL::ComPtr<ID3D11Resource> m_SharedTextureRef; //Pointer to render target texture, should only be used for calls to SetSharedOverlayTexture()
        int m_RepeatFrame;

        bool m_DesktopMode;
        bool m_KeyboardEditorMode;
        bool m_OpenVRLoaded;         //Desktop mode can run with or without OpenVR and we want to avoid needlessly starting up SteamVR
        bool m_NoRestartOnExit;      //Prevent auto-restart when closing from desktop mode while dashboard app is running (i.e. when using troubleshooting buttons)

        float m_UIScale;
        ImFont* m_FontCompact;
        ImFont* m_FontLarge;         //Only loaded when the UI scale is set to large

        bool m_LowCompositorRes;     //Set when compositor's resolution is set below 100% during init. The user is warned about this as it affects overlay rendering
        bool m_LowCompositorQuality; //Set when the compositor's quality setting to set to something other than High or Auto
        bool m_HasAnyWarning;        //Set when there's any warning (by calling UpdateWarningState)
        vr::EVROverlayError m_OverlayErrorLast; //Last encountered error when adding an overlay (usually just overlay limit exceeded)
        HRESULT m_WinRTErrorLast;    //Last encountered error when a Graphics Capture thread crashed (ideally never happens)

        bool m_ElevatedTaskSetUp;
        bool m_ComInitDone;

        vr::VROverlayHandle_t m_OvrlHandleOverlayBar;
        vr::VROverlayHandle_t m_OvrlHandleFloatingUI;
        vr::VROverlayHandle_t m_OvrlHandleSettings;
        vr::VROverlayHandle_t m_OvrlHandleOverlayProperties;
        vr::VROverlayHandle_t m_OvrlHandleKeyboard;
        vr::VROverlayHandle_t m_OvrlHandleAuxUI;
        vr::VROverlayHandle_t m_OvrlHandleDPlusDashboard;   //Cached to minimize FindOverlay() calls
        vr::VROverlayHandle_t m_OvrlHandleSystemUI;         //Cached to minimize FindOverlay() calls
        bool m_OvrlVisible;

        float m_OvrlOverlayBarAlpha;
        ULONGLONG m_SystemUIActiveTick;
        ULONGLONG m_OverlayBarFadeInTick;
        bool m_IsSystemUIHoveredFromSwitch;     //Set when the dashboard was hovered when dashboard tab was switched to prevent the UI overlay fading out right away
        bool m_IsDummyOverlayTransformUnstable;

        //Dimensions of the mirror texture, updated from OpenVR when opening up settings or receiving a resolution update message from the overlay application
        int m_OvrlPixelWidth;
        int m_OvrlPixelHeight;

        unsigned int m_TransformSyncValueCount;
        float m_TransformSyncValues[16];        //Stores transform sync values until all are set for a full Matrix4

        std::vector<MSG> m_DelayedICPMessages;  //Stores ICP messages that need to be delayed for processing within an ImGui frame

        void DisplayDashboardAppError(const std::string& str);
        void DisplayInitialSetupNotification();
        void SetOverlayInputEnabled(bool is_enabled);

        UITexspaceID GetTexspaceIDForOverlayHandle(vr::VROverlayHandle_t overlay_handle) const;

        void HandleOverlayProfileLoadMessage(LPARAM lparam);

    public:
        static UIManager* Get();

        UIManager(bool desktop_mode, bool keyboard_editor_mode);
        ~UIManager();

        vr::EVRInitError InitOverlay();
        void HandleIPCMessage(const MSG& msg, bool handle_delayed = false); //Messages that need processing within an ImGui frame are stored in m_DelayedICPMessages when handle_delayed is false
        void HandleDelayedIPCMessages();                                    //Calls HandleIPCMessage() for messages in m_DelayedICPMessages and clears it
        void OnInitDone();                                                  //Finishes up applying things that can only be applied after everything has finished loading
        void OnExit();
        void OnProfileLoaded();

        FloatingUI& GetFloatingUI();
        VRKeyboard& GetVRKeyboard();
        AuxUI& GetAuxUI();
        WindowOverlayBar& GetOverlayBarWindow();
        WindowSettings& GetSettingsWindow();
        WindowOverlayProperties& GetOverlayPropertiesWindow();
        WindowPerformance& GetPerformanceWindow();
        WindowDesktopMode& GetDesktopModeWindow();

        void SetWindowHandle(HWND handle);
        HWND GetWindowHandle() const;
        NotificationIcon& GetNotificationIcon();
        void SetSharedTextureRef(ID3D11Resource* ref);
        ID3D11Resource* GetSharedTextureRef() const;
        OverlayDragger& GetOverlayDragger();

        vr::VROverlayHandle_t GetOverlayHandleOverlayBar()         const;
        vr::VROverlayHandle_t GetOverlayHandleFloatingUI()         const;
        vr::VROverlayHandle_t GetOverlayHandleSettings()           const;
        vr::VROverlayHandle_t GetOverlayHandleOverlayProperties()  const;
        vr::VROverlayHandle_t GetOverlayHandleKeyboard()           const;
        vr::VROverlayHandle_t GetOverlayHandleAuxUI()              const;
        vr::VROverlayHandle_t GetOverlayHandleDPlusDashboard()     const;
        vr::VROverlayHandle_t GetOverlayHandleSystemUI()           const;
        std::array<vr::VROverlayHandle_t, 6> GetUIOverlayHandles() const;
        bool IsDummyOverlayTransformUnstable() const;
        void SendUIIntersectionMaskToDashboardApp(std::vector<vr::VROverlayIntersectionMaskPrimitive_t>& primitives) const;

        //This can be called by functions knowingly making changes which will cause visible layout re-alignment due to ImGui's nature of intermediate UI
        //This will cause 2 extra frames to be calculated but thrown away instantly to be more pleasing to the eye. In rare cases more are needed and can be specified instead
        void RepeatFrame(int extra_frame_count = 2); 
        bool GetRepeatFrame() const;
        void DecreaseRepeatFrameCount();

        bool IsInDesktopMode() const;
        bool IsInKeyboardEditorMode() const;
        bool IsOpenVRLoaded() const;
        void DisableRestartOnExit();

        void Restart(bool desktop_mode);
        void RestartIntoKeyboardEditor();
        void RestartDashboardApp(bool force_steam = false);
        void ElevatedModeEnter();
        void ElevatedModeLeave();

        void UpdateStyle();
        void SetUIScale(float scale);
        float GetUIScale() const;
        void SetFonts(ImFont* font_compact, ImFont* font_large);
        ImFont* GetFontCompact() const;
        ImFont* GetFontLarge() const;               //May return nullptr
        void AddFontBuilderStringIfAnyUnmappedCharacters(const char* str);  //Checks if string has unmapped characters and schedules a texture refresh to load them on the next frame
        void OnDPIChanged(int new_dpi, const RECT& new_window_rect);
        void OnTranslationChanged();                //Calls functions in several classes to reset translation-dependent cached strings. Called when translation is changed
        void OnOverlayNameChanged();                //Calls functions in several classes to reset overlay-name-dependent cached strings. Called when overlay names changed
        void UpdateOverlayDimming();

        bool IsCompositorResolutionLow() const;
        bool IsCompositorRenderQualityLow() const;
        void UpdateCompositorRenderQualityLow();
        bool IsAnyWarningDisplayed() const;
        void UpdateAnyWarningDisplayedState();
        vr::EVROverlayError GetOverlayErrorLast() const;
        HRESULT GetWinRTErrorLast() const;
        void ResetOverlayErrorLast();
        void ResetWinRTErrorLast();
        bool IsElevatedTaskSetUp() const;
        void TryChangingWindowFocus() const;

        bool IsOverlayBarOverlayVisible() const;

        void GetDesktopOverlayPixelSize(int& width, int& height) const;
        void UpdateDesktopOverlayPixelSize();

        void PositionOverlay();
        void UpdateOverlayDrag();
        void StartOverlayDrag(vr::VROverlayHandle_t overlay_handle);
        void FinishOverlayDrag();
        void HighlightOverlay(unsigned int overlay_id);
        float GetOverlayHeight(vr::VROverlayHandle_t overlay_handle) const;
        Matrix4 GetOverlay2DPointTransform(Vector2 point_2d, vr::VROverlayHandle_t overlay_handle) const; //GetTransformForOverlayCoordinates() but works with cropped UVs (UI overlays only)

        //Invalid device index triggers on current primary device
        void TriggerLaserPointerHaptics(vr::VROverlayHandle_t overlay_handle, vr::TrackedDeviceIndex_t device_index = vr::k_unTrackedDeviceIndexInvalid) const;
};