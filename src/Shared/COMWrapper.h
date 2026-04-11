#pragma once

#define NOMINMAX
#include <windows.h>

#include <string>
#include <mutex>

//Wrapper for certain COM functions and functions using COM to run them in a separate thread using a multi-threaded apartment
//Reason for this is that the simple single-threaded apartment use is pumping messages and as such may eat our custom window messages at the wrong time
//This could likely also be avoided by simply using an MTA on the main thread, but we keep this separate in case we do end up using COM in a way that requires a STA somewhere in the future
class COMWrapper
{
    struct ThreadData
    {
        std::wstring ParamPath;
        std::wstring ParamArg;
        std::wstring ParamDir;
        bool ReturnValue = false;
    };

    public:
        static COMWrapper& Get();

        //- Only called by main thread
        void SetActive(bool is_active); //Thread is destroyed when COMWrapper is inactive, but this doesn't need to be called to activate as the thread is initialized done on demand
        bool IsActive() const;

        void CallShellExecute(const std::wstring& path, const std::wstring& arg, const std::wstring& dir, INT showcmd = SW_SHOWNORMAL);
        bool CallShellExecuteUnelevated(const std::wstring& path, const std::wstring& arg, const std::wstring& dir, INT showcmd = SW_SHOWNORMAL);
        void CallShowWindowSwitcher();

    private:
        //- Only accessed in main thread
        HANDLE m_ThreadHandle = nullptr;
        DWORD m_ThreadID = 0;

        //- Protected by m_ThreadMutex
        std::mutex m_ThreadMutex;
        ThreadData m_ThreadData;

        //- Synchronization variables
        HANDLE m_ThreadReadyEvent = nullptr;
        HANDLE m_ThreadReturnEvent = nullptr;

        //- Only called by main thread
        void InitIfNeeded();

        //- Only called by COMWrapper thread
        static bool ShellExecuteUnelevated(LPCWSTR lpFile, LPCWSTR lpParameters = nullptr, LPCWSTR lpDirectory = nullptr, LPCWSTR lpOperation = nullptr, INT nShowCmd = SW_SHOWNORMAL);

        static DWORD COMWrapperThreadEntry(void* param);
};
