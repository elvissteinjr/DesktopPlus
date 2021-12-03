#include "ElevatedMode.h"

#include <windowsx.h>

#include "InterprocessMessaging.h"
#include "InputSimulator.h"
#include "Util.h"

static bool g_ElevatedMode_ComInitDone = false;

LRESULT CALLBACK WndProcElevated(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool HandleIPCMessage(MSG msg);

int ElevatedModeEnter(HINSTANCE hinstance)
{
    //Don't run if the dashboard app is not running or process isn't even elevated
    if ( (!IPCManager::Get().IsDashboardAppRunning()) || (!IsProcessElevated()) )
        return E_NOT_VALID_STATE;

    //Register class
    WNDCLASSEXW wc;
    wc.cbSize           = sizeof(WNDCLASSEXW);
    wc.style            = 0;
    wc.lpfnWndProc      = WndProcElevated;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = hinstance;
    wc.hIcon            = nullptr;
    wc.hCursor          = nullptr;
    wc.hbrBackground    = nullptr;
    wc.lpszMenuName     = nullptr;
    wc.lpszClassName    = g_WindowClassNameElevatedMode;
    wc.hIconSm          = nullptr;

    if (!::RegisterClassExW(&wc))
    {
        return E_FAIL;
    }

    //Create window
    HWND window_handle = ::CreateWindowW(g_WindowClassNameElevatedMode, L"Desktop+ Elevated",
                                         0,
                                         0, 0,
                                         1, 1,
                                         HWND_MESSAGE, nullptr, hinstance, nullptr);
    if (!window_handle)
    {
        return E_FAIL;
    }

    //Allow IPC messages even when elevated
    IPCManager::Get().DisableUIPForRegisteredMessages(window_handle);

    //Send config update to dashboard and UI process to set elevated mode active
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_state_misc_elevated_mode_active, true);
    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_misc_elevated_mode_active, true);

    //Wait for callbacks, update or quit message
    MSG msg;
    while (::GetMessage(&msg, 0, 0, 0))
    {
        //Custom IPC messages
        if (msg.message >= 0xC000)
        {
            HandleIPCMessage(msg);
        }
    }

    //Send config update to dashboard and UI process to disable it again
    IPCManager::Get().PostConfigMessageToDashboardApp(configid_bool_state_misc_elevated_mode_active, false);
    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_misc_elevated_mode_active, false);

    //Uninitialize COM if it was used
    if (!g_ElevatedMode_ComInitDone)
    {
        ::CoUninitialize();
        g_ElevatedMode_ComInitDone = false;
    }

    return 0;
}

