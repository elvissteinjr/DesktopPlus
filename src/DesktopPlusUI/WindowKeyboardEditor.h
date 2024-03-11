#pragma once

#include "VRKeyboardCommon.h"
#include <utility>

class KeyboardEditor
{
    //Not exactly space efficient, but simple and works
    struct HistoryItem
    {
        KeyboardLayoutMetadata Metadata;
        std::vector<KeyboardLayoutKey> KeyboardKeys[kbdlayout_sub_MAX];
    };

    private:
        float m_TopWindowHeight = 0.0f;
        float m_WindowRounding  = 0.0f;

        KeyboardLayoutSubLayout m_SelectedSublayout = kbdlayout_sub_base;
        int m_SelectedRowID = -1;
        int m_SelectedKeyID = -1;
        bool m_HasChangedSelectedKey = false;
        bool m_RefreshLayout = true;

        std::vector<HistoryItem> m_HistoryUndo;
        std::vector<HistoryItem> m_HistoryRedo;
        bool m_HistoryHasPendingEdit = false;   //Set by widgets that make continuous changes but should only push history once until deactivated

        bool m_PreviewCluster[kbdlayout_cluster_MAX] = {true, true, true, true, true};

        void UpdateWindowKeyList();
        void UpdateWindowKeyProperties();
        void UpdateWindowMetadata();
        void UpdateWindowPreview();

        std::pair<int, int> GetKeyRowRange(KeyboardLayoutSubLayout sublayout, int row_id);  //Returns beginning & end key IDs
        int FindKeyWithClosestPosInNewSubLayout(int key_index, KeyboardLayoutSubLayout sublayout_id_current, KeyboardLayoutSubLayout sublayout_id_new);

        void HistoryPush();
        void HistoryPushInternal(std::vector<HistoryItem>& target_history);
        void HistoryUndo();
        void HistoryRedo();
        void HistoryClear();

        void CloseCurrentModalPopupFromInput();                                             //Imitates non-modal popup closing behavior

    public:
        void Update();
};
