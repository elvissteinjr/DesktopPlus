#pragma once

#include "openvr.h"

class BackgroundOverlay
{
    private:
        vr::VROverlayHandle_t m_OvrlHandle;

    public:
        BackgroundOverlay();
        ~BackgroundOverlay();
        void Update();
};