#include "OutputManager.h"

//Keep building with 10.0.17763.0 / 1809 SDK optional
#ifdef NTDDI_WIN10_RS5
    #include <dxgi1_5.h>
#else
    #define DPLUS_DUP_NO_HDR
#endif

#include <dwmapi.h>
#include <windowsx.h>
#include <ShlDisp.h>
#include <DirectXPackedVector.h>
using namespace DirectX;
#include <sstream>

#include <limits.h>
#include <time.h>

#include "WindowManager.h"
#include "Util.h"
#include "OpenVRExt.h"
#include "Logging.h"

#include "DesktopPlusWinRT.h"
#include "DPBrowserAPIClient.h"

static OutputManager* g_OutputManager; //May not always exist, but there also should never be two, so this is fine

OutputManager* OutputManager::Get()
{
    return g_OutputManager;
}

//
//Quick note about OutputManager (and Desktop+ in general) handles multi-overlay access:
//Most functions use the "current" overlay as set by the OverlayManager or by having ConfigManager forward config values from the *_overlay_* configids
//When needed, the current overlay is temporarily changed to the one to act on. 
//To have the UI act in such a scenario, the configid_int_state_overlay_current_id_override is typically used, as there may be visible changes to the user for one frame otherwise
//To change the current overlay while nested in a temporary override, post a configid_int_interface_overlay_current_id message to both applications instead of just the counterpart
//
//This may all seem a bit messy, but helped retrofit the single overlay code a lot. Feel like cleaning this up with a way better scheme? Go ahead.
//

OutputManager::OutputManager(HANDLE PauseDuplicationEvent, HANDLE ResumeDuplicationEvent) :
    m_Device(nullptr),
    m_DeviceContext(nullptr),
    m_Sampler(nullptr),
    m_BlendState(nullptr),
    m_RasterizerState(nullptr),
    m_VertexShader(nullptr),
    m_PixelShader(nullptr),
    m_PixelShaderCursor(nullptr),
    m_InputLayout(nullptr),
    m_SharedSurf(nullptr),
    m_VertexBuffer(nullptr),
    m_ShaderResource(nullptr),
    m_KeyMutex(nullptr),
    m_WindowHandle(nullptr),
    m_PauseDuplicationEvent(PauseDuplicationEvent),
    m_ResumeDuplicationEvent(ResumeDuplicationEvent),
    m_DesktopX(0),
    m_DesktopY(0),
    m_DesktopWidth(-1),
    m_DesktopHeight(-1),
    m_MaxActiveRefreshDelay(16),
    m_OutputPendingSkippedFrame(false),
    m_OutputPendingFullRefresh(false),
    m_OutputHDRAvailable(false),
    m_OutputInvalid(false),
    m_OutputPendingDirtyRect{-1, -1, -1, -1},
    m_OutputAlphaCheckFailed(false),
    m_OutputAlphaChecksPending(0),
    m_OvrlHandleIcon(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleDashboardDummy(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleDesktopTexture(vr::k_ulOverlayHandleInvalid),
    m_OvrlTex(nullptr),
    m_OvrlRTV(nullptr),
    m_OvrlActiveCount(0),
    m_OvrlDesktopDuplActiveCount(0),
    m_OvrlDashboardActive(false),
    m_OvrlInputActive(false),
    m_OvrlDirectDragActive(false),
    m_OvrlTempDragStartTick(0),
    m_PendingDashboardDummyHeight(0.0f),
    m_LastApplyTransformTick(0),
    m_MouseLastClickTick(0),
    m_MouseIgnoreMoveEvent(false),
    m_MouseCursorNeedsUpdate(false),
    m_MouseLaserPointerUsedLastUpdate(false),
    m_MouseLastLaserPointerMoveBlocked(false),
    m_MouseLastLaserPointerX(-1),
    m_MouseLastLaserPointerY(-1),
    m_MouseIgnoreMoveEventMissCount(0),
    m_MouseLeftDownOverlayID(k_ulOverlayID_None),
    m_MouseLaserPointerScrollDeltaStart{0},
    m_MouseLaserPointerScrollDeltaFrequency{0},
    m_IsFirstLaunch(false),
    m_ComInitDone(false),
    m_DashboardActivatedOnce(false),
    m_MultiGPUTargetDevice(nullptr),
    m_MultiGPUTargetDeviceContext(nullptr),
    m_MultiGPUTexStaging(nullptr),
    m_MultiGPUTexTarget(nullptr),
    m_PerformanceFrameCount(0),
    m_PerformanceFrameCountStartTick(0),
    m_PerformanceUpdateLimiterDelay{0},
    m_IsAnyHotkeyActive(false),
    m_RegisteredHotkeyCount(0)
{
    m_MouseLastInfo = {0};
    m_MouseLastInfo.ShapeInfo.Type = DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR;
    ::QueryPerformanceFrequency(&m_MouseLaserPointerScrollDeltaFrequency);

    //Initialize ConfigManager and set first launch state based on existence of config file (used to detect first launch in Steam version)
    m_IsFirstLaunch = !ConfigManager::Get().LoadConfigFromFile();

    g_OutputManager = this;
}

//
// Destructor which calls CleanRefs to release all references and memory.
//
OutputManager::~OutputManager()
{
    CleanRefs();
    g_OutputManager = nullptr;
}

//
// Releases all references
//
void OutputManager::CleanRefs()
{
    //Release the shared desktop overlay texture since we're destroying the D3D11 device it's attached to (don't need this after shutting down OpenVR though)
    if (vr::VROverlay() != nullptr)
    {
        vr::VROverlayEx()->ReleaseSharedOverlayTexture(m_OvrlHandleDesktopTexture);
    }

    if (m_VertexShader)
    {
        m_VertexShader->Release();
        m_VertexShader = nullptr;
    }

    if (m_PixelShader)
    {
        m_PixelShader->Release();
        m_PixelShader = nullptr;
    }

    if (m_PixelShaderCursor)
    {
        m_PixelShaderCursor->Release();
        m_PixelShaderCursor = nullptr;
    }

    if (m_InputLayout)
    {
        m_InputLayout->Release();
        m_InputLayout = nullptr;
    }

    if (m_Sampler)
    {
        m_Sampler->Release();
        m_Sampler = nullptr;
    }

    if (m_BlendState)
    {
        m_BlendState->Release();
        m_BlendState = nullptr;
    }

    if (m_RasterizerState)
    {
        m_RasterizerState->Release();
        m_RasterizerState = nullptr;
    }

    if (m_DeviceContext)
    {
        m_DeviceContext->Release();
        m_DeviceContext = nullptr;
    }

    if (m_Device)
    {
        m_Device->Release();
        m_Device = nullptr;
    }

    if (m_SharedSurf)
    {
        m_SharedSurf->Release();
        m_SharedSurf = nullptr;
    }

    if (m_VertexBuffer)
    {
        m_VertexBuffer->Release();
        m_VertexBuffer = nullptr;
    }

    if (m_ShaderResource)
    {
        m_ShaderResource->Release();
        m_ShaderResource = nullptr;
    }

    if (m_OvrlTex)
    {
        m_OvrlTex->Release();
        m_OvrlTex = nullptr;
    }

    if (m_OvrlRTV)
    {
        m_OvrlRTV->Release();
        m_OvrlRTV = nullptr;
    }

    m_MouseTex.Reset();
    m_MouseShaderRes.Reset();

    //Reset mouse state variables too
    m_MouseLastClickTick = 0;
    m_MouseIgnoreMoveEvent = false;
    m_MouseLastInfo = {0};
    m_MouseLastInfo.ShapeInfo.Type = DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR;
    m_MouseLastLaserPointerX = -1;
    m_MouseLastLaserPointerY = -1;

    if (m_KeyMutex)
    {
        m_KeyMutex->Release();
        m_KeyMutex = nullptr;
    }

    if (m_ComInitDone)
    {
        ::CoUninitialize();
    }

    if (m_MultiGPUTargetDevice)
    {
        m_MultiGPUTargetDevice->Release();
        m_MultiGPUTargetDevice = nullptr;
    }

    if (m_MultiGPUTargetDeviceContext)
    {
        m_MultiGPUTargetDeviceContext->Release();
        m_MultiGPUTargetDeviceContext = nullptr;
    }

    if (m_MultiGPUTexStaging)
    {
        m_MultiGPUTexStaging->Release();
        m_MultiGPUTexStaging = nullptr;
    }

    if (m_MultiGPUTexTarget)
    {
        m_MultiGPUTexTarget->Release();
        m_MultiGPUTexTarget = nullptr;
    }
}

//
// Initialize all state
//
DUPL_RETURN OutputManager::InitOutput(HWND Window, _Out_ INT& SingleOutput, _Out_ UINT* OutCount, _Out_ RECT* DeskBounds)
{
    LOG_F(INFO, "Initializing Desktop Duplication...");

    HRESULT hr = S_OK;

    m_OutputInvalid = false;

    if (ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring))
    {
        SingleOutput = clamp(ConfigManager::GetValue(configid_int_overlay_desktop_id), -1, ::GetSystemMetrics(SM_CMONITORS) - 1);
    }
    else
    {
        SingleOutput = -1;
    }
    
    // Store window handle
    m_WindowHandle = Window;

    //Get preferred adapter if there is any, this detects which GPU the target desktop is on
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr_preferred;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr_vr;

    std::vector<DPRect> desktop_rects_prev = m_DesktopRects;
    int desktop_x_prev = m_DesktopX;
    int desktop_y_prev = m_DesktopY;

    EnumerateOutputs(SingleOutput, &adapter_ptr_preferred, &adapter_ptr_vr);

    //If there's no preferred adapter it should default to the one the HMD is connected to
    if (adapter_ptr_preferred == nullptr) 
    {
        //If both are nullptr it'll still try to find a working adapter to init, though it'll probably not work at the end in that scenario
        adapter_ptr_preferred = adapter_ptr_vr; 
    }
    //If they're the same, we don't need to do any multi-gpu handling
    if (adapter_ptr_vr == adapter_ptr_preferred)
    {
        adapter_ptr_vr = nullptr;
    }

    // Driver types supported
    D3D_DRIVER_TYPE DriverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,       //WARP shouldn't work, but this was like this in the duplication sample, so eh
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

    // Feature levels supported
    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);
    D3D_FEATURE_LEVEL FeatureLevel;

    // Create device
    if (adapter_ptr_preferred != nullptr) //Try preferred adapter first if we have one
    {
        hr = D3D11CreateDevice(adapter_ptr_preferred.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, FeatureLevels, NumFeatureLevels,
                               D3D11_SDK_VERSION, &m_Device, &FeatureLevel, &m_DeviceContext);

        if (FAILED(hr))
        {
            adapter_ptr_preferred = nullptr;
        }
    }

    if (adapter_ptr_preferred == nullptr)
    {
        for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
        {
            hr = D3D11CreateDevice(nullptr, DriverTypes[DriverTypeIndex], nullptr, 0, FeatureLevels, NumFeatureLevels,
                                   D3D11_SDK_VERSION, &m_Device, &FeatureLevel, &m_DeviceContext);

            if (SUCCEEDED(hr))
            {
                // Device creation succeeded, no need to loop anymore
                break;
            }
        }
    }

    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Device creation failed", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //Create multi-gpu target device if needed
    if (adapter_ptr_vr != nullptr)
    {
        hr = D3D11CreateDevice(adapter_ptr_vr.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, FeatureLevels, NumFeatureLevels,
                               D3D11_SDK_VERSION, &m_MultiGPUTargetDevice, &FeatureLevel, &m_MultiGPUTargetDeviceContext);

        if (FAILED(hr))
        {
            return ProcessFailure(m_Device, L"Secondary device creation failed", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
        }

        adapter_ptr_vr = nullptr;

        LOG_F(INFO, "Using cross-GPU copy");
    }

    //Check Desktop Duplication HDR support
    m_OutputHDRAvailable = false;

    #ifndef DPLUS_DUP_NO_HDR
    {
        Microsoft::WRL::ComPtr<IDXGIOutput> DxgiOutput;
        hr = adapter_ptr_preferred->EnumOutputs(0, &DxgiOutput);
        if (SUCCEEDED(hr))
        {
            Microsoft::WRL::ComPtr<IDXGIOutput5> DxgiOutput5;
            hr = DxgiOutput.As(&DxgiOutput5);
            m_OutputHDRAvailable = SUCCEEDED(hr);
        }

        if (m_OutputHDRAvailable)
        {
            LOG_F(INFO, "Desktop Duplication HDR mirroring is supported");
        }
        else
        {
            LOG_F(INFO, "Desktop Duplication HDR mirroring is not supported");
        }
    }
    #else
        LOG_F(INFO, "Desktop+ was not built with Desktop Duplication HDR support");
    #endif

    // Create shared texture
    DUPL_RETURN Return = CreateTextures(SingleOutput, OutCount, DeskBounds);
    if (Return != DUPL_RETURN_SUCCESS)
    {
        return Return;
    }

    // Make new render target view
    Return = MakeRTV();
    if (Return != DUPL_RETURN_SUCCESS)
    {
        return Return;
    }

    // Set view port
    D3D11_VIEWPORT VP;
    VP.Width = static_cast<FLOAT>(m_DesktopWidth);
    VP.Height = static_cast<FLOAT>(m_DesktopHeight);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0;
    VP.TopLeftY = 0;
    m_DeviceContext->RSSetViewports(1, &VP);

    // Create the sample state
    D3D11_SAMPLER_DESC SampDesc;
    RtlZeroMemory(&SampDesc, sizeof(SampDesc));
    SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SampDesc.MinLOD = 0;
    SampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = m_Device->CreateSamplerState(&SampDesc, &m_Sampler);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create sampler state", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    // Create the blend state
    D3D11_BLEND_DESC BlendStateDesc;
    BlendStateDesc.AlphaToCoverageEnable = FALSE;
    BlendStateDesc.IndependentBlendEnable = FALSE;
    BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
    hr = m_Device->CreateBlendState(&BlendStateDesc, &m_BlendState);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create blend state", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //Create the rasterizer state
    D3D11_RASTERIZER_DESC RasterizerDesc;
    RtlZeroMemory(&RasterizerDesc, sizeof(RasterizerDesc));
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_BACK;
    RasterizerDesc.ScissorEnable = true;

    hr = m_Device->CreateRasterizerState(&RasterizerDesc, &m_RasterizerState);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create rasterizer state", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }
    m_DeviceContext->RSSetState(m_RasterizerState);

    //Create vertex buffer for drawing whole texture
    VERTEX Vertices[NUMVERTICES] =
    {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3( 1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 1.0f,  1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
    };

    D3D11_BUFFER_DESC BufferDesc;
    RtlZeroMemory(&BufferDesc, sizeof(BufferDesc));
    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.ByteWidth = sizeof(VERTEX) * NUMVERTICES;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufferDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    RtlZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = Vertices;

    // Create vertex buffer
    hr = m_Device->CreateBuffer(&BufferDesc, &InitData, &m_VertexBuffer);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create vertex buffer", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //Set scissor rect to full
    const D3D11_RECT rect_scissor_full = { 0, 0, m_DesktopWidth, m_DesktopHeight };
    m_DeviceContext->RSSetScissorRects(1, &rect_scissor_full);

    // Initialize shaders
    Return = InitShaders();
    if (Return != DUPL_RETURN_SUCCESS)
    {
        return Return;
    }

    //In case this was called due to a resolution change, check if the crop was just exactly the set desktop in each overlay and adapt then
    if (!ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring))
    {
        unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();

        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
        {
            OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

            if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication)
            {
                int desktop_id = data.ConfigInt[configid_int_overlay_desktop_id];
                if ((desktop_id >= 0) && (desktop_id < desktop_rects_prev.size()) && (desktop_id < m_DesktopRects.size()))
                {
                    int crop_x = data.ConfigInt[configid_int_overlay_crop_x];
                    int crop_y = data.ConfigInt[configid_int_overlay_crop_y];
                    int crop_width = data.ConfigInt[configid_int_overlay_crop_width];
                    int crop_height = data.ConfigInt[configid_int_overlay_crop_height];
                    DPRect crop_rect(crop_x, crop_y, crop_x + crop_width, crop_y + crop_height);
                    DPRect desktop_rect = desktop_rects_prev[desktop_id];
                    desktop_rect.Translate({-desktop_x_prev, -desktop_y_prev});

                    if (crop_rect == desktop_rect)
                    {
                        OverlayManager::Get().SetCurrentOverlayID(i);
                        CropToDisplay(desktop_id, true);
                    }
                }
            }
        }

        OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
    }

    ResetOverlays();

    //On some systems, the Desktop Duplication output is translucent for some reason
    //We check the texture's pixels for the first few frame updates to make sure we can use the straight copy path, which should be the case on most machines
    //If it fails we use a pixel shader to fix the alpha channel during frame updates
    m_OutputAlphaCheckFailed   = false;
    m_OutputAlphaChecksPending = 10;

    return Return;
}

std::tuple<vr::EVRInitError, vr::EVROverlayError, bool> OutputManager::InitOverlay()
{
    vr::EVRInitError init_error   = vr::VRInitError_None;
    vr::VROverlayError ovrl_error = vr::VROverlayError_None;

    vr::VR_Init(&init_error, vr::VRApplication_Overlay);

    if (init_error != vr::VRInitError_None)
        return {init_error, ovrl_error, false};

    if (!vr::VROverlay())
        return {vr::VRInitError_Init_InvalidInterface, ovrl_error, false};

    DPLog_SteamVR_SystemInfo();

    m_OvrlHandleDashboardDummy = vr::k_ulOverlayHandleInvalid;
    m_OvrlHandleIcon = vr::k_ulOverlayHandleInvalid;
    m_OvrlHandleDesktopTexture = vr::k_ulOverlayHandleInvalid;

    //We already got rid of another instance of this app if there was any, but this loop takes care of it too if the detection failed or something uses our overlay key
    for (int tries = 0; tries < 10; ++tries)
    {
        //Create dashboard dummy overlay. It's only used to get a button, transform origin and position the top bar in the dashboard
        ovrl_error = vr::VROverlay()->CreateDashboardOverlay("elvissteinjr.DesktopPlusDashboard", "Desktop+", &m_OvrlHandleDashboardDummy, &m_OvrlHandleIcon);

        if (ovrl_error == vr::VROverlayError_KeyInUse)  //If the key is already in use, kill the owning process (hopefully another instance of this app)
        {
            ovrl_error = vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &m_OvrlHandleDashboardDummy);

            if ((ovrl_error == vr::VROverlayError_None) && (m_OvrlHandleDashboardDummy != vr::k_ulOverlayHandleInvalid))
            {
                LOG_F(INFO, "Overlay key already in use, killing owning process...");

                uint32_t pid = vr::VROverlay()->GetOverlayRenderingPid(m_OvrlHandleDashboardDummy);

                HANDLE phandle = ::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, TRUE, pid);

                if (phandle != nullptr)
                {
                    ::TerminateProcess(phandle, 0);
                    ::CloseHandle(phandle);
                }
                else
                {
                    ovrl_error = vr::VROverlayError_KeyInUse;
                }
            }
            else
            {
                ovrl_error = vr::VROverlayError_KeyInUse;
            }
        }
        else
        {
            break;
        }

        //Try again in a bit to check if it's just a race with some external cleanup
        ::Sleep(200);
    }


    if (m_OvrlHandleDashboardDummy != vr::k_ulOverlayHandleInvalid)
    {
        //Create desktop texture overlay. This overlay holds the desktop texture shared between Desktop Duplication Overlays
        ovrl_error = vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusDesktopTexture", "Desktop+", &m_OvrlHandleDesktopTexture);

        //Load config again to properly initialize overlays that were loaded before OpenVR was available
        const bool loaded_overlay_profile = ConfigManager::Get().GetAppProfileManager().ActivateProfileForCurrentSceneApp(); //Check if overlays from app profile need to be loaded first
        if (!loaded_overlay_profile)
        {
            ConfigManager::Get().LoadConfigFromFile();
        }

        if (m_OvrlHandleDashboardDummy != vr::k_ulOverlayHandleInvalid)
        {
            unsigned char bytes[2 * 2 * 4] = {0}; //2x2 transparent RGBA

            //Set dashboard dummy content instead of leaving it totally blank, which is undefined
            vr::VROverlay()->SetOverlayRaw(m_OvrlHandleDashboardDummy, bytes, 2, 2, 4);

            vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleDashboardDummy, vr::VROverlayInputMethod_None);
            vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleDashboardDummy, 1.5f);

            vr::VROverlay()->SetOverlayFlag(m_OvrlHandleDashboardDummy, vr::VROverlayFlags_MinimalControlBar, true);

            //ResetOverlays() is called later

            //Use different icon if GamepadUI (SteamVR 2 dashboard) exists
            vr::VROverlayHandle_t handle_gamepad_ui = vr::k_ulOverlayHandleInvalid;
            vr::VROverlay()->FindOverlay("valve.steam.gamepadui.bar", &handle_gamepad_ui);
            const char* icon_file = (handle_gamepad_ui != vr::k_ulOverlayHandleInvalid) ? "images/icon_dashboard_gamepadui.png" : "images/icon_dashboard.png";

            vr::VROverlay()->SetOverlayFromFile(m_OvrlHandleIcon, (ConfigManager::Get().GetApplicationPath() + icon_file).c_str());
        }
    }

    m_MaxActiveRefreshDelay = 1000.0f / GetHMDFrameRate();

    //Check if this process was launched by Steam by checking if the "SteamClientLaunch" environment variable exists
    bool is_steam_app = (::GetEnvironmentVariable(L"SteamClientLaunch", nullptr, 0) != 0);
    ConfigManager::SetValue(configid_bool_state_misc_process_started_by_steam, is_steam_app);
    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_misc_process_started_by_steam, is_steam_app);

    //Add application manifest and set app key to Steam one if needed (setting the app key will make it load Steam input bindings even when not launched by it)
    if (!is_steam_app)
    {
        LOG_F(INFO, "Process was not launched by Steam, setting SteamVR application identity");

        vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), g_AppKeyDashboardApp);
        m_IsFirstLaunch = (!vr::VRApplications()->IsApplicationInstalled(g_AppKeyDashboardApp));
    }

    //Even if not first launch, make sure the Theater Screen dummy app is installed
    //The Theater Screen needs a separate app entry as on Steam we don't control the contents of the app manifest for our appid, yet need to have "starts_theater_mode" in it somewhere
    if ((m_IsFirstLaunch) || (!vr::VRApplications()->IsApplicationInstalled(g_AppKeyTheaterScreen)))
    {
        vr::VRApplications()->AddApplicationManifest((ConfigManager::Get().GetApplicationPath() + "manifest.vrmanifest").c_str());
    }

    //Set application auto-launch to true if it's the first launch
    if (m_IsFirstLaunch)
    {
        LOG_F(INFO, "First launch detected. Setting application to auto-launch with SteamVR");
        vr::EVRApplicationError app_error = vr::VRApplications()->SetApplicationAutoLaunch(g_AppKeyDashboardApp, true);

        if (app_error == vr::VRApplicationError_None)
        {
            //Have the UI app display the initial setup notification
            IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_notification_show);

            //Show the dashboard overlay as well to make it easier to find when first using the app
            vr::VROverlay()->ShowDashboard("elvissteinjr.DesktopPlusDashboard");
        }
    }

    const bool vrinput_init_success = m_VRInput.Init();

    //Check if it's a WMR system and set up for that if needed
    ConfigManager::Get().InitConfigForWMR();
    DPWinRT_SetDesktopEnumerationFlags( (ConfigManager::GetValue(configid_int_interface_wmr_ignore_vscreens) == 1) );
    LOG_IF_F(INFO, (ConfigManager::GetValue(configid_int_interface_wmr_ignore_vscreens) == 1), "WMR headset detected, ignoring additional virtual displays");

    //Set pen simulation support state so the UI can act on it
    ConfigManager::SetValue(configid_bool_state_pen_simulation_supported, m_InputSim.IsPenSimulationSupported());
    LOG_F(INFO, "Pen input simulation is %s", (m_InputSim.IsPenSimulationSupported()) ? "supported" : "not supported");

    //Init background overlay if needed
    m_BackgroundOverlay.Update();

    //Hotkeys can trigger actions requiring OpenVR, so only register after OpenVR init
    RegisterHotkeys();

    //Try to get dashboard in proper state if needed
    FixInvalidDashboardLaunchState();

    //Return error state to allow for accurate display if needed
    return {vr::VRInitError_None, ovrl_error, vrinput_init_success};
}

