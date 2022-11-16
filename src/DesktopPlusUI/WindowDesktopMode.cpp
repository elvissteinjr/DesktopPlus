#include "WindowDesktopMode.h"

#include "imgui.h"
#include "UIManager.h"
#include "InterprocessMessaging.h"

WindowDesktopMode::WindowDesktopMode()
{
    m_PageStack.push_back(wnddesktopmode_page_main);
}

void WindowDesktopMode::UpdateTitleBar()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 img_size_line_height = {ImGui::GetTextLineHeight() * 1.6f, ImGui::GetTextLineHeight() * 1.6f};
    ImVec2 img_size, img_uv_min, img_uv_max;

    ImVec4 title_rect = {0.0f, 0.0f, ImGui::GetWindowSize().x, img_size_line_height.y + (style.WindowPadding.y * 2.0f)};
    ImGui::PushClipRect({title_rect.x, title_rect.y}, {title_rect.z, title_rect.w}, false);

    //Background color
    ImGui::GetWindowDrawList()->AddRectFilled({0.0f, 0.0f}, {title_rect.z, title_rect.w}, ImGui::GetColorU32(ImGuiCol_TitleBg));

    //Icon and title text
    static ImVec2 back_button_size;
    const float back_button_x = title_rect.z - back_button_size.x - style.FramePadding.x;

    const char* title_str = nullptr;

    switch (m_PageStack[m_PageStackPos])
    {
        case wnddesktopmode_page_main:       title_str = "Desktop+";
                                             TextureManager::Get().GetTextureInfo(tmtex_icon_small_app_icon, img_size, img_uv_min, img_uv_max);              break;
        case wnddesktopmode_page_settings:   title_str = UIManager::Get()->GetSettingsWindow().DesktopModeGetTitle();
                                             UIManager::Get()->GetSettingsWindow().DesktopModeGetIconTextureInfo(img_size, img_uv_min, img_uv_max);          break;
        case wnddesktopmode_page_properties: title_str = UIManager::Get()->GetOverlayPropertiesWindow().DesktopModeGetTitle();
                                             UIManager::Get()->GetOverlayPropertiesWindow().DesktopModeGetIconTextureInfo(img_size, img_uv_min, img_uv_max); break;
    }

    ImGui::SetCursorPos(style.WindowPadding);

    ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

    ImGui::SetCursorPosY( (title_rect.w / 2.0f) - (ImGui::GetTextLineHeight() / 1.90f) );   //1.90f to adjust alignment a bit

    ImVec2 clip_end = ImGui::GetCursorScreenPos();
    clip_end.x  = back_button_x - style.FramePadding.x;
    clip_end.y += ImGui::GetFrameHeight();

    ImGui::PushClipRect(ImGui::GetCursorScreenPos(), clip_end, true);
    ImGui::TextUnformatted(title_str);
    ImGui::PopClipRect();

    float title_text_width = ImGui::GetItemRectSize().x;

    //Right end of title bar
    ImGui::PushStyleColor(ImGuiCol_Button, 0);

    const bool can_go_back = (m_PageStackPos != 0);

    if (!can_go_back)
        ImGui::PushItemDisabled();

    TextureManager::Get().GetTextureInfo(tmtex_icon_xxsmall_browser_back, img_size, img_uv_min, img_uv_max);
    ImVec2 img_size_line_height_back(img_size_line_height.x * 0.75f, img_size_line_height.y * 0.75f);

    ImGui::SetCursorScreenPos({back_button_x, title_rect.y + (title_rect.w / 2.0f) - (back_button_size.y / 2.0f) });

    ImGui::PushID("BackButton");
    if ( (ImGui::ImageButton(io.Fonts->TexID, img_size_line_height_back, img_uv_min, img_uv_max, 1)) || (ImGui::IsMouseClicked(3 /* MouseX1 / Back */)) )
    {
        bool did_go_back = false;

        switch (m_PageStack[m_PageStackPos])
        {
            case wnddesktopmode_page_settings:   did_go_back = UIManager::Get()->GetSettingsWindow().DesktopModeGoBack();          break;
            case wnddesktopmode_page_properties: did_go_back = UIManager::Get()->GetOverlayPropertiesWindow().DesktopModeGoBack(); break;
            default: break;
        }

        //If embedded page didn't go back, go back on our own pagination
        if (!did_go_back)
        {
            PageGoBack();
        }
    }
    ImGui::PopID();

    if (!can_go_back)
        ImGui::PopItemDisabled();

    back_button_size = ImGui::GetItemRectSize();

    ImGui::PopStyleColor();
    ImGui::PopClipRect();

    ImGui::SetCursorPosY(title_rect.w + style.WindowPadding.y);
}

