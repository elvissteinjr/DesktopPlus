#include "SoftwareCursorGrabber.h"
#include "Logging.h"

bool SoftwareCursorGrabber::CopyMonoMask(const ICONINFO& icon_info)
{
    bool ret = false;

    HDC hdc = ::GetDC(nullptr);
    if (!hdc)
        return ret;

    //Get bitmap info from cursor bitmap
    BITMAPINFO bmp_info = {0};
    bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
    if (::GetDIBits(hdc, icon_info.hbmMask, 0, 0, nullptr, &bmp_info, DIB_RGB_COLORS) != 0)
    {
        int cursor_width  = bmp_info.bmiHeader.biWidth;
        int cursor_height = abs(bmp_info.bmiHeader.biHeight);

        bmp_info.bmiHeader.biSize        = sizeof(bmp_info.bmiHeader);
        bmp_info.bmiHeader.biBitCount    = 1;
        bmp_info.bmiHeader.biCompression = BI_RGB;
        bmp_info.bmiHeader.biHeight      = -cursor_height; //Always use top-down order (negative height)

        //Add monochrome palette (and deal with the dynamic struct size)
        RGBQUAD palette[2] = {{0, 0, 0, 0}, {255, 255, 255, 255}};

        auto bmp_info_extended_data = std::unique_ptr<BYTE[]>{new BYTE[sizeof(bmp_info.bmiHeader) + sizeof(palette)]};
        BITMAPINFO* bmp_info_extended_ptr = (BITMAPINFO*)bmp_info_extended_data.get();
        memcpy(&bmp_info_extended_ptr->bmiHeader, &bmp_info.bmiHeader, sizeof(bmp_info.bmiHeader));
        memcpy(bmp_info_extended_ptr->bmiColors,   palette,            sizeof(palette));

        //Read the actual bitmap buffer into a temporary pixel data array (4-byte aligned)
        const int padded_stride = ((cursor_width + 31) / 32) * 4;
        auto temp_buffer = std::unique_ptr<BYTE[]>{new BYTE[padded_stride * cursor_height]};
        if (::GetDIBits(hdc, icon_info.hbmMask, 0, cursor_height, temp_buffer.get(), bmp_info_extended_ptr, DIB_RGB_COLORS) != 0)
        {
            //Remove padding returned by GetDIBits so we end up with something that matches what Desktop Duplication returns
            size_t used_bytes_per_scanline = ((size_t)cursor_width + 7) / 8;
            m_DDPPointerInfo.ShapeBuffer.assign(used_bytes_per_scanline * cursor_height, 0);

            for (int y = 0; y < cursor_height; ++y) 
            {
                BYTE* src_line = temp_buffer.get()                   + y * padded_stride;
                BYTE* dst_line = m_DDPPointerInfo.ShapeBuffer.data() + y * used_bytes_per_scanline;

                memcpy(dst_line, src_line, used_bytes_per_scanline);
            }

            m_DDPPointerInfo.ShapeInfo.Width  = cursor_width;
            m_DDPPointerInfo.ShapeInfo.Height = cursor_height;
            m_DDPPointerInfo.ShapeInfo.Pitch  = 4;

            ret = true;
        }
    }

    ::ReleaseDC(nullptr, hdc);
    return ret;
}

