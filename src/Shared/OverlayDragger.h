#pragma once

#include "Matrices.h"
#include "ConfigManager.h"

//Class handling dragging overlays with motion controllers, with support for all Desktop+ overlay origins
class OverlayDragger
{
    private:
        int m_DragModeDeviceID;                 //-1 if not dragging
        unsigned int m_DragModeOverlayID;
        vr::VROverlayHandle_t m_DragModeOverlayHandle;
        OverlayOrigin m_DragModeOverlayOrigin;
        OverlayOriginConfig m_DragModeOverlayOriginConfig;
        float m_DragModeMaxWidth;

        Matrix4 m_DragModeMatrixTargetStart;
        Matrix4 m_DragModeMatrixSourceStart;
        Matrix4 m_DragModeMatrixTargetCurrent;
        float m_DragModeSnappedExtraWidth;

        bool  m_DragGestureActive;
        float m_DragGestureScaleDistanceStart;
        float m_DragGestureScaleWidthStart;
        float m_DragGestureScaleDistanceLast;
        Matrix4 m_DragGestureRotateMatLast;

        bool m_AbsoluteModeActive;              //Absolute mode forces the overlay to stay centered on the controller tip + offset
        float m_AbsoluteModeOffsetForward;

        Matrix4 m_DashboardMatLast;
        float m_DashboardHMD_Y;                 //The HMDs y-position when the dashboard was activated. Used for dashboard-relative positioning
        Vector3 m_TempStandingPosition;         //Standing position updated when the dashboard tab is activated or drag-mode activated while dashboard tab closed. Used as semi-fixed reference point

        void DragStartBase(bool is_gesture_drag = false);
        void DragGestureStartBase();

        void TransformForceUpright(Matrix4& transform) const;
        void TransformForceDistance(Matrix4& transform, Vector3 reference_pos, float distance, bool use_cylinder_shape = false, bool auto_tilt = false) const;

    public:
        OverlayDragger();

        Matrix4 GetBaseOffsetMatrix();
        Matrix4 GetBaseOffsetMatrix(OverlayOrigin overlay_origin);  //Not recommended to use with origins that have config values
        Matrix4 GetBaseOffsetMatrix(OverlayOrigin overlay_origin, const OverlayOriginConfig& origin_config);
        void ApplyDashboardScale(Matrix4& matrix);

        void DragStart(unsigned int overlay_id);
        void DragStart(vr::VROverlayHandle_t overlay_handle, OverlayOrigin overlay_origin = ovrl_origin_room); //Not recommended to use with origins that have config values
        void DragUpdate();
        void DragAddDistance(float distance);
        float DragAddWidth(float width);                            //Returns new width
        void DragSetMaxWidth(float max_width);                      //Maximum width applied whenever width would otherwise change. Resets on drag start, so set after
        Matrix4 DragFinish();                                       //Returns new overlay origin-relative transform
        void DragCancel();                                          //Stops drag without applying any changes

        void DragGestureStart(unsigned int overlay_id);
        void DragGestureStart(vr::VROverlayHandle_t overlay_handle, OverlayOrigin overlay_origin = ovrl_origin_room);
        void DragGestureUpdate();
        Matrix4 DragGestureFinish();                                //Returns new overlay origin-relative transform

        void AbsoluteModeSet(bool is_active, float offset_forward); //Automatically reset on DragFinish()

        void UpdateDashboardHMD_Y();
        void UpdateTempStandingPosition();

        bool IsDragActive() const;                                  //Doesn't include active gesture drag
        bool IsDragGestureActive() const;
        int GetDragDeviceID() const;
        unsigned int GetDragOverlayID() const;
        vr::VROverlayHandle_t GetDragOverlayHandle() const;
        const Matrix4& GetDragOverlayMatrix() const;                //Only valid while IsDragActive() returns true
};