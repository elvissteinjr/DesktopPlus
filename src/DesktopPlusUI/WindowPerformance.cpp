#include "WindowPerformance.h"

#include <fstream>
#include <codecvt>

#include "implot.h"
#include "ImGuiExt.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"
#include "Util.h"
#include "OpenVRExt.h"
#include "UIManager.h"
#include "OverlayManager.h"

static LPCWSTR const g_ViveWirelessLogPathBase = L"%ProgramData%\\VIVE Wireless\\ConnectionUtility\\Log\\";

WindowPerformance::WindowPerformance() : 
    m_Visible(false),
    m_VisibleTickLast(0),
    m_IsPopupOpen(false),
    m_PIDLast(0),
    m_OffsetFrameIndex(0),
    m_OffsetFramesPresents(0),
    m_OffsetReprojectedFrames(0),
    m_OffsetDroppedFrames(0),
    m_FrameTimeCPU(0.0f),
    m_FrameTimeGPU(0.0f),
    m_ReprojectionRatio(0.0f),
    m_DroppedFrames(0),
    m_BatteryHMD(-1.0f),
    m_BatteryLeft(-1.0f),
    m_BatteryRight(-1.0f),
    m_FrameTimeLastIndex(0),
    m_ViveWirelessTemp(-1),
    m_ViveWirelessLogFileLastLine(0),
    m_IsOverlaySharedTextureUpdateNeeded(false)
{
    ResetCumulativeValues();

    //Set m_TimeLast to something invalid so the first time always counts as different
    m_TimeLast = {0};
    m_TimeLast.wHour   = 99;
    m_TimeLast.wMinute = 99;

    //Expand path for Vive Wireless log files
    wchar_t wpath[MAX_PATH] = L"\0";
    ::ExpandEnvironmentStrings(g_ViveWirelessLogPathBase, wpath, MAX_PATH);
    m_ViveWirelessLogPath = wpath;
    m_ViveWirelessLogPathExists = DirectoryExists(wpath);
}

void WindowPerformance::Update(bool show_as_popup)
{
    ImGuiIO& io = ImGui::GetIO();

    const DPRect& tex_rect = UITextureSpaces::Get().GetRect(ui_texspace_performance_monitor);
    ImGui::SetNextWindowSizeConstraints({0.0f, 0.0f}, {(float)tex_rect.GetWidth() - 1, (float)tex_rect.GetHeight() - 1});

    if (show_as_popup)
    {
        //Don't update values when repeating the frame
        if (!UIManager::Get()->GetRepeatFrame())
        {
            UpdateStatValues();
        }

        //Set popup rounding to the same as a normal window
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ImGui::GetStyle().WindowRounding);
        bool is_popup_rounding_pushed = true;

        const bool ui_large_style = ((ConfigManager::GetValue(configid_bool_interface_large_style)) && (!UIManager::Get()->IsInDesktopMode()));

        if (ui_large_style)
        {
            ImGui::PushFont(UIManager::Get()->GetFontCompact());
        }

        //Setting the window size here is a workaround for an invalid clipping assert raised when switching columns in the compact layout
        //Came with ImGui 1.80, but has quite a few variables involved, so I'm not even sure who's at fault here. This works, though.
        ImGui::SetNextWindowSize({1.0f, -1.0f}, ImGuiCond_Appearing);
        ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopup("PopupPerformanceMonitor", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                                                         ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::PopStyleVar(); //ImGuiStyleVar_PopupRounding
            is_popup_rounding_pushed = false;

            if (ImGui::IsWindowAppearing())
            {
                UIManager::Get()->RepeatFrame();
            }

            if (ConfigManager::GetValue(configid_bool_performance_monitor_large_style))
            {
                DisplayStatsLarge();
            }
            else
            {
                DisplayStatsCompact();
            }

            ImGui::EndPopup();
        }

        if (ui_large_style)
        {
            ImGui::PopFont();
        }

        if (is_popup_rounding_pushed)
        {
            ImGui::PopStyleVar(); //ImGuiStyleVar_PopupRounding
        }
    }
    else
    {
        if (!m_Visible)
            return;

        //Don't update values when repeating the frame
        if (!UIManager::Get()->GetRepeatFrame())
        {
            UpdateStatValues();
            CheckScheduledOverlaySharedTextureUpdate();
        }

        //Center in overlay space
        const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_performance_monitor);
        ImGui::SetNextWindowPos(ImVec2((float)rect.GetCenter().x, (float)rect.GetCenter().y), 0, ImVec2(0.5f, 0.5f));

        ImGui::Begin("WindowPerformance", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
                                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);

        if (ConfigManager::GetValue(configid_bool_performance_monitor_large_style))
        {
            DisplayStatsLarge();
        }
        else
        {
            DisplayStatsCompact();
        }

        //Store window bounds and update affected overlays if they changed
        ImVec2 pos_new  = ImGui::GetWindowPos();
        ImVec2 size_new = ImGui::GetWindowSize();

        if ((pos_new.x != m_Pos.x) || (pos_new.y != m_Pos.y) || (size_new.x != m_Size.x) || (size_new.y != m_Size.y))
        {
            m_Pos  = pos_new;
            m_Size = size_new;

            OnWindowBoundsChanged();
        }

        ImGui::End();
    }
}

void WindowPerformance::UpdateVisibleState()
{
    bool was_visible = m_Visible;

    m_Visible = ( (m_IsPopupOpen) || (IsAnyOverlayUsingPerformanceMonitor()) );

    if (m_Visible)
    {
        if (!was_visible)
        {
            m_PerfData.EnableCounters(!ConfigManager::GetValue(configid_bool_performance_monitor_disable_gpu_counters));
        }
    }
    else
    {
        if (was_visible)
        {
            m_VisibleTickLast = ::GetTickCount64();
        }
        else if (m_VisibleTickLast + 3000 <= ::GetTickCount64())    //Only actually disable counters after at least 3 seconds of being hidden
        {
            m_PerfData.DisableCounters();
        }
    }
}

