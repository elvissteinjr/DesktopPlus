#include "InputSimulator.h"

#include "InterprocessMessaging.h"
#include "OutputManager.h"
#include "Util.h"

void InputSimulator::SetEventForMouseKeyCode(INPUT& input_event, unsigned char keycode, bool down) const
{
    input_event.type = INPUT_MOUSE;

    if (down)
    {
        switch (keycode)
        {
            case VK_LBUTTON:  input_event.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;   break;
            case VK_RBUTTON:  input_event.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;  break;
            case VK_MBUTTON:  input_event.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; break;
            case VK_XBUTTON1: input_event.mi.dwFlags = MOUSEEVENTF_XDOWN;
                              input_event.mi.mouseData = XBUTTON1;             break;
            case VK_XBUTTON2: input_event.mi.dwFlags = MOUSEEVENTF_XDOWN;
                              input_event.mi.mouseData = XBUTTON2;             break;
            default:          break;
        }
    }
    else
    {
        switch (keycode)
        {
            case VK_LBUTTON:  input_event.mi.dwFlags = MOUSEEVENTF_LEFTUP;   break;
            case VK_RBUTTON:  input_event.mi.dwFlags = MOUSEEVENTF_RIGHTUP;  break;
            case VK_MBUTTON:  input_event.mi.dwFlags = MOUSEEVENTF_MIDDLEUP; break;
            case VK_XBUTTON1: input_event.mi.dwFlags = MOUSEEVENTF_XUP;
                              input_event.mi.mouseData = XBUTTON1;           break;
            case VK_XBUTTON2: input_event.mi.dwFlags = MOUSEEVENTF_XUP;
                              input_event.mi.mouseData = XBUTTON2;           break;
            default:          break;
        }
    }


}

InputSimulator::InputSimulator() : 
    m_SpaceMultiplierX(1.0f), m_SpaceMultiplierY(1.0f), m_SpaceOffsetX(0), m_SpaceOffsetY(0), m_ForwardToElevatedModeProcess(false), m_ElevatedModeHasTextQueued(false)
{
    RefreshScreenOffsets();
}

void InputSimulator::RefreshScreenOffsets()
{
    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_refresh);
    }

    m_SpaceMultiplierX = 65536.0f / GetSystemMetrics(SM_CXVIRTUALSCREEN);
    m_SpaceMultiplierY = 65536.0f / GetSystemMetrics(SM_CYVIRTUALSCREEN);

    m_SpaceOffsetX = GetSystemMetrics(SM_XVIRTUALSCREEN) * -1;
    m_SpaceOffsetY = GetSystemMetrics(SM_YVIRTUALSCREEN) * -1;
}

void InputSimulator::MouseMove(int x, int y)
{
    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_mouse_move, MAKELPARAM(x, y));
        return;
    }

    INPUT input_event = { 0 };

    input_event.type = INPUT_MOUSE;
    input_event.mi.dx = (x + m_SpaceOffsetX) * m_SpaceMultiplierX;
    input_event.mi.dy = (y + m_SpaceOffsetY) * m_SpaceMultiplierY;
    input_event.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_ABSOLUTE;
    
    ::SendInput(1, &input_event, sizeof(INPUT));
}

void InputSimulator::MouseSetLeftDown(bool down)
{
    (down) ? KeyboardSetDown(VK_LBUTTON) : KeyboardSetUp(VK_LBUTTON);
}

void InputSimulator::MouseSetRightDown(bool down)
{
    (down) ? KeyboardSetDown(VK_RBUTTON) : KeyboardSetUp(VK_RBUTTON);
}

void InputSimulator::MouseSetMiddleDown(bool down)
{
    (down) ? KeyboardSetDown(VK_MBUTTON) : KeyboardSetUp(VK_MBUTTON);
}

void InputSimulator::MouseWheelHorizontal(float delta)
{
    INPUT input_event = { 0 };

    input_event.type = INPUT_MOUSE;
    input_event.mi.dwFlags = MOUSEEVENTF_HWHEEL;
    input_event.mi.mouseData = WHEEL_DELTA * delta;

    ::SendInput(1, &input_event, sizeof(INPUT));
}

