#pragma once

#include "openvr.h"
#include "DPRect.h"

//About the Overlay class:
//Overlay 0 (k_ulOverlayID_Dashboard) is is the dashboard overlay. It always exists and is safe to access/returned when trying to access an invalid overlay id.
//OutputManager's m_OvrlHandleDesktopTexture holds the actual texture handle for every other overlay created by SteamVR
//This is *not* documented functionality in SteamVR, but it is the one with the best results.
//Additional overlays are also almost free except for the compositor rendering them.
//The alternative approach for this would be the documented way of using one texture handle for every overlay created by the overlay application, but this
//prevents SteamVR from using the advanced overlay texture filter, so we'd get blurry overlays. Not good.
//Given that variant exists, doing it this way is probably somewhat safe. It wouldn't be super hard to fix this up if it broke eventually, though.
//Using a separate texture for every overlay would be slower and take up more memory, so there's honestly no upside of that.

class Overlay
{
    private:
        unsigned int m_ID;
        vr::VROverlayHandle_t m_OvrlHandle;
        bool m_Visible;                     //IVROverlay::IsOverlayVisible() is unreliable if the state changed during the same frame so we keep track ourselves
        float m_Opacity;                    //This is the opacity the overlay is currently set at, which may differ from what the config value is
        bool m_GlobalInteractive;           //True if VROverlayFlags_MakeOverlaysInteractiveIfVisible is set for this overlay
        DPRect m_ValidatedCropRect;         //Validated cropping rectangle used in OutputManager::Update() to check against dirty update regions

    public:
        Overlay(unsigned int id);
        Overlay(Overlay&& b);
        Overlay& operator=(Overlay&& b);
        ~Overlay();

        void InitOverlay();
        void AssignTexture();
        unsigned int GetID() const;
        void SetID(unsigned int id);
        vr::VROverlayHandle_t GetHandle() const;

        void SetOpacity(float opacity);
        float GetOpacity() const;

        void SetVisible(bool visible);      //Call OutputManager::Show/HideOverlay() instead of this to properly manage duplication state based on active overlays
        bool IsVisible() const;
        bool ShouldBeVisible() const;

        void SetGlobalInteractiveFlag(bool interactive);
        bool GetGlobalInteractiveFlag();

        void UpdateValidatedCropRect();
        const DPRect& GetValidatedCropRect() const;
};