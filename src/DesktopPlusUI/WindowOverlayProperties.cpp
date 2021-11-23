#include "WindowOverlayProperties.h"

#include "ImGuiExt.h"
#include "UIManager.h"
#include "TranslationManager.h"
#include "InterprocessMessaging.h"
#include "WindowManager.h"
#include "Util.h"
#include "DesktopPlusWinRT.h"

#include <sstream>

WindowOverlayProperties::WindowOverlayProperties() :
    m_PageStackPos(0),
    m_PageStackPosAnimation(0),
    m_PageAnimationDir(0),
    m_PageAnimationProgress(0.0f),
    m_PageAnimationStartPos(0.0f),
    m_PageAnimationOffset(0.0f),
    m_PageFadeDir(0),
    m_PageFadeAlpha(1.0f),
    m_PageAppearing(wndovrlprop_page_main),
    m_ActiveOverlayID(k_ulOverlayID_None),
    m_Column0Width(0.0f),
    m_IsConfigDataModified(false),
    m_BufferOverlayName{0}
{
    m_WindowIcon = tmtex_icon_xsmall_settings;
    m_OvrlWidth = OVERLAY_WIDTH_METERS_SETTINGS;

    //Leave 2 pixel padding around so interpolation doesn't cut off the pixel border
    const DPRect& rect = UITextureSpaces::Get().GetRect(ui_texspace_overlay_properties);
    m_Size = {float(rect.GetWidth() - 4), float(rect.GetHeight() - 4)};
    m_Pos =  {float(rect.GetTL().x + 2),  float(rect.GetTL().y + 2)};

    m_PageStack.push_back(wndovrlprop_page_main);

    ResetTransform();
}

void WindowOverlayProperties::Show(bool skip_fade)
{
    UIManager::Get()->UpdateDesktopOverlayPixelSize();
    SetActiveOverlayID(m_ActiveOverlayID, true);        //ensure that current overlay ID is still in sync with dashboard app (can desync if dashboard app was restarted)
    FloatingWindow::Show(skip_fade);
}

void WindowOverlayProperties::Hide(bool skip_fade)
{
    FloatingWindow::Hide(skip_fade);

    //When hiding while the position change page is open, sync transform
    if (m_PageStack.back() == wndovrlprop_page_position_change)
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_transform_sync, (int)m_ActiveOverlayID);
    }
}

void WindowOverlayProperties::ResetTransform()
{
    m_Transform.identity();
    m_Transform.rotateY(15.0f);
    m_Transform.translate_relative(-OVERLAY_WIDTH_METERS_DASHBOARD_UI / 3.0f, 0.70f, 0.15f);
}

vr::VROverlayHandle_t WindowOverlayProperties::GetOverlayHandle() const
{
    return UIManager::Get()->GetOverlayHandleOverlayProperties();
}

unsigned int WindowOverlayProperties::GetActiveOverlayID() const
{
    return m_ActiveOverlayID;
}

void WindowOverlayProperties::SetActiveOverlayID(unsigned int overlay_id, bool skip_fade)
{
    unsigned int overlay_id_prev = m_ActiveOverlayID;

    //This needs to cancel any active page state for the previous overlay if it's not the same
    if ( (!skip_fade) && (overlay_id_prev != overlay_id) )
    {
        //Animate overlay switching by fading out first if the window is visible
        if (m_PageFadeDir == 0)
        {
            PageFadeStart(overlay_id);
            return;
        }

        PageGoHome(true);
    }

    //These need to always reset in case of underlying changes or even just language switching
    m_CropButtonLabel = "";
    m_WinRTSourceButtonLabel = "";

    m_ActiveOverlayID = overlay_id;

    OverlayManager::Get().SetCurrentOverlayID(overlay_id);

    //k_ulOverlayID_None is used when fading out an overlay that got deleted, so keep old info around while fading out
    if (overlay_id != k_ulOverlayID_None)
    {
        OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();

        m_WindowTitle = data.ConfigNameStr;

        //Update overlay name buffer
        if (data.ConfigBool[configid_bool_overlay_name_custom])
        {
            size_t copied_length = m_WindowTitle.copy(m_BufferOverlayName, IM_ARRAYSIZE(m_BufferOverlayName) - 1);
            m_BufferOverlayName[copied_length] = '\0';
        }
        else
        {
            m_BufferOverlayName[0] = '\0';
        }

        bool has_win32_window_icon = false;
        m_WindowIcon = TextureManager::Get().GetOverlayIconTextureID(data, true, &has_win32_window_icon);
    
        if (has_win32_window_icon)
        {
            m_WindowIconWin32IconCacheID = TextureManager::Get().GetWindowIconCacheID((HWND)data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd], 
                                                                                      data.ConfigHandle[configid_handle_overlay_state_winrt_last_hicon]);
        }
        else
        {
            m_WindowIconWin32IconCacheID = -1;
        }
    }

    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_interface_overlay_current_id), (int)overlay_id);
}

void WindowOverlayProperties::WindowUpdate()
{
    ImGui::SetWindowSize(m_Size);

    //Keep window blank if no overlay is set (like when fading out from removing an overlay)
    if (m_ActiveOverlayID == k_ulOverlayID_None)
        return;

    ImGuiStyle& style = ImGui::GetStyle();

    if (m_Column0Width == 0.0f)
    {
        m_Column0Width = ImGui::GetFontSize() * 12.75f;
    }

    const float page_width = m_Size.x - style.WindowBorderSize - style.WindowPadding.x - style.WindowPadding.x;

    //Page animation
    if (m_PageAnimationDir != 0)
    {
        float target_x = (page_width + style.ItemSpacing.x) * -m_PageStackPosAnimation;
        m_PageAnimationProgress += ImGui::GetIO().DeltaTime * 3.0f;

        m_PageAnimationOffset = smoothstep(m_PageAnimationProgress, m_PageAnimationStartPos, target_x);

        if (m_PageAnimationProgress >= 1.0f)
        {
            //Remove pages in the stack after finishing going back
            if (m_PageAnimationDir == 1)
            {
                while ((int)m_PageStack.size() > m_PageStackPosAnimation + 1)
                {
                    m_PageStack.pop_back();
                }
            }

            m_PageAnimationOffset = target_x;
            m_PageAnimationDir = 0;
        }
    }
    else if (m_PageStackPosAnimation != m_PageStackPos) //Only start new animation if none is running
    {
        m_PageAnimationDir = (m_PageStackPosAnimation < m_PageStackPos) ? -1 : 1;
        m_PageStackPosAnimation = m_PageStackPos;
        m_PageAnimationStartPos = m_PageAnimationOffset;
        m_PageAnimationProgress = 0.0f;

        //Set appearing value to top of stack when starting animation to it
        if (m_PageAnimationDir == -1)
        {
            m_PageAppearing = m_PageStack.back();
        }
    }
    else if (ImGui::IsWindowAppearing()) //Set appearing value when the whole window appeared again
    {
        m_PageAppearing = m_PageStack.back();
    }

    //Page Fade
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Alpha * m_PageFadeAlpha);

    if (m_PageFadeDir == -1)
    {
        m_PageFadeAlpha -= 0.1f;

        if (m_PageFadeAlpha <= 0.0f) //Completed fade-out
        {
            //Switch active overlay and fade back in
            m_PageFadeAlpha = 0.0f;
            m_PageFadeDir = 1;

            SetActiveOverlayID(m_PageFadeTargetOverlayID); //Calls PageGoHome() among other things
        }
    }
    else if (m_PageFadeDir == 1)
    {
        m_PageFadeAlpha += 0.1f;

        if (m_PageFadeAlpha >= 1.0f) //Completed fading back in
        {
            m_PageFadeAlpha = 1.0f;
            m_PageFadeDir = 0;
        }
    }

    //Set up page offset and clipping
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + m_PageAnimationOffset);

    ImGui::PushClipRect({m_Pos.x + style.WindowBorderSize, 0.0f}, {m_Pos.x + m_Size.x - style.WindowBorderSize, FLT_MAX}, false);

    const char* const child_str_id[]{"OvrlPropsPageMain", "OvrlPropsPage1", "OvrlPropsPage2", "OvrlPropsPage3"}; //No point in generating these on the fly
    int child_id = 0;
    int stack_size = (int)m_PageStack.size();
    for (WindowOverlayPropertiesPage page_id : m_PageStack)
    {
        if (child_id >= IM_ARRAYSIZE(child_str_id))
            break;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));

        if ((ImGui::BeginChild(child_str_id[child_id], {page_width, ImGui::GetContentRegionAvail().y})) || (m_PageAppearing == page_id)) //Process page if currently appearing
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg

            switch (page_id)
            {
                case wndovrlprop_page_main:                    UpdatePageMain();                  break;
                case wndovrlprop_page_position_change:         UpdatePagePositionChange();        break;
                case wndovrlprop_page_crop_change:             UpdatePageCropChange();            break;
                case wndovrlprop_page_graphics_capture_source: UpdatePageGraphicsCaptureSource(); break;
                default: break;
            }
        }
        else
        {
            ImGui::PopStyleColor(); //ImGuiCol_ChildBg
        }

        ImGui::EndChild();

        child_id++;

        if (stack_size > child_id)
        {
            ImGui::SameLine();
        }
    }

    m_PageAppearing = wndovrlprop_page_none;

    ImGui::PopClipRect();

    ImGui::PopStyleVar(); //ImGuiStyleVar_Alpha
}

