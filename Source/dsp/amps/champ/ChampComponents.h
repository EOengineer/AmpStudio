#pragma once

#include <cmath>
#include <algorithm>

namespace champ
{
/**
 * Named Fender Champ 5F1 parts (factory K-EE / K-8E, Weber, Robinette).
 * V1A cathode bypass included — common factory fit; missing on some K-EE prints.
 */
namespace Comp
{
    // Input network (Hi jack)
    inline constexpr float kGridLeakR   = 1.0e6f;
    inline constexpr float kGridStopperR = 68.0e3f;

    // V1A
    inline constexpr float kV1aPlateR   = 100.0e3f;
    inline constexpr float kV1aCathodeR = 1.5e3f;
    inline constexpr float kV1aBypassC  = 25.0e-6f;
    inline constexpr float kCouplingC1  = 0.02e-6f; // to volume

    // Volume
    inline constexpr float kVolumePotR  = 1.0e6f;

    // V1B driver (unbypassed cathode + NFB)
    inline constexpr float kV1bPlateR   = 100.0e3f;
    inline constexpr float kV1bCathodeR = 1.5e3f;
    inline constexpr float kNfbR        = 22.0e3f;
    inline constexpr float kCouplingC2  = 0.02e-6f; // to 6V6 grid

    // 6V6GT power
    inline constexpr float kPowerGridLeakR = 220.0e3f;
    inline constexpr float kPowerCathodeR  = 470.0f;
    inline constexpr float kPowerBypassC   = 25.0e-6f;

    // Supplies (typical operating points)
    inline constexpr float kBplusPreamp = 250.0f;
    inline constexpr float kBplusPlate  = 360.0f;
    inline constexpr float kBplusScreen = 325.0f;

    // OT: ~5k primary into 8 Ω secondary → n = sqrt(5000/8)
    inline constexpr float kOtPrimaryZ  = 5000.0f;
    inline constexpr float kOtSecondaryZ = 8.0f;

    // Analytic anchors
    inline constexpr float kNfbLoopRatio = (kNfbR + kV1bCathodeR) / kV1bCathodeR; // ≈15.67
    inline constexpr float kCoupling1CornerIntoVol =
        1.0f / (2.0f * 3.14159265358979323846f * kVolumePotR * kCouplingC1); // ~8 Hz
    inline constexpr float kCoupling2CornerIntoGrid =
        1.0f / (2.0f * 3.14159265358979323846f * kPowerGridLeakR * kCouplingC2); // ~36 Hz
}

struct ComponentSet
{
    float gridLeakR = Comp::kGridLeakR;
    float gridStopperR = Comp::kGridStopperR;
    float v1aPlateR = Comp::kV1aPlateR;
    float v1aCathodeR = Comp::kV1aCathodeR;
    float v1aBypassC = Comp::kV1aBypassC;
    float couplingC1 = Comp::kCouplingC1;
    float volumePotR = Comp::kVolumePotR;
    float v1bPlateR = Comp::kV1bPlateR;
    float v1bCathodeR = Comp::kV1bCathodeR;
    float nfbR = Comp::kNfbR;
    float couplingC2 = Comp::kCouplingC2;
    float powerGridLeakR = Comp::kPowerGridLeakR;
    float powerCathodeR = Comp::kPowerCathodeR;
    float powerBypassC = Comp::kPowerBypassC;
    float bplusPreamp = Comp::kBplusPreamp;
    float bplusPlate = Comp::kBplusPlate;
    float otPrimaryZ = Comp::kOtPrimaryZ;
    float otSecondaryZ = Comp::kOtSecondaryZ;

    static float otTurnsRatio (float primaryZ = Comp::kOtPrimaryZ,
                               float secondaryZ = Comp::kOtSecondaryZ) noexcept
    {
        return std::sqrt (std::max (primaryZ, 1.0f) / std::max (secondaryZ, 0.1f));
    }

    float turnsRatio() const noexcept { return otTurnsRatio (otPrimaryZ, otSecondaryZ); }

    /** NFB voltage divider ratio (speaker → cathode injection attenuation). */
    float nfbAttenuation() const noexcept
    {
        return v1bCathodeR / (nfbR + v1bCathodeR);
    }

    static float nfbLoopRatio() noexcept { return Comp::kNfbLoopRatio; }
};

/** Guitar digital ↔ grid volts (same −18 dBFS contract as TS). */
inline constexpr float kVoltsPerFullScale = 1.0f;

/** Speaker volts → digital. 4 V peak ≈ 0 dBFS so a small Champ is obviously audible. */
inline constexpr float kSpeakerVoltsFullScale = 4.0f;

inline float digitalToVolts (float x) noexcept { return x * kVoltsPerFullScale; }
inline float voltsToDigital (float v) noexcept { return v / kVoltsPerFullScale; }
inline float speakerVoltsToDigital (float v) noexcept { return v / kSpeakerVoltsFullScale; }

/** Audio-taper-ish volume (0..1 → divider fraction toward wiper). */
inline float volumeFraction (float volume01) noexcept
{
    const float x = std::clamp (volume01, 0.0f, 1.0f);
    return x * x;
}
} // namespace champ
