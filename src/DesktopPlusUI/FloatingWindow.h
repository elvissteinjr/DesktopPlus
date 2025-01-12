#pragma once

#include "OverlayDragger.h"
#include "TextureManager.h"
#include "TranslationManager.h"
#include "OverlayManager.h"
#include <string>

enum FloatingWindowOverlayStateID
{
    floating_window_ovrl_state_room,
    floating_window_ovrl_state_dashboard_tab
};

struct FloatingWindowOverlayState
{
    bool IsVisible = false;
    bool IsPinned  = false;
    float Size = 1.0f;
    Matrix4 Transform;
    Matrix4 TransformAbs;
};

struct FloatingWindowInputOverlayTagsState
{
    std::string TagEditOrigStr;
    char TagEditBuffer[1024]   = "";
    float ChildHeightLines     = 1.0f;

    std::vector<OverlayManager::TagListEntry> KnownTagsList;
    ImGuiTextFilter KnownTagsFilter;
    bool IsTagAlreadyInBuffer  = false;
    bool FocusTextInput        = false;

    bool PopupShow             = false;
    float PopupAlpha           = 0.0f;
    float PopupHeight          = FLT_MIN;
    float PopupHeightPrev      = FLT_MIN;
    ImGuiDir PosDir            = ImGuiDir_Down;
    ImGuiDir PosDirDefault     = ImGuiDir_Down;
    float PosAnimationProgress = 0.0f;
    bool IsFadingOut           = false;
};

struct FloatingWindowActionOrderListState
{
    std::vector<ActionManager::ActionNameListEntry> ActionsList;
    ActionManager::ActionList ActionListOrig;
    bool HasSavedChanges       = false;
    int KeyboardSwappedIndex   = -1;
    int SelectedIndex          = -1;
    ActionUID HoveredAction    = k_ActionUID_Invalid;    
};

struct FloatingWindowActionAddSelectorState
{
    std::vector<ActionManager::ActionNameListEntry> ActionsList;
    std::vector<char> ActionsTickedList;
    bool IsAnyActionTicked = false;
};

//Base class for drag-able floating overlay windows, such as the Settings, Overlay Properties and Keyboard windows
class FloatingWindow
{
    protected:
        float m_OvrlWidth;
        float m_OvrlWidthMax;             //Maximum width passed to OverlayDragger
        float m_Alpha;
        bool m_OvrlVisible;
        bool m_IsTransitionFading;

        FloatingWindowOverlayStateID m_OverlayStateCurrentID;
        FloatingWindowOverlayState m_OverlayStateRoom;
        FloatingWindowOverlayState m_OverlayStateDashboardTab;
        FloatingWindowOverlayState m_OverlayStateFading;
        FloatingWindowOverlayState* m_OverlayStateCurrent;
        FloatingWindowOverlayState* m_OverlayStatePending;

        std::string m_WindowTitle;
        std::string m_WindowID;
        TRMGRStrID m_WindowTitleStrID;
        TMNGRTexID m_WindowIcon;
        int m_WindowIconWin32IconCacheID; //TextureManager Icon cache ID when using a Win32 window icon as the ImGui window icon

        ImVec2 m_Pos;
        ImVec2 m_PosPivot;
        ImVec2 m_Size;                   //Set in derived constructor, 2 pixel-wide padding around actual texture space expected
        ImVec2 m_SizeUnscaled;           //Set in derived constructor, size before applying UI scale factor, so equal to m_Size initally
        ImGuiWindowFlags m_WindowFlags;
        bool m_AllowRoomUnpinning;       //Set to enable pin button while room overlay state is active
        OverlayOrigin m_DragOrigin;      //Origin passed to OverlayDragger for window drags, doesn't affect overlay positioning (override relevant functions instead)

        float m_TitleBarMinWidth;
        float m_TitleBarTitleMaxWidth;   //Width available for the title string without icon and buttons
        float m_TitleBarTitleIconAlpha;  //Alpha value applied to both window title text & icon
        bool m_IsTitleBarHovered;
        bool m_IsTitleIconClicked;
        bool m_HasAppearedOnce;
        bool m_IsWindowAppearing;

