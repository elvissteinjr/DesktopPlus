#ifndef _OUTPUTMANAGER_H_
#define _OUTPUTMANAGER_H_

#include <stdio.h>

#include "CommonTypes.h"
#include "warning.h"

#include "openvr.h"
#include "Matrices.h"
#include "RadialFollowSmoothing.h"

#include "OverlayManager.h"
#include "ConfigManager.h"
#include "InputSimulator.h"
#include "VRInput.h"
#include "BackgroundOverlay.h"
#include "OUtoSBSConverter.h"
#include "InterprocessMessaging.h"
#include "OverlayDragger.h"
#include "LaserPointer.h"

class Overlay;
//
// This class evolved into handling almost everything
// Updates the output texture, sends it to OpenVR, handles OpenVR events, IPC messages...
// Most of the tasks are related, but splitting stuff up might be an idea in the future
//
class OutputManager
{
    public:
        static OutputManager* Get();

        OutputManager(HANDLE PauseDuplicationEvent, HANDLE ResumeDuplicationEvent);
        ~OutputManager();
        void CleanRefs();
        DUPL_RETURN InitOutput(HWND Window, _Out_ INT& SingleOutput, _Out_ UINT* OutCount, _Out_ RECT* DeskBounds);
        std::tuple<vr::EVRInitError, vr::EVROverlayError, bool> InitOverlay();  //Returns error state <InitError, OverlayError, VRInputInitSuccess>
        DUPL_RETURN_UPD Update(_In_ PTR_INFO* PointerInfo, _In_ DPRect& DirtyRegionTotal, bool NewFrame, bool SkipFrame);
        void BusyUpdate();                        //Updates minimal state (i.e. OverlayDragger) during busy waits (i.e. waiting for browser startup) to appear more responsive
        bool HandleIPCMessage(const MSG& msg);    //Returns true if message caused a duplication reset (i.e. desktop switch)
        void HandleWinRTMessage(const MSG& msg);  //Messages sent by the Desktop+ WinRT library
        void HandleHotkeyMessage(const MSG& msg);
        void OnExit();

        HWND GetWindowHandle();
        HANDLE GetSharedHandle();
        IDXGIAdapter* GetDXGIAdapter(); //Don't forget to call Release() on the returned pointer when done with it

        void ResetOverlays();
        void ResetCurrentOverlay();

        ID3D11Texture2D* GetOverlayTexture() const; //This returns m_OvrlTex, the backing texture used by the desktop texture overlay (and all overlays stealing its texture)
        ID3D11Texture2D* GetMultiGPUTargetTexture() const;
        vr::VROverlayHandle_t GetDesktopTextureOverlay() const;
        bool GetOverlayActive() const;
        bool GetOverlayInputActive() const;
        DWORD GetMaxRefreshDelay() const;
        float GetHMDFrameRate() const;
        int GetDesktopWidth() const;
        int GetDesktopHeight() const;
        const std::vector<DPRect>& GetDesktopRects() const;
        float GetDesktopHDRWhiteLevelAdjustment(int desktop_id, bool is_for_graphics_capture, bool wmr_ignore_vscreens) const;

        void ShowOverlay(unsigned int id);
        void ShowTheaterOverlay(unsigned int id);
        void HideOverlay(unsigned int id);
        void ResetOverlayActiveCount();     //Called by OverlayManager after removing all overlays, makes sure the active counts are correct

        bool HasDashboardBeenActivatedOnce() const;
        bool IsDashboardTabActive() const;
        float GetDashboardScale() const;
        float GetOverlayHeight(unsigned int overlay_id) const;
        Matrix4 GetFallbackOverlayTransform() const;    //Returns a matrix resulting in the overlay facing the HMD at 2m distance, used when other referenced aren't available

        void SetOutputErrorTexture(vr::VROverlayHandle_t overlay_handle);
        void SetOutputInvalid(); //Handles state when there's no valid output
        bool IsOutputInvalid() const;

        void SetOverlayEnabled(unsigned int overlay_id, bool is_enabled);
        void CropToActiveWindowToggle(unsigned int overlay_id);
        void ShowWindowSwitcher();
        void SwitchToWindow(HWND window, bool warp_cursor);
        void OverlayDirectDragStart(unsigned int overlay_id);
        void OverlayDirectDragFinish(unsigned int overlay_id);

        VRInput& GetVRInput();
        InputSimulator& GetInputSimulator();

        void UpdatePerformanceStates();
        const LARGE_INTEGER& GetUpdateLimiterDelay();
        //This updates the cached desktop rects and count and optionally chooses the adapters/desktop for desktop duplication (previously part of InitOutput())
        int EnumerateOutputs(int target_desktop_id = -1, Microsoft::WRL::ComPtr<IDXGIAdapter>* out_adapter_preferred = nullptr, Microsoft::WRL::ComPtr<IDXGIAdapter>* out_adapter_vr = nullptr);
        void CropToDisplay(int display_id, int& crop_x, int& crop_y, int& crop_width, int& crop_height);
        bool CropToActiveWindow(int& crop_x, int& crop_y, int& crop_width, int& crop_height);             //Returns true if values have changed
        void InitComIfNeeded();

