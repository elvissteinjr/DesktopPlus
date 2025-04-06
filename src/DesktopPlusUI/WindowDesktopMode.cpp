#include "WindowDesktopMode.h"

#include "imgui.h"
#include "UIManager.h"
#include "InterprocessMessaging.h"
#include "WindowManager.h"
#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"

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

    m_TitleBarRect = {0.0f, 0.0f, ImGui::GetWindowSize().x, img_size_line_height.y + (style.WindowPadding.y * 2.0f)};

    const ImVec2 title_bar_rect_min(m_TitleBarRect.x, m_TitleBarRect.y), title_bar_rect_max(m_TitleBarRect.z, m_TitleBarRect.w);
    ImGui::PushClipRect(title_bar_rect_min, title_bar_rect_max, false);

    //Background color
    ImGui::GetWindowDrawList()->AddRectFilled({0.0f, 0.0f}, {m_TitleBarRect.z, m_TitleBarRect.w}, ImGui::GetColorU32(ImGuiCol_TitleBg));

    //Icon and title text
    static ImVec2 back_button_size;
    const float back_button_x = m_TitleBarRect.z - back_button_size.x - style.FramePadding.x;

    const char* title_str = nullptr;
    float title_icon_alpha = 1.0f;

    switch (m_PageStack[m_PageStackPos])
    {
        case wnddesktopmode_page_main:
        {
            title_str = "Desktop+";
            TextureManager::Get().GetTextureInfo(tmtex_icon_small_app_icon, img_size, img_uv_min, img_uv_max);
            break;
        }
        case wnddesktopmode_page_settings:
        case wnddesktopmode_page_profiles:
        case wnddesktopmode_page_app_profiles:
        case wnddesktopmode_page_actions:
        {
            const WindowSettings& window_settings = UIManager::Get()->GetSettingsWindow();
            title_str = window_settings.DesktopModeGetTitle();
            window_settings.DesktopModeGetIconTextureInfo(img_size, img_uv_min, img_uv_max);
            break;
        }
        case wnddesktopmode_page_properties:
        {
            const WindowOverlayProperties& window_properties = UIManager::Get()->GetOverlayPropertiesWindow();
            title_str        = window_properties.DesktopModeGetTitle();
            title_icon_alpha = window_properties.DesktopModeGetTitleIconAlpha();
            window_properties.DesktopModeGetIconTextureInfo(img_size, img_uv_min, img_uv_max);
            break;
        }
        case wnddesktopmode_page_add_window_overlay: 
        {
            title_str = TranslationManager::GetString(tstr_DesktopModePageAddWindowOverlayTitle);
            TextureManager::Get().GetTextureInfo(tmtex_icon_add, img_size, img_uv_min, img_uv_max);
            break;
        }
    }

    ImGui::SetCursorPos(style.WindowPadding);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, title_icon_alpha);

    ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

    if ((ImGui::IsItemClicked()) && (m_PageStack[m_PageStackPos] == wnddesktopmode_page_properties))
    {
        UIManager::Get()->GetOverlayPropertiesWindow().DesktopModeOnTitleIconClick();
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    ImGui::SetCursorPosY( (m_TitleBarRect.w / 2.0f) - (ImGui::GetTextLineHeight() / 1.90f) );   //1.90f to adjust alignment a bit

    ImVec2 clip_end = ImGui::GetCursorScreenPos();
    clip_end.x  = back_button_x - style.FramePadding.x;
    clip_end.y += ImGui::GetFrameHeight();

    ImGui::PushClipRect(ImGui::GetCursorScreenPos(), clip_end, true);
    ImGui::TextUnformatted(title_str);
    ImGui::PopClipRect();

    ImGui::PopStyleVar();

    float title_text_width = ImGui::GetItemRectSize().x;

    //Right end of title bar
    ImGui::PushStyleColor(ImGuiCol_Button, 0);

    const bool can_go_back = (m_PageStackPos != 0);

    if (!can_go_back)
        ImGui::PushItemDisabled();

    TextureManager::Get().GetTextureInfo(tmtex_icon_xxsmall_browser_back, img_size, img_uv_min, img_uv_max);
    ImVec2 img_size_line_height_back(img_size_line_height.x * 0.75f, img_size_line_height.y * 0.75f);

    ImGui::SetCursorScreenPos({back_button_x, m_TitleBarRect.y + (m_TitleBarRect.w / 2.0f) - (back_button_size.y / 2.0f) });

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
    if ( (ImGui::ImageButton("BackButton", io.Fonts->TexID, img_size_line_height_back, img_uv_min, img_uv_max)) || 
         (ImGui::IsMouseClicked(3 /* MouseX1 / Back */)) || ( (ImGui::IsKeyPressed(ImGuiKey_Backspace)) && (!ImGui::IsAnyInputTextActive()) ) )
    {
        if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
        {
            bool did_go_back = false;

            switch (m_PageStack[m_PageStackPos])
            {
                case wnddesktopmode_page_settings:     /*fallthrough*/
                case wnddesktopmode_page_profiles:     /*fallthrough*/
                case wnddesktopmode_page_app_profiles: /*fallthrough*/
                case wnddesktopmode_page_actions:      did_go_back = UIManager::Get()->GetSettingsWindow().DesktopModeGoBack();          break;
                case wnddesktopmode_page_properties:   did_go_back = UIManager::Get()->GetOverlayPropertiesWindow().DesktopModeGoBack(); break;
                default: break;
            }

            //If embedded page didn't go back, go back on our own pagination
            if (!did_go_back)
            {
                PageGoBack();
            }
        }
    }
    ImGui::PopStyleVar();

    if (!can_go_back)
        ImGui::PopItemDisabled();

    back_button_size = ImGui::GetItemRectSize();

    //Check if title bar is hovered (except the back button) and notify embedded page windows that need this info
    if (m_PageStack[m_PageStackPos] == wnddesktopmode_page_properties)
    {
        const bool is_title_bar_hovered = ( (!ImGui::IsItemHovered()) && (ImGui::IsMouseHoveringRect(title_bar_rect_min, title_bar_rect_max)) );
        UIManager::Get()->GetOverlayPropertiesWindow().DesktopModeOnTitleBarHover(is_title_bar_hovered);
    }

    ImGui::PopStyleColor();
    ImGui::PopClipRect();

    ImGui::SetCursorPosY(m_TitleBarRect.w + style.WindowPadding.y);
}

