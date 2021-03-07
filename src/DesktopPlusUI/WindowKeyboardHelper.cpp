#include "WindowKeyboardHelper.h"

#include "UIManager.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"

WindowKeyboardHelper::WindowKeyboardHelper()
{
    //Nothing... turns out this class is just a bunch of functions for now
}

void WindowKeyboardHelper::Update()
{
    if (!IsVisible())
        return;

    ImGuiIO& io = ImGui::GetIO();

    //Style to blend in with SteamVR keyboard (this will probably be wrong once the keyboard finally updated to something else)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(2.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(2.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      ImVec4(0.098f, 0.157f, 0.239f, 0.000f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.631f, 0.635f, 0.639f, 1.000f));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.137f, 0.153f, 0.176f, 1.000f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.216f, 0.231f, 0.255f, 1.000f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.091f, 0.102f, 0.118f, 1.000f));

    ImGui::SetNextWindowSize(ImVec2(TEXSPACE_TOTAL_WIDTH * TEXSPACE_KEYBOARD_HELPER_SCALE, 0.0f));
    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - TEXSPACE_KEYBOARD_HELPER_HEIGHT), 0, ImVec2(0.0f, 0.0f));  //Center window at bottom of the overlay
    ImGui::Begin("WindowKeyboardHelper", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

    static float button_right_width = 0.0f;
    static float button_f12_width = 0.0f;

    //Toggle button state, synced with actual keyboard state, but through the dashboard app in case an eleveated application has focus (where it would just all be unpressed)
    unsigned int modifiers = (unsigned int)ConfigManager::Get().GetConfigInt(configid_int_state_keyboard_modifiers);
    bool is_ctrl_down  = (modifiers & MOD_CONTROL);
    bool is_alt_down   = (modifiers & MOD_ALT);
    bool is_shift_down = (modifiers & MOD_SHIFT);
    bool is_win_down   = (modifiers & MOD_WIN);

    LPARAM message_key = 0;

    //Ctrl
    if (is_ctrl_down)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    if (ImGui::Button("Ctrl"))
        message_key = VK_CONTROL;

    ImGui::SameLine();

    if (is_ctrl_down)
        ImGui::PopStyleColor();

    //Alt
    if (is_alt_down)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    if (ImGui::Button("Alt"))
        message_key = VK_MENU;

    ImGui::SameLine();

    if (is_alt_down)
        ImGui::PopStyleColor();

    //Shift
    if (is_shift_down)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    if (ImGui::Button("Shift"))
        message_key = VK_SHIFT;

    ImGui::SameLine();

    if (is_shift_down)
        ImGui::PopStyleColor();

    //Win
    if (is_win_down)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    if (ImGui::Button("Win"))
        message_key = VK_LWIN;

    ImGui::SameLine();

    if (is_win_down)
        ImGui::PopStyleColor();

    //Function keys (F1 - F6)
    float fpos_x = ImGui::GetCursorPosX();

    ImVec2 fsize(button_f12_width, 0); //Keep consistent size, based off of F12

    if (ImGui::Button("F1", fsize)) { message_key = VK_F1; } ImGui::SameLine();
    if (ImGui::Button("F2", fsize)) { message_key = VK_F2; } ImGui::SameLine();
    if (ImGui::Button("F3", fsize)) { message_key = VK_F3; } ImGui::SameLine();
    if (ImGui::Button("F4", fsize)) { message_key = VK_F4; } ImGui::SameLine();
    if (ImGui::Button("F5", fsize)) { message_key = VK_F5; } ImGui::SameLine();
    if (ImGui::Button("F6", fsize)) { message_key = VK_F6; } ImGui::SameLine();

    //Arrow Up
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - (button_right_width * 2) - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::ArrowButton("Up", ImGuiDir_Up)) { message_key = VK_UP; }

    //Esc, Tab
    if (ImGui::Button("Esc", fsize)) { message_key = VK_ESCAPE; } ImGui::SameLine();
    if (ImGui::Button("Tab", fsize)) { message_key = VK_TAB;    } ImGui::SameLine();
    if (ImGui::Button("Del", fsize)) { message_key = VK_DELETE; } ImGui::SameLine();

    //Function keys (F7 - F12)
    ImGui::SetCursorPosX(fpos_x);

    if (ImGui::Button("F7", fsize))  { message_key = VK_F7;  } ImGui::SameLine();
    if (ImGui::Button("F8", fsize))  { message_key = VK_F8;  } ImGui::SameLine();
    if (ImGui::Button("F9", fsize))  { message_key = VK_F9;  } ImGui::SameLine();
    if (ImGui::Button("F10", fsize)) { message_key = VK_F10; } ImGui::SameLine();
    if (ImGui::Button("F11", fsize)) { message_key = VK_F11; } ImGui::SameLine();
    if (ImGui::Button("F12"))        { message_key = VK_F12; } ImGui::SameLine();

    button_f12_width = ImGui::GetItemRectSize().x;

    //Remaining Arrow keys
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - (button_right_width * 3) - ImGui::GetStyle().ItemSpacing.x - ImGui::GetStyle().ItemSpacing.x);

    if (ImGui::ArrowButton("Left", ImGuiDir_Left))   { message_key = VK_LEFT;  } ImGui::SameLine();
    if (ImGui::ArrowButton("Down", ImGuiDir_Down))   { message_key = VK_DOWN;  } ImGui::SameLine();
    if (ImGui::ArrowButton("Right", ImGuiDir_Right)) { message_key = VK_RIGHT; } ImGui::SameLine();

    button_right_width = ImGui::GetItemRectSize().x;

    if (message_key != 0) //Something was pressed, send it over to the dashboard app
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_helper, message_key);
    }

    ImGui::End();

    //Pop keyboard styling
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(4);
}

bool WindowKeyboardHelper::IsVisible()
{
    return ( (ConfigManager::Get().GetConfigBool(configid_bool_input_keyboard_helper_enabled)) &&
             (ConfigManager::Get().GetConfigInt(configid_int_state_keyboard_visible_for_overlay_id) != -1) );
}

void WindowKeyboardHelper::Hide()
{
    //Nothing to do for Show(), but release held modifiers when hiding
    //And yes, this could conflict with somebody actually holding them down on a real keyboard, 
    //but the damage is less than some ghost key still being pressed after the keyboard helper was dismissed or VR was quit completely
    bool is_ctrl_down  = (GetAsyncKeyState(VK_CONTROL) < 0);
    bool is_alt_down   = (GetAsyncKeyState(VK_MENU)    < 0);
    bool is_shift_down = (GetAsyncKeyState(VK_SHIFT)   < 0);
    bool is_win_down   = (GetAsyncKeyState(VK_LWIN)    < 0);

    if (is_ctrl_down)
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_helper, VK_CONTROL);
    if (is_alt_down)
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_helper, VK_MENU);
    if (is_shift_down)
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_helper, VK_SHIFT);
    if (is_win_down)
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_keyboard_helper, VK_LWIN);
}
