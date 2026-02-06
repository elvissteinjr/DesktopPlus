/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//This header and accompanying source file is based on
//AbstractQbit's Radial Follow Smoothing OpenTabletDriver Plugin (https://github.com/AbstractQbit/AbstractOTDPlugins) (RadialFollowCore.cs only)

#pragma once

#include "Util.h"

class RadialFollowCore
{
    public:
        double GetOuterRadius();
        void   SetOuterRadius(double value);

        double GetInnerRadius();
        void   SetInnerRadius(double value);

        double GetSmoothingCoefficient();
        void   SetSmoothingCoefficient(double value);

        double GetSoftKneeScale();
        void   SetSoftKneeScale(double value);

        double GetSmoothingLeakCoefficient();
        void   SetSmoothingLeakCoefficient(double value);

        void ApplyPresetSettings(int preset_id);

        float SampleRadialCurve(float dist);

        Vector2 Filter(Vector2 target);

    private:
        double m_RadiusOuter	   = 5.0;
        double m_RadiusInner       = 0.0;
        double m_SmoothingCoef     = 0.95;
        double m_SoftKneeScale     = 1.0;
        double m_SmoothingLeakCoef = 0.0;
        double m_GridScale		   = 1.0;

        Vector2 m_LastPos;
        ULONGLONG m_LastTick = 0;

        double m_XOffset   = -1.0;
        double m_ScaleComp =  1.0;

        void UpdateDerivedParams();

        //Math functions
        double KneeFunc(double x);
        double KneeScaled(double x);
        double InverseTanh(double x);
        double InverseKneeScaled(double x);
        double DeriveKneeScaled(double x);
        double GetXOffset();
        double GetScaleComp();
        double GetRadiusOuterAdjusted();
        double GetRadiusInnerAdjusted();
        double LeakedFn(double x, double offset, double scaleComp);
        double SmoothedFn(double x, double offset, double scaleComp);
        double ScaleToOuter(double x, double offset, double scaleComp);
        double DeltaFn(double x, double offset, double scaleComp);
};