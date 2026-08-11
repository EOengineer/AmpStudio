// Standalone Champ 5F1 schematic / Newton checks (no JUCE).
// Build: clang++ -std=c++17 -O2 -I Source tools/champ_verify_main.cpp -o tools/champ_verify

#include "dsp/amps/champ/ChampVerify.h"
#include <iostream>

int main()
{
    const auto report = champ::runAllChampVerifications();
    std::cout << report.toString();
    return report.allPassed() ? 0 : 1;
}