LRESULT CALLBACK WndProcElevated(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_COPYDATA:
        {
            MSG msg;
            // Process all custom window messages posted before this
            while (::PeekMessage(&msg, nullptr, 0xC000, 0xFFFF, PM_REMOVE))
            {
                HandleIPCMessage(msg);
            }

            msg.hwnd = hWnd;
            msg.message = message;
            msg.wParam = wParam;
            msg.lParam = lParam;

            HandleIPCMessage(msg);
            break;
        }
        case WM_DESTROY:
        {
            ::PostQuitMessage(0);
            break;
        }
        default:
            return ::DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

bool HandleIPCMessage(MSG msg)
{
    static InputSimulator input_sim;

    static std::string action_exe_path;
    static std::string action_exe_arg;

    //Input strings come as WM_COPYDATA
    if (msg.message == WM_COPYDATA)
    {
        //At least check if the dashboard app is running and the wParam is its window
        HWND dashboard_app = ::FindWindow(g_WindowClassNameDashboardApp, nullptr);
        if ((dashboard_app == nullptr) || ((HWND)msg.wParam != dashboard_app))
        {
            return false;
        }

        COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)msg.lParam;

        //Arbitrary size limit to prevent some malicous applications from sending bad data
        if ( (pcds->cbData > 0) && (pcds->cbData <= 4096) ) 
        {
            std::string copystr((char*)pcds->lpData, pcds->cbData); //We rely on the data length. The data is sent without the NUL byte

            switch (pcds->dwData)
            {
                case ipcestrid_keyboard_text:
                case ipcestrid_keyboard_text_force_unicode:
                {
                    //Pass string to InputSimulator
                    input_sim.KeyboardText(copystr.c_str(), (pcds->dwData == ipcestrid_keyboard_text_force_unicode) );
                    break;
                }
                case ipcestrid_launch_application_path:
                {
                    action_exe_path = copystr;
                    action_exe_arg  = "";       //Also reset arg str since it's often not needed
                    break;
                }
                case ipcestrid_launch_application_arg:
                {
                    action_exe_arg = copystr;
                    break;
                }
            }
            
        }
    }
    else
    {
        IPCMsgID msgid = IPCManager::Get().GetIPCMessageID(msg.message);

        if (msgid == ipcmsg_elevated_action)
        {
            switch (msg.wParam)
            {
                case ipceact_refresh:
                {
                    input_sim.RefreshScreenOffsets();
                    break;
                }
                case ipceact_mouse_move:
                {
                    input_sim.MouseMove(GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam));
                    break;
                };
                case ipceact_mouse_hwheel:
                {
                    input_sim.MouseWheelHorizontal(pun_cast<float, LPARAM>(msg.lParam));
                    break;
                };
                case ipceact_mouse_vwheel:
                {
                    input_sim.MouseWheelVertical(pun_cast<float, LPARAM>(msg.lParam));
                    break;
                };
                case ipceact_key_down:
                {
                    //Copy 3 keycodes from lparam
                    unsigned char keycodes[3];
                    memcpy(keycodes, &msg.lParam, 3);

                    input_sim.KeyboardSetDown(keycodes);
                    break;
                };
                case ipceact_key_up:
                {
                    //Copy 3 keycodes from lparam
                    unsigned char keycodes[3];
                    memcpy(keycodes, &msg.lParam, 3);

                    input_sim.KeyboardSetUp(keycodes);
                    break;
                };
                case ipceact_key_toggle:
                {
                    //Copy 3 keycodes from lparam
                    unsigned char keycodes[3];
                    memcpy(keycodes, &msg.lParam, 3);

                    input_sim.KeyboardToggleState(keycodes);
                    break;
                };
                case ipceact_key_press_and_release:
                {
                    input_sim.KeyboardPressAndRelease(msg.lParam);
                    break;
                };
                case ipceact_key_togglekey_set:
                {
                    input_sim.KeyboardSetToggleKey(LOWORD(msg.lParam), HIWORD(msg.lParam));
                    break;
                }
                case ipceact_keystate_w32_set:
                {
                    input_sim.KeyboardSetFromWin32KeyState(LOWORD(msg.lParam), HIWORD(msg.lParam));
                    break;
                }
                case ipceact_keystate_set:
                {
                    input_sim.KeyboardSetKeyState((IPCKeyboardKeystateFlags)LOWORD(msg.lParam), HIWORD(msg.lParam));
                    break;
                }
                case ipceact_keyboard_text_finish:
                {
                    input_sim.KeyboardTextFinish();
                    break;
                }
                case ipceact_launch_application:
                {
                    //Init COM if necessary
                    if (!g_ElevatedMode_ComInitDone)
                    {
                        if (::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) != RPC_E_CHANGED_MODE)
                        {
                            g_ElevatedMode_ComInitDone = true;
                        }
                    }

                    //Convert path and arg to utf16
                    std::wstring path_wstr = WStringConvertFromUTF8(action_exe_path.c_str());
                    std::wstring arg_wstr  = WStringConvertFromUTF8(action_exe_arg.c_str());

                    if (!path_wstr.empty())
                    {   
                        ::ShellExecute(nullptr, nullptr, path_wstr.c_str(), arg_wstr.c_str(), nullptr, SW_SHOWNORMAL);
                    }
                    break;
                }
            }
        }
    }

    return true;
}