void WindowOverlayProperties::OverlayPositionReset()
{
    float& up      = ConfigManager::GetRef(configid_float_overlay_offset_up);
    float& right   = ConfigManager::GetRef(configid_float_overlay_offset_right);
    float& forward = ConfigManager::GetRef(configid_float_overlay_offset_forward);
    up = right = forward = 0.0f;

    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_up),      pun_cast<LPARAM, float>(up));
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_right),   pun_cast<LPARAM, float>(right));
    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_forward), pun_cast<LPARAM, float>(forward));

    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
}

void WindowOverlayProperties::UpdatePageMain()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::BeginChild("OvrlPropsMainContent");
    ImGui::PopStyleColor();

    UpdatePageMainCatPosition();
    UpdatePageMainCatAppearance();
    UpdatePageMainCatCapture();
    UpdatePageMainCatPerformanceMonitor();
    UpdatePageMainCatAdvanced();
    UpdatePageMainCatInterface();

    ImGui::EndChild();
}

void WindowOverlayProperties::UpdatePageMainCatPosition()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsCatPosition));
    ImGui::Columns(2, "ColumnPosition", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    //Origin
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPositionOrigin));
    ImGui::NextColumn();

    int& mode_origin = ConfigManager::GetRef(configid_int_overlay_origin);

    const ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
    ImVec2 img_size, img_uv_min, img_uv_max;
    const ImVec2 combo_pos = ImGui::GetCursorScreenPos();

    ImGui::PushItemWidth(-1.0f);
    if (ImGui::BeginComboAnimated("##ComboPositionOrigin", ""))
    {
        static bool is_generic_tracker_connected = false;

        if ((ImGui::IsWindowAppearing()) && (UIManager::Get()->IsOpenVRLoaded()))
        {
            is_generic_tracker_connected = (GetFirstVRTracker() != vr::k_unTrackedDeviceIndexInvalid);
        }

        const int loop_max = (is_generic_tracker_connected) ? ovrl_origin_dplus_tab : ovrl_origin_aux;

        //Make use of the fact origin, icon and translation string IDs are laid out sequentially and shorten this to a nice loop
        for (int i = ovrl_origin_room; i < loop_max; ++i)
        {
            ImGui::PushID(i);

            if (ImGui::Selectable("", (mode_origin == i)))
            {
                mode_origin = i;
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_origin), mode_origin);
            }

            ImGui::SameLine(0.0f, 0.0f);

            TextureManager::Get().GetTextureInfo((TMNGRTexID)(tmtex_icon_xsmall_origin_room + i), img_size, img_uv_min, img_uv_max);
            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::TextUnformatted(TranslationManager::GetString( (TRMGRStrID)(tstr_OvrlPropsPositionOriginRoom + i) ));

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    //Custom combo preview content (icon with text)
    const ImVec2 backup_pos = ImGui::GetCursorScreenPos();
    ImVec2 clip_end = ImGui::GetItemRectMax();
    clip_end.x -= ImGui::GetFrameHeight();

    ImGui::SetCursorScreenPos(ImVec2(combo_pos.x + style.FramePadding.x, combo_pos.y + style.FramePadding.y));

    TextureManager::Get().GetTextureInfo((TMNGRTexID)(tmtex_icon_xsmall_origin_room + mode_origin), img_size, img_uv_min, img_uv_max);
    ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY());

    ImGui::PushClipRect(ImGui::GetCursorScreenPos(), clip_end, true);
    ImGui::TextUnformatted(TranslationManager::GetString( (TRMGRStrID)(tstr_OvrlPropsPositionOriginRoom + mode_origin) ));
    ImGui::PopClipRect();

    ImGui::SetCursorScreenPos(backup_pos);
    ImGui::NextColumn();

    //Display Mode
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPositionDispMode));
    ImGui::NextColumn();

    int& mode_display = ConfigManager::GetRef(configid_int_overlay_display_mode);

    ImGui::PushItemWidth(-1.0f);

    if (TranslatedComboAnimated("##ComboPositionDisplayMode", mode_display, tstr_OvrlPropsPositionDispModeAlways, tstr_OvrlPropsPositionDispModeDPlus))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_display_mode), mode_display);
    }

    ImGui::NextColumn();

    //Position Change
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPositionPos));

    if (!UIManager::Get()->IsOpenVRLoaded()) //Show tip if position can't be changed due to desktop mode
    {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_OvrlPropsPositionPosTip));
    }

    ImGui::NextColumn();

    if (!UIManager::Get()->IsOpenVRLoaded())
        ImGui::PushItemDisabled();

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionChange)))
    {
        PageGoForward(wndovrlprop_page_position_change);
    }

    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionReset)))
    {
        OverlayPositionReset();
    }

    if (!UIManager::Get()->IsOpenVRLoaded())
        ImGui::PopItemDisabled();

    ImGui::Columns(1);
}

void WindowOverlayProperties::UpdatePageMainCatAppearance()
{
    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    ImGui::Spacing();
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsCatAppearance));
    ImGui::Columns(2, "ColumnAppearance", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    //Width
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsAppearanceWidth));
    ImGui::NextColumn();

    float& width = ConfigManager::GetRef(configid_float_overlay_width);
    const float width_slider_max = (ConfigManager::GetRef(configid_int_overlay_origin) >= ovrl_origin_right_hand) ? 1.5f : 5.0f;

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("OverlayWidth") );
    if (ImGui::SliderWithButtonsFloat("OverlayWidth", width, 0.1f, 0.01f, 0.05f, width_slider_max, "%.2f m", ImGuiSliderFlags_Logarithmic))
    {
        if (width < 0.05f)
            width = 0.05f;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_width), pun_cast<LPARAM, float>(width));
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::NextColumn();

    //Curvature
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsAppearanceCurve));
    ImGui::NextColumn();

    float& curve = ConfigManager::GetRef(configid_float_overlay_curvature);

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("OverlayCurvature") );
    if (ImGui::SliderWithButtonsFloatPercentage("OverlayCurvature", curve, 5, 1, 0, 35, "%d%%"))
    {
        curve = clamp(curve, 0.0f, 1.0f);

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_curvature), pun_cast<LPARAM, float>(curve));
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::NextColumn();

    //Opacity
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsAppearanceOpacity));
    ImGui::NextColumn();

    float& opacity = ConfigManager::GetRef(configid_float_overlay_opacity);

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("OverlayOpacity") );
    if (ImGui::SliderWithButtonsFloatPercentage("OverlayOpacity", opacity, 5, 1, 0, 100, "%d%%"))
    {
        opacity = clamp(opacity, 0.0f, 1.0f);

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_opacity), pun_cast<LPARAM, float>(opacity));
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::NextColumn();

    //Crop
    if (ConfigManager::GetValue(configid_int_overlay_capture_source) != ovrl_capsource_ui) //Don't show crop settings for UI source overlays
    {
        bool& crop_enabled = ConfigManager::GetRef(configid_bool_overlay_crop_enabled);

        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsAppearanceCrop), &crop_enabled))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_crop_enabled), crop_enabled);
        }
        ImGui::NextColumn();
        ImGui::Spacing();

        //Build button string if empty
        if (m_CropButtonLabel.empty())
        {
            int crop_width  = ConfigManager::GetValue(configid_int_overlay_crop_width);
            int crop_height = ConfigManager::GetValue(configid_int_overlay_crop_height);

            std::stringstream ss;
            ss << ConfigManager::GetValue(configid_int_overlay_crop_x) << ", " << ConfigManager::GetValue(configid_int_overlay_crop_y) << " | ";
            (crop_width  == -1) ? ss << TranslationManager::GetString(tstr_OvrlPropsAppearanceCropValueMax) << " " : ss << crop_width;
            ss << "x";
            (crop_height == -1) ? ss << " " << TranslationManager::GetString(tstr_OvrlPropsAppearanceCropValueMax) : ss << crop_height;

            m_CropButtonLabel = ss.str();
        }

        if (ImGui::Button(m_CropButtonLabel.c_str()))
        {
            PageGoForward(wndovrlprop_page_crop_change);
        }
    }

    ImGui::Columns(1);
}

