#include "DesktopPlusWinRT.h"

#ifndef DPLUSWINRT_STUB

#pragma comment (lib, "windowsapp.lib")

#include <mutex>
#include <utility>
#include <limits.h>

#include "CommonHeaders.h"
#include "CaptureManager.h"

#include "ThreadData.h"

#include "Util.h"

#include "Util/hwnd.interop.h"

//Globals
//- Not modified after DPWinRT_Init()
static DWORD g_MainThreadID;
static bool g_IsCaptureSupported;
static int  g_APIContractPresent;

//- Protected by g_ThreadsMutex
static std::mutex g_ThreadsMutex;
static std::vector<DPWinRTThreadData> g_Threads;

//- Only accessed by main thread
static bool g_IsCursorEnabled;

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Metadata;
    using namespace Windows::Graphics::Capture;
}

namespace util
{
    using namespace desktop;
}

#endif //DPLUSWINRT_STUB

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

#ifndef DPLUSWINRT_STUB

DWORD WINAPI WinRTCaptureThreadEntry(_In_ void* Param);

bool DPWinRT_Internal_StartCapture(vr::VROverlayHandle_t overlay_handle, const DPWinRTThreadData& data)
{
    //Make sure this overlay handle is not already used by a thread
    DPWinRT_StopCapture(overlay_handle);

    HANDLE thread_handle = nullptr;
    DWORD thread_id = 0;

    {
        std::lock_guard<std::mutex> lock(g_ThreadsMutex);

        DPWinRTOverlayData overlay_data;
        overlay_data.Handle = overlay_handle;

        //Try to find a thread already capturing this item
        for (auto& thread : g_Threads)
        {
            if ((thread.DesktopID == data.DesktopID) && (thread.SourceWindow == data.SourceWindow))
            {
                thread.Overlays.push_back(overlay_data);

                ::PostThreadMessage(thread.ThreadID, WM_DPLUSWINRT_UPDATE_DATA, 0, 0);
                return true;
            }
        }

        //Create new thread if no existing one was found
        g_Threads.push_back(data);
        g_Threads.back().Overlays.push_back(overlay_data);
        g_Threads.back().IsCursorEnabledInitial = g_IsCursorEnabled;

        //Thread may run before CreateThread() returns, but the thread will have to wait on the mutex so it's fine
        thread_handle = ::CreateThread(nullptr, 0, WinRTCaptureThreadEntry, &g_Threads.back(), 0, &thread_id);

        g_Threads.back().ThreadHandle = thread_handle;
        g_Threads.back().ThreadID     = thread_id;
    }

    if (thread_handle == nullptr)
    {
        return false;
    }

    //Wait for the thread's message queue to be ready to not risk any messages getting lost
    BOOL is_ready = false;

    do
    {
        is_ready = ::PostThreadMessage(thread_id, WM_NULL, 0, 0);
        ::Sleep(10);
    }
    while (is_ready == 0);

    return true;
}

#endif //DPLUSWINRT_STUB

void DPWinRT_Init()
{
    #ifndef DPLUSWINRT_STUB

    g_MainThreadID = ::GetCurrentThreadId();
    //Reserve thread data vector size to avoid potential race condition between resize and thread creation when many overlays are created back-to-back (while also avoiding explicit syncing there).
    //100 should be pretty safe as a limit, considering k_unMaxOverlayCount (128) with system overlays and Desktop+ UI overlays... also 100 captures are insane either way
    g_Threads.reserve(100);

    //Init results of capability query functions so we don't need an apartment on the main thread
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    {
        //Try getting feature support information. This pretty much only throws when on Windows 8/8.1, where the APIs are simply not present yet. 
        //We still want to support running on there, though
        try
        {
            g_IsCaptureSupported = winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();

            //This only checks for contract levels we care about
            if (winrt::ApiInformation::IsApiContractPresent(L"Windows.Foundation.UniversalApiContract", 9))
                g_APIContractPresent = 9;
            else if (winrt::ApiInformation::IsApiContractPresent(L"Windows.Foundation.UniversalApiContract", 8))
                g_APIContractPresent = 8;
            else
                g_APIContractPresent = 1;
        }
        catch (const winrt::hresult_error&)
        {
            //We can do nothing, but at least we didn't crash
            g_IsCaptureSupported = false;
            g_APIContractPresent = 0;
        }
    }
    winrt::clear_factory_cache();
    winrt::uninit_apartment();

    g_IsCursorEnabled = true;

    #endif
}

bool DPWinRT_IsCaptureSupported()
{
    #ifndef DPLUSWINRT_STUB
        return ((g_IsCaptureSupported) && (DPWinRT_IsCaptureFromHandleSupported()));
    #else
        return false;
    #endif
}

