#ifndef _THREADMANAGER_H_
#define _THREADMANAGER_H_

#include "CommonTypes.h"

class THREADMANAGER
{
    public:
        THREADMANAGER();
        ~THREADMANAGER();
        void Clean();
        DUPL_RETURN Initialize(INT SingleOutput, UINT OutputCount, HANDLE UnexpectedErrorEvent, HANDLE ExpectedErrorEvent, HANDLE NewFrameProcessedEvent,
                               HANDLE PauseDuplicationEvent, HANDLE ResumeDuplicationEvent, HANDLE TerminateThreadsEvent,
                               HANDLE SharedHandle, _In_ RECT* DesktopDim, IDXGIAdapter* DXGIAdapter, bool WMRIgnoreVScreens);
        PTR_INFO* GetPointerInfo();         //Should only be called when shared surface mutex has be aquired
        DPRect& GetDirtyRegionTotal();      //Should only be called when shared surface mutex has be aquired
        void WaitForThreadTermination();

    private:
        DUPL_RETURN InitializeDx(_Out_ DX_RESOURCES* Data, IDXGIAdapter* DXGIAdapter); //Doesn't Release() the DXGIAdapter
        void CleanDx(_Inout_ DX_RESOURCES* Data);

        PTR_INFO m_PtrInfo;
        DPRect m_DirtyRegionTotal;
        UINT m_ThreadCount;
        _Field_size_(m_ThreadCount) HANDLE* m_ThreadHandles;
        _Field_size_(m_ThreadCount) THREAD_DATA* m_ThreadData;
};

#endif
