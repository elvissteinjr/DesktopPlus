#include "WindowSettings.h"

#define NOMINMAX
#include <windows.h>
#include <dxgi1_2.h>
#include <sstream>

#include "imgui.h"

#include "UIManager.h"
#include "Ini.h"
#include "Util.h"
#include "ConfigManager.h"
#include "TextureManager.h"
#include "InterprocessMessaging.h"
#include "ImGuiExt.h"

void WindowSettings::UpdateWarnings()
{
    bool warning_displayed = false;

    //Compositor resolution warning
    {
        bool& hide_compositor_res_warning = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_warning_compositor_res_hidden);

        if ((!hide_compositor_res_warning) && (UIManager::Get()->IsCompositorResolutionLow()))
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningCompRes"))
            {
                ImGui::OpenPopup("DontShowAgain");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: Compositor resolution is below 100%%! This affects overlay rendering quality.");

            if (ImGui::BeginPopup("DontShowAgain"))
            {
                if (ImGui::Selectable("Don't show this again"))
                {
                    hide_compositor_res_warning = true;
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Compositor quality warning
    {
        bool& hide_compositor_quality_warning = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_warning_compositor_quality_hidden);

        if ((!hide_compositor_quality_warning) && (UIManager::Get()->IsCompositorRenderQualityLow()))
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningCompQuality"))
            {
                ImGui::OpenPopup("DontShowAgain2");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: Overlay render quality is not set to high!");

            if (ImGui::BeginPopup("DontShowAgain2"))
            {
                if (ImGui::Selectable("Don't show this again"))
                {
                    hide_compositor_quality_warning = true;
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Dashboard app process elevation warning
    {
        bool& hide_process_elevation_warning = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_warning_process_elevation_hidden);

        if ((!hide_process_elevation_warning) && (ConfigManager::Get().GetConfigBool(configid_bool_state_misc_process_elevated)))
        {
            //Use selectable stretching over the text area to make it clickable
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); //Make the selectable invisible though
            if (ImGui::Selectable("##WarningElevation"))
            {
                ImGui::OpenPopup("DontShowAgain3");
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(Style_ImGuiCol_TextWarning, "Warning: Desktop+ is running with administrative privileges!");

            if (ImGui::BeginPopup("DontShowAgain3"))
            {
                if (ImGui::Selectable("Don't show this again"))
                {
                    hide_process_elevation_warning = true;
                }
                ImGui::EndPopup();
            }

            warning_displayed = true;
        }
    }

    //Separate from the main content if a warning was actually displayed
    if (warning_displayed)
    {
        //Horizontal separator
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
        ImGui::BeginChild("hsepW", ImVec2(0.0f, 1.0f), true);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

void WindowSettings::UpdateCatOverlay()
{
    ImGui::Text("Overlay");
            
    //Horizontal separator (not using ImGui::Separator() since it looks slightly different to the vertical makeshift one with transparent colors)
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewOverlaySettings");

        const float column_width_0 = ImGui::GetFontSize() * 10.0f;

        bool& detached = ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_detached);

        //General
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "General");
            ImGui::Columns(2, "ColumnGeneral", false);
            ImGui::SetColumnWidth(0, column_width_0);

            if (detached)   //Disable dashboard setting when they're not relevant to avoid confusion
                ImGui::PushItemDisabled();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Width");
            ImGui::NextColumn();

            float& width = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_width);

            if (ImGui::SliderWithButtonsFloat("OverlayWidth", width, 0.1f, 0.05f, 20.0f, "%.2f m"))
            {
                if (width < 0.05f)
                    width = 0.05f;

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_width), *(LPARAM*)&width);
            }
            ImGui::NextColumn();

            if (detached)
                ImGui::PopItemDisabled();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Curvature");
            ImGui::NextColumn();

            //This maps the float curve as int percentage, so the cropping stuff for the rest
            float& curve = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_curvature);
            int curve_ui = (curve == -1.0f) ? 101 : int(curve * 100.0f);

            if (ImGui::SliderWithButtonsInt("OverlayCurvature", curve_ui, 5, 0, 101, (curve == -1.0f) ? "Auto" : "%d%%"))
            {
                curve = clamp(curve_ui, 0, 101) / 100.0f;

                if (curve_ui > 100)
                    curve = -1.0f;

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_curvature), *(LPARAM*)&curve);
            }

            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Opacity");
            ImGui::NextColumn();

            //This maps the float curve as int percentage, so the cropping stuff for the rest
            float& opacity = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_opacity);

            if (ImGui::SliderWithButtonsFloatPercentage("OverlayOpacity", opacity, 5, 0, 100, "%d%%"))
            {
                opacity = clamp(opacity, 0.0f, 1.0f);

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_opacity), *(LPARAM*)&opacity);
            }

            ImGui::Columns(1);
        }

        //Position
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Position");
            ImGui::Columns(2, "ColumnPosition", false);
            ImGui::SetColumnWidth(0, column_width_0);

            if (detached)
                ImGui::PushItemDisabled();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Up/Down Offset");
            ImGui::NextColumn();

            float& up = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_offset_up);
                
            if (ImGui::SliderWithButtonsFloat("OverlayOffsetUp", up, 0.1f, -20.0f, 20.0f, "%.2f m"))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_up), *(LPARAM*)&up);
            }
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Right/Left Offset");
            ImGui::NextColumn();

            float& right = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_offset_right);
                
            if (ImGui::SliderWithButtonsFloat("OverlayOffsetRight", right, 0.1f, -20.0f, 20.0f, "%.2f m"))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_right), *(LPARAM*)&right);
            }
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Forward/Backward Offset");
            ImGui::NextColumn();

            float& forward = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_offset_forward);
                
            if (ImGui::SliderWithButtonsFloat("OverlayOffsetForward", forward, 0.1f, -20.0f, 20.0f, "%.2f m"))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_forward), *(LPARAM*)&forward);
            }

            ImGui::Columns(1);

            if (detached)
                ImGui::PopItemDisabled();
        }

        //Cropping
        {
            int ovrl_width, ovrl_height;
            UIManager::Get()->GetOverlayPixelSize(ovrl_width, ovrl_height);

            int& crop_x = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_x);
            int& crop_y = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_y);
            int& crop_width = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_width);
            int& crop_height = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_crop_height);

            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Cropping");
            ImGui::Columns(2, "ColumnCrop", false);
            ImGui::SetColumnWidth(0, column_width_0);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Crop X");
            ImGui::NextColumn();

            if (ImGui::SliderWithButtonsInt("CropX", crop_x, 1, 0, ovrl_width - 1, "%d px"))
            {
                //Note that we need to clamp the new value as neither the buttons nor the slider on direct input do so (they could, but this is in line with the rest of ImGui)
                crop_x = clamp(crop_x, 0, ovrl_width - 1);

                if (crop_x + crop_width > ovrl_width)
                {
                    crop_width = ovrl_width - crop_x;
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_width), crop_width);
                }

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_x), crop_x);
            }
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Crop Y");
            ImGui::NextColumn();

            if (ImGui::SliderWithButtonsInt("CropY", crop_y, 1, 0, ovrl_height - 1 /*DesktopHeight*/, "%d px"))
            {
                crop_y = clamp(crop_y, 0, ovrl_height - 1);

                if (crop_y + crop_height > ovrl_height)
                {
                    crop_height = ovrl_height - crop_y;
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);
                }

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_y), crop_y);
            }
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Crop Width");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1);

            //The way mapping max + 1 == -1 value into the slider is done is a bit convoluted, but it works
            int crop_width_max = ovrl_width - crop_x;
            int crop_width_ui = (crop_width == -1) ? crop_width_max + 1 : crop_width;

            if (ImGui::SliderWithButtonsInt("CropWidth", crop_width_ui, 1, 1, crop_width_max + 1, (crop_width == -1) ? "Max" : "%d px"))
            {
                crop_width = clamp(crop_width_ui, 1, crop_width_max + 1);

                if (crop_width_ui > crop_width_max)
                    crop_width = -1;

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_width), crop_width);
            }
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Crop Height");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1);

            int crop_height_max = ovrl_height - crop_y;
            int crop_height_ui = (crop_height == -1) ? crop_height_max + 1 : crop_height;

            if (ImGui::SliderWithButtonsInt("CropHeight", crop_height_ui, 1, 1, crop_height_max + 1, (crop_height == -1) ? "Max" : "%d px"))
            {
                crop_height = clamp(crop_height_ui, 1, crop_height_max + 1);

                if (crop_height_ui > crop_height_max)
                    crop_height = -1;

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);
            }

            ImGui::Columns(1);
        }

        //3D
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "3D");
            ImGui::Columns(2, "Column3D", false);
            ImGui::SetColumnWidth(0, column_width_0);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("3D Mode");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1);
            const char* items[] = { "Off", "Half Side-by-Side", "Side-by-Side" };
            int& mode_3D = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_3D_mode);
            if (ImGui::Combo("##Combo3DMode", &mode_3D, items, IM_ARRAYSIZE(items)))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_3D_mode), mode_3D);
            }

            ImGui::NextColumn();

            bool& swapped_3D = ConfigManager::Get().GetConfigBoolRef(configid_bool_overlay_3D_swapped);
            if (ImGui::Checkbox("Swap Left/Right", &swapped_3D))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_3D_swapped), swapped_3D);
            }

            ImGui::Columns(1);
        }

        //Detached Overlay
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Detached Overlay");
            ImGui::Columns(2, "ColumnDetached", false);
            ImGui::SetColumnWidth(0, column_width_0);

            static bool is_generic_tracker_connected = false;

            if ((ImGui::IsWindowAppearing()) && (UIManager::Get()->IsOpenVRLoaded()))
            {
                is_generic_tracker_connected = (GetFirstVRTracker() != vr::k_unTrackedDeviceIndexInvalid);
            }

            if (ImGui::Checkbox("Detach Overlay", &detached))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_detached), detached);
                
                //Automatically reset the matrix to a saner default if it still has the zero value
                if (ConfigManager::Get().GetOverlayDetachedTransform().isZero())
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
                }

                if (!UIManager::Get()->IsInDesktopMode())
                {
                    vr::VROverlay()->SetOverlaySortOrder(UIManager::Get()->GetOverlayHandle(), (detached) ? 0 : 1);
                }
            }

            ImGui::NextColumn();
            ImGui::NextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Display Mode");
            ImGui::NextColumn();
                
            int& mode_origin = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_detached_origin);

            if (mode_origin == ovrl_origin_hmd)
                ImGui::PushItemDisabled();

            ImGui::SetNextItemWidth(-1);
                
            int& mode_display = ConfigManager::Get().GetConfigIntRef(configid_int_overlay_detached_display_mode);

            const char* items_display[] = {"Always", "Only in Dashboard", "Only in Scene", "Only in Desktop+ Tab"};
            if (ImGui::BeginCombo("##ComboDetachedDisplayMode", items_display[mode_display]))
            {
                int mode_display_old = mode_display;

                //Displays some only when origin isn't dashboard
                if ( (mode_origin != ovrl_origin_dashboard) && (ImGui::Selectable(items_display[ovrl_dispmode_always], (mode_display == ovrl_dispmode_always))) )
                    mode_display = ovrl_dispmode_always;
                if (ImGui::Selectable(items_display[ovrl_dispmode_dashboard], (mode_display == ovrl_dispmode_dashboard)))
                    mode_display = ovrl_dispmode_dashboard;
                if ( (mode_origin != ovrl_origin_dashboard) && (ImGui::Selectable(items_display[ovrl_dispmode_scene], (mode_display == ovrl_dispmode_scene))) )
                    mode_display = ovrl_dispmode_scene;
                if (ImGui::Selectable(items_display[ovrl_dispmode_dplustab], (mode_display == ovrl_dispmode_dplustab)))
                    mode_display = ovrl_dispmode_dplustab;

                if (mode_display != mode_display_old)
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_detached_display_mode), mode_display);

                ImGui::EndCombo();
            }
                
            if (mode_origin == ovrl_origin_hmd)
                ImGui::PopItemDisabled();

            ImGui::NextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Width");
            ImGui::NextColumn();

            float& width = ConfigManager::Get().GetConfigFloatRef(configid_float_overlay_detached_width);
            if (ImGui::SliderWithButtonsFloat("OverlayDetachedWidth", width, 0.1f, 0.05f, 20.0f, "%.2f m"))
            {
                if (width < 0.05f)
                    width = 0.05f;

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_detached_width), *(LPARAM*)&width);
            }

            ImGui::NextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Position Origin");
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Some origins are restricted to certain display modes");
            ImGui::NextColumn();
                
            ImGui::SetNextItemWidth(-1);

            const char* items_origin[] = {"Play Area", "HMD Floor Position", "Dashboard", "HMD", "Right Controller", "Left Controller", "Tracker #1"};
            if (ImGui::BeginCombo("##ComboDetachedOrigin", items_origin[mode_origin]))
            {
                int mode_origin_old = mode_origin;

                //Displays tracker option only when one is connected
                if ( (ImGui::Selectable(items_origin[ovrl_origin_room],       (mode_origin == ovrl_origin_room))) )
                    mode_origin = ovrl_origin_room;
                if ( (ImGui::Selectable(items_origin[ovrl_origin_hmd_floor],  (mode_origin == ovrl_origin_hmd_floor))) )
                    mode_origin = ovrl_origin_hmd_floor;
                if ( (ImGui::Selectable(items_origin[ovrl_origin_dashboard],  (mode_origin == ovrl_origin_dashboard))) )
                    mode_origin = ovrl_origin_dashboard;
                if ( (ImGui::Selectable(items_origin[ovrl_origin_hmd],        (mode_origin == ovrl_origin_hmd))) )
                    mode_origin = ovrl_origin_hmd;
                if ( (ImGui::Selectable(items_origin[ovrl_origin_right_hand], (mode_origin == ovrl_origin_right_hand))) )
                    mode_origin = ovrl_origin_right_hand;
                if ( (ImGui::Selectable(items_origin[ovrl_origin_left_hand],  (mode_origin == ovrl_origin_left_hand))) )
                    mode_origin = ovrl_origin_left_hand;
                if ( (is_generic_tracker_connected) && (ImGui::Selectable(items_origin[ovrl_origin_aux], (mode_origin == ovrl_origin_aux))) )
                    mode_origin = ovrl_origin_aux;

                if (mode_origin != mode_origin_old)
                {
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_detached_origin), mode_origin);

                    if (mode_origin == ovrl_origin_dashboard)
                    {
                        mode_display = ovrl_dispmode_dashboard;
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_detached_display_mode), mode_display);
                    }
                    else if (mode_origin == ovrl_origin_hmd)
                    {
                        mode_display = ovrl_dispmode_scene;
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_detached_display_mode), mode_display);
                    }

                    //Automatically reset the matrix to a saner default if it still has the zero value
                    if (ConfigManager::Get().GetOverlayDetachedTransform().isZero())
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::NextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Position");

            if (!UIManager::Get()->IsOpenVRLoaded())
            {
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::FixedHelpMarker("Position can only be changed or reset when Desktop+ is running");
            }
                
            ImGui::NextColumn();

            if (!UIManager::Get()->IsOpenVRLoaded())
                ImGui::PushItemDisabled();

            bool& is_changing_position = ConfigManager::Get().GetConfigBoolRef(configid_bool_state_overlay_dragmode);

            if (ImGui::Button("Change"))
            {
                ImGui::OpenPopup("OverlayChangePosPopup");

                //Detach overlay if it isn't so it can actually be dragged around
                if (!detached)
                {
                    detached = true;

                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_detached), detached);

                    //Automatically reset the matrix to a saner default if it still has the zero value
                    if (ConfigManager::Get().GetOverlayDetachedTransform().isZero())
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
                    }
                }

                is_changing_position = true;
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragmode), is_changing_position);
            }

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

            if (ImGui::Button("Reset"))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
            }

            if (!UIManager::Get()->IsOpenVRLoaded())
                ImGui::PopItemDisabled();

            ImGui::Columns(1);

            PopupOverlayDetachedPositionChange();
        }

    ImGui::EndChild();
}

