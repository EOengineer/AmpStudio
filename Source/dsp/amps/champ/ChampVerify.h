#pragma once

#include "ChampComponents.h"
#include "ChampDsp.h"
#include "ChampPowerStage.h"
#include "ChampTriodeStage.h"
#include "../../cabs/SpeakerRlc.h"
#include "../../circuit/RcFilters.h"
#include "../../circuit/TubeModel.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
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

inline bool inBand (float x, float lo, float hi) noexcept
{
    return x >= lo && x <= hi;
}

/**
 * Host-path RMS regression (ChampDsp @ 48 kHz, 1 kHz, digital amp 0.05, vol 1).
 * Captured on the cab-Z golden after the ChampDsp extract — later physics
 * must not collapse to silence or jump an octave of gain.
 */
namespace Golden
{
    // ChampDsp @ 48 kHz, 1 kHz, digital amp 0.05, vol 1, flat 8 Ω.
    inline constexpr float kNfbOffRmsVsFlat8 = 3.56f;
    inline constexpr float kNfbOnRmsVsFlat8 = 3.36f;
    inline constexpr float kNfbOffRmsDigitalFlat8 = 0.89f;
    inline constexpr float kRmsLo = 0.5f;
    inline constexpr float kRmsHi = 2.0f;

    // Hot guitar-ish drive (vol 1, NFB Stock). Golden ~ hf 0.12, hp 0.10, holds 0.
    // Tripwire for lastGood stutter / huge HF. The reverted NFB-LPF and soft
    // 6V6-cutoff slices did not move these numbers offline — host listen remains
    // the gate for that class of hash.
    inline constexpr float kHashHfMax = 0.25f;
    inline constexpr float kHashHpMax = 0.22f;
    inline constexpr float kHashHighBandMax = 0.02f;
    inline constexpr float kHashHoldMax = 0.005f;
    inline constexpr float kHashDerivFlipMax = 0.18f;
}

inline bool inGoldenRms (float rms, float golden) noexcept
{
    return inBand (rms, golden * Golden::kRmsLo, golden * Golden::kRmsHi);
}

/** ChampDsp host path (same processSample as the plugin). */
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
    float maxDeltaVs = 0.0f;
    float maxDeltaAc1 = 0.0f;
    float maxDeltaAc2 = 0.0f;
    float maxDeltaDigital = 0.0f;
    float idleRmsDigital = 0.0f;
    float idleSignFlipRate = 0.0f;
    float idleHfRatio = 0.0f;
    float idleMaxDeltaDigital = 0.0f;
    float toneSignFlipRate = 0.0f;
    float toneHfRatio = 0.0f;
    float volume = 1.0f;

    std::string toDetail() const
    {
        std::ostringstream d;
        d << "vol=" << volume
          << " first ac1=" << firstAc1 << " ac2=" << firstAc2
          << " idle Vp1=" << idlePlate1 << " Vp2=" << idlePlate2
          << " Vk1=" << idleVk1 << " Ip1=" << idleIp1mA << " mA"
          << " peak ac1=" << peakAc1 << " ac2=" << peakAc2 << " Vs=" << peakVs
          << " rms ac1=" << rmsAc1 << " ac2=" << rmsAc2
          << " Vs=" << rmsVs << " digital=" << rmsDigital
          << " dVs=" << maxDeltaVs << " dAc2=" << maxDeltaAc2
          << " dDig=" << maxDeltaDigital
          << " idleRms=" << idleRmsDigital
          << " idleFlip=" << idleSignFlipRate
          << " idleHf=" << idleHfRatio
          << " idleD=" << idleMaxDeltaDigital
          << " toneFlip=" << toneSignFlipRate
          << " toneHf=" << toneHfRatio
          << " finite0=" << (finiteFirst ? "yes" : "NO")
          << " finiteAll=" << (finiteAll ? "yes" : "NO");
        return d.str();
    }
};

struct HfAccum
{
    float prev = 0.0f;
    bool havePrev = false;
    int flips = 0;
    int steps = 0;
    double diffE = 0.0;
    double totE = 0.0;
    float maxDelta = 0.0f;
    int n = 0;

    void add (float y) noexcept
    {
        totE += (double) y * (double) y;
        ++n;
        if (havePrev)
        {
            const float d = y - prev;
            maxDelta = std::max (maxDelta, std::abs (d));
            diffE += (double) d * (double) d;
            if ((prev > 0.0f && y < 0.0f) || (prev < 0.0f && y > 0.0f))
                ++flips;
            ++steps;
        }
        prev = y;
        havePrev = true;
    }

