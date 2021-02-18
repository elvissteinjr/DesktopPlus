#include "Win32PerformanceData.h"

#include <memory>

#include <PdhMsg.h>

LUID Win32PerformanceData::GetLUIDFromFormattedCounterNameString(const std::wstring& str)
{
    LUID parsed_luid = {0, 0};

    //Find and extract LUID parts
    size_t pos = str.find(L"_0x");
    if (pos != std::string::npos)
    {
        std::wstring str_high_part = str.substr(pos + 1, 10);

        pos = str.find(L"_0x", pos + 1);
        if (pos != std::string::npos)
        {
            std::wstring str_low_part = str.substr(pos + 1, 10);

            //Convert from string
            parsed_luid.HighPart = stol(str_high_part, 0, 16);
            parsed_luid.LowPart  = stoul(str_low_part, 0, 16);
        }
    }

    return parsed_luid;
}

Win32PerformanceData::Win32PerformanceData() : 
    m_QueryCPU(nullptr), m_QueryGPU(nullptr), m_QueryVRAM(nullptr), m_CounterCPU(nullptr), m_CounterGPU(nullptr), m_CounterVRAM(nullptr), 
    m_GPUTargetLUID{0, 0}, m_CPULoad(0.0f), m_GPULoad(0.0f), m_VRAMTotalGB(0.0f), m_VRAMUsedGB(0.0f), m_LastUpdateTick(0)
{
    //RAM Total
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    ::GlobalMemoryStatusEx(&mem_info);
    m_RAMTotalGB = float(mem_info.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));

    Update(); //Fills m_RamUsedGB
}

Win32PerformanceData::~Win32PerformanceData()
{
    DisableCounters();
}

void Win32PerformanceData::EnableCounters(bool enable_gpu)
{
    PDH_STATUS pdh_status;
    bool new_counter_added = false;

    //CPU Load
    if (m_QueryCPU == nullptr)
    {
        pdh_status = ::PdhOpenQuery(nullptr, 0, &m_QueryCPU);

        if (pdh_status == ERROR_SUCCESS)
        {
            pdh_status = ::PdhAddEnglishCounter(m_QueryCPU, L"\\Processor(_Total)\\% Processor Time", 0, &m_CounterCPU);

            if (pdh_status != ERROR_SUCCESS)
            {
                ::PdhCloseQuery(&m_QueryCPU);
                m_QueryCPU = nullptr;
            }
            else
            {
                new_counter_added = true;
            }
        }
    }

    if (enable_gpu)
    {
        //GPU Load
        if (m_QueryGPU == nullptr)
        {
            pdh_status = ::PdhOpenQuery(nullptr, 0, &m_QueryGPU);

            if (pdh_status == ERROR_SUCCESS)
            {
                pdh_status = ::PdhAddEnglishCounter(m_QueryGPU, L"\\GPU Engine(*)\\Utilization Percentage", 0, &m_CounterGPU);

                if (pdh_status != ERROR_SUCCESS)
                {
                    ::PdhCloseQuery(&m_QueryGPU);
                    m_QueryGPU = nullptr;
                }
                else
                {
                    new_counter_added = true;
                }
            }
        }

        //GPU VRAM
        if (m_QueryVRAM == nullptr)
        {
            pdh_status = ::PdhOpenQuery(nullptr, 0, &m_QueryVRAM);

            if (pdh_status == ERROR_SUCCESS)
            {
                pdh_status = ::PdhAddEnglishCounter(m_QueryVRAM, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &m_CounterVRAM);

                if (pdh_status != ERROR_SUCCESS)
                {
                    ::PdhCloseQuery(&m_QueryVRAM);
                    m_QueryVRAM = nullptr;
                }
                else
                {
                    new_counter_added = true;
                }
            }
        }
    }

    if (new_counter_added)
    {
        m_LastUpdateTick = ::GetTickCount64();  //Not an update, but we need to start waiting from when a counter was first added
    }
}

void Win32PerformanceData::DisableGPUCounters()
{
    if (m_QueryGPU != nullptr)
    {
        PdhCloseQuery(&m_QueryGPU);
        m_QueryGPU = nullptr;
    }

    if (m_QueryVRAM != nullptr)
    {
        PdhCloseQuery(&m_QueryVRAM);
        m_QueryVRAM = nullptr;
    }
}

void Win32PerformanceData::DisableCounters()
{
    if (m_QueryCPU != nullptr)
    {
        PdhCloseQuery(&m_QueryCPU);
        m_QueryCPU = nullptr;
    }

    if (m_QueryGPU != nullptr)
    {
        PdhCloseQuery(&m_QueryGPU);
        m_QueryGPU = nullptr;
    }

    if (m_QueryVRAM != nullptr)
    {
        PdhCloseQuery(&m_QueryVRAM);
        m_QueryVRAM = nullptr;
    }
}

