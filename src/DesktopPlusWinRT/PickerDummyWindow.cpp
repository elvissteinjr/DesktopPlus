#ifndef DPLUSWINRT_STUB

#include "CommonHeaders.h"
#include "CaptureManager.h"
#include "PickerDummyWindow.h"
#include "Util.h"


void PickerDummyWindow::RegisterWindowClass()
{
    WNDCLASSEX wcex = { sizeof(wcex) };
    wcex.style = 0;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = GetModuleHandleW(nullptr);
    wcex.hIcon = nullptr;
    wcex.hCursor = nullptr;
    wcex.hbrBackground = nullptr;
    wcex.lpszClassName = L"DesktopPlusWinRTDummy";
    wcex.hIconSm = nullptr;
    RegisterClassExW(&wcex);
}

PickerDummyWindow::PickerDummyWindow()
{
    //WS_EX_TOOLWINDOW prevents it from being in Alt+Tab and taskbar, which is intentional since it's visually awkward otherwise
    //To prevent it being lost behind windows we put it as topmost. This could be considered annoying, but even more annoying is
    //to lose the window and have the picker hang around after the app already gone (yeah that happens)
    ::CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"DesktopPlusWinRTDummy", L"Desktop+", WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), this);

    //Picker needs a foreground window, but we don't really want to show anything, so cloak it
    BOOL do_cloak = true;
    ::DwmSetWindowAttribute(m_window, DWMWA_CLOAK, &do_cloak, sizeof(do_cloak));

    //Center it so the picker is centered as well
    CenterWindowToMonitor(m_window, true);

    ::ShowWindow(m_window, SW_SHOW);
    ::SetForegroundWindow(m_window);
}

PickerDummyWindow::~PickerDummyWindow()
{
    ::DestroyWindow(m_window);
}

LRESULT PickerDummyWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam)
{
    if (message == WM_DESTROY)
    {
        return 0;
    }

    return DefWindowProc(m_window, message, wparam, lparam);
}

#endif //DPLUSWINRT_STUB