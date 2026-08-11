#pragma once

#include <JuceHeader.h>
#include "ChampVerify.h"

namespace champ
{
/** Runs Champ schematic / Newton checks once per process (Debug builds). */
inline void logVerificationOnce()
{
#if JUCE_DEBUG
    static bool done = false;
    if (done)
        return;
    done = true;
    const auto report = runAllChampVerifications();
    DBG (report.toString());
    jassert (report.allPassed());
#endif
}
} // namespace champ
