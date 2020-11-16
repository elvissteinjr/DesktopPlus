#pragma once

#define NOMINMAX
#include <d3d11.h>
#include <wrl/client.h>

//This class rearranges an OU 3D texture to a SBS 3D texture
class OUtoSBSConverter
{
    private:
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_TexSBS;                 //Owned by device
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_MultiGPUTexSBSStaging;  //Staging texture, owned by device
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_MultiGPUTexSBSTarget;   //Target texture to copy to, owned by multi_gpu_device
        int m_TexSBSWidth;
        int m_TexSBSHeight;

    public:
        OUtoSBSConverter();
        ~OUtoSBSConverter();

        ID3D11Texture2D* GetTexture() const; //Does not add a reference
        HRESULT Convert(ID3D11Device* device, ID3D11DeviceContext* device_context, ID3D11Device* multi_gpu_device, ID3D11DeviceContext* multi_gpu_device_context, 
                        ID3D11Texture2D* tex_source, int tex_source_width, int tex_source_height, int crop_x, int crop_y, int crop_width, int crop_height);
        void CleanRefs();

};