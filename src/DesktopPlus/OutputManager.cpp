#include "OutputManager.h"

#include <dwmapi.h>
using namespace DirectX;
#include <sstream>

#include <limits.h>
#include <time.h>

#include "OverlayManager.h"
#include "Util.h"

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
    m_Factory(nullptr),
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
    m_OutputInvalid(false),
    m_OutputPendingDirtyRect{-1, -1, -1, -1},
    m_OvrlHandleMain(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleIcon(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleDashboardDummy(vr::k_ulOverlayHandleInvalid),
    m_OvrlHandleDesktopTexture(vr::k_ulOverlayHandleInvalid),
    m_OvrlTex(nullptr),
    m_OvrlRTV(nullptr),
    m_OvrlShaderResView(nullptr),
    m_OvrlActiveCount(0),
    m_OvrlDashboardActive(false),
    m_OvrlInputActive(false),
    m_OvrlDetachedInteractiveAll(false),
    m_MouseTex(nullptr),
    m_MouseShaderRes(nullptr),
    m_MouseLastClickTick(0),
    m_MouseIgnoreMoveEvent(false),
    m_MouseCursorNeedsUpdate(false),
    m_MouseLaserPointerUsedLastUpdate(false),
    m_MouseLastLaserPointerX(-1),
    m_MouseLastLaserPointerY(-1),
    m_MouseDefaultHotspotX(0),
    m_MouseDefaultHotspotY(0),
    m_MouseIgnoreMoveEventMissCount(0),
    m_ComInitDone(false),
    m_DragModeDeviceID(-1),
    m_DragModeOverlayID(0),
    m_DragGestureActive(false),
    m_DragGestureScaleDistanceStart(0.0f),
    m_DragGestureScaleWidthStart(0.0f),
    m_DragGestureScaleDistanceLast(0.0f),
    m_DashboardActivatedOnce(false),
    m_DashboardHMD_Y(-100.0f),
    m_MultiGPUTargetDevice(nullptr),
    m_MultiGPUTargetDeviceContext(nullptr),
    m_MultiGPUTexStaging(nullptr),
    m_MultiGPUTexTarget(nullptr),
    m_PerformanceFrameCount(0),
    m_PerformanceFrameCountStartTick(0),
    m_PerformanceUpdateLimiterDelay{0}
{
    m_MouseLastInfo = {0};
    m_MouseLastInfo.ShapeInfo.Type = DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR;

    //Initialize ConfigManager
    ConfigManager::Get().LoadConfigFromFile();

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
    if ( (m_OvrlActiveCount != 0) || (m_OvrlDashboardActive) )
    {
        //Actually causes extreme load while not really being necessary (looks nice tho)
        if ( (m_OvrlInputActive) && (ConfigManager::Get().GetConfigBool(configid_bool_performance_rapid_laser_pointer_updates)) )
        {
            return 0;
        }
        else
        {
            return m_MaxActiveRefreshDelay;
        }
    }
    else if ( (m_VRInput.IsAnyActionBound()) || IsAnyOverlayUsingGazeFade() )
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

float OutputManager::GetTimeNowToPhotons() const
{
    float seconds_since_last_vsync;
    vr::VRSystem()->GetTimeSinceLastVsync(&seconds_since_last_vsync, nullptr);

    float vsync_to_photons = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);

    return (1.0f / GetHMDFrameRate()) - seconds_since_last_vsync + vsync_to_photons;
}

int OutputManager::GetDesktopWidth() const
{
    return m_DesktopWidth;
}

int OutputManager::GetDesktopHeight() const
{
    return m_DesktopHeight;
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

        //Signal duplication threads to resume in case they're paused
        ::ResetEvent(m_PauseDuplicationEvent);
        ::SetEvent(m_ResumeDuplicationEvent);

        //Set last pointer values to current to not trip the movement detection up
        ResetMouseLastLaserPointerPos();

        m_MouseIgnoreMoveEvent = false;

        ForceScreenRefresh();
    }

    if ( (ConfigManager::Get().GetConfigBool(configid_bool_overlay_input_enabled)) && (ConfigManager::Get().GetConfigBool(configid_bool_input_mouse_hmd_pointer_override)) &&
         (!ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)) && (!ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_selectmode)) )
    {
        vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_Mouse);
    }

    m_OvrlActiveCount++;

    overlay.SetVisible(true);

    ApplySettingTransform();

    //Overlay could affect update limiter, so apply setting
    if (data.ConfigInt[configid_int_overlay_update_limit_override_mode] != update_limit_mode_off)
    {
        ApplySettingUpdateLimiter();
    }

    //If the last clipping rect doesn't fully contain the overlay's crop rect, the desktop texture overlay is probably outdated there, so force a full refresh
    if (!m_OutputLastClippingRect.Contains(overlay.GetValidatedCropRect()))
    {
        RefreshOpenVROverlayTexture(DPRect(-1, -1, -1, -1), true);
    }

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

    if (ConfigManager::Get().GetConfigInt(configid_int_state_keyboard_visible_for_overlay_id) == id) //Don't leave the keyboard open when hiding
    {
        vr::VROverlay()->HideKeyboard();
    }

    //Overlay could've affected update limiter, so apply setting
    if (data.ConfigInt[configid_int_overlay_update_limit_override_mode] != update_limit_mode_off)
    {
        ApplySettingUpdateLimiter();
    }

    m_OvrlActiveCount--;

    if (m_OvrlActiveCount == 0) //Last overlay to become inactive
    {
        ::timeEndPeriod(1);

        //Signal duplication threads to pause since we don't need them to do needless work
        ::ResetEvent(m_ResumeDuplicationEvent);
        ::SetEvent(m_PauseDuplicationEvent);
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

bool OutputManager::HasDashboardBeenActivatedOnce() const
{
    return m_DashboardActivatedOnce;
}

bool OutputManager::IsDashboardTabActive() const
{
    return m_OvrlDashboardActive;
}

void OutputManager::SetOutputInvalid()
{
    m_OutputInvalid = true;

    vr::VROverlay()->SetOverlayFromFile(m_OvrlHandleMain, (ConfigManager::Get().GetApplicationPath() + "images/output_error.png").c_str());

    uint32_t ovrl_w, ovrl_h;
    vr::EVROverlayError vr_error = vr::VROverlay()->GetOverlayTextureSize(m_OvrlHandleMain, &ovrl_w, &ovrl_h);
    if (vr_error == vr::VROverlayError_None)
    {
        m_DesktopWidth  = ovrl_w;
        m_DesktopHeight = ovrl_h;
    }
    else //GetOverlayTextureSize() generally seems to return VROverlayError_InvalidHandle for some reason, even though we just wanted to do the right thing for once
    {
        //Hardcode the size as a fallback then, eh
        m_DesktopWidth  = 960;
        m_DesktopHeight = 450;
    }

    vr::VROverlay()->SetOverlayFlag(m_OvrlHandleMain, vr::VROverlayFlags_SideBySide_Parallel, false);
    vr::VROverlay()->SetOverlayFlag(m_OvrlHandleMain, vr::VROverlayFlags_SideBySide_Crossed, false);
    vr::VROverlay()->SetOverlayTexelAspect(m_OvrlHandleMain, 1.0f);

    ResetOverlays();
}

bool OutputManager::IsOutputInvalid() const
{
    return m_OutputInvalid;
}

//
// Initialize all state
//
DUPL_RETURN OutputManager::InitOutput(HWND Window, _Out_ INT& SingleOutput, _Out_ UINT* OutCount, _Out_ RECT* DeskBounds)
{
    HRESULT hr;

    m_OutputInvalid = false;

    if (ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring))
    {
        SingleOutput = clamp(ConfigManager::Get().GetConfigInt(configid_int_overlay_desktop_id), -1, ::GetSystemMetrics(SM_CMONITORS) - 1);
    }
    else
    {
        SingleOutput = -1;
    }
    
    // Store window handle
    m_WindowHandle = Window;

    //Get preferred adapter if there is any, this detects which GPU the target desktop is on
    IDXGIFactory1* factory_ptr;
    IDXGIAdapter* adapter_ptr_preferred = nullptr;
    IDXGIAdapter* adapter_ptr_vr = nullptr;
    int output_id_adapter = SingleOutput;           //Output ID on the adapter actually used. Only different from initial SingleOutput if there's desktops across multiple GPUs
    std::vector<DPRect> desktop_rects_prev = m_DesktopRects;
    int desktop_x_prev = m_DesktopX;
    int desktop_y_prev = m_DesktopY;
    m_DesktopRects.clear();

    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory_ptr);
    if (!FAILED(hr))
    {
        IDXGIAdapter* adapter_ptr = nullptr;
        UINT i = 0;
        int output_count = 0;

        //Also look for the device the HMD is connected to
        int32_t vr_gpu_id;
        vr::VRSystem()->GetDXGIOutputInfo(&vr_gpu_id);  

        while (factory_ptr->EnumAdapters(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND)
        {
            int first_output_adapter = output_count;

            if (i == vr_gpu_id)
            {
                adapter_ptr_vr = adapter_ptr;
                adapter_ptr_vr->AddRef();
            }

            //Count the available outputs
            IDXGIOutput* output_ptr;
            while (adapter_ptr->EnumOutputs(output_count, &output_ptr) != DXGI_ERROR_NOT_FOUND)
            {
                //Check if this happens to be the output we're looking for (or for combined desktop, set the first adapter with available output)
                if ( (adapter_ptr_preferred == nullptr) && ( (SingleOutput == output_count) || (SingleOutput == -1) ) )
                {
                    adapter_ptr_preferred = adapter_ptr;
                    adapter_ptr_preferred->AddRef();

                    if (SingleOutput != -1)
                    {
                        output_id_adapter = output_count - first_output_adapter;
                    }
                }

                //Cache rect of the output
                DXGI_OUTPUT_DESC output_desc;
                output_ptr->GetDesc(&output_desc);
                m_DesktopRects.emplace_back(output_desc.DesktopCoordinates.left,  output_desc.DesktopCoordinates.top, 
                                            output_desc.DesktopCoordinates.right, output_desc.DesktopCoordinates.bottom);

                output_ptr->Release();
                ++output_count;
            }

            adapter_ptr->Release();
            ++i;
        }

        factory_ptr->Release();
        factory_ptr = nullptr;
    }

    SingleOutput = output_id_adapter;

    //If they're the same, we don't need to do any multi-gpu handling
    if (adapter_ptr_vr == adapter_ptr_preferred)
    {
        if (adapter_ptr_vr != nullptr)
        {
            adapter_ptr_vr->Release();
            adapter_ptr_vr = nullptr;
        }
    }
    //If there's no preferred adapter it should default to one the HMD is connected to
    if (adapter_ptr_preferred == nullptr) 
    {
        //If both are nullptr it'll still try to find a working adapter to init, though it'll probably not work at the end in that scenario
        adapter_ptr_preferred = adapter_ptr_vr; 
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
        hr = D3D11CreateDevice(adapter_ptr_preferred, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, FeatureLevels, NumFeatureLevels,
                               D3D11_SDK_VERSION, &m_Device, &FeatureLevel, &m_DeviceContext);

        if (FAILED(hr))
        {
            adapter_ptr_preferred->Release();
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
    else
    {
        adapter_ptr_preferred->Release();
        adapter_ptr_preferred = nullptr;
    }

    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Device creation failed", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    //Create multi-gpu target device if needed
    if (adapter_ptr_vr != nullptr)
    {
        hr = D3D11CreateDevice(adapter_ptr_vr, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, FeatureLevels, NumFeatureLevels,
                               D3D11_SDK_VERSION, &m_MultiGPUTargetDevice, &FeatureLevel, &m_MultiGPUTargetDeviceContext);

        if (FAILED(hr))
        {
            return ProcessFailure(m_Device, L"Secondary device creation failed", L"Error", hr, SystemTransitionsExpectedErrors);
        }

        adapter_ptr_vr->Release();
        adapter_ptr_vr = nullptr;
    }

    // Get DXGI factory
    IDXGIDevice* DxgiDevice = nullptr;
    hr = m_Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI for DXGI device", L"Error", hr, nullptr);
    }

    IDXGIAdapter* DxgiAdapter = nullptr;
    hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
    DxgiDevice->Release();
    DxgiDevice = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to get parent DXGI adapter", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    hr = DxgiAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&m_Factory));
    DxgiAdapter->Release();
    DxgiAdapter = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to get parent DXGI factory", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Create shared texture
    DUPL_RETURN Return = CreateTextures(output_id_adapter, OutCount, DeskBounds);
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
        return ProcessFailure(m_Device, L"Failed to create sampler state", L"Error", hr, SystemTransitionsExpectedErrors);
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
        return ProcessFailure(m_Device, L"Failed to create blend state", L"Error", hr, SystemTransitionsExpectedErrors);
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
        return ProcessFailure(m_Device, L"Failed to create rasterizer state", L"Error", hr, SystemTransitionsExpectedErrors);
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
        return ProcessFailure(m_Device, L"Failed to create vertex buffer", L"Error", hr, SystemTransitionsExpectedErrors);
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

    // Load default cursor
    HCURSOR Cursor = nullptr;
    Cursor = LoadCursor(nullptr, IDC_ARROW);
    //Get default cursor hotspot for laser pointing
    if (Cursor)
    {
        ICONINFO info = { 0 };
        if (::GetIconInfo(Cursor, &info) != 0)
        {
            m_MouseDefaultHotspotX = info.xHotspot;
            m_MouseDefaultHotspotY = info.yHotspot;

            ::DeleteObject(info.hbmColor);
            ::DeleteObject(info.hbmMask);
        }
    }

    //In case this was called due to a resolution change, check if the crop was just exactly the set desktop in each overlay and adapt then
    if (!ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring))
    {
        unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();

        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
        {
            OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);
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

        OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
    }

    m_InputSim.RefreshScreenOffsets();

    ResetMouseLastLaserPointerPos();
    ResetOverlays();

    return Return;
}

