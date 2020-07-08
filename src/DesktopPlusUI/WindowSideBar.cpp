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
    //Comapred to WindowMainBar's function, this one puts the tooltip to the left of the button
    if (ImGui::IsItemHovered())
    {
        ImVec2 pos = ImGui::GetItemRectMin();
        float button_width  = ImGui::GetItemRectSize().x;
        float button_height = ImGui::GetItemRectSize().y;

        //Default tooltips are not suited for this as there's too much trouble with resize flickering and stuff
        ImGui::Begin(text, NULL, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text(text);

        ImVec2 window_size = ImGui::GetWindowSize();

        //32 is the default size returned by ImGui on the first frame
        //I'm not sure if there isn't a better way to make the tooltip go away before we can position it correctly (ImGui::IsWindowAppearing() doesn't help)
        //Well, it works
        if (window_size.y == 32.0f)
        {
            UIManager::Get()->RepeatFrame(); //Getting it out of the way isn't feasible if the tooltip is dynamic as it causes flickering, so repeat the frame instead
        }
        else
        {
            pos.x -= window_size.x + ImGui::GetStyle().WindowPadding.x;
            pos.y += (button_height / 2.0f) - (window_size.y / 2.0f);

            //Clamp x so the tooltip does not get cut off
            pos.x = clamp(pos.x, 0.0f, ImGui::GetIO().DisplaySize.x - window_size.x);
        }
        ImGui::SetWindowPos(pos);

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
    ImGui::Begin("WindowSideBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
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