void WindowPerformance::DisplayStatsLarge()
{
    const float column_width_0 = ImGui::GetFontSize() * 7.5f;
    const float column_width_1 = ImGui::GetFontSize() * 4.0f;
    const float column_width_3 = ImGui::GetFontSize() * 6.0f;
    const ImVec2 graph_size    = {column_width_0 + column_width_3 - ImGui::GetStyle().WindowPadding.x, ImGui::GetFontSize() * 2.0f};

    static float row_gpu_y = 0.0f;

    //Both graphs are being created outside of the normal widget flow, due to how they span multiple rows and columns, which isn't exactly supported by ImGui's columns

    //Shared offsets
    ImVec2 cursor_pos_prev = ImGui::GetCursorPos();
    float graph_pos_x  = cursor_pos_prev.x + column_width_0 + column_width_1 - ImGui::GetStyle().FramePadding.x;

    double plot_xmin = (m_FrameTimeLastIndex > (uint32_t)m_FrameTimeCPUHistory.MaxSize) ? m_FrameTimeLastIndex - m_FrameTimeCPUHistory.MaxSize + 0.5f : 0.5f;
    double plot_xmax = m_FrameTimeLastIndex - 0.5f;
    double plot_ymax = ceilf(m_FrameTimeVsyncLimit * 1.4f); 

    //Set fixed window width from graph cursor pos even if all graphs are disabled since columns don't increase the window size
    ImGui::SetCursorPos({graph_pos_x, cursor_pos_prev.y + ImGui::GetStyle().FramePadding.y});

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, 0.0f});
    ImGui::Dummy({graph_size.x, 0.0f});
    ImGui::PopStyleVar();
    ImGui::SetCursorPos({graph_pos_x, cursor_pos_prev.y + ImGui::GetStyle().FramePadding.y});

    if (ConfigManager::GetValue(configid_bool_performance_monitor_show_graphs))
    {
        //-CPU Frame Time Graph
        if (ConfigManager::GetValue(configid_bool_performance_monitor_show_cpu))
        {
            DrawFrameTimeGraphCPU(graph_size, plot_xmin, plot_xmax, plot_ymax);
        }

        //-GPU Frame Time Graph
        if (ConfigManager::GetValue(configid_bool_performance_monitor_show_gpu))
        {
            ImGui::SetCursorPos({graph_pos_x, row_gpu_y + ImGui::GetStyle().FramePadding.y});

            DrawFrameTimeGraphGPU(graph_size, plot_xmin, plot_xmax, plot_ymax);
        }
    }

    ImGui::SetCursorPos(cursor_pos_prev);

    //Store text width of units for right alignment
    //This is required since the text width can be off by one pixel on certain values if the units are rendered as part of the formatted string... odd but whatever
    static const float text_ms_width      = ImGui::CalcTextSize(" ms").x;
    static const float text_percent_width = ImGui::CalcTextSize("%").x;
    static const float text_temp_width    = ImGui::CalcTextSize("\xC2\xB0""C").x;

    float item_spacing_half = ImGui::GetStyle().ItemSpacing.x / 2.0f;
    float frame_time_warning_limit = m_FrameTimeVsyncLimit * 0.95f;


    ImGui::Columns(4, "ColumnStatsLarge", false);
    ImGui::SetColumnWidth(0, column_width_0);
    ImGui::SetColumnWidth(1, column_width_1);
    ImGui::SetColumnWidth(2, column_width_0);
    ImGui::SetColumnWidth(3, column_width_3);

    float right_border_offset = -ImGui::GetStyle().FramePadding.x;

    //--Table CPU
    if (ConfigManager::GetValue(configid_bool_performance_monitor_show_cpu))
    {
        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_PerformanceMonitorCPU));
        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::NextColumn();
        ImGui::NextColumn();

        //-CPU Frame Time
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorFrameTime));
        ImGui::NextColumn();

        //Warning color when frame time above 95% vsync time
        if (m_FrameTimeCPU > frame_time_warning_limit)
            ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

        ImGui::TextRight(text_ms_width, "%.2f", m_FrameTimeCPU);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(" ms");

        if (m_FrameTimeCPU > frame_time_warning_limit)
            ImGui::PopStyleColor();

        ImGui::NextColumn();

        ImGui::NextColumn();
        ImGui::NextColumn();

        //-CPU Load
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorLoad));
        ImGui::NextColumn();

        ImGui::TextRight(text_percent_width, "%.2f", m_PerfData.GetCPULoadPrecentage());
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted("%");

        ImGui::NextColumn();

        //-RAM
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - item_spacing_half);  //Reduce horizontal spacing
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorRAM));
        ImGui::NextColumn();

        //Right align
        ImGui::TextRight(right_border_offset - 1.0f, "%.2f/%.2f GB", m_PerfData.GetRAMUsedGB(), m_PerfData.GetRAMTotalGB());
        ImGui::NextColumn();
    }

    //--Table GPU
    if (ConfigManager::GetValue(configid_bool_performance_monitor_show_gpu))
    {
        row_gpu_y = ImGui::GetCursorPosY();
        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_PerformanceMonitorGPU));
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::NextColumn();

        //-GPU Frame Time
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorFrameTime));
        ImGui::NextColumn();

        if (m_FrameTimeGPU > frame_time_warning_limit)
            ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

        ImGui::TextRight(text_ms_width, "%.2f", m_FrameTimeGPU);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(" ms");
        ImGui::NextColumn();

        if (m_FrameTimeGPU > frame_time_warning_limit)
            ImGui::PopStyleColor();

        ImGui::NextColumn();
        ImGui::NextColumn();

        if (!ConfigManager::GetValue(configid_bool_performance_monitor_disable_gpu_counters)) //No point in showing it all if it's not updating
        {
            //-GPU Load
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorLoad));
            ImGui::NextColumn();

            ImGui::TextRight(text_percent_width, "%.2f", m_PerfData.GetGPULoadPrecentage());
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted("%");
            ImGui::NextColumn();

            //-VRAM
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - item_spacing_half);  //Reduce horizontal spacing
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorVRAM));
            ImGui::NextColumn();
            ImGui::TextRight(right_border_offset - 1.0f, "%.2f/%.2f GB", m_PerfData.GetVRAMUsedGB(), m_PerfData.GetVRAMTotalGB());
            ImGui::NextColumn();
        }
    }

    //-Table SteamVR
    if ( (ConfigManager::GetValue(configid_bool_performance_monitor_show_fps)) || (ConfigManager::GetValue(configid_bool_performance_monitor_show_battery)) )
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "SteamVR");
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::NextColumn();

        //-Time
        if (ConfigManager::GetValue(configid_bool_performance_monitor_show_time))
        {
            ImGui::TextRightUnformatted(right_border_offset, m_TimeStr.c_str());
        }

        ImGui::NextColumn();

        if (ConfigManager::GetValue(configid_bool_performance_monitor_show_fps))
        {
            //-FPS
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorFPS));
            ImGui::NextColumn();
            ImGui::TextRight(0.0f, "%d", m_FPS);
            ImGui::NextColumn();

            //-Average FPS
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - item_spacing_half);  //Reduce horizontal spacing
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorFPSAverage));
            ImGui::NextColumn();
            ImGui::TextRight(right_border_offset, "%.2f", m_FPS_Average);
            ImGui::NextColumn();

            //No VR app means no frame statistics (FPS still gets counted, though)
            if (m_PIDLast == 0)
                ImGui::PushItemDisabled();

            //-Reprojection Ratio
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorReprojectionRatio));
            ImGui::NextColumn();

            ImGui::TextRight(text_percent_width, "%.2f", m_ReprojectionRatio);
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted("%");
            ImGui::NextColumn();

            //-Dropped Frames
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - item_spacing_half);  //Reduce horizontal spacing
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorDroppedFrames));
            ImGui::NextColumn();
            ImGui::TextRight(right_border_offset, "%u", m_DroppedFrames);
            ImGui::NextColumn();

            if (m_PIDLast == 0)
                ImGui::PopItemDisabled();
        }

        if (ConfigManager::GetValue(configid_bool_performance_monitor_show_battery))
        {
            //-Battery Left
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorBatteryLeft));
            ImGui::NextColumn();

            if (m_BatteryLeft != -1.0f)
            {
                //15% warning color (Same percentage the SteamVR dashboard battery icon goes red at)
                if (m_BatteryLeft < 15.0f)
                    ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

                ImGui::TextRight(text_percent_width, "%.0f", m_BatteryLeft);
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextUnformatted("%");
                ImGui::NextColumn();

                if (m_BatteryLeft < 15.0f)
                    ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushItemDisabled();
                ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString(tstr_PerformanceMonitorBatteryDisconnected));
                ImGui::NextColumn();
                ImGui::PopItemDisabled();
            }

            //-Battery Right
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - item_spacing_half);  //Reduce horizontal spacing
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorBatteryRight));
            ImGui::NextColumn();

            if (m_BatteryRight != -1.0f)
            {
                //15% warning color
                if (m_BatteryRight < 15.0f)
                    ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

                ImGui::TextRight(text_percent_width + right_border_offset, "%.0f", m_BatteryRight);
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextUnformatted("%");
                ImGui::NextColumn();

                if (m_BatteryRight < 15.0f)
                    ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushItemDisabled();
                ImGui::TextRightUnformatted(right_border_offset, TranslationManager::GetString(tstr_PerformanceMonitorBatteryDisconnected));
                ImGui::NextColumn();
                ImGui::PopItemDisabled();
            }

            //-Battery HMD (only shown if available)
            if (m_BatteryHMD != -1.0f)
            {
                ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorBatteryHMD));
                ImGui::NextColumn();
                //15% warning color
                if (m_BatteryHMD < 15.0f)
                    ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

                ImGui::TextRight(text_percent_width, "%.0f", m_BatteryHMD);
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextUnformatted("%");
                ImGui::NextColumn();

                if (m_BatteryHMD < 15.0f)
                    ImGui::PopStyleColor();
            }

            float right_offset;

            //-Battery Trackers
            if (ConfigManager::GetValue(configid_bool_performance_monitor_show_trackers))
            {
                unsigned int tracker_number = 1;
                for (const auto& tracker_info : m_BatteryTrackers)
                {
                    //Reduce horizontal spacing on the right column
                    if (ImGui::GetColumnIndex() == 2)
                    {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - item_spacing_half);
                        right_offset = text_percent_width + right_border_offset;
                    }
                    else
                    {
                        right_offset = text_percent_width;
                    }

                    ImGui::TextUnformatted(tracker_info.Name.c_str());
                    ImGui::NextColumn();

                    //15% warning color
                    if (tracker_info.BatteryLevel < 15.0f)
                        ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

                    ImGui::TextRight(right_offset, "%.0f", tracker_info.BatteryLevel);
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::TextUnformatted("%");
                    ImGui::NextColumn();

                    if (tracker_info.BatteryLevel < 15.0f)
                        ImGui::PopStyleColor();

                    tracker_number++;
                }
            }

            //-Vive Wireless
            if ( (m_ViveWirelessLogPathExists) && (ConfigManager::GetValue(configid_bool_performance_monitor_show_vive_wireless)) )
            {
                //Reduce horizontal spacing on the right column
                if (ImGui::GetColumnIndex() == 2)
                {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - item_spacing_half);  
                    right_offset = right_border_offset;
                }
                else
                {
                    right_offset = 0.0f;
                }

                ImGui::Text("Vive Wireless:");
                ImGui::NextColumn();

                if (m_ViveWirelessTemp != -1)
                {
                    //90 degrees celsius warning color (arbitrarily chosen, but that's not a temp it should be at constantly)
                    if (m_ViveWirelessTemp > 90)
                        ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

                    ImGui::TextRight(right_offset, "%d\xC2\xB0""C", m_ViveWirelessTemp);

                    if (m_ViveWirelessTemp > 90)
                        ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::PushItemDisabled();
                    ImGui::TextRightUnformatted(right_offset, TranslationManager::GetString(tstr_PerformanceMonitorCompactViveWirelessTempNotAvailable));
                    ImGui::PopItemDisabled();
                }
            }
        }
    }

    //Last item rect height is the padding dummy == empty window
    if (ImGui::GetItemRectSize().y == 0.0f)
    {
        ImGui::Columns(1);
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorEmpty));
    }
}

