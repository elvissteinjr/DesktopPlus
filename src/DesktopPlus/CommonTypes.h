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
#include <string>

#include "DPRect.h"

#include "PixelShader.h"
#include "PixelShaderCursor.h"
#include "VertexShader.h"

#define NUMVERTICES 6
#define BPP         4

#define OCCLUSION_STATUS_MSG WM_USER

extern HRESULT SystemTransitionsExpectedErrors[];
extern HRESULT CreateDuplicationExpectedErrors[];
extern HRESULT FrameInfoExpectedErrors[];
extern HRESULT AcquireFrameExpectedError[];
extern HRESULT EnumOutputsExpectedErrors[];

typedef _Return_type_success_(return == DUPL_RETURN_SUCCESS) enum
{
    DUPL_RETURN_SUCCESS             = 0,
    DUPL_RETURN_ERROR_EXPECTED      = 1,
    DUPL_RETURN_ERROR_UNEXPECTED    = 2
}DUPL_RETURN;

//
// Used by OutputManager::Update(), maps to DUPL_RETURN values where applicable
//
typedef _Return_type_success_(return == DUPL_RETURN_UPD_SUCCESS) enum
{
    DUPL_RETURN_UPD_SUCCESS = 0,
    DUPL_RETURN_UPD_ERROR_EXPECTED = 1,
    DUPL_RETURN_UPD_ERROR_UNEXPECTED = 2,
    DUPL_RETURN_UPD_QUIT = 3,
    DUPL_RETURN_UPD_RETRY = 4,
    DUPL_RETURN_UPD_SUCCESS_REFRESHED_OVERLAY = 5
}DUPL_RETURN_UPD;

_Post_satisfies_(return != DUPL_RETURN_SUCCESS)
DUPL_RETURN ProcessFailure(_In_opt_ ID3D11Device* device, _In_ LPCWSTR str, _In_ LPCWSTR title, HRESULT hr, _In_opt_z_ HRESULT* expected_errors = nullptr);

void DisplayMsg(_In_ LPCWSTR str, _In_ LPCWSTR title, HRESULT hr);

//
// Holds info about the pointer/cursor
//
typedef struct _PTR_INFO
{
    _Field_size_bytes_(BufferSize) BYTE* PtrShapeBuffer;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO ShapeInfo;
    POINT Position;
    bool Visible;
    UINT BufferSize;
    UINT WhoUpdatedPositionLast;
    LARGE_INTEGER LastTimeStamp;
    bool CursorShapeChanged;
} PTR_INFO;

//
// Structure that holds D3D resources not directly tied to any one thread
//
typedef struct _DX_RESOURCES
{
    ID3D11Device* Device;
    ID3D11DeviceContext* Context;
    ID3D11VertexShader* VertexShader;
    ID3D11PixelShader* PixelShader;
    ID3D11InputLayout* InputLayout;
    ID3D11SamplerState* Sampler;
} DX_RESOURCES;

//
// Structure to pass to a new thread
//
typedef struct _THREAD_DATA
{
    // Used to indicate abnormal error condition
    HANDLE UnexpectedErrorEvent;

    // Used to indicate a transition event occurred e.g. PnpStop, PnpStart, mode change, TDR, desktop switch and the application needs to recreate the duplication interface
    HANDLE ExpectedErrorEvent;

    HANDLE NewFrameProcessedEvent;
    HANDLE PauseDuplicationEvent;
    HANDLE ResumeDuplicationEvent;

    // Used by WinProc to signal to threads to exit
    HANDLE TerminateThreadsEvent;

    HANDLE TexSharedHandle;
    UINT Output;
    INT OffsetX;
    INT OffsetY;
    PTR_INFO* PtrInfo;
    DX_RESOURCES DxRes;
    DPRect* DirtyRegionTotal;
    bool WMRIgnoreVScreens;
} THREAD_DATA;

//
// FRAME_DATA holds information about an acquired frame
//
typedef struct _FRAME_DATA
{
    ID3D11Texture2D* Frame;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;
    _Field_size_bytes_((MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT)) + (DirtyCount * sizeof(RECT))) BYTE* MetaData;
    UINT DirtyCount;
    UINT MoveCount;
} FRAME_DATA;

//
// A vertex with a position and texture coordinate
//
typedef struct _VERTEX
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT2 TexCoord;
} VERTEX;

#endif