void WindowSettings::UpdateCatInterface()
{
    ImGui::Text("Interface");
            
    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewInterfaceSettings");

    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Most interface options don't need to be sent to the dashboard overlay application

    //Desktop Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Desktop Buttons");
        ImGui::Columns(2, "ColumnDesktopButtons", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Listing Style");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1);
        const char* items[] = { "None", "Individual Desktops", "Cycle Buttons" };
        int& button_style = ConfigManager::Get().GetConfigIntRef(configid_int_interface_mainbar_desktop_listing);
        if (ImGui::Combo("##ComboButtonStyle", &button_style, items, IM_ARRAYSIZE(items)))
        {
            UIManager::Get()->RepeatFrame();
        }

        ImGui::NextColumn();

        bool& include_all = ConfigManager::Get().GetConfigBoolRef(configid_bool_interface_mainbar_desktop_include_all);
        if (ImGui::Checkbox("Add Combined Desktop", &include_all))
        {
            UIManager::Get()->RepeatFrame();
        }

        ImGui::Columns(1);
    }

    //Action Buttons
    {
        static int list_selected_pos = -1;

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Action Buttons");

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

        float arrows_width       = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        float column_0_width     = ImGui::GetContentRegionAvail().x - arrows_width;
        float viewbuttons_height = (ImGui::GetFrameHeightWithSpacing() * 7.0f) + (ImGui::GetStyle().ItemSpacing.y * 2.0f);

        ImGui::Columns(2, "ColumnActionButtons", false);
        ImGui::SetColumnWidth(0, column_0_width);
        ImGui::SetColumnWidth(1, arrows_width);

        //ActionButton list
        ImGui::BeginChild("ViewActionButtons", ImVec2(0.0f, viewbuttons_height), true);

        auto& actions = ConfigManager::Get().GetCustomActions();
        auto& action_order = ConfigManager::Get().GetActionMainBarOrder();
        int list_id = 0;
        for (auto& order_data : ConfigManager::Get().GetActionMainBarOrder())
        {
            ActionButtonRow((ActionID)order_data.action_id, list_id, list_selected_pos);
            list_id++;
        }

        ImGui::EndChild();

        //Reduce horizontal spacing a bit so the arrows are closer to the list
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x / 3.0f, ImGui::GetStyle().ItemSpacing.y});

        ImGui::NextColumn();


        //This is a bit of a mess, but centers the buttons vertically, yeah.
        ImGui::Dummy(ImVec2(0.0f, (viewbuttons_height / 2.0f) - ( (ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight()) / 2.0f ) - ImGui::GetStyle().ItemSpacing.y));
            
        int list_selected_pos_pre = list_selected_pos;

        //Up
        if (list_selected_pos_pre <= 0)
            ImGui::PushItemDisabled();

        if (ImGui::ArrowButton("MoveUp", ImGuiDir_Up))
        {
            std::iter_swap(action_order.begin() + list_selected_pos, action_order.begin() + list_selected_pos - 1);
            list_selected_pos--;
        }

        if (list_selected_pos_pre <= 0)
            ImGui::PopItemDisabled();

        //Down
        if ( (list_selected_pos_pre < 0) || (list_selected_pos_pre + 1 == action_order.size()) )
            ImGui::PushItemDisabled();

        if (ImGui::ArrowButton("MoveDown", ImGuiDir_Down))
        {
            std::iter_swap(action_order.begin() + list_selected_pos, action_order.begin() + list_selected_pos + 1);
            list_selected_pos++;
        }

        if ( (list_selected_pos_pre < 0) || (list_selected_pos_pre + 1 == action_order.size()) )
            ImGui::PopItemDisabled();

        ImGui::PopStyleVar();

        ImGui::Columns(1);
    }

    //Windows Mixed Reality
    {
        static bool is_wmr_system = false; //This stuff is only shown to WMR systems

        bool ignore_selection = (ConfigManager::Get().GetConfigInt(configid_int_interface_wmr_ignore_vscreens_selection) == 1);
        bool ignore_combined = (ConfigManager::Get().GetConfigInt(configid_int_interface_wmr_ignore_vscreens_combined_desktop) == 1);

        if (ImGui::IsWindowAppearing())
        {
            if (UIManager::Get()->IsOpenVRLoaded()) //Check if it's a WMR system
            {            
                char buffer[vr::k_unMaxPropertyStringSize];
                vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, buffer, vr::k_unMaxPropertyStringSize);

                is_wmr_system = (strcmp(buffer, "holographic") == 0);
            }
            else if ( (ignore_selection) || (ignore_combined) ) //Assume it's WMR if these settings were changed if there's no way to check for real
            {
                is_wmr_system = true;
            }
        }

        if (is_wmr_system)
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Windows Mixed Reality");
            ImGui::Columns(2, "ColumnInterfaceWMR", false);
            ImGui::SetColumnWidth(0, column_width_0 * 2.0f);

            if (ImGui::Checkbox("Ignore WMR Virtual Desktops for Desktop Buttons", &ignore_selection))
            {
                ConfigManager::Get().SetConfigInt(configid_int_interface_wmr_ignore_vscreens_selection, ignore_selection);
                UIManager::Get()->RepeatFrame();
            }

            if (ImGui::Checkbox("Ignore WMR Virtual Desktops for the Combined Desktop", &ignore_combined))
            {
                ConfigManager::Get().SetConfigInt(configid_int_interface_wmr_ignore_vscreens_combined_desktop, ignore_combined);
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_wmr_ignore_vscreens_combined_desktop), ignore_combined);
            }

            ImGui::Columns(1);
        }
    }

    ImGui::EndChild();
}