void WindowPerformance::DisplayStatsCompact()
{
    const float item_spacing_prev = ImGui::GetStyle().ItemSpacing.x;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, ImGui::GetStyle().ItemSpacing.y});

    //These width caluclations are not using the translated strings, but there really is no space for those to be longer either, so whatever
    static const float column_width_0     = ImGui::GetFontSize() * 1.5f;
    float column_width_cpu_1              = ImGui::CalcTextSize(" 000.00 ms ").x;
    float column_width_cpu_2              = ImGui::CalcTextSize(" 000.00% ").x;
    float column_width_cpu_3              = ImGui::CalcTextSize(" 000.00/000.00 GB ").x;
    static const float column_width_fps_1 = ImGui::CalcTextSize(" 000 ").x;
    static const float column_width_fps_2 = ImGui::CalcTextSize(" 000.00 AVG ").x;
    static const float column_width_fps_3 = ImGui::CalcTextSize(" 00.00% RPR ").x;
    static const float column_width_fps_4 = ImGui::CalcTextSize(" 0000 DRP ").x;
    float column_width_bat                = ImGui::CalcTextSize(" T1 100% ").x;

    static const float text_percentage_cwidth = ImGui::CalcTextSize(" 100%").x;
    static const float text_avg_cwidth        = ImGui::CalcTextSize(" AVG").x;
    static const float text_rpr_cwidth        = ImGui::CalcTextSize("% RPR").x;
    static const float text_drp_cwidth        = ImGui::CalcTextSize(" DRP").x;
    static const float text_ms_width          = ImGui::CalcTextSize(" ms").x;
    static const float text_percent_width     = ImGui::CalcTextSize("%").x;
    static const float text_gb_part_width     = ImGui::CalcTextSize("00.00 GB").x;

    const float width_total_cpu = column_width_0 + column_width_cpu_1 + column_width_cpu_2 + column_width_cpu_3;
    const float width_total_fps = column_width_0 + column_width_fps_1 + column_width_fps_2 + column_width_fps_3 + column_width_fps_4;
    const float width_total_bat = column_width_0 + (column_width_bat * 4.0f);

    const float frame_time_warning_limit = m_FrameTimeVsyncLimit * 0.95f;

    //Pad shorter columns to match total length with the FPS row
    float padding = (width_total_fps - width_total_cpu) / 2.0f; //Only pad the second and last column so the first one lines up
    column_width_cpu_2 += padding;
    column_width_cpu_3 += padding;

    padding = (width_total_fps - width_total_bat) / 4.0f;
    column_width_bat += padding;

    //Pad window content to fps row width since columns don't increase the window size
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, 0.0f});
    ImGui::Dummy({width_total_fps, 0.0f});
    ImGui::PopStyleVar();

    //Align RAM and VRAM values even if they have different lengths
    static float text_ram_total_width  = 0.0f;
    static float text_vram_total_width = 0.0f;
    float text_ram_padding = std::max(text_ram_total_width, text_vram_total_width);

    //--CPU/GPU Frame Time Graphs

    //Shared offsets
    //Show both graphs if showing CPU and GPU stats or none of them (if graphs themselves are still active)
    bool show_both_graphs = (ConfigManager::GetValue(configid_bool_performance_monitor_show_cpu) == ConfigManager::GetValue(configid_bool_performance_monitor_show_gpu));
    const ImVec2 graph_size = {(show_both_graphs) ? floorf( (width_total_fps - item_spacing_prev) / 2.0f) : width_total_fps, ImGui::GetFontSize() * 2.0f};

    double plot_xmin = (m_FrameTimeLastIndex > (uint32_t)m_FrameTimeCPUHistory.MaxSize) ? m_FrameTimeLastIndex - m_FrameTimeCPUHistory.MaxSize + 0.5f : 0.5f;
    double plot_xmax = m_FrameTimeLastIndex - 0.75f;
    double plot_ymax = ceilf(m_FrameTimeVsyncLimit * 1.4f); 

    if (ConfigManager::GetValue(configid_bool_performance_monitor_show_graphs))
    {
        //-CPU Frame Time Graph
        if ( (show_both_graphs) || (ConfigManager::GetValue(configid_bool_performance_monitor_show_cpu)) )
        {
            DrawFrameTimeGraphCPU(graph_size, plot_xmin, plot_xmax, plot_ymax);

            if (show_both_graphs)
            {
                ImGui::SameLine(0.0f, item_spacing_prev);
            }
        }

        //-GPU Frame Time Graph
        if ( (show_both_graphs) || (ConfigManager::GetValue(configid_bool_performance_monitor_show_gpu)) )
        {
            DrawFrameTimeGraphGPU(graph_size, plot_xmin, plot_xmax, plot_ymax);
        }
    }

    //CPU/GPU columns
    if ( (ConfigManager::GetValue(configid_bool_performance_monitor_show_cpu)) || (ConfigManager::GetValue(configid_bool_performance_monitor_show_gpu)) )
    {
        ImGui::Columns(4, "ColumnStatsCompact", false);
        ImGui::SetColumnWidth(0, column_width_0);
        ImGui::SetColumnWidth(1, column_width_cpu_1);
        ImGui::SetColumnWidth(2, column_width_cpu_2);
        ImGui::SetColumnWidth(3, column_width_cpu_3);
    }

    //--Row CPU
    if (ConfigManager::GetValue(configid_bool_performance_monitor_show_cpu))
    {
        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_PerformanceMonitorCompactCPU));
        ImGui::NextColumn();

        //-CPU Frame Time
        //Warning color when frame time above 95% vsync time
        if (m_FrameTimeCPU > frame_time_warning_limit)
            ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

        ImGui::TextRight(text_ms_width, "%.2f", m_FrameTimeCPU);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(" ms");
        ImGui::NextColumn();

        if (m_FrameTimeCPU > frame_time_warning_limit)
            ImGui::PopStyleColor();

        //-CPU Load
        ImGui::TextRight(text_percent_width, "%.2f", m_PerfData.GetCPULoadPrecentage());
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted("%");
        ImGui::NextColumn();

        //-RAM
        ImGui::TextRight(text_ram_padding, "%.2f GB/", m_PerfData.GetRAMUsedGB());
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextRight(0.0f, "%.2f GB", m_PerfData.GetRAMTotalGB());
        text_ram_total_width = ImGui::GetItemRectSize().x;
        ImGui::NextColumn();
    }

    //--Row GPU
    if (ConfigManager::GetValue(configid_bool_performance_monitor_show_gpu))
    {
        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_PerformanceMonitorCompactGPU));
        ImGui::NextColumn();

        //-GPU Frame Time
        if (m_FrameTimeGPU > frame_time_warning_limit)
            ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

        ImGui::TextRight(text_ms_width, "%.2f", m_FrameTimeGPU);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(" ms");
        ImGui::NextColumn();

        if (m_FrameTimeGPU > frame_time_warning_limit)
            ImGui::PopStyleColor();

        if (!ConfigManager::GetValue(configid_bool_performance_monitor_disable_gpu_counters)) //No point in showing it all if it's not updating
        {
            //-GPU Load
            ImGui::TextRight(text_percent_width, "%.2f", m_PerfData.GetGPULoadPrecentage());
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted("%");
            ImGui::NextColumn();

            //-VRAM
            ImGui::TextRight(text_ram_padding, "%.2f GB/", m_PerfData.GetVRAMUsedGB());
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextRight(0.0f, "%.2f GB", m_PerfData.GetVRAMTotalGB());
            text_vram_total_width = ImGui::GetItemRectSize().x;
            ImGui::NextColumn();
        }
    }

    //--Row FPS
    if (ConfigManager::GetValue(configid_bool_performance_monitor_show_fps))
    {
        ImGui::Columns(5, "ColumnStatsCompactFPS", false);
        ImGui::SetColumnWidth(0, column_width_0);
        ImGui::SetColumnWidth(1, column_width_fps_1);
        ImGui::SetColumnWidth(2, column_width_fps_2);
        ImGui::SetColumnWidth(3, column_width_fps_3);
        ImGui::SetColumnWidth(4, column_width_fps_4);

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_PerformanceMonitorCompactFPS));

        //-FPS
        ImGui::NextColumn();
        ImGui::TextRight(0.0f, "%d", m_FPS);
        ImGui::NextColumn();

        //-Average FPS
        ImGui::TextRight(text_avg_cwidth, "%.2f", m_FPS_Average);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString(tstr_PerformanceMonitorCompactFPSAverage));
        ImGui::NextColumn();

        //No VR app means no frame statistics (FPS still gets counted, though)
        if (m_PIDLast == 0)
            ImGui::PushItemDisabled();

        //-Reprojection Ratio
        ImGui::TextRight(text_rpr_cwidth, "%.2f", m_ReprojectionRatio);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString(tstr_PerformanceMonitorCompactReprojectionRatio));
        ImGui::NextColumn();

        //-Dropped Frames
        ImGui::TextRight(text_drp_cwidth, "%u", m_DroppedFrames);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString(tstr_PerformanceMonitorCompactDroppedFrames));
        ImGui::NextColumn();

        if (m_PIDLast == 0)
            ImGui::PopItemDisabled();
    }

    //--Row BAT
    if (ConfigManager::GetValue(configid_bool_performance_monitor_show_battery))
    {
        ImGui::Columns(1);  //Reset columns or else it won't treat it as a sepearate layout
        ImGui::Columns(5, "ColumnStatsCompactBAT", false);
        ImGui::SetColumnWidth(0, column_width_0);
        ImGui::SetColumnWidth(1, column_width_bat);
        ImGui::SetColumnWidth(2, column_width_bat);
        ImGui::SetColumnWidth(3, column_width_bat);
        ImGui::SetColumnWidth(4, column_width_bat);

        ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_PerformanceMonitorCompactBattery));
        ImGui::NextColumn();

        //-Battery Left
        ImGui::TextRightUnformatted(text_percentage_cwidth, TranslationManager::GetString(tstr_PerformanceMonitorCompactBatteryLeft));
        ImGui::SameLine(0.0f, 0.0f);

        if (m_BatteryLeft != -1.0f)
        {
            //15% warning color
            if (m_BatteryLeft < 15.0f)
                ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

            ImGui::TextRight(0.0f, "%.0f%%", m_BatteryLeft);

            if (m_BatteryLeft < 15.0f)
                ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushItemDisabled();
            ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString(tstr_PerformanceMonitorCompactBatteryDisconnected));
            ImGui::PopItemDisabled();
        }

        ImGui::NextColumn();

        //-Battery Right
        ImGui::TextRightUnformatted(text_percentage_cwidth, TranslationManager::GetString(tstr_PerformanceMonitorCompactBatteryRight));
        ImGui::SameLine(0.0f, 0.0f);

        if (m_BatteryRight != -1.0f)
        {
            //15% warning color
            if (m_BatteryRight < 15.0f)
                ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

            ImGui::TextRight(0.0f, "%.0f%%", m_BatteryRight);

            if (m_BatteryRight < 15.0f)
                ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushItemDisabled();
            ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString(tstr_PerformanceMonitorCompactBatteryDisconnected));
            ImGui::PopItemDisabled();
        }

        ImGui::NextColumn();

        //-Battery HMD (only shown if available)
        if (m_BatteryHMD != -1.0f)
        {
            ImGui::TextRightUnformatted(text_percentage_cwidth, TranslationManager::GetString(tstr_PerformanceMonitorCompactBatteryHMD));
            ImGui::SameLine(0.0f, 0.0f);

            //15% warning color
            if (m_BatteryHMD < 15.0f)
                ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

            ImGui::TextRight(0.0f, "%.0f%%", m_BatteryHMD);

            if (m_BatteryHMD < 15.0f)
                ImGui::PopStyleColor();
        }

        ImGui::NextColumn();

        //-Battery Trackers
        if (ConfigManager::GetValue(configid_bool_performance_monitor_show_trackers))
        {
            unsigned int tracker_number = 1;
            for (const auto& tracker_info : m_BatteryTrackers)
            {
                //Skip first column
                if (ImGui::GetColumnIndex() == 0)
                {
                    ImGui::NextColumn();
                }

                ImGui::TextRightUnformatted(text_percentage_cwidth, tracker_info.NameCompact.c_str());
                ImGui::SameLine(0.0f, 0.0f);

                //15% warning color
                if (tracker_info.BatteryLevel < 15.0f)
                    ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

                ImGui::TextRight(0.0f, "%.0f%%", tracker_info.BatteryLevel);

                if (tracker_info.BatteryLevel < 15.0f)
                    ImGui::PopStyleColor();

                ImGui::NextColumn();

                tracker_number++;
            }
        }

        //-Vive Wireless
        if ( (m_ViveWirelessLogPathExists) && (ConfigManager::GetValue(configid_bool_performance_monitor_show_vive_wireless)) )
        {
            //Skip first column
            if (ImGui::GetColumnIndex() == 0)
            {
                ImGui::NextColumn();
            }

            ImGui::TextRightUnformatted(text_percentage_cwidth, "VW");
            ImGui::SameLine(0.0f, 0.0f);

            if (m_ViveWirelessTemp != -1)
            {
                //90 degrees celsius warning color
                if (m_ViveWirelessTemp > 90)
                    ImGui::PushStyleColor(ImGuiCol_Text, Style_ImGuiCol_TextWarning);

                ImGui::TextRight(0.0f, "%d\xC2\xB0""C", m_ViveWirelessTemp);

                if (m_ViveWirelessTemp > 90)
                    ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushItemDisabled();
                ImGui::TextRightUnformatted(0.0f, TranslationManager::GetString(tstr_PerformanceMonitorCompactViveWirelessTempNotAvailable));
                ImGui::PopItemDisabled();
            }
        }
    }

    ImGui::PopStyleVar();

    //Last item rect height is the padding dummy == empty window
    if (ImGui::GetItemRectSize().y == 0.0f)
    {
        ImGui::Columns(1);
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_PerformanceMonitorEmpty));
    }
}

