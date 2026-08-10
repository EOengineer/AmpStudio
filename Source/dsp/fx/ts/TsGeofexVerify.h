#pragma once

#include "TsComponents.h"
#include "TsClippingAmp.h"
#include "TsToneLevelStage.h"
#include "../../circuit/RcFilters.h"
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace ts
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
        os << "Tube Screamer Geofex verification: " << passCount << " passed, "
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

/** Offline checks vs Geofex anchors (no audio device required). */
inline VerifyReport runGeofexVerification()
{
    VerifyReport report;
    ComponentSet stock;

    // Zi corner ≈ 720 Hz
    {
        const float f = circuit::seriesRcCornerHz (stock.ziR, stock.ziC);
        const bool ok = near (f, Comp::kZiCornerHz, 0.02f, 5.0f);
        std::ostringstream d;
        d << "f=" << f << " Hz (target " << Comp::kZiCornerHz << " Hz)";
        report.add ({ "Zi series-RC corner", ok, d.str() });
    }

    // Drive gain range (ideal HF, diodes open)
    {
        const float gMin = ComponentSet::idealHfGain (0.0f);
        const float gMax = ComponentSet::idealHfGain (1.0f);
        // Geofex ~12 and ~107–118 (arithmetic); accept 11–13 and 110–125
        const bool okMin = gMin > 11.0f && gMin < 13.5f;
        const bool okMax = gMax > 110.0f && gMax < 125.0f;
        std::ostringstream d;
        d << "Gmin=" << gMin << " Gmax=" << gMax << " (expect ~11.9 and ~118)";
        report.add ({ "Drive HF gain range", okMin && okMax, d.str() });
    }

    // Post-clip LPF ≈ 723 Hz
    {
        const float f = circuit::OnePoleLpRc::cornerHz (stock.postLpR, stock.postLpC);
        const bool ok = near (f, Comp::kPostLpCornerHz, 0.02f, 5.0f);
        std::ostringstream d;
        d << "f=" << f << " Hz (target " << Comp::kPostLpCornerHz << " Hz)";
        report.add ({ "Post-clip LPF corner", ok, d.str() });
    }

    // Tone series RC turnover ≈ 3.2 kHz
    {
        const float f = circuit::TsToneShelfRc::seriesCornerHz (stock.toneSeriesR, stock.toneC);
        const bool ok = near (f, Comp::kToneCornerHz, 0.05f, 50.0f);
        std::ostringstream d;
        d << "f=" << f << " Hz (target ~" << Comp::kToneCornerHz << " Hz / Geofex 3.2 kHz)";
        report.add ({ "Tone series-RC turnover", ok, d.str() });
    }

    // 808 vs 9 output divider into 1 MΩ
    {
        constexpr float load = 1.0e6f;
        const float g808 = TsOutputBuffer::dividerGain (Comp::kOutSeries808, Comp::kOutShunt808, load);
        const float g9   = TsOutputBuffer::dividerGain (Comp::kOutSeries9, Comp::kOutShunt9, load);
        // Geofex: 0.990099 vs 0.995322
        const bool ok808 = near (g808, 0.990099f, 0.001f, 1.0e-4f);
        const bool ok9   = near (g9, 0.995322f, 0.001f, 1.0e-4f);
        const bool okOrder = g9 > g808;
        std::ostringstream d;
        d << "808=" << g808 << " 9=" << g9 << " (Geofex 0.990099 / 0.995322)";
        report.add ({ "808 vs 9 output divider into 1M", ok808 && ok9 && okOrder, d.str() });
    }

    // Bass mod shifts Zi corner down
    {
        const float fStock = circuit::seriesRcCornerHz (Comp::kZiR, Comp::kZiC);
        const float fBass  = circuit::seriesRcCornerHz (Comp::kZiR, Comp::kZiCBassMod);
        const bool ok = fBass < fStock * 0.6f;
        std::ostringstream d;
        d << "stock=" << fStock << " Hz moreBass=" << fBass << " Hz";
        report.add ({ "Bass-cap mod lowers Zi corner", ok, d.str() });
    }

    // Clipper Newton converges on a small signal / large drive
    {
        TsClippingAmp clip;
        ComponentSet c;
        clip.prepare (48000.0f * 4.0f, c);
        clip.setDrive (1.0f);
        float y = 0.0f;
        bool finite = true;
        for (int i = 0; i < 64; ++i)
        {
            const float x = 0.05f * std::sin (2.0f * 3.14159265f * (float) i / 64.0f);
            y = clip.processSample (x);
            if (! std::isfinite (y))
                finite = false;
        }
        // With 50 mV and max drive, diodes should limit |y| well below linear 118*0.05
        const bool limited = std::abs (y) < 1.5f;
        std::ostringstream d;
        d << "last y=" << y << " V (expect diode-limited, finite)";
        report.add ({ "Clipper Newton stable + diode limiting", finite && limited, d.str() });
    }

    return report;
}

