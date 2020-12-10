//This code belongs to the Desktop+ OpenVR overlay application, licensed under GPL 3.0
//
//Much of the code here is based on the Dear ImGui Win32 DirectX 11 sample

#include "imgui.h"
#include "imgui_impl_win32_openvr.h"
#include "imgui_impl_dx11_openvr.h"
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#include <dwmapi.h>
#include <shellscalingapi.h>

#include "resource.h"
#include "UIManager.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"
#include "WindowMainBar.h"
#include "WindowSideBar.h"
#include "WindowSettings.h"
#include "WindowKeyboardHelper.h"
#include "Util.h"

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
void InitOverlayTextureSharing();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void InitImGui(HWND hwnd);
void ProcessCmdline(bool& force_desktop_mode);

#include "ImGuiExt.h"

// Main code
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow)
{
    bool force_desktop_mode = false;

    ProcessCmdline(force_desktop_mode);

    //Automatically use desktop mode if dashboard app isn't running
    bool desktop_mode = ( (force_desktop_mode) || (!IPCManager::IsDashboardAppRunning()) );

    //Make sure only one instance is running
    StopProcessByWindowClass(g_WindowClassNameUIApp);

    UIManager ui_manager(desktop_mode);
    ConfigManager::Get().LoadConfigFromFile();
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_sync_config_state);

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

    //Initialize overlay texture sharing if needed
    if (ui_manager.IsOpenVRLoaded())
    {
        InitOverlayTextureSharing();
    }
    
    InitImGui(hwnd);
    ImGuiIO& io = ImGui::GetIO();

    if (desktop_mode)
    {
        //Set real window size
        RECT r;
        r.left   = 0;
        r.top    = 0;
        r.right  = int(TEXSPACE_TOTAL_WIDTH         * ui_manager.GetUIScale());
        r.bottom = int(TEXSPACE_DASHBOARD_UI_HEIGHT * ui_manager.GetUIScale());

        ::AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
        ::SetWindowPos(hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        //Center window on screen
        CenterWindowToMonitor(hwnd, true);

        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);
    }

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    //Windows
    WindowKeyboardHelper window_kbdhelper;

    //Init WinRT DLL
    DPWinRT_Init();

    //Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        vr::VROverlayHandle_t ovrl_handle_dplus = vr::k_ulOverlayHandleInvalid;

        if (!desktop_mode)
        {
            vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &ovrl_handle_dplus);

            if (ovrl_handle_dplus != vr::k_ulOverlayHandleInvalid)
            {
                ImGui_ImplOpenVR_InputResetVRKeyboard(ovrl_handle_dplus);
            }
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
            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandle(), &vr_event, sizeof(vr_event)))
            {
                ImGui_ImplOpenVR_InputEventHandler(vr_event);
                
                switch (vr_event.eventType)
                {
                    case vr::VREvent_FocusEnter:
                    {
                        //Adjust sort order so mainbar tooltips are displayed right
                        vr::VROverlay()->SetOverlaySortOrder(ui_manager.GetOverlayHandle(), 1);
                        break;
                    }
                    case vr::VREvent_FocusLeave:
                    {
                        //Reset adjustment so other overlays are not always behind the UI unless really needed
                        if (!ui_manager.GetDashboardUI().GetSettingsWindow().IsShown())
                        {
                            vr::VROverlay()->SetOverlaySortOrder(ui_manager.GetOverlayHandle(), 0);
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
            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleFloatingUI(), &vr_event, sizeof(vr_event)))
            {
                if (!ImGui_ImplOpenVR_InputEventHandler(vr_event))
                {
                    //Event was not handled by ImGui
                    /*switch (vr_event.eventType)
                    {

                    }*/
                }
            }

            //Handle OpenVR events for the keyboard helper
            while (vr::VROverlay()->PollNextOverlayEvent(ui_manager.GetOverlayHandleKeyboardHelper(), &vr_event, sizeof(vr_event)))
            {
                if (!ImGui_ImplOpenVR_InputEventHandler(vr_event))
                {
                    //Event was not handled by ImGui
                    /*switch (vr_event.eventType)
                    {

                    }*/
                }
            }

            ui_manager.PositionOverlay(window_kbdhelper);
            ui_manager.GetFloatingUI().UpdateUITargetState();

            do_idle = ( (!ui_manager.IsOverlayVisible()) && (!ui_manager.IsOverlayKeyboardHelperVisible()) && (!ui_manager.GetFloatingUI().IsVisible()) );

            if (do_quit)
            {
                break; //Breaks the message loop, causing clean shutdown
            }
        }
        else
        {
            do_idle = ::IsIconic(hwnd);
        }

        //Do texture reload now if it had been scheduled
        if (TextureManager::Get().GetReloadLaterFlag())
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
        
        ImGui::NewFrame();

        if (!desktop_mode)
        {
            //Make ImGui think the surface is smaller than it is (a poor man's multi-viewport hack)
            io.DisplaySize.y = TEXSPACE_DASHBOARD_UI_HEIGHT * ui_manager.GetUIScale();

            ui_manager.GetDashboardUI().Update();

            //Once again for the floating surface
            io.DisplaySize.y = (TEXSPACE_DASHBOARD_UI_HEIGHT + TEXSPACE_VERTICAL_SPACING + TEXSPACE_FLOATING_UI_HEIGHT) * ui_manager.GetUIScale();

            ui_manager.GetFloatingUI().Update();

            //Reset/full size for the keyboard helper
            io.DisplaySize.y = TEXSPACE_TOTAL_HEIGHT * ui_manager.GetUIScale();

            window_kbdhelper.Update();
        }
        else
        {
            ui_manager.GetDashboardUI().Update();
        }

        //Haptic feedback for hovered items, like the rest of the SteamVR UI
        if ( (!desktop_mode) && (ImGui::HasHoveredNewItem()) )
        {
            if (vr::VROverlay()->IsHoverTargetOverlay(ui_manager.GetOverlayHandle()))
            {
                vr::VROverlay()->TriggerLaserMouseHapticVibration(ui_manager.GetOverlayHandle(), 0.0f, 1.0f, 0.16f);
            }
            else if (vr::VROverlay()->IsHoverTargetOverlay(ui_manager.GetOverlayHandleFloatingUI()))
            {
                vr::VROverlay()->TriggerLaserMouseHapticVibration(ui_manager.GetOverlayHandleFloatingUI(), 0.0f, 1.0f, 0.16f);
            }
            else if (vr::VROverlay()->IsHoverTargetOverlay(ui_manager.GetOverlayHandleKeyboardHelper()))
            {
                vr::VROverlay()->TriggerLaserMouseHapticVibration(ui_manager.GetOverlayHandleKeyboardHelper(), 0.0f, 1.0f, 0.16f);
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
                if ((ui_manager.GetOverlayHandle() != vr::k_ulOverlayHandleInvalid) && (g_vrTex))
                {
                    vr::Texture_t vrtex;
                    vrtex.handle = g_vrTex;
                    vrtex.eType = vr::TextureType_DirectX;
                    vrtex.eColorSpace = vr::ColorSpace_Gamma;

                    vr::VROverlay()->SetOverlayTexture(ui_manager.GetOverlayHandle(), &vrtex);
                }

                //Set overlay intersection mask... there doesn't seem to be much overhead from doing this every frame, even though we only need to update this sometimes
                ImGui_ImplOpenVR_SetIntersectionMaskFromWindows(ui_manager.GetOverlayHandle());
                ImGui_ImplOpenVR_SetIntersectionMaskFromWindows(ui_manager.GetOverlayHandleFloatingUI());
                ImGui_ImplOpenVR_SetIntersectionMaskFromWindows(ui_manager.GetOverlayHandleKeyboardHelper());

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
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd, bool desktop_mode)
{   
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

        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
        if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
            return false;
    }
    else //No swap chain needed for VR
    {
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

        //Get the adapter recommended by OpenVR
        IDXGIFactory1* factory_ptr;
        IDXGIAdapter* adapter_ptr_vr = nullptr;
        int32_t vr_gpu_id;
        vr::VRSystem()->GetDXGIOutputInfo(&vr_gpu_id);  

        HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory_ptr);
        if (!FAILED(hr))
        {
            IDXGIAdapter* adapter_ptr = nullptr;
            UINT i = 0;

            while (factory_ptr->EnumAdapters(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND)
            {
                if (i == vr_gpu_id)
                {
                    adapter_ptr_vr = adapter_ptr;
                    adapter_ptr_vr->AddRef();

                    adapter_ptr->Release();
                    break;
                }

                adapter_ptr->Release();
                ++i;
            }

            factory_ptr->Release();
            factory_ptr = nullptr;
        }

        if (adapter_ptr_vr != nullptr)
        {
            if (D3D11CreateDevice(adapter_ptr_vr, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
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

    // Create overlay texture
    D3D11_TEXTURE2D_DESC TexD;
    RtlZeroMemory(&TexD, sizeof(D3D11_TEXTURE2D_DESC));
    TexD.Width = TEXSPACE_TOTAL_WIDTH;
    TexD.Height = TEXSPACE_TOTAL_HEIGHT;
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
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_desktopRenderTargetView);
        pBackBuffer->Release();
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

void InitOverlayTextureSharing()
{
    //Set up advanced texture sharing between the overlays

    //Set texture to g_vrTex for the main overlay
    vr::Texture_t vrtex;
    vrtex.handle = g_vrTex;
    vrtex.eType = vr::TextureType_DirectX;
    vrtex.eColorSpace = vr::ColorSpace_Gamma;

    vr::VROverlay()->SetOverlayTexture(UIManager::Get()->GetOverlayHandle(), &vrtex);

    //Share this with the other UI overlays
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandle(), UIManager::Get()->GetOverlayHandleFloatingUI(),     g_vrTex);
    SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandle(), UIManager::Get()->GetOverlayHandleKeyboardHelper(), g_vrTex);
}



// Win32 message handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if ( (UIManager::Get()->IsInDesktopMode()) && (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) )
        return true;

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
            break;
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
    ImGuiIO& io = ImGui::GetIO(); //(void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.IniFilename = nullptr; //We don't need any imgui.ini support

    //Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //Do a bit of custom styling
    //ImGui's default dark style is already close to what SteamVR... used to be going for. 
    //Desktop+ was trying to fit in as well, but the new style is meh and not simple to replicate with just colors
    //This is fine.
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.36f, 0.38f, 0.41f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.03f, 0.04f, 0.06f, 0.96f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.03f, 0.04f, 0.06f, 0.94f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.04f, 0.47f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    Style_ImGuiCol_TextWarning              = ImVec4(0.98f, 0.81f, 0.26f, 1.00f);
    Style_ImGuiCol_TextError                = ImVec4(0.97f, 0.33f, 0.33f, 1.00f);
    Style_ImGuiCol_ButtonPassiveToggled     = ImVec4(0.180f, 0.349f, 0.580f, 0.404f);


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
        dpi_scale = (dpix / 96.0f) * 0.625f;  //Scaling based on 100% being the VR font at 32pt and desktop 100% DPI font being at 20pt
    }

    UIManager::Get()->SetUIScale(dpi_scale);
    
    TextureManager::Get().LoadAllTexturesAndBuildFonts();

    //Set DPI-dependent style
    ImGuiStyle& style = ImGui::GetStyle();
    style.LogSliderDeadzone = (float)int(58.0f * dpi_scale); //Force whole pixel size

    if (UIManager::Get()->IsInDesktopMode())
    {
        io.DisplaySize.x = TEXSPACE_TOTAL_WIDTH  * dpi_scale;
        io.DisplaySize.y = TEXSPACE_DASHBOARD_UI_HEIGHT * dpi_scale;

        style.ScrollbarSize = (float)int(23.0f * dpi_scale); 
    }
    else
    {
        io.DisplaySize.x = TEXSPACE_TOTAL_WIDTH;
        io.DisplaySize.y = TEXSPACE_DASHBOARD_UI_HEIGHT;

        style.ScrollbarSize = (float)int(32.0f * dpi_scale);
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
    }
}