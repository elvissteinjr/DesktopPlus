#pragma once
#include "hwnd.interop.h"

namespace util::desktop
{
    // adapted from https://gist.github.com/kennykerr/64e0248323267b9b158acd26b51b3c8b
    template <typename T>
    struct DesktopWindow
    {
        using base_type = DesktopWindow<T>;
        HWND m_window = nullptr;

        static T* GetThisFromHandle(HWND const window) noexcept
        {
            return reinterpret_cast<T*>(GetWindowLongPtr(window, GWLP_USERDATA));
        }

        static LRESULT __stdcall WndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
        {
            WINRT_ASSERT(window);

            if (WM_NCCREATE == message)
            {
                auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
                T* that = static_cast<T*>(cs->lpCreateParams);
                WINRT_ASSERT(that);
                WINRT_ASSERT(!that->m_window);
                that->m_window = window;
                SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
            }
            else if (T* that = GetThisFromHandle(window))
            {
                return that->MessageHandler(message, wparam, lparam);
            }

            return DefWindowProc(window, message, wparam, lparam);
        }

        LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
        {
            if (WM_DESTROY == message)
            {
                PostQuitMessage(0);
                return 0;
            }

            return DefWindowProc(m_window, message, wparam, lparam);
        }

        void InitializeObjectWithWindowHandle(winrt::Windows::Foundation::IUnknown const& object)
        {
            auto initializer = object.as<IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(m_window));
        }
    };
}

