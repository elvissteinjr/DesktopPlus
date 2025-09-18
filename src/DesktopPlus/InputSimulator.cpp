#include "InputSimulator.h"

#include "InterprocessMessaging.h"
#include "OutputManager.h"
#include "Util.h"

enum KeyboardWin32KeystateFlags
{
    kbd_w32keystate_flag_shift_down       = 1 << 0,
    kbd_w32keystate_flag_ctrl_down        = 1 << 1,
    kbd_w32keystate_flag_alt_down         = 1 << 2
};

fn_CreateSyntheticPointerDevice  InputSimulator::s_p_CreateSyntheticPointerDevice  = nullptr;
fn_InjectSyntheticPointerInput   InputSimulator::s_p_InjectSyntheticPointerInput   = nullptr;
fn_DestroySyntheticPointerDevice InputSimulator::s_p_DestroySyntheticPointerDevice = nullptr;

InputSimulator::InputSimulator()
{
    RefreshScreenOffsets();

    //Try to init pen functions if they're not loaded yet
    if (!IsPenSimulationSupported())
    {
        LoadPenFunctions();
    }

    //Init pen state (rest can stay at default 0)
    m_PenState.type = PT_PEN;
    m_PenState.penInfo.pointerInfo.pointerType = PT_PEN;
    m_PenState.penInfo.pressure = 1024;                     //Full pressure
    m_PenState.penInfo.penMask = PEN_MASK_PRESSURE;         //Marked as optional, but without, applications expecting it may read 0 pressure still
}

InputSimulator::~InputSimulator()
{
    if (m_PenDevice != nullptr)
    {
        s_p_DestroySyntheticPointerDevice(m_PenDevice);
    }
}

void InputSimulator::SetEventForMouseKeyCode(INPUT& input_event, unsigned char keycode, bool down)
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

bool InputSimulator::SetEventForKeyCode(INPUT& input_event, unsigned char keycode, bool down, bool skip_check)
{
    //Check if the mouse buttons are swapped as this also affects SendInput
    if ( ((keycode == VK_LBUTTON) || (keycode == VK_RBUTTON)) && (::GetSystemMetrics(SM_SWAPBUTTON) != 0) )
    {
        keycode = (keycode == VK_LBUTTON) ? VK_RBUTTON : VK_LBUTTON;
    }

    bool key_down = (::GetAsyncKeyState(keycode) < 0);

    if ( (keycode == 0) || ((key_down == down) && (!skip_check)) )
        return false;

    if ((keycode <= 6) && (keycode != VK_CANCEL)) //Mouse buttons need to be handled differently
    {
        SetEventForMouseKeyCode(input_event, keycode, down);
    }
    else
    {
        input_event.type = INPUT_KEYBOARD;

        //Use scancodes if possible to increase compatibility (e.g. DirectInput games need scancodes)
        UINT scancode = ::MapVirtualKey(keycode, MAPVK_VK_TO_VSC_EX);

        //Pause/PrintScreen have a scancode too long for SendInput. May be possible to get to work with multiple input calls, but let's not bother for now
        if ( (scancode != 0) || (keycode == VK_PAUSE) || (keycode == VK_SNAPSHOT) )
        {
            BYTE highbyte = HIBYTE(scancode);
            bool is_extended = ((highbyte == 0xe0) || (highbyte == 0xe1));

            //Not extended but needs to be for proper input simulation
            if (!is_extended)
            {
                switch (keycode)
                {
                    case VK_INSERT:
                    case VK_DELETE:
                    case VK_PRIOR:
                    case VK_NEXT:
                    case VK_END:
                    case VK_HOME:
                    case VK_LEFT:
                    case VK_UP:
                    case VK_RIGHT:
                    case VK_DOWN:
                    {
                        is_extended = true;
                    }
                    default: break;
                }
            }

            input_event.ki.dwFlags = (is_extended) ? KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY : KEYEVENTF_SCANCODE;
            input_event.ki.wScan   = scancode;
        }
        else //No scancode, use keycode
        {
            input_event.ki.dwFlags = 0;
            input_event.ki.wVk     = keycode;
        }

        if (!down)
        {
            input_event.ki.dwFlags |= KEYEVENTF_KEYUP;
        }
    }

    return true;
}