//
// Update Overlay and handle events
//
DUPL_RETURN_UPD OutputManager::Update(_In_ PTR_INFO* PointerInfo,  _In_ DPRect& DirtyRectTotal, bool NewFrame, bool SkipFrame)
{
    if (HandleOpenVREvents())   //If quit event received, quit.
    {
        return DUPL_RETURN_UPD_QUIT;
    }

    UINT64 sync_key = 1; //Key used by duplication threads to lock for this function (duplication threads lock with 1, Update() with 0 and unlock vice versa)

    //If we previously skipped a frame, we want to actually process a new one at the next valid opportunity
    if ( (m_OutputPendingSkippedFrame) && (!SkipFrame) )
    {
        //If there isn't new frame yet, we have to unlock the keyed mutex with the one we locked it with ourselves before
        //However, if the laser pointer was used since the last update, we simply use the key for new frame data to wait for the new mouse position or frame
        //Not waiting for it reduces latency usually, but laser pointer mouse movements are weirdly not picked up without doing this or enabling the rapid laser pointer update setting
        if ( (!NewFrame) && (!m_MouseLaserPointerUsedLastUpdate) )
        {
            sync_key = 0;
        }

        NewFrame = true; //Treat this as a new frame now
        m_MouseLaserPointerUsedLastUpdate = false;
    }

    //If frame skipped and no new frame, do nothing (if there's a new frame, we have to at least re-lock the keyed mutex so the duplication threads can access it again)
    if ( (SkipFrame) && (!NewFrame) )
    {
        m_OutputPendingSkippedFrame = true; //Process the frame next time we can
        return DUPL_RETURN_UPD_SUCCESS;
    }

    //When invalid output is set, key mutex can be null, so just do nothing
    if (m_KeyMutex == nullptr)
    {
        return DUPL_RETURN_UPD_SUCCESS;
    }

    // Try and acquire sync on common display buffer (needed to safely access the PointerInfo)
    HRESULT hr = m_KeyMutex->AcquireSync(sync_key, GetMaxRefreshDelay());
    if (hr == static_cast<HRESULT>(WAIT_TIMEOUT))
    {
        // Another thread has the keyed mutex so try again later
        return DUPL_RETURN_UPD_RETRY;
    }
    else if (FAILED(hr))
    {
        return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to acquire keyed mutex", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    DUPL_RETURN_UPD ret = DUPL_RETURN_UPD_SUCCESS;

    //Got mutex, so we can access pointer info and shared surface
    DPRect mouse_rect = {PointerInfo->Position.x, PointerInfo->Position.y, int(PointerInfo->Position.x + PointerInfo->ShapeInfo.Width),
                         int(PointerInfo->Position.y + PointerInfo->ShapeInfo.Height)};

    //If mouse state got updated, expand dirty rect to include old and new cursor regions
    if ( (ConfigManager::GetValue(configid_bool_input_mouse_render_cursor)) && (m_MouseLastInfo.LastTimeStamp.QuadPart < PointerInfo->LastTimeStamp.QuadPart) )
    {
        //Only invalidate if position or shape changed, otherwise it would be a visually identical result
        if ( (m_MouseLastInfo.Position.x != PointerInfo->Position.x) || (m_MouseLastInfo.Position.y != PointerInfo->Position.y) ||
             (PointerInfo->CursorShapeChanged) || (m_MouseCursorNeedsUpdate) || (m_MouseLastInfo.Visible != PointerInfo->Visible) )
        {
            if ( (PointerInfo->Visible) )
            {
                (DirtyRectTotal.GetTL().x == -1) ? DirtyRectTotal = mouse_rect : DirtyRectTotal.Add(mouse_rect);
            }

            if (m_MouseLastInfo.Visible)
            {
                DPRect mouse_rect_last(m_MouseLastInfo.Position.x, m_MouseLastInfo.Position.y, int(m_MouseLastInfo.Position.x + m_MouseLastInfo.ShapeInfo.Width),
                                       int(m_MouseLastInfo.Position.y + m_MouseLastInfo.ShapeInfo.Height));

                (DirtyRectTotal.GetTL().x == -1) ? DirtyRectTotal = mouse_rect_last : DirtyRectTotal.Add(mouse_rect_last);
            }
        }
    }


    //If frame is skipped, skip all GPU work
    if (SkipFrame)
    {
        //Collect dirty rects for the next time we render
        (m_OutputPendingDirtyRect.GetTL().x == -1) ? m_OutputPendingDirtyRect = DirtyRectTotal : m_OutputPendingDirtyRect.Add(DirtyRectTotal);

        //Remember if the cursor changed so it's updated the next time we actually render it
        if (PointerInfo->CursorShapeChanged)
        {
            m_MouseCursorNeedsUpdate = true;
        }

        m_OutputPendingSkippedFrame = true;
        hr = m_KeyMutex->ReleaseSync(0);

        return DUPL_RETURN_UPD_SUCCESS;
    }
    else if (m_OutputPendingDirtyRect.GetTL().x != -1) //Add previously collected dirty rects if there are any
    {
        (DirtyRectTotal.GetTL().x == -1) ? DirtyRectTotal = m_OutputPendingDirtyRect : DirtyRectTotal.Add(m_OutputPendingDirtyRect);
    }

    bool has_updated_overlay = false;

    //Check all overlays for overlap and collect clipping region from matches
    DPRect clipping_region(-1, -1, -1, -1);

    if (!m_OutputPendingFullRefresh)
    {
        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
        {
            const Overlay& overlay = OverlayManager::Get().GetOverlay(i);

            if ( (overlay.IsVisible()) && ( (overlay.GetTextureSource() == ovrl_texsource_desktop_duplication) || (overlay.GetTextureSource() == ovrl_texsource_desktop_duplication_3dou_converted) ) )
            {
                const DPRect& cropping_region = overlay.GetValidatedCropRect();

                if (DirtyRectTotal.Overlaps(cropping_region))
                {
                    if (clipping_region.GetTL().x != -1)
                    {
                        clipping_region.Add(cropping_region);
                    }
                    else
                    {
                        clipping_region = cropping_region;
                    }
                }
            }
        }

        DirtyRectTotal.ClipWithFull(clipping_region);
    }
    else   //Set dirty & clipping rect to total surface for full refresh
    {
        DirtyRectTotal = {0, 0, m_DesktopWidth, m_DesktopHeight};
        clipping_region = DirtyRectTotal;
        m_OutputPendingFullRefresh = false;
    }

    m_OutputLastClippingRect = clipping_region;

    if (clipping_region.GetTL().x != -1) //Overlapped with at least one overlay
    {
        //Set scissor rect for overlay drawing function
        const D3D11_RECT rect_scissor = { DirtyRectTotal.GetTL().x, DirtyRectTotal.GetTL().y, DirtyRectTotal.GetBR().x, DirtyRectTotal.GetBR().y };
        m_DeviceContext->RSSetScissorRects(1, &rect_scissor);

        //Draw shared surface to overlay texture to avoid trouble with transparency on some systems
        bool is_full_texture = DirtyRectTotal.Contains({0, 0, m_DesktopWidth, m_DesktopHeight});
        DrawFrameToOverlayTex(is_full_texture);

        //Only handle cursor if it's in cropping region
        if (mouse_rect.Overlaps(DirtyRectTotal))
        {
            DrawMouseToOverlayTex(PointerInfo);
        }
        else if (PointerInfo->CursorShapeChanged) //But remember if the cursor changed for next time
        {
            m_MouseCursorNeedsUpdate = true;
        }

        //Set Overlay texture
        ret = RefreshOpenVROverlayTexture(DirtyRectTotal);

        //Reset scissor rect
        const D3D11_RECT rect_scissor_full = { 0, 0, m_DesktopWidth, m_DesktopHeight };
        m_DeviceContext->RSSetScissorRects(1, &rect_scissor_full);

        has_updated_overlay = (ret == DUPL_RETURN_UPD_SUCCESS_REFRESHED_OVERLAY);
    }
    else if (PointerInfo->CursorShapeChanged) //But remember if the cursor changed for next time
    {
        m_MouseCursorNeedsUpdate = true;
    }

    //Set cached mouse values
    m_MouseLastInfo = *PointerInfo;
    m_MouseLastInfo.PtrShapeBuffer = nullptr; //Not used or copied properly so remove info to avoid confusion
    m_MouseLastInfo.BufferSize = 0;

    //Reset dirty rect
    DirtyRectTotal = DPRect(-1, -1, -1, -1);

    // Release keyed mutex
    hr = m_KeyMutex->ReleaseSync(0);
    if (FAILED(hr))
    {
        return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to Release keyed mutex", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //Count frames
    if (has_updated_overlay)
    {
        m_PerformanceFrameCount++;
    }

    m_OutputPendingSkippedFrame = false;
    m_OutputPendingDirtyRect = {-1, -1, -1, -1};

    return ret;
}

void OutputManager::BusyUpdate()
{
    //Improve responsiveness of temp drag during overlay creation when applying capture source can take a bit longer (i.e. browser overlays)
    if ( (m_OverlayDragger.IsDragActive()) && (ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp)) )
    {
        m_OverlayDragger.DragUpdate();
    }
}

bool OutputManager::HandleIPCMessage(const MSG& msg)
{
    //Handle messages sent by browser process in the APIClient
    if (msg.message == DPBrowserAPIClient::Get().GetRegisteredMessageID())
    {
        DPBrowserAPIClient::Get().HandleIPCMessage(msg);
        return false;
    }

    //Apply overlay id override if needed
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    int overlay_override_id = ConfigManager::GetValue(configid_int_state_overlay_current_id_override);

    if (overlay_override_id != -1)
    {
        OverlayManager::Get().SetCurrentOverlayID(overlay_override_id);
    }

    //Config strings come as WM_COPYDATA
    if (msg.message == WM_COPYDATA)
    {
        COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)msg.lParam;
        
        //Arbitrary size limit to prevent some malicous applications from sending bad data, especially when this is running elevated
        if ( (pcds->dwData < configid_str_MAX) && (pcds->cbData <= 4096) ) 
        {
            std::string copystr((char*)pcds->lpData, pcds->cbData); //We rely on the data length. The data is sent without the NUL byte

            ConfigID_String str_id = (ConfigID_String)pcds->dwData;
            ConfigManager::SetValue(str_id, copystr);

            switch (str_id)
            {
                case configid_str_state_keyboard_string:
                {
                    m_InputSim.KeyboardText(copystr.c_str());
                    break;
                }
                case configid_str_state_app_profile_data:
                {
                    const std::string& app_key = ConfigManager::GetValue(configid_str_state_app_profile_key);

                    if (!app_key.empty())
                    {
                        AppProfile new_profile;
                        new_profile.Deserialize(copystr);

                        const bool loaded_overlay_profile = ConfigManager::Get().GetAppProfileManager().StoreProfile(app_key, new_profile);

                        if (loaded_overlay_profile)
                            ResetOverlays();
                    }
                    break;
                }
                case configid_str_state_action_data:
                {
                    Action new_action;
                    new_action.Deserialize(copystr);

                    ConfigManager::Get().GetActionManager().StoreAction(new_action);
                    break;
                }
                default: break;
            }
        }

        //Restore overlay id override
        if (overlay_override_id != -1)
        {
            OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
        }

        return false;
    }

    bool reset_mirroring = false;
    IPCMsgID msgid = IPCManager::Get().GetIPCMessageID(msg.message);

    switch (msgid)
    {
        case ipcmsg_action:
        {
            switch (msg.wParam)
            {
                case ipcact_mirror_reset:
                {
                    reset_mirroring = true;
                    break;
                }
                case ipcact_overlay_position_reset:
                {
                    DetachedTransformReset();
                    break;
                }
                case ipcact_overlay_position_adjust:
                {
                    DetachedTransformAdjust(msg.lParam);
                    break;
                }
                case ipcact_action_delete:
                {
                    ConfigManager::Get().GetActionManager().RemoveAction(msg.lParam);
                    break;
                }
                case ipcact_action_do:
                {
                    ConfigManager::Get().GetActionManager().DoAction(msg.lParam, (overlay_override_id != -1) ? (unsigned int)overlay_override_id : k_ulOverlayID_None);
                    break;
                }
                case ipcact_action_start:
                {
                    ConfigManager::Get().GetActionManager().StartAction(msg.lParam, (overlay_override_id != -1) ? (unsigned int)overlay_override_id : k_ulOverlayID_None);
                    break;
                }
                case ipcact_action_stop:
                {
                    ConfigManager::Get().GetActionManager().StopAction(msg.lParam, (overlay_override_id != -1) ? (unsigned int)overlay_override_id : k_ulOverlayID_None);
                    break;
                }
                case ipcact_keyboard_vkey:
                case ipcact_keyboard_wchar:
                {
                    HandleKeyboardMessage((IPCActionID)msg.wParam, msg.lParam);
                    break;
                }
                case ipcact_overlay_profile_load:
                {
                    reset_mirroring = HandleOverlayProfileLoadMessage(msg.lParam);
                    break;
                }
                case ipcact_crop_to_active_window:
                {
                    CropToActiveWindow();
                    break;
                }
                case ipcact_switch_task:
                {
                    ShowWindowSwitcher();
                    break;
                }
                case ipcact_overlay_duplicate:
                {
                    DuplicateOverlay((unsigned int)msg.lParam);
                    break;
                }
                case ipcact_overlay_new:
                {
                    int desktop_id = msg.lParam;

                    OverlayCaptureSource capsource;

                    switch (desktop_id)
                    {
                        case -2: capsource = ovrl_capsource_winrt_capture;       break;
                        case -3: capsource = ovrl_capsource_ui;                  break;
                        case -4: capsource = ovrl_capsource_browser;             break;
                        default: capsource = ovrl_capsource_desktop_duplication;
                    }

                    AddOverlay(capsource, desktop_id, (HWND)ConfigManager::GetValue(configid_handle_state_arg_hwnd));
                    break;
                }
                case ipcact_overlay_new_drag:
                {
                    int desktop_id         = GET_X_LPARAM(msg.lParam);
                    float pointer_distance = GET_Y_LPARAM(msg.lParam) / 100.0f;

                    OverlayCaptureSource capsource;

                    switch (desktop_id)
                    {
                        case -2: capsource = ovrl_capsource_winrt_capture;       break;
                        case -3: capsource = ovrl_capsource_ui;                  break;
                        case -4: capsource = ovrl_capsource_browser;             break;
                        default: capsource = ovrl_capsource_desktop_duplication;
                    }

                    AddOverlayDrag(pointer_distance, capsource, desktop_id, (HWND)ConfigManager::GetValue(configid_handle_state_arg_hwnd));
                    break;
                }
                case ipcact_overlay_remove:
                {
                    OverlayManager::Get().RemoveOverlay((unsigned int)msg.lParam);
                    //RemoveOverlay() may have changed active ID, keep in sync
                    ConfigManager::SetValue(configid_int_interface_overlay_current_id, OverlayManager::Get().GetCurrentOverlayID());
                    break;
                }
                case ipcact_overlay_transform_sync:
                {
                    DetachedTransformSyncAll();
                    break;
                }
                case ipcact_overlay_swap:
                {
                    OverlayManager::Get().SwapOverlays(OverlayManager::Get().GetCurrentOverlayID(), (unsigned int)msg.lParam);
                    break;
                }
                case ipcact_overlay_gaze_fade_auto:
                {
                    DetachedOverlayGazeFadeAutoConfigure();
                    break;
                }
                case ipcact_overlay_make_standalone:
                {
                    OverlayManager::Get().ConvertDuplicatedOverlayToStandalone((unsigned int)msg.lParam);
                    break;
                }
                case ipcact_browser_navigate_to_url:
                {
                    unsigned int overlay_id = (unsigned int)msg.lParam;
                    DPBrowserAPIClient::Get().DPBrowser_SetURL(OverlayManager::Get().GetOverlay(overlay_id).GetHandle(), 
                                                               OverlayManager::Get().GetConfigData(overlay_id).ConfigStr[configid_str_overlay_browser_url]);

                    break;
                }
                case ipcact_browser_recreate_context:
                {
                    unsigned int overlay_id = (unsigned int)msg.lParam;
                    DPBrowserAPIClient::Get().DPBrowser_RecreateBrowser(OverlayManager::Get().GetOverlay(overlay_id).GetHandle(), 
                                                                        OverlayManager::Get().GetConfigData(overlay_id).ConfigBool[configid_bool_overlay_browser_allow_transparency]);

                    break;
                }
                case ipcact_winmanager_drag_start:
                {
                    unsigned int overlay_id = (unsigned int)msg.lParam;
                    const bool use_pen = ConfigManager::GetValue(configid_bool_input_mouse_simulate_pen_input);

                    if (m_OverlayDragger.GetDragDeviceID() == -1)
                    {
                        unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
                        OverlayManager::Get().SetCurrentOverlayID(overlay_id);

                        //Check if it's still being hovered since it could be off before the message is processed
                        if (ConfigManager::Get().IsLaserPointerTargetOverlay(OverlayManager::Get().GetCurrentOverlay().GetHandle(), true))
                        {
                            //Reset input and WindowManager state manually since the overlay mouse up even will be consumed to finish the drag later
                            (use_pen) ? m_InputSim.PenSetPrimaryDown(false) : m_InputSim.MouseSetLeftDown(false);
                            WindowManager::Get().SetTargetWindow(nullptr);

                            if (ConfigManager::GetValue(configid_int_overlay_origin) != ovrl_origin_theater_screen)
                            {
                                if (!ConfigManager::GetValue(configid_bool_overlay_transform_locked))
                                {
                                    m_OverlayDragger.DragStart(overlay_id);
                                }
                                else
                                {
                                    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
                                    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 1);
                                }
                            }
                            else
                            {
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 2);
                            }
                        }

                        OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
                    }
                    else if (overlay_id == k_ulOverlayID_None) //Means it came from a blocked drag, reset input and WindowManager state
                    {
                        (use_pen) ? m_InputSim.PenSetPrimaryDown(false) : m_InputSim.MouseSetLeftDown(false);
                        WindowManager::Get().SetTargetWindow(nullptr);
                    }

                    break;
                }
                case ipcact_winmanager_winlist_add:
                case ipcact_winmanager_winlist_update:
                {
                    const WindowInfo* window_info = nullptr;
                    bool has_title_changed = true;

                    if (msg.wParam == ipcact_winmanager_winlist_add)
                        window_info = &WindowManager::Get().WindowListAdd((HWND)msg.lParam);
                    else
                        window_info = WindowManager::Get().WindowListUpdateTitle((HWND)msg.lParam, &has_title_changed);

                    //Find inactive overlays using the window and start capture for them
                    if ( (window_info != nullptr) && (has_title_changed) ) //Only do this when the title changed
                    {
                        for (auto& i : OverlayManager::Get().FindInactiveOverlaysForWindow(*window_info))
                        {
                            OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                            data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] = msg.lParam;

                            if (ConfigManager::GetValue(configid_int_windows_winrt_capture_lost_behavior) == window_caplost_hide_overlay)
                                data.ConfigBool[configid_bool_overlay_enabled] = true;

                            OnSetOverlayWinRTCaptureWindow(i);

                            //Send to UI
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)i);
                            IPCManager::Get().PostConfigMessageToUIApp(configid_handle_overlay_state_winrt_hwnd, msg.lParam);

                            if (ConfigManager::GetValue(configid_int_windows_winrt_capture_lost_behavior) == window_caplost_hide_overlay)
                                IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_enabled, true);

                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
                        }
                    }
                    break;
                }
                case ipcact_winmanager_winlist_remove:
                {
                    std::wstring last_title_w = WindowManager::Get().WindowListRemove((HWND)msg.lParam);
                    std::string last_title = StringConvertFromUTF16(last_title_w.c_str());

                    //Some windows clear their title entirely before ceasing to exist, skip those
                    if (last_title.empty())
                        break;

                    //Set last known title for overlays that captured this window
                    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                    {
                        OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                        if (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] == msg.lParam)
                        {
                            data.ConfigStr[configid_str_overlay_winrt_last_window_title] = last_title;
                        }
                    }

                    break;
                }
                case ipcact_winmanager_text_input_focus:
                {
                    WindowManager::Get().UpdateTextInputFocusedState(msg.lParam);
                    break;
                }
                case ipcact_sync_config_state:
                {
                    //Overlay state
                    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                    {
                        const Overlay& overlay        = OverlayManager::Get().GetOverlay(i);
                        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)i);

                        IPCManager::Get().PostConfigMessageToUIApp(configid_handle_overlay_state_overlay_handle,  data.ConfigHandle[configid_handle_overlay_state_overlay_handle]);

                        IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_width,  data.ConfigInt[configid_int_overlay_state_content_width]);
                        IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_height, data.ConfigInt[configid_int_overlay_state_content_height]);

                        //Send over current HWND if there's an active capture
                        if ( (overlay.GetTextureSource() == ovrl_texsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0))
                        {
                            IPCManager::Get().PostConfigMessageToUIApp(configid_handle_overlay_state_winrt_hwnd, data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd]);
                        }
                        else if (overlay.GetTextureSource() == ovrl_texsource_browser) //Send browser nav state if it's an active browser overlay
                        {
                            IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_state_browser_nav_can_go_back,    data.ConfigBool[configid_bool_overlay_state_browser_nav_can_go_back]);
                            IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_state_browser_nav_can_go_forward, data.ConfigBool[configid_bool_overlay_state_browser_nav_can_go_forward]);
                            IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_state_browser_nav_is_loading,     data.ConfigBool[configid_bool_overlay_state_browser_nav_is_loading]);
                        }

                        IPCManager::Get().PostConfigMessageToUIApp(configid_float_overlay_state_brightness_extra_multiplier, data.ConfigFloat[configid_float_overlay_state_brightness_extra_multiplier]);

                        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
                    }

                    //Global config state
                    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_interface_desktop_count,               ConfigManager::GetValue(configid_int_state_interface_desktop_count));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_overlay_dragmode,                     ConfigManager::GetValue(configid_bool_state_overlay_dragmode));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_overlay_selectmode,                   ConfigManager::GetValue(configid_bool_state_overlay_selectmode));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_overlay_dragselectmode_show_hidden,   ConfigManager::GetValue(configid_bool_state_overlay_dragselectmode_show_hidden));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_overlay_dragmode_temp,                ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_pen_simulation_supported,             ConfigManager::GetValue(configid_bool_state_pen_simulation_supported));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_window_focused_process_elevated,      ConfigManager::GetValue(configid_bool_state_window_focused_process_elevated));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_misc_process_elevated,                ConfigManager::GetValue(configid_bool_state_misc_process_elevated));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_misc_process_started_by_steam,        ConfigManager::GetValue(configid_bool_state_misc_process_started_by_steam));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_dplus_laser_pointer_device,            ConfigManager::GetValue(configid_int_state_dplus_laser_pointer_device));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_handle_state_dplus_laser_pointer_target_overlay, ConfigManager::GetValue(configid_handle_state_dplus_laser_pointer_target_overlay));
                    IPCManager::Get().PostConfigMessageToUIApp(configid_handle_state_theater_orig_overlay_handle,        ConfigManager::GetValue(configid_handle_state_theater_orig_overlay_handle));

                    //Sync usually means new UI process, so get new handles
                    m_LaserPointer.RefreshCachedOverlayHandles();
                    break;
                }
                case ipcact_focus_window:
                {
                    WindowManager::Get().RaiseAndFocusWindow((HWND)msg.lParam, &m_InputSim);
                    break;
                }
                case ipcact_keyboard_ovrl_focus_enter:
                {
                    //If a WinRT window capture is the focused overlay, check for window auto-focus so it's possible to type things
                    if (ConfigManager::GetValue(configid_bool_windows_winrt_auto_focus))
                    {
                        const bool drag_or_select_mode_enabled = ( (ConfigManager::GetValue(configid_bool_state_overlay_dragmode)) || (ConfigManager::GetValue(configid_bool_state_overlay_selectmode)) );

                        if (!drag_or_select_mode_enabled)
                        {
                            int focused_overlay_id = ConfigManager::Get().GetValue(configid_int_state_overlay_focused_id);

                            if (focused_overlay_id != -1)
                            {
                                const Overlay& overlay = OverlayManager::Get().GetOverlay((unsigned int)focused_overlay_id);
                                const OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)focused_overlay_id);

                                if ((overlay.GetTextureSource() == ovrl_texsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0))
                                {
                                    WindowManager::Get().RaiseAndFocusWindow((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd], &m_InputSim);
                                }
                            }
                        }
                    }

                    break;
                }
                case ipcact_keyboard_ovrl_focus_leave:
                {
                    //If leaving the keyboard while a WinRT window capture is the focused overlay and the option is enabled, focus the active scene app
                    if (ConfigManager::GetValue(configid_bool_windows_winrt_auto_focus_scene_app))
                    {
                        int focused_overlay_id = ConfigManager::Get().GetValue(configid_int_state_overlay_focused_id);

                        if (focused_overlay_id != -1)
                        {
                            const Overlay& overlay = OverlayManager::Get().GetOverlay((unsigned int)focused_overlay_id);
                            const OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)focused_overlay_id);

                            if ((overlay.GetTextureSource() == ovrl_texsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0))
                            {
                                WindowManager::Get().FocusActiveVRSceneApp(&m_InputSim);
                            }
                        }
                    }

                    break;
                }
                case ipcact_lpointer_trigger_haptics:
                {
                    m_LaserPointer.TriggerLaserPointerHaptics((vr::TrackedDeviceIndex_t)msg.lParam);
                    break;
                }
                case ipcact_lpointer_ui_mask_rect:
                {
                    if (msg.lParam == -1)
                    {
                        m_LaserPointer.UIIntersectionMaskFinish();
                    }
                    else
                    {
                        DPRect rect;
                        rect.Unpack16(msg.lParam);
                        m_LaserPointer.UIIntersectionMaskAddRect(rect);
                    }
                    break;
                }
                case ipcact_app_profile_remove:
                {
                    const bool loaded_overlay_profile = ConfigManager::Get().GetAppProfileManager().RemoveProfile(ConfigManager::GetValue(configid_str_state_app_profile_key));

                    if (loaded_overlay_profile)
                        ResetOverlays();

                    break;
                }
                case ipcact_global_shortcut_set:
                {
                    ActionManager::ActionList& global_shortcut_list = ConfigManager::Get().GetGlobalShortcuts();
                    int shortcut_id = msg.lParam;

                    if ((shortcut_id >= 0) && (shortcut_id < (int)global_shortcut_list.size()))
                    {
                        global_shortcut_list[shortcut_id] = ConfigManager::GetValue(configid_handle_state_action_uid);
                    }
                    break;
                }
                case ipcact_hotkey_set:
                {
                    ConfigHotkeyList& hotkey_list = ConfigManager::Get().GetHotkeys();
                    const std::string& hotkey_data = ConfigManager::GetValue(configid_str_state_hotkey_data);
                    int hotkey_id = msg.lParam;

                    if ((hotkey_id >= 0) && (hotkey_id < (int)hotkey_list.size()))
                    {
                        if (!hotkey_data.empty())
                        {
                            hotkey_list[hotkey_id].Deserialize(hotkey_data);
                        }
                        else
                        {
                            hotkey_list.erase(hotkey_list.begin() + msg.lParam);
                        }
                    }
                    else if (!hotkey_data.empty())
                    {
                        ConfigHotkey hotkey_new;
                        hotkey_new.Deserialize(hotkey_data);

                        hotkey_list.push_back(hotkey_new);
                    }

                    RegisterHotkeys();
                    break;
                }
            }
            break;
        }
        case ipcmsg_set_config:
        {
            if (msg.wParam < configid_bool_MAX)
            {
                ConfigID_Bool bool_id = (ConfigID_Bool)msg.wParam;

                bool previous_value = ConfigManager::GetValue(bool_id);
                ConfigManager::SetValue(bool_id, msg.lParam);

                switch (bool_id)
                {
                    case configid_bool_overlay_3D_enabled:
                    {
                        ApplySettingTransform();
                        ApplySetting3DMode();
                        break;
                    }
                    case configid_bool_overlay_3D_swapped:
                    {
                        ApplySetting3DMode();
                        break;
                    }
                    case configid_bool_overlay_origin_hmd_floor_use_turning:
                    {
                        const OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
                        OverlayOriginConfig origin_config = OverlayManager::Get().GetOriginConfigFromData(data);
                        OverlayOriginConfig origin_config_prev = origin_config;
                        origin_config_prev.HMDFloorUseTurning = previous_value;

                        OverlayOrigin origin = (OverlayOrigin)data.ConfigInt[configid_int_overlay_origin];

                        DetachedTransformConvertOrigin(OverlayManager::Get().GetCurrentOverlayID(), origin, origin, origin_config_prev, origin_config);
                        ApplySettingTransform();
                        break;
                    }
                    case configid_bool_overlay_enabled:
                    case configid_bool_overlay_gazefade_enabled:
                    case configid_bool_overlay_update_invisible:
                    {
                        ApplySettingTransform();
                        break;
                    }
                    case configid_bool_overlay_crop_enabled:
                    {
                        ApplySettingCrop();
                        ApplySettingTransform();
                        ApplySettingMouseScale();
                        break;
                    }
                    case configid_bool_overlay_winrt_window_matching_strict:
                    {
                        OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

                        //Check if new matching setting finds an existing window
                        if ((OverlayManager::Get().GetCurrentOverlay().GetTextureSource() == ovrl_texsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0))
                            break;

                        HWND window = WindowInfo::FindClosestWindowForTitle(data.ConfigStr[configid_str_overlay_winrt_last_window_title], data.ConfigStr[configid_str_overlay_winrt_last_window_class_name],
                                                                            data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name], WindowManager::Get().WindowListGet(),
                                                                            data.ConfigBool[configid_bool_overlay_winrt_window_matching_strict]);

                        if (window != nullptr)
                        {
                            data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] = (uint64_t)window;

                            if (ConfigManager::GetValue(configid_int_windows_winrt_capture_lost_behavior) == window_caplost_hide_overlay)
                            {
                                data.ConfigBool[configid_bool_overlay_enabled] = true;
                            }

                            OnSetOverlayWinRTCaptureWindow(OverlayManager::Get().GetCurrentOverlayID());

                            //Send to UI
                            IPCManager::Get().PostConfigMessageToUIApp(configid_handle_overlay_state_winrt_hwnd, (LPARAM)window);

                            if (ConfigManager::GetValue(configid_int_windows_winrt_capture_lost_behavior) == window_caplost_hide_overlay)
                            {
                                IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_enabled, true);
                            }
                        }
                        break;
                    }
                    case configid_bool_overlay_input_enabled:
                    case configid_bool_input_mouse_render_intersection_blob:
                    {
                        ApplySettingMouseInput();
                        break;
                    }
                    case configid_bool_input_mouse_allow_pointer_override:
                    {
                        //Reset Pointer override
                        if (m_MouseIgnoreMoveEvent)
                        {
                            m_MouseIgnoreMoveEvent = false;

                            ResetMouseLastLaserPointerPos();
                            ApplySettingMouseInput();
                        }
                        break;
                    }
                    case configid_bool_interface_dim_ui:
                    {
                        DimDashboard( ((m_OvrlDashboardActive) && (msg.lParam)) );
                        break;
                    }
                    case configid_bool_performance_single_desktop_mirroring:
                    {
                        if (msg.lParam) //Unify the desktop IDs when turning the setting on
                        {
                            CropToDisplay(OverlayManager::Get().GetConfigData(0).ConfigInt[configid_int_overlay_desktop_id], true);
                        }

                        reset_mirroring = true;
                        break;
                    }
                    case configid_bool_performance_hdr_mirroring:
                    {
                        reset_mirroring = true;
                        DPWinRT_SetHDREnabled(msg.lParam);
                        break;
                    }
                    case configid_bool_input_mouse_render_cursor:
                    {
                        m_OutputPendingFullRefresh = true;

                        if (DPWinRT_IsCaptureCursorEnabledPropertySupported())
                            DPWinRT_SetCaptureCursorEnabled(msg.lParam);

                        break;
                    }
                    case configid_bool_input_mouse_scroll_smooth:
                    {
                        ApplySettingMouseInput();
                        break;
                    }
                    case configid_bool_input_laser_pointer_block_input:
                    {
                        //Set SteamVR setting to allow global overlay input as we need it for this to work. Messing with user settings is not ideal, but we're not the first to do so
                        if ( (msg.lParam) && (!vr::VRSettings()->GetBool(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_AllowGlobalActionSetPriority)) )
                        {
                            vr::VRSettings()->SetBool(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_AllowGlobalActionSetPriority, true);
                        }
                        break;
                    }
                    case configid_bool_input_laser_pointer_hmd_device:
                    {
                        //Laser pointer keyboard input hotkeys should be disabled if this setting is off
                        RegisterHotkeys();
                        break;
                    }
                    case configid_bool_windows_winrt_keep_on_screen:
                    {
                        WindowManager::Get().UpdateConfigState();
                        break;
                    }
                    case configid_bool_browser_content_blocker:
                    {
                        DPBrowserAPIClient::Get().DPBrowser_ContentBlockSetEnabled(msg.lParam);
                        break;
                    }
                    case configid_bool_state_overlay_dragmode:
                    {
                        //Update temporary standing position if dragmode has been activated and dashboard tab isn't active
                        if ((msg.lParam) && (!m_OvrlDashboardActive))
                        {
                            m_OverlayDragger.UpdateTempStandingPosition();
                        }

                        ApplySettingInputMode();
                        break;
                    }
                    case configid_bool_state_overlay_selectmode:
                    {
                        ApplySettingInputMode();
                        break;
                    }
                    case configid_bool_state_misc_elevated_mode_active:
                    {
                        m_InputSim.SetElevatedModeForwardingActive(msg.lParam);
                        break;
                    }
                    default: break;
                }
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX)
            {
                ConfigID_Int int_id = (ConfigID_Int)(msg.wParam - configid_bool_MAX);

                int previous_value = ConfigManager::GetValue(int_id);
                ConfigManager::SetValue(int_id, msg.lParam);

                switch (int_id)
                {
                    case configid_int_interface_overlay_current_id:
                    {
                        OverlayManager::Get().SetCurrentOverlayID(msg.lParam);
                        current_overlay_old = (unsigned int)msg.lParam;
                        break;
                    }
                    case configid_int_interface_background_color:
                    case configid_int_interface_background_color_display_mode:
                    {
                        m_BackgroundOverlay.Update();
                        break;
                    }
                    case configid_int_overlay_desktop_id:
                    {
                        if (ConfigManager::GetValue(configid_bool_overlay_crop_enabled))
                        {
                            CropToDisplay(msg.lParam);
                        }
                        else //Don't touch cropping setting values if it's disabled and just update the validated crop rect instead
                        {
                            OverlayManager::Get().GetCurrentOverlay().UpdateValidatedCropRect();        
                            ApplySettingCrop();
                            ApplySettingTransform();
                            ApplySettingMouseScale();
                            ApplySettingExtraBrightness();
                        }

                        reset_mirroring = (ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring) && (msg.lParam != previous_value));
                        break;
                    }
                    case configid_int_overlay_capture_source:
                    {
                        ResetOverlayActiveCount();
                        ResetCurrentOverlay();
                        break;
                    }
                    case configid_int_overlay_winrt_desktop_id:
                    {
                        if (previous_value != msg.lParam)
                        {
                            OverlayManager::Get().GetCurrentOverlay().SetTextureSource(ovrl_texsource_none);
                            ResetCurrentOverlay();
                        }
                        break;
                    }
                    case configid_int_overlay_user_width:
                    case configid_int_overlay_user_height:
                    {
                        if (OverlayManager::Get().GetCurrentOverlay().GetTextureSource() == ovrl_texsource_browser)
                        {
                            const int user_width  = ConfigManager::GetValue(configid_int_overlay_user_width);
                            const int user_height = ConfigManager::GetValue(configid_int_overlay_user_height);

                            DPBrowserAPIClient::Get().DPBrowser_SetResolution(OverlayManager::Get().GetCurrentOverlay().GetHandle(), user_width, user_height);

                            //Also set as content width
                            ConfigManager::SetValue(configid_int_overlay_state_content_width,  user_width);
                            ConfigManager::SetValue(configid_int_overlay_state_content_height, user_height);

                            //Update crop as it depends on user size
                            if ((ConfigManager::GetValue(configid_bool_overlay_crop_enabled)) || (ConfigManager::GetValue(configid_bool_overlay_3D_enabled)))
                            {
                                ApplySettingCrop();
                            }

                            //Send to UI
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)OverlayManager::Get().GetCurrentOverlayID());
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_width,  user_width);
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_height, user_height);
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);

                            //Also do it for everything using this as duplication source
                            unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
                            for (unsigned int overlay_id : OverlayManager::Get().FindDuplicatedOverlaysForOverlay(OverlayManager::Get().GetCurrentOverlayID()))
                            {
                                OverlayManager::Get().SetCurrentOverlayID(overlay_id);

                                //Set config values for duplicated overlays as well so code only reading from it doesn't have to care about them being duplicated
                                ConfigManager::SetValue(configid_int_overlay_user_width,           user_width);
                                ConfigManager::SetValue(configid_int_overlay_user_height,          user_height);
                                ConfigManager::SetValue(configid_int_overlay_state_content_width,  user_width);
                                ConfigManager::SetValue(configid_int_overlay_state_content_height, user_height);

                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)overlay_id);
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_width,  user_width);
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_height, user_height);
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);

                                if (ConfigManager::GetValue(configid_bool_overlay_crop_enabled))
                                {
                                    ApplySettingCrop();
                                }
                            }
                            OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

                            ApplySettingMouseInput();
                        }
                        break;
                    }
                    case configid_int_overlay_crop_x:
                    case configid_int_overlay_crop_y:
                    case configid_int_overlay_crop_width:
                    case configid_int_overlay_crop_height:
                    {
                        ApplySettingCrop();
                        ApplySettingTransform();
                        ApplySettingMouseScale();
                        break;
                    }
                    case configid_int_overlay_3D_mode:
                    {
                        ApplySettingTransform();
                        ApplySetting3DMode();
                        break;
                    }
                    case configid_int_overlay_display_mode:
                    {
                        ApplySettingTransform();
                        break;
                    }
                    case configid_int_overlay_origin:
                    {
                        DetachedTransformConvertOrigin(OverlayManager::Get().GetCurrentOverlayID(), (OverlayOrigin)previous_value, (OverlayOrigin)msg.lParam);
                        ApplySettingTransform();
                        break;
                    }
                    case configid_int_overlay_browser_max_fps_override:
                    {
                        if (OverlayManager::Get().GetCurrentOverlay().GetTextureSource() == ovrl_texsource_browser)
                        {
                            DPBrowserAPIClient::Get().DPBrowser_SetFPS(OverlayManager::Get().GetCurrentOverlay().GetHandle(), msg.lParam);
                        }
                        break;
                    }
                    case configid_int_interface_wmr_ignore_vscreens:
                    {
                        DPWinRT_SetDesktopEnumerationFlags((msg.lParam == 1));
                        //May affect desktop enumeration, reset mirroring
                        reset_mirroring = true;
                        break;
                    }
                    case configid_int_input_mouse_dbl_click_assist_duration_ms:
                    {
                        ApplySettingMouseInput();
                        break;
                    }
                    case configid_int_input_laser_pointer_hmd_device_keycode_toggle:
                    case configid_int_input_laser_pointer_hmd_device_keycode_left:
                    case configid_int_input_laser_pointer_hmd_device_keycode_right:
                    case configid_int_input_laser_pointer_hmd_device_keycode_middle:
                    {
                        RegisterHotkeys();
                        break;
                    }
                    case configid_int_windows_winrt_dragging_mode:
                    {
                        WindowManager::Get().UpdateConfigState();
                        break;
                    }
                    case configid_int_performance_update_limit_mode:
                    case configid_int_performance_update_limit_fps:
                    case configid_int_overlay_update_limit_override_mode:
                    case configid_int_overlay_update_limit_override_fps:
                    {
                        ApplySettingUpdateLimiter();
                        break;
                    }
                    case configid_int_browser_max_fps:
                    {
                        DPBrowserAPIClient::Get().DPBrowser_GlobalSetFPS(msg.lParam);
                        break;
                    }
                    default: break;
                }
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX + configid_float_MAX)
            {
                ConfigID_Float float_id = (ConfigID_Float)(msg.wParam - configid_bool_MAX - configid_int_MAX);

                float value = pun_cast<float, LPARAM>(msg.lParam);
                float previous_value = ConfigManager::GetValue(float_id);
                ConfigManager::SetValue(float_id, value);

                switch (float_id)
                {
                    case configid_float_overlay_width:
                    case configid_float_overlay_curvature:
                    case configid_float_overlay_opacity:
                    case configid_float_overlay_brightness:
                    case configid_float_overlay_offset_right:
                    case configid_float_overlay_offset_up:
                    case configid_float_overlay_offset_forward:
                    {
                        ApplySettingTransform();
                        break;
                    }
                    case configid_float_overlay_browser_zoom:
                    {
                        if (OverlayManager::Get().GetCurrentOverlay().GetTextureSource() == ovrl_texsource_browser)
                        {
                            DPBrowserAPIClient::Get().DPBrowser_SetZoomLevel(OverlayManager::Get().GetCurrentOverlay().GetHandle(), value);
                        }
                        break;
                    }
                    case configid_float_performance_update_limit_ms:
                    case configid_float_overlay_update_limit_override_ms:
                    {
                        ApplySettingUpdateLimiter();
                        break;
                    }
                    default: break;
                }

            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX + configid_float_MAX + configid_handle_MAX)
            {
                ConfigID_Handle handle_id = (ConfigID_Handle)(msg.wParam - configid_bool_MAX - configid_int_MAX - configid_float_MAX);

                uint64_t value = pun_cast<uint64_t, LPARAM>(msg.lParam);
                uint64_t previous_value = ConfigManager::GetValue(handle_id);
                ConfigManager::SetValue(handle_id, value);

                switch (handle_id)
                {
                    case configid_handle_overlay_state_winrt_hwnd:
                    {
                        if (value != previous_value)
                        {
                            OnSetOverlayWinRTCaptureWindow(OverlayManager::Get().GetCurrentOverlayID());
                        }
                        break;
                    }
                }
            }

            break;
        }
    }

    //Restore overlay id override
    if (overlay_override_id != -1)
    {
        OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
    }

    return reset_mirroring;
}

void OutputManager::HandleWinRTMessage(const MSG& msg)
{
    switch (msg.message)
    {
        case WM_DPLUSWINRT_SIZE:
        {
            const unsigned int overlay_id = OverlayManager::Get().FindOverlayID(msg.wParam);

            if (overlay_id == k_ulOverlayID_None)
            {
                break;
            }

            const int content_width  = GET_X_LPARAM(msg.lParam);
            const int content_height = GET_Y_LPARAM(msg.lParam);

            const Overlay& overlay  = OverlayManager::Get().GetOverlay(overlay_id);
            OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

            //Skip if no real change
            if ((data.ConfigInt[configid_int_overlay_state_content_width] == content_width) && (data.ConfigInt[configid_int_overlay_state_content_height] == content_height))
            {
                break;
            }

            //Adaptive Size
            bool adaptive_size_apply = ( (ConfigManager::GetValue(configid_bool_windows_winrt_auto_size_overlay)) && (overlay.GetTextureSource() == ovrl_texsource_winrt_capture) && 
                                         (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0) && (data.ConfigInt[configid_int_overlay_state_content_width] != -1) && 
                                         (content_width != -1) );

            if (adaptive_size_apply)
            {
                data.ConfigFloat[configid_float_overlay_width] *= (float)content_width / data.ConfigInt[configid_int_overlay_state_content_width];
            }

            data.ConfigInt[configid_int_overlay_state_content_width]  = content_width;
            data.ConfigInt[configid_int_overlay_state_content_height] = content_height;

            //Send update to UI
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)overlay_id);

            IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_width,  content_width);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_content_height, content_height);

            if (adaptive_size_apply)
            {
                IPCManager::Get().PostConfigMessageToUIApp(configid_float_overlay_width, data.ConfigFloat[configid_float_overlay_width]);
            }

            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);

            //Apply change to overlay
            unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
            OverlayManager::Get().SetCurrentOverlayID(overlay_id);
            ApplySettingCrop();
            ApplySettingTransform();
            ApplySettingMouseScale();
            OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

            break;
        }
        case WM_DPLUSWINRT_CAPTURE_LOST:
        {
            const unsigned int overlay_id = OverlayManager::Get().FindOverlayID(msg.wParam);

            if (overlay_id == k_ulOverlayID_None)
            {
                break;
            }

            Overlay& overlay = OverlayManager::Get().GetOverlay(overlay_id);
            OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

            //Hide affected overlay if setting enabled
            if ( (data.ConfigBool[configid_bool_overlay_enabled]) && (ConfigManager::GetValue(configid_int_windows_winrt_capture_lost_behavior) == window_caplost_hide_overlay) )
            {
                unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
                OverlayManager::Get().SetCurrentOverlayID(overlay_id);
                data.ConfigBool[configid_bool_overlay_enabled] = false;
                ApplySettingTransform();
                OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

                //Send to UI
                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)overlay_id);
                IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_enabled, false);
                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
            }
            else if (ConfigManager::GetValue(configid_int_windows_winrt_capture_lost_behavior) == window_caplost_remove_overlay) //Or remove it
            {
                //Queue up removal instead of doing it right away in case there are multiple overlays with the same target lost at once (which breaks otherwise)
                m_RemoveOverlayQueue.push_back(overlay_id);
                break;
            }

            //Only change texture source if the overlay is still a winrt capture (this can be false when a picker gets canceled late)
            if ( (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) || (overlay.GetTextureSource() == ovrl_texsource_winrt_capture) )
            {
                overlay.SetTextureSource(ovrl_texsource_none);
            }

            break;
        }
        case WM_DPLUSWINRT_THREAD_ERROR:
        {
            //We get capture lost messages for each affected overlay, so just forward the error to the UI so a warning can be displayed for now
            IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_winrt_thread_error, msg.lParam);

            LOG_F(ERROR, "Unexpected error occurred in a Graphics Capture thread! (%#x)", (unsigned int)msg.lParam);
            break;
        }
        case WM_DPLUSWINRT_FPS:
        {
            const unsigned int overlay_id = OverlayManager::Get().FindOverlayID(msg.wParam);

            if (overlay_id == k_ulOverlayID_None)
            {
                break;
            }

            OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
            data.ConfigInt[configid_int_overlay_state_fps] = msg.lParam;

            //Send update to UI
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)overlay_id);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_state_fps, msg.lParam);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);

            break;
        }
    }
}

void OutputManager::HandleHotkeyMessage(const MSG& msg)
{
    ConfigHotkeyList& hotkey_list = ConfigManager::Get().GetHotkeys();
    int hotkey_id = msg.wParam;

    if ((hotkey_id >= 0) && (hotkey_id < (int)hotkey_list.size()))
    {
        ConfigHotkey& hotkey = hotkey_list[hotkey_id];

        //StateIsDown blocks HandleHotkeys() and the hotkey messages from triggering hotkey actions twice. It's reset in HandleHotkeys() when the key is no longer pressed
        if (!hotkey.StateIsDown)
        {
            ConfigManager::Get().GetActionManager().DoAction(hotkey.ActionUID);

            hotkey.StateIsDown = true;
        }
    }
}

void OutputManager::OnExit()
{
    //Release all held keyboard keys. Might be going overboard but better than keeping keys down unexpectedly
    for (int i = 0; i < 256; ++i) 
    {
        m_InputSim.KeyboardSetKeyState((IPCKeyboardKeystateFlags)0, i);
    }

    //Undo dimmed dashboard
    DimDashboard(false);

    //Shutdown VR for good
    vr::VR_Shutdown();
}

HWND OutputManager::GetWindowHandle()
{
    return m_WindowHandle;
}

//
// Returns shared handle
//
HANDLE OutputManager::GetSharedHandle()
{
    HANDLE Hnd = nullptr;

    // QI IDXGIResource interface to synchronized shared surface.
    IDXGIResource* DXGIResource = nullptr;
    HRESULT hr = m_SharedSurf->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(&DXGIResource));
    if (SUCCEEDED(hr))
    {
        // Obtain handle to IDXGIResource object.
        DXGIResource->GetSharedHandle(&Hnd);
        DXGIResource->Release();
        DXGIResource = nullptr;
    }

    return Hnd;
}

