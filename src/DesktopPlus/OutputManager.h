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
#include "InterprocessMessaging.h"

//
// This class evolved into handling almost everything
// Updates the output texture, sends it to OpenVR, handles OpenVR events, IPC messages...
// Most of the tasks are related, but splitting stuff up might be an idea in the future
//
class OutputManager
{
    public:
        OutputManager(HANDLE PauseDuplicationEvent, HANDLE ResumeDuplicationEvent);
        ~OutputManager();
        DUPL_RETURN InitOutput(HWND Window, _Out_ INT& SingleOutput, _Out_ UINT* OutCount, _Out_ RECT* DeskBounds);
        vr::EVRInitError InitOverlay();
        DUPL_RETURN_UPD Update(_In_ PTR_INFO* PointerInfo, bool NewFrame, bool SkipFrame);
        bool HandleIPCMessage(const MSG& msg); //Returns true if message caused a duplication reset (i.e. desktop switch)
        void CleanRefs();
        HWND GetWindowHandle();
        HANDLE GetSharedHandle();
        IDXGIAdapter* GetDXGIAdapter(); //Don't forget to call Release() on the returned pointer when done with it
        void ResetOverlay();
        bool GetOverlayActive();
        bool GetOverlayInputActive();
        DWORD GetMaxRefreshDelay();
        float GetHMDFrameRate();
        float GetTimeNowToPhotons();

        void ShowMainOverlay();
        void HideMainOverlay();

        void SetMainOverlayOpacity(float opacity);
        float GetMainOverlayOpacity();
        bool GetMainOverlayShouldBeVisible();

        void SetOutputInvalid(); //Handles state when there's no valid output

        void DoAction(ActionID action_id);
        void DoStartAction(ActionID action_id);
        void DoStopAction(ActionID action_id);

        void UpdatePerformanceStates();
        const LARGE_INTEGER& GetUpdateLimiterDelay();

    private:
    // Methods
        DUPL_RETURN ProcessMonoMask(bool IsMono, _Inout_ PTR_INFO* PtrInfo, _Out_ INT* PtrWidth, _Out_ INT* PtrHeight, _Out_ INT* PtrLeft, _Out_ INT* PtrTop, _Outptr_result_bytebuffer_(*PtrHeight * *PtrWidth * BPP) BYTE** InitBuffer, _Out_ D3D11_BOX* Box);
        DUPL_RETURN MakeRTV();
        DUPL_RETURN InitShaders();
        DUPL_RETURN CreateTextures(INT SingleOutput, _Out_ UINT* OutCount, _Out_ RECT* DeskBounds);
        DUPL_RETURN DrawFrameToOverlayTex();
        DUPL_RETURN DrawMouseToOverlayTex(_In_ PTR_INFO* PtrInfo);

        bool HandleOpenVREvents();  //Returns true if quit event happened
        void HandleKeyboardHelperMessage(LPARAM lparam);
        
        void LaunchApplication(const std::string& path_utf8, const std::string& arg_utf8);
        void ResetMouseLastLaserPointerPos();
        void GetValidatedCropValues(int& x, int& y, int& width, int& height);

        void ApplySetting3DMode();
        void ApplySettingTransform();
        void ApplySettingCrop();
        void ApplySettingDragMode();
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
        
        void DetachedTransformReset();
        void DetachedTransformAdjust(unsigned int packed_value);
        void DetachedTransformUpdateHMDFloor();

        void DetachedInteractionAutoToggle();
        void DetachedOverlayGazeFade();

        void UpdateDashboardHMD_Y();
        bool HasDashboardMoved();

    // ClassVars
        InputSimulator m_inputsim;
        VRInput m_vrinput;
        IPCManager m_ipcman;

    // Vars
        ID3D11Device* m_Device;
        IDXGIFactory2* m_Factory;
        ID3D11DeviceContext* m_DeviceContext;
        ID3D11SamplerState* m_Sampler;
        ID3D11BlendState* m_BlendState;
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
        DWORD m_MaxActiveRefreshDelay;
        bool m_OutputInvalid;
        bool m_OutputPendingSkippedFrame;

        vr::VROverlayHandle_t m_OvrlHandleDashboard;
        vr::VROverlayHandle_t m_OvrlHandleMain;
        vr::VROverlayHandle_t m_OvrlHandleIcon;
        ID3D11Texture2D* m_OvrlTex;
        ID3D11RenderTargetView* m_OvrlRTV;
        ID3D11ShaderResourceView* m_OvrlShaderResView;
        bool m_OvrlActive;
        bool m_OvrlDashboardActive;
        bool m_OvrlInputActive;
        bool m_OvrlDetachedInteractive;
        float m_OvrlOpacity;                    //This is the opacity the overlay is currently set at, which may differ from what the config value is

        ID3D11Texture2D* m_MouseTex;
        ID3D11ShaderResourceView* m_MouseShaderRes;

        ULONGLONG m_MouseLastClickTick;
        bool m_MouseIgnoreMoveEvent;
        bool m_MouseLastVisible;
        DXGI_OUTDUPL_POINTER_SHAPE_TYPE m_MouseLastCursorType;
        bool m_MouseCursorNeedsUpdate;
        int m_MouseLastLaserPointerX;
        int m_MouseLastLaserPointerY;
        int m_MouseDefaultHotspotX;
        int m_MouseDefaultHotspotY;
        int m_MouseIgnoreMoveEventMissCount;

        bool m_ComInitDone;

        int m_DragModeDeviceID;                 //-1 if not dragging
        Matrix4 m_DragModeMatrixTargetStart;
        Matrix4 m_DragModeMatrixSourceStart;
        bool  m_DragGestureActive;
        float m_DragGestureScaleDistanceStart;
        float m_DragGestureScaleWidthStart;
        float m_DragGestureScaleDistanceLast;
        Matrix4 m_DragGestureRotateMatLast;

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
