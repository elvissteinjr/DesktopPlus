#ifndef DPLUSWINRT_STUB

#include "CommonHeaders.h"
#include "OverlayCapture.h"

#include "DesktopPlusWinRT.h"
#include "Util.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::System;
    using namespace Windows::Graphics;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
    using namespace Windows::Foundation::Numerics;
}

OverlayCapture::OverlayCapture(winrt::IDirect3DDevice const& device, winrt::GraphicsCaptureItem const& item, winrt::DirectXPixelFormat pixel_format, DWORD global_main_thread_id,
                             const std::vector<DPWinRTOverlayData>& overlays, HWND source_window) :
    m_Overlays(overlays),
    m_SourceWindow(source_window)
{
    m_Item = item;
    m_Device = device;
    m_PixelFormat = pixel_format;
    m_GlobalMainThreadID = global_main_thread_id;

    auto d3d_device = GetDXGIInterfaceFromObject<ID3D11Device>(m_Device);
    d3d_device->GetImmediateContext(m_D3DContext.put());

    // Creating our frame pool with 'Create' instead of 'CreateFreeThreaded'
    // means that the frame pool's FrameArrived event is called on the thread
    // the frame pool was created on. This also means that the creating thread
    // must have a DispatcherQueue. If you use this method, it's best not to do
    // it on the UI thread. 
    m_FramePool = winrt::Direct3D11CaptureFramePool::Create(m_Device, m_PixelFormat, 2, m_Item.Size());
    m_LastContentSize = m_Item.Size();
    m_Session = m_FramePool.CreateCaptureSession(m_Item);
    m_FramePool.FrameArrived({ this, &OverlayCapture::OnFrameArrived });

    //Set mouse scale on all overlays and send size updates to default them to -1 until we get the real size on the first frame update
    vr::HmdVector2_t mouse_scale = {(float)m_Item.Size().Width, (float)m_Item.Size().Height};
    for (const auto& overlay : m_Overlays)
    {
        ::PostThreadMessage(m_GlobalMainThreadID, WM_DPLUSWINRT_SIZE, overlay.Handle, MAKELPARAM(-1, -1));

        vr::VROverlay()->SetOverlayMouseScale(overlay.Handle, &mouse_scale);
    }

    //Init update limiter variables
    ::QueryPerformanceFrequency(&m_UpdateLimiterFrequency);
    ::QueryPerformanceCounter(&m_UpdateLimiterStartingTime);

    OnOverlayDataRefresh();

    WINRT_ASSERT(m_Session != nullptr);
}

void OverlayCapture::StartCapture()
{
    CheckClosed();
    m_Session.StartCapture();
}

void OverlayCapture::RestartCapture()
{
    m_Session.Close();
    m_FramePool.Close();

    m_FramePool = nullptr;
    m_Session = nullptr;

    m_FramePool = m_FramePool.Create(m_Device, m_PixelFormat, 2, m_LastContentSize);
    m_Session = m_FramePool.CreateCaptureSession(m_Item);
    m_FramePool.FrameArrived({this, &OverlayCapture::OnFrameArrived});

    m_Session.StartCapture();

    m_CursorEnabledInternal = true;
}

void OverlayCapture::IsCursorEnabled(bool value)
{
    CheckClosed();
    m_CursorEnabled = value;

    //Only directly set it when it's either turning off or it's not a window capture (not auto-switching cursor state)
    if ( (!m_CursorEnabled) || (m_SourceWindow == nullptr) )
    {
        m_Session.IsCursorCaptureEnabled(m_CursorEnabled);
        m_CursorEnabledInternal = m_CursorEnabled;
    }
}

void OverlayCapture::OnOverlayDataRefresh()
{
    //Find the smallest update limiter delay and count Over-Under overlays
    size_t ou_count = 0;
    m_UpdateLimiterDelay.QuadPart = UINT_MAX;
    vr::HmdVector2_t mouse_scale = {(float)m_LastTextureSize.Width, (float)m_LastTextureSize.Height};

    for (const auto& overlay : m_Overlays)
    {
        if (!overlay.IsPaused)
        {
            if (overlay.UpdateLimiterDelay.QuadPart < m_UpdateLimiterDelay.QuadPart)
            {
                m_UpdateLimiterDelay = overlay.UpdateLimiterDelay;
            }
        }

        if (overlay.IsOverUnder3D)
        {
            ou_count++;
        }

        //And also send size and set mouse scale again in case a fresh overlay was added
        if (m_InitialSizingDone)
        {
            ::PostThreadMessage(m_GlobalMainThreadID, WM_DPLUSWINRT_SIZE, overlay.Handle, MAKELPARAM(m_LastTextureSize.Width, m_LastTextureSize.Height));
            vr::VROverlay()->SetOverlayMouseScale(overlay.Handle, &mouse_scale);
        }
    }

    //Adjust OUConverter cache size as needed
    m_OUConverters.resize(ou_count);

    //Make sure the shared textures are set up again on the next update
    m_OverlaySharedTextureSetupsNeeded = 2;
}

