//This code belongs to the Desktop+ OpenVR overlay application, licensed under GPL 3.0
//
//Desktop+ is heavily based on the DXGI Desktop Duplication sample code by Microsoft: https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/DXGIDesktopDuplication
//Much of the code is simply modified or expanded upon it, so structure and many comments are leftovers from that

#include <limits.h>
#include <fstream>
#include <sstream>
#include <system_error>

#include <userenv.h>

#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"

#include "Util.h"
#include "DisplayManager.h"
#include "DuplicationManager.h"
#include "OutputManager.h"
#include "WindowManager.h"
#include "ThreadManager.h"
#include "InterprocessMessaging.h"
#include "ElevatedMode.h"
#include "Logging.h"

// Below are lists of errors expect from Dxgi API calls when a transition event like mode change, PnpStop, PnpStart
// desktop switch, TDR or session disconnect/reconnect. In all these cases we want the application to clean up the threads that process
// the desktop updates and attempt to recreate them.
// If we get an error that is not on the appropriate list then we exit the application

// These are the errors we expect from general Dxgi API due to a transition
HRESULT SystemTransitionsExpectedErrors[] = {
                                                DXGI_ERROR_DEVICE_REMOVED,
                                                DXGI_ERROR_ACCESS_LOST,
                                                static_cast<HRESULT>(WAIT_ABANDONED),
                                                S_OK                                    // Terminate list with zero valued HRESULT
                                            };

// These are the errors we expect from IDXGIOutput1::DuplicateOutput due to a transition
HRESULT CreateDuplicationExpectedErrors[] = {
                                                DXGI_ERROR_DEVICE_REMOVED,
                                                static_cast<HRESULT>(E_ACCESSDENIED),
                                                DXGI_ERROR_UNSUPPORTED,
                                                DXGI_ERROR_SESSION_DISCONNECTED,
                                                S_OK                                    // Terminate list with zero valued HRESULT
                                            };

// These are the errors we expect from IDXGIOutputDuplication methods due to a transition
HRESULT FrameInfoExpectedErrors[] = {
                                        DXGI_ERROR_DEVICE_REMOVED,
                                        DXGI_ERROR_ACCESS_LOST,
                                        S_OK                                    // Terminate list with zero valued HRESULT
                                    };

// These are the errors we expect from IDXGIAdapter::EnumOutputs methods due to outputs becoming stale during a transition
HRESULT EnumOutputsExpectedErrors[] = {
                                          DXGI_ERROR_NOT_FOUND,
                                          S_OK                                    // Terminate list with zero valued HRESULT
                                      };


//
// Forward Declarations
//
DWORD WINAPI CaptureThreadEntry(_In_ void* Param);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool SpawnProcessWithDefaultEnv(LPCWSTR application_name, LPWSTR commandline = nullptr);
void ProcessCmdline(bool& use_elevated_mode);
bool DisplayInitError(vr::EVRInitError vr_init_error, vr::EVROverlayError vr_overlay_error, bool vr_input_success);

//
// Class for progressive waits
//
typedef struct
{
    UINT    WaitTime;
    UINT    WaitCount;
}WAIT_BAND;

#define WAIT_BAND_COUNT 5
#define WAIT_BAND_STOP 0

class DYNAMIC_WAIT
{
    public :
        DYNAMIC_WAIT();
        ~DYNAMIC_WAIT();

        void Wait();

    private :

    static const WAIT_BAND   m_WaitBands[WAIT_BAND_COUNT];

    // Period in seconds that a new wait call is considered part of the same wait sequence
    static const UINT       m_WaitSequenceTimeInSeconds = 2;

    UINT                    m_CurrentWaitBandIdx;
    UINT                    m_WaitCountInCurrentBand;
    LARGE_INTEGER           m_QPCFrequency;
    LARGE_INTEGER           m_LastWakeUpTime;
    BOOL                    m_QPCValid;
};
const WAIT_BAND DYNAMIC_WAIT::m_WaitBands[WAIT_BAND_COUNT] = {
                                                                 {10, 40},
                                                                 {50, 20},
                                                                 {250, 20},
                                                                 {2000, 60},
                                                                 {5000, WAIT_BAND_STOP}   // Never move past this band
                                                             };