void WindowDesktopMode::UpdatePageMain()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DesktopModeCatTools)); 
    ImGui::Indent();

    const float item_height = ImGui::GetTextLineHeight() + style.ItemInnerSpacing.y;
    ImGui::BeginChild("SettingsList", ImVec2(0.0f, (item_height * 4.0f) + style.ItemInnerSpacing.y), true);

    //Focus nav if we came back from settings
    if ( (io.NavVisible) && (m_PageReturned == wnddesktopmode_page_settings) )
    {
        ImGui::SetKeyboardFocusHere();
        m_PageReturned = wnddesktopmode_page_none;
    }

    if (ImGui::Selectable(TranslationManager::GetString(tstr_DesktopModeToolSettings))) 
    {
        UIManager::Get()->GetSettingsWindow().DesktopModeSetRootPage(wndsettings_page_main);
        PageGoForward(wnddesktopmode_page_settings);
    }

    //Focus nav if we came back from profiles
    if ( (io.NavVisible) && (m_PageReturned == wnddesktopmode_page_profiles) )
    {
        ImGui::SetKeyboardFocusHere();
        m_PageReturned = wnddesktopmode_page_none;
    }

    if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsProfilesOverlays))) 
    {
        UIManager::Get()->GetSettingsWindow().DesktopModeSetRootPage(wndsettings_page_profiles);
        PageGoForward(wnddesktopmode_page_profiles);
    }

    //Focus nav if we came back from app profiles
    if ( (io.NavVisible) && (m_PageReturned == wnddesktopmode_page_app_profiles) )
    {
        ImGui::SetKeyboardFocusHere();
        m_PageReturned = wnddesktopmode_page_none;
    }

    if (ImGui::Selectable(TranslationManager::GetString(tstr_SettingsProfilesApps))) 
    {
        UIManager::Get()->GetSettingsWindow().DesktopModeSetRootPage(wndsettings_page_app_profiles);
        PageGoForward(wnddesktopmode_page_app_profiles);
    }

    //Focus nav if we came back from actions
    if ( (io.NavVisible) && (m_PageReturned == wnddesktopmode_page_actions) )
    {
        ImGui::SetKeyboardFocusHere();
        m_PageReturned = wnddesktopmode_page_none;
    }

    if (ImGui::Selectable(TranslationManager::GetString(tstr_DesktopModeToolActions))) 
    {
        UIManager::Get()->GetSettingsWindow().DesktopModeSetRootPage(wndsettings_page_actions);
        PageGoForward(wnddesktopmode_page_actions);
    }

    ImGui::EndChild();

    ImGui::Unindent();
    ImGui::Spacing();

    UpdatePageMainOverlayList();
}