void WindowPerformance::UpdateStatValues()
{
    m_PerfData.Update();

    //Localized time string
    SYSTEMTIME system_time;
    ::GetSystemTime(&system_time);

    if ( (system_time.wHour != m_TimeLast.wHour) || (system_time.wMinute != m_TimeLast.wMinute) ) //Only grab a new string when the hour or minute changed
    {
        wchar_t time_wstr[64];

        if (::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, nullptr, nullptr, time_wstr, 63) != 0)
        {
            m_TimeStr = StringConvertFromUTF16(time_wstr);
        }

        m_TimeLast = system_time;
    }

    UpdateStatValuesSteamVR();
    UpdateStatValuesViveWireless();
}

void WindowPerformance::UpdateStatValuesSteamVR()
{
    //No OpenVR, no frame data
    if (!UIManager::Get()->IsOpenVRLoaded())
        return;

    //Get compositor timing from OpenVR
    vr::Compositor_FrameTiming frame_timing_current;
    frame_timing_current.m_nSize    = sizeof(vr::Compositor_FrameTiming);
    bool frame_timing_current_valid = vr::VRCompositor()->GetFrameTiming(&frame_timing_current, 0);

    if (frame_timing_current_valid)
    {
        //Set current timings
        m_FrameTimeCPU = frame_timing_current.m_flClientFrameIntervalMs + frame_timing_current.m_flCompositorRenderCpuMs;
        m_FrameTimeGPU = frame_timing_current.m_flTotalRenderGpuMs;

        //Update frame time history
        vr::Compositor_FrameTiming frame_timing_prev;
        frame_timing_prev.m_nSize    = sizeof(vr::Compositor_FrameTiming);
        bool frame_timing_prev_valid = false;

        //Sanity check
        if (frame_timing_current.m_nFrameIndex < m_FrameTimeLastIndex)
        {
            m_FrameTimeLastIndex = 0;
        }

        //Get all frame timings since the last time we updated (but not more than max history size)
        for (uint32_t frames_ago = std::min(frame_timing_current.m_nFrameIndex - m_FrameTimeLastIndex, (uint32_t)m_FrameTimeCPUHistory.MaxSize); frames_ago != 0; --frames_ago)
        {
            frame_timing_prev_valid = vr::VRCompositor()->GetFrameTiming(&frame_timing_prev, frames_ago);
            //Calculate our own frame index as we get duplicates if SteamVR runs out of history
            float frame_index = float(frame_timing_current.m_nFrameIndex - frames_ago);

            if (frame_timing_prev_valid)
            {
                float frame_time_cpu = frame_timing_prev.m_flClientFrameIntervalMs + frame_timing_prev.m_flCompositorRenderCpuMs;

                m_FrameTimeCPUHistory.AddFrame(frame_index, frame_time_cpu);
                m_FrameTimeCPUHistoryWarning.AddFrame(frame_index, (frame_time_cpu > m_FrameTimeVsyncLimit) ? frame_time_cpu : 0.0f);

                m_FrameTimeGPUHistory.AddFrame(frame_index, frame_timing_prev.m_flTotalRenderGpuMs);
                m_FrameTimeGPUHistoryWarning.AddFrame(frame_index, (frame_timing_prev.m_flTotalRenderGpuMs > m_FrameTimeVsyncLimit) ? frame_timing_prev.m_flTotalRenderGpuMs : 0.0f);
            }
            else //No valid data, leave gap in history
            {
                m_FrameTimeCPUHistory.AddFrame(frame_index, 0.0f);
                m_FrameTimeCPUHistoryWarning.AddFrame(frame_index, 0.0f);

                m_FrameTimeGPUHistory.AddFrame(frame_index, 0.0f);
                m_FrameTimeGPUHistoryWarning.AddFrame(frame_index, 0.0f);
            }
        }

        m_FrameTimeLastIndex = frame_timing_current.m_nFrameIndex;
    }
    else
    {
        m_FrameTimeCPU = 0.0f;
        m_FrameTimeGPU = 0.0f;
    }

    //Update cumulative stats
    vr::Compositor_CumulativeStats frame_stats = {0};
    vr::VRCompositor()->GetCumulativeStats(&frame_stats, sizeof(vr::Compositor_CumulativeStats));

    //Reset when process changed
    if (frame_stats.m_nPid != m_PIDLast)
    {
        //Additionally check if it's actually the running application and not just some past app's stats
        if (frame_stats.m_nPid == vr::VRApplications()->GetCurrentSceneProcessId())
        {
            m_PIDLast = frame_stats.m_nPid;
            ResetCumulativeValues();
        }
    }
    else if ( (frame_stats.m_nPid != vr::VRApplications()->GetCurrentSceneProcessId()) && (m_PIDLast != 0) ) //Not actually the running application, set last pid to 0 instead if it isn't yet
    {
        m_PIDLast = 0;
        ResetCumulativeValues();
    }

    //Apply offsets to stat values
    uint32_t frame_presents               = frame_stats.m_nNumFramePresents             - m_OffsetFramesPresents;
    uint32_t reprojected_frames           = frame_stats.m_nNumReprojectedFrames         - m_OffsetReprojectedFrames;

    //Update frame count if at least a second passed since the last time
    if (m_FPS_TickLast + 1000 <= ::GetTickCount64())
    {
        uint32_t frame_count = frame_timing_current.m_nFrameIndex - m_OffsetFrameIndex;

        if (vr::VRSystem()->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd) != vr::k_EDeviceActivityLevel_Standby) //Don't count frames when entering standby
        {
            if (m_FrameCountLast != 0)
            {
                m_FPS = frame_count - m_FrameCountLast;                                   //Total unreprojected frames rendered since last time
                m_FPS = int(m_FPS / ((::GetTickCount64() - m_FPS_TickLast) / 1000.0f));   //Divided by seconds passed in case it has been more than just 1

                m_FrameCountTotal += m_FPS;     //This means m_FrameCountTotal may not be the total frames rendered but the sum of whatever we displayed as FPS before
                m_FrameCountTotalCount++;

                if (m_FrameCountTotalCount != 0)
                {
                    m_FPS_Average = m_FrameCountTotal / (float)m_FrameCountTotalCount;

                    //Ignore very small decimals as they occur often and are more confusing than helpful when the fps are actually totally fine (as they don't really go away)
                    if (fabs(m_FPS_Average - roundf(m_FPS_Average)) < 0.05f)
                    {
                        m_FPS_Average = roundf(m_FPS_Average);
                    }
                }
            }
        }

        m_FPS_TickLast   = ::GetTickCount64();
        m_FrameCountLast = frame_count;
    }

    //Reprojection ratio and dropped frames
    m_ReprojectionRatio = (frame_presents != 0) ? ((float)reprojected_frames / frame_presents) * 100.f : 0.0f;
    m_DroppedFrames     = frame_stats.m_nNumDroppedFrames - m_OffsetDroppedFrames;

    //Battery Left
    vr::TrackedDeviceIndex_t device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);

    if (device_index != vr::k_unTrackedDeviceIndexInvalid)
    {
        m_BatteryLeft = vr::VRSystem()->GetFloatTrackedDeviceProperty(device_index, vr::Prop_DeviceBatteryPercentage_Float) * 100.0f;
    }
    else
    {
        m_BatteryRight = -1.0f;
    }

    //Battery Right
    device_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

    if (device_index != vr::k_unTrackedDeviceIndexInvalid)
    {
        m_BatteryRight = vr::VRSystem()->GetFloatTrackedDeviceProperty(device_index, vr::Prop_DeviceBatteryPercentage_Float) * 100.0f;
    }
    else
    {
        m_BatteryRight = -1.0f;
    }

    //Battery HMD
    if (vr::VRSystem()->GetBoolTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DeviceProvidesBatteryStatus_Bool))
    {
        m_BatteryHMD = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DeviceBatteryPercentage_Float) * 100.0f;
    }
    else
    {
        m_BatteryHMD = -1.0f;
    }

    //Battery Trackers
    for (auto& tracker_info : m_BatteryTrackers)
    {
        tracker_info.BatteryLevel = vr::VRSystem()->GetFloatTrackedDeviceProperty(tracker_info.DeviceIndex, vr::Prop_DeviceBatteryPercentage_Float) * 100.0f;
    }
}

