#include "WindowKeyboardEditor.h"

#include "UIManager.h"

void KeyboardEditor::UpdateWindowKeyList()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    ImGui::SetNextWindowSize({io.DisplaySize.x / 4.0f, m_TopWindowHeight});
    ImGui::SetNextWindowPos({0.0f, 0.0f});

    ImGui::Begin(TranslationManager::GetString(tstr_KeyboardEditorKeyListTitle), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (float)(int)(style.WindowPadding.y / 2.0f));   //Bring tab bar closer to window title
    if (ImGui::BeginTabBar("TabBarSublayouts"))
    {
        for (int i_sublayout = kbdlayout_sub_base; i_sublayout < kbdlayout_sub_MAX; ++i_sublayout)
        {
            KeyboardLayoutSubLayout current_sublayout = (KeyboardLayoutSubLayout)i_sublayout;

            if ((current_sublayout == kbdlayout_sub_altgr) && (!vr_keyboard.GetLayoutMetadata().HasAltGr))
                continue;

            if (ImGui::BeginTabItem( TranslationManager::GetString((TRMGRStrID)(tstr_KeyboardEditorSublayoutBase + i_sublayout) ), nullptr))
            {
                auto& sublayout_keys = vr_keyboard.GetLayout(current_sublayout);

                //Tab context menu
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::BeginMenu(TranslationManager::GetString(tstr_KeyboardEditorKeyListTabContextReplace)))
                    {
                        for (int i_sublayout_menu = kbdlayout_sub_base; i_sublayout_menu < kbdlayout_sub_MAX; ++i_sublayout_menu)
                        {
                            if ((i_sublayout_menu == kbdlayout_sub_altgr) && (!vr_keyboard.GetLayoutMetadata().HasAltGr))
                                continue;

                            if ((i_sublayout_menu != i_sublayout) && (ImGui::MenuItem( TranslationManager::GetString((TRMGRStrID)(tstr_KeyboardEditorSublayoutBase + i_sublayout_menu)) )))
                            {
                                HistoryPush();
                                sublayout_keys = vr_keyboard.GetLayout((KeyboardLayoutSubLayout)i_sublayout_menu);
                                UIManager::Get()->RepeatFrame();
                            }
                        }

                        ImGui::EndMenu();
                    }

                    if (ImGui::MenuItem(TranslationManager::GetString(tstr_KeyboardEditorKeyListTabContextClear)))
                    {
                        HistoryPush();
                        sublayout_keys.clear();
                        UIManager::Get()->RepeatFrame();
                    }

                    ImGui::EndPopup();
                }

                //Make the child expand past window padding
                ImGui::SetCursorPos({ImGui::GetCursorPosX() - style.WindowPadding.x, ImGui::GetCursorPosY() - style.WindowPadding.y - style.TabBarBorderSize});

                ImGui::BeginChild("TabContents", {ImGui::GetContentRegionAvail().x + style.WindowPadding.x, -ImGui::GetFrameHeightWithSpacing()}, ImGuiChildFlags_Borders);

                if (current_sublayout != m_SelectedSublayout)
                {
                    m_SelectedKeyID = FindKeyWithClosestPosInNewSubLayout(m_SelectedKeyID, m_SelectedSublayout, current_sublayout);
                    m_SelectedSublayout = current_sublayout;

                    m_HasChangedSelectedKey = true;
                }

                int row_id = -1;
                int row_id_show = -1;
                int i = 0;

                //Drag-reordering needs stable IDs to work
                static std::vector<int> list_unique_ids;
                static std::vector<int> list_unique_row_ids;

                while (list_unique_ids.size() < sublayout_keys.size())
                {
                    list_unique_ids.push_back((int)list_unique_ids.size());
                }

                static std::vector<std::string> str_row_id;

                //Find which row to open in order to focus the newly selected key
                if (m_HasChangedSelectedKey)
                {
                    if (m_SelectedRowID != -1)
                    {
                        row_id_show = m_SelectedRowID;
                    }
                    else
                    {
                        row_id_show = 0;
                        for (const auto& key : vr_keyboard.GetLayout(current_sublayout))
                        {
                            if (i == m_SelectedKeyID)
                            {
                                break;
                            }
                            else if (key.IsRowEnd)
                            {
                                ++row_id_show;
                            }

                            ++i;
                        }
                    }

                    i = 0;
                }

                bool pushed_row = false;
                bool is_row_node_open = false;

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {style.ItemSpacing.x, 0.0f});

                for (auto& key : vr_keyboard.GetLayout(current_sublayout))
                {
                    //Row entry
                    if (!pushed_row)
                    {
                        row_id++;

                        //Expand unique row IDs here since we don't know how many there are beforehand
                        while (list_unique_row_ids.size() <= row_id)
                        {
                            list_unique_row_ids.push_back((int)list_unique_row_ids.size());
                        }

                        //Expand row ID translated strings if we need to
                        while (str_row_id.size() <= row_id)
                        {
                            std::string row_str = TranslationManager::GetString(tstr_KeyboardEditorKeyListRow);
                            StringReplaceAll(row_str, "%ID%", std::to_string(str_row_id.size() + 1));
                            str_row_id.push_back(row_str);
                        }

                        //Set focus if row itself was just selected
                        if ((m_HasChangedSelectedKey) && (row_id == m_SelectedRowID))
                        {
                            ImGui::SetKeyboardFocusHere();
                            UIManager::Get()->RepeatFrame();
                        }
                        else if (row_id == row_id_show)     //Open row when this is the row that has the key we want to focus
                        {
                            ImGui::SetNextItemOpen(true);
                        }

                        ImGui::PushID(list_unique_row_ids[row_id]);

                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
                        flags |= (row_id == m_SelectedRowID) ? ImGuiTreeNodeFlags_Selected : 0;

                        is_row_node_open = ImGui::TreeNodeEx("##Row", flags, str_row_id[row_id].c_str());

                        if ((ImGui::IsItemClicked()) && (!ImGui::IsItemToggledOpen()))
                        {
                            m_SelectedRowID = row_id;
                            m_SelectedKeyID = -1;
                            m_HasChangedSelectedKey = true;
                        }

                        //Drag reordering
                        static int hovered_row_id = -1;
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                        {
                            hovered_row_id = row_id;
                        }

                        if ((ImGui::IsItemActive()) && (!ImGui::IsItemHovered()))
                        {
                            int row_id_swap = row_id + ((ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y < 0.0f) ? -1 : 1);
                            if ((hovered_row_id != row_id) && (row_id_swap >= 0) && (row_id_swap < sublayout_keys.size()))
                            {
                                HistoryPush();
                                m_HistoryHasPendingEdit = true;

                                auto pair_row      = GetKeyRowRange(current_sublayout, row_id);
                                auto pair_row_swap = GetKeyRowRange(current_sublayout, row_id_swap);

                                //Swap the row ranges if needed so the second row is always after the first
                                if (pair_row.first > pair_row_swap.first)
                                {
                                    pair_row.swap(pair_row_swap);
                                }

                                //Swap row contents by copying the first row to a temporary, erasing the first row and pasting the first row past where the second row now is
                                std::vector<KeyboardLayoutKey> row_keys(sublayout_keys.begin() + pair_row.first, sublayout_keys.begin() + pair_row.second);
                                sublayout_keys.erase(sublayout_keys.begin()  + pair_row.first, sublayout_keys.begin() + pair_row.second);
                                sublayout_keys.insert(sublayout_keys.begin() + pair_row.first + (pair_row_swap.second - pair_row_swap.first), row_keys.begin(), row_keys.end());

                                std::iter_swap(list_unique_row_ids.begin() + row_id, list_unique_row_ids.begin() + row_id_swap);

                                m_SelectedRowID = row_id_swap;

                                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                                UIManager::Get()->RepeatFrame();
                            }
                        }

                        pushed_row = true;
                    }

                    //Key entry
                    if (is_row_node_open)
                    {
                        ImGui::PushID(list_unique_ids[i]);

                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                        flags |= (i == m_SelectedKeyID) ? ImGuiTreeNodeFlags_Selected : 0;

                        if ((m_HasChangedSelectedKey) && (i == m_SelectedKeyID))
                        {
                            ImGui::SetKeyboardFocusHere();
                            UIManager::Get()->RepeatFrame();
                        }

                        if (!m_PreviewCluster[key.KeyCluster])
                            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

                        if (key.KeyType == kbdlayout_key_blank_space)
                        {
                            ImGui::TreeNodeEx(TranslationManager::GetString(tstr_KeyboardEditorKeyListSpacing), flags);
                        }
                        else
                        {
                            ImGui::TreeNodeEx(key.Label.c_str(), flags);
                        }

                        if (!m_PreviewCluster[key.KeyCluster])
                            ImGui::PopStyleVar();

                        if ((ImGui::IsItemClicked()) && (!ImGui::IsItemToggledOpen()))
                        {
                            m_SelectedRowID = -1;
                            m_SelectedKeyID = i;
                            m_HasChangedSelectedKey = true;
                        }

                        //Drag reordering
                        static int hovered_id = -1;
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                        {
                            hovered_id = i;
                        }

                        if ((ImGui::IsItemActive()) && (!ImGui::IsItemHovered()))
                        {
                            int index_swap = i + ((ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y < 0.0f) ? -1 : 1);
                            if ((hovered_id != i) && (index_swap >= 0) && (index_swap < sublayout_keys.size()))
                            {
                                HistoryPush();
                                m_HistoryHasPendingEdit = true;

                                KeyboardLayoutKey& key_swap = sublayout_keys[index_swap];
                                std::swap(key.IsRowEnd, key_swap.IsRowEnd);

                                std::iter_swap(sublayout_keys.begin()  + i, sublayout_keys.begin()  + index_swap);
                                std::iter_swap(list_unique_ids.begin() + i, list_unique_ids.begin() + index_swap);

                                m_SelectedKeyID = index_swap;

                                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                            }
                        }

                        ImGui::PopID();
                    }

                    if (key.IsRowEnd)
                    {
                        ImGui::PopID();
                        if (is_row_node_open)
                        {
                            ImGui::TreePop();
                        }

                        pushed_row = false;
                    }

                    ++i;
                }

                if (pushed_row)
                {
                    ImGui::TreePop();
                    ImGui::PopID();
                }

                //Clamp selected row ID to available range (with invalid 0 being corrected to -1)
                m_SelectedRowID = clamp(m_SelectedRowID, -1, row_id);

                ImGui::PopStyleVar();

                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();
    }

    const bool has_no_undo = m_HistoryUndo.empty();
    const bool has_no_redo = m_HistoryRedo.empty();

    //ImGui::IsKeyChordPressed() seems neat here but doesn't appear to support key repeat...
    const bool undo_shortcut_pressed = ((io.KeyMods == ImGuiMod_Ctrl) && (ImGui::IsKeyPressed(ImGuiKey_Z)));
    const bool redo_shortcut_pressed = ((io.KeyMods == ImGuiMod_Ctrl) && (ImGui::IsKeyPressed(ImGuiKey_Y)));

    if (has_no_undo)
        ImGui::PushItemDisabled();

    if ( (ImGui::Button(TranslationManager::GetString(tstr_DialogUndo))) || ((!has_no_undo) && (!ImGui::IsAnyInputTextActive()) && (undo_shortcut_pressed)) )
    {
        HistoryUndo();
        UIManager::Get()->RepeatFrame();
    }

    if (has_no_undo)
        ImGui::PopItemDisabled();

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    if (has_no_redo)
        ImGui::PushItemDisabled();

    if ( (ImGui::Button(TranslationManager::GetString(tstr_DialogRedo))) || ((!has_no_redo) && (!ImGui::IsAnyInputTextActive()) && (redo_shortcut_pressed)) )
    {
        HistoryRedo();
        UIManager::Get()->RepeatFrame();
    }

    if (has_no_redo)
        ImGui::PopItemDisabled();

    ImGui::SameLine();

    static float bottom_buttons_width = 0.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - bottom_buttons_width);
    ImGui::BeginGroup();

    if (ImGui::Button(TranslationManager::GetString(tstr_KeyboardEditorKeyListKeyAdd)))
    {
        HistoryPush();

        auto& sublayout_keys = vr_keyboard.GetLayout(m_SelectedSublayout);
        KeyboardLayoutKey new_key;
        new_key.KeyType = kbdlayout_key_virtual_key;

        auto it_insert = sublayout_keys.end();

        if (m_SelectedRowID != -1)
        {
            int row_id = 0;
            for (auto it = sublayout_keys.begin(); it != sublayout_keys.end(); ++it)
            {
                if (row_id == m_SelectedRowID + 1)
                {
                    it_insert = it;
                    break;
                }
                else if (it->IsRowEnd)
                {
                    ++row_id;
                }
            }

            //Set row end for previous key to make a new row
            if (it_insert != sublayout_keys.begin())
            {
                (it_insert - 1)->IsRowEnd = true;
            }

            new_key.IsRowEnd = true;
        }
        else if (m_SelectedKeyID != -1)
        {
            it_insert = sublayout_keys.begin() + m_SelectedKeyID;

            //Take over row end if selected key is
            if (it_insert->IsRowEnd)
            {
                it_insert->IsRowEnd = false;
                new_key.IsRowEnd = true;
            }

            ++it_insert;    //Insert after selected key
        }
        else
        {
            new_key.IsRowEnd = true;
        }

        new_key.Label = "#" + std::to_string(sublayout_keys.size());

        sublayout_keys.insert(it_insert, new_key);
        UIManager::Get()->RepeatFrame();
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    const bool has_invalid_selection = ((m_SelectedKeyID == -1) && (m_SelectedRowID == -1));

    if (has_invalid_selection)
        ImGui::PushItemDisabled();

    if (ImGui::Button(TranslationManager::GetString(tstr_KeyboardEditorKeyListKeyDuplicate)))
    {        
        auto& sublayout_keys = vr_keyboard.GetLayout(m_SelectedSublayout);

        if (m_SelectedRowID != -1)
        {
            HistoryPush();

            auto pair_row_key_id = GetKeyRowRange(m_SelectedSublayout, m_SelectedRowID);
            sublayout_keys.insert(sublayout_keys.begin() + pair_row_key_id.second, sublayout_keys.begin() + pair_row_key_id.first, sublayout_keys.begin() + pair_row_key_id.second);
        }
        else if (m_SelectedKeyID != -1)
        {
            HistoryPush();

            auto it_duplicate = sublayout_keys.begin() + m_SelectedKeyID;

            KeyboardLayoutKey new_key = *it_duplicate;

            //Take over row end if selected key is
            if (it_duplicate->IsRowEnd)
            {
                it_duplicate->IsRowEnd = false;
                new_key.IsRowEnd = true;
            }

            sublayout_keys.insert(it_duplicate + 1, new_key);
        }

        UIManager::Get()->RepeatFrame();
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    if (ImGui::Button(TranslationManager::GetString(tstr_KeyboardEditorKeyListKeyRemove)))
    {
        auto& sublayout_keys = vr_keyboard.GetLayout(m_SelectedSublayout);

        if (m_SelectedRowID != -1)
        {
            HistoryPush();

            auto pair_row_key_id = GetKeyRowRange(m_SelectedSublayout, m_SelectedRowID);
            sublayout_keys.erase(sublayout_keys.begin() + pair_row_key_id.first, sublayout_keys.begin() + pair_row_key_id.second);
        }
        else if (m_SelectedKeyID != -1)
        {
            HistoryPush();

            auto it_erase = sublayout_keys.begin() + m_SelectedKeyID;

            //Set row end for previous key if selected key has it
            if ((it_erase->IsRowEnd) && (it_erase != sublayout_keys.begin()))
            {
                (it_erase - 1)->IsRowEnd = true;
            }

            sublayout_keys.erase(it_erase);
        }

        m_HasChangedSelectedKey = true;
    }

    if (has_invalid_selection)
        ImGui::PopItemDisabled();

    ImGui::EndGroup();
    bottom_buttons_width = ImGui::GetItemRectSize().x;

    ImGui::End();
}

void KeyboardEditor::UpdateWindowKeyProperties()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    //This adds border size to overlap with neighboring window borders instead of making it thicker
    ImGui::SetNextWindowSize({io.DisplaySize.x / 2.0f + (style.WindowBorderSize + style.WindowBorderSize), m_TopWindowHeight});
    ImGui::SetNextWindowPos({io.DisplaySize.x / 2.0f, 0.0f}, ImGuiCond_Always, {0.5f, 0.0f});

    ImGui::Begin(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesTitle), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    auto& sublayout_keys = vr_keyboard.GetLayout(m_SelectedSublayout);
    m_SelectedKeyID = clamp(m_SelectedKeyID, -1, (int)sublayout_keys.size() - 1);

    if (m_SelectedKeyID == -1)
    {
        static ImVec2 no_selection_text_size;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x / 2.0f - (no_selection_text_size.x / 2.0f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y / 2.0f - (no_selection_text_size.y / 2.0f));

        ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesNoSelection));
        no_selection_text_size = ImGui::GetItemRectSize();

        ImGui::End();
        return;
    }

    KeyboardLayoutKey& key = sublayout_keys[m_SelectedKeyID];
    static char buffer_key_label[128]   = "";
    static char buffer_key_string[1024] = "";

    if (m_HasChangedSelectedKey)
    {
        size_t copied_length = key.Label.copy(buffer_key_label, IM_ARRAYSIZE(buffer_key_label) - 1);
        buffer_key_label[copied_length] = '\0';
        copied_length = key.KeyString.copy(buffer_key_string, IM_ARRAYSIZE(buffer_key_string) - 1);
        buffer_key_string[copied_length] = '\0';
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, m_WindowRounding);

    const float item_spacing_x = style.ItemSpacing.x;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, style.ItemSpacing.y});    //Avoid horizontal padding from the column

    ImGui::Columns(2, "ColumnKeyProps", false);
    ImGui::SetColumnWidth(0, (float)(int)(io.DisplaySize.x / 6.0f));

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesType));

    if (key.KeyType == kbdlayout_key_virtual_key_iso_enter)
    {
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::FixedHelpMarker(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesTypeVirtualKeyIsoEnterTip));
    }
    else if (key.KeyType == kbdlayout_key_string)
    {
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::FixedHelpMarker(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesTypeStringTip));
    }
    ImGui::NextColumn();

    key.KeyType = (KeyboardLayoutKeyType)clamp((int)key.KeyType, 0, (tstr_KeyboardEditorKeyPropertiesTypeAction - tstr_KeyboardEditorKeyPropertiesTypeBlank));
    int key_type_temp = key.KeyType;

    ImGui::SetNextItemWidth(-1.0f);
    if (FloatingWindow::TranslatedComboAnimated("##ComboType", key_type_temp, tstr_KeyboardEditorKeyPropertiesTypeBlank, tstr_KeyboardEditorKeyPropertiesTypeAction))
    {
        if (key_type_temp != key.KeyType)
        {
            HistoryPush();
            key.KeyType = (KeyboardLayoutKeyType)key_type_temp;
        }
    }
    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesSize));
    ImGui::NextColumn();

    const float input_width = ImGui::GetFontSize() * 5.5f;
    float input_size_w = key.Width  * 100.0f;
    float input_size_h = key.Height * 100.0f;

    ImGui::SetNextItemWidth(input_width);
    if (ImGui::InputFloat("##InputSizeW", &input_size_w, 5.0f, 10.0f, "%.0f%%"))
    {
        HistoryPush();
        m_HistoryHasPendingEdit = true;

        key.Width = std::max(0.10f, roundf(input_size_w) / 100.0f);
    }

    ImGui::SameLine(0.0f, item_spacing_x);

    ImGui::TextUnformatted("x");
    ImGui::SameLine(0.0f, item_spacing_x);

    if (key.KeyType == kbdlayout_key_virtual_key_iso_enter)
        ImGui::PushItemDisabled();

    ImGui::SetNextItemWidth(input_width);
    if (ImGui::InputFloat("##InputSizeH", &input_size_h, 5.0f, 10.0f, "%.0f%%"))
    {
        HistoryPush();
        m_HistoryHasPendingEdit = true;

        key.Height = std::max(0.10f, roundf(input_size_h) / 100.0f);
    }

    if (key.KeyType == kbdlayout_key_virtual_key_iso_enter)
        ImGui::PopItemDisabled();

    ImGui::NextColumn();

    if (key.KeyType != kbdlayout_key_blank_space)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesLabel));
        ImGui::NextColumn();

        ImVec2 multiline_input_size(-1, (ImGui::GetTextLineHeight() * 2.0f) + (style.FramePadding.y * 2.0f));
        if (ImGui::InputTextMultiline("##InputLabel", buffer_key_label, IM_ARRAYSIZE(buffer_key_label), multiline_input_size))
        {
            HistoryPush();
            m_HistoryHasPendingEdit = true;

            UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(buffer_key_label);
            key.Label = buffer_key_label;
        }
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();

        switch (key.KeyType)
        {
            case kbdlayout_key_virtual_key:
            case kbdlayout_key_virtual_key_toggle:
            case kbdlayout_key_virtual_key_iso_enter:
            {
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesKeyCode));
                ImGui::NextColumn();

                if (ImGui::Button((key.KeyCode == 0) ? TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCodeNone) : GetStringForKeyCode(key.KeyCode)))
                {
                    ImGui::OpenPopup(TranslationManager::GetString(tstr_DialogKeyCodePickerHeader));
                }
                break;
            }
            case kbdlayout_key_string:
            {
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesString));
                ImGui::NextColumn();

                ImVec2 multiline_input_size(-1, (ImGui::GetTextLineHeight() * 3.0f) + (style.FramePadding.y * 2.0f));
                if (ImGui::InputTextMultiline("##InputKeyString", buffer_key_string, IM_ARRAYSIZE(buffer_key_string), multiline_input_size))
                {
                    UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(buffer_key_string);
                    key.KeyString = buffer_key_string;
                }
                break;
            }
            case kbdlayout_key_sublayout_toggle:
            {
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesSublayout));
                ImGui::NextColumn();

                key.KeySubLayoutToggle = (KeyboardLayoutSubLayout)clamp((int)key.KeySubLayoutToggle, 0, (tstr_KeyboardEditorSublayoutAux - tstr_KeyboardEditorSublayoutBase));
                int key_sublayout_toggle_temp = key.KeySubLayoutToggle;

                ImGui::SetNextItemWidth(-1.0f);
                if (FloatingWindow::TranslatedComboAnimated("##ComboSublayout", key_sublayout_toggle_temp, tstr_KeyboardEditorSublayoutBase, tstr_KeyboardEditorSublayoutAux))
                {
                    if (key_sublayout_toggle_temp != key.KeySubLayoutToggle)
                    {
                        HistoryPush();
                        key.KeySubLayoutToggle = (KeyboardLayoutSubLayout)key_sublayout_toggle_temp;
                    }
                }
                break;
            }
            case kbdlayout_key_action:
            {
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesAction));
                ImGui::NextColumn();

                if (ImGui::Button(ConfigManager::Get().GetActionManager().GetTranslatedName(key.KeyActionUID)))
                {
                    ImGui::OpenPopup(TranslationManager::GetString(tstr_DialogActionPickerHeader));
                }
                break;
            }
            default: break;
        }

        ImGui::NextColumn();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesCluster));
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::FixedHelpMarker(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesClusterTip));

    ImGui::NextColumn();

    key.KeyCluster = (KeyboardLayoutCluster)clamp((int)key.KeyCluster, 0, (tstr_SettingsKeyboardKeyClusterExtra - tstr_SettingsKeyboardKeyClusterBase));
    int key_cluster_temp = key.KeyCluster;

    ImGui::SetNextItemWidth(-1.0f);
    if (FloatingWindow::TranslatedComboAnimated("##ComboCluster", key_cluster_temp, tstr_SettingsKeyboardKeyClusterBase, tstr_SettingsKeyboardKeyClusterExtra))
    {
        if (key_cluster_temp != key.KeyCluster)
        {
            HistoryPush();
            key.KeyCluster = (KeyboardLayoutCluster)key_cluster_temp;
        }
    }
    ImGui::NextColumn();

    ImGui::Columns(1);
    ImGui::PopStyleVar();   //ImGuiStyleVar_ItemSpacing

    if (key.KeyType == kbdlayout_key_virtual_key)
    {
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesBlockModifiers), &key.BlockModifiers))
        {
            //Switch around the values temporarily to allow the current state to be pushed just in time
            key.BlockModifiers = !key.BlockModifiers;
            HistoryPush();
            key.BlockModifiers = !key.BlockModifiers;
        }
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::FixedHelpMarker(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesBlockModifiersTip));

        ImGui::SameLine();
    }

    if (key.KeyType != kbdlayout_key_blank_space)
    {
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesNoRepeat), &key.NoRepeat))
        {
            key.NoRepeat = !key.NoRepeat;
            HistoryPush();
            key.NoRepeat = !key.NoRepeat;
        }
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::FixedHelpMarker(TranslationManager::GetString(tstr_KeyboardEditorKeyPropertiesNoRepeatTip));
    }


    //-Key Code Picker popup
    ImGui::SetNextWindowSize({io.DisplaySize.x / 3.0f, io.DisplaySize.y * 0.8f});
    ImGui::SetNextWindowPos( {io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f}, ImGuiCond_Always, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal(TranslationManager::GetString(tstr_DialogKeyCodePickerHeader), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        static ImGuiTextFilter filter;
        static int list_id = 0;
        static bool scroll_to_selection = false;

        if (ImGui::IsWindowAppearing())
        {
            scroll_to_selection = true;

            for (int i = 0; i < 256; i++)
            {
                //Not the smartest, but most straight forward way
                if (GetKeyCodeForListID(i) == key.KeyCode)
                {
                    list_id = i;

                    //Clear filter if it wouldn't show the current selection
                    if (!filter.PassFilter( (key.KeyCode == 0) ? TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCodeNone) : GetStringForKeyCode(key.KeyCode) ))
                    {
                        filter.Clear();
                    }

                    break;
                }
            }
        }

        CloseCurrentModalPopupFromInput();

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##FilterList", TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCodeHint), filter.InputBuf, IM_ARRAYSIZE(filter.InputBuf)))
        {
            UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(filter.InputBuf);

            filter.Build();
        }

        ImGui::BeginChild("KeyCodePickerList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Borders);

        unsigned char list_keycode;
        const char* list_keycode_str = nullptr;
        for (int i = 0; i < 256; i++)
        {
            list_keycode = GetKeyCodeForListID(i);
            list_keycode_str = (list_keycode == 0) ? TranslationManager::GetString(tstr_DialogKeyCodePickerKeyCodeNone) : GetStringForKeyCode(list_keycode);
            if (filter.PassFilter(list_keycode_str))
            {
                if (ImGui::Selectable(list_keycode_str, (i == list_id)))
                {
                    list_id = i;

                    if (key.KeyCode != i)
                    {
                        HistoryPush();
                        key.KeyCode = list_keycode;
                    }

                    ImGui::CloseCurrentPopup();
                    UIManager::Get()->RepeatFrame();
                }

                if ( (scroll_to_selection) && (i == list_id) )
                {
                    ImGui::SetScrollHereY();

                    if (ImGui::IsItemVisible())
                    {
                        scroll_to_selection = false;
                    }
                }
            }
        }

        ImGui::EndChild();

        static float list_buttons_width = 0.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - list_buttons_width);

        if (ImGui::Button(TranslationManager::GetString(tstr_DialogKeyCodePickerFromInput)))
        {
            ImGui::OpenPopup("PopupBindKey");
        }

        list_buttons_width = ImGui::GetItemRectSize().x;

        ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("PopupBindKey", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNavInputs))
        {
            bool close_picker = false;

            ImGui::Text(TranslationManager::GetString(tstr_DialogKeyCodePickerFromInputPopup));

            ImGuiIO& io = ImGui::GetIO();

            //We can no longer use ImGui's keyboard state to query all possible keyboard keys, so we do it manually via GetAsyncKeyState()
            //To avoid issues with keys that are already down to begin with, we store the state in the moment of the popup appearing and only act on changes to that
            static bool keyboard_state_initial[255] = {0};
            static bool wait_for_key_release = false; 

            if (ImGui::IsWindowAppearing())
            {
                for (int i = 0; i < IM_ARRAYSIZE(keyboard_state_initial); ++i)
                {
                    keyboard_state_initial[i] = (::GetAsyncKeyState(i) < 0);
                }

                wait_for_key_release = false;
            }

            for (int i = 0; i < IM_ARRAYSIZE(keyboard_state_initial); ++i)
            {
                if ((::GetAsyncKeyState(i) < 0) != keyboard_state_initial[i])
                {
                    //Key was up before, so it's a key press
                    if (!keyboard_state_initial[i])
                    {
                        if (key.KeyCode != i)
                        {
                            HistoryPush();
                            key.KeyCode = i;
                        }

                        for (int i = 0; i < 256; i++)
                        {
                            if (GetKeyCodeForListID(i) == key.KeyCode)
                            {
                                list_id = i;
                                break;
                            }
                        }

                        scroll_to_selection = true;

                        //Wait for the key to be released to avoid inputs triggering other things
                        wait_for_key_release = true;
                        keyboard_state_initial[i] = true;
                        break;
                    }
                    else   //Key was down before, so it's a key release. Update the initial state so it can be pressed again and registered as such
                    {
                        keyboard_state_initial[i] = false;

                        //Close popup here if we are waiting for this key to be released
                        if ((wait_for_key_release) && (i == key.KeyCode))
                        {
                            close_picker = true;

                            ImGui::CloseCurrentPopup();
                            io.ClearInputKeys();
                        }
                    }
                }
            }

            ImGui::EndPopup();

            if (close_picker)
            {
                ImGui::CloseCurrentPopup();
                UIManager::Get()->RepeatFrame();
            }
        }

        ImGui::EndPopup();
    }


    //-Action Picker popup
    ImGui::SetNextWindowSize({io.DisplaySize.x / 3.0f, io.DisplaySize.y * 0.8f});
    ImGui::SetNextWindowPos( {io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f}, ImGuiCond_Always, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal(TranslationManager::GetString(tstr_DialogActionPickerHeader), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        static std::vector<ActionManager::ActionNameListEntry> action_list;
        static ActionUID list_uid = k_ActionUID_Invalid;
        static bool scroll_to_selection = false;
        static ImVec2 no_actions_text_size;

        if (ImGui::IsWindowAppearing())
        {
            action_list = ConfigManager::Get().GetActionManager().GetActionNameList();

            list_uid = key.KeyActionUID;
            scroll_to_selection = true;

            //Set to invalid if selection doesn't exist
            if (!ConfigManager::Get().GetActionManager().ActionExists(list_uid))
            {
                list_uid = k_ActionUID_Invalid;
            }
        }

        CloseCurrentModalPopupFromInput();

        //No Action entry
        {
            ImGui::PushID(0);

            if (ImGui::Selectable(TranslationManager::GetString(tstr_ActionNone), (list_uid == k_ActionUID_Invalid) ))
            {
                list_uid = k_ActionUID_Invalid;

                if (key.KeyActionUID != k_ActionUID_Invalid)
                {
                    HistoryPush();
                    key.KeyActionUID = k_ActionUID_Invalid;
                }

                ImGui::CloseCurrentPopup();
            }

            if ( (scroll_to_selection) && (list_uid == k_ActionUID_Invalid) )
            {
                ImGui::SetScrollHereY();

                if (ImGui::IsItemVisible())
                {
                    scroll_to_selection = false;
                }
            }

            ImGui::PopID();
        }

        //List actions
        for (const auto& entry : action_list)
        {
            ImGui::PushID((void*)entry.UID);

            if (ImGui::Selectable(entry.Name.c_str(), (entry.UID == list_uid) ))
            {
                list_uid = entry.UID;

                if (key.KeyActionUID != entry.UID)
                {
                    HistoryPush();
                    key.KeyActionUID = entry.UID;
                }

                ImGui::CloseCurrentPopup();
            }

            if ( (scroll_to_selection) && (entry.UID == list_uid) )
            {
                ImGui::SetScrollHereY();

                if (ImGui::IsItemVisible())
                {
                    scroll_to_selection = false;
                }
            }

            ImGui::PopID();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();   //ImGuiStyleVar_WindowRounding
    ImGui::End();
}

void KeyboardEditor::UpdateWindowMetadata()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();
    const KeyboardLayoutMetadata& metadata = vr_keyboard.GetLayoutMetadata();

    ImGui::SetNextWindowSize({io.DisplaySize.x / 4.0f, m_TopWindowHeight});
    ImGui::SetNextWindowPos({io.DisplaySize.x, 0.0f}, ImGuiCond_Always, {1.0f, 0.0f});

    ImGui::Begin(TranslationManager::GetString(tstr_KeyboardEditorMetadataTitle), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    static char buffer_name[128]   = "";
    static char buffer_author[256] = "";

    if (m_RefreshLayout)
    {
        size_t copied_length = vr_keyboard.GetLayoutMetadata().Name.copy(buffer_name, IM_ARRAYSIZE(buffer_name) - 1);
        buffer_name[copied_length] = '\0';
        copied_length = vr_keyboard.GetLayoutMetadata().Author.copy(buffer_author, IM_ARRAYSIZE(buffer_author) - 1);
        buffer_author[copied_length] = '\0';
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, m_WindowRounding);

    //We don't need scrolling yet but the child window with border offers consistency with the key list window for now
    //Make the child expand past window padding
    ImGui::SetCursorPos({ImGui::GetCursorPosX() - style.WindowPadding.x, ImGui::GetCursorPosY() - style.WindowPadding.y - style.TabBarBorderSize});

    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::BeginChild("MetadataContents", {ImGui::GetContentRegionAvail().x + style.WindowPadding.x, -ImGui::GetFrameHeightWithSpacing()}, ImGuiChildFlags_Borders);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, style.ItemSpacing.y});    //Avoid horizontal padding from the column

    ImGui::Columns(2, "ColumnMetadata", false);
    ImGui::SetColumnWidth(0, io.DisplaySize.x / 12.0f);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorMetadataName));
    ImGui::NextColumn();

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##InputName", buffer_name, IM_ARRAYSIZE(buffer_name)))
    {
        HistoryPush();
        m_HistoryHasPendingEdit = true;

        UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(buffer_name);

        KeyboardLayoutMetadata metadata_new = metadata;
        metadata_new.Name = buffer_name;
        vr_keyboard.SetLayoutMetadata(metadata_new);
    }
    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorMetadataAuthor));
    ImGui::NextColumn();

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##InputAuthor", buffer_author, IM_ARRAYSIZE(buffer_author)))
    {
        HistoryPush();
        m_HistoryHasPendingEdit = true;

        UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(buffer_author);

        KeyboardLayoutMetadata metadata_new = metadata;
        metadata_new.Author = buffer_author;
        vr_keyboard.SetLayoutMetadata(metadata_new);
    }

    ImGui::Columns(1);
    ImGui::PopStyleVar();   //ImGuiStyleVar_ItemSpacing

    ImGui::Spacing();

    bool has_altgr_temp = metadata.HasAltGr;
    if (ImGui::Checkbox(TranslationManager::GetString(tstr_KeyboardEditorMetadataHasAltGr), &has_altgr_temp))
    {
        HistoryPush();

        KeyboardLayoutMetadata metadata_new = metadata;
        metadata_new.HasAltGr = has_altgr_temp;
        vr_keyboard.SetLayoutMetadata(metadata_new);

        UIManager::Get()->RepeatFrame();
    }
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::FixedHelpMarker(TranslationManager::GetString(tstr_KeyboardEditorMetadataHasAltGrTip));

    ImGui::Spacing();

    const float column_start_x = (float)(int)((ImGui::GetContentRegionAvail().x / 2.0f) + style.IndentSpacing);
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorMetadataClusterPreview));
    ImGui::Indent();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyClusterFunction), &m_PreviewCluster[kbdlayout_cluster_function]))
    {
        UIManager::Get()->RepeatFrame();
    }
    ImGui::SameLine(column_start_x);

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyClusterNavigation), &m_PreviewCluster[kbdlayout_cluster_navigation]))
    {
        UIManager::Get()->RepeatFrame();
    }

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyClusterNumpad), &m_PreviewCluster[kbdlayout_cluster_numpad]))
    {
        UIManager::Get()->RepeatFrame();
    }
    ImGui::SameLine(column_start_x);

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_SettingsKeyboardKeyClusterExtra), &m_PreviewCluster[kbdlayout_cluster_extra]))
    {
        UIManager::Get()->RepeatFrame();
    }

    ImGui::EndChild();

    static float bottom_buttons_width = 0.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - bottom_buttons_width);
    ImGui::BeginGroup();

    if (ImGui::Button(TranslationManager::GetString(tstr_KeyboardEditorMetadataSave))) 
    {
        ImGui::OpenPopup(TranslationManager::GetString(tstr_KeyboardEditorMetadataSavePopupTitle));
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    if (ImGui::Button(TranslationManager::GetString(tstr_KeyboardEditorMetadataLoad))) 
    {
        ImGui::OpenPopup(TranslationManager::GetString(tstr_KeyboardEditorMetadataLoadPopupTitle));
    }

    ImGui::EndGroup();
    bottom_buttons_width = ImGui::GetItemRectSize().x;


    //-Save Layout popup
    ImGui::SetNextWindowSize({io.DisplaySize.x / 3.0f, io.DisplaySize.y * 0.8f});
    ImGui::SetNextWindowPos( {io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f}, ImGuiCond_Always, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal(TranslationManager::GetString(tstr_KeyboardEditorMetadataSavePopupTitle), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        static std::vector<KeyboardLayoutMetadata> list_layouts;
        static int list_id = 0;
        static char buffer_filename[256] = "";
        static bool is_name_blank = false;
        static bool is_name_taken = false;
        static bool has_saving_failed = false;

        bool update_entry_match = false;
        bool is_entry_double_clicked = false;

        if (ImGui::IsWindowAppearing())
        {
            list_id = -1;
            list_layouts = VRKeyboard::GetKeyboardLayoutList();
            has_saving_failed = false;

            size_t copied_length = vr_keyboard.GetLayoutMetadata().FileName.copy(buffer_filename, IM_ARRAYSIZE(buffer_filename) - 1);
            buffer_filename[copied_length] = '\0';

            update_entry_match = true;
        }

        CloseCurrentModalPopupFromInput();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_KeyboardEditorMetadataSavePopupFilename));

        const float input_spacing_offset =  (float)int(ImGui::GetContentRegionAvail().x * 0.10f);
        const float input_spacing = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;

        if (is_name_blank)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            ImGui::FixedHelpMarker(TranslationManager::GetString(tstr_KeyboardEditorMetadataSavePopupFilenameBlankTip), "(!)");
        }

        ImGui::SameLine(input_spacing_offset, input_spacing);

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##InputFilename", buffer_filename, IM_ARRAYSIZE(buffer_filename), ImGuiInputTextFlags_CallbackCharFilter,
                                                                                                [](ImGuiInputTextCallbackData* data)
                                                                                                {
                                                                                                    return (int)IsWCharInvalidForFileName(data->EventChar);
                                                                                                }
           ))
        {
            UIManager::Get()->AddFontBuilderStringIfAnyUnmappedCharacters(buffer_filename);
            update_entry_match = true;
        }

        //Select matching entry
        if (update_entry_match)
        {
            const std::string& current_filename = buffer_filename;
            auto it = std::find_if(list_layouts.begin(), list_layouts.end(), [&current_filename](const auto& list_entry){ return (current_filename == list_entry.FileName); });

            if (it != list_layouts.end())
            {
                list_id = (int)std::distance(list_layouts.begin(), it);
            }
            else
            {
                list_id = -1;
            }

            is_name_blank = current_filename.empty();
        }

        ImGui::BeginChild("LayoutList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Borders);

        int index = 0;
        for (const auto& metadata : list_layouts)
        {
            ImGui::PushID(index);

            if (ImGui::Selectable(metadata.Name.c_str(), (index == list_id)))
            {
                list_id = index;

                size_t copied_length = metadata.FileName.copy(buffer_filename, IM_ARRAYSIZE(buffer_filename) - 1);
                buffer_filename[copied_length] = '\0';

                is_name_blank = false;
            }

            if ((ImGui::IsItemActive()) && (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)))
            {
                is_entry_double_clicked = true;
            }

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
            ImGui::TextRightUnformatted(0.0f, metadata.FileName.c_str());
            ImGui::PopStyleColor();

            ImGui::PopID();

            index++;
        }

        ImGui::EndChild();

        if (is_name_blank)
            ImGui::PushItemDisabled();

        if ((ImGui::Button(TranslationManager::GetString(tstr_KeyboardEditorMetadataSavePopupConfirm))) || (is_entry_double_clicked))
        {
            //Append extension if needed
            std::string filename(buffer_filename);
            if (filename.rfind(".ini") != filename.length() - 4)
            {
                filename += ".ini";
            }

            has_saving_failed = !vr_keyboard.SaveCurrentLayoutToFile(filename);

            if (!has_saving_failed)
            {
                ImGui::CloseCurrentPopup();
            }
        }

        if (is_name_blank)
            ImGui::PopItemDisabled();

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel)))
        {
            ImGui::CloseCurrentPopup();
        }

        if (has_saving_failed)
        {
            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextError);
            ImGui::TextRightUnformatted(style.ItemInnerSpacing.x, TranslationManager::GetString(tstr_KeyboardEditorMetadataSavePopupConfirmError));
            ImGui::PopStyleColor();
        }

        ImGui::EndPopup();
    }


    //-Load Layout popup
    ImGui::SetNextWindowSize({io.DisplaySize.x / 3.0f, io.DisplaySize.y * 0.8f});
    ImGui::SetNextWindowPos( {io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f}, ImGuiCond_Always, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal(TranslationManager::GetString(tstr_KeyboardEditorMetadataLoadPopupTitle), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        static std::vector<KeyboardLayoutMetadata> list_layouts;
        static int list_id = 0;

        bool is_entry_double_clicked = false;

        if (ImGui::IsWindowAppearing())
        {
            list_id = -1;
            list_layouts = VRKeyboard::GetKeyboardLayoutList();

            //Select matching entry
            const std::string& current_filename = vr_keyboard.GetLayoutMetadata().FileName;
            auto it = std::find_if(list_layouts.begin(), list_layouts.end(), [&current_filename](const auto& list_entry){ return (current_filename == list_entry.FileName); });

            if (it != list_layouts.end())
            {
                list_id = (int)std::distance(list_layouts.begin(), it);
            }
        }

        CloseCurrentModalPopupFromInput();

        ImGui::BeginChild("LayoutList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Borders);

        int index = 0;
        for (const auto& metadata : list_layouts)
        {
            ImGui::PushID(index);

            if (ImGui::Selectable(metadata.Name.c_str(), (index == list_id)))
            {
                list_id = index;
            }

            if ((ImGui::IsItemActive()) && (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)))
            {
                is_entry_double_clicked = true;
            }

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
            ImGui::TextRightUnformatted(0.0f, metadata.FileName.c_str());
            ImGui::PopStyleColor();

            ImGui::PopID();

            index++;
        }

        ImGui::EndChild();

        if ((ImGui::Button(TranslationManager::GetString(tstr_KeyboardEditorMetadataLoadPopupConfirm))) || (is_entry_double_clicked))
        {
            vr_keyboard.LoadLayoutFromFile(list_layouts[list_id].FileName);
            ImGui::CloseCurrentPopup();

            m_RefreshLayout = true;
            m_HasChangedSelectedKey = true;
            HistoryClear();
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();   //ImGuiStyleVar_WindowRounding
    ImGui::End();
}

void KeyboardEditor::UpdateWindowPreview()
{
    //This needs to be kept in sync with WindowKeyboard::WindowUpdate(), but is also sufficiently different to not try to make a mess in that function from trying to shove it there
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    //We only scale style sizes up in desktop mode (using DPI instead of UI scale value), but for accurate keyboard preview we need spacing to scale with UI scale
    const float ui_scale = UIManager::Get()->GetUIScale();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      {8.0f * ui_scale, 4.0f * ui_scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {4.0f * ui_scale, 4.0f * ui_scale});

    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_keyboard);
    ImGui::SetNextWindowSizeConstraints({ImGui::GetFrameHeight() * 6.0f, ImGui::GetFrameHeight() * 2.0f}, {(rect.GetWidth() - 4) * ui_scale, (rect.GetHeight() - 4) * ui_scale});

    ImGui::SetNextWindowPos({io.DisplaySize.x / 2.0f, io.DisplaySize.y}, ImGuiCond_Always, {0.5f, 1.0f});

    ImGui::Begin(TranslationManager::GetString(tstr_KeyboardEditorPreviewTitle), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    const float base_width = (float)int(ImGui::GetTextLineHeightWithSpacing() * 2.225f);

    //Used for kbdlayout_key_virtual_key_iso_enter key
    ImVec2 iso_enter_top_pos(-1.0f, -1.0f);
    float  iso_enter_top_width    = -1.0f;
    int    iso_enter_top_index    = -1;

    int row_index = 0;
    int key_index = 0;
    ImVec2 cursor_pos = ImGui::GetCursorPos();
    ImVec2 cursor_pos_line_start = cursor_pos;
    for (const auto& key : vr_keyboard.GetLayout(m_SelectedSublayout))
    {
        //Skip key as if it was never loaded if preview is disabled for it
        if (!m_PreviewCluster[key.KeyCluster])
        {
            key_index++;

            if (key.IsRowEnd)
            {
                ++row_index;

                //Only force a new line if the cursor moved (meaning we didn't skip all keys in this row)
                if ((cursor_pos_line_start.x != ImGui::GetCursorPos().x) || (cursor_pos_line_start.x != ImGui::GetCursorPos().x))
                {
                    ImGui::NewLine();
                    cursor_pos_line_start = ImGui::GetCursorPos();
                }
            }
            continue;
        }
        else if ((cursor_pos_line_start.x != ImGui::GetCursorPos().x) || (cursor_pos_line_start.x != ImGui::GetCursorPos().x))
        {
            //To make above work correctly, spacing is applied here, with cursor_pos still holding values from the previous key
            cursor_pos.x += style.ItemInnerSpacing.x;
            ImGui::SetCursorPos(cursor_pos);
        }

        ImGui::PushID(key_index);

        //Keep cursor pos on integer values
        cursor_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos({ceilf(cursor_pos.x), ceilf(cursor_pos.y)});

        //This accounts for the spacing that is missing with wider keys so rows still line up and also forces integer values
        const float key_width_f = base_width * key.Width  + (style.ItemInnerSpacing.x * (key.Width  - 1.0f));
        const float key_height  = (float)(int)( base_width * key.Height + (style.ItemInnerSpacing.y * (key.Height - 1.0f)) );
        float key_width         = (float)(int)( key_width_f );

        //Add an extra pixel of width if the untruncated values would push it further
        //There might be a smarter way to do this (simple rounding doesn't seem to be it), but this helps the keys align while rendering on full pixels only
        if (cursor_pos.x + key_width_f > ImGui::GetCursorPosX() + key_width)
        {
            key_width += 1.0f;
        }

        const bool highlight_key = ((key_index == m_SelectedKeyID) || (m_SelectedRowID == row_index));

        if (highlight_key)
            ImGui::PushStyleColor(ImGuiCol_Button, Style_ImGuiCol_ButtonPassiveToggled);

        switch (key.KeyType)
        {
            case kbdlayout_key_blank_space:
            {
                ImGui::PushStyleColor(ImGuiCol_Button, 0);
                if (ImGui::Button("", {key_width, key_height}))
                {
                    m_SelectedRowID = -1;
                    m_SelectedKeyID = key_index;
                    m_HasChangedSelectedKey = true;
                }
                ImGui::PopStyleColor();

                if (highlight_key)
                {
                    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_Button), style.FrameRounding, 0, style.WindowBorderSize * 2.0f);
                }
                break;
            }
            case kbdlayout_key_virtual_key:
            case kbdlayout_key_virtual_key_toggle:
            case kbdlayout_key_string:
            case kbdlayout_key_sublayout_toggle:
            case kbdlayout_key_action:
            {
                if (ImGui::Button(key.Label.c_str(), {key_width, key_height}))
                {
                    m_SelectedRowID = -1;
                    m_SelectedKeyID = key_index;
                    m_HasChangedSelectedKey = true;
                }

                break;
            }
            case kbdlayout_key_virtual_key_iso_enter:
            {
                //This one's a bit of a mess, but builds an ISO-Enter shaped button out of two key entries
                //First step is the top "key". Its label is unused, but the width is.
                //Second step is the bottom "key". It stretches itself over the row above it and hosts the label.
                //First and second step used invisible buttons first to check item state and have it synced up,
                //then in the second step there are two visual-only buttons used with style color adjusted to the state of the invisible buttons
                //
                //...now that a custom button implementation is used anyways this could be solved more cleanly... but it still works, so eh.

                const bool is_bottom_key = (iso_enter_top_index != -1);
                const ImVec2 cursor_pos = ImGui::GetCursorPos();
                float offset_y = 0.0f;

                //If second ISO-enter key, offset cursor to the previous row and stretch the button down to the end of the current row
                if (is_bottom_key)
                {
                    offset_y = style.ItemSpacing.y + base_width;
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - offset_y);
                }
                else //else remember the width of the top part for later
                {
                    iso_enter_top_pos   = cursor_pos;
                    iso_enter_top_width = key_width;
                    iso_enter_top_index = key_index;
                }

                if (ImGui::Button((is_bottom_key) ? key.Label.c_str() : "##IsoEnterDummy", {key_width, base_width + offset_y}))
                {
                    m_SelectedRowID = -1;
                    m_SelectedKeyID = key_index;
                    m_HasChangedSelectedKey = true;
                }

                break;
            }
            default: break;
        }

        if (highlight_key)
            ImGui::PopStyleColor();

        //Select row by right clicking
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            m_SelectedRowID = row_index;
            m_SelectedKeyID = -1;
            m_HasChangedSelectedKey = true;
        }

        //Return to normal row position if the key was taller than 100%
        if (key.Height > 1.0f)
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - key_height + base_width);
            ImGui::SetPreviousLineHeight(ImGui::GetPreviousLineHeight() - key_height + base_width);
        }

        if (!key.IsRowEnd)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            cursor_pos.x += key_width_f;
        }
        else
        {
            ++row_index;
            cursor_pos_line_start = ImGui::GetCursorPos();
        }

        ImGui::PopID();

        ++key_index;
    }

    ImGui::End();

    ImGui::PopStyleVar(2);  //ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_ItemInnerSpacing
}

