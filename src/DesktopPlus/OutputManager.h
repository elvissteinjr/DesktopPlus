#ifndef _OUTPUTMANAGER_H_
#define _OUTPUTMANAGER_H_

#include <stdio.h>

#include "CommonTypes.h"
#include "warning.h"

#include "openvr.h"
#include "Matrices.h"

#include "ConfigManager.h"
#include "InputSimulator.h"
#include "VRInput.h"
#include "OUtoSBSConverter.h"
#include "InterprocessMessaging.h"

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
        vr::EVRInitError InitOverlay();
        DUPL_RETURN_UPD Update(_In_ PTR_INFO* PointerInfo, _In_ DPRect& DirtyRegionTotal, bool NewFrame, bool SkipFrame);
        bool HandleIPCMessage(const MSG& msg);   //Returns true if message caused a duplication reset (i.e. desktop switch)
        void HandleWinRTMessage(const MSG& msg); //Messages sent by the Desktop+ WinRT library

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
        float GetTimeNowToPhotons() const;
        int GetDesktopWidth() const;
        int GetDesktopHeight() const;

        void ShowOverlay(unsigned int id);
        void HideOverlay(unsigned int id);
        void ResetOverlayActiveCount();     //Called by OverlayManager after removing all overlays, makes sure the active counts are correct

        bool HasDashboardBeenActivatedOnce() const;
        bool IsDashboardTabActive() const;

        void SetOutputErrorTexture(vr::VROverlayHandle_t overlay_handle);
        void SetOutputInvalid(); //Handles state when there's no valid output
        bool IsOutputInvalid() const;

        void DoAction(ActionID action_id);
        void DoStartAction(ActionID action_id);
        void DoStopAction(ActionID action_id);

        void UpdatePerformanceStates();
        const LARGE_INTEGER& GetUpdateLimiterDelay();

        void ConvertOUtoSBS(Overlay& overlay, OUtoSBSConverter& converter);

    private:
    // Methods
        DUPL_RETURN ProcessMonoMask(bool IsMono, _Inout_ PTR_INFO* PtrInfo, _Out_ INT* PtrWidth, _Out_ INT* PtrHeight, _Out_ INT* PtrLeft, _Out_ INT* PtrTop, _Outptr_result_bytebuffer_(*PtrHeight * *PtrWidth * BPP) BYTE** InitBuffer, _Out_ D3D11_BOX* Box);
        DUPL_RETURN MakeRTV();
        DUPL_RETURN InitShaders();
        DUPL_RETURN CreateTextures(INT SingleOutput, _Out_ UINT* OutCount, _Out_ RECT* DeskBounds);
        void DrawFrameToOverlayTex(bool clear_rtv = true);
        DUPL_RETURN DrawMouseToOverlayTex(_In_ PTR_INFO* PtrInfo);
        DUPL_RETURN_UPD RefreshOpenVROverlayTexture(DPRect& DirtyRectTotal, bool force_full_copy = false); //Refreshes the overlay texture of the VR runtime with content of the m_OvrlTex backing texture

        bool HandleOpenVREvents();  //Returns true if quit event happened
        void OnOpenVRMouseEvent(const vr::VREvent_t& vr_event, unsigned int& current_overlay_old);
        void HandleKeyboardHelperMessage(LPARAM lparam);
        bool HandleOverlayProfileLoadMessage(LPARAM lparam);

        void LaunchApplication(const std::string& path_utf8, const std::string& arg_utf8);
        void ResetMouseLastLaserPointerPos();
        void CropToActiveWindow();
        void CropToDisplay(int display_id, bool do_not_apply_setting = false);
        void AddOverlay(unsigned int base_id);

        void ApplySettingCaptureSource();
        void ApplySetting3DMode();
        void ApplySettingTransform();
        void ApplySettingCrop();
        void ApplySettingInputMode();
        void ApplySettingMouseInput();
        void ApplySettingKeyboardScale(float last_used_scale);
        void ApplySettingUpdateLimiter();

        void DragStart(bool is_gesture_drag = false);
        void DragUpdate();
        void DragAddDistance(float distance);
        Matrix4 DragGetBaseOffsetMatrix();
        void DragFinish();

        void DragGestureStart();
        void DragGestureUpdate();
        void DragGestureFinish();

        void DetachedTransformSyncAll();
        void DetachedTransformReset(vr::VROverlayHandle_t ovrl_handle_ref = vr::k_ulOverlayHandleInvalid);
        void DetachedTransformAdjust(unsigned int packed_value);
        void DetachedTransformUpdateHMDFloor();

        void DetachedInteractionAutoToggle();
        void DetachedOverlayGazeFade();

        void UpdateDashboardHMD_Y();
        bool HasDashboardMoved();
        bool IsAnyOverlayUsingGazeFade() const;

    // ClassVars
        InputSimulator m_InputSim;
        VRInput m_VRInput;
        IPCManager m_IPCMan;

    // Vars
        ID3D11Device* m_Device;
        IDXGIFactory2* m_Factory;
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

        int m_DesktopX;
        int m_DesktopY;
        int m_DesktopWidth;
        int m_DesktopHeight;
        std::vector<DPRect> m_DesktopRects;     //Cached position and size of available desktops
        DWORD m_MaxActiveRefreshDelay;
        bool m_OutputInvalid;
        bool m_OutputPendingSkippedFrame;
        bool m_OutputPendingFullRefresh;
        DPRect m_OutputPendingDirtyRect;
        DPRect m_OutputLastClippingRect;

        vr::VROverlayHandle_t m_OvrlHandleDashboardDummy;
        vr::VROverlayHandle_t m_OvrlHandleIcon;
        vr::VROverlayHandle_t m_OvrlHandleMain;
        vr::VROverlayHandle_t m_OvrlHandleDesktopTexture;
        ID3D11Texture2D* m_OvrlTex;
        ID3D11RenderTargetView* m_OvrlRTV;
        ID3D11ShaderResourceView* m_OvrlShaderResView;
        int m_OvrlActiveCount;
        int m_OvrlDesktopDuplActiveCount;
        bool m_OvrlDashboardActive;
        bool m_OvrlInputActive;
        bool m_OvrlDetachedInteractiveAll;

        ID3D11Texture2D* m_MouseTex;
        ID3D11ShaderResourceView* m_MouseShaderRes;

        ULONGLONG m_MouseLastClickTick;
        bool m_MouseIgnoreMoveEvent;
        bool m_MouseCursorNeedsUpdate;
        PTR_INFO m_MouseLastInfo;
        Vector2Int m_MouseLastCursorSize;
        bool m_MouseLaserPointerUsedLastUpdate;
        bool m_MouseLastLaserPointerMoveBlocked;
        int m_MouseLastLaserPointerX;
        int m_MouseLastLaserPointerY;
        int m_MouseDefaultHotspotX;
        int m_MouseDefaultHotspotY;
        int m_MouseIgnoreMoveEventMissCount;

        bool m_IsFirstLaunch;
        bool m_ComInitDone;

        int m_DragModeDeviceID;                 //-1 if not dragging
        unsigned int m_DragModeOverlayID;
        Matrix4 m_DragModeMatrixTargetStart;
        Matrix4 m_DragModeMatrixSourceStart;
        bool  m_DragGestureActive;
        float m_DragGestureScaleDistanceStart;
        float m_DragGestureScaleWidthStart;
        float m_DragGestureScaleDistanceLast;
        Matrix4 m_DragGestureRotateMatLast;

        bool m_DashboardActivatedOnce;
        Matrix4 m_DashboardTransformLast;       //This is only used to check if the dashboard has moved from events we can't detect otherwise
        float m_DashboardHMD_Y;                 //The HMDs y-position when the dashboard was activated. Used for dashboard-relative positioning

        //These are only used when duplicating outputs from a different GPU
        ID3D11Device* m_MultiGPUTargetDevice;   //Target D3D11 device, meaning the one the HMD is connected to
        ID3D11DeviceContext* m_MultiGPUTargetDeviceContext;
        ID3D11Texture2D* m_MultiGPUTexStaging;  //Staging texture, owned by m_Device
        ID3D11Texture2D* m_MultiGPUTexTarget;   //Target texture to copy to, owned by m_MultiGPUTargetDevice

        int m_PerformanceFrameCount;
        ULONGLONG m_PerformanceFrameCountStartTick;
        LARGE_INTEGER m_PerformanceUpdateLimiterDelay;
};

#endif