void WindowOverlayProperties::UpdatePageMainCatCapture()
{
    //All capture settings are considered advanced
    if (!ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        return;

    ImGuiStyle& style = ImGui::GetStyle();

    static float winrt_source_label_width = 0.0f;

    int& capture_method = ConfigManager::GetRef(configid_int_overlay_capture_source);

    //Don't show capture settings for UI source overlays
    if (capture_method == ovrl_capsource_ui)
        return;

    int& winrt_selected_desktop = ConfigManager::GetRef(configid_int_overlay_winrt_desktop_id);
    HWND winrt_selected_window  = (HWND)ConfigManager::GetValue(configid_handle_overlay_state_winrt_hwnd);

    if (m_WinRTSourceButtonLabel.empty())
    {
        m_WinRTSourceButtonLabel = GetStringForWinRTSource(winrt_selected_window, winrt_selected_desktop);
        winrt_source_label_width = ImGui::CalcTextSize(m_WinRTSourceButtonLabel.c_str()).x;
    }

    ImGui::Spacing();
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsCatCapture));
    ImGui::Columns(2, "ColumnCapture", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    //Capture Method
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsCaptureMethod));

    if (!DPWinRT_IsCaptureSupported())
    {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_OvrlPropsCaptureMethodGCUnsupportedTip), "(!)");
    }
    else if (!DPWinRT_IsCaptureFromCombinedDesktopSupported())
    {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_OvrlPropsCaptureMethodGCUnsupportedPartialTip), "(!)");
    }

    ImGui::NextColumn();

    if (ImGui::RadioButton(TranslationManager::GetString(tstr_OvrlPropsCaptureMethodDup), (capture_method == ovrl_capsource_desktop_duplication)))
    {
        capture_method = ovrl_capsource_desktop_duplication;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_capture_source), capture_method);

        OverlayManager::Get().SetCurrentOverlayNameAuto();
        SetActiveOverlayID(m_ActiveOverlayID);

        UIManager::Get()->RepeatFrame();
    }

    ImGui::SameLine(0.0f, style.ItemSpacing.x / 2.0f); //Getting tight there, use less spacing

    if (!DPWinRT_IsCaptureSupported())
        ImGui::PushItemDisabled();

    if (ImGui::RadioButton(TranslationManager::GetString(tstr_OvrlPropsCaptureMethodGC), (capture_method == ovrl_capsource_winrt_capture)))
    {
        capture_method = ovrl_capsource_winrt_capture;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_capture_source), capture_method);

        OverlayManager::Get().SetCurrentOverlayNameAuto();
        SetActiveOverlayID(m_ActiveOverlayID);

        UIManager::Get()->RepeatFrame();
    }

    if (!DPWinRT_IsCaptureSupported())
        ImGui::PopItemDisabled();

    ImGui::NextColumn();

    //Source
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsCaptureSource));
    ImGui::NextColumn();

    if (capture_method == ovrl_capsource_desktop_duplication)
    {
        int desktop_count = ConfigManager::GetValue(configid_int_state_interface_desktop_count);
        int& selected_desktop = ConfigManager::GetRef(configid_int_overlay_desktop_id);

        ImGui::PushItemWidth(-1.0f);
        if (ImGui::BeginComboAnimated("##ComboDesktopSource", TranslationManager::Get().GetDesktopIDString(selected_desktop) ))
        {
            for (int i = -1; i < desktop_count; ++i)
            {
                ImGui::PushID(i);

                if (ImGui::Selectable(TranslationManager::Get().GetDesktopIDString(i), (selected_desktop == i)))
                {
                    selected_desktop = i;
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_desktop_id), selected_desktop);

                    OverlayManager::Get().SetCurrentOverlayNameAuto();
                    SetActiveOverlayID(m_ActiveOverlayID);
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        ImGui::NextColumn();
    }
    else //ovrl_capsource_winrt_capture
    {
        ImVec2 button_size(0.0f, 0.0f);

        if (winrt_source_label_width > ImGui::GetContentRegionAvail().x - style.FramePadding.x * 2.0f)
        {
            button_size.x = ImGui::GetContentRegionAvail().x;
        }

        if (ImGui::Button(m_WinRTSourceButtonLabel.c_str(), button_size))
        {
            PageGoForward(wndovrlprop_page_graphics_capture_source);
        }
    }

    ImGui::Columns(1);
}

void WindowOverlayProperties::UpdatePageMainCatPerformanceMonitor()
{
    //Don't show performance monitor settings for non-UI source overlays
    if (ConfigManager::GetValue(configid_int_overlay_capture_source) != ovrl_capsource_ui)
        return;

    ImGui::Spacing();
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsCatPerformanceMonitor));
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    if ( (UIManager::Get()->IsOpenVRLoaded()) && (UIManager::Get()->IsInDesktopMode()) )
    {
        HelpMarker(TranslationManager::GetString(tstr_OvrlPropsPerfMonDesktopModeTip));
    }
    else
    {
        HelpMarker(TranslationManager::GetString(tstr_OvrlPropsPerfMonGlobalTip));
    }

    ImGui::Columns(2, "ColumnPerformanceMonitor", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPerfMonStyle));
    ImGui::NextColumn();

    bool& use_large_style = ConfigManager::GetRef(configid_bool_performance_monitor_large_style);

    if (ImGui::RadioButton(TranslationManager::GetString(tstr_OvrlPropsPerfMonStyleCompact), !use_large_style))
    {
        use_large_style = false;
        UIManager::Get()->RepeatFrame();
    }

    ImGui::SameLine();

    if (ImGui::RadioButton(TranslationManager::GetString(tstr_OvrlPropsPerfMonStyleLarge), use_large_style))
    {
        use_large_style = true;
        UIManager::Get()->RepeatFrame();
    }

    ImGui::Columns(1);

    //Monitor items
    ImGui::Spacing();

    ImGui::Columns(2, "ColumnPerformanceMonitorItems", false);
    ImGui::SetColumnWidth(0, m_Column0Width);
    ImGui::SetColumnWidth(1, m_Column0Width);

    bool& show_cpu             = ConfigManager::GetRef(configid_bool_performance_monitor_show_cpu);
    bool& show_gpu             = ConfigManager::GetRef(configid_bool_performance_monitor_show_gpu);
    bool& show_graphs          = ConfigManager::GetRef(configid_bool_performance_monitor_show_graphs);
    bool& show_fps             = ConfigManager::GetRef(configid_bool_performance_monitor_show_fps);
    bool& show_battery         = ConfigManager::GetRef(configid_bool_performance_monitor_show_battery);
    bool& show_time            = ConfigManager::GetRef(configid_bool_performance_monitor_show_time);
    bool& show_trackers        = ConfigManager::GetRef(configid_bool_performance_monitor_show_trackers);
    bool& show_vive_wireless   = ConfigManager::GetRef(configid_bool_performance_monitor_show_vive_wireless);
    bool& disable_gpu_counters = ConfigManager::GetRef(configid_bool_performance_monitor_disable_gpu_counters);

    //Keep unavailable options as enabled but show the check boxes as unticked to avoid confusion
    bool show_graphs_visual        = ( (use_large_style) && ((!show_cpu) && (!show_gpu)) ) ? false : show_graphs;
    bool show_time_visual          = ( ((!show_fps) && (!show_battery)) || (!use_large_style) ) ? false : show_time;
    bool show_trackers_visual      = (!show_battery) ? false : show_trackers;
    bool show_vive_wireless_visual = (!show_battery) ? false : show_vive_wireless;

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPerfMonShowCPU), &show_cpu))
    {
        UIManager::Get()->RepeatFrame();
    }

    ImGui::NextColumn();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPerfMonShowGPU), &show_gpu))
    {
        UIManager::Get()->RepeatFrame();
    }

    ImGui::NextColumn();

    if ( (use_large_style) && ((!show_cpu) && (!show_gpu)) )
        ImGui::PushItemDisabled();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPerfMonShowGraphs), &show_graphs_visual))
    {
        show_graphs = show_graphs_visual;
        UIManager::Get()->RepeatFrame();
    }

    if ( (use_large_style) && ((!show_cpu) && (!show_gpu)) )
        ImGui::PopItemDisabled();

    ImGui::NextColumn();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPerfMonShowFrameStats), &show_fps))
    {
        UIManager::Get()->RepeatFrame();
    }

    ImGui::NextColumn();

    if ( ((!show_fps) && (!show_battery)) || (!use_large_style) )
        ImGui::PushItemDisabled();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPerfMonShowTime), &show_time_visual))
    {
        show_time = show_time_visual;
        UIManager::Get()->RepeatFrame();
    }

    if ( ((!show_fps) && (!show_battery)) || (!use_large_style) )
        ImGui::PopItemDisabled();

    ImGui::NextColumn();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPerfMonShowBattery), &show_battery))
    {
        UIManager::Get()->RepeatFrame();
    }

    ImGui::NextColumn();

    if (!show_battery)
        ImGui::PushItemDisabled();

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPerfMonShowTrackerBattery), &show_trackers_visual))
    {
        show_trackers = show_trackers_visual;
        UIManager::Get()->RepeatFrame();
    }

    ImGui::NextColumn();

    if (UIManager::Get()->GetPerformanceWindow().IsViveWirelessInstalled())
    {
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPerfMonShowViveWirelessTemp), &show_vive_wireless_visual))
        {
            show_vive_wireless = show_vive_wireless_visual;
            UIManager::Get()->RepeatFrame();
        }
    }

    if (!show_battery)
        ImGui::PopItemDisabled();

    ImGui::Columns(1);

    ImGui::Indent();

    if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
    {
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPerfMonDisableGPUCounter), &disable_gpu_counters))
        {
            //Update active performance counter state if the window is currently visible
            if (UIManager::Get()->GetPerformanceWindow().IsVisible())
            {
                if (disable_gpu_counters)
                {
                    UIManager::Get()->GetPerformanceWindow().GetPerformanceData().DisableGPUCounters();
                }
                else
                {
                    UIManager::Get()->GetPerformanceWindow().GetPerformanceData().EnableCounters(true);
                }
            }

            UIManager::Get()->RepeatFrame();
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_OvrlPropsPerfMonDisableGPUCounterTip));
    }

    ImGui::Spacing();

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPerfMonResetValues)))
    {
        UIManager::Get()->GetPerformanceWindow().ResetCumulativeValues();
    }

    ImGui::Unindent();
}

