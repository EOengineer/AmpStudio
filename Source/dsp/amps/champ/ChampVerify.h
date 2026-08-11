#pragma once

#include "ChampComponents.h"
#include "ChampPowerStage.h"
#include "ChampTriodeStage.h"
#include "../../cabs/SpeakerRlc.h"
#include "../../circuit/RcFilters.h"
#include "../../circuit/TubeModel.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace champ
{
struct VerifyItem
{
    std::string name;
    bool passed = false;
    std::string detail;
};

struct VerifyReport
{
    std::vector<VerifyItem> items;
    int passCount = 0;
    int failCount = 0;

    void add (VerifyItem item)
    {
        if (item.passed)
            ++passCount;
        else
            ++failCount;
        items.push_back (std::move (item));
    }

    bool allPassed() const noexcept { return failCount == 0; }

    std::string toString() const
    {
        std::ostringstream os;
        os << "Champ 5F1 verification: " << passCount << " passed, "
           << failCount << " failed\n";
        for (const auto& it : items)
            os << (it.passed ? "  [PASS] " : "  [FAIL] ") << it.name << " — " << it.detail << "\n";
        return os.str();
    }
};

inline bool near (float a, float b, float relTol, float absTol = 1.0f) noexcept
{
    return std::abs (a - b) <= absTol + relTol * std::max (std::abs (a), std::abs (b));
}

/** Base-rate (no JUCE OS) full path: V1A → C1 → V1B → C2 → 6V6 into flat 8 Ω. */
struct EndToEndProbe
{
    bool finiteFirst = true;
    bool finiteAll = true;
    float firstAc1 = 0.0f;
    float firstAc2 = 0.0f;
    float idlePlate1 = 0.0f;
    float idlePlate2 = 0.0f;
    float idleVk1 = 0.0f;
    float idleIp1mA = 0.0f;
    float peakAc1 = 0.0f;
    float peakAc2 = 0.0f;
    float peakVs = 0.0f;
    float rmsAc1 = 0.0f;
    float rmsAc2 = 0.0f;
    float rmsVs = 0.0f;
    float rmsDigital = 0.0f;

    std::string toDetail() const
    {
        std::ostringstream d;
        d << "first ac1=" << firstAc1 << " ac2=" << firstAc2
          << " idle Vp1=" << idlePlate1 << " Vp2=" << idlePlate2
          << " Vk1=" << idleVk1 << " Ip1=" << idleIp1mA << " mA"
          << " peak ac1=" << peakAc1 << " ac2=" << peakAc2 << " Vs=" << peakVs
          << " rms ac1=" << rmsAc1 << " ac2=" << rmsAc2
          << " Vs=" << rmsVs << " digital=" << rmsDigital
          << " finite0=" << (finiteFirst ? "yes" : "NO")
          << " finiteAll=" << (finiteAll ? "yes" : "NO");
        return d.str();
    }
};

inline EndToEndProbe runEndToEndPath (float fs = 48000.0f) noexcept
{
    EndToEndProbe p;
    ComponentSet stock;

    ChampTriodeStage v1a;
    CouplingHp couple1;
    ChampTriodeStage v1b;
    CouplingHp couple2;
    ChampPowerStage power;

    v1a.prepare (fs, stock.v1aPlateR, stock.v1aCathodeR,
                 stock.v1aBypassC, stock.bplusPreamp, 0.0f);
    couple1.prepare (stock.couplingC1, stock.volumePotR, fs);
    couple1.seedFromPlate (v1a.getPlate());

    v1b.prepare (fs, stock.v1bPlateR, stock.v1bCathodeR,
                 1.0e-12f, stock.bplusPreamp, stock.nfbR);
    couple2.prepare (stock.couplingC2, stock.powerGridLeakR, fs);
    couple2.seedFromPlate (v1b.getPlate());

    power.prepare (fs, stock);
    power.setSpeakerRlc (cab::makePreset (cab::ImpedancePreset::flat8));

    p.idlePlate1 = v1a.getPlate();
    p.idlePlate2 = v1b.getPlate();
    p.idleVk1 = v1a.getCathode();
    p.idleIp1mA = (stock.bplusPreamp - p.idlePlate1) / stock.v1aPlateR * 1000.0f;

    const float vol = volumeFraction (1.0f);
    float lastVs = 0.0f;
    double sumAc1 = 0.0, sumAc2 = 0.0, sumVs = 0.0, sumDig = 0.0;
    int rmsN = 0;

    constexpr int kZeroSamples = 64;
    constexpr int kToneSamples = 2400; // 50 ms @ 48 kHz
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kHz = 1000.0f;
    constexpr float kAmp = 0.05f; // 50 mV grid, guitar-ish

    auto tick = [&] (float vin, bool accumulateRms, bool isFirst)
    {
        const float plate1 = v1a.processSample (vin, 0.0f);
        const float ac1 = couple1.processFromPlate (plate1) * vol;
        const float plate2 = v1b.processSample (ac1, lastVs);
        const float ac2 = couple2.processFromPlate (plate2);
        lastVs = power.processSample (ac2);
        const float digital = speakerVoltsToDigital (lastVs);

        const bool ok = std::isfinite (plate1) && std::isfinite (ac1)
                     && std::isfinite (plate2) && std::isfinite (ac2)
                     && std::isfinite (lastVs) && std::isfinite (digital);
        if (! ok)
            p.finiteAll = false;

        if (isFirst)
        {
            p.finiteFirst = ok;
            p.firstAc1 = ac1;
            p.firstAc2 = ac2;
        }

        p.peakAc1 = std::max (p.peakAc1, std::abs (ac1));
        p.peakAc2 = std::max (p.peakAc2, std::abs (ac2));
        p.peakVs = std::max (p.peakVs, std::abs (lastVs));

        if (accumulateRms)
        {
            sumAc1 += (double) ac1 * (double) ac1;
            sumAc2 += (double) ac2 * (double) ac2;
            sumVs += (double) lastVs * (double) lastVs;
            sumDig += (double) digital * (double) digital;
            ++rmsN;
        }
    };

    tick (0.0f, false, true);
    for (int i = 1; i < kZeroSamples; ++i)
        tick (0.0f, false, false);

    for (int i = 0; i < kToneSamples; ++i)
    {
        const float vin = kAmp * std::sin (2.0f * kPi * kHz * (float) i / fs);
        tick (vin, true, false);
    }

    if (rmsN > 0)
    {
        p.rmsAc1 = (float) std::sqrt (sumAc1 / (double) rmsN);
        p.rmsAc2 = (float) std::sqrt (sumAc2 / (double) rmsN);
        p.rmsVs = (float) std::sqrt (sumVs / (double) rmsN);
        p.rmsDigital = (float) std::sqrt (sumDig / (double) rmsN);
    }

    return p;
}

inline VerifyReport runChampVerification()
{
    VerifyReport report;
    ComponentSet stock;

    // Coupling C1 into 1M volume ≈ 8 Hz
    {
        const float f = CouplingHp::cornerHz (stock.couplingC1, stock.volumePotR);
        const bool ok = near (f, Comp::kCoupling1CornerIntoVol, 0.05f, 1.0f);
        std::ostringstream d;
        d << "f=" << f << " Hz (target ~" << Comp::kCoupling1CornerIntoVol << " Hz)";
        report.add ({ "V1A coupling → volume corner", ok, d.str() });
    }

    // Coupling C2 into 220k grid leak ≈ 36 Hz
    {
        const float f = CouplingHp::cornerHz (stock.couplingC2, stock.powerGridLeakR);
        const bool ok = near (f, Comp::kCoupling2CornerIntoGrid, 0.05f, 2.0f);
        std::ostringstream d;
        d << "f=" << f << " Hz (target ~" << Comp::kCoupling2CornerIntoGrid << " Hz)";
        report.add ({ "V1B coupling → 6V6 grid corner", ok, d.str() });
    }

    // NFB loop ratio (22k+1.5k)/1.5k ≈ 15.67
    {
        const float r = ComponentSet::nfbLoopRatio();
        const bool ok = near (r, Comp::kNfbLoopRatio, 0.01f, 0.05f);
        std::ostringstream d;
        d << "ratio=" << r << " (target " << Comp::kNfbLoopRatio << ")";
        report.add ({ "NFB loop ratio", ok, d.str() });
    }

    // OT turns ratio sqrt(5k/8)
    {
        const float n = stock.turnsRatio();
        const float target = std::sqrt (Comp::kOtPrimaryZ / Comp::kOtSecondaryZ);
        const bool ok = near (n, target, 0.01f, 0.05f);
        std::ostringstream d;
        d << "n=" << n << " (target " << target << ")";
        report.add ({ "OT turns ratio (5k:8)", ok, d.str() });
    }

    // 12AX7 idle from the solved V1A island (self-consistent Champ bias)
    {
        ChampTriodeStage stage;
        stage.prepare (48000.0f, Comp::kV1aPlateR, Comp::kV1aCathodeR,
                       Comp::kV1aBypassC, Comp::kBplusPreamp, 0.0f);
        const float ip = (Comp::kBplusPreamp - stage.getPlate()) / Comp::kV1aPlateR;
        const bool ok = ip > 0.0003f && ip < 0.003f
                     && stage.getCathode() > 0.6f && stage.getCathode() < 3.5f;
        std::ostringstream d;
        d << "Ip=" << (ip * 1000.0f) << " mA Vp=" << stage.getPlate()
          << " Vk=" << stage.getCathode() << " (expect ~0.5–2 mA, Vk ~1–2 V)";
        report.add ({ "12AX7 idle plate current", ok, d.str() });
    }

    // 6V6 idle ~40 mA @ −19 Vgk / 340 Vak ballpark
    {
        const auto t = circuit::sixV6();
        const float ip = t.plateCurrent (-19.0f, 340.0f);
        const bool ok = ip > 0.015f && ip < 0.080f;
        std::ostringstream d;
        d << "Ip=" << (ip * 1000.0f) << " mA (expect ~20–60 mA cathode-bias)";
        report.add ({ "6V6 idle plate current", ok, d.str() });
    }

    // Flat 8 Ω |Z| ≈ 8
    {
        const auto z = cab::evaluateRlc (cab::makePreset (cab::ImpedancePreset::flat8), 1000.0f);
        const bool ok = near (std::abs (z), 8.0f, 0.02f, 0.2f);
        std::ostringstream d;
        d << "|Z(1kHz)|=" << std::abs (z) << " ohm";
        report.add ({ "Flat 8 speaker |Z|", ok, d.str() });
    }

    // Mesa preset has |Z| resonance peak above Re
    {
        const auto p = cab::makePreset (cab::ImpedancePreset::mesa4x12V30);
        const auto z100 = cab::evaluateRlc (p, 100.0f);
        const bool ok = std::abs (z100) > p.re * 1.5f && p.syntheticPlaceholder;
        std::ostringstream d;
        d << "|Z(100Hz)|=" << std::abs (z100) << " Re=" << p.re
          << " synthetic=" << (p.syntheticPlaceholder ? "yes" : "no");
        report.add ({ "Mesa Z(f) resonance (synthetic)", ok, d.str() });
    }

    // V1A Newton stable
    {
        ChampTriodeStage stage;
        stage.prepare (48000.0f * 4.0f, Comp::kV1aPlateR, Comp::kV1aCathodeR,
                       Comp::kV1aBypassC, Comp::kBplusPreamp, 0.0f);
        bool finite = true;
        float y = 0.0f;
        for (int i = 0; i < 128; ++i)
        {
            const float x = 0.05f * std::sin (2.0f * 3.14159265f * (float) i / 64.0f);
            y = stage.processSample (x, 0.0f);
            if (! std::isfinite (y))
                finite = false;
        }
        const bool okBias = stage.getCathode() > 0.6f && stage.getCathode() < 3.5f;
        std::ostringstream d;
        d << "last plate=" << y << " V cathode=" << stage.getCathode() << " V";
        report.add ({ "V1A Newton stable", finite && okBias && std::isfinite (y), d.str() });
    }

    // End-to-end base-rate path: coupling seeded, all finite, audible RMS
    {
        const auto probe = runEndToEndPath (48000.0f);
        const bool firstQuiet = std::abs (probe.firstAc1) < 1.0f
                             && std::abs (probe.firstAc2) < 1.0f;
        const bool hasAc = probe.rmsAc1 > 0.05f && probe.rmsAc2 > 0.05f;
        const bool audible = probe.rmsVs > 0.05f && probe.rmsDigital > 0.002f
                          && probe.peakVs < 80.0f;
        const bool ok = probe.finiteFirst && probe.finiteAll && firstQuiet
                     && hasAc && audible;
        report.add ({ "End-to-end base-rate audio (resistive 8 ohm)", ok,
                      probe.toDetail() });
    }

    // Power stage into flat 8 Ω — finite speaker volts
    {
        ChampPowerStage power;
        ComponentSet c;
        power.prepare (48000.0f * 4.0f, c);
        power.setSpeakerRlc (cab::makePreset (cab::ImpedancePreset::flat8));
        bool finite = true;
        float peak = 0.0f;
        for (int i = 0; i < 256; ++i)
        {
            const float x = 0.5f * std::sin (2.0f * 3.14159265f * (float) i / 64.0f);
            const float y = power.processSample (x);
            if (! std::isfinite (y))
                finite = false;
            peak = std::max (peak, std::abs (y));
        }
        const bool ok = finite && peak > 0.01f && peak < 200.0f;
        std::ostringstream d;
        d << "peak |Vs|=" << peak << " V into flat 8 ohm";
        report.add ({ "Power+OT into resistive 8 ohm", ok, d.str() });
    }

    // Reactive cab preset still converges
    {
        ChampPowerStage power;
        ComponentSet c;
        power.prepare (48000.0f * 4.0f, c);
        power.setSpeakerRlc (cab::makePreset (cab::ImpedancePreset::fenderDlx1x12));
        bool finite = true;
        float peak = 0.0f;
        for (int i = 0; i < 256; ++i)
        {
            const float x = 0.3f * std::sin (2.0f * 3.14159265f * (float) i / 64.0f);
            const float y = power.processSample (x);
            if (! std::isfinite (y))
                finite = false;
            peak = std::max (peak, std::abs (y));
        }
        const bool ok = finite && peak > 0.01f && peak < 200.0f;
        std::ostringstream d;
        d << "peak |Vs|=" << peak << " V into Fender 1x12 Z(f)";
        report.add ({ "Power+OT into reactive Z(f)", ok, d.str() });
    }

    // Volume taper monotonic
    {
        const bool ok = volumeFraction (0.25f) < volumeFraction (0.5f)
                     && volumeFraction (0.5f) < volumeFraction (1.0f)
                     && volumeFraction (0.0f) == 0.0f;
        report.add ({ "Volume audio taper monotonic", ok, "0→1 maps x^2" });
    }

    return report;
}

inline VerifyReport runAllChampVerifications()
{
    return runChampVerification();
}
} // namespace champ
