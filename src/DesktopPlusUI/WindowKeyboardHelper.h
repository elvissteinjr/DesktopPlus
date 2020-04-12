#pragma once

#include "imgui.h"

//Window representing the keyboard helper
class WindowKeyboardHelper
{
    public:
        WindowKeyboardHelper();

        void Update();
        bool IsVisible();
        void Hide();
};