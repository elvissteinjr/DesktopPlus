#pragma once
#include "OverlayCapture.h"

class CaptureManager
{
    public:
        CaptureManager(DPWinRTThreadData& thread_data, DWORD global_main_thread_id);
        ~CaptureManager() {}

        winrt::Windows::Graphics::Capture::GraphicsCaptureItem StartCaptureFromWindowHandle(HWND hwnd);
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem StartCaptureFromMonitorHandle(HMONITOR hmon);
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Graphics::Capture::GraphicsCaptureItem> StartCaptureWithPickerAsync();
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat PixelFormat() { return m_PixelFormat; }
        void PixelFormat(winrt::Windows::Graphics::DirectX::DirectXPixelFormat pixel_format);

        bool IsCursorEnabled();
        void IsCursorEnabled(bool value);
        bool IsCapturePaused();

        void OnOverlayDataRefresh();

        void PauseCapture(bool pause);
        void StopCapture();

    private:
        void StartCaptureFromItem(winrt::Windows::Graphics::Capture::GraphicsCaptureItem item);
        void OnCaptureItemClosed(winrt::Windows::Graphics::Capture::GraphicsCaptureItem const&, winrt::Windows::Foundation::IInspectable const&);
        static HWND FindWindowFromCaptureItem(winrt::Windows::Graphics::Capture::GraphicsCaptureItem item, int& desktop_id);

    private:
        winrt::Windows::System::DispatcherQueue m_CaptureMainThread{ nullptr };

        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_Device { nullptr };
        std::unique_ptr<OverlayCapture> m_Capture { nullptr };
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem::Closed_revoker m_ItemClosedRevoker;
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat m_PixelFormat = winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized;

        DPWinRTThreadData& m_ThreadData;
        DWORD m_GlobalMainThreadID;
};