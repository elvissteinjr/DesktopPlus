#pragma once

#include "util/DesktopWindow.h"

//Window used as an invisible dummy to act as a parent for the Graphics Capture picker window
struct PickerDummyWindow : util::desktop::DesktopWindow<PickerDummyWindow>
{
    static void RegisterWindowClass();

    PickerDummyWindow();
    ~PickerDummyWindow();

    void InitializeObjectWithWindowHandle(winrt::Windows::Foundation::IUnknown const& object)
    {
        auto initializer = object.as<util::desktop::IInitializeWithWindow>();
        winrt::check_hresult(initializer->Initialize(m_window));
    }

    LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam);
};