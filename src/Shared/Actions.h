#pragma once

#define NOMINMAX
#include <windows.h>

#include <string>
#include <vector>

#ifdef DPLUS_UI
    #include "imgui.h" //Don't include ImGui stuff for dashboard application
#endif

enum ActionID: int
{
    action_none,
    action_show_keyboard,
    action_crop_active_window_toggle,
    action_toggle_overlay_enabled_group_1,
    action_toggle_overlay_enabled_group_2,
    action_toggle_overlay_enabled_group_3,
    action_built_in_MAX,
    //Leaving some room here, as we don't want to mess with user order later
    action_custom = 1000          //+ Custom Action ID
};

enum CustomActionFunctionID
{
    caction_press_keys,
    caction_type_string,
    caction_launch_application,
    caction_toggle_overlay_enabled_state
};

struct ActionMainBarOrderData
{
    ActionID action_id = action_none;
    bool visible = true;
};

struct CustomAction
{
    std::string Name;
    CustomActionFunctionID FunctionType = caction_press_keys;
    unsigned char KeyCodes[3] = { 0 };
    std::string StrMain;     //Type String / Executable Path
    std::string StrArg;
    int IntID = 0;           //Overlay ID / Key Toggle bool

    #ifdef DPLUS_UI
        std::string IconFilename;
        int IconImGuiRectID = -1; //-1 when no icon loaded (ID on ImGui end is not valid after building the font)
        ImVec2 IconAtlasSize = {0.0f, 0.0f};
        ImVec4 IconAtlasUV   = {0.0f, 0.0f, 0.0f, 0.0f};
    #endif

    void ApplyIntFromConfig();
    void ApplyStringFromConfig();
    void SendUpdateToDashboardApp(int id, HWND window_handle) const;
};

class ActionManager
{
    private:
        std::vector<CustomAction> m_CustomActions;
        std::vector<ActionMainBarOrderData> m_ActionMainBarOrder;

    public:
        static ActionManager& Get();

        std::vector<CustomAction>& GetCustomActions();
        std::vector<ActionMainBarOrderData>& GetActionMainBarOrder();

        bool IsActionIDValid(ActionID action_id) const;
        const char* GetActionName(ActionID action_id) const;
        const char* GetActionButtonLabel(ActionID action_id) const;
        void EraseCustomAction(int custom_action_id);

        static CustomActionFunctionID ParseCustomActionFunctionString(const std::string& str);
        static const char* CustomActionFunctionToString(CustomActionFunctionID function_id);
};