std::pair<int, int> KeyboardEditor::GetKeyRowRange(KeyboardLayoutSubLayout sublayout, int row_id)
{
    auto& sublayout_keys = UIManager::Get()->GetVRKeyboard().GetLayout(sublayout);

    int key_id_begin = 0;
    int key_id_end   = (int)sublayout_keys.size(); //Set to size() so begin() + key_id_end results in end() if nothing is found

    for (int i = 0, loop_row_id = 0; i < sublayout_keys.size(); ++i)
    {
        if (sublayout_keys[i].IsRowEnd)
        {
            ++loop_row_id;

            if (loop_row_id == row_id)
            {
                key_id_begin = i + 1;
            }
            else if (loop_row_id == row_id + 1)
            {
                key_id_end = i + 1;
                break;
            }
        }
    }

    return std::make_pair(key_id_begin, key_id_end);
}

int KeyboardEditor::FindKeyWithClosestPosInNewSubLayout(int key_index, KeyboardLayoutSubLayout sublayout_id_current, KeyboardLayoutSubLayout sublayout_id_new)
{
    //Not to be confused with WindowKeyboard::FindSameKeyInNewSubLayout() which only finds exact matches with same function at same pos instead
    if (key_index < 0)
        return -1;

    //Return index of key with position closest to sublayout_current's key_index key position
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();
    const auto& sublayout_current = vr_keyboard.GetLayout(sublayout_id_current);
    const auto& sublayout_new     = vr_keyboard.GetLayout(sublayout_id_new);

    //Skip if index doesn't exist in layout for some reason
    if (sublayout_current.size() <= key_index)
        return -1;

    //Get key position in current layout
    int i = 0;
    Vector2 key_current_pos;

    for (const auto& key : sublayout_current)
    {
        if (i == key_index)
        {
            break;
        }

        key_current_pos.x += key.Width;

        if (key.IsRowEnd)
        {
            key_current_pos.x  = 0.0f;
            key_current_pos.y += 1.0f; //Key height doesn't matter, advance a row
        }

        i++;
    }

    //Find key closest to the old position in new layout
    i = 0;
    Vector2 key_new_pos;
    int key_lowest_dist_id = -1;
    float key_lowest_dist = FLT_MAX;

    for (const auto& key : sublayout_new)
    {
        if ( (key_new_pos.y == key_current_pos.y) && (key_new_pos.x == key_current_pos.x) )
        {
            //Key exists at the same position return that
            return i;
        }
        else
        {
            //Alternatively get the closest key and return that at the end if we don't find anything better
            float distance = key_new_pos.distance(key_current_pos);

            if (distance < key_lowest_dist)
            {
                key_lowest_dist_id = i;
                key_lowest_dist = distance;
            }
        }

        key_new_pos.x += key.Width;

        if (key.IsRowEnd)
        {
            key_new_pos.x  = 0.0f;
            key_new_pos.y += 1.0f;
        }

        i++;
    }

    return key_lowest_dist_id;
}