void WindowSettings::UpdateCatInput()
{
    ImGui::Text("Input");
            
    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewInputSettings");
    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Active Controller Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Active Controller Buttons");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        /*if (UIManager::Get()->IsOpenVRLoaded()) //OpenBindingUI() is currently broken
        {
            ImGui::FixedHelpMarker("Controller bindings when pointing at the overlay.\nClick here to configure the VR Dashboard controller bindings and change which buttons these are.");

            //Somewhat hidden, but still convenient shortcut to the controller binding page
            if ((UIManager::Get()->IsOpenVRLoaded()) && (ImGui::IsItemClicked()))
            {
                ImGui::OpenPopup("PopupOpenControllerBindingsCompositor");  //OpenPopupOnItemClick() doesn't work with this btw
            }

            if (ImGui::BeginPopup("PopupOpenControllerBindingsCompositor"))
            {
                if (ImGui::Selectable("Open VR Compositor Controller Bindings"))
                {
                    vr::VRInput()->OpenBindingUI("openvr.component.vrcompositor", vr::k_ulInvalidActionSetHandle, vr::k_ulInvalidInputValueHandle, UIManager::Get()->IsInDesktopMode());
                }
                ImGui::EndPopup();
            }
        }
        else*/
        {
            ImGui::FixedHelpMarker("Controller bindings when pointing at the overlay.\nConfigure the VR Compositor controller bindings to change which buttons these are.");
        }

        ActionID actionid_home = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_go_home_action_id);
        ActionID actionid_back = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_go_back_action_id);

        ImGui::Columns(2, "ColumnControllerButtonActions", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Go Home\" Action");
        ImGui::NextColumn();

        if (ButtonAction(actionid_home))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_go_home_action_id, actionid_home);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_go_home_action_id), actionid_home);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Go Back\" Action");
        ImGui::NextColumn();
            
        if (ButtonAction(actionid_back))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_go_back_action_id, actionid_back);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_go_back_action_id), actionid_back);
        }

        ImGui::Columns(1);
    }

    //Global Controller Buttons
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Global Controller Buttons");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        /*if (UIManager::Get()->IsOpenVRLoaded()) //OpenBindingUI() is currently broken
        {
            ImGui::FixedHelpMarker("Controller bindings when the dashboard is closed and not pointing at an overlay.\nClick here to configure the Desktop+ controller bindings and change which buttons these are.");

            //Somewhat hidden, but still convenient shortcut to the controller binding page
            if ((UIManager::Get()->IsOpenVRLoaded()) && (ImGui::IsItemClicked()))
            {
                ImGui::OpenPopup("PopupOpenControllerBindingsDesktopPlus");  //OpenPopupOnItemClick() doesn't work with this btw
            }

            if (ImGui::BeginPopup("PopupOpenControllerBindingsDesktopPlus"))
            {
                if (ImGui::Selectable("Desktop+ Controller Bindings"))
                {
                    vr::VRInput()->OpenBindingUI("elvissteinjr.DesktopPlus", vr::k_ulInvalidActionSetHandle, vr::k_ulInvalidInputValueHandle, UIManager::Get()->IsInDesktopMode());
                }
                ImGui::EndPopup();
            }
        }
        else*/
        {
            ImGui::FixedHelpMarker("Controller bindings when the dashboard is closed and not pointing at an overlay.\nConfigure the Desktop+ controller bindings to change which buttons these are.");
        }

        ActionID actionid_global_01 = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut01_action_id);
        ActionID actionid_global_02 = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut02_action_id);
        ActionID actionid_global_03 = (ActionID)ConfigManager::Get().GetConfigInt(configid_int_input_shortcut03_action_id);

        ImGui::Columns(2, "ColumnControllerButtonGlobalActions", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Global Shortcut 1\" Action");
        ImGui::NextColumn();

        if (ButtonAction(actionid_global_01))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_shortcut01_action_id, actionid_global_01);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_shortcut01_action_id), actionid_global_01);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Global Shortcut 2\" Action");
        ImGui::NextColumn();

        if (ButtonAction(actionid_global_02))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_shortcut02_action_id, actionid_global_02);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_shortcut02_action_id), actionid_global_02);
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("\"Global Shortcut 3\" Action");
        ImGui::NextColumn();

        if (ButtonAction(actionid_global_03))
        {
            ConfigManager::Get().SetConfigInt(configid_int_input_shortcut03_action_id, actionid_global_03);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_shortcut03_action_id), actionid_global_03);
        }

        ImGui::Columns(1);
    }

    //Custom Actions
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Custom Actions");

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);
        ImGui::BeginChild("ViewCustomActions", ImVec2(-ImGui::GetStyle().ItemSpacing.y, ImGui::GetFrameHeight() * 7.0f), true);

        static int list_selected_index = -1;
        static bool delete_confirm_state = false; //Simple uninstrusive extra confirmation step for deleting actions 

        std::vector<CustomAction>& actions = ConfigManager::Get().GetCustomActions();
        int act_index = 0;
        for (CustomAction& action : actions)
        {
            ImGui::PushID(&action);

            if (ImGui::Selectable(action.Name.c_str(), (list_selected_index == act_index)))
            {
                list_selected_index = act_index;
                delete_confirm_state = false;
            }

            if (m_ActionEditIsNew) //Make newly created action visible
            {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::PopID();

            act_index++;
        }

        ImGui::EndChild();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button("New Action"))
        {
            list_selected_index = (int)actions.size();
            delete_confirm_state = false;
                
            CustomAction act;
            act.Name = "New Action";

            actions.push_back(act);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current), actions.size() - 1);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_current_sub), 1);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_state_action_value_int), (int)act.FunctionType);

            ConfigManager::Get().GetActionMainBarOrder().push_back({ (ActionID)(actions.size() - 1 + action_custom), false });

            m_ActionEditIsNew = true;

            ImGui::OpenPopup("ActionEditPopup");
        }

        ImGui::SameLine();

        bool buttons_disabled = (list_selected_index == -1); //State can change in-between

        if (buttons_disabled)
            ImGui::PushItemDisabled();

        if (UIManager::Get()->IsOpenVRLoaded())
        {
            if (ImGui::Button("Do"))
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_do, action_custom + list_selected_index);
                delete_confirm_state = false;
            }

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }


        if (ImGui::Button("Edit"))
        {
            m_ActionEditIsNew = false;
            ImGui::OpenPopup("ActionEditPopup");
            delete_confirm_state = false;
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (delete_confirm_state)
        {
            if (ImGui::Button("Really?"))
            {
                ActionManager::Get().EraseCustomAction(list_selected_index);
                UIManager::Get()->RepeatFrame();

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_delete, list_selected_index);

                list_selected_index = -1;

                delete_confirm_state = false;
            }
        }
        else
        {
            if (ImGui::Button("Delete"))
            {
                delete_confirm_state = true;
            }
        }

        if (buttons_disabled)
            ImGui::PopItemDisabled();

        //
        if ( (list_selected_index != -1) && (actions.size() > list_selected_index) ) //If actually exists
        {
            PopupActionEdit(actions[list_selected_index], list_selected_index);

            if (actions.size() <= list_selected_index) //New Action got deleted by the popup, reset selection
            {
                list_selected_index = -1;
            }
        }

    }

    //Mouse
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Mouse");
        ImGui::Columns(2, "ColumnMouse", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& render_cursor = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_mouse_render_cursor);
        if (ImGui::Checkbox("Render Cursor", &render_cursor))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_mouse_render_cursor), render_cursor);
        }

        bool& render_blob = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_mouse_render_intersection_blob);
        if (ImGui::Checkbox("Render Intersection Blob", &render_blob))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_mouse_render_intersection_blob), render_blob);
        }

        bool& pointer_override = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_mouse_hmd_pointer_override);
        if (ImGui::Checkbox("Allow HMD-Pointer Override", &pointer_override))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_mouse_hmd_pointer_override), pointer_override);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Disables the laser pointer when the physical mouse is moved rapidly after the dashboard was opened with the HMD button.\nRe-open or click the overlay to get the laser pointer back.");

        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Double-Click Assistant"); 
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Freezes the mouse cursor for the set duration to ease the input of double-clicks");

        ImGui::NextColumn();

        //The way mapping max + 1 == -1 value into the slider is done is a bit convoluted again, but still works
        int& assist_duration = ConfigManager::Get().GetConfigIntRef(configid_int_input_mouse_dbl_click_assist_duration_ms);
        int assist_duration_max = 3000; //The "Auto" wrapping makes this the absolute maximum value even with manual input, but longer than 3 seconds is questionable either way
        int assist_duration_ui = (assist_duration == -1) ? assist_duration_max + 1 : assist_duration;

        if (ImGui::SliderWithButtonsInt("DBLClickAssist", assist_duration_ui, 25, 0, assist_duration_max + 1, (assist_duration == -1) ? "Auto" : (assist_duration == 0) ? "Off" : "%d ms"))
        {
            assist_duration = clamp(assist_duration_ui, 0, assist_duration_max + 1);

            if (assist_duration_ui > assist_duration_max)
                assist_duration = -1;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_input_mouse_dbl_click_assist_duration_ms), assist_duration);
        }

        ImGui::Columns(1);
    }

    //Keyboard
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Keyboard");
        ImGui::Columns(2, "ColumnKeyboard", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& enable_keyboard_helper = ConfigManager::Get().GetConfigBoolRef(configid_bool_input_keyboard_helper_enabled);
        if (ImGui::Checkbox("Enable Keyboard Extension", &enable_keyboard_helper))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_keyboard_helper_enabled), enable_keyboard_helper);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Detached Size");
        ImGui::NextColumn();

        //This maps the float limit as int percentage, see the cropping stuff for the rest
        float& size = ConfigManager::Get().GetConfigFloatRef(configid_float_input_keyboard_detached_size);

        if (ImGui::SliderWithButtonsFloatPercentage("KeyboardSize", size, 5, 10, 100, "%d%%"))
        {
            if (size < 0.10f)
                size = 0.10f;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_input_keyboard_detached_size), *(LPARAM*)&size);

            //If the change would be visible, apply it directly
            if ( (ConfigManager::Get().GetConfigBool(configid_bool_state_keyboard_visible_for_dashboard)) && (ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached)) )
            {
                vr::VROverlayHandle_t ovrl_handle_keyboard = vr::k_ulOverlayHandleInvalid;
                vr::VROverlay()->FindOverlay("system.keyboard", &ovrl_handle_keyboard);

                if (ovrl_handle_keyboard != vr::k_ulOverlayHandleInvalid)
                {
                    //vr::VROverlay()->SetOverlaysizeInMeters(ovrl_handle_keyboard, ConfigManager::Get().GetConfigFloat(configid_float_input_keyboard_detached_size));
                }
            }
        }
        ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::EndChild();
}

