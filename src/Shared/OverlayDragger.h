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

        Matrix4 m_DragModeMatrixTargetStart;
        Matrix4 m_DragModeMatrixSourceStart;
        Matrix4 m_DragModeMatrixTargetCurrent;

        bool  m_DragGestureActive;
        float m_DragGestureScaleDistanceStart;
        float m_DragGestureScaleWidthStart;
        float m_DragGestureScaleDistanceLast;
        Matrix4 m_DragGestureRotateMatLast;

        bool m_AbsoluteModeActive;              //Absolute mode forces the overlay to stay centered on the controller tip + offset
        float m_AbsoluteModeOffsetForward;

        Matrix4 m_DashboardMatLast;
        float m_DashboardHMD_Y;                 //The HMDs y-position when the dashboard was activated. Used for dashboard-relative positioning

        void DragStartBase(bool is_gesture_drag = false);
        void DragGestureStartBase();

        void TransformForceUpright(Matrix4& transform) const;

    public:
        OverlayDragger();

        Matrix4 GetBaseOffsetMatrix();
        Matrix4 GetBaseOffsetMatrix(OverlayOrigin overlay_origin);

        void DragStart(unsigned int overlay_id);
        void DragStart(vr::VROverlayHandle_t overlay_handle, OverlayOrigin overlay_origin = ovrl_origin_room);
        void DragUpdate();
        void DragAddDistance(float distance);
        float DragAddWidth(float width);                            //Returns new width
        Matrix4 DragFinish();                                       //Returns new overlay origin-relative transform
        void DragCancel();                                          //Stops drag without applying any changes

        void DragGestureStart(unsigned int overlay_id);
        void DragGestureStart(vr::VROverlayHandle_t overlay_handle, OverlayOrigin overlay_origin = ovrl_origin_room);
        void DragGestureUpdate();
        Matrix4 DragGestureFinish();                                //Returns new overlay origin-relative transform

        void AbsoluteModeSet(bool is_active, float offset_forward); //Automatically reset on DragFinish()

        void UpdateDashboardHMD_Y();

        bool IsDragActive() const;                                  //Doesn't include active gesture drag
        bool IsDragGestureActive() const;
        int GetDragDeviceID() const;
        unsigned int GetDragOverlayID() const;
        vr::VROverlayHandle_t GetDragOverlayHandle() const;
        const Matrix4& GetDragOverlayMatrix() const;                //Only valid while IsDragActive() returns true
};