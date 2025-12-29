#pragma once

#include <vector>

#include "imgui.h"
#include "openvr.h"

#include "Win32PerformanceData.h"

//Taken from ImPlot
struct ScrollingBufferFrameTime
{
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;

    ScrollingBufferFrameTime() 
    {
        MaxSize = 150;
        Offset  = 0;
        Data.reserve(MaxSize);
    }

    void AddFrame(float frame_number, float ms) 
    {
        if (Data.size() < MaxSize)
        {
            Data.push_back(ImVec2(frame_number, ms));
        }
        else 
        {
            Data[Offset] = ImVec2(frame_number, ms);
            Offset = (Offset + 1) % MaxSize;
        }
    }

    void Erase() 
    {
        if (Data.size() > 0) 
        {
            Data.shrink(0);
            Offset = 0;
        }
    }
};

struct TrackerInfo
{
    vr::TrackedDeviceIndex_t DeviceIndex = vr::k_unTrackedDeviceIndexInvalid;
    std::string Name;
    std::string NameCompact;
    float BatteryLevel;
};

class WindowPerformance
{
    private:
        ImVec2 m_Pos;
        ImVec2 m_Size;
        bool m_Visible;
        ULONGLONG m_VisibleTickLast; //Valid when m_Visible is false
        bool m_IsPopupOpen;

        float m_MinimalItemLineWrapMaxLength;
        float m_MinimalItemLineWrapPrevX;

        Win32PerformanceData m_PerfData;

        uint32_t m_PIDLast;

        //Frame-Rate stats, updated once a second
        int m_FPS;
        float m_FPS_Average;

        uint32_t m_FrameCountLast;
        uint32_t m_FrameCountTotal;
        uint32_t m_FrameCountTotalCount;
        ULONGLONG m_FPS_TickLast;

        //Offset values for cumulative counters
        uint32_t m_OffsetFrameIndex;
        uint32_t m_OffsetFramesPresents;
        uint32_t m_OffsetReprojectedFrames;
        uint32_t m_OffsetDroppedFrames;

        //Updated every frame
        float m_FrameTimeCPU;
        ScrollingBufferFrameTime m_FrameTimeCPUHistory;
        ScrollingBufferFrameTime m_FrameTimeCPUHistoryWarning;
        float m_FrameTimeGPU;
        ScrollingBufferFrameTime m_FrameTimeGPUHistory;
        ScrollingBufferFrameTime m_FrameTimeGPUHistoryWarning;

        float m_ReprojectionRatio;
        uint32_t m_DroppedFrames;

        float m_BatteryHMD;
        float m_BatteryLeft;
        float m_BatteryRight;
        std::vector<TrackerInfo> m_BatteryTrackers; //List updated in RefreshTrackerBatteryList(), called on devices connect/disconnect

        uint32_t m_FrameTimeLastIndex;
        float m_FrameTimeVsyncLimit;

        //Localized time string, updated once a minute
        SYSTEMTIME m_TimeLast;
        std::string m_TimeStr;

        //Vive Wireless
        int m_ViveWirelessTemp;
        ULONGLONG m_ViveWirelessTickLast;
        std::wstring m_ViveWirelessLogPath;
        bool m_ViveWirelessLogPathExists;
        std::wstring m_ViveWirelessLogFileLast;
        int m_ViveWirelessLogFileLastLine;

        //Overlay state
        bool m_IsOverlaySharedTextureUpdateNeeded;

        //Wrapped text functions to choose whether outline variant should be used or not
        static void PerfMonText(const char* fmt, ...) IM_FMTARGS(2);
        static void PerfMonTextUnformatted(const char* text, const char* text_end = nullptr);
        static void PerfMonTextRight(float offset_x, float fixed_w, const char* fmt, ...) IM_FMTARGS(4);
        static void PerfMonTextRightUnformatted(float offset_x, float fixed_w, const char* text, const char* text_end = nullptr);

        //Called in Minimal style to do line wrapper on a per-item basis
        void StatsMinimalItemLineWrap();

        void DisplayStatsLarge();
        void DisplayStatsCompact();
        void DisplayStatsMinimal();

        void UpdateStatValues();
        void UpdateStatValuesSteamVR();
        void UpdateStatValuesViveWireless();

        void DrawFrameTimeGraphCPU(const ImVec2& graph_size, double plot_xmin, double plot_xmax, double plot_ymax);
        void DrawFrameTimeGraphGPU(const ImVec2& graph_size, double plot_xmin, double plot_xmax, double plot_ymax);

        void CheckScheduledOverlaySharedTextureUpdate();
        void OnWindowBoundsChanged();

    public:
        WindowPerformance();

        void Update(bool show_as_popup = false);
        void UpdateVisibleState();
        void RefreshTrackerBatteryList();
        void ResetCumulativeValues();
        void ScheduleOverlaySharedTextureUpdate();

        Win32PerformanceData& GetPerformanceData();
        bool IsViveWirelessInstalled();

        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;
        bool IsVisible() const;
        void SetPopupOpen(bool is_open);

        static bool IsAnyOverlayUsingPerformanceMonitor();
};