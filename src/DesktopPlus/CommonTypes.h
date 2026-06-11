#ifndef _COMMONTYPES_H_
#define _COMMONTYPES_H_


//If NTDDI_WIN11_GA is available, use it (optional requirement by OutputManager)
//This *could* blow up if one of the few files that include windows.h itself comes first somehow but it's fine for now
#include <sdkddkver.h>
#ifdef NTDDI_WIN11_GA
    #undef NTDDI_VERSION
    #define NTDDI_VERSION NTDDI_WIN11_GA
#else
    #pragma message("Using older Windows SDK! Not all Desktop+ features will be available in this build.")
#endif

#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <sal.h>
#include <new>
#include <warning.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>
#include <vector>

#include "DPRect.h"

#include "PixelShader.h"
#include "PixelShaderCursor.h"
#include "VertexShader.h"

#define DDP_NUMVERTICES 6

extern HRESULT SystemTransitionsExpectedErrors[];
extern HRESULT CreateDuplicationExpectedErrors[];
extern HRESULT FrameInfoExpectedErrors[];
extern HRESULT AcquireFrameExpectedError[];
extern HRESULT EnumOutputsExpectedErrors[];

enum DDPDuplReturn
{
    ddp_dupl_return_success          = 0,
    ddp_dupl_return_error_expected   = 1,
    ddp_dupl_return_error_unexpected = 2
};

//
// Used by OutputManager::Update(), maps to DDP_DuplReturn values where applicable
//
enum DDPDuplReturnUpdate
{
    ddp_dupl_return_update_success                   = 0,
    ddp_dupl_return_update_error_expected            = 1,
    ddp_dupl_return_update_error_unexpected          = 2,
    ddp_dupl_return_update_quit                      = 3,
    ddp_dupl_return_update_retry                     = 4,
    ddp_dupl_return_update_success_refreshed_overlay = 5
};

DDPDuplReturn ProcessFailure(_In_opt_ ID3D11Device* device, _In_ LPCWSTR str, _In_ LPCWSTR title, HRESULT hr, _In_opt_z_ HRESULT* expected_errors = nullptr);

void DisplayMsg(_In_ LPCWSTR str, _In_ LPCWSTR title, HRESULT hr);

//
// Holds info about the pointer/cursor
//
struct DDPPtrInfo
{
    std::vector<BYTE> ShapeBuffer;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO ShapeInfo = {};
    POINT Position = {0, 0};
    bool Visible = false;
    UINT WhoUpdatedPositionLast = 0;
    LARGE_INTEGER LastTimeStamp = {0};
    bool CursorShapeChanged = false;
};

//
// Structure that holds D3D resources not directly tied to any one thread
//
struct DDPDxResources
{
    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> Sampler;
};

//
// Structure to pass to a new thread
//
struct DDPThreadData
{
    // Used to indicate abnormal error condition
    HANDLE UnexpectedErrorEvent = nullptr;

    // Used to indicate a transition event occurred e.g. PnpStop, PnpStart, mode change, TDR, desktop switch and the application needs to recreate the duplication interface
    HANDLE ExpectedErrorEvent = nullptr;

    HANDLE NewFrameProcessedEvent = nullptr;
    HANDLE PauseDuplicationEvent  = nullptr;
    HANDLE ResumeDuplicationEvent = nullptr;

    // Used by WinProc to signal to threads to exit
    HANDLE TerminateThreadsEvent = nullptr;

    HANDLE TexSharedHandle = nullptr;
    UINT Output = 0;
    INT OffsetX = 0;
    INT OffsetY = 0;
    DDPPtrInfo* PtrInfo = nullptr;                  //Should only be called when shared surface mutex has be aquired, always points to DDPThreadManager::m_PtrInfo
    DDPDxResources DxRes;
    DPRect* DirtyRegionTotal = nullptr;             //Should only be called when shared surface mutex has be aquired, always points to DDPThreadManager::m_DirtyRegionTotal
    bool WMRIgnoreVScreens = false;
};

//
// FrameData holds information about an acquired frame
//
struct DDPFrameData
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Frame;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo = {};
    std::vector<BYTE>* MetaDataBuffer = nullptr;    //Should only be called when shared surface mutex has be aquired, always points to DDPDuplicationManager::m_MetaDataBuffer
    UINT DirtyCount = 0;
    UINT MoveCount = 0;
};

//
// A vertex with a position and texture coordinate
//
struct DDPVertex
{
    DirectX::XMFLOAT3 Pos = {};
    DirectX::XMFLOAT2 TexCoord = {};
};

#endif
