#include "ThreadManager.h"

DWORD WINAPI CaptureThreadEntry(_In_ void* Param);

//
// Clean up resources
//
void DDPThreadManager::Clean()
{
    m_PtrInfo = DDPPtrInfo();

    for (const auto& thread_handle : m_ThreadHandles)
    {
        ::CloseHandle(thread_handle);
    }
    m_ThreadHandles.clear();

    m_ThreadData.clear();
}


//
// Start up threads for DDA
//
DDPDuplReturn DDPThreadManager::Initialize(INT SingleOutput, UINT OutputCount, HANDLE UnexpectedErrorEvent, HANDLE ExpectedErrorEvent, HANDLE NewFrameProcessedEvent,
                                           HANDLE PauseDuplicationEvent, HANDLE ResumeDuplicationEvent, HANDLE TerminateThreadsEvent,
                                           HANDLE SharedHandle, const RECT& DesktopDim, Microsoft::WRL::ComPtr<IDXGIAdapter> DXGIAdapter, bool WMRIgnoreVScreens)
{
    m_ThreadData.resize(OutputCount);

    // Create appropriate # of threads for duplication
    DDPDuplReturn Ret = ddp_dupl_return_success;
    for (UINT i = 0; i < m_ThreadData.size(); ++i)
    {
        DDPThreadData& ThreadData = m_ThreadData[i];

        ThreadData.UnexpectedErrorEvent   = UnexpectedErrorEvent;
        ThreadData.ExpectedErrorEvent     = ExpectedErrorEvent;
        ThreadData.NewFrameProcessedEvent = NewFrameProcessedEvent;
        ThreadData.PauseDuplicationEvent  = PauseDuplicationEvent;
        ThreadData.ResumeDuplicationEvent = ResumeDuplicationEvent;
        ThreadData.TerminateThreadsEvent  = TerminateThreadsEvent;
        ThreadData.Output                 = (SingleOutput < 0) ? i : SingleOutput;
        ThreadData.TexSharedHandle        = SharedHandle;
        ThreadData.OffsetX                = DesktopDim.left;
        ThreadData.OffsetY                = DesktopDim.top;
        ThreadData.PtrInfo                = &m_PtrInfo;
        ThreadData.DirtyRegionTotal       = &m_DirtyRegionTotal;
        ThreadData.WMRIgnoreVScreens      = WMRIgnoreVScreens;

        Ret = InitializeDx(ThreadData.DxRes, DXGIAdapter.Get());
        if (Ret != ddp_dupl_return_success)
        {
            return Ret;
        }

        DWORD ThreadId;
        HANDLE ThreadHandle = CreateThread(nullptr, 0, CaptureThreadEntry, &ThreadData, 0, &ThreadId);

        if (ThreadHandle == nullptr)
        {
            return ProcessFailure(nullptr, L"Failed to create thread", L"Desktop+ Error", E_FAIL);
        }

        m_ThreadHandles.push_back(ThreadHandle);
    }

    return Ret;
}

//
// Get DDPDxResources
//
DDPDuplReturn DDPThreadManager::InitializeDx(DDPDxResources& Data, IDXGIAdapter* DXGIAdapter)
{
    HRESULT hr = S_OK;

    // Driver types supported
    D3D_DRIVER_TYPE DriverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

    // Feature levels supported
    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

    D3D_FEATURE_LEVEL FeatureLevel;

    // Create device
    hr = D3D11CreateDevice(DXGIAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, FeatureLevels, NumFeatureLevels, D3D11_SDK_VERSION, &Data.Device, &FeatureLevel, &Data.Context);

    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to create device for thread", L"Desktop+ Error", hr);
    }

    // VERTEX shader
    UINT Size = ARRAYSIZE(g_VS);
    hr = Data.Device->CreateVertexShader(g_VS, Size, nullptr, &Data.VertexShader);
    if (FAILED(hr))
    {
        return ProcessFailure(Data.Device.Get(), L"Failed to create vertex shader for thread", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    // Input layout
    D3D11_INPUT_ELEMENT_DESC Layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    UINT NumElements = ARRAYSIZE(Layout);
    hr = Data.Device->CreateInputLayout(Layout, NumElements, g_VS, Size, &Data.InputLayout);
    if (FAILED(hr))
    {
        return ProcessFailure(Data.Device.Get(), L"Failed to create input layout for thread", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }
    Data.Context->IASetInputLayout(Data.InputLayout.Get());

    // Pixel shader
    Size = ARRAYSIZE(g_PS);
    hr = Data.Device->CreatePixelShader(g_PS, Size, nullptr, &Data.PixelShader);
    if (FAILED(hr))
    {
        return ProcessFailure(Data.Device.Get(), L"Failed to create pixel shader for thread", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    // Set up sampler
    D3D11_SAMPLER_DESC SampDesc;
    RtlZeroMemory(&SampDesc, sizeof(SampDesc));
    SampDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SampDesc.MinLOD         = 0;
    SampDesc.MaxLOD         = D3D11_FLOAT32_MAX;
    hr = Data.Device->CreateSamplerState(&SampDesc, &Data.Sampler);
    if (FAILED(hr))
    {
        return ProcessFailure(Data.Device.Get(), L"Failed to create sampler state for thread", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    return ddp_dupl_return_success;
}

//
// Getter for the DDPPtrInfo structure
//
DDPPtrInfo& DDPThreadManager::GetPointerInfo()
{
    return m_PtrInfo;
}

DPRect& DDPThreadManager::GetDirtyRegionTotal()
{
    return m_DirtyRegionTotal;
}

//
// Waits infinitely for all spawned threads to terminate
//
void DDPThreadManager::WaitForThreadTermination()
{
    if (m_ThreadHandles.size() != 0)
    {
        WaitForMultipleObjectsEx(m_ThreadHandles.size(), m_ThreadHandles.data(), TRUE, INFINITE, FALSE);
    }
}