vr::EVRInitError OutputManager::InitOverlay()
{
    vr::EVRInitError init_error;
    vr::IVRSystem* vr_ptr = vr::VR_Init(&init_error, vr::VRApplication_Overlay);

    if (init_error != vr::VRInitError_None)
        return init_error;

    if (!vr::VROverlay())
        return vr::VRInitError_Init_InvalidInterface;

    m_OvrlHandleDashboardDummy = vr::k_ulOverlayHandleInvalid;
    m_OvrlHandleMain = vr::k_ulOverlayHandleInvalid;
    m_OvrlHandleIcon = vr::k_ulOverlayHandleInvalid;
    m_OvrlHandleDesktopTexture = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayError ovrl_error = vr::VROverlayError_None;

    //We already got rid of another instance of this app if there was any, but this loop takes care of it too if the detection failed or something uses our overlay key
    while (true)
    {
        ovrl_error = vr::VROverlay()->CreateDashboardOverlay("elvissteinjr.DesktopPlusDashboard", "Desktop+", &m_OvrlHandleDashboardDummy, &m_OvrlHandleIcon);

        if (ovrl_error == vr::VROverlayError_KeyInUse)  //If the key is already in use, kill the owning process (hopefully another instance of this app)
        {
            ovrl_error = vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlusDashboard", &m_OvrlHandleDashboardDummy);

            if ((ovrl_error == vr::VROverlayError_None) && (m_OvrlHandleDashboardDummy != vr::k_ulOverlayHandleInvalid))
            {
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
                    break;
                }
            }       
            else
            {
                ovrl_error = vr::VROverlayError_KeyInUse;
                break;
            }
        }
        else
        {
            break;
        }
    }

    
    if (m_OvrlHandleDashboardDummy != vr::k_ulOverlayHandleInvalid)
    {
        ovrl_error = vr::VROverlay()->CreateOverlay("elvissteinjr.DesktopPlusDesktopTexture", "Desktop+", &m_OvrlHandleDesktopTexture);

        //Get main overlay from OverlayManager. The Dashboard overlay is only used as a dummy to get a button, transform origin and position the top bar in the dashboard
        //k_OverlayID_Dashboard is guaranteed to always exist. The handle itself may not, but there'll be a bunch of other issues if it isn't
        Overlay& overlay_dashboard = OverlayManager::Get().GetOverlay(k_ulOverlayID_Dashboard);

        //If dashboard overlay doesn't exist yet (fresh startup), try initializing it
        if (overlay_dashboard.GetHandle() == vr::k_ulOverlayHandleInvalid)
        {
            overlay_dashboard.InitOverlay();
            ConfigManager::Get().LoadConfigFromFile(); //Also load config again to properly initialize overlays that were loaded before OpenVR was available
        }

        m_OvrlHandleMain = overlay_dashboard.GetHandle();

        if (m_OvrlHandleDashboardDummy != vr::k_ulOverlayHandleInvalid)
        {
            unsigned char bytes[2 * 2 * 4] = {0}; //2x2 transparent RGBA

            //Set dashboard dummy content instead of leaving it totally blank, which is undefined
            vr::VROverlay()->SetOverlayRaw(m_OvrlHandleDashboardDummy, bytes, 2, 2, 4);

            vr::VROverlay()->SetOverlayInputMethod(m_OvrlHandleDashboardDummy, vr::VROverlayInputMethod_None);

            //ResetOverlays() is called later

            vr::VROverlay()->SetOverlayFromFile(m_OvrlHandleIcon, (ConfigManager::Get().GetApplicationPath() + "images/icon_dashboard.png").c_str());
        }
    }

    m_MaxActiveRefreshDelay = 1000.0f / GetHMDFrameRate();

    bool input_res = m_VRInput.Init();

    //Add application manifest if needed
    if (!vr::VRApplications()->IsApplicationInstalled("elvissteinjr.DesktopPlus"))
    {
        vr::EVRApplicationError app_error;
        app_error = vr::VRApplications()->AddApplicationManifest( (ConfigManager::Get().GetApplicationPath() + "manifest.vrmanifest").c_str() );

        if (app_error == vr::VRApplicationError_None)
        {
            app_error = vr::VRApplications()->SetApplicationAutoLaunch("elvissteinjr.DesktopPlus", true);

            if (app_error == vr::VRApplicationError_None)
            {
                DisplayMsg(L"Desktop+ has been successfully added to SteamVR.\nIt will now automatically launch when SteamVR is run.", L"Desktop+ Initial Setup", S_OK);
            }
        }
    }

    //Check if it's a WMR system and set up for that if needed
    SetConfigForWMR(ConfigManager::Get().GetConfigIntRef(configid_int_interface_wmr_ignore_vscreens));

    if ((ovrl_error == vr::VROverlayError_None) && (input_res))
        return vr::VRInitError_None;
    else
        return vr::VRInitError_Compositor_OverlayInitFailed;
}