void WindowPerformance::UpdateStatValuesViveWireless()
{
    //Vive Wireless Temperatures can seemingly only be read from the log file it's constantly writing too
    //This doesn't seem terribly efficient, but it's better than nothing
    //Reading is done every 5 seconds

    //Don't update if the path doesn't exist or it's not even enabled
    if ( (!m_ViveWirelessLogPathExists) || (!ConfigManager::GetValue(configid_bool_performance_monitor_show_vive_wireless)) )
        return;

    if (m_ViveWirelessTickLast + 5000 <= ::GetTickCount64())
    {
        m_ViveWirelessTickLast = ::GetTickCount64();
        m_ViveWirelessTemp = -1;

        //Find the newest log file
        std::wstring path_str = m_ViveWirelessLogPath;
        path_str += L"*.txt";
        std::vector< std::pair<std::wstring, ULARGE_INTEGER> > file_list;   //std::pair<filename, last_modified_time>
        WIN32_FIND_DATA find_data;
        HANDLE handle_find = ::FindFirstFileW(path_str.c_str(), &find_data);

        if (handle_find != INVALID_HANDLE_VALUE)
        {
            do
            {
                //Add filename and last modified time in list
                file_list.emplace_back(find_data.cFileName, ULARGE_INTEGER{find_data.ftLastWriteTime.dwLowDateTime, find_data.ftLastWriteTime.dwHighDateTime});
            }
            while (::FindNextFileW(handle_find, &find_data) != 0);

            ::FindClose(handle_find);
        }

        auto it = std::max_element(file_list.begin(), file_list.end(), [](const auto& data_a, const auto& data_b){ return (data_a.second.QuadPart < data_b.second.QuadPart); });

        if (it == file_list.end())
            return;

        //Check if the newest file is older than 2 minutes, in which case we don't use it to read a temperature at all
        FILETIME ftime_current;
        ::GetSystemTimeAsFileTime(&ftime_current);
        ULARGE_INTEGER time_current{ftime_current.dwLowDateTime, ftime_current.dwHighDateTime};

        if (it->second.QuadPart + 1200000000 <= time_current.QuadPart) //+ 2 minutes in 100 ns intervals
        {
            return;
        }

        //If the newest file is not the same as last time, reset the last used line number
        if (it->first != m_ViveWirelessLogFileLast)
        {
            m_ViveWirelessLogFileLast     = it->first;
            m_ViveWirelessLogFileLastLine = 0;
        }

        //Read log file
        {
            path_str = m_ViveWirelessLogPath + m_ViveWirelessLogFileLast;

            std::wifstream log_file(path_str);

            if (log_file.good())
            {
                //Imbue with utf16-le locale, as are the files Vive Wireless writes (codecvt_utf16 is deprecated starting C++17, but this is the most straight forward way to deal with this)
                log_file.imbue(std::locale(log_file.getloc(), new std::codecvt_utf16<wchar_t, 0x10ffff, std::little_endian>()));

                //Read lines and see if the M_Temperature can be found (R_Temperature isn't interesting in our case)
                int line_count = 0;
                std::wstring line_str;
                std::string temp_str;
                size_t mtemp_pos;

                while (log_file.good())
                {
                    std::getline(log_file, line_str);
                    line_count++;

                    //Only check for the temperature value if this line is the same or greater than last time (0 if it's a new file)
                    if (line_count >= m_ViveWirelessLogFileLastLine)
                    {
                        mtemp_pos = line_str.find(L"M_Temperature=");

                        if (mtemp_pos != std::wstring::npos)
                        {
                            temp_str = StringConvertFromUTF16(line_str.substr(mtemp_pos + 14).c_str());
                            m_ViveWirelessTemp = atoi(temp_str.c_str());
                            m_ViveWirelessLogFileLastLine = line_count;
                        }
                    }
                }
            }
        }
    }
}

