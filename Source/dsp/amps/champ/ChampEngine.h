#pragma once

#include "../../cabs/SpeakerImpedance.h"
#include "../../circuit/Oversampler.h"
#include "ChampComponents.h"
#include "ChampPowerStage.h"
#include "ChampTriodeStage.h"
#include <JuceHeader.h>
#include <cmath>

namespace champ
{
/**
 * Champ 5F1 signal path:
 *   input LPF → V1A → coupling → volume → V1B (+NFB) → coupling → 6V6+OT → speaker
 *
 * Nonlinear islands run at 4×. loadContext → resolveLoadRlc for OT secondary
 * (flat 8 Ω when unloaded; cab Z(f) when Cab IR follows).
 */
class ChampEngine
{
public:
    static constexpr size_t kOversampleFactor = circuit::Oversampler::kDefaultFactor;
    /** Approximate Miller C at V1A grid for stopper LPF. */
    static constexpr float kMillerC = 100.0e-12f;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        baseSpec = spec;
        oversampler.prepare ({ spec.sampleRate, spec.maximumBlockSize, 1 }, kOversampleFactor);
        const float osRate = oversampler.getOversampledSampleRate();
        rebuildStages (osRate);
        reset();
    }

    void reset()
    {
        oversampler.reset();
        inputFilter.reset();
        v1a.reset();
        couple1.reset();
        v1b.reset();
        couple2.reset();
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
            rebuildStages (oversampler.getOversampledSampleRate());
    }

    ComponentSet& getComponentSet() noexcept { return components; }
    const ComponentSet& getComponentSet() const noexcept { return components; }

    /** Stamp OT secondary from chain load (cab Z(f) or flat 8 Ω). */
    void setLoadContext (const ElectricalPort& load) noexcept
    {
        power.setSpeakerRlc (cab::resolveLoadRlc (load));
    }

    float getInputZohms() const noexcept { return components.gridLeakR; }
    float getOutputZohms() const noexcept
    {
        return power.getSpeakerRlc().nominalOhms;
    }

    int getLatencySamples() const noexcept { return oversampler.getLatencySamples(); }

    ChampTriodeStage& getV1a() noexcept { return v1a; }
    ChampTriodeStage& getV1b() noexcept { return v1b; }
    ChampPowerStage& getPower() noexcept { return power; }
    circuit::Oversampler& getOversampler() noexcept { return oversampler; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0 || numCh <= 0)
            return;

        auto* left = buffer.getWritePointer (0);

        juce::dsp::AudioBlock<float> block (buffer);
        auto monoBlock = block.getSingleChannelBlock (0);
        auto osBlock = oversampler.processSamplesUp (monoBlock);
        const int osNum = (int) osBlock.getNumSamples();
        auto* os = osBlock.getChannelPointer (0);

        const float vol = volumeFraction (volume);

        for (int i = 0; i < osNum; ++i)
        {
            float g = digitalToVolts (os[i]);
            g = inputFilter.process (g);

            const float p1 = v1a.processSample (g, 0.0f);
            float ac1 = couple1.processFromPlate (p1);
            ac1 *= vol;

            // NFB from previous speaker sample (1-sample @ OS rate — fine at 4×)
            const float p2 = v1b.processSample (ac1, lastSpeakerV);
            float ac2 = couple2.processFromPlate (p2);

            lastSpeakerV = power.processSample (ac2);
            os[i] = speakerVoltsToDigital (lastSpeakerV);
        }

        oversampler.processSamplesDown (monoBlock);

        for (int ch = 1; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }

private:
    void rebuildStages (float osRate)
    {
        inputFilter.prepare (components.gridStopperR, kMillerC, osRate);
        v1a.prepare (osRate, components.v1aPlateR, components.v1aCathodeR,
                     components.v1aBypassC, components.bplusPreamp, 0.0f);
        couple1.prepare (components.couplingC1, components.volumePotR, osRate);
        v1b.prepare (osRate, components.v1bPlateR, components.v1bCathodeR,
                     1.0e-12f, // essentially unbypassed (tiny C)
                     components.bplusPreamp, components.nfbR);
        couple2.prepare (components.couplingC2, components.powerGridLeakR, osRate);
        power.prepare (osRate, components);
    }

    juce::dsp::ProcessSpec baseSpec {};
    ComponentSet components;
    float volume = 0.5f;
    float lastSpeakerV = 0.0f;
    circuit::Oversampler oversampler;
    GridInputFilter inputFilter;
    ChampTriodeStage v1a;
    CouplingHp couple1;
    ChampTriodeStage v1b;
    CouplingHp couple2;
    ChampPowerStage power;
};
} // namespace champ