void WindowDesktopMode::UpdatePageMainOverlayList()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();

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

    static unsigned int hovered_overlay_id_last     = k_ulOverlayID_None;
    static unsigned int right_down_overlay_id       = k_ulOverlayID_None;
    static unsigned int keyboard_swapped_overlay_id = k_ulOverlayID_None;
    static bool add_overlay_right_down = false;
    unsigned int hovered_overlay_id = k_ulOverlayID_None;
    bool has_swapped = false;

    ImVec4 color_text_hidden = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    color_text_hidden.w = 0.5f;

    for (unsigned int i = 0; i < u_overlay_count; ++i)
    {
        ImGui::PushID(list_unique_ids[i]);

        //Force active header color when a menu is active or right mouse button is held down
        const bool is_active = ( (m_OverlayListActiveMenuID == i) || (right_down_overlay_id == i) );

        if (is_active)
        {
            ImGui::PushStyleColor(ImGuiCol_Header,        ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        }

        //Set focus for nav if we just came back from the properties page and this is the active overlay
        if ( (io.NavVisible) && (m_PageReturned == wnddesktopmode_page_properties) && (UIManager::Get()->GetOverlayPropertiesWindow().GetActiveOverlayID() == i) )
        {
            ImGui::SetKeyboardFocusHere();
            m_PageReturned = wnddesktopmode_page_none;
        }

        //Set focus for nav if we previously re-ordered overlays via keyboard
        if (keyboard_swapped_overlay_id == i)
        {
            ImGui::SetKeyboardFocusHere();

            //Nav works against us here, so keep setting focus until ctrl isn't down anymore
            if ((!io.KeyCtrl) || (!io.NavVisible))
            {
                keyboard_swapped_overlay_id = k_ulOverlayID_None;
            }
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

            if ( (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) || ((io.NavVisible) && (ImGui::IsItemFocused())) )
            {
                drag_last_hovered_selectable = i;
                hovered_overlay_id = i;

                if ( (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) || (ImGui::IsKeyReleased(ImGuiKey_Menu)) )
                {
                    if ((m_OverlayListActiveMenuID != i) && (!m_IsDraggingOverlaySelectables))
                    {
                        HideMenus();
                        m_OverlayListActiveMenuID = i;
                        ImGui::OpenPopup("OverlayListMenu");
                    }

                    selectable_active = true;
                }
                else if ( (ImGui::IsMouseDown(ImGuiMouseButton_Right)) || (ImGui::IsKeyDown(ImGuiKey_Menu)) )
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

            //Keyboard reordering
            if ((io.NavVisible) && (io.KeyCtrl) && (hovered_overlay_id == i))
            {
                int index_swap = i + ((ImGui::IsNavInputPressed(ImGuiNavInput_DpadDown, true)) ? 1 : (ImGui::IsNavInputPressed(ImGuiNavInput_DpadUp, true)) ? -1 : 0);
                if ((i != index_swap) && (index_swap >= 0) && (index_swap < overlay_count))
                {
                    OverlayManager::Get().SwapOverlays(i, index_swap);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, i);
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_swap, index_swap);
                    IPCManager::Get().PostConfigMessageToDashboardApp(configid_int_state_overlay_current_id_override, -1);

                    std::iter_swap(list_unique_ids.begin() + i, list_unique_ids.begin() + index_swap);

                    m_OverlayListActiveMenuID = k_ulOverlayID_None;

                    //Skip the rest of this frame to avoid double-swaps
                    keyboard_swapped_overlay_id = index_swap;
                    ImGui::PopID();
                    UIManager::Get()->RepeatFrame();
                    break;
                }
            }

            if (m_OverlayListActiveMenuID == i)
            {
                MenuOverlayList(i);

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
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            TextureManager::Get().GetOverlayIconTextureInfo(data, img_size, img_uv_min, img_uv_max, true);
            ImGui::ImageWithBg(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max, {0, 0, 0, 0}, tint_color);

            //Origin icon
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            TextureManager::Get().GetTextureInfo((TMNGRTexID)(tmtex_icon_xsmall_origin_room + data.ConfigInt[configid_int_overlay_origin]), img_size, img_uv_min, img_uv_max);
            ImGui::ImageWithBg(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max, {0, 0, 0, 0}, tint_color);

            //Label
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            ImGui::TextColoredUnformatted(data.ConfigBool[configid_bool_overlay_enabled] ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : color_text_hidden, data.ConfigNameStr.c_str());
        }

        ImGui::PopID();
    }

    //Static Add Overlay entry
    {
        if (u_overlay_count != 0)
        {
            ImGui::Separator();
        }

        //Force active header color when a menu is active or right mouse button is held down
        const bool is_active = ( (m_IsOverlayAddMenuVisible) || (add_overlay_right_down) );

        if (is_active)
        {
            ImGui::PushStyleColor(ImGuiCol_Header,        ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        }

        //Use empty label here. Icon and actual label are manually created further down
        if (ImGui::Selectable("##AddOverlay", is_active))
        {
            if (!m_IsDraggingOverlaySelectables)
            {
                HideMenus();
                m_IsOverlayAddMenuVisible = true;
                ImGui::OpenPopup("AddOverlayMenu");
            }
        }

        if (is_active)
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemVisible())
        {
            //Also allow right-click/menu key as the result looks like a context menu
            if ((ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) || ((io.NavVisible) && (ImGui::IsItemFocused())))
            {
                if ((ImGui::IsMouseReleased(ImGuiMouseButton_Right)) || (ImGui::IsKeyReleased(ImGuiKey_Menu)))
                {
                    if ((!m_IsOverlayAddMenuVisible) && (!m_IsDraggingOverlaySelectables))
                    {
                        HideMenus();
                        m_IsOverlayAddMenuVisible = true;
                        ImGui::OpenPopup("AddOverlayMenu");
                    }
                }
                else if ((ImGui::IsMouseDown(ImGuiMouseButton_Right)) || (ImGui::IsKeyDown(ImGuiKey_Menu)))
                {
                    add_overlay_right_down = true;   //For correct right-click visual
                }
            }

            //Custom render the selectable label with icon
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            TextureManager::Get().GetTextureInfo(tmtex_icon_add, img_size, img_uv_min, img_uv_max);
            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

            //Label
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_DesktopModeOverlayListAdd));
        }

        if (m_IsOverlayAddMenuVisible)
        {
            MenuAddOverlay();
        }
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

    if ( (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) || (ImGui::IsKeyReleased(ImGuiKey_Menu)) )
    {
        right_down_overlay_id = k_ulOverlayID_None;
        add_overlay_right_down = false;
    }

    ImGui::EndChild();
    ImGui::Unindent();
}