//
// Recreate textures
//
DUPL_RETURN OutputManager::CreateTextures(INT SingleOutput, _Out_ UINT* OutCount, _Out_ RECT* DeskBounds)
{
    HRESULT hr;

    // Get DXGI resources
    IDXGIDevice* DxgiDevice = nullptr;
    hr = m_Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI for DXGI device", L"Error", hr);
    }

    IDXGIAdapter* DxgiAdapter = nullptr;
    hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
    DxgiDevice->Release();
    DxgiDevice = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to get parent DXGI adapter", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Set initial values so that we always catch the right coordinates
    DeskBounds->left = INT_MAX;
    DeskBounds->right = INT_MIN;
    DeskBounds->top = INT_MAX;
    DeskBounds->bottom = INT_MIN;

    IDXGIOutput* DxgiOutput = nullptr;

    // Figure out right dimensions for full size desktop texture and # of outputs to duplicate
    UINT OutputCount;
    if (SingleOutput < 0)
    {
        int monitor_count = ::GetSystemMetrics(SM_CMONITORS);
        bool wmr_ignore_vscreens = (ConfigManager::Get().GetConfigInt(configid_int_interface_wmr_ignore_vscreens) == 1);

        if (wmr_ignore_vscreens)
        {
            monitor_count = std::max(1, monitor_count - 3); //If the 3 screen assumption doesn't hold up, at least have one button
        }

        hr = S_OK;
        for (OutputCount = 0; SUCCEEDED(hr); ++OutputCount)
        {
            //Break early if the WMR setting says to ignore extra desktops
            if ( (wmr_ignore_vscreens) && (OutputCount >= monitor_count) )
            {
                OutputCount++;
                break;
            }

            if (DxgiOutput)
            {
                DxgiOutput->Release();
                DxgiOutput = nullptr;
            }
            hr = DxgiAdapter->EnumOutputs(OutputCount, &DxgiOutput);
            if (DxgiOutput && (hr != DXGI_ERROR_NOT_FOUND))
            {
                DXGI_OUTPUT_DESC DesktopDesc;
                DxgiOutput->GetDesc(&DesktopDesc);

                DeskBounds->left = std::min(DesktopDesc.DesktopCoordinates.left, DeskBounds->left);
                DeskBounds->top = std::min(DesktopDesc.DesktopCoordinates.top, DeskBounds->top);
                DeskBounds->right = std::max(DesktopDesc.DesktopCoordinates.right, DeskBounds->right);
                DeskBounds->bottom = std::max(DesktopDesc.DesktopCoordinates.bottom, DeskBounds->bottom);
            }
        }

        --OutputCount;
    }
    else
    {
        hr = DxgiAdapter->EnumOutputs(SingleOutput, &DxgiOutput);
        if (FAILED(hr)) //Output doesn't exist. This will result in a soft-error invalid output state
        {
            DxgiAdapter->Release();
            DxgiAdapter = nullptr;

            m_DesktopX = 0;
            m_DesktopY = 0;
            m_DesktopWidth = -1;
            m_DesktopHeight = -1;

            *OutCount = 0;

            return DUPL_RETURN_ERROR_EXPECTED;
        }

        DXGI_OUTPUT_DESC DesktopDesc;
        DxgiOutput->GetDesc(&DesktopDesc);
        *DeskBounds = DesktopDesc.DesktopCoordinates;

        DxgiOutput->Release();
        DxgiOutput = nullptr;

        OutputCount = 1;
    }

    DxgiAdapter->Release();
    DxgiAdapter = nullptr;

    // Set passed in output count variable
    *OutCount = OutputCount;

    if (OutputCount == 0) //This state can only be entered on the combined desktop setting now... oops?
    {
        // We could not find any outputs, the system must be in a transition so return expected error
        // so we will attempt to recreate
        return DUPL_RETURN_ERROR_EXPECTED;
    }

    //Store size and position
    m_DesktopX = DeskBounds->left;
    m_DesktopY = DeskBounds->top;
    m_DesktopWidth = DeskBounds->right - DeskBounds->left;
    m_DesktopHeight = DeskBounds->bottom - DeskBounds->top;

    // Create shared texture for all duplication threads to draw into
    D3D11_TEXTURE2D_DESC TexD;
    RtlZeroMemory(&TexD, sizeof(D3D11_TEXTURE2D_DESC));
    TexD.Width = m_DesktopWidth;
    TexD.Height = m_DesktopHeight;
    TexD.MipLevels = 1;
    TexD.ArraySize = 1;
    TexD.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    TexD.SampleDesc.Count = 1;
    TexD.Usage = D3D11_USAGE_DEFAULT;
    TexD.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    TexD.CPUAccessFlags = 0;
    TexD.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    
    hr = m_Device->CreateTexture2D(&TexD, nullptr, &m_SharedSurf);

    if (!FAILED(hr))
    {
        TexD.MiscFlags = 0;
        hr = m_Device->CreateTexture2D(&TexD, nullptr, &m_OvrlTex);
    }

    if (FAILED(hr))
    {
        if (OutputCount != 1)
        {
            // If we are duplicating the complete desktop we try to create a single texture to hold the
            // complete desktop image and blit updates from the per output DDA interface.  The GPU can
            // always support a texture size of the maximum resolution of any single output but there is no
            // guarantee that it can support a texture size of the desktop.
            return ProcessFailure(m_Device, L"Failed to create shared texture. Combined desktop texture size may be larger than the maximum supported supported size of the GPU", L"Error", hr, SystemTransitionsExpectedErrors);
        }
        else
        {
            return ProcessFailure(m_Device, L"Failed to create shared texture", L"Error", hr, SystemTransitionsExpectedErrors);
        }
    }

    // Get keyed mutex
    hr = m_SharedSurf->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&m_KeyMutex));

    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to query for keyed mutex", L"Error", hr);
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
        return ProcessFailure(m_Device, L"Failed to create shader resource", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    //Create textures for multi GPU handling if needed
    if (m_MultiGPUTargetDevice != nullptr)
    {
        //Staging texture
        TexD.Usage = D3D11_USAGE_STAGING;
        TexD.BindFlags = 0;
        TexD.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        TexD.MiscFlags = 0;

        hr = m_Device->CreateTexture2D(&TexD, nullptr, &m_MultiGPUTexStaging);

        if (FAILED(hr))
        {
            return ProcessFailure(m_Device, L"Failed to create staging texture", L"Error", hr);
        }

        //Copy-target texture
        TexD.Usage = D3D11_USAGE_DYNAMIC;
        TexD.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        TexD.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        TexD.MiscFlags = 0;

        hr = m_MultiGPUTargetDevice->CreateTexture2D(&TexD, nullptr, &m_MultiGPUTexTarget);

        if (FAILED(hr))
        {
            return ProcessFailure(m_MultiGPUTargetDevice, L"Failed to create copy-target texture", L"Error", hr);
        }
    }

    return DUPL_RETURN_SUCCESS;
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
    HRESULT hr = m_KeyMutex->AcquireSync(sync_key, m_MaxActiveRefreshDelay);
    if (hr == static_cast<HRESULT>(WAIT_TIMEOUT))
    {
        // Another thread has the keyed mutex so try again later
        return DUPL_RETURN_UPD_RETRY;
    }
    else if (FAILED(hr))
    {
        return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to acquire keyed mutex", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    DUPL_RETURN_UPD ret = DUPL_RETURN_UPD_SUCCESS;

    //Got mutex, so we can access pointer info and shared surface
    DPRect mouse_rect = {PointerInfo->Position.x, PointerInfo->Position.y, int(PointerInfo->Position.x + PointerInfo->ShapeInfo.Width),
                         int(PointerInfo->Position.y + PointerInfo->ShapeInfo.Height)};

    //If mouse state got updated, expand dirty rect to include old and new cursor regions
    if ( (ConfigManager::Get().GetConfigBool(configid_bool_input_mouse_render_cursor)) && (m_MouseLastInfo.LastTimeStamp.QuadPart < PointerInfo->LastTimeStamp.QuadPart) )
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
        DirtyRectTotal.Add(m_OutputPendingDirtyRect);
    }

    bool has_updated_overlay = false;

    //Check all overlays for overlap and collect clipping region from matches
    DPRect clipping_region(-1, -1, -1, -1);

    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        const Overlay& overlay = OverlayManager::Get().GetOverlay(i);

        if (overlay.IsVisible())
        {
            const DPRect& cropping_region = OverlayManager::Get().GetOverlay(i).GetValidatedCropRect();

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

    m_OutputLastClippingRect = clipping_region;

    if (clipping_region.GetTL().x != -1) //Overlapped with at least one overlay
    {
        //Clip unless it's a pending full refresh
        if (m_OutputPendingFullRefresh)
        {
            DirtyRectTotal = {0, 0, m_DesktopWidth, m_DesktopHeight};
            m_OutputPendingFullRefresh = false;
        }
        else
        {
            DirtyRectTotal.ClipWithFull(clipping_region);
        }

        //Set scissor rect for overlay drawing function
        const D3D11_RECT rect_scissor = { DirtyRectTotal.GetTL().x, DirtyRectTotal.GetTL().y, DirtyRectTotal.GetBR().x, DirtyRectTotal.GetBR().y };
        m_DeviceContext->RSSetScissorRects(1, &rect_scissor);

        //Draw shared surface to overlay texture to avoid trouble with transparency on some systems
        bool is_full_texture = DirtyRectTotal.Contains({0, 0, m_DesktopWidth, m_DesktopHeight});
        DrawFrameToOverlayTex(is_full_texture);

        //Only handle cursor if it's in cropping region
        if (mouse_rect.Overlaps(DirtyRectTotal))
        {
            ret = (DUPL_RETURN_UPD)DrawMouseToOverlayTex(PointerInfo);
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

        has_updated_overlay = (ret == DUPL_RETURN_UPD_SUCCESS);
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
        return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to Release keyed mutex", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    //Count frames if performance stats are active
    if ( (has_updated_overlay) && (ConfigManager::Get().GetConfigBool(configid_bool_state_performance_stats_active)) )
    {
        m_PerformanceFrameCount++;
    }

    m_OutputPendingSkippedFrame = false;
    m_OutputPendingDirtyRect = {-1, -1, -1, -1};

    return ret;
}

bool OutputManager::HandleIPCMessage(const MSG& msg)
{
    //Config strings come as WM_COPYDATA
    if (msg.message == WM_COPYDATA)
    {
        COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)msg.lParam;
        
        //Arbitrary size limit to prevent some malicous applications from sending bad data, especially when this is running elevated
        if ( (pcds->dwData < configid_str_MAX) && (pcds->cbData > 0) && (pcds->cbData <= 4096) ) 
        {
            std::string copystr((char*)pcds->lpData, pcds->cbData); //We rely on the data length. The data is sent without the NUL byte

            ConfigID_String str_id = (ConfigID_String)pcds->dwData;
            ConfigManager::Get().SetConfigString(str_id, copystr);

            switch (str_id)
            {
                case configid_str_state_action_value_string:
                {
                    std::vector<CustomAction>& actions = ConfigManager::Get().GetCustomActions();
                    const int id = ConfigManager::Get().GetConfigInt(configid_int_state_action_current);

                    //Unnecessary, but let's save us from the near endless loop in case of a coding error
                    if ( (id < 0) || (id > 10000) )
                        break;

                    while (id >= actions.size())
                    {
                        actions.push_back(CustomAction());
                    }

                    actions[id].ApplyStringFromConfig();
                    break;
                }
                default: break;
            }
        }

        return false;
    }

    bool reset_mirroring = false;
    IPCMsgID msgid = IPCManager::Get().GetIPCMessageID(msg.message);

    //Apply overlay id override if needed
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    int overlay_override_id = ConfigManager::Get().GetConfigInt(configid_int_state_overlay_current_id_override);

    if (overlay_override_id != -1)
    {
        OverlayManager::Get().SetCurrentOverlayID(overlay_override_id);
    }

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
                    std::vector<CustomAction>& actions = ConfigManager::Get().GetCustomActions();

                    if (actions.size() > msg.lParam)
                    {
                        ActionManager::Get().EraseCustomAction(msg.lParam);
                    }

                    break;
                }
                case ipcact_action_do:
                {
                    DoAction((ActionID)msg.lParam);
                    break;
                }
                case ipcact_action_start:
                {
                    DoStartAction((ActionID)msg.lParam);
                    break;
                }
                case ipcact_action_stop:
                {
                    DoStopAction((ActionID)msg.lParam);
                    break;
                }
                case ipcact_keyboard_helper:
                {
                    HandleKeyboardHelperMessage(msg.lParam);
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
                case ipcact_overlay_new:
                {
                    AddOverlay((unsigned int)msg.lParam);
                    break;
                }
                case ipcact_overlay_remove:
                {
                    OverlayManager::Get().RemoveOverlay((unsigned int)msg.lParam);
                    //RemoveOverlay() may have changed active ID, keep in sync
                    ConfigManager::Get().SetConfigInt(configid_int_interface_overlay_current_id, OverlayManager::Get().GetCurrentOverlayID());
                    break;
                }
                case ipcact_overlay_position_sync:
                {
                    DetachedTransformSyncAll();
                    break;
                }
                case ipcact_overlay_swap:
                {
                    OverlayManager::Get().SwapOverlays(OverlayManager::Get().GetCurrentOverlayID(), (unsigned int)msg.lParam);
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
                ConfigManager::Get().SetConfigBool(bool_id, msg.lParam);

                switch (bool_id)
                {
                    case configid_bool_overlay_3D_swapped:
                    {
                        ApplySetting3DMode();
                        break;
                    }
                    case configid_bool_overlay_enabled:
                    case configid_bool_overlay_gazefade_enabled:
                    {
                        ApplySettingTransform();
                        break;
                    }
                    case configid_bool_state_overlay_dragmode:
                    case configid_bool_state_overlay_selectmode:
                    {
                        ApplySettingInputMode();
                        break;
                    }
                    case configid_bool_overlay_input_enabled:
                    case configid_bool_input_mouse_render_intersection_blob:
                    case configid_bool_input_mouse_hmd_pointer_override:
                    {
                        ApplySettingMouseInput();
                        break;
                    }
                    case configid_bool_input_mouse_render_cursor:
                    {
                        m_OutputPendingFullRefresh = true;
                        break;
                    }
                    case configid_bool_performance_single_desktop_mirroring:
                    {
                        if (msg.lParam) //Unify the desktop IDs when turning the setting on
                        {
                            CropToDisplay(OverlayManager::Get().GetConfigData(k_ulOverlayID_Dashboard).ConfigInt[configid_int_overlay_desktop_id], true);
                        }

                        reset_mirroring = true;
                    }
                    case configid_bool_state_performance_stats_active:
                    {
                        if (msg.lParam) //Update GPU Copy state
                        {
                            ConfigManager::Get().SetConfigBool(configid_bool_state_performance_gpu_copy_active, (m_MultiGPUTargetDevice != nullptr));
                            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_bool_state_performance_gpu_copy_active), 
                                                                 (m_MultiGPUTargetDevice != nullptr));
                        }
                    }
                    default: break;
                }
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX)
            {
                ConfigID_Int int_id = (ConfigID_Int)(msg.wParam - configid_bool_MAX);
                ConfigManager::Get().SetConfigInt(int_id, msg.lParam);

                switch (int_id)
                {
                    case configid_int_interface_overlay_current_id:
                    {
                        OverlayManager::Get().SetCurrentOverlayID(msg.lParam);
                        current_overlay_old = (unsigned int)msg.lParam;
                        break;
                    }
                    case configid_int_overlay_desktop_id:
                    {
                        CropToDisplay(msg.lParam);

                        reset_mirroring = ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring);
                        break;
                    }
                    case configid_int_overlay_crop_x:
                    case configid_int_overlay_crop_y:
                    case configid_int_overlay_crop_width:
                    case configid_int_overlay_crop_height:
                    {
                        ApplySettingCrop();
                        ApplySettingTransform();
                        break;
                    }
                    case configid_int_overlay_3D_mode:
                    {
                        ApplySettingTransform();
                        ApplySetting3DMode();
                        break;
                    }
                    case configid_int_overlay_detached_display_mode:
                    case configid_int_overlay_detached_origin:
                    {
                        ApplySettingTransform();
                        break;
                    }
                    case configid_int_state_action_value_int:
                    {
                        std::vector<CustomAction>& actions = ConfigManager::Get().GetCustomActions();
                        const int id = ConfigManager::Get().GetConfigInt(configid_int_state_action_current);

                        //Unnecessary, but let's save us from the near endless loop in case of a coding error
                        if (id > 10000)
                            break;

                        while (id >= actions.size())
                        {
                            actions.push_back(CustomAction());
                        }

                        actions[id].ApplyIntFromConfig();
                        break;
                    }
                    case configid_int_input_mouse_dbl_click_assist_duration_ms:
                    {
                        ApplySettingMouseInput();
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
                    default: break;
                }
            }
            else if (msg.wParam < configid_bool_MAX + configid_int_MAX + configid_float_MAX)
            {
                ConfigID_Float float_id = (ConfigID_Float)(msg.wParam - configid_bool_MAX - configid_int_MAX);

                float previous_value = ConfigManager::Get().GetConfigFloat(float_id);
                ConfigManager::Get().SetConfigFloat(float_id, *(float*)&msg.lParam);    //Interpret lParam as a float variable

                switch (float_id)
                {
                    case configid_float_overlay_width:
                    case configid_float_overlay_curvature:
                    case configid_float_overlay_opacity:
                    case configid_float_overlay_offset_right:
                    case configid_float_overlay_offset_up:
                    case configid_float_overlay_offset_forward:
                    {
                        ApplySettingTransform();
                        break;
                    }
                    case configid_float_input_keyboard_detached_size:
                    {
                        ApplySettingKeyboardScale(previous_value);
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
    //Reset all overlays
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        ApplySettingCrop();
        ApplySettingTransform();
        ApplySetting3DMode();
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

    //These apply to all overlays within the function itself
    ApplySettingMouseInput();
    ApplySettingInputMode();
    ApplySettingUpdateLimiter();

    //Post resolution update to UI app
    IPCManager::Get().PostMessageToUIApp(ipcmsg_action, ipcact_resolution_update);

    //Check if process is elevated and send that info to the UI too
    bool elevated = IsProcessElevated();
    ConfigManager::Get().SetConfigBool(configid_bool_state_misc_process_elevated, elevated);
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_bool_state_misc_process_elevated), elevated);

    //Make sure that the entire overlay texture gets at least one full update for regions that will never be dirty (i.e. blank space not occupied by any desktop)
    m_OutputPendingFullRefresh = true;
}

void OutputManager::ResetCurrentOverlay()
{
    //Reset current overlay
    ApplySettingCrop();
    ApplySettingTransform();
    ApplySettingMouseInput();
    ApplySettingInputMode();
    ApplySetting3DMode();

    ApplySettingUpdateLimiter();

    //Make sure that the entire overlay texture gets at least one full update for regions that will never be dirty (i.e. blank space not occupied by any desktop)
    m_OutputPendingFullRefresh = true;
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

void OutputManager::DrawFrameToOverlayTex(bool clear_rtv)
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

//
// Process both masked and monochrome pointers
//
DUPL_RETURN OutputManager::ProcessMonoMask(bool IsMono, _Inout_ PTR_INFO* PtrInfo, _Out_ INT* PtrWidth, _Out_ INT* PtrHeight, _Out_ INT* PtrLeft, _Out_ INT* PtrTop, _Outptr_result_bytebuffer_(*PtrHeight * *PtrWidth * BPP) BYTE** InitBuffer, _Out_ D3D11_BOX* Box)
{
    // Desktop dimensions
    D3D11_TEXTURE2D_DESC FullDesc;
    m_SharedSurf->GetDesc(&FullDesc);
    INT DesktopWidth = FullDesc.Width;
    INT DesktopHeight = FullDesc.Height;

    // Pointer position
    INT GivenLeft = PtrInfo->Position.x;
    INT GivenTop = PtrInfo->Position.y;

    // Figure out if any adjustment is needed for out of bound positions
    if (GivenLeft < 0)
    {
        *PtrWidth = GivenLeft + static_cast<INT>(PtrInfo->ShapeInfo.Width);
    }
    else if ((GivenLeft + static_cast<INT>(PtrInfo->ShapeInfo.Width)) > DesktopWidth)
    {
        *PtrWidth = DesktopWidth - GivenLeft;
    }
    else
    {
        *PtrWidth = static_cast<INT>(PtrInfo->ShapeInfo.Width);
    }

    if (IsMono)
    {
        PtrInfo->ShapeInfo.Height = PtrInfo->ShapeInfo.Height / 2;
    }

    if (GivenTop < 0)
    {
        *PtrHeight = GivenTop + static_cast<INT>(PtrInfo->ShapeInfo.Height);
    }
    else if ((GivenTop + static_cast<INT>(PtrInfo->ShapeInfo.Height)) > DesktopHeight)
    {
        *PtrHeight = DesktopHeight - GivenTop;
    }
    else
    {
        *PtrHeight = static_cast<INT>(PtrInfo->ShapeInfo.Height);
    }

    if (IsMono)
    {
        PtrInfo->ShapeInfo.Height = PtrInfo->ShapeInfo.Height * 2;
    }

    *PtrLeft = (GivenLeft < 0) ? 0 : GivenLeft;
    *PtrTop = (GivenTop < 0) ? 0 : GivenTop;

    // Staging buffer/texture
    D3D11_TEXTURE2D_DESC CopyBufferDesc;
    CopyBufferDesc.Width = *PtrWidth;
    CopyBufferDesc.Height = *PtrHeight;
    CopyBufferDesc.MipLevels = 1;
    CopyBufferDesc.ArraySize = 1;
    CopyBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    CopyBufferDesc.SampleDesc.Count = 1;
    CopyBufferDesc.SampleDesc.Quality = 0;
    CopyBufferDesc.Usage = D3D11_USAGE_STAGING;
    CopyBufferDesc.BindFlags = 0;
    CopyBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    CopyBufferDesc.MiscFlags = 0;

    ID3D11Texture2D* CopyBuffer = nullptr;
    HRESULT hr = m_Device->CreateTexture2D(&CopyBufferDesc, nullptr, &CopyBuffer);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed creating staging texture for pointer", L"Error", S_OK, SystemTransitionsExpectedErrors); //Shouldn't be critical
    }

    // Copy needed part of desktop image
    Box->left = *PtrLeft;
    Box->top = *PtrTop;
    Box->right = *PtrLeft + *PtrWidth;
    Box->bottom = *PtrTop + *PtrHeight;
    m_DeviceContext->CopySubresourceRegion(CopyBuffer, 0, 0, 0, 0, m_SharedSurf, 0, Box);

    // QI for IDXGISurface
    IDXGISurface* CopySurface = nullptr;
    hr = CopyBuffer->QueryInterface(__uuidof(IDXGISurface), (void **)&CopySurface);
    CopyBuffer->Release();
    CopyBuffer = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI staging texture into IDXGISurface for pointer", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Map pixels
    DXGI_MAPPED_RECT MappedSurface;
    hr = CopySurface->Map(&MappedSurface, DXGI_MAP_READ);
    if (FAILED(hr))
    {
        CopySurface->Release();
        CopySurface = nullptr;
        return ProcessFailure(m_Device, L"Failed to map surface for pointer", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // New mouseshape buffer
    *InitBuffer = new (std::nothrow) BYTE[*PtrWidth * *PtrHeight * BPP];
    if (!(*InitBuffer))
    {
        return ProcessFailure(nullptr, L"Failed to allocate memory for new mouse shape buffer.", L"Error", E_OUTOFMEMORY);
    }

    UINT* InitBuffer32 = reinterpret_cast<UINT*>(*InitBuffer);
    UINT* Desktop32 = reinterpret_cast<UINT*>(MappedSurface.pBits);
    UINT  DesktopPitchInPixels = MappedSurface.Pitch / sizeof(UINT);

    // What to skip (pixel offset)
    UINT SkipX = (GivenLeft < 0) ? (-1 * GivenLeft) : (0);
    UINT SkipY = (GivenTop < 0) ? (-1 * GivenTop) : (0);

    if (IsMono)
    {
        for (INT Row = 0; Row < *PtrHeight; ++Row)
        {
            // Set mask
            BYTE Mask = 0x80;
            Mask = Mask >> (SkipX % 8);
            for (INT Col = 0; Col < *PtrWidth; ++Col)
            {
                // Get masks using appropriate offsets
                BYTE AndMask = PtrInfo->PtrShapeBuffer[((Col + SkipX) / 8) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch))] & Mask;
                BYTE XorMask = PtrInfo->PtrShapeBuffer[((Col + SkipX) / 8) + ((Row + SkipY + (PtrInfo->ShapeInfo.Height / 2)) * (PtrInfo->ShapeInfo.Pitch))] & Mask;
                UINT AndMask32 = (AndMask) ? 0xFFFFFFFF : 0xFF000000;
                UINT XorMask32 = (XorMask) ? 0x00FFFFFF : 0x00000000;

                // Set new pixel
                InitBuffer32[(Row * *PtrWidth) + Col] = (Desktop32[(Row * DesktopPitchInPixels) + Col] & AndMask32) ^ XorMask32;

                // Adjust mask
                if (Mask == 0x01)
                {
                    Mask = 0x80;
                }
                else
                {
                    Mask = Mask >> 1;
                }
            }
        }
    }
    else
    {
        UINT* Buffer32 = reinterpret_cast<UINT*>(PtrInfo->PtrShapeBuffer);

        // Iterate through pixels
        for (INT Row = 0; Row < *PtrHeight; ++Row)
        {
            for (INT Col = 0; Col < *PtrWidth; ++Col)
            {
                // Set up mask
                UINT MaskVal = 0xFF000000 & Buffer32[(Col + SkipX) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch / sizeof(UINT)))];
                if (MaskVal)
                {
                    // Mask was 0xFF
                    InitBuffer32[(Row * *PtrWidth) + Col] = (Desktop32[(Row * DesktopPitchInPixels) + Col] ^ Buffer32[(Col + SkipX) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch / sizeof(UINT)))]) | 0xFF000000;
                }
                else
                {
                    // Mask was 0x00
                    InitBuffer32[(Row * *PtrWidth) + Col] = Buffer32[(Col + SkipX) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch / sizeof(UINT)))] | 0xFF000000;
                }
            }
        }
    }

    // Done with resource
    hr = CopySurface->Unmap();
    CopySurface->Release();
    CopySurface = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to unmap surface for pointer", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Draw mouse provided in buffer to overlay texture