void WindowOverlayProperties::UpdatePageMainCatAdvanced()
{
    ImGuiStyle& style = ImGui::GetStyle();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    ImGui::Spacing();
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsCatAdvanced));
    ImGui::Columns(2, "ColumnAdvanced", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    //3D
    if (ConfigManager::GetValue(configid_int_overlay_capture_source) != ovrl_capsource_ui) //Don't show 3D settings for UI source overlays
    {
        bool& is_3D_enabled = ConfigManager::GetRef(configid_bool_overlay_3D_enabled);
        bool& is_3D_swapped = ConfigManager::GetRef(configid_bool_overlay_3D_swapped);
        int& mode_3d        = ConfigManager::GetRef(configid_int_overlay_3D_mode);

        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsAdvanced3D), &is_3D_enabled))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_3D_enabled), is_3D_enabled);
        }
        ImGui::NextColumn();

        if (!is_3D_enabled)
            ImGui::PushItemDisabled();

        ImGui::PushItemWidth(-1.0f);
        if (ImGui::BeginComboAnimated("##Combo3DMode", TranslationManager::GetString( (TRMGRStrID)(tstr_OvrlPropsAdvancedHSBS + mode_3d) ) ))
        {
            for (int i = ovrl_3Dmode_hsbs; i < ovrl_3Dmode_MAX; ++i)
            {
                if (ImGui::Selectable(TranslationManager::GetString( (TRMGRStrID)(tstr_OvrlPropsAdvancedHSBS + i) ), (mode_3d == i)))
                {
                    mode_3d = i;
                    IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_3D_mode), mode_3d);
                }
            }

            ImGui::EndCombo();
        }

        ImGui::NextColumn();
        ImGui::NextColumn();

        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsAdvanced3DSwap), &is_3D_swapped))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_3D_swapped), is_3D_swapped);
        }

        if (!is_3D_enabled)
            ImGui::PopItemDisabled();

        ImGui::Spacing();
        ImGui::NextColumn();
    }

    //Gaze Fade
    {
        bool& gazefade_enabled = ConfigManager::GetRef(configid_bool_overlay_gazefade_enabled);

        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsAdvancedGazeFade), &gazefade_enabled))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_gazefade_enabled), gazefade_enabled);
        }
        ImGui::NextColumn();

        if (!UIManager::Get()->IsOpenVRLoaded())
            ImGui::PushItemDisabled();

        if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsAdvancedGazeFadeAuto)))
        {
            if (!UIManager::Get()->GetAuxUI().IsActive())
            {
                //Show GazeFade Auto Hint window which will do the countdown and trigger auto-configuration in the dashboard app when done
                UIManager::Get()->GetAuxUI().GetGazeFadeAutoHintWindow().SetTargetOverlay(m_ActiveOverlayID);
                UIManager::Get()->GetAuxUI().GetGazeFadeAutoHintWindow().Show();
            }
        }
        ImGui::NextColumn();

        if (!UIManager::Get()->IsOpenVRLoaded())
            ImGui::PopItemDisabled();

        //Only show auto-configure when advanced settings are hidden
        if (ConfigManager::GetValue(configid_bool_interface_show_advanced_settings))
        {
            if (!gazefade_enabled)
                ImGui::PushItemDisabled();

            ImGui::Indent(ImGui::GetFrameHeightWithSpacing());

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsAdvancedGazeFadeDistance));
            ImGui::NextColumn();

            float& distance = ConfigManager::GetRef(configid_float_overlay_gazefade_distance);
            const char* alt_text = (distance < 0.01f) ? TranslationManager::GetString(tstr_OvrlPropsAdvancedGazeFadeDistanceValueInf) : nullptr;

            vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("GazeFadeDistance") );
            if (ImGui::SliderWithButtonsFloat("GazeFadeDistance", distance, 0.05f, 0.01f, 0.0f, 1.5f, (distance < 0.01f) ? "##%.2f" : "%.2f m", 0, nullptr, alt_text))
            {
                if (distance < 0.01f)
                    distance = 0.0f;

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_gazefade_distance), *(LPARAM*)&distance);
            }
            vr_keyboard.VRKeyboardInputEnd();

            ImGui::NextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsAdvancedGazeFadeSensitivity));
            ImGui::NextColumn();

            float& rate = ConfigManager::GetRef(configid_float_overlay_gazefade_rate);

            vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("GazeFadeRate") );
            if (ImGui::SliderWithButtonsFloat("GazeFadeRate", rate, 0.1f, 0.025f, 0.4f, 3.0f, "%.2fx", ImGuiSliderFlags_Logarithmic))
            {
                if (rate < 0.0f)
                    rate = 0.0f;

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_gazefade_rate), *(LPARAM*)&rate);
            }
            vr_keyboard.VRKeyboardInputEnd();
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsAdvancedGazeFadeOpacity));
            ImGui::NextColumn();

            float& target_opacity = ConfigManager::GetRef(configid_float_overlay_gazefade_opacity);

            vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("GazeFadeOpacity") );
            if (ImGui::SliderWithButtonsFloatPercentage("GazeFadeOpacity", target_opacity, 5, 1, 0, 100, "%d%%"))
            {
                target_opacity = clamp(target_opacity, 0.0f, 1.0f);

                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_gazefade_opacity), *(LPARAM*)&target_opacity);
            }
            vr_keyboard.VRKeyboardInputEnd();

            if (!gazefade_enabled)
                ImGui::PopItemDisabled();

            ImGui::Spacing();
            ImGui::NextColumn();

            ImGui::Unindent(ImGui::GetFrameHeightWithSpacing());
        }
    }

    //Laser Pointer Input
    {
        bool& input_enabled = ConfigManager::GetRef(configid_bool_overlay_input_enabled);

        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsAdvancedInput), &input_enabled))
        {
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_input_enabled), input_enabled);
        }
        ImGui::NextColumn();

        if (!input_enabled)
            ImGui::PushItemDisabled();

        bool& input_dplus_lp_enabled = ConfigManager::GetRef(configid_bool_overlay_input_dplus_lp_enabled);
        bool& floating_ui_enabled    = ConfigManager::GetRef(configid_bool_overlay_floatingui_enabled);

        //Show disabled as unticked to avoid confusion
        bool input_dplus_lp_enabled_visual = (!input_enabled) ? false : input_dplus_lp_enabled;
        bool floating_ui_enabled_visual    = (!input_enabled) ? false : floating_ui_enabled;

        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsAdvancedInputInGame), &input_dplus_lp_enabled_visual))
        {
            input_dplus_lp_enabled = input_dplus_lp_enabled_visual;
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_input_dplus_lp_enabled), input_dplus_lp_enabled);
        }

        ImGui::NextColumn();
        ImGui::NextColumn();


        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsAdvancedInputFloatingUI), &floating_ui_enabled_visual))
        {
            floating_ui_enabled = floating_ui_enabled_visual;
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_floatingui_enabled), floating_ui_enabled);
        }

        if (!input_enabled)
            ImGui::PopItemDisabled();
    }

    ImGui::Columns(1);
}

