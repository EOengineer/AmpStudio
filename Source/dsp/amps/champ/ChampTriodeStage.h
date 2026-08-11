#pragma once

#include "../../circuit/MnaNewton.h"
#include "../../circuit/RcFilters.h"
#include "../../circuit/TubeModel.h"
#include "ChampComponents.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace champ
{
/**
 * Cathode-biased 12AX7 stage.
 * Unknowns: plate Vp, cathode Vk. Grid voltage is driven (AC + 0 DC).
 * Optional NFB current into cathode from speaker volts through Rnfb.
 */
class ChampTriodeStage
{
public:
    void prepare (float sampleRateHz, float plateR, float cathodeR, float bypassC,
                  float bplus, float nfbR = 0.0f) noexcept
    {
        fs = sampleRateHz;
        ra = plateR;
        rk = cathodeR;
        vb = bplus;
        ga = 1.0f / std::max (ra, 1.0f);
        gk = 1.0f / std::max (rk, 1.0f);
        bypass.prepare (bypassC, fs);
        tube = circuit::twelveAx7();
        setNfbOhms (nfbR);
        reset();
    }

    /** Update Rnfb without resettling the island (Stock/Off toggle). */
    void setNfbOhms (float nfbR) noexcept
    {
        rnfb = nfbR;
        gnfb = (rnfb > 1.0f) ? (1.0f / rnfb) : 0.0f;
    }

    void reset() noexcept
    {
        // Champ-typical idle seeds (Robinette / Weber ballpark). Fixed-point
        // Ip*R iteration is unstable with this Koren set, so seed then Newton.
        vk = 1.5f;
        vp = 170.0f;
        bypass.reset();
        bypass.vPrev = vk;
        bypass.iEq = bypass.geq * vk;

        for (int i = 0; i < 24; ++i)
            processSample (0.0f, 0.0f);

        // Re-anchor bypass to zero cap current, then settle again at DC.
        bypass.vPrev = vk;
        bypass.iEq = bypass.geq * vk;
        for (int i = 0; i < 16; ++i)
            processSample (0.0f, 0.0f);
        bypass.vPrev = vk;
        bypass.iEq = bypass.geq * vk;
    }

    /** Process one sample. vinGrid = grid volts (AC), vSpeaker for NFB (0 if unused). */
    float processSample (float vinGrid, float vSpeaker = 0.0f) noexcept
    {
        // Grid-to-ground window (leak at 0 V). Blocks coupling-cap DC dumps.
        vinGrid = std::clamp (vinGrid, -5.0f, 1.0f);
        if (! std::isfinite (vSpeaker))
            vSpeaker = 0.0f;

        std::array<float, 2> x { vp, vk };
        circuit::NewtonSolver<2> newton;
        newton.maxIterations = 10;
        newton.absTol = 1.0e-7f;

        const auto fill = [&] (const std::array<float, 2>& xIn,
                               std::array<float, 2>& f,
                               std::array<float, 2 * 2>& j)
        {
            const float vpX = xIn[0];
            const float vkX = xIn[1];
            const float vgk = std::clamp (vinGrid - vkX, -5.0f, 1.0f);
            const float vak = vpX - vkX;

            float gG = 0.0f, gP = 0.0f;
            const float ip = tube.plateCurrent (vgk, vak);
            tube.plateConductances (vgk, vak, gG, gP);

            // Cathode bypass: iC = geq*vk - iEq
            const float ic = bypass.geq * vkX - bypass.iEq;

            // NFB: current from speaker into cathode node through Rnfb
            const float infb = gnfb * (vSpeaker - vkX);

            // KCL plate: (Vb - Vp)*ga - Ip = 0
            f[0] = (vb - vpX) * ga - ip;

            // KCL cathode: Ip + infb - vk*gk - ic = 0
            f[1] = ip + infb - vkX * gk - ic;

            // dIp/dVp = gP, dIp/dVk = -gG - gP  (vgk=vin-vk, vak=vp-vk)
            const float dIp_dVp = gP;
            const float dIp_dVk = -gG - gP;

            // f0 wrt vp, vk
            j[0] = -ga - dIp_dVp;
            j[1] = -dIp_dVk;

            // f1 wrt vp, vk
            j[2] = dIp_dVp;
            j[3] = dIp_dVk - gk - bypass.geq - gnfb;
        };

        newton.solve (x, fill);
        if (! std::isfinite (x[0]) || ! std::isfinite (x[1]))
            return vp;

        vp = x[0];
        vk = x[1];
        bypass.advance (vk);
        return vp;
    }

    float getPlate() const noexcept { return vp; }
    float getCathode() const noexcept { return vk; }

private:
    float fs = 48000.0f;
    float ra = 100e3f, rk = 1.5e3f, vb = 250.0f, rnfb = 0.0f;
    float ga = 0.0f, gk = 0.0f, gnfb = 0.0f;
    float vp = 170.0f, vk = 1.5f;
    circuit::CapacitorTrap bypass;
    circuit::TriodeKoren tube;
};

/** Series coupling cap into a resistive grid leak (one-pole HPF). */
class CouplingHp
{
public:
    void prepare (float cFarads, float rLoadOhms, float fs) noexcept
    {
        // H(s) = sRC/(1+sRC) — bilinear
        const float rc = std::max (cFarads * rLoadOhms, 1.0e-12f);
        const float T = 1.0f / std::max (fs, 1.0f);
        const float a = 2.0f * rc / T;
        const float inv = 1.0f / (1.0f + a);
        b0 = a * inv;
        b1 = -a * inv;
        a1 = (1.0f - a) * inv;
        reset();
    }

    void reset() noexcept
    {
        x1 = 0.0f;
        y1 = 0.0f;
        dcIn = 0.0f;
    }

    /**
     * Equilibrium of the series coupling cap: vC = Vp_idle so grid AC starts at 0.
     * Call after the driving triode has settled, and after any coupler reset().
     */
    void seedFromPlate (float plateVolts) noexcept
    {
        dcIn = plateVolts;
        x1 = 0.0f;
        y1 = 0.0f;
    }

    /** AC through the coupling cap (plate minus seeded idle DC, then HPF). */
    float processFromPlate (float plateVolts) noexcept
    {
        const float x = plateVolts - dcIn;
        const float y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    static float cornerHz (float cFarads, float rLoadOhms) noexcept
    {
        return circuit::OnePoleLpRc::cornerHz (rLoadOhms, cFarads);
    }

private:
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f;
    float x1 = 0.0f, y1 = 0.0f, dcIn = 0.0f;
};

/** Grid stopper + Miller-ish input LPF (passive). */
class GridInputFilter
{
public:
    void prepare (float rStopper, float cMiller, float fs) noexcept
    {
        lp.prepare (rStopper, cMiller, fs);
    }

    void reset() noexcept { lp.reset(); }

    float process (float v) noexcept { return lp.process (v); }

private:
    circuit::OnePoleLpRc lp;
};
} // namespace champ
