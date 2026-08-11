#pragma once

#include "ChampComponents.h"
#include "ChampPowerStage.h"
#include "ChampTriodeStage.h"
#include "../../cabs/SpeakerRlc.h"
#include "../../circuit/RcFilters.h"
#include "../../circuit/TubeModel.h"
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

    // 12AX7 idle plate current in Champ bias ballpark
    {
        const auto t = circuit::twelveAx7();
        // ~1.5 V cathode bias, plate ~170 V → Vgk=-1.5, Vak≈168.5
        const float ip = t.plateCurrent (-1.5f, 168.5f);
        const bool ok = ip > 0.0003f && ip < 0.003f; // ~0.3–3 mA
        std::ostringstream d;
        d << "Ip=" << (ip * 1000.0f) << " mA (expect ~0.5–2 mA @ −1.5 Vgk)";
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
        const bool okBias = stage.getCathode() > 1.0f && stage.getCathode() < 3.0f;
        std::ostringstream d;
        d << "last plate=" << y << " V cathode=" << stage.getCathode() << " V";
        report.add ({ "V1A Newton stable", finite && okBias && std::isfinite (y), d.str() });
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
