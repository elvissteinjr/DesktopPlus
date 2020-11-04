#pragma once

#include "CommonTypes.h"

//This class rearranges an OU 3D texture to an SBS 3D texture
class OUtoSBSConverter
{
    private:
        ID3D11Texture2D* m_TexSBS;                 //Owned by device
        ID3D11Texture2D* m_MultiGPUTexSBSStaging;  //Staging texture, owned by device
        ID3D11Texture2D* m_MultiGPUTexSBSTarget;   //Target texture to copy to, owned by multi_gpu_device
        int m_TexSBSWidth;
        int m_TexSBSHeight;
        ID3D11ShaderResourceView* m_ShaderResource;
        ID3D11RenderTargetView* m_RTV;
        ID3D11Buffer* m_VertexBuffer;
        int m_LastCropX;                           //Y isn't used, so no need 
        int m_LastCropWidth;
        int m_LastCropHeight;

    public:
        OUtoSBSConverter();
        ~OUtoSBSConverter();

        ID3D11Texture2D* GetTexture() const; //Does not add a reference
        bool Convert(ID3D11Device* device, ID3D11DeviceContext* device_context, ID3D11Device* multi_gpu_device, ID3D11DeviceContext* multi_gpu_device_context, 
                     ID3D11Texture2D* tex_source, int tex_source_width, int tex_source_height, int crop_x, int crop_y, int crop_width, int crop_height);
        void CleanRefs();

};