void InputSimulator::MouseWheelVertical(float delta)
{
    INPUT input_event = { 0 };

    input_event.type = INPUT_MOUSE;
    input_event.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input_event.mi.mouseData = WHEEL_DELTA * delta;

    ::SendInput(1, &input_event, sizeof(INPUT));
}

void InputSimulator::KeyboardSetDown(unsigned char keycode)
{
    if (keycode == 0)
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        unsigned char elevated_keycodes[sizeof(LPARAM)] = {0};
        elevated_keycodes[0] = keycode;

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_down, *(LPARAM*)&elevated_keycodes);
        return;
    }

    //Check if the mouse buttons are swapped as this also affects SendInput
    if ( ((keycode == VK_LBUTTON) || (keycode == VK_RBUTTON)) && (::GetSystemMetrics(SM_SWAPBUTTON) != 0) )
    {
        keycode = (keycode == VK_LBUTTON) ? VK_RBUTTON : VK_LBUTTON;
    }

    INPUT input_event = { 0 };

    if (GetAsyncKeyState(keycode) < 0)  //Only send if not already pressed
        return;

    if ((keycode <= 6) && (keycode != VK_CANCEL)) //Mouse buttons need to be handled differently
    {
        SetEventForMouseKeyCode(input_event, keycode, true);
    }
    else
    {
        input_event.type = INPUT_KEYBOARD;
        input_event.ki.dwFlags = 0;
        input_event.ki.wVk = keycode;
    }

    ::SendInput(1, &input_event, sizeof(INPUT));
}

void InputSimulator::KeyboardSetUp(unsigned char keycode)
{
    if (keycode == 0)
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        unsigned char elevated_keycodes[sizeof(LPARAM)] = {0};
        elevated_keycodes[0] = keycode;

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_up, *(LPARAM*)&elevated_keycodes);
        return;
    }

    //Check if the mouse buttons are swapped as this also affects SendInput
    if ( ((keycode == VK_LBUTTON) || (keycode == VK_RBUTTON)) && (::GetSystemMetrics(SM_SWAPBUTTON) != 0) )
    {
        keycode = (keycode == VK_LBUTTON) ? VK_RBUTTON : VK_LBUTTON;
    }

    INPUT input_event = { 0 };

    if (GetAsyncKeyState(keycode) >= 0) //Only send if already down
        return;

    if ((keycode <= 6) && (keycode != VK_CANCEL)) //Mouse buttons need to be handled differently
    {
        SetEventForMouseKeyCode(input_event, keycode, false);
    }
    else
    {
        input_event.type = INPUT_KEYBOARD;
        input_event.ki.dwFlags = KEYEVENTF_KEYUP;
        input_event.ki.wVk = keycode;
    }

    ::SendInput(1, &input_event, sizeof(INPUT));
}

//Why so awfully specific, seems wasteful? Spamming the key events separately can confuse applications sometimes and we want to make sure the keys are really pressed at once
void InputSimulator::KeyboardSetDown(unsigned char keycodes[3])
{
    if (m_ForwardToElevatedModeProcess)
    {
        unsigned char elevated_keycodes[sizeof(LPARAM)] = {0};
        std::copy(keycodes, keycodes + 3, elevated_keycodes);

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_down, *(LPARAM*)&elevated_keycodes);
        return;
    }

    INPUT input_event[3] = { 0 };

    int used_event_count = 0;
    for (int i = 0; i < 3; ++i)
    {
        if ( (keycodes[i] == 0) || (GetAsyncKeyState(keycodes[i]) < 0) )  //Most significant bit set, meaning pressed
            continue; //Nothing to be done, skip

        if ((keycodes[i] <= 6) && (keycodes[i] != VK_CANCEL)) //Mouse buttons need to be handled differently
        {
            SetEventForMouseKeyCode(input_event[used_event_count], keycodes[i], true);
        }
        else
        {
            input_event[used_event_count].type = INPUT_KEYBOARD;
            input_event[used_event_count].ki.dwFlags = 0;
            input_event[used_event_count].ki.wVk = keycodes[i];
        }

        used_event_count++;
    }

    if (used_event_count != 0)
    {
        ::SendInput(used_event_count, input_event, sizeof(INPUT));
    }
}