DYNAMIC_WAIT::DYNAMIC_WAIT() : m_CurrentWaitBandIdx(0), m_WaitCountInCurrentBand(0)
{
    m_QPCValid = QueryPerformanceFrequency(&m_QPCFrequency);
    m_LastWakeUpTime.QuadPart = 0L;
}

DYNAMIC_WAIT::~DYNAMIC_WAIT()
{
}

void DYNAMIC_WAIT::Wait()
{
    LARGE_INTEGER CurrentQPC = {0};

    // Is this wait being called with the period that we consider it to be part of the same wait sequence
    QueryPerformanceCounter(&CurrentQPC);
    if (m_QPCValid && (CurrentQPC.QuadPart <= (m_LastWakeUpTime.QuadPart + (m_QPCFrequency.QuadPart * m_WaitSequenceTimeInSeconds))))
    {
        // We are still in the same wait sequence, lets check if we should move to the next band
        if ((m_WaitBands[m_CurrentWaitBandIdx].WaitCount != WAIT_BAND_STOP) && (m_WaitCountInCurrentBand > m_WaitBands[m_CurrentWaitBandIdx].WaitCount))
        {
            m_CurrentWaitBandIdx++;
            m_WaitCountInCurrentBand = 0;
        }
    }
    else
    {
        // Either we could not get the current time or we are starting a new wait sequence
        m_WaitCountInCurrentBand = 0;
        m_CurrentWaitBandIdx = 0;
    }

    // Sleep for the required period of time
    Sleep(m_WaitBands[m_CurrentWaitBandIdx].WaitTime);

    // Record the time we woke up so we can detect wait sequences
    QueryPerformanceCounter(&m_LastWakeUpTime);
    m_WaitCountInCurrentBand++;
}