IDXGIAdapter* OutputManager::GetDXGIAdapter()
{
    HRESULT hr;

    // Get DXGI factory
    IDXGIDevice* DxgiDevice = nullptr;
    hr = m_Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
    if (FAILED(hr))
    {
        return nullptr;
    }

    IDXGIAdapter* DxgiAdapter = nullptr;
    hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
    DxgiDevice->Release();
    DxgiDevice = nullptr;
    if (FAILED(hr))
    {
        return nullptr;
    }

    return DxgiAdapter;
}

void OutputManager::ResetOverlays()
{
    //Check if process is elevated and send that info to the UI too (DPBrowser needs this info so do this first)
    bool elevated = IsProcessElevated();
    ConfigManager::SetValue(configid_bool_state_misc_process_elevated, elevated);
    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_misc_process_elevated, elevated);

    //Reset all overlays
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        ApplySettingCrop();
        ApplySettingTransform();
        ApplySettingCaptureSource();
        ApplySetting3DMode();
    }

    //Second pass for browser overlays using a duplication ID that is higher than the overlay's
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);
        const OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

        if ( (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser) && (data.ConfigInt[configid_int_overlay_duplication_id] != -1) )
        {
            ApplySettingCrop();
            ApplySettingCaptureSource();
            ApplySetting3DMode();
        }
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

    //These apply to all overlays within the function itself
    ApplySettingInputMode();
    ApplySettingUpdateLimiter();

    ResetOverlayActiveCount();

    //Post overlays reset message to UI app
    IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_overlays_reset);

    //Make sure that the entire overlay texture gets at least one full update for regions that will never be dirty (i.e. blank space not occupied by any desktop)
    m_OutputPendingFullRefresh = true;
}

void OutputManager::ResetCurrentOverlay()
{
    if ((OverlayManager::Get().GetCurrentOverlayID() == k_ulOverlayID_None) || (OverlayManager::Get().GetCurrentOverlay().GetID() == k_ulOverlayID_None))
        return;

    ApplySettingCrop();
    ApplySettingTransform();
    ApplySettingCaptureSource();
    ApplySettingInputMode();
    ApplySetting3DMode();

    ApplySettingUpdateLimiter();

    //Make sure that the entire overlay texture gets at least one full update for regions that will never be dirty (i.e. blank space not occupied by any desktop)
    if (ConfigManager::GetValue(configid_int_overlay_capture_source) == ovrl_capsource_desktop_duplication)
    {
        m_OutputPendingFullRefresh = true;
    }
}

ID3D11Texture2D* OutputManager::GetOverlayTexture() const
{
    return m_OvrlTex;
}

ID3D11Texture2D* OutputManager::GetMultiGPUTargetTexture() const
{
    return m_MultiGPUTexTarget;
}

vr::VROverlayHandle_t OutputManager::GetDesktopTextureOverlay() const
{
    return m_OvrlHandleDesktopTexture;
}

bool OutputManager::GetOverlayActive() const
{
    return (m_OvrlActiveCount != 0);
}

bool OutputManager::GetOverlayInputActive() const
{
    return m_OvrlInputActive;
}

DWORD OutputManager::GetMaxRefreshDelay() const
{
    if ( (m_OvrlActiveCount != 0) || (m_OvrlDashboardActive) || (m_LaserPointer.IsActive()) )
    {
        //Actually causes extreme load while not really being necessary (looks nice tho)
        if ( (m_OvrlInputActive) && (ConfigManager::GetValue(configid_bool_performance_rapid_laser_pointer_updates)) )
        {
            return 0;
        }
        else if (m_LaserPointer.IsScrolling())
        {
            //While scrolling with the Desktop+ Laser Pointer, we actually need to update frequently to keep scroll speeds and generated haptic feedback at the usual pace
            return 3;
        }
        else if (m_OvrlInputActive)
        {
            //While input is active, especially with the HMD pointer, we need to update more frequently to allow for smooth cursor movements
            return m_MaxActiveRefreshDelay / 2;
        }
        else
        {
            return m_MaxActiveRefreshDelay;
        }
    }
    else if ( (m_VRInput.IsAnyGlobalActionBound()) || (IsAnyOverlayUsingGazeFade()) || (m_IsAnyHotkeyActive) )
    {
        return m_MaxActiveRefreshDelay * 2;
    }
    else
    {
        return 300;
    }
}

float OutputManager::GetHMDFrameRate() const
{
    return vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
}

int OutputManager::GetDesktopWidth() const
{
    return m_DesktopWidth;
}

int OutputManager::GetDesktopHeight() const
{
    return m_DesktopHeight;
}

const std::vector<DPRect>& OutputManager::GetDesktopRects() const
{
    return m_DesktopRects;
}

float OutputManager::GetDesktopHDRWhiteLevelAdjustment(int desktop_id, bool is_for_graphics_capture, bool wmr_ignore_vscreens) const
{
    #ifdef DPLUS_DUP_NO_HDR
        return 1.0f;
    #else

    if (desktop_id == -1)
        desktop_id = 0;

    if ((!m_OutputHDRAvailable) && (!is_for_graphics_capture))
        return 1.0f;

    DXGI_OUTPUT_DESC output_desc = {};
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory_ptr;

    //This needs to go through DXGI as QueryDisplayConfig()'s order can be different
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory_ptr);
    if (!FAILED(hr))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr;
        UINT i = 0;
        int output_count = 0;

        while (factory_ptr->EnumAdapters(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND)
        {
            //Check if this a WMR virtual display adapter and skip it when the option is enabled
            if (wmr_ignore_vscreens)
            {
                DXGI_ADAPTER_DESC adapter_desc;
                adapter_ptr->GetDesc(&adapter_desc);

                if (wcscmp(adapter_desc.Description, L"Virtual Display Adapter") == 0)
                {
                    ++i;
                    continue;
                }
            }

            //Enum the available outputs
            Microsoft::WRL::ComPtr<IDXGIOutput> output_ptr;
            UINT output_index = 0;
            while (adapter_ptr->EnumOutputs(output_index, &output_ptr) != DXGI_ERROR_NOT_FOUND)
            {
                //Check if this happens to be the output we're looking for
                if (desktop_id == output_count)
                {
                    //Get output desc
                    output_ptr->GetDesc(&output_desc);
                }

                ++output_index;
                ++output_count;
            }

            ++i;
        }
    }

    //Find display config with the same device path
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    const UINT32 flags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE;
    LONG result = ERROR_SUCCESS;

    //Loop until buffer allocation for paths match the requirements
    do
    {
        UINT32 path_count, mode_count;
        result = ::GetDisplayConfigBufferSizes(flags, &path_count, &mode_count);

        if (result != ERROR_SUCCESS)
        {
            LOG_F(ERROR, "GetDisplayConfigBufferSizes() failed with %ld", result);
            return 1.0f;
        }

        paths.resize(path_count);
        modes.resize(mode_count);

        result = ::QueryDisplayConfig(flags, &path_count, paths.data(), &mode_count, modes.data(), nullptr);

        paths.resize(path_count);
        modes.resize(mode_count);
    } 
    while (result == ERROR_INSUFFICIENT_BUFFER);

    if (result != ERROR_SUCCESS)
    {
        LOG_F(ERROR, "QueryDisplayConfig() failed with %ld", result);
        return 1.0f;
    }

    //Check each active path
    for (auto& path : paths)
    {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name = {};
        source_name.header.adapterId = path.sourceInfo.adapterId;
        source_name.header.id = path.sourceInfo.id;
        source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source_name.header.size = sizeof(source_name);

        result = ::DisplayConfigGetDeviceInfo(&source_name.header);

        if (result == ERROR_SUCCESS)
        {
            if (wcscmp(source_name.viewGdiDeviceName, output_desc.DeviceName) == 0)
            {
                //Found the right display config path, time to grab the data
                bool is_hdr_enabled = false;
                bool is_8bit = true;
                ULONG sdr_white_level = 1000;

                #if (NTDDI_VERSION >= 0x0A00000F/*NTDDI_WIN11_GA*/)
                    DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2 adv_color_info_2 = {};
                    adv_color_info_2.header.adapterId = path.targetInfo.adapterId;
                    adv_color_info_2.header.id = path.targetInfo.id;
                    adv_color_info_2.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2;
                    adv_color_info_2.header.size = sizeof(adv_color_info_2);

                    result = ::DisplayConfigGetDeviceInfo(&adv_color_info_2.header);

                    if (result == ERROR_SUCCESS)
                    {
                        is_8bit = (adv_color_info_2.bitsPerColorChannel == 8);
                        //DISPLAYCONFIG_ADVANCED_COLOR_MODE_WCG is still higher bit-depth but seems like it needs to be handled differently
                        is_hdr_enabled = (adv_color_info_2.activeColorMode == DISPLAYCONFIG_ADVANCED_COLOR_MODE_HDR);
                    }

                    if (is_hdr_enabled)
                    {
                        DISPLAYCONFIG_SDR_WHITE_LEVEL config_sdr_white_level = {};
                        config_sdr_white_level.header.adapterId = path.targetInfo.adapterId;
                        config_sdr_white_level.header.id = path.targetInfo.id;
                        config_sdr_white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
                        config_sdr_white_level.header.size = sizeof(sdr_white_level);

                        result = ::DisplayConfigGetDeviceInfo(&config_sdr_white_level.header);

                        if (result == ERROR_SUCCESS)
                        {
                            sdr_white_level = config_sdr_white_level.SDRWhiteLevel;
                        }
                    }
                #endif

                DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO adv_color_info = {};
                adv_color_info.header.adapterId = path.targetInfo.adapterId;
                adv_color_info.header.id = path.targetInfo.id;
                adv_color_info.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
                adv_color_info.header.size = sizeof(adv_color_info);

                result = ::DisplayConfigGetDeviceInfo(&adv_color_info.header);

                if (result == ERROR_SUCCESS)
                {
                    is_8bit = (adv_color_info.bitsPerColorChannel == 8);
                }

                //The following is based on potentially incomplete observations and doesn't appear to be documented anywhere otherwise
                //Checking different OS versions without access to a HDR display in a VM makes things a little bit messy... so this likely needs to be fixed up later
                if (is_8bit)
                {
                    //This the easiest to check and has been observed across several Windows 10 and 11 versions, why it's like this I don't know
                    return (is_for_graphics_capture) ? 0.5f : 1.0f;
                }
                else if (is_hdr_enabled)
                {
                    //Observed on Windows 11 24H2, but not on Windows 10 (DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2 doesn't exist there so it won't hit this path)
                    return 1000.0f / sdr_white_level;
                }
                else
                {
                    //Observed on Windows 10 20H2, but potentially always applies to DISPLAYCONFIG_ADVANCED_COLOR_MODE_WCG in general and DISPLAYCONFIG_ADVANCED_COLOR_MODE_HDR doesn't exist there?
                    //However, also observed displays set to HDR get non-linear pixel data written by Desktop Duplication on there (Graphics Capture and Desktop Duplication non-HDR display pixels are correct)
                    //Might have some unknown factor causing it, even if fixable with extra steps in theory... so it is what it is for now.
                    return (is_for_graphics_capture) ? 0.5f : 1.0f;
                }
            }
        }
    }

    LOG_F(WARNING, "Could not find display config for desktop %d, defaulting to 100%% brightness adjustment", desktop_id);
    return 1.0f;

    #endif //DPLUS_DUP_NO_HDR
}

void OutputManager::ShowOverlay(unsigned int id)
{
    Overlay& overlay = OverlayManager::Get().GetOverlay(id);

    if (overlay.IsVisible()) //Already visible? Abort.
    {
        return;
    }

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    OverlayManager::Get().SetCurrentOverlayID(id);
    vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();
    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(id);

    if (m_OvrlActiveCount == 0) //First overlay to become active
    {
        ::timeBeginPeriod(1);   //This is somewhat frowned upon, but we want to hit the polling rate, it's only when active and we're in a high performance situation anyways

        //Set last pointer values to current to not trip the movement detection up
        ResetMouseLastLaserPointerPos();
        m_MouseIgnoreMoveEvent = false;

        WindowManager::Get().SetOverlayActive(true);
    }

    m_OvrlActiveCount++;

    if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication)
    {
        if (m_OvrlDesktopDuplActiveCount == 0) //First Desktop Duplication overlay to become active
        {
            //Signal duplication threads to resume in case they're paused
            ::ResetEvent(m_PauseDuplicationEvent);
            ::SetEvent(m_ResumeDuplicationEvent);

            ForceScreenRefresh();
        }

        m_OvrlDesktopDuplActiveCount++;
    }
    else if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture)
    {
        //Unpause capture
        DPWinRT_PauseCapture(ovrl_handle, false);
    }
    else if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser)
    {
        //Don't call this before texture source is set (browser process may not be running), ApplySettingCaptureSource() will call it in that case
        if (overlay.GetTextureSource() == ovrl_texsource_browser)
        {
            //Unpause browser
            DPBrowserAPIClient::Get().DPBrowser_PauseBrowser(overlay.GetHandle(), false);
        }
    }

    overlay.SetVisible(true);

    ApplySettingTransform();

    //Overlay could affect update limiter, so apply setting
    if (data.ConfigInt[configid_int_overlay_update_limit_override_mode] != update_limit_mode_off)
    {
        ApplySettingUpdateLimiter();
    }

    //If the last clipping rect doesn't fully contain the overlay's crop rect, the desktop texture overlay is probably outdated there, so force a full refresh
    if ( (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication) && (!m_OutputLastClippingRect.Contains(overlay.GetValidatedCropRect())) )
    {
        RefreshOpenVROverlayTexture(DPRect(-1, -1, -1, -1), true);
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::ShowTheaterOverlay(unsigned int id)
{
    if (OverlayManager::Get().GetTheaterOverlayID() == id)
        return;

    //Don't set theater overlay before texture source is initialized 
    if (OverlayManager::Get().GetOverlay(id).GetTextureSource() == ovrl_texsource_invalid)
        return;

    OverlayManager::Get().SetTheaterOverlayID(id);

    //Check every other existing overlay for theater origin and disable them (only one theater overlay can be active)
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        Overlay& overlay = OverlayManager::Get().GetOverlay(i);
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

        if ((data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_theater_screen) && (i != id))
        {
            SetOverlayEnabled(i, false);
        }
    }

    //Reset overlay so the theater overlay has every property applied
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    OverlayManager::Get().SetCurrentOverlayID(id);
    ResetCurrentOverlay();
    OverlayManager::Get().GetCurrentOverlay().AssignDesktopDuplicationTexture();    //Desktop Duplication texture isn't reset and only assigned on change, so do it manually
    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::HideOverlay(unsigned int id)
{
    Overlay& overlay = OverlayManager::Get().GetOverlay(id);

    if (!overlay.IsVisible()) //Already hidden? Abort.
    {
        return;
    }

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    OverlayManager::Get().SetCurrentOverlayID(id);
    vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();
    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(id);

    overlay.SetVisible(false);

    //Overlay could've affected update limiter, so apply setting
    if (data.ConfigInt[configid_int_overlay_update_limit_override_mode] != update_limit_mode_off)
    {
        ApplySettingUpdateLimiter();
    }

    m_OvrlActiveCount--;

    if (m_OvrlActiveCount == 0) //Last overlay to become inactive
    {
        ::timeEndPeriod(1);
        WindowManager::Get().SetOverlayActive(false);
    }

    if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication)
    {
        m_OvrlDesktopDuplActiveCount--;

        if (m_OvrlDesktopDuplActiveCount == 0) //Last Desktop Duplication overlay to become inactive
        {
            //Signal duplication threads to pause since we don't need them to do needless work
            ::ResetEvent(m_ResumeDuplicationEvent);
            ::SetEvent(m_PauseDuplicationEvent);
        }
    }
    else if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture)
    {
        //Pause capture
        DPWinRT_PauseCapture(ovrl_handle, true);
    }
    else if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_browser)
    {
        //Don't call this before texture source is set (browser process may not be running), ApplySettingCaptureSource() will call it in that case
        if (overlay.GetTextureSource() == ovrl_texsource_browser)
        {
            //Pause browser
            DPBrowserAPIClient::Get().DPBrowser_PauseBrowser(overlay.GetHandle(), true);
        }
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::ResetOverlayActiveCount()
{
    bool desktop_duplication_was_paused = (m_OvrlDesktopDuplActiveCount == 0);

    m_OvrlActiveCount = 0;
    m_OvrlDesktopDuplActiveCount = 0;

    //Check every existing overlay for visibility and count them as active
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        Overlay& overlay = OverlayManager::Get().GetOverlay(i);
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

        if (overlay.IsVisible())
        {
            m_OvrlActiveCount++;

            if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication)
            {
                m_OvrlDesktopDuplActiveCount++;
            }
        }
    }

    //Fixup desktop duplication state
    if ( (desktop_duplication_was_paused) && (m_OvrlDesktopDuplActiveCount > 0) )
    {
        //Signal duplication threads to resume
        ::ResetEvent(m_PauseDuplicationEvent);
        ::SetEvent(m_ResumeDuplicationEvent);

        ForceScreenRefresh();
    }
    else if ( (!desktop_duplication_was_paused) && (m_OvrlDesktopDuplActiveCount == 0) )
    {
        //Signal duplication threads to pause
        ::ResetEvent(m_ResumeDuplicationEvent);
        ::SetEvent(m_PauseDuplicationEvent);
    }

    //Fixup WindowManager state
    WindowManager::Get().SetOverlayActive( (m_OvrlActiveCount > 0) );
}

bool OutputManager::HasDashboardBeenActivatedOnce() const
{
    return m_DashboardActivatedOnce;
}

bool OutputManager::IsDashboardTabActive() const
{
    return m_OvrlDashboardActive;
}

float OutputManager::GetDashboardScale() const
{
    vr::HmdMatrix34_t matrix = {0};
    vr::VROverlay()->GetTransformForOverlayCoordinates(m_OvrlHandleDashboardDummy, vr::TrackingUniverseStanding, {0.5f, 0.5f}, &matrix);
    Vector3 row_1(matrix.m[0][0], matrix.m[1][0], matrix.m[2][0]);

    return row_1.length(); //Scaling is always uniform so we just check the x-axis
}

float OutputManager::GetOverlayHeight(unsigned int overlay_id) const
{
    const Overlay& overlay        = OverlayManager::Get().GetOverlay(overlay_id);
    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

    const DPRect& crop_rect = overlay.GetValidatedCropRect();
    int crop_width = crop_rect.GetWidth(), crop_height = crop_rect.GetHeight();

    bool is_3d_enabled = data.ConfigBool[configid_bool_overlay_3D_enabled];
    int mode_3d = data.ConfigInt[configid_int_overlay_3D_mode];

    if ( (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication) && (m_OutputInvalid) ) //No cropping on invalid output image
    {
        crop_width  = k_lOverlayOutputErrorTextureWidth;
        crop_height = k_lOverlayOutputErrorTextureHeight;
        is_3d_enabled = false;
    }
    else if ( (overlay.GetTextureSource() == ovrl_texsource_none) || (crop_width <= 0) || (crop_height <= 0) )
    {
        //Get dimensions from mouse scale if possible 
        vr::HmdVector2_t mouse_scale;
        if (vr::VROverlay()->GetOverlayMouseScale(overlay.GetHandle(), &mouse_scale) == vr::VROverlayError_None)
        {
            crop_width  = mouse_scale.v[0];
            crop_height = mouse_scale.v[1];
        }
        else
        {
            crop_width  = k_lOverlayOutputErrorTextureWidth;
            crop_height = k_lOverlayOutputErrorTextureHeight;
        }

        is_3d_enabled = false;
    }
    else if ( (is_3d_enabled) && (mode_3d == ovrl_3Dmode_ou) ) //Converted Over-Under changes texture dimensions, so adapt
    {
        crop_width  *= 2;
        crop_height /= 2;
    }

    //Overlay is twice as tall when SBS3D/OU3D is active
    if ( (is_3d_enabled) && ( (mode_3d == ovrl_3Dmode_sbs) || (mode_3d == ovrl_3Dmode_ou) ) )
        crop_height *= 2;


    return data.ConfigFloat[configid_float_overlay_width] * ((float)crop_height / crop_width);
}

Matrix4 OutputManager::GetFallbackOverlayTransform() const
{
    Matrix4 transform;

    //Get HMD pose
    vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, vr::IVRSystemEx::GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

    if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
    {
        //Set to HMD position and offset 2m away
        Matrix4 mat_hmd(poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
        transform = mat_hmd;
        transform.translate_relative(0.0f, 0.0f, -2.0f);

        //Rotate towards HMD position
        vr::IVRSystemEx::TransformLookAt(transform, mat_hmd.getTranslation());
    }

    return transform;
}

void OutputManager::SetOutputErrorTexture(vr::VROverlayHandle_t overlay_handle)
{
    vr::EVROverlayError vr_error = vr::VROverlayEx()->SetOverlayFromFileEx(overlay_handle, (ConfigManager::Get().GetApplicationPath() + "images/output_error.png").c_str());    

    vr::VRTextureBounds_t tex_bounds = {0.0f};
    tex_bounds.uMax = 1.0f;
    tex_bounds.vMax = 1.0f;

    vr::VROverlay()->SetOverlayTextureBounds(overlay_handle, &tex_bounds);

    //Make sure to remove 3D on the overlay too
    vr::VROverlay()->SetOverlayFlag(overlay_handle, vr::VROverlayFlags_SideBySide_Parallel, false);
    vr::VROverlay()->SetOverlayFlag(overlay_handle, vr::VROverlayFlags_SideBySide_Crossed,  false);
    vr::VROverlay()->SetOverlayTexelAspect(overlay_handle, 1.0f);

    //Mouse scale needs to be updated as well
    ApplySettingMouseInput();
}

void OutputManager::SetOutputInvalid()
{
    LOG_F(WARNING, "No outputs available!");

    m_OutputInvalid = true;
    SetOutputErrorTexture(m_OvrlHandleDesktopTexture);
    m_DesktopWidth  = k_lOverlayOutputErrorTextureWidth;
    m_DesktopHeight = k_lOverlayOutputErrorTextureHeight;

    ResetOverlays();
}

bool OutputManager::IsOutputInvalid() const
{
    return m_OutputInvalid;
}

void OutputManager::SetOverlayEnabled(unsigned int overlay_id, bool is_enabled)
{
    OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

    if (data.ConfigBool[configid_bool_overlay_enabled] == is_enabled)
        return;

    data.ConfigBool[configid_bool_overlay_enabled] = is_enabled;

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    OverlayManager::Get().SetCurrentOverlayID(overlay_id);
    ApplySettingTransform();
    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

    //Sync change
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, overlay_id);
    IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_enabled, data.ConfigBool[configid_bool_overlay_enabled]);
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
}

void OutputManager::CropToActiveWindowToggle(unsigned int overlay_id)
{
    //If the action is used with one of the controller buttons, the events will fire another time if the new cropping values happen to have the laser pointer leave and
    //re-enter the overlay for a split second while the button is still down during the dimension change. 
    //This would immediately undo the action, which we want to prevent, so a 100 ms pause between toggles is enforced 
    //-Currently not actually enforcing this with the new action commands to see where this goes-
    static ULONGLONG last_toggle_tick = 0;

    if ((overlay_id == k_ulOverlayID_None) /*|| (::GetTickCount64() <= last_toggle_tick + 100)*/)
        return;

    last_toggle_tick = ::GetTickCount64();

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    OverlayManager::Get().SetCurrentOverlayID(overlay_id);

    bool& crop_enabled = ConfigManager::GetRef(configid_bool_overlay_crop_enabled);
    int& crop_x        = ConfigManager::GetRef(configid_int_overlay_crop_x);
    int& crop_y        = ConfigManager::GetRef(configid_int_overlay_crop_y);
    int& crop_width    = ConfigManager::GetRef(configid_int_overlay_crop_width);
    int& crop_height   = ConfigManager::GetRef(configid_int_overlay_crop_height);

    //Check if crop is just exactly the current desktop
    bool crop_equals_current_desktop = false;
    const int desktop_id = ConfigManager::GetValue(configid_int_overlay_desktop_id);

    if ( (desktop_id >= 0) && (desktop_id < m_DesktopRects.size()) )
    {
        DPRect crop_rect(crop_x, crop_y, crop_x + crop_width, crop_y + crop_height);

        crop_equals_current_desktop = (crop_rect == m_DesktopRects[desktop_id]);
    }

    //Check if crop already matches the active window
    bool crop_equals_new_window_crop = false;
    int crop_x_new      = crop_x;
    int crop_y_new      = crop_y;
    int crop_width_new  = crop_width;
    int crop_height_new = crop_height;

    if (CropToActiveWindow(crop_x_new, crop_y_new, crop_width_new, crop_height_new))
    {
        crop_equals_new_window_crop = ((crop_x == crop_x_new) && (crop_y == crop_y_new) && (crop_width == crop_width_new) && (crop_height == crop_height_new));
    }

    //If uncropped or different, crop to active window
    if ( (!crop_enabled) || (!crop_equals_new_window_crop) )
    {
        CropToActiveWindow();
    }
    else //Otherwise, disable cropping (leaving the existing cropping values around)
    {
        crop_enabled = false;
        IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_crop_enabled, crop_enabled);

        ApplySettingCrop();
        ApplySettingTransform();
        ApplySettingMouseScale();
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::ShowWindowSwitcher()
{
    InitComIfNeeded();

    Microsoft::WRL::ComPtr<IShellDispatch5> shell_dispatch;
    HRESULT sc = ::CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_SERVER, IID_IDispatch, &shell_dispatch);

    if (SUCCEEDED(sc))
    {
        shell_dispatch->WindowSwitcher();
    }
}

void OutputManager::SwitchToWindow(HWND window, bool warp_cursor)
{
    WindowManager::Get().RaiseAndFocusWindow(window, &m_InputSim);

    if (warp_cursor)
    {
        //Figure out center point of the window and put the cursor there
        RECT window_rect = {0};

        //Just using GetWindowRect() can include shadows and such, which we don't want
        if (::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect, sizeof(window_rect)) == S_OK)
        {
            DPRect window_dprect(window_rect.left, window_rect.top, window_rect.right, window_rect.bottom);
            Vector2Int pt_center = window_dprect.GetCenter();

            m_InputSim.MouseMove(pt_center.x, pt_center.y);
        }
    }
}

void OutputManager::OverlayDirectDragStart(unsigned int overlay_id)
{
    if (overlay_id == k_ulOverlayID_None)
        return;

    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

    if (m_OverlayDragger.GetDragDeviceID() == -1)
    {
        if (data.ConfigInt[configid_int_overlay_origin] != ovrl_origin_theater_screen)
        {
            if (!data.ConfigBool[configid_bool_overlay_transform_locked])
            {
                m_OverlayDragger.DragStart(overlay_id);

                if (m_OverlayDragger.IsDragActive())
                {
                    m_OvrlDirectDragActive = true;
                    ApplySettingInputMode();

                    if (m_LaserPointer.IsActive())
                    {
                        m_LaserPointer.ForceTargetOverlay(OverlayManager::Get().GetOverlay(overlay_id).GetHandle());
                    }
                }
            }
            else
            {
                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 1);
            }
        }
        else
        {
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 2);
        }
    }
}

void OutputManager::OverlayDirectDragFinish(unsigned int overlay_id)
{
    if (overlay_id == k_ulOverlayID_None)
        return;

    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);

    if ((m_OverlayDragger.GetDragOverlayID() == overlay_id) && (m_OverlayDragger.IsDragActive()))
    {
        OnDragFinish();
        m_OverlayDragger.DragFinish();

        ApplySettingTransform();
    }

    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 0);
}

VRInput& OutputManager::GetVRInput()
{
    return m_VRInput;
}

InputSimulator& OutputManager::GetInputSimulator()
{
    return m_InputSim;
}

void OutputManager::UpdatePerformanceStates()
{
    //Frame counter, the frames themselves are counted in Update()
    if (::GetTickCount64() >= m_PerformanceFrameCountStartTick + 1000)
    {
        //A second has passed, reset the value
        ConfigManager::SetValue(configid_int_state_performance_duplication_fps, m_PerformanceFrameCount);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_performance_duplication_fps, m_PerformanceFrameCount);

        m_PerformanceFrameCountStartTick = ::GetTickCount64();
        m_PerformanceFrameCount = 0;
    }
}

const LARGE_INTEGER& OutputManager::GetUpdateLimiterDelay()
{
    return m_PerformanceUpdateLimiterDelay;
}

int OutputManager::EnumerateOutputs(int target_desktop_id, Microsoft::WRL::ComPtr<IDXGIAdapter>* out_adapter_preferred, Microsoft::WRL::ComPtr<IDXGIAdapter>* out_adapter_vr)
{
    LOG_SCOPE_F(INFO, "Detected Outputs");

    Microsoft::WRL::ComPtr<IDXGIFactory1> factory_ptr;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr_preferred;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr_vr;
    int output_id_adapter = target_desktop_id;           //Output ID on the adapter actually used. Only different from initial SingleOutput if there's desktops across multiple GPUs

    m_DesktopRects.clear();
    m_DesktopRectTotal = DPRect();   //Figure out right dimensions for full size desktop rect (this is also done in CreateTextures() but for Desktop Duplication only)
    m_DesktopHDRWhiteLevelAdjustments.clear();

    const bool is_hdr_in_use = ((m_OutputHDRAvailable) && (ConfigManager::GetValue(configid_bool_performance_hdr_mirroring)));

    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory_ptr);
    if (!FAILED(hr))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr;
        UINT i = 0;
        int output_count = 0;
        bool wmr_ignore_vscreens = (ConfigManager::GetValue(configid_int_interface_wmr_ignore_vscreens) == 1);

        //Also look for the device the HMD is connected to
        int32_t vr_gpu_id;
        vr::VRSystem()->GetDXGIOutputInfo(&vr_gpu_id);

        while (factory_ptr->EnumAdapters(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND)
        {
            DXGI_ADAPTER_DESC adapter_desc;
            adapter_ptr->GetDesc(&adapter_desc);

            int first_output_adapter = output_count;

            if (i == vr_gpu_id)
            {
                adapter_ptr_vr = adapter_ptr;
            }

            //Check if this a WMR virtual display adapter and skip it when the option is enabled
            //This still only works correctly when they have the last desktops in the system, but that should pretty much be always the case
            if (wmr_ignore_vscreens)
            {
                if (wcscmp(adapter_desc.Description, L"Virtual Display Adapter") == 0)
                {
                    LOG_F(INFO, "Skipping \"Virtual Display Adapter\"");
                    ++i;
                    continue;
                }
            }

            //Count the available outputs
            Microsoft::WRL::ComPtr<IDXGIOutput> output_ptr;

            //Check if there are gonna be any outputs before logging the GPU to avoid confusion (there may be multiple adapters per GPU with no outputs attached)
            if (adapter_ptr->EnumOutputs(0, &output_ptr) == DXGI_ERROR_NOT_FOUND)
            {
                ++i;
                continue;
            }

            LOG_SCOPE_F(INFO, "GPU %u: %s (Device ID %u)", i + 1, StringConvertFromUTF16(adapter_desc.Description).c_str(), adapter_desc.DeviceId);

            UINT output_index = 0;
            while (adapter_ptr->EnumOutputs(output_index, &output_ptr) != DXGI_ERROR_NOT_FOUND)
            {
                //Check if this happens to be the output we're looking for (or for combined desktop, set the first adapter with available output)
                if ( (adapter_ptr_preferred == nullptr) && ( (target_desktop_id == output_count) || (target_desktop_id == -1) ) )
                {
                    adapter_ptr_preferred = adapter_ptr;

                    if (target_desktop_id != -1)
                    {
                        output_id_adapter = output_index;
                    }
                }

                //Cache rect of the output
                DXGI_OUTPUT_DESC output_desc;
                output_ptr->GetDesc(&output_desc);
                m_DesktopRects.emplace_back(output_desc.DesktopCoordinates.left,  output_desc.DesktopCoordinates.top, 
                                            output_desc.DesktopCoordinates.right, output_desc.DesktopCoordinates.bottom);

                (m_DesktopRectTotal.GetWidth() == 0) ? m_DesktopRectTotal = m_DesktopRects.back() : m_DesktopRectTotal.Add(m_DesktopRects.back());


                //Cache HDR white level adjustment
                const float white_level_adjust = GetDesktopHDRWhiteLevelAdjustment(output_count, false, wmr_ignore_vscreens);
                m_DesktopHDRWhiteLevelAdjustments.push_back(white_level_adjust);

                //Log display info, with white level adjustment if HDR is actually on
                LOG_IF_F(INFO, !is_hdr_in_use, "Desktop %u: %4d,%4d | %4dx%4d (%s)", output_count + 1, 
                         output_desc.DesktopCoordinates.left, output_desc.DesktopCoordinates.top,
                         output_desc.DesktopCoordinates.right - output_desc.DesktopCoordinates.left, output_desc.DesktopCoordinates.bottom - output_desc.DesktopCoordinates.top,
                         StringConvertFromUTF16(output_desc.DeviceName).c_str());

                LOG_IF_F(INFO, is_hdr_in_use, "Desktop %u: %4d,%4d | %4dx%4d (%s) | %.2fx SDR Brightness", output_count + 1, 
                         output_desc.DesktopCoordinates.left, output_desc.DesktopCoordinates.top,
                         output_desc.DesktopCoordinates.right - output_desc.DesktopCoordinates.left, output_desc.DesktopCoordinates.bottom - output_desc.DesktopCoordinates.top,
                         StringConvertFromUTF16(output_desc.DeviceName).c_str(), 1.0f / white_level_adjust);


                ++output_count;
                ++output_index;
            }

            ++i;
        }

        //Store output/desktop count and send it over to UI
        ConfigManager::SetValue(configid_int_state_interface_desktop_count, output_count);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_interface_desktop_count, output_count);
    }

    if (out_adapter_preferred != nullptr)
    {
        *out_adapter_preferred = adapter_ptr_preferred;
    }

    if (out_adapter_vr != nullptr)
    {
        *out_adapter_vr = adapter_ptr_vr;
    }

    m_InputSim.RefreshScreenOffsets();
    ResetMouseLastLaserPointerPos();

    return output_id_adapter;
}

void OutputManager::CropToDisplay(int display_id, int& crop_x, int& crop_y, int& crop_width, int& crop_height)
{   
    if ( (!ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring)) && (display_id >= 0) && (display_id < m_DesktopRects.size()) ) 
    {
        //Individual desktop on full desktop texture
        const DPRect& rect = m_DesktopRects[display_id];

        crop_x      = rect.GetTL().x;
        crop_y      = rect.GetTL().y;
        crop_width  = rect.GetWidth();
        crop_height = rect.GetHeight();

        //Offset by desktop coordinates
        crop_x      -= m_DesktopX;
        crop_y      -= m_DesktopY;
    }
    else //Full desktop
    {
        crop_x      =  0;
        crop_y      =  0;
        crop_width  = -1;
        crop_height = -1;
    }
}

bool OutputManager::CropToActiveWindow(int& crop_x, int& crop_y, int& crop_width, int& crop_height)
{
    HWND window_handle = ::GetForegroundWindow();

    if (window_handle != nullptr)
    {
        RECT window_rect = {0};

        //Just using GetWindowRect() can include shadows and such, which we don't want
        if (::DwmGetWindowAttribute(window_handle, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect, sizeof(window_rect)) == S_OK)
        {
            DPRect crop_rect(window_rect.left, window_rect.top, window_rect.right, window_rect.bottom);

            crop_rect.Translate({-m_DesktopX, -m_DesktopY});                    //Translate crop rect by desktop offset to get desktop-local coordinates
            crop_rect.ClipWithFull({0, 0, m_DesktopWidth, m_DesktopHeight});    //Clip to available desktop space

            if ((crop_rect.GetWidth() > 0) && (crop_rect.GetHeight() > 0))
            {
                //Set new crop values
                crop_x      = crop_rect.GetTL().x;
                crop_y      = crop_rect.GetTL().y;
                crop_width  = crop_rect.GetWidth();
                crop_height = crop_rect.GetHeight();

                return true;
            }
        }
    }

    return false;
}

void OutputManager::InitComIfNeeded()
{
    if (!m_ComInitDone)
    {
        if (::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) != RPC_E_CHANGED_MODE)
        {
            m_ComInitDone = true;
        }
    }
}

