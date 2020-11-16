#pragma once

#include "openvr.h"
#include "DPRect.h"
#include "OUtoSBSConverter.h"

//About the Overlay class:
//Overlay 0 (k_ulOverlayID_Dashboard) is the dashboard overlay. It always exists and is safe to access/returned when trying to access an invalid overlay id.
//OutputManager's m_OvrlHandleDesktopTexture holds the actual texture handle for every other desktop duplication overlay created by SteamVR
//This is *not* documented functionality in SteamVR, but it is the one with the best results.
//Additional overlays are also almost free except for the compositor rendering them.
//The alternative approach for this would be the documented way of using one texture handle for every overlay created by the overlay application, but this
//prevents SteamVR from using the advanced overlay texture filter, so we'd get blurry overlays. Not good.
//Given that variant exists, doing it this way is probably somewhat safe. It wouldn't be super hard to fix this up if it broke eventually, though.
//Using a separate texture for every overlay would be slower and take up more memory, so there's honestly no upside of that.

enum OverlayTextureSource
{
    ovrl_texsource_none,                                //Used with capture sources other than desktop duplication while capture is not active
    ovrl_texsource_desktop_duplication,
    ovrl_texsource_desktop_duplication_3dou_converted,
    ovrl_texsource_winrt_capture
};

class Overlay
{
    private:
        unsigned int m_ID;
        vr::VROverlayHandle_t m_OvrlHandle;
        bool m_Visible;                       //IVROverlay::IsOverlayVisible() is unreliable if the state changed during the same frame so we keep track ourselves
        float m_Opacity;                      //This is the opacity the overlay is currently set at, which may differ from what the config value is
        bool m_GlobalInteractive;             //True if VROverlayFlags_MakeOverlaysInteractiveIfVisible is set for this overlay
        DPRect m_ValidatedCropRect;           //Validated cropping rectangle used in OutputManager::Update() to check against dirty update regions
        OverlayTextureSource m_TextureSource;
        OUtoSBSConverter m_OUtoSBSConverter;

    public:
        Overlay(unsigned int id);
        Overlay(Overlay&& b);
        Overlay& operator=(Overlay&& b);
        ~Overlay();

        void InitOverlay();
        void AssignDesktopDuplicationTexture();
        unsigned int GetID() const;
        void SetID(unsigned int id);
        vr::VROverlayHandle_t GetHandle() const;
        void SetHandle(vr::VROverlayHandle_t handle);   //Sets the handle of the overlay without calling DestroyOverlay() on the previous one, used by OverlayManager

        void SetOpacity(float opacity);
        float GetOpacity() const;

        void SetVisible(bool visible);      //Call OutputManager::Show/HideOverlay() instead of this to properly manage duplication state based on active overlays
        bool IsVisible() const;
        bool ShouldBeVisible() const;

        void SetGlobalInteractiveFlag(bool interactive);
        bool GetGlobalInteractiveFlag();

        void UpdateValidatedCropRect();
        const DPRect& GetValidatedCropRect() const;

        void SetTextureSource(OverlayTextureSource tex_source);
        OverlayTextureSource GetTextureSource() const;
        void OnDesktopDuplicationUpdate();  //Called by OutputManager::RefreshOpenVROverlayTexture() for every overlay, but only if the texture has actually changed
};