bool DPWinRT_IsCaptureFromHandleSupported()
{
    #ifndef DPLUSWINRT_STUB
        return (g_APIContractPresent >= 8);
    #else
        return false;
    #endif
}

bool DPWinRT_IsCaptureFromCombinedDesktopSupported()
{
    #ifndef DPLUSWINRT_STUB
        return (g_APIContractPresent >= 9);
    #else
        return false;
    #endif
}

bool DPWinRT_IsCaptureCursorEnabledPropertySupported()
{
    #ifndef DPLUSWINRT_STUB
        return (g_APIContractPresent >= 9);
    #else
        return false;
    #endif
}

bool DPWinRT_StartCaptureFromHWND(vr::VROverlayHandle_t overlay_handle, HWND handle)
{
    #ifndef DPLUSWINRT_STUB
        DPWinRTThreadData data;
        data.SourceWindow = handle;

        return DPWinRT_Internal_StartCapture(overlay_handle, data);
    #else
        return false;
    #endif
}

bool DPWinRT_StartCaptureFromDesktop(vr::VROverlayHandle_t overlay_handle, int desktop_id)
{
    #ifndef DPLUSWINRT_STUB
        DPWinRTThreadData data;
        data.DesktopID = desktop_id;

        return DPWinRT_Internal_StartCapture(overlay_handle, data);
    #else
        return false;
    #endif
}

bool DPWinRT_StartCaptureFromOverlay(vr::VROverlayHandle_t overlay_handle, vr::VROverlayHandle_t overlay_handle_source)
{
    #ifndef DPLUSWINRT_STUB

    std::lock_guard<std::mutex> lock(g_ThreadsMutex);

    //Find thread with the source overlay assigned and add the other overlay to it with duplicated state
    //This means this function is only good for adding capture after an overlay was duplicated, otherwise some state needs to be adjusted right after
    for (auto& thread : g_Threads)
    {
        auto it = std::find_if(thread.Overlays.begin(), thread.Overlays.end(), [&](const auto& data){ return (data.Handle == overlay_handle_source); });

        if (it != thread.Overlays.end())
        {
            DPWinRTOverlayData overlay_data = *it;
            overlay_data.Handle = overlay_handle;

            thread.Overlays.push_back(overlay_data);

            ::PostThreadMessage(thread.ThreadID, WM_DPLUSWINRT_UPDATE_DATA, 0, 0);
            return true;
        }
    }

    #endif //DPLUSWINRT_STUB

    return false; //Overlay wasn't used in a capture in the first place
}

bool DPWinRT_PauseCapture(vr::VROverlayHandle_t overlay_handle, bool pause)
{
    #ifndef DPLUSWINRT_STUB

    std::lock_guard<std::mutex> lock(g_ThreadsMutex);

    //Find thread with the overlay assigned and tell it to set pause state
    for (auto& thread : g_Threads)
    {
        auto it = std::find_if(thread.Overlays.begin(), thread.Overlays.end(), [&](const auto& data){ return (data.Handle == overlay_handle); });

        if (it != thread.Overlays.end())
        {
            it->IsPaused = pause;
            ::PostThreadMessage(thread.ThreadID, WM_DPLUSWINRT_CAPTURE_PAUSE, overlay_handle, pause);
            return true;
        }
    }

    #endif //DPLUSWINRT_STUB

    return false; //Overlay wasn't used in a capture in the first place
}

bool DPWinRT_StopCapture(vr::VROverlayHandle_t overlay_handle)
{
    #ifndef DPLUSWINRT_STUB

    bool wait_for_ack = false;
    //Clear out existing WM_DPLUSWINRT_THREAD_ACK messages first just in case
    MSG msg;
    while (PeekMessage(&msg, nullptr, WM_DPLUSWINRT_THREAD_ACK, WM_DPLUSWINRT_THREAD_ACK, PM_REMOVE));

    //Find thread with overlay and remove overlay from it
    {
        std::lock_guard<std::mutex> lock(g_ThreadsMutex);

        for (auto thread_it = g_Threads.begin(); thread_it != g_Threads.end(); ++thread_it)
        {
            auto& thread = *thread_it;
            auto it = std::find_if(thread.Overlays.begin(), thread.Overlays.end(), [&](const auto& data) { return (data.Handle == overlay_handle); });

            if (it != thread.Overlays.end())
            {
                thread.Overlays.erase(it);

                if (thread.Overlays.empty()) //Quit and remove thread when no overlays left
                {
                    ::PostThreadMessage(thread.ThreadID, WM_DPLUSWINRT_THREAD_QUIT, 0, 0);

                    ::CloseHandle(thread.ThreadHandle);
                    g_Threads.erase(thread_it);
                }
                else //otherwise, update data
                {
                    ::PostThreadMessage(thread.ThreadID, WM_DPLUSWINRT_UPDATE_DATA, 0, 0);
                }

                wait_for_ack = true;
                break;
            }
        }
    }

    //Wait for WM_DPLUSWINRT_THREAD_ACK so we can be sure there won't be any additional overlay updates after this function returns
    ULONGLONG start_tick = ::GetTickCount64();
    while (wait_for_ack)
    {
        if ( (PeekMessage(&msg, nullptr, WM_DPLUSWINRT_THREAD_ACK, WM_DPLUSWINRT_THREAD_ACK, PM_REMOVE)) || (::GetTickCount64() >= start_tick + 500) ) //On message or 500ms timeout
        {
            return true;
        }

        ::Sleep(0);
    }

    #endif //DPLUSWINRT_STUB

    return false; //Overlay wasn't used in a capture in the first place
}