void OutputManager::ConvertOUtoSBS(Overlay& overlay, OUtoSBSConverter& converter)
{
    //Convert()'s arguments are almost all stuff from OutputManager, so we take this roundabout way of calling it
    const DPRect& crop_rect = overlay.GetValidatedCropRect();

    HRESULT hr = converter.Convert(m_Device, m_DeviceContext, m_MultiGPUTargetDevice, m_MultiGPUTargetDeviceContext, m_OvrlTex,
                                   m_DesktopWidth, m_DesktopHeight, crop_rect.GetTL().x, crop_rect.GetTL().y, crop_rect.GetWidth(), crop_rect.GetHeight());

    if (hr == S_OK)
    {
        vr::Texture_t vrtex;
        vrtex.eType = vr::TextureType_DirectX;
        vrtex.eColorSpace = vr::ColorSpace_Gamma;
        vrtex.handle = converter.GetTexture(); //OUtoSBSConverter takes care of multi-gpu support automatically, so no further processing needed

        vr::VROverlay()->SetOverlayTexture(overlay.GetHandle(), &vrtex);
    }
    else
    {
        ProcessFailure(m_Device, L"Failed to convert OU texture to SBS", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }
}


//
// Process both masked and monochrome pointers
//
DUPL_RETURN OutputManager::ProcessMonoMask(bool is_mono, PTR_INFO& ptr_info, int& ptr_width, int& ptr_height, int& ptr_left, int& ptr_top, 
                                           Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_tex, DXGI_FORMAT& out_tex_format, D3D11_BOX& box)
{
    out_tex_format = DXGI_FORMAT_UNKNOWN;

    //PtrShapeBuffer can sometimes be nullptr when the secure desktop is active, skip
    if (ptr_info.PtrShapeBuffer == nullptr)
        return DUPL_RETURN_SUCCESS;

    //Desktop dimensions
    D3D11_TEXTURE2D_DESC FullDesc;
    m_SharedSurf->GetDesc(&FullDesc);
    int desktop_width  = FullDesc.Width;
    int desktop_height = FullDesc.Height;

    //Pointer position
    int ptr_info_pos_left = ptr_info.Position.x;
    int ptr_info_pos_top  = ptr_info.Position.y;

    //Figure out if any adjustment is needed for out of bound positions
    if (ptr_info_pos_left < 0)
    {
        ptr_width = ptr_info_pos_left + (int)ptr_info.ShapeInfo.Width;
    }
    else if ((ptr_info_pos_left + (int)ptr_info.ShapeInfo.Width) > desktop_width)
    {
        ptr_width = desktop_height - ptr_info_pos_left;
    }
    else
    {
        ptr_width = (int)ptr_info.ShapeInfo.Width;
    }

    if (is_mono)
    {
        ptr_info.ShapeInfo.Height = ptr_info.ShapeInfo.Height / 2;
    }

    if (ptr_info_pos_top < 0)
    {
        ptr_height = ptr_info_pos_top + (int)ptr_info.ShapeInfo.Height;
    }
    else if ((ptr_info_pos_top + (int)ptr_info.ShapeInfo.Height) > desktop_height)
    {
        ptr_height = desktop_height - ptr_info_pos_top;
    }
    else
    {
        ptr_height = (int)ptr_info.ShapeInfo.Height;
    }

    if (is_mono)
    {
        ptr_info.ShapeInfo.Height = ptr_info.ShapeInfo.Height * 2;
    }

    ptr_left = (ptr_info_pos_left < 0) ? 0 : ptr_info_pos_left;
    ptr_top  = (ptr_info_pos_top < 0)  ? 0 : ptr_info_pos_top;

    // Staging buffer/texture
    D3D11_TEXTURE2D_DESC copy_buffer_desc = {};
    copy_buffer_desc.Width              = ptr_width;
    copy_buffer_desc.Height             = ptr_height;
    copy_buffer_desc.MipLevels          = 1;
    copy_buffer_desc.ArraySize          = 1;
    copy_buffer_desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    copy_buffer_desc.SampleDesc.Count   = 1;
    copy_buffer_desc.SampleDesc.Quality = 0;
    copy_buffer_desc.Usage              = D3D11_USAGE_STAGING;
    copy_buffer_desc.BindFlags          = 0;
    copy_buffer_desc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;
    copy_buffer_desc.MiscFlags          = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> copy_buffer;
    HRESULT hr = m_Device->CreateTexture2D(&copy_buffer_desc, nullptr, &copy_buffer);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed creating staging texture for pointer", L"Desktop+ Error", S_OK, SystemTransitionsExpectedErrors); //Shouldn't be critical
    }

    //Copy needed part of desktop image
    box.left   = ptr_left;
    box.top    = ptr_top;
    box.right  = ptr_left + ptr_width;
    box.bottom = ptr_top  + ptr_height;
    m_DeviceContext->CopySubresourceRegion(copy_buffer.Get(), 0, 0, 0, 0, m_SharedSurf, 0, &box);

    //QI for IDXGISurface
    Microsoft::WRL::ComPtr<IDXGISurface> copy_surface;
    hr = copy_buffer.As(&copy_surface);
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI staging texture into IDXGISurface for pointer", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    // Map pixels
    DXGI_MAPPED_RECT mapped_surface;
    hr = copy_surface->Map(&mapped_surface, DXGI_MAP_READ);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to map surface for pointer", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //New mouseshape buffer
    auto init_buffer = std::unique_ptr<BYTE[]>{new (std::nothrow)BYTE[ptr_width * ptr_height * 4]};
    if (init_buffer == nullptr)
    {
        return ProcessFailure(nullptr, L"Failed to allocate memory for new mouse shape buffer.", L"Desktop+ Error", E_OUTOFMEMORY);
    }

    uint32_t* init_buffer_u32    = (uint32_t*)init_buffer.get();
    uint32_t* desktop_buffer_u32 = (uint32_t*)mapped_surface.pBits;
    int desktop_buffer_pitch     = mapped_surface.Pitch / sizeof(uint32_t);

    // What to skip (pixel offset)
    unsigned int ptr_skip_x = (ptr_info_pos_left < 0) ? (-1 * ptr_info_pos_left) : (0);
    unsigned int ptr_skip_y = (ptr_info_pos_top < 0)  ? (-1 * ptr_info_pos_top)  : (0);
    int ptr_height_half = ptr_info.ShapeInfo.Height / 2;

    if (is_mono)
    {
        //Iterate through pointer shape pixels
        for (int ptr_pixel_row = 0; ptr_pixel_row < ptr_height; ++ptr_pixel_row)
        {
            //Set mask
            uint8_t mask_base = 0x80;
            mask_base = mask_base >> (ptr_skip_x % 8);
            for (int ptr_pixel_col = 0; ptr_pixel_col < ptr_width; ++ptr_pixel_col)
            {
                //Get masks using appropriate offsets
                int mask_offset_base = ((ptr_pixel_col + ptr_skip_x) / 8);
                BYTE mask_and = ptr_info.PtrShapeBuffer[mask_offset_base + ((ptr_pixel_row + ptr_skip_y) * ptr_info.ShapeInfo.Pitch)                  ] & mask_base;
                BYTE mask_xor = ptr_info.PtrShapeBuffer[mask_offset_base + ((ptr_pixel_row + ptr_skip_y + ptr_height_half) * ptr_info.ShapeInfo.Pitch)] & mask_base;
                uint32_t mask_and_u32 = (mask_and) ? 0xFFFFFFFF : 0xFF000000;
                uint32_t mask_xor_u32 = (mask_xor) ? 0x00FFFFFF : 0x00000000;

                //Set new pixel
                init_buffer_u32[(ptr_pixel_row * ptr_width) + ptr_pixel_col] = (desktop_buffer_u32[(ptr_pixel_row * desktop_buffer_pitch) + ptr_pixel_col] & mask_and_u32) ^ mask_xor_u32;

                //Adjust mask
                mask_base = (mask_base == 0x01) ? 0x80 : mask_base >> 1;
            }
        }
    }
    else
    {
        uint32_t* shape_buffer_u32 = (uint32_t*)ptr_info.PtrShapeBuffer;

        //Iterate through pointer shape pixels
        for (unsigned int ptr_pixel_row = 0; ptr_pixel_row < ptr_height; ++ptr_pixel_row)
        {
            for (unsigned int ptr_pixel_col = 0; ptr_pixel_col < ptr_width; ++ptr_pixel_col)
            {
                // Set up mask
                uint32_t MaskVal = 0xFF000000 & shape_buffer_u32[(ptr_pixel_col + ptr_skip_x) + ((ptr_pixel_row + ptr_skip_y) * uint32_t(ptr_info.ShapeInfo.Pitch / sizeof(uint32_t)))];
                if (MaskVal)
                {
                    //mask_value was 0xFF
                    init_buffer_u32[(ptr_pixel_row * ptr_width) + ptr_pixel_col] = (desktop_buffer_u32[(ptr_pixel_row * desktop_buffer_pitch) + ptr_pixel_col] ^ shape_buffer_u32[(ptr_pixel_col + ptr_skip_x) + 
                                                                                    ((ptr_pixel_row + ptr_skip_y) * uint32_t(ptr_info.ShapeInfo.Pitch / sizeof(uint32_t)))]) | 0xFF000000;
                }
                else
                {
                    //mask_value was 0x00
                    init_buffer_u32[(ptr_pixel_row * ptr_width) + ptr_pixel_col] = shape_buffer_u32[(ptr_pixel_col + ptr_skip_x) + 
                                                                                   ((ptr_pixel_row + ptr_skip_y) * uint32_t(ptr_info.ShapeInfo.Pitch / sizeof(uint32_t)))] | 0xFF000000;
                }
            }
        }
    }

    //Unmap surface
    hr = copy_surface->Unmap();
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to unmap surface for pointer", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //Create texture
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width  = ptr_width;
    tex_desc.Height = ptr_height;
    tex_desc.MipLevels          = 1;
    tex_desc.ArraySize          = 1;
    tex_desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    tex_desc.SampleDesc.Count   = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage              = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    tex_desc.CPUAccessFlags     = 0;
    tex_desc.MiscFlags          = 0;

    //Set shader resource properties
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format                    = tex_desc.Format;
    srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = tex_desc.MipLevels - 1;
    srv_desc.Texture2D.MipLevels       = tex_desc.MipLevels;

    //Set up init data
    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem     = init_buffer.get();
    init_data.SysMemPitch = ptr_width * 4;

    //Create mouse pointer texture
    hr = m_Device->CreateTexture2D(&tex_desc, &init_data, &out_tex);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create mouse pointer texture", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    out_tex_format = tex_desc.Format;

    return DUPL_RETURN_SUCCESS;
}

DUPL_RETURN OutputManager::ProcessMonoMaskFloat16(bool is_mono, PTR_INFO& ptr_info, int& ptr_width, int& ptr_height, int& ptr_left, int& ptr_top, 
                                                  Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_tex, DXGI_FORMAT& out_tex_format, D3D11_BOX& box)
{
    out_tex_format = DXGI_FORMAT_UNKNOWN;

    //PtrShapeBuffer can sometimes be nullptr when the secure desktop is active, skip
    if (ptr_info.PtrShapeBuffer == nullptr)
        return DUPL_RETURN_SUCCESS;

    //Desktop dimensions
    D3D11_TEXTURE2D_DESC FullDesc;
    m_SharedSurf->GetDesc(&FullDesc);
    int DesktopWidth  = FullDesc.Width;
    int DesktopHeight = FullDesc.Height;

    //Pointer position
    int ptr_info_pos_left = ptr_info.Position.x;
    int ptr_info_pos_top  = ptr_info.Position.y;

    //Figure out if any adjustment is needed for out of bound positions
    if (ptr_info_pos_left < 0)
    {
        ptr_width = ptr_info_pos_left + (int)ptr_info.ShapeInfo.Width;
    }
    else if ((ptr_info_pos_left + (int)ptr_info.ShapeInfo.Width) > DesktopWidth)
    {
        ptr_width = DesktopWidth - ptr_info_pos_left;
    }
    else
    {
        ptr_width = (int)ptr_info.ShapeInfo.Width;
    }

    if (is_mono)
    {
        ptr_info.ShapeInfo.Height = ptr_info.ShapeInfo.Height / 2;
    }

    if (ptr_info_pos_top < 0)
    {
        ptr_height = ptr_info_pos_top + (int)ptr_info.ShapeInfo.Height;
    }
    else if ((ptr_info_pos_top + (int)ptr_info.ShapeInfo.Height) > DesktopHeight)
    {
        ptr_height = DesktopHeight - ptr_info_pos_top;
    }
    else
    {
        ptr_height = (int)ptr_info.ShapeInfo.Height;
    }

    if (is_mono)
    {
        ptr_info.ShapeInfo.Height = ptr_info.ShapeInfo.Height * 2;
    }

    ptr_left = (ptr_info_pos_left < 0) ? 0 : ptr_info_pos_left;
    ptr_top  = (ptr_info_pos_top < 0)  ? 0 : ptr_info_pos_top;

    //Staging buffer/texture
    D3D11_TEXTURE2D_DESC copy_buffer_desc = {};
    copy_buffer_desc.Width              = ptr_width;
    copy_buffer_desc.Height             = ptr_height;
    copy_buffer_desc.MipLevels          = 1;
    copy_buffer_desc.ArraySize          = 1;
    copy_buffer_desc.Format             = DXGI_FORMAT_R16G16B16A16_FLOAT;
    copy_buffer_desc.SampleDesc.Count   = 1;
    copy_buffer_desc.SampleDesc.Quality = 0;
    copy_buffer_desc.Usage              = D3D11_USAGE_STAGING;
    copy_buffer_desc.BindFlags          = 0;
    copy_buffer_desc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;
    copy_buffer_desc.MiscFlags          = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> copy_buffer;
    HRESULT hr = m_Device->CreateTexture2D(&copy_buffer_desc, nullptr, &copy_buffer);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed creating staging texture for pointer", L"Desktop+ Error", S_OK, SystemTransitionsExpectedErrors); //Shouldn't be critical
    }

    //Copy needed part of desktop image
    box.left   = ptr_left;
    box.top    = ptr_top;
    box.right  = ptr_left + ptr_width;
    box.bottom = ptr_top  + ptr_height;
    m_DeviceContext->CopySubresourceRegion(copy_buffer.Get(), 0, 0, 0, 0, m_SharedSurf, 0, &box);

    //QI for IDXGISurface
    Microsoft::WRL::ComPtr<IDXGISurface> copy_surface;
    hr = copy_buffer.As(&copy_surface);
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI staging texture into IDXGISurface for pointer", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //Map pixels
    DXGI_MAPPED_RECT mapped_surface;
    hr = copy_surface->Map(&mapped_surface, DXGI_MAP_READ);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to map surface for pointer", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    //New mouseshape buffer
    auto init_buffer = std::unique_ptr<BYTE[]>{new (std::nothrow)BYTE[ptr_width * ptr_height * 4 * sizeof(PackedVector::HALF)]};
    if (init_buffer == nullptr)
    {
        return ProcessFailure(nullptr, L"Failed to allocate memory for new mouse shape buffer.", L"Desktop+ Error", E_OUTOFMEMORY);
    }

    PackedVector::HALF* init_buffer_f16    = (PackedVector::HALF*)init_buffer.get();
    PackedVector::HALF* desktop_buffer_f16 = (PackedVector::HALF*)mapped_surface.pBits;
    int desktop_buffer_pitch               = mapped_surface.Pitch / sizeof(PackedVector::HALF);

    //What to skip (pixel offset)
    unsigned int ptr_skip_x = (ptr_info_pos_left < 0) ? (-1 * ptr_info_pos_left) : (0);
    unsigned int ptr_skip_y = (ptr_info_pos_top < 0)  ? (-1 * ptr_info_pos_top)  : (0);
    int ptr_height_half = ptr_info.ShapeInfo.Height / 2;

    //While the float value of SDR white may not be 1.0 depending on OS and system settings, the cursor texture is always 8-bit per channel
    //This might not be 100% accurate, but masked color cursors are also very rare, so we mostly care about XOR negative color effects working
    const float sdr_white_level_adjustment = (m_DesktopHDRWhiteLevelAdjustments.empty()) ? 1.0f : 
                                              m_DesktopHDRWhiteLevelAdjustments[clamp((size_t)ptr_info.WhoUpdatedPositionLast, (size_t)0, m_DesktopHDRWhiteLevelAdjustments.size()-1)];

    if (is_mono)
    {
        //Iterate through pointer shape pixels
        for (int ptr_pixel_row = 0; ptr_pixel_row < ptr_height; ++ptr_pixel_row)
        {
            //Set mask
            uint8_t mask_base = 0x80;
            mask_base = mask_base >> (ptr_skip_x % 8);
            for (int ptr_pixel_col = 0; ptr_pixel_col < ptr_width; ++ptr_pixel_col)
            {
                const int offset_in  = (ptr_pixel_row * desktop_buffer_pitch) + (ptr_pixel_col * 4);
                const int offset_out = ((ptr_pixel_row * ptr_width) + (ptr_pixel_col)) * 4;

                //Get masks using appropriate offsets
                int mask_offset_base = ((ptr_pixel_col + ptr_skip_x) / 8);
                BYTE mask_and = ptr_info.PtrShapeBuffer[mask_offset_base + ((ptr_pixel_row + ptr_skip_y) * ptr_info.ShapeInfo.Pitch)                  ] & mask_base;
                BYTE mask_xor = ptr_info.PtrShapeBuffer[mask_offset_base + ((ptr_pixel_row + ptr_skip_y + ptr_height_half) * ptr_info.ShapeInfo.Pitch)] & mask_base;

                //Set new pixel
                float f32_value_r = (mask_and) ? PackedVector::XMConvertHalfToFloat(desktop_buffer_f16[offset_in])     : 0.0f;
                float f32_value_g = (mask_and) ? PackedVector::XMConvertHalfToFloat(desktop_buffer_f16[offset_in + 1]) : 0.0f;
                float f32_value_b = (mask_and) ? PackedVector::XMConvertHalfToFloat(desktop_buffer_f16[offset_in + 2]) : 0.0f;
                float f32_value_a = (mask_and) ? PackedVector::XMConvertHalfToFloat(desktop_buffer_f16[offset_in + 3]) : 0.0f;

                if (mask_xor)
                {
                    //Approximation for XOR negative color effect in non-linear space
                    const float xor_neg = 0.77f / sdr_white_level_adjustment;
                    init_buffer_f16[offset_out]     = PackedVector::XMConvertFloatToHalf( std::max(xor_neg - f32_value_r, 0.0f) );
                    init_buffer_f16[offset_out + 1] = PackedVector::XMConvertFloatToHalf( std::max(xor_neg - f32_value_g, 0.0f) );
                    init_buffer_f16[offset_out + 2] = PackedVector::XMConvertFloatToHalf( std::max(xor_neg - f32_value_b, 0.0f) );
                }
                else
                {
                    init_buffer_f16[offset_out]     = PackedVector::XMConvertFloatToHalf(f32_value_r);
                    init_buffer_f16[offset_out + 1] = PackedVector::XMConvertFloatToHalf(f32_value_g);
                    init_buffer_f16[offset_out + 2] = PackedVector::XMConvertFloatToHalf(f32_value_b);
                }

                init_buffer_f16[offset_out + 3] = desktop_buffer_f16[offset_in + 3];

                //Adjust mask
                mask_base = (mask_base == 0x01) ? 0x80 : mask_base >> 1;
            }
        }
    }
    else
    {
        uint32_t* shape_buffer_u32 = (uint32_t*)ptr_info.PtrShapeBuffer;

        //Iterate through pointer shape pixels
        for (unsigned int ptr_pixel_row = 0; ptr_pixel_row < ptr_height; ++ptr_pixel_row)
        {
            for (unsigned int ptr_pixel_col = 0; ptr_pixel_col < ptr_width; ++ptr_pixel_col)
            {
                // Set up mask
                const int offset_in  = (ptr_pixel_row * desktop_buffer_pitch) + (ptr_pixel_col * 4);
                const int offset_out = ((ptr_pixel_row * ptr_width) + (ptr_pixel_col)) * 4;

                uint32_t mask_value = 0xFF000000 & shape_buffer_u32[(ptr_pixel_col + ptr_skip_x) + ((ptr_pixel_row + ptr_skip_y) * uint32_t(ptr_info.ShapeInfo.Pitch / sizeof(uint32_t)))];
                if (mask_value)
                {
                    //mask_value was 0xFF

                    //Cast float values to regular RGB ones and XOR them as intended (though this is still in linear color space)
                    float f32_value_r = PackedVector::XMConvertHalfToFloat(desktop_buffer_f16[offset_in]);
                    float f32_value_g = PackedVector::XMConvertHalfToFloat(desktop_buffer_f16[offset_in + 1]);
                    float f32_value_b = PackedVector::XMConvertHalfToFloat(desktop_buffer_f16[offset_in + 2]);
                    float f32_value_a = PackedVector::XMConvertHalfToFloat(desktop_buffer_f16[offset_in + 3]);

                    uint32_t u32_value_r = f32_value_r * 255.0f * sdr_white_level_adjustment;
                    uint32_t u32_value_g = f32_value_g * 255.0f * sdr_white_level_adjustment;
                    uint32_t u32_value_b = f32_value_b * 255.0f * sdr_white_level_adjustment;

                    uint32_t ptr_rgba = shape_buffer_u32[(ptr_pixel_col + ptr_skip_x) + ((ptr_pixel_row + ptr_skip_y) * uint32_t(ptr_info.ShapeInfo.Pitch / sizeof(uint32_t)))];
                    uint32_t u32_buff_r = (ptr_rgba >> 16) & 0xFF;
                    uint32_t u32_buff_g = (ptr_rgba >>  8) & 0xFF;
                    uint32_t u32_buff_b =  ptr_rgba        & 0xFF;

                    u32_value_r ^= u32_buff_r;
                    u32_value_g ^= u32_buff_g;
                    u32_value_b ^= u32_buff_b;

                    //Cast them back again
                    f32_value_r = u32_value_r / 255.0f / sdr_white_level_adjustment;
                    f32_value_g = u32_value_g / 255.0f / sdr_white_level_adjustment;
                    f32_value_b = u32_value_b / 255.0f / sdr_white_level_adjustment;

                    init_buffer_f16[offset_out]     = PackedVector::XMConvertFloatToHalf(f32_value_r);
                    init_buffer_f16[offset_out + 1] = PackedVector::XMConvertFloatToHalf(f32_value_g);
                    init_buffer_f16[offset_out + 2] = PackedVector::XMConvertFloatToHalf(f32_value_b);
                    init_buffer_f16[offset_out + 3] = desktop_buffer_f16[offset_in + 3];
                }
                else
                {
                    //mask_value was 0x00
                    uint32_t ptr_rgb = shape_buffer_u32[(ptr_pixel_col + ptr_skip_x) + ((ptr_pixel_row + ptr_skip_y) * uint32_t(ptr_info.ShapeInfo.Pitch / sizeof(UINT)))];

                    float f32_value_r = ((ptr_rgb >> 16) & 0xFF) / 255.0f / sdr_white_level_adjustment;
                    float f32_value_g = ((ptr_rgb >>  8) & 0xFF) / 255.0f / sdr_white_level_adjustment;
                    float f32_value_b =  (ptr_rgb        & 0xFF) / 255.0f / sdr_white_level_adjustment;

                    init_buffer_f16[offset_out]     = PackedVector::XMConvertFloatToHalf( f32_value_r );
                    init_buffer_f16[offset_out + 1] = PackedVector::XMConvertFloatToHalf( f32_value_g );
                    init_buffer_f16[offset_out + 2] = PackedVector::XMConvertFloatToHalf( f32_value_b );
                    init_buffer_f16[offset_out + 3] = desktop_buffer_f16[offset_in + 3];
                }
            }
        }
    }

    //Unmap surface
    hr = copy_surface->Unmap();
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to unmap surface for pointer", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    //Create texture
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width  = ptr_width;
    tex_desc.Height = ptr_height;
    tex_desc.MipLevels          = 1;
    tex_desc.ArraySize          = 1;
    tex_desc.Format             = DXGI_FORMAT_R16G16B16A16_FLOAT;
    tex_desc.SampleDesc.Count   = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage              = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    tex_desc.CPUAccessFlags     = 0;
    tex_desc.MiscFlags          = 0;

    //Set shader resource properties
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format                    = tex_desc.Format;
    srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = tex_desc.MipLevels - 1;
    srv_desc.Texture2D.MipLevels       = tex_desc.MipLevels;

    //Set up init data
    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem     = init_buffer.get();
    init_data.SysMemPitch = ptr_width * 4 * (int)sizeof(PackedVector::HALF);

    //Create mouse pointer texture
    hr = m_Device->CreateTexture2D(&tex_desc, &init_data, &out_tex);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create mouse pointer texture", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    out_tex_format = tex_desc.Format;

    return DUPL_RETURN_SUCCESS;
}

//
// Reset render target view
//
DUPL_RETURN OutputManager::MakeRTV()
{
    D3D11_TEXTURE2D_DESC desc_ovrl_tex;
    m_OvrlTex->GetDesc(&desc_ovrl_tex);

    //Create render target view for overlay texture
    D3D11_RENDER_TARGET_VIEW_DESC ovrl_tex_rtv_desc = {};

    ovrl_tex_rtv_desc.Format = desc_ovrl_tex.Format;
    ovrl_tex_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    ovrl_tex_rtv_desc.Texture2D.MipSlice = 0;

    m_Device->CreateRenderTargetView(m_OvrlTex, &ovrl_tex_rtv_desc, &m_OvrlRTV);

    return DUPL_RETURN_SUCCESS;
}

//
// Initialize shaders for drawing
//
DUPL_RETURN OutputManager::InitShaders()
{
    HRESULT hr;

    UINT Size = ARRAYSIZE(g_VS);
    hr = m_Device->CreateVertexShader(g_VS, Size, nullptr, &m_VertexShader);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create vertex shader", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    D3D11_INPUT_ELEMENT_DESC Layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT NumElements = ARRAYSIZE(Layout);
    hr = m_Device->CreateInputLayout(Layout, NumElements, g_VS, Size, &m_InputLayout);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create input layout", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }
    m_DeviceContext->IASetInputLayout(m_InputLayout);

    Size = ARRAYSIZE(g_PS);
    hr = m_Device->CreatePixelShader(g_PS, Size, nullptr, &m_PixelShader);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create pixel shader", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }
    Size = ARRAYSIZE(g_PSCURSOR);
    hr = m_Device->CreatePixelShader(g_PSCURSOR, Size, nullptr, &m_PixelShaderCursor);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create cursor pixel shader", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}


//
// Recreate textures
//
DUPL_RETURN OutputManager::CreateTextures(INT SingleOutput, _Out_ UINT* OutCount, _Out_ RECT* DeskBounds)
{
    HRESULT hr;
    *OutCount = 0;
    const int desktop_count = m_DesktopRects.size();

    //Output doesn't exist. This will result in a soft-error invalid output state (system may be in an transition state, in which case we'll automatically recover)
    if (SingleOutput >= desktop_count) 
    {
        m_DesktopX      = 0;
        m_DesktopY      = 0;
        m_DesktopWidth  = -1;
        m_DesktopHeight = -1;

        return DUPL_RETURN_ERROR_EXPECTED;
    }

    //Figure out right dimensions for full size desktop texture
    DPRect output_rect_total;
    if (SingleOutput < 0)
    {
        //Combined desktop, also count desktops on the used adapter
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr = GetDXGIAdapter();

        UINT output_index_adapter = 0;
        hr = S_OK;

        while (SUCCEEDED(hr))
        {
            //Break early if used desktop count is lower than actual output count
            if (output_index_adapter >= desktop_count)
            {
                ++output_index_adapter;
                break;
            }

            Microsoft::WRL::ComPtr<IDXGIOutput> output_ptr;
            hr = adapter_ptr->EnumOutputs(output_index_adapter, &output_ptr);
            if ((output_ptr != nullptr) && (hr != DXGI_ERROR_NOT_FOUND))
            {
                DXGI_OUTPUT_DESC output_desc;
                output_ptr->GetDesc(&output_desc);

                DPRect output_rect(output_desc.DesktopCoordinates.left,  output_desc.DesktopCoordinates.top, 
                                   output_desc.DesktopCoordinates.right, output_desc.DesktopCoordinates.bottom);

                (output_rect_total.GetWidth() == 0) ? output_rect_total = output_rect : output_rect_total.Add(output_rect);
            }

            ++output_index_adapter;
        }

        *OutCount = output_index_adapter - 1;
    }
    else
    {
        //Single desktop, grab cached desktop rect
        if (SingleOutput < desktop_count)
        {
            output_rect_total = m_DesktopRects[SingleOutput];
            *OutCount = 1;
        }
    }

    //Store size and position
    m_DesktopX      = output_rect_total.GetTL().x;
    m_DesktopY      = output_rect_total.GetTL().y;
    m_DesktopWidth  = output_rect_total.GetWidth();
    m_DesktopHeight = output_rect_total.GetHeight();

    DeskBounds->left   = output_rect_total.GetTL().x;
    DeskBounds->top    = output_rect_total.GetTL().y;
    DeskBounds->right  = output_rect_total.GetBR().x;
    DeskBounds->bottom = output_rect_total.GetBR().y;

    //Set it as mouse scale on the desktop texture overlay for the UI to read the resolution from there
    vr::HmdVector2_t mouse_scale = {0};
    mouse_scale.v[0] = m_DesktopWidth;
    mouse_scale.v[1] = m_DesktopHeight;
    vr::VROverlay()->SetOverlayMouseScale(m_OvrlHandleDesktopTexture, &mouse_scale);

    //Create shared texture for all duplication threads to draw into
    D3D11_TEXTURE2D_DESC TexD;
    RtlZeroMemory(&TexD, sizeof(D3D11_TEXTURE2D_DESC));
    TexD.Width            = m_DesktopWidth;
    TexD.Height           = m_DesktopHeight;
    TexD.MipLevels        = 1;
    TexD.ArraySize        = 1;
    TexD.Format           = ((m_OutputHDRAvailable) && (ConfigManager::GetValue(configid_bool_performance_hdr_mirroring))) ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
    TexD.SampleDesc.Count = 1;
    TexD.Usage            = D3D11_USAGE_DEFAULT;
    TexD.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    TexD.CPUAccessFlags   = 0;
    TexD.MiscFlags        = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    hr = m_Device->CreateTexture2D(&TexD, nullptr, &m_SharedSurf);

    if (!FAILED(hr))
    {
        TexD.MiscFlags = 0;
        hr = m_Device->CreateTexture2D(&TexD, nullptr, &m_OvrlTex);
    }

    if (FAILED(hr))
    {
        if (output_rect_total.GetWidth() != 0)
        {
            // If we are duplicating the complete desktop we try to create a single texture to hold the
            // complete desktop image and blit updates from the per output DDA interface.  The GPU can
            // always support a texture size of the maximum resolution of any single output but there is no
            // guarantee that it can support a texture size of the desktop.
            return ProcessFailure(m_Device, L"Failed to create shared texture. Combined desktop texture size may be larger than the maximum supported supported size of the GPU", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
        }
        else
        {
            return ProcessFailure(m_Device, L"Failed to create shared texture", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
        }
    }

    // Get keyed mutex
    hr = m_SharedSurf->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&m_KeyMutex));

    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to query for keyed mutex", L"Desktop+ Error", hr);
    }

    //Create shader resource for shared texture
    D3D11_TEXTURE2D_DESC FrameDesc;
    m_SharedSurf->GetDesc(&FrameDesc);

    D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc;
    ShaderDesc.Format = FrameDesc.Format;
    ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    ShaderDesc.Texture2D.MostDetailedMip = FrameDesc.MipLevels - 1;
    ShaderDesc.Texture2D.MipLevels = FrameDesc.MipLevels;

    // Create new shader resource view
    hr = m_Device->CreateShaderResourceView(m_SharedSurf, &ShaderDesc, &m_ShaderResource);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create shader resource", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //Create textures for multi GPU handling if needed
    if (m_MultiGPUTargetDevice != nullptr)
    {
        //Staging texture
        TexD.Usage          = D3D11_USAGE_STAGING;
        TexD.BindFlags      = 0;
        TexD.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        TexD.MiscFlags      = 0;

        hr = m_Device->CreateTexture2D(&TexD, nullptr, &m_MultiGPUTexStaging);

        if (FAILED(hr))
        {
            return ProcessFailure(m_Device, L"Failed to create staging texture", L"Desktop+ Error", hr);
        }

        //Copy-target texture
        TexD.Usage          = D3D11_USAGE_DYNAMIC;
        TexD.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
        TexD.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        TexD.MiscFlags      = 0;

        hr = m_MultiGPUTargetDevice->CreateTexture2D(&TexD, nullptr, &m_MultiGPUTexTarget);

        if (FAILED(hr))
        {
            return ProcessFailure(m_MultiGPUTargetDevice, L"Failed to create copy-target texture", L"Desktop+ Error", hr);
        }
    }

    return DUPL_RETURN_SUCCESS;
}

void OutputManager::DrawFrameToOverlayTex(bool clear_rtv)
{
    //Do a straight copy if there are no issues with that or do the alpha check if it's still pending
    if ((!m_OutputAlphaCheckFailed) || (m_OutputAlphaChecksPending > 0))
    {
        m_DeviceContext->CopyResource(m_OvrlTex, m_SharedSurf);

        if (m_OutputAlphaChecksPending > 0)
        {
            //Check for translucent pixels (not fast)
            m_OutputAlphaCheckFailed = DesktopTextureAlphaCheck();
            m_OutputAlphaChecksPending--;

            LOG_IF_F(WARNING, (m_OutputAlphaCheckFailed) && (m_OutputAlphaChecksPending == 0), "Failed Desktop Duplication alpha check, using extra render pass");
        }
    }

    //Draw the frame to the texture with the alpha channel fixing pixel shader if we have to
    if (m_OutputAlphaCheckFailed)
    {
        // Set resources
        UINT Stride = sizeof(VERTEX);
        UINT Offset = 0;
        const FLOAT blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_DeviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
        m_DeviceContext->OMSetRenderTargets(1, &m_OvrlRTV, nullptr);
        m_DeviceContext->VSSetShader(m_VertexShader, nullptr, 0);
        m_DeviceContext->PSSetShader(m_PixelShader, nullptr, 0);
        m_DeviceContext->PSSetShaderResources(0, 1, &m_ShaderResource);
        m_DeviceContext->PSSetSamplers(0, 1, &m_Sampler);
        m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_DeviceContext->IASetVertexBuffers(0, 1, &m_VertexBuffer, &Stride, &Offset);

        // Draw textured quad onto render target
        if (clear_rtv)
        {
            const float bgColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            m_DeviceContext->ClearRenderTargetView(m_OvrlRTV, bgColor);
        }

        m_DeviceContext->Draw(NUMVERTICES, 0);
    }
}

