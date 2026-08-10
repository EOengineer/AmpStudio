#pragma once

#include <JuceHeader.h>
#include "TsGeofexVerify.h"

namespace ts
{
/** Runs Geofex + aliasing smoke checks once per process (Debug builds). */
inline void logVerificationOnce()
{
#if JUCE_DEBUG
    static bool done = false;
    if (done)
        return;
    done = true;
    const auto report = runAllTsVerifications();
    DBG (report.toString());
    jassert (report.allPassed());
#endif
}
} // namespace ts
