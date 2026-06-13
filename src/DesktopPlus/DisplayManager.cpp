#include "DisplayManager.h"
using namespace DirectX;

#include "DPRect.h"

//
// Initialize D3D variables
//
void DDPDisplayManager::InitD3D(const DDPDxResources& Data)
{
    m_Device        = Data.Device;
    m_DeviceContext = Data.Context;
    m_VertexShader  = Data.VertexShader;
    m_PixelShader   = Data.PixelShader;
    m_InputLayout   = Data.InputLayout;
    m_SamplerLinear = Data.Sampler;
}

//
// Process a given frame and its metadata
//
DDPDuplReturn DDPDisplayManager::ProcessFrame(const DDPFrameData& Data, _Inout_ ID3D11Texture2D* SharedSurf, INT OffsetX, INT OffsetY, const DXGI_OUTPUT_DESC& DeskDesc, _Inout_ DPRect& DirtyRectTotal)
{
    DDPDuplReturn Ret = ddp_dupl_return_success;

    // Process dirties and moves
    if (Data.FrameInfo.TotalMetadataBufferSize)
    {
        D3D11_TEXTURE2D_DESC Desc;
        Data.Frame->GetDesc(&Desc);

        if (Data.MoveCount)
        {
            Ret = CopyMove(SharedSurf, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(Data.MetaDataBuffer->data()), Data.MoveCount, OffsetX, OffsetY, DeskDesc, Desc.Width, Desc.Height, DirtyRectTotal);
            if (Ret != ddp_dupl_return_success)
            {
                return Ret;
            }
        }

        if (Data.DirtyCount)
        {
            Ret = CopyDirty(Data.Frame.Get(), SharedSurf, reinterpret_cast<RECT*>(Data.MetaDataBuffer->data() + (Data.MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT))), Data.DirtyCount, OffsetX, OffsetY, DeskDesc, 
                            DirtyRectTotal);
        }
    }

    return Ret;
}

void DDPDisplayManager::OnPause()
{
    //Release intermediate move texture to reduce memory while idle
    m_MoveSurf.Reset();

    //Flush, clear state & trim to free memory right away
    m_DeviceContext->Flush();
    m_DeviceContext->ClearState();

    Microsoft::WRL::ComPtr<IDXGIDevice3> DxgiDevice3;
    HRESULT hr = m_Device.As(&DxgiDevice3);
    if (SUCCEEDED(hr))
    {
        DxgiDevice3->Trim();
    }
}

