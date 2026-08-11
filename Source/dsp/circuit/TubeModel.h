#pragma once

#include <cmath>
#include <algorithm>

namespace circuit
{
/**
 * Koren-style triode plate current (Ayumi / SPICE-family parameters).
 * Ip = (E1^EX) / KG1 with E1 from KP/MU/KVB soft knee.
 */
struct TriodeKoren
{
    float mu = 100.0f;
    float ex = 1.4f;
    float kg1 = 1060.0f;
    float kp = 600.0f;
    float kvb = 300.0f;

    /** Plate current (A) for grid-cathode Vgk and plate-cathode Vak. */
    float plateCurrent (float vgk, float vak) const noexcept
    {
        const float va = std::max (vak, 0.0f);
        const float logArg = 1.0f + std::exp (kp * (1.0f / mu + vgk / std::sqrt (kp * kp + va * va)));
        // Avoid log(0); exp path is always > 1
        float e1 = (va / kp) * std::log (logArg);
        if (e1 < 0.0f)
            e1 = 0.0f;
        return std::pow (e1, ex) / kg1;
    }

    /** dIp/dVgk and dIp/dVak via central differences (island size is tiny). */
    void plateConductances (float vgk, float vak, float& gGrid, float& gPlate) const noexcept
    {
        constexpr float h = 1.0e-3f;
        const float i0 = plateCurrent (vgk, vak);
        gGrid = (plateCurrent (vgk + h, vak) - i0) / h;
        gPlate = (plateCurrent (vgk, vak + h) - i0) / h;
        gGrid = std::max (gGrid, 0.0f);
        gPlate = std::max (gPlate, 1.0e-12f);
    }
};

/** Stock 12AX7A-ish Koren set used for Champ V1A / V1B. */
inline TriodeKoren twelveAx7() noexcept
{
    TriodeKoren t;
    t.mu = 100.0f;
    t.ex = 1.4f;
    t.kg1 = 1060.0f;
    t.kp = 600.0f;
    t.kvb = 300.0f;
    return t;
}

/**
 * Simplified beam-power / tetrode plate model with fixed screen.
 * Tuned so idle ≈ 40 mA at Vgk≈−19 V, Vak≈340 V (Champ 6V6 cathode-bias).
 */
struct BeamPowerTube
{
    float vgkCutoff = -55.0f;   // hard cutoff
    float g = 1.15e-4f;         // scales Child-law current
    float expN = 1.5f;
    float earlyVa = 180.0f;     // plate Early-like stretch
    float screenFactor = 0.55f;

    float plateCurrent (float vgk, float vak) const noexcept
    {
        const float va = std::max (vak, 1.0f);
        const float drive = vgk - vgkCutoff;
        if (drive <= 0.0f)
            return 0.0f;
        const float child = g * std::pow (drive, expN);
        return child * (1.0f + screenFactor * va / (va + earlyVa));
    }

    void plateConductances (float vgk, float vak, float& gGrid, float& gPlate) const noexcept
    {
        constexpr float h = 1.0e-3f;
        const float i0 = plateCurrent (vgk, vak);
        gGrid = (plateCurrent (vgk + h, vak) - i0) / h;
        gPlate = (plateCurrent (vgk, vak + h) - i0) / h;
        gGrid = std::max (gGrid, 0.0f);
        gPlate = std::max (gPlate, 1.0e-12f);
    }
};

inline BeamPowerTube sixV6() noexcept
{
    return BeamPowerTube {};
}
} // namespace circuit