void InputSimulator::LoadPenFunctions()
{
    HMODULE h_user32 = ::LoadLibraryW(L"user32.dll");

    if (h_user32 != nullptr)
    {
        s_p_CreateSyntheticPointerDevice  = (fn_CreateSyntheticPointerDevice) GetProcAddress(h_user32, "CreateSyntheticPointerDevice");
        s_p_InjectSyntheticPointerInput   = (fn_InjectSyntheticPointerInput)  GetProcAddress(h_user32, "InjectSyntheticPointerInput");
        s_p_DestroySyntheticPointerDevice = (fn_DestroySyntheticPointerDevice)GetProcAddress(h_user32, "DestroySyntheticPointerDevice");
    }
}

void InputSimulator::CreatePenDeviceIfNeeded()
{
    if (m_PenDevice == nullptr)
    {
        m_PenDevice = s_p_CreateSyntheticPointerDevice(PT_PEN, 1, POINTER_FEEDBACK_INDIRECT);
    }
}

void InputSimulator::RefreshScreenOffsets()
{
    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_refresh);
    }

    m_SpaceMaxX = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    m_SpaceMaxY = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    m_SpaceMultiplierX = 65536.0f / m_SpaceMaxX;
    m_SpaceMultiplierY = 65536.0f / m_SpaceMaxY;

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

    input_event.type       = INPUT_MOUSE;
    input_event.mi.dx      = LONG((x + m_SpaceOffsetX) * m_SpaceMultiplierX);
    input_event.mi.dy      = LONG((y + m_SpaceOffsetY) * m_SpaceMultiplierY);
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
    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_mouse_hwheel, pun_cast<LPARAM, float>(delta));
        return;
    }

    INPUT input_event = {0};

    input_event.type         = INPUT_MOUSE;
    input_event.mi.dwFlags   = MOUSEEVENTF_HWHEEL;
    input_event.mi.mouseData = DWORD(WHEEL_DELTA * delta);

    ::SendInput(1, &input_event, sizeof(INPUT));
}

void InputSimulator::MouseWheelVertical(float delta)
{
    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_mouse_vwheel, pun_cast<LPARAM, float>(delta));
        return;
    }

    INPUT input_event = {0};

    input_event.type         = INPUT_MOUSE;
    input_event.mi.dwFlags   = MOUSEEVENTF_WHEEL;
    input_event.mi.mouseData = DWORD(WHEEL_DELTA * delta);

    ::SendInput(1, &input_event, sizeof(INPUT));
}

void InputSimulator::PenMove(int x, int y)
{
    if (!IsPenSimulationSupported())
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_pen_move, MAKELPARAM(x, y));
        return;
    }

    CreatePenDeviceIfNeeded();

    m_PenState.penInfo.pointerInfo.pointerFlags |= POINTER_FLAG_INRANGE | POINTER_FLAG_UPDATE;

    //Pen input position doesn't appear to be clamped by OS like mouse input and can do weird things if it goes out of range
    m_PenState.penInfo.pointerInfo.ptPixelLocation.x = clamp(x + m_SpaceOffsetX, 0, m_SpaceMaxX);
    m_PenState.penInfo.pointerInfo.ptPixelLocation.y = clamp(y + m_SpaceOffsetY, 0, m_SpaceMaxY);

    s_p_InjectSyntheticPointerInput(m_PenDevice, &m_PenState, 1);
}

