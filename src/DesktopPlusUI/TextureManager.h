//Desktop+UI loads all textures into Dear ImGui's font texture atlas
//Bigger texture sizes are well supported on VR-running GPUs and less texture switching is more efficient
//It's also more convenient in general
//This is using GDI+ to load PNGs. Seemed like the next best option without too much overhead and doesn't need any extra library to ship

#pragma once

#include <memory>
#include "imgui.h"

#include "Actions.h"

enum TMNGRTexID
{
    tmtex_icon_desktop,
    tmtex_icon_desktop_all,
    tmtex_icon_desktop_1,
    tmtex_icon_desktop_2,
    tmtex_icon_desktop_3,
    tmtex_icon_desktop_4,
    tmtex_icon_desktop_5,
    tmtex_icon_desktop_6,
    tmtex_icon_desktop_next,
    tmtex_icon_desktop_prev,
    tmtex_icon_desktop_none,
    tmtex_icon_performance_monitor,
    tmtex_icon_settings,
    tmtex_icon_keyboard,
    tmtex_icon_add,
    tmtex_icon_window_overlay,
    tmtex_icon_small_close,
    tmtex_icon_small_move,
    tmtex_icon_small_add_window,
    tmtex_icon_small_actionbar,
    tmtex_icon_xsmall_desktop,
    tmtex_icon_xsmall_desktop_all,
    tmtex_icon_xsmall_desktop_1,
    tmtex_icon_xsmall_desktop_2,
    tmtex_icon_xsmall_desktop_3,
    tmtex_icon_xsmall_desktop_4,
    tmtex_icon_xsmall_desktop_5,
    tmtex_icon_xsmall_desktop_6,
    tmtex_icon_xsmall_desktop_none,
    tmtex_icon_xsmall_performance_monitor,
    tmtex_icon_xsmall_settings,
    tmtex_icon_xsmall_keyboard,
    tmtex_icon_xsmall_origin_room,
    tmtex_icon_xsmall_origin_hmd_floor,
    tmtex_icon_xsmall_origin_seated_space,
    tmtex_icon_xsmall_origin_dashboard,
    tmtex_icon_xsmall_origin_hmd,
    tmtex_icon_xsmall_origin_right_hand,
    tmtex_icon_xsmall_origin_left_hand,
    tmtex_icon_xsmall_origin_aux,
    tmtex_icon_xxsmall_close,
    tmtex_icon_xxsmall_pin,
    tmtex_icon_xxsmall_unpin,
    tmtex_icon_temp,         //This is an odd one to hack-ishly load one icon without associating it with anything. The file for this can be set freely by TextureManager
    tmtex_MAX
};

struct TMNGRWindowIcon
{
    HICON IconHandle = nullptr;
    std::unique_ptr<BYTE[]> PixelData;  //RGBA
    ImVec2 Size = {0.0f, 0.0f};
    int ImGuiRectID  = -1; //-1 when no icon loaded (ID on ImGui end is not valid after building the font)
    ImVec4 AtlasUV = {0.0f, 0.0f, 0.0f, 0.0f};
};

class OverlayConfigData;

class TextureManager
{
    private:
        static const wchar_t* s_TextureFilenames[tmtex_MAX];
        int m_ImGuiRectIDs[tmtex_MAX];  //-1 when not loaded (ID on ImGui end is not valid after building the font as data is cleared to save memory)
        ImVec2 m_AtlasSizes[tmtex_MAX];
        ImVec4 m_AtlasUVs[tmtex_MAX];
        std::wstring m_TextureFilenameIconTemp;
        std::vector<std::string> m_FontBuilderExtraStrings; //Extra strings containing characters to be included when building the fonts. Might fill up over time but better than nothing
        std::vector<TMNGRWindowIcon> m_WindowIcons;

        bool m_ReloadLater;

    public:
        TextureManager();
        static TextureManager& Get();

        bool LoadAllTexturesAndBuildFonts();
        void ReloadAllTexturesLater();          //Schedule reload for the beginning of the next frame since we can't do it in the middle of one
        bool GetReloadLaterFlag();

        const wchar_t* GetTextureFilename(TMNGRTexID texid) const;
        void SetTextureFilenameIconTemp(const wchar_t* filename);
        bool GetTextureInfo(TMNGRTexID texid, ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const;
        bool GetTextureInfo(const CustomAction& action, ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const;

        int  GetWindowIconCacheID(HWND window_handle); //Returns -1 on error
        int  GetWindowIconCacheID(HWND window_handle, uint64_t& icon_handle_config); //Updates icon_handle_config when lookup with window_handle succeeds or falls back to icon_handle_config
        int  GetWindowIconCacheID(HICON icon_handle);  //Returns -1 on error
        bool GetWindowIconTextureInfo(int icon_cache_id, ImVec2& size, ImVec2& uv_min, ImVec2& uv_max) const;

        bool GetOverlayIconTextureInfo(OverlayConfigData& data, ImVec2& size, ImVec2& uv_min, ImVec2& uv_max, bool is_xsmall = false, bool* has_window_icon = nullptr);

        bool AddFontBuilderString(const char* str);   //Returns true if string has been added (not already in extra string list)

        static TMNGRTexID GetOverlayIconTextureID(const OverlayConfigData& data, bool is_xsmall, bool* has_window_icon = nullptr);
};