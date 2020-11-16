//Desktop+UI loads all textures into Dear ImGui's font texture atlas
//Bigger texture sizes are well supported on VR-running GPUs and less texture switching is more efficient
//It's also more convenient in general
//This is using GDI+ to load PNGs. Seemed like the next best option without too much overhead and doesn't need any extra library to ship

#pragma once

#include "imgui.h"

#include "Actions.h"

enum TMNGRTexID
{
    tmtex_icon_desktop,
    tmtex_icon_desktop_1,
    tmtex_icon_desktop_2,
    tmtex_icon_desktop_3,
    tmtex_icon_desktop_4,
    tmtex_icon_desktop_5,
    tmtex_icon_desktop_6,
    tmtex_icon_desktop_all,
    tmtex_icon_desktop_next,
    tmtex_icon_desktop_prev,
    tmtex_icon_settings,
    tmtex_icon_keyboard,
    tmtex_icon_small_close,
    tmtex_icon_small_move,
    tmtex_icon_small_actionbar,
    tmtex_icon_temp,         //This is an odd one to hack-ishly load one icon without associating it with anything. The file for this can be set freely by TextureManager
    tmtex_MAX
};

class TextureManager
{
    private:
        static const wchar_t* TextureManager::s_TextureFilenames[tmtex_MAX];
        int m_ImGuiRectIDs[tmtex_MAX];  //-1 when not loaded (ID on ImGui end is not valid after building the font as data is cleared to save memory)
        ImVec2 m_AtlasSizes[tmtex_MAX];
        ImVec4 m_AtlasUVs[tmtex_MAX];
        std::wstring m_TextureFilenameIconTemp;
        std::vector<std::string> m_FontBuilderExtraStrings; //Extra strings containing characters to be included when building the fonts. Might fill up over time but better than nothing

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

        void AddFontBuilderString(const char* str);
};