void InputSimulator::PenSetPrimaryDown(bool down)
{
    if (!IsPenSimulationSupported())
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, (down) ? ipceact_pen_button_down : ipceact_pen_button_up, 0);
        return;
    }

    CreatePenDeviceIfNeeded();

    if (down)
    {
        m_PenState.penInfo.pointerInfo.pointerFlags |= POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN | POINTER_FLAG_FIRSTBUTTON;
        m_PenState.penInfo.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;
    }
    else
    {
        m_PenState.penInfo.pointerInfo.pointerFlags &= ~(POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN | POINTER_FLAG_FIRSTBUTTON);
        m_PenState.penInfo.pointerInfo.pointerFlags |= POINTER_FLAG_UP;
        m_PenState.penInfo.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;
    }

    m_PenState.penInfo.pointerInfo.pointerFlags &= ~POINTER_FLAG_UPDATE;

    s_p_InjectSyntheticPointerInput(m_PenDevice, &m_PenState, 1);

    m_PenState.penInfo.pointerInfo.pointerFlags &= ~(POINTER_FLAG_DOWN | POINTER_FLAG_UP);
    m_PenState.penInfo.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
}

void InputSimulator::PenSetSecondaryDown(bool down)
{
    if (!IsPenSimulationSupported())
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, (down) ? ipceact_pen_button_down : ipceact_pen_button_up, 1);
        return;
    }

    CreatePenDeviceIfNeeded();

    if (down)
    {
        m_PenState.penInfo.pointerInfo.pointerFlags |= POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN | POINTER_FLAG_SECONDBUTTON;
        m_PenState.penInfo.pointerInfo.ButtonChangeType = POINTER_CHANGE_SECONDBUTTON_DOWN;
        m_PenState.penInfo.penFlags = PEN_FLAG_BARREL;
    }
    else
    {
        m_PenState.penInfo.pointerInfo.pointerFlags &= ~(POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN | POINTER_FLAG_SECONDBUTTON);
        m_PenState.penInfo.pointerInfo.pointerFlags |= POINTER_FLAG_UP;
        m_PenState.penInfo.pointerInfo.ButtonChangeType = POINTER_CHANGE_SECONDBUTTON_UP;
        m_PenState.penInfo.penFlags = 0;
    }

    m_PenState.penInfo.pointerInfo.pointerFlags &= ~POINTER_FLAG_UPDATE;

    s_p_InjectSyntheticPointerInput(m_PenDevice, &m_PenState, 1);

    m_PenState.penInfo.pointerInfo.pointerFlags &= ~(POINTER_FLAG_DOWN | POINTER_FLAG_UP);
    m_PenState.penInfo.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
}

void InputSimulator::PenLeave()
{
    if (!IsPenSimulationSupported())
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_pen_leave);
        return;
    }

    CreatePenDeviceIfNeeded();

    if (m_PenState.penInfo.pointerInfo.pointerFlags & POINTER_FLAG_INRANGE)
    {
        m_PenState.penInfo.pointerInfo.pointerFlags = POINTER_FLAG_UPDATE;
        m_PenState.penInfo.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
        m_PenState.penInfo.penFlags = 0;

        s_p_InjectSyntheticPointerInput(m_PenDevice, &m_PenState, 1);
    }
}

void InputSimulator::KeyboardSetDown(unsigned char keycode)
{
    if (keycode == 0)
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        LPARAM elevated_keycodes = 0;
        memcpy(&elevated_keycodes, &keycode, 1);

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_down, elevated_keycodes);
        return;
    }

    INPUT input_event = {0};

    if (SetEventForKeyCode(input_event, keycode, true))
    {
        ::SendInput(1, &input_event, sizeof(INPUT));
    }
}

void InputSimulator::KeyboardSetDown(unsigned char keycode, bool down)
{
    (down) ? KeyboardSetDown(keycode) : KeyboardSetUp(keycode);
}

void InputSimulator::KeyboardSetUp(unsigned char keycode)
{
    if (keycode == 0)
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        LPARAM elevated_keycodes = 0;
        memcpy(&elevated_keycodes, &keycode, 1);

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_up, elevated_keycodes);
        return;
    }

    INPUT input_event = {0};

    if (SetEventForKeyCode(input_event, keycode, false))
    {
        ::SendInput(1, &input_event, sizeof(INPUT));
    }
}

