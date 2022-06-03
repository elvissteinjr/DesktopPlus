#pragma once

#include "DPBrowserAPI.h"

class DPBrowserAPIClient : public DPBrowserAPI
{
    private:
        bool m_IsServerAvailable = false;
        bool m_HasServerAPIMismatch = false;
        HWND m_ServerWindowHandle = nullptr;
        UINT m_Win32MessageID = 0;

        std::string m_IPCStrings[dpbrowser_ipcstr_MAX - dpbrowser_ipcstr_MIN];
        vr::VROverlayHandle_t m_IPCOverlayTarget = vr::k_ulOverlayHandleInvalid;

        //Pending settings stored here when server isn't running yet and applied later on launch
        int m_PendingSettingGlobalFPS = -1;

        bool LaunchServerIfNotRunning();                        //Should be called and checked for in most API implementations, also makes sure m_ServerWindowHandle is updated
        void SendStringMessage(DPBrowserICPStringID str_id, const std::string& str) const;

        std::string& GetIPCString(DPBrowserICPStringID str_id); //Abstracts the minimum string ID away when acccessing m_IPCStrings

    public:
        static DPBrowserAPIClient& Get();

        bool Init();
        void Quit();
        bool IsBrowserAvailable() const;
        DWORD GetServerAppProcessID();
        UINT GetRegisteredMessageID() const;

        void HandleIPCMessage(const MSG& msg);

        //DPBrowserAPI:
        virtual void DPBrowser_StartBrowser(vr::VROverlayHandle_t overlay_handle, const std::string& url, bool use_transparent_background) override;
        virtual void DPBrowser_DuplicateBrowserOutput(vr::VROverlayHandle_t overlay_handle_src, vr::VROverlayHandle_t overlay_handle_dst) override;
        virtual void DPBrowser_PauseBrowser(vr::VROverlayHandle_t overlay_handle, bool pause) override;
        virtual void DPBrowser_RecreateBrowser(vr::VROverlayHandle_t overlay_handle, bool use_transparent_background) override;
        virtual void DPBrowser_StopBrowser(vr::VROverlayHandle_t overlay_handle) override;

        virtual void DPBrowser_SetURL(vr::VROverlayHandle_t overlay_handle, const std::string& url) override;
        virtual void DPBrowser_SetResolution(vr::VROverlayHandle_t overlay_handle, int width, int height) override;
        virtual void DPBrowser_SetFPS(vr::VROverlayHandle_t overlay_handle, int fps) override;
        virtual void DPBrowser_SetZoomLevel(vr::VROverlayHandle_t overlay_handle, float zoom_level) override;

        virtual void DPBrowser_MouseMove(vr::VROverlayHandle_t overlay_handle, int x, int y) override;
        virtual void DPBrowser_MouseLeave(vr::VROverlayHandle_t overlay_handle) override;
        virtual void DPBrowser_MouseDown(vr::VROverlayHandle_t overlay_handle, vr::EVRMouseButton button) override;
        virtual void DPBrowser_MouseUp(vr::VROverlayHandle_t overlay_handle, vr::EVRMouseButton button) override;
        virtual void DPBrowser_Scroll(vr::VROverlayHandle_t overlay_handle, float x_delta, float y_delta) override;

        virtual void DPBrowser_KeyboardSetKeyState(vr::VROverlayHandle_t overlay_handle, DPBrowserIPCKeyboardKeystateFlags flags, unsigned char keycode) override;
        virtual void DPBrowser_KeyboardTypeWChar(vr::VROverlayHandle_t overlay_handle, wchar_t wchar, bool down) override;
        virtual void DPBrowser_KeyboardTypeString(vr::VROverlayHandle_t overlay_handle, const std::string& str) override;

        virtual void DPBrowser_GoBack(vr::VROverlayHandle_t overlay_handle) override;
        virtual void DPBrowser_GoForward(vr::VROverlayHandle_t overlay_handle) override;
        virtual void DPBrowser_Refresh(vr::VROverlayHandle_t overlay_handle) override;

        virtual void DPBrowser_GlobalSetFPS(int fps) override;
};