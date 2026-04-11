#include "COMWrapper.h"

#include <wrl/client.h>
#include <shldisp.h>
#include <shlobj.h>
#include <shellapi.h>

COMWrapper g_COMWrapper;

#define WM_COMWRAPPER                          WM_APP           //Base ID
#define WM_COMWRAPPER_SHELL_EXECUTE            WM_COMWRAPPER    //Sent to COMWrapper thread to call ShellExecute()
#define WM_COMWRAPPER_SHELL_EXECUTE_UNELEVATED WM_COMWRAPPER+1  //Sent to COMWrapper thread to call ShellExecute() in the explorer process
#define WM_COMWRAPPER_SHOW_WINDOW_SWITCHER     WM_COMWRAPPER+2  //Sent to COMWrapper thread to call WindowSwitcher() in the shell dispatch

COMWrapper& COMWrapper::Get()
{
    return g_COMWrapper;
}

void COMWrapper::SetActive(bool is_active)
{
    if (is_active)
    {
        InitIfNeeded();     //Not really necessary to ever call like this though
    }
    else
    {
        if (m_ThreadHandle != nullptr)
        {
            ::PostThreadMessage(m_ThreadID, WM_QUIT, 0, 0);

            ::CloseHandle(m_ThreadHandle);
            m_ThreadHandle = nullptr;
            m_ThreadID     = 0;
        }
    }
}

bool COMWrapper::IsActive() const
{
    return (m_ThreadHandle != nullptr);
}

void COMWrapper::CallShellExecute(const std::wstring& path, const std::wstring& arg, const std::wstring& dir, INT showcmd)
{
    InitIfNeeded();

    {
        std::lock_guard<std::mutex> lock(m_ThreadMutex);
        m_ThreadData.ParamPath = path;
        m_ThreadData.ParamArg  = arg;
        m_ThreadData.ParamDir  = dir;
    }

    ::PostThreadMessage(m_ThreadID, WM_COMWRAPPER_SHELL_EXECUTE, showcmd, 0);
}

bool COMWrapper::CallShellExecuteUnelevated(const std::wstring& path, const std::wstring& arg, const std::wstring& dir, INT showcmd)
{
    InitIfNeeded();

    {
        std::lock_guard<std::mutex> lock(m_ThreadMutex);
        m_ThreadData.ParamPath = path;
        m_ThreadData.ParamArg  = arg;
        m_ThreadData.ParamDir  = dir;
    }

    ::PostThreadMessage(m_ThreadID, WM_COMWRAPPER_SHELL_EXECUTE_UNELEVATED, showcmd, 0);

    //Wait for thread to execute and put return value into thread data
    if (m_ThreadReturnEvent != nullptr)
    {
        if (::WaitForSingleObject(m_ThreadReturnEvent, 5000) == WAIT_OBJECT_0)
        {
            ::ResetEvent(m_ThreadReturnEvent);

            {
                std::lock_guard<std::mutex> lock(m_ThreadMutex);
                return m_ThreadData.ReturnValue;
            }
        }
    }

    return false;
}

void COMWrapper::CallShowWindowSwitcher()
{
    InitIfNeeded();
    ::PostThreadMessage(m_ThreadID, WM_COMWRAPPER_SHOW_WINDOW_SWITCHER, 0, 0);
}

void COMWrapper::InitIfNeeded()
{
    if (m_ThreadHandle == nullptr)
    {
        if (m_ThreadReadyEvent == nullptr)
        {
            m_ThreadReadyEvent  = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
            m_ThreadReturnEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
        }

        m_ThreadData = COMWrapper::ThreadData(); //No need to lock since no other thread exists to race with
        m_ThreadHandle = ::CreateThread(nullptr, 0, COMWrapperThreadEntry, nullptr, 0, &m_ThreadID);

        if (m_ThreadReadyEvent != nullptr)
        {
            ::WaitForSingleObject(m_ThreadReadyEvent, INFINITE);
        }
    }
}