        void ConvertOUtoSBS(Overlay& overlay, OUtoSBSConverter& converter);

    private:
    // Methods
        DUPL_RETURN ProcessMonoMask(bool is_mono, PTR_INFO& ptr_info, int& ptr_width, int& ptr_height, int& ptr_left, int& ptr_top, 
                                    Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_tex, DXGI_FORMAT& out_tex_format, D3D11_BOX& box);
        DUPL_RETURN ProcessMonoMaskFloat16(bool is_mono, PTR_INFO& ptr_info, int& ptr_width, int& ptr_height, int& ptr_left, int& ptr_top, 
                                           Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_tex, DXGI_FORMAT& out_tex_format, D3D11_BOX& box);
        DUPL_RETURN MakeRTV();
        DUPL_RETURN InitShaders();
        DUPL_RETURN CreateTextures(INT SingleOutput, _Out_ UINT* OutCount, _Out_ RECT* DeskBounds);
        void DrawFrameToOverlayTex(bool clear_rtv = true);
        DUPL_RETURN DrawMouseToOverlayTex(_In_ PTR_INFO* PtrInfo);
        DUPL_RETURN_UPD RefreshOpenVROverlayTexture(DPRect& DirtyRectTotal, bool force_full_copy = false); //Refreshes the overlay texture of the VR runtime with content of the m_OvrlTex backing texture
        bool DesktopTextureAlphaCheck();

        bool HandleOpenVREvents();  //Returns true if quit event happened
        void OnOpenVRMouseEvent(const vr::VREvent_t& vr_event, unsigned int& current_overlay_old);
        void HandleKeyboardMessage(IPCActionID ipc_action_id, LPARAM lparam);
        bool HandleOverlayProfileLoadMessage(LPARAM lparam);

        void ResetMouseLastLaserPointerPos();
        void CropToActiveWindow();
        void CropToDisplay(int display_id, bool do_not_apply_setting = false);
        void DuplicateOverlay(unsigned int base_id, bool is_ui_overlay = false);
        unsigned int AddOverlay(OverlayCaptureSource capture_source, int desktop_id = -2, HWND window_handle = nullptr);
        unsigned int AddOverlayDrag(float source_distance, OverlayCaptureSource capture_source, int desktop_id = -2, HWND window_handle = nullptr, float overlay_width = 0.5f);

        void ApplySettingCaptureSource();
        void ApplySetting3DMode();
        void ApplySettingTransform();
        void ApplySettingCrop();
        void ApplySettingInputMode();
        void ApplySettingMouseInput();
        void ApplySettingMouseScale();
        void ApplySettingUpdateLimiter();
        void ApplySettingExtraBrightness();

        void DetachedTransformSync(unsigned int overlay_id);
        void DetachedTransformSyncAll();
        void DetachedTransformReset(unsigned int overlay_id_ref = k_ulOverlayID_None);
        void DetachedTransformAdjust(unsigned int packed_value);
        void DetachedTransformConvertOrigin(unsigned int overlay_id, OverlayOrigin origin_from, OverlayOrigin origin_to);
        void DetachedTransformConvertOrigin(unsigned int overlay_id, OverlayOrigin origin_from, OverlayOrigin origin_to, 
                                            const OverlayOriginConfig& origin_config_from, const OverlayOriginConfig& origin_config_to);
        void DetachedTransformUpdateHMDFloor();
        void DetachedTransformUpdateSeatedPosition();

        void DetachedInteractionAutoToggleAll();
        void DetachedOverlayGazeFade();
        void DetachedOverlayGazeFadeAutoConfigure();
        void DetachedOverlayAutoDockingAll();

        void DetachedTempDragStart(unsigned int overlay_id, float offset_forward = 0.5f);
        void DetachedTempDragFinish();  //Calls OnDragFinish()
        void OnDragFinish();            //Called before calling m_OverlayDragger.DragFinish() to handle OutputManager post drag state adjustments (like auto docking)

        void OnSetOverlayWinRTCaptureWindow(unsigned int overlay_id); //Called when configid_intptr_overlay_state_winrt_hwnd changed
        void FinishQueuedOverlayRemovals();                           //Overlay removals are currently only queued up when requested during HandleWinRTMessage()

        void FixInvalidDashboardLaunchState();
        void UpdateDashboardHMD_Y();
        bool HasDashboardMoved();
        void DimDashboard(bool do_dim);
        void UpdatePendingDashboardDummyHeight();
        bool IsAnyOverlayUsingGazeFade() const;

        void RegisterHotkeys();
        void HandleHotkeys();

        void HandleKeyboardAutoVisibility();

    // ClassVars
        InputSimulator m_InputSim;
        VRInput m_VRInput;
        IPCManager m_IPCMan;
        BackgroundOverlay m_BackgroundOverlay;
        OverlayDragger m_OverlayDragger;
        LaserPointer m_LaserPointer;

