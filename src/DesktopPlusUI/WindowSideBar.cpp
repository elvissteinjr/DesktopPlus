#include "WindowSideBar.h"

#include "ImGuiExt.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"
#include "WindowSettings.h"
#include "Util.h"
#include "UIManager.h"
#include "OverlayManager.h"

static const ImVec4 Col_ButtonActiveDesktop(0.180f, 0.349f, 0.580f, 0.404f);

void WindowSideBar::DisplayTooltipIfHovered(const char* text)
{
    //Compared to WindowMainBar's function, this one puts the tooltip to the left of the button
    if (ImGui::IsItemHovered())
    {
        static ImVec2 pos_last; //Remeber last position and use it when posible. This avoids flicker when the same tooltip string is used in different places
        ImVec2 pos = ImGui::GetItemRectMin();
        float button_width  = ImGui::GetItemRectSize().x;
        float button_height = ImGui::GetItemRectSize().y;

        //Default tooltips are not suited for this as there's too much trouble with resize flickering and stuff
        ImGui::SetNextWindowPos(pos_last);
        ImGui::Begin(text, NULL, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

        ImGui::Text(text);

        ImVec2 window_size = ImGui::GetWindowSize();

        //Repeat frame when the window is appearing as it will not have the right position (either from being first time or still having old pos)
        if (ImGui::IsWindowAppearing())
        {
            UIManager::Get()->RepeatFrame();
        }
        else
        {
            pos.x -= window_size.x + ImGui::GetStyle().WindowPadding.x;
            pos.y += (button_height / 2.0f) - (window_size.y / 2.0f);
        }

        pos_last = pos;

        ImGui::End();
    }
}

WindowSideBar::WindowSideBar()
{

}

void WindowSideBar::Update(float mainbar_height, unsigned int overlay_id)
{
    OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData(overlay_id);

    ImGuiIO& io = ImGui::GetIO();

    ImVec2 b_size, b_uv_min, b_uv_max;
    TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
    const ImGuiStyle& style = ImGui::GetStyle();
    float offset_y = mainbar_height + style.FramePadding.y + style.WindowPadding.y;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x, io.DisplaySize.y - offset_y), 0, ImVec2(1.0f, 1.0f));  //Put window at bottom right of the overlay
    ImGui::Begin("WindowSideBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
                                            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    //There is leftover space for a fourth button in the overlay texture so one could be easily added if the need ever arises

    //Close/Disable Button
    ImGui::PushID(tmtex_icon_small_close);
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_close, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        overlay_data.ConfigBool[configid_bool_overlay_enabled] = false;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)overlay_id);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_bool_overlay_enabled), false);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
    }

    DisplayTooltipIfHovered("Disable Overlay");
        
    ImGui::PopID();
    //

    //Drag-Mode Toggle Button (this is a global state)
    bool& is_dragmode_enabled = ConfigManager::Get().GetConfigBoolRef(configid_bool_state_overlay_dragmode);
    bool dragmode_was_enabled = is_dragmode_enabled;
    
    if (dragmode_was_enabled)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    ImGui::PushID(tmtex_icon_small_move);
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_move, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        is_dragmode_enabled = !is_dragmode_enabled;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragselectmode_show_hidden), false);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragmode), is_dragmode_enabled);
    }

    if (dragmode_was_enabled)
        ImGui::PopStyleColor();

    DisplayTooltipIfHovered( (dragmode_was_enabled) ? "Disable Drag-Mode" : "Enable Drag-Mode");
        
    ImGui::PopID();
    //

    //Show Action-Bar Button (this is a per-overlay state)
    bool& is_actionbar_enabled = overlay_data.ConfigBool[configid_bool_overlay_actionbar_enabled];
    bool actionbar_was_enabled = is_actionbar_enabled;

    if (actionbar_was_enabled)
        ImGui::PushStyleColor(ImGuiCol_Button, Style_ImGuiCol_ButtonPassiveToggled);

    ImGui::PushID(tmtex_icon_small_actionbar);
    TextureManager::Get().GetTextureInfo(tmtex_icon_small_actionbar, b_size, b_uv_min, b_uv_max);
    if (ImGui::ImageButton(io.Fonts->TexID, b_size, b_uv_min, b_uv_max))
    {
        is_actionbar_enabled = !is_actionbar_enabled;
        //This is an UI state so no need to sync
    }

    if (actionbar_was_enabled)
        ImGui::PopStyleColor();

    DisplayTooltipIfHovered( (actionbar_was_enabled) ? "Hide Action-Bar" : "Show Action-Bar");
        
    ImGui::PopID();
    //

    ImGui::PopStyleColor(); //ImGuiCol_Button
    ImGui::PopStyleVar();   //ImGuiStyleVar_FrameRounding

    m_Pos  = ImGui::GetWindowPos();
    m_Size = ImGui::GetWindowSize();

    ImGui::End();
}

const ImVec2 & WindowSideBar::GetPos() const
{
    return m_Pos;
}

const ImVec2 & WindowSideBar::GetSize() const
{
    return m_Size;
}