void KeyboardEditor::HistoryPush()
{
    if (!m_HistoryHasPendingEdit)
    {
        HistoryPushInternal(m_HistoryUndo);
        m_HistoryRedo.clear();
    }
}

void KeyboardEditor::HistoryPushInternal(std::vector<HistoryItem>& target_history)
{
    //This copies the entire keyboard data for every history item
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    HistoryItem item;
    item.Metadata = vr_keyboard.GetLayoutMetadata();

    for (int i_sublayout = kbdlayout_sub_base; i_sublayout < kbdlayout_sub_MAX; ++i_sublayout)
    {
        KeyboardLayoutSubLayout sublayout = (KeyboardLayoutSubLayout)i_sublayout;
        item.KeyboardKeys[sublayout] = vr_keyboard.GetLayout(sublayout);
    }

    target_history.push_back(item);
}

void KeyboardEditor::HistoryUndo()
{
    if (m_HistoryUndo.empty())
        return;

    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    HistoryItem item = m_HistoryUndo.back();
    m_HistoryUndo.pop_back();
    HistoryPushInternal(m_HistoryRedo);

    vr_keyboard.SetLayoutMetadata(item.Metadata);

    for (int i_sublayout = kbdlayout_sub_base; i_sublayout < kbdlayout_sub_MAX; ++i_sublayout)
    {
        KeyboardLayoutSubLayout sublayout = (KeyboardLayoutSubLayout)i_sublayout;
        vr_keyboard.SetLayout(sublayout, item.KeyboardKeys[sublayout]);
    }

    m_HasChangedSelectedKey = true;
    m_RefreshLayout = true;
}