void WindowDesktopMode::UpdatePageMain()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DesktopModeCatTools)); 
    ImGui::Indent();

    const float item_height = ImGui::GetTextLineHeight() + style.ItemInnerSpacing.y;
    ImGui::BeginChild("SettingsList", ImVec2(0.0f, (item_height * 3.0f) + style.ItemInnerSpacing.y), true);

    if (ImGui::Selectable(TranslationManager::GetString(tstr_DesktopModeToolSettings))) 
    {
        UIManager::Get()->GetSettingsWindow().DesktopModeSetRootPage(wndsettings_page_main);
        PageGoForward(wnddesktopmode_page_settings);
    }

    if (ImGui::Selectable("Switch to Action Editor")) 
    {
        UIManager::Get()->RestartIntoActionEditor();
    }

    if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsProfilesOverlays))) 
    {
        UIManager::Get()->GetSettingsWindow().DesktopModeSetRootPage(wndsettings_page_profiles);
        PageGoForward(wnddesktopmode_page_settings);
    }

    ImGui::EndChild();

    ImGui::Unindent();
    ImGui::Spacing();

    UpdatePageMainOverlayList();
}

void WindowDesktopMode::UpdatePageMainOverlayList()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
    ImVec2 img_size, img_uv_min, img_uv_max;

    //List of unique IDs for overlays so ImGui can identify the same list entries after reordering or list expansion (needed for drag reordering)
    static std::vector<int> list_unique_ids;
    static unsigned int drag_last_hovered_selectable = k_ulOverlayID_None;

    const int overlay_count = (int)OverlayManager::Get().GetOverlayCount();
    const unsigned int u_overlay_count = OverlayManager::Get().GetOverlayCount();

    //Reset unique IDs when page is appearing
    if (m_PageAppearing == wnddesktopmode_page_main)
    {
        list_unique_ids.clear();
    }

    //Expand unique id lists if overlays were added (also does initialization since it's empty then)
    while (list_unique_ids.size() < u_overlay_count)
    {
        list_unique_ids.push_back((int)list_unique_ids.size());
    }

    //List overlays in a scrollable child container
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DesktopModeCatOverlays)); 
    ImGui::Indent();

    ImGui::BeginChild("OverlayList", ImVec2(0.0f, 0.0f), true);

    static unsigned int hovered_overlay_id_last = k_ulOverlayID_None;
    static unsigned int right_down_overlay_id   = k_ulOverlayID_None;
    unsigned int hovered_overlay_id = k_ulOverlayID_None;

    ImVec4 color_text_hidden = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    color_text_hidden.w = 0.5f;

    for (unsigned int i = 0; i < u_overlay_count; ++i)
    {
        ImGui::PushID(list_unique_ids[i]);

        const bool is_active = ( (m_OverlayListActiveMenuID == i) || (right_down_overlay_id == i) );

        //Force active header color when a menu is active or right mouse button is held down
        if (is_active)
        {
            ImGui::PushStyleColor(ImGuiCol_Header,        ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        }

        //Use empty label here. Icon and actual label are manually created further down
        if (ImGui::Selectable("", is_active))
        {
            if (!m_IsDraggingOverlaySelectables)
            {
                UIManager::Get()->GetOverlayPropertiesWindow().SetActiveOverlayID(i, true);
                PageGoForward(wnddesktopmode_page_properties);

                HideMenus();
            }
        }

        if (is_active)
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemVisible())
        {
            if (ImGui::IsItemHovered())
            {
                hovered_overlay_id = i;
            }

            //Additional selectable behavior
            bool selectable_active = ImGui::IsItemActive();
            ImVec2 pos = ImGui::GetItemRectMin();
            float width = ImGui::GetItemRectSize().x;

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            {
                drag_last_hovered_selectable = i;
                hovered_overlay_id = i;

                if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
                {
                    if ((m_OverlayListActiveMenuID != i) && (!m_IsDraggingOverlaySelectables))
                    {
                        HideMenus();
                        m_OverlayListActiveMenuID = i;
                        ImGui::OpenPopup("OverlayListMenu");
                    }

                    selectable_active = true;
                }
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
                {
                    right_down_overlay_id = i;   //For correct right-click visual
                }
            }

            //Drag reordering
            if ((ImGui::IsItemActive()) && (!ImGui::IsItemHovered()))
            {
                int index_swap = i + ((ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y < 0.0f) ? -1 : 1);
                if ((drag_last_hovered_selectable != i) && (index_swap >= 0) && (index_swap < overlay_count))
                {
                    OverlayManager::Get().SwapOverlays(i, index_swap);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, i);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_swap, index_swap);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

                    std::iter_swap(list_unique_ids.begin() + i, list_unique_ids.begin() + index_swap);

                    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                    m_IsDraggingOverlaySelectables = true;

                    m_OverlayListActiveMenuID = k_ulOverlayID_None;
                }
            }

            if (m_OverlayListActiveMenuID == i)
            {
                MenuOverlayList(i, selectable_active);

                //Check if menu modified overlay count and bail then
                if (OverlayManager::Get().GetOverlayCount() != u_overlay_count)
                {
                    ImGui::PopID();
                    UIManager::Get()->RepeatFrame();
                    break;
                }

                hovered_overlay_id = i;
            }

            //Custom render the selectable label with icons
            OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);
            const ImVec4 tint_color = ImVec4(1.0f, 1.0f, 1.0f, data.ConfigBool[configid_bool_overlay_enabled] ? 1.0f : 0.5f); //Transparent when hidden
            //Overlay icon
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            TextureManager::Get().GetOverlayIconTextureInfo(data, img_size, img_uv_min, img_uv_max, true);
            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max, tint_color);

            //Origin icon
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            TextureManager::Get().GetTextureInfo((TMNGRTexID)(tmtex_icon_xsmall_origin_room + data.ConfigInt[configid_int_overlay_origin]), img_size, img_uv_min, img_uv_max);
            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max, tint_color);

            //Label
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::TextColoredUnformatted(data.ConfigBool[configid_bool_overlay_enabled] ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : color_text_hidden, data.ConfigNameStr.c_str());
        }

        ImGui::PopID();
    }

    //Don't change overlay highlight while mouse down as it won't be correct while dragging and flicker just before it
    if ( (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) && ((hovered_overlay_id_last != k_ulOverlayID_None) || (hovered_overlay_id != k_ulOverlayID_None)) )
    {
        UIManager::Get()->HighlightOverlay(hovered_overlay_id);
        hovered_overlay_id_last = hovered_overlay_id;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        m_IsDraggingOverlaySelectables = false;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
    {
        right_down_overlay_id = k_ulOverlayID_None;
    }

    ImGui::EndChild();
    ImGui::Unindent();
}