    float rms() const noexcept
    {
        return n > 0 ? (float) std::sqrt (totE / (double) n) : 0.0f;
    }

    float signFlipRate() const noexcept
    {
        return steps > 0 ? (float) flips / (float) steps : 0.0f;
    }

    float hfRatio() const noexcept
    {
        return totE > 1.0e-20 ? (float) (diffE / totE) : 0.0f;
    }
};

inline float goertzelPower (const float* x, int n, float fs, float freqHz) noexcept
{
    const float w = 2.0f * 3.14159265358979323846f * freqHz / fs;
    const float coeff = 2.0f * std::cos (w);
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const float real = s1 - s2 * std::cos (w);
    const float imag = s2 * std::sin (w);
    return real * real + imag * imag;
}

/**
 * Driven hash / static (the two reverted slices were quiet at idle and hashed
 * on guitar). Plugin defaults: vol 0.5, NFB Stock, 220 Hz, 0.25 digital.
 */
struct HashProbe
{
    float rmsDigital = 0.0f;
    float hfRatio = 0.0f;
    float signFlipRate = 0.0f;
    float derivFlipRate = 0.0f;
    float highBandRatio = 0.0f; // (8+12+16+20 kHz) / (fund + those)
    float hpRatio = 0.0f;       // energy above ~1.5 kHz / total (catches 2–4 kHz chatter)
    float holdRate = 0.0f;      // ChampPowerStage lastGood / Newton rejects
    int holdCount = 0;
    bool finiteAll = true;

    std::string toDetail() const
    {
        std::ostringstream d;
        d << "rms=" << rmsDigital
          << " hf=" << hfRatio
          << " flip=" << signFlipRate
          << " dFlip=" << derivFlipRate
          << " highBand=" << highBandRatio
          << " hp=" << hpRatio
          << " holds=" << holdCount
          << " holdRate=" << holdRate
          << " finite=" << (finiteAll ? "yes" : "NO");
        return d.str();
    }
};

/** Guitar-ish drive shared by champ_verify and champ_wav_dump. */
inline float drivenHashInput (int i, float fs,
                              float digitalAmp = 0.4f,
                              float toneHz = 220.0f) noexcept
{
    constexpr float kPi = 3.14159265358979323846f;
    const float ph = 2.0f * kPi * toneHz * (float) i / fs;
    uint32_t rng = 0xA3C5u + (uint32_t) i * 747796405u;
    rng = rng * 1664525u + 1013904223u;
    const float noise = (float) (int32_t) rng * (1.0f / 2147483648.0f);
    return digitalAmp * (0.70f * std::sin (ph)
                       + 0.20f * std::sin (3.0f * ph)
                       + 0.10f * std::sin (5.0f * ph)
                       + 0.15f * std::sin (2.0f * kPi * 3000.0f * (float) i / fs)
                       + 0.20f * noise);
}