void WindowPerformance::DrawFrameTimeGraphCPU(const ImVec2& graph_size, double plot_xmin, double plot_xmax, double plot_ymax)
{
    float frame_offset = 1.0f + ImGui::GetStyle().FrameBorderSize;
    ImVec2 cursor_screen_pos_graph = ImGui::GetCursorScreenPos();

    ImPlot::SetNextAxesLimits(plot_xmin, plot_xmax, 0.0, plot_ymax, ImGuiCond_Always);

    if (ImPlot::BeginPlot("##PlotCPU", graph_size, ImPlotFlags_CanvasOnly))
    {
        const ImVector<ImVec2>& plot_data      = m_FrameTimeCPUHistory.Data;
        const ImVector<ImVec2>& plot_data_warn = m_FrameTimeCPUHistoryWarning.Data;

        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_LockMin);

        if (!plot_data.empty())
        {
            ImGui::PushClipRect({cursor_screen_pos_graph.x + frame_offset, cursor_screen_pos_graph.y + frame_offset}, 
                                {(cursor_screen_pos_graph.x + graph_size.x) - frame_offset, cursor_screen_pos_graph.y + graph_size.y - frame_offset},
                                false);


            ImPlot::SetNextFillStyle(ImVec4(0.5f, 1.0f, 0.0f, 1.0f));
            ImPlot::PlotShaded("##DataShaded", &plot_data[0].x, &plot_data[0].y, plot_data.size(), 0.0, ImPlotShadedFlags_None, m_FrameTimeCPUHistory.Offset, 2 * sizeof(float));

            ImPlot::SetNextLineStyle(ImVec4(0.5f, 1.0f, 0.0f, 1.0f));
            ImPlot::PlotLine("##DataLine", &plot_data[0].x, &plot_data[0].y, plot_data.size(), ImPlotLineFlags_None, m_FrameTimeCPUHistory.Offset, 2 * sizeof(float));

            ImPlot::SetNextFillStyle(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImPlot::SetNextLineStyle(ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImPlot::PlotBars("##DataWarn", &plot_data_warn[0].x, &plot_data_warn[0].y, plot_data_warn.size(), 1.0, m_FrameTimeCPUHistoryWarning.Offset, 2 * sizeof(float));

            ImVec2 rmin = ImPlot::PlotToPixels(ImPlotPoint(plot_xmin, m_FrameTimeVsyncLimit));
            ImVec2 rmax = ImPlot::PlotToPixels(ImPlotPoint(plot_xmax, m_FrameTimeVsyncLimit));
            ImPlot::PushPlotClipRect();
            ImPlot::GetPlotDrawList()->AddLine(rmin, rmax, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Border)) ); 
            ImPlot::PopPlotClipRect();

            ImGui::PopClipRect();
        }

        ImPlot::EndPlot();
    }
}

