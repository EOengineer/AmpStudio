#pragma once

#include <JuceHeader.h>

namespace circuit
{
/**
 * Shared oversampler for nonlinear islands (TS clipper, future tube stages).
 * Default 4×, high-quality FIR; latency is documented via getLatencySamples().
 */
class Oversampler
{
public:
    static constexpr size_t kDefaultFactor = 4;

    Oversampler() = default;

    void prepare (const juce::dsp::ProcessSpec& baseSpec, size_t factor = kDefaultFactor)
    {
        oversampleFactor = std::max<size_t> (factor, 1);
        baseSampleRate = baseSpec.sampleRate;
        maxBlock = (int) baseSpec.maximumBlockSize;

        if (oversampleFactor <= 1)
        {
            oversampling.reset();
            return;
        }

        // stages = log2(factor); factor must be power of two for juce::dsp::Oversampling
        size_t stages = 0;
        size_t f = oversampleFactor;
        while (f > 1)
        {
            f >>= 1;
            ++stages;
        }

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) baseSpec.numChannels,
            stages,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,
            false);

        oversampling->initProcessing ((size_t) maxBlock);
        oversampling->reset();
    }

    void reset()
    {
        if (oversampling != nullptr)
            oversampling->reset();
    }

    float getBaseSampleRate() const noexcept { return (float) baseSampleRate; }
    float getOversampledSampleRate() const noexcept
    {
        return (float) baseSampleRate * (float) oversampleFactor;
    }

    size_t getFactor() const noexcept { return oversampleFactor; }

    int getLatencySamples() const noexcept
    {
        if (oversampling == nullptr)
            return 0;
        return (int) oversampling->getLatencyInSamples();
    }

    /** Upsample a mono (or multi-channel) block; returns oversampled AudioBlock view. */
    juce::dsp::AudioBlock<float> processSamplesUp (const juce::dsp::AudioBlock<float>& input)
    {
        if (oversampling == nullptr || oversampleFactor <= 1)
            return input;
        return oversampling->processSamplesUp (input);
    }

    void processSamplesDown (juce::dsp::AudioBlock<float>& output)
    {
        if (oversampling == nullptr || oversampleFactor <= 1)
            return;
        oversampling->processSamplesDown (output);
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    size_t oversampleFactor = kDefaultFactor;
    double baseSampleRate = 48000.0;
    int maxBlock = 512;
};
} // namespace circuit
