#ifndef _DUPLICATIONMANAGER_H_
#define _DUPLICATIONMANAGER_H_

#include "CommonTypes.h"

//
// Handles the task of duplicating an output.
//
class DDPDuplicationManager
{
    public:
        _Success_(Timeout == false && return == ddp_dupl_return_success) DDPDuplReturn GetFrame(DDPFrameData& Data, bool& Timeout);
        DDPDuplReturn DoneWithFrame();
        DDPDuplReturn InitDupl(const Microsoft::WRL::ComPtr<ID3D11Device>& Device, UINT Output, bool WMRIgnoreVScreens, bool UseHDR);
        DDPDuplReturn GetMouse(DDPPtrInfo& PtrInfo, DXGI_OUTDUPL_FRAME_INFO& FrameInfo, INT OffsetX, INT OffsetY);
        void GetOutputDesc(DXGI_OUTPUT_DESC& DescOut);

    private:

    // vars
        Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_DeskDupl;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_AcquiredDesktopImage;
        std::vector<BYTE> m_MetaDataBuffer;
        UINT m_OutputNumber = 0;
        DXGI_OUTPUT_DESC m_OutputDesc = {};
        Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
};

#endif