void WindowOverlayProperties::UpdatePageMainCatInterface()
{
    OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    ImGui::Spacing();
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsCatInterface));
    ImGui::Columns(2, "ColumnInterface", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    //Overlay Name
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsInterfaceOverlayName));
        ImGui::NextColumn();

        ImGui::PushItemWidth(-1.0f);
        vr_keyboard.VRKeyboardInputBegin("##InputOverlayName");
        if (ImGui::InputTextWithHint("##InputOverlayName", TranslationManager::GetString(tstr_OvrlPropsInterfaceOverlayNameAuto), m_BufferOverlayName, 1024))
        {
            //If name buffer is not empty, set name from user input, otherwise fall back to auto naming
            if (m_BufferOverlayName[0] != '\0')
            {
                data.ConfigBool[configid_bool_overlay_name_custom] = true;
                data.ConfigNameStr = m_BufferOverlayName;

                if (ImGui::StringContainsUnmappedCharacter(m_BufferOverlayName))
                {
                    TextureManager::Get().ReloadAllTexturesLater();
                }
            }
            else
            {
                data.ConfigBool[configid_bool_overlay_name_custom] = false;
                OverlayManager::Get().SetCurrentOverlayNameAuto();
            }

            m_WindowTitle = data.ConfigNameStr;
        }
        vr_keyboard.VRKeyboardInputEnd();

        ImGui::Spacing();
        ImGui::NextColumn();
    }

    //Don't show for UI source overlays
    if (data.ConfigInt[configid_int_overlay_capture_source] != ovrl_capsource_ui)
    {
        if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsInterfaceDesktopButtons), &data.ConfigBool[configid_bool_overlay_floatingui_desktops_enabled]))
        {
            UIManager::Get()->RepeatFrame();
        }
    }

    ImGui::Columns(1);
}

void WindowOverlayProperties::UpdatePagePositionChange()
{
    ImGuiStyle& style = ImGui::GetStyle();

    static float column_width_0 = 0.0f;
    static float button_width = 0.0f;

    static int active_capture_type = 0; //0 = off, 1 = Move, 2 = Rotate
    static ImVec2 active_capture_pos;

    if (m_PageAppearing == wndovrlprop_page_position_change)
    {
        //Find longer label and use that as minimum column width
        float column_label_width = std::max(ImGui::CalcTextSize(TranslationManager::GetString(tstr_OvrlPropsPositionChangeMove)).x, 
                                            ImGui::CalcTextSize(TranslationManager::GetString(tstr_OvrlPropsPositionChangeRotate)).x);

        column_width_0 = std::max(ImGui::GetFontSize() * 3.75f, column_label_width + style.ItemInnerSpacing.x);

        //Find longest button label
        float button_label_width = 0.0f;
        float label_width = 0.0f;

        for (int i = tstr_OvrlPropsPositionChangeForward; i <= tstr_OvrlPropsPositionChangeLookAt; ++i)
        {
            label_width = ImGui::CalcTextSize(TranslationManager::GetString((TRMGRStrID)i)).x;

            if (label_width > button_label_width)
            {
                button_label_width = label_width;
            }
        }

        button_width = button_label_width + (style.FramePadding.x * 2.0f);

        //Set dragmode state
        //Dragging the overlay the UI is open on is pretty inconvenient to get out of when not sitting in front of a real mouse, so let's prevent this
        if (!UIManager::Get()->IsInDesktopMode())
        {
            //Automatically reset the matrix to a saner default if it still has the zero value
            if (ConfigManager::Get().GetOverlayDetachedTransform().isZero())
            {
                IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_reset);
            }

            bool& is_changing_position = ConfigManager::GetRef(configid_bool_state_overlay_dragmode);
            is_changing_position = true;
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragselectmode_show_hidden), is_changing_position);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragmode), is_changing_position);
        }
    }

    const float column_width_1 = ImGui::GetFrameHeightWithSpacing() * 3.0f + style.ItemInnerSpacing.x;
    const float column_width_2 = button_width + (style.ItemInnerSpacing.x * 2.0f);
    const float column_width_3 = style.ItemSpacing.x * 3.0f;

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsPositionChangeHeader)); 
    ImGui::Indent();

    if (!UIManager::Get()->IsInDesktopMode())
    {
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPositionChangeHelp));
    }
    else
    {
        ImGui::TextWrapped("%s", TranslationManager::GetString(tstr_OvrlPropsPositionChangeHelpDesktop));
    }

    ImGui::Unindent();

    //--Manual Adjustment
    ImGui::Spacing();
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsPositionChangeManualAdjustment));
    ImGui::Indent();

    ImGui::Columns(7, "ColumnManualAdjust", false);

    const ImVec2 arrow_button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
    const ImVec2 button_size(button_width, 0.0f);

    ImGui::SetColumnWidth(0, column_width_0);
    ImGui::SetColumnWidth(1, column_width_1);
    ImGui::SetColumnWidth(2, column_width_2);
    ImGui::SetColumnWidth(3, column_width_3);
    ImGui::SetColumnWidth(4, column_width_0);
    ImGui::SetColumnWidth(5, column_width_1);
    ImGui::SetColumnWidth(6, column_width_2);

    ImGui::PushButtonRepeat(true);

    //Row 1
    ImGui::NextColumn();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing());

    if (ImGui::ArrowButton("MoveUp", ImGuiDir_Up))
    {
        //Do some packing
        unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;  //Increase bit
        packed_value |= ipcactv_ovrl_pos_adjust_updown;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
    }

    ImGui::NextColumn();

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionChangeForward), button_size))
    {
        unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
        packed_value |= ipcactv_ovrl_pos_adjust_forwback;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
    }

    ImGui::NextColumn();
    ImGui::NextColumn();
    ImGui::NextColumn();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing());

    if (ImGui::ArrowButton("RotUp", ImGuiDir_Up))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_rotx);
    }

    ImGui::NextColumn();

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionChangeRollCW), button_size))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_rotz);
    }

    ImGui::NextColumn();

    //Row 2
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPositionChangeMove));
    ImGui::NextColumn();

    if (ImGui::ArrowButton("MoveLeft", ImGuiDir_Left))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_rightleft);
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    if (UIManager::Get()->IsInDesktopMode())
    {
        bool is_active = (active_capture_type == 1);

        if (is_active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);

        ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionChangeDragButton), arrow_button_size);

        //Activate on mouse down instead of normal button behavior, which is on mouse up
        if ((active_capture_type == 0) && (ImGui::IsItemHovered()) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            active_capture_type = 1;
            active_capture_pos = ImGui::GetIO().MousePos;
        }

        if (is_active)
            ImGui::PopStyleColor();
    }
    else
    {
        ImGui::Dummy(arrow_button_size);
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    if (ImGui::ArrowButton("MoveRight", ImGuiDir_Right))
    {
        unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
        packed_value |= ipcactv_ovrl_pos_adjust_rightleft;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
    }

    ImGui::NextColumn();

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionChangeBackward), button_size))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_forwback);
    }

    ImGui::NextColumn();
    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPositionChangeRotate));
    ImGui::NextColumn();

    if (ImGui::ArrowButton("RotLeft", ImGuiDir_Left))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_roty);
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    if (UIManager::Get()->IsInDesktopMode())
    {
        ImGui::PushID("##Rot");

        bool is_active = (active_capture_type == 2);

        if (is_active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);

        ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionChangeDragButton), arrow_button_size);

        //Activate on mouse down instead of normal button behavior, which is on mouse up
        if ((active_capture_type == 0) && (ImGui::IsItemHovered()) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            active_capture_type = 2;
            active_capture_pos = ImGui::GetIO().MousePos;
        }

        if (is_active)
            ImGui::PopStyleColor();
    
        ImGui::PopID();
    }
    else
    {
        ImGui::Dummy(arrow_button_size);
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

    if (ImGui::ArrowButton("RotRight", ImGuiDir_Right))
    {
        unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
        packed_value |= ipcactv_ovrl_pos_adjust_roty;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
    }

    ImGui::NextColumn();

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionChangeRollCCW), button_size))
    {
        unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
        packed_value |= ipcactv_ovrl_pos_adjust_rotz;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
    }

    ImGui::NextColumn();

    //Row 3
    ImGui::NextColumn();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing());

    if (ImGui::ArrowButton("MoveDown", ImGuiDir_Down))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_updown);
    }

    ImGui::NextColumn();
    ImGui::NextColumn();
    ImGui::NextColumn();
    ImGui::NextColumn();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFrameHeightWithSpacing());

    if (ImGui::ArrowButton("RotDown", ImGuiDir_Down))
    {
        unsigned int packed_value = ipcactv_ovrl_pos_adjust_increase;
        packed_value |= ipcactv_ovrl_pos_adjust_rotx;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
    }

    ImGui::PopButtonRepeat();

    ImGui::NextColumn();

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionChangeLookAt), button_size))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, ipcactv_ovrl_pos_adjust_lookat);
    }

    ImGui::Columns(1);
    ImGui::Unindent();

    //--Additional Offset
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    ImGui::Spacing();
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsPositionChangeOffset));

    ImGui::Columns(2, "ColumnOffset", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPositionChangeOffsetUpDown));
    ImGui::NextColumn();

    float& up = ConfigManager::GetRef(configid_float_overlay_offset_up);

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("OverlayOffsetUp") );
    if (ImGui::SliderWithButtonsFloat("OverlayOffsetUp", up, 0.1f, 0.01f, -5.0f, 5.0f, "%.2f m", ImGuiSliderFlags_Logarithmic))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_up), pun_cast<LPARAM, float>(up));
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPositionChangeOffsetRightLeft));
    ImGui::NextColumn();

    float& right = ConfigManager::GetRef(configid_float_overlay_offset_right);

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("OverlayOffsetRight") );
    if (ImGui::SliderWithButtonsFloat("OverlayOffsetRight", right, 0.1f, 0.01f, -5.0f, 5.0f, "%.2f m", ImGuiSliderFlags_Logarithmic))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_right), pun_cast<LPARAM, float>(right));
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsPositionChangeOffsetForwardBackward));
    ImGui::NextColumn();

    float& forward = ConfigManager::GetRef(configid_float_overlay_offset_forward);

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("OverlayOffsetForward") );
    if (ImGui::SliderWithButtonsFloat("OverlayOffsetForward", forward, 0.1f, 0.01f, -5.0f, 5.0f, "%.2f m", ImGuiSliderFlags_Logarithmic))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_float_overlay_offset_forward), pun_cast<LPARAM, float>(forward));
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::Columns(1);

    //--Drag Settings
    ImGui::Spacing();
    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsPositionChangeDragSettings));

    ImGui::Indent();

    bool& force_upright = ConfigManager::GetRef(configid_bool_input_drag_force_upright);

    if (ImGui::Checkbox(TranslationManager::GetString(tstr_OvrlPropsPositionChangeDragSettingsForceUpright), &force_upright))
    {
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_input_drag_force_upright), force_upright);
    }

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //--Confirmation buttons
    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogDone))) 
    {
        PageGoBack();
    }

    ImGui::SameLine();

    static float button_reset_width = 0.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - button_reset_width);

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionReset)))
    {
        OverlayPositionReset();
    }

    button_reset_width = ImGui::GetItemRectSize().x;

    //--Mouse Dragging
    if (active_capture_type != 0)
    {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            active_capture_type = 0;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        }
        else
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);

            const float delta_step = 5.0f;
            ImVec2 mouse_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

            if (active_capture_type == 1)
            {
                //X -> Right/Left
                if (fabs(mouse_delta.x) > delta_step)
                {
                    unsigned int packed_value = (mouse_delta.x > 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                    packed_value |= ipcactv_ovrl_pos_adjust_rightleft;

                    //Using the existing position adjust message a few times might be cheap, but it also results in actually useful grid-snapped adjustments
                    int steps = (int)(fabs(mouse_delta.x) / delta_step);
                    for (int i = 0; i < steps; ++i)
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                    }
                }

                //Y -> Up/Down
                if (fabs(mouse_delta.y) > delta_step)
                {
                    unsigned int packed_value = (mouse_delta.y < 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                    packed_value |= ipcactv_ovrl_pos_adjust_updown;

                    int steps = (int)(fabs(mouse_delta.y) / delta_step);
                    for (int i = 0; i < steps; ++i)
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                    }
                }

                //Wheel -> Forward/Backward
                if (fabs(ImGui::GetIO().MouseWheel) > 0.0f)
                {
                    unsigned int packed_value = (ImGui::GetIO().MouseWheel < 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                    packed_value |= ipcactv_ovrl_pos_adjust_forwback;

                    int steps = (int)(fabs(ImGui::GetIO().MouseWheel) * delta_step);
                    for (int i = 0; i < steps; ++i)
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                    }
                }
            }
            else //active_capture_type == 2
            {
                //X -> Rotate Y+-
                if (fabs(mouse_delta.x) > delta_step)
                {
                    unsigned int packed_value = (mouse_delta.x > 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                    packed_value |= ipcactv_ovrl_pos_adjust_roty;

                    int steps = (int)(fabs(mouse_delta.x) / delta_step);
                    for (int i = 0; i < steps; ++i)
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                    }
                }

                //Y -> Rotate X+-
                if (fabs(mouse_delta.y) > delta_step)
                {
                    unsigned int packed_value = (mouse_delta.y > 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                    packed_value |= ipcactv_ovrl_pos_adjust_rotx;

                    int steps = (int)(fabs(mouse_delta.y) / delta_step);
                    for (int i = 0; i < steps; ++i)
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                    }
                }

                //Wheel -> Rotate Z+-
                if (fabs(ImGui::GetIO().MouseWheel) > 0.0f)
                {
                    unsigned int packed_value = (ImGui::GetIO().MouseWheel > 0.0f) ? ipcactv_ovrl_pos_adjust_increase : 0;
                    packed_value |= ipcactv_ovrl_pos_adjust_rotz;

                    int steps = (int)(fabs(ImGui::GetIO().MouseWheel) * delta_step);
                    for (int i = 0; i < steps; ++i)
                    {
                        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_overlay_position_adjust, packed_value);
                    }
                }
            }

            //Reset mouse cursor if needed
            ImGuiIO& io = ImGui::GetIO();

            if (fabs(mouse_delta.x) > delta_step)
            {
                io.WantSetMousePos = true;
                io.MousePos.x = active_capture_pos.x;
                io.MouseClickedPos[0].x = io.MousePos.x;    //for drag delta
            }

            if (fabs(mouse_delta.y) > delta_step)
            {
                io.WantSetMousePos = true;
                io.MousePos.y = active_capture_pos.y;
                io.MouseClickedPos[0].y = io.MousePos.y;
            }
        }
    }
}

