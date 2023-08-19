#pragma once

#include <algorithm>

#include "ConfigManager.h"

#ifndef DPLUS_UI
    #include "Overlays.h"   //UI app only deals with overlay config data
#endif

static const unsigned int k_ulOverlayID_None = UINT_MAX;      //Most functions return this on error, which will fall back to m_OverlayNull when requested
static const int k_lOverlayOutputErrorTextureWidth  = 960;    //Unfortunately the best option is to just hardcode the size in some places
static const int k_lOverlayOutputErrorTextureHeight = 540;
typedef std::vector<unsigned int> OverlayIDList;

class WindowInfo;

class OverlayManager
{
    public:

        struct TagListEntry
        {
            std::string Tag;
            bool IsAutoTag = false;
        };

    private:
        #ifndef DPLUS_UI
            std::vector<Overlay> m_Overlays;
            Overlay m_OverlayNull;
        #endif
        std::vector<OverlayConfigData> m_OverlayConfigData;

        unsigned int m_CurrentOverlayID;
        OverlayConfigData m_OverlayConfigDataNull;

        Matrix4 GetOverlayTransformBase(vr::VROverlayHandle_t ovrl_handle, unsigned int id, bool add_bottom_offset) const;

    public:
        static OverlayManager& Get();

        OverlayManager();
        unsigned int DuplicateOverlay(const OverlayConfigData& data, unsigned int source_id = k_ulOverlayID_None);
        unsigned int AddOverlay(OverlayCaptureSource capture_source, int desktop_id = -2, HWND window_handle = nullptr);
        unsigned int AddUIOverlay();                                    //Adds UI overlay without using any base data
        #ifndef DPLUS_UI
            Overlay& GetOverlay(unsigned int id);                       //Returns m_OverlayNull on error
            const Overlay& GetOverlay(unsigned int id) const;           //Returns m_OverlayNull on error
            Overlay& GetCurrentOverlay();
            Overlay& GetPrimaryDashboardOverlay();                      //Returns first visible overlay with dashboard origin, or m_OverlayNull if none exists
        #endif
        OverlayConfigData& GetConfigData(unsigned int id);
        const OverlayConfigData& GetConfigData(unsigned int id) const;
        OverlayConfigData& GetCurrentConfigData();
        OverlayOriginConfig GetOriginConfigFromData(const OverlayConfigData& data) const;

        unsigned int GetCurrentOverlayID() const;
        void SetCurrentOverlayID(unsigned int id);

        unsigned int FindOverlayID(vr::VROverlayHandle_t handle) const; //Returns k_ulOverlayID_None on error
        unsigned int GetOverlayCount() const;
        void SwapOverlays(unsigned int id, unsigned int id2);
        void RemoveOverlay(unsigned int id);
        void RemoveAllOverlays();

        #ifndef DPLUS_UI
            //Returns list of inactive (not current capturing) overlay IDs with winrt_last_* strings matching the given window
            OverlayIDList FindInactiveOverlaysForWindow(const WindowInfo& window_info) const;
        #endif

        //Returns list of overlay IDs using source_id as duplication ID
        OverlayIDList FindDuplicatedOverlaysForOverlay(unsigned int source_id) const;
        //Returns list of overlay IDs that contain the given tags
        OverlayIDList FindOverlaysWithTags(const char* str_tags) const;

        void ConvertDuplicatedOverlayToStandalone(unsigned int id, bool no_reset = false);

        #ifdef DPLUS_UI
            std::vector<TagListEntry> GetKnownOverlayTagList();

            void SetCurrentOverlayNameAuto(const WindowInfo* window_info = nullptr);
            void SetOverlayNameAuto(unsigned int id, const WindowInfo* window_info = nullptr); //window_info is optional, can be passed as override for when the handle isn't stored
            void SetOverlayNamesAutoForWindow(const WindowInfo& window_info);                  //Calls SetOverlayNameAuto() for all overlays currently using window_handle as source
        #endif

        Matrix4 GetOverlayMiddleTransform(unsigned int id, vr::VROverlayHandle_t ovrl_handle = vr::k_ulOverlayHandleInvalid) const;
        Matrix4 GetOverlayCenterBottomTransform(unsigned int id, vr::VROverlayHandle_t ovrl_handle = vr::k_ulOverlayHandleInvalid) const;

        static bool MatchOverlayTagSingle(const char* str_tags, const char* str_single_tag);
        static bool MatchOverlayTagSingle(const char* str_tags, const char* str_tags_end, const char* str_single_tag, size_t str_single_tag_length);
        //Returns if any tags in str a match with any in str b, optionally uses data b to match built-in auto tags from str a
        static bool MatchOverlayTags(const char* str_tags_a, const char* str_tags_b, const OverlayConfigData* data_b = nullptr);
};