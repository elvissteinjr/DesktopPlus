#pragma once

#include "imgui.h"

class WindowSettings;

//The side bar visible on the bottom right of floating overlays when pointing at them
class WindowSideBar
{
    private:
        ImVec2 m_Pos;
        ImVec2 m_Size;

        void DisplayTooltipIfHovered(const char* text);

    public:
        WindowSideBar();

        void Update(float mainbar_height, unsigned int overlay_id);
        
        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;

};