void WindowDesktopMode::UpdatePageAddWindowOverlay()
{
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_DesktopModePageAddWindowOverlayHeader));
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::BeginChild("WindowList", ImVec2(0.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y), true);

    ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
    ImVec2 img_size, img_uv_min, img_uv_max;

    //List windows
    for (const auto& window_info : WindowManager::Get().WindowListGet())
    {
        ImGui::PushID(window_info.GetWindowHandle());
        if (ImGui::Selectable(""))
        {
            //Add overlay
            HWND window_handle = window_info.GetWindowHandle();
            OverlayManager::Get().AddOverlay(ovrl_capsource_winrt_capture, -2, window_handle);

            //Send to dashboard app
            IPCManager::Get().PostConfigMessageToDashboardApp(configid_handle_state_arg_hwnd, (LPARAM)window_handle);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_new, -2);

            PageGoBack();
        }

        ImGui::SameLine(0.0f, 0.0f);

        int icon_id = TextureManager::Get().GetWindowIconCacheID(window_info.GetIcon());

        if (icon_id != -1)
        {
            TextureManager::Get().GetWindowIconTextureInfo(icon_id, img_size, img_uv_min, img_uv_max);
            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }

        ImGui::TextUnformatted(window_info.GetListTitle().c_str());

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::Unindent();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()));

    //--Cancel button
    //No separator since this is right after a boxed child window

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel)))
    {
        PageGoBack();
    }
}

