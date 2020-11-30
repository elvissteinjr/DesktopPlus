#include "TextureManager.h"

#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <vector>

//Make GDI+ header work with NOMINMAX
namespace Gdiplus
{
    using std::min;
    using std::max;
}

#include <gdiplus.h>
#include <gdipluspixelformats.h>

#include "ConfigManager.h"
#include "Util.h"
#include "UIManager.h"
#include "OverlayManager.h"
#include "imgui_impl_dx11_openvr.h"

const wchar_t* TextureManager::s_TextureFilenames[] =
{
    L"images/icons/desktop.png",
    L"images/icons/desktop_1.png",
    L"images/icons/desktop_2.png",
    L"images/icons/desktop_3.png",
    L"images/icons/desktop_4.png",
    L"images/icons/desktop_5.png",
    L"images/icons/desktop_6.png",
    L"images/icons/desktop_all.png",
    L"images/icons/desktop_next.png",
    L"images/icons/desktop_previous.png",
    L"images/icons/settings.png",
    L"images/icons/keyboard.png",
    L"images/icons/small_close.png",
    L"images/icons/small_move.png",
    L"images/icons/small_actionbar.png",
    L"",                                    //tmtex_icon_temp, blank
};

static TextureManager g_TextureManager;

TextureManager::TextureManager() : m_ReloadLater(false)
{
    std::fill(std::begin(m_ImGuiRectIDs), std::end(m_ImGuiRectIDs), -1);
    std::fill(std::begin(m_AtlasSizes), std::end(m_AtlasSizes), ImVec2(-1, -1));
    std::fill(std::begin(m_AtlasUVs), std::end(m_AtlasUVs), ImVec4(0, 0, 0, 0));
}

TextureManager& TextureManager::Get()
{
    return g_TextureManager;
}

