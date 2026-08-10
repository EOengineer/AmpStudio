// Standalone Geofex / aliasing checks (no JUCE).
// Build: clang++ -std=c++17 -O2 -I Source tools/ts_geofex_verify_main.cpp -o tools/ts_geofex_verify

#include "dsp/fx/ts/TsGeofexVerify.h"
#include <iostream>

int main()
{
    const auto report = ts::runAllTsVerifications();
    std::cout << report.toString();
    return report.allPassed() ? 0 : 1;
}
