#pragma once

#include <cmath>
#include <algorithm>

namespace ts
{
/** Named Tube Screamer components (Geofex / stock 808 defaults). */
namespace Comp
{
    // Input buffer
    inline constexpr float kInputBiasR = 510e3f;
    inline constexpr float kEmitterR   = 10e3f;

    // Clipping amp Zi
    inline constexpr float kZiR        = 4.7e3f;
    inline constexpr float kZiC        = 0.047e-6f;
    inline constexpr float kZiCBassMod = 0.1e-6f;   // "more bass" mod

    // Clipping amp Zf
    inline constexpr float kDriveFixedR = 51e3f;
    inline constexpr float kDrivePotR   = 500e3f;
    inline constexpr float kFeedbackC   = 51e-12f;

    // Post-clip LPF
    inline constexpr float kPostLpR = 1.0e3f;
    inline constexpr float kPostLpC = 0.22e-6f;

    // Tone
    inline constexpr float kTonePotR    = 20e3f;
    inline constexpr float kToneSeriesR = 220.0f;
    inline constexpr float kToneC       = 0.22e-6f;

    // Output buffer (808 vs 9)
    inline constexpr float kOutSeries808 = 100.0f;
    inline constexpr float kOutShunt808  = 10e3f;
    inline constexpr float kOutSeries9   = 470.0f;
    inline constexpr float kOutShunt9    = 100e3f;

    // Geofex analytic anchors
    inline constexpr float kZiCornerHz     = 720.5f;   // 1/(2π·4.7k·0.047µ)
    inline constexpr float kPostLpCornerHz = 723.4f;   // 1/(2π·1k·0.22µ)
    inline constexpr float kToneCornerHz   = 3283.0f;  // 1/(2π·220·0.22µ) ≈ 3.2 kHz
}

enum class OutputVariant
{
    ts808 = 0,
    ts9   = 1
};

enum class BassCap
{
    stock   = 0, // 0.047 µF
    moreBass = 1 // 0.1 µF
};

enum class DiodeMode
{
    siliconPair   = 0, // Si / Si
    asymmetricSi  = 1, // subtle asymmetry (scaled Is one side)
    germaniumSi   = 2, // Ge one polarity emphasis via mixed model
    ledPair       = 3
};

struct ComponentSet
{
    float ziR = Comp::kZiR;
    float ziC = Comp::kZiC;
    float driveFixedR = Comp::kDriveFixedR;
    float drivePotR = Comp::kDrivePotR;
    float feedbackC = Comp::kFeedbackC;
    float postLpR = Comp::kPostLpR;
    float postLpC = Comp::kPostLpC;
    float tonePotR = Comp::kTonePotR;
    float toneSeriesR = Comp::kToneSeriesR;
    float toneC = Comp::kToneC;
    float outSeriesR = Comp::kOutSeries808;
    float outShuntR = Comp::kOutShunt808;
    OutputVariant outputVariant = OutputVariant::ts808;
    BassCap bassCap = BassCap::stock;
    DiodeMode diodeMode = DiodeMode::siliconPair;

    void applyOutputVariant (OutputVariant v) noexcept
    {
        outputVariant = v;
        if (v == OutputVariant::ts808)
        {
            outSeriesR = Comp::kOutSeries808;
            outShuntR = Comp::kOutShunt808;
        }
        else
        {
            outSeriesR = Comp::kOutSeries9;
            outShuntR = Comp::kOutShunt9;
        }
    }

    void applyBassCap (BassCap b) noexcept
    {
        bassCap = b;
        ziC = (b == BassCap::moreBass) ? Comp::kZiCBassMod : Comp::kZiC;
    }

    static float driveFeedbackR (float drive01) noexcept
    {
        const float d = std::clamp (drive01, 0.0f, 1.0f);
        return Comp::kDriveFixedR + d * Comp::kDrivePotR;
    }

    /** Ideal HF gain 1 + Rfb/Rzi (diodes open, Czi short). */
    static float idealHfGain (float drive01) noexcept
    {
        return 1.0f + driveFeedbackR (drive01) / Comp::kZiR;
    }
};

/** 0 dBFS ↔ 1 V peak; −18 dBFS ≈ guitar hard-play into the circuit. */
inline constexpr float kVoltsPerFullScale = 1.0f;

inline float digitalToVolts (float x) noexcept { return x * kVoltsPerFullScale; }
inline float voltsToDigital (float v) noexcept { return v / kVoltsPerFullScale; }

/** Audio-taper-ish map for Level pot (0..1 → amplitude). */
inline float audioTaperGain (float level01) noexcept
{
    const float x = std::clamp (level01, 0.0f, 1.0f);
    // Approximate log pot: quieter in the lower half
    return x * x;
}
} // namespace ts
