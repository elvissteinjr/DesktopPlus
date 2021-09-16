#ifndef _INPUTSIMULATOR_H_
#define _INPUTSIMULATOR_H_

#define NOMINMAX

#include <vector>
#include <windows.h>

//Dashboard_Back exists, but not doesn't map to "Go Back" ...okay!
#define Button_Dashboard_GoHome vr::k_EButton_IndexController_A
#define Button_Dashboard_GoBack vr::k_EButton_IndexController_B

enum IPCKeyboardKeystateFlags : unsigned char;

class InputSimulator
{
    private:
        float m_SpaceMultiplierX;
        float m_SpaceMultiplierY;
        int m_SpaceOffsetX;
        int m_SpaceOffsetY;

        std::vector<INPUT> m_KeyboardTextQueue;
        bool m_ForwardToElevatedModeProcess;
        bool m_ElevatedModeHasTextQueued;

        static void SetEventForMouseKeyCode(INPUT& input_event, unsigned char keycode, bool down);
        //Set the event if it would change key state. Returns if anything was written to input_event
        static bool SetEventForKeyCode(INPUT& input_event, unsigned char keycode, bool down, bool skip_check = false);

    public:
        InputSimulator();
        void RefreshScreenOffsets();

        void MouseMove(int x, int y);
        void MouseSetLeftDown(bool down);
        void MouseSetRightDown(bool down);
        void MouseSetMiddleDown(bool down);
        void MouseWheelHorizontal(float delta);
        void MouseWheelVertical(float delta);

        void KeyboardSetDown(unsigned char keycode);
        void KeyboardSetDown(unsigned char keycode, bool down);
        void KeyboardSetUp(unsigned char keycode);
        void KeyboardSetDown(unsigned char keycodes[3]);
        void KeyboardSetUp(unsigned char keycodes[3]);
        void KeyboardToggleState(unsigned char keycode);
        void KeyboardToggleState(unsigned char keycodes[3]);
        void KeyboardPressAndRelease(unsigned char keycode);
        void KeyboardSetToggleKey(unsigned char keycode, bool toggled);
        void KeyboardSetFromWin32KeyState(unsigned short keystate, bool down);          //Keystate as returned by VkKeyScan()
        void KeyboardSetKeyState(IPCKeyboardKeystateFlags flags, unsigned char keycode);
        void KeyboardText(const char* str_utf8, bool always_use_unicode_event = false);
        void KeyboardTextFinish();

        void SetElevatedModeForwardingActive(bool do_forward);

        static bool IsKeyDown(unsigned char keycode);
};

#endif