//Why so awfully specific, seems wasteful? Spamming the key events separately can confuse applications sometimes and we want to make sure the keys are really pressed at once
void InputSimulator::KeyboardSetDown(unsigned char keycodes[3])
{
    if (m_ForwardToElevatedModeProcess)
    {
        LPARAM elevated_keycodes = 0;
        memcpy(&elevated_keycodes, keycodes, 3);

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_down, elevated_keycodes);
        return;
    }

    INPUT input_event[3] = { 0 };

    int used_event_count = 0;
    for (int i = 0; i < 3; ++i)
    {
        used_event_count += SetEventForKeyCode(input_event[used_event_count], keycodes[i], true);
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
        LPARAM elevated_keycodes = 0;
        memcpy(&elevated_keycodes, keycodes, 3);

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_up, elevated_keycodes);
        return;
    }

    INPUT input_event[3] = { 0 };

    int used_event_count = 0;
    for (int i = 0; i < 3; ++i)
    {
        used_event_count += SetEventForKeyCode(input_event[used_event_count], keycodes[i], false);
    }

    if (used_event_count != 0)
    {
        ::SendInput(used_event_count, input_event, sizeof(INPUT));
    }
}

void InputSimulator::KeyboardToggleState(unsigned char keycode)
{
    if (keycode == 0)
        return;

    //GetAsyncKeyState is subject to UIPI, so always forward it
    if (m_ForwardToElevatedModeProcess)
    {
        LPARAM elevated_keycodes = 0;
        memcpy(&elevated_keycodes, &keycode, 1);

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_toggle, elevated_keycodes);
        return;
    }

    if (IsKeyDown(keycode))  //If already pressed, release key
    {
        KeyboardSetUp(keycode);
    }
    else
    {
        KeyboardSetDown(keycode);
    }
}

void InputSimulator::KeyboardToggleState(unsigned char keycodes[3])
{
    if (m_ForwardToElevatedModeProcess)
    {
        LPARAM elevated_keycodes = 0;
        memcpy(&elevated_keycodes, keycodes, 3);

        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_toggle, elevated_keycodes);
        return;
    }

    INPUT input_event[3] = {0};
    int used_event_count = 0;

    for (int i = 0; i < 3; ++i)
    {
        used_event_count += SetEventForKeyCode(input_event[used_event_count], keycodes[i], !IsKeyDown(keycodes[i]));
    }

    if (used_event_count != 0)
    {
        ::SendInput(used_event_count, input_event, sizeof(INPUT));
    }
}

void InputSimulator::KeyboardPressAndRelease(unsigned char keycode)
{
    if (keycode == 0)
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_press_and_release, keycode);
        return;
    }

    INPUT input_event[2] = {0};
    int used_event_count = 0;

    used_event_count += SetEventForKeyCode(input_event[used_event_count], keycode, true);
    used_event_count += SetEventForKeyCode(input_event[used_event_count], keycode, false);

    ::SendInput(used_event_count, input_event, sizeof(INPUT));
}

void InputSimulator::KeyboardSetToggleKey(unsigned char keycode, bool toggled)
{
    if (keycode == 0) 
        return;

    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_key_togglekey_set, MAKELPARAM(keycode, toggled));
        return;
    }

    bool is_toggled = ((::GetKeyState(keycode) & 0x0001) != 0);

    if (toggled == is_toggled)
        return;

    INPUT input_event[3] = {0};
    int used_event_count = 0;

    used_event_count += SetEventForKeyCode(input_event[used_event_count], keycode, false);       //Release if it happens to be down
    used_event_count += SetEventForKeyCode(input_event[used_event_count], keycode, true,  true); //Press...
    used_event_count += SetEventForKeyCode(input_event[used_event_count], keycode, false, true); //...and release, even if the state wouldn't change (last arg)

    ::SendInput(used_event_count, input_event, sizeof(INPUT));
}

