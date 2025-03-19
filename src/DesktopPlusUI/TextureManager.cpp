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
#include "WindowManager.h"
#include "imgui_impl_dx11_openvr.h"

const wchar_t* TextureManager::s_TextureFilenames[tmtex_MAX] =
{
    L"images/icons/desktop.png",
    L"images/icons/desktop_all.png",
    L"images/icons/desktop_1.png",
    L"images/icons/desktop_2.png",
    L"images/icons/desktop_3.png",
    L"images/icons/desktop_4.png",
    L"images/icons/desktop_5.png",
    L"images/icons/desktop_6.png",
    L"images/icons/desktop_next.png",
    L"images/icons/desktop_previous.png",
    L"images/icons/desktop_none.png",
    L"images/icons/performance_monitor.png",
    L"images/icons/browser.png",
    L"images/icons/settings.png",
    L"images/icons/keyboard.png",
    L"images/icons/task_switch.png",
    L"images/icons/add.png",
    L"images/icons/window_overlay.png",
    L"images/icons_small/small_app_icon.png",
    L"images/icons_small/small_close.png",
    L"images/icons_small/small_move.png",
    L"images/icons_small/small_move_locked.png",
    L"images/icons_small/small_add_window.png",
    L"images/icons_small/small_actionbar.png",
    L"images/icons_small/small_performance_monitor_reset.png",
    L"images/icons_small/small_browser_back.png",
    L"images/icons_small/small_browser_forward.png",
    L"images/icons_small/small_browser_refresh.png",
    L"images/icons_small/small_browser_stop.png",
    L"images/icons_small/xsmall_desktop.png",
    L"images/icons_small/xsmall_desktop_all.png",
    L"images/icons_small/xsmall_desktop_1.png",
    L"images/icons_small/xsmall_desktop_2.png",
    L"images/icons_small/xsmall_desktop_3.png",
    L"images/icons_small/xsmall_desktop_4.png",
    L"images/icons_small/xsmall_desktop_5.png",
    L"images/icons_small/xsmall_desktop_6.png",
    L"images/icons_small/xsmall_desktop_none.png",
    L"images/icons_small/xsmall_performance_monitor.png",
    L"images/icons_small/xsmall_browser.png",
    L"images/icons_small/xsmall_settings.png",
    L"images/icons_small/xsmall_keyboard.png",
    L"images/icons_small/xsmall_origin_playspace.png",
    L"images/icons_small/xsmall_origin_hmd_pos.png",
    L"images/icons_small/xsmall_origin_seated.png",
    L"images/icons_small/xsmall_origin_dashboard.png",
    L"images/icons_small/xsmall_origin_hmd.png",
    L"images/icons_small/xsmall_origin_controller_left.png",
    L"images/icons_small/xsmall_origin_controller_right.png",
    L"images/icons_small/xsmall_origin_aux.png",
    L"images/icons_small/xsmall_origin_theater_screen.png",
    L"images/icons_small/xxsmall_close.png",
    L"images/icons_small/xxsmall_pin.png",
    L"images/icons_small/xxsmall_unpin.png",
    L"images/icons_small/xxsmall_browser_back.png",
    L"",                                            //tmtex_icon_temp, blank
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

    ActionManager& action_manager = ConfigManager::Get().GetActionManager();
    for (ActionUID uid : action_manager.GetActionOrderListUI())
    {
        const Action action = action_manager.GetAction(uid);

        builder.AddText(action.Name.c_str());
        builder.AddText(action.Label.c_str());

        for (const auto& command : action.Commands)
        {
            builder.AddText(command.StrMain.c_str());
            builder.AddText(command.StrArg.c_str());
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

    for (const WindowInfo& window_info :  WindowManager::Get().WindowListGet()) //And window list
    {
        builder.AddText(window_info.GetListTitle().c_str());
    }

    for (const std::string& str : m_FontBuilderExtraStrings) //And extra strings... yeah. This might not be the best way to tackle this issue
    {
        builder.AddText(str.c_str());
    }

    //Characters from current translation
    TranslationManager::Get().AddStringsToFontBuilder(builder);

    //Characters used by the VR Keyboard
    builder.AddText(UIManager::Get()->GetVRKeyboard().GetKeyLabelsString().c_str());

    //Extra characters used by the UI directly
    builder.AddText(k_pch_bold_exclamation_mark);

    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.BuildRanges(&ranges);

    ImFontConfig config_compact;
    ImFontConfig config_large;
    config_compact.GlyphOffset.y = -1; //Set offset to make it not look so bad
    config_large.GlyphOffset.y   = -1;
    ImFontConfig* config = &config_compact;

    //Try to load fonts
    ImFont* font = nullptr;
    ImFont* font_compact = nullptr;
    ImFont* font_large = nullptr;
    float font_base_size = 32.0f;
    bool load_large_font = ( (ConfigManager::GetValue(configid_bool_interface_large_style)) && (!UIManager::Get()->IsInDesktopMode()) );

    //Loop to do the same for the large font if needed
    for (;;)
    {
        //Load preferred font first, if the translation has set one
        const std::string& preferred_font_name      = TranslationManager::Get().GetCurrentTranslationFontName();
        const std::wstring preferred_font_name_wstr = WStringConvertFromUTF8(TranslationManager::Get().GetCurrentTranslationFontName().c_str());

        if (!preferred_font_name.empty())
        {
            //AddFontFromFileTTF asserts when failing to load, so check for existence, though it's not really an issue in release mode
            if (FileExists( (L"C:\\Windows\\Fonts\\" + preferred_font_name_wstr).c_str() ))
            {
                font = io.Fonts->AddFontFromFileTTF( ("C:\\Windows\\Fonts\\" + preferred_font_name).c_str(), font_base_size * UIManager::Get()->GetUIScale(), config, ranges.Data);

                //Other fonts are still used as fallback
                config->MergeMode = true;
            }
            else if (FileExists( (WStringConvertFromUTF8(ConfigManager::Get().GetApplicationPath().c_str()) + L"/lang/" + preferred_font_name_wstr).c_str() ))
            {
                //Also allow for a custom font from the application directory
                font = io.Fonts->AddFontFromFileTTF( (ConfigManager::Get().GetApplicationPath() + "/lang/" + preferred_font_name).c_str(), font_base_size * UIManager::Get()->GetUIScale(), 
                                                    config, ranges.Data);

                config->MergeMode = true;
            }
        }

        //Continue with the standard font selection
        if (FileExists(L"C:\\Windows\\Fonts\\segoeui.ttf"))
        {
            font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", font_base_size * UIManager::Get()->GetUIScale(), config, ranges.Data);
        }

        if (font != nullptr)
        {
            //Segoe UI doesn't have any CJK, use some fallbacks (loading this is actually pretty fast)
            config->MergeMode = true;

            //Prefer Meiryo over MS Gothic. The former isn't installed on non-japanese systems by default though
            if (FileExists(L"C:\\Windows\\Fonts\\meiryo.ttc"))
                io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", font_base_size * UIManager::Get()->GetUIScale(), config, ranges.Data);
            else if (FileExists(L"C:\\Windows\\Fonts\\msgothic.ttc"))
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
        LOG_F(ERROR, "Initializing GDI+ failed! Icons will not be loaded");

        //Still build the font so we can have text at least
        const bool font_build_success = io.Fonts->Build();

        //If building the font atlas failed, fall back to the internal default font and try again
        if (!font_build_success)
        {
            LOG_F(ERROR, "Building font atlas failed (invalid font file?)! Falling back to internal font");

            io.Fonts->Clear();

            font = io.Fonts->AddFontDefault();
            font_compact = font;
            font_large   = font;

            io.Fonts->Build();
        }

        io.Fonts->ClearInputData(); //We don't need to keep this around, reduces RAM use a lot

        UIManager::Get()->SetFonts(font_compact, font_large);

        m_ReloadLater = false;
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
    struct ActionIconTextureData
    {
        int IconImGuiRectID  = -1;
        ImVec2 IconAtlasSize;
        ImVec4 IconAtlasUV;
    };

    std::vector<ActionIconTextureData> action_icon_tex_data;
    action_icon_tex_data.reserve(action_manager.GetActionOrderListUI().size());

    action_manager.ClearIconData();
    for (ActionUID uid : action_manager.GetActionOrderListUI())
    {
        const Action& action = action_manager.GetAction(uid);
        ActionIconTextureData tex_data;

        if (!action.IconFilename.empty())
        {
            std::string icon_path = "images/icons/" + action.IconFilename;
            std::unique_ptr<Gdiplus::Bitmap> bmp( new Gdiplus::Bitmap(WStringConvertFromUTF8(icon_path.c_str()).c_str()) );

            if (bmp->GetLastStatus() == Gdiplus::Ok)
            {
                tex_data.IconImGuiRectID = io.Fonts->AddCustomRectRegular(bmp->GetWidth(), bmp->GetHeight());

                if (io.Fonts->TexDesiredWidth <= (int)bmp->GetWidth())
                {
                    //See above
                    io.Fonts->TexDesiredWidth = (bmp->GetWidth() >= 2048) ? 4096 : (bmp->GetWidth() >= 1024) ? 2048 : (bmp->GetWidth() >= 512) ? 1024 : 512;
                }
            }

            bitmaps.push_back(std::move(bmp));
        }

        action_icon_tex_data.push_back(tex_data);
    }

    //Set up already loaded window icons
    for (auto& window_icon : m_WindowIcons)
    {
        window_icon.ImGuiRectID = io.Fonts->AddCustomRectRegular((int)window_icon.Size.x, (int)window_icon.Size.y);

        if (io.Fonts->TexDesiredWidth <= (int)window_icon.Size.x)
        {
            //See above
            io.Fonts->TexDesiredWidth = (window_icon.Size.x >= 2048) ? 4096 : (window_icon.Size.x >= 1024) ? 2048 : (window_icon.Size.x >= 512) ? 1024 : 512;
        }
    }

    //Build atlas
    const bool font_build_success = io.Fonts->Build();

    //If building the font atlas failed, fall back to the internal default font and try again
    if (!font_build_success)
    {
        LOG_F(ERROR, "Building font atlas failed (invalid font file?)! Falling back to internal font");

        ImVector<ImFontAtlasCustomRect> custom_rects_back = io.Fonts->CustomRects;
        int desired_width_back = io.Fonts->TexDesiredWidth;
        io.Fonts->Clear();

        font = io.Fonts->AddFontDefault();
        font_compact = font;
        font_large   = font;

        io.Fonts->CustomRects     = custom_rects_back;
        io.Fonts->TexDesiredWidth = desired_width_back;

        io.Fonts->Build();
    }

    //Retrieve atlas texture in RGBA format
    unsigned char* tex_pixels = nullptr;
    int tex_width, tex_height;
    io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_width, &tex_height);

    //Actually do the copying now
    icon_id = 0;
    auto action_tex_data_it = action_icon_tex_data.begin();
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
                //Get the next action, skipping the ones with no icon
                do
                {
                    if (action_tex_data_it == action_icon_tex_data.end())
                    {
                        break;
                    }

                    ActionIconTextureData& tex_data = *action_tex_data_it;
                    rect_id    = &tex_data.IconImGuiRectID;
                    atlas_size = &tex_data.IconAtlasSize;
                    atlas_uvs  = &tex_data.IconAtlasUV;

                    action_tex_data_it++;
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
                        for (int y = 0; y < rect->Height; ++y)
                        {
                            ImU32* p = (ImU32*)tex_pixels + (rect->Y + y) * tex_width + (rect->X);
                            UINT8* pgdi = (UINT8*)bitmapData.Scan0 + (y * bitmapData.Stride);

                            for (int x = 0; x < rect->Width; ++x)
                            {
                                //GDI+ order is BGRA, convert
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

    //Copy cached window icons into atlas
    for (auto& window_icon : m_WindowIcons)
    {
        if (const ImFontAtlasCustomRect* rect = io.Fonts->GetCustomRectByIndex(window_icon.ImGuiRectID))
        {
            UINT8* psrc = (UINT8*)window_icon.PixelData.get();
            size_t stride = rect->Width * 4;
            //Copy RGBA pixels line-by-line
            for (int y = 0; y < rect->Height; ++y, psrc += stride)
            {
                ImU32* p = (ImU32*)tex_pixels + (rect->Y + y) * tex_width + (rect->X);
                memcpy(p, psrc, stride);
            }

            //Store UVs
            window_icon.AtlasUV.x = (float)rect->X * io.Fonts->TexUvScale.x;                  //Min U
            window_icon.AtlasUV.y = (float)rect->Y * io.Fonts->TexUvScale.y;                  //Min V
            window_icon.AtlasUV.z = (float)(rect->X + rect->Width)  * io.Fonts->TexUvScale.x; //Max U
            window_icon.AtlasUV.w = (float)(rect->Y + rect->Height) * io.Fonts->TexUvScale.y; //Max V
        }
        else
        {
            window_icon.ImGuiRectID = -1;
            all_ok = false;
        }
    }

    //Store action texture data in actual actions
    IM_ASSERT(action_icon_tex_data.size() == action_manager.GetActionOrderListUI().size());
    for (size_t i = 0; i < action_icon_tex_data.size(); ++i)
    {
        const ActionIconTextureData& tex_data = action_icon_tex_data[i];

        //We can just skip this when the rect ID is still -1 since we cleared all texture data at beginning
        if (tex_data.IconImGuiRectID != -1)
        {
            Action action = action_manager.GetAction( action_manager.GetActionOrderListUI()[i] );

            action.IconImGuiRectID = tex_data.IconImGuiRectID;
            action.IconAtlasSize   = tex_data.IconAtlasSize;
            action.IconAtlasUV     = tex_data.IconAtlasUV;

            action_manager.StoreAction(action);
        }
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
    return (texid != tmtex_icon_temp) ? s_TextureFilenames[texid] : m_TextureFilenameIconTemp.c_str();
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

bool TextureManager::GetTextureInfo(const Action& action, ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const
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

int TextureManager::GetWindowIconCacheID(HWND window_handle)
{
    WindowInfo const* info_ptr = WindowManager::Get().WindowListFindWindow(window_handle);

    return (info_ptr != nullptr) ? GetWindowIconCacheID(info_ptr->GetIcon()) : -1;
}

int TextureManager::GetWindowIconCacheID(HWND window_handle, uint64_t& icon_handle_config)
{
    WindowInfo const* info_ptr = WindowManager::Get().WindowListFindWindow(window_handle);
    HICON icon_handle = (info_ptr != nullptr) ? info_ptr->GetIcon() : nullptr;

    if (icon_handle != nullptr)
    {
        icon_handle_config = (uint64_t)icon_handle;
        return GetWindowIconCacheID(icon_handle);
    }
    else if (icon_handle_config != 0)
    {
        return GetWindowIconCacheID((HICON)icon_handle_config);
    }

    return -1;
}

int TextureManager::GetWindowIconCacheID(HICON icon_handle)
{
    //Look if the icon is already loaded
    for (int i = 0; i < m_WindowIcons.size(); ++i)
    {
        if (m_WindowIcons[i].IconHandle == icon_handle)
        {
            return i;
        }
    }

    //Icon not loaded yet, try to do that
    int ret = -1;
    ICONINFO icon_info = {0};
    if (::GetIconInfo(icon_handle, &icon_info) != 0)
    {
        HDC hdc, hdcMem;

        hdc    = ::GetDC(nullptr);
        hdcMem = ::CreateCompatibleDC(hdc);

        //Get bitmap info from icon bitmap
        BITMAPINFO bmp_info = {0};
        bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
        if (::GetDIBits(hdcMem, icon_info.hbmColor, 0, 0, nullptr, &bmp_info, DIB_RGB_COLORS) != 0)
        {
            TMNGRWindowIcon window_icon;
            int icon_width  = bmp_info.bmiHeader.biWidth;
            int icon_height = abs(bmp_info.bmiHeader.biHeight);
            const size_t icon_pixel_count = icon_width * icon_height;

            auto PixelData = std::unique_ptr<BYTE[]>{new BYTE[icon_pixel_count * 4]};

            bmp_info.bmiHeader.biSize        = sizeof(bmp_info.bmiHeader);
            bmp_info.bmiHeader.biBitCount    = 32;
            bmp_info.bmiHeader.biCompression = BI_RGB;
            bmp_info.bmiHeader.biHeight      = -icon_height; //Always use top-down order (negative height)

            //Read the actual bitmap buffer into the pixel data array
            if (::GetDIBits(hdc, icon_info.hbmColor, 0, bmp_info.bmiHeader.biHeight, (LPVOID)PixelData.get(), &bmp_info, DIB_RGB_COLORS) != 0)
            {
                //Even if we don't override biBitCount to 32, it's still returned as that for 24-bit and lower bit-depth icons (probably just the screen DC format)
                //It seems the only way to check if the icon needs its mask applied is to see if the alpha channel is fully blank
                //32-bit icons still come with masks, but applying them means to override the alpha channel with a 1-bit one (and doing so is also wasteful)
                bool needs_mask = true;
                BYTE* psrc = PixelData.get() + 3; //BGRA alpha pixel
                const BYTE* const psrc_end = PixelData.get() + (icon_pixel_count * 4);
                for (; psrc < psrc_end; psrc += 4)
                {
                    if (*psrc != 0)
                    {
                        needs_mask = false;
                        break;
                    }
                }

                //Apply mask if we need to
                if (needs_mask)
                {
                    //Get bitmap info for the mask this time
                    BITMAPINFO bmp_info = {0};
                    bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
                    if (::GetDIBits(hdcMem, icon_info.hbmMask, 0, 0, nullptr, &bmp_info, DIB_RGB_COLORS) != 0)
                    {
                        int mask_width  = bmp_info.bmiHeader.biWidth;
                        int mask_height = abs(bmp_info.bmiHeader.biHeight);

                        //Only continue if icon and mask are really the same size (can be different for monochrome bitmap formats, which are not supported here)
                        if ( (icon_width == mask_width) && (icon_height == mask_height) )
                        {
                            auto PixelDataMask = std::unique_ptr<BYTE[]>{new BYTE[icon_pixel_count * 4]};

                            bmp_info.bmiHeader.biSize        = sizeof(bmp_info.bmiHeader);
                            bmp_info.bmiHeader.biBitCount    = 32;
                            bmp_info.bmiHeader.biCompression = BI_RGB;
                            bmp_info.bmiHeader.biHeight      = -abs(bmp_info.bmiHeader.biHeight); //Always use top-down order (negative height)

                            //Read the mask bitmap buffer
                            if (::GetDIBits(hdc, icon_info.hbmMask, 0, bmp_info.bmiHeader.biHeight, (LPVOID)PixelDataMask.get(), &bmp_info, DIB_RGB_COLORS) != 0)
                            {
                                //Apply mask to color pixel data
                                psrc       = PixelData.get() + 3; //BGRA alpha pixel
                                BYTE* pmsk = PixelDataMask.get(); //BGRA blue pixel (alpha channel is blank for the mask)
                                for (; psrc < psrc_end; psrc += 4, pmsk += 4)
                                {
                                    *psrc = ~(*pmsk);
                                }
                            }
                        }
                    }
                }

                //Convert BGRA to RGBA for ImGui's texture atlas
                window_icon.PixelData = std::unique_ptr<BYTE[]>{new BYTE[icon_pixel_count * 4]};
                psrc         = PixelData.get();
                UINT32* pdst = (UINT32*)window_icon.PixelData.get();
                for (; psrc < psrc_end; psrc += 4, ++pdst)
                {
                    *pdst = IM_COL32(*(psrc + 2), *(psrc + 1), *psrc, *(psrc + 3));
                }

                //Fill out other data and move the icon to the cache
                window_icon.IconHandle = icon_handle;
                window_icon.Size = {(float)icon_width, (float)icon_height};
                m_WindowIcons.push_back(std::move(window_icon));

                //We succeeded, but the icon won't be ready until next frame, so schedule reload and skip rendering this frame
                ReloadAllTexturesLater();
                UIManager::Get()->RepeatFrame();

                ret = (int)m_WindowIcons.size() - 1;
            }
        }

        ::DeleteObject(icon_info.hbmColor);
        ::DeleteObject(icon_info.hbmMask);

        ::DeleteDC(hdcMem);
        ::ReleaseDC(nullptr, hdc);
    }

    return ret;
}

bool TextureManager::GetWindowIconTextureInfo(int icon_cache_id, ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const
{
    if ( (icon_cache_id >= 0) && (icon_cache_id < m_WindowIcons.size()) )
    {
        const TMNGRWindowIcon& window_icon = m_WindowIcons[icon_cache_id];

        size     = window_icon.Size;
        uv_min.x = window_icon.AtlasUV.x;
        uv_min.y = window_icon.AtlasUV.y;
        uv_max.x = window_icon.AtlasUV.z;
        uv_max.y = window_icon.AtlasUV.w;

        return true;
    }

    return false;
}

bool TextureManager::GetOverlayIconTextureInfo(OverlayConfigData& data, ImVec2& size, ImVec2& uv_min, ImVec2& uv_max, bool is_xsmall, bool* has_window_icon)
{
    if ( (is_xsmall) && (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0) )
    {
        //XSmall returns the window icon itself
        int cache_id = GetWindowIconCacheID((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd], data.ConfigHandle[configid_handle_overlay_state_winrt_last_hicon]);

        if (cache_id != -1)
        {
            if (has_window_icon != nullptr)
                *has_window_icon = true;

            return GetWindowIconTextureInfo(cache_id, size, uv_min, uv_max);
        }
    }

    return GetTextureInfo(GetOverlayIconTextureID(data, is_xsmall, has_window_icon), size, uv_min, uv_max);
}

bool TextureManager::AddFontBuilderString(const std::string& str)
{
    //Add only if it's not already in the extra string list. Avoids duplicates and unnecessary texture rebuilds if the requested character can't be found in the loaded fonts
    if (std::find(m_FontBuilderExtraStrings.begin(), m_FontBuilderExtraStrings.end(), str) == m_FontBuilderExtraStrings.end())
    {
        m_FontBuilderExtraStrings.push_back(str);
        return true;
    }

    return false;
}

TMNGRTexID TextureManager::GetOverlayIconTextureID(const OverlayConfigData& data, bool is_xsmall, bool* has_window_icon)
{
    TMNGRTexID texture_id = (is_xsmall) ? tmtex_icon_xsmall_desktop_none : tmtex_icon_desktop_none;
    int desktop_id = -2;

    if (has_window_icon != nullptr)
        *has_window_icon = false;

    switch (data.ConfigInt[configid_int_overlay_capture_source])
    {
        case ovrl_capsource_desktop_duplication:
        {
            desktop_id = data.ConfigInt[configid_int_overlay_desktop_id];
            break;
        }
        case ovrl_capsource_winrt_capture:
        {
            if (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0)
            {
                if (has_window_icon != nullptr)
                    *has_window_icon = true;

                texture_id = (is_xsmall) ? tmtex_icon_xsmall_desktop_none : tmtex_icon_window_overlay;
            }
            else if (data.ConfigInt[configid_int_overlay_winrt_desktop_id] != -2)
            {
                desktop_id = data.ConfigInt[configid_int_overlay_winrt_desktop_id];
            }
            else
            {
                texture_id = (is_xsmall) ? tmtex_icon_xsmall_desktop_none : tmtex_icon_desktop_none;
            }
            break;
        }
        case ovrl_capsource_ui:
        {
            texture_id = (is_xsmall) ? tmtex_icon_xsmall_performance_monitor : tmtex_icon_performance_monitor;
            break;
        }
        case ovrl_capsource_browser:
        {
            texture_id = (is_xsmall) ? tmtex_icon_xsmall_browser : tmtex_icon_browser;
            break;
        }
    }

    if (desktop_id != -2)
    {
        if (is_xsmall)
        {
            texture_id = (tmtex_icon_xsmall_desktop_1 + desktop_id <= tmtex_icon_xsmall_desktop_6) ? (TMNGRTexID)(tmtex_icon_xsmall_desktop_1 + desktop_id) : tmtex_icon_xsmall_desktop;
        }
        else
        {
            texture_id = (tmtex_icon_desktop_1 + desktop_id <= tmtex_icon_desktop_6) ? (TMNGRTexID)(tmtex_icon_desktop_1 + desktop_id) : tmtex_icon_desktop;
        }
    }

    return texture_id;
}
