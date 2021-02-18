#pragma once

#include <string>

#define NOMINMAX
#include <pdh.h>

class Win32PerformanceData
{
    private:
        PDH_HQUERY m_QueryCPU;
        PDH_HQUERY m_QueryGPU;
        PDH_HQUERY m_QueryVRAM;
        PDH_HCOUNTER m_CounterCPU;
        PDH_HCOUNTER m_CounterGPU;
        PDH_HCOUNTER m_CounterVRAM;

        LUID m_GPUTargetLUID;

        float m_CPULoad;
        float m_GPULoad;
        float m_RAMTotalGB;
        float m_RAMUsedGB;
        float m_VRAMTotalGB;
        float m_VRAMUsedGB;

        ULONGLONG m_LastUpdateTick;

        static LUID GetLUIDFromFormattedCounterNameString(const std::wstring& str);

    public:
        Win32PerformanceData();
        ~Win32PerformanceData();

        void EnableCounters(bool enable_gpu);
        void DisableGPUCounters();
        void DisableCounters();

        bool Update();
        void SetTargetGPU(LUID gpu_luid, DWORDLONG vram_total_bytes);

        float GetCPULoadPrecentage() const;
        float GetGPULoadPrecentage() const;
        float GetRAMTotalGB()        const;
        float GetRAMUsedGB()         const;
        float GetVRAMTotalGB()       const;
        float GetVRAMUsedGB()        const;
};

class PerformanceDataOpenVR
{

};

class PerformanceDataViveWireless
{

};