//
// Program entry point
//
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    bool use_elevated_mode = false;
    ProcessCmdline(use_elevated_mode);

    if (use_elevated_mode)
    {
        //Pass all control to eleveated mode and exit when we're done there
        return ElevatedModeEnter(hInstance);
    }

    DPLog_Init("DesktopPlus");

    INT SingleOutput = 0;

    // Synchronization
    HANDLE UnexpectedErrorEvent   = nullptr;
    HANDLE ExpectedErrorEvent     = nullptr;
    HANDLE NewFrameProcessedEvent = nullptr;
    HANDLE PauseDuplicationEvent  = nullptr;
    HANDLE ResumeDuplicationEvent = nullptr;
    HANDLE TerminateThreadsEvent  = nullptr;

    // Window
    HWND WindowHandle = nullptr;

    //Make sure only one instance is running
    StopProcessByWindowClass(g_WindowClassNameDashboardApp);

    // Event used by the threads to signal an unexpected error and we want to quit the app
    UnexpectedErrorEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!UnexpectedErrorEvent)
    {
        ProcessFailure(nullptr, L"UnexpectedErrorEvent creation failed", L"Desktop+ Error", E_UNEXPECTED);
        return 0;
    }

    // Event for when a thread encounters an expected error
    ExpectedErrorEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ExpectedErrorEvent)
    {
        ProcessFailure(nullptr, L"ExpectedErrorEvent creation failed", L"Desktop+ Error", E_UNEXPECTED);
        return 0;
    }

    // Event for when a thread succeeded in processing a frame
    NewFrameProcessedEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!NewFrameProcessedEvent)
    {
        ProcessFailure(nullptr, L"NewFrameProcessedEvent creation failed", L"Desktop+ Error", E_UNEXPECTED);
        return 0;
    }

    //Event to tell threads to pause duplication (signaled by default)
    PauseDuplicationEvent = ::CreateEvent(nullptr, TRUE, TRUE, nullptr);
    if (!PauseDuplicationEvent)
    {
        ProcessFailure(nullptr, L"PauseDuplicationEvent creation failed", L"Desktop+ Error", E_UNEXPECTED);
        return 0;
    }

    //Event to tell threads to resume duplication
    ResumeDuplicationEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ResumeDuplicationEvent)
    {
        ProcessFailure(nullptr, L"ResumeDuplicationEvent creation failed", L"Desktop+ Error", E_UNEXPECTED);
        return 0;
    }

    // Event to tell spawned threads to quit
    TerminateThreadsEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!TerminateThreadsEvent)
    {
        ProcessFailure(nullptr, L"TerminateThreadsEvent creation failed", L"Desktop+ Error", E_UNEXPECTED);
        return 0;
    }

    // Register class
    WNDCLASSEXW Wc;
    Wc.cbSize           = sizeof(WNDCLASSEXW);
    Wc.style            = 0;
    Wc.lpfnWndProc      = WndProc;
    Wc.cbClsExtra       = 0;
    Wc.cbWndExtra       = 0;
    Wc.hInstance        = hInstance;
    Wc.hIcon            = nullptr;
    Wc.hCursor          = nullptr;
    Wc.hbrBackground    = nullptr;
    Wc.lpszMenuName     = nullptr;
    Wc.lpszClassName    = g_WindowClassNameDashboardApp;
    Wc.hIconSm          = nullptr;
    if (!RegisterClassExW(&Wc))
    {
        ProcessFailure(nullptr, L"Window class registration failed", L"Desktop+ Error", E_UNEXPECTED);
        return 0;
    }

    // Create window
    WindowHandle = ::CreateWindowW(g_WindowClassNameDashboardApp, L"Desktop+ Overlay",
                                   0,
                                   0, 0,
                                   1, 1,
                                   HWND_DESKTOP, nullptr, hInstance, nullptr);
    if (!WindowHandle)
    {
        ProcessFailure(nullptr, L"Window creation failed", L"Desktop+ Error", E_FAIL);
        return 0;
    }

    //Init WinRT DLL
    DPWinRT_Init();
    DPLog_DPWinRT_SupportInfo();
    LOG_F(INFO, "Loaded WinRT library");

    //Init BrowserClientAPI (this doesn't start the browser process, only checks for presence)
    DPBrowserAPIClient::Get().Init();

    //Allow IPC messages even when elevated
    IPCManager::Get().DisableUIPForRegisteredMessages(WindowHandle);

    THREADMANAGER ThreadMgr;
    OutputManager OutMgr(PauseDuplicationEvent, ResumeDuplicationEvent);
    RECT DeskBounds;
    UINT OutputCount;

    //Start up UI process unless disabled or already running
    if ( (!ConfigManager::GetValue(configid_bool_interface_no_ui)) && (!IPCManager::IsUIAppRunning()) )
    {
        std::wstring path = WStringConvertFromUTF8(ConfigManager::Get().GetApplicationPath().c_str()) + L"DesktopPlusUI.exe";
        SpawnProcessWithDefaultEnv(path.c_str());

        LOG_F(INFO, "Launched Desktop+ UI process");
    }

    //Message loop
    MSG msg = {0};
    DUPL_RETURN Ret = DUPL_RETURN_SUCCESS;
    DUPL_RETURN_UPD RetUpdate = DUPL_RETURN_UPD_SUCCESS;
    bool FirstTime = true;

    DYNAMIC_WAIT DynamicWait;

    LARGE_INTEGER UpdateLimiterStartingTime, UpdateLimiterEndingTime, UpdateLimiterElapsedMicroseconds;
    LARGE_INTEGER UpdateLimiterFrequency;

    bool IsNewFrame = false;
    bool SkipFrame = false;

    ::QueryPerformanceFrequency(&UpdateLimiterFrequency);
    ::QueryPerformanceCounter(&UpdateLimiterStartingTime);

    while (WM_QUIT != msg.message)
    {
        if ((!FirstTime) && (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)))  //Wait for init before processing messages
        {
            // Process window messages
            if (msg.message >= 0xC000)  //Custom message from UI process, handle in output manager (WM_COPYDATA is handled in WndProc())
            {
                if (OutMgr.HandleIPCMessage(msg))
                {
                    SetEvent(ExpectedErrorEvent);
                }
            }
            else if (msg.message >= WM_DPLUSWINRT) //WinRT library messages
            {
                OutMgr.HandleWinRTMessage(msg);
            }
            else if (msg.message == WM_HOTKEY)
            {
                OutMgr.HandleHotkeyMessage(msg);
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else if (WaitForSingleObjectEx(UnexpectedErrorEvent, 0, FALSE) == WAIT_OBJECT_0)
        {
            // Unexpected error occurred so exit the application
            break;
        }
        else if ((FirstTime) || (WaitForSingleObjectEx(ExpectedErrorEvent, 0, FALSE) == WAIT_OBJECT_0))
        {
            if (!FirstTime)
            {
                LOG_F(INFO, "System transition occured, reinitializing...");

                // Terminate other threads
                ResetEvent(PauseDuplicationEvent);
                SetEvent(ResumeDuplicationEvent);
                SetEvent(TerminateThreadsEvent);
                ThreadMgr.WaitForThreadTermination();
                ResetEvent(TerminateThreadsEvent);
                ResetEvent(ExpectedErrorEvent);
                ResetEvent(NewFrameProcessedEvent);
                ResetEvent(ResumeDuplicationEvent);

                // Clean up
                ThreadMgr.Clean();
                OutMgr.CleanRefs();

                // As we have encountered an error due to a system transition we wait before trying again, using this dynamic wait
                // the wait periods will get progressively long to avoid wasting too much system resource if this state lasts a long time
                DynamicWait.Wait();
            }

            // Re-initialize
            vr::EVRInitError vr_init_error = vr::VRInitError_None;
            if (FirstTime) //InitOutput() needs OpenVR to be initialized already
            {
                auto init_error = OutMgr.InitOverlay();

                //Display error message if init error states indicates one
                if (DisplayInitError(std::get<vr::EVRInitError>(init_error), std::get<vr::EVROverlayError>(init_error), std::get<bool>(init_error)))
                {
                    //An error message was displayed, abort
                    Ret = DUPL_RETURN_ERROR_UNEXPECTED;
                    break;
                }

                if ( (ConfigManager::GetValue(configid_bool_misc_no_steam)) && (ConfigManager::GetValue(configid_bool_state_misc_process_started_by_steam)) )
                {
                    //Was started by Steam but running through it was turned off, so restart without
                    std::wstring path = WStringConvertFromUTF8(ConfigManager::Get().GetApplicationPath().c_str()) + L"DesktopPlus.exe";
                    SpawnProcessWithDefaultEnv(path.c_str());

                    break;
                }
            }

            Ret = OutMgr.InitOutput(WindowHandle, SingleOutput, &OutputCount, &DeskBounds);
            if (Ret == DUPL_RETURN_SUCCESS)
            {
                HANDLE SharedHandle = OutMgr.GetSharedHandle();
                if (SharedHandle)
                {
                    Ret = ThreadMgr.Initialize(SingleOutput, OutputCount, UnexpectedErrorEvent, ExpectedErrorEvent, NewFrameProcessedEvent, PauseDuplicationEvent,
                                               ResumeDuplicationEvent, TerminateThreadsEvent, SharedHandle, &DeskBounds, OutMgr.GetDXGIAdapter(), 
                                               (ConfigManager::GetValue(configid_int_interface_wmr_ignore_vscreens) == 1));
                }
                else
                {
                    DisplayMsg(L"Failed to get handle of shared surface", L"Desktop+ Error", E_FAIL);

                    Ret = DUPL_RETURN_ERROR_UNEXPECTED;
                }

                if (FirstTime)
                {
                    // First time through the loop
                    FirstTime = false;
                }
                else
                {
                    ForceScreenRefresh();
                }
            }
            else if (Ret == DUPL_RETURN_ERROR_EXPECTED)
            {
                if (OutputCount == 0) //No outputs right now, oops
                {
                    OutMgr.SetOutputInvalid();
                    Ret = DUPL_RETURN_SUCCESS; //Entered "valid" state now, prevent auto-retry to needlessly kick in
                }

                FirstTime = false;
            }

        }
        else //Present frame or handle events as fast as needed
        {
            if (WaitForSingleObjectEx(NewFrameProcessedEvent, OutMgr.GetMaxRefreshDelay(), FALSE) == WAIT_OBJECT_0)   //New frame
            {
                ResetEvent(NewFrameProcessedEvent);
                IsNewFrame = true;
            }
            else
            {
                IsNewFrame = (RetUpdate == DUPL_RETURN_UPD_RETRY); //Retry is treated as if it's new frame, otherwise false
            }

            //Update limiter/skipper
            const LARGE_INTEGER& limiter_delay = OutMgr.GetUpdateLimiterDelay();
            bool update_limiter_active = (limiter_delay.QuadPart != 0);
            if (update_limiter_active)
            {
                QueryPerformanceCounter(&UpdateLimiterEndingTime);
                UpdateLimiterElapsedMicroseconds.QuadPart = UpdateLimiterEndingTime.QuadPart - UpdateLimiterStartingTime.QuadPart;

                UpdateLimiterElapsedMicroseconds.QuadPart *= 1000000;
                UpdateLimiterElapsedMicroseconds.QuadPart /= UpdateLimiterFrequency.QuadPart;

                SkipFrame = (UpdateLimiterElapsedMicroseconds.QuadPart < limiter_delay.QuadPart);
            }
            else
            {
                SkipFrame = false;
            }

            RetUpdate = OutMgr.Update(ThreadMgr.GetPointerInfo(), ThreadMgr.GetDirtyRegionTotal(), IsNewFrame, SkipFrame);

            //Map return value to DUPL_RETRUN Ret
            switch (RetUpdate)
            {
                case DUPL_RETURN_UPD_QUIT:                      Ret = DUPL_RETURN_ERROR_UNEXPECTED; break;
                case DUPL_RETURN_UPD_RETRY:                     Ret = DUPL_RETURN_SUCCESS;          break;
                case DUPL_RETURN_UPD_SUCCESS_REFRESHED_OVERLAY: Ret = DUPL_RETURN_SUCCESS;          break;
                default:                                        Ret = (DUPL_RETURN)RetUpdate;
            }

            if ( (RetUpdate == DUPL_RETURN_UPD_SUCCESS_REFRESHED_OVERLAY) && (update_limiter_active) )
            {
                QueryPerformanceCounter(&UpdateLimiterStartingTime);
            }

            OutMgr.UpdatePerformanceStates();
        }

        // Check if for errors
        if (Ret != DUPL_RETURN_SUCCESS)
        {
            if (Ret == DUPL_RETURN_ERROR_EXPECTED)
            {
                // Some type of system transition is occurring so retry
                SetEvent(ExpectedErrorEvent);
            }
            else
            {
                // Unexpected error or exit event, so exit
                break;
            }
        }
    }

    LOG_F(INFO, "Shutting down...");

    //Remove all overlays since they may access things on destruction after we're shut down otherwise
    OverlayManager::Get().RemoveAllOverlays();

    //Quit Browser processes if they're running (we do this early since the browser doesn't poll for VREvent_Quit itself)
    DPBrowserAPIClient::Get().Quit();

    // Make sure all other threads have exited
    if (SetEvent(TerminateThreadsEvent))
    {
        //Wake them up first if needed
        ResetEvent(PauseDuplicationEvent);
        SetEvent(ResumeDuplicationEvent);

        ThreadMgr.WaitForThreadTermination();
    }

    // Clean up
    CloseHandle(UnexpectedErrorEvent);
    CloseHandle(ExpectedErrorEvent);
    CloseHandle(NewFrameProcessedEvent);
    CloseHandle(PauseDuplicationEvent);
    CloseHandle(ResumeDuplicationEvent);
    CloseHandle(TerminateThreadsEvent);

    //Tell WindowManager to exit if it's still active
    WindowManager::Get().ClearTempTopMostWindow();
    WindowManager::Get().SetActive(false);

    //Do other shutdown steps, like undimming the dashboard if needed
    OutMgr.OnExit();

    //Kindly ask elevated mode process to quit if it exists
    if (HWND window = ::FindWindow(g_WindowClassNameElevatedMode, nullptr))
    {
        ::PostMessage(window, WM_QUIT, 0, 0);
    }

    if (msg.message == WM_QUIT)
    {
        // For a WM_QUIT message we should return the wParam value
        return static_cast<INT>(msg.wParam);
    }

    return 0;
}

