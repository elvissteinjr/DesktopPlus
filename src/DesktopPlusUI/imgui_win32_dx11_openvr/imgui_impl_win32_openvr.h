// Desktop+UI: Modified for OpenVR compatibility

// dear imgui: Platform Backend for Windows (standard windows API for 32 and 64 bits applications)
// This needs to be used along with a Renderer (e.g. DirectX11, OpenGL3, Vulkan..)

// Implemented features:
//  [X] Platform: Clipboard support (for Win32 this is actually part of core dear imgui)
//  [X] Platform: Mouse cursor shape and visibility. Disable with 'io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange'.
//  [X] Platform: Keyboard arrays indexed using VK_* Virtual Key Codes, e.g. ImGui::IsKeyPressed(VK_SPACE).
//  [X] Platform: Gamepad support. Enabled with 'io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad'.

// You can copy and use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#pragma once

IMGUI_IMPL_API bool     ImGui_ImplWin32_Init(void* hwnd);
IMGUI_IMPL_API void     ImGui_ImplWin32_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplWin32_NewFrame();

// Configuration
// - Disable gamepad support or linking with xinput.lib
//#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
//#define IMGUI_IMPL_WIN32_DISABLE_LINKING_XINPUT
																										  
// Win32 message handler your application need to call.
// - Intentionally commented out in a '#if 0' block to avoid dragging dependencies on <windows.h> from this helper.
// - You should COPY the line below into your .cpp code to forward declare the function and then you can call it.
#if 0
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

// DPI-related helpers (optional)
// - Use to enable DPI awareness without having to create an application manifest.
// - Your own app may already do this via a manifest or explicit calls. This is mostly useful for our examples/ apps.
// - In theory we could call simple functions from Windows SDK such as SetProcessDPIAware(), SetProcessDpiAwareness(), etc.
//   but most of the functions provided by Microsoft require Windows 8.1/10+ SDK at compile time and Windows 8/10+ at runtime,
//   neither we want to require the user to have. So we dynamically select and load those functions to avoid dependencies.
IMGUI_IMPL_API void     ImGui_ImplWin32_EnableDpiAwareness();
IMGUI_IMPL_API float    ImGui_ImplWin32_GetDpiScaleForHwnd(void* hwnd);       // HWND hwnd
IMGUI_IMPL_API float    ImGui_ImplWin32_GetDpiScaleForMonitor(void* monitor); // HMONITOR monitor

// Transparency related helpers (optional) [experimental]
// - Use to enable alpha compositing transparency with the desktop.
// - Use together with e.g. clearing your framebuffer with zero-alpha.
IMGUI_IMPL_API void     ImGui_ImplWin32_EnableAlphaCompositing(void* hwnd);   // HWND hwnd

#include "openvr.h"

// If this is called, do not call ImGui_ImplWin32_NewFrame(). ImGui_ImplWin32_NewFrame() can still be used for a normal desktop mode or similar.
IMGUI_IMPL_API void ImGui_ImplOpenVR_NewFrame();

// Handler for OpenVR Events
// Returns true if the event was handled
IMGUI_IMPL_API bool ImGui_ImplOpenVR_InputEventHandler(const vr::VREvent_t& vr_event);

// Resets internal extra keyboard state for VR keyboard handling
// This needs to be called before a NewFrame and handling the OpenVR events
// Returns the overlay error returned by IVROverlay::ShowKeyboardForOverlay()
IMGUI_IMPL_API vr::EVROverlayError ImGui_ImplOpenVR_InputResetVRKeyboard(vr::VROverlayHandle_t overlay_handle);

// Called on VREvent_KeyboardClosed
// Exposed for application to call if the keyboard is handled by something else
IMGUI_IMPL_API void ImGui_ImplOpenVR_InputOnVRKeyboardClosed();

// Called on VREvent_KeyboardCharInput
// Exposed for application to call if the keyboard is handled by something else
IMGUI_IMPL_API void ImGui_ImplOpenVR_AddInputFromOSK(const char* input);

// Set overlay intersection mask from current top-level window outer rects
IMGUI_IMPL_API void ImGui_ImplOpenVR_SetIntersectionMaskFromWindows(vr::VROverlayHandle_t overlay_handle);