inline HashProbe runDrivenHash (
    float fs = 48000.0f,
    bool nfbOn = true,
    float volume01 = 1.0f,
    float digitalAmp = 0.4f,
    float toneHz = 220.0f,
    int toneSamples = 4096) noexcept
{
    HashProbe p;
    ChampDsp dsp;
    dsp.prepare (fs);
    dsp.setVolume (volume01);
    dsp.setNfbEnabled (nfbOn);
    dsp.setSpeakerRlc (cab::makePreset (cab::ImpedancePreset::flat8));
    dsp.reset();

    constexpr float kPi = 3.14159265358979323846f;
    for (int i = 0; i < 256; ++i)
        dsp.processSample (0.0f);

    std::vector<float> y ((size_t) toneSamples, 0.0f);
    HfAccum tone;
    float prevD = 0.0f;
    bool haveD = false;
    int derivFlips = 0;
    int derivSteps = 0;
    circuit::OnePoleLpRc hpRef;
    hpRef.prepare (10.0e3f, 1.0f / (2.0f * kPi * 1500.0f * 10.0e3f), fs);
    double hpE = 0.0, totE = 0.0;

    for (int i = 0; i < toneSamples; ++i)
    {
        const float vin = drivenHashInput (i, fs, digitalAmp, toneHz);
        const float out = dsp.processSample (vin);
        if (! std::isfinite (out))
            p.finiteAll = false;
        y[(size_t) i] = out;
        tone.add (out);
        const float lp = hpRef.process (out);
        const float hp = out - lp;
        hpE += (double) hp * (double) hp;
        totE += (double) out * (double) out;

        if (i > 0)
        {
            const float d = out - y[(size_t) i - 1];
            if (haveD)
            {
                if ((prevD > 0.0f && d < 0.0f) || (prevD < 0.0f && d > 0.0f))
                    ++derivFlips;
                ++derivSteps;
            }
            prevD = d;
            haveD = true;
        }
    }

    p.rmsDigital = tone.rms();
    p.hfRatio = tone.hfRatio();
    p.signFlipRate = tone.signFlipRate();
    p.derivFlipRate = derivSteps > 0 ? (float) derivFlips / (float) derivSteps : 0.0f;

    const float fund = goertzelPower (y.data(), toneSamples, fs, toneHz);
    const float hi = goertzelPower (y.data(), toneSamples, fs, 8000.0f)
                   + goertzelPower (y.data(), toneSamples, fs, 12000.0f)
                   + goertzelPower (y.data(), toneSamples, fs, 16000.0f)
                   + goertzelPower (y.data(), toneSamples, fs, 20000.0f);
    p.highBandRatio = hi / std::max (fund + hi, 1.0e-20f);
    p.hpRatio = totE > 1.0e-20 ? (float) (hpE / totE) : 0.0f;
    p.holdCount = dsp.getPower().getHoldCount();
    p.holdRate = dsp.getPower().getHoldRate();
    return p;
}

/** nfbOn = Stock 22k with inverted speaker volts (negative feedback). */
inline EndToEndProbe runHostPath (
    float fs = 48000.0f,
    bool nfbOn = false,
    cab::SpeakerRlc load = cab::makePreset (cab::ImpedancePreset::flat8),
    float digitalAmp = 0.05f,
    int toneSamples = 2400,
    float volume01 = 1.0f,
    int idleSamples = 2048) noexcept
{
    EndToEndProbe p;
    p.volume = volume01;

    ChampDsp dsp;
    dsp.prepare (fs);
    dsp.setVolume (volume01);
    dsp.setNfbEnabled (nfbOn);
    dsp.setSpeakerRlc (load);
    // Reseed coupling at the configured idle (plugin usually prepare()s with
    // Stock NFB already matching the default param). Avoids counting an NFB
    // Off disconnect step as “idle grind.”
    dsp.reset();

    p.idlePlate1 = dsp.getV1a().getPlate();
    p.idlePlate2 = dsp.getV1b().getPlate();
    p.idleVk1 = dsp.getV1a().getCathode();
    p.idleIp1mA = (Comp::kBplusPreamp - p.idlePlate1) / Comp::kV1aPlateR * 1000.0f;

    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kHz = 1000.0f;

    HfAccum idleDig, toneDig;
    float prevVs = 0.0f, prevAc1 = 0.0f, prevAc2 = 0.0f;
    bool havePrevStages = false;
    double sumAc1 = 0.0, sumAc2 = 0.0, sumVs = 0.0;
    int rmsN = 0;

    auto tick = [&] (float vin, bool accumulateTone, bool isFirst)
    {
        const float digital = dsp.processSample (vin);
        const float ac1 = dsp.getLastAc1();
        const float ac2 = dsp.getLastAc2();
        const float vs = dsp.getLastSpeakerV();

        const bool ok = std::isfinite (dsp.getV1a().getPlate())
                     && std::isfinite (ac1) && std::isfinite (ac2)
                     && std::isfinite (vs) && std::isfinite (digital);
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
        p.peakVs = std::max (p.peakVs, std::abs (vs));

        if (havePrevStages)
        {
            p.maxDeltaVs = std::max (p.maxDeltaVs, std::abs (vs - prevVs));
            p.maxDeltaAc1 = std::max (p.maxDeltaAc1, std::abs (ac1 - prevAc1));
            p.maxDeltaAc2 = std::max (p.maxDeltaAc2, std::abs (ac2 - prevAc2));
        }
        prevVs = vs;
        prevAc1 = ac1;
        prevAc2 = ac2;
        havePrevStages = true;

        if (accumulateTone)
        {
            toneDig.add (digital);
            p.maxDeltaDigital = std::max (p.maxDeltaDigital, toneDig.maxDelta);
            sumAc1 += (double) ac1 * (double) ac1;
            sumAc2 += (double) ac2 * (double) ac2;
            sumVs += (double) vs * (double) vs;
            ++rmsN;
        }
        else
        {
            idleDig.add (digital);
        }
    };

    const int nIdle = std::max (idleSamples, 1);
    tick (0.0f, false, true);
    for (int i = 1; i < nIdle; ++i)
        tick (0.0f, false, false);

    for (int i = 0; i < toneSamples; ++i)
    {
        const float vin = digitalAmp * std::sin (2.0f * kPi * kHz * (float) i / fs);
        tick (vin, true, false);
    }

    p.idleRmsDigital = idleDig.rms();
    p.idleSignFlipRate = idleDig.signFlipRate();
    p.idleHfRatio = idleDig.hfRatio();
    p.idleMaxDeltaDigital = idleDig.maxDelta;
    p.toneSignFlipRate = toneDig.signFlipRate();
    p.toneHfRatio = toneDig.hfRatio();
    p.maxDeltaDigital = std::max (p.maxDeltaDigital, toneDig.maxDelta);
    p.rmsDigital = toneDig.rms();

    if (rmsN > 0)
    {
        p.rmsAc1 = (float) std::sqrt (sumAc1 / (double) rmsN);
        p.rmsAc2 = (float) std::sqrt (sumAc2 / (double) rmsN);
        p.rmsVs = (float) std::sqrt (sumVs / (double) rmsN);
    }

    return p;
}