bool SoftwareCursorGrabber::CopyColor(const ICONINFO& icon_info)
{
    bool ret = false;

    HDC hdc = ::GetDC(nullptr);
    if (!hdc)
        return ret;

    BITMAPINFO bmp_info = {0};
    bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
    if (::GetDIBits(hdc, icon_info.hbmColor, 0, 0, nullptr, &bmp_info, DIB_RGB_COLORS) != 0)
    {
        int cursor_width  = bmp_info.bmiHeader.biWidth;
        int cursor_height = abs(bmp_info.bmiHeader.biHeight);

        bmp_info.bmiHeader.biSize        = sizeof(bmp_info.bmiHeader);
        bmp_info.bmiHeader.biBitCount    = 32;
        bmp_info.bmiHeader.biCompression = BI_RGB;
        bmp_info.bmiHeader.biHeight      = -cursor_height; //Always use top-down order (negative height)

        m_DDPPointerInfo.ShapeBuffer.resize((size_t)cursor_width * cursor_height * 4);

        if (::GetDIBits(hdc, icon_info.hbmColor, 0, cursor_height, m_DDPPointerInfo.ShapeBuffer.data(), &bmp_info, DIB_RGB_COLORS) != 0)
        {
            //Even if we don't override biBitCount to 32, it's still returned as that for 24-bit and lower bit-depth cursors (probably just the screen DC format)
            //It seems the only way to check if the cursor needs its mask applied is to see if the alpha channel is fully blank
            //32-bit cursors still come with masks, but applying them means to override the alpha channel with a 1-bit one (and doing so is also wasteful)
            const bool needs_mask = IsShapeBufferBlank(m_DDPPointerInfo.ShapeBuffer.data(), m_DDPPointerInfo.ShapeBuffer.size(), DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR);
            if (needs_mask)
            {
                auto pixel_data_mask = std::unique_ptr<BYTE[]>{new BYTE[m_DDPPointerInfo.ShapeBuffer.size()]};

                //Read the mask bitmap buffer
                if (::GetDIBits(hdc, icon_info.hbmMask, 0, cursor_height, (LPVOID)pixel_data_mask.get(), &bmp_info, DIB_RGB_COLORS) != 0)
                {
                    //Apply mask to color pixel data
                    BYTE* psrc = m_DDPPointerInfo.ShapeBuffer.data() + 3;  //BGRA alpha pixel
                    BYTE* pmsk = pixel_data_mask.get();                    //BGRA blue pixel (alpha channel is blank for the mask)
                    const BYTE* const psrc_end = m_DDPPointerInfo.ShapeBuffer.data() + m_DDPPointerInfo.ShapeBuffer.size();
                    for (; psrc < psrc_end; psrc += 4, pmsk += 4)
                    {
                        *psrc = ~(*pmsk);
                    }
                }
            }

            m_DDPPointerInfo.ShapeInfo.Width  = cursor_width;
            m_DDPPointerInfo.ShapeInfo.Height = cursor_height;
            m_DDPPointerInfo.ShapeInfo.Pitch  = cursor_width * 4;

            ret = true;
        }
    }

    ::ReleaseDC(nullptr, hdc);
    return ret;
}

bool SoftwareCursorGrabber::IsShapeBufferBlank(BYTE* psrc, size_t size, UINT type)
{
    if (type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR)
    {
        const BYTE* const psrc_end = psrc + size;
        psrc = psrc + 3; //first BGRA alpha pixel
        for (; psrc < psrc_end; psrc += 4)
        {
            if (*psrc != 0)
            {
                return false;
            }
        }
    }
    else if (type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME)
    {
        const BYTE* const psrc_end = psrc + size;
        psrc = psrc + (size / 2); //first mask pixel
        for (; psrc < psrc_end; ++psrc)
        {
            if (*psrc != 0)
            {
                return false;
            }
        }
    }

    return true;
}

