#include "DuplicationManager.h"

#include <sdkddkver.h>

//Keep building with 10.0.17763.0 / 1809 SDK optional
#ifdef NTDDI_WIN10_RS5
    #include <dxgi1_5.h>
#else
    #define DPLUS_DUP_NO_HDR
#endif

//
// Initialize duplication interfaces
//
DDPDuplReturn DDPDuplicationManager::InitDupl(const Microsoft::WRL::ComPtr<ID3D11Device>& Device, UINT Output, bool WMRIgnoreVScreens, bool UseHDR)
{
    m_OutputNumber = Output;

    #ifdef DPLUS_DUP_NO_HDR
        UseHDR = false;
    #endif

    // Take a reference on the device
    m_Device = Device;

    //Enumerate adapters the same way the main thread does so IDs match in multi-GPU situations
    //Desktop Duplication of desktops spanned across multiple GPUs is not supported right now, however
    //DuplicateOutput() will fail if the adapter for the output doesn't match Device
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory_ptr;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr_output;
    int output_id_adapter = Output;           //Output ID on the adapter actually used. Only different from initial Output if there's desktops across multiple GPUs

    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory_ptr);
    if (!FAILED(hr))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr;
        UINT i = 0;
        int output_count = 0;

        while (factory_ptr->EnumAdapters(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND)
        {
            //Check if this a WMR virtual display adapter and skip it when the option is enabled
            if (WMRIgnoreVScreens)
            {
                DXGI_ADAPTER_DESC adapter_desc;
                adapter_ptr->GetDesc(&adapter_desc);

                if (wcscmp(adapter_desc.Description, L"Virtual Display Adapter") == 0)
                {
                    ++i;
                    continue;
                }
            }

            //Count the available outputs
            Microsoft::WRL::ComPtr<IDXGIOutput> output_ptr;
            UINT output_index = 0;
            while (adapter_ptr->EnumOutputs(output_index, &output_ptr) != DXGI_ERROR_NOT_FOUND)
            {
                //Check if this happens to be the output we're looking for
                if ( (adapter_ptr_output == nullptr) && (Output == output_count) )
                {
                    adapter_ptr_output = adapter_ptr;
                    output_id_adapter = output_index;
                }

                ++output_count;
                ++output_index;
            }

            ++i;
        }
    }

    if (adapter_ptr_output == nullptr)
    {
        return ProcessFailure(m_Device.Get(), L"Failed to get the output's DXGI adapter", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //Get output
    Microsoft::WRL::ComPtr<IDXGIOutput> DxgiOutput;
    hr = adapter_ptr_output->EnumOutputs(output_id_adapter, &DxgiOutput);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device.Get(), L"Failed to get specified DXGI output", L"Desktop+ Error", hr, EnumOutputsExpectedErrors);
    }

    DxgiOutput->GetDesc(&m_OutputDesc);

    //Create desktop duplication
    if (!UseHDR)
    {
        Microsoft::WRL::ComPtr<IDXGIOutput1> DxgiOutput1;
        hr = DxgiOutput.As(&DxgiOutput1);
        if (FAILED(hr))
        {
            return ProcessFailure(nullptr, L"Failed to get output as DxgiOutput1", L"Desktop+ Error", hr);
        }

        hr = DxgiOutput1->DuplicateOutput(m_Device.Get(), &m_DeskDupl);
        if (FAILED(hr))
        {
            if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
            {
                ProcessFailure(m_Device.Get(), L"There is already the maximum number of applications using the Desktop Duplication API running, please close one of those applications and then try again", L"Desktop+ Error", hr);
                return ddp_dupl_return_error_unexpected;
            }
            return ProcessFailure(m_Device.Get(), L"Failed to get duplicate output", L"Desktop+ Error", hr, CreateDuplicationExpectedErrors);
        }
    }
    else
    {
        #ifndef DPLUS_DUP_NO_HDR

        Microsoft::WRL::ComPtr<IDXGIOutput5> DxgiOutput5;
        hr = DxgiOutput.As(&DxgiOutput5);
        if (FAILED(hr))
        {
            return ProcessFailure(nullptr, L"Failed to get output as DxgiOutput5", L"Desktop+ Error", hr);
        }

        const DXGI_FORMAT supported_formats[] = {DXGI_FORMAT_R16G16B16A16_FLOAT};
        hr = DxgiOutput5->DuplicateOutput1(m_Device.Get(), 0, 1, supported_formats, &m_DeskDupl);
        if (FAILED(hr))
        {
            if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
            {
                ProcessFailure(m_Device.Get(), L"There is already the maximum number of applications using the Desktop Duplication API running, please close one of those applications and then try again", L"Desktop+ Error", hr);
                return ddp_dupl_return_error_unexpected;
            }
            return ProcessFailure(m_Device.Get(), L"Failed to get duplicate output", L"Desktop+ Error", hr, CreateDuplicationExpectedErrors);
        }

        #endif
    }

    return ddp_dupl_return_success;
}