//
// Draw mouse provided in buffer to overlay texture
//
DUPL_RETURN OutputManager::DrawMouseToOverlayTex(_In_ PTR_INFO* PtrInfo)
{
    //Just return if we don't need to render it
    if ((!ConfigManager::GetValue(configid_bool_input_mouse_render_cursor)) || (!PtrInfo->Visible))
    {
        return DUPL_RETURN_SUCCESS;
    }

    ID3D11Buffer* VertexBuffer = nullptr;

    // Vars to be used
    D3D11_SUBRESOURCE_DATA InitData = {};
    D3D11_TEXTURE2D_DESC Desc = {};
    D3D11_SHADER_RESOURCE_VIEW_DESC SDesc = {};

    // Position will be changed based on mouse position
    VERTEX Vertices[NUMVERTICES] =
    {
        { XMFLOAT3(-1.0f, -1.0f, 0), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f, 0), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f,  -1.0f, 0), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f,  -1.0f, 0), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f, 0), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f,   1.0f, 0), XMFLOAT2(1.0f, 0.0f) },
    };

    // Center of desktop dimensions
    FLOAT CenterX = (m_DesktopWidth  / 2.0f);
    FLOAT CenterY = (m_DesktopHeight / 2.0f);

    // Clipping adjusted coordinates / dimensions
    INT PtrWidth  = 0;
    INT PtrHeight = 0;
    INT PtrLeft   = 0;
    INT PtrTop    = 0;

    // Buffer used if necessary (in case of monochrome or masked pointer)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> CursorTexNew;
    DXGI_FORMAT CursorTexNewFormat = DXGI_FORMAT_UNKNOWN;

    // Used for copying pixels if necessary
    D3D11_BOX Box = {};
    Box.front = 0;
    Box.back  = 1;

    //Process shape (or just get position when not new cursor)
    switch (PtrInfo->ShapeInfo.Type)
    {
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
        {
            PtrLeft = PtrInfo->Position.x;
            PtrTop  = PtrInfo->Position.y;

            PtrWidth  = static_cast<INT>(PtrInfo->ShapeInfo.Width);
            PtrHeight = static_cast<INT>(PtrInfo->ShapeInfo.Height);

            break;
        }
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
        {
            PtrInfo->CursorShapeChanged = true; //Texture content is screen dependent
            const bool is_mono_cursor = (PtrInfo->ShapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME);

            //Process for HDR is needed
            if ((m_OutputHDRAvailable) && (ConfigManager::GetValue(configid_bool_performance_hdr_mirroring)))
            {
                ProcessMonoMaskFloat16(is_mono_cursor, *PtrInfo, PtrWidth, PtrHeight, PtrLeft, PtrTop, CursorTexNew, CursorTexNewFormat, Box);
            }
            else
            {
                ProcessMonoMask(is_mono_cursor, *PtrInfo, PtrWidth, PtrHeight, PtrLeft, PtrTop, CursorTexNew, CursorTexNewFormat, Box);
            }
            
            break;
        }
        default: break;
    }

    if (m_MouseCursorNeedsUpdate)
    {
        PtrInfo->CursorShapeChanged = true;
    }

    // VERTEX creation
    Vertices[0].Pos.x = (PtrLeft - CenterX) / CenterX;
    Vertices[0].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / CenterY;
    Vertices[1].Pos.x = (PtrLeft - CenterX) / CenterX;
    Vertices[1].Pos.y = -1 * (PtrTop - CenterY) / CenterY;
    Vertices[2].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / CenterX;
    Vertices[2].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / CenterY;
    Vertices[3].Pos.x = Vertices[2].Pos.x;
    Vertices[3].Pos.y = Vertices[2].Pos.y;
    Vertices[4].Pos.x = Vertices[1].Pos.x;
    Vertices[4].Pos.y = Vertices[1].Pos.y;
    Vertices[5].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / CenterX;
    Vertices[5].Pos.y = -1 * (PtrTop - CenterY) / CenterY;

    //Vertex buffer description
    D3D11_BUFFER_DESC BDesc;
    ZeroMemory(&BDesc, sizeof(D3D11_BUFFER_DESC));
    BDesc.Usage = D3D11_USAGE_DEFAULT;
    BDesc.ByteWidth = sizeof(VERTEX) * NUMVERTICES;
    BDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BDesc.CPUAccessFlags = 0;

    ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
    InitData.pSysMem = Vertices;

    // Create vertex buffer
    HRESULT hr = m_Device->CreateBuffer(&BDesc, &InitData, &VertexBuffer);
    if (FAILED(hr))
    {
        m_MouseShaderRes.Reset();
        m_MouseTex.Reset();

        return ProcessFailure(m_Device, L"Failed to create mouse pointer vertex buffer in OutputManager", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
    }

    //It can occasionally happen that no cursor shape update is detected after resetting duplication, so the m_MouseTex check is more of a workaround, but unproblematic
    if ( (PtrInfo->CursorShapeChanged) || (m_MouseTex == nullptr) ) 
    {
        //Only create a texture here for regular color cursors (mask/mono were already created)
        if (PtrInfo->ShapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR)
        {
            Desc.Width              = PtrWidth;
            Desc.Height             = PtrHeight;
            Desc.MipLevels          = 1;
            Desc.ArraySize          = 1;
            Desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
            Desc.SampleDesc.Count   = 1;
            Desc.SampleDesc.Quality = 0;
            Desc.Usage              = D3D11_USAGE_DEFAULT;
            Desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
            Desc.CPUAccessFlags     = 0;
            Desc.MiscFlags          = 0;

            //Set up init data
            InitData.pSysMem          = PtrInfo->PtrShapeBuffer;
            InitData.SysMemPitch      = PtrInfo->ShapeInfo.Pitch;
            InitData.SysMemSlicePitch = 0;

            // Create mouseshape as texture
            hr = m_Device->CreateTexture2D(&Desc, &InitData, &m_MouseTex);
            if (FAILED(hr))
            {
                return ProcessFailure(m_Device, L"Failed to create mouse pointer texture", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
            }
        }
        else
        {
            m_MouseTex = CursorTexNew;
        }

        if (m_MouseTex != nullptr)
        {
            //Set shader resource properties
            SDesc.Format                    = (PtrInfo->ShapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) ? DXGI_FORMAT_B8G8R8A8_UNORM : CursorTexNewFormat;
            SDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
            SDesc.Texture2D.MostDetailedMip = 0;
            SDesc.Texture2D.MipLevels       = 1;

            // Create shader resource from texture
            hr = m_Device->CreateShaderResourceView(m_MouseTex.Get(), &SDesc, &m_MouseShaderRes);
            if (FAILED(hr))
            {
                m_MouseTex.Reset();
                return ProcessFailure(m_Device, L"Failed to create shader resource from mouse pointer texture", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
            }
        }
    }

    // Set resources
    FLOAT BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    UINT Stride = sizeof(VERTEX);
    UINT Offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    m_DeviceContext->OMSetBlendState(m_BlendState, BlendFactor, 0xFFFFFFFF);
    m_DeviceContext->OMSetRenderTargets(1, &m_OvrlRTV, nullptr);
    m_DeviceContext->VSSetShader(m_VertexShader, nullptr, 0);
    m_DeviceContext->PSSetShader(m_PixelShaderCursor, nullptr, 0);
    m_DeviceContext->PSSetShaderResources(0, 1, m_MouseShaderRes.GetAddressOf());
    m_DeviceContext->PSSetSamplers(0, 1, &m_Sampler);

    // Draw
    m_DeviceContext->Draw(NUMVERTICES, 0);

    // Clean
    if (VertexBuffer)
    {
        VertexBuffer->Release();
        VertexBuffer = nullptr;
    }

    m_MouseCursorNeedsUpdate = false;

    return DUPL_RETURN_SUCCESS;
}

DUPL_RETURN_UPD OutputManager::RefreshOpenVROverlayTexture(DPRect& DirtyRectTotal, bool force_full_copy)
{
    if ((m_OvrlHandleDesktopTexture != vr::k_ulOverlayHandleInvalid) && (m_OvrlTex))
    {
        vr::Texture_t vrtex;
        vrtex.eType       = vr::TextureType_DirectX;
        vrtex.eColorSpace = ((m_OutputHDRAvailable) && (ConfigManager::GetValue(configid_bool_performance_hdr_mirroring))) ? vr::ColorSpace_Linear : vr::ColorSpace_Gamma;
        vrtex.handle      = m_OvrlTex;

        //The intermediate texture can be assumed to be not complete when a full copy is forced, so redraw that
        if (force_full_copy)
        {
            //Try to acquire sync for shared surface needed by DrawFrameToOverlayTex()
            HRESULT hr = m_KeyMutex->AcquireSync(0, m_MaxActiveRefreshDelay);
            if (hr == static_cast<HRESULT>(WAIT_TIMEOUT))
            {
                //Another thread has the keyed mutex so there will be a new frame ready after this.
                //Bail out and just set the pending dirty region to full so everything gets drawn over on the next update
                m_OutputPendingDirtyRect = {0, 0, m_DesktopWidth, m_DesktopHeight};
                return DUPL_RETURN_UPD_RETRY;
            }
            else if (FAILED(hr))
            {
                return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to acquire keyed mutex", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
            }

            DrawFrameToOverlayTex(true);

            //Release keyed mutex
            hr = m_KeyMutex->ReleaseSync(0);
            if (FAILED(hr))
            {
                return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to Release keyed mutex", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
            }

            //We don't draw the cursor here as this can lead to tons of issues for little gain. We might not even know what the cursor looks like if it was cropped out previously, etc.
            //We do mark where the cursor has last been seen as pending dirty region, however, so it gets updated at the next best moment even if it didn't move

            if (m_MouseLastInfo.Visible)
            {
                m_OutputPendingDirtyRect = {    m_MouseLastInfo.Position.x, m_MouseLastInfo.Position.y, int(m_MouseLastInfo.Position.x + m_MouseLastInfo.ShapeInfo.Width),
                                            int(m_MouseLastInfo.Position.y + m_MouseLastInfo.ShapeInfo.Height) };
            }
        }

        //Copy texture over to GPU connected to VR HMD if needed
        if (m_MultiGPUTargetDevice != nullptr)
        {
            //This isn't very fast but the only way to my knowledge. Happy to receive improvements on this though
            m_DeviceContext->CopyResource(m_MultiGPUTexStaging, m_OvrlTex);

            D3D11_MAPPED_SUBRESOURCE mapped_resource_staging;
            RtlZeroMemory(&mapped_resource_staging, sizeof(D3D11_MAPPED_SUBRESOURCE));
            HRESULT hr = m_DeviceContext->Map(m_MultiGPUTexStaging, 0, D3D11_MAP_READ, 0, &mapped_resource_staging);

            if (FAILED(hr))
            {
                return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to map staging texture", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
            }

            D3D11_MAPPED_SUBRESOURCE mapped_resource_target;
            RtlZeroMemory(&mapped_resource_target, sizeof(D3D11_MAPPED_SUBRESOURCE));
            hr = m_MultiGPUTargetDeviceContext->Map(m_MultiGPUTexTarget, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource_target);

            if (FAILED(hr))
            {
                return (DUPL_RETURN_UPD)ProcessFailure(m_MultiGPUTargetDevice, L"Failed to map copy-target texture", L"Desktop+ Error", hr, SystemTransitionsExpectedErrors);
            }

            memcpy(mapped_resource_target.pData, mapped_resource_staging.pData, m_DesktopHeight * mapped_resource_staging.RowPitch);

            m_DeviceContext->Unmap(m_MultiGPUTexStaging, 0);
            m_MultiGPUTargetDeviceContext->Unmap(m_MultiGPUTexTarget, 0);

            vrtex.handle = m_MultiGPUTexTarget;
        }

        //Do a simple full copy (done below) if the rect covers the whole texture (this isn't slower than a full rect copy and works with size changes)
        force_full_copy = ( (force_full_copy) || (DirtyRectTotal.Contains({0, 0, m_DesktopWidth, m_DesktopHeight})) );

        if (!force_full_copy) //Otherwise do a partial copy
        {
            //Get overlay texture from OpenVR and copy dirty rect directly into it
            ID3D11Texture2D* reference_texture = (ID3D11Texture2D*)vrtex.handle;
            ID3D11ShaderResourceView* ovrl_shader_rsv;

            ovrl_shader_rsv = vr::VROverlayEx()->GetOverlayTextureEx(m_OvrlHandleDesktopTexture, reference_texture);

            if (ovrl_shader_rsv != nullptr)
            {
                ID3D11DeviceContext* device_context = (m_MultiGPUTargetDevice != nullptr) ? m_MultiGPUTargetDeviceContext : m_DeviceContext;

                Microsoft::WRL::ComPtr<ID3D11Resource> ovrl_tex;
                ovrl_shader_rsv->GetResource(&ovrl_tex);

                D3D11_BOX box = {0};
                box.left   = DirtyRectTotal.GetTL().x;
                box.top    = DirtyRectTotal.GetTL().y;
                box.front  = 0;
                box.right  = DirtyRectTotal.GetBR().x;
                box.bottom = DirtyRectTotal.GetBR().y;
                box.back   = 1;

                device_context->CopySubresourceRegion(ovrl_tex.Get(), 0, box.left, box.top, 0, reference_texture, 0, &box);

                //RSV is kept around by IVROverlayEx and not released here
            }
            else //Usually shouldn't fail, but fall back to full copy then
            {
                force_full_copy = true;
            }
        }

        if (force_full_copy) //This is down here so a failed partial copy is picked up as well
        {
            bool refresh_shared_texture = false;
            vr::VROverlayEx()->SetOverlayTextureEx(m_OvrlHandleDesktopTexture, &vrtex, {m_DesktopWidth, m_DesktopHeight}, &refresh_shared_texture);

            //Apply potential texture change to all overlays and notify them of duplication update
            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                Overlay& overlay = OverlayManager::Get().GetOverlay(i);

                if (refresh_shared_texture)
                {
                    overlay.AssignDesktopDuplicationTexture();
                }

                overlay.OnDesktopDuplicationUpdate();
            }
        }
        else
        {
            //Notifiy all overlays of duplication update
            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                OverlayManager::Get().GetOverlay(i).OnDesktopDuplicationUpdate();
            }
        }
    }

    return DUPL_RETURN_UPD_SUCCESS_REFRESHED_OVERLAY;
}

bool OutputManager::DesktopTextureAlphaCheck()
{
    if (m_DesktopRects.empty())
        return false;

    //Sanity check texture dimensions
    D3D11_TEXTURE2D_DESC desc_ovrl_tex;
    m_OvrlTex->GetDesc(&desc_ovrl_tex);

    if ( ((UINT)m_DesktopWidth != desc_ovrl_tex.Width) || ((UINT)m_DesktopHeight != desc_ovrl_tex.Height) )
        return false;

    //Read one pixel for each desktop
    const int pixel_count = m_DesktopRects.size();

    //Create a staging texture
    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width              = pixel_count;
    desc.Height             = 1;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = desc_ovrl_tex.Format;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage              = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;
    desc.BindFlags          = 0;
    desc.MiscFlags          = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex_staging;
    HRESULT hr = m_Device->CreateTexture2D(&desc, nullptr, &tex_staging);
    if (FAILED(hr))
    {
        return false;
    }

    //Copy a single pixel to staging texture for each desktop
    D3D11_BOX box = {0};
    box.front = 0;
    box.back  = 1;

    UINT dst_x = 0;
    for (const DPRect& rect : m_DesktopRects)
    {
        box.left   = clamp(rect.GetTL().x - m_DesktopX, 0, m_DesktopWidth - 1);
        box.right  = clamp(box.left + 1, 1u, (UINT)m_DesktopWidth);
        box.top    = clamp(rect.GetTL().y - m_DesktopY, 0, m_DesktopHeight - 1);
        box.bottom = clamp(box.top + 1, 1u, (UINT)m_DesktopHeight);

        m_DeviceContext->CopySubresourceRegion(tex_staging.Get(), 0, dst_x, 0, 0, m_OvrlTex, 0, &box);
        dst_x++;
    }

    //Map texture and get the pixels we just copied
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    hr = m_DeviceContext->Map(tex_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource);
    if (FAILED(hr))
    {
        return false;
    }

    //Check alpha value for anything between 0% and 100% transparency, which should not happen but apparently does
    bool ret = false;

    if (desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        for (int i = 0; i < pixel_count * 4; i += 4)
        {
            unsigned char a = ((unsigned char*)mapped_resource.pData)[i + 3];

            if ((a > 0) && (a < 255))
            {
                ret = true;
                break;
            }
        }
    }
    else if (desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
        for (int i = 0; i < pixel_count * 4; i += 4)
        {
            PackedVector::HALF a_half = ((PackedVector::HALF*)mapped_resource.pData)[i + 3];
            float a = PackedVector::XMConvertHalfToFloat(a_half);

            if ((a > 0.0f) && (a < 1.0f))
            {
                ret = true;
                break;
            }
        }
    }

    //Cleanup
    m_DeviceContext->Unmap(tex_staging.Get(), 0);

    return ret;
}

bool OutputManager::HandleOpenVREvents()
{
    vr::VREvent_t vr_event;

    //Handle Dashboard dummy ones first
    while (vr::VROverlay()->PollNextOverlayEvent(m_OvrlHandleDashboardDummy, &vr_event, sizeof(vr_event)))
    {
        switch (vr_event.eventType)
        {
            case vr::VREvent_OverlayShown:
            {
                if (!m_OvrlDashboardActive)
                {
                    m_OvrlDashboardActive = true;
                    m_DashboardActivatedOnce = true;    //Bringing up the Desktop+ also fixes what we work around with by keeping track of this

                    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                    {
                        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                        if (data.ConfigInt[configid_int_overlay_display_mode] != ovrl_dispmode_scene)
                        {
                            ShowOverlay(i);
                        }
                    }

                    m_BackgroundOverlay.Update();
                    m_OverlayDragger.UpdateTempStandingPosition();

                    if (ConfigManager::GetValue(configid_bool_interface_dim_ui))
                    {
                        DimDashboard(true);
                    }
                }

                break;
            }
            case vr::VREvent_OverlayHidden:
            {
                if (m_OvrlDashboardActive)
                {
                    m_OvrlDashboardActive = false;

                    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                    {
                        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                        if (data.ConfigInt[configid_int_overlay_display_mode] == ovrl_dispmode_dplustab)
                        {
                            HideOverlay(i);
                        }
                    }

                    m_BackgroundOverlay.Update();

                    if (ConfigManager::GetValue(configid_bool_interface_dim_ui))
                    {
                        DimDashboard(false);
                    }
                }

                break;
            }
            case vr::VREvent_DashboardActivated:
            {
                //The dashboard transform we're using basically cannot be trusted to be correct unless the dashboard has been manually brought up once.
                //On launch, SteamVR activates the dashboard automatically. Sometimes with and sometimes without this event firing.
                //In that case, the primary dashboard device is the HMD (or sometimes just invalid). That means we can be sure other dashboard device's activations are user-initiated.
                //We simply don't show dashboard-origin overlays until the dashboard has been properly activated once.
                //For HMD-only usage, switching to the Desktop+ dashboard tab also works
                if ( (!m_DashboardActivatedOnce) && (vr::VROverlay()->GetPrimaryDashboardDevice() != vr::k_unTrackedDeviceIndex_Hmd)  && 
                                                    (vr::VROverlay()->GetPrimaryDashboardDevice() != vr::k_unTrackedDeviceIndexInvalid) )
                {
                    m_DashboardActivatedOnce = true;
                }

                //Get current HMD y-position, used for getting the overlay position
                UpdateDashboardHMD_Y();

                //Hacky workaround, need to wait for the dashboard to finish appearing when not in Desktop+ tab
                if (!m_OvrlDashboardActive)
                {
                    ::Sleep(300);
                }

                for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                {
                    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                    if ((m_DashboardActivatedOnce) && (data.ConfigInt[configid_int_overlay_display_mode] == ovrl_dispmode_dashboard))
                    {
                        ShowOverlay(i);
                    }
                    else if (data.ConfigInt[configid_int_overlay_display_mode] == ovrl_dispmode_scene)
                    {
                        HideOverlay(i);
                    }
                    else if (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_dashboard) //Dashboard origin with Always display mode, update pos
                    {
                        unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
                        OverlayManager::Get().SetCurrentOverlayID(i);
                        ApplySettingTransform();
                        OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
                    }
                }

                //Finish a direct drag if one's going as the Desktop+ laser pointer will not be told inputs are released when the dashboard is up
                if (m_OvrlDirectDragActive)
                {
                    OverlayDirectDragFinish(m_OverlayDragger.GetDragOverlayID());
                }

                break;
            }
            case vr::VREvent_DashboardDeactivated:
            {
                for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                {
                    OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                    if (data.ConfigInt[configid_int_overlay_display_mode] == ovrl_dispmode_scene)
                    {
                        ShowOverlay(i);
                    }
                    else if (data.ConfigInt[configid_int_overlay_display_mode] == ovrl_dispmode_dashboard)
                    {
                        HideOverlay(i);
                    }
                }

                if (ConfigManager::GetValue(configid_bool_windows_auto_focus_scene_app_dashboard))
                {
                    WindowManager::Get().FocusActiveVRSceneApp(&m_InputSim);
                }

                //In unfortunate situations we can have a target window set and close the dashboard without getting a mouse up event ever, 
                //so we reset the target and mouse on dashboard close
                if (WindowManager::Get().GetTargetWindow() != nullptr)
                {
                    m_InputSim.MouseSetLeftDown(false);
                    WindowManager::Get().SetTargetWindow(nullptr);
                }

                //Finish a temp drag if one's going
                DetachedTempDragFinish();

                //If there is a drag going with a dashboard origin overlay (or related display mode), cancel or finish it to avoid the overlay ending up in a random spot
                if ( (m_OverlayDragger.IsDragActive()) || (m_OverlayDragger.IsDragGestureActive()) )
                {
                    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_OverlayDragger.GetDragOverlayID());

                    if ( (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_dashboard) || (data.ConfigInt[configid_int_overlay_display_mode] == ovrl_dispmode_dashboard) || 
                         (data.ConfigInt[configid_int_overlay_display_mode] == ovrl_dispmode_dplustab) )
                    { 
                        if (m_OverlayDragger.IsDragActive())
                        {
                            unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
                            OverlayManager::Get().SetCurrentOverlayID(m_OverlayDragger.GetDragOverlayID());

                            OnDragFinish();
                            m_OverlayDragger.DragCancel();  //Overlay can still disappear, so we just cancel the drag instead

                            ApplySettingTransform();

                            OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
                        }
                        else if (m_OverlayDragger.IsDragGestureActive())
                        {
                            m_OverlayDragger.DragGestureFinish();
                        }
                    }
                }

                break;
            }
            case vr::VREvent_SeatedZeroPoseReset:
            case vr::VREvent_ChaperoneUniverseHasChanged:
            {
                DetachedTransformUpdateSeatedPosition();
                break;
            }
            case vr::VREvent_SceneApplicationChanged:
            {
                DetachedTransformUpdateSeatedPosition();

                const bool loaded_overlay_profile = ConfigManager::Get().GetAppProfileManager().ActivateProfileForProcess(vr_event.data.process.pid);

                if (loaded_overlay_profile)
                    ResetOverlays();

                break;
            }
            case vr::VREvent_Input_ActionManifestReloaded:
            case vr::VREvent_Input_BindingsUpdated:
            case vr::VREvent_Input_BindingLoadSuccessful:
            case vr::VREvent_TrackedDeviceActivated:
            {
                m_VRInput.RefreshAnyGlobalActionBound();
                break;
            }
            case vr::VREvent_TrackedDeviceDeactivated:
            {
                m_VRInput.RefreshAnyGlobalActionBound();
                m_LaserPointer.RemoveDevice(vr_event.trackedDeviceIndex);
                break;
            }
            case vr::VREvent_Quit:
            {
                return true;
            }
        }
    }

    //Now handle events for the actual overlays
    int overlay_focus_count = (m_OvrlInputActive) ? 1 : 0;  //Keep track of multiple overlay focus enter/leave happening within the same frame to set m_OvrlInputActive correctly afterwards

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
        vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();
        const OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

        while (vr::VROverlay()->PollNextOverlayEvent(ovrl_handle, &vr_event, sizeof(vr_event)))
        {
            switch (vr_event.eventType)
            {
                case vr::VREvent_MouseMove:
                case vr::VREvent_MouseButtonDown:
                case vr::VREvent_MouseButtonUp:
                case vr::VREvent_ScrollDiscrete:
                case vr::VREvent_ScrollSmooth:
                {
                    OnOpenVRMouseEvent(vr_event, current_overlay_old);
                    break;
                }
                case vr::VREvent_ButtonPress:
                {
                    if (vr_event.data.controller.button == Button_Dashboard_GoHome)
                    {
                        ConfigManager::Get().GetActionManager().StartAction(ConfigManager::GetValue(configid_handle_input_go_home_action_uid), i);
                    }
                    else if (vr_event.data.controller.button == Button_Dashboard_GoBack)
                    {
                        ConfigManager::Get().GetActionManager().StartAction(ConfigManager::GetValue(configid_handle_input_go_back_action_uid), i);
                    }

                    break;
                }
                case vr::VREvent_ButtonUnpress:
                {
                    if (vr_event.data.controller.button == Button_Dashboard_GoHome)
                    {
                        ConfigManager::Get().GetActionManager().StopAction(ConfigManager::GetValue(configid_handle_input_go_home_action_uid), i);
                    }
                    else if (vr_event.data.controller.button == Button_Dashboard_GoBack)
                    {
                        ConfigManager::Get().GetActionManager().StopAction(ConfigManager::GetValue(configid_handle_input_go_back_action_uid), i);
                    }

                    break;
                }
                case vr::VREvent_FocusEnter:
                {
                    overlay_focus_count++;

                    const bool drag_or_select_mode_enabled = ( (ConfigManager::GetValue(configid_bool_state_overlay_dragmode)) || (ConfigManager::GetValue(configid_bool_state_overlay_selectmode)) );

                    if (!drag_or_select_mode_enabled)
                    {
                        if (ConfigManager::Get().GetPrimaryLaserPointerDevice() == vr::k_unTrackedDeviceIndex_Hmd)
                        {
                            ResetMouseLastLaserPointerPos();
                        }

                        //If it's a WinRT window capture, check for window management stuff
                        if ( (overlay.GetTextureSource() == ovrl_texsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0) )
                        {
                            if ( (!m_MouseIgnoreMoveEvent) && (ConfigManager::GetValue(configid_bool_windows_winrt_auto_focus)) )
                            {
                                WindowManager::Get().RaiseAndFocusWindow((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd], &m_InputSim);
                            }

                            if (ConfigManager::GetValue(configid_bool_windows_winrt_keep_on_screen))
                            {
                                WindowManager::MoveWindowIntoWorkArea((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd]);
                            }
                        }
                    }

                    break;
                }
                case vr::VREvent_FocusLeave:
                {
                    overlay_focus_count--;

                    const bool drag_or_select_mode_enabled = ( (ConfigManager::GetValue(configid_bool_state_overlay_dragmode)) || (ConfigManager::GetValue(configid_bool_state_overlay_selectmode)) );

                    if (!drag_or_select_mode_enabled)
                    {
                        //If leaving a WinRT window capture and the option is enabled, focus the active scene app
                        if ( (overlay.GetTextureSource() == ovrl_texsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0) && 
                             (ConfigManager::GetValue(configid_bool_windows_winrt_auto_focus_scene_app)) )
                        {
                            WindowManager::Get().FocusActiveVRSceneApp(&m_InputSim);
                        }

                        //A resize while drag can make the pointer lose focus, which is pretty janky. Remove target and do mouse up at least.
                        if (WindowManager::Get().GetTargetWindow() != nullptr)
                        {
                            const bool use_pen = ConfigManager::GetValue(configid_bool_input_mouse_simulate_pen_input);
                            (use_pen) ? m_InputSim.PenSetPrimaryDown(false) : m_InputSim.MouseSetLeftDown(false);

                            WindowManager::Get().SetTargetWindow(nullptr);
                        }

                        WindowManager::Get().ClearTempTopMostWindow();
                    }

                    //Finish drag if there's somehow still one going (and not temp drag mode, where this is expected)
                    if ( (m_OverlayDragger.IsDragActive()) && (!ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp)) )
                    {
                        OnDragFinish();
                        m_OverlayDragger.DragFinish();

                        ApplySettingTransform();
                    }

                    //For browser overlays, forward leave event to browser process
                    if (overlay.GetTextureSource() == ovrl_texsource_browser)
                    {
                        DPBrowserAPIClient::Get().DPBrowser_MouseLeave(overlay.GetHandle());
                        break;
                    }

                    m_InputSim.PenLeave();

                    break;
                }
                case vr::VREvent_OverlayClosed:
                case vr::VREvent_OverlayHidden:
                {
                    //Theater overlay was hidden by something (close button, other overlay taking over, etc.), disable it
                    //There may be cases where the overlay doesn't send the hidden event for this (varies between SteamVR builds), for that we also have a hack further below,
                    //though it's mostly harmless if this doesn't work (phantom dashboard tab)
                    if (OverlayManager::Get().GetTheaterOverlayID() == i)
                    {
                        SetOverlayEnabled(i, false);
                    }
                    break;
                }
                case vr::VREvent_ChaperoneUniverseHasChanged:
                {
                    //We also get this when tracking is lost, which ends up updating the dashboard position
                    if (m_OvrlActiveCount != 0)
                    {
                        ApplySettingTransform();
                    }
                    break;
                }
                default:
                {
                    //Output unhandled events when looking for something useful
                    /*std::wstringstream ss;
                    ss << L"Event: " << (int)vr_event.eventType << L"\n";
                    OutputDebugString(ss.str().c_str());*/
                    break;
                }
            }
        }
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

    m_OvrlInputActive = (overlay_focus_count > 0);

    //Handle stuff coming from SteamVR Input
    m_VRInput.Update();

    //Handle Enable Global Laser Pointer binding
    vr::InputDigitalActionData_t enable_laser_pointer_state = m_VRInput.GetEnableGlobalLaserPointerState();
    if (enable_laser_pointer_state.bChanged)
    {
        if (enable_laser_pointer_state.bState)
        {
            if (!m_LaserPointer.IsActive())  //Don't switch devices if the pointer is already active
            {
                //Get tracked device index from origin
                vr::InputOriginInfo_t origin_info = m_VRInput.GetOriginTrackedDeviceInfoEx(enable_laser_pointer_state.activeOrigin);

                if (origin_info.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid)
                {
                    m_LaserPointer.SetActiveDevice(origin_info.trackedDeviceIndex, dplp_activation_origin_input_binding);

                    //Disable active laser pointer override so the device's pointer can be used right away
                    m_MouseIgnoreMoveEvent = false;
                    ResetMouseLastLaserPointerPos();
                }
            }
        }
        else if (m_LaserPointer.GetActivationOrigin() == dplp_activation_origin_input_binding)
        {
            m_LaserPointer.ClearActiveDevice();
        }
    }

    m_VRInput.HandleGlobalActionShortcuts(*this);

    //Finish up pending keyboard input collected into the queue
    m_InputSim.KeyboardTextFinish();

    HandleHotkeys();
    HandleKeyboardAutoVisibility();

    //Update position if necessary
    bool dashboard_origin_was_updated = false;
    if (HasDashboardMoved()) //The dashboard can move from events we can't detect, like putting the HMD back on, so we check manually as a workaround
    {
        UpdateDashboardHMD_Y();
        dashboard_origin_was_updated = true;
    }

    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);
        Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
        const OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

        if (data.ConfigBool[configid_bool_overlay_enabled])
        {
            if (overlay.IsVisible())
            {
                if (m_OverlayDragger.GetDragOverlayID() == overlay.GetID())
                {
                    if (m_OverlayDragger.IsDragActive())
                    {
                        m_OverlayDragger.DragUpdate();
                    }
                    else if (m_OverlayDragger.IsDragGestureActive())
                    {
                        m_OverlayDragger.DragGestureUpdate();
                    }
                }
                else if (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_hmd_floor)
                {
                    DetachedTransformUpdateHMDFloor();
                }
                else if ( (dashboard_origin_was_updated) && (m_OverlayDragger.GetDragDeviceID() == -1) && (!m_OverlayDragger.IsDragGestureActive()) && 
                          (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_dashboard) )
                {
                    ApplySettingTransform();
                }
            }

            DetachedOverlayGazeFade();
        }
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

    DetachedInteractionAutoToggleAll();
    DetachedOverlayAutoDockingAll();

    m_LaserPointer.Update();

    //Hack:
    //We don't get an event when the Theater Screen close button is being used, which puts it in the dashboard (we don't want this), 
    //but IsActiveDashboardOverlay() is true if it's visible on the screen so it doesn't help either to detected this.
    //So we instead check if its middle transform gets too close to the dashboard (transform is based on Theater Screen if it's on there, otherwise the dashboard tab). 
    //The case of this happening in normal use is fairly low, so it'll have to do for now
    if (OverlayManager::Get().GetTheaterOverlayID() != k_ulOverlayID_None)
    {
        if (vr::VROverlay()->IsDashboardVisible())
        {
            vr::VROverlayHandle_t system_dashboard;
            vr::VROverlay()->FindOverlay("system.systemui", &system_dashboard);

            const Matrix4 mat_dashboard = m_OverlayDragger.GetBaseOffsetMatrix(ovrl_origin_dashboard);
            const Matrix4 mat_theater   = OverlayManager::Get().GetOverlayMiddleTransform(OverlayManager::Get().GetTheaterOverlayID());

            Vector3 pos_dashboard = mat_dashboard.getTranslation();
            Vector3 pos_theater   = mat_theater.getTranslation();
            pos_dashboard.y = pos_theater.y;

            float distance = pos_dashboard.distance(pos_theater);

            //Distance should be between 0.009f and 0.019f (SteamVR 2 dashboard) in practice so this hopefully has a big enough tolerance
            if (distance < 0.05f)
            {
                vr::VROverlay()->ShowDashboard("elvissteinjr.DesktopPlusDashboard");    //Defaults to Steam tab so try to get it to display ours instead
                SetOverlayEnabled(OverlayManager::Get().GetTheaterOverlayID(), false);
            }
        }
    }

    //Handle delayed dashboard dummy updates
    if ( (m_PendingDashboardDummyHeight != 0.0f) && (m_LastApplyTransformTick + 100 < ::GetTickCount64()) )
    {
        UpdatePendingDashboardDummyHeight();
    }

    FinishQueuedOverlayRemovals();

    return false;
}

