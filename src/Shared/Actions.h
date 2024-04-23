#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#ifdef DPLUS_UI
    #include "imgui.h"
    #include "TranslationManager.h"
#endif

struct ActionCommand
{
    enum CommandType
    {
        command_none,
        command_key,
        command_mouse_pos,
        command_string,
        command_launch_app,
        command_show_keyboard,
        command_crop_active_window,
        command_show_overlay,
        command_switch_task,
        command_load_overlay_profile,
        command_unknown,                                //Set when loading unrecognized command
        command_MAX
    };

    enum CommandToggleArg : unsigned int
    {
        command_arg_toggle,
        command_arg_always_show,
        command_arg_always_hide,
    };

    static const char* s_CommandTypeNames[command_MAX]; //Type names used when loading/saving config

    CommandType Type = command_none;

    unsigned int UIntID  = 0;
    unsigned int UIntArg = 0;
    std::string StrMain;
    std::string StrArg;

    std::string Serialize() const;              //Serializes into binary data stored as string (contains NUL bytes), not suitable for storage
    void Deserialize(const std::string& str);   //Deserializes from strings created by above function
};

typedef uint64_t ActionUID;
static const ActionUID k_ActionUID_Invalid = 0;
typedef std::vector<unsigned int> OverlayIDList;

struct Action
{
    ActionUID UID = 0;
    std::string Name;
    std::string Label;
    std::vector<ActionCommand> Commands;
    bool TargetUseTags = false;
    std::string TargetTags;
    std::string IconFilename;

    #ifdef DPLUS_UI
        int IconImGuiRectID           = -1; //-1 when no icon loaded (ID on ImGui end is not valid after building the font)
        ImVec2 IconAtlasSize;
        ImVec4 IconAtlasUV;
        TRMGRStrID NameTranslationID  = tstr_NONE;
        TRMGRStrID LabelTranslationID = tstr_NONE;
    #endif

    std::string Serialize() const;              //Serializes into binary data stored as string (contains NUL bytes), not suitable for storage
    void Deserialize(const std::string& str);   //Deserializes from strings created by above function
};

class ActionManager
{
    public:
        typedef std::vector<ActionUID> ActionList;

        struct ActionNameListEntry
        {
            ActionUID UID;
            std::string Name;
        };

    private:
        std::unordered_map<ActionUID, Action> m_Actions;
        Action m_NullAction;

        #ifdef DPLUS_UI
            ActionList m_ActionOrderUI;
            ActionList m_ActionOrderBarDefault;
            ActionList m_ActionOrderOverlayBar;
        #endif

        #ifndef DPLUS_UI
            void DoKeyCommand(               const ActionCommand& command, OverlayIDList& overlay_targets, bool down) const;
            void DoMousePosCommand(          const ActionCommand& command, OverlayIDList& overlay_targets)            const;
            void DoStringCommand(            const ActionCommand& command, OverlayIDList& overlay_targets)            const;
            void DoLaunchAppCommand(         const ActionCommand& command, OverlayIDList& overlay_targets)            const;
            void DoShowKeyboardCommand(      const ActionCommand& command, OverlayIDList& overlay_targets)            const;
            void DoCropActiveWindowCommand(  const ActionCommand& command, OverlayIDList& overlay_targets)            const;
            void DoShowOverlayCommand(       const ActionCommand& command, OverlayIDList& overlay_targets, bool undo) const;
            void DoSwitchTaskCommand(        const ActionCommand& command, OverlayIDList& overlay_targets)            const;
            void DoLoadOverlayProfileCommand(const ActionCommand& command, OverlayIDList& overlay_targets)            const;
        #endif

        #ifdef DPLUS_UI
            void UpdateActionOrderListUI();
            void ValidateActionOrderList(ActionList& ui_order) const;
        #endif

    public:
        ActionManager();

        bool LoadActionsFromFile(const char* filename = nullptr);
        void SaveActionsToFile();
        void RestoreActionsFromDefault();

        const Action& GetAction(ActionUID action_uid) const;
        bool ActionExists(ActionUID action_uid) const;
        void StoreAction(const Action& action);
        void RemoveAction(ActionUID action_uid);

        //Start/StopAction forward to the dashboard app if called from UI app, but other functions like Store/RemoveAction do *not* and have to be synced manually
        void StartAction(ActionUID action_uid, unsigned int overlay_source_id = UINT_MAX) const;
        void StopAction( ActionUID action_uid, unsigned int overlay_source_id = UINT_MAX) const;     //Releases keys pressed down in StartAction()
        void DoAction(   ActionUID action_uid, unsigned int overlay_source_id = UINT_MAX) const;     //Just calls Start() and Stop() together

        uint64_t GenerateUID() const;

        static std::string ActionOrderListToString(const ActionList& action_order);
        static ActionList ActionOrderListFromString(const std::string& str);

        #ifdef DPLUS_UI
            ActionUID DuplicateAction(const Action& action);        //Returns UID of new action
            void ClearIconData();                                   //Resets icon-related values of all actions, used when reloading textures

            const ActionList& GetActionOrderListUI() const;
            void SetActionOrderListUI(const ActionList& ui_order);
            ActionList&       GetActionOrderListBarDefault();
            const ActionList& GetActionOrderListBarDefault() const;
            void              SetActionOrderListBarDefault(const ActionList& ui_order);
            ActionList&       GetActionOrderListOverlayBar();
            const ActionList& GetActionOrderListOverlayBar() const;
            void              SetActionOrderListOverlayBar(const ActionList& ui_order);

            const char* GetTranslatedName(ActionUID action_uid) const;
            const char* GetTranslatedLabel(ActionUID action_uid) const;
            std::vector<ActionNameListEntry> GetActionNameList();
            static std::vector<std::string> GetIconFileList();
            static TRMGRStrID GetTranslationIDForName(const std::string& str);
            static std::string GetCommandDescription(const ActionCommand& command, float max_width = -1.0f);
        #endif
};
