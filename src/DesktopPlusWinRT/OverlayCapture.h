#pragma once

#include <mutex>

#include "ThreadData.h"
#include "OUtoSBSConverter.h"

class OverlayCapture
{
public:
    OverlayCapture(winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device, winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
                  winrt::Windows::Graphics::DirectX::DirectXPixelFormat pixel_format, DWORD global_main_thread_id, const std::vector<DPWinRTOverlayData>& overlays, HWND source_window);
    ~OverlayCapture() { Close(); }

    void StartCapture();
    void RestartCapture();

    bool IsCursorEnabled()                                               { return m_CursorEnabled; }
	void IsCursorEnabled(bool value);
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureItem() { return m_Item; }

    void PauseCapture(bool pause)  { m_Paused = pause; OnOverlayDataRefresh(); }
    bool IsPaused()                { return m_Paused; }

    void OnOverlayDataRefresh();

    void Close();

private:
    void OnFrameArrived(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args);

    inline void CheckClosed()
    {
        if (m_Closed.load() == true)
        {
            throw winrt::hresult_error(RO_E_CLOSED);
        }
    }

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_Item { nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_FramePool { nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_Session { nullptr };
    winrt::Windows::Graphics::SizeInt32 m_LastContentSize { 0, 0 };

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_Device { nullptr };
    winrt::com_ptr<ID3D11DeviceContext> m_D3DContext { nullptr };
    winrt::Windows::Graphics::DirectX::DirectXPixelFormat m_PixelFormat;

    std::atomic<bool> m_Closed = false;

    //Below are only accessed while on the capture's main thread
    const std::vector<DPWinRTOverlayData>& m_Overlays;
    const HWND m_SourceWindow;
    DWORD m_GlobalMainThreadID = 0;

    bool m_Paused = false;
    bool m_CursorEnabled = true;            //Public cursor enabled state. False is always off, but true may be overridden by m_CursorEnabledInternal
    bool m_CursorEnabledInternal = true;    //Internal cursor enabled state, which may override m_CursorEnabled during window capture
    int m_OverlaySharedTextureSetupsNeeded = 2;

    bool m_InitialSizingDone = false;
    winrt::Windows::Graphics::SizeInt32 m_LastTextureSize { 0, 0 };
    bool m_RestartPending = false;

    LARGE_INTEGER m_UpdateLimiterStartingTime = {0, 0};
    LARGE_INTEGER m_UpdateLimiterFrequency = {0, 0};
    LARGE_INTEGER m_UpdateLimiterDelay = {0, 0};

    std::vector<OUtoSBSConverter> m_OUConverters; //Rarely used, so the cache is kept here instead of directly as part of the overlay data
};