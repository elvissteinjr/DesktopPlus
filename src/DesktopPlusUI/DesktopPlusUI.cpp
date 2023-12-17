//This code belongs to the Desktop+ OpenVR overlay application, licensed under GPL 3.0
//
//Much of the code here is based on the Dear ImGui Win32 DirectX 11 sample

#define NOMINMAX
#include "imgui.h"
#include "imgui_impl_win32_openvr.h"
#include "imgui_impl_dx11_openvr.h"
#include "implot.h"
#include <d3d11.h>
#include <wrl/client.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#include <dwmapi.h>
#include <shellscalingapi.h>

#include "resource.h"
#include "UIManager.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"
#include "WindowSettings.h"
#include "Util.h"
#include "ImGuiExt.h"

#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"

#include "WindowDesktopMode.h"

// Data
static Microsoft::WRL::ComPtr<ID3D11Device>           g_pd3dDevice;
static Microsoft::WRL::ComPtr<ID3D11DeviceContext>    g_pd3dDeviceContext;
static Microsoft::WRL::ComPtr<IDXGISwapChain>         g_pSwapChain;
static Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_desktopRenderTargetView;
static Microsoft::WRL::ComPtr<ID3D11Texture2D>        g_vrTex;
static Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_vrRenderTargetView;


// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd, bool desktop_mode);
void CleanupDeviceD3D();
void CreateRenderTarget(bool desktop_mode);
void CleanupRenderTarget();
void RefreshOverlayTextureSharing();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void InitImGui(HWND hwnd, bool desktop_mode);
void ProcessCmdline(bool& force_desktop_mode);