//
DUPL_RETURN OutputManager::DrawMouseToOverlayTex(_In_ PTR_INFO* PtrInfo)
{
    //Just return if we don't need to render it
    if ((!ConfigManager::Get().GetConfigBool(configid_bool_input_mouse_render_cursor)) || (!PtrInfo->Visible))
    {
        return DUPL_RETURN_SUCCESS;
    }

    ID3D11Buffer* VertexBuffer = nullptr;

    // Vars to be used
    D3D11_SUBRESOURCE_DATA InitData;
    D3D11_TEXTURE2D_DESC Desc;
    D3D11_SHADER_RESOURCE_VIEW_DESC SDesc;

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
    INT PtrWidth = 0;
    INT PtrHeight = 0;
    INT PtrLeft = 0;
    INT PtrTop = 0;

    // Buffer used if necessary (in case of monochrome or masked pointer)
    BYTE* InitBuffer = nullptr;

    // Used for copying pixels if necessary
    D3D11_BOX Box;
    Box.front = 0;
    Box.back = 1;

    //Process shape (or just get position when not new cursor)
    switch (PtrInfo->ShapeInfo.Type)
    {
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
        {
            PtrLeft = PtrInfo->Position.x;
            PtrTop = PtrInfo->Position.y;

            PtrWidth = static_cast<INT>(PtrInfo->ShapeInfo.Width);
            PtrHeight = static_cast<INT>(PtrInfo->ShapeInfo.Height);

            break;
        }

        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
        {
            PtrInfo->CursorShapeChanged = true; //Texture content is screen dependent
            ProcessMonoMask(true, PtrInfo, &PtrWidth, &PtrHeight, &PtrLeft, &PtrTop, &InitBuffer, &Box);
            break;
        }

        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
        {
            PtrInfo->CursorShapeChanged = true; //Texture content is screen dependent
            ProcessMonoMask(false, PtrInfo, &PtrWidth, &PtrHeight, &PtrLeft, &PtrTop, &InitBuffer, &Box);
            break;
        }

        default:
            break;
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
        if (m_MouseShaderRes)
        {
            m_MouseShaderRes->Release();
            m_MouseShaderRes = nullptr;
        }

        if (m_MouseTex)
        {
            m_MouseTex->Release();
            m_MouseTex = nullptr;
        }

        return ProcessFailure(m_Device, L"Failed to create mouse pointer vertex buffer in OutputManager", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    //It can occasionally happen that no cursor shape update is detected after resetting duplication, so the m_MouseTex check is more of a workaround, but unproblematic
    if ( (PtrInfo->CursorShapeChanged) || (m_MouseTex == nullptr) ) 
    {
        if (m_MouseTex)
        {
            m_MouseTex->Release();
            m_MouseTex = nullptr;
        }

        if (m_MouseShaderRes)
        {
            m_MouseShaderRes->Release();
            m_MouseShaderRes = nullptr;
        }

        Desc.MipLevels = 1;
        Desc.ArraySize = 1;
        Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        Desc.SampleDesc.Count = 1;
        Desc.SampleDesc.Quality = 0;
        Desc.Usage = D3D11_USAGE_DEFAULT;
        Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Desc.CPUAccessFlags = 0;
        Desc.MiscFlags = 0;

        // Set shader resource properties
        SDesc.Format = Desc.Format;
        SDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SDesc.Texture2D.MostDetailedMip = Desc.MipLevels - 1;
        SDesc.Texture2D.MipLevels = Desc.MipLevels;

        // Set texture properties
        Desc.Width = PtrWidth;
        Desc.Height = PtrHeight;

        // Set up init data
        InitData.pSysMem = (PtrInfo->ShapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) ? PtrInfo->PtrShapeBuffer : InitBuffer;
        InitData.SysMemPitch = (PtrInfo->ShapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) ? PtrInfo->ShapeInfo.Pitch : PtrWidth * BPP;
        InitData.SysMemSlicePitch = 0;

        // Create mouseshape as texture
        hr = m_Device->CreateTexture2D(&Desc, &InitData, &m_MouseTex);
        if (FAILED(hr))
        {
            return ProcessFailure(m_Device, L"Failed to create mouse pointer texture", L"Error", hr, SystemTransitionsExpectedErrors);
        }

        // Create shader resource from texture
        hr = m_Device->CreateShaderResourceView(m_MouseTex, &SDesc, &m_MouseShaderRes);
        if (FAILED(hr))
        {
            m_MouseTex->Release();
            m_MouseTex = nullptr;
            return ProcessFailure(m_Device, L"Failed to create shader resource from mouse pointer texture", L"Error", hr, SystemTransitionsExpectedErrors);
        }
    }

    // Clean init buffer
    if (InitBuffer)
    {
        delete[] InitBuffer;
        InitBuffer = nullptr;
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
    m_DeviceContext->PSSetShaderResources(0, 1, &m_MouseShaderRes);
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
        vrtex.eType = vr::TextureType_DirectX;
        vrtex.eColorSpace = vr::ColorSpace_Gamma;
        vrtex.handle = m_OvrlTex;

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
                return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to acquire keyed mutex", L"Error", hr, SystemTransitionsExpectedErrors);
            }

            DrawFrameToOverlayTex(true);

            //Release keyed mutex
            hr = m_KeyMutex->ReleaseSync(0);
            if (FAILED(hr))
            {
                return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to Release keyed mutex", L"Error", hr, SystemTransitionsExpectedErrors);
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
                return (DUPL_RETURN_UPD)ProcessFailure(m_Device, L"Failed to map staging texture", L"Error", hr, SystemTransitionsExpectedErrors);
            }

            D3D11_MAPPED_SUBRESOURCE mapped_resource_target;
            RtlZeroMemory(&mapped_resource_target, sizeof(D3D11_MAPPED_SUBRESOURCE));
            hr = m_MultiGPUTargetDeviceContext->Map(m_MultiGPUTexTarget, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource_target);

            if (FAILED(hr))
            {
                return (DUPL_RETURN_UPD)ProcessFailure(m_MultiGPUTargetDevice, L"Failed to map copy-target texture", L"Error", hr, SystemTransitionsExpectedErrors);
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
            ID3D11ShaderResourceView* ovrl_shader_res;
            uint32_t ovrl_width;
            uint32_t ovrl_height;
            uint32_t ovrl_native_format;
            vr::ETextureType ovrl_api_type;
            vr::EColorSpace ovrl_color_space;
            vr::VRTextureBounds_t ovrl_tex_bounds;

            vr::VROverlayError ovrl_error = vr::VROverlay()->GetOverlayTexture(m_OvrlHandleDesktopTexture, (void**)&ovrl_shader_res, vrtex.handle, &ovrl_width, &ovrl_height, &ovrl_native_format, 
                                                                               &ovrl_api_type, &ovrl_color_space, &ovrl_tex_bounds);

            if (ovrl_error == vr::VROverlayError_None)
            {
                ID3D11DeviceContext* device_context = (m_MultiGPUTargetDevice != nullptr) ? m_MultiGPUTargetDeviceContext : m_DeviceContext;

                ID3D11Resource* ovrl_tex;
                ovrl_shader_res->GetResource(&ovrl_tex);

                D3D11_BOX box;
                box.left = DirtyRectTotal.GetTL().x;
                box.top = DirtyRectTotal.GetTL().y;
                box.front = 0;
                box.right = DirtyRectTotal.GetBR().x;
                box.bottom = DirtyRectTotal.GetBR().y;
                box.back = 1;

                device_context->CopySubresourceRegion(ovrl_tex, 0, box.left, box.top, 0, (ID3D11Texture2D*)vrtex.handle, 0, &box);

                ovrl_tex->Release();
                ovrl_tex = nullptr;

                // Release shader resource
                vr::VROverlay()->ReleaseNativeOverlayHandle(m_OvrlHandleDesktopTexture, (void*)ovrl_shader_res);
                ovrl_shader_res = nullptr;
            }
            else //Usually shouldn't fail, but fall back to full copy then
            {
                force_full_copy = true;
            }               
        }

        if (force_full_copy) //This is down here so a failed partial copy is picked up as well
        {
            vr::VROverlay()->SetOverlayTexture(m_OvrlHandleDesktopTexture, &vrtex);

            //Apply potential texture change to all overlays and notify them of duplication update
            for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                Overlay& overlay = OverlayManager::Get().GetOverlay(i);
                overlay.AssignTexture();
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

    return DUPL_RETURN_UPD_SUCCESS;
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
        return ProcessFailure(m_Device, L"Failed to create vertex shader", L"Error", hr, SystemTransitionsExpectedErrors);
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
        return ProcessFailure(m_Device, L"Failed to create input layout", L"Error", hr, SystemTransitionsExpectedErrors);
    }
    m_DeviceContext->IASetInputLayout(m_InputLayout);

    Size = ARRAYSIZE(g_PS);
    hr = m_Device->CreatePixelShader(g_PS, Size, nullptr, &m_PixelShader);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create pixel shader", L"Error", hr, SystemTransitionsExpectedErrors);
    }
    Size = ARRAYSIZE(g_PSCURSOR);
    hr = m_Device->CreatePixelShader(g_PSCURSOR, Size, nullptr, &m_PixelShaderCursor);
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create cursor pixel shader", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Reset render target view
//
DUPL_RETURN OutputManager::MakeRTV()
{
    // Create render target for overlay texture
    D3D11_RENDER_TARGET_VIEW_DESC ovrl_tex_rtv_desc;
    D3D11_SHADER_RESOURCE_VIEW_DESC ovrl_tex_shader_res_view_desc;

    ovrl_tex_rtv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    ovrl_tex_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    ovrl_tex_rtv_desc.Texture2D.MipSlice = 0;

    m_Device->CreateRenderTargetView(m_OvrlTex, &ovrl_tex_rtv_desc, &m_OvrlRTV);

    // Create the shader resource view for overlay texture while we're at it
    ovrl_tex_shader_res_view_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    ovrl_tex_shader_res_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    ovrl_tex_shader_res_view_desc.Texture2D.MostDetailedMip = 0;
    ovrl_tex_shader_res_view_desc.Texture2D.MipLevels = 1;

    m_Device->CreateShaderResourceView(m_OvrlTex, &ovrl_tex_shader_res_view_desc, &m_OvrlShaderResView);

    return DUPL_RETURN_SUCCESS;
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

                        if ((!data.ConfigBool[configid_bool_overlay_detached]) || (data.ConfigInt[configid_int_overlay_detached_display_mode] != ovrl_dispmode_scene))
                        {
                            ShowOverlay(i);
                        }
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

                        if ((!data.ConfigBool[configid_bool_overlay_detached]) || (data.ConfigInt[configid_int_overlay_detached_display_mode] == ovrl_dispmode_dplustab))
                        {
                            HideOverlay(i);
                        }
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

                for (unsigned int i = 1; i < OverlayManager::Get().GetOverlayCount(); ++i)
                {
                    OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                    if ((m_DashboardActivatedOnce) && (data.ConfigInt[configid_int_overlay_detached_display_mode] == ovrl_dispmode_dashboard))
                    {
                        ShowOverlay(i);
                    }
                    else if (data.ConfigInt[configid_int_overlay_detached_display_mode] == ovrl_dispmode_scene)
                    {
                        HideOverlay(i);
                    }
                }

                break;
            }
            case vr::VREvent_DashboardDeactivated:
            {
                for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                {
                    OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

                    if ((data.ConfigBool[configid_bool_overlay_detached]) && (data.ConfigInt[configid_int_overlay_detached_display_mode] == ovrl_dispmode_scene))
                    {
                        ShowOverlay(i);
                    }
                    else if ((data.ConfigBool[configid_bool_overlay_detached]) && (data.ConfigInt[configid_int_overlay_detached_display_mode] == ovrl_dispmode_dashboard))
                    {
                        HideOverlay(i);
                    }
                }

                if (ConfigManager::Get().GetConfigBool(configid_bool_misc_auto_focus_scene_app))
                {
                    //Try finding and focusing the window of the current scene application
                    uint32_t pid = vr::VRApplications()->GetCurrentSceneProcessId();

                    if (pid != 0)
                    {
                        HWND scene_process_window = FindMainWindow(pid);

                        if (scene_process_window != 0)
                        {
                            ::SetForegroundWindow(scene_process_window);
                        }
                    }
                }

                break;
            }
            case vr::VREvent_KeyboardCharInput:
            {
                if (vr_event.data.keyboard.uUserValue == m_OvrlHandleMain)      //Input meant for desktop overlays
                {
                    m_InputSim.KeyboardText(vr_event.data.keyboard.cNewInput);
                }
                else  //We don't have the handle of the UI overlay at hand so just assume everything else is meant for that
                {
                    //As only one application can poll events for the dashboard dummy overlay, yet we want dashboard keyboard looks, we send inputs over to the UI app here
                    IPCManager::Get().SendStringToUIApp(configid_str_state_ui_keyboard_string, vr_event.data.keyboard.cNewInput, m_WindowHandle);
                }
                break;
            }
            case vr::VREvent_KeyboardClosed:
            {
                //Tell UI that the keyboard helper should no longer be displayed
                ConfigManager::Get().SetConfigInt(configid_int_state_keyboard_visible_for_overlay_id, -1);
                IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_keyboard_visible_for_overlay_id), -1);
                break;
            }
            case vr::VREvent_Input_ActionManifestReloaded:
            case vr::VREvent_Input_BindingsUpdated:
            case vr::VREvent_Input_BindingLoadSuccessful:
            case vr::VREvent_TrackedDeviceActivated:
            case vr::VREvent_TrackedDeviceDeactivated:
            {
                m_VRInput.RefreshAnyActionBound();
                break;
            }
            case vr::VREvent_Quit:
            {
                return true;
            }
        }
    }

    //Now handle events for the actual overlays
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
        vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

        while (vr::VROverlay()->PollNextOverlayEvent(ovrl_handle, &vr_event, sizeof(vr_event)))
        {
            switch (vr_event.eventType)
            {
                case vr::VREvent_MouseMove:
                {
                    if ( (!ConfigManager::Get().GetConfigBool(configid_bool_overlay_input_enabled)) || (m_MouseIgnoreMoveEvent) ||
                         ( (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)) && (i != k_ulOverlayID_Dashboard) ) ||
                         (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_selectmode)) )
                    {
                        break;
                    }

                    if ((ConfigManager::Get().GetConfigInt(configid_int_state_mouse_dbl_click_assist_duration_ms) == 0) ||
                        (::GetTickCount64() >= m_MouseLastClickTick + ConfigManager::Get().GetConfigInt(configid_int_state_mouse_dbl_click_assist_duration_ms)))
                    {
                        //Check coordinates if HMDPointerOverride is enabled
                        if ((ConfigManager::Get().GetConfigBool(configid_bool_input_mouse_hmd_pointer_override)) &&
                            (vr::VROverlay()->GetPrimaryDashboardDevice() == vr::k_unTrackedDeviceIndex_Hmd))
                        {
                            POINT pt;
                            ::GetCursorPos(&pt);

                            //If mouse coordinates are not what the last laser pointer was (with tolerance), meaning some other source moved it
                            if ((abs(pt.x - m_MouseLastLaserPointerX) > 32) || (abs(pt.y - m_MouseLastLaserPointerY) > 32))
                            {
                                m_MouseIgnoreMoveEventMissCount++; //GetCursorPos() may lag behind or other jumps may occasionally happen. We count up a few misses first before acting on them

                                int max_miss_count = 10; //Arbitrary number, but appears to work reliably

                                if (m_PerformanceUpdateLimiterDelay.QuadPart != 0) //When updates are limited, try adapting for the lower update rate
                                {
                                    max_miss_count = std::max(1, max_miss_count - int((m_PerformanceUpdateLimiterDelay.QuadPart / 1000) / 20));
                                }

                                if (m_MouseIgnoreMoveEventMissCount > max_miss_count)
                                {
                                    m_MouseIgnoreMoveEvent = true;

                                    //Set flag for all overlays
                                    if ((i == 0) || (!ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)))
                                    {
                                        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
                                        {
                                            vr::VROverlay()->SetOverlayFlag(OverlayManager::Get().GetOverlay(i).GetHandle(), vr::VROverlayFlags_HideLaserIntersection, true);
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

                        //Get hotspot value to use
                        int hotspot_x = 0;
                        int hotspot_y = 0;

                        if (m_MouseLastInfo.Visible) //We're using a cached value here so we don't have to lock the shared surface for this function
                        {
                            hotspot_x = m_MouseDefaultHotspotX;
                            hotspot_y = m_MouseDefaultHotspotY;
                        }

                        //GL space (0,0 is bottom left), so we need to flip that around
                        m_MouseLastLaserPointerX = (round(vr_event.data.mouse.x) - hotspot_x) + m_DesktopX;
                        m_MouseLastLaserPointerY = ((-round(vr_event.data.mouse.y) + m_DesktopHeight) - hotspot_y) + m_DesktopY;
                        m_InputSim.MouseMove(m_MouseLastLaserPointerX, m_MouseLastLaserPointerY);

                        //This is only relevant when limiting updates. See Update() for details.
                        m_MouseLaserPointerUsedLastUpdate = true;
                    }

                    break;
                }
                case vr::VREvent_MouseButtonDown:
                {
                    if (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_selectmode))
                    {
                        if (vr_event.data.mouse.button == vr::VRMouseButton_Left)
                        {
                            //Select this as current overlay
                            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_overlay_current_id), i);
                            current_overlay_old = i; //Set a new reset value since we're in the middle of a temporary current overlay loop
                        }
                        break;
                    }
                    else if ( (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)) && (i != k_ulOverlayID_Dashboard) )
                    {
                        if (vr_event.data.mouse.button == vr::VRMouseButton_Left)
                        {
                            if (m_DragModeDeviceID == -1)
                            {
                                DragStart();
                            }
                        }
                        else if (vr_event.data.mouse.button == vr::VRMouseButton_Right)
                        {
                            if (!m_DragGestureActive)
                            {
                                DragGestureStart();
                            }
                        }
                        break;
                    }

                    if (m_MouseIgnoreMoveEvent) //This can only be true if IgnoreHMDPointer enabled
                    {
                        m_MouseIgnoreMoveEvent = false;

                        ResetMouseLastLaserPointerPos();
                        ApplySettingMouseInput();

                        break;  //Click to restore shouldn't generate a mouse click
                    }

                    m_MouseLastClickTick = ::GetTickCount64();

                    switch (vr_event.data.mouse.button)
                    {
                        case vr::VRMouseButton_Left:    m_InputSim.MouseSetLeftDown(true);   break;
                        case vr::VRMouseButton_Right:   m_InputSim.MouseSetRightDown(true);  break;
                        case vr::VRMouseButton_Middle:  m_InputSim.MouseSetMiddleDown(true); break; //This is never sent by SteamVR, but supported in case it ever starts happening
                    }

                    break;
                }
                case vr::VREvent_MouseButtonUp:
                {
                    if (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_selectmode))
                    {
                        break;
                    }
                    else if ( (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)) && (i != k_ulOverlayID_Dashboard) )
                    {
                        if ((vr_event.data.mouse.button == vr::VRMouseButton_Left) && (m_DragModeDeviceID != -1))
                        {
                            DragFinish();
                        }
                        else if ((vr_event.data.mouse.button == vr::VRMouseButton_Right) && (m_DragGestureActive))
                        {
                            DragGestureFinish();
                        }

                        break;
                    }

                    switch (vr_event.data.mouse.button)
                    {
                        case vr::VRMouseButton_Left:    m_InputSim.MouseSetLeftDown(false);   break;
                        case vr::VRMouseButton_Right:   m_InputSim.MouseSetRightDown(false);  break;
                        case vr::VRMouseButton_Middle:  m_InputSim.MouseSetMiddleDown(false); break;
                    }

                    break;
                }
                case vr::VREvent_LockMousePosition: //This seems like an interesting feature to support, but this never triggers and the freeze on the default desktop also doesn't work here somehow
                {
                    break;
                }
                case vr::VREvent_ScrollDiscrete:
                {
                    if ( ( (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)) && (i != k_ulOverlayID_Dashboard) ) ||
                         (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_selectmode)) )
                        break;

                    if (vr_event.data.scroll.xdelta != 0.0f) //This doesn't seem to be ever sent by SteamVR
                    {
                        m_InputSim.MouseWheelHorizontal(vr_event.data.scroll.xdelta);
                    }

                    if (vr_event.data.scroll.ydelta != 0.0f)
                    {
                        m_InputSim.MouseWheelVertical(vr_event.data.scroll.ydelta);
                    }

                    break;
                }
                case vr::VREvent_ScrollSmooth:
                {
                    //Smooth scrolls are only used for dragging mode
                    if (m_DragModeDeviceID != -1)
                    {
                        DragAddDistance(vr_event.data.scroll.ydelta);
                    }
                    break;
                }
                case vr::VREvent_ButtonPress:
                {
                    if (vr_event.data.controller.button == Button_Dashboard_GoHome)
                    {
                        DoAction((ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_go_home_action_id));
                    }
                    else if (vr_event.data.controller.button == Button_Dashboard_GoBack)
                    {
                        DoAction((ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_go_back_action_id));
                    }

                    break;
                }
                case vr::VREvent_KeyboardCharInput:
                {
                    m_InputSim.KeyboardText(vr_event.data.keyboard.cNewInput);
                    break;
                }
                case vr::VREvent_KeyboardClosed:
                {
                    //Tell UI that the keyboard helper should no longer be displayed
                    ConfigManager::Get().SetConfigInt(configid_int_state_keyboard_visible_for_overlay_id, -1);
                    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_keyboard_visible_for_overlay_id), -1);

                    break;
                }
                case vr::VREvent_FocusEnter:
                {
                    m_OvrlInputActive = true;

                    if (vr::VROverlay()->GetPrimaryDashboardDevice() == vr::k_unTrackedDeviceIndex_Hmd)
                        ResetMouseLastLaserPointerPos();

                    break;
                }
                case vr::VREvent_FocusLeave:
                {
                    m_OvrlInputActive = false;

                    //If input is active from the input binding, reset the flag for every overlay (except dashboard)
                    if (m_OvrlDetachedInteractiveAll)
                    {
                        m_OvrlDetachedInteractiveAll = false;

                        for (unsigned int i = 1; i < OverlayManager::Get().GetOverlayCount(); ++i)
                        {
                            OverlayManager::Get().GetOverlay(i).SetGlobalInteractiveFlag(false);
                        }
                    }
                    else
                    {
                        overlay.SetGlobalInteractiveFlag(false);
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

    //Handle stuff coming from SteamVR Input
    m_VRInput.Update();

    if (m_VRInput.GetSetDetachedInteractiveDown())
    {
        if (!m_OvrlDetachedInteractiveAll) //This isn't a direct toggle since the laser pointer blocks the SteamVR Input Action Set
        {
            m_OvrlDetachedInteractiveAll = true;
            
            //Set flag for all overlays, except dashboard
            for (unsigned int i = 1; i < OverlayManager::Get().GetOverlayCount(); ++i)
            {
                OverlayManager::Get().GetOverlay(i).SetGlobalInteractiveFlag(true);
            }
        }
    }

    m_VRInput.HandleGlobalActionShortcuts(*this);

    //Finish up pending keyboard input collected into the queue
    m_InputSim.KeyboardTextFinish();

    //Update postion if necessary
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
                if ( (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)) && (i != k_ulOverlayID_Dashboard) )
                {
                    if (m_DragModeDeviceID != -1)
                    {
                        DragUpdate();
                    }
                    else if (m_DragGestureActive)
                    {
                        DragGestureUpdate();
                    }
                }

                if ((data.ConfigBool[configid_bool_overlay_detached]) && (m_DragModeDeviceID == -1) && (!m_DragGestureActive) &&
                    (data.ConfigInt[configid_int_overlay_detached_origin] == ovrl_origin_hmd_floor))
                {
                    DetachedTransformUpdateHMDFloor();
                }
                else if ( (dashboard_origin_was_updated) && (m_DragModeDeviceID == -1) && (!m_DragGestureActive) && 
                          ( (i == k_ulOverlayID_Dashboard) || (data.ConfigInt[configid_int_overlay_detached_origin] == ovrl_origin_dashboard) ) )
                {
                    ApplySettingTransform();
                }

                DetachedInteractionAutoToggle();
            }

            DetachedOverlayGazeFade();
        }
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

    return false;
}

