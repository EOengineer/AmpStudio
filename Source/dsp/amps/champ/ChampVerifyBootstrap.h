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
    // Do not jassert here — a failed check must not abort prepare / mute the amp.
#endif
}
} // namespace champ