// Main code
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow)
{
    DPLog_Init("DesktopPlusUI");

    bool force_desktop_mode = false;
    ProcessCmdline(force_desktop_mode);

    //Automatically use desktop mode if dashboard app isn't running
    bool desktop_mode = ( (force_desktop_mode) || (!IPCManager::IsDashboardAppRunning()) );
    LOG_F(INFO, "Desktop+ UI running in %s", (desktop_mode) ? "desktop mode" : "VR mode");

    //Make sure only one instance is running
    StopProcessByWindowClass(g_WindowClassNameUIApp);

    //Enable DPI support for desktop mode
    ImGui_ImplWin32_EnableDpiAwareness();

    //Register application class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, g_WindowClassNameUIApp, nullptr };
    wc.hIcon   = (HICON)::LoadImage(hInstance, MAKEINTRESOURCE(IDI_DPLUS), IMAGE_ICON, GetSystemMetrics(SM_CXICON),   GetSystemMetrics(SM_CYICON),   LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)::LoadImage(hInstance, MAKEINTRESOURCE(IDI_DPLUS), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    ::RegisterClassEx(&wc);

    //Create window
    HWND hwnd;
    if (desktop_mode) 
        hwnd = ::CreateWindow(wc.lpszClassName, L"Desktop+", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, -1, -1, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    else
        hwnd = ::CreateWindow(wc.lpszClassName, L"Desktop+ UI", 0, 0, 0, 1, 1, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);

    //Init UITextureSpaces
    UITextureSpaces::Get().Init(desktop_mode);

    //Init WinRT DLL
    DPWinRT_Init();
    LOG_F(INFO, "Loaded WinRT library");

    //Init BrowserClientAPI (this doesn't start the browser process, only checks for presence)
    DPBrowserAPIClient::Get().Init();

    //Allow IPC messages even when elevated (though in normal operation, this process should not be elevated)
    IPCManager::Get().DisableUIPForRegisteredMessages(hwnd);

    //Init UIManager and load config
    UIManager ui_manager(desktop_mode);
    ConfigManager::Get().LoadConfigFromFile();
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_sync_config_state);
    ui_manager.SetWindowHandle(hwnd);

    //Init OpenVR
    //Don't try to init OpenVR without the dashboard app running since checking for active VR means launching SteamVR
    if ( (!desktop_mode) || (IPCManager::IsDashboardAppRunning()) )
    {
        if (ui_manager.InitOverlay() != vr::VRInitError_None)
        {
            ::UnregisterClass(wc.lpszClassName, wc.hInstance);

            LOG_F(ERROR, "OpenVR init failed!");

            //Try starting in desktop mode instead
            if (!desktop_mode)
            {
                LOG_F(INFO, "Attempting to start in desktop mode instead...");
                ui_manager.Restart(true);
            }

            return 2;
        }
    }

    DPLog_SteamVR_SystemInfo();

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd, desktop_mode))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);

        LOG_F(ERROR, "Direct3D init failed!");

        return 1;
    }
    LOG_F(INFO, "Loaded Direct3D");

    //Center window to the right monitor before setting real size for DPI detection to be correct
    if (desktop_mode)
    {
        CenterWindowToMonitor(hwnd, true);
    }

    InitImGui(hwnd, desktop_mode);
    ImGuiIO& io = ImGui::GetIO();

    LOG_F(INFO, "Loaded Dear ImGui");

    if (desktop_mode)
    {
        const DPRect& rect_total = UITextureSpaces::Get().GetRect(ui_texspace_total);

        //Set real window size
        RECT r;
        r.left   = 0;
        r.top    = 0;
        r.right  = int(rect_total.GetWidth()  * ui_manager.GetUIScale());
        r.bottom = int(rect_total.GetHeight() * ui_manager.GetUIScale());

        ::AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
        ::SetWindowPos(hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        //Center window on screen
        CenterWindowToMonitor(hwnd, true);

        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);
    }

    float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    //Init notification icon if OpenVR is running (no need for it in pure desktop mode without switching back)
    if ( (!ConfigManager::GetValue(configid_bool_interface_no_notification_icon)) && (ui_manager.IsOpenVRLoaded()) )
    {
        ui_manager.GetNotificationIcon().Init(hInstance);
    }

    ui_manager.OnInitDone();
    LOG_F(INFO, "Finished startup");

    //Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        vr::VROverlayHandle_t ovrl_handle_dplus = vr::k_ulOverlayHandleInvalid;

        if (!desktop_mode)
        {
            vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &ovrl_handle_dplus);
        }

        //Poll and handle messages (inputs, window resize, etc.)
        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            if (msg.message >= 0xC000)  //Custom message from overlay process, handle in UI manager
            {
                ui_manager.HandleIPCMessage(msg);
            }
            else
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
            continue;
        }

        bool do_idle = false;

        if (!desktop_mode)
        {
            vr::VREvent_t vr_event;
            bool do_quit = false;

            //Handle OpenVR events for the dashboard UI
            ImVec4 rect_v4 = UITextureSpaces::Get().GetRectAsVec4(ui_texspace_overlay_bar);

            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleOverlayBar(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event, &rect_v4);

                switch (vr_event.eventType)
                {
                    case vr::VREvent_FocusEnter:
                    {
                        //Adjust sort order so mainbar tooltips are displayed right
                        vr::VROverlay()->SetOverlaySortOrder(ui_manager.GetOverlayHandleOverlayBar(), 1);
                        break;
                    }
                    case vr::VREvent_FocusLeave:
                    case vr::VREvent_OverlayHidden:
                    {
                        //Reset adjustment so other overlays are not always behind the UI unless really needed
                        if (!ui_manager.GetOverlayBarWindow().IsAnyMenuVisible())
                        {
                            vr::VROverlay()->SetOverlaySortOrder(ui_manager.GetOverlayHandleOverlayBar(), 0);
                        }
                        break;
                    }
                    case vr::VREvent_TrackedDeviceActivated:
                    case vr::VREvent_TrackedDeviceDeactivated:
                    {
                        ui_manager.GetPerformanceWindow().RefreshTrackerBatteryList();
                        break;
                    }
                    case vr::VREvent_LeaveStandbyMode:
                    {
                        //Reset performance stats when leaving standby since it adds to the dropped frame count during that
                        ui_manager.GetPerformanceWindow().ResetCumulativeValues();
                        break;
                    }
                    case vr::VREvent_OverlaySharedTextureChanged:
                    {
                        //This should only happen during startup since the texture size never changes, but it has happened seemingly randomly before, so handle it
                        RefreshOverlayTextureSharing();
                        break;
                    }
                    case vr::VREvent_SceneApplicationChanged:
                    {
                        const bool loaded_overlay_profile = ConfigManager::Get().GetAppProfileManager().ActivateProfileForProcess(vr_event.data.process.pid);

                        if (loaded_overlay_profile)
                        {
                            ui_manager.OnProfileLoaded();
                        }
                        break;
                    }
                    case vr::VREvent_Quit:
                    {
                        do_quit = true;
                        break;
                    }
                }
            }

            //Handle OpenVR events for the floating UI
            rect_v4 = UITextureSpaces::Get().GetRectAsVec4(ui_texspace_floating_ui);

            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleFloatingUI(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event, &rect_v4);

                switch (vr_event.eventType)
                {
                    case vr::VREvent_FocusEnter:
                    {
                        //Adjust sort order so tooltips are displayed right
                        vr::VROverlay()->SetOverlaySortOrder(ui_manager.GetOverlayHandleFloatingUI(), 1);
                        break;
                    }
                    case vr::VREvent_FocusLeave:
                    {
                        //Reset adjustment so other overlays are not always behind the UI unless really needed
                        vr::VROverlay()->SetOverlaySortOrder(ui_manager.GetOverlayHandleFloatingUI(), 0);
                        break;
                    }
                }

            }

            //Handle OpenVR events for the floating settings
            rect_v4 = UITextureSpaces::Get().GetRectAsVec4(ui_texspace_settings);

            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleSettings(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event, &rect_v4);
            }

            //Handle OpenVR events for the overlay properties
            rect_v4 = UITextureSpaces::Get().GetRectAsVec4(ui_texspace_overlay_properties);

            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleOverlayProperties(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event);
            }

            //Handle OpenVR events for the VR keyboard
            rect_v4 = UITextureSpaces::Get().GetRectAsVec4(ui_texspace_keyboard);

            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleKeyboard(), &vr_event, sizeof(vr_event)))
            {
                //Let the keyboard window handle events first for multi-laser support
                if (!ui_manager.GetVRKeyboard().GetWindow().HandleOverlayEvent(vr_event))
                {
                    ImGui_ImplOpenVR_InputEventHandler(vr_event, &rect_v4);
                }
            }

            //Handle OpenVR events for the Aux UI
            rect_v4 = UITextureSpaces::Get().GetRectAsVec4(ui_texspace_aux_ui);

            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleAuxUI(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event, &rect_v4);

                switch (vr_event.eventType)
                {
                    case vr::VREvent_DashboardActivated:
                    case vr::VREvent_DashboardDeactivated:
                    {
                        ui_manager.GetAuxUI().HideTemporaryWindows();
                        break;
                    }
                }
            }

            ui_manager.PositionOverlay();
            ui_manager.GetFloatingUI().UpdateUITargetState();
            ui_manager.GetSettingsWindow().UpdateVisibility();
            ui_manager.GetOverlayPropertiesWindow().UpdateVisibility();
            ui_manager.GetVRKeyboard().GetWindow().UpdateVisibility();

            do_idle = ( (!ui_manager.IsOverlayBarOverlayVisible())                     &&
                        (!ui_manager.GetFloatingUI().IsVisible())                      &&
                        (!ui_manager.GetSettingsWindow().IsVisibleOrFading())          &&
                        (!ui_manager.GetOverlayPropertiesWindow().IsVisibleOrFading()) &&
                        (!ui_manager.GetPerformanceWindow().IsVisible())               &&
                        (!ui_manager.GetVRKeyboard().GetWindow().IsVisibleOrFading())  &&
                        (!ui_manager.GetAuxUI().IsActive()) );

            if (do_quit)
            {
                break; //Breaks the message loop, causing clean shutdown
            }
        }
        else
        {
            do_idle = ::IsIconic(hwnd);
        }

        ui_manager.GetPerformanceWindow().UpdateVisibleState();

        //Do texture reload now if it had been scheduled
        //However, do not reload texture while an item is active as the ID changes on ImageButtons... but allow it if InputText is active to not have placeholder characters on user input
        if ( (TextureManager::Get().GetReloadLaterFlag()) && ( (!ImGui::IsAnyItemActiveOrDeactivated()) || (ImGui::IsAnyInputTextActive()) ) )
        {
            TextureManager::Get().LoadAllTexturesAndBuildFonts();
        }

        //While we still need to poll, greatly reduce the rate and don't do any ImGui stuff to not waste resources (hopefully this does not mess up ImGui input state)
        if (do_idle)
        {
            ::Sleep(64); //Could wait longer, but it doesn't really make much of a difference in load and we stay more responsive like this)
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();

        if (desktop_mode)
            ImGui_ImplWin32_NewFrame();
        else
            ImGui_ImplOpenVR_NewFrame();

        ui_manager.GetVRKeyboard().OnImGuiNewFrame();

        ImGui::NewFrame();

        //Handle delayed ICP messages that need to be handled within an ImGui frame now
        ui_manager.HandleDelayedIPCMessages();

        if (!desktop_mode)
        {
            //Overlay dragging needs to happen within an ImGui frame for a valid scroll input state (ImGui::EndFrame() clears it by the time we'd loop again)
            ui_manager.UpdateOverlayDrag();

            //Make ImGui think the surface is smaller than it is (a poor man's multi-viewport hack)

            //Overlay Bar (this and floating UI need no X adjustments)
            io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_overlay_bar).GetBR().y;
            ImGui::GetMainViewport()->Size.y = io.DisplaySize.y;

            ui_manager.GetOverlayBarWindow().Update();

            //Floating UI
            io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_floating_ui).GetBR().y;
            ImGui::GetMainViewport()->Size.y = io.DisplaySize.y;

            ui_manager.GetFloatingUI().Update();

            //Overlay Properties
            io.DisplaySize.x = (float)UITextureSpaces::Get().GetRect(ui_texspace_overlay_properties).GetBR().x;
            ImGui::GetMainViewport()->Size.x = io.DisplaySize.x;
            io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_overlay_properties).GetBR().y;
            ImGui::GetMainViewport()->Size.y = io.DisplaySize.y;

            ui_manager.GetOverlayPropertiesWindow().Update();

            //Settings
            io.DisplaySize.x = (float)UITextureSpaces::Get().GetRect(ui_texspace_settings).GetBR().x;
            ImGui::GetMainViewport()->Size.x = io.DisplaySize.x;
            io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_settings).GetBR().y;
            ImGui::GetMainViewport()->Size.y = io.DisplaySize.y;

            ui_manager.GetSettingsWindow().Update();

            //Keyboard
            io.DisplaySize.x = (float)UITextureSpaces::Get().GetRect(ui_texspace_keyboard).GetBR().x;
            ImGui::GetMainViewport()->Size.x = io.DisplaySize.x;
            io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_keyboard).GetBR().y;
            ImGui::GetMainViewport()->Size.y = io.DisplaySize.y;

            ui_manager.GetVRKeyboard().GetWindow().Update();

            //Performance Monitor
            io.DisplaySize.x = (float)UITextureSpaces::Get().GetRect(ui_texspace_performance_monitor).GetBR().x;
            ImGui::GetMainViewport()->Size.x = io.DisplaySize.x;
            io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_performance_monitor).GetBR().y;
            ImGui::GetMainViewport()->Size.y = io.DisplaySize.y;

            ui_manager.GetPerformanceWindow().Update();

            //Aux UI
            io.DisplaySize.x = (float)UITextureSpaces::Get().GetRect(ui_texspace_aux_ui).GetBR().x;
            ImGui::GetMainViewport()->Size.x = io.DisplaySize.x;
            io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_aux_ui).GetBR().y;
            ImGui::GetMainViewport()->Size.y = io.DisplaySize.y;

            ui_manager.GetAuxUI().Update();
        }
        else
        {
            ui_manager.GetDesktopModeWindow().Update();
        }

        //Haptic feedback for hovered items, like the rest of the SteamVR UI
        if ( (!desktop_mode) && (ImGui::HasHoveredNewItem()) )
        {
            for (const auto& overlay_handle : ui_manager.GetUIOverlayHandles())
            {
                if (ConfigManager::Get().IsLaserPointerTargetOverlay(overlay_handle))
                {
                    ui_manager.TriggerLaserPointerHaptics(overlay_handle);
                }
            }
        }

        // Rendering
        if (ui_manager.GetRepeatFrame()) //If frame repeat is enabled, don't actually render and skip vsync
        {
            ImGui::EndFrame();
            ui_manager.DecreaseRepeatFrameCount();
        }
        else
        {
            ImGui::Render();

            if (desktop_mode)
            {
                g_pd3dDeviceContext->OMSetRenderTargets(1, g_desktopRenderTargetView.GetAddressOf(), nullptr);
                g_pd3dDeviceContext->ClearRenderTargetView(g_desktopRenderTargetView.Get(), clear_color);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                HRESULT res = g_pSwapChain->Present(1, 0); // Present with vsync

                if (res == DXGI_STATUS_OCCLUDED) //When occluded, Present() will not wait for us
                {
                    ::DwmFlush(); //We could wait longer, but let's stay responsive
                }
            }
            else
            {
                g_pd3dDeviceContext->OMSetRenderTargets(1, g_vrRenderTargetView.GetAddressOf(), nullptr);
                g_pd3dDeviceContext->ClearRenderTargetView(g_vrRenderTargetView.Get(), clear_color);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                //Set Overlay texture
                if ((ui_manager.GetOverlayHandleOverlayBar() != vr::k_ulOverlayHandleInvalid) && (g_vrTex))
                {
                    vr::Texture_t vrtex;
                    vrtex.handle = g_vrTex.Get();
                    vrtex.eType = vr::TextureType_DirectX;
                    vrtex.eColorSpace = vr::ColorSpace_Gamma;

                    vr::VROverlay()->SetOverlayTexture(ui_manager.GetOverlayHandleOverlayBar(), &vrtex);
                }

                //Set overlay intersection mask... there doesn't seem to be much overhead from doing this every frame, even though we only need to update this sometimes
                auto overlay_handles = ui_manager.GetUIOverlayHandles();
                std::vector<vr::VROverlayIntersectionMaskPrimitive_t> mask_primitives;
                ImGui_ImplOpenVR_SetIntersectionMaskFromWindows(overlay_handles.data(), overlay_handles.size(), &mask_primitives);
                ui_manager.SendUIIntersectionMaskToDashboardApp(mask_primitives);

                //Wait for VR frame sync (34 ms timeout so we don't go much below 30 fps worst case)
                g_pd3dDeviceContext->Flush();
                vr::VROverlay()->WaitFrameSync(34);
            }
        }
    }

    LOG_F(INFO, "Shutting down...");

    // Cleanup
    ui_manager.OnExit();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd, bool desktop_mode)
{   
    //Get the adapter recommended by OpenVR if it's loaded (needed for Performance Monitor even in desktop mode)
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr_vr;

    if (UIManager::Get()->IsOpenVRLoaded())
    {
        Microsoft::WRL::ComPtr<IDXGIFactory1> factory_ptr;
        int32_t vr_gpu_id;
        vr::VRSystem()->GetDXGIOutputInfo(&vr_gpu_id);

        HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory_ptr);
        if (!FAILED(hr))
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr;
            UINT i = 0;

            while (factory_ptr->EnumAdapters(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND)
            {
                if (i == vr_gpu_id)
                {
                    adapter_ptr_vr = adapter_ptr;
                    break;
                }

                ++i;
            }
        }

        if (adapter_ptr_vr != nullptr)
        {
            //Set target GPU and total VRAM for Performance Monitor
            DXGI_ADAPTER_DESC adapter_desc;
            if (adapter_ptr_vr->GetDesc(&adapter_desc) == S_OK)
            {
                UIManager::Get()->GetPerformanceWindow().GetPerformanceData().SetTargetGPU(adapter_desc.AdapterLuid, adapter_desc.DedicatedVideoMemory);
            }
        }
    }

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

    if (desktop_mode)
    {
        //Setup swap chain
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;   //DXGI_SWAP_EFFECT_FLIP_DISCARD also would work, but we still support Windows 8

        if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
            return false;
    }
    else 
    {
        //No swap chain needed for VR

        if (adapter_ptr_vr != nullptr)
        {
            if (D3D11CreateDevice(adapter_ptr_vr.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
            {
                return false;
            }
        }
        else //Still try /something/, but it probably won't work
        {
            if (D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
            {
                return false;
            }
        }
    }

    CreateRenderTarget(desktop_mode);
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();

    g_pSwapChain.Reset();
    g_pd3dDeviceContext.Reset();
    g_pd3dDevice.Reset();
}

void CreateRenderTarget(bool desktop_mode)
{
    HRESULT hr;
    const DPRect& texrect_total = UITextureSpaces::Get().GetRect(ui_texspace_total);

    // Create overlay texture
    D3D11_TEXTURE2D_DESC TexD;
    RtlZeroMemory(&TexD, sizeof(D3D11_TEXTURE2D_DESC));
    TexD.Width  = texrect_total.GetWidth();
    TexD.Height = texrect_total.GetHeight();
    TexD.MipLevels = 1;
    TexD.ArraySize = 1;
    TexD.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    TexD.SampleDesc.Count = 1;
    TexD.Usage = D3D11_USAGE_DEFAULT;
    TexD.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    TexD.CPUAccessFlags = 0;
    TexD.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    hr = g_pd3dDevice->CreateTexture2D(&TexD, nullptr, &g_vrTex);

    if (FAILED(hr))
    {
        //Cry
        return;
    }

    UIManager::Get()->SetSharedTextureRef(g_vrTex.Get());

    // Create render target view for overlay texture
    D3D11_RENDER_TARGET_VIEW_DESC tex_rtv_desc = {};

    tex_rtv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    tex_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    tex_rtv_desc.Texture2D.MipSlice = 0;

    g_pd3dDevice->CreateRenderTargetView(g_vrTex.Get(), &tex_rtv_desc, &g_vrRenderTargetView);

    if (desktop_mode)
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
        g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

        if (pBackBuffer != nullptr)
        {
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &g_desktopRenderTargetView);
        }
    }
}