        float m_CompactTableHeaderHeight;

        void WindowUpdateBase();         //Sets up ImGui window with custom title bar, pinning and overlay-based dragging
        virtual void WindowUpdate() = 0; //Window content, called within an ImGui Begin()'d window

        void OverlayStateSwitchCurrent(bool use_dashboard_tab);
        void OverlayStateSwitchFinish();

        virtual void OnWindowPinButtonPressed();         //Called when the pin button is pressed, after updating overlay state
        virtual void OnWindowCloseButtonPressed();       //Called when the close button is pressed, after updating overlay state
        virtual bool IsVirtualWindowItemHovered() const; //Returns false by default, can be overridden to signal hover state of widgets that don't touch global ImGui state (used for blank space drag)

        void HelpMarker(const char* desc, const char* marker_str = "(?)") const;    //Help marker, but tooltip is fixed to top or bottom of the window
        void UpdateLimiterSetting(bool is_override) const;

        //Input widget for a collection of overlay tags. clip_parent_depth is the depth of parent window look up for popup's clipping rect, change when used in nested child windows
        static bool InputOverlayTags(const char* str_id, char* buffer_tags, size_t buffer_tags_size, FloatingWindowInputOverlayTagsState& state, int clip_parent_depth = 0, bool show_auto_tags = true);

        //Almost entire pages but implemented here to be shared between multiple windows
        bool ActionOrderList(ActionManager::ActionList& list_actions_target, bool is_appearing, bool is_returning, FloatingWindowActionOrderListState& state, 
                             bool& go_add_actions, float height_offset = 0.0f);
        bool ActionAddSelector(ActionManager::ActionList& list_actions_target, bool is_appearing, FloatingWindowActionAddSelectorState& state, float height_offset = 0.0f);

        //BeginTable(), but with some hacks to allow for compact, gap-less selectable + border around header (always set). Fairly specific, so not a generic ImGui extension
        //Text has to be aligned to frame padding with this
        bool BeginCompactTable(const char* str_id, int column, ImGuiTableFlags flags = 0, const ImVec2& outer_size = ImVec2(0.0f, 0.0f), float inner_width = 0.0f);
        void CompactTableHeadersRow();
        void EndCompactTable();

    public:
        FloatingWindow();
        virtual ~FloatingWindow() = default;
        void Update();                   //Not called when idling (no windows visible)
        virtual void UpdateVisibility(); //Only called in VR mode

        virtual void Show(bool skip_fade = false);
        virtual void Hide(bool skip_fade = false);
        void HideAll(bool skip_fade = false);       //Hide(), but applies to all overlay visibility states
        bool IsVisible() const;
        bool IsVisibleOrFading() const;  //Returns true if m_Visible is true *or* m_Alpha isn't 0 yet
        float GetAlpha() const;

        virtual void ApplyUIScale();

        bool CanUnpinRoom() const;
        bool IsPinned() const;
        void SetPinned(bool is_pinned, bool no_state_copy = false);  //no_state_copy = don't copy dashboard state to room (default behavior for pin button)

        FloatingWindowOverlayState& GetOverlayState(FloatingWindowOverlayStateID id);
        FloatingWindowOverlayStateID GetOverlayStateCurrentID();

        Matrix4& GetTransform();
        void SetTransform(const Matrix4& transform);
        virtual void ApplyCurrentOverlayState(); //Applies current absolute transform to the overlay if pinned and sets the width
        virtual void RebaseTransform();
        virtual void ResetTransformAll();
        virtual void ResetTransform(FloatingWindowOverlayStateID state_id);

        virtual void StartDrag(); //Starts a regular laser pointer drag of the window with the necessary parameters

        const ImVec2& GetPos() const;
        const ImVec2& GetSize() const;
        virtual vr::VROverlayHandle_t GetOverlayHandle() const = 0;

        static bool TranslatedComboAnimated(const char* label, int& value, TRMGRStrID trstr_min, TRMGRStrID trstr_max);
};