void OutputManager::OnOpenVRMouseEvent(const vr::VREvent_t& vr_event, unsigned int& current_overlay_old)
{
    const Overlay& overlay_current = OverlayManager::Get().GetCurrentOverlay();
    const OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
    const bool use_pen = ConfigManager::GetValue(configid_bool_input_mouse_simulate_pen_input);

    switch (vr_event.eventType)
    {
        case vr::VREvent_MouseMove:
        {
            if ( (!data.ConfigBool[configid_bool_overlay_input_enabled]) || ( (m_MouseIgnoreMoveEvent) && (overlay_current.GetTextureSource() != ovrl_texsource_browser) ) ||
                 (ConfigManager::GetValue(configid_bool_state_overlay_dragmode)) || (ConfigManager::GetValue(configid_bool_state_overlay_selectmode)) || 
                 (m_OverlayDragger.IsDragActive()) || (overlay_current.GetTextureSource() == ovrl_texsource_none) || (overlay_current.GetTextureSource() == ovrl_texsource_ui) )
            {
                break;
            }

            Vector2 event_mouse_pos(vr_event.data.mouse.x, vr_event.data.mouse.y);

            //Smooth input
            if (ConfigManager::GetValue(configid_int_input_mouse_input_smoothing_level) != 0)
            {
                m_MouseLaserPointerSmoother.ApplyPresetSettings(ConfigManager::GetValue(configid_int_input_mouse_input_smoothing_level));

                event_mouse_pos = m_MouseLaserPointerSmoother.Filter(event_mouse_pos);
            }

            //Offset depending on capture source
            int content_height = data.ConfigInt[configid_int_overlay_state_content_height];
            int offset_x = 0;
            int offset_y = 0;

            if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture)
            {
                int desktop_id = data.ConfigInt[configid_int_overlay_winrt_desktop_id];

                if (desktop_id != -2) //Desktop capture through WinRT
                {
                    if ( (desktop_id >= 0) && (desktop_id < m_DesktopRects.size()) )
                    {
                        offset_x = m_DesktopRects[desktop_id].GetTL().x;
                        offset_y = m_DesktopRects[desktop_id].GetTL().y;
                    }
                    else if (desktop_id == -1) //Combined desktop
                    {
                        content_height = m_DesktopRectTotal.GetHeight();
                        offset_x = m_DesktopRectTotal.GetTL().x;
                        offset_y = m_DesktopRectTotal.GetTL().y;
                    }
                }
                else //Window capture
                {
                    HWND window_handle = (HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd];

                    //Get position of the window
                    RECT window_rect = {0};

                    if (::DwmGetWindowAttribute(window_handle, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect, sizeof(window_rect)) == S_OK)
                    {
                        offset_x = window_rect.left;
                        offset_y = window_rect.top;
                    }
                }
            }
            else if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication)
            {
                content_height = m_DesktopHeight;
                offset_x = m_DesktopX;
                offset_y = m_DesktopY;
            }

            //SteamVR ignores 3D properties when adjusting coordinates for custom UV values, so we need to add offsets manually
            if (data.ConfigBool[configid_bool_overlay_3D_enabled])
            {
                Overlay3DMode mode_3D = (Overlay3DMode)data.ConfigInt[configid_int_overlay_3D_mode];

                if (mode_3D >= ovrl_3Dmode_hou)
                {
                    offset_x += overlay_current.GetValidatedCropRect().GetTL().x;
                }
                else
                {
                    offset_x += overlay_current.GetValidatedCropRect().GetTL().x / 2;
                }
            }

            //GL space (0,0 is bottom left), so we need to flip that around (not correct for browser overlays, but also not relevant for how the values are used with them right now)
            int pointer_x =   round(event_mouse_pos.x) + offset_x;
            int pointer_y = (-round(event_mouse_pos.y) + content_height) + offset_y;

            //If double click assist is current active, check if there was an obviously deliberate movement and cancel it then
            if ( (ConfigManager::GetValue(configid_int_state_mouse_dbl_click_assist_duration_ms) != 0) &&
                 (::GetTickCount64() < m_MouseLastClickTick + ConfigManager::GetValue(configid_int_state_mouse_dbl_click_assist_duration_ms)) )
            {
                if ((abs(pointer_x - m_MouseLastLaserPointerX) > 64) || (abs(pointer_y - m_MouseLastLaserPointerY) > 64))
                {
                    m_MouseLastClickTick = 0;
                }
                else //But if not, still block the movement
                {
                    m_MouseLastLaserPointerMoveBlocked = true;
                    break;
                }
            }

            //If browser overlay, pass event along and skip the rest
            if (overlay_current.GetTextureSource() == ovrl_texsource_browser)
            {
                DPBrowserAPIClient::Get().DPBrowser_MouseMove(overlay_current.GetHandle(), pointer_x, pointer_y);
                m_MouseLastLaserPointerX = pointer_x;
                m_MouseLastLaserPointerY = pointer_y;

                break;
            }

            //Check if this mouse move would start a drag of a maximized window's title bar
            if ( (ConfigManager::GetValue(configid_int_windows_winrt_dragging_mode) != window_dragging_none) && 
                 (overlay_current.GetTextureSource() == ovrl_texsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0) )
            {
                if (WindowManager::Get().WouldDragMaximizedTitleBar((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd],
                                                                    m_MouseLastLaserPointerX, m_MouseLastLaserPointerY, pointer_x, pointer_y))
                {
                    //Reset input and WindowManager state manually to block the drag but still move the cursor on the next mouse move event
                    (use_pen) ? m_InputSim.PenSetPrimaryDown(false) : m_InputSim.MouseSetLeftDown(false);
                    
                    WindowManager::Get().SetTargetWindow(nullptr);

                    //Start overlay drag if setting enabled
                    if (ConfigManager::GetValue(configid_int_windows_winrt_dragging_mode) == window_dragging_overlay)
                    {
                        if (data.ConfigInt[configid_int_overlay_origin] != ovrl_origin_theater_screen)
                        {
                            if (!data.ConfigBool[configid_bool_overlay_transform_locked])
                            {
                                m_OverlayDragger.DragStart(OverlayManager::Get().GetCurrentOverlayID());
                            }
                            else
                            {
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 1);
                            }
                        }
                        else
                        {
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 2);
                        }
                    }

                    break; //We're not moving the cursor this time, get out
                }
            }

            //Check coordinates if laser pointer override is enabled, unless left mouse is held down by the laser pointer
            if ( (m_MouseLeftDownOverlayID == k_ulOverlayID_None) && (ConfigManager::GetValue(configid_bool_input_mouse_allow_pointer_override)) )
            {
                POINT pt;
                ::GetCursorPos(&pt);

                //Only check for override if the last laser pointer position was inside a desktop (outside coordinates are possible via extended laser drag and combined desktop edge cases)
                bool do_check_for_override = false;
                const Vector2Int laser_pos(m_MouseLastLaserPointerX, m_MouseLastLaserPointerY);
                for (const DPRect& rect : m_DesktopRects)
                {
                    if (rect.Contains(laser_pos))
                    {
                        do_check_for_override = true;
                        break;
                    }
                }

                //Simulated pen input does not move the mouse cursor when an application supports pen input directly, which makes the normal check result in false positives
                //In this case we only check for override when the mouse cursor position changed compared to last time
                //While it might seem to make sense to always do this, it actually makes it harder to trigger the override during mouse simulation
                if (ConfigManager::GetValue(configid_bool_input_mouse_simulate_pen_input))
                {
                    static POINT pt_last = {0};
                    if ((pt.x == pt_last.x) && (pt.y == pt_last.y))
                    {
                        do_check_for_override = false;
                    }

                    pt_last = pt;
                }

                //Check if the cursor is near the corner of one of the desktops and opt out of the pointer override to avoid sticky mouse corners triggering it
                //It can still happen sometimes, but this catches most cases
                if (do_check_for_override)
                {
                    //Only do for combined desktops though
                    if ( ((data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication) && (data.ConfigInt[configid_int_overlay_desktop_id] == -1)) ||
                         ((data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && (data.ConfigInt[configid_int_overlay_winrt_desktop_id] == -1)) )
                    {
                        const Vector2Int cursor_pos(pt.x, pt.y);
                        for (const DPRect& rect : m_DesktopRects)
                        {
                            if ((fabs(cursor_pos.distance(rect.GetTL())) < 64.0f) || (fabs(cursor_pos.distance(rect.GetTR())) < 64.0f) ||
                                (fabs(cursor_pos.distance(rect.GetBL())) < 64.0f) || (fabs(cursor_pos.distance(rect.GetBR())) < 64.0f))
                            {
                                do_check_for_override = false;
                                break;
                            }
                        }
                    }
                }

                //If mouse coordinates are not what the last laser pointer was (with tolerance), meaning some other source moved it
                if ((do_check_for_override) && ((abs(pt.x - m_MouseLastLaserPointerX) > 32) || (abs(pt.y - m_MouseLastLaserPointerY) > 32)))
                {
                    m_MouseIgnoreMoveEventMissCount++; //GetCursorPos() may lag behind or other jumps may occasionally happen. We count up a few misses first before acting on them

                    int max_miss_count = 10; //Arbitrary number, but appears to work reliably

                    //Reduce max miss count to 1 for simulated pen input
                    if (ConfigManager::GetValue(configid_bool_input_mouse_simulate_pen_input))
                    {
                        max_miss_count = 1;
                    }
                    else
                    {
                        //When updates are limited, try adapting for the lower update rate
                        if (m_PerformanceUpdateLimiterDelay.QuadPart != 0)
                        {
                            max_miss_count = std::max(1, max_miss_count - int((m_PerformanceUpdateLimiterDelay.QuadPart / 1000) / 20));
                        }
                    }

                    if (m_MouseIgnoreMoveEventMissCount > max_miss_count)
                    {
                        m_MouseIgnoreMoveEvent = true;

                        //Set flag for all overlays
                        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                        {
                            const Overlay& overlay = OverlayManager::Get().GetOverlay(i);

                            if ( (overlay.GetTextureSource() != ovrl_texsource_none) && (overlay.GetTextureSource() != ovrl_texsource_ui) && (overlay.GetTextureSource() != ovrl_texsource_browser) )
                            {
                                vr::VROverlay()->SetOverlayFlag(overlay.GetHandle(), vr::VROverlayFlags_HideLaserIntersection, true);
                            }
                        }
                    }
                    break;
                }
                else
                {
                    m_MouseIgnoreMoveEventMissCount = 0;
                }
            }

            //To improve compatibility with dragging certain windows around, simulate a small movement first before fully unlocking the cursor from double-click assist
            if (m_MouseLastLaserPointerMoveBlocked) 
            {
                //Move a single pixel in the direction of the new pointer position
                pointer_x = m_MouseLastLaserPointerX + sgn(pointer_x - m_MouseLastLaserPointerX);
                pointer_y = m_MouseLastLaserPointerY + sgn(pointer_y - m_MouseLastLaserPointerY);

                (use_pen) ? m_InputSim.PenMove(pointer_x, pointer_y) : m_InputSim.MouseMove(pointer_x, pointer_y);

                m_MouseLastLaserPointerMoveBlocked = false;
                //Real movement continues on the next mouse move event
            }
            else
            {
                //Finally do the actual cursor movement if we're still here
                (use_pen) ? m_InputSim.PenMove(pointer_x, pointer_y) : m_InputSim.MouseMove(pointer_x, pointer_y);

                m_MouseLastLaserPointerX = pointer_x;
                m_MouseLastLaserPointerY = pointer_y;
            }

            //This is only relevant when limiting updates. See Update() for details.
            m_MouseLaserPointerUsedLastUpdate = true;

            break;
        }
        case vr::VREvent_MouseButtonDown:
        {
            ActionManager& action_manager = ConfigManager::Get().GetActionManager();

            if (ConfigManager::GetValue(configid_bool_state_overlay_selectmode))
            {
                if (vr_event.data.mouse.button == vr::VRMouseButton_Left)
                {
                    //Select this as current overlay
                    IPCManager::Get().PostConfigMessageToUIApp(configid_int_interface_overlay_current_id, overlay_current.GetID());
                    current_overlay_old = overlay_current.GetID(); //Set a new reset value since we're in the middle of a temporary current overlay loop
                }
                break;
            }
            else if (ConfigManager::GetValue(configid_bool_state_overlay_dragmode))
            {
                if (vr_event.data.mouse.button == vr::VRMouseButton_Left)
                {
                    if (m_OverlayDragger.GetDragDeviceID() == -1)
                    {
                        if (data.ConfigInt[configid_int_overlay_origin] != ovrl_origin_theater_screen)
                        {
                            if (!data.ConfigBool[configid_bool_overlay_transform_locked])
                            {
                                m_OverlayDragger.DragStart(OverlayManager::Get().GetCurrentOverlayID());
                            }
                            else
                            {
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 1);
                            }
                        }
                        else
                        {
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 2);
                        }
                    }
                }
                else if (vr_event.data.mouse.button == vr::VRMouseButton_Right)
                {
                    if (!m_OverlayDragger.IsDragGestureActive())
                    {
                        if (data.ConfigInt[configid_int_overlay_origin] != ovrl_origin_theater_screen)
                        {
                            if (!data.ConfigBool[configid_bool_overlay_transform_locked])
                            {
                                m_OverlayDragger.DragGestureStart(OverlayManager::Get().GetCurrentOverlayID());
                            }
                            else
                            {
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
                                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 1);
                            }
                        }
                        else
                        {
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_device, ConfigManager::Get().GetPrimaryLaserPointerDevice());
                            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 2);
                        }
                    }
                }
                break;
            }

            //Set focused ID when clicking on an overlay
            ConfigManager::Get().SetValue(configid_int_state_overlay_focused_id, (int)overlay_current.GetID());
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_focused_id, (int)overlay_current.GetID());

            if (overlay_current.GetTextureSource() == ovrl_texsource_browser)
            {
                if (vr_event.data.mouse.button <= vr::VRMouseButton_Middle)
                {
                    m_MouseLastClickTick = ::GetTickCount64();

                    DPBrowserAPIClient::Get().DPBrowser_MouseDown(overlay_current.GetHandle(), (vr::EVRMouseButton)vr_event.data.mouse.button);
                }
                else
                {
                    switch (vr_event.data.mouse.button)
                    {
                        case VRMouseButton_DP_Aux01: action_manager.StartAction(ConfigManager::GetValue(configid_handle_input_go_back_action_uid), overlay_current.GetID()); break;
                        case VRMouseButton_DP_Aux02: action_manager.StartAction(ConfigManager::GetValue(configid_handle_input_go_home_action_uid), overlay_current.GetID()); break;
                    }
                }
                break;
            }
            else if ((overlay_current.GetTextureSource() == ovrl_texsource_none) || (overlay_current.GetTextureSource() == ovrl_texsource_ui))
            {
                break;
            }

            if (m_MouseIgnoreMoveEvent) //This can only be true if AllowPointerOverride enabled
            {
                m_MouseIgnoreMoveEvent = false;

                ResetMouseLastLaserPointerPos();
                ApplySettingMouseInput();

                //If it's a WinRT window capture, also check for window management stuff that would've happened on overlay focus enter otherwise
                if ( (overlay_current.GetTextureSource() == ovrl_texsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0) )
                {
                    if (ConfigManager::GetValue(configid_bool_windows_winrt_auto_focus))
                    {
                        WindowManager::Get().RaiseAndFocusWindow((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd], &m_InputSim);
                    }
                }

                break;  //Click to restore shouldn't generate a mouse click
            }

            //If a WindowManager drag event could occur, set the current window for it
            if ( (vr_event.data.mouse.button == vr::VRMouseButton_Left) && (overlay_current.GetTextureSource() == ovrl_texsource_winrt_capture) && 
                 (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0) )
            {
                WindowManager::Get().SetTargetWindow((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd], overlay_current.GetID());
            }

            if (vr_event.data.mouse.button <= vr::VRMouseButton_Middle)
            {
                m_MouseLastClickTick = ::GetTickCount64();

                if (vr_event.data.mouse.button == vr::VRMouseButton_Left)
                {
                    m_MouseLeftDownOverlayID = overlay_current.GetID();

                    if (ConfigManager::GetValue(configid_bool_input_keyboard_auto_show_desktop))
                    {
                        WindowManager::Get().OnTextInputLeftMouseClick();
                    }
                }
            }

            switch (vr_event.data.mouse.button)
            {
                case vr::VRMouseButton_Left:   (use_pen) ? m_InputSim.PenSetPrimaryDown(true)   : m_InputSim.MouseSetLeftDown(true);   break;
                case vr::VRMouseButton_Right:  (use_pen) ? m_InputSim.PenSetSecondaryDown(true) : m_InputSim.MouseSetRightDown(true);  break;
                case vr::VRMouseButton_Middle:             m_InputSim.MouseSetMiddleDown(true);                                        break;
                case VRMouseButton_DP_Aux01:   action_manager.StartAction(ConfigManager::GetValue(configid_handle_input_go_back_action_uid), overlay_current.GetID()); break;
                case VRMouseButton_DP_Aux02:   action_manager.StartAction(ConfigManager::GetValue(configid_handle_input_go_home_action_uid), overlay_current.GetID()); break;
            }

            break;
        }
        case vr::VREvent_MouseButtonUp:
        {
            ActionManager& action_manager = ConfigManager::Get().GetActionManager();

            if (ConfigManager::GetValue(configid_bool_state_overlay_selectmode))
            {
                break;
            }
            else if (ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp))
            {
                //Temp drag doesn't have proper overlay focus so can be called on any overlay if it's in front. Finish drag in any case.
                if (vr_event.data.mouse.button == vr::VRMouseButton_Left)
                {
                    //Don't actually react to this unless it's been a few milliseconds
                    //That way releasing the trigger after the UI selection instead of holding it doesn't just drop the overlay on the spot
                    if (::GetTickCount64() >= m_OvrlTempDragStartTick + 250)
                    {
                        OnDragFinish();
                        DetachedTempDragFinish();

                        break;
                    }
                }
            }
            else if ( (m_OverlayDragger.GetDragOverlayID() == overlay_current.GetID()) && ( (m_OverlayDragger.IsDragActive()) || (m_OverlayDragger.IsDragGestureActive()) ) )
            {
                if ((vr_event.data.mouse.button == vr::VRMouseButton_Left) && (m_OverlayDragger.IsDragActive()))
                {
                    OnDragFinish();
                    m_OverlayDragger.DragFinish();

                    ApplySettingTransform();
                }
                else if ((vr_event.data.mouse.button == vr::VRMouseButton_Right) && (m_OverlayDragger.IsDragGestureActive()))
                {
                    m_OverlayDragger.DragGestureFinish();

                    ApplySettingTransform();
                }

                break;
            }
            else if (overlay_current.GetTextureSource() == ovrl_texsource_browser)
            {
                if (vr_event.data.mouse.button <= vr::VRMouseButton_Middle)
                {
                    DPBrowserAPIClient::Get().DPBrowser_MouseUp(overlay_current.GetHandle(), (vr::EVRMouseButton)vr_event.data.mouse.button);
                }
                else
                {
                    switch (vr_event.data.mouse.button)
                    {
                        case VRMouseButton_DP_Aux01: action_manager.StopAction(ConfigManager::GetValue(configid_handle_input_go_back_action_uid), overlay_current.GetID()); break;
                        case VRMouseButton_DP_Aux02: action_manager.StopAction(ConfigManager::GetValue(configid_handle_input_go_home_action_uid), overlay_current.GetID()); break;
                    }
                }

                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 0);
                break;
            }
            else if ((overlay_current.GetTextureSource() == ovrl_texsource_none) || (overlay_current.GetTextureSource() == ovrl_texsource_ui))
            {
                IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 0);
                break;
            }

            switch (vr_event.data.mouse.button)
            {
                case vr::VRMouseButton_Left:   (use_pen) ? m_InputSim.PenSetPrimaryDown(false)   : m_InputSim.MouseSetLeftDown(false);  break;
                case vr::VRMouseButton_Right:  (use_pen) ? m_InputSim.PenSetSecondaryDown(false) : m_InputSim.MouseSetRightDown(false); break;
                case vr::VRMouseButton_Middle:             m_InputSim.MouseSetMiddleDown(false);                                        break;
                case VRMouseButton_DP_Aux01:   action_manager.StopAction(ConfigManager::GetValue(configid_handle_input_go_back_action_uid), overlay_current.GetID()); break;
                case VRMouseButton_DP_Aux02:   action_manager.StopAction(ConfigManager::GetValue(configid_handle_input_go_home_action_uid), overlay_current.GetID()); break;
            }

            //If there was a possible WindowManager drag event prepared for, reset the target window
            if ( (vr_event.data.mouse.button == vr::VRMouseButton_Left) && (overlay_current.GetTextureSource() == ovrl_texsource_winrt_capture) && 
                (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0) )
            {
                WindowManager::Get().SetTargetWindow(nullptr);
            }

            if (vr_event.data.mouse.button == vr::VRMouseButton_Left)
            {
                m_MouseLeftDownOverlayID = k_ulOverlayID_None;
            }

            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_drag_hint_type, 0);

            break;
        }
        case vr::VREvent_ScrollDiscrete:
        case vr::VREvent_ScrollSmooth:
        {
            //Discrete scroll events are sent at a fixed frame interval for the system laser pointer and at a fixed interval relative to SteamVR Input action set update calls for Desktop+ pointer
            //This can result in vastly different scroll rates at different frame or update rates
            //We counteract this by scaling the sent scroll values by the delta between the events to achieve a constant scroll rate
            float scroll_step_multiplier = 1.0f;

            if (vr_event.eventType == vr::VREvent_ScrollDiscrete)
            {
                LARGE_INTEGER scroll_delta = {0};
                const float scroll_step_ms = 58.31f; //7 frame tick of a 120 Hz HMD... hardly universal, but going with that for now.

                ::QueryPerformanceCounter(&scroll_delta);
                scroll_delta.QuadPart -= m_MouseLaserPointerScrollDeltaStart.QuadPart;
                scroll_delta.QuadPart *= 1000000;
                scroll_delta.QuadPart /= m_MouseLaserPointerScrollDeltaFrequency.QuadPart;

                scroll_step_multiplier = scroll_delta.QuadPart / (1000.0f * scroll_step_ms);

                //We typically don't need more than 2x, so treat everything higher as interrupted scrolling and use 1x for them
                if (scroll_step_multiplier > 2.0f)
                {
                    scroll_step_multiplier = 1.0f;
                }

                ::QueryPerformanceCounter(&m_MouseLaserPointerScrollDeltaStart);
            }

            const float xdelta = vr_event.data.scroll.xdelta * scroll_step_multiplier;  //Discrete scrolling will never have X as non-0, but just in case this ever changes
            const float ydelta = vr_event.data.scroll.ydelta * scroll_step_multiplier;

            //Check deadzone
            const float xdelta_abs = fabs(xdelta);
            const float ydelta_abs = fabs(ydelta);
            const bool do_scroll_h = (xdelta_abs > 0.025f);
            const bool do_scroll_v = (ydelta_abs > 0.025f);

            //Drag-mode scroll
            if (m_OverlayDragger.IsDragActive())
            {
                //Block drag scroll actions when a temp drag just started to avoid mis-inputs
                if (::GetTickCount64() <= m_OvrlTempDragStartTick + 250)
                    break;

                //Additional deadzone
                if ((xdelta_abs > 0.05f) || (ydelta_abs > 0.05f))
                {
                    //Add distance as long as y-delta input is bigger
                    if (xdelta_abs < ydelta_abs)
                    {
                        m_OverlayDragger.DragAddDistance(ydelta);
                    }
                    else
                    {
                        m_OverlayDragger.DragAddWidth(xdelta * -0.25f);
                    }
                }

                break;
            }

            //Overlay scrolls
            if ( (ConfigManager::GetValue(configid_bool_state_overlay_dragmode)) || (ConfigManager::GetValue(configid_bool_state_overlay_selectmode)) || 
                 (overlay_current.GetTextureSource() == ovrl_texsource_none) || (overlay_current.GetTextureSource() == ovrl_texsource_ui) )
            {
                break;
            }
            else if (overlay_current.GetTextureSource() == ovrl_texsource_browser)
            {
                if ((do_scroll_h) || (do_scroll_v))
                {
                    DPBrowserAPIClient::Get().DPBrowser_Scroll(overlay_current.GetHandle(), (do_scroll_h) ? xdelta : 0.0f, (do_scroll_v) ? ydelta : 0.0f);
                }
                break;
            }

            //Overlay drag-scroll window-title-pull creation... thingy
            if (do_scroll_v)
            {
                bool is_desktop_overlay = (  (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication) ||
                                            ((data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) && (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] == 0)) );

                const float delta_trigger_min = (ConfigManager::GetValue(configid_bool_input_mouse_scroll_smooth)) ? -0.16f : 0.9f;

                if ( (is_desktop_overlay) && (m_MouseLeftDownOverlayID == overlay_current.GetID()) && (ydelta <= delta_trigger_min) )
                {
                    HWND current_window = ::GetForegroundWindow();

                    if ( (WindowManager::Get().IsHoveringCapturableTitleBar(current_window, m_MouseLastLaserPointerX, m_MouseLastLaserPointerY)) )
                    {
                        vr::TrackedDeviceIndex_t device_index = ConfigManager::Get().GetPrimaryLaserPointerDevice();

                        //If no dashboard device, try finding one
                        if (device_index == vr::k_unTrackedDeviceIndexInvalid)
                        {
                            device_index = vr::IVROverlayEx::FindPointerDeviceForOverlay(overlay_current.GetHandle());
                        }

                        float source_distance = 1.0f;
                        float target_width = 0.3f;
                        vr::VROverlayIntersectionResults_t results;

                        if (vr::IVROverlayEx::ComputeOverlayIntersectionForDevice(overlay_current.GetHandle(), device_index, vr::TrackingUniverseStanding, &results))
                        {
                            source_distance = results.fDistance;

                            //Get window width in meters relative to the source overlay width
                            RECT window_rect = {0};

                            if (::DwmGetWindowAttribute(current_window, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect, sizeof(window_rect)) == S_OK)
                            {
                                target_width = (float(window_rect.right - window_rect.left) / overlay_current.GetValidatedCropRect().GetWidth()) * data.ConfigFloat[configid_float_overlay_width];
                                target_width = std::max(target_width, 0.2f); //Don't let it become too tiny
                            }
                        }

                        //Set device as hint, just in case
                        ConfigManager::SetValue(configid_int_state_laser_pointer_device_hint, (int)device_index);
                        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_laser_pointer_device_hint, (int)device_index);

                        //Send to UI
                        IPCManager::Get().PostConfigMessageToUIApp(configid_handle_state_arg_hwnd, (LPARAM)current_window);
                        IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_overlay_new_drag, MAKELPARAM(-2, 0 /*UI doesn't need distance*/));

                        //Reset input and WindowManager state manually since the overlay mouse up even will be consumed to finish the drag later
                        m_InputSim.MouseSetLeftDown(false);
                        WindowManager::Get().SetTargetWindow(nullptr);

                        //Start drag
                        AddOverlayDrag(source_distance, ovrl_capsource_winrt_capture, -2, current_window, target_width);

                        break;
                    }
                }

            }

            //Normal scrolling
            if (do_scroll_v)
            {
                m_InputSim.MouseWheelVertical(ydelta);
            }

            if (do_scroll_h)
            {
                m_InputSim.MouseWheelHorizontal(-xdelta);
            }

            break;
        }
    }
}

void OutputManager::HandleKeyboardMessage(IPCActionID ipc_action_id, LPARAM lparam)
{
    switch (ipc_action_id)
    {
        case ipcact_keyboard_vkey:
        {
            m_InputSim.KeyboardSetKeyState((IPCKeyboardKeystateFlags)LOWORD(lparam), HIWORD(lparam));
            break;
        }
        case ipcact_keyboard_wchar:
        {
            wchar_t wchar =  LOWORD(lparam);
            bool down     = (HIWORD(lparam) == 1);

            //Check if it can be pressed on the current windows keyboard layout
            SHORT w32_keystate = ::VkKeyScanW(wchar);
            unsigned char keycode = LOBYTE(w32_keystate);
            unsigned char flags   = HIBYTE(w32_keystate);

            bool is_valid = ((keycode != 255) || (flags != 255));

            if (is_valid)
            {
                m_InputSim.KeyboardSetFromWin32KeyState(w32_keystate, down);
            }
            else if (down) //Otherwise use it as a text input
            {
                wchar_t wstr[2] = {0};
                wstr[0] = wchar;

                m_InputSim.KeyboardText(StringConvertFromUTF16(wstr).c_str(), true);
            }
            break;
        }
        default: return;
    }
}

bool OutputManager::HandleOverlayProfileLoadMessage(LPARAM lparam)
{
    IPCActionOverlayProfileLoadArg profile_load_arg = (IPCActionOverlayProfileLoadArg)LOWORD(lparam);
    int profile_overlay_id = GET_Y_LPARAM(lparam);

    int desktop_id_prev = ConfigManager::GetValue(configid_int_overlay_desktop_id);
    const std::string& profile_name = ConfigManager::GetValue(configid_str_state_profile_name_load);

    if (profile_overlay_id == -2)
    {
        ConfigManager::Get().LoadOverlayProfileDefault(true);
    }
    else if (profile_load_arg == ipcactv_ovrl_profile_multi)
    {
        ConfigManager::Get().LoadMultiOverlayProfileFromFile(profile_name + ".ini", true);
    }
    else if (profile_load_arg == ipcactv_ovrl_profile_multi_add)
    {
        if (profile_overlay_id == -1)  //Load queued up overlays
        {
            //Usually, elements should have been sent in order, but lets make sure they really are
            std::sort(m_ProfileAddOverlayIDQueue.begin(), m_ProfileAddOverlayIDQueue.end());

            std::vector<char> ovrl_inclusion_list;
            ovrl_inclusion_list.reserve(m_ProfileAddOverlayIDQueue.back());

            //Build overlay inclusion list
            for (int ovrl_id : m_ProfileAddOverlayIDQueue)
            {
                if ((int)ovrl_inclusion_list.size() > ovrl_id)      //Skip if already in list (double entry)
                {
                    continue;
                }

                while ((int)ovrl_inclusion_list.size() < ovrl_id)   //Exclude overlays not queued
                {
                    ovrl_inclusion_list.push_back(0);
                }

                ovrl_inclusion_list.push_back(1);                   //Include ovrl_id
            }

            ConfigManager::Get().LoadMultiOverlayProfileFromFile(profile_name + ".ini", false, &ovrl_inclusion_list);

            m_ProfileAddOverlayIDQueue.clear();
        }
        else if ( (profile_overlay_id >= 0) && (profile_overlay_id < 1000) )  //Queue up overlays
        {
            m_ProfileAddOverlayIDQueue.push_back(profile_overlay_id);
            return false;
        }
    }

    //Reset mirroing entirely if desktop was changed (only in single desktop mode)
    if ( (ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring)) && (ConfigManager::GetValue(configid_int_overlay_desktop_id) != desktop_id_prev) )
        return true; //Reset mirroring

    ResetOverlays(); //This does everything relevant

    return false;
}

void OutputManager::ResetMouseLastLaserPointerPos()
{
    //Set last pointer values to current to not trip the movement detection up
    POINT pt;
    ::GetCursorPos(&pt);
    m_MouseLastLaserPointerX = pt.x;
    m_MouseLastLaserPointerY = pt.y;

    //Also reset this state which may be left unclean when window drags get triggered
    m_MouseLeftDownOverlayID = k_ulOverlayID_None;
}

void OutputManager::CropToActiveWindow()
{
    bool& crop_enabled = ConfigManager::GetRef(configid_bool_overlay_crop_enabled);
    int& crop_x        = ConfigManager::GetRef(configid_int_overlay_crop_x);
    int& crop_y        = ConfigManager::GetRef(configid_int_overlay_crop_y);
    int& crop_width    = ConfigManager::GetRef(configid_int_overlay_crop_width);
    int& crop_height   = ConfigManager::GetRef(configid_int_overlay_crop_height);

    if (CropToActiveWindow(crop_x, crop_y, crop_width, crop_height))
    {
        crop_enabled = true;

        //Send them over to UI
        IPCManager::Get().PostConfigMessageToUIApp(configid_bool_overlay_crop_enabled, crop_enabled);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_crop_x,        crop_x);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_crop_y,        crop_y);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_crop_width,    crop_width);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_crop_height,   crop_height);

        ApplySettingCrop();
        ApplySettingTransform();
        ApplySettingMouseScale();
    }
}

void OutputManager::CropToDisplay(int display_id, bool do_not_apply_setting)
{
    int& crop_x      = ConfigManager::GetRef(configid_int_overlay_crop_x);
    int& crop_y      = ConfigManager::GetRef(configid_int_overlay_crop_y);
    int& crop_width  = ConfigManager::GetRef(configid_int_overlay_crop_width);
    int& crop_height = ConfigManager::GetRef(configid_int_overlay_crop_height);

    CropToDisplay(display_id, crop_x, crop_y, crop_width, crop_height);

    //Send change to UI as well (also set override since this may be called during one)
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, OverlayManager::Get().GetCurrentOverlayID());
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_crop_x,      crop_x);
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_crop_y,      crop_y);
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_crop_width,  crop_width);
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_crop_height, crop_height);
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);

    //In single desktop mode, set desktop ID for all overlays
    if (ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring))
    {
        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
        {
            OverlayManager::Get().GetConfigData(i).ConfigInt[configid_int_overlay_desktop_id] = display_id;

            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)i);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_desktop_id, display_id);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
        }
    }

    //Applying the setting when a duplication resets happens right after has the chance of screwing up the transform (too many transform updates?), so give the option to not do it
    if (!do_not_apply_setting)
    {
        ApplySettingCrop();
        ApplySettingTransform();
        ApplySettingMouseScale();
        ApplySettingExtraBrightness();
    }
}

void OutputManager::DuplicateOverlay(unsigned int base_id, bool is_ui_overlay)
{
    //Add overlay based on data of base_id overlay and reset it
    unsigned int new_id = k_ulOverlayID_None;

    if (!is_ui_overlay)
    {
        new_id = OverlayManager::Get().DuplicateOverlay(OverlayManager::Get().GetConfigData(base_id), base_id);
    }
    else
    {
        new_id = OverlayManager::Get().AddUIOverlay();
    }

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    OverlayManager::Get().SetCurrentOverlayID(new_id);

    if (!is_ui_overlay)
    {
        const Overlay& overlay_base = OverlayManager::Get().GetOverlay(base_id);
        Overlay& overlay_current = OverlayManager::Get().GetCurrentOverlay();

        //If base overlay is an active WinRT Capture, duplicate capture before resetting the overlay
        if (overlay_base.GetTextureSource() == ovrl_texsource_winrt_capture)
        {
            if (DPWinRT_StartCaptureFromOverlay(overlay_current.GetHandle(), overlay_base.GetHandle()))
            {
                overlay_current.SetTextureSource(ovrl_texsource_winrt_capture);
            }
        }

        //Automatically reset the matrix to a saner default by putting it next to the base overlay in most cases
        DetachedTransformReset(overlay_base.GetID());
    }
    else
    {
        DetachedTransformReset();
    }

    ResetCurrentOverlay();

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

unsigned int OutputManager::AddOverlay(OverlayCaptureSource capture_source, int desktop_id, HWND window_handle)
{
    unsigned int new_id = OverlayManager::Get().AddOverlay(capture_source, desktop_id, window_handle);

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    OverlayManager::Get().SetCurrentOverlayID(new_id);

    if (capture_source == ovrl_capsource_desktop_duplication)
    {
        CropToDisplay(desktop_id, true);
    }

    //Adjust width to a more suited default (UI does the same so no need to send over)
    ConfigManager::SetValue(configid_float_overlay_width, 1.0f);

    //Reset transform to default next to the primary dashboard overlay in most cases
    DetachedTransformReset();
    ResetCurrentOverlay();

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

    return new_id;
}

unsigned int OutputManager::AddOverlayDrag(float source_distance, OverlayCaptureSource capture_source, int desktop_id, HWND window_handle, float overlay_width)
{
    unsigned int new_id = OverlayManager::Get().AddOverlay(capture_source, desktop_id, window_handle);

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    OverlayManager::Get().SetCurrentOverlayID(new_id);

    if (capture_source == ovrl_capsource_desktop_duplication)
    {
        CropToDisplay(desktop_id, true);
    }

    //Adjust width and send it over to UI app
    ConfigManager::SetValue(configid_float_overlay_width, overlay_width);

    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)new_id);
    IPCManager::Get().PostConfigMessageToUIApp(configid_float_overlay_width, overlay_width);
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);

    //Start drag and apply overlay config
    DetachedTempDragStart(new_id, std::max(source_distance - 0.25f, 0.01f));
    ResetCurrentOverlay();

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

    return new_id;
}