void InputSimulator::KeyboardSetFromWin32KeyState(unsigned short keystate, bool down)
{
    unsigned char keycode  = LOBYTE(keystate);
    bool key_down = IsKeyDown(keycode);

    if (key_down == down)
        return; //Nothing to be done

    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_keystate_w32_set, MAKELPARAM(keystate, down));
        return;
    }

    unsigned char flags = HIBYTE(keystate);
    bool flag_shift   = (flags & kbd_w32keystate_flag_shift_down);
    bool flag_ctrl    = (flags & kbd_w32keystate_flag_ctrl_down);
    bool flag_alt     = (flags & kbd_w32keystate_flag_alt_down);
    bool caps_toggled = ((::GetKeyState(VK_CAPITAL) & 0x0001) != 0);

    INPUT input_event[16] = { 0 };
    int used_event_count = 0;

    //Add events for modifier keys if needed
    if (!flag_shift) 
    {
        //Try releasing both if any is down
        if (IsKeyDown(VK_SHIFT))
        {
            used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_LSHIFT, false);
            used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_RSHIFT, false);
        }
    }
    else if (!IsKeyDown(VK_SHIFT)) //Only set the left key if none are down
    {
        used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_LSHIFT, true);
    }

    if (!flag_ctrl)
    { 
        if (IsKeyDown(VK_CONTROL))
        {
            used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_LCONTROL, false);
            used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_RCONTROL, false);
        }
    }
    else if (!IsKeyDown(VK_CONTROL))
    {
        used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_LCONTROL, true);
    }

    if (!flag_alt)
    { 
        if (IsKeyDown(VK_MENU))
        {
            used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_LMENU, false);
            used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_RMENU, false);
        }
    }
    else if (!IsKeyDown(VK_MENU))
    {
        used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_LMENU, true);
    }

    //Add events to handle caps lock state
    if (caps_toggled)
    {
        used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_CAPITAL, false);       //Release if it happens to be down
        used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_CAPITAL, true,  true); //Press...
        used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_CAPITAL, false, true); //...and release, even if the state wouldn't change (last arg)
    }

    //Add event for actual keycode
    used_event_count += SetEventForKeyCode(input_event[used_event_count], keycode, down);

    if (used_event_count != 0)
    {
        ::SendInput(used_event_count, input_event, sizeof(INPUT));
    }
}

