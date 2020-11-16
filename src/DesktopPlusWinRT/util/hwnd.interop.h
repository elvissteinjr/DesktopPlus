#pragma once
#include <Unknwn.h>

namespace util::desktop
{
    // Taken from shobjidl_core.h
    struct __declspec(uuid("3E68D4BD-7135-4D10-8018-9FB6D9F33FA1"))
        IInitializeWithWindow : ::IUnknown
    {
        virtual HRESULT __stdcall Initialize(HWND hwnd) = 0;
    };
}