bool DPWinRT_SetOverlayUpdateLimitDelay(vr::VROverlayHandle_t overlay_handle, LONGLONG delay_quadpart)
{
    #ifndef DPLUSWINRT_STUB

    std::lock_guard<std::mutex> lock(g_ThreadsMutex);

    //Find thread with the overlay assigned and update the thread data
    for (auto& thread : g_Threads)
    {
        auto it = std::find_if(thread.Overlays.begin(), thread.Overlays.end(), [&](const auto& data){ return (data.Handle == overlay_handle); });

        if (it != thread.Overlays.end())
        {
            //If no change, back out
            if (it->UpdateLimiterDelay.QuadPart == delay_quadpart)
                return true;

            it->UpdateLimiterDelay.QuadPart = delay_quadpart;

            ::PostThreadMessage(thread.ThreadID, WM_DPLUSWINRT_UPDATE_DATA, 0, 0);
            return true;
        }
    }

    #endif //DPLUSWINRT_STUB

    return false; //Overlay wasn't used in a capture in the first place
}

bool DPWinRT_SetOverlayOverUnder3D(vr::VROverlayHandle_t overlay_handle, bool is_over_under_3D, int crop_x, int crop_y, int crop_width, int crop_height)
{
    #ifndef DPLUSWINRT_STUB

    std::lock_guard<std::mutex> lock(g_ThreadsMutex);

    //Find thread with the overlay assigned and update the thread data
    for (auto& thread : g_Threads)
    {
        auto it = std::find_if(thread.Overlays.begin(), thread.Overlays.end(), [&](const auto& data){ return (data.Handle == overlay_handle); });

        if (it != thread.Overlays.end())
        {
            //If no change, back out
            if (it->IsOverUnder3D == is_over_under_3D)
            {
                //Only check if crop matches if OU3D is on
                if ( (!is_over_under_3D) || ( (it->OU3D_crop_x == crop_x) && (it->OU3D_crop_y == crop_y) && (it->OU3D_crop_width == crop_width) && (it->OU3D_crop_height == crop_height) ) )
                {
                    return true;
                }
            }

            it->IsOverUnder3D    = is_over_under_3D;
            it->OU3D_crop_x      = crop_x;
            it->OU3D_crop_y      = crop_y;
            it->OU3D_crop_width  = crop_width;
            it->OU3D_crop_height = crop_height;

            ::PostThreadMessage(thread.ThreadID, WM_DPLUSWINRT_UPDATE_DATA, 0, 0);
            return true;
        }
    }

    #endif //DPLUSWINRT_STUB

    return false; //Overlay wasn't used in a capture in the first place
}

void DPWinRT_SetCaptureCursorEnabled(bool is_cursor_enabled)
{
    #ifndef DPLUSWINRT_STUB

    //Send enable cursor message to all threads if the value changed
    if (g_IsCursorEnabled != is_cursor_enabled)
    {
        std::lock_guard<std::mutex> lock(g_ThreadsMutex);

        for (const auto& thread : g_Threads)
        {
            ::PostThreadMessage(thread.ThreadID, WM_DPLUSWINRT_ENABLE_CURSOR, is_cursor_enabled, 0);
        }

        g_IsCursorEnabled = is_cursor_enabled;
    }
    #endif //DPLUSWINRT_STUB
}

#undef _DEBUG

#ifndef DPLUSWINRT_STUB