bool Win32PerformanceData::Update()
{
    //Do not update more often than once a second to get reliable CPU load readings (and reduce said load)
    if (::GetTickCount64() < m_LastUpdateTick + 1000)
        return false;

    m_LastUpdateTick = ::GetTickCount64();

    PDH_STATUS pdh_status;

    //CPU Load
    if (m_QueryCPU != nullptr)
    {
        pdh_status = PdhCollectQueryData(m_QueryCPU);

        if (pdh_status == ERROR_SUCCESS)
        {
            PDH_FMT_COUNTERVALUE counter_value = {0};
            pdh_status = PdhGetFormattedCounterValue(m_CounterCPU, PDH_FMT_DOUBLE, nullptr, &counter_value);

            if (pdh_status == ERROR_SUCCESS)
            {
                m_CPULoad = (float)counter_value.doubleValue;
            }
        }
    }

    //GPU Load
    if (m_QueryGPU != nullptr)
    {
        pdh_status = PdhCollectQueryData(m_QueryGPU);

        if (pdh_status == ERROR_SUCCESS)
        {
            PDH_FMT_COUNTERVALUE counter_value = {0};
            DWORD buffer_size = 0;
            DWORD item_count = 0;

            pdh_status = PdhGetFormattedCounterArray(m_CounterGPU, PDH_FMT_DOUBLE, &buffer_size, &item_count, nullptr);

            if (pdh_status == PDH_MORE_DATA)
            {
                std::unique_ptr<uint8_t[]> item_buffer = std::unique_ptr<uint8_t[]>{new uint8_t[buffer_size]};
                PDH_FMT_COUNTERVALUE_ITEM* items = (PDH_FMT_COUNTERVALUE_ITEM*)item_buffer.get();

                pdh_status = PdhGetFormattedCounterArray(m_CounterGPU, PDH_FMT_DOUBLE, &buffer_size, &item_count, items);

                if (pdh_status == ERROR_SUCCESS)
                {
                    double total_load = 0.0;

                    for (DWORD i = 0; i < item_count; i++)
                    {
                        std::wstring item_name(items[i].szName);

                        //Only count engine type "3D"
                        if (item_name.find(L"_engtype_3D") != std::string::npos)
                        {
                            //Make sure it's from the right GPU
                            LUID item_luid = GetLUIDFromFormattedCounterNameString(item_name);

                            if ( (item_luid.LowPart == m_GPUTargetLUID.LowPart) && (item_luid.HighPart == m_GPUTargetLUID.HighPart) )
                            {
                                total_load += items[i].FmtValue.doubleValue;
                            }
                        }
                    }

                    m_GPULoad = (float)total_load;
                }
            }
        }
    }

    //GPU VRAM Used
    if (m_QueryVRAM != nullptr)
    {
        pdh_status = PdhCollectQueryData(m_QueryVRAM);

        if (pdh_status == ERROR_SUCCESS)
        {
            PDH_FMT_COUNTERVALUE counter_value = {0};
            DWORD buffer_size = 0;
            DWORD item_count = 0;

            pdh_status = PdhGetFormattedCounterArray(m_CounterVRAM, PDH_FMT_DOUBLE, &buffer_size, &item_count, nullptr);

            if (pdh_status == PDH_MORE_DATA)
            {
                std::unique_ptr<uint8_t[]> item_buffer = std::unique_ptr<uint8_t[]>{new uint8_t[buffer_size]};
                PDH_FMT_COUNTERVALUE_ITEM* items = (PDH_FMT_COUNTERVALUE_ITEM*)item_buffer.get();

                pdh_status = PdhGetFormattedCounterArray(m_CounterVRAM, PDH_FMT_DOUBLE, &buffer_size, &item_count, items);

                if (pdh_status == ERROR_SUCCESS)
                {
                    for (DWORD i = 0; i < item_count; i++)
                    {
                        std::wstring item_name(items[i].szName);

                        //Make sure it's from the right GPU
                        LUID item_luid = GetLUIDFromFormattedCounterNameString(item_name);

                        if ( (item_luid.LowPart == m_GPUTargetLUID.LowPart) && (item_luid.HighPart == m_GPUTargetLUID.HighPart) )
                        {
                            m_VRAMUsedGB = float(items[i].FmtValue.doubleValue / (1024.0 * 1024.0 * 1024.0));
                            break;
                        }
                    }
                }
            }
        }
    }

    //RAM Used
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    ::GlobalMemoryStatusEx(&mem_info);
    m_RAMUsedGB = float((mem_info.ullTotalPhys - mem_info.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0));

    return true;
}

void Win32PerformanceData::SetTargetGPU(LUID gpu_luid, DWORDLONG vram_total_bytes)
{
    m_GPUTargetLUID = gpu_luid;
    m_VRAMTotalGB = float(vram_total_bytes / (1024.0 * 1024.0 * 1024.0));
}

float Win32PerformanceData::GetCPULoadPrecentage() const
{
    return m_CPULoad;
}

float Win32PerformanceData::GetGPULoadPrecentage() const
{
    return m_GPULoad;
}

float Win32PerformanceData::GetRAMTotalGB() const
{
    return m_RAMTotalGB;
}

float Win32PerformanceData::GetRAMUsedGB() const
{
    return m_RAMUsedGB;
}

float Win32PerformanceData::GetVRAMTotalGB() const
{
    return m_VRAMTotalGB;
}

float Win32PerformanceData::GetVRAMUsedGB() const
{
    return m_VRAMUsedGB;
}