    // Vars
        ID3D11Device* m_Device;
        ID3D11DeviceContext* m_DeviceContext;
        ID3D11SamplerState* m_Sampler;
        ID3D11BlendState* m_BlendState;
        ID3D11RasterizerState* m_RasterizerState;
        ID3D11VertexShader* m_VertexShader;
        ID3D11PixelShader* m_PixelShader;
        ID3D11PixelShader* m_PixelShaderCursor;
        ID3D11InputLayout* m_InputLayout;
        ID3D11Texture2D* m_SharedSurf;
        ID3D11Buffer* m_VertexBuffer;
        ID3D11ShaderResourceView* m_ShaderResource;
        IDXGIKeyedMutex* m_KeyMutex;
        HWND m_WindowHandle;
        //These handles are not created or closed by this class, they're valid for the entire runtime though
        HANDLE m_PauseDuplicationEvent;
        HANDLE m_ResumeDuplicationEvent;

        int m_DesktopX;                         //These are the desktop coordinates/size valid for Desktop Duplication and may be different from the m_DesktopRectTotal
        int m_DesktopY;
        int m_DesktopWidth;
        int m_DesktopHeight;
        std::vector<DPRect> m_DesktopRects;     //Cached position and size of available desktops
        DPRect m_DesktopRectTotal;              //Total rect of all available desktops (may not be the same as above Desktop Duplication rect if that's not using the combined desktop)
        std::vector<float> m_DesktopHDRWhiteLevelAdjustments; //Cached GetDesktopHDRWhiteLevelAdjustment() results used during cursor updates
        DWORD m_MaxActiveRefreshDelay;
        bool m_OutputHDRAvailable;              //False if OS doesn't support the required interface, regardless of hardware connected
        bool m_OutputInvalid;
        bool m_OutputPendingSkippedFrame;
        bool m_OutputPendingFullRefresh;
        DPRect m_OutputPendingDirtyRect;
        DPRect m_OutputLastClippingRect;
        int m_OutputAlphaChecksPending;
        bool m_OutputAlphaCheckFailed;          //Output appears to be translucent and needs its alpha channel stripped during texture copy

        vr::VROverlayHandle_t m_OvrlHandleDashboardDummy;
        vr::VROverlayHandle_t m_OvrlHandleIcon;
        vr::VROverlayHandle_t m_OvrlHandleDesktopTexture;
        ID3D11Texture2D* m_OvrlTex;
        ID3D11RenderTargetView* m_OvrlRTV;
        int m_OvrlActiveCount;
        int m_OvrlDesktopDuplActiveCount;
        bool m_OvrlDashboardActive;
        bool m_OvrlInputActive;
        bool m_OvrlDirectDragActive;
        ULONGLONG m_OvrlTempDragStartTick;
        float m_PendingDashboardDummyHeight;
        ULONGLONG m_LastApplyTransformTick;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_MouseTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_MouseShaderRes;

        ULONGLONG m_MouseLastClickTick;
        bool m_MouseIgnoreMoveEvent;
        bool m_MouseCursorNeedsUpdate;
        PTR_INFO m_MouseLastInfo;
        Vector2Int m_MouseLastCursorSize;
        bool m_MouseLaserPointerUsedLastUpdate;
        bool m_MouseLastLaserPointerMoveBlocked;
        int m_MouseLastLaserPointerX;
        int m_MouseLastLaserPointerY;
        int m_MouseIgnoreMoveEventMissCount;
        unsigned int m_MouseLeftDownOverlayID;
        RadialFollowCore m_MouseLaserPointerSmoother;
        LARGE_INTEGER m_MouseLaserPointerScrollDeltaStart;
        LARGE_INTEGER m_MouseLaserPointerScrollDeltaFrequency;

        bool m_IsFirstLaunch;
        bool m_ComInitDone;

        bool m_DashboardActivatedOnce;
        Matrix4 m_DashboardTransformLast;       //This is only used to check if the dashboard has moved from events we can't detect otherwise
        Matrix4 m_SeatedTransformLast;

        //These are only used when duplicating outputs from a different GPU
        ID3D11Device* m_MultiGPUTargetDevice;   //Target D3D11 device, meaning the one the HMD is connected to
        ID3D11DeviceContext* m_MultiGPUTargetDeviceContext;
        ID3D11Texture2D* m_MultiGPUTexStaging;  //Staging texture, owned by m_Device
        ID3D11Texture2D* m_MultiGPUTexTarget;   //Target texture to copy to, owned by m_MultiGPUTargetDevice

        int m_PerformanceFrameCount;
        ULONGLONG m_PerformanceFrameCountStartTick;
        LARGE_INTEGER m_PerformanceUpdateLimiterDelay;

        std::vector<int> m_ProfileAddOverlayIDQueue;
        std::vector<unsigned int> m_RemoveOverlayQueue;

        bool m_IsAnyHotkeyActive;
        int m_RegisteredHotkeyCount;
};

#endif