bool COMWrapper::ShellExecuteUnelevated(LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, LPCWSTR lpOperation, INT nShowCmd)
{
    //This function will fail if explorer.exe is not running, but it could be argued that this scenario is not exactly the sanest for a desktop viewing application
    //Elevated mode should be avoided in the first place to be fair

    //Use desktop automation to get the active shell view for the desktop
    Microsoft::WRL::ComPtr<IShellView> shell_view;
    Microsoft::WRL::ComPtr<IShellWindows> shell_windows;
    HRESULT hr = ::CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER, IID_IShellWindows, &shell_windows);

    if (SUCCEEDED(hr))
    {
        HWND hwnd = nullptr;
        Microsoft::WRL::ComPtr<IDispatch> dispatch;
        VARIANT v_empty = {};

        if (shell_windows->FindWindowSW(&v_empty, &v_empty, SWC_DESKTOP, (long*)&hwnd, SWFO_NEEDDISPATCH, &dispatch) == S_OK)
        {
            Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
            hr = dispatch.As(&service_provider);

            if (SUCCEEDED(hr))
            {
                Microsoft::WRL::ComPtr<IShellBrowser> shell_browser;
                hr = service_provider->QueryService(SID_STopLevelBrowser, __uuidof(IShellBrowser), (void**)&shell_browser);

                if (SUCCEEDED(hr))
                {
                    hr = shell_browser->QueryActiveShellView(&shell_view);
                }
            }
        }
        else
        {
            hr = E_FAIL;
        }
    }

    if (FAILED(hr))
        return false;

    //Use the shell view to get the shell dispatch interface
    Microsoft::WRL::ComPtr<IShellDispatch2> shell_dispatch2;
    Microsoft::WRL::ComPtr<IDispatch> dispatch_background;
    hr = shell_view->GetItemObject(SVGIO_BACKGROUND, __uuidof(IDispatch), (void**)&dispatch_background);
    if (SUCCEEDED(hr))
    {
        Microsoft::WRL::ComPtr<IShellFolderViewDual> shell_folderview_dual;
        hr = dispatch_background.As(&shell_folderview_dual);

        if (SUCCEEDED(hr))
        {
            Microsoft::WRL::ComPtr<IDispatch> dispatch;
            hr = shell_folderview_dual->get_Application(&dispatch);

            if (SUCCEEDED(hr))
            {
                hr = dispatch.As(&shell_dispatch2);
            }
        }
    }

    if (FAILED(hr))
        return false;

    //Use the shell dispatch interface to call ShellExecuteW() in the explorer process, which is running unelevated in most cases
    BSTR bstr_file = ::SysAllocString(lpFile);
    hr = (bstr_file != nullptr) ? S_OK : E_OUTOFMEMORY;

    if (SUCCEEDED(hr))
    {
        VARIANT v_args = {};
        VARIANT v_dir = {};
        VARIANT v_operation = {};
        VARIANT v_show = {};
        v_show.vt = VT_I4;
        v_show.intVal = nShowCmd;

        //Optional parameters (SysAllocString() returns nullptr on nullptr input)
        v_args.bstrVal      = ::SysAllocString(lpParameters);
        v_dir.bstrVal       = ::SysAllocString(lpDirectory);
        v_operation.bstrVal = ::SysAllocString(lpOperation);
        v_args.vt       = (v_args.bstrVal != nullptr)      ? VT_BSTR : VT_EMPTY;
        v_dir.vt        = (v_dir.bstrVal != nullptr)       ? VT_BSTR : VT_EMPTY;
        v_operation.vt  = (v_operation.bstrVal != nullptr) ? VT_BSTR : VT_EMPTY;

        hr = shell_dispatch2->ShellExecuteW(bstr_file, v_args, v_dir, v_operation, v_show);

        ::SysFreeString(bstr_file);
        ::SysFreeString(v_args.bstrVal);
        ::SysFreeString(v_dir.bstrVal);
        ::SysFreeString(v_operation.bstrVal);

        return true;
    }

    return false;
}

DWORD COMWrapper::COMWrapperThreadEntry(void* param)
{
    ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    //Initialize message queue before signaling ready
    MSG msg;
    ::PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE);

    COMWrapper& cwrapper = Get();
    ::SetEvent(cwrapper.m_ThreadReadyEvent);

    //Wait for command, or quit message
    while (::GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_COMWRAPPER_SHELL_EXECUTE)
        {
            std::lock_guard<std::mutex> lock(cwrapper.m_ThreadMutex);
            ::ShellExecuteW(nullptr, nullptr, cwrapper.m_ThreadData.ParamPath.c_str(), (!cwrapper.m_ThreadData.ParamPath.empty()) ? cwrapper.m_ThreadData.ParamArg.c_str() : nullptr, 
                            (!cwrapper.m_ThreadData.ParamDir.empty())  ? cwrapper.m_ThreadData.ParamDir.c_str() : nullptr, (INT)msg.wParam);
        }
        else if (msg.message == WM_COMWRAPPER_SHELL_EXECUTE_UNELEVATED)
        {
            {
                std::lock_guard<std::mutex> lock(cwrapper.m_ThreadMutex);
                cwrapper.m_ThreadData.ReturnValue = ShellExecuteUnelevated(cwrapper.m_ThreadData.ParamPath.c_str(), 
                                                                           (!cwrapper.m_ThreadData.ParamPath.empty()) ? cwrapper.m_ThreadData.ParamArg.c_str() : nullptr, 
                                                                           (!cwrapper.m_ThreadData.ParamDir.empty())  ? cwrapper.m_ThreadData.ParamDir.c_str() : nullptr, nullptr, (INT)msg.wParam);
            }

            ::SetEvent(cwrapper.m_ThreadReturnEvent);
        }
        else if (msg.message == WM_COMWRAPPER_SHOW_WINDOW_SWITCHER)
        {
            Microsoft::WRL::ComPtr<IShellDispatch5> shell_dispatch;
            HRESULT sc = ::CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_SERVER, IID_PPV_ARGS(&shell_dispatch));

            if (SUCCEEDED(sc))
            {
                shell_dispatch->WindowSwitcher();
            }
        }
    }

    ::ResetEvent(cwrapper.m_ThreadReadyEvent);
    ::CoUninitialize();
    return 0;
}