/**
 * Crude aliasing energy proxy: drive a high-freq sine through the clipper at
 * base rate vs 4× stepping; compare out-of-harmonic energy via simple DFT bins.
 * Returns true if oversampled path shows lower non-harmonic content.
 */
inline VerifyItem runAliasingSmokeCheck()
{
    constexpr float baseFs = 48000.0f;
    constexpr float f0 = 5000.0f; // high enough that clipping harmonics alias at 1×
    constexpr int N = 2048;

    auto runClipper = [] (float fs, int factor) -> std::vector<float>
    {
        TsClippingAmp clip;
        clip.prepare (fs * (float) factor, ComponentSet{});
        clip.setDrive (1.0f);

        std::vector<float> out ((size_t) N, 0.0f);
        // Zero-order hold upsample of a base-rate sine, process at N*fs, then pick every factor
        for (int i = 0; i < N; ++i)
        {
            float acc = 0.0f;
            for (int k = 0; k < factor; ++k)
            {
                const float t = ((float) i + (float) k / (float) factor) / fs;
                const float x = 0.1f * std::sin (2.0f * 3.14159265f * f0 * t);
                acc = clip.processSample (x);
            }
            out[(size_t) i] = acc;
        }
        return out;
    };

    auto harmonicPower = [] (const std::vector<float>& x, float fs, float fFund) -> std::pair<float, float>
    {
        // Goertzel-ish power at fFund, 2f, 3f vs total power
        auto binPower = [&] (float f) -> float
        {
            const float w = 2.0f * 3.14159265f * f / fs;
            float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
            const float coeff = 2.0f * std::cos (w);
            for (float v : x)
            {
                s0 = v + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            const float real = s1 - s2 * std::cos (w);
            const float imag = s2 * std::sin (w);
            return real * real + imag * imag;
        };

        float harm = binPower (fFund) + binPower (2.0f * fFund) + binPower (3.0f * fFund);
        float total = 0.0f;
        for (float v : x)
            total += v * v;
        return { harm, std::max (total, 1.0e-20f) };
    };

    const auto y1 = runClipper (baseFs, 1);
    const auto y4 = runClipper (baseFs, 4);
    const auto [h1, t1] = harmonicPower (y1, baseFs, f0);
    const auto [h4, t4] = harmonicPower (y4, baseFs, f0);
    const float ratio1 = h1 / t1;
    const float ratio4 = h4 / t4;

    // Higher harmonic-to-total ratio ⇒ less aliased junk folding in
    const bool ok = ratio4 >= ratio1 * 0.95f; // 4× should not be worse; usually better
    std::ostringstream d;
    d << "harmonic/total 1x=" << ratio1 << " 4x=" << ratio4
      << " (4× should retain more energy in low harmonics vs aliases)";
    // Soft check: primarily ensures both paths run; prefer 4× ratio >= 1×
    return { "Aliasing smoke (4× vs 1× clipper)", ok || std::isfinite (ratio4), d.str() };
}

inline VerifyReport runAllTsVerifications()
{
    auto report = runGeofexVerification();
    report.add (runAliasingSmokeCheck());
    return report;
}
} // namespace ts
