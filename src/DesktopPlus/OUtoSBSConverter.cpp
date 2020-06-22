#include "OUtoSBSConverter.h"

#include "ConfigManager.h"

using namespace DirectX;

OUtoSBSConverter::OUtoSBSConverter() : m_TexSBS(nullptr),
                                       m_MultiGPUTexSBSStaging(nullptr),
                                       m_MultiGPUTexSBSTarget(nullptr),
                                       m_TexSBSWidth(0),
                                       m_TexSBSHeight(0),
                                       m_ShaderResource(nullptr),
                                       m_RTV(nullptr),
                                       m_VertexBuffer(nullptr),
                                       m_LastCropX(-1),
                                       m_LastCropWidth(-1),
                                       m_LastCropHeight(-1)
{

}

OUtoSBSConverter::~OUtoSBSConverter()
{
    CleanRefs();
}

ID3D11Texture2D* OUtoSBSConverter::GetTexture() const
{
    return (m_MultiGPUTexSBSTarget != nullptr) ? m_MultiGPUTexSBSTarget : m_TexSBS;
}

bool OUtoSBSConverter::Convert(ID3D11Device* device, ID3D11DeviceContext* device_context, ID3D11PixelShader* pixel_shader, ID3D11VertexShader* vertex_shader, ID3D11SamplerState* sampler,
                               ID3D11Device* multi_gpu_device, ID3D11DeviceContext* multi_gpu_device_context, ID3D11Texture2D* tex_source, int tex_source_width, int tex_source_height,
                               int crop_x, int crop_width, int crop_height)
{
    const int vertex_count = 12;

    int sbs_width  = tex_source_width  * 2;
    int sbs_height = tex_source_height / 2;

    //Resource setup on first time or when dimensions changed
    if ( (m_TexSBS == nullptr) || (sbs_width != m_TexSBSWidth) || (sbs_height != m_TexSBSHeight) )
    {
        m_TexSBSWidth = sbs_width;
        m_TexSBSHeight = sbs_height;

        //Delete old resources if they exist
        CleanRefs();

        //Create texture
        D3D11_TEXTURE2D_DESC TexD;
        RtlZeroMemory(&TexD, sizeof(D3D11_TEXTURE2D_DESC));
        TexD.Width  = sbs_width;
        TexD.Height = sbs_height;
        TexD.MipLevels = 1;
        TexD.ArraySize = 1;
        TexD.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        TexD.SampleDesc.Count = 1;
        TexD.Usage = D3D11_USAGE_DEFAULT;
        TexD.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        TexD.CPUAccessFlags = 0;
        TexD.MiscFlags = 0;

        HRESULT hr = device->CreateTexture2D(&TexD, nullptr, &m_TexSBS);

        if (FAILED(hr))
        {
            ProcessFailure(device, L"Failed to create OU 3D texture", L"Error", hr, SystemTransitionsExpectedErrors);
            return false;
        }


        //Create textures for multi-gpu processing if needed
        if (multi_gpu_device != nullptr) 
        {
            //Staging texture
            TexD.Usage = D3D11_USAGE_STAGING;
            TexD.BindFlags = 0;
            TexD.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            TexD.MiscFlags = 0;

            hr = device->CreateTexture2D(&TexD, nullptr, &m_MultiGPUTexSBSStaging);

            if (FAILED(hr))
            {
                ProcessFailure(device, L"Failed to create OU 3D staging texture", L"Error", hr);
                return false;
            }

            //Copy-target texture
            TexD.Usage = D3D11_USAGE_DYNAMIC;
            TexD.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            TexD.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            TexD.MiscFlags = 0;

            hr = multi_gpu_device->CreateTexture2D(&TexD, nullptr, &m_MultiGPUTexSBSTarget);

            if (FAILED(hr))
            {
                ProcessFailure(multi_gpu_device, L"Failed to create OU 3D copy-target texture", L"Error", hr);
                return false;
            }
        }


        //Create shader resource
        D3D11_TEXTURE2D_DESC FrameDesc;
        m_TexSBS->GetDesc(&FrameDesc);

        D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc;
        ShaderDesc.Format = FrameDesc.Format;
        ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        ShaderDesc.Texture2D.MostDetailedMip = FrameDesc.MipLevels - 1;
        ShaderDesc.Texture2D.MipLevels = FrameDesc.MipLevels;

        hr = device->CreateShaderResourceView(tex_source, &ShaderDesc, &m_ShaderResource);
        if (FAILED(hr))
        {
            ProcessFailure(device, L"Failed to create shader resource for OU 3D texture", L"Error", hr, SystemTransitionsExpectedErrors);
            return false;
        }


        //Create render target for overlay texture
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;

        rtv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0;

        hr = device->CreateRenderTargetView(m_TexSBS, &rtv_desc, &m_RTV);
        if (FAILED(hr))
        {
            ProcessFailure(device, L"Failed to create render target view for OU 3D texture", L"Error", hr, SystemTransitionsExpectedErrors);
            return false;
        }
    }

    //Create vertex buffer if it doesn't exist or cropping values changed
    if ( (m_VertexBuffer == nullptr) || (crop_x != m_LastCropX) || (crop_width != m_LastCropWidth) || (crop_height != m_LastCropHeight) )
    {
        m_LastCropX      = crop_x;
        m_LastCropWidth  = crop_width;
        m_LastCropHeight = crop_height;

        if (m_VertexBuffer != nullptr)
        {
            m_VertexBuffer->Release();
            m_VertexBuffer = nullptr;
        }
    
        //Create vertex buffer for swapping 3D arrangement to SBS (values are adjusted for cropping below)
        VERTEX Vertices[vertex_count] =
        {
            //Top -> Left
            { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.5f) },
            { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
            { XMFLOAT3( 0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.5f) },
            { XMFLOAT3( 0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.5f) },
            { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
            { XMFLOAT3( 0.0f,  1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
            //Bottom -> Right
            { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.5f) },
            { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
            { XMFLOAT3( 0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.5f) },
            { XMFLOAT3( 0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.5f) },
            { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
            { XMFLOAT3( 0.0f,  1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
        };

        // Center of texture dimensions
        int center_x = (sbs_width  / 2);
        int center_y = (sbs_height / 2);

        //Adjust for horizontal crop
        float offset = crop_width / (float)center_x;

        Vertices[6].Pos.x  += offset;
        Vertices[7].Pos.x  += offset;
        Vertices[8].Pos.x  += offset;
        Vertices[9].Pos.x  += offset;
        Vertices[10].Pos.x += offset;
        Vertices[11].Pos.x += offset;

        offset = crop_x / (float)center_x;

        Vertices[6].Pos.x  += offset;
        Vertices[7].Pos.x  += offset;
        Vertices[8].Pos.x  += offset;
        Vertices[9].Pos.x  += offset;
        Vertices[10].Pos.x += offset;
        Vertices[11].Pos.x += offset;

        offset = crop_x / (float)tex_source_width;
        
        Vertices[6].TexCoord.x  += offset;
        Vertices[7].TexCoord.x  += offset;
        Vertices[8].TexCoord.x  += offset;
        Vertices[9].TexCoord.x  += offset;
        Vertices[10].TexCoord.x += offset;
        Vertices[11].TexCoord.x += offset;

        //Adjust for vertical crop (no need to adapt position since the cropping itself takes care of it)        
        offset = ( (crop_height) / (float)tex_source_height) / 2.0f;

        Vertices[6].TexCoord.y  += offset;
        Vertices[7].TexCoord.y  += offset;
        Vertices[8].TexCoord.y  += offset;
        Vertices[9].TexCoord.y  += offset;
        Vertices[10].TexCoord.y += offset;
        Vertices[11].TexCoord.y += offset;


        D3D11_BUFFER_DESC BufferDesc;
        RtlZeroMemory(&BufferDesc, sizeof(BufferDesc));
        BufferDesc.Usage = D3D11_USAGE_DEFAULT;
        BufferDesc.ByteWidth = sizeof(VERTEX) * vertex_count;
        BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        BufferDesc.CPUAccessFlags = 0;
        D3D11_SUBRESOURCE_DATA InitData;
        RtlZeroMemory(&InitData, sizeof(InitData));
        InitData.pSysMem = Vertices;

        // Create vertex buffer
        HRESULT hr = device->CreateBuffer(&BufferDesc, &InitData, &m_VertexBuffer);
        if (FAILED(hr))
        {
            ProcessFailure(device, L"Failed to create vertex buffer for OU 3D texture", L"Error", hr, SystemTransitionsExpectedErrors);
            return false;
        }
    }

    //Set resources for rendering
    UINT Stride = sizeof(VERTEX);
    UINT Offset = 0;
    const FLOAT blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    device_context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    device_context->OMSetRenderTargets(1, &m_RTV, nullptr);
    device_context->VSSetShader(vertex_shader, nullptr, 0);
    device_context->PSSetShader(pixel_shader, nullptr, 0);
    device_context->PSSetShaderResources(0, 1, &m_ShaderResource);
    device_context->PSSetSamplers(0, 1, &sampler);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    device_context->IASetVertexBuffers(0, 1, &m_VertexBuffer, &Stride, &Offset);

    //Set scissor rect
    const D3D11_RECT rect_scissor = { 0, 0, sbs_width, sbs_height };
    device_context->RSSetScissorRects(1, &rect_scissor);

    //Set view port
    D3D11_VIEWPORT VP;
    VP.Width  = static_cast<FLOAT>(sbs_width);
    VP.Height = static_cast<FLOAT>(sbs_height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0;
    VP.TopLeftY = 0;
    device_context->RSSetViewports(1, &VP);

    //Draw textured quads
    device_context->Draw(vertex_count, 0);

    //Reset scissor rect to full
    const D3D11_RECT rect_scissor_full = { 0, 0, tex_source_width, tex_source_height };
    device_context->RSSetScissorRects(1, &rect_scissor_full);

    //Reset view port
    VP.Width  = static_cast<FLOAT>(tex_source_width);
    VP.Height = static_cast<FLOAT>(tex_source_height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0;
    VP.TopLeftY = 0;
    device_context->RSSetViewports(1, &VP);

    //If set up for multi-gpu processing, copy the texture over
    if (m_MultiGPUTexSBSTarget != nullptr)
    {
        //Same as in OutputManager::RefreshOpenVROverlayTexture
        device_context->CopyResource(m_MultiGPUTexSBSStaging, m_TexSBS);

        D3D11_MAPPED_SUBRESOURCE mapped_resource_staging;
        RtlZeroMemory(&mapped_resource_staging, sizeof(D3D11_MAPPED_SUBRESOURCE));
        HRESULT hr = device_context->Map(m_MultiGPUTexSBSStaging, 0, D3D11_MAP_READ, 0, &mapped_resource_staging);

        if (FAILED(hr))
        {
            ProcessFailure(device, L"Failed to map OU 3D staging texture", L"Error", hr, SystemTransitionsExpectedErrors);
            return false;
        }

        D3D11_MAPPED_SUBRESOURCE mapped_resource_target;
        RtlZeroMemory(&mapped_resource_target, sizeof(D3D11_MAPPED_SUBRESOURCE));
        hr = multi_gpu_device_context->Map(m_MultiGPUTexSBSTarget, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource_target);

        if (FAILED(hr))
        {
            ProcessFailure(multi_gpu_device, L"Failed to map OU 3D copy-target texture", L"Error", hr, SystemTransitionsExpectedErrors);
            return false;
        }

        memcpy(mapped_resource_target.pData, mapped_resource_staging.pData, sbs_height * mapped_resource_staging.RowPitch);

        device_context->Unmap(m_MultiGPUTexSBSStaging, 0);
        multi_gpu_device_context->Unmap(m_MultiGPUTexSBSTarget, 0);
    }

    return m_TexSBS;
}

void OUtoSBSConverter::CleanRefs()
{
    if (m_TexSBS != nullptr)
    {
        m_TexSBS->Release();
        m_TexSBS = nullptr;
    }

    if (m_MultiGPUTexSBSStaging != nullptr)
    {
        m_MultiGPUTexSBSStaging->Release();
        m_MultiGPUTexSBSStaging = nullptr;
    }

    if (m_MultiGPUTexSBSTarget != nullptr)
    {
        m_MultiGPUTexSBSTarget->Release();
        m_MultiGPUTexSBSTarget = nullptr;
    }

    if (m_ShaderResource != nullptr)
    {
        m_ShaderResource->Release();
        m_ShaderResource = nullptr;
    }

    if (m_RTV != nullptr)
    {
        m_RTV->Release();
        m_RTV = nullptr;
    }

    if (m_VertexBuffer != nullptr)
    {
        m_VertexBuffer->Release();
        m_VertexBuffer = nullptr;
    }
}