void OutputManager::HandleKeyboardHelperMessage(LPARAM lparam)
{
    switch (lparam)
    {
        //The 3 toggle keys (UI state is automatically synced from actual keyboard state)
        case VK_CONTROL:
        case VK_MENU:
        case VK_SHIFT:
        case VK_LWIN:
        {
            if (GetAsyncKeyState(lparam) < 0) //Is key already down
                m_InputSim.KeyboardSetUp(lparam);
            else
                m_InputSim.KeyboardSetDown(lparam);
            
            break;
        }
        default:
        {
            m_InputSim.KeyboardPressAndRelease(lparam); //This mimics the rest of the SteamVR keyboards behavior, as in, no holding down of keys
        }
    }
}

bool OutputManager::HandleOverlayProfileLoadMessage(LPARAM lparam)
{
    bool multi_overlay = (lparam != ipcactv_ovrl_profile_single);
    int desktop_id_prev = ConfigManager::Get().GetConfigInt(configid_int_overlay_desktop_id);
    const std::string& profile_name = ConfigManager::Get().GetConfigString(configid_str_state_profile_name_load);

    if (profile_name == "Default")
    {
        ConfigManager::Get().LoadOverlayProfileDefault(multi_overlay);
    }
    else
    {
        if (multi_overlay)
        {
            ConfigManager::Get().LoadMultiOverlayProfileFromFile(profile_name + ".ini", (lparam == ipcactv_ovrl_profile_multi));
        }
        else
        {
            ConfigManager::Get().LoadOverlayProfileFromFile(profile_name + ".ini");
        }
    }

    //Reset mirroing entirely if desktop was changed (only in single desktop mode)
    if ( (ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring)) && (ConfigManager::Get().GetConfigInt(configid_int_overlay_desktop_id) != desktop_id_prev) )
        return true; //Reset mirroring

    ResetOverlays(); //This does everything relevant

    return false;
}

void OutputManager::LaunchApplication(const std::string& path_utf8, const std::string& arg_utf8)
{
    if (!m_ComInitDone) //Let's only do this if really needed
    {
        if (::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) != RPC_E_CHANGED_MODE)
        {
            m_ComInitDone = true;
        }
    }

    //Convert path and arg to utf16
    std::wstring path_wstr = WStringConvertFromUTF8(path_utf8.c_str());
    std::wstring arg_wstr  = WStringConvertFromUTF8(arg_utf8.c_str());

    if (!path_wstr.empty())
    {   
        ::ShellExecute(nullptr, nullptr, path_wstr.c_str(), arg_wstr.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

void OutputManager::ResetMouseLastLaserPointerPos()
{
    //Set last pointer values to current to not trip the movement detection up
    POINT pt;
    ::GetCursorPos(&pt);
    m_MouseLastLaserPointerX = pt.x;
    m_MouseLastLaserPointerY = pt.y;
}

void OutputManager::CropToActiveWindow()
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
                int& crop_x      = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_x);
                int& crop_y      = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_y);
                int& crop_width  = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_width);
                int& crop_height = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_height);

                crop_x = crop_rect.GetTL().x;
                crop_y = crop_rect.GetTL().y;
                crop_width = crop_rect.GetWidth();
                crop_height = crop_rect.GetHeight();

                //Send them over to UI
                IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_overlay_crop_x),      crop_x);
                IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_overlay_crop_y),      crop_y);
                IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_overlay_crop_width),  crop_width);
                IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);

                ApplySettingCrop();
                ApplySettingTransform();
            }
        }
    }
}

void OutputManager::CropToDisplay(int display_id, bool do_not_apply_setting)
{
    int& crop_x      = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_x);
    int& crop_y      = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_y);
    int& crop_width  = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_width);
    int& crop_height = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_height);
    
    if ( (!ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring)) && (display_id >= 0) && (display_id < m_DesktopRects.size()) ) 
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

    //Send change to UI as well (also set override since this may be called during one)
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), OverlayManager::Get().GetCurrentOverlayID());
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_x),      crop_x);
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_y),      crop_y);
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_width),  crop_width);
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);
    IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);

    //In single desktop mode, set desktop ID for all overlays
    if (ConfigManager::Get().GetConfigBool(configid_bool_performance_single_desktop_mirroring))
    {
        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
        {
            OverlayManager::Get().GetConfigData(i).ConfigInt[configid_int_overlay_desktop_id] = display_id;

            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)i);
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_overlay_desktop_id), display_id);
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
        }
    }

    //Applying the setting when a duplication resets happens right after has the chance of screwing up the transform (too many transform updates?), so give the option to not do it
    if (!do_not_apply_setting)
    {
        ApplySettingCrop();
        ApplySettingTransform();
    }
}

void OutputManager::AddOverlay(unsigned int base_id)
{
    //Add overlay based on data of lParam ID overlay and set it active
    unsigned int new_id = OverlayManager::Get().GetOverlayCount();
    OverlayManager::Get().AddOverlay(OverlayManager::Get().GetConfigData(base_id), (base_id == k_ulOverlayID_Dashboard));
    OverlayManager::Get().SetCurrentOverlayID(new_id);
    ConfigManager::Get().SetConfigInt(configid_int_interface_overlay_current_id, (int)new_id);

    //Automatically reset the matrix to a saner default by putting it next to the base overlay in most cases
    DetachedTransformReset(OverlayManager::Get().GetOverlay(base_id).GetHandle());

    ResetCurrentOverlay();
}

void OutputManager::ApplySetting3DMode()
{
    vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().GetCurrentOverlay().GetHandle();
    int mode = ConfigManager::Get().GetConfigInt(configid_int_overlay_3D_mode);

    if (mode != ovrl_3Dmode_none)
    {
        if (ConfigManager::Get().GetConfigBool(configid_bool_overlay_3D_swapped))
        {
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Parallel, false);
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Crossed, true);
        }
        else
        {
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Parallel, true);
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Crossed, false);
        }
    }
    else
    {
        vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Parallel, false);
        vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SideBySide_Crossed, false);
        vr::VROverlay()->SetOverlayTexelAspect(ovrl_handle, 1.0f);
    }

    if ( (mode == ovrl_3Dmode_ou) || (mode == ovrl_3Dmode_hou) )
    {
        OverlayManager::Get().GetCurrentOverlay().SetTextureSource(ovrl_tex_source_desktop_duplication_3dou_converted);
    }
    else
    {
        OverlayManager::Get().GetCurrentOverlay().SetTextureSource(ovrl_tex_source_desktop_duplication);
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

    ApplySettingCrop();

    RefreshOpenVROverlayTexture(DPRect(-1, -1, -1, -1), true);
}

