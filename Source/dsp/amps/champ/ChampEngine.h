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
 * Stripped vs full 5F1: Hi jack, no 5Y3, resistive 8 Ω, NFB open
 * (re-enable once audio is confirmed).
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
    }

    void setVolume (float volume01) noexcept
    {
        volume = std::clamp (volume01, 0.0f, 1.0f);
    }

    void setComponentSet (const ComponentSet& c)
    {
        components = c;
        if (baseSpec.sampleRate > 0.0)
            rebuildStages ((float) baseSpec.sampleRate);
    }

    ComponentSet& getComponentSet() noexcept { return components; }
    const ComponentSet& getComponentSet() const noexcept { return components; }

    /** Resistive 8 Ω only until in-host audio is proven. */
    void setLoadContext (const ElectricalPort&) noexcept
    {
        power.setSpeakerRlc (cab::makePreset (cab::ImpedancePreset::flat8));
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

            const float p2 = v1b.processSample (ac1, 0.0f); // NFB open
            const float ac2 = couple2.processFromPlate (p2);

            lastSpeakerV = power.processSample (ac2);
            float out = speakerVoltsToDigital (lastSpeakerV);
            if (! std::isfinite (out))
                out = 0.0f;
            out = std::clamp (out, -2.0f, 2.0f);
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
        // NFB resistor omitted until the open-loop path is audible in-host.
        v1b.prepare (fs, components.v1bPlateR, components.v1bCathodeR,
                     1.0e-12f,
                     components.bplusPreamp, 0.0f);
        couple2.prepare (components.couplingC2, components.powerGridLeakR, fs);
        power.prepare (fs, components);
        power.setSpeakerRlc (cab::makePreset (cab::ImpedancePreset::flat8));
        couple1.seedFromPlate (v1a.getPlate());
        couple2.seedFromPlate (v1b.getPlate());
    }

    juce::dsp::ProcessSpec baseSpec {};
    ComponentSet components;
    float volume = 0.5f;
    float lastSpeakerV = 0.0f;
    int dbgBlocks = 0;
    GridInputFilter inputFilter;
    ChampTriodeStage v1a;
    CouplingHp couple1;
    ChampTriodeStage v1b;
    CouplingHp couple2;
    ChampPowerStage power;
};
} // namespace champ