//
// Set appropriate source and destination rects for move rects
//
void DDPDisplayManager::SetMoveRect(_Out_ RECT& SrcRect, _Out_ RECT& DestRect, const DXGI_OUTPUT_DESC& DeskDesc, const DXGI_OUTDUPL_MOVE_RECT& MoveRect, INT TexWidth, INT TexHeight)
{
    switch (DeskDesc.Rotation)
    {
        case DXGI_MODE_ROTATION_UNSPECIFIED:
        case DXGI_MODE_ROTATION_IDENTITY:
        {
            SrcRect.left   = MoveRect.SourcePoint.x;
            SrcRect.top    = MoveRect.SourcePoint.y;
            SrcRect.right  = MoveRect.SourcePoint.x + MoveRect.DestinationRect.right  - MoveRect.DestinationRect.left;
            SrcRect.bottom = MoveRect.SourcePoint.y + MoveRect.DestinationRect.bottom - MoveRect.DestinationRect.top;

            DestRect = MoveRect.DestinationRect;
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE90:
        {
            SrcRect.left     = TexHeight - (MoveRect.SourcePoint.y + MoveRect.DestinationRect.bottom - MoveRect.DestinationRect.top);
            SrcRect.top      = MoveRect.SourcePoint.x;
            SrcRect.right    = TexHeight - MoveRect.SourcePoint.y;
            SrcRect.bottom   = MoveRect.SourcePoint.x + MoveRect.DestinationRect.right - MoveRect.DestinationRect.left;

            DestRect.left   = TexHeight - MoveRect.DestinationRect.bottom;
            DestRect.top    = MoveRect.DestinationRect.left;
            DestRect.right  = TexHeight - MoveRect.DestinationRect.top;
            DestRect.bottom = MoveRect.DestinationRect.right;
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE180:
        {
            SrcRect.left   = TexWidth  - (MoveRect.SourcePoint.x + MoveRect.DestinationRect.right - MoveRect.DestinationRect.left);
            SrcRect.top    = TexHeight - (MoveRect.SourcePoint.y + MoveRect.DestinationRect.bottom - MoveRect.DestinationRect.top);
            SrcRect.right  = TexWidth  -  MoveRect.SourcePoint.x;
            SrcRect.bottom = TexHeight -  MoveRect.SourcePoint.y;

            DestRect.left   = TexWidth  - MoveRect.DestinationRect.right;
            DestRect.top    = TexHeight - MoveRect.DestinationRect.bottom;
            DestRect.right  = TexWidth  - MoveRect.DestinationRect.left;
            DestRect.bottom = TexHeight - MoveRect.DestinationRect.top;
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE270:
        {
            SrcRect.left   = MoveRect.SourcePoint.x;
            SrcRect.top    = TexWidth - (MoveRect.SourcePoint.x + MoveRect.DestinationRect.right - MoveRect.DestinationRect.left);
            SrcRect.right  = MoveRect.SourcePoint.y + MoveRect.DestinationRect.bottom - MoveRect.DestinationRect.top;
            SrcRect.bottom = TexWidth - MoveRect.SourcePoint.x;

            DestRect.left   = MoveRect.DestinationRect.top;
            DestRect.top    = TexWidth - MoveRect.DestinationRect.right;
            DestRect.right  = MoveRect.DestinationRect.bottom;
            DestRect.bottom = TexWidth - MoveRect.DestinationRect.left;
            break;
        }
        default:
        {
            DestRect = {};
            SrcRect  = {};
            break;
        }
    }
}

//
// Copy move rectangles
//
DDPDuplReturn DDPDisplayManager::CopyMove(_Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(MoveCount) DXGI_OUTDUPL_MOVE_RECT* MoveBuffer, UINT MoveCount, INT OffsetX, INT OffsetY, const DXGI_OUTPUT_DESC& DeskDesc,
                                          INT TexWidth, INT TexHeight, _Inout_ DPRect& DirtyRectTotal)
{
    D3D11_TEXTURE2D_DESC FullDesc;
    SharedSurf->GetDesc(&FullDesc);

    // Make new intermediate surface to copy into for moving
    if (!m_MoveSurf)
    {
        D3D11_TEXTURE2D_DESC MoveDesc;
        MoveDesc = FullDesc;
        MoveDesc.Width  = DeskDesc.DesktopCoordinates.right  - DeskDesc.DesktopCoordinates.left;
        MoveDesc.Height = DeskDesc.DesktopCoordinates.bottom - DeskDesc.DesktopCoordinates.top;
        MoveDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
        MoveDesc.MiscFlags = 0;
        HRESULT hr = m_Device->CreateTexture2D(&MoveDesc, nullptr, &m_MoveSurf);
        if (FAILED(hr))
        {
            return ProcessFailure(m_Device.Get(), L"Failed to create staging texture for move rects", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
        }
    }

    for (UINT i = 0; i < MoveCount; ++i)
    {
        RECT SrcRect;
        RECT DestRect;

        SetMoveRect(SrcRect, DestRect, DeskDesc, MoveBuffer[i], TexWidth, TexHeight);

        // Copy rect out of shared surface
        D3D11_BOX Box = {};
        Box.left   = SrcRect.left + DeskDesc.DesktopCoordinates.left - OffsetX;
        Box.top    = SrcRect.top  + DeskDesc.DesktopCoordinates.top  - OffsetY;
        Box.front  = 0;
        Box.right  = SrcRect.right  + DeskDesc.DesktopCoordinates.left - OffsetX;
        Box.bottom = SrcRect.bottom + DeskDesc.DesktopCoordinates.top  - OffsetY;
        Box.back   = 1;
        m_DeviceContext->CopySubresourceRegion(m_MoveSurf.Get(), 0, SrcRect.left, SrcRect.top, 0, SharedSurf, 0, &Box);

        // Copy back to shared surface
        Box.left   = SrcRect.left;
        Box.top    = SrcRect.top;
        Box.front  = 0;
        Box.right  = SrcRect.right;
        Box.bottom = SrcRect.bottom;
        Box.back   = 1;

        //Adjust by desktop and destination offsets
        DestRect.left += DeskDesc.DesktopCoordinates.left - OffsetX;
        DestRect.top  += DeskDesc.DesktopCoordinates.top  - OffsetY;

        m_DeviceContext->CopySubresourceRegion(SharedSurf, 0, DestRect.left, DestRect.top, 0, m_MoveSurf.Get(), 0, &Box);

        //Add rect to total dirty region rect
        DPRect drect(DestRect.left, DestRect.top, (int)Box.right, (int)Box.bottom);
        (DirtyRectTotal.GetTL().x == -1) ? DirtyRectTotal = drect : DirtyRectTotal.Add(drect);
    }

    return ddp_dupl_return_success;
}

//
// Sets up vertices for dirty rects for rotated desktops
//
void DDPDisplayManager::SetDirtyVert(_Out_writes_(DDP_NUMVERTICES) DDPVertex* Vertices, _In_ RECT* Dirty, INT OffsetX, INT OffsetY, const DXGI_OUTPUT_DESC& DeskDesc, const D3D11_TEXTURE2D_DESC& FullDesc,
                                     const D3D11_TEXTURE2D_DESC& ThisDesc, _Inout_ DPRect& DirtyRectTotal)
{
    FLOAT CenterX = FullDesc.Width  / 2.0f;
    FLOAT CenterY = FullDesc.Height / 2.0f;

    INT Width  = DeskDesc.DesktopCoordinates.right  - DeskDesc.DesktopCoordinates.left;
    INT Height = DeskDesc.DesktopCoordinates.bottom - DeskDesc.DesktopCoordinates.top;

    // Rotation compensated destination rect
    RECT DestDirty = *Dirty;

    // Set appropriate coordinates compensated for rotation
    switch (DeskDesc.Rotation)
    {
        case DXGI_MODE_ROTATION_ROTATE90:
        {
            DestDirty.left   = Width - Dirty->bottom;
            DestDirty.top    = Dirty->left;
            DestDirty.right  = Width - Dirty->top;
            DestDirty.bottom = Dirty->right;

            Vertices[0].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc.Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->left  / static_cast<FLOAT>(ThisDesc.Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc.Width), Dirty->top / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->left  / static_cast<FLOAT>(ThisDesc.Width), Dirty->top / static_cast<FLOAT>(ThisDesc.Height));
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE180:
        {
            DestDirty.left   = Width  - Dirty->right;
            DestDirty.top    = Height - Dirty->bottom;
            DestDirty.right  = Width  - Dirty->left;
            DestDirty.bottom = Height - Dirty->top;

            Vertices[0].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc.Width), Dirty->top    / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc.Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->left  / static_cast<FLOAT>(ThisDesc.Width), Dirty->top    / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->left  / static_cast<FLOAT>(ThisDesc.Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc.Height));
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE270:
        {
            DestDirty.left   = Dirty->top;
            DestDirty.top    = Height - Dirty->right;
            DestDirty.right  = Dirty->bottom;
            DestDirty.bottom = Height - Dirty->left;

            Vertices[0].TexCoord = XMFLOAT2(Dirty->left  / static_cast<FLOAT>(ThisDesc.Width), Dirty->top / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc.Width), Dirty->top / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->left  / static_cast<FLOAT>(ThisDesc.Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc.Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc.Height));
            break;
        }
        default:
            assert(false); // drop through
        case DXGI_MODE_ROTATION_UNSPECIFIED:
        case DXGI_MODE_ROTATION_IDENTITY:
        {
            Vertices[0].TexCoord = XMFLOAT2(Dirty->left  / static_cast<FLOAT>(ThisDesc.Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->left  / static_cast<FLOAT>(ThisDesc.Width), Dirty->top    / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc.Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc.Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc.Width), Dirty->top    / static_cast<FLOAT>(ThisDesc.Height));
            break;
        }
    }

    // Set positions
    Vertices[0].Pos = XMFLOAT3(     (DestDirty.left   + DeskDesc.DesktopCoordinates.left - OffsetX - CenterX) / CenterX,
                               -1 * (DestDirty.bottom + DeskDesc.DesktopCoordinates.top  - OffsetY - CenterY) / CenterY,
                               0.0f);
    Vertices[1].Pos = XMFLOAT3(     (DestDirty.left + DeskDesc.DesktopCoordinates.left - OffsetX - CenterX) / CenterX,
                               -1 * (DestDirty.top + DeskDesc.DesktopCoordinates.top   - OffsetY - CenterY) / CenterY,
                               0.0f);
    Vertices[2].Pos = XMFLOAT3(     (DestDirty.right + DeskDesc.DesktopCoordinates.left - OffsetX - CenterX) / CenterX,
                               -1 * (DestDirty.bottom + DeskDesc.DesktopCoordinates.top - OffsetY - CenterY) / CenterY,
                               0.0f);
    Vertices[3].Pos = Vertices[2].Pos;
    Vertices[4].Pos = Vertices[1].Pos;
    Vertices[5].Pos = XMFLOAT3(    (DestDirty.right + DeskDesc.DesktopCoordinates.left - OffsetX - CenterX) / CenterX,
                               -1 * (DestDirty.top + DeskDesc.DesktopCoordinates.top   - OffsetY - CenterY) / CenterY,
                               0.0f);

    Vertices[3].TexCoord = Vertices[2].TexCoord;
    Vertices[4].TexCoord = Vertices[1].TexCoord;

    //Add rect to total dirty region rect
    DPRect drect(DestDirty.left, DestDirty.top, DestDirty.right, DestDirty.bottom);
    drect.Translate({DeskDesc.DesktopCoordinates.left - OffsetX, DeskDesc.DesktopCoordinates.top - OffsetY});
    (DirtyRectTotal.GetTL().x == -1) ? DirtyRectTotal = drect : DirtyRectTotal.Add(drect);
}

//
// Copies dirty rectangles
//
DDPDuplReturn DDPDisplayManager::CopyDirty(_In_ ID3D11Texture2D* SrcSurface, _Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(DirtyCount) RECT* DirtyBuffer, UINT DirtyCount, INT OffsetX, INT OffsetY, 
                                           const DXGI_OUTPUT_DESC& DeskDesc, _Inout_ DPRect& DirtyRectTotal)
{
    HRESULT hr;

    D3D11_TEXTURE2D_DESC FullDesc;
    SharedSurf->GetDesc(&FullDesc);

    D3D11_TEXTURE2D_DESC ThisDesc;
    SrcSurface->GetDesc(&ThisDesc);

    if (m_RTV == nullptr)
    {
        hr = m_Device->CreateRenderTargetView(SharedSurf, nullptr, &m_RTV);
        if (FAILED(hr))
        {
            return ProcessFailure(m_Device.Get(), L"Failed to create render target view for dirty rects", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
        }
    }

    //Create shader resource view if needed (format and such should in theory not change during duplication...)
    if (m_DirtyVertexShaderResource == nullptr)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc = {};
        ShaderDesc.Format = ThisDesc.Format;
        ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        ShaderDesc.Texture2D.MostDetailedMip = ThisDesc.MipLevels - 1;
        ShaderDesc.Texture2D.MipLevels = ThisDesc.MipLevels;

        hr = m_Device->CreateShaderResourceView(SrcSurface, &ShaderDesc, &m_DirtyVertexShaderResource);
        if (FAILED(hr))
        {
            return ProcessFailure(m_Device.Get(), L"Failed to create shader resource view for dirty rects", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
        }
    }

    const FLOAT BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};
    m_DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
    m_DeviceContext->OMSetRenderTargets(1, m_RTV.GetAddressOf(), nullptr);
    m_DeviceContext->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    m_DeviceContext->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    m_DeviceContext->PSSetShaderResources(0, 1, m_DirtyVertexShaderResource.GetAddressOf());
    m_DeviceContext->PSSetSamplers(0, 1, m_SamplerLinear.GetAddressOf());
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create space for vertices for the dirty rects if the current space isn't large enough and reset cached buffer
    UINT BytesNeeded = sizeof(DDPVertex) * DDP_NUMVERTICES * DirtyCount;
    if (BytesNeeded > m_DirtyVertexBufferData.size())
    {
        m_DirtyVertexBuffer.Reset();

        try
        {
            m_DirtyVertexBufferData.resize(BytesNeeded);
        }
        catch (std::bad_alloc)
        {
            return ProcessFailure(nullptr, L"Failed to allocate memory for dirty vertex buffer.", L"Desktop+ Error", E_OUTOFMEMORY);
        }
    }

    // Fill them in
    DDPVertex* DirtyVertex = reinterpret_cast<DDPVertex*>(m_DirtyVertexBufferData.data());
    for (UINT i = 0; i < DirtyCount; ++i, DirtyVertex += DDP_NUMVERTICES)
    {
        SetDirtyVert(DirtyVertex, &(DirtyBuffer[i]), OffsetX, OffsetY, DeskDesc, FullDesc, ThisDesc, DirtyRectTotal);
    }

    if (m_DirtyVertexBuffer == nullptr)
    {
        // Create vertex buffer
        D3D11_BUFFER_DESC BufferDesc = {};
        BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        BufferDesc.ByteWidth = BytesNeeded;
        BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA InitData = {};
        InitData.pSysMem = m_DirtyVertexBufferData.data();

        hr = m_Device->CreateBuffer(&BufferDesc, &InitData, &m_DirtyVertexBuffer);
        if (FAILED(hr))
        {
            return ProcessFailure(m_Device.Get(), L"Failed to create vertex buffer in dirty rect processing", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
        }
    }
    else
    {
        //Update existing buffer
        D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
        m_DeviceContext->Map(m_DirtyVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        memcpy(mapped_resource.pData, m_DirtyVertexBufferData.data(), BytesNeeded);
        m_DeviceContext->Unmap(m_DirtyVertexBuffer.Get(), 0);
    }

    UINT Stride = sizeof(DDPVertex);
    UINT Offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, m_DirtyVertexBuffer.GetAddressOf(), &Stride, &Offset);

    D3D11_VIEWPORT VP = {};
    VP.Width  = static_cast<FLOAT>(FullDesc.Width);
    VP.Height = static_cast<FLOAT>(FullDesc.Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    m_DeviceContext->RSSetViewports(1, &VP);

    m_DeviceContext->Draw(DDP_NUMVERTICES * DirtyCount, 0);

    return ddp_dupl_return_success;
}