DWORD WINAPI WinRTCaptureThreadEntry(_In_ void* Param)
{
    //The thread shouldn't have been created in the first place then, but exit if it really happens
    if (!DPWinRT_IsCaptureSupported())
    {
        return 0;
    }

    //Keep own copy of thread data
    DPWinRTThreadData data;

    {
        std::lock_guard<std::mutex> lock(g_ThreadsMutex);
        data = *(DPWinRTThreadData*)Param;
    }

    // Initialize WinRT and scope the rest of the code so it's cleaned up before unloading WinRT again
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    //Catch all unhandled WinRT exceptions in release builds so we can get rid of the thread instead of crashing the entire app
    //This assumes that doing so is alright (i.e. no process-irrecoverable exceptions occur)
    #ifndef _DEBUG
    try
    #endif
    {
        // Create the DispatcherQueue that the compositor needs to run
        auto controller = util::CreateDispatcherQueueControllerForCurrentThread();

        // Create the capture manager
        auto capture_manager = std::make_unique<CaptureManager>(data, g_MainThreadID);

        //Start capture
        if (DPWinRT_IsCaptureFromHandleSupported())
        {
            if (data.SourceWindow != nullptr)
            {
                capture_manager->StartCaptureFromWindowHandle(data.SourceWindow);
            }
            else if (data.DesktopID != -2)
            {
                if (data.DesktopID != -1)
                {
                    HMONITOR monitor_handle = nullptr;
                    GetDevmodeForDisplayID(data.DesktopID, &monitor_handle);

                    if (monitor_handle != nullptr)
                    {
                        capture_manager->StartCaptureFromMonitorHandle(monitor_handle);
                    }
                }
                else if (DPWinRT_IsCaptureFromCombinedDesktopSupported())
                {
                    capture_manager->StartCaptureFromMonitorHandle(nullptr);
                }
            }

            //Set initial cursor enabled state
            capture_manager->IsCursorEnabled(data.IsCursorEnabledInitial);
        }

        //Ideally, capabilities are checked by before creating the thread, but if not and no capture starts, there will just be an idling thread until StopCapture is called

        // Message pump
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0))
        {
            if ((msg.message >= WM_DPLUSWINRT) && (msg.message <= 0xBFFF))
            {
                switch (msg.message)
                {
                    case WM_DPLUSWINRT_UPDATE_DATA:
                    {
                        //Look for thread data and update local copy
                        std::lock_guard<std::mutex> lock(g_ThreadsMutex);

                        for (const auto& thread : g_Threads)
                        {
                            if (thread.ThreadID == data.ThreadID)
                            {
                                data = thread;
                                capture_manager->OnOverlayDataRefresh();
                                break;
                            }
                        }

                        ::PostThreadMessage(g_MainThreadID, WM_DPLUSWINRT_THREAD_ACK, 0, 0);

                        break;
                    }
                    case WM_DPLUSWINRT_CAPTURE_PAUSE:
                    {
                        const bool do_pause = msg.lParam;

                        bool is_unchanged = false;
                        bool all_paused = true;
                        for (DPWinRTOverlayData& overlay_data : data.Overlays)
                        {
                            if (overlay_data.Handle == msg.wParam)
                            {
                                //No change, back out
                                if (overlay_data.IsPaused == do_pause)
                                {
                                    is_unchanged = true;
                                    break;
                                }

                                overlay_data.IsPaused = do_pause;
                            }

                            if (!overlay_data.IsPaused)
                            {
                                all_paused = false;
                            }
                        }

                        if (!is_unchanged)
                        {
                            capture_manager->PauseCapture(all_paused);
                        }
                        break;
                    }
                    case WM_DPLUSWINRT_ENABLE_CURSOR:
                    {
                        capture_manager->IsCursorEnabled(msg.wParam);
                        break;
                    }
                    case WM_DPLUSWINRT_THREAD_QUIT:
                    {
                        //Clear overlays here so they won't receive any more updates before the actually thread exits
                        data.Overlays.clear();
                        ::PostThreadMessage(g_MainThreadID, WM_DPLUSWINRT_THREAD_ACK, 0, 0);

                        ::PostQuitMessage(0);
                        break;
                    }

                }
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        capture_manager = nullptr;
        controller = nullptr;
    }
    #ifndef _DEBUG

    catch (const winrt::hresult_error& e)
    {
        //It's worth noting that exceptions from WinRT are not supposed to get thrown on regular errors and only things that are coding mistakes
        //But we know things will go wrong when they can, let's be honest. What can go wrong isn't really well documented either, so if something
        //comes up, handle it somewhat gracefully

        //Send capture lost messages for all overlays
        //Resulting StopCapture() calls will cause cleanup of the thread book-keeping, even if this target thread is already gone
        for (const auto& overlay : data.Overlays)
        {
            ::PostThreadMessage(g_MainThreadID, WM_DPLUSWINRT_CAPTURE_LOST, overlay.Handle, 0);
        }

        //Send thread error message
        ::PostThreadMessage(g_MainThreadID, WM_DPLUSWINRT_THREAD_ERROR, data.ThreadID, e.code());

        //...and then get out of this thread
    }

    #endif

    winrt::clear_factory_cache();
    winrt::uninit_apartment();

    return 0;
}

#endif //DPLUSWINRT_STUB