void InputSimulator::KeyboardSetUp(unsigned char keycodes[3])
{
    if (m_ForwardToElevatedModeProcess)
    {
        unsigned char elevated_keycodes[sizeof(LPARAM)] = {0};
        std::copy(keycodes, keycodes + 3, elevated_keycodes);

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_up, *(LPARAM*)&elevated_keycodes);
        return;
    }

    INPUT input_event[3] = { 0 };

    int used_event_count = 0;
    for (int i = 0; i < 3; ++i)
    {
        if ( (keycodes[i] == 0) || (GetAsyncKeyState(keycodes[i]) > 0) )    //Most significant bit not set, meaning not pressed
            continue; //Nothing to be done, skip

        if ((keycodes[i] <= 6) && (keycodes[i] != VK_CANCEL)) //Mouse buttons need to be handled differently
        {
            SetEventForMouseKeyCode(input_event[used_event_count], keycodes[i], false);
        }
        else
        {
            input_event[used_event_count].type = INPUT_KEYBOARD;
            input_event[used_event_count].ki.dwFlags = KEYEVENTF_KEYUP;
            input_event[used_event_count].ki.wVk = keycodes[i];
        }

        used_event_count++;
    }

    if (used_event_count != 0)
    {
        ::SendInput(used_event_count, input_event, sizeof(INPUT));
    }
}

void InputSimulator::KeyboardPressAndRelease(unsigned char keycode)
{
    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_press_and_release, keycode);
        return;
    }

    if (keycode == 0)
        return;

    INPUT input_event[2] = { 0 };

    bool already_down = (GetAsyncKeyState(keycode) < 0);    //Most significant bit set, meaning pressed
    int upid = 1 - already_down;

    if ((keycode <= 6) && (keycode != VK_CANCEL)) //Mouse buttons need to be handled differently
    {
        if (!already_down)  //Only send down event if not already pressed
        {
            SetEventForMouseKeyCode(input_event[0], keycode, true);
        }

        SetEventForMouseKeyCode(input_event[upid], keycode, false);
    }
    else
    {
        if (!already_down)  //Only send down event if not already pressed
        {
            input_event[0].type = INPUT_KEYBOARD;
            input_event[0].ki.dwFlags = 0;
            input_event[0].ki.wVk = keycode;
        }

        input_event[upid].type = INPUT_KEYBOARD;
        input_event[upid].ki.dwFlags = KEYEVENTF_KEYUP;
        input_event[upid].ki.wVk = keycode;
    }

    ::SendInput(upid + 1, input_event, sizeof(INPUT));
}

#include "CommonTypes.h"