//
// Window message processor
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_COPYDATA:
        {
            //Forward to output manager
            if (OutputManager::Get())
            {
                bool mirror_reset_required = false;

                MSG msg;
                // Process all custom window messages posted before this
                while (PeekMessage(&msg, nullptr, 0xC000, 0xFFFF, PM_REMOVE))
                {
                    //Skip mirror reset messages here to prevent an endless loop
                    if ((IPCManager::Get().GetIPCMessageID(msg.message) == ipcmsg_action) && (msg.wParam == ipcact_mirror_reset))
                    {
                        continue;
                    }

                    if (OutputManager::Get()->HandleIPCMessage(msg))
                    {
                        mirror_reset_required = true;
                    }
                }

                
                msg.hwnd = hWnd;
                msg.message = message;
                msg.wParam = wParam;
                msg.lParam = lParam;

                if (OutputManager::Get()->HandleIPCMessage(msg))
                {
                    mirror_reset_required = true;
                }

                if (mirror_reset_required)
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_mirror_reset);
                }
            }
            break;
        }
        case WM_DISPLAYCHANGE:
        {
            //Update desktop count and rects
            if (OutputManager::Get())
            {
                LOG_F(INFO, "WM_DISPLAYCHANGE recieved, re-enumerating outputs...");
                OutputManager::Get()->EnumerateOutputs();
            }
            break;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

bool SpawnProcessWithDefaultEnv(LPCWSTR application_name, LPWSTR commandline)
{
    LPVOID env = nullptr;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    //Use a new environment block since we don't want to copy the Steam environment variables if there are any
    if (::CreateEnvironmentBlock(&env, ::GetCurrentProcessToken(), FALSE))
    {
        bool ret = ::CreateProcess(application_name, commandline, nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT, env, nullptr, &si, &pi);

        //We don't care about these, so close right away
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);

        ::DestroyEnvironmentBlock(env);

        return ret;
    }

    return false;
}