void OutputManager::ApplySettingTransform()
{
    Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
    vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

    //Fixup overlay visibility if needed
    //This has to be done first since there seem to be issues with moving invisible overlays
    const bool is_detached = ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached);
    bool should_be_visible = overlay.ShouldBeVisible();

    if ( (!should_be_visible) && (is_detached) && (m_OvrlDashboardActive) && (m_OvrlDashboardActive) && (ConfigManager::Get().GetConfigBool(configid_bool_overlay_enabled)) &&
         (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragselectmode_show_hidden)) )
    {
        should_be_visible = true;
        overlay.SetOpacity(0.25f);
    }
    else if (!ConfigManager::Get().GetConfigBool(configid_bool_overlay_gazefade_enabled))
    {
        overlay.SetOpacity(ConfigManager::Get().GetConfigFloat(configid_float_overlay_opacity));
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

    //Update width
    const float width = ConfigManager::Get().GetConfigFloat(configid_float_overlay_width);
    float height = 0.0f;
    float dashboard_offset = 0.0f;
    OverlayOrigin overlay_origin = (is_detached) ? (OverlayOrigin)ConfigManager::Get().GetConfigInt(configid_int_overlay_detached_origin) : ovrl_origin_dashboard;

    vr::VROverlay()->SetOverlayWidthInMeters(ovrl_handle, width);

    //Calculate height of the main overlay to set dashboard dummy height correctly
    if ( (overlay.GetID() == k_ulOverlayID_Dashboard) && (should_be_visible) )
    {
        const DPRect& crop_rect = overlay.GetValidatedCropRect();
        int crop_width = crop_rect.GetWidth(), crop_height = crop_rect.GetHeight();

        int mode_3d = ConfigManager::Get().GetConfigInt(configid_int_overlay_3D_mode);

        if (m_OutputInvalid) //No cropping on invalid output image
        {
            crop_width  = m_DesktopWidth;
            crop_height = m_DesktopHeight;
        }
        else if ((mode_3d == ovrl_3Dmode_ou)) //Converted Over-Under changes texture dimensions, so adapt
        {
            crop_width  *= 2;
            crop_height /= 2;
        }

        //Overlay is twice as tall when SBS3D/OU3D is active
        if ((mode_3d == ovrl_3Dmode_sbs) || (mode_3d == ovrl_3Dmode_ou))
            crop_height *= 2;

        height = width * ((float)crop_height / crop_width);

        //Setting the dashboard dummy width/height has some kind of race-condition and getting the transform coordinates below may use the old size
        //So we instead calculate the offset the height change would cause and change the dummy height last
        vr::VROverlay()->GetOverlayWidthInMeters(m_OvrlHandleDashboardDummy, &dashboard_offset);
        dashboard_offset = (dashboard_offset - (height + 0.20f)) / 2.0f;
    }

    //Update Curvature
    float curve = ConfigManager::Get().GetConfigFloat(configid_float_overlay_curvature);

    if (curve == -1.0f) //-1 is auto, match the dashboard
    {
        vr::VROverlayHandle_t system_dashboard;
        vr::VROverlay()->FindOverlay("system.systemui", &system_dashboard);

        if (system_dashboard != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlay()->GetOverlayCurvature(system_dashboard, &curve);
        }
        else //Very odd, but hey
        {
            curve = 0.0f;
        }
    }

    vr::VROverlay()->SetOverlayCurvature(ovrl_handle, curve);

    //Update transform
    vr::HmdMatrix34_t matrix = {0};
    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

    switch (overlay_origin)
    {
        case ovrl_origin_room:
        {
            matrix = ConfigManager::Get().GetOverlayDetachedTransform().toOpenVR34();
            vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, universe_origin, &matrix);
            break;
        }
        case ovrl_origin_hmd_floor:
        {
            DetachedTransformUpdateHMDFloor();
            break;
        }
        case ovrl_origin_dashboard:
        {
            if (is_detached)
            {
                Matrix4 matrix_base = DragGetBaseOffsetMatrix() * ConfigManager::Get().GetOverlayDetachedTransform();
                matrix = matrix_base.toOpenVR34();
            }
            else //Attach to dashboard dummy to pretend we have normal dashboard overlay
            {
                vr::VROverlay()->GetTransformForOverlayCoordinates(m_OvrlHandleDashboardDummy, universe_origin, {0.5f, -0.5f}, &matrix); //-0.5 is past bottom end of the overlay, might break someday

                //Y: Align from bottom edge, and add 0.28m base offset to make space for the UI bar 
                OffsetTransformFromSelf(matrix, ConfigManager::Get().GetConfigFloat(configid_float_overlay_offset_right),
                                                ConfigManager::Get().GetConfigFloat(configid_float_overlay_offset_up) + height + dashboard_offset + 0.28,
                                                ConfigManager::Get().GetConfigFloat(configid_float_overlay_offset_forward));
            }

            vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, universe_origin, &matrix);
            break;
        }
        case ovrl_origin_hmd:
        {
            matrix = ConfigManager::Get().GetOverlayDetachedTransform().toOpenVR34();
            vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(ovrl_handle, vr::k_unTrackedDeviceIndex_Hmd, &matrix);
            break;
        }
        case ovrl_origin_right_hand:
        {
            vr::TrackedDeviceIndex_t device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

            if (device_index != vr::k_unTrackedDeviceIndexInvalid)
            {
                matrix = ConfigManager::Get().GetOverlayDetachedTransform().toOpenVR34();
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
            vr::TrackedDeviceIndex_t index_tracker = GetFirstVRTracker();

            if (index_tracker != vr::k_unTrackedDeviceIndexInvalid)
            {
                matrix = ConfigManager::Get().GetOverlayDetachedTransform().toOpenVR34();
                vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(ovrl_handle, index_tracker, &matrix);
            }
            else //Not connected, uh put it to 0?
            {
                vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, universe_origin, &matrix);
            }

            break;
        }
    }

    //Dashboard dummy still needs correct width/height set for the top dashboard bar above it to be visible
    if (overlay.GetID() == k_ulOverlayID_Dashboard)
    {
        vr::VROverlay()->SetOverlayWidthInMeters(m_OvrlHandleDashboardDummy, std::max(height + 0.20f, 1.525f)); //Enforce minimum height to fit open settings UI
    }
}

void OutputManager::ApplySettingCrop()
{
    Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
    OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
    vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

    //Initialize crop to desktop 0 first if desktop ID is -2 (default value)
    if (data.ConfigInt[configid_int_overlay_desktop_id] == -2)
    {
        data.ConfigInt[configid_int_overlay_desktop_id] = 0;
        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), OverlayManager::Get().GetCurrentOverlayID());
        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_overlay_desktop_id), 0);
        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);

        CropToDisplay(0);
        return; //CropToDisplay will call this function again
    }

    //Set up overlay cropping
    vr::VRTextureBounds_t tex_bounds;
    vr::VRTextureBounds_t tex_bounds_prev;

    if (m_OutputInvalid)
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
    
    //Use full texture if everything checks out or texture source is 3D Over-Under converted
    if ( (overlay.GetTextureSource() == ovrl_tex_source_desktop_duplication_3dou_converted) || 
         ( (crop_rect.GetTL().x == 0) && (crop_rect.GetTL().y == 0) && (crop_rect.GetWidth() == m_DesktopWidth) && (crop_rect.GetHeight() == m_DesktopHeight) ) )
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

        tex_bounds.uMin = (crop_rect.GetTL().x + offset_x) / m_DesktopWidth;
        tex_bounds.vMin = (crop_rect.GetTL().y + offset_y) / m_DesktopHeight;
        tex_bounds.uMax = (crop_rect.GetBR().x - offset_x) / m_DesktopWidth;
        tex_bounds.vMax = (crop_rect.GetBR().y - offset_y) / m_DesktopHeight;
    }

    //Compare old to new bounds to see if a full refresh is required
    vr::VROverlay()->GetOverlayTextureBounds(ovrl_handle, &tex_bounds_prev);

    if ((tex_bounds.uMin < tex_bounds_prev.uMin) || (tex_bounds.vMin < tex_bounds_prev.vMin) || (tex_bounds.uMax > tex_bounds_prev.uMax) || (tex_bounds.vMax > tex_bounds_prev.vMax))
    {
        RefreshOpenVROverlayTexture(DPRect(-1, -1, -1, -1), true);
    }

    vr::VROverlay()->SetOverlayTextureBounds(ovrl_handle, &tex_bounds);
}

void OutputManager::ApplySettingInputMode()
{
    bool drag_or_select_mode_enabled = ( (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)) || (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_selectmode)) );
    //Always applies to all overlays
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().GetCurrentOverlay().GetHandle();

        if ((ConfigManager::Get().GetConfigBool(configid_bool_overlay_input_enabled)) || (drag_or_select_mode_enabled) )
        {
            //Don't activate drag mode for HMD origin when the pointer is also the HMD (or it's the dashboard overlay)
            if ( ((vr::VROverlay()->GetPrimaryDashboardDevice() == vr::k_unTrackedDeviceIndex_Hmd) && (ConfigManager::Get().GetConfigInt(configid_int_overlay_detached_origin) == ovrl_origin_hmd)) )
            {
                vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_None);
            }
            else if ( (i != k_ulOverlayID_Dashboard) || (ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_selectmode)) )
            {
                vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_Mouse);
                vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_HideLaserIntersection, false);
            }
            else
            {
                vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_HideLaserIntersection, !ConfigManager::Get().GetConfigBool(configid_bool_input_mouse_render_intersection_blob));
            }

            m_MouseIgnoreMoveEvent = false;
        }
        else
        {
            vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_None);
        }

        //Sync matrix if it's been turned off
        if ( (!drag_or_select_mode_enabled) && (!ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)) && (i != k_ulOverlayID_Dashboard) )
        {
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)i);
            IPCManager::Get().SendStringToUIApp(configid_str_state_detached_transform_current, ConfigManager::Get().GetOverlayDetachedTransform().toString(), m_WindowHandle);
            IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
        }

        ApplySettingTransform();
    }

    //Restore mouse settings it's been turned off
    if (!drag_or_select_mode_enabled)
    {
        ApplySettingMouseInput();
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::ApplySettingMouseInput()
{
    //Always applies to all overlays
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        vr::VROverlayHandle_t ovrl_handle = OverlayManager::Get().GetCurrentOverlay().GetHandle();

        if ( (ConfigManager::Get().GetConfigBool(configid_bool_overlay_input_enabled)) && (!m_OutputInvalid) )
        {
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);
            vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_Mouse);

            //Set double-click assist duration from user config value
            if (ConfigManager::Get().GetConfigInt(configid_int_input_mouse_dbl_click_assist_duration_ms) == -1)
            {
                ConfigManager::Get().SetConfigInt(configid_int_state_mouse_dbl_click_assist_duration_ms, ::GetDoubleClickTime());
            }
            else
            {
                ConfigManager::Get().SetConfigInt(configid_int_state_mouse_dbl_click_assist_duration_ms, ConfigManager::Get().GetConfigInt(configid_int_input_mouse_dbl_click_assist_duration_ms));
            }
        }
        else
        {
            vr::VROverlay()->SetOverlayInputMethod(ovrl_handle, vr::VROverlayInputMethod_None);
        }

        vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_HideLaserIntersection, !ConfigManager::Get().GetConfigBool(configid_bool_input_mouse_render_intersection_blob));

        vr::HmdVector2_t mouse_scale;
        mouse_scale.v[0] = m_DesktopWidth;
        mouse_scale.v[1] = m_DesktopHeight;

        vr::VROverlay()->SetOverlayMouseScale(ovrl_handle, &mouse_scale);
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::ApplySettingKeyboardScale(float last_used_scale)
{
    vr::VROverlayHandle_t ovrl_handle_keyboard = vr::k_ulOverlayHandleInvalid;
    vr::VROverlay()->FindOverlay("system.keyboard", &ovrl_handle_keyboard);

    if (ovrl_handle_keyboard != vr::k_ulOverlayHandleInvalid)
    {
        if (ConfigManager::Get().GetConfigInt(configid_int_state_keyboard_visible_for_overlay_id) > k_ulOverlayID_Dashboard)
        {
            vr::HmdMatrix34_t hmd_mat;
            vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

            vr::VROverlay()->GetOverlayTransformAbsolute(ovrl_handle_keyboard, &universe_origin, &hmd_mat);

            Matrix4 mat = hmd_mat;
            Vector3 translation = mat.getTranslation();

            mat.setTranslation(Vector3(0.0f, 0.0f, 0.0f));
            mat.scale(1.0 / last_used_scale);               //Undo last scale (not ideal, but this is a stopgap solution anyways)
            mat.scale(ConfigManager::Get().GetConfigFloat(configid_float_input_keyboard_detached_size));
            mat.setTranslation(translation);

            hmd_mat = mat.toOpenVR34();

            vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle_keyboard, universe_origin, &hmd_mat);
        }
    }
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
    if (ConfigManager::Get().GetConfigInt(configid_int_performance_update_limit_mode) == update_limit_mode_ms)
    {
        limit_ms = ConfigManager::Get().GetConfigFloat(configid_float_performance_update_limit_ms);
    }
    else if (ConfigManager::Get().GetConfigInt(configid_int_performance_update_limit_mode) == update_limit_mode_fps)
    {
        int enum_id = ConfigManager::Get().GetConfigInt(configid_int_performance_update_limit_fps);

        if (enum_id <= update_limit_fps_50)
        {
            limit_ms = fps_enum_values_ms[enum_id];
        }
    }
 
    //See if there are any overrides from visible overlays
    //This is the straight forward and least error-prone way, not quite the most efficient one
    //Calls to this are minimized and there typically aren't many overlays so it's not really that bad (and we do iterate over all of them in many other places too)
    bool is_first_override = true;
    for (unsigned int i = k_ulOverlayID_Dashboard; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        const Overlay& overlay        = OverlayManager::Get().GetOverlay(i);
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

        if ( (overlay.IsVisible()) && (data.ConfigInt[configid_int_overlay_update_limit_override_mode] != update_limit_mode_off) )
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
    }
    
    m_PerformanceUpdateLimiterDelay.QuadPart = 1000.0f * limit_ms;
}

void OutputManager::DragStart(bool is_gesture_drag)
{
    //This is also used by DragGestureStart() (with is_gesture_drag = true), but only to convert between overlay origins.
    //Doesn't need calls to the other DragUpdate() or DragFinish() functions in that case
    vr::TrackedDeviceIndex_t device_index = vr::VROverlay()->GetPrimaryDashboardDevice();

    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

    if ( (device_index < vr::k_unMaxTrackedDeviceCount) && (poses[device_index].bPoseIsValid) )
    {
        if (!is_gesture_drag)
        {
            m_DragModeDeviceID = device_index;
        }

        Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
        vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();
        m_DragModeOverlayID = overlay.GetID();

        m_DragModeMatrixSourceStart = poses[device_index].mDeviceToAbsoluteTracking;

        switch (ConfigManager::Get().GetConfigInt(configid_int_overlay_detached_origin))
        {
            case ovrl_origin_hmd:
            {
                if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
                {
                    vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, vr::TrackingUniverseStanding, &poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
                }
                break;
            }
            case ovrl_origin_right_hand:
            {
                vr::TrackedDeviceIndex_t index_right_hand = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

                if ( (index_right_hand != vr::k_unTrackedDeviceIndexInvalid) && (poses[index_right_hand].bPoseIsValid) )
                {
                    vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, vr::TrackingUniverseStanding, &poses[index_right_hand].mDeviceToAbsoluteTracking);
                }
                break;
            }
            case ovrl_origin_left_hand:
            {
                vr::TrackedDeviceIndex_t index_left_hand = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);

                if ( (index_left_hand != vr::k_unTrackedDeviceIndexInvalid) && (poses[index_left_hand].bPoseIsValid) )
                {
                    vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, vr::TrackingUniverseStanding, &poses[index_left_hand].mDeviceToAbsoluteTracking);
                }
                break;
            }
            case ovrl_origin_aux:
            {
                vr::TrackedDeviceIndex_t index_tracker = GetFirstVRTracker();

                if ( (index_tracker != vr::k_unTrackedDeviceIndexInvalid) && (poses[index_tracker].bPoseIsValid) )
                {
                    vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, vr::TrackingUniverseStanding, &poses[index_tracker].mDeviceToAbsoluteTracking);
                }
                break;
            }
        }

        vr::HmdMatrix34_t transform_target;
        vr::TrackingUniverseOrigin origin;
        vr::VROverlay()->GetOverlayTransformAbsolute(ovrl_handle, &origin, &transform_target);
        m_DragModeMatrixTargetStart = transform_target;

        if (!is_gesture_drag)
        {
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, false);
            vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SendVRSmoothScrollEvents, true);
        }
    }
}