void OutputManager::ApplySettingCaptureSource()
{
    Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();

    switch (ConfigManager::GetValue(configid_int_overlay_capture_source))
    {
        case ovrl_capsource_desktop_duplication:
        {
            if (!m_OutputInvalid)
            {
                OverlayTextureSource tex_source = overlay.GetTextureSource();
                if ((tex_source != ovrl_texsource_desktop_duplication) || (tex_source != ovrl_texsource_desktop_duplication_3dou_converted))
                {
                    ApplySetting3DMode(); //Sets texture source for us when capture source is desktop duplication
                }
            }
            else
            {
                overlay.SetTextureSource(ovrl_texsource_none);
            }
            break;
        }
        case ovrl_capsource_winrt_capture:
        {
            if (overlay.GetTextureSource() != ovrl_texsource_winrt_capture)
            {
                if (DPWinRT_IsCaptureFromHandleSupported())
                {
                    OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

                    if (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] != 0)
                    {
                        //Set configid_str_overlay_winrt_last_* strings from window info so returning windows can be restored later
                        const WindowInfo* window_info = WindowManager::Get().WindowListFindWindow((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd]);

                        if (window_info != nullptr)
                        {
                            data.ConfigStr[configid_str_overlay_winrt_last_window_title]      = StringConvertFromUTF16(window_info->GetTitle().c_str());
                            data.ConfigStr[configid_str_overlay_winrt_last_window_class_name] = StringConvertFromUTF16(window_info->GetWindowClassName().c_str());
                            data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name]   = window_info->GetExeName();
                        }

                        if (DPWinRT_StartCaptureFromHWND(overlay.GetHandle(), (HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd]))
                        {
                            overlay.SetTextureSource(ovrl_texsource_winrt_capture);
                            ApplySetting3DMode(); //Syncs 3D state if needed
                            ApplySettingUpdateLimiter();

                            //Pause if not visible
                            if (!overlay.IsVisible())
                            {
                                DPWinRT_PauseCapture(overlay.GetHandle(), true);
                            }

                            if (ConfigManager::GetValue(configid_bool_windows_winrt_auto_focus))
                            {
                                WindowManager::Get().RaiseAndFocusWindow((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd], &m_InputSim);
                            }
                        }
                        break;
                    }
                    else if (data.ConfigInt[configid_int_overlay_winrt_desktop_id] != -2)
                    {
                        if (DPWinRT_StartCaptureFromDesktop(overlay.GetHandle(), data.ConfigInt[configid_int_overlay_winrt_desktop_id]))
                        {
                            overlay.SetTextureSource(ovrl_texsource_winrt_capture);
                            ApplySetting3DMode();
                            ApplySettingUpdateLimiter();

                            //Pause if not visible
                            if (!overlay.IsVisible())
                            {
                                DPWinRT_PauseCapture(overlay.GetHandle(), true);
                            }
                        }
                        break;
                    }
                }

                //Couldn't set up capture, set source to none
                overlay.SetTextureSource(ovrl_texsource_none);
            }
            break;
        }
        case ovrl_capsource_ui:
        {
            //Set texture source to UI if possible, which sets the rendering PID to the UI process
            overlay.SetTextureSource(IPCManager::IsUIAppRunning() ? ovrl_texsource_ui : ovrl_texsource_none);

            break;
        }
        case ovrl_capsource_browser:
        {
            if (overlay.GetTextureSource() != ovrl_texsource_browser)
            {
                bool has_started_browser = false;

                if (DPBrowserAPIClient::Get().IsBrowserAvailable())
                {
                    OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

                    //Load placeholder texture and apply input mode now since browser startup can take a few seconds
                    vr::VROverlay()->SetOverlayFromFile(overlay.GetHandle(), (ConfigManager::Get().GetApplicationPath() + "images/browser_load.png").c_str());
                    ApplySettingInputMode();

                    if (data.ConfigInt[configid_int_overlay_duplication_id] == -1)
                    {
                        DPBrowserAPIClient::Get().DPBrowser_StartBrowser(overlay.GetHandle(), data.ConfigStr[configid_str_overlay_browser_url], 
                                                                         data.ConfigBool[configid_bool_overlay_browser_allow_transparency]);

                        DPBrowserAPIClient::Get().DPBrowser_SetResolution(overlay.GetHandle(), data.ConfigInt[configid_int_overlay_user_width], data.ConfigInt[configid_int_overlay_user_height]);
                        DPBrowserAPIClient::Get().DPBrowser_SetFPS(overlay.GetHandle(),        data.ConfigInt[configid_int_overlay_browser_max_fps_override]);
                        DPBrowserAPIClient::Get().DPBrowser_SetZoomLevel(overlay.GetHandle(),  data.ConfigFloat[configid_float_overlay_browser_zoom]);

                        has_started_browser = true;
                    }
                    else
                    {
                        const Overlay& overlay_src = OverlayManager::Get().GetOverlay((unsigned int)data.ConfigInt[configid_int_overlay_duplication_id]);

                        //Source overlay may be invalid on launch or not have texture source set up if duplication ID is higher than this overlay's ID
                        if ( (overlay_src.GetHandle() != vr::k_ulOverlayHandleInvalid) && (overlay_src.GetTextureSource() == ovrl_texsource_browser) )
                        {
                            DPBrowserAPIClient::Get().DPBrowser_DuplicateBrowserOutput(overlay_src.GetHandle(), overlay.GetHandle());
                            DPBrowserAPIClient::Get().DPBrowser_SetFPS(overlay.GetHandle(), data.ConfigInt[configid_int_overlay_browser_max_fps_override]);
                            has_started_browser = true;
                        }
                    }

                    data.ConfigInt[configid_int_overlay_state_content_width]  = data.ConfigInt[configid_int_overlay_user_width];
                    data.ConfigInt[configid_int_overlay_state_content_height] = data.ConfigInt[configid_int_overlay_user_height];

                    ApplySetting3DMode();
                    ApplySettingUpdateLimiter();

                    //Set pause state in case overlay is hidden or differs from duplication source
                    DPBrowserAPIClient::Get().DPBrowser_PauseBrowser(overlay.GetHandle(), !overlay.IsVisible());
                }

                //Set texture source to browser if possible, which sets the rendering PID to the browser server process
                overlay.SetTextureSource((has_started_browser) ? ovrl_texsource_browser : ovrl_texsource_none);
            }

            break;
        }
        default:
        {
            //Unknown capture source, perhaps from the future. Set to texture source none.
            overlay.SetTextureSource(ovrl_texsource_none);
        }
    }

    ApplySettingExtraBrightness();
}

void OutputManager::ApplySetting3DMode()
{
    const Overlay& overlay_current = OverlayManager::Get().GetCurrentOverlay();
    const OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

    vr::VROverlayHandle_t ovrl_handle = overlay_current.GetHandle();
    bool is_enabled = ConfigManager::GetValue(configid_bool_overlay_3D_enabled);
    int mode = ConfigManager::GetValue(configid_int_overlay_3D_mode);

    //Override mode to none if texsource is none or the desktop duplication output is invalid
    if ( (overlay_current.GetTextureSource() == ovrl_texsource_none) || ( (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication) && (m_OutputInvalid) ) )
    {
        is_enabled = false;
    }

    if (is_enabled)
    {
        if (data.ConfigBool[configid_bool_overlay_3D_swapped])
        {
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Parallel, false);
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Crossed, true);
        }
        else
        {
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Parallel, true);
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Crossed, false);
        }

        switch (mode)
        {
            case ovrl_3Dmode_hsbs:
            {
                vr::VROverlay()->SetOverlayTexelAspect(ovrl_handle, 2.0f);
                break;
            }
            case ovrl_3Dmode_sbs:
            case ovrl_3Dmode_ou:  //Over-Under is converted to SBS
            {
                vr::VROverlay()->SetOverlayTexelAspect(ovrl_handle, 1.0f);
                break;
            }
            case ovrl_3Dmode_hou: //Half-Over-Under is converted to SBS with half height
            {
                vr::VROverlay()->SetOverlayTexelAspect(ovrl_handle, 0.5f);
                break;
            }
            default: break;
        }
    }
    else
    {
        vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Parallel, false);
        vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Crossed, false);
        vr::VROverlay()->SetOverlayTexelAspect(ovrl_handle, 1.0f);
    }

    if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication)
    {
        if ( (is_enabled) && ((mode == ovrl_3Dmode_ou) || (mode == ovrl_3Dmode_hou)) )
        {
            OverlayManager::Get().GetCurrentOverlay().SetTextureSource(ovrl_texsource_desktop_duplication_3dou_converted);
        }
        else
        {
            OverlayManager::Get().GetCurrentOverlay().SetTextureSource(ovrl_texsource_desktop_duplication);
        }

        RefreshOpenVROverlayTexture(DPRect(-1, -1, -1, -1), true);
    }
    //WinRT OU3D state is set in ApplySettingCrop since it needs cropping values

    ApplySettingCrop();
    ApplySettingMouseScale();
}

void OutputManager::ApplySettingTransform()
{
    Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
    const OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

    vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

    //Fixup overlay visibility if needed
    //This has to be done first since there seem to be issues with moving invisible overlays
    bool should_be_visible = overlay.ShouldBeVisible();

    if ( (!should_be_visible) && (m_OvrlDashboardActive) && (m_OvrlDashboardActive) && (ConfigManager::GetValue(configid_bool_overlay_enabled)) &&
         (ConfigManager::GetValue(configid_bool_state_overlay_dragselectmode_show_hidden)) )
    {
        should_be_visible = true;
        overlay.SetOpacity(0.25f);
    }
    else if ( (!ConfigManager::GetValue(configid_bool_overlay_gazefade_enabled)) && (overlay.GetOpacity() != ConfigManager::GetValue(configid_float_overlay_opacity)) )
    {
        overlay.SetOpacity(ConfigManager::GetValue(configid_float_overlay_opacity));
        should_be_visible = overlay.ShouldBeVisible(); //Re-evaluate this in case the overlay was left hidden after deactivating gaze fade
    }

    if ( (should_be_visible) && (!overlay.IsVisible()) )
    {
        ShowOverlay(overlay.GetID());
        return;     //ShowOverlay() calls this function so we back out here
    }
    else if ( (!should_be_visible) && (overlay.IsVisible()) )
    {
        HideOverlay(overlay.GetID());
    }

    unsigned int primary_dashboard_overlay_id = OverlayManager::Get().GetPrimaryDashboardOverlay().GetID();
    bool is_primary_dashboard_overlay = (primary_dashboard_overlay_id == overlay.GetID());

    float width = ConfigManager::GetValue(configid_float_overlay_width);
    float height = 0.0f;
    float dummy_height = 0.0f;
    OverlayOrigin overlay_origin = (OverlayOrigin)ConfigManager::GetValue(configid_int_overlay_origin);

    if (is_primary_dashboard_overlay)
    {
        height = GetOverlayHeight(overlay.GetID());
        //Dashboard uses differently scaled transform depending on the current setting. We counteract that scaling to ensure the config value actually matches world scale
        dummy_height = height / GetDashboardScale();
    }
    else
    {
        //Clear theater mode if this overlay is used as source and being disabled or no longer set to theater origin
        if ((OverlayManager::Get().GetTheaterOverlayID() == overlay.GetID()) && ((!ConfigManager::GetValue(configid_bool_overlay_enabled)) || (overlay_origin != ovrl_origin_theater_screen)) )
        {
            OverlayManager::Get().ClearTheaterOverlay();
            ResetCurrentOverlay();                      //Reset overlay so all changes made to the theater one get reflected on the normal one
            overlay.AssignDesktopDuplicationTexture();  //Re-assign Desktop Duplication Texture in case it has changed during theater mode
            return;
        }
        else if ((ConfigManager::GetValue(configid_bool_overlay_enabled) && (overlay_origin == ovrl_origin_theater_screen)))   //Enable theater mode if needed
        {
            ShowTheaterOverlay(overlay.GetID());
        }
    }

    //Dashboard dummy still needs correct width/height set for the top dashboard bar above it to be visible
    if ( (is_primary_dashboard_overlay) || (primary_dashboard_overlay_id == k_ulOverlayID_None) )           //When no dashboard overlay exists we set this on every overlay, not ideal.
    {
        float old_dummy_height = 0.0f;
        vr::VROverlay()->GetOverlayWidthInMeters(m_OvrlHandleDashboardDummy, &old_dummy_height);

        dummy_height = std::max(dummy_height + 0.30f, 1.5f); //Enforce minimum height to fit default height offset (which makes space for Floating UI)

        //Sanity check. Things like inf can make the entire interface disappear
        if (dummy_height > 20.0f)
        {
            dummy_height = 1.525f;
        }

        if (dummy_height != old_dummy_height)
        {
            //Delay setting the dummy height to after we made sure ApplySettingTransform() is not called right again
            //Avoiding flicker properly unfortunately needs a sleep, so we can't call this right away or else we're just gonna stutter around
            m_PendingDashboardDummyHeight = dummy_height;
        }
    }

    //Update transform
    vr::HmdMatrix34_t matrix = {0};
    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

    switch (overlay_origin)
    {
        case ovrl_origin_room:
        {
            matrix = ConfigManager::Get().GetOverlayDetachedTransform().toOpenVR34();

            //Offset transform by additional offset values
            vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix, ConfigManager::GetValue(configid_float_overlay_offset_right),
                                                                        ConfigManager::GetValue(configid_float_overlay_offset_up),
                                                                        ConfigManager::GetValue(configid_float_overlay_offset_forward));

            vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, universe_origin, &matrix);
            break;
        }
        case ovrl_origin_hmd_floor:
        {
            DetachedTransformUpdateHMDFloor();
            break;
        }
        case ovrl_origin_seated_universe:
        {
            Matrix4 matrix_base = m_OverlayDragger.GetBaseOffsetMatrix() * ConfigManager::Get().GetOverlayDetachedTransform();

            //Offset transform by additional offset values
            matrix_base.translate_relative(ConfigManager::GetValue(configid_float_overlay_offset_right),
                                           ConfigManager::GetValue(configid_float_overlay_offset_up),
                                           ConfigManager::GetValue(configid_float_overlay_offset_forward));

            matrix = matrix_base.toOpenVR34();
            vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, vr::TrackingUniverseStanding, &matrix);
            break;
        }
        case ovrl_origin_dashboard:
        {
            Matrix4 matrix_base = m_OverlayDragger.GetBaseOffsetMatrix() * ConfigManager::Get().GetOverlayDetachedTransform();

            //Offset transform by additional offset values
            matrix_base.translate_relative(ConfigManager::GetValue(configid_float_overlay_offset_right),
                                           ConfigManager::GetValue(configid_float_overlay_offset_up),
                                           ConfigManager::GetValue(configid_float_overlay_offset_forward));

            //Apply origin offset, which basically adjusts transform to be aligned bottom-center instead of centered on both axes
            if (height == 0.0f)     //Get overlay height if it's not set yet
            {
                height = GetOverlayHeight(overlay.GetID());
            }

            matrix_base.translate_relative(0.0f, height / 2.0f, 0.0f);

            matrix = matrix_base.toOpenVR34();

            vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, universe_origin, &matrix);
            break;
        }
        case ovrl_origin_hmd:
        {
            matrix = ConfigManager::Get().GetOverlayDetachedTransform().toOpenVR34();

            //Offset transform by additional offset values
            vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix, ConfigManager::GetValue(configid_float_overlay_offset_right),
                                                                        ConfigManager::GetValue(configid_float_overlay_offset_up),
                                                                        ConfigManager::GetValue(configid_float_overlay_offset_forward));

            vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(ovrl_handle, vr::k_unTrackedDeviceIndex_Hmd, &matrix);
            break;
        }
        case ovrl_origin_right_hand:
        {
            vr::TrackedDeviceIndex_t device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

            if (device_index != vr::k_unTrackedDeviceIndexInvalid)
            {
                matrix = ConfigManager::Get().GetOverlayDetachedTransform().toOpenVR34();

                //Offset transform by additional offset values
                vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix, ConfigManager::GetValue(configid_float_overlay_offset_right),
                                                                            ConfigManager::GetValue(configid_float_overlay_offset_up),
                                                                            ConfigManager::GetValue(configid_float_overlay_offset_forward));

                vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(ovrl_handle, device_index, &matrix);
            }
            else //No controller connected, uh put it to 0?
            {
                vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, universe_origin, &matrix);
            }
            break;
        }
        case ovrl_origin_left_hand:
        {
            vr::TrackedDeviceIndex_t device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);

            if (device_index != vr::k_unTrackedDeviceIndexInvalid)
            {
                matrix = ConfigManager::Get().GetOverlayDetachedTransform().toOpenVR34();

                //Offset transform by additional offset values
                vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix, ConfigManager::GetValue(configid_float_overlay_offset_right),
                                                                            ConfigManager::GetValue(configid_float_overlay_offset_up),
                                                                            ConfigManager::GetValue(configid_float_overlay_offset_forward));

                vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(ovrl_handle, device_index, &matrix);
            }
            else //No controller connected, uh put it to 0?
            {
                vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, universe_origin, &matrix);
            }
            break;
        }
        case ovrl_origin_aux:
        {
            vr::TrackedDeviceIndex_t index_tracker = vr::IVRSystemEx::GetFirstVRTracker();

            if (index_tracker != vr::k_unTrackedDeviceIndexInvalid)
            {
                matrix = ConfigManager::Get().GetOverlayDetachedTransform().toOpenVR34();

                //Offset transform by additional offset values
                vr::IVRSystemEx::TransformOpenVR34TranslateRelative(matrix, ConfigManager::GetValue(configid_float_overlay_offset_right),
                                                                            ConfigManager::GetValue(configid_float_overlay_offset_up),
                                                                            ConfigManager::GetValue(configid_float_overlay_offset_forward));

                vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(ovrl_handle, index_tracker, &matrix);
            }
            else //Not connected, uh put it to 0?
            {
                vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, universe_origin, &matrix);
            }

            break;
        }
    }

    //Update Width
    vr::VROverlay()->SetOverlayWidthInMeters(ovrl_handle, width);

    //Update Curvature
    vr::VROverlay()->SetOverlayCurvature(ovrl_handle, ConfigManager::GetValue(configid_float_overlay_curvature));

    //Update Brightness
    //We use the logarithmic counterpart since the changes in higher steps are barely visible while the lower range can really use those additional steps
    float brightness = lin2log(ConfigManager::GetValue(configid_float_overlay_brightness)) * ConfigManager::GetValue(configid_float_overlay_state_brightness_extra_multiplier);
    vr::VROverlay()->SetOverlayColor(ovrl_handle, brightness, brightness, brightness);

    //Set last tick for dashboard dummy delayed update
    m_LastApplyTransformTick = ::GetTickCount64();
}

void OutputManager::ApplySettingCrop()
{
    Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
    OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
    vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

    //UI overlays don't do any cropping and handle the texture bounds themselves
    if (overlay.GetTextureSource() == ovrl_texsource_ui)
        return;

    //Set up overlay cropping
    vr::VRTextureBounds_t tex_bounds;
    vr::VRTextureBounds_t tex_bounds_prev;

    const int content_width  = data.ConfigInt[configid_int_overlay_state_content_width];
    const int content_height = data.ConfigInt[configid_int_overlay_state_content_height];

    if ( (overlay.GetTextureSource() == ovrl_texsource_none) || ((content_width == -1) && (content_height == -1)) )
    {
        tex_bounds.uMin = 0.0f;
        tex_bounds.vMin = 0.0f;
        tex_bounds.uMax = 1.0f;
        tex_bounds.vMax = 1.0f;

        vr::VROverlay()->SetOverlayTextureBounds(ovrl_handle, &tex_bounds);
        return;
    }

    overlay.UpdateValidatedCropRect();
    const DPRect& crop_rect = overlay.GetValidatedCropRect();

    const bool is_3d_enabled = ConfigManager::GetValue(configid_bool_overlay_3D_enabled);
    const int mode_3d = ConfigManager::GetValue(configid_int_overlay_3D_mode);
    const bool is_ou3d = ( (is_3d_enabled) && ((mode_3d == ovrl_3Dmode_ou) || (mode_3d == ovrl_3Dmode_hou)) );

    //Use full texture if everything checks out or 3D mode is Over-Under (converted to a 1:1 fitting texture)
    if ( (is_ou3d) || ( (crop_rect.GetTL().x == 0) && (crop_rect.GetTL().y == 0) && (crop_rect.GetWidth() == content_width) && (crop_rect.GetHeight() == content_height) ) )
    {
        tex_bounds.uMin = 0.0f;
        tex_bounds.vMin = 0.0f;
        tex_bounds.uMax = 1.0f;
        tex_bounds.vMax = 1.0f;
    }
    else
    {
        //Otherwise offset the calculated texel coordinates a bit. This is to reduce having colors from outside the cropping area bleeding in from the texture filtering
        //This means the border pixels of the overlay are smaller, but that's something we need to accept it seems
        //This doesn't 100% solve texel bleed, especially not on high overlay rendering quality where it can require pretty big offsets depending on overlay size/distance
        float offset_x = (crop_rect.GetWidth() <= 2) ? 0.0f : 1.5f, offset_y = (crop_rect.GetHeight() <= 2) ? 0.0f : 1.5f; //Yes, we do handle the case of <3 pixel crops

        tex_bounds.uMin = (crop_rect.GetTL().x + offset_x) / content_width;
        tex_bounds.vMin = (crop_rect.GetTL().y + offset_y) / content_height;
        tex_bounds.uMax = (crop_rect.GetBR().x - offset_x) / content_width;
        tex_bounds.vMax = (crop_rect.GetBR().y - offset_y) / content_height;
    }

    //If capture source is WinRT, set 3D mode with cropping values
    if (ConfigManager::GetValue(configid_int_overlay_capture_source) == ovrl_capsource_winrt_capture)
    {
        DPWinRT_SetOverlayOverUnder3D(ovrl_handle, is_ou3d, crop_rect.GetTL().x, crop_rect.GetTL().y, crop_rect.GetWidth(), crop_rect.GetHeight());
    }
    else if (ConfigManager::GetValue(configid_int_overlay_capture_source) == ovrl_capsource_browser) //Same with browser
    {
        DPBrowserAPIClient::Get().DPBrowser_SetOverUnder3D(ovrl_handle, is_ou3d, crop_rect.GetTL().x, crop_rect.GetTL().y, crop_rect.GetWidth(), crop_rect.GetHeight());
    }
    else //For Desktop Duplication, compare old to new bounds to see if a full refresh is required
    {
        vr::VROverlay()->GetOverlayTextureBounds(ovrl_handle, &tex_bounds_prev);

        if ((tex_bounds.uMin < tex_bounds_prev.uMin) || (tex_bounds.vMin < tex_bounds_prev.vMin) || (tex_bounds.uMax > tex_bounds_prev.uMax) || (tex_bounds.vMax > tex_bounds_prev.vMax))
        {
            RefreshOpenVROverlayTexture(DPRect(-1, -1, -1, -1), true);
        }
    }

    vr::VROverlay()->SetOverlayTextureBounds(ovrl_handle, &tex_bounds);
}

void OutputManager::ApplySettingInputMode()
{
    //Apply/Restore mouse settings first
    ApplySettingMouseInput();

    const bool drag_or_select_mode_enabled = ( (ConfigManager::GetValue(configid_bool_state_overlay_dragmode)) || (ConfigManager::GetValue(configid_bool_state_overlay_selectmode)) );
    //Always applies to all overlays
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        const Overlay& overlay_current = OverlayManager::Get().GetCurrentOverlay();
        vr::VROverlayHandle_t ovrl_handle = overlay_current.GetHandle();

        if ((ConfigManager::GetValue(configid_bool_overlay_input_enabled)) || (drag_or_select_mode_enabled) )
        {
            //Don't activate drag mode for HMD origin when the pointer is also the HMD (or it's the dashboard overlay)
            if ( ((ConfigManager::Get().GetPrimaryLaserPointerDevice() == vr::k_unTrackedDeviceIndex_Hmd) && (ConfigManager::GetValue(configid_int_overlay_origin) == ovrl_origin_hmd)) )
            {
                vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_None);
            }
            else
            {
                vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_Mouse);
            }
        }
        else
        {
            vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_None);
        }

        //Sync matrix if it's been turned off
        if ( (!drag_or_select_mode_enabled) && (!ConfigManager::GetValue(configid_bool_state_overlay_dragmode)) )
        {
            DetachedTransformSync(i);
        }

        ApplySettingTransform();
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::ApplySettingMouseInput()
{
    //Set double-click assist duration from user config value
    if (ConfigManager::GetValue(configid_int_input_mouse_dbl_click_assist_duration_ms) == -1)
    {
        ConfigManager::SetValue(configid_int_state_mouse_dbl_click_assist_duration_ms, ::GetDoubleClickTime());
    }
    else
    {
        ConfigManager::SetValue(configid_int_state_mouse_dbl_click_assist_duration_ms, ConfigManager::GetValue(configid_int_input_mouse_dbl_click_assist_duration_ms));
    }

    const bool drag_mode_enabled = ((ConfigManager::GetValue(configid_bool_state_overlay_dragmode)) || (m_OvrlDirectDragActive));
    const bool select_mode_enabled = ConfigManager::GetValue(configid_bool_state_overlay_selectmode);
    const bool drag_or_select_mode_enabled = ((drag_mode_enabled) || (select_mode_enabled));
    //Always applies to all overlays
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
        vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

        //Set input method (possibly overridden by ApplyInputMethod() right afterwards)
        if ((ConfigManager::GetValue(configid_bool_overlay_input_enabled) || (drag_or_select_mode_enabled)))
        {
            //Temp drag needs every input-enabled overlay to have smooth scroll
            if ( (ConfigManager::GetValue(configid_bool_input_mouse_scroll_smooth)) || (ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp)) || (drag_mode_enabled) )
            {
                vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, false);
                vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SendVRSmoothScrollEvents,   true);
            }
            else
            {
                vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);
                vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SendVRSmoothScrollEvents,   false);
            }

            vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_Mouse);
        }
        else
        {
            vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_None);
        }

        //Set intersection blob state
        bool hide_intersection = false;

        if (OverlayManager::Get().GetTheaterOverlayID() == i) //Always hide if theater overlay
        {
            hide_intersection = true;
        }
        else if ( (overlay.GetTextureSource() != ovrl_texsource_none) && (overlay.GetTextureSource() != ovrl_texsource_ui) && (overlay.GetTextureSource() != ovrl_texsource_browser) && 
                  (!drag_mode_enabled) && (!select_mode_enabled) )
        {
            hide_intersection = !ConfigManager::GetValue(configid_bool_input_mouse_render_intersection_blob);
        }

        vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_HideLaserIntersection, hide_intersection);

        ApplySettingMouseScale();

        //Set intersection mask for desktop duplication overlays
        if (overlay.GetTextureSource() == ovrl_texsource_desktop_duplication)
        {
            std::vector<vr::VROverlayIntersectionMaskPrimitive_t> primitives;
            primitives.reserve(m_DesktopRects.size());

            for (const DPRect& rect : m_DesktopRects)
            {
                vr::VROverlayIntersectionMaskPrimitive_t primitive;
                primitive.m_nPrimitiveType = vr::OverlayIntersectionPrimitiveType_Rectangle;
                primitive.m_Primitive.m_Rectangle.m_flTopLeftX = rect.GetTL().x - m_DesktopX;
                primitive.m_Primitive.m_Rectangle.m_flTopLeftY = rect.GetTL().y - m_DesktopY;
                primitive.m_Primitive.m_Rectangle.m_flWidth    = rect.GetWidth();
                primitive.m_Primitive.m_Rectangle.m_flHeight   = rect.GetHeight();

                primitives.push_back(primitive);
            }

            vr::EVROverlayError err = vr::VROverlay()->SetOverlayIntersectionMask(ovrl_handle, primitives.data(), (uint32_t)primitives.size());
        }
        else if (overlay.GetTextureSource() != ovrl_texsource_ui) //Or reset intersection mask if not UI overlay
        {
            vr::VROverlay()->SetOverlayIntersectionMask(ovrl_handle, nullptr, 0);
        }
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::ApplySettingMouseScale()
{
    Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
    OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
    vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

    //UI overlays handle the mouse scale themselves
    if (overlay.GetTextureSource() == ovrl_texsource_ui)
        return;

    //Set mouse scale based on capture source
    vr::HmdVector2_t mouse_scale = {0};

    if (overlay.GetTextureSource() == ovrl_texsource_none)
    {
        //The mouse scale defines the surface aspect ratio for the intersection test... yeah. If it's off there will be hits over empty space, so try to match it even here
        //uint32_t ovrl_tex_width = 1, ovrl_tex_height = 1;

        //Content size might not be what the current texture size is in case of ovrl_texsource_none
        /*if (vr::VROverlay()->GetOverlayTextureSize(ovrl_handle, &ovrl_tex_width, &ovrl_tex_height) == vr::VROverlayError_None) //GetOverlayTextureSize() currently leaks, so don't use it
        {
        mouse_scale.v[0] = ovrl_tex_width;
        mouse_scale.v[1] = ovrl_tex_height;
        }
        else*/ //ovrl_texsource_none pretty much means overlay output error texture, so fall back to that if we can't get the real size
        {
            mouse_scale.v[0] = k_lOverlayOutputErrorTextureWidth;
            mouse_scale.v[1] = k_lOverlayOutputErrorTextureHeight;
        }
    }
    else
    {
        switch (data.ConfigInt[configid_int_overlay_capture_source])
        {
            case ovrl_capsource_desktop_duplication:
            {
                mouse_scale.v[0] = m_DesktopWidth;
                mouse_scale.v[1] = m_DesktopHeight;
                break;
            }
            case ovrl_capsource_winrt_capture:
            case ovrl_capsource_browser:
            {
                //Use duplication IDs' data if any is set
                int duplication_id = ConfigManager::GetValue(configid_int_overlay_duplication_id);
                const OverlayConfigData& overlay_data = (duplication_id != -1) ? OverlayManager::Get().GetConfigData((unsigned int)duplication_id) : data;

                mouse_scale.v[0] = overlay_data.ConfigInt[configid_int_overlay_state_content_width];
                mouse_scale.v[1] = overlay_data.ConfigInt[configid_int_overlay_state_content_height];
                break;
            }
        }

        //Adjust for 3D so surface aspect ratio for laser pointing matches what is seen.
        //This blocks half or more pixels, but it's not clear what the real target coordinates are with content being different in each eye
        if (data.ConfigBool[configid_bool_overlay_3D_enabled])
        {
            switch (data.ConfigInt[configid_int_overlay_3D_mode])
            {
                case ovrl_3Dmode_hsbs:
                case ovrl_3Dmode_sbs:
                {
                    mouse_scale.v[0] /= 2.0f; 
                    break;
                }
                case ovrl_3Dmode_hou:
                case ovrl_3Dmode_ou:
                {
                    //OU converted to SBS will have texture size based on crop rect
                    const DPRect& crop_rect = overlay.GetValidatedCropRect();

                    mouse_scale.v[0] = crop_rect.GetWidth();
                    mouse_scale.v[1] = crop_rect.GetHeight() / 2.0f;
                }
            }
        }
    }

    vr::VROverlay()->SetOverlayMouseScale(ovrl_handle, &mouse_scale);
}

void OutputManager::ApplySettingUpdateLimiter()
{
    //Here's the deal with the fps-based limiter: It just barely works
    //A simple fps cut-off doesn't work since mouse updates add up to them
    //Using the right frame time value seems to work in most cases
    //A full-range, user-chosen fps value doesn't really work though, as the frame time values required don't seem to predictably change ("1000/fps" is close, but the needed adjustment varies)
    //The frame time method also doesn't work reliably above 50 fps. It limits, but the resulting fps isn't constant.
    //This is why the fps limiter is somewhat restricted in what settings it offers. It does cover the most common cases, however.
    //The frame time limiter is still there to offer more fine-tuning after all

    //Map tested frame time values to the fps enum IDs
    //FPS:                                 1       2       5     10      15      20      25      30      40      50
    const float fps_enum_values_ms[] = { 985.0f, 485.0f, 195.0f, 96.50f, 63.77f, 47.76f, 33.77f, 31.73f, 23.72f, 15.81f };

    float limit_ms = 0.0f;

    //Set limiter value from global setting
    if (ConfigManager::GetValue(configid_int_performance_update_limit_mode) == update_limit_mode_ms)
    {
        limit_ms = ConfigManager::GetValue(configid_float_performance_update_limit_ms);
    }
    else if (ConfigManager::GetValue(configid_int_performance_update_limit_mode) == update_limit_mode_fps)
    {
        int enum_id = ConfigManager::GetValue(configid_int_performance_update_limit_fps);

        if (enum_id <= update_limit_fps_50)
        {
            limit_ms = fps_enum_values_ms[enum_id];
        }
    }

    LARGE_INTEGER limit_delay_global = {0};
    limit_delay_global.QuadPart = 1000.0f * limit_ms;

    //See if there are any overrides from visible overlays
    //This is the straight forward and least error-prone way, not quite the most efficient one
    //Calls to this are minimized and there typically aren't many overlays so it's not really that bad (and we do iterate over all of them in many other places too)
    bool is_first_override = true;
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        const Overlay& overlay        = OverlayManager::Get().GetOverlay(i);
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

        if ( (overlay.IsVisible()) && (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_desktop_duplication) && 
             (data.ConfigInt[configid_int_overlay_update_limit_override_mode] != update_limit_mode_off) )
        {
            float override_ms = 0.0f;

            if (data.ConfigInt[configid_int_overlay_update_limit_override_mode] == update_limit_mode_ms)
            {
                override_ms = data.ConfigFloat[configid_float_overlay_update_limit_override_ms];
            }
            else
            {
                int enum_id = data.ConfigInt[configid_int_overlay_update_limit_override_fps];

                if (enum_id <= update_limit_fps_50)
                {
                    override_ms = fps_enum_values_ms[enum_id];
                }
            }

            //Use override if it results in more updates (except first override, which always has priority over global setting)
            if ( (is_first_override) || (override_ms < limit_ms) )
            {
                limit_ms = override_ms;
                is_first_override = false;
            }
        }
        else if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_winrt_capture) //Set limit values for WinRT overlays as well
        {
            LARGE_INTEGER limit_delay = limit_delay_global;

            if (data.ConfigInt[configid_int_overlay_update_limit_override_mode] == update_limit_mode_ms)
            {
                limit_delay.QuadPart = 1000.0f * data.ConfigFloat[configid_float_overlay_update_limit_override_ms];
            }
            else if (data.ConfigInt[configid_int_overlay_update_limit_override_mode] == update_limit_mode_fps)
            {
                int enum_id = data.ConfigInt[configid_int_overlay_update_limit_override_fps];

                if (enum_id <= update_limit_fps_50)
                {
                    limit_delay.QuadPart = 1000.0f * fps_enum_values_ms[enum_id];
                }
            }

            //Calling this regardless of change might be overkill, but doesn't seem too bad for now
            DPWinRT_SetOverlayUpdateLimitDelay(overlay.GetHandle(), limit_delay.QuadPart);
        }
    }

    m_PerformanceUpdateLimiterDelay.QuadPart = 1000.0f * limit_ms;
}

void OutputManager::ApplySettingExtraBrightness()
{
    Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
    OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

    float extra_brightness_mulitplier = 1.0f;

    if (ConfigManager::GetValue(configid_bool_performance_hdr_mirroring))
    {
        const bool ignore_wmr_vscreens = (ConfigManager::GetValue(configid_int_interface_wmr_ignore_vscreens) == 1);

        switch (overlay.GetTextureSource())
        {
            case ovrl_texsource_desktop_duplication:
            case ovrl_texsource_desktop_duplication_3dou_converted:
            {
                extra_brightness_mulitplier = GetDesktopHDRWhiteLevelAdjustment(data.ConfigInt[configid_int_overlay_desktop_id], false, ignore_wmr_vscreens);
                break;
            }
            case ovrl_texsource_winrt_capture:
            {
                int desktop_id = data.ConfigInt[configid_int_overlay_winrt_desktop_id];

                //Window capture, need to find the desktop ID it's on
                if (desktop_id == -2)
                {
                    HMONITOR monitor_handle = ::MonitorFromWindow((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd], MONITOR_DEFAULTTONEAREST);
                    desktop_id = GetDisplayIDFromHMonitor(monitor_handle, ignore_wmr_vscreens);
                }

                extra_brightness_mulitplier = GetDesktopHDRWhiteLevelAdjustment(desktop_id, true, ignore_wmr_vscreens);
            }
            default: break;
        }
    }

    if (data.ConfigFloat[configid_float_overlay_state_brightness_extra_multiplier] != extra_brightness_mulitplier)
    {
        data.ConfigFloat[configid_float_overlay_state_brightness_extra_multiplier] = extra_brightness_mulitplier;

        //Send change over to UI
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)overlay.GetID());
        IPCManager::Get().PostConfigMessageToUIApp(configid_float_overlay_state_brightness_extra_multiplier, extra_brightness_mulitplier);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
    }
}

void OutputManager::DetachedTransformSync(unsigned int overlay_id)
{
    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_transform_sync_target_id, (int)overlay_id);

    const float* values = OverlayManager::Get().GetConfigData(overlay_id).ConfigTransform.get();
    for (size_t i = 0; i < 16; ++i)
    {
        IPCManager::Get().PostConfigMessageToUIApp(configid_float_state_overlay_transform_sync_value, values[i]);
    }

    IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_transform_sync_target_id, -1);
}

void OutputManager::DetachedTransformSyncAll()
{
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        DetachedTransformSync(i);
    }
}