void KeyboardEditor::HistoryRedo()
{
    if (m_HistoryRedo.empty())
        return;

    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    HistoryItem item = m_HistoryRedo.back();
    m_HistoryRedo.pop_back();
    HistoryPushInternal(m_HistoryUndo);

    vr_keyboard.SetLayoutMetadata(item.Metadata);

    for (int i_sublayout = kbdlayout_sub_base; i_sublayout < kbdlayout_sub_MAX; ++i_sublayout)
    {
        KeyboardLayoutSubLayout sublayout = (KeyboardLayoutSubLayout)i_sublayout;
        vr_keyboard.SetLayout(sublayout, item.KeyboardKeys[sublayout]);
    }

    m_HasChangedSelectedKey = true;
    m_RefreshLayout = true;
}

void KeyboardEditor::HistoryClear()
{
    m_HistoryUndo.clear();
    m_HistoryRedo.clear();
}

void KeyboardEditor::CloseCurrentModalPopupFromInput()
{
    //Imitate non-modal non-window click closing behavior (we go modal to get the title bar)
    if ( ( (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) && ( (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) || (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) )) || 
           (ImGui::IsKeyPressed(ImGuiKey_Escape)) )
    {
        ImGui::CloseCurrentPopup();
    }
}

void KeyboardEditor::Update()
{
    const bool has_changed_key_prev = m_HasChangedSelectedKey;
    const bool refresh_layout_prev  = m_RefreshLayout;

    m_TopWindowHeight = ImGui::GetIO().DisplaySize.y - (UITextureSpaces::Get().GetRect(ui_texspace_keyboard).GetHeight() * UIManager::Get()->GetUIScale());
    m_WindowRounding = ImGui::GetStyle().WindowRounding;    //Store window rounding before pushing it to 0 so we have the scaled value for popups later

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    UpdateWindowKeyList();
    UpdateWindowKeyProperties();
    UpdateWindowMetadata();

    ImGui::PopStyleVar();

    UpdateWindowPreview();


    if (ImGui::IsAnyItemDeactivated())
    {
        m_HistoryHasPendingEdit = false;
    }

    if (has_changed_key_prev == m_HasChangedSelectedKey)
    {
        m_HasChangedSelectedKey = false;
    }
    else
    {
        UIManager::Get()->RepeatFrame();
    }

    if (refresh_layout_prev == m_RefreshLayout)
    {
        m_RefreshLayout = false;
    }
    else
    {
        UIManager::Get()->RepeatFrame();
    }
}