void OutputManager::DragUpdate()
{
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

    if (poses[m_DragModeDeviceID].bPoseIsValid)
    {       
        Matrix4 matrix_source_current = poses[m_DragModeDeviceID].mDeviceToAbsoluteTracking;
        Matrix4 matrix_target_new = m_DragModeMatrixTargetStart;

        Matrix4 matrix_source_start_inverse = m_DragModeMatrixSourceStart;
        matrix_source_start_inverse.invert();

        matrix_source_current = matrix_source_current * matrix_source_start_inverse;

        matrix_target_new = matrix_source_current * matrix_target_new;
           
        matrix_source_current = matrix_target_new;

        vr::HmdMatrix34_t vrmat = matrix_source_current.toOpenVR34();
        vr::VROverlay()->SetOverlayTransformAbsolute(OverlayManager::Get().GetOverlay(m_DragModeOverlayID).GetHandle(), vr::TrackingUniverseStanding, &vrmat);
    }
}

void OutputManager::DragAddDistance(float distance)
{
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

    if (poses[m_DragModeDeviceID].bPoseIsValid)
    {
        //Clamp distance as there can be inflated scroll values when holding the wheel down before starting a drag for some reason (doesn't limit normally occuring values)
        distance = clamp(distance, -0.5f, 0.5f);

        Matrix4 mat_drag_device = m_DragModeMatrixSourceStart;

        //Apply tip offset if possible (usually the case)
        vr::TrackedDeviceIndex_t index_right = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
        vr::TrackedDeviceIndex_t index_left  = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
        if ( (m_DragModeDeviceID == index_left) || (m_DragModeDeviceID == index_right) ) 
        {
            mat_drag_device = mat_drag_device * GetControllerTipMatrix( (m_DragModeDeviceID == index_right) );
        }

        //Take the drag device start orientation and the overlay's start translation and offset forward from there
        mat_drag_device.setTranslation(m_DragModeMatrixTargetStart.getTranslation());
        OffsetTransformFromSelf(mat_drag_device, 0.0f, 0.0f, distance * -0.5f);
        m_DragModeMatrixTargetStart.setTranslation(mat_drag_device.getTranslation());
    }
}

Matrix4 OutputManager::DragGetBaseOffsetMatrix()
{
    Matrix4 matrix; //Identity

    float width = ConfigManager::Get().GetConfigFloat(configid_float_overlay_width);
    OverlayOrigin overlay_origin;

    if (ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached))
    {
        overlay_origin = (OverlayOrigin)ConfigManager::Get().GetConfigInt(configid_int_overlay_detached_origin);
    }
    else
    {
        overlay_origin = ovrl_origin_dashboard;
    }

    vr::VROverlayTransformType transform_type;
    vr::VROverlay()->GetOverlayTransformType(OverlayManager::Get().GetCurrentOverlay().GetHandle(), &transform_type);

    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;

    switch (overlay_origin)
    {
        case ovrl_origin_room:
        {
            break;
        }
        case ovrl_origin_hmd_floor:
        {
            vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
            vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

            if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
            {
                Matrix4 mat_pose = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
                Vector3 pos_offset = mat_pose.getTranslation();

                pos_offset.y = 0.0f;
                matrix.setTranslation(pos_offset);
            }
            break;
        }
        case ovrl_origin_dashboard:
        {
            //This code is prone to break when Valve changes the entire dashboard once again
            vr::VROverlayHandle_t system_dashboard;
            vr::VROverlay()->FindOverlay("system.systemui", &system_dashboard);

            if (system_dashboard != vr::k_ulOverlayHandleInvalid)
            {
                vr::HmdMatrix34_t matrix_overlay_system;

                vr::HmdVector2_t overlay_system_size;
                vr::VROverlay()->GetOverlayMouseScale(system_dashboard, &overlay_system_size); //Coordinate size should be mouse scale
                
                vr::VROverlay()->GetTransformForOverlayCoordinates(system_dashboard, universe_origin, { overlay_system_size.v[0]/2.0f, 0.0f }, &matrix_overlay_system);
                matrix = matrix_overlay_system;

                if (m_DashboardHMD_Y == -100.0f)    //If Desktop+ was started with the dashboard open, the value will still be default, so set it now
                {
                    UpdateDashboardHMD_Y();
                }

                Vector3 pos_offset = matrix.getTranslation();
                pos_offset.y = m_DashboardHMD_Y;
                matrix.setTranslation(pos_offset);

            }
                
            break;
        }
        case ovrl_origin_hmd:
        case ovrl_origin_right_hand:
        case ovrl_origin_left_hand:
        case ovrl_origin_aux:
        {
            //This is used for the dragging only. In other cases the origin is identity, as it's attached to the controller via OpenVR
            vr::TrackedDeviceIndex_t device_index;

            switch (overlay_origin)
            {
                case ovrl_origin_hmd:        device_index = vr::k_unTrackedDeviceIndex_Hmd;                                                              break;
                case ovrl_origin_right_hand: device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand); break;
                case ovrl_origin_left_hand:  device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);  break;
                case ovrl_origin_aux:        device_index = GetFirstVRTracker();                                                                         break;
                default:                     device_index = vr::k_unTrackedDeviceIndexInvalid;
            }
             
            if (device_index != vr::k_unTrackedDeviceIndexInvalid)
            {
                vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
                vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

                if (poses[device_index].bPoseIsValid)
                {
                    matrix = poses[device_index].mDeviceToAbsoluteTracking;
                }
            }
            break;
        }
    }

    return matrix;
}

void OutputManager::DragFinish()
{
    DragUpdate();

    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID(); //DragGetBaseOffsetMatrix() needs the right overlay as current
    OverlayManager::Get().SetCurrentOverlayID(m_DragModeOverlayID);

    Overlay& overlay = OverlayManager::Get().GetOverlay(m_DragModeOverlayID);
    vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

    vr::HmdMatrix34_t transform_target;
    vr::TrackingUniverseOrigin origin;

    vr::VROverlay()->GetOverlayTransformAbsolute(ovrl_handle, &origin, &transform_target);
    Matrix4 matrix_target_finish = transform_target;

    Matrix4 matrix_target_base = DragGetBaseOffsetMatrix();
    matrix_target_base.invert();

    ConfigManager::Get().GetOverlayDetachedTransform() = matrix_target_base * matrix_target_finish;
    ApplySettingTransform();

    //Restore normal mode
    m_DragModeDeviceID = -1;
    
    vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SendVRSmoothScrollEvents, false);

    if (ConfigManager::Get().GetConfigBool(configid_bool_overlay_input_enabled))
    {
        vr::VROverlay()->SetOverlayFlag(ovrl_handle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);
    }

    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::DragGestureStart()
{
    DragStart(true); //Call the other drag start function to convert the overlay transform to absolute. This doesn't actually start the normal drag

    DragGestureUpdate();

    m_DragGestureScaleDistanceStart = m_DragGestureScaleDistanceLast;
    m_DragGestureScaleWidthStart = ConfigManager::Get().GetConfigFloat(configid_float_overlay_width);
    m_DragGestureActive = true;
}

void OutputManager::DragGestureUpdate()
{
    vr::TrackedDeviceIndex_t index_right = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
    vr::TrackedDeviceIndex_t index_left  = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);

    if ( (index_right != vr::k_unTrackedDeviceIndexInvalid) && (index_left != vr::k_unTrackedDeviceIndexInvalid) )
    {
        Overlay& overlay = OverlayManager::Get().GetOverlay(m_DragModeOverlayID);
        OverlayConfigData& overlay_data = OverlayManager::Get().GetConfigData(m_DragModeOverlayID);
        vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

        vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

        if ( (poses[index_right].bPoseIsValid) && (poses[index_left].bPoseIsValid) )
        {
            Matrix4 mat_right = poses[index_right].mDeviceToAbsoluteTracking;
            Matrix4 mat_left  = poses[index_left].mDeviceToAbsoluteTracking;

            //Gesture Scale
            m_DragGestureScaleDistanceLast = mat_right.getTranslation().distance(mat_left.getTranslation());

            if (m_DragGestureActive)
            {
                //Scale is just the start scale multiplied by the factor of changed controller distance
                float width = m_DragGestureScaleWidthStart * (m_DragGestureScaleDistanceLast / m_DragGestureScaleDistanceStart);
                vr::VROverlay()->SetOverlayWidthInMeters(ovrl_handle, width);
                overlay_data.ConfigFloat[configid_float_overlay_width] = width;
            
                //Send adjusted width to the UI app
                IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)m_DragModeOverlayID);
                IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_float_overlay_width), *(LPARAM*)&width);
                IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
            }

            //Gesture Rotate
            Matrix4 matrix_rotate_current = mat_left;
            //Use up-vector multiplied by rotation matrix to avoid locking at near-up transforms
            Vector3 up = m_DragGestureRotateMatLast * Vector3(0.0f, 1.0f, 0.0f);
            up.normalize();
            //Rotation motion is taken from the differences between left controller lookat(right controller) results
            TransformLookAt(matrix_rotate_current, mat_right.getTranslation(), up);

            if (m_DragGestureActive)
            {
                //Get difference of last drag frame
                Matrix4 matrix_rotate_last_inverse = m_DragGestureRotateMatLast;
                matrix_rotate_last_inverse.setTranslation({0.0f, 0.0f, 0.0f});
                matrix_rotate_last_inverse.invert();

                Matrix4 matrix_rotate_current_at_origin = matrix_rotate_current;
                matrix_rotate_current_at_origin.setTranslation({0.0f, 0.0f, 0.0f});

                Matrix4 matrix_rotate_diff = matrix_rotate_current_at_origin * matrix_rotate_last_inverse;

                //Apply difference
                Matrix4& mat_overlay = m_DragModeMatrixTargetStart;
                Vector3 pos = mat_overlay.getTranslation();
                mat_overlay.setTranslation({0.0f, 0.0f, 0.0f});
                mat_overlay = matrix_rotate_diff * mat_overlay;
                mat_overlay.setTranslation(pos);

                vr::HmdMatrix34_t vrmat = mat_overlay.toOpenVR34();
                vr::VROverlay()->SetOverlayTransformAbsolute(ovrl_handle, vr::TrackingUniverseStanding, &vrmat);
            }

            m_DragGestureRotateMatLast = matrix_rotate_current;
        }
    }
}

void OutputManager::DragGestureFinish()
{
    unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID(); //DragGetBaseOffsetMatrix() needs the right overlay as current
    OverlayManager::Get().SetCurrentOverlayID(m_DragModeOverlayID);

    Matrix4 matrix_target_base = DragGetBaseOffsetMatrix();
    matrix_target_base.invert();

    ConfigManager::Get().GetOverlayDetachedTransform() = matrix_target_base * m_DragModeMatrixTargetStart;
    ApplySettingTransform();

    m_DragGestureActive = false;
    OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);
}

void OutputManager::DetachedTransformSyncAll()
{
    for (unsigned int i = 1; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        OverlayManager::Get().SetCurrentOverlayID(i);

        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)i);
        IPCManager::Get().SendStringToUIApp(configid_str_state_detached_transform_current, ConfigManager::Get().GetOverlayDetachedTransform().toString(), m_WindowHandle);
        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
    }
}

void OutputManager::DetachedTransformReset(vr::VROverlayHandle_t ovrl_handle_ref)
{
    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;
    Matrix4& transform = ConfigManager::Get().GetOverlayDetachedTransform();
    transform.identity(); //Reset to identity

    OverlayOrigin overlay_origin = (OverlayOrigin)ConfigManager::Get().GetConfigInt(configid_int_overlay_detached_origin);

    if (ovrl_handle_ref == vr::k_ulOverlayHandleInvalid)
    {
        ovrl_handle_ref = m_OvrlHandleMain;
    }

    //Position next to reference if not main overlay or room/HMD Pos/dashboard origin
    if ((ovrl_handle_ref != m_OvrlHandleMain) || (overlay_origin <= ovrl_origin_dashboard))
    {
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
        float x_offset_mul = ( (ConfigManager::Get().GetConfigFloat(configid_float_overlay_width) / ref_overlay_width) / 2.0f) + 1.0f;

        //Put it next to the refernce overlay so it can actually be seen
        vr::HmdVector2_t coordinate_offset = {mouse_scale.v[0] * x_offset_mul, mouse_scale.v[1] / 2.0f};
        vr::VROverlay()->GetTransformForOverlayCoordinates(ovrl_handle_ref, universe_origin, coordinate_offset, &overlay_transform);
        transform = overlay_transform;

        //Restore reference overlay state if it was changed
        if (ref_overlay_changed)
        {
            vr::VROverlay()->HideOverlay(ovrl_handle_ref);
            vr::VROverlay()->SetOverlayAlpha(ovrl_handle_ref, ref_overlay_alpha_orig);
        }

        //Adapt to base offset for non-room origins
        if (overlay_origin != ovrl_origin_room)
        {
            //DragGetBaseOffsetMatrix() needs detached to be true or else it will return offset for dashboard
            bool detached_old = ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached); 
            ConfigManager::Get().SetConfigBool(configid_bool_overlay_detached, true);

            Matrix4 transform_base = DragGetBaseOffsetMatrix();
            transform_base.invert();
            transform = transform_base * transform;

            ConfigManager::Get().SetConfigBool(configid_bool_overlay_detached, detached_old);
        }
    }
    else //Otherwise add some default offset to the previously reset to identity value
    {
        switch (overlay_origin)
        {
            case ovrl_origin_hmd:
            {
                OffsetTransformFromSelf(transform, 0.0f, 0.0f, -2.5f);
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
                    OffsetTransformFromSelf(transform, 0.0f, -0.1f, 0.0f); //This seems like a good default, at least for Index controllers
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
                    OffsetTransformFromSelf(transform, 0.0f, -0.1f, 0.0f);
                }

                break;
            }
            case ovrl_origin_aux:
            {
                OffsetTransformFromSelf(transform, 0.0f, 0.0f, -0.05f);
                break;
            }
            default: break;
        }
    }

    //Sync reset with UI app
    IPCManager::Get().SendStringToUIApp(configid_str_state_detached_transform_current, ConfigManager::Get().GetOverlayDetachedTransform().toString(), m_WindowHandle);

    ApplySettingTransform();
}

void OutputManager::DetachedTransformAdjust(unsigned int packed_value)
{
    Matrix4& transform = ConfigManager::Get().GetOverlayDetachedTransform();
    float distance = 0.05f;
    float angle = 1.0f;
    Vector3 translation = transform.getTranslation();

    //Unpack
    IPCActionOverlayPosAdjustTarget target = (IPCActionOverlayPosAdjustTarget)(packed_value & 0xF);
    bool increase = (packed_value >> 4);

    if (target >= ipcactv_ovrl_pos_adjust_rotx)
    {
        transform.setTranslation(Vector3(0.0f, 0.0f, 0.0f));
    }

    if (!increase)
    {
        distance *= -1.0f;
        angle *= -1.0f;
    }

    switch (target)
    {
        case ipcactv_ovrl_pos_adjust_updown:    OffsetTransformFromSelf(transform, 0.0f,     distance, 0.0f);     break;
        case ipcactv_ovrl_pos_adjust_rightleft: OffsetTransformFromSelf(transform, distance, 0.0f,     0.0f);     break;
        case ipcactv_ovrl_pos_adjust_forwback:  OffsetTransformFromSelf(transform, 0.0f,     0.0f,     distance); break;
        case ipcactv_ovrl_pos_adjust_rotx:      transform.rotateX(angle);                                         break;
        case ipcactv_ovrl_pos_adjust_roty:      transform.rotateY(angle);                                         break;
        case ipcactv_ovrl_pos_adjust_rotz:      transform.rotateZ(angle);                                         break;
    }

    if (target >= ipcactv_ovrl_pos_adjust_rotx)
    {
        transform.setTranslation(translation);
    }

    ApplySettingTransform();
}