void OutputManager::DetachedTransformReset(unsigned int overlay_id_ref)
{
    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;
    Matrix4& transform = ConfigManager::Get().GetOverlayDetachedTransform();
    transform.identity(); //Reset to identity

    OverlayOrigin overlay_origin = (OverlayOrigin)ConfigManager::GetValue(configid_int_overlay_origin);
    const Overlay& overlay_ref = OverlayManager::Get().GetOverlay(overlay_id_ref);
    vr::VROverlayHandle_t ovrl_handle_ref = overlay_ref.GetHandle(); //Results in vr::k_ulOverlayHandleInvalid for k_ulOverlayID_None

    const bool dont_use_reference = (overlay_origin >= ovrl_origin_hmd) || ( (overlay_origin == ovrl_origin_hmd_floor) && (ConfigManager::GetValue(configid_bool_overlay_origin_hmd_floor_use_turning)) );

    if ( (ovrl_handle_ref == vr::k_ulOverlayHandleInvalid) && (!dont_use_reference) )
    {
        const Overlay& overlay_dashboard = OverlayManager::Get().GetPrimaryDashboardOverlay();

        if (overlay_dashboard.GetID() != OverlayManager::Get().GetCurrentOverlayID())
        {
            overlay_id_ref  = overlay_dashboard.GetID();
            ovrl_handle_ref = overlay_dashboard.GetHandle();
        }
    }

    //Position next to reference if available
    if (ovrl_handle_ref != vr::k_ulOverlayHandleInvalid)
    {
        OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
        const OverlayConfigData& data_ref = OverlayManager::Get().GetConfigData(overlay_id_ref);
        vr::HmdMatrix34_t overlay_transform;
        vr::HmdVector2_t mouse_scale;

        bool ref_overlay_changed = false;
        float ref_overlay_alpha_orig = 0.0f;

        //GetTransformForOverlayCoordinates() won't work if the reference overlay is not visible, so make it "visible" by showing it with 0% alpha
        if (!vr::VROverlay()->IsOverlayVisible(ovrl_handle_ref))
        {
            vr::VROverlay()->GetOverlayAlpha(ovrl_handle_ref, &ref_overlay_alpha_orig);
            vr::VROverlay()->SetOverlayAlpha(ovrl_handle_ref, 0.0f);
            vr::VROverlay()->ShowOverlay(ovrl_handle_ref);

            //Showing overlays and getting coordinates from them has a race condition if it's the first time the overlay is shown
            //Doesn't seem like it can be truly detected when it's ready, so as cheap as it is, this Sleep() seems to get around the issue
            ::Sleep(50);

            ref_overlay_changed = true;
        }

        //Get mouse scale for overlay coordinate offset
        vr::VROverlay()->GetOverlayMouseScale(ovrl_handle_ref, &mouse_scale);

        //Get x-offset multiplier, taking width differences into account
        float ref_overlay_width;
        vr::VROverlay()->GetOverlayWidthInMeters(ovrl_handle_ref, &ref_overlay_width);
        float dashboard_scale = GetDashboardScale();
        float x_offset_mul = ( (data.ConfigFloat[configid_float_overlay_width] / ref_overlay_width) / 2.0f) + 1.0f;

        //Put it next to the reference overlay so it can actually be seen
        vr::HmdVector2_t coordinate_offset = {mouse_scale.v[0] * x_offset_mul, mouse_scale.v[1] / 2.0f};
        vr::VROverlay()->GetTransformForOverlayCoordinates(ovrl_handle_ref, universe_origin, coordinate_offset, &overlay_transform);
        transform = overlay_transform;

        //Counteract additonal offset that might've been present on the transform
        transform.translate_relative(-data_ref.ConfigFloat[configid_float_overlay_offset_right],
                                     -data_ref.ConfigFloat[configid_float_overlay_offset_up],
                                     -data_ref.ConfigFloat[configid_float_overlay_offset_forward]);

        //Counteract origin offset for dashboard origin overlays
        if (overlay_origin == ovrl_origin_dashboard)
        {
            float height = GetOverlayHeight(overlay_id_ref);
            transform.translate_relative(0.0f, height / -2.0f, 0.0f);
        }

        //Restore reference overlay state if it was changed
        if (ref_overlay_changed)
        {
            vr::VROverlay()->HideOverlay(ovrl_handle_ref);
            vr::VROverlay()->SetOverlayAlpha(ovrl_handle_ref, ref_overlay_alpha_orig);
        }

        //If the reference overlay appears to be below ground we assume it has an invalid origin (i.e. dashboard tab never opened for dashboard overlay) and use the fallback transform
        if (transform.getTranslation().y < 0.0f)
        {
            transform = GetFallbackOverlayTransform();
        }

        //Adapt to base offset for non-room origins
        if (overlay_origin != ovrl_origin_room)
        {
            Matrix4 transform_base = m_OverlayDragger.GetBaseOffsetMatrix();
            transform_base.invert();
            transform = transform_base * transform;
        }
    }
    else //Otherwise add some default offset to the previously reset to identity value
    {
        switch (overlay_origin)
        {
            case ovrl_origin_room:
            case ovrl_origin_hmd_floor:
            {
                //Put it in front of the user's face instead when turning is enabled
                if ( (overlay_origin == ovrl_origin_hmd_floor) && (ConfigManager::GetValue(configid_bool_overlay_origin_hmd_floor_use_turning)) )
                {
                    transform.translate_relative(0.0f, 1.0f, -2.5f);
                    break;
                }

                vr::HmdMatrix34_t overlay_transform = {0};
                vr::VROverlay()->GetTransformForOverlayCoordinates(m_OvrlHandleDashboardDummy, vr::TrackingUniverseStanding, {0.5f, 0.5f}, &overlay_transform);

                transform = overlay_transform;

                //Use fallback transform if dashboard dummy is still set to identity (never opened Desktop+ dashboard tab)
                if (transform == Matrix4())
                {
                    transform = GetFallbackOverlayTransform();
                }

                //Adapt to base offset for non-room origin
                if (overlay_origin != ovrl_origin_room)
                {
                    Matrix4 transform_base = m_OverlayDragger.GetBaseOffsetMatrix();
                    transform_base.invert();
                    transform = transform_base * transform;
                }

                //Remove dashboard scale from transform
                Vector3 translation = transform.getTranslation();
                transform.setTranslation({0.0f, 0.0f, 0.0f});
                transform.scale(1.0f / GetDashboardScale());
                transform.setTranslation(translation);

                break;
            }
            case ovrl_origin_seated_universe:
            {
                transform.translate_relative(0.0f, 0.0f, -1.0f);
                break;
            }
            case ovrl_origin_dashboard:
            {
                //Counteract dashboard scale applied by origin
                transform.scale(1.0f / GetDashboardScale());
                break;
            }
            case ovrl_origin_hmd:
            {
                transform.translate_relative(0.0f, 0.0f, -2.5f);
                break;
            }
            case ovrl_origin_right_hand:
            {
                //Set it to a controller component so it doesn't appear right where the laser pointer comes out
                //There's some doubt about this code, but it seems to work in the end (is there really no better way?)
                char buffer[vr::k_unMaxPropertyStringSize];
                vr::VRSystem()->GetStringTrackedDeviceProperty(vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand), 
                                                               vr::Prop_RenderModelName_String, buffer, vr::k_unMaxPropertyStringSize);

                vr::VRInputValueHandle_t input_value = vr::k_ulInvalidInputValueHandle;
                vr::VRInput()->GetInputSourceHandle("/user/hand/right", &input_value);
                vr::RenderModel_ControllerMode_State_t controller_state = {0};
                vr::RenderModel_ComponentState_t component_state = {0};
            
                if (vr::VRRenderModels()->GetComponentStateForDevicePath(buffer, vr::k_pch_Controller_Component_HandGrip, input_value, &controller_state, &component_state))
                {
                    transform = component_state.mTrackingToComponentLocal;
                    transform.rotateX(-90.0f);
                    transform.translate_relative(0.0f, -0.1f, 0.0f); //This seems like a good default, at least for Index controllers
                }

                break;
            }
            case ovrl_origin_left_hand:
            {
                char buffer[vr::k_unMaxPropertyStringSize];
                vr::VRSystem()->GetStringTrackedDeviceProperty(vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand), 
                                                               vr::Prop_RenderModelName_String, buffer, vr::k_unMaxPropertyStringSize);

                vr::VRInputValueHandle_t input_value = vr::k_ulInvalidInputValueHandle;
                vr::VRInput()->GetInputSourceHandle("/user/hand/left", &input_value);
                vr::RenderModel_ControllerMode_State_t controller_state = {0};
                vr::RenderModel_ComponentState_t component_state = {0};
            
                if (vr::VRRenderModels()->GetComponentStateForDevicePath(buffer, vr::k_pch_Controller_Component_HandGrip, input_value, &controller_state, &component_state))
                {
                    transform = component_state.mTrackingToComponentLocal;
                    transform.rotateX(-90.0f);
                    transform.translate_relative(0.0f, -0.1f, 0.0f);
                }

                break;
            }
            case ovrl_origin_aux:
            {
                transform.translate_relative(0.0f, 0.0f, -0.05f);
                break;
            }
            default: break;
        }
    }

    //Sync reset with UI app
    DetachedTransformSync(OverlayManager::Get().GetCurrentOverlayID());

    ApplySettingTransform();
}

void OutputManager::DetachedTransformAdjust(unsigned int packed_value)
{
    Matrix4& transform = ConfigManager::Get().GetOverlayDetachedTransform();

    //Unpack
    IPCActionOverlayPosAdjustTarget target = (IPCActionOverlayPosAdjustTarget)(packed_value & 0xF);
    bool increase = (packed_value >> 4);

    //"To HMD" / LookAt button, seperate code path entirely
    if (target == ipcactv_ovrl_pos_adjust_lookat)
    {
        //Get HMD pose
        vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
        vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, vr::IVRSystemEx::GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

        if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
        {
            Matrix4 mat_base_offset = m_OverlayDragger.GetBaseOffsetMatrix();

            //Preserve scaling from transform, which can be present in matrices originating from the dashboard
            Vector3 row_1(transform[0], transform[1], transform[2]);
            float scale_x = row_1.length(); //Scaling is always uniform so we just check the x-axis
            //Dashboard origin itself also contains scale, so take the base scale in account as well
            Vector3 row_1_base(mat_base_offset[0], mat_base_offset[1], mat_base_offset[2]);
            float scale_x_base = row_1_base.length();
            scale_x *= scale_x_base;

            //Rotate towards HMD position
            Matrix4 mat_hmd(poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
            Matrix4 mat_lookat = mat_base_offset * transform;   //Apply base offset for LookAt

            //Apply dashboard origin offset if needed
            float overlay_height = 0.0f;
            if (ConfigManager::GetValue(configid_int_overlay_origin) == ovrl_origin_dashboard)
            {
                overlay_height = GetOverlayHeight(OverlayManager::Get().GetCurrentOverlayID());
                mat_lookat.translate_relative(0.0f, overlay_height / 2.0f, 0.0f);
            }

            vr::IVRSystemEx::TransformLookAt(mat_lookat, mat_hmd.getTranslation());

            //Remove dashboard origin offset again
            if (ConfigManager::GetValue(configid_int_overlay_origin) == ovrl_origin_dashboard)
            {
                mat_lookat.translate_relative(0.0f, overlay_height / -2.0f, 0.0f);
            }

            //Remove base offset again
            mat_base_offset.invert();
            mat_lookat = mat_base_offset * mat_lookat;

            //Restore scale factor
            mat_lookat.setTranslation({0.0f, 0.0f, 0.0f});
            mat_lookat.scale(scale_x);
            mat_lookat.setTranslation(transform.getTranslation());

            transform = mat_lookat;
            ApplySettingTransform();
        }
        return;
    }

    Matrix4 mat_back;
    if (target >= ipcactv_ovrl_pos_adjust_rotx)
    {
        //Perform rotation locally
        mat_back = transform;
        transform.identity();
    }

    float distance = 0.05f;
    float angle = 1.0f;

    //Use snap size as distance if drag snapping is enabled
    if (ConfigManager::GetValue(configid_bool_input_drag_snap_position))
    {
        distance = ConfigManager::GetValue(configid_float_input_drag_snap_position_size);
    }

    if (!increase)
    {
        distance *= -1.0f;
        angle *= -1.0f;
    }

    switch (target)
    {
        case ipcactv_ovrl_pos_adjust_updown:    transform.translate_relative(0.0f,     distance, 0.0f);     break;
        case ipcactv_ovrl_pos_adjust_rightleft: transform.translate_relative(distance, 0.0f,     0.0f);     break;
        case ipcactv_ovrl_pos_adjust_forwback:  transform.translate_relative(0.0f,     0.0f,     distance); break;
        case ipcactv_ovrl_pos_adjust_rotx:      transform.rotateX(angle);                                   break;
        case ipcactv_ovrl_pos_adjust_roty:      transform.rotateY(angle);                                   break;
        case ipcactv_ovrl_pos_adjust_rotz:      transform.rotateZ(angle);                                   break;
    }

    if (target >= ipcactv_ovrl_pos_adjust_rotx)
    {
        transform = mat_back * transform;
    }

    ApplySettingTransform();
}

void OutputManager::DetachedTransformConvertOrigin(unsigned int overlay_id, OverlayOrigin origin_from, OverlayOrigin origin_to)
{
    if (origin_from == origin_to)
        return;

    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
    const OverlayOriginConfig origin_config = OverlayManager::Get().GetOriginConfigFromData(data);
    DetachedTransformConvertOrigin(overlay_id, origin_from, origin_to, origin_config, origin_config);
}

void OutputManager::DetachedTransformConvertOrigin(unsigned int overlay_id, OverlayOrigin origin_from, OverlayOrigin origin_to, 
                                                   const OverlayOriginConfig& origin_config_from, const OverlayOriginConfig& origin_config_to)
{
    OverlayConfigData& data = OverlayManager::Get().GetConfigData(overlay_id);
    Matrix4& transform = data.ConfigTransform;

    OverlayOriginConfig origin_config = OverlayManager::Get().GetOriginConfigFromData(data);
    Matrix4 mat_origin_from = m_OverlayDragger.GetBaseOffsetMatrix(origin_from, origin_config_from);

    Matrix4 mat_origin_to_inverse = m_OverlayDragger.GetBaseOffsetMatrix(origin_to, origin_config_to);
    mat_origin_to_inverse.invert();

    //Apply origin-from matrix to get absolute transform
    transform = mat_origin_from * transform;

    if ( (origin_from == ovrl_origin_dashboard) || (origin_to == ovrl_origin_dashboard) )
    {
        //Apply or counteract origin offset used in ApplySettingTransform for dashboard origin
        float height = GetOverlayHeight(overlay_id);

        transform.translate_relative(0.0f, (origin_from == ovrl_origin_dashboard) ? height / 2.0f : height / -2.0f, 0.0f);
    }

    //Apply inverse of origin-to matrix to get transform relative to new origin
    transform = mat_origin_to_inverse * transform;

    //Sync transform so the UI doesn't have the wrong idea if no drag occurs after this
    DetachedTransformSync(overlay_id);
}

void OutputManager::DetachedTransformUpdateHMDFloor()
{
    Matrix4 matrix = m_OverlayDragger.GetBaseOffsetMatrix();
    matrix *= ConfigManager::Get().GetOverlayDetachedTransform();

    //Offset transform by additional offset values
    matrix.translate_relative(ConfigManager::GetValue(configid_float_overlay_offset_right),
                              ConfigManager::GetValue(configid_float_overlay_offset_up),
                              ConfigManager::GetValue(configid_float_overlay_offset_forward));

    vr::HmdMatrix34_t matrix_ovr = matrix.toOpenVR34();
    vr::VROverlay()->SetOverlayTransformAbsolute(OverlayManager::Get().GetCurrentOverlay().GetHandle(), vr::TrackingUniverseStanding, &matrix_ovr);
}

void OutputManager::DetachedTransformUpdateSeatedPosition()
{
    Matrix4 mat_seated_zero = vr::VRSystem()->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();

    //Sounds stupid, but we can be too fast to react to position updates and get the old seated zero pose as a result... so let's wait once if that happens
    if (mat_seated_zero == m_SeatedTransformLast)
    {
        ::Sleep(100);
        mat_seated_zero = vr::VRSystem()->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
    }

    //Update transforms of relevant overlays
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        if (ConfigManager::GetValue(configid_int_overlay_origin) == ovrl_origin_seated_universe)
        {
            ApplySettingTransform();
        }
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

    m_SeatedTransformLast = mat_seated_zero;
}

void OutputManager::DetachedInteractionAutoToggleAll()
{
    //Don't change flags while any drag is currently active
    if ( (m_OverlayDragger.IsDragActive()) || (m_OverlayDragger.IsDragGestureActive()) )
        return;

    float max_distance = ConfigManager::GetValue(configid_float_input_detached_interaction_max_distance);

    if ( (max_distance != 0.0f) && (!vr::VROverlay()->IsDashboardVisible()) )
    {
        bool do_set_interactive = false;

        //Add some additional distance for disabling interaction again
        if (m_LaserPointer.IsActive())
        {
            //...but abort if pointer wasn't activated by this function (not our job to deactivate it)
            if (m_LaserPointer.GetActivationOrigin() != dplp_activation_origin_auto_toggle)
                return;

            max_distance += 0.01f;
        }

        vr::TrackedDeviceIndex_t device_index_hovering = m_LaserPointer.IsAnyOverlayHovered(max_distance);

        //Set pointer device if interaction not active yet and a device is hovering
        if ( (!m_LaserPointer.IsActive()) && (device_index_hovering != vr::k_unTrackedDeviceIndexInvalid) )
        {
            m_LaserPointer.SetActiveDevice(device_index_hovering, dplp_activation_origin_auto_toggle);
        }
        else if ( (m_LaserPointer.IsActive()) && (device_index_hovering == vr::k_unTrackedDeviceIndexInvalid) ) //Clear pointer device if interaction active and no device is hovering
        {
            m_LaserPointer.ClearActiveDevice();
        }
    }
}

void OutputManager::DetachedOverlayGazeFade()
{
    if (ConfigManager::GetValue(configid_bool_overlay_gazefade_enabled))
    {
        Overlay& current_overlay = OverlayManager::Get().GetCurrentOverlay();

        //When drag/select mode are active or HMD pose not available, default to most visible alpha setting
        const float max_alpha = ConfigManager::GetValue(configid_float_overlay_opacity);
        const float min_alpha = ConfigManager::GetValue(configid_float_overlay_gazefade_opacity);
        float alpha = std::max(min_alpha, max_alpha);

        if ((!ConfigManager::GetValue(configid_bool_state_overlay_dragmode)) && (!ConfigManager::GetValue(configid_bool_state_overlay_selectmode)))
        {
            vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;
            vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
            vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, vr::IVRSystemEx::GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

            if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
            {
                //Distance the gaze point is offset from HMD (useful range 0.25 - 1.0)
                float gaze_distance = ConfigManager::GetValue(configid_float_overlay_gazefade_distance);
                //Rate the fading gets applied when looking off the gaze point (useful range 4.0 - 30, depends on overlay size) 
                float fade_rate = ConfigManager::GetValue(configid_float_overlay_gazefade_rate) * 10.0f; 

                Matrix4 mat_pose = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;

                Matrix4 mat_overlay = m_OverlayDragger.GetBaseOffsetMatrix();
                mat_overlay *= ConfigManager::Get().GetOverlayDetachedTransform();

                //Infinite/Auto distance mode
                if (gaze_distance == 0.0f) 
                {
                    gaze_distance = mat_overlay.getTranslation().distance(mat_pose.getTranslation()); //Match gaze distance to distance between HMD and overlay
                }
                else
                {
                    gaze_distance += 0.20f; //Useful range starts at ~0.20 - 0.25 (lower is in HMD or culled away), so offset the settings value
                }

                mat_pose.translate_relative(0.0f, 0.0f, -gaze_distance);

                Vector3 pos_gaze = mat_pose.getTranslation();
                float distance = mat_overlay.getTranslation().distance(pos_gaze);

                gaze_distance = std::min(gaze_distance, 1.0f); //To get useful fading past 1m distance we'll have to limit the value to 1m here for the math below

                alpha = clamp((distance * -fade_rate) + ((gaze_distance - 0.1f) * 10.0f), 0.0f, 1.0f); //There's nothing smart behind this, just trial and error

                //Use max alpha when the overlay or the Floating UI targeting the overlay is being pointed at
                if ((ConfigManager::Get().IsLaserPointerTargetOverlay(current_overlay.GetHandle())) || 
                    ((unsigned int)ConfigManager::GetValue(configid_int_state_interface_floating_ui_hovered_id) == current_overlay.GetID()))
                {
                    alpha = std::max(min_alpha, max_alpha); //Take whatever's more visible as the user probably wants to be able to see the overlay
                }
                else //Adapt alpha result from a 0.0 - 1.0 range to gazefade_opacity - overlay_opacity and invert if necessary
                {
                    const float range_length = max_alpha - min_alpha;

                    if (range_length >= 0.0f)
                    {
                        alpha = (alpha * range_length) + min_alpha;
                    }
                    else //Gaze Fade target opacity higher than overlay opcacity, invert behavior
                    {
                        alpha = ((alpha - 1.0f) * range_length) + max_alpha;
                    }
                }
            }
        }

        //Limit alpha change per frame to smooth out things when abrupt changes happen (i.e. overlay capture took a bit to re-enable or laser pointer forces full alpha)
        const float prev_alpha = current_overlay.GetOpacity();
        const float diff = alpha - prev_alpha;

        current_overlay.SetOpacity(prev_alpha + clamp(diff, -0.1f, 0.1f));
    }
}

void OutputManager::DetachedOverlayGazeFadeAutoConfigure()
{
    vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, vr::IVRSystemEx::GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

    if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
    {
        OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

        Matrix4 mat_pose = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;

        Matrix4 mat_overlay = m_OverlayDragger.GetBaseOffsetMatrix();
        mat_overlay *= ConfigManager::Get().GetOverlayDetachedTransform();

        //Match gaze distance to distance between HMD and overlay
        float gaze_distance = mat_overlay.getTranslation().distance(mat_pose.getTranslation());
        gaze_distance -= 0.20f;

        //Set fade rate to roughly decrease when the overlay is bigger and further away
        float fade_rate = 2.5f / data.ConfigFloat[configid_float_overlay_width] * gaze_distance;

        //Don't let the math go overboard
        gaze_distance = std::max(gaze_distance, 0.01f);
        fade_rate     = clamp(fade_rate, 0.3f, 1.75f);

        data.ConfigFloat[configid_float_overlay_gazefade_distance] = gaze_distance;
        data.ConfigFloat[configid_float_overlay_gazefade_rate]     = fade_rate;

        IPCManager::Get().PostConfigMessageToUIApp(configid_float_overlay_gazefade_distance, gaze_distance);
        IPCManager::Get().PostConfigMessageToUIApp(configid_float_overlay_gazefade_rate,     fade_rate);
    }
}

void OutputManager::DetachedOverlayAutoDockingAll()
{
    if ( (!m_OverlayDragger.IsDragActive()) || (!ConfigManager::Get().GetValue(configid_bool_input_drag_auto_docking)) )
    {
        //Set config value to 0 if the drag ended while it wasn't yet
        if (ConfigManager::GetValue(configid_int_state_auto_docking_state) != 0)
        {
            ConfigManager::SetValue(configid_int_state_auto_docking_state, 0);
            IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_auto_docking_state, 0);
        }

        return;
    }

    const OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_OverlayDragger.GetDragOverlayID());
    int config_value = 0;

    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, vr::IVRSystemEx::GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

    //Check left and right hand controller
    vr::ETrackedControllerRole controller_role = vr::TrackedControllerRole_LeftHand;
    for (int controller_role = vr::TrackedControllerRole_LeftHand; controller_role <= vr::TrackedControllerRole_RightHand; ++controller_role)
    {
        vr::TrackedDeviceIndex_t device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole((vr::ETrackedControllerRole)controller_role);

        if ( ((int)device_index != m_OverlayDragger.GetDragDeviceID()) && (device_index < vr::k_unMaxTrackedDeviceCount) && (poses[device_index].bPoseIsValid) )
        {
            //Get distance between dragged overlay and controller
            Matrix4 controller_pose = poses[device_index].mDeviceToAbsoluteTracking;
            float distance = m_OverlayDragger.GetDragOverlayMatrix().getTranslation().distance( controller_pose.getTranslation() );
            bool is_docked = false;

            //Check if overlay is already docked
            if ( ((controller_role == vr::TrackedControllerRole_LeftHand)  && (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_left_hand)) ||
                 ((controller_role == vr::TrackedControllerRole_RightHand) && (data.ConfigInt[configid_int_overlay_origin] == ovrl_origin_right_hand)) )
            {
                is_docked = true;
            }

            //Set config value if distance is low enough
            if ( (!is_docked) && (distance < 0.21f) )
            {
                config_value = controller_role;
            }
            else if ( (is_docked) && (distance > 0.23f) ) //...or high enough if already docked
            {
                config_value = controller_role + 2;
            }
        }
    }

    //Adjust config value if it changed
    if (config_value != ConfigManager::GetValue(configid_int_state_auto_docking_state))
    {
        ConfigManager::SetValue(configid_int_state_auto_docking_state, config_value);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_auto_docking_state, config_value);
    }
}

void OutputManager::DetachedTempDragStart(unsigned int overlay_id, float offset_forward)
{
    m_OverlayDragger.DragStart(overlay_id);

    //Drag may fail when dashboard device goes missing
    //Should not happen as the relevant functions try their best to get an alternative before calling this, but avoid being stuck in temp drag mode
    if (m_OverlayDragger.IsDragActive())
    {
        m_OverlayDragger.AbsoluteModeSet(true, offset_forward);
        m_OvrlTempDragStartTick = ::GetTickCount64();

        if (m_LaserPointer.IsActive())
        {
            m_LaserPointer.ForceTargetOverlay(OverlayManager::Get().GetOverlay(overlay_id).GetHandle());
        }

        ConfigManager::SetValue(configid_bool_state_overlay_dragmode_temp, true);
        IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_overlay_dragmode_temp, true);
    }
}

void OutputManager::DetachedTempDragFinish()
{
    if ( (m_OverlayDragger.IsDragActive()) && (ConfigManager::GetValue(configid_bool_state_overlay_dragmode_temp)) )
    {
        OnDragFinish();

        //Adjust current overlay for ApplySettingTransform()
        unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
        OverlayManager::Get().SetCurrentOverlayID(m_OverlayDragger.GetDragOverlayID());

        m_OverlayDragger.DragFinish();

        IPCManager::Get().PostConfigMessageToUIApp(configid_bool_state_overlay_dragmode_temp, false);
        ConfigManager::SetValue(configid_bool_state_overlay_dragmode_temp, false);

        ApplySettingTransform();
        DetachedTransformSync(OverlayManager::Get().GetCurrentOverlayID());
        OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

        ApplySettingMouseInput();
    }
}

void OutputManager::OnDragFinish()
{
    if (!m_OverlayDragger.IsDragActive())
        return;

    if (m_OvrlDirectDragActive)
    {
        m_OvrlDirectDragActive = false;
        ApplySettingMouseInput();
    }

    ResetMouseLastLaserPointerPos();

    //Handle auto-docking
    int auto_dock_value = ConfigManager::GetValue(configid_int_state_auto_docking_state);

    if (auto_dock_value != 0)
    {
        //Unpack settings value
        vr::ETrackedControllerRole role = (auto_dock_value % 2 == 0) ? vr::TrackedControllerRole_RightHand : vr::TrackedControllerRole_LeftHand;
        bool is_docking = (auto_dock_value <= 2);
        OverlayConfigData& data = OverlayManager::Get().GetConfigData(m_OverlayDragger.GetDragOverlayID());

        if (is_docking)
        {
            data.ConfigInt[configid_int_overlay_origin] = (role == vr::TrackedControllerRole_RightHand) ? ovrl_origin_right_hand : ovrl_origin_left_hand;
        }
        else
        {
            data.ConfigInt[configid_int_overlay_origin] = ovrl_origin_room;
        }

        //Send change over to UI
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, (int)m_OverlayDragger.GetDragOverlayID());
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_overlay_origin, data.ConfigInt[configid_int_overlay_origin]);
        IPCManager::Get().PostConfigMessageToUIApp(configid_int_state_overlay_current_id_override, -1);
    }
}

void OutputManager::OnSetOverlayWinRTCaptureWindow(unsigned int overlay_id)
{
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    OverlayManager::Get().SetCurrentOverlayID(overlay_id);

    Overlay& overlay        = OverlayManager::Get().GetCurrentOverlay();
    OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

    overlay.SetTextureSource(ovrl_texsource_none);
    ResetCurrentOverlay();

    //Reset config_str_overlay_winrt_last_* strings when HWND was explicitly set to null
    if (data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd] == 0)
    {
        data.ConfigStr[configid_str_overlay_winrt_last_window_title]      = "";
        data.ConfigStr[configid_str_overlay_winrt_last_window_class_name] = "";
        data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name]   = "";
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::FinishQueuedOverlayRemovals()
{
    if (m_RemoveOverlayQueue.empty())
        return;

    //Sort in descending order since removals from end will end up with less re-ordering (or even none if the top removed overlay is also the last one)
    std::sort(m_RemoveOverlayQueue.rbegin(), m_RemoveOverlayQueue.rend());

    for (unsigned int overlay_id : m_RemoveOverlayQueue)
    {
        OverlayManager::Get().RemoveOverlay(overlay_id);

        IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_overlay_remove, overlay_id);
    }

    m_RemoveOverlayQueue.clear();

    //RemoveOverlay() may have changed active ID, keep in sync
    ConfigManager::SetValue(configid_int_interface_overlay_current_id, OverlayManager::Get().GetCurrentOverlayID());
}

void OutputManager::FixInvalidDashboardLaunchState()
{
    //Workaround for glitchy behavior in SteamVR
    //When launching SteamVR while wearing the HMD, the dashboard appears automatically. In this state, IsDashboardVisible() returns false and the HMD button is unable to close the dashboard.
    //There are other things that aren't quite right and Desktop+ relies on the info to be correct in many cases.
    //This somewhat works around the issue by opening the dashboard for our dashboard overlay. 
    //While a little bit intrusive and not 100% reliable when Desktop+ is auto-launched alongside, it's better than nothing.
    vr::VROverlayHandle_t system_dashboard;
    vr::VROverlay()->FindOverlay("system.systemui", &system_dashboard);

    if ( (vr::VROverlay()->IsOverlayVisible(system_dashboard)) && (!vr::VROverlay()->IsDashboardVisible()) )
    {
        vr::VROverlay()->ShowDashboard("elvissteinjr.DesktopPlusDashboard");
    }
}

void OutputManager::UpdateDashboardHMD_Y()
{
    m_OverlayDragger.UpdateDashboardHMD_Y();
}

bool OutputManager::HasDashboardMoved()
{
    //Don't trust anything if it's not visible
    if (!vr::VROverlay()->IsDashboardVisible())
        return false;

    vr::HmdMatrix34_t hmd_matrix = {0};
    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

    vr::VROverlay()->GetTransformForOverlayCoordinates(m_OvrlHandleDashboardDummy, universe_origin, {0.0f, 0.0f}, &hmd_matrix);

    Matrix4 matrix_new = hmd_matrix;

    if (m_DashboardTransformLast != matrix_new)
    {
        m_DashboardTransformLast = hmd_matrix;

        return true;
    }

    return false;
}

void OutputManager::DimDashboard(bool do_dim)
{
    if (vr::VROverlay() == nullptr)
        return;

    //This *could* terribly conflict with other apps messing with these settings, but I'm unaware of any that are right now, so let's just say we're the first
    vr::VROverlayHandle_t system_dashboard;
    vr::VROverlay()->FindOverlay("system.systemui", &system_dashboard);

    if (system_dashboard != vr::k_ulOverlayHandleInvalid)
    {
        if (do_dim)
        {
            vr::VROverlay()->SetOverlayColor(system_dashboard, 0.05f, 0.05f, 0.05f);
        }
        else
        {
            vr::VROverlay()->SetOverlayColor(system_dashboard, 1.0f, 1.0f, 1.0f);
        }
    }

    vr::VROverlayHandle_t gamepadui;
    vr::VROverlay()->FindOverlay("valve.steam.gamepadui.bar", &gamepadui);

    if (gamepadui != vr::k_ulOverlayHandleInvalid)
    {
        if (do_dim)
        {
            vr::VROverlay()->SetOverlayColor(gamepadui, 0.05f, 0.05f, 0.05f);
        }
        else
        {
            vr::VROverlay()->SetOverlayColor(gamepadui, 1.0f, 1.0f, 1.0f);
        }
    }
}

void OutputManager::UpdatePendingDashboardDummyHeight()
{
    float old_dummy_height = 0.0f;
    vr::VROverlay()->GetOverlayWidthInMeters(m_OvrlHandleDashboardDummy, &old_dummy_height);

    if (m_PendingDashboardDummyHeight != old_dummy_height)
    {
        vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleDashboardDummy, m_PendingDashboardDummyHeight);

        //Perform tactical sleep to avoid flickering caused by dummy adjustments not being done in time when dashboard overlay positioning depends on it
        if (vr::VROverlay()->IsOverlayVisible(m_OvrlHandleDashboardDummy))
        {
            ::Sleep(100);
        }

        m_PendingDashboardDummyHeight = 0.0f;
    }
}

bool OutputManager::IsAnyOverlayUsingGazeFade() const
{
    //This is the straight forward, simple version. The smart one, efficiently keeping track properly, could come some other time*
    //*we know it probably won't happen any time soon
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

        if ( (data.ConfigBool[configid_bool_overlay_enabled]) && (data.ConfigBool[configid_bool_overlay_gazefade_enabled]) )
        {
            return true;
        }
    }

    return false;
}

void OutputManager::RegisterHotkeys()
{
    //Just unregister all we have when updating any
    for (int i = 0; i < m_RegisteredHotkeyCount; ++i)
    {
        ::UnregisterHotKey(nullptr, i);
    }
    m_IsAnyHotkeyActive = false;

    //...and register them again if there's an action assigned
    int id = 0;
    for (ConfigHotkey& hotkey : ConfigManager::Get().GetHotkeys())
    {
        if (hotkey.ActionUID != k_ActionUID_Invalid)
        {
            ::RegisterHotKey(nullptr, id, hotkey.Modifiers | MOD_NOREPEAT, hotkey.KeyCode);
            m_IsAnyHotkeyActive = true;
        }
        ++id;
    }

    m_RegisteredHotkeyCount = id;

    //Laser Pointer HMD Device uses hotkeys just as means to block key input to other applications
    //Actual inputs are checked via VRInput::UpdateKeyboardDeviceState(), so they do not need any handling in HandleHotkeys() or HandleHotkeyMessage()
    const int hmd_device_hotkey_count = configid_int_input_laser_pointer_hmd_device_keycode_drag - configid_int_input_laser_pointer_hmd_device_keycode_toggle;
    for (int i = 0; i < hmd_device_hotkey_count + 1; ++i)
    {
        ConfigID_Int config_id = (ConfigID_Int)(configid_int_input_laser_pointer_hmd_device_keycode_toggle + i);
        ::UnregisterHotKey(nullptr, 0xBFFF - i);  //0xBFFF is max allowed id, we count down from that to avoid conflicts
    }

    if (ConfigManager::GetValue(configid_bool_input_laser_pointer_hmd_device))  //No need to block these keys when the feature is disabled
    {
        for (int i = 0; i < hmd_device_hotkey_count + 1; ++i)
        {
            ConfigID_Int config_id = (ConfigID_Int)(configid_int_input_laser_pointer_hmd_device_keycode_toggle + i);
            ::RegisterHotKey(nullptr, 0xBFFF - i, MOD_NOREPEAT, ConfigManager::GetValue(config_id));
        }
    }
}

void OutputManager::HandleHotkeys()
{
    //This function handles hotkeys manually via GetAsyncKeyState() for some very special games that think consuming all keyboard input is a nice thing to do
    //Win32 hotkeys are still used simultaneously. Their input blocking might be considered an advantage and the hotkey configurability is designed around them already
    //Win32 hotkeys also still work while an elevated application has focus, GetAsyncKeyState() doesn't

    if (!m_IsAnyHotkeyActive)
        return;

    for (ConfigHotkey& hotkey : ConfigManager::Get().GetHotkeys())
    {
        if (hotkey.ActionUID != k_ActionUID_Invalid)
        {
            if ( (::GetAsyncKeyState(hotkey.KeyCode) < 0) && 
                 ( ((hotkey.Modifiers & MOD_SHIFT)   == 0) ||  (::GetAsyncKeyState(VK_SHIFT)   < 0) ) &&
                 ( ((hotkey.Modifiers & MOD_CONTROL) == 0) ||  (::GetAsyncKeyState(VK_CONTROL) < 0) ) &&
                 ( ((hotkey.Modifiers & MOD_ALT)     == 0) ||  (::GetAsyncKeyState(VK_MENU)    < 0) ) &&
                 ( ((hotkey.Modifiers & MOD_WIN)     == 0) || ((::GetAsyncKeyState(VK_LWIN)    < 0) || (::GetAsyncKeyState(VK_RWIN) < 0)) ) )
            {
                if (!hotkey.StateIsDown)
                {
                    hotkey.StateIsDown = true;

                    ConfigManager::Get().GetActionManager().DoAction(hotkey.ActionUID);
                }
            }
            else if (hotkey.StateIsDown)
            {
                hotkey.StateIsDown = false;
            }
        }
    }
}

void OutputManager::HandleKeyboardAutoVisibility()
{
    //This only handles auto-visibility for desktop/window overlays

    //Check if state changed
    static bool is_text_input_focused_prev = false;
    bool is_text_input_focused = ( (!ConfigManager::GetValue(configid_bool_input_keyboard_auto_show_desktop)) || (m_MouseIgnoreMoveEvent) ) ? false : WindowManager::Get().IsTextInputFocused();

    //Overlay input has to be active during a state change for it to count as focused
    if ((is_text_input_focused != is_text_input_focused_prev) && (!m_OvrlInputActive))
    {
        is_text_input_focused = false;
    }

    if (is_text_input_focused != is_text_input_focused_prev)
    {
        //Send update to UI process
        IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_keyboard_show_auto, (is_text_input_focused) ? ConfigManager::GetValue(configid_int_state_overlay_focused_id) : -1);

        is_text_input_focused_prev = is_text_input_focused;
    }
}