void InputSimulator::KeyboardText(const char* str_utf8, bool always_use_unicode_event)
{
    if (m_ForwardToElevatedModeProcess)
    {
        OutputManager* outmgr = OutputManager::Get();

        if (outmgr != nullptr)
        {
            IPCElevatedStringID str_id = (always_use_unicode_event) ? ipcestrid_keyboard_text_force_unicode : ipcestrid_keyboard_text;
            IPCManager::Get().SendStringToElevatedModeProcess(str_id, str_utf8, outmgr->GetWindowHandle());
            m_ElevatedModeHasTextQueued = true;
        }
        return;
    }

    //Convert to UTF16
    std::wstring wstr = WStringConvertFromUTF8(str_utf8);

    INPUT input_event = { 0 };
    input_event.type = INPUT_KEYBOARD;

    if (!always_use_unicode_event)
    {
        //This function could just use KEYEVENTF_UNICODE on all printable characters to get working text input (we still do that for type string actions though)
        //However, in order to trigger shortcuts and non-text events in applications, at least 0-9 & A-Z are simulated as proper key events
        //For consistent handling, capslock and shift are reset at the start, but that shouldn't be an issue in practice

        if ((GetKeyState(VK_CAPITAL) & 0x0001) != 0) //Turn off capslock if it's on
        {
            input_event.ki.dwFlags = 0;
            input_event.ki.wVk = VK_CAPITAL;
            m_KeyboardTextQueue.push_back(input_event);

            input_event.ki.dwFlags = KEYEVENTF_KEYUP;
            m_KeyboardTextQueue.push_back(input_event);
        }

        if (GetAsyncKeyState(VK_SHIFT) < 0) //Release shift if it's down
        {
            input_event.ki.dwFlags = KEYEVENTF_KEYUP;
            input_event.ki.wVk = VK_SHIFT;
            m_KeyboardTextQueue.push_back(input_event);
        }

        for (wchar_t current_char: wstr)
        {
            if (current_char == '\b')   //Backspace, needs special handling
            {
                input_event.ki.dwFlags = 0;
                input_event.ki.wVk = VK_BACK;
                m_KeyboardTextQueue.push_back(input_event);

                input_event.ki.dwFlags = KEYEVENTF_KEYUP;
                m_KeyboardTextQueue.push_back(input_event);
            }
            else if (current_char == '\n')  //Enter, needs special handling
            {
                input_event.ki.dwFlags = 0;
                input_event.ki.wVk = VK_RETURN;
                m_KeyboardTextQueue.push_back(input_event);

                input_event.ki.dwFlags = KEYEVENTF_KEYUP;
                m_KeyboardTextQueue.push_back(input_event);
            }
            else if ( ((current_char >= '0') && (current_char <= '9')) || (current_char == ' ') ) //0 - 9 and space, simulate keydown/up                          
            {
                input_event.ki.dwFlags = 0;
                input_event.ki.wVk = current_char;
                m_KeyboardTextQueue.push_back(input_event);

                input_event.ki.dwFlags = KEYEVENTF_KEYUP;
                m_KeyboardTextQueue.push_back(input_event);
            }
            else if ((current_char >= 'a') && (current_char <= 'z')) //a - z, simulate keydown/up  
            {
                input_event.ki.dwFlags = 0;
                input_event.ki.wVk = current_char - ('a' - 'A');
                m_KeyboardTextQueue.push_back(input_event);

                input_event.ki.dwFlags = KEYEVENTF_KEYUP;
                m_KeyboardTextQueue.push_back(input_event);
            }
            else if ((current_char >= 'A') && (current_char <= 'Z')) //A - Z, simulate keydown/up  
            {
                input_event.ki.dwFlags = 0;
                input_event.ki.wVk = VK_SHIFT;
                m_KeyboardTextQueue.push_back(input_event);

                input_event.ki.dwFlags = 0;
                input_event.ki.wVk = current_char;
                m_KeyboardTextQueue.push_back(input_event);

                input_event.ki.dwFlags = KEYEVENTF_KEYUP;
                m_KeyboardTextQueue.push_back(input_event);

                input_event.ki.wVk = VK_SHIFT;
                m_KeyboardTextQueue.push_back(input_event);
            }
            else
            {
                input_event.ki.dwFlags = KEYEVENTF_UNICODE;
                input_event.ki.wVk = 0;
                input_event.ki.wScan = current_char;
                m_KeyboardTextQueue.push_back(input_event);

                input_event.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
                m_KeyboardTextQueue.push_back(input_event);
            }
        }
    }
    else
    {
        for (wchar_t current_char: wstr)
        {
            input_event.ki.dwFlags = KEYEVENTF_UNICODE;
            input_event.ki.wScan = current_char;
            m_KeyboardTextQueue.push_back(input_event);

            input_event.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            m_KeyboardTextQueue.push_back(input_event);
        }
    }
}

void InputSimulator::KeyboardTextFinish()
{
    if (m_ForwardToElevatedModeProcess)
    {
        //Only send if we know there is queued text in that process
        if (m_ElevatedModeHasTextQueued)
        {
            IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_keyboard_text_finish);
            m_ElevatedModeHasTextQueued = false;
        }
        return;
    }

    if (!m_KeyboardTextQueue.empty())
    {
        ::SendInput(m_KeyboardTextQueue.size(), m_KeyboardTextQueue.data(), sizeof(INPUT));

        m_KeyboardTextQueue.clear();
    }
}

void InputSimulator::SetElevatedModeForwardingActive(bool do_forward)
{
    m_ForwardToElevatedModeProcess = do_forward;
}
