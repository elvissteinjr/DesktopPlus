// Desktop+UI: Modified for OpenVR compatibility

// dear imgui: Platform Binding for Windows (standard windows API for 32 and 64 bits applications)
// This needs to be used along with a Renderer (e.g. DirectX11, OpenGL3, Vulkan..)

// Implemented features:
//  [X] Platform: Clipboard support (for Win32 this is actually part of core imgui)
//  [X] Platform: Mouse cursor shape and visibility. Disable with 'io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange'.
//  [X] Platform: Keyboard arrays indexed using VK_* Virtual Key Codes, e.g. ImGui::IsKeyPressed(VK_SPACE).
//  [X] Platform: Gamepad support. Enabled with 'io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad'.

#pragma once

IMGUI_IMPL_API bool     ImGui_ImplWin32_Init(void* hwnd);
IMGUI_IMPL_API void     ImGui_ImplWin32_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplWin32_NewFrame();

// Handler for Win32 messages, update mouse/keyboard data.
// You may or not need this for your implementation, but it can serve as reference for handling inputs.
// Intentionally commented out to avoid dragging dependencies on <windows.h> types. You can COPY this line into your .cpp code instead.
/*
IMGUI_IMPL_API LRESULT  ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
*/

#include "openvr.h"

// If this is called, do not call ImGui_ImplWin32_NewFrame(). ImGui_ImplWin32_NewFrame() can still be used for a normal desktop mode or similar.
IMGUI_IMPL_API void ImGui_ImplOpenVR_NewFrame();

// Handler for OpenVR Events
// Returns true if the event was handled
IMGUI_IMPL_API bool ImGui_ImplOpenVR_InputEventHandler(const vr::VREvent_t& vr_event);

// Resets internal extra keyboard state for VR keyboard handling
// This needs to be called before a NewFrame and handling the OpenVR events
IMGUI_IMPL_API void ImGui_ImplOpenVR_InputResetVRKeyboard(vr::VROverlayHandle_t overlay_handle);

// Called on VREvent_KeyboardClosed
// Exposed for application to call if the keyboard is handled by something else
IMGUI_IMPL_API void ImGui_ImplOpenVR_InputOnVRKeyboardClosed();

// Called on VREvent_KeyboardCharInput
// Exposed for application to call if the keyboard is handled by something else
IMGUI_IMPL_API void ImGui_ImplOpenVR_AddInputFromOSK(const char* input);

// Set overlay intersection mask from current top-level window outer rects
IMGUI_IMPL_API void ImGui_ImplOpenVR_SetIntersectionMaskFromWindows(vr::VROverlayHandle_t overlay_handle);