void WindowSettings::UpdateCatPerformance()
{
    ImGui::Text("Performance");
            
    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewPerformanceSettings");
    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Early Updates
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Early Updates");

        ImGui::Columns(2, "ColumnEarlyUpdates", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& ignore_early_updates = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_ignore_early_updates);
        if (ImGui::Checkbox("Ignore Early Updates", &ignore_early_updates))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_performance_ignore_early_updates), ignore_early_updates);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Ignore duplication updates coming faster than the desktop refreshes (i.e. from cursor movement).\nReduces GPU load, but adds latency.");

        ImGui::NextColumn();
        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Update Limit");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Percentage of frame time considered to be too early when early duplication updates are ignored.\nHigh values can be used to limit the frame rate.");
        ImGui::NextColumn();

        if (!ignore_early_updates)
            ImGui::PushItemDisabled();

        //This maps the float limit as int percentage, see the cropping stuff for the rest
        float& update_limit = ConfigManager::Get().GetConfigFloatRef(configid_float_performance_early_update_limit_multiplier);

        if (ImGui::SliderWithButtonsFloatPercentage("EarlyUpdateLimit", update_limit, 5, 0, 100, "%d%%"))
        {
            if (update_limit < 0.0f)
                update_limit = 0.0f;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_performance_early_update_limit_multiplier), *(LPARAM*)&update_limit);
        }

        if (!ignore_early_updates)
            ImGui::PopItemDisabled();

        ImGui::Columns(1);
    }

    //Misc Performance Stuff... there just isn't much and the category shouldn't be exactly the same as the single item in it
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Misc");

        ImGui::Columns(2, "ColumnPerformanceMisc", false);
        ImGui::SetColumnWidth(0, column_width_0);

        bool& rapid_updates = ConfigManager::Get().GetConfigBoolRef(configid_bool_performance_rapid_laser_pointer_updates);
        if (ImGui::Checkbox("Rapid Laser Pointer Updates", &rapid_updates))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_performance_rapid_laser_pointer_updates), rapid_updates);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Burn additional CPU cycles to make the laser pointer cursor as accurate as possible.\nOnly affects CPU load when pointing at the overlay.");

        ImGui::Columns(1);
    }

    //Stats
    if (UIManager::Get()->IsOpenVRLoaded())
    {
        //Get compositor timing from OpenVR
        vr::Compositor_FrameTiming frame_timing;
        frame_timing.m_nSize = sizeof(vr::Compositor_FrameTiming);
        bool frame_timing_valid = vr::VRCompositor()->GetFrameTiming(&frame_timing, 0);

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Statistics");

        ImGui::Columns(2, "ColumnPerformanceStats", false);
        ImGui::SetColumnWidth(0, column_width_0);

        ImGui::Text("VR Compositor CPU Frame Time: ");
        ImGui::NextColumn();

        if (frame_timing_valid)
            ImGui::Text("%.2f ms", frame_timing.m_flCompositorRenderCpuMs);
        else
            ImGui::Text("?");


        ImGui::NextColumn();
        ImGui::Text("VR Compositor GPU Frame Time: ");
        ImGui::NextColumn();

        if (frame_timing_valid)
            ImGui::Text("%.2f ms", frame_timing.m_flCompositorRenderGpuMs);
        else
            ImGui::Text("?");

        ImGui::NextColumn();
        ImGui::Text("Desktop Duplication Update Rate: ");

        ImGui::NextColumn();

        ImGui::Text("%d fps", ConfigManager::Get().GetConfigInt(configid_int_state_performance_duplication_fps));
        ImGui::NextColumn();

        ImGui::Text("Cross-GPU Copy Active: ");
        ImGui::NextColumn();

        ImGui::Text((ConfigManager::Get().GetConfigBool(configid_bool_state_performance_gpu_copy_active)) ? "Yes" : "No");
        ImGui::NextColumn();
    }

    ImGui::EndChild();
}

