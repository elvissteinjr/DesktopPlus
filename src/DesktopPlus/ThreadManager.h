#ifndef _THREADMANAGER_H_
#define _THREADMANAGER_H_

#include "CommonTypes.h"

class DDPThreadManager
{
    public:
        void Clean();
        DDPDuplReturn Initialize(INT SingleOutput, UINT OutputCount, HANDLE UnexpectedErrorEvent, HANDLE ExpectedErrorEvent, HANDLE NewFrameProcessedEvent,
                                 HANDLE PauseDuplicationEvent, HANDLE ResumeDuplicationEvent, HANDLE TerminateThreadsEvent,
                                 HANDLE SharedHandle, const RECT& DesktopDim, Microsoft::WRL::ComPtr<IDXGIAdapter> DXGIAdapter, bool WMRIgnoreVScreens);
        DDPPtrInfo& GetPointerInfo();       //Should only be called when shared surface mutex has be aquired
        DPRect& GetDirtyRegionTotal();      //Should only be called when shared surface mutex has be aquired
        void WaitForThreadTermination();

    private:
        DDPDuplReturn InitializeDx(DDPDxResources& Data, IDXGIAdapter* DXGIAdapter); //Doesn't Release() the DXGIAdapter

        DDPPtrInfo m_PtrInfo;
        DPRect m_DirtyRegionTotal;
        std::vector<HANDLE> m_ThreadHandles;
        std::vector<DDPThreadData> m_ThreadData;
};

#endif