void CleanupRenderTarget()
{
    g_desktopRenderTargetView.Reset();
    g_vrRenderTargetView.Reset();
    g_vrTex.Reset();
}

void RefreshOverlayTextureSharing()
{
    //Set up advanced texture sharing between the overlays
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleFloatingUI(),        g_vrTex.Get());
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleSettings(),          g_vrTex.Get());
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleOverlayProperties(), g_vrTex.Get());
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleKeyboard(),          g_vrTex.Get());
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleAuxUI(),             g_vrTex.Get());
    //Also schedule for performance overlays, in case there are any
    UIManager::Get()->GetPerformanceWindow().ScheduleOverlaySharedTextureUpdate();
}



// Win32 message handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if ((UIManager::Get()) && (UIManager::Get()->IsInDesktopMode()))
    {
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }

    switch (msg)
    {
        case WM_SIZE:
        {
            if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
            {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget(UIManager::Get()->IsInDesktopMode());
            }
            return 0;
        }
        case WM_DPICHANGED:
        {
            UIManager::Get()->OnDPIChanged(HIWORD(wParam), *(RECT*)lParam);
            return 0;
        }
        case WM_COPYDATA:
        {
            if (UIManager::Get())
            {
                MSG pmsg;
                //Process all custom window messages posted before this
                while (PeekMessage(&pmsg, nullptr, 0xC000, 0xFFFF, PM_REMOVE))
                {
                    UIManager::Get()->HandleIPCMessage(pmsg);
                }

                MSG wmsg;
                wmsg.hwnd = hWnd;
                wmsg.message = msg;
                wmsg.wParam = wParam;
                wmsg.lParam = lParam;

                UIManager::Get()->HandleIPCMessage(wmsg);
            }
            return 0;
        }
        case WM_SYSCOMMAND:
        {
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            {
                return 0;
            }
            break;
        }
        case WM_DESTROY:
        {
            ::PostQuitMessage(0);
            return 0;
        }
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void InitImGui(HWND hwnd, bool desktop_mode)
{
    //Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    //Enable keyboard controls when in desktop mode
    if (desktop_mode)
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    io.IniFilename = nullptr;                   //We don't need any imgui.ini support
    io.ConfigInputTrickleEventQueue = false;    //Opt out of input trickling since it doesn't play well with VR scrolling (and lowers responsiveness on certain inputs)
    ImGui::ConfigDisableCtrlTab();

    //Use system double-click time in desktop mode, but set it to something VR-trigger-friendly otherwise
    io.MouseDoubleClickTime = (desktop_mode) ? ::GetDoubleClickTime() / 1000.0f : 0.50f;
    io.MouseDoubleClickMaxDist = 16.0f;

    //Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice.Get(), g_pd3dDeviceContext.Get());

    UIManager::Get()->UpdateStyle();
}

void ProcessCmdline(bool& force_desktop_mode)
{
    //__argv and __argc are global vars set by system
    for (UINT i = 0; i < static_cast<UINT>(__argc); ++i)
    {
        if ((strcmp(__argv[i], "-DesktopMode") == 0) ||
            (strcmp(__argv[i], "/DesktopMode") == 0))
        {
            force_desktop_mode = true;
        }
    }
}