void WindowDesktopMode::MenuOverlayList(unsigned int overlay_id, bool is_item_active)
{
    m_MenuAlpha += ImGui::GetIO().DeltaTime * 12.0f;

    if (m_MenuAlpha > 1.0f)
        m_MenuAlpha = 1.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_MenuAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("OverlayListMenu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | 
                                                           ImGuiWindowFlags_AlwaysAutoResize))
    {
        if ( (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) && (ImGui::IsAnyMouseClicked()) && (!is_item_active) )
        {
            HideMenus();
        }

        bool& is_enabled = OverlayManager::Get().GetConfigData(overlay_id).ConfigBool[configid_bool_overlay_enabled];
        WindowOverlayProperties& properties_window = UIManager::Get()->GetOverlayPropertiesWindow();

        if (ImGui::Selectable(TranslationManager::GetString((is_enabled) ? tstr_OverlayBarOvrlHide : tstr_OverlayBarOvrlShow), false))
        {
            is_enabled = !is_enabled;

            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, (int)overlay_id);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_overlay_enabled, is_enabled);
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

            HideMenus();
        }

        if (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlClone), false))
        {
            //Copy data of overlay and add a new one based on it
            OverlayManager::Get().DuplicateOverlay(OverlayManager::Get().GetConfigData(overlay_id), overlay_id);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_duplicate, (int)overlay_id);

            HideMenus();
        }

        if (m_IsMenuRemoveConfirmationVisible)
        {
            if (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlRemoveConfirm), false))
            {
                OverlayManager::Get().RemoveOverlay(overlay_id);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_remove, overlay_id);

                HideMenus();
            }
        }
        else
        {
            if (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlRemove), false, ImGuiSelectableFlags_DontClosePopups))
            {
                m_IsMenuRemoveConfirmationVisible = true;
            }
        }

        //Clamp window to display rect so it's always visible
        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 window_pos  = ImGui::GetWindowPos();
        window_pos.x = clamp(window_pos.x, 0.0f, ImGui::GetIO().DisplaySize.x - window_size.x);
        window_pos.y = clamp(window_pos.y, 0.0f, ImGui::GetIO().DisplaySize.y - window_size.y);

        ImGui::SetWindowPos(window_pos);
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    ImGui::End();
}

