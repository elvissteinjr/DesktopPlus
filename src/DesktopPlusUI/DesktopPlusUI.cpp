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

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_desktopRenderTargetView = nullptr;
static ID3D11Texture2D*         g_vrTex = nullptr;
static ID3D11RenderTargetView*  g_vrRenderTargetView = nullptr;


// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd, bool desktop_mode);
void CleanupDeviceD3D();
void CreateRenderTarget(bool desktop_mode);
void CleanupRenderTarget();
void RefreshOverlayTextureSharing();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void InitImGui(HWND hwnd);
void ProcessCmdline(bool& force_desktop_mode);

static bool g_ActionEditMode = false;

// Main code
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow)
{
    bool force_desktop_mode = false;

    ProcessCmdline(force_desktop_mode);

    //Automatically use desktop mode if dashboard app isn't running
    bool desktop_mode = ( (force_desktop_mode) || (!IPCManager::IsDashboardAppRunning()) || (g_ActionEditMode) );

    //Make sure only one instance is running
    StopProcessByWindowClass(g_WindowClassNameUIApp);

    //Enable basic DPI support for desktop mode
    ::SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

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
            return 2;
        }
    }

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd, desktop_mode))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    InitImGui(hwnd);
    ImGuiIO& io = ImGui::GetIO();

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

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    //Init WinRT DLL
    DPWinRT_Init();

    //Init notification icon if OpenVR is running (no need for it in pure desktop mode without switching back)
    if ( (!ConfigManager::Get().GetConfigBool(configid_bool_interface_no_notification_icon)) && (ui_manager.IsOpenVRLoaded()) )
    {
        ui_manager.GetNotificationIcon().Init(hInstance);
    }

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
            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleOverlayBar(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event);

                switch (vr_event.eventType)
                {
                    case vr::VREvent_MouseMove:
                    {
                        //Clamp coordinates to overlay texture space to avoid leaking into others while click dragging
                        const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_overlay_bar);
                        io.MousePos.x = (float)clamp((int)io.MousePos.x, rect.Min.x, rect.Max.x);
                        io.MousePos.y = (float)clamp((int)io.MousePos.y, rect.Min.y, rect.Max.y);
                        break;
                    }
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
                    case vr::VREvent_Quit:
                    {
                        do_quit = true;
                        break;
                    }
                }
            }

            //Handle OpenVR events for the floating UI
            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleFloatingUI(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event);

                switch (vr_event.eventType)
                {
                    case vr::VREvent_MouseMove:
                    {
                        //Clamp coordinates to overlay texture space
                        const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_floating_ui);
                        io.MousePos.x = (float)clamp((int)io.MousePos.x, rect.Min.x, rect.Max.x);
                        io.MousePos.y = (float)clamp((int)io.MousePos.y, rect.Min.y, rect.Max.y);
                        break;
                    }
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
            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleSettings(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event);

                if (vr_event.eventType == vr::VREvent_MouseMove)
                {
                    //Clamp coordinates to overlay texture space
                    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_settings);
                    io.MousePos.x = (float)clamp((int)io.MousePos.x, rect.Min.x, rect.Max.x);
                    io.MousePos.y = (float)clamp((int)io.MousePos.y, rect.Min.y, rect.Max.y);
                }
            }

            //Handle OpenVR events for the overlay properties
            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleOverlayProperties(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event);

                if (vr_event.eventType == vr::VREvent_MouseMove)
                {
                    //Clamp coordinates to overlay texture space
                    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_overlay_properties);
                    io.MousePos.x = (float)clamp((int)io.MousePos.x, rect.Min.x, rect.Max.x);
                    io.MousePos.y = (float)clamp((int)io.MousePos.y, rect.Min.y, rect.Max.y);
                }
            }

            //Handle OpenVR events for the VR keyboard
            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleKeyboard(), &vr_event, sizeof(vr_event)))
            {
                //Let the keyboard window handle events first for multi-laser support
                if (!ui_manager.GetVRKeyboard().GetWindow().HandleOverlayEvent(vr_event))
                {
                    ImGui_ImplOpenVR_InputEventHandler(vr_event);

                    if (vr_event.eventType == vr::VREvent_MouseMove)
                    {
                        //Clamp coordinates to overlay texture space
                        const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_keyboard);
                        io.MousePos.x = (float)clamp((int)io.MousePos.x, rect.Min.x, rect.Max.x);
                        io.MousePos.y = (float)clamp((int)io.MousePos.y, rect.Min.y, rect.Max.y);
                    }
                }
            }

            //Handle OpenVR events for the Aux UI
            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleAuxUI(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event);

                switch (vr_event.eventType)
                {
                    case vr::VREvent_MouseMove:
                    {
                        //Clamp coordinates to overlay texture space
                        const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_aux_ui);
                        io.MousePos.x = (float)clamp((int)io.MousePos.x, rect.Min.x, rect.Max.x);
                        io.MousePos.y = (float)clamp((int)io.MousePos.y, rect.Min.y, rect.Max.y);

                        break;
                    }
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
        if ( (TextureManager::Get().GetReloadLaterFlag()) && (!ImGui::IsAnyItemActiveOrDeactivated()) ) //Do not reload texture while an item is active as the ID changes on ImageButtons
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
            if (g_ActionEditMode)  //Temporary action edit mode
            {
                ui_manager.GetSettingsActionEditWindow().Update();
            }
            else
            {
                ui_manager.GetOverlayBarWindow().Update();
                ui_manager.GetSettingsWindow().Update();
                ui_manager.GetOverlayPropertiesWindow().Update();
                ui_manager.GetVRKeyboard().GetWindow().Update();
                ui_manager.GetAuxUI().Update();
            }
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
                g_pd3dDeviceContext->OMSetRenderTargets(1, &g_desktopRenderTargetView, nullptr);
                g_pd3dDeviceContext->ClearRenderTargetView(g_desktopRenderTargetView, (float*)&clear_color);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                HRESULT res = g_pSwapChain->Present(1, 0); // Present with vsync

                if (res == DXGI_STATUS_OCCLUDED) //When occluded, Present() will not wait for us
                {
                    ::DwmFlush(); //We could wait longer, but let's stay responsive
                }
            }
            else
            {
                g_pd3dDeviceContext->OMSetRenderTargets(1, &g_vrRenderTargetView, nullptr);
                g_pd3dDeviceContext->ClearRenderTargetView(g_vrRenderTargetView, (float*)&clear_color);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                //Set Overlay texture
                if ((ui_manager.GetOverlayHandleOverlayBar() != vr::k_ulOverlayHandleInvalid) && (g_vrTex))
                {
                    vr::Texture_t vrtex;
                    vrtex.handle = g_vrTex;
                    vrtex.eType = vr::TextureType_DirectX;
                    vrtex.eColorSpace = vr::ColorSpace_Gamma;

                    vr::VROverlay()->SetOverlayTexture(ui_manager.GetOverlayHandleOverlayBar(), &vrtex);
                }

                //Set overlay intersection mask... there doesn't seem to be much overhead from doing this every frame, even though we only need to update this sometimes
                auto overlay_handles = ui_manager.GetUIOverlayHandles();
                ImGui_ImplOpenVR_SetIntersectionMaskFromWindows(overlay_handles.data(), overlay_handles.size());

                //Since we don't get vsync on our message-only window from a swapchain, we don't use any in non-desktop mode.
                //While this is still synced to the desktop instead of the HMD, it's not using inaccurate timers at least and works well enough for this kind of content
                //Valve should should think about providing VSync for overlays maybe for those that need it
                g_pd3dDeviceContext->Flush();
                ::DwmFlush();                    //Use DwmFlush as vsync equivalent 
            }
        }
    }

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
        // Setup swap chain
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
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

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

    if (g_pSwapChain) 
    { 
        g_pSwapChain->Release(); 
        g_pSwapChain = nullptr;
    }

    if (g_pd3dDeviceContext) 
    { 
        g_pd3dDeviceContext->Release(); 
        g_pd3dDeviceContext = nullptr; 
    }

    if (g_pd3dDevice) 
    { 
        g_pd3dDevice->Release(); 
        g_pd3dDevice = nullptr; 
    }
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

    UIManager::Get()->SetSharedTextureRef(g_vrTex);

    // Create render target view for overlay texture
    D3D11_RENDER_TARGET_VIEW_DESC tex_rtv_desc;

    tex_rtv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    tex_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    tex_rtv_desc.Texture2D.MipSlice = 0;

    g_pd3dDevice->CreateRenderTargetView(g_vrTex, &tex_rtv_desc, &g_vrRenderTargetView);

    if (desktop_mode)
    {
        ID3D11Texture2D* pBackBuffer;
        g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

        if (pBackBuffer != nullptr)
        {
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_desktopRenderTargetView);
            pBackBuffer->Release();
        }
    }
}

