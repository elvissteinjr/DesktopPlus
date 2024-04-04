/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//This source file and accompanying header is based on
//AbstractQbit's Radial Follow Smoothing OpenTabletDriver Plugin (https://github.com/AbstractQbit/AbstractOTDPlugins) (RadialFollowCore.cs only)

#include "RadialFollowSmoothing.h"

#include <cmath>

double RadialFollowCore::GetOuterRadius()
{
    return m_RadiusOuter;
}

void RadialFollowCore::SetOuterRadius(double value)
{
    m_RadiusOuter = clamp(m_RadiusOuter, 0.0, 1000000.0);
}

double RadialFollowCore::GetInnerRadius()
{
    return m_RadiusInner;
}

void RadialFollowCore::SetInnerRadius(double value)
{
    m_RadiusInner = clamp(value, 0.0, 1000000.0);
}

double RadialFollowCore::GetSmoothingCoefficient()
{
    return m_SmoothingCoef;
}

void RadialFollowCore::SetSmoothingCoefficient(double value)
{
    m_SmoothingCoef = clamp(value, 0.0001, 1.0);
}

double RadialFollowCore::GetSoftKneeScale()
{
    return m_SoftKneeScale;
}

void RadialFollowCore::SetSoftKneeScale(double value)
{
    m_SoftKneeScale = clamp(value, 0.0, 100.0);
    UpdateDerivedParams();
}

double RadialFollowCore::GetSmoothingLeakCoefficient()
{
    return m_SmoothingLeakCoef;
}

void RadialFollowCore::SetSmoothingLeakCoefficient(double value)
{
    m_SmoothingLeakCoef = clamp(value, 0.0, 1.0);
}

void RadialFollowCore::ApplyPresetSettings(int preset_id)
{
    preset_id = clamp(preset_id, 0, 5);

    //These are just presets used by Desktop+, in hopes that they make sense for laser pointing and are easier to use than adjusting values directly
    switch (preset_id)
    {
        case 0: //Not really used, skip calling Filter() entirely instead
        {
            SetOuterRadius(0.0);
            SetInnerRadius(0.0);
            SetSmoothingCoefficient(0.0);
            SetSoftKneeScale(0.0);
            SetSmoothingLeakCoefficient(0.0);
            break;
        }
        case 1:
        {
            SetOuterRadius(5.0);
            SetInnerRadius(0.5);
            SetSmoothingCoefficient(0.95);
            SetSoftKneeScale(1.0);
            SetSmoothingLeakCoefficient(0.0);
            break;
        }
        case 2:
        {
            SetOuterRadius(5.0);
            SetInnerRadius(3.0);
            SetSmoothingCoefficient(0.95);
            SetSoftKneeScale(1.0);
            SetSmoothingLeakCoefficient(0.0);
            break;
        }
        case 3:
        {
            SetOuterRadius(7.5);
            SetInnerRadius(4.5);
            SetSmoothingCoefficient(1.0);
            SetSoftKneeScale(5.0);
            SetSmoothingLeakCoefficient(0.25);
            break;
        }
        case 4:
        {
            SetOuterRadius(12.5);
            SetInnerRadius(7.5);
            SetSmoothingCoefficient(1.0);
            SetSoftKneeScale(10.0);
            SetSmoothingLeakCoefficient(0.5);
            break;
        }
        case 5:
        {
            SetOuterRadius(32.0);
            SetInnerRadius(12.0);
            SetSmoothingCoefficient(1.0);
            SetSoftKneeScale(50.0);
            SetSmoothingLeakCoefficient(0.75);
            break;
        }
    }
}

float RadialFollowCore::SampleRadialCurve(float dist)
{
    return (float)DeltaFn(dist, m_XOffset, m_ScaleComp);
}

Vector2 RadialFollowCore::Filter(Vector2 target)
{
    Vector2 direction = target - m_LastPos;
    float distToMove = SampleRadialCurve(direction.length());
    direction.normalize();
    m_LastPos = m_LastPos + (direction * distToMove);

    //Catch NaNs and interrupted input
    if ( !((std::isfinite(m_LastPos.x)) && (std::isfinite(m_LastPos.y)) && (::GetTickCount64() <= m_LastTick + 50)) )
	    m_LastPos = target;

    m_LastTick = ::GetTickCount64();

    return m_LastPos;
}

void RadialFollowCore::UpdateDerivedParams()
{
	if (m_SoftKneeScale > 0.0001f)
	{
		m_XOffset   = GetXOffset();
		m_ScaleComp = GetScaleComp();
	}
	else //Calculating them with functions would use / by 0
	{
		m_XOffset   = -1.0;
		m_ScaleComp =  1.0;
	}
}

double RadialFollowCore::KneeFunc(double x)
{
    if (x < -3.0)
        return x;
    else if (x < 3.0)
        return log(tanh(exp(x)));
    else
        return 0.0;
}

double RadialFollowCore::KneeScaled(double x)
{
    if (m_SoftKneeScale > 0.0001)
        return m_SoftKneeScale * KneeFunc(x / m_SoftKneeScale) + 1.0;
    else
        return (x > 0.0) ? 1.0 : 1.0 + x;
}

double RadialFollowCore::InverseTanh(double x)
{
    return log((1.0 + x) / (1.0 - x)) / 2.0;
}

double RadialFollowCore::InverseKneeScaled(double x)
{
    return m_SoftKneeScale * log(InverseTanh(exp((x - 1.0) / m_SoftKneeScale)));
}

double RadialFollowCore::DeriveKneeScaled(double x)
{
    const double x_e = exp(x / m_SoftKneeScale);
    const double x_e_tanh = tanh(x_e);
    return (x_e - x_e * (x_e_tanh * x_e_tanh)) / x_e_tanh;
}

double RadialFollowCore::GetXOffset()
{
    return InverseKneeScaled(0.0);
}

double RadialFollowCore::GetScaleComp()
{
    return DeriveKneeScaled(GetXOffset());
}

double RadialFollowCore::GetRadiusOuterAdjusted()
{
    return m_GridScale * std::max(m_RadiusOuter, m_RadiusInner + 0.0001);
}

double RadialFollowCore::GetRadiusInnerAdjusted()
{
    return m_GridScale * m_RadiusInner;
}

double RadialFollowCore::LeakedFn(double x, double offset, double scaleComp)
{
    return KneeScaled(x + offset) * (1 - m_SmoothingLeakCoef) + x * m_SmoothingLeakCoef * scaleComp;
}

double RadialFollowCore::SmoothedFn(double x, double offset, double scaleComp)
{
    return LeakedFn(x * m_SmoothingCoef / scaleComp, offset, scaleComp);
}

double RadialFollowCore::ScaleToOuter(double x, double offset, double scaleComp)
{
    return (GetRadiusOuterAdjusted() - GetRadiusInnerAdjusted()) * SmoothedFn(x / (GetRadiusOuterAdjusted() - GetRadiusInnerAdjusted()), offset, scaleComp);
}

double RadialFollowCore::DeltaFn(double x, double offset, double scaleComp)
{
    return (x > GetRadiusInnerAdjusted()) ? x - ScaleToOuter(x - GetRadiusInnerAdjusted(), offset, scaleComp) - GetRadiusInnerAdjusted() : 0.0;
}