void ProcessCmdline(bool& use_elevated_mode)
{
    //__argv and __argc are global vars set by system
    for (UINT i = 0; i < static_cast<UINT>(__argc); ++i)
    {
        if ((strcmp(__argv[i], "-ElevatedMode") == 0) ||
            (strcmp(__argv[i], "/ElevatedMode") == 0))
        {
            use_elevated_mode = true;
        }
    }
}

bool DisplayInitError(vr::EVRInitError vr_init_error, vr::EVROverlayError vr_overlay_error, bool vr_input_success)
{
    if (vr_init_error != vr::VRInitError_None)
    {
        if ( (vr_init_error == vr::VRInitError_Init_HmdNotFound) || (vr_init_error == vr::VRInitError_Init_HmdNotFoundPresenceFailed) )
        {
            DisplayMsg(L"Failed to init OpenVR: HMD not found.\nRe-launch application with a HMD connected.", L"Desktop+ Error", E_FAIL);
        }
        else if (vr_init_error == vr::VRInitError_Init_InvalidInterface)
        {
            DisplayMsg(L"Failed to init OpenVR: Invalid Interface.\nMake sure to have the latest version of SteamVR installed.", L"Desktop+ Error", E_FAIL);
        }
        else if (vr_init_error != vr::VRInitError_Init_InitCanceledByUser) //Exclude canceled, supposed to be always silent exit
        {
            std::wstring error_str = L"Failed to init OpenVR: ";
            error_str += WStringConvertFromUTF8(vr::VR_GetVRInitErrorAsEnglishDescription(vr_init_error));
            error_str += L".";

            DisplayMsg(error_str.c_str(), L"Desktop+ Error", E_FAIL);
        }

        return true;
    }

    if (vr_overlay_error != vr::VROverlayError_None)
    {
        std::wstring error_str = L"Failed to init overlay: ";
        error_str += WStringConvertFromUTF8(vr::VROverlay()->GetOverlayErrorNameFromEnum(vr_overlay_error));
        error_str += L".";

        DisplayMsg(error_str.c_str(), L"Desktop+ Error", E_FAIL);

        return true;
    }

    if (!vr_input_success)
    {
        //VRInput not working is bad, but doesn't stop us from running, so just log it
        LOG_F(WARNING, "Failed to load VRInput action manifest. Some input-related functionality will not be available");
    }

    return false;
}