void OutputManager::DetachedTransformUpdateHMDFloor()
{
    Matrix4 matrix = DragGetBaseOffsetMatrix();
    matrix *= ConfigManager::Get().GetOverlayDetachedTransform();

    vr::HmdMatrix34_t matrix_ovr = matrix.toOpenVR34();
    vr::VROverlay()->SetOverlayTransformAbsolute(OverlayManager::Get().GetCurrentOverlay().GetHandle(), vr::TrackingUniverseStanding, &matrix_ovr);
}

void OutputManager::DetachedInteractionAutoToggle()
{
    Overlay& overlay = OverlayManager::Get().GetCurrentOverlay();
    vr::VROverlayHandle_t ovrl_handle = overlay.GetHandle();

    float max_distance = ConfigManager::Get().GetConfigFloat(configid_float_input_detached_interaction_max_distance);

    if ((ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached)) && (overlay.IsVisible()) && (max_distance != 0.0f) && (!vr::VROverlay()->IsDashboardVisible()))
    {
        bool do_set_interactive = false;

        //Add some additional distance for disabling interaction again
        if (overlay.GetGlobalInteractiveFlag())
        {
            max_distance += 0.01f;
        }

        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, GetTimeNowToPhotons(), poses, vr::k_unMaxTrackedDeviceCount);

        //Check left and right hand controller
        vr::ETrackedControllerRole controller_role = vr::TrackedControllerRole_LeftHand;
        for (;;)
        {
            vr::TrackedDeviceIndex_t device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(controller_role);

            if ((device_index < vr::k_unMaxTrackedDeviceCount) && (poses[device_index].bPoseIsValid))
            {
                //Get matrix with tip offset
                Matrix4 mat_controller = poses[device_index].mDeviceToAbsoluteTracking;
                mat_controller = mat_controller * GetControllerTipMatrix( (controller_role == vr::TrackedControllerRole_RightHand) );

                //Set up intersection test
                Vector3 v_pos = mat_controller.getTranslation();
                Vector3 forward = {mat_controller[8], mat_controller[9], mat_controller[10]};
                forward *= -1.0f;

                vr::VROverlayIntersectionParams_t params;
                params.eOrigin = vr::TrackingUniverseStanding;
                params.vSource = {v_pos.x, v_pos.y, v_pos.z};
                params.vDirection = {forward.x, forward.y, forward.z};

                vr::VROverlayIntersectionResults_t results;

                if ( (vr::VROverlay()->ComputeOverlayIntersection(ovrl_handle, &params, &results)) && (results.fDistance <= max_distance) )
                {
                    do_set_interactive = true;
                }
            }

            if (controller_role == vr::TrackedControllerRole_LeftHand)
            {
                controller_role = vr::TrackedControllerRole_RightHand;
            }
            else
            {
                break;
            }
        }

        overlay.SetGlobalInteractiveFlag(do_set_interactive);
    }
}

void OutputManager::DetachedOverlayGazeFade()
{
    if (  (ConfigManager::Get().GetConfigBool(configid_bool_overlay_gazefade_enabled)) && (!ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_dragmode)) && 
         (!ConfigManager::Get().GetConfigBool(configid_bool_state_overlay_selectmode)) )
    {
        vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;
        vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

        if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
        {
            //Distance the gaze point is offset from HMD (useful range 0.25 - 1.0)
            float gaze_distance = ConfigManager::Get().GetConfigFloat(configid_float_overlay_gazefade_distance);
            //Rate the fading gets applied when looking off the gaze point (useful range 4.0 - 30, depends on overlay size) 
            float fade_rate = ConfigManager::Get().GetConfigFloat(configid_float_overlay_gazefade_rate) * 10.0f; 

            Matrix4 mat_pose = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;

            Matrix4 mat_overlay = DragGetBaseOffsetMatrix();
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

            OffsetTransformFromSelf(mat_pose, 0.0f, 0.0f, -gaze_distance);

            Vector3 pos_gaze = mat_pose.getTranslation();
            float distance = mat_overlay.getTranslation().distance(pos_gaze);

            gaze_distance = std::min(gaze_distance, 1.0f); //To get useful fading past 1m distance we'll have to limit the value to 1m here for the math below

            float alpha = clamp((distance * -fade_rate) + ((gaze_distance - 0.1f) * 10.0f), 0.0f, 1.0f); //There's nothing smart behind this, just trial and error
            alpha *= ConfigManager::Get().GetConfigFloat(configid_float_overlay_opacity);
            
            OverlayManager::Get().GetCurrentOverlay().SetOpacity(alpha);
        }
    }
}

void OutputManager::UpdateDashboardHMD_Y()
{
    vr::TrackingUniverseOrigin universe_origin = vr::TrackingUniverseStanding;
    vr::TrackedDevicePose_t poses[vr::k_unTrackedDeviceIndex_Hmd + 1];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe_origin, GetTimeNowToPhotons(), poses, vr::k_unTrackedDeviceIndex_Hmd + 1);

    if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
    {
        Matrix4 mat_pose = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
        m_DashboardHMD_Y = mat_pose.getTranslation().y;
    }
}

bool OutputManager::HasDashboardMoved()
{
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

bool OutputManager::IsAnyOverlayUsingGazeFade() const
{
    //This is the straight forward, simple version. The smart one, efficiently keeping track properly, could come some other time*
    //*we know it probably won't happen any time soon
    for (unsigned int i = 1; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

        if ( (data.ConfigBool[configid_bool_overlay_enabled]) && (data.ConfigBool[configid_bool_overlay_gazefade_enabled]) )
        {
            return true;
        }
    }

    return false;
}

void OutputManager::UpdatePerformanceStates()
{
    //Frame counter, the frames themselves are counted in Update()
    if ( (ConfigManager::Get().GetConfigBool(configid_bool_state_performance_stats_active)) && (::GetTickCount64() >= m_PerformanceFrameCountStartTick + 1000) )
    {
        //A second has passed, reset the value
        ConfigManager::Get().SetConfigInt(configid_int_state_performance_duplication_fps, m_PerformanceFrameCount);
        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_performance_duplication_fps), m_PerformanceFrameCount);

        m_PerformanceFrameCountStartTick = ::GetTickCount64();
        m_PerformanceFrameCount = 0;
    }
}

const LARGE_INTEGER& OutputManager::GetUpdateLimiterDelay()
{
    return m_PerformanceUpdateLimiterDelay;
}

void OutputManager::ConvertOUtoSBS(Overlay& overlay, OUtoSBSConverter& converter)
{
    //Convert()'s arguments are almost all stuff from OutputManager, so we take this roundabout way of calling it
    const DPRect& crop_rect = overlay.GetValidatedCropRect();

    bool res = converter.Convert(m_Device, m_DeviceContext, m_MultiGPUTargetDevice, m_MultiGPUTargetDeviceContext, m_OvrlTex,
                                 m_DesktopWidth, m_DesktopHeight, crop_rect.GetTL().x, crop_rect.GetTL().y, crop_rect.GetWidth(), crop_rect.GetHeight());

    if (res)
    {
        vr::Texture_t vrtex;
        vrtex.eType = vr::TextureType_DirectX;
        vrtex.eColorSpace = vr::ColorSpace_Gamma;
        vrtex.handle = converter.GetTexture(); //OUtoSBSConverter takes care of multi-gpu support automatically, so no further processing needed

        vr::VROverlay()->SetOverlayTexture(overlay.GetHandle(), &vrtex);
    }
}

void OutputManager::DoAction(ActionID action_id)
{
    if (action_id >= action_custom)
    {
        std::vector<CustomAction>& actions = ConfigManager::Get().GetCustomActions();

        if (actions.size() + action_custom > action_id)
        {
            CustomAction& action = actions[action_id - action_custom];

            switch (action.FunctionType)
            {
                case caction_press_keys:
                {
                    m_InputSim.KeyboardSetDown(action.KeyCodes);
                    m_InputSim.KeyboardSetUp(action.KeyCodes);
                    break;
                }
                case caction_type_string:
                {
                    m_InputSim.KeyboardText(action.StrMain.c_str(), true);
                    m_InputSim.KeyboardTextFinish();
                    break;
                }
                case caction_launch_application:
                {
                    LaunchApplication(action.StrMain.c_str(), action.StrArg.c_str());
                    break;
                }
                case caction_toggle_overlay_enabled_state:
                {
                    if (OverlayManager::Get().GetOverlayCount() > (unsigned int)action.IntID)
                    {
                        OverlayConfigData& data = OverlayManager::Get().GetConfigData((unsigned int)action.IntID);
                        data.ConfigBool[configid_bool_overlay_enabled] = !data.ConfigBool[configid_bool_overlay_enabled];

                        unsigned int current_overlay_old = OverlayManager::Get().GetCurrentOverlayID();
                        OverlayManager::Get().SetCurrentOverlayID((unsigned int)action.IntID);
                        ApplySettingTransform();
                        OverlayManager::Get().SetCurrentOverlayID(current_overlay_old);

                        //Sync change
                        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), (int)action.IntID);
                        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_bool_overlay_enabled), data.ConfigBool[configid_bool_overlay_enabled]);
                        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_overlay_current_id_override), -1);
                    }
                }
            }
            return;
        }
    }
    else
    {
        switch (action_id)
        {
            case action_show_keyboard:
            {
                const Overlay& ovrl_current = OverlayManager::Get().GetCurrentOverlay();

                if (ConfigManager::Get().GetConfigInt(configid_int_state_keyboard_visible_for_overlay_id) != -1)
                {
                    //If it's already displayed for an overlay, hide it instead
                    vr::VROverlay()->HideKeyboard();

                    //Config state is set from the event
                }
                else
                {
                    vr::VROverlayHandle_t ovrl_keyboard_target = ovrl_current.GetHandle();
                    int ovrl_keyboard_id = ovrl_current.GetID();
                    
                    //If dashboard overlay or using dashboard origin, show it for the dummy so it gets treated like a dashboard keyboard
                    if ( (ovrl_current.GetID() == k_ulOverlayID_Dashboard) || (ConfigManager::Get().GetConfigInt(configid_int_overlay_detached_origin) == ovrl_origin_dashboard) )
                    {
                        ovrl_keyboard_target = m_OvrlHandleDashboardDummy;
                        ovrl_keyboard_id = 0;
                    }

                    vr::EVROverlayError keyboard_error = vr::VROverlay()->ShowKeyboardForOverlay(ovrl_keyboard_target, vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine,
                                                                                                 vr::KeyboardFlag_Minimal, "Desktop+", 1024, "", m_OvrlHandleMain);

                    if (keyboard_error == vr::VROverlayError_None)
                    {
                        //Covers whole overlay
                        vr::HmdRect2_t keyrect;
                        keyrect.vTopLeft = {0.0f, 1.0f};
                        keyrect.vBottomRight = {1.0f, 0.0f};

                        vr::VROverlay()->SetKeyboardPositionForOverlay(ovrl_keyboard_target, keyrect);  //Avoid covering the overlay with the keyboard

                        ConfigManager::Get().SetConfigInt(configid_int_state_keyboard_visible_for_overlay_id, ovrl_keyboard_id);

                        ApplySettingKeyboardScale(1.0f);    //Apply detached keyboard scale if necessary

                        //Tell UI that the keyboard helper can be displayed
                        IPCManager::Get().PostMessageToUIApp(ipcmsg_set_config, ConfigManager::Get().GetWParamForConfigID(configid_int_state_keyboard_visible_for_overlay_id), ovrl_keyboard_id);
                    }
                }
                break;
            }
            case action_crop_active_window_toggle:
            {
                //If the action is used with one of the controller buttons, the events will fire another time if the new cropping values happen to have the laser pointer leave and
                //re-enter the overlay for a split second while the button is still down during the dimension change. 
                //This would immediately undo the action, which we want to prevent, so a 100 ms pause between toggles is enforced 
                static ULONGLONG last_toggle_tick = 0;

                if (::GetTickCount64() <= last_toggle_tick + 100)
                    break;

                last_toggle_tick = ::GetTickCount64();

                int& crop_x      = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_x);
                int& crop_y      = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_y);
                int& crop_width  = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_width);
                int& crop_height = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_height);

                //Check if crop is just exactly the current desktop
                bool crop_equals_current_desktop = false;
                int desktop_id = ConfigManager::Get().GetConfigInt(configid_int_overlay_desktop_id);

                if ( (desktop_id >= 0) && (desktop_id < m_DesktopRects.size()) )
                {
                    DPRect crop_rect(crop_x, crop_y, crop_x + crop_width, crop_y + crop_height);

                    crop_equals_current_desktop = (crop_rect == m_DesktopRects[desktop_id]);
                }

                //If uncropped, crop to active window
                if ( (crop_equals_current_desktop) || ((crop_x == 0) && (crop_y == 0) && (crop_width == -1) && (crop_height == -1)) )
                {
                    CropToActiveWindow();
                }
                else //If cropped in some way, active window or not, reset it
                {
                    CropToDisplay(desktop_id);
                }
                break;
            }
            default: break;
        }
    }
}

//This is like DoAction, but split between start and stop
//Currently only used for input actions. The UI will send a start message already when pressing down on the button and an stop one only after releasing for these kind of actions.
//Also used for global shortcuts, where non-input actions simply get forwarded to DoAction()
void OutputManager::DoStartAction(ActionID action_id) 
{
    if (action_id >= action_custom)
    {
        std::vector<CustomAction>& actions = ConfigManager::Get().GetCustomActions();

        if (actions.size() + action_custom > action_id)
        {
            CustomAction& action = actions[action_id - action_custom];

            if (action.FunctionType == caction_press_keys)
            {
                m_InputSim.KeyboardSetDown(action.KeyCodes);
            }
            else
            {
                DoAction(action_id);
            }
        }
    }
}

void OutputManager::DoStopAction(ActionID action_id)
{
    if (action_id >= action_custom)
    {
        std::vector<CustomAction>& actions = ConfigManager::Get().GetCustomActions();

        if (actions.size() + action_custom > action_id)
        {
            CustomAction& action = actions[action_id - action_custom];

            if (action.FunctionType == caction_press_keys)
            {
                m_InputSim.KeyboardSetUp(action.KeyCodes);
            }
        }
    }
}

//
// Releases all references
//
void OutputManager::CleanRefs()
{
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

    if (m_OvrlShaderResView)
    {
        m_OvrlShaderResView->Release();
        m_OvrlShaderResView = nullptr;
    }

    if (m_MouseTex)
    {
        m_MouseTex->Release();
        m_MouseTex = nullptr;
    }

    if (m_MouseShaderRes)
    {
        m_MouseShaderRes->Release();
        m_MouseShaderRes = nullptr;
    }

    //Reset mouse state variables too
    m_MouseLastClickTick = 0;
    m_MouseIgnoreMoveEvent = false;
    m_MouseLastInfo = {0};
    m_MouseLastInfo.ShapeInfo.Type = DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR;
    m_MouseLastLaserPointerX = -1;
    m_MouseLastLaserPointerY = -1;
    m_MouseDefaultHotspotX = 0;
    m_MouseDefaultHotspotY = 0;
    
    if (m_KeyMutex)
    {
        m_KeyMutex->Release();
        m_KeyMutex = nullptr;
    }

    if (m_Factory)
    {
        m_Factory->Release();
        m_Factory = nullptr;
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