void WindowOverlayProperties::UpdatePageCropChange(bool only_restore_settings)
{
    VRKeyboard& vr_keyboard = UIManager::Get()->GetVRKeyboard();

    bool& crop_enabled = ConfigManager::GetRef(configid_bool_overlay_crop_enabled);
    int& crop_x        = ConfigManager::GetRef(configid_int_overlay_crop_x);
    int& crop_y        = ConfigManager::GetRef(configid_int_overlay_crop_y);
    int& crop_width    = ConfigManager::GetRef(configid_int_overlay_crop_width);
    int& crop_height   = ConfigManager::GetRef(configid_int_overlay_crop_height);

    if ( (only_restore_settings) && (m_IsConfigDataModified) )
    {
        //Restore previous settings
        OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
        crop_enabled       = m_ConfigDataTemp.ConfigBool[configid_bool_overlay_crop_enabled];
        crop_x             = m_ConfigDataTemp.ConfigInt[configid_int_overlay_crop_x];
        crop_y             = m_ConfigDataTemp.ConfigInt[configid_int_overlay_crop_y];
        crop_width         = m_ConfigDataTemp.ConfigInt[configid_int_overlay_crop_width];
        crop_height        = m_ConfigDataTemp.ConfigInt[configid_int_overlay_crop_height];
        data.ConfigNameStr = m_ConfigDataTemp.ConfigNameStr;

        SetActiveOverlayID(m_ActiveOverlayID);

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_crop_enabled), crop_enabled);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_width),    crop_width);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height),   crop_height);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_x),        crop_x);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_y),        crop_y);

        m_IsConfigDataModified = false;
        return;
    }

    if (m_PageAppearing == wndovrlprop_page_crop_change)
    {
        m_IsConfigDataModified = true;
        m_ConfigDataTemp = OverlayManager::Get().GetCurrentConfigData();

        //Enable cropping so modifications are always visible
        crop_enabled = true;
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_overlay_crop_enabled), true);
    }

    ImGui::TextUnformatted("(More things will still go here)");

    int ovrl_width, ovrl_height;

    if (ConfigManager::GetValue(configid_int_overlay_capture_source) == ovrl_capsource_desktop_duplication)
    {
        UIManager::Get()->GetDesktopOverlayPixelSize(ovrl_width, ovrl_height);
    }
    else //This would also work for desktop duplication except the above works without the dashboard app running while this doesn't
    {
        ovrl_width  = ConfigManager::GetValue(configid_int_overlay_state_content_width);
        ovrl_height = ConfigManager::GetValue(configid_int_overlay_state_content_height);

        //If overlay width and height are uninitialized, set them to the crop values at least
        if ((ovrl_width == -1) && (ovrl_height == -1))
        {
            ovrl_width  = crop_width  + crop_x;
            ovrl_height = crop_height + crop_y;

            ConfigManager::SetValue(configid_int_overlay_state_content_width,  ovrl_width);
            ConfigManager::SetValue(configid_int_overlay_state_content_height, ovrl_height);
        }
    }

    int crop_width_max  = ovrl_width  - crop_x;
    int crop_height_max = ovrl_height - crop_y;
    int crop_width_ui   = (crop_width  == -1) ? crop_width_max  + 1 : crop_width;
    int crop_height_ui  = (crop_height == -1) ? crop_height_max + 1 : crop_height;

    const bool disable_sliders = ((ovrl_width == -1) && (ovrl_height == -1));
    const bool is_crop_invalid = ((crop_x > ovrl_width - 1) || (crop_y > ovrl_height - 1) || (crop_width_max < 1) || (crop_height_max < 1));

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsCropManualAdjust));

    if ( (!disable_sliders) && (is_crop_invalid) )
    {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        HelpMarker(TranslationManager::GetString(tstr_OvrlPropsCropInvalidTip), "(!)");
    }

    ImGui::Columns(2, "ColumnCrop", false);
    ImGui::SetColumnWidth(0, m_Column0Width);

    if (disable_sliders)
        ImGui::PushItemDisabled();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsCropX));
    ImGui::NextColumn();

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("CropX") );
    if (ImGui::SliderWithButtonsInt("CropX", crop_x, 1, 1, 0, ovrl_width - 1, "%d px"))
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
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsCropY));
    ImGui::NextColumn();

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("CropY") );
    if (ImGui::SliderWithButtonsInt("CropY", crop_y, 1, 1, 0, ovrl_height - 1, "%d px"))
    {
        crop_y = clamp(crop_y, 0, ovrl_height - 1);

        if (crop_y + crop_height > ovrl_height)
        {
            crop_height = ovrl_height - crop_y;
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);
        }

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_y), crop_y);
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsCropWidth));
    ImGui::NextColumn();

    ImGui::SetNextItemWidth(-1);

    //The way mapping max + 1 == -1 value into the slider is done is a bit convoluted, but it works
    const char* text_alt_w = (crop_width == -1) ? TranslationManager::GetString(tstr_OvrlPropsAppearanceCropValueMax) : nullptr;

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("CropWidth") );
    if (ImGui::SliderWithButtonsInt("CropWidth", crop_width_ui, 1, 1, 1, crop_width_max + 1, (crop_width == -1) ? "" : "%d px", 0, nullptr, text_alt_w))
    {
        crop_width = clamp(crop_width_ui, 1, crop_width_max + 1);

        if (crop_width_ui > crop_width_max)
            crop_width = -1;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_width), crop_width);
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::NextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_OvrlPropsCropHeight));
    ImGui::NextColumn();

    ImGui::SetNextItemWidth(-1);

    const char* text_alt_h = (crop_height == -1) ? TranslationManager::GetString(tstr_OvrlPropsAppearanceCropValueMax) : nullptr;

    vr_keyboard.VRKeyboardInputBegin( ImGui::SliderWithButtonsGetSliderID("CropHeight") );
    if (ImGui::SliderWithButtonsInt("CropHeight", crop_height_ui, 1, 1, 1, crop_height_max + 1, (crop_height == -1) ? "" : "%d px", 0, nullptr, text_alt_h))
    {
        crop_height = clamp(crop_height_ui, 1, crop_height_max + 1);

        if (crop_height_ui > crop_height_max)
            crop_height = -1;

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);
    }
    vr_keyboard.VRKeyboardInputEnd();

    ImGui::NextColumn();
    ImGui::NextColumn();

    //Needs dashboard app to be running
    if ( (ConfigManager::GetValue(configid_int_overlay_capture_source) == ovrl_capsource_desktop_duplication) && (UIManager::Get()->IsOpenVRLoaded()) )
    {
        if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsCropToWindow)))
        {
            //Have the dashboard app figure out how to do this as the UI doesn't have all data needed at hand
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_action, ipcact_crop_to_active_window);

            WindowInfo window_info(::GetForegroundWindow());
            OverlayManager::Get().SetCurrentOverlayNameAuto(&window_info);
            SetActiveOverlayID(m_ActiveOverlayID);
        }
    }

    if (disable_sliders)
        ImGui::PopItemDisabled();

    ImGui::Columns(1);

    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()) );

    //--Confirmation buttons
    ImGui::Separator();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogOk))) 
    {
        m_IsConfigDataModified = false;
        PageGoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel))) 
    {
        PageGoBack();
    }

    ImGui::SameLine();

    static float button_reset_width = 0.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - button_reset_width);

    if (ImGui::Button(TranslationManager::GetString(tstr_OvrlPropsPositionReset)))
    {
        if ( (ConfigManager::GetValue(configid_int_overlay_capture_source) != ovrl_capsource_desktop_duplication) || (ConfigManager::GetValue(configid_bool_performance_single_desktop_mirroring)) || 
             (!UIManager::Get()->IsOpenVRLoaded()) )
        {
            crop_x = 0;
            crop_y = 0;
            crop_width  = -1;
            crop_height = -1;

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_x), crop_x);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_y), crop_y);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_width), crop_width);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_crop_height), crop_height);
        }
        else
        {
            //Have the dashboard figure out the right crop by changing the desktop ID to the current value again
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_desktop_id),
                                                        ConfigManager::GetValue(configid_int_overlay_desktop_id));
        }

        OverlayManager::Get().SetCurrentOverlayNameAuto();
        SetActiveOverlayID(m_ActiveOverlayID);
    }

    button_reset_width = ImGui::GetItemRectSize().x;
}

