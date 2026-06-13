#ifndef _DISPLAYMANAGER_H_
#define _DISPLAYMANAGER_H_

#include "CommonTypes.h"

//
// Handles the task of processing frames
//
class DDPDisplayManager
{
    public:
        void InitD3D(const DDPDxResources& Data);
        DDPDuplReturn ProcessFrame(const DDPFrameData& Data, _Inout_ ID3D11Texture2D* SharedSurf, INT OffsetX, INT OffsetY, const DXGI_OUTPUT_DESC& DeskDesc, _Inout_ DPRect& DirtyRectTotal);
        void OnPause();

    private:
    // methods
        DDPDuplReturn CopyDirty(_In_ ID3D11Texture2D* SrcSurface, _Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(DirtyCount) RECT* DirtyBuffer, UINT DirtyCount, INT OffsetX, INT OffsetY,
                                const DXGI_OUTPUT_DESC& DeskDesc, _Inout_ DPRect& DirtyRectTotal);
        DDPDuplReturn CopyMove(_Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(MoveCount) DXGI_OUTDUPL_MOVE_RECT* MoveBuffer, UINT MoveCount, INT OffsetX, INT OffsetY, const DXGI_OUTPUT_DESC& DeskDesc,
                               INT TexWidth, INT TexHeight, _Inout_ DPRect& DirtyRectTotal);
        void SetDirtyVert(_Out_writes_(DDP_NUMVERTICES) DDPVertex* Vertices, _In_ RECT* Dirty, INT OffsetX, INT OffsetY, const DXGI_OUTPUT_DESC& DeskDesc, const D3D11_TEXTURE2D_DESC& FullDesc, 
                          const D3D11_TEXTURE2D_DESC& ThisDesc, _Inout_ DPRect& DirtyRectTotal);
        void SetMoveRect(_Out_ RECT& SrcRect, _Out_ RECT& DestRect, const DXGI_OUTPUT_DESC& DeskDesc, const DXGI_OUTDUPL_MOVE_RECT& MoveRect, INT TexWidth, INT TexHeight);

    // variables
        Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_DeviceContext;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_MoveSurf;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PixelShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RTV;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_SamplerLinear;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_DirtyVertexShaderResource;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_DirtyVertexBuffer;
        std::vector<BYTE> m_DirtyVertexBufferData;
};

#endif