bool TextureManager::LoadAllTexturesAndBuildFonts()
{
    bool all_ok = true;     //We don't need to abort when something fails, but let's not ignore it completely

    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();
    ImGui_ImplDX11_InvalidateDeviceObjects(); //I really feel like I shouldn't have to call a renderer-specific function to make reloading fonts work, but it seems necessary

    //Clear arrays
    std::fill(std::begin(m_ImGuiRectIDs), std::end(m_ImGuiRectIDs), -1);
    std::fill(std::begin(m_AtlasSizes), std::end(m_AtlasSizes), ImVec2(-1, -1));
    std::fill(std::begin(m_AtlasUVs), std::end(m_AtlasUVs), ImVec4(0, 0, 0, 0));

    //Prepare font range to add more characters from action names/properties when needed
    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;

    for (const CustomAction& action : ConfigManager::Get().GetCustomActions())
    {
        builder.AddText(action.Name.c_str());

        if (action.FunctionType != caction_press_keys)
        {
            builder.AddText(action.StrMain.c_str());
            builder.AddText(action.StrArg.c_str());
        }
    }

    for (const std::string& str : ConfigManager::Get().GetOverlayProfileList()) //Also from overlay profiles
    {
        builder.AddText(str.c_str());
    }

    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i) //And overlay names
    {
        builder.AddText(OverlayManager::Get().GetConfigData(i).ConfigNameStr.c_str());
    }

    for (const std::string& str : m_FontBuilderExtraStrings) //And extra strings... yeah. This might not be the best way to tackle this issue
    {
        builder.AddText(str.c_str());
    }

    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.BuildRanges(&ranges);

    ImFontConfig config_compact;
    ImFontConfig config_large;
    config_compact.MergeMode = true;
    config_large.MergeMode = true;
    ImFontConfig* config = &config_compact;

    //Try to load fonts
    ImFont* font = nullptr;
    ImFont* font_compact = nullptr;
    ImFont* font_large = nullptr;
    float font_base_size = 32.0f;
    bool load_large_font = ( (ConfigManager::Get().GetConfigBool(configid_bool_interface_large_style)) && (!UIManager::Get()->IsInDesktopMode()) );

    //Loop to do the same for the large font if needed
    for (;;)
    {
        //AddFontFromFileTTF asserts when failing to load, so check for existence, though it's not really an issue in release mode
        if (FileExists(L"C:\\Windows\\Fonts\\segoeui.ttf"))
        {
            font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", font_base_size * UIManager::Get()->GetUIScale(), nullptr, ranges.Data);
        }

        if (font != nullptr)
        {
            font->DisplayOffset.y = -1; //Set offset to make it not look so bad

            //Segoe UI doesn't have any CJK, use some fallbacks (loading this is actually pretty fast)
            if (FileExists(L"C:\\Windows\\Fonts\\msgothic.ttc"))
                io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msgothic.ttc", font_base_size * UIManager::Get()->GetUIScale(), config, ranges.Data);

            if (FileExists(L"C:\\Windows\\Fonts\\malgun.ttf"))
                io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", font_base_size * UIManager::Get()->GetUIScale(), config, ranges.Data);

            if (FileExists(L"C:\\Windows\\Fonts\\msyh.ttc"))
                io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", font_base_size * UIManager::Get()->GetUIScale(), config, ranges.Data);
            //Also add some symbol support at least... yeah this is far from comprehensive all in all but should cover most uses
            if (FileExists(L"C:\\Windows\\Fonts\\seguisym.ttf"))
                io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisym.ttf", font_base_size * UIManager::Get()->GetUIScale(), config, ranges.Data);
        }
        else
        {
            //Though we have the default as fallback if it isn't somehow
            font = io.Fonts->AddFontDefault();
        }

        if (font_compact == nullptr)
        {
            font_compact = font;
        }
        else
        {
            font_large = font;
        }

        if ( (load_large_font) && (font_large == nullptr) )
        {
            font_base_size *= 1.5f;
            config = &config_large;
        }
        else
        {
            break;
        }
    }

    //Initialize GDI+.
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
    {
        io.Fonts->Build();          //Still build the font so we can have text at least
        io.Fonts->ClearInputData(); //We don't need to keep this around, reduces RAM use a lot

        UIManager::Get()->SetFonts(font_compact, font_large);

        return false;       //Everything below will fail as well if this did
    }

    //Load images and add custom rects for them
    //Since we have to build the font before actually copying the image data, we have to keep the bitmaps loaded, which is a bit messy, especially with custom action icons
    std::vector< std::unique_ptr<Gdiplus::Bitmap> > bitmaps;
    //Load application icons
    int icon_id = 0;
    for (; icon_id < tmtex_MAX; ++icon_id)
    {
        std::unique_ptr<Gdiplus::Bitmap> bmp;
        
        if (icon_id == tmtex_icon_temp)
        {
            bmp = std::unique_ptr<Gdiplus::Bitmap>( new Gdiplus::Bitmap(m_TextureFilenameIconTemp.c_str()) );
        }
        else
        {
            bmp = std::unique_ptr<Gdiplus::Bitmap>( new Gdiplus::Bitmap(s_TextureFilenames[icon_id]) );
        }

        if (bmp->GetLastStatus() == Gdiplus::Ok)
        {
            //ID has to be above 0x110000 as below is reserved by ImGui
            m_ImGuiRectIDs[icon_id] = io.Fonts->AddCustomRectRegular(bmp->GetWidth(), bmp->GetHeight());

            if (io.Fonts->TexDesiredWidth <= (int)bmp->GetWidth())
            {
                //We could go smarter here, but let's be honest, we actually shouldn't load large images into the atlas in the first place!
                //But well, I tried out of curiosity once and the result was a disaster without these checks.
                //And yes, we need more space than the texture's width, unfortunately. Probably for that one white pixel in the atlas or something
                io.Fonts->TexDesiredWidth = (bmp->GetWidth() >= 2048) ? 4096 : (bmp->GetWidth() >= 1024) ? 2048 : (bmp->GetWidth() >= 512) ? 1024 : 512;
            }
        }

        bitmaps.push_back(std::move(bmp));
    }

    //Load custom action icons
    for (auto& action : ConfigManager::Get().GetCustomActions())
    {
        icon_id++;

        action.IconImGuiRectID = -1;
        if (!action.IconFilename.empty())
        {
            std::unique_ptr<Gdiplus::Bitmap> bmp( new Gdiplus::Bitmap(WStringConvertFromUTF8(action.IconFilename.c_str()).c_str()) );

            if (bmp->GetLastStatus() == Gdiplus::Ok)
            {
                //ID has to be above 0x11000 as below is reserved by ImGui
                action.IconImGuiRectID = io.Fonts->AddCustomRectRegular(bmp->GetWidth(), bmp->GetHeight());

                if (io.Fonts->TexDesiredWidth <= (int)bmp->GetWidth())
                {
                    //See above
                    io.Fonts->TexDesiredWidth = (bmp->GetWidth() >= 2048) ? 4096 : (bmp->GetWidth() >= 1024) ? 2048 : (bmp->GetWidth() >= 512) ? 1024 : 512;
                }
            }

            bitmaps.push_back(std::move(bmp));
        }
    }

    //Build atlas
    io.Fonts->Build();

    //Retrieve atlas texture in RGBA format
    unsigned char* tex_pixels = nullptr;
    int tex_width, tex_height;
    io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_width, &tex_height);

    //Actually do the copying now
    icon_id = 0;
    int action_id = 0;
    for (auto& bmp : bitmaps)
    {
        if (bmp->GetLastStatus() == Gdiplus::Ok)
        {
            int*    rect_id    = nullptr;
            ImVec2* atlas_size = nullptr;
            ImVec4* atlas_uvs  = nullptr;

            if (icon_id < tmtex_MAX)
            {
                rect_id    = &m_ImGuiRectIDs[icon_id];
                atlas_size = &m_AtlasSizes[icon_id];
                atlas_uvs  = &m_AtlasUVs[icon_id];
            }
            else
            {
                //Get the next action, skipping custom actions with no icon
                do
                {
                    if (action_id >= ConfigManager::Get().GetCustomActions().size())
                    {
                        break;
                    }

                    CustomAction& action = ConfigManager::Get().GetCustomActions()[action_id];
                    rect_id    = &action.IconImGuiRectID;
                    atlas_size = &action.IconAtlasSize;
                    atlas_uvs  = &action.IconAtlasUV;

                    action_id++;
                }
                while (*rect_id == -1);
            }
            
            if ( (rect_id != nullptr) && (*rect_id != -1) )
            {
                if (const ImFontAtlasCustomRect* rect = io.Fonts->GetCustomRectByIndex(*rect_id))
                {
                    Gdiplus::BitmapData bitmapData;
                    Gdiplus::Rect gdirect(0, 0, rect->Width, rect->Height);
                    if (bmp->LockBits(&gdirect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) == Gdiplus::Ok)  //Access bitmap data from GDI+
                    {
                        for (int y = 0; y < rect->Height; y++)
                        {
                            ImU32* p = (ImU32*)tex_pixels + (rect->Y + y) * tex_width + (rect->X);
                            UINT8* pgdi = (UINT8*)bitmapData.Scan0 + (y * bitmapData.Stride);

                            for (int x = 0; x < rect->Width; x++)
                            {
                                //GDI+ order is BGRA or it's just different endianess... either way, as simple memcpy doesn't cut it
                                *p++ = IM_COL32(*(pgdi + 2), *(pgdi + 1), *pgdi, *(pgdi + 3));
                                pgdi += 4;
                            }
                        }

                        bmp->UnlockBits(&bitmapData);

                        //Store UVs and size since we succeeded with copying
                        atlas_size->x = rect->Width;
                        atlas_size->y = rect->Height;

                        atlas_uvs->x = (float)rect->X * io.Fonts->TexUvScale.x;                  //Min U
                        atlas_uvs->y = (float)rect->Y * io.Fonts->TexUvScale.y;                  //Min V
                        atlas_uvs->z = (float)(rect->X + rect->Width)  * io.Fonts->TexUvScale.x; //Max U
                        atlas_uvs->w = (float)(rect->Y + rect->Height) * io.Fonts->TexUvScale.y; //Max V
                    }
                    else
                    {
                        *rect_id = -1;
                        all_ok = false;
                    }
                }
                else
                {
                    *rect_id = -1;
                    all_ok = false;
                }
            }
            else
            {
                all_ok = false;
            }
        }

        icon_id++;
    }

    //Delete Bitmaps before shutting down GDI+
    bitmaps.clear();

    //Shutdown GDI+, we won't need it again
    Gdiplus::GdiplusShutdown(gdiplusToken);
    m_ReloadLater = false;

    //We don't need to keep this around, reduces RAM use a lot
    io.Fonts->ClearInputData();

    UIManager::Get()->SetFonts(font_compact, font_large);

    return all_ok;
}