void CleanupRenderTarget()
{
    if (g_desktopRenderTargetView)
    { 
        g_desktopRenderTargetView->Release();
        g_desktopRenderTargetView = nullptr; 
    }

    if (g_vrRenderTargetView)
    {
        g_vrRenderTargetView->Release();
        g_vrRenderTargetView = nullptr;
    }

    if (g_vrTex)
    {
        g_vrTex->Release();
        g_vrTex = nullptr;
    }
}

void RefreshOverlayTextureSharing()
{
    //Set up advanced texture sharing between the overlays
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleFloatingUI(),        g_vrTex);
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleSettings(),          g_vrTex);
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleOverlayProperties(), g_vrTex);
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleKeyboard(),          g_vrTex);
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), UIManager::Get()->GetOverlayHandleAuxUI(),             g_vrTex);
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

void InitImGui(HWND hwnd)
{
    //Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); //(void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.IniFilename = nullptr; //We don't need any imgui.ini support

    //Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //Do a bit of custom styling
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 7.0f;

    style.FrameRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.IndentSpacing = style.ItemSpacing.x;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.085f, 0.135f, 0.155f, 0.96f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.088f, 0.138f, 0.158f, 0.96f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.185f, 0.245f, 0.285f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.109f, 0.175f, 0.224f, 1.000f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.08f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.298f, 0.596f, 0.859f, 1.000f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.333f, 0.616f, 1.000f, 1.000f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.063f, 0.548f, 1.000f, 1.000f);
    colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.28f, 0.305f, 0.3f, 0.25f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    //colors[ImGuiCol_ModalWindowDimBg]    = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    Style_ImGuiCol_TextNotification        = ImVec4(0.64f, 0.97f, 0.26f, 1.00f);
    Style_ImGuiCol_TextWarning             = ImVec4(0.98f, 0.81f, 0.26f, 1.00f);
    Style_ImGuiCol_TextError               = ImVec4(0.97f, 0.33f, 0.33f, 1.00f);
    Style_ImGuiCol_ButtonPassiveToggled    = ImVec4(0.180f, 0.349f, 0.580f, 0.404f);
    Style_ImGuiCol_SteamVRCursor           = ImVec4(0.463f, 0.765f, 0.882f, 1.000f);
    Style_ImGuiCol_SteamVRCursorBorder     = ImVec4(0.161f, 0.176f, 0.196f, 0.929f);

    //Setup ImPlot style
    ImPlotStyle& plot_style = ImPlot::GetStyle();
    plot_style.PlotPadding      = {0.0f, 0.0f};
    plot_style.AntiAliasedLines = true;
    plot_style.FillAlpha        = 0.25f;
    plot_style.Colors[ImPlotCol_FrameBg] = ImVec4(0.03f, 0.05f, 0.06f, 0.10f);
    plot_style.Colors[ImPlotCol_PlotBg]  = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    //Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    //Adapt to DPI
    float dpi_scale = 1.0f;
    if (UIManager::Get()->IsInDesktopMode())
    {
        HMONITOR mon = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        UINT dpix, dpiy;
        ::GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);   //X and Y will always be identical... interesting API
        dpi_scale = (dpix / 96.0f) /** 0.625f*/;  //Scaling based on 100% being the VR font at 32pt and desktop 100% DPI font being at 20pt

        //Only apply proper scaling in action edit mode since the new UI doesn't scale properly yet
        if (g_ActionEditMode)
            dpi_scale *= 0.625f;
    }

    UIManager::Get()->SetUIScale(dpi_scale);
    
    TextureManager::Get().LoadAllTexturesAndBuildFonts();

    //Set DPI-dependent style
    style.LogSliderDeadzone = (float)int(58.0f * dpi_scale); //Force whole pixel size

    if (UIManager::Get()->IsInDesktopMode())
    {
        io.DisplaySize.x = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetWidth()  * dpi_scale;
        io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetHeight() * dpi_scale;

        style.ScrollbarSize = (float)int(23.0f * dpi_scale); 
    }
    else
    {
        io.DisplaySize.x = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetWidth();
        io.DisplaySize.y = (float)UITextureSpaces::Get().GetRect(ui_texspace_total).GetHeight();

        style.ScrollbarSize = (float)int(32.0f * dpi_scale);

        //UpdateOverlayDimming() relies on loaded ImGui/style, so do the initial call to that here
        UIManager::Get()->UpdateOverlayDimming();
    }
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

        if ((strcmp(__argv[i], "-ActionEditor") == 0) ||
            (strcmp(__argv[i], "/ActionEditor") == 0))
        {
            g_ActionEditMode = true;
        }
    }
}