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

        //Log things only once per session as it would be quite spammy otherwise
        bool m_LoggedOnceUsed = false;
        bool m_LoggedOnceFallbackDefault = false;
        bool m_LoggedOnceFallbackBlob = false;

        bool CopyMonoMask(const ICONINFO& icon_info);
        bool CopyColor(const ICONINFO& icon_info);
        static bool IsShapeBufferBlank(BYTE* psrc, size_t size, UINT type);

    public:
        bool SynthesizeDDPCursorInfo();
        PTR_INFO& GetDDPCursorInfo();
};