void WindowOverlayProperties::UpdatePageGraphicsCaptureSource(bool only_restore_settings)
{
    int desktop_count           = ConfigManager::GetValue(configid_int_state_interface_desktop_count);
    int& winrt_selected_desktop = ConfigManager::GetRef(configid_int_overlay_winrt_desktop_id);
    HWND winrt_selected_window  = (HWND)ConfigManager::GetValue(configid_handle_overlay_state_winrt_hwnd);

    if (only_restore_settings)
    {
        if (m_IsConfigDataModified)
        {
            //Restore previous settings
            OverlayConfigData& data = OverlayManager::Get().GetCurrentConfigData();
            data.ConfigInt[configid_int_overlay_winrt_desktop_id]             = m_ConfigDataTemp.ConfigInt[configid_int_overlay_winrt_desktop_id];
            data.ConfigHandle[configid_handle_overlay_state_winrt_hwnd]       = m_ConfigDataTemp.ConfigHandle[configid_handle_overlay_state_winrt_hwnd];
            data.ConfigStr[configid_str_overlay_winrt_last_window_title]      = m_ConfigDataTemp.ConfigStr[configid_str_overlay_winrt_last_window_title];
            data.ConfigStr[configid_str_overlay_winrt_last_window_class_name] = m_ConfigDataTemp.ConfigStr[configid_str_overlay_winrt_last_window_class_name];
            data.ConfigStr[configid_str_overlay_winrt_last_window_exe_name]   = m_ConfigDataTemp.ConfigStr[configid_str_overlay_winrt_last_window_exe_name];
            data.ConfigNameStr = m_ConfigDataTemp.ConfigNameStr;

            SetActiveOverlayID(m_ActiveOverlayID);

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_handle_overlay_state_winrt_hwnd), (LPARAM)winrt_selected_window);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id),            winrt_selected_desktop);

            m_IsConfigDataModified = false;
        }
        return;
    }

    if (m_PageAppearing == wndovrlprop_page_graphics_capture_source)
    {
        m_IsConfigDataModified = true;
        m_ConfigDataTemp = OverlayManager::Get().GetCurrentConfigData();
    }

    ImGui::TextColoredUnformatted(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), TranslationManager::GetString(tstr_OvrlPropsCaptureGCSource));
    ImGui::Indent();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::BeginChild("SourceList", ImVec2(0.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y), true);

    ImVec2 img_size_line_height = {ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
    ImVec2 img_size, img_uv_min, img_uv_max;

    //List None
    ImGui::PushID(-2);

    if (ImGui::Selectable("", ((winrt_selected_desktop == -2) && (winrt_selected_window == nullptr))))
    {
        winrt_selected_desktop = -2;
        winrt_selected_window = nullptr;
        ConfigManager::SetValue(configid_handle_overlay_state_winrt_hwnd, 0);
        ConfigManager::SetValue(configid_str_overlay_winrt_last_window_title, "");
        ConfigManager::SetValue(configid_str_overlay_winrt_last_window_class_name, "");
        ConfigManager::SetValue(configid_str_overlay_winrt_last_window_exe_name, "");
        ConfigManager::SetValue(configid_handle_overlay_state_winrt_last_hicon, 0);

        OverlayManager::Get().SetCurrentOverlayNameAuto();
        SetActiveOverlayID(m_ActiveOverlayID);

        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id), winrt_selected_desktop);
        IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_handle_overlay_state_winrt_hwnd), 0);

        UIManager::Get()->RepeatFrame();
    }

    ImGui::SameLine(0.0f, 0.0f);

    TextureManager::Get().GetTextureInfo(tmtex_icon_xsmall_desktop_none, img_size, img_uv_min, img_uv_max);
    ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::TextUnformatted(TranslationManager::GetString(tstr_SourceWinRTNone));

    ImGui::PopID();

    ImGui::Separator();

    //List Combined desktop, if supported
    if (DPWinRT_IsCaptureFromCombinedDesktopSupported())
    {
        ImGui::PushID(-1);

        if (ImGui::Selectable("", (winrt_selected_desktop == -1)))
        {
            winrt_selected_desktop = -1;
            winrt_selected_window = nullptr;
            ConfigManager::SetValue(configid_handle_overlay_state_winrt_hwnd, 0);
            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_title, "");
            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_class_name, "");
            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_exe_name, "");
            ConfigManager::SetValue(configid_handle_overlay_state_winrt_last_hicon, 0);

            OverlayManager::Get().SetCurrentOverlayNameAuto();
            SetActiveOverlayID(m_ActiveOverlayID);

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_handle_overlay_state_winrt_hwnd), 0);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id), winrt_selected_desktop);

            UIManager::Get()->RepeatFrame();
        }

        ImGui::SameLine(0.0f, 0.0f);

        TextureManager::Get().GetTextureInfo(tmtex_icon_xsmall_desktop_all, img_size, img_uv_min, img_uv_max);
        ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(TranslationManager::GetString(tstr_SourceDesktopAll));

        ImGui::PopID();
    }

    //List desktops
    for (int i = 0; i < desktop_count; ++i)
    {
        ImGui::PushID(i);

        if (ImGui::Selectable("", (winrt_selected_desktop == i)))
        {
            winrt_selected_desktop = i;
            winrt_selected_window = nullptr;
            ConfigManager::SetValue(configid_handle_overlay_state_winrt_hwnd, 0);
            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_title, "");
            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_class_name, "");
            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_exe_name, "");
            ConfigManager::SetValue(configid_handle_overlay_state_winrt_last_hicon, 0);

            OverlayManager::Get().SetCurrentOverlayNameAuto();
            SetActiveOverlayID(m_ActiveOverlayID);

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_handle_overlay_state_winrt_hwnd), 0);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id), winrt_selected_desktop);

            UIManager::Get()->RepeatFrame();
        }

        ImGui::SameLine(0.0f, 0.0f);

        const TMNGRTexID texid = (tmtex_icon_desktop_1 + i <= tmtex_icon_desktop_6) ? (TMNGRTexID)(tmtex_icon_xsmall_desktop_1 + i) : tmtex_icon_xsmall_desktop;
        TextureManager::Get().GetTextureInfo(texid, img_size, img_uv_min, img_uv_max);
        ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(TranslationManager::Get().GetDesktopIDString(i));

        ImGui::PopID();
    }

    ImGui::Separator();

    //List windows
    for (const auto& window_info : WindowManager::Get().WindowListGet())
    {
        ImGui::PushID(window_info.GetWindowHandle());
        if (ImGui::Selectable("", (winrt_selected_window == window_info.GetWindowHandle())))
        {
            winrt_selected_desktop = -2;
            winrt_selected_window = window_info.GetWindowHandle();

            ConfigManager::SetValue(configid_handle_overlay_state_winrt_hwnd, (uint64_t)winrt_selected_window);
            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_title,       StringConvertFromUTF16(window_info.GetTitle().c_str())); //No need to sync these
            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_class_name,  StringConvertFromUTF16(window_info.GetWindowClassName().c_str()));
            ConfigManager::SetValue(configid_str_overlay_winrt_last_window_exe_name,    window_info.GetExeName());

            OverlayManager::Get().SetCurrentOverlayNameAuto();
            SetActiveOverlayID(m_ActiveOverlayID);

            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_int_overlay_winrt_desktop_id), -2);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_handle_overlay_state_winrt_hwnd), (LPARAM)winrt_selected_window);
        }

        ImGui::SameLine(0.0f, 0.0f);

        int icon_id = TextureManager::Get().GetWindowIconCacheID(window_info.GetIcon());

        if (icon_id != -1)
        {
            TextureManager::Get().GetWindowIconTextureInfo(icon_id, img_size, img_uv_min, img_uv_max);
            ImGui::Image(ImGui::GetIO().Fonts->TexID, img_size_line_height, img_uv_min, img_uv_max);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }

        ImGui::TextUnformatted(window_info.GetListTitle().c_str());

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::Unindent();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()));

    //--Confirmation buttons
    //No separator since this is right after a boxed child window

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogOk)))
    {
        m_IsConfigDataModified = false; //Stops reset from being triggered on page leave
        PageGoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button(TranslationManager::GetString(tstr_DialogCancel)))
    {
        PageGoBack();
    }
}