//
// Entry point for new duplication threads
//
DWORD WINAPI CaptureThreadEntry(_In_ void* Param)
{
    // Classes
    DISPLAYMANAGER DispMgr;
    DUPLICATIONMANAGER DuplMgr;

    // D3D objects
    ID3D11Texture2D* SharedSurf = nullptr;
    IDXGIKeyedMutex* KeyMutex = nullptr;

    // Data passed in from thread creation
    THREAD_DATA* TData = reinterpret_cast<THREAD_DATA*>(Param);

    // Get desktop
    DUPL_RETURN Ret;
    HDESK CurrentDesktop = nullptr;
    CurrentDesktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
    if (!CurrentDesktop)
    {
        // We do not have access to the desktop so request a retry
        SetEvent(TData->ExpectedErrorEvent);
        Ret = DUPL_RETURN_ERROR_EXPECTED;
        goto Exit;
    }

    // Attach desktop to this thread
    bool DesktopAttached = SetThreadDesktop(CurrentDesktop) != 0;
    CloseDesktop(CurrentDesktop);
    CurrentDesktop = nullptr;
    if (!DesktopAttached)
    {
        // We do not have access to the desktop so request a retry
        Ret = DUPL_RETURN_ERROR_EXPECTED;
        goto Exit;
    }

    // New display manager
    DispMgr.InitD3D(&TData->DxRes);

    // Obtain handle to sync shared Surface
    HRESULT hr = TData->DxRes.Device->OpenSharedResource(TData->TexSharedHandle, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&SharedSurf));
    if (FAILED (hr))
    {
        Ret = ProcessFailure(TData->DxRes.Device, L"Opening shared texture failed", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
        goto Exit;
    }

    hr = SharedSurf->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&KeyMutex));
    if (FAILED(hr))
    {
        Ret = ProcessFailure(nullptr, L"Failed to get keyed mutex interface in spawned thread", L"Desktop+ Error", hr);
        goto Exit;
    }

    // Make duplication manager
    Ret = DuplMgr.InitDupl(TData->DxRes.Device, TData->Output, TData->WMRIgnoreVScreens);
    if (Ret != DUPL_RETURN_SUCCESS)
    {
        goto Exit;
    }

    // Get output description
    DXGI_OUTPUT_DESC DesktopDesc;
    RtlZeroMemory(&DesktopDesc, sizeof(DXGI_OUTPUT_DESC));
    DuplMgr.GetOutputDesc(&DesktopDesc);

    // Main duplication loop
    bool WaitToProcessCurrentFrame = false;
    FRAME_DATA CurrentData;

    while ((WaitForSingleObjectEx(TData->TerminateThreadsEvent, 0, FALSE) == WAIT_TIMEOUT))
    {
        //Wait if pause event was signaled
        if ((WaitForSingleObjectEx(TData->PauseDuplicationEvent, 0, FALSE) == WAIT_OBJECT_0))
        {
            WaitForSingleObjectEx(TData->ResumeDuplicationEvent, INFINITE, FALSE); //Wait forever. Thread shutdown will also signal resume
        }

        if (!WaitToProcessCurrentFrame)
        {
            // Get new frame from desktop duplication
            bool TimeOut;
            Ret = DuplMgr.GetFrame(&CurrentData, &TimeOut);
            if (Ret != DUPL_RETURN_SUCCESS)
            {
                // An error occurred getting the next frame drop out of loop which
                // will check if it was expected or not
                break;
            }

            // Check for timeout
            if (TimeOut)
            {
                // No new frame at the moment
                continue;
            }
        }

        // We have a new frame so try and process it
        // Try to acquire keyed mutex in order to access shared surface
        hr = KeyMutex->AcquireSync(0, 1000);
        if (hr == static_cast<HRESULT>(WAIT_TIMEOUT))
        {
            // Can't use shared surface right now, try again later
            WaitToProcessCurrentFrame = true;
            continue;
        }
        else if (FAILED(hr))
        {
            // Generic unknown failure
            Ret = ProcessFailure(TData->DxRes.Device, L"Unexpected error acquiring keyed mutex", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
            DuplMgr.DoneWithFrame();
            break;
        }

        // We can now process the current frame
        WaitToProcessCurrentFrame = false;

        // Get mouse info
        Ret = DuplMgr.GetMouse(TData->PtrInfo, &(CurrentData.FrameInfo), TData->OffsetX, TData->OffsetY);
        if (Ret != DUPL_RETURN_SUCCESS)
        {
            DuplMgr.DoneWithFrame();
            KeyMutex->ReleaseSync(1);
            break;
        }

        // Process new frame
        Ret = DispMgr.ProcessFrame(&CurrentData, SharedSurf, TData->OffsetX, TData->OffsetY, &DesktopDesc, *TData->DirtyRegionTotal);
        if (Ret != DUPL_RETURN_SUCCESS)
        {
            DuplMgr.DoneWithFrame();
            KeyMutex->ReleaseSync(1);
            SetEvent(TData->NewFrameProcessedEvent);
            break;
        }

        // Release acquired keyed mutex
        hr = KeyMutex->ReleaseSync(1);
        if (FAILED(hr))
        {
            Ret = ProcessFailure(TData->DxRes.Device, L"Unexpected error releasing the keyed mutex", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
            DuplMgr.DoneWithFrame();
            break;
        }

        // Release frame back to desktop duplication
        Ret = DuplMgr.DoneWithFrame();
        if (Ret != DUPL_RETURN_SUCCESS)
        {
            break;
        }

        SetEvent(TData->NewFrameProcessedEvent);
    }

Exit:
    if (Ret != DUPL_RETURN_SUCCESS)
    {
        if (Ret == DUPL_RETURN_ERROR_EXPECTED)
        {
            // The system is in a transition state so request the duplication be restarted
            SetEvent(TData->ExpectedErrorEvent);
        }
        else
        {
            // Unexpected error so exit the application
            SetEvent(TData->UnexpectedErrorEvent);
        }
    }

    if (SharedSurf)
    {
        SharedSurf->Release();
        SharedSurf = nullptr;
    }

    if (KeyMutex)
    {
        KeyMutex->Release();
        KeyMutex = nullptr;
    }

    return 0;
}

_Post_satisfies_(return != DUPL_RETURN_SUCCESS)
DUPL_RETURN ProcessFailure(_In_opt_ ID3D11Device* device, _In_ LPCWSTR str, _In_ LPCWSTR title, HRESULT hr, _In_opt_z_ HRESULT* expected_errors)
{
    HRESULT translated_hr;

    // On an error check if the DX device is lost
    if (device)
    {
        HRESULT device_removed_reason = device->GetDeviceRemovedReason();

        switch (device_removed_reason)
        {
            case DXGI_ERROR_DEVICE_REMOVED:
            case DXGI_ERROR_DEVICE_RESET:
            case static_cast<HRESULT>(E_OUTOFMEMORY):
            {
                // Our device has been stopped due to an external event on the GPU so map them all to
                // device removed and continue processing the condition
                translated_hr = DXGI_ERROR_DEVICE_REMOVED;
                break;
            }
            case S_OK:
            {
                // Device is not removed so use original error
                translated_hr = hr;
                break;
            }
            default:
            {
                // Device is removed but not a error we want to remap
                translated_hr = device_removed_reason;
            }
        }
    }
    else
    {
        translated_hr = hr;
    }

    // Check if this error was expected or not
    if (expected_errors)
    {
        HRESULT* current_result = expected_errors;

        while (*current_result != S_OK)
        {
            if (*(current_result++) == translated_hr)
            {
                return DUPL_RETURN_ERROR_EXPECTED;
            }
        }
    }

    // Error was not expected so display the message box
    DisplayMsg(str, title, translated_hr);

    return DUPL_RETURN_ERROR_UNEXPECTED;
}

//
// Displays a message
//
void DisplayMsg(_In_ LPCWSTR str, _In_ LPCWSTR title, HRESULT hr)
{
    //Generate a proper error message with description from the OS, unless it's E_FAIL which is unspecified anyways and we use it for our own errors passed to this function
    std::wstringstream ss;

    if (hr != E_FAIL)
    {
        std::error_code ec(hr, std::system_category());
        ss << str << L":\n" << WStringConvertFromLocalEncoding(ec.message().c_str()) << L" (0x" << std::hex << std::setfill(L'0') << std::setw(8) << hr << L")";
    }
    else
    {
        ss << str;
    }

    std::string str_u8 = StringConvertFromUTF16(ss.str().c_str());

    //Try having the UI app display it if it's an error
    if (!SUCCEEDED(hr))
    {
        HWND window = (OutputManager::Get() != nullptr) ? OutputManager::Get()->GetWindowHandle() : nullptr;
        IPCManager::Get().SendStringToUIApp(configid_str_state_dashboard_error_string, str_u8.c_str(), window);
    }

    //Get rid of newlines before logging (system error strings comes with CRLF while \n is treated as LF-only before writing to file...)
    StringReplaceAll(str_u8, "\r\n", " ");
    StringReplaceAll(str_u8, "\n",   " ");

    VLOG_F((SUCCEEDED(hr)) ? loguru::Verbosity_INFO : loguru::Verbosity_ERROR, str_u8.c_str());

    //While we always try to send error messages to the UI app to have it display in VR, show the message on the desktop if UI is not running or VR isn't loaded
    if ((!IPCManager::IsUIAppRunning()) || (vr::VROverlay() == nullptr))
    {
        ::MessageBoxW(nullptr, str, title, MB_OK);
    }
}
