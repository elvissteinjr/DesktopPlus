#pragma once

#include <Unknwn.h>
#include <inspectable.h>

#include <wil/cppwinrt.h>

// WinRT
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>

#include <DispatcherQueue.h>

// STL
#include <atomic>
#include <memory>
#include <algorithm>
#include <vector>

// D3D
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d2d1_3.h>
#include <wincodec.h>

// DWM
#include <dwmapi.h>

// WIL
#include <wil/resource.h>

// Helpers
#include "util/direct3d11.interop.h"
#include "util/capture.desktop.interop.h"
#include "util/dispatcherqueue.desktop.interop.h"
#include "util/hwnd.interop.h"