std::string WindowOverlayProperties::GetStringForWinRTSource(HWND source_window, int source_desktop)
{
    std::string source_str;

    if (source_window == nullptr)
    {
        switch (source_desktop)
        {
            case -2: source_str = TranslationManager::GetString(tstr_SourceWinRTNone); break;
            default: source_str = TranslationManager::Get().GetDesktopIDString(source_desktop);
        }
    }
    else
    {
        const auto& window_list = WindowManager::Get().WindowListGet();
        const auto it = std::find_if(window_list.begin(), window_list.end(), [&](const auto& window) { return (window.GetWindowHandle() == source_window); });

        if ( (it != window_list.end()) && (::IsWindow(source_window)) )
        {
            source_str = it->GetListTitle();
        }
        else
        {
            const std::string& last_title = ConfigManager::GetValue(configid_str_overlay_winrt_last_window_title);
            source_str = (last_title.empty()) ? TranslationManager::GetString(tstr_SourceWinRTUnknown) : TranslationManager::GetString(tstr_SourceWinRTClosed) + std::string(" " + last_title);
        }
    }

    return source_str;
}

void WindowOverlayProperties::PageGoForward(WindowOverlayPropertiesPage new_page)
{
    m_PageStack.push_back(new_page);
    m_PageStackPos++;
}

void WindowOverlayProperties::PageGoBack()
{
    if (m_PageStackPos != 0)
    {
        OnPageLeaving(m_PageStack[m_PageStackPos]);
        m_PageStackPos--;
    }
}

void WindowOverlayProperties::PageGoHome(bool skip_animation)
{
    while (m_PageStackPos != 0)
    {
        OnPageLeaving(m_PageStack[m_PageStackPos]);
        m_PageStackPos--;
    }

    if (skip_animation)
    {
        m_PageStackPosAnimation = m_PageStackPos;
        m_PageAnimationOffset = 0.0f;

        while (m_PageStack.size() > 1)
        {
            m_PageStack.pop_back();
        }
    }
}

void WindowOverlayProperties::PageFadeStart(unsigned int overlay_id)
{
    m_PageFadeDir = -1; //Needed in any case for SetActiveOverlayID()

    if (m_Visible)
    {
        m_PageFadeTargetOverlayID = overlay_id;
    }
    else //Not visible, skip fade
    {
        SetActiveOverlayID(overlay_id); //Calls PageGoHome() for us

        m_PageFadeAlpha = 1.0f;
        m_PageFadeDir = 0;
    }
}

void WindowOverlayProperties::OnPageLeaving(WindowOverlayPropertiesPage previous_page)
{
    switch (previous_page)
    {
        case wndovrlprop_page_position_change:
        {
            //Disable dragmode when leaving page
            ConfigManager::SetValue(configid_bool_state_overlay_dragmode, false);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragselectmode_show_hidden), false);
            IPCManager::Get().PostMessageToDashboardApp(ipcmsg_set_config, ConfigManager::GetWParamForConfigID(configid_bool_state_overlay_dragmode),                   false);
            break;
        }
        case wndovrlprop_page_crop_change:
        {
            m_CropButtonLabel = "";
            UpdatePageCropChange(true); //Call to reset settings
            break;
        }
        case wndovrlprop_page_graphics_capture_source:
        {
            m_WinRTSourceButtonLabel = "";
            UpdatePageGraphicsCaptureSource(true); //Call to reset settings
            break;
        }
        default: break;
    }
}