//
// Retrieves mouse info and write it into PtrInfo
//
DDPDuplReturn DDPDuplicationManager::GetMouse(DDPPtrInfo& PtrInfo, DXGI_OUTDUPL_FRAME_INFO& FrameInfo, INT OffsetX, INT OffsetY)
{
    // A non-zero mouse update timestamp indicates that there is a mouse position update and optionally a shape change
    if (FrameInfo.LastMouseUpdateTime.QuadPart == 0)
    {
        return ddp_dupl_return_success;
    }

    bool UpdatePosition = true;

    // Make sure we don't update pointer position wrongly
    // If pointer is invisible, make sure we did not get an update from another output that the last time that said pointer
    // was visible, if so, don't set it to invisible or update.
    if (!FrameInfo.PointerPosition.Visible && (PtrInfo.WhoUpdatedPositionLast != m_OutputNumber))
    {
        UpdatePosition = false;
    }

    // If two outputs both say they have a visible, only update if new update has newer timestamp
    if (FrameInfo.PointerPosition.Visible && PtrInfo.Visible && (PtrInfo.WhoUpdatedPositionLast != m_OutputNumber) && (PtrInfo.LastTimeStamp.QuadPart > FrameInfo.LastMouseUpdateTime.QuadPart))
    {
        UpdatePosition = false;
    }

    // Update position
    if (UpdatePosition)
    {
        PtrInfo.Position.x             = FrameInfo.PointerPosition.Position.x + m_OutputDesc.DesktopCoordinates.left - OffsetX;
        PtrInfo.Position.y             = FrameInfo.PointerPosition.Position.y + m_OutputDesc.DesktopCoordinates.top - OffsetY;
        PtrInfo.WhoUpdatedPositionLast = m_OutputNumber;
        PtrInfo.LastTimeStamp          = FrameInfo.LastMouseUpdateTime;
        PtrInfo.Visible                = FrameInfo.PointerPosition.Visible != 0;

        //If pointer is not visible, set the hotspot to 0,0
        if (!PtrInfo.Visible)
        {
            PtrInfo.ShapeInfo.HotSpot.x = 0;
            PtrInfo.ShapeInfo.HotSpot.y = 0;
        }
    }

    // No new shape
    if (FrameInfo.PointerShapeBufferSize == 0)
    {
        PtrInfo.CursorShapeChanged = false;
        return ddp_dupl_return_success;
    }

    PtrInfo.CursorShapeChanged = true;

    // Get shape
    PtrInfo.ShapeBuffer.resize(FrameInfo.PointerShapeBufferSize, 0);
    UINT BufferSizeRequired;
    HRESULT hr = m_DeskDupl->GetFramePointerShape(FrameInfo.PointerShapeBufferSize, PtrInfo.ShapeBuffer.data(), &BufferSizeRequired, &(PtrInfo.ShapeInfo));
    if (FAILED(hr))
    {
        PtrInfo.ShapeBuffer.clear();
        return ProcessFailure(m_Device.Get(), L"Failed to get frame pointer shape", L"Desktop+ Error", hr, FrameInfoExpectedErrors);
    }

    return ddp_dupl_return_success;
}