void InputSimulator::KeyboardSetKeyState(IPCKeyboardKeystateFlags flags, unsigned char keycode)
{
    if (m_ForwardToElevatedModeProcess)
    {
        IPCManager::Get().PostMessageToElevatedModeProcess(ipcmsg_elevated_action, ipceact_keystate_set, MAKELPARAM(flags, keycode));
        return;
    }

    //Numpad input simulation is a bit weird
    //It seems like numpad (especially shift+numpad) is something that is taken care of in a lower layer and as such confuses Windows if we just send our straight inputs
    //Basically it either doesn't produce an input with numlock off or shift can get stuck in a way that requires both shift keys to be released
    //So we just end up handling this manually below
    if (((keycode >= VK_NUMPAD0) && (keycode <= VK_NUMPAD9)) || (keycode == VK_DECIMAL))
    {
        const bool is_numlock_on        = ((::GetKeyState(VK_NUMLOCK) & 0x0001) != 0);
        const bool is_shift_down        = ((flags & kbd_keystate_flag_lshift_down) || (flags & kbd_keystate_flag_rshift_down));
        const bool is_double_shift_down = ((flags & kbd_keystate_flag_lshift_down) && (flags & kbd_keystate_flag_rshift_down));

        if ((!is_numlock_on) || (is_shift_down))
        {
            //Swap numpad keycodes with regular ones
            //One might think these should be sent as unextended scancodes down the line, but doing so makes them be treated as numpad inputs again with all the issues we're trying to avoid here
            switch (keycode)
            {
                case VK_NUMPAD0: keycode = VK_INSERT; break;
                case VK_NUMPAD1: keycode = VK_END;    break;
                case VK_NUMPAD2: keycode = VK_DOWN;   break;
                case VK_NUMPAD3: keycode = VK_NEXT;   break;
                case VK_NUMPAD4: keycode = VK_LEFT;   break;
                case VK_NUMPAD5: keycode = VK_CLEAR;  break;
                case VK_NUMPAD6: keycode = VK_RIGHT;  break;
                case VK_NUMPAD7: keycode = VK_HOME;   break;
                case VK_NUMPAD8: keycode = VK_UP;     break;
                case VK_NUMPAD9: keycode = VK_PRIOR;  break;
                case VK_DECIMAL: keycode = VK_DELETE; break;
            }

            //Shift needs to be up to get normal cursor movement (unless both shifts are down)
            if ((is_numlock_on) && (is_shift_down) && (!is_double_shift_down))
            {
                flags = IPCKeyboardKeystateFlags(flags & ~(kbd_keystate_flag_lshift_down | kbd_keystate_flag_rshift_down));
            }
        }
    }

    INPUT input_event[10] = {0};
    int used_event_count = 0;

    //Add events for modifier keys if needed
    used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_LSHIFT,   (flags & kbd_keystate_flag_lshift_down));
    used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_RSHIFT,   (flags & kbd_keystate_flag_rshift_down));
    used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_LCONTROL, (flags & kbd_keystate_flag_lctrl_down));
    used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_RCONTROL, (flags & kbd_keystate_flag_rctrl_down));
    used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_LMENU,    (flags & kbd_keystate_flag_lalt_down));
    used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_RMENU,    (flags & kbd_keystate_flag_ralt_down));

    //Add events to handle caps lock state
    bool caps_toggled = ((::GetKeyState(VK_CAPITAL) & 0x0001) != 0);
    if (caps_toggled != bool(flags & kbd_keystate_flag_capslock_toggled))
    {
        used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_CAPITAL, false);       //Release if it happens to be down
        used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_CAPITAL, true,  true); //Press...
        used_event_count += SetEventForKeyCode(input_event[used_event_count], VK_CAPITAL, false, true); //...and release, even if the state wouldn't change (last arg)
    }

    //Add event for actual keycode, but skip keys handled by the keystate flags
    switch (keycode)
    {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_CAPITAL:
        {
            break;
        }

        default:
        {
            used_event_count += SetEventForKeyCode(input_event[used_event_count], keycode, (flags & kbd_keystate_flag_key_down));
        }
    }

    if (used_event_count != 0)
    {
        ::SendInput(used_event_count, input_event, sizeof(INPUT));
    }
}

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

        if ((::GetKeyState(VK_CAPITAL) & 0x0001) != 0) //Turn off capslock if it's on
        {
            input_event.ki.dwFlags = 0;
            input_event.ki.wVk = VK_CAPITAL;
            m_KeyboardTextQueue.push_back(input_event);

            input_event.ki.dwFlags = KEYEVENTF_KEYUP;
            m_KeyboardTextQueue.push_back(input_event);
        }

        if (::GetAsyncKeyState(VK_SHIFT) < 0) //Release shift if it's down
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
        ::SendInput((UINT)m_KeyboardTextQueue.size(), m_KeyboardTextQueue.data(), sizeof(INPUT));

        m_KeyboardTextQueue.clear();
    }
}

void InputSimulator::SetElevatedModeForwardingActive(bool do_forward)
{
    m_ForwardToElevatedModeProcess = do_forward;
}

bool InputSimulator::IsKeyDown(unsigned char keycode)
{
    //Check if the mouse buttons are swapped
    if ( ((keycode == VK_LBUTTON) || (keycode == VK_RBUTTON)) && (::GetSystemMetrics(SM_SWAPBUTTON) != 0) )
    {
        keycode = (keycode == VK_LBUTTON) ? VK_RBUTTON : VK_LBUTTON;
    }

    return (::GetAsyncKeyState(keycode) < 0);
}

bool InputSimulator::IsPenSimulationSupported()
{
    return (s_p_CreateSyntheticPointerDevice != nullptr);
}
