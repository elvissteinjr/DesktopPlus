#ifndef DPLUSWINRT_STUB

#include "CommonHeaders.h"
#include "CaptureManager.h"

#include "DesktopPlusWinRT.h"

namespace winrt
{
    using namespace Windows::Storage;
    using namespace Windows::System;
    using namespace Windows::Foundation;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
    using namespace Windows::UI::Popups;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
}

namespace util
{
    using namespace desktop;
}

CaptureManager::CaptureManager(DPWinRTThreadData& thread_data, DWORD global_main_thread_id) : m_ThreadData(thread_data)
{
    m_CaptureMainThread = winrt::DispatcherQueue::GetForCurrentThread();
    m_GlobalMainThreadID = global_main_thread_id;
    WINRT_VERIFY(m_CaptureMainThread != nullptr);

    //Get the adapter recommended by OpenVR
    winrt::com_ptr<ID3D11Device> d3d_device;
    winrt::com_ptr<IDXGIFactory1> factory_ptr;
    winrt::com_ptr<IDXGIAdapter> adapter_ptr_vr;
    int32_t vr_gpu_id;
    vr::VRSystem()->GetDXGIOutputInfo(&vr_gpu_id);  

    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), factory_ptr.put_void());
    if (!FAILED(hr))
    {
        winrt::com_ptr<IDXGIAdapter> adapter_ptr;
        UINT i = 0;

        while (factory_ptr->EnumAdapters(i, adapter_ptr.put()) != DXGI_ERROR_NOT_FOUND)
        {
            if (i == vr_gpu_id)
            {
                adapter_ptr_vr = adapter_ptr;
                break;
            }

            adapter_ptr = nullptr;
            ++i;
        }
    }

    if (adapter_ptr_vr != nullptr)
    {
        hr = D3D11CreateDevice(adapter_ptr_vr.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, d3d_device.put(), nullptr, nullptr);
    }
    
    if (d3d_device == nullptr)   //Try something else, but it probably won't work either
    {
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, d3d_device.put(), nullptr, nullptr);
        if (FAILED(hr))
        {
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, d3d_device.put(), nullptr, nullptr);
        }
    }

    //Get it as WinRT D3D11 device
    auto dxgi_device = d3d_device.try_as<IDXGIDevice>();
    m_Device = CreateDirect3DDevice(dxgi_device.get());
}

winrt::GraphicsCaptureItem CaptureManager::StartCaptureFromWindowHandle(HWND hwnd)
{
    auto item = util::CreateCaptureItemForWindow(hwnd);
    StartCaptureFromItem(item);
    return item;
}

winrt::GraphicsCaptureItem CaptureManager::StartCaptureFromMonitorHandle(HMONITOR hmon)
{
    auto item = util::CreateCaptureItemForMonitor(hmon);
    StartCaptureFromItem(item);
    return item;
}

void CaptureManager::StartCaptureFromItem(winrt::GraphicsCaptureItem item)
{
    m_Capture = std::make_unique<OverlayCapture>(m_Device, item, m_PixelFormat, m_GlobalMainThreadID, m_ThreadData.Overlays, m_ThreadData.SourceWindow);

    m_Capture->StartCapture();
    m_ItemClosedRevoker = item.Closed(winrt::auto_revoke, { this, &CaptureManager::OnCaptureItemClosed });

    //Check if all overlays of this capture are already paused and pause the capture as well then
    bool all_paused = true;
    for (DPWinRTOverlayData& overlay_data : m_ThreadData.Overlays)
    {
        if (!overlay_data.IsPaused)
        {
            all_paused = false;
            break;
        }
    }

    if (all_paused)
    {
        m_Capture->PauseCapture(true);
    }
}

void CaptureManager::OnCaptureItemClosed(winrt::GraphicsCaptureItem const&, winrt::Windows::Foundation::IInspectable const&)
{
    StopCapture();

    //Send overlay status updates
    for (const auto& overlay : m_ThreadData.Overlays)
    {
        ::PostThreadMessage(m_GlobalMainThreadID, WM_DPLUSWINRT_CAPTURE_LOST, overlay.Handle, 0);
    }
}

bool CaptureManager::IsCursorEnabled()
{
    if (m_Capture != nullptr)
    {
        return m_Capture->IsCursorEnabled();
    }
    return false;
}

void CaptureManager::IsCursorEnabled(bool value)
{
    if (m_Capture != nullptr)
    {
        m_Capture->IsCursorEnabled(value);
    }
}

bool CaptureManager::IsCapturePaused()
{
    return ( (m_Capture) && (m_Capture->IsPaused()) );
}

void CaptureManager::OnOverlayDataRefresh()
{
    if (m_Capture)
    {
        m_Capture->OnOverlayDataRefresh();
    }
}

void CaptureManager::PauseCapture(bool pause)
{
    if (m_Capture)
    {
        m_Capture->PauseCapture(pause);
    }
}

void CaptureManager::StopCapture()
{
    if (m_Capture)
    {
        m_Capture = nullptr;
    }
}

void CaptureManager::PixelFormat(winrt::DirectXPixelFormat pixel_format)
{
    m_PixelFormat = pixel_format;

    if (m_Capture)
    {
        auto item = m_Capture->CaptureItem();
        bool is_cursor_enabled = m_Capture->IsCursorEnabled();

        StopCapture();
        StartCaptureFromItem(item);

        if (!is_cursor_enabled)
        {
            m_Capture->IsCursorEnabled(is_cursor_enabled);
        }
    }
}

#endif //DPLUSWINRT_STUB