void WindowSettings::UpdateCatMisc()
{
    ImGui::Text("Misc");
            
    //Horizontal separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("hsep", ImVec2(0.0f, 1.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ViewMiscSettings");
    const float column_width_0 = ImGui::GetFontSize() * 15.0f;

    //Version Info
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Version Info");

        ImGui::Columns(2, "ColumnVersionInfo", false);
        ImGui::SetColumnWidth(0, column_width_0 * 2.0f);

        ImGui::Text("Desktop+ Version 2.0.1");

        ImGui::Columns(1);
    }

    //Warnings
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Warnings");

        ImGui::Columns(2, "ColumnResetWarnings", false);
        ImGui::SetColumnWidth(0, column_width_0);

        int warning_hidden_count = 0;

        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_warning_compositor_quality_hidden))
            warning_hidden_count++;
        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_warning_compositor_res_hidden))
            warning_hidden_count++;
        if (ConfigManager::Get().GetConfigBool(configid_bool_interface_warning_process_elevation_hidden))
            warning_hidden_count++;

        ImGui::Text("Warnings Hidden: %i", warning_hidden_count);

        if (ImGui::Button("Reset Hidden Warnings"))
        {
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_compositor_quality_hidden, false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_compositor_res_hidden, false);
            ConfigManager::Get().SetConfigBool(configid_bool_interface_warning_process_elevation_hidden, false);
        }

        ImGui::Columns(1);
    }

    //Troubleshooting
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Troubleshooting");

        ImGui::Columns(2, "ColumnTroubleshooting", false);
        ImGui::SetColumnWidth(0, column_width_0);

        //All the restart buttons only start up new processes, but both UI and dashboard app get rid of the older instance when starting

        ImGui::Text("Desktop+");
        ImGui::NextColumn();

        if (ImGui::Button("Restart"))
        {
            ConfigManager::Get().SaveConfigToFile();

            STARTUPINFO si = {0};
            PROCESS_INFORMATION pi = {0};
            si.cb = sizeof(si);

            ::CreateProcess(L"DesktopPlus.exe", nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

            //We don't care about these, so close right away
            ::CloseHandle(pi.hProcess);
            ::CloseHandle(pi.hThread);
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (UIManager::Get()->IsElevatedTaskSetUp())
        {
            if (ImGui::Button("Restart Elevated"))
            {
                ConfigManager::Get().SaveConfigToFile();

                STARTUPINFO si = {0};
                PROCESS_INFORMATION pi = {0};
                si.cb = sizeof(si);

                WCHAR cmd[] = L"\"schtasks\" /Run /TN \"DesktopPlus Elevated\""; //"CreateProcessW, can modify the contents of this string", so don't go optimize this away

                ::CreateProcess(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

                //We don't care about these, so close right away
                ::CloseHandle(pi.hProcess);
                ::CloseHandle(pi.hThread);
            }
        }

        ImGui::NextColumn();

        ImGui::Text("Desktop+ UI");
        ImGui::NextColumn();

        if (ImGui::Button("Restart##UI"))
        {
            ConfigManager::Get().SaveConfigToFile();

            STARTUPINFO si = {0};
            PROCESS_INFORMATION pi = {0};
            si.cb = sizeof(si);

            ::CreateProcess(L"DesktopPlusUI.exe", nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

            //We don't care about these, so close right away
            ::CloseHandle(pi.hProcess);
            ::CloseHandle(pi.hThread);
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        if (ImGui::Button("Restart in Desktop Mode"))
        {
            ConfigManager::Get().SaveConfigToFile();

            STARTUPINFO si = {0};
            PROCESS_INFORMATION pi = {0};
            si.cb = sizeof(si);

            WCHAR cmd[] = L"-DesktopMode";

            ::CreateProcess(L"DesktopPlusUI.exe", cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

            //We don't care about these, so close right away
            ::CloseHandle(pi.hProcess);
            ::CloseHandle(pi.hThread);
        }

        ImGui::Columns(1);
    }

    ImGui::EndChild();
}

bool WindowSettings::ButtonKeybind(unsigned char* key_code)
{
    //ID hierarchy prevents properly opening the popups directly from within the button popup, so this is a workaround
    static bool open_bind_popup = false, open_list_popup = false;
 
    ImGui::PushID(key_code);

    ImGui::PushID("KeycodeBindButton");
    if (ImGui::Button(GetStringForKeyCode(*key_code)))
    {
        ImGui::PopID();

        if (UIManager::Get()->IsInDesktopMode())
        {
            ImGui::OpenPopup("KeycodeButtonPopup");
        }
        else
        {
            open_list_popup = true;
        }
    }
    else
    {
        ImGui::PopID();
    }

    if (ImGui::BeginPopup("KeycodeButtonPopup"))
    {
        if (ImGui::Selectable("Set from Input..."))
        {
            open_bind_popup = true;
        }

        if (ImGui::Selectable("Set from List..."))
        {
            open_list_popup = true;
        }

        ImGui::EndPopup();
    }
        
    if (open_bind_popup)
    {
        ImGui::OpenPopup("Bind Key");
        open_bind_popup = false;
    }

    if (open_list_popup)
    {
        ImGui::OpenPopup("Select Key");
        open_list_popup = false;
    }

    if (ImGui::BeginPopupModal("Bind Key", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::Text("Press any key or mouse button...");

        ImGuiIO& io = ImGui::GetIO();
        static bool close_later = false;

        if (close_later) //Workaround for ImGui bug, see https://github.com/ocornut/imgui/issues/2880
        {
            ImGui::CloseCurrentPopup();
            close_later = false;
        }

        for (int i = 0; i < 5; ++i)
        {
            if (ImGui::IsMouseClicked(i, false)) //Checking io.MouseClicked would do the same, but let's use the thing that is not marked [Internal] here
            {
                switch (i) //Virtual key code for mouse is unfortunately not just VK_LBUTTON + i
                {
                    case 0: *key_code = VK_LBUTTON;  break;
                    case 1: *key_code = VK_RBUTTON;  break;
                    case 2: *key_code = VK_MBUTTON;  break;
                    case 3: *key_code = VK_XBUTTON1; break;
                    case 4: *key_code = VK_XBUTTON2; break;
                }
                //ImGui::CloseCurrentPopup();
                close_later = true;
                break;
            }
        }

        for (int i = 0; i < IM_ARRAYSIZE(io.KeysDown); ++i)
        {
            if ( (io.KeysDown[i]) && (io.KeysDownDuration[i] == 0.0f) )
            {
                *key_code = i;
                ImGui::CloseCurrentPopup();
                break;
            }
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Select Key", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::SetWindowSize(ImVec2(GetSize().x * 0.5f, GetSize().y * 0.75f));

        static ImGuiTextFilter filter;
        static int list_id = 0;

        if (ImGui::IsWindowAppearing())
        {
            for (int i = 0; i < 256; i++)
            {
                //Not the smartest, but most straight forward way
                if (GetKeyCodeForListID(i) == *key_code)
                {
                    list_id = i;
                    break;
                }
            }
        }

        ImGui::Text("Select Key Code");

        ImGui::SetNextItemWidth(-1.0f);

        if (ImGui::InputTextWithHint("", "Filter List", filter.InputBuf, IM_ARRAYSIZE(filter.InputBuf)))
        {
            filter.Build();
        }

        ImGui::BeginChild("KeyList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        for (int i = 0; i < 256; i++)
        {
            if (filter.PassFilter( GetStringForKeyCode(GetKeyCodeForListID(i))) )
            {
                if (ImGui::Selectable( GetStringForKeyCode(GetKeyCodeForListID(i)), (i == list_id)))
                {
                    list_id = i;
                }
            }
        }

        ImGui::EndChild();

        if (ImGui::Button("Ok")) 
        {
            *key_code = GetKeyCodeForListID(list_id);
            ImGui::CloseCurrentPopup();
        }
            
        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopID();

    return false;
}

bool WindowSettings::ButtonAction(ActionID& action_id)
{
    bool result = false;

    ImGui::PushID(&action_id);

    if (ImGui::Button(ActionManager::Get().GetActionName(action_id)))
    {
        ImGui::OpenPopup("Select Action");
    }

    if (ImGui::BeginPopupModal("Select Action", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::SetWindowSize(ImVec2(GetSize().x * 0.5f, GetSize().y * 0.75f));

        static ActionID list_id = action_none;

        if (ImGui::IsWindowAppearing())
        {
            list_id = action_id;
        }

        ImGui::Text("Select Action");

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::BeginChild("ActionList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), true);

        //List default actions
        for (int i = 0; i < action_built_in_MAX; ++i)
        {
            if (ImGui::Selectable(ActionManager::Get().GetActionName((ActionID)i), (i == list_id)))
            {
                list_id = (ActionID)i;
            }
        }

        //List custom actions
        int act_index = 0;
        for (CustomAction& action : ConfigManager::Get().GetCustomActions())
        {
            ImGui::PushID(&action);
            if (ImGui::Selectable(action.Name.c_str(), (act_index + action_custom == list_id)))
            {
                list_id = (ActionID)(act_index + action_custom);
            }
            ImGui::PopID();

            act_index++;
        }

        ImGui::EndChild();

        if (ImGui::Button("Ok")) 
        {
            action_id = list_id;
            UIManager::Get()->RepeatFrame();
            ImGui::CloseCurrentPopup();
            result = true;
        }
            
        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopID();

    return result;
}

bool WindowSettings::ActionButtonRow(ActionID action_id, int list_pos, int& list_selected_pos)
{
    auto& action_order = ConfigManager::Get().GetActionMainBarOrder();
    bool delete_pressed = false;

    static float column_width_1 = 0.0f;
    const float column_width_0 = ImGui::GetStyle().ItemSpacing.x + ImGui::GetContentRegionAvail().x - column_width_1;

    ImGui::PushID(action_id);
    ImGui::PushID(ActionManager::Get().GetActionName(action_id));

    ImGui::Columns(2, "ColumnActionRow", false);

    ImGui::SetColumnWidth(0, column_width_0);
    ImGui::SetColumnWidth(1, column_width_1);

    column_width_1 = ImGui::GetStyle().ItemInnerSpacing.x;

    ImGui::AlignTextToFramePadding();
    if (ImGui::Checkbox("##VisibleCheckbox", &action_order[list_pos].visible))
    {
        UIManager::Get()->RepeatFrame();
    }
    ImGui::SameLine();

    if (ImGui::Selectable(ActionManager::Get().GetActionName(action_id), (list_selected_pos == list_pos)))
    {
        list_selected_pos = list_pos;
    }

    ImGui::NextColumn();

    ImGui::PopID();
    ImGui::PopID();

    ImGui::Columns(1);

    return delete_pressed;
}

void WindowSettings::PopupActionEdit(CustomAction& action, int id)
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(GetSize().x * 0.5f, -1),  ImVec2(GetSize().x * 0.5f, -1));
    if (ImGui::BeginPopupModal("ActionEditPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        //Working with fixed sized char arrays for input fields makes this a bit simpler
        static char buf_name[1024] = "";
        static std::string str_icon_file;
        static bool use_action_icon = true;   //Icon to use for the preview button. Switches to tmtex_icon_temp when the icon was changed
        static int action_function = caction_press_keys;
        static unsigned char keycodes[3] = {0};
        static char buf_type_str[1024] = "";
        static char buf_exe_path[1024] = "";
        static char buf_exe_arg[1024] = "";

        if (ImGui::IsWindowAppearing())
        {
            //Load data from action
            size_t copied_length = action.Name.copy(buf_name, 1023);
            buf_name[copied_length] = '\0';
            action_function = action.FunctionType;

            keycodes[0] = 0;
            keycodes[1] = 0;
            keycodes[2] = 0;
            buf_type_str[0] = '\0';
            buf_exe_path[0] = '\0';
            buf_exe_arg[0] = '\0';

            switch (action_function)
            {
                case caction_press_keys:
                {
                    keycodes[0] = action.KeyCodes[0];
                    keycodes[1] = action.KeyCodes[1];
                    keycodes[2] = action.KeyCodes[2];
                    break;
                }
                case caction_type_string:
                {
                    copied_length = action.StrMain.copy(buf_type_str, 1023);
                    buf_type_str[copied_length] = '\0';
                    break;
                }
                case caction_launch_application:
                {
                    copied_length = action.StrMain.copy(buf_exe_path, 1023);
                    buf_exe_path[copied_length] = '\0';
                    copied_length = action.StrArg.copy(buf_exe_arg, 1023);
                    buf_exe_arg[copied_length] = '\0';
                    break;
                }
            }

            str_icon_file = action.IconFilename;
            use_action_icon = true;
        }

        bool do_save = false;

        const float column_width_0 = ImGui::GetFontSize() * 10.0f;

        ImGui::Columns(2, "ColumnCustomAction", false);
        ImGui::SetColumnWidth(0, column_width_0);


        ImGui::AlignTextToFramePadding();
        ImGui::Text("Name");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1.0f);

        if (ImGui::InputText("##Name", buf_name, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            do_save = true;
        }

        ImGui::PopupContextMenuInputText(nullptr, buf_name, 1024);

        ImGui::NextColumn();

        //Button Appearance stuff
        ImVec2 b_size, b_uv_min, b_uv_max;
        TextureManager::Get().GetTextureInfo(tmtex_icon_settings, b_size, b_uv_min, b_uv_max);
        //Default button size for custom actions
        ImVec2 b_size_default = b_size;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Button Appearance");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1.0f);

        if (str_icon_file.empty()) //No icon
        {
            //Adapt to the last known scale used in VR so the text alignment matches what's seen in the headset later
            if (UIManager::Get()->IsInDesktopMode())
            {
                b_size_default.x *= UIManager::Get()->GetUIScale();
                b_size_default.y *= UIManager::Get()->GetUIScale();
                b_size_default.x *= ConfigManager::Get().GetConfigFloat(configid_float_interface_last_vr_ui_scale);
                b_size_default.y *= ConfigManager::Get().GetConfigFloat(configid_float_interface_last_vr_ui_scale);
            }

            if (ImGui::ButtonWithWrappedLabel(buf_name, b_size_default))
            {
                ImGui::OpenPopup("Select Icon");
            }
        }
        else
        {
            if (use_action_icon)
            {
                TextureManager::Get().GetTextureInfo(action, b_size, b_uv_min, b_uv_max);
            }
            else
            {
                TextureManager::Get().GetTextureInfo(tmtex_icon_temp, b_size, b_uv_min, b_uv_max);
            }
            
            if (ImGui::ImageButton(ImGui::GetIO().Fonts->TexID, b_size_default, b_uv_min, b_uv_max, -1, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)))
            {
                ImGui::OpenPopup("Select Icon");
            }
        }

        if (PopupIconSelect(str_icon_file)) //True if icon was changed
        {
            if (!str_icon_file.empty())
            {
                TextureManager::Get().SetTextureFilenameIconTemp(WStringConvertFromUTF8(str_icon_file.c_str()).c_str());
                TextureManager::Get().ReloadAllTexturesLater(); //Might be considering excessive, but the loading is pretty fast
                use_action_icon = false;
            }
        }

        ImGui::NextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Function");
        ImGui::NextColumn();

        ImGui::SetNextItemWidth(-1.0f);

        const char* f_items[] = {"Press Keys", "Type String", "Launch Application"};
        ImGui::Combo("##ComboFunction", &action_function, f_items, IM_ARRAYSIZE(f_items));

        ImGui::Columns(1);
        ImGui::Separator();

        ImGui::Columns(2, "ColumnCustomActionF", false);
        ImGui::SetColumnWidth(0, column_width_0);

        if (action_function == caction_press_keys)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Key Code 1");
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("Desktop+ uses virtual key codes to simulate input.\nThe meaning of some of them depend on the used keyboard layout.\nWhen Desktop+UI is launched in desktop mode, the key code can also be directly set from user input.");
            ImGui::NextColumn();

            ButtonKeybind(&keycodes[0]);
            ImGui::NextColumn();
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Key Code 2");
            ImGui::NextColumn();

            ButtonKeybind(&keycodes[1]);
            ImGui::NextColumn();
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Key Code 3");
            ImGui::NextColumn();

            ButtonKeybind(&keycodes[2]);
        }
        else if (action_function == caction_type_string)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Typed String");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1.0f);
            
            if (ImGui::InputText("##TypeString", buf_type_str, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                do_save = true;
            }

            ImGui::PopupContextMenuInputText(nullptr, buf_type_str, 1024);
        }
        else if (action_function == caction_launch_application)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Executable Path");
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("This can also be a normal file or URL");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1.0f);

            if (ImGui::InputText("##ExePath", buf_exe_path, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                do_save = true;
            }

            ImGui::PopupContextMenuInputText(nullptr, buf_exe_path, 1024);

            ImGui::NextColumn();
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Application Arguments");
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::FixedHelpMarker("These are passed to the launched application.\nIf unsure, leave this blank.");
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1.0f);

            if (ImGui::InputText("##ExeArg", buf_exe_arg, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                do_save = true;
            }

            ImGui::PopupContextMenuInputText(nullptr, buf_exe_arg, 1024);
        }

        ImGui::Columns(1);

        ImGui::Separator();

        if (ImGui::Button("Ok"))
        {
            do_save = true;
        }

        if (do_save)
        {
            std::string name_new = buf_name;

            //We clear unrelated fields to avoid old data to appear when editing at another time
            action = CustomAction();
            action.FunctionType = (CustomActionFunctionID)action_function;
            action.Name = buf_name;

            switch (action_function)
            {
                case caction_press_keys:
                {
                    action.KeyCodes[0] = keycodes[0];
                    action.KeyCodes[1] = keycodes[1];
                    action.KeyCodes[2] = keycodes[2];
                    break;
                }
                case caction_type_string:
                {
                    action.StrMain = buf_type_str;
                    break;
                }
                case caction_launch_application:
                {
                    action.StrMain = buf_exe_path;
                    action.StrArg = buf_exe_arg;
                    break;
                }
            }

            action.IconFilename = str_icon_file;

            //Reload textures later in case the icon has changed or a previously unloaded character is part of the name now
            TextureManager::Get().ReloadAllTexturesLater();

            action.SendUpdateToDashboardApp(id, UIManager::Get()->GetWindowHandle());

            ImGui::CloseCurrentPopup();
        }
            
        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            if (m_ActionEditIsNew) //Canceling a new Action should remove it again
            {
                ActionManager::Get().EraseCustomAction(id);
                UIManager::Get()->RepeatFrame();

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_action_delete, id);

                m_ActionEditIsNew = false;
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void WindowSettings::PopupOverlayDetachedPositionChange()
{
    ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
    const float column_width_0 = ImGui::GetFontSize() * 10.0f;
    const float column_width_1 = ImGui::GetFrameHeightWithSpacing() * 2.0f + style.ItemInnerSpacing.x;
    const float column_width_2 = column_width_0 * 0.5f;
    const float popup_width = column_width_0 + (column_width_1 * 2.0f) + column_width_2 + (ImGui::GetStyle().ItemSpacing.x * 2.0f);

    ImGui::SetNextWindowSizeConstraints(ImVec2(popup_width, -1),  ImVec2(popup_width, -1));
    if (ImGui::BeginPopupModal("OverlayChangePosPopup", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        bool dragging_enabled = true;
        //Adding another shared setting state would more efficient, but this should be alright as it's just this popup
        vr::VROverlayHandle_t ovrl_handle_dplus;    
        vr::VROverlay()->FindOverlay("elvissteinjr.DesktopPlus", &ovrl_handle_dplus);

        if (ovrl_handle_dplus != vr::k_ulOverlayHandleInvalid)
        {
            vr::VROverlayInputMethod method = vr::VROverlayInputMethod_None;
            vr::VROverlay()->GetOverlayInputMethod(ovrl_handle_dplus, &method);

            dragging_enabled = (method != vr::VROverlayInputMethod_None);

            //Allow restoring drag mode with right click anywhere on the UI overlay
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                vr::VROverlay()->SetOverlayInputMethod(ovrl_handle_dplus, (method == vr::VROverlayInputMethod_None) ? vr::VROverlayInputMethod_Mouse : vr::VROverlayInputMethod_None);
            }
        }

        if (dragging_enabled)
        {
            ImGui::Text("Drag the overlay around to change its position.");
            ImGui::Text("Use right click to disable dragging.");
        }
        else
        {
            ImGui::TextDisabled("Drag the overlay around to change its position.");
            ImGui::Text("Right click here to re-enable dragging.");
        }

        ImGui::Separator();

        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "Manual Adjustment");
        ImGui::Columns(4, "ColumnManualAdjust", false);

        ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
        ImGui::SetColumnWidth(0, column_width_0);
        ImGui::SetColumnWidth(1, column_width_1);
        ImGui::SetColumnWidth(2, column_width_2);
        ImGui::SetColumnWidth(3, column_width_1);

        ImGui::PushButtonRepeat(true);

        ImGui::PushID("Up/Down");
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Move Up/Down");
        ImGui::NextColumn();

        if (ImGui::Button("-", button_size))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_updown);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("+", button_size))
        {
            //Do some packing
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;  //Increase bit
            packed_value |= ipcactv_ovrl_pos_adjust_updown;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::PopID();
        ImGui::NextColumn();

                        
        ImGui::PushID("RotX");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Rotate X");
        ImGui::NextColumn();

        if (ImGui::Button("-", button_size))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_rotx);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("+", button_size))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_rotx;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::PushID("Right/Left");
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Move Right/Left");
        ImGui::NextColumn();

        if (ImGui::Button("-", button_size))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_rightleft);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("+", button_size))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_rightleft;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::PopID();
        ImGui::NextColumn();

                        
        ImGui::PushID("RotY");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Rotate Y");
        ImGui::NextColumn();

        if (ImGui::Button("-", button_size))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_roty);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("+", button_size))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_roty;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::PushID("Forward/Backward");
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Move Forward/Backward");
        ImGui::NextColumn();

        if (ImGui::Button("-", button_size))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_forwback);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("+", button_size))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_forwback;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::PopID();
        ImGui::NextColumn();

                        
        ImGui::PushID("RotZ");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Rotate Z");
        ImGui::NextColumn();

        if (ImGui::Button("-", button_size))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_rotz);
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        if (ImGui::Button("+", button_size))
        {
            unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
            packed_value |= ipcactv_ovrl_pos_adjust_rotz;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
        }

        ImGui::PopID();
        ImGui::PopButtonRepeat();
                        
        ImGui::Columns(1);
        ImGui::Separator();

        if ( (ImGui::Button("Done")) || 
             ( (!UIManager::Get()->IsInDesktopMode()) && (!vr::VROverlay()->IsOverlayVisible(UIManager::Get()->GetOverlayHandle())) ) ) //Will auto-dismiss when UI overlay not active
        {
            bool& is_changing_position = ConfigManager::Get().GetConfigBoolRef(configid_bool_state_overlay_dragmode);

            is_changing_position = false;
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragmode), is_changing_position);

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

