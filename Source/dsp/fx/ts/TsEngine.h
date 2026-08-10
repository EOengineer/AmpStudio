#pragma once

#include "../../circuit/Oversampler.h"
#include "TsClippingAmp.h"
#include "TsToneLevelStage.h"
#include "TsComponents.h"
#include <JuceHeader.h>

namespace ts
{
/**
 * Full Tube Screamer signal path:
 *   input (unity) → upsample → clipper → post-clip LPF → downsample
 *   → tone/level → output buffer (loadContext)
 */
class TsEngine
{
public:
    static constexpr size_t kOversampleFactor = circuit::Oversampler::kDefaultFactor;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        baseSpec = spec;
        oversampler.prepare ({ spec.sampleRate, spec.maximumBlockSize, 1 }, kOversampleFactor);

        components.applyOutputVariant (components.outputVariant);
        components.applyBassCap (components.bassCap);

        const float osRate = oversampler.getOversampledSampleRate();
        clipper.prepare (osRate, components);
        clipper.setDiodeMode (components.diodeMode);
        postLpOs.prepare (components.postLpR, components.postLpC, osRate);
        toneLevel.prepare ((float) spec.sampleRate, components);
        output.prepare (components);
        reset();
    }

    void reset()
    {
        oversampler.reset();
        clipper.reset();
        postLpOs.reset();
        toneLevel.reset();
    }

    void setParams (float drive01, float tone01, float level01) noexcept
    {
        clipper.setDrive (drive01);
        toneLevel.setTone (tone01);
        toneLevel.setLevel (level01);
    }

    void setComponentSet (const ComponentSet& c)
    {
        components = c;
        components.applyOutputVariant (c.outputVariant);
        components.applyBassCap (c.bassCap);

        if (baseSpec.sampleRate > 0.0)
        {
            const float osRate = oversampler.getOversampledSampleRate();
            clipper.prepare (osRate, components);
            clipper.setDiodeMode (components.diodeMode);
            postLpOs.prepare (components.postLpR, components.postLpC, osRate);
            toneLevel.prepare ((float) baseSpec.sampleRate, components);
            output.prepare (components);
        }
    }

    ComponentSet& getComponentSet() noexcept { return components; }
    const ComponentSet& getComponentSet() const noexcept { return components; }

    void setLoadOhms (float ohms) noexcept { output.setLoadOhms (ohms); }

    float getOutputZohms() const noexcept { return output.theveninZoutOhms(); }
    float getInputZohms() const noexcept { return Comp::kInputBiasR; }

    int getLatencySamples() const noexcept { return oversampler.getLatencySamples(); }

    TsClippingAmp& getClipper() noexcept { return clipper; }
    TsToneLevelStage& getToneLevel() noexcept { return toneLevel; }
    TsOutputBuffer& getOutput() noexcept { return output; }
    circuit::Oversampler& getOversampler() noexcept { return oversampler; }
    circuit::OnePoleLpRc& getPostLpOs() noexcept { return postLpOs; }

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

        for (int i = 0; i < osNum; ++i)
        {
            const float vin = digitalToVolts (os[i]);
            float v = clipper.processSample (vin);
            v = postLpOs.process (v);
            os[i] = voltsToDigital (v);
        }

        oversampler.processSamplesDown (monoBlock);

        for (int i = 0; i < numSamples; ++i)
        {
            float v = digitalToVolts (left[i]);
            v = toneLevel.processSample (v);
            v = output.processSample (v);
            left[i] = voltsToDigital (v);
        }

        for (int ch = 1; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }

private:
    juce::dsp::ProcessSpec baseSpec {};
    ComponentSet components;
    circuit::Oversampler oversampler;
    TsClippingAmp clipper;
    circuit::OnePoleLpRc postLpOs;
    TsToneLevelStage toneLevel;
    TsOutputBuffer output;
};
} // namespace ts