/** @deprecated name kept for call-site clarity — host path, not rewired stages. */
inline EndToEndProbe runEndToEndPath (
    float fs = 48000.0f,
    bool nfbOn = false,
    cab::SpeakerRlc load = cab::makePreset (cab::ImpedancePreset::flat8),
    float gridAmp = 0.05f,
    int toneSamples = 2400) noexcept
{
    return runHostPath (fs, nfbOn, load, gridAmp, toneSamples, 1.0f);
}

inline bool idleQuiet (const EndToEndProbe& p) noexcept
{
    // This model sits on a small idle speaker offset (~0.07 digital, flat 8).
    // Nyquist lock (the dumped “silence” grind) is a near-fs/2 square wave:
    // high RMS, sign-flip ≈ 0.5, hfRatio ≈ 4, sample steps of volts.
    const bool nyquistLock = p.idleRmsDigital > 0.02f
                          && p.idleSignFlipRate > 0.25f
                          && p.idleHfRatio > 1.0f;
    const bool idleRunaway = p.idleRmsDigital > 0.35f
                          || p.idleMaxDeltaDigital > 1.5f;
    return ! nyquistLock && ! idleRunaway;
}

inline bool notHeldSilent (const EndToEndProbe& p, float digitalAmp) noexcept
{
    if (digitalAmp < 0.01f)
        return true;
    return p.rmsDigital > 0.0005f && p.maxDeltaDigital > 1.0e-6f;
}

inline bool noSnaps (const EndToEndProbe& p) noexcept
{
    return p.maxDeltaVs < 20.0f && p.maxDeltaAc2 < 15.0f && p.maxDeltaAc1 < 15.0f;
}