bool WindowSettings::PopupIconSelect(std::string& filename)
{
    bool ret = false;

    if (ImGui::BeginPopupModal("Select Icon", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::SetWindowSize(ImVec2(GetSize().x * 0.5f, GetSize().y * 0.75f));

        static int list_id = 0;
        static std::vector<std::string> list_files;

        if (ImGui::IsWindowAppearing())
        {
            list_files.clear();
            list_files.emplace_back("[Text Label]");

            WIN32_FIND_DATA find_data;
            HANDLE handle_find = FindFirstFileW(L"images/icons/*.png", &find_data);

            if (handle_find != INVALID_HANDLE_VALUE)
            {
                do
                {
                    list_files.push_back(StringConvertFromUTF16(find_data.cFileName));
                }
                while (FindNextFile(handle_find, &find_data) != 0);

                FindClose(handle_find);
            }
        }

        ImGui::Text("Select Icon");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::FixedHelpMarker("Custom icons can be added as PNG files in the \"images\\icons\" directory");

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::BeginChild("IconList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), true);

        //List files
        int index = 0;
        for (const auto& str: list_files)
        {
            ImGui::PushID(&str);
            if (ImGui::Selectable(str.c_str(), (index == list_id)))
            {
                list_id = index;
            }
            ImGui::PopID();

            index++;
        }

        ImGui::EndChild();

        if (ImGui::Button("Ok")) 
        {
            if (list_id == 0)
            {
                filename = "";
            }
            else
            {
                filename = "images/icons/" + list_files[list_id];
            }

            UIManager::Get()->RepeatFrame();
            ImGui::CloseCurrentPopup();

            ret = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) 
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return ret;
}

WindowSettings::WindowSettings() : m_Visible(false), m_Alpha(0.0f), m_ActionEditIsNew(false)
{
    UIManager::Get()->UpdateOverlayPixelSize();

    m_Size.x = OVERLAY_WIDTH * UIManager::Get()->GetUIScale();

    if (UIManager::Get()->IsInDesktopMode())    //Act as a "fullscreen" window if in desktop mode
        m_Size.y = ImGui::GetIO().DisplaySize.y;
    else
        m_Size.y = ImGui::GetIO().DisplaySize.y * 0.83f;
    
}

void WindowSettings::Show()
{
    m_Visible = true;
    UIManager::Get()->UpdateOverlayPixelSize(); //Make sure we still have the correct size to work with
    UIManager::Get()->UpdateCompositorRenderQualityLow();

    //Adjust sort order when settings window is visible. This will still result in weird visuals with other overlays when active, but at least not constantly.
    //It's a compromise, really. The other reliable method would be about 1m distance between the two overlays, which is not happening
    if ( (!UIManager::Get()->IsInDesktopMode()) && (!ConfigManager::Get().GetConfigBool(configid_bool_overlay_detached)) )
    {
        vr::VROverlay()->SetOverlaySortOrder(UIManager::Get()->GetOverlayHandle(), 1);
    }
}

void WindowSettings::Hide()
{
    m_Visible = false;
    ConfigManager::Get().SaveConfigToFile();
}

void WindowSettings::Update()
{  
    if (UIManager::Get()->IsInDesktopMode())    //In desktop mode it's the only thing displayed, so no transition
    {
        m_Alpha = 1.0f;
        m_Visible = true;
    }
    else if ( (m_Alpha != 0.0f) || (m_Visible) )
    {
        //Alpha fade animation
        m_Alpha += (m_Visible) ? 0.1f : -0.1f;

        if (m_Alpha > 1.0f)
            m_Alpha = 1.0f;
        else if (m_Alpha < 0.0f)
            m_Alpha = 0.0f;

        //Undo sort order once the settings window fully disappeared, unless the overlay is being hovered still
        if ( (!m_Visible) && (m_Alpha == 0.0f) && (!vr::VROverlay()->IsHoverTargetOverlay(UIManager::Get()->GetOverlayHandle())) )
        {
            vr::VROverlay()->SetOverlaySortOrder(UIManager::Get()->GetOverlayHandle(), 0);
        }
    }

    if (m_Alpha == 0.0f)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Alpha);

    if (UIManager::Get()->IsInDesktopMode())    //Act as a "fullscreen" window if in desktop mode
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(m_Size);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    if (!m_Visible)
        flags |= ImGuiWindowFlags_NoInputs;

    ImGui::Begin("WindowSettings", nullptr, flags);

    //Early pop as we have popups which should have a normal border
    if (UIManager::Get()->IsInDesktopMode())
        ImGui::PopStyleVar(); //ImGuiStyleVar_WindowBorderSize

    //Left
    static int selected = 0;
        ImGui::BeginChild("left pane", ImVec2(ImGui::GetFontSize() * 5, 0), false);

            ImGui::Dummy(ImVec2(ImGui::GetFontSize() * 5, (m_Size.y / 2.0f) - (ImGui::GetFrameHeightWithSpacing()*2.5f) ));

            if (ImGui::Selectable("Overlay", selected == 0))
                selected = 0;
            if (ImGui::Selectable("Interface", selected == 1))
                selected = 1;
            if (ImGui::Selectable("Input", selected == 2))
                selected = 2;
            if (ImGui::Selectable("Performance", selected == 3))
                selected = 3;
            if (ImGui::Selectable("Misc", selected == 4))
                selected = 4;
            /*if (ImGui::Selectable("Debug", selected == 5))
                selected = 5;*/

        ImGui::EndChild();
    ImGui::SameLine();

    //Vertical separator
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::BeginChild("vsep", ImVec2(1.0f, 0.0f), true);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    //Right
    ImGui::BeginGroup();

        UpdateWarnings();

        switch (selected)
        {
            case 0: UpdateCatOverlay();     break;
            case 1: UpdateCatInterface();   break;
            case 2: UpdateCatInput();       break;
            case 3: UpdateCatPerformance(); break;
            case 4: UpdateCatMisc();        break;
        }

    ImGui::EndGroup();

    ImGui::End();
    ImGui::PopStyleVar(); //ImGuiStyleVar_Alpha

    //Toggle performance stats based on the active page
    bool& performance_stats_active = ConfigManager::Get().GetConfigBoolRef(configid_bool_state_performance_stats_active);
    if ((selected == 3) && (!performance_stats_active))
    {
        performance_stats_active = true;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_performance_stats_active), true);
    }
    else if (( (selected != 3) || (!m_Visible) ) && (performance_stats_active))
    {
        performance_stats_active = false;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_performance_stats_active), false);
    }
}

bool WindowSettings::IsShown() const
{
    return m_Visible;
}

const ImVec2& WindowSettings::GetSize() const
{
    return m_Size;
}