#pragma once

#include <cmath>
#include <algorithm>

namespace circuit
{
/** Shockley diode I–V and small-signal conductance for Newton stamps. */
struct DiodeModel
{
    float isat = 2.52e-9f;   // ~1N4148-ish
    float nVt  = 0.026f * 1.75f; // n * thermal voltage
    float vMax = 0.9f;       // clamp |V| for exp stability

    float current (float v) const noexcept
    {
        const float vc = std::clamp (v, -vMax, vMax);
        return isat * (std::exp (vc / nVt) - 1.0f);
    }

    float conductance (float v) const noexcept
    {
        const float vc = std::clamp (v, -vMax, vMax);
        return (isat / nVt) * std::exp (vc / nVt);
    }
};

/** Anti-parallel pair across a feedback path (voltage V = Va - Vb). */
inline float antiparallelCurrent (const DiodeModel& d, float v) noexcept
{
    return d.current (v) - d.current (-v);
}

inline float antiparallelConductance (const DiodeModel& d, float v) noexcept
{
    return d.conductance (v) + d.conductance (-v);
}

/** Presets used by Tube Screamer diode mods. */
inline DiodeModel siliconSignal() noexcept
{
    return { 2.52e-9f, 0.026f * 1.75f, 0.9f };
}

inline DiodeModel germanium() noexcept
{
    return { 2.0e-7f, 0.026f * 1.0f, 0.5f };
}

inline DiodeModel ledRed() noexcept
{
    return { 1.0e-12f, 0.026f * 2.0f, 2.0f };
}
} // namespace circuit
