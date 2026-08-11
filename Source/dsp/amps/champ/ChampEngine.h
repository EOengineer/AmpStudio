#pragma once

#include "../../cabs/SpeakerImpedance.h"
#include "ChampDsp.h"
#include <JuceHeader.h>
#include <cmath>

namespace champ
{
/**
 * JUCE façade over ChampDsp (base sample rate).
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
    static constexpr float kMillerC = ChampDsp::kMillerC;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        dsp.prepare ((float) std::max (spec.sampleRate, 1.0));
        dbgBlocks = 0;
    }

    void reset() { dsp.reset(); }

    void setVolume (float volume01) noexcept { dsp.setVolume (volume01); }

    /** Stock = 22k speaker → V1B cathode; Off = resistor lifted. */
    void setNfbEnabled (bool on) noexcept { dsp.setNfbEnabled (on); }

    void setComponentSet (const ComponentSet& c) { dsp.setComponentSet (c); }

    ComponentSet& getComponentSet() noexcept { return dsp.getComponentSet(); }
    const ComponentSet& getComponentSet() const noexcept { return dsp.getComponentSet(); }

    /** Cab Z(f) when present; unloaded / high-Z falls back to flat 8 Ω. */
    void setLoadContext (const ElectricalPort& load) noexcept
    {
        dsp.setSpeakerRlc (cab::resolveLoadRlc (load));
    }

    float getInputZohms() const noexcept { return dsp.getInputZohms(); }
    float getOutputZohms() const noexcept { return dsp.getOutputZohms(); }
    int getLatencySamples() const noexcept { return 0; }

    ChampDsp& getDsp() noexcept { return dsp; }
    const ChampDsp& getDsp() const noexcept { return dsp; }

    ChampTriodeStage& getV1a() noexcept { return dsp.getV1a(); }
    ChampTriodeStage& getV1b() noexcept { return dsp.getV1b(); }
    ChampPowerStage& getPower() noexcept { return dsp.getPower(); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0 || numCh <= 0)
            return;

        auto* left = buffer.getWritePointer (0);

        float inPeak = 0.0f;
        float outPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            inPeak = std::max (inPeak, std::abs (left[i]));
            left[i] = dsp.processSample (left[i]);
            outPeak = std::max (outPeak, std::abs (left[i]));
        }

        for (int ch = 1; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

#if JUCE_DEBUG
        if (dbgBlocks < 8)
        {
            DBG ("Champ 5F1 block " << dbgBlocks
                 << " inPeak=" << inPeak
                 << " outPeak=" << outPeak
                 << " vol=" << volumeFraction (dsp.getVolume())
                 << " Vp1=" << dsp.getV1a().getPlate()
                 << " Vp2=" << dsp.getV1b().getPlate()
                 << " Vs=" << dsp.getLastSpeakerV());
            ++dbgBlocks;
        }
#endif
    }

private:
    ChampDsp dsp;
    int dbgBlocks = 0;
};
} // namespace champ
