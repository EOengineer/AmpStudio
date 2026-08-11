#pragma once

#include "../../cabs/SpeakerRlc.h"
#include "../../circuit/MnaNewton.h"
#include "../../circuit/RcFilters.h"
#include "../../circuit/TubeModel.h"
#include "ChampComponents.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace champ
{
/**
 * 6V6GT + ideal OT into speaker secondary (AC-coupled through the transformer).
 *
 * DC bias is solved with Vs=0 (magnetizing path carries idle Ip). Audio:
 * tube Newton sees Re only; ipAc drives motional Z (Res||Ces||Les) for vs/NFB
 * plus a lossy Le (sL||Reddy) on speaker volts only (not NFB).
 *
 * loadContext → cab::resolveLoadRlc in ChampEngine / ChampDsp (flat 8 Ω when unloaded).
 */
class ChampPowerStage
{
public:
    void prepare (float sampleRateHz, const ComponentSet& comps) noexcept
    {
        fs = sampleRateHz;
        components = comps;
        n = comps.turnsRatio();
        vb = comps.bplusPlate;
        rk = comps.powerCathodeR;
        gk = 1.0f / std::max (rk, 1.0f);
        cathodeBypass.prepare (comps.powerBypassC, fs);
        tube = circuit::sixV6();
        setSpeakerRlc (cab::makePreset (cab::ImpedancePreset::flat8));
        reset();
    }

    void setSpeakerRlc (const cab::SpeakerRlc& z) noexcept
    {
        speaker = z;
        re = std::max (speaker.re, 0.5f);
        gre = 1.0f / re;

        useLe = speaker.le > 1.0e-6f;
        if (useLe)
        {
            // Eddy R puts the Le pole ~8 kHz so |Z| saturates (no Nyquist spike).
            const float reddy = 2.0f * 3.14159265f * kEddyCornerHz * speaker.le;
            lossyLe.prepare (speaker.le, reddy, fs);
            lossyLe.prime (lastIs);
        }
        else
            lossyLe.reset();

        const bool openMech = speaker.res > 1.0e6f || speaker.res < 0.1f;
        useMech = ! openMech;
        if (useMech)
        {
            gres = 1.0f / std::max (speaker.res, 1.0f);
            cesTrap.prepare (speaker.ces, fs);
            lesTrap.prepare (std::max (speaker.les, 1.0e-6f), fs);
        }
        else
        {
            gres = 0.0f;
            cesTrap.reset();
            lesTrap.reset();
        }

        if (useMech)
            primeReactiveTraps (lastIs);
    }

    void reset() noexcept
    {
        cathodeBypass.reset();
        lossyLe.reset();
        cesTrap.reset();
        lesTrap.reset();
        vs = 0.0f;
        vsTube = 0.0f;
        vCoil = 0.0f;
        nfbSense = 0.0f;
        lastIs = 0.0f;
        settleBias();
    }

    static constexpr float kMaxSpeakerV = 40.0f;

    /** @param vg 6V6 grid AC volts; @return speaker secondary AC volts */
    float processSample (float vg) noexcept
    {
        // Grid leak at 0 V; window keeps vgk in roughly −55…0 around cathode bias.
        vg = std::clamp (vg, -40.0f, 2.0f);

        // Tube Newton always sees Re (the stable flat-8 path). Motional Z and
        // lossy Le are linear filters on is.
        const float y = processResistive (vg);
        if (! std::isfinite (y) || ! std::isfinite (vsTube) || ! std::isfinite (vk)
            || std::abs (vsTube) > kMaxSpeakerV)
        {
            vs = lastGoodVs;
            vk = lastGoodVk;
            vCoil = lastGoodVCoil;
            nfbSense = lastGoodNfbSense;
            return lastGoodVs;
        }

        if (useMech || useLe)
            applyLinearZ (lastIs);
        else
        {
            vs = vsTube;
            nfbSense = vsTube;
        }

        if (! std::isfinite (vs) || std::abs (vs) > kMaxSpeakerV)
        {
            vs = lastGoodVs;
            nfbSense = lastGoodNfbSense;
            return lastGoodVs;
        }

        lastGoodVs = vs;
        lastGoodVk = vk;
        lastGoodVCoil = vCoil;
        lastGoodNfbSense = nfbSense;
        return vs;
    }

    float getSpeakerVolts() const noexcept { return vs; }
    /** NFB tap: Re + Zmech (lossy Le is on vs only). */
    float getNfbSenseVolts() const noexcept { return nfbSense; }
    float getCathode() const noexcept { return vk; }
    float getTurnsRatio() const noexcept { return n; }
    float getIdlePlateCurrent() const noexcept { return ipDc; }
    const cab::SpeakerRlc& getSpeakerRlc() const noexcept { return speaker; }

private:
    void settleBias() noexcept
    {
        // DC operating point with secondary open (ideal OT magnetizing).
        vk = 19.0f;
        float vak = vb - vk;
        for (int i = 0; i < 24; ++i)
        {
            const float vgk = 0.0f - vk;
            ipDc = tube.plateCurrent (vgk, vak);
            // Cathode: Ip * Rk ≈ Vk (bypass open at DC → just Rk)
            vk = ipDc * rk;
            vak = vb - vk;
        }
        vkDc = vk;
        vakDc = vak;
        ipDc = tube.plateCurrent (-vkDc, vakDc);
        // Warm bypass cap to VkDc
        cathodeBypass.vPrev = vkDc;
        cathodeBypass.iEq = cathodeBypass.geq * vkDc;
        lastGoodVs = vs;
        lastGoodVk = vk;
        lastGoodVCoil = vCoil;
        lastGoodNfbSense = nfbSense;
    }

    float processResistive (float vg) noexcept
    {
        std::array<float, 2> x { vsTube, vk };
        circuit::NewtonSolver<2> newton;
        newton.maxIterations = 12;
        newton.absTol = 1.0e-7f;

        const auto fill = [&] (const std::array<float, 2>& xIn,
                               std::array<float, 2>& f,
                               std::array<float, 2 * 2>& j)
        {
            const float vsX = xIn[0];
            const float vkX = xIn[1];
            const float vgk = vg - vkX;
            // AC plate drop through reflected load; DC bias held in vakDc/vkDc
            const float vak = vakDc - n * vsX - (vkX - vkDc);

            float gG = 0.0f, gP = 0.0f;
            const float ip = tube.plateCurrent (vgk, vak);
            tube.plateConductances (vgk, vak, gG, gP);
            const float ipAc = ip - ipDc;

            const float ic = cathodeBypass.geq * vkX - cathodeBypass.iEq;
            const float is = n * ipAc;

            f[0] = vsX * gre - is;
            f[1] = ip - vkX * gk - ic;

            // dVak/dVs = -n, dVak/dVk = -1
            const float dIp_dVs = gP * (-n);
            const float dIp_dVk = -gG + gP * (-1.0f);

            j[0] = gre - n * dIp_dVs;
            j[1] = -n * dIp_dVk;
            j[2] = dIp_dVs;
            j[3] = dIp_dVk - gk - cathodeBypass.geq;
        };

        const auto result = newton.solve (x, fill);
        if (! std::isfinite (x[0]) || ! std::isfinite (x[1])
            || std::abs (x[0]) > kMaxSpeakerV
            || result.residualNorm > 1.0f)
            return lastGoodVs;

        vsTube = x[0];
        vk = x[1];
        const float vgk = vg - vk;
        const float vak = vakDc - n * vsTube - (vk - vkDc);
        lastIs = n * (tube.plateCurrent (vgk, vak) - ipDc);
        cathodeBypass.advance (vk);
        return vsTube;
    }

    /** Zero motional drop at the current is so a curve change does not click. */
    void primeReactiveTraps (float is) noexcept
    {
        lesTrap.iEq = 0.0f;
        cesTrap.iEq = -is;
        vCoil = 0.0f;
    }

    void applyLinearZ (float is) noexcept
    {
        float vMech = 0.0f;
        if (useMech)
        {
            const float gMech = gres + cesTrap.geq + lesTrap.geq;
            vMech = (is - lesTrap.iEq + cesTrap.iEq) / std::max (gMech, 1.0e-12f);
            vCoil = vMech;
            cesTrap.advance (vMech);
            lesTrap.advance (vMech);
        }

        const float vLe = useLe ? lossyLe.process (is) : 0.0f;
        nfbSense = is * re + vMech;
        vs = nfbSense + vLe;
    }

    float fs = 48000.0f;
    float n = 25.0f;
    float vb = 360.0f;
    float rk = 470.0f;
    float gk = 0.0f;
    float re = 8.0f;
    float gre = 0.125f;
    float gres = 0.0f;
    float vs = 0.0f, vsTube = 0.0f, vk = 19.0f, vCoil = 0.0f, nfbSense = 0.0f;
    float lastIs = 0.0f;
    float lastGoodVs = 0.0f, lastGoodVk = 19.0f, lastGoodVCoil = 0.0f, lastGoodNfbSense = 0.0f;
    float vkDc = 19.0f, vakDc = 340.0f, ipDc = 0.04f;
    static constexpr float kEddyCornerHz = 8000.0f;
    bool useLe = false;
    bool useMech = false;
    ComponentSet components;
    cab::SpeakerRlc speaker = cab::makePreset (cab::ImpedancePreset::flat8);
    circuit::CapacitorTrap cathodeBypass;
    circuit::ParallelLRDrop lossyLe;
    circuit::CapacitorTrap cesTrap;
    circuit::InductorTrap lesTrap;
    circuit::BeamPowerTube tube;
};
} // namespace champ