void WindowDesktopMode::HideMenus()
{
    m_MenuAlpha = 0.0f;
    m_OverlayListActiveMenuID = k_ulOverlayID_None;
    m_IsMenuRemoveConfirmationVisible = false;

    UIManager::Get()->RepeatFrame();
}

void WindowDesktopMode::PageGoForward(WindowDesktopModePage new_page)
{
    m_PageStack.push_back(new_page);
    m_PageStackPos++;
}

void WindowDesktopMode::PageGoBack()
{
    if (m_PageStackPos != 0)
    {
        m_PageStackPos--;
        m_PageReturned = m_PageStack.back();
    }
}

void WindowDesktopMode::Update()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::SetNextWindowPos({0.0f,0.0f});
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("##WindowDesktopMode", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PopStyleVar(2);

    UpdateTitleBar();
    UIManager::Get()->GetSettingsWindow().UpdateDesktopModeWarnings();

    const float page_width = ImGui::GetIO().DisplaySize.x - style.WindowPadding.x - style.WindowPadding.x;

    //Page animation
    if (m_PageAnimationDir != 0)
    {
        m_PageAnimationProgress += ImGui::GetIO().DeltaTime * 3.0f;

        if (m_PageAnimationProgress >= 1.0f)
        {
            //Remove pages in the stack after finishing going back
            if (m_PageAnimationDir == 1)
            {
                while ((int)m_PageStack.size() > m_PageStackPosAnimation + 1)
                {
                    m_PageStack.pop_back();
                }
            }

            m_PageAnimationProgress = 1.0f;
            m_PageAnimationDir      = 0;
        }
    }
    else if (m_PageStackPosAnimation != m_PageStackPos) //Only start new animation if none is running
    {
        m_PageAnimationDir      = (m_PageStackPosAnimation < m_PageStackPos) ? -1 : 1;
        m_PageStackPosAnimation = m_PageStackPos;
        m_PageAnimationStartPos = m_PageAnimationOffset;
        m_PageAnimationProgress = 0.0f;

        //Set appearing value to top of stack when starting animation to it
        if (m_PageAnimationDir == -1)
        {
            m_PageAppearing = m_PageStack.back();
        }
    }

    const float target_x = (page_width + style.ItemSpacing.x) * -m_PageStackPosAnimation;
    m_PageAnimationOffset = smoothstep(m_PageAnimationProgress, m_PageAnimationStartPos, target_x);

    //Set up page offset and clipping
    ImGui::SetCursorPosX( (ImGui::GetCursorPosX() + m_PageAnimationOffset) );

    ImGui::PushClipRect({0.0f, 0.0f}, {FLT_MAX, FLT_MAX}, false);

    const char* const child_str_id[] {"DesktopModePageMain", "DesktopModePage1", "DesktopModePage2", "DesktopModePage3"}; //No point in generating these on the fly
    int child_id = 0;
    int stack_size = (int)m_PageStack.size();
    for (WindowDesktopModePage page_id : m_PageStack)
    {
        if (child_id >= IM_ARRAYSIZE(child_str_id))
            break;

        //Disable items when the page isn't active
        const bool is_inactive_page = (child_id + 1 < stack_size);

        if (is_inactive_page)
        {
            ImGui::PushItemDisabledNoVisual();
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f)); //This prevents child bg color being visible if there's a widget before this (e.g. warnings)

        if ( (ImGui::BeginChild(child_str_id[child_id], {page_width, ImGui::GetContentRegionAvail().y})) || (m_PageAppearing == page_id) ) //Process page if currently appearing
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg

            switch (page_id)
            {
                case wnddesktopmode_page_main:       UpdatePageMain();                                                   break;
                case wnddesktopmode_page_settings:   UIManager::Get()->GetSettingsWindow().UpdateDesktopMode();          break;
                case wnddesktopmode_page_properties: UIManager::Get()->GetOverlayPropertiesWindow().UpdateDesktopMode(); break;
                default: break;
            }
        }
        else
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg
        }

        if (is_inactive_page)
        {
            ImGui::PopItemDisabledNoVisual();
        }

        ImGui::EndChild();

        if (is_inactive_page)
        {
            ImGui::SameLine();
        }

        child_id++;
    }

    m_PageAppearing = wnddesktopmode_page_none;

    ImGui::PopClipRect();

    ImGui::End();
}
