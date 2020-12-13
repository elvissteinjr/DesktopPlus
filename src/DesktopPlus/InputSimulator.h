#ifndef _INPUTSIMULATOR_H_
#define _INPUTSIMULATOR_H_

#define NOMINMAX

#include <vector>
#include <windows.h>

//Dashboard_Back exists, but not doesn't map to "Go Back" ...okay!
#define Button_Dashboard_GoHome vr::k_EButton_IndexController_A
#define Button_Dashboard_GoBack vr::k_EButton_IndexController_B

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

        void SetEventForMouseKeyCode(INPUT& input_event, unsigned char keycode, bool down) const;

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
        void KeyboardSetUp(unsigned char keycode);
        void KeyboardSetDown(unsigned char keycodes[3]);
        void KeyboardSetUp(unsigned char keycodes[3]);
        void KeyboardPressAndRelease(unsigned char keycode);
        void KeyboardText(const char* str_utf8, bool always_use_unicode_event = false);
        void KeyboardTextFinish();

        void SetElevatedModeForwardingActive(bool do_forward);
};

#endif