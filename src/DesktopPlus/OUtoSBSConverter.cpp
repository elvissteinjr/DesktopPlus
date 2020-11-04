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

bool OUtoSBSConverter::Convert(ID3D11Device* device, ID3D11DeviceContext* device_context, ID3D11Device* multi_gpu_device, ID3D11DeviceContext* multi_gpu_device_context, 
                               ID3D11Texture2D* tex_source, int tex_source_width, int tex_source_height, int crop_x, int crop_y, int crop_width, int crop_height)
{
    int sbs_width  = crop_width  * 2;
    int sbs_height = crop_height / 2;

    //Resource setup on first time or when dimensions changed
    if ( (m_TexSBS == nullptr) || (sbs_width != m_TexSBSWidth) || (sbs_height != m_TexSBSHeight) )
    {
        m_TexSBSWidth  = sbs_width;
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
    }

    //Copy top and bottom half of the cropped region into the left and right halves of SBS texture
    D3D11_BOX source_region;
    source_region.left   = clamp(crop_x, 0, tex_source_width);
    source_region.right  = clamp(crop_x + crop_width, 0, tex_source_width);
    source_region.top    = clamp(crop_y, 0, tex_source_height);
    source_region.bottom = clamp(crop_y + sbs_height, 0, tex_source_height);
    source_region.front  = 0;
    source_region.back   = 1;

    device_context->CopySubresourceRegion(m_TexSBS, 0, 0, 0, 0, tex_source, 0, &source_region);          //Top -> Left

    source_region.top    = source_region.bottom;
    source_region.bottom = clamp((int)source_region.top + sbs_height, 0, tex_source_height);

    device_context->CopySubresourceRegion(m_TexSBS, 0, crop_width, 0, 0, tex_source, 0, &source_region); //Bottom -> Right

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

    return true;
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