bool SoftwareCursorGrabber::SynthesizeDDPCursorInfo()
{
    LOG_IF_F(INFO, !m_LoggedOnceUsed, "Using Alternative Cursor Rendering");
    m_LoggedOnceUsed = true;

    CURSORINFO cursor_info = {0};
    cursor_info.cbSize = sizeof(CURSORINFO);

    bool ret = false;

    if (::GetCursorInfo(&cursor_info))
    {
        const bool shape_changed = (cursor_info.hCursor != m_CursorInfoLast.hCursor);
        if (shape_changed)
        {
            if (cursor_info.hCursor != nullptr)
            {
                ICONINFO icon_info = {};
                if (::GetIconInfo(cursor_info.hCursor, &icon_info))
                {
                    ret = (icon_info.hbmColor != nullptr) ? CopyColor(icon_info) : CopyMonoMask(icon_info);
                    m_DDPPointerInfo.ShapeInfo.Type = (icon_info.hbmColor != nullptr) ? DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR : DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME;

                    ::DeleteObject(icon_info.hbmColor);
                    ::DeleteObject(icon_info.hbmMask);
                }

                //Fallback: If getting the icon info failed or "succeeded" with blank data, try copying the default arrow pointer instead
                if ((!ret) || (IsShapeBufferBlank(m_DDPPointerInfo.ShapeBuffer.data(), m_DDPPointerInfo.ShapeBuffer.size(), m_DDPPointerInfo.ShapeInfo.Type)))
                {
                    LOG_IF_F(WARNING, !m_LoggedOnceFallbackDefault, "Alternative Cursor Rendering: Getting current cursor failed. Falling back to default cursor.");
                    m_LoggedOnceFallbackDefault = true;

                    ret = false;

                    if (::GetIconInfo(::LoadCursor(nullptr, IDC_ARROW), &icon_info))
                    {
                        ret = (icon_info.hbmColor != nullptr) ? CopyColor(icon_info) : CopyMonoMask(icon_info);
                        m_DDPPointerInfo.ShapeInfo.Type = (icon_info.hbmColor != nullptr) ? DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR : DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME;

                        //Check if somehow still blank
                        if (ret)
                        {
                            ret = !IsShapeBufferBlank(m_DDPPointerInfo.ShapeBuffer.data(), m_DDPPointerInfo.ShapeBuffer.size(), m_DDPPointerInfo.ShapeInfo.Type);
                        }

                        ::DeleteObject(icon_info.hbmColor);
                        ::DeleteObject(icon_info.hbmMask);
                    }
                }

                //Set other data if cursor copy was successful
                if (ret)
                {
                    m_DDPPointerInfo.CursorShapeChanged = true;
                    m_DDPPointerInfo.ShapeInfo.HotSpot.x = icon_info.xHotspot;
                    m_DDPPointerInfo.ShapeInfo.HotSpot.y = icon_info.yHotspot;
                }
                else
                {
                    LOG_IF_F(WARNING, !m_LoggedOnceFallbackBlob, "Alternative Cursor Rendering: Getting default cursor failed. Falling back to simple blob.");
                    m_LoggedOnceFallbackBlob = true;

                    //Everything else failed, put a blob there
                    m_DDPPointerInfo.ShapeInfo.Width  = 12;
                    m_DDPPointerInfo.ShapeInfo.Height = 19 * 2; //Double height for mask
                    m_DDPPointerInfo.ShapeBuffer.assign((size_t)m_DDPPointerInfo.ShapeInfo.Width * m_DDPPointerInfo.ShapeInfo.Height, 255);

                    m_DDPPointerInfo.CursorShapeChanged = true;
                    m_DDPPointerInfo.ShapeInfo.Type  = DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME;
                    m_DDPPointerInfo.ShapeInfo.Pitch = 4;
                    m_DDPPointerInfo.ShapeInfo.HotSpot.x = 0;
                    m_DDPPointerInfo.ShapeInfo.HotSpot.y = 0;

                    ret = true;
                }
            }
            else
            {
                //Create blank 1x1 pixel data for null cursor handle
                m_DDPPointerInfo.ShapeBuffer.assign(4, 0);
                m_DDPPointerInfo.ShapeInfo.Width  = 1;
                m_DDPPointerInfo.ShapeInfo.Height = 1;

                ret = true;
            }
        }
        else
        {
            ret = true;
        }

        if (ret)
        {
            m_DDPPointerInfo.Visible = (cursor_info.flags == CURSOR_SHOWING);
            m_DDPPointerInfo.Position = cursor_info.ptScreenPos;
            m_DDPPointerInfo.Position.x -= m_DDPPointerInfo.ShapeInfo.HotSpot.x;
            m_DDPPointerInfo.Position.y -= m_DDPPointerInfo.ShapeInfo.HotSpot.y;
            QueryPerformanceCounter(&m_DDPPointerInfo.LastTimeStamp);
        }

        m_CursorInfoLast = cursor_info;
    }

    return ret;
}

PTR_INFO& SoftwareCursorGrabber::GetDDPCursorInfo()
{
    return m_DDPPointerInfo;
}