void WindowPerformance::DrawFrameTimeGraphGPU(const ImVec2& graph_size, double plot_xmin, double plot_xmax, double plot_ymax)
{
    float frame_offset = 1.0f + ImGui::GetStyle().FrameBorderSize;
    ImVec2 cursor_screen_pos_graph = ImGui::GetCursorScreenPos();

    ImPlot::SetNextAxesLimits(plot_xmin, plot_xmax, 0.0, plot_ymax, ImGuiCond_Always);

    if (ImPlot::BeginPlot("##PlotGPU", graph_size, ImPlotFlags_CanvasOnly))
    {
        const ImVector<ImVec2>& plot_data      = m_FrameTimeGPUHistory.Data;
        const ImVector<ImVec2>& plot_data_warn = m_FrameTimeGPUHistoryWarning.Data;

        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_LockMin);

        if (!plot_data.empty())
        {
            ImGui::PushClipRect({cursor_screen_pos_graph.x + frame_offset, cursor_screen_pos_graph.y + frame_offset}, 
                                {(cursor_screen_pos_graph.x + graph_size.x) - frame_offset, cursor_screen_pos_graph.y + graph_size.y - frame_offset},
                                false);


            ImPlot::SetNextFillStyle(ImVec4(0.5f, 1.0f, 0.0f, 1.0f));
            ImPlot::PlotShaded("##DataShaded", &plot_data[0].x, &plot_data[0].y, plot_data.size(), 0.0, ImPlotShadedFlags_None, m_FrameTimeGPUHistory.Offset, 2 * sizeof(float));

            ImPlot::SetNextLineStyle(ImVec4(0.5f, 1.0f, 0.0f, 1.0f));
            ImPlot::PlotLine("##DataLine", &plot_data[0].x, &plot_data[0].y, plot_data.size(), ImPlotLineFlags_None, m_FrameTimeGPUHistory.Offset, 2 * sizeof(float));

            ImPlot::SetNextFillStyle(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImPlot::SetNextLineStyle(ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImPlot::PlotBars("##DataWarn", &plot_data_warn[0].x, &plot_data_warn[0].y, plot_data_warn.size(), 1.0, m_FrameTimeGPUHistoryWarning.Offset, 2 * sizeof(float));

            ImVec2 rmin = ImPlot::PlotToPixels(ImPlotPoint(plot_xmin, m_FrameTimeVsyncLimit));
            ImVec2 rmax = ImPlot::PlotToPixels(ImPlotPoint(plot_xmax, m_FrameTimeVsyncLimit));
            ImPlot::PushPlotClipRect();
            ImPlot::GetPlotDrawList()->AddLine(rmin, rmax, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Border)) ); 
            ImPlot::PopPlotClipRect();


            ImGui::PopClipRect();
        }

        ImPlot::EndPlot();
    }
}

void WindowPerformance::CheckScheduledOverlaySharedTextureUpdate()
{
    if (!UIManager::Get()->IsOpenVRLoaded())
        return;

    if (m_IsOverlaySharedTextureUpdateNeeded)
    {
        const DPRect& rect_total = UITextureSpaces::Get().GetRect(ui_texspace_total);

        vr::HmdVector2_t mouse_scale = {(float)rect_total.GetWidth(), (float)rect_total.GetHeight()};

        bool all_ok = true;

        for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
        {
            const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

            if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui)
            {
                vr::VROverlayHandle_t ovrl_handle = data.ConfigHandle[configid_handle_overlay_state_overlay_handle];

                if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
                {
                    //Check if we're allowed to render to it from this process yet, otherwise try again next frame
                    if (vr::VROverlay()->GetOverlayRenderingPid(ovrl_handle) == ::GetCurrentProcessId())
                    {
                        vr::VROverlayEx()->SetSharedOverlayTexture(UIManager::Get()->GetOverlayHandleOverlayBar(), ovrl_handle, UIManager::Get()->GetSharedTextureRef());
                        vr::VROverlay()->SetOverlayMouseScale(ovrl_handle, &mouse_scale);
                    }
                    else
                    {
                        all_ok = false;
                    }
                }
                else
                {
                    all_ok = false;
                }
            }
        }

        if (all_ok)
        {
            OnWindowBoundsChanged();
            m_IsOverlaySharedTextureUpdateNeeded = false;
        }
    }
}

