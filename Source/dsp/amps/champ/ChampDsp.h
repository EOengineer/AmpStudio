#pragma once

#include "../../cabs/SpeakerRlc.h"
#include "ChampComponents.h"
#include "ChampPowerStage.h"
#include "ChampTriodeStage.h"
#include <algorithm>
#include <cmath>

namespace champ
{
/**
 * Canonical Champ 5F1 sample path (JUCE-free).
 *
 * ChampEngine is a thin AudioBuffer façade; champ_verify must tick this
 * class so offline checks cannot drift from the plugin.
 *
 * Base sample rate only — do not wrap in circuit::Oversampler to debug
 * (JUCE half-band OS has muted this amp in-host).
 */
class ChampDsp
{
public:
    /** Approximate Miller C at V1A grid for stopper LPF. */
    static constexpr float kMillerC = 100.0e-12f;

    void prepare (float sampleRateHz)
    {
        fs = std::max (sampleRateHz, 1.0f);
        rebuildStages();
        reset();
    }

    void reset()
    {
        inputFilter.reset();
        v1a.reset();
        couple1.reset();
        couple1.seedFromPlate (v1a.getPlate());
        v1b.reset();
        couple2.reset();
        couple2.seedFromPlate (v1b.getPlate());
        power.reset();
        lastAc1 = 0.0f;
        lastAc2 = 0.0f;
        lastSpeakerV = 0.0f;
        lastDigitalOut = 0.0f;
    }

    void setVolume (float volume01) noexcept
    {
        volume = std::clamp (volume01, 0.0f, 1.0f);
    }

    /** Stock = 22k speaker → V1B cathode; Off = resistor lifted. */
    void setNfbEnabled (bool on) noexcept
    {
        nfbEnabled = on;
        v1b.setNfbOhms (on ? components.nfbR : 0.0f);
    }

    void setComponentSet (const ComponentSet& c)
    {
        components = c;
        if (fs > 0.0f)
            rebuildStages();
    }

    /** Cab Z(f) RLC into OT secondary; flat 8 Ω is the default. */
    void setSpeakerRlc (const cab::SpeakerRlc& rlc) noexcept
    {
        if (cab::sameRlc (rlc, cachedLoadRlc))
            return;
        cachedLoadRlc = rlc;
        power.setSpeakerRlc (rlc);
    }

    float getVolume() const noexcept { return volume; }
    bool getNfbEnabled() const noexcept { return nfbEnabled; }
    float getInputZohms() const noexcept { return components.gridLeakR; }
    float getOutputZohms() const noexcept { return Comp::kOtSecondaryZ; }

    ComponentSet& getComponentSet() noexcept { return components; }
    const ComponentSet& getComponentSet() const noexcept { return components; }

    ChampTriodeStage& getV1a() noexcept { return v1a; }
    ChampTriodeStage& getV1b() noexcept { return v1b; }
    ChampPowerStage& getPower() noexcept { return power; }
    const ChampTriodeStage& getV1a() const noexcept { return v1a; }
    const ChampTriodeStage& getV1b() const noexcept { return v1b; }
    const ChampPowerStage& getPower() const noexcept { return power; }

    float getLastAc1() const noexcept { return lastAc1; }
    float getLastAc2() const noexcept { return lastAc2; }
    float getLastSpeakerV() const noexcept { return lastSpeakerV; }
    float getLastDigitalOut() const noexcept { return lastDigitalOut; }

    /**
     * One host-rate sample: digital in → digital out.
     * Same loop as the plugin (input LPF, volume taper, 1-sample NFB, ±2 clamp).
     */
    float processSample (float digitalIn) noexcept
    {
        float g = digitalToVolts (digitalIn);
        g = inputFilter.process (g);

        const float p1 = v1a.processSample (g, 0.0f);
        lastAc1 = couple1.processFromPlate (p1) * volumeFraction (volume);

        // V1B and 6V6 each invert; raw speaker volts into the cathode is
        // positive feedback. Invert so Stock reduces gain. Sense is Re+Zmech
        // (no Le) so voice-coil rise does not motorboat the 1-sample NFB loop.
        const float nfbV = nfbEnabled ? -power.getNfbSenseVolts() : 0.0f;
        const float p2 = v1b.processSample (lastAc1, nfbV);
        lastAc2 = couple2.processFromPlate (p2);

        lastSpeakerV = power.processSample (lastAc2);
        float out = speakerVoltsToDigital (lastSpeakerV);
        if (! std::isfinite (out))
            out = lastDigitalOut;
        out = std::clamp (out, -2.0f, 2.0f);
        lastDigitalOut = out;
        return out;
    }

private:
    void rebuildStages()
    {
        inputFilter.prepare (components.gridStopperR, kMillerC, fs);
        v1a.prepare (fs, components.v1aPlateR, components.v1aCathodeR,
                     components.v1aBypassC, components.bplusPreamp, 0.0f);
        couple1.prepare (components.couplingC1, components.volumePotR, fs);
        v1b.prepare (fs, components.v1bPlateR, components.v1bCathodeR,
                     1.0e-12f,
                     components.bplusPreamp,
                     nfbEnabled ? components.nfbR : 0.0f);
        couple2.prepare (components.couplingC2, components.powerGridLeakR, fs);
        power.prepare (fs, components);
        // prepare() stamps flat 8; restore cached cab Z (or flat 8 default).
        power.setSpeakerRlc (cachedLoadRlc);
        couple1.seedFromPlate (v1a.getPlate());
        couple2.seedFromPlate (v1b.getPlate());
    }

    float fs = 0.0f;
    ComponentSet components;
    cab::SpeakerRlc cachedLoadRlc = cab::makePreset (cab::ImpedancePreset::flat8);
    float volume = 0.5f;
    bool nfbEnabled = true;
    float lastAc1 = 0.0f;
    float lastAc2 = 0.0f;
    float lastSpeakerV = 0.0f;
    float lastDigitalOut = 0.0f;
    GridInputFilter inputFilter;
    ChampTriodeStage v1a;
    CouplingHp couple1;
    ChampTriodeStage v1b;
    CouplingHp couple2;
    ChampPowerStage power;
};
} // namespace champ