inline bool noToneNyquist (const EndToEndProbe& p) noexcept
{
    // 1 kHz @ 48 kHz flips ~0.04 of samples; Nyquist lock is ~0.5 with hfRatio ~4.
    return p.toneSignFlipRate < 0.20f && p.toneHfRatio < 1.0f;
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

    constexpr float kAmp = 0.05f;
    const auto flat8 = cab::makePreset (cab::ImpedancePreset::flat8);

    EndToEndProbe nfbOffProbe;
    {
        nfbOffProbe = runHostPath (48000.0f, false, flat8, kAmp, 2400, 1.0f);
        const bool firstQuiet = std::abs (nfbOffProbe.firstAc1) < 1.0f
                             && std::abs (nfbOffProbe.firstAc2) < 1.0f;
        const bool hasAc = nfbOffProbe.rmsAc1 > 0.05f && nfbOffProbe.rmsAc2 > 0.05f;
        const bool audible = nfbOffProbe.rmsVs > 0.05f && nfbOffProbe.rmsDigital > 0.002f
                          && nfbOffProbe.peakVs < 80.0f;
        const bool ok = nfbOffProbe.finiteFirst && nfbOffProbe.finiteAll && firstQuiet
                     && hasAc && audible;
        report.add ({ "Host-path audio (resistive 8 ohm, NFB Off, vol 1)", ok,
                      nfbOffProbe.toDetail() });
    }

    {
        const bool ok = idleQuiet (nfbOffProbe) && nfbOffProbe.finiteAll;
        std::ostringstream d;
        d << "idleRms=" << nfbOffProbe.idleRmsDigital
          << " flip=" << nfbOffProbe.idleSignFlipRate
          << " hf=" << nfbOffProbe.idleHfRatio;
        report.add ({ "Host-path idle quiet (no Nyquist grind, NFB Off)", ok, d.str() });
    }

    {
        const bool ok = notHeldSilent (nfbOffProbe, kAmp) && noToneNyquist (nfbOffProbe)
                     && noSnaps (nfbOffProbe);
        std::ostringstream d;
        d << "held=" << (notHeldSilent (nfbOffProbe, kAmp) ? "no" : "YES")
          << " snaps=" << (noSnaps (nfbOffProbe) ? "no" : "YES")
          << " " << nfbOffProbe.toDetail();
        report.add ({ "Host-path not held / no snaps (NFB Off)", ok, d.str() });
    }

    {
        const bool ok = inGoldenRms (nfbOffProbe.rmsVs, Golden::kNfbOffRmsVsFlat8)
                     && inGoldenRms (nfbOffProbe.rmsDigital, Golden::kNfbOffRmsDigitalFlat8);
        std::ostringstream d;
        d << "rmsVs=" << nfbOffProbe.rmsVs << " (golden " << Golden::kNfbOffRmsVsFlat8
          << ") rmsDig=" << nfbOffProbe.rmsDigital << " (golden "
          << Golden::kNfbOffRmsDigitalFlat8 << ") band=[" << Golden::kRmsLo
          << "×, " << Golden::kRmsHi << "×]";
        report.add ({ "Host-path RMS in golden band (NFB Off, flat 8)", ok, d.str() });
    }

    EndToEndProbe nfbOnProbe;
    {
        nfbOnProbe = runHostPath (48000.0f, true, flat8, kAmp, 2400, 1.0f);
        // High drive slams the 6V6 grid window; NFB ratio is measured small-signal.
        constexpr float kSmall = 0.01f;
        const auto nfbOffSmall = runHostPath (48000.0f, false, flat8, kSmall, 2400, 1.0f);
        const auto nfbOnSmall = runHostPath (48000.0f, true, flat8, kSmall, 2400, 1.0f);
        const bool quieter = nfbOnSmall.rmsVs < nfbOffSmall.rmsVs * 0.9f;
        const bool stillAudible = nfbOnProbe.rmsVs > 0.01f && nfbOnProbe.rmsDigital > 0.0005f;
        const bool ok = nfbOnProbe.finiteFirst && nfbOnProbe.finiteAll
                     && quieter && stillAudible && nfbOnProbe.peakVs < 80.0f
                     && idleQuiet (nfbOnProbe) && noToneNyquist (nfbOnProbe)
                     && noSnaps (nfbOnProbe);
        std::ostringstream d;
        d << "Stock rmsVs=" << nfbOnProbe.rmsVs
          << " Off rmsVs=" << nfbOffProbe.rmsVs
          << " small Stock=" << nfbOnSmall.rmsVs
          << " small Off=" << nfbOffSmall.rmsVs
          << " " << nfbOnProbe.toDetail();
        report.add ({ "NFB Stock quieter than Off (host path)", ok, d.str() });
    }

    {
        const bool ok = inGoldenRms (nfbOnProbe.rmsVs, Golden::kNfbOnRmsVsFlat8);
        std::ostringstream d;
        d << "Stock rmsVs=" << nfbOnProbe.rmsVs << " (golden " << Golden::kNfbOnRmsVsFlat8 << ")";
        report.add ({ "Host-path RMS in golden band (NFB Stock, flat 8)", ok, d.str() });
    }

    {
        const auto volHalf = runHostPath (48000.0f, false, flat8, kAmp, 2400, 0.5f);
        const bool audible = volHalf.finiteAll && notHeldSilent (volHalf, kAmp)
                          && volHalf.rmsDigital > 0.0005f && volHalf.rmsVs > 0.01f;
        const bool quieterThanFull = volHalf.rmsDigital < nfbOffProbe.rmsDigital;
        const bool ok = audible && quieterThanFull && idleQuiet (volHalf) && noSnaps (volHalf);
        std::ostringstream d;
        d << "vol0.5 rmsDig=" << volHalf.rmsDigital
          << " vol1 rmsDig=" << nfbOffProbe.rmsDigital
          << " " << volHalf.toDetail();
        report.add ({ "Host-path volume 0.5 still audible", ok, d.str() });
    }

    const auto fenderZ = cab::makePreset (cab::ImpedancePreset::fenderDlx1x12);
    {
        const auto nfbOffFender = runHostPath (48000.0f, false, fenderZ, kAmp, 2400, 1.0f);
        const auto nfbOnFender = runHostPath (48000.0f, true, fenderZ, kAmp, 2400, 1.0f);
        constexpr float kSmall = 0.01f;
        const auto nfbOffFenderSmall = runHostPath (48000.0f, false, fenderZ, kSmall, 2400, 1.0f);
        const auto nfbOnFenderSmall = runHostPath (48000.0f, true, fenderZ, kSmall, 2400, 1.0f);
        const bool audible = nfbOnFender.finiteFirst && nfbOnFender.finiteAll
                          && nfbOnFender.rmsVs > 0.01f && nfbOnFender.rmsDigital > 0.0005f
                          && nfbOnFender.peakVs < 80.0f;
        const bool quieter = nfbOnFenderSmall.rmsVs < nfbOffFenderSmall.rmsVs * 0.9f;
        const bool stable = idleQuiet (nfbOnFender) && idleQuiet (nfbOffFender)
                         && noSnaps (nfbOnFender) && noToneNyquist (nfbOnFender);
        const bool ok = audible && nfbOffFender.finiteAll && quieter && stable;
        std::ostringstream d;
        d << "Stock rmsVs=" << nfbOnFender.rmsVs
          << " Off rmsVs=" << nfbOffFender.rmsVs
          << " small Stock=" << nfbOnFenderSmall.rmsVs
          << " small Off=" << nfbOffFenderSmall.rmsVs
          << " " << nfbOnFender.toDetail();
        report.add ({ "Host-path NFB Stock into Fender 1x12 Z(f)", ok, d.str() });
    }

    {
        const auto mesaZ = cab::makePreset (cab::ImpedancePreset::mesa4x12V30);
        constexpr int kLong = 9600; // 200 ms @ 48 kHz
        const auto nfbOffMesa = runHostPath (48000.0f, false, mesaZ, kAmp, kLong, 1.0f);
        const auto nfbOnMesa = runHostPath (48000.0f, true, mesaZ, kAmp, kLong, 1.0f);
        const bool audible = nfbOnMesa.finiteFirst && nfbOnMesa.finiteAll
                          && nfbOnMesa.rmsVs > 0.01f && nfbOnMesa.rmsDigital > 0.0005f
                          && nfbOnMesa.peakVs < 80.0f;
        const bool quieter = nfbOnMesa.rmsVs < nfbOffMesa.rmsVs;
        const bool noChatter = noSnaps (nfbOnMesa) && noSnaps (nfbOffMesa);
        const bool stable = idleQuiet (nfbOnMesa) && idleQuiet (nfbOffMesa)
                         && noToneNyquist (nfbOnMesa);
        const bool ok = audible && nfbOffMesa.finiteAll && quieter && noChatter && stable;
        std::ostringstream d;
        d << "Stock rmsVs=" << nfbOnMesa.rmsVs
          << " Off rmsVs=" << nfbOffMesa.rmsVs
          << " " << nfbOnMesa.toDetail();
        report.add ({ "Host-path NFB Stock into Mesa 4x12 Z(f)", ok, d.str() });
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

    // Driven hash. Quiet idle is not enough — the reverted LPF / soft-cutoff
    // slices hashed on guitar. Hot + noisy drive is what actually hits cutoff.
    {
        const auto h = runDrivenHash();
        const bool audible = h.finiteAll && h.rmsDigital > 0.01f;
        const bool notHashed = h.holdRate <= Golden::kHashHoldMax
                            && h.hfRatio <= Golden::kHashHfMax
                            && h.hpRatio <= Golden::kHashHpMax
                            && h.highBandRatio <= Golden::kHashHighBandMax
                            && h.derivFlipRate <= Golden::kHashDerivFlipMax;
        report.add ({ "Host-path driven hash (vol 1, NFB Stock, hot 220 Hz)",
                      audible && notHashed, h.toDetail() });
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