void WindowPerformance::OnWindowBoundsChanged()
{
    if (!UIManager::Get()->IsOpenVRLoaded())
        return;

    //Texture bounds
    vr::VRTextureBounds_t bounds;
    //Avoid resize flicker before a real size is known (ImGui defaults to 32) and just set bounds to 0 in that case
    if (m_Size.x > 32.0f)
    {
        const DPRect& rect       = UITextureSpaces::Get().GetRect(ui_texspace_performance_monitor);
        const DPRect& rect_total = UITextureSpaces::Get().GetRect(ui_texspace_total);
        float tex_width  = (float)rect_total.GetWidth();
        float tex_height = (float)rect_total.GetHeight();
        bounds.uMin = rect.GetTL().x / tex_width;
        bounds.vMin = rect.GetTL().y / tex_height;
        bounds.uMax = rect.GetBR().x / tex_width;
        bounds.vMax = rect.GetBR().y / tex_height;
    }
    else
    {
        bounds.uMin = 0.0f;
        bounds.uMax = 0.0f;
        bounds.vMin = 0.0f;
        bounds.vMax = 0.0f;
    }

    //Interesection mask
    vr::VROverlayIntersectionMaskPrimitive_t intersection_mask;
    intersection_mask.m_nPrimitiveType = vr::OverlayIntersectionPrimitiveType_Rectangle;
    intersection_mask.m_Primitive.m_Rectangle.m_flTopLeftX = m_Pos.x;
    intersection_mask.m_Primitive.m_Rectangle.m_flTopLeftY = m_Pos.y;
    intersection_mask.m_Primitive.m_Rectangle.m_flWidth    = m_Size.x;
    intersection_mask.m_Primitive.m_Rectangle.m_flHeight   = m_Size.y;

    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

        if (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui)
        {
            vr::VROverlayHandle_t ovrl_handle = data.ConfigHandle[configid_handle_overlay_state_overlay_handle];

            if (ovrl_handle != vr::k_ulOverlayHandleInvalid)
            {
                vr::VROverlay()->SetOverlayIntersectionMask(ovrl_handle, &intersection_mask, 1);
                vr::VROverlay()->SetOverlayTextureBounds(ovrl_handle, &bounds);
            }
        }
    }
}

void WindowPerformance::RefreshTrackerBatteryList()
{
    m_BatteryTrackers.clear();

    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
        if ( (vr::VRSystem()->GetTrackedDeviceClass(i) == vr::TrackedDeviceClass_GenericTracker) && (vr::VRSystem()->IsTrackedDeviceConnected(i)) )
        {
            TrackerInfo info;
            info.DeviceIndex  = i;
            info.BatteryLevel = vr::VRSystem()->GetFloatTrackedDeviceProperty(i, vr::Prop_DeviceBatteryPercentage_Float) * 100.0f;

            info.Name = TranslationManager::GetString(tstr_PerformanceMonitorBatteryTracker);
            StringReplaceAll(info.Name, "%ID%", std::to_string(m_BatteryTrackers.size() + 1));

            info.NameCompact = TranslationManager::GetString(tstr_PerformanceMonitorCompactBatteryTracker);
            StringReplaceAll(info.NameCompact, "%ID%", std::to_string(m_BatteryTrackers.size() + 1));

            m_BatteryTrackers.push_back(info);
        }
    }
}

void WindowPerformance::ResetCumulativeValues()
{
    m_FPS         = 0;
    m_FPS_Average = 0.0f;

    m_FrameCountLast       = 0;
    m_FrameCountTotal      = 0;
    m_FrameCountTotalCount = 0;
    m_FPS_TickLast         = 0;

    m_ViveWirelessTickLast = 0;

    //This is also called from the constructor when UIManager does not exist yet
    if ((UIManager::Get() != nullptr) && (UIManager::Get()->IsOpenVRLoaded()))
    {
        m_FrameTimeVsyncLimit = 1000.f / vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

        //Update cumulative offset values
        vr::Compositor_CumulativeStats frame_stats = {0};
        vr::VRCompositor()->GetCumulativeStats(&frame_stats, sizeof(vr::Compositor_CumulativeStats));

        m_OffsetFramesPresents              = frame_stats.m_nNumFramePresents;
        m_OffsetReprojectedFrames           = frame_stats.m_nNumReprojectedFrames;
        m_OffsetDroppedFrames               = frame_stats.m_nNumDroppedFrames;

        //Update frame index offset
        vr::Compositor_FrameTiming frame_timing_current;
        frame_timing_current.m_nSize = sizeof(vr::Compositor_FrameTiming);
        m_OffsetFrameIndex = 0;

        if (vr::VRCompositor()->GetFrameTiming(&frame_timing_current, 0))
        {
            m_OffsetFrameIndex = frame_timing_current.m_nFrameIndex;
        }
    }
}

void WindowPerformance::ScheduleOverlaySharedTextureUpdate()
{
    //We only want to set the shared texture once when needed, but also have to wait for the render PID to change (which is async) and stay responsive
    //so setting it is done by the performance window when possible

    m_IsOverlaySharedTextureUpdateNeeded = true;
}

Win32PerformanceData& WindowPerformance::GetPerformanceData()
{
    return m_PerfData;
}

bool WindowPerformance::IsViveWirelessInstalled()
{
    return m_ViveWirelessLogPathExists;
}

const ImVec2 & WindowPerformance::GetPos() const
{
    return m_Pos;
}

const ImVec2 & WindowPerformance::GetSize() const
{
    return m_Size;
}

bool WindowPerformance::IsVisible() const
{
    return m_Visible;
}

void WindowPerformance::SetPopupOpen(bool is_open)
{
    m_IsPopupOpen = is_open;
}

bool WindowPerformance::IsAnyOverlayUsingPerformanceMonitor()
{
    if (!UIManager::Get()->IsOpenVRLoaded())
        return false;

    //This isn't the most efficient check thanks to the split responsibilities of each process, but it's still better than always rendering the window
    for (unsigned int i = 0; i < OverlayManager::Get().GetOverlayCount(); ++i)
    {
        const OverlayConfigData& data = OverlayManager::Get().GetConfigData(i);

        if ( (data.ConfigBool[configid_bool_overlay_enabled]) && (data.ConfigInt[configid_int_overlay_capture_source] == ovrl_capsource_ui) )
        {
            vr::VROverlayHandle_t ovrl_handle = data.ConfigHandle[configid_handle_overlay_state_overlay_handle];

            if ( (ovrl_handle != vr::k_ulOverlayHandleInvalid) && (vr::VROverlay()->IsOverlayVisible(ovrl_handle)) )
            {
                return true;
            }
        }
    }

    return false;
}