//
// Get next frame and write it into Data
//
_Success_(Timeout == false && return == ddp_dupl_return_success)
DDPDuplReturn DDPDuplicationManager::GetFrame(DDPFrameData& Data, bool& Timeout)
{
    Microsoft::WRL::ComPtr<IDXGIResource> DesktopResource;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;

    // Get new frame
    HRESULT hr = m_DeskDupl->AcquireNextFrame(100, &FrameInfo, &DesktopResource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
    {
        Timeout = true;
        return ddp_dupl_return_success;
    }
    Timeout = false;

    if (FAILED(hr))
    {
        return ProcessFailure(m_Device.Get(), L"Failed to acquire next frame", L"Desktop+ Error", hr, FrameInfoExpectedErrors);
    }

    // Get texture from IDXGIResource
    hr = DesktopResource.As(&m_AcquiredDesktopImage);
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed get ID3D11Texture2D from acquired IDXGIResource", L"Desktop+ Error", hr);
    }

    // Get metadata
    if (FrameInfo.TotalMetadataBufferSize)
    {
        // Old buffer too small
        if (FrameInfo.TotalMetadataBufferSize > m_MetaDataBuffer.size())
        {
            try
            {
                m_MetaDataBuffer.resize(FrameInfo.TotalMetadataBufferSize);
            }
            catch (std::bad_alloc)
            {
                Data.MoveCount = 0;
                Data.DirtyCount = 0;
                return ProcessFailure(nullptr, L"Failed to allocate memory for metadata in DDPDuplicationManager", L"Error", E_OUTOFMEMORY);
            }
        }

        UINT BufSize = FrameInfo.TotalMetadataBufferSize;

        // Get move rectangles
        hr = m_DeskDupl->GetFrameMoveRects(BufSize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(m_MetaDataBuffer.data()), &BufSize);
        if (FAILED(hr))
        {
            Data.MoveCount = 0;
            Data.DirtyCount = 0;
            return ProcessFailure(nullptr, L"Failed to get frame move rects", L"Desktop+ Error", hr, FrameInfoExpectedErrors);
        }
        Data.MoveCount = BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

        BYTE* DirtyRects = m_MetaDataBuffer.data() + BufSize;
        BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;

        // Get dirty rectangles
        hr = m_DeskDupl->GetFrameDirtyRects(BufSize, reinterpret_cast<RECT*>(DirtyRects), &BufSize);
        if (FAILED(hr))
        {
            Data.MoveCount = 0;
            Data.DirtyCount = 0;
            return ProcessFailure(nullptr, L"Failed to get frame dirty rects", L"Desktop+ Error", hr, FrameInfoExpectedErrors);
        }
        Data.DirtyCount = BufSize / sizeof(RECT);

        Data.MetaDataBuffer = &m_MetaDataBuffer;
    }

    Data.Frame = m_AcquiredDesktopImage;
    Data.FrameInfo = FrameInfo;

    return ddp_dupl_return_success;
}

//
// Release frame
//
DDPDuplReturn DDPDuplicationManager::DoneWithFrame()
{
    HRESULT hr = m_DeskDupl->ReleaseFrame();
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device.Get(), L"Failed to release frame", L"Desktop+ Error", hr, FrameInfoExpectedErrors);
    }

    m_AcquiredDesktopImage.Reset();

    return ddp_dupl_return_success;
}

//
// Gets output desc into DescPtr
//
void DDPDuplicationManager::GetOutputDesc(DXGI_OUTPUT_DESC& DescOut)
{
    DescOut = m_OutputDesc;
}
