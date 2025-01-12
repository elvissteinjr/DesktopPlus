#ifndef _INPUTSIMULATOR_H_
#define _INPUTSIMULATOR_H_

#define NOMINMAX
#include <windows.h>

#include <vector>

//Dashboard_Back exists, but not doesn't map to "Go Back" ...okay!
#define Button_Dashboard_GoHome vr::k_EButton_IndexController_A
#define Button_Dashboard_GoBack vr::k_EButton_IndexController_B

enum IPCKeyboardKeystateFlags : unsigned char;

//SyntheticPointer functions are loaded manually to not require OS support to run the application (Windows 10 1809+ should have them though)
typedef HANDLE HSYNTHETICPOINTERDEVICE_DPLUS;
#ifndef NTDDI_WIN10_RS5
    typedef enum { POINTER_FEEDBACK_DEFAULT = 1, POINTER_FEEDBACK_INDIRECT = 2, POINTER_FEEDBACK_NONE = 3 } POINTER_FEEDBACK_MODE;
#endif

typedef HSYNTHETICPOINTERDEVICE_DPLUS (WINAPI* fn_CreateSyntheticPointerDevice) (_In_ POINTER_INPUT_TYPE pointerType, _In_ ULONG maxCount, _In_ POINTER_FEEDBACK_MODE mode);
typedef BOOL                          (WINAPI* fn_InjectSyntheticPointerInput)  (_In_ HSYNTHETICPOINTERDEVICE_DPLUS device, _In_reads_(count) CONST POINTER_TYPE_INFO* pointerInfo, _In_ UINT32 count);
typedef void                          (WINAPI* fn_DestroySyntheticPointerDevice)(_In_ HSYNTHETICPOINTERDEVICE_DPLUS device);

class InputSimulator
{
    private:
        int m_SpaceMaxX = 0;
        int m_SpaceMaxY = 0;
        float m_SpaceMultiplierX = 1.0f;
        float m_SpaceMultiplierY = 1.0f;
        int m_SpaceOffsetX = 0;
        int m_SpaceOffsetY = 0;

        static fn_CreateSyntheticPointerDevice  s_p_CreateSyntheticPointerDevice;
        static fn_InjectSyntheticPointerInput   s_p_InjectSyntheticPointerInput;
        static fn_DestroySyntheticPointerDevice s_p_DestroySyntheticPointerDevice;

        HSYNTHETICPOINTERDEVICE_DPLUS m_PenDevice = nullptr;
        POINTER_TYPE_INFO m_PenState = {0};

        std::vector<INPUT> m_KeyboardTextQueue;
        bool m_ForwardToElevatedModeProcess = false;
        bool m_ElevatedModeHasTextQueued    = false;

        void CreatePenDeviceIfNeeded();

        static void LoadPenFunctions();
        static void SetEventForMouseKeyCode(INPUT& input_event, unsigned char keycode, bool down);
        //Set the event if it would change key state. Returns if anything was written to input_event
        static bool SetEventForKeyCode(INPUT& input_event, unsigned char keycode, bool down, bool skip_check = false);

    public:
        InputSimulator();
        ~InputSimulator();
        void RefreshScreenOffsets();

        void MouseMove(int x, int y);
        void MouseSetLeftDown(bool down);
        void MouseSetRightDown(bool down);
        void MouseSetMiddleDown(bool down);
        void MouseWheelHorizontal(float delta);
        void MouseWheelVertical(float delta);

        void PenMove(int x, int y);
        void PenSetPrimaryDown(bool down);
        void PenSetSecondaryDown(bool down);
        void PenLeave();

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

        static bool IsPenSimulationSupported();
        static bool IsKeyDown(unsigned char keycode);
};

#endif