void OverlayCapture::Close()
{
    auto expected = false;
    if (m_Closed.compare_exchange_strong(expected, true))
    {
        m_Session.Close();

        //Wait for GraphicsCapture.dll thread to finish up
        //
        //When multiple captures are active and one stops, a fail-fast crash can occur sometimes.
        //Always in internal frame pool cleanup code, as if there was a race condition somewhere...
        //This may be a bug in Graphics Capture and seems only to happen on Windows 10 1809
        //Waiting it out works and this thread is only cleaning up so it doesn't really matter if we sleep a bit before doing so... eh
        Sleep(500); //A few ms is actually enough, but do 500 just to be safe

        m_FramePool.Close();

        m_FramePool = nullptr;
        m_Session   = nullptr;
        m_Item      = nullptr;
    }
}

void OverlayCapture::OnFrameArrived(winrt::Direct3D11CaptureFramePool const& sender, winrt::IInspectable const&)
{
    auto frame = sender.TryGetNextFrame();

    if ( (m_Paused) || (frame == nullptr) )
        return;

    //Update limiter/skipper
    bool update_limiter_active = (m_UpdateLimiterDelay.QuadPart != 0);
    
    if (update_limiter_active)
    {
        LARGE_INTEGER UpdateLimiterEndingTime, UpdateLimiterElapsedMicroseconds;

        QueryPerformanceCounter(&UpdateLimiterEndingTime);
        UpdateLimiterElapsedMicroseconds.QuadPart = UpdateLimiterEndingTime.QuadPart - m_UpdateLimiterStartingTime.QuadPart;

        UpdateLimiterElapsedMicroseconds.QuadPart *= 1000000;
        UpdateLimiterElapsedMicroseconds.QuadPart /= m_UpdateLimiterFrequency.QuadPart;

        if (UpdateLimiterElapsedMicroseconds.QuadPart < m_UpdateLimiterDelay.QuadPart)
            return; //Skip frame
    }

    bool recreate_frame_pool = false;

    //Scope surface texture to release it earlier
    {
        auto surface_texture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        auto const frame_content_size = frame.ContentSize();
        auto d3d_device = GetDXGIInterfaceFromObject<ID3D11Device>(m_Device);

        //Recreate frame pool with new content size (not current texture size) if needed
        if ((frame_content_size.Width != m_LastContentSize.Width) || (frame_content_size.Height != m_LastContentSize.Height))
        {
            m_LastContentSize = frame_content_size;
            recreate_frame_pool = true; //Recreate frame pool after we're done with the frame

            m_OverlaySharedTextureSetupsNeeded = 2;
        }

        //Direct3D11CaptureFrame::ContentSize isn't always matching the size of the texture (lagging behind if content resized but frame pool hasn't yet)
        //We'll grab the values from the texture itself instead.
        D3D11_TEXTURE2D_DESC texture_desc;
        surface_texture->GetDesc(&texture_desc);

        //Check if size of the frame texture changed
        if ((texture_desc.Width != m_LastTextureSize.Width) || (texture_desc.Height != m_LastTextureSize.Height))
        {
            m_LastTextureSize.Width = texture_desc.Width;
            m_LastTextureSize.Height = texture_desc.Height;

            m_OverlaySharedTextureSetupsNeeded = 2;

            //Send overlay size updates and set mouse scale
            //If the initial sizing has not been done yet, wait until there's no frame pool recreation pending before setting it
            //We do this because windows with native decorations are initially just reported with the client size.
            //The real size follows on the next frame after having resized the frame pool
            //This is necessary to not trip up adaptive overlay sizing
            if ((m_InitialSizingDone) || (!recreate_frame_pool))
            {
                vr::HmdVector2_t mouse_scale = {(float)texture_desc.Width, (float)texture_desc.Height};
                for (const auto& overlay : m_Overlays)
                {
                    ::PostThreadMessage(m_GlobalMainThreadID, WM_DPLUSWINRT_SIZE, overlay.Handle, MAKELPARAM(texture_desc.Width, texture_desc.Height));

                    vr::VROverlay()->SetOverlayMouseScale(overlay.Handle, &mouse_scale);
                }

                m_InitialSizingDone = true;
            }
        }

        //Set overlay textures
        vr::Texture_t vrtex;
        vrtex.eType = vr::TextureType_DirectX;
        vrtex.eColorSpace = vr::ColorSpace_Gamma;
        vrtex.handle = surface_texture.get();

        size_t ou_count = 0;
        vr::VROverlayHandle_t ovrl_shared_source = vr::k_ulOverlayHandleInvalid;
        for (const auto& overlay : m_Overlays)
        {
            if (overlay.IsOverUnder3D)
            {
                if (ou_count < m_OUConverters.size())
                {
                    HRESULT hr = m_OUConverters[ou_count].Convert(d3d_device.get(), m_D3DContext.get(), nullptr, nullptr, surface_texture.get(), texture_desc.Width, texture_desc.Height,
                        overlay.OU3D_crop_x, overlay.OU3D_crop_y, overlay.OU3D_crop_width, overlay.OU3D_crop_height);

                    if (hr == S_OK)
                    {
                        vr::Texture_t vrtex_ou;
                        vrtex_ou.eType = vr::TextureType_DirectX;
                        vrtex_ou.eColorSpace = vr::ColorSpace_Gamma;
                        vrtex_ou.handle = m_OUConverters[ou_count].GetTexture();

                        vr::VROverlay()->SetOverlayTexture(overlay.Handle, &vrtex_ou);
                    }

                    ou_count++;
                }
            }
            else if (ovrl_shared_source == vr::k_ulOverlayHandleInvalid) //For the first non-OU3D overlay, set the texture as normal
            {
                vr::VROverlay()->SetOverlayTexture(overlay.Handle, &vrtex);
                ovrl_shared_source = overlay.Handle;
            }
            else if (m_OverlaySharedTextureSetupsNeeded > 0) //For all others, set it shared from the normal overlay if an update is needed
            {
                SetSharedOverlayTexture(ovrl_shared_source, overlay.Handle, surface_texture.get());
            }
        }
    }

    //Release frame early
    frame = nullptr;

    //Due to a bug in Graphics Capture, the capture itself can get offset permanently on the texture when window borders change
    //We combat this by checking if the texture size matches the DWM frame bounds and schedule a restart of the capture when that's case
    //There are sometimes a few false positives on size change, but nothing terrible
    if ( (m_RestartPending) && (m_OverlaySharedTextureSetupsNeeded == 0) )
    {
        RestartCapture();
        m_RestartPending = false;
        recreate_frame_pool = false;
    }

    //As noted in OverlayCapture::Close(), there's a bug with closing the capture on 1809
    //Except sleeping doesn't help when restarting the capture, so we don't even try on versions older than 1903 (where capture from handle was added)
    if ( (DPWinRT_IsCaptureFromHandleSupported()) && (m_SourceWindow != nullptr) && (m_OverlaySharedTextureSetupsNeeded == 0) )
    {
        RECT window_rect = {0};
        if (::DwmGetWindowAttribute(m_SourceWindow, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect, sizeof(window_rect)) == S_OK)
        {
            int dwm_w = window_rect.right  - window_rect.left;
            int dwm_h = window_rect.bottom - window_rect.top;

            if ((m_LastTextureSize.Width != dwm_w) || (m_LastTextureSize.Height != dwm_h))
            {
                m_RestartPending = true;
            }
        }
    }

    //We can only be sure about not needing to set the shared overlay texture after doing it at least twice after resize. Not entirely sure why, but it works.
    if (m_OverlaySharedTextureSetupsNeeded > 0)
    {
        m_OverlaySharedTextureSetupsNeeded--;
    }

    //Hide cursor from capture if the window is not in front as it just adds more confusion when it's there
    if ( (m_CursorEnabled) && (m_SourceWindow != nullptr) && (DPWinRT_IsCaptureCursorEnabledPropertySupported()) )
    {
        bool should_enable_cursor = (m_SourceWindow != ::GetForegroundWindow());

        if (m_CursorEnabledInternal != should_enable_cursor)
        {
            m_Session.IsCursorCaptureEnabled(should_enable_cursor);
            m_CursorEnabledInternal = should_enable_cursor;
        }
    }

    //Recreate frame pool if it was scheduled earlier
    if (recreate_frame_pool)
    {
        m_FramePool.Recreate(m_Device, m_PixelFormat, 2, m_LastContentSize);
    }

    //Set frame limiter starting time after we're done with everything
    if (update_limiter_active)
    {
        ::QueryPerformanceCounter(&m_UpdateLimiterStartingTime);
    }
}

#endif //DPLUSWINRT_STUB