void TextureManager::ReloadAllTexturesLater()
{
    m_ReloadLater = true;
}

bool TextureManager::GetReloadLaterFlag()
{
    return m_ReloadLater;
}

const wchar_t* TextureManager::GetTextureFilename(TMNGRTexID texid) const
{
    return s_TextureFilenames[texid];
}

void TextureManager::SetTextureFilenameIconTemp(const wchar_t* filename)
{
    m_TextureFilenameIconTemp = filename;
}

bool TextureManager::GetTextureInfo(TMNGRTexID texid, ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const
{
    int rect_id = m_ImGuiRectIDs[texid];
    if (rect_id != -1)
    {
        size = m_AtlasSizes[texid];
        //Also set cached UV coordinates
        uv_min.x = m_AtlasUVs[texid].x;
        uv_min.y = m_AtlasUVs[texid].y;
        uv_max.x = m_AtlasUVs[texid].z;
        uv_max.y = m_AtlasUVs[texid].w;

        return true;
    }

    return false;
}

bool TextureManager::GetTextureInfo(const CustomAction& action, ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const
{
    if (action.IconImGuiRectID != -1)
    {
        size = action.IconAtlasSize;
        //Also set cached UV coordinates
        uv_min.x = action.IconAtlasUV.x;
        uv_min.y = action.IconAtlasUV.y;
        uv_max.x = action.IconAtlasUV.z;
        uv_max.y = action.IconAtlasUV.w;

        return true;
    }

    return false;
}

void TextureManager::AddFontBuilderString(const char* str)
{
    m_FontBuilderExtraStrings.emplace_back(str);
}
