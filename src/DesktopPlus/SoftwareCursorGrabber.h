#pragma once

#include <unordered_map>
#include "CommonTypes.h"

//Alternative method to grab the cursor image data
//Stores the data in the same way the DuplicationManager class does for Desktop Duplication cursor output to act as a drop-in alternative
class SoftwareCursorGrabber
{
    private:
        CURSORINFO m_CursorInfoLast = {};
        PTR_INFO m_DDPPointerInfo = {};
        std::unordered_map<HCURSOR, bool> m_CursorUseMaskCache;

        bool CopyMonoMask(const ICONINFO& icon_info);
        bool CopyColor(const ICONINFO& icon_info);

    public:
        bool SynthesizeDDPCursorInfo();
        PTR_INFO& GetDDPCursorInfo();
};
