#pragma once

#include "../../circuit/DiodeModel.h"
#include "../../circuit/MnaNewton.h"
#include "../../circuit/RcFilters.h"
#include "TsComponents.h"
#include <array>

namespace ts
{
/**
 * Clipping amp island: ideal non-inv op-amp (Vn = Vin) + Zi series RC
 * + Zf (Rdrive || diodes || Cf). Solved with 2×2 Newton each oversampled tick.
 *
 * Topology:
 *   Vn -- Rzi -- Vmid -- Czi -- gnd
 *   Vo -- Rfb -- Vn
 *   Vo -- Cf  -- Vn
 *   anti-parallel diodes Vo ↔ Vn
 */
class TsClippingAmp
{
public:
    void prepare (float sampleRateHz, const ComponentSet& comps) noexcept
    {
        fs = sampleRateHz;
        setComponents (comps);
        reset();
    }

    void setComponents (const ComponentSet& comps) noexcept
    {
        components = comps;
        const float T = 1.0f / std::max (fs, 1.0f);
        czi.prepare (components.ziC, fs);
        // Feedback cap between Vo and Vn uses same trap geq
        cfGeq = 2.0f * components.feedbackC / T;
        configureDiodes (components.diodeMode);
    }

    void setDrive (float drive01) noexcept
    {
        drive = std::clamp (drive01, 0.0f, 1.0f);
        rfb = ComponentSet::driveFeedbackR (drive);
        gfb = 1.0f / std::max (rfb, 1.0f);
    }

    void setDiodeMode (DiodeMode mode) noexcept
    {
        components.diodeMode = mode;
        configureDiodes (mode);
    }

    void reset() noexcept
    {
        czi.reset();
        cfIeq = 0.0f;
        vo = 0.0f;
        vmid = 0.0f;
    }

    float processSample (float vinVolts) noexcept
    {
        const float vn = vinVolts;
        const float gzi = 1.0f / std::max (components.ziR, 1.0f);

        std::array<float, 2> x { vo, vmid };

        circuit::NewtonSolver<2> newton;
        newton.maxIterations = 10;
        newton.absTol = 1.0e-8f;

        const auto fill = [&] (const std::array<float, 2>& xIn,
                               std::array<float, 2>& f,
                               std::array<float, 2 * 2>& j)
        {
            const float voX = xIn[0];
            const float vm  = xIn[1];
            const float vd  = voX - vn; // across feedback

            const float id = diodeCurrent (vd);
            const float gd = diodeConductance (vd);

            // Cap Cf: I = cfGeq*(vo-vn) - cfIeq  (current Vo → Vn)
            const float icf = cfGeq * vd - cfIeq;

            // Cap Czi to ground at mid: I = geq*vm - iEq
            const float iczi = czi.geq * vm - czi.iEq;

            // KCL at Vmid: (vn-vm)*gzi - iczi = 0
            f[0] = (vn - vm) * gzi - iczi;

            // KCL at Vn: (vm-vn)*gzi + (vo-vn)*gfb + icf + id = 0
            f[1] = (vm - vn) * gzi + (voX - vn) * gfb + icf + id;

            // Jacobian
            // f0 wrt vo=0, wrt vm = -gzi - czi.geq
            j[0] = 0.0f;
            j[1] = -gzi - czi.geq;

            // f1 wrt vo = gfb + cfGeq + gd
            // f1 wrt vm = gzi
            j[2] = gfb + cfGeq + gd;
            j[3] = gzi;
        };

        newton.solve (x, fill);
        vo = x[0];
        vmid = x[1];

        // Advance capacitor histories
        czi.advance (vmid);
        {
            const float vd = vo - vn;
            const float icf = cfGeq * vd - cfIeq;
            cfIeq = cfGeq * vd + icf;
        }

        return vo;
    }

    /** Linear HF gain with diodes open (analytic). */
    float idealHfGain() const noexcept { return ComponentSet::idealHfGain (drive); }

    const ComponentSet& getComponents() const noexcept { return components; }

private:
    void configureDiodes (DiodeMode mode) noexcept
    {
        dPos = circuit::siliconSignal();
        dNeg = circuit::siliconSignal();

        switch (mode)
        {
            case DiodeMode::siliconPair:
                break;
            case DiodeMode::asymmetricSi:
                dNeg.isat *= 3.0f; // softer opposite polarity
                break;
            case DiodeMode::germaniumSi:
                dPos = circuit::germanium();
                dNeg = circuit::siliconSignal();
                break;
            case DiodeMode::ledPair:
                dPos = circuit::ledRed();
                dNeg = circuit::ledRed();
                break;
        }
    }

    float diodeCurrent (float vd) const noexcept
    {
        // Positive vd: dPos conducts Vo→Vn; negative: dNeg
        return dPos.current (vd) - dNeg.current (-vd);
    }

    float diodeConductance (float vd) const noexcept
    {
        return dPos.conductance (vd) + dNeg.conductance (-vd);
    }

    ComponentSet components;
    float fs = 48000.0f;
    float drive = 0.5f;
    float rfb = Comp::kDriveFixedR + 0.5f * Comp::kDrivePotR;
    float gfb = 1.0f / rfb;

    circuit::CapacitorTrap czi;
    float cfGeq = 0.0f;
    float cfIeq = 0.0f;

    circuit::DiodeModel dPos, dNeg;
    float vo = 0.0f;
    float vmid = 0.0f;
};
} // namespace ts
