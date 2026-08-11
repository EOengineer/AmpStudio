#pragma once

#include <cmath>
#include <complex>
#include <algorithm>

namespace cab
{
/**
 * Speaker / cab load RLC family (no JUCE).
 *
 * TODO(measured-z): These RLC coefficients are SYNTHETIC PLACEHOLDERS approximating
 * characteristic guitar-cab impedance families (bass resonance + voice-coil rise).
 * Replace with measured REW/CSV tables or cab-specific fits when available.
 * Qualitative Mesa 4x12 shape cues: Rig-Talk "Mesa OS 4x12 vs load boxes"
 * (power-amp response plots — not absolute ohm curves).
 *
 * Topology: Z = Re + jω Le + Zmech,  Zmech = Res || Ces || Les.
 *
 * Measured Z tables and co-process amp↔cab solves are deferred until consuming
 * these synthetic curves through OT/NFB proves insufficient.
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
            // No motional peak: Z ≈ Re (short the Res||Ces||Les branch).
            z.nominalOhms = 8.0f;
            z.re = 8.0f;
            z.le = 0.0f;
            z.res = 1.0e-4f;
            z.ces = 1.0f;
            z.les = 1.0e-12f;
            break;

        case ImpedancePreset::fenderDlx1x12:
            z.nominalOhms = 8.0f;
            z.re = 6.2f;
            z.le = 0.45e-3f;
            z.res = 28.0f;
            z.ces = 120.0e-6f;
            z.les = 28.0e-3f;
            break;

        case ImpedancePreset::marshall4x12Greenback:
            z.nominalOhms = 8.0f;
            z.re = 6.4f;
            z.le = 0.55e-3f;
            z.res = 45.0f;
            z.ces = 90.0e-6f;
            z.les = 35.0e-3f;
            break;

        case ImpedancePreset::mesa4x12V30:
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

    const std::complex<float> yRes = 1.0f / std::max (p.res, 1.0f);
    const std::complex<float> yCes = jw * p.ces;
    const std::complex<float> yLes = 1.0f / (jw * std::max (p.les, 1.0e-9f));
    const auto zMech = 1.0f / (yRes + yCes + yLes);

    return p.re + jw * p.le + zMech;
}
} // namespace cab
