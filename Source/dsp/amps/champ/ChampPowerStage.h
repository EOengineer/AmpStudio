#pragma once

#include "../../cabs/SpeakerRlc.h"
#include "../../circuit/MnaNewton.h"
#include "../../circuit/RcFilters.h"
#include "../../circuit/TubeModel.h"
#include "ChampComponents.h"
#include <array>
#include <cmath>

namespace champ
{
/**
 * 6V6GT + ideal OT into speaker secondary (AC-coupled through the transformer).
 *
 * DC bias is solved with Vs=0 (magnetizing path carries idle Ip). Audio uses
 * ipAc = Ip − IpDc into the secondary Z (Re + Le + Res||Ces||Les).
 *
 * loadContext → cab::resolveLoadRlc in ChampEngine (flat 8 Ω when unloaded).
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
            leTrap.prepare (speaker.le, fs);
        else
            leTrap.reset();

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
    }

    void reset() noexcept
    {
        cathodeBypass.reset();
        leTrap.reset();
        cesTrap.reset();
        lesTrap.reset();
        vs = 0.0f;
        vCoil = 0.0f;
        settleBias();
    }

    /** @param vg 6V6 grid AC volts; @return speaker secondary AC volts */
    float processSample (float vg) noexcept
    {
        if (! useLe && ! useMech)
            return processResistive (vg);
        return processReactive (vg);
    }

    float getSpeakerVolts() const noexcept { return vs; }
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
    }

    float processResistive (float vg) noexcept
    {
        std::array<float, 2> x { vs, vk };
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

        newton.solve (x, fill);
        vs = x[0];
        vk = x[1];
        cathodeBypass.advance (vk);
        return vs;
    }

    float processReactive (float vg) noexcept
    {
        std::array<float, 3> x { vs, vk, vCoil };
        circuit::NewtonSolver<3> newton;
        newton.maxIterations = 12;
        newton.absTol = 1.0e-7f;

        const auto fill = [&] (const std::array<float, 3>& xIn,
                               std::array<float, 3>& f,
                               std::array<float, 3 * 3>& j)
        {
            const float vsX = xIn[0];
            const float vkX = xIn[1];
            const float vcX = xIn[2];

            const float vgk = vg - vkX;
            const float vak = vakDc - n * vsX - (vkX - vkDc);

            float gG = 0.0f, gP = 0.0f;
            const float ip = tube.plateCurrent (vgk, vak);
            tube.plateConductances (vgk, vak, gG, gP);
            const float ipAc = ip - ipDc;
            const float is = n * ipAc;

            const float icath = cathodeBypass.geq * vkX - cathodeBypass.iEq;
            const float iRe = (vsX - vcX) * gre;

            float iLe = 0.0f;
            float gLe = 0.0f;
            if (useLe)
            {
                iLe = leTrap.current (vcX);
                gLe = leTrap.geq;
            }

            float iMech = 0.0f;
            float gMech = 0.0f;
            if (useMech)
            {
                const float iCes = cesTrap.geq * vcX - cesTrap.iEq;
                const float iLes = lesTrap.current (vcX);
                iMech = vcX * gres + iCes + iLes;
                gMech = gres + cesTrap.geq + lesTrap.geq;
            }

            const float dIp_dVs = gP * (-n);
            const float dIp_dVk = -gG - gP;

            if (! useLe)
            {
                f[0] = iRe - is;
                f[1] = ip - vkX * gk - icath;
                f[2] = iRe - iMech;

                j[0] = gre - n * dIp_dVs;
                j[1] = -n * dIp_dVk;
                j[2] = -gre;

                j[3] = dIp_dVs;
                j[4] = dIp_dVk - gk - cathodeBypass.geq;
                j[5] = 0.0f;

                j[6] = gre;
                j[7] = 0.0f;
                j[8] = -gre - gMech;
            }
            else
            {
                f[0] = iRe - is;
                f[1] = ip - vkX * gk - icath;
                f[2] = iRe - iLe - iMech;

                j[0] = gre - n * dIp_dVs;
                j[1] = -n * dIp_dVk;
                j[2] = -gre;

                j[3] = dIp_dVs;
                j[4] = dIp_dVk - gk - cathodeBypass.geq;
                j[5] = 0.0f;

                j[6] = gre;
                j[7] = 0.0f;
                j[8] = -gre - gLe - gMech;
            }
        };

        newton.solve (x, fill);
        vs = x[0];
        vk = x[1];
        vCoil = x[2];
        cathodeBypass.advance (vk);
        if (useLe)
            leTrap.advance (vCoil);
        if (useMech)
        {
            cesTrap.advance (vCoil);
            lesTrap.advance (vCoil);
        }
        return vs;
    }

    float fs = 48000.0f;
    float n = 25.0f;
    float vb = 360.0f;
    float rk = 470.0f;
    float gk = 0.0f;
    float re = 8.0f;
    float gre = 0.125f;
    float gres = 0.0f;
    float vs = 0.0f, vk = 19.0f, vCoil = 0.0f;
    float vkDc = 19.0f, vakDc = 340.0f, ipDc = 0.04f;
    bool useLe = false;
    bool useMech = false;
    ComponentSet components;
    cab::SpeakerRlc speaker = cab::makePreset (cab::ImpedancePreset::flat8);
    circuit::CapacitorTrap cathodeBypass;
    circuit::InductorTrap leTrap;
    circuit::CapacitorTrap cesTrap;
    circuit::InductorTrap lesTrap;
    circuit::BeamPowerTube tube;
};
} // namespace champ