void WindowDesktopMode::MenuOverlayList(unsigned int overlay_id)
{
    m_MenuAlpha += ImGui::GetIO().DeltaTime * 12.0f;

    if (m_MenuAlpha > 1.0f)
        m_MenuAlpha = 1.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_MenuAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    if (ImGui::BeginPopup("OverlayListMenu", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                             ImGuiWindowFlags_AlwaysAutoResize))
    {
        if ( (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) && (ImGui::IsAnyMouseClicked()) )
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

            if (ImGui::IsNavInputReleased(ImGuiNavInput_Activate))
            {
                ImGui::SetKeyboardFocusHere(-1);
            }
        }
        else
        {
            if (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlRemove), false, ImGuiSelectableFlags_NoAutoClosePopups))
            {
                m_IsMenuRemoveConfirmationVisible = true;
                UIManager::Get()->RepeatFrame();
            }
        }

        if (ImGui::IsNavInputPressed(ImGuiNavInput_Cancel))
        {
            HideMenus();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    //Handle popup being closed from nav
    if (!ImGui::IsPopupOpen("OverlayListMenu"))
    {
        HideMenus();
    }
}

void WindowDesktopMode::MenuAddOverlay()
{
    m_MenuAlpha += ImGui::GetIO().DeltaTime * 12.0f;

    if (m_MenuAlpha > 1.0f)
        m_MenuAlpha = 1.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_MenuAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    if (ImGui::BeginPopup("AddOverlayMenu", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                            ImGuiWindowFlags_AlwaysAutoResize))
    {
        if ( (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) && (ImGui::IsAnyMouseClicked()) )
        {
            HideMenus();
        }

        int desktop_count = ConfigManager::GetValue(configid_int_state_interface_desktop_count);

        int new_overlay_desktop_id = -255;

        for (int i = 0; i < desktop_count; ++i)
        {
            ImGui::PushID(i);

            if (ImGui::Selectable(TranslationManager::Get().GetDesktopIDString(i)))
            {
                new_overlay_desktop_id = i;
            }

            ImGui::PopID();
        }

        if ( (DPWinRT_IsCaptureSupported()) && (ImGui::Selectable(TranslationManager::GetString(tstr_OverlayBarOvrlAddWindow), false)) )
        {
            HideMenus();
            PageGoForward(wnddesktopmode_page_add_window_overlay);
        }

        //Don't offer Performance Monitor in desktop mode as this kind of overlay won't be visible after creation since its window isn't rendered in desktop mode
        /*if (ImGui::Selectable(TranslationManager::GetString(tstr_SourcePerformanceMonitor)))
        {
            new_overlay_desktop_id = -3;
        }*/

        if (DPBrowserAPIClient::Get().IsBrowserAvailable())
        {
            if (ImGui::Selectable(TranslationManager::GetString(tstr_SourceBrowser)))
            {
                new_overlay_desktop_id = -4;
            }
        }

        //Create new overlay if desktop or UI/Browser selectables were triggered
        if (new_overlay_desktop_id != -255)
        {
            //Choose capture source
            OverlayCaptureSource capsource;

            switch (new_overlay_desktop_id)
            {
                case -3: capsource = ovrl_capsource_ui;      break;
                case -4: capsource = ovrl_capsource_browser; break;
                default: capsource = ovrl_capsource_desktop_duplication;
            }

            //Add overlay and sent to dashboard app
            unsigned int overlay_id_new = OverlayManager::Get().AddOverlay(capsource, new_overlay_desktop_id);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_new, new_overlay_desktop_id);

            //Adjust width to a more suited default (done here as well so it's set even without dashboard app running)
            OverlayManager::Get().GetConfigData(overlay_id_new).ConfigFloat[configid_float_overlay_width] = 1.0f;

            HideMenus();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    //Handle popup being closed from nav
    if (!ImGui::IsPopupOpen("AddOverlayMenu"))
    {
        HideMenus();
    }
}

void WindowDesktopMode::HideMenus()
{
    m_MenuAlpha = 0.0f;
    m_OverlayListActiveMenuID = k_ulOverlayID_None;
    m_IsOverlayAddMenuVisible = false;
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
        //Use the averaged framerate value instead of delta time for the first animation step
        //This is to smooth over increased frame deltas that can happen when a new page needs to do initial larger computations or save/load files
        const float progress_step = (m_PageAnimationProgress == 0.0f) ? (1.0f / ImGui::GetIO().Framerate) * 3.0f : ImGui::GetIO().DeltaTime * 3.0f;
        m_PageAnimationProgress += progress_step;

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
    const ImVec2 child_size = {page_width, ImGui::GetContentRegionAvail().y};
    int child_id = 0;
    int stack_size = (int)m_PageStack.size();
    for (WindowDesktopModePage page_id : m_PageStack)
    {
        if (child_id >= IM_ARRAYSIZE(child_str_id))
            break;

        //Disable items when the page isn't active
        const bool is_inactive_page = (child_id != m_PageStackPos);

        if (is_inactive_page)
        {
            ImGui::PushItemDisabledNoVisual();
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f)); //This prevents child bg color being visible if there's a widget before this (e.g. warnings)

        if ( (ImGui::BeginChild(child_str_id[child_id], child_size, ImGuiChildFlags_NavFlattened)) || (m_PageAppearing == page_id) ) //Process page if currently appearing
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg

            switch (page_id)
            {
                case wnddesktopmode_page_main:               UpdatePageMain();                                                   break;
                case wnddesktopmode_page_settings:           /*fallthrough*/
                case wnddesktopmode_page_profiles:           /*fallthrough*/
                case wnddesktopmode_page_app_profiles:       /*fallthrough*/
                case wnddesktopmode_page_actions:            UIManager::Get()->GetSettingsWindow().UpdateDesktopMode();          break;
                case wnddesktopmode_page_properties:         UIManager::Get()->GetOverlayPropertiesWindow().UpdateDesktopMode(); break;
                case wnddesktopmode_page_add_window_overlay: UpdatePageAddWindowOverlay();                                       break;
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

        if (child_id + 1 < stack_size)
        {
            ImGui::SameLine();
        }

        child_id++;
    }

    m_PageAppearing = wnddesktopmode_page_none;

    ImGui::PopClipRect();

    ImGui::End();

    //Display any potential Aux UI on top of this window
    UIManager::Get()->GetAuxUI().Update();
}

ImVec4 WindowDesktopMode::GetTitleBarRect() const
{
    return m_TitleBarRect;
}
