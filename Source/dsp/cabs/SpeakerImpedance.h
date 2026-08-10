#pragma once

#include "../ElectricalPort.h"
#include <cmath>
#include <complex>

namespace cab
{
/**
 * Speaker / cab load published to amp via ElectricalPort::response.
 *
 * TODO(measured-z): These RLC coefficients are SYNTHETIC PLACEHOLDERS approximating
 * characteristic guitar-cab impedance families (bass resonance + voice-coil rise).
 * Replace with measured REW/CSV tables or cab-specific fits when available.
 * Qualitative Mesa 4x12 shape cues: Rig-Talk "Mesa OS 4x12 vs load boxes"
 * (power-amp response plots — not absolute ohm curves).
 *
 * Topology: Z = Re + jω Le + Zmech,  Zmech = Res || Ces || Les.
 */
enum class ImpedancePreset
{
    flat8 = 0,
    fenderDlx1x12 = 1,
    marshall4x12Greenback = 2,
    mesa4x12V30 = 3
};

struct SpeakerRlc
{
    float nominalOhms = 8.0f;
    float re = 6.5f;     // DC / voice-coil resistance
    float le = 0.5e-3f;  // voice-coil inductance (H)
    float res = 40.0f;   // mechanical resonance peak resistance
    float ces = 80.0e-6f;
    float les = 40.0e-3f;
    bool syntheticPlaceholder = true; // TODO(measured-z): clear when measured data loaded
};

inline SpeakerRlc makePreset (ImpedancePreset preset) noexcept
{
    SpeakerRlc z;
    z.syntheticPlaceholder = true; // TODO(measured-z)

    switch (preset)
    {
        case ImpedancePreset::flat8:
            z.nominalOhms = 8.0f;
            z.re = 8.0f;
            z.le = 0.0f;
            z.res = 1.0e9f; // effectively open resonance branch
            z.ces = 1.0e-12f;
            z.les = 1.0e3f;
            break;

        case ImpedancePreset::fenderDlx1x12:
            // Open-back 1x12 family: lower / broader resonance peak.
            z.nominalOhms = 8.0f;
            z.re = 6.2f;
            z.le = 0.45e-3f;
            z.res = 28.0f;
            z.ces = 120.0e-6f;
            z.les = 28.0e-3f;
            break;

        case ImpedancePreset::marshall4x12Greenback:
            // Closed 4x12 Greenback-ish: sharper mid-bass bump.
            z.nominalOhms = 8.0f;
            z.re = 6.4f;
            z.le = 0.55e-3f;
            z.res = 45.0f;
            z.ces = 90.0e-6f;
            z.les = 35.0e-3f;
            break;

        case ImpedancePreset::mesa4x12V30:
            // Mesa OS 4x12 / V30 family — stronger resonance vs Greenback averages
            // (qualitative vs Rig-Talk cab-comparison posts).
            z.nominalOhms = 8.0f;
            z.re = 6.8f;
            z.le = 0.65e-3f;
            z.res = 55.0f;
            z.ces = 100.0e-6f;
            z.les = 45.0e-3f;
            break;
    }

    return z;
}

inline std::complex<float> evaluateRlc (const SpeakerRlc& p, float frequencyHz) noexcept
{
    constexpr float kPi = 3.14159265358979323846f;
    const float f = std::max (frequencyHz, 1.0f);
    const float w = 2.0f * kPi * f;
    const std::complex<float> jw (0.0f, w);

    // Mechanical parallel: Res || Ces || Les
    const std::complex<float> yRes = 1.0f / std::max (p.res, 1.0f);
    const std::complex<float> yCes = jw * p.ces;
    const std::complex<float> yLes = 1.0f / (jw * std::max (p.les, 1.0e-9f));
    const auto zMech = 1.0f / (yRes + yCes + yLes);

    return p.re + jw * p.le + zMech;
}

/**
 * ImpedanceResponse owned by CabIR (and readable by Champ via dynamic_cast).
 * Exposes RLC coeffs for time-domain OT secondary stamps.
 */
class SpeakerImpedance final : public ImpedanceResponse
{
public:
    SpeakerImpedance() { setPreset (ImpedancePreset::mesa4x12V30); }

    void setPreset (ImpedancePreset preset) noexcept
    {
        currentPreset = preset;
        rlc = makePreset (preset);
    }

    ImpedancePreset getPreset() const noexcept { return currentPreset; }
    const SpeakerRlc& getRlc() const noexcept { return rlc; }
    bool isSyntheticPlaceholder() const noexcept { return rlc.syntheticPlaceholder; }

    std::complex<float> evaluate (float frequencyHz) const override
    {
        return evaluateRlc (rlc, frequencyHz);
    }

    float nominalOhms() const noexcept { return rlc.nominalOhms; }

    static constexpr bool kSyntheticPlaceholderDefault = true; // TODO(measured-z)

private:
    ImpedancePreset currentPreset = ImpedancePreset::mesa4x12V30;
    SpeakerRlc rlc = makePreset (ImpedancePreset::mesa4x12V30);
};

/** Resolve speaker load for an amp: cab SpeakerImpedance, else resistive 8 Ω. */
inline SpeakerRlc resolveLoadRlc (const ElectricalPort& loadContext) noexcept
{
    if (loadContext.isFrequencyDependent && loadContext.response != nullptr)
    {
        if (auto* z = dynamic_cast<const SpeakerImpedance*> (loadContext.response))
            return z->getRlc();
    }

    // Unloaded (~1 MΩ) or unknown — do NOT use as speaker; fall back to flat 8 Ω.
    if (loadContext.resistiveOhms > 1000.0f)
        return makePreset (ImpedancePreset::flat8);

    SpeakerRlc flat = makePreset (ImpedancePreset::flat8);
    flat.re = loadContext.resistiveOhms;
    flat.nominalOhms = loadContext.resistiveOhms;
    return flat;
}
} // namespace cab
