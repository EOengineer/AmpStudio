#pragma once

#include "../../cabs/SpeakerImpedance.h"
#include "ChampComponents.h"
#include "ChampPowerStage.h"
#include "ChampTriodeStage.h"
#include <JuceHeader.h>
#include <cmath>

namespace champ
{
/**
 * Champ 5F1 at base sample rate (same path as champ_verify).
 *
 * juce::dsp::Oversampling has muted this amp in-host while the identical
 * stages pass offline; 4× OS is deferred until this path is audible.
 *
 * Stripped vs full 5F1: Hi jack, no 5Y3. Unloaded OT is flat 8 Ω; a Cab IR
 * Z-curve is stamped via loadContext. NFB is Stock 22k by default
 * (Off = lifted resistor).
 */
class ChampEngine
{
public:
    /** Approximate Miller C at V1A grid for stopper LPF. */
    static constexpr float kMillerC = 100.0e-12f;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        baseSpec = spec;
        rebuildStages ((float) std::max (spec.sampleRate, 1.0));
        reset();
        dbgBlocks = 0;
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
        if (baseSpec.sampleRate > 0.0)
            rebuildStages ((float) baseSpec.sampleRate);
    }

    ComponentSet& getComponentSet() noexcept { return components; }
    const ComponentSet& getComponentSet() const noexcept { return components; }

    /** Cab Z(f) when present; unloaded / high-Z falls back to flat 8 Ω. */
    void setLoadContext (const ElectricalPort& load) noexcept
    {
        applyLoadRlc (cab::resolveLoadRlc (load));
    }

    float getInputZohms() const noexcept { return components.gridLeakR; }
    float getOutputZohms() const noexcept { return Comp::kOtSecondaryZ; }
    int getLatencySamples() const noexcept { return 0; }

    ChampTriodeStage& getV1a() noexcept { return v1a; }
    ChampTriodeStage& getV1b() noexcept { return v1b; }
    ChampPowerStage& getPower() noexcept { return power; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0 || numCh <= 0)
            return;

        auto* left = buffer.getWritePointer (0);
        const float vol = volumeFraction (volume);

        float inPeak = 0.0f;
        float outPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            inPeak = std::max (inPeak, std::abs (left[i]));

            float g = digitalToVolts (left[i]);
            g = inputFilter.process (g);

            const float p1 = v1a.processSample (g, 0.0f);
            float ac1 = couple1.processFromPlate (p1);
            ac1 *= vol;

            // V1B and 6V6 each invert; raw speaker volts into the cathode is
            // positive feedback. Invert so Stock reduces gain. Sense is Re+Zmech
            // (no Le) so voice-coil rise does not motorboat the 1-sample NFB loop.
            const float nfbV = nfbEnabled ? -power.getNfbSenseVolts() : 0.0f;
            const float p2 = v1b.processSample (ac1, nfbV);
            const float ac2 = couple2.processFromPlate (p2);

            lastSpeakerV = power.processSample (ac2);
            float out = speakerVoltsToDigital (lastSpeakerV);
            if (! std::isfinite (out))
                out = lastDigitalOut;
            out = std::clamp (out, -2.0f, 2.0f);
            lastDigitalOut = out;
            left[i] = out;
            outPeak = std::max (outPeak, std::abs (out));
        }

        for (int ch = 1; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

#if JUCE_DEBUG
        if (dbgBlocks < 8)
        {
            DBG ("Champ 5F1 block " << dbgBlocks
                 << " inPeak=" << inPeak
                 << " outPeak=" << outPeak
                 << " vol=" << vol
                 << " Vp1=" << v1a.getPlate()
                 << " Vp2=" << v1b.getPlate()
                 << " Vs=" << lastSpeakerV);
            ++dbgBlocks;
        }
#endif
    }

private:
    void rebuildStages (float fs)
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

    void applyLoadRlc (const cab::SpeakerRlc& rlc) noexcept
    {
        if (cab::sameRlc (rlc, cachedLoadRlc))
            return;
        cachedLoadRlc = rlc;
        power.setSpeakerRlc (rlc);
    }

    juce::dsp::ProcessSpec baseSpec {};
    ComponentSet components;
    cab::SpeakerRlc cachedLoadRlc = cab::makePreset (cab::ImpedancePreset::flat8);
    float volume = 0.5f;
    bool nfbEnabled = true;
    float lastSpeakerV = 0.0f;
    float lastDigitalOut = 0.0f;
    int dbgBlocks = 0;
    GridInputFilter inputFilter;
    ChampTriodeStage v1a;
    CouplingHp couple1;
    ChampTriodeStage v1b;
    CouplingHp couple2;
    ChampPowerStage power;
};
} // namespace champ
