#pragma once

#include "../Block.h"
#include "../../util/ParamIDs.h"
#include "SpeakerImpedance.h"
#include <atomic>

/**
 * Cab IR block — acoustic IR convolution + publishes speaker Z(f) to upstream amp.
 *
 * Audio: amp speaker voltage → juce::dsp::Convolution (max 2048 taps,
 * trim + energy-normalise, zero extra latency). Dry passthrough until an IR is loaded.
 * Electrical: getInputLoad() = SpeakerImpedance for the selected preset.
 */
class CabIR final : public Block
{
public:
    static constexpr int kMaxIrSamples = 2048;

    CabIR()
    {
        state.setProperty ("typeId", ModuleIds::cabIR, nullptr);
        state.setProperty ("irPath", juce::String(), nullptr);
        setParam (ParamIDs::CabIR::impedancePreset, 3.0f); // mesa4x12V30
        speakerZ.setPreset (cab::ImpedancePreset::mesa4x12V30);
    }

    juce::String getTypeId() const override { return ModuleIds::cabIR; }
    juce::String getDisplayName() const override { return "Cab IR"; }
    ModuleCategory getCategory() const override { return ModuleCategory::cab; }

    ElectricalPort getInputLoad() const override
    {
        return ElectricalPort::withResponse (speakerZ.nominalOhms(), &speakerZ);
    }

    ElectricalPort getOutputPort() const override
    {
        return ElectricalPort::bufferedSource();
    }

    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        applyPresetFromParam();
        convolution.prepare (spec);
        maybeReloadIrFromState();
        // Second prepare flushes a queued IR so the first process() can use it.
        if (irReady.load (std::memory_order_acquire))
            convolution.prepare (spec);
        prepared = true;
    }

    void reset() override
    {
        convolution.reset();
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        if (! prepared)
            return;

        applyPresetFromParam();

        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numSamples <= 0 || numCh <= 0)
            return;

        if (! irReady.load (std::memory_order_acquire))
        {
            // No IR requested — dry passthrough.
            return;
        }

        juce::dsp::AudioBlock<float> block (buffer);
        auto mono = block.getSingleChannelBlock (0);
        juce::dsp::ProcessContextReplacing<float> context (mono);
        convolution.process (context);

        auto* left = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            left[i] = 1.5f * std::tanh (left[i] / 1.5f);

        for (int ch = 1; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }

    /** UI-thread IR load. JUCE resamples / trims / energy-normalises on a background queue. */
    bool loadImpulseResponse (const juce::File& file)
    {
        state.setProperty ("irPath", file.getFullPathName(), nullptr);

        if (! file.existsAsFile())
        {
            irReady.store (false, std::memory_order_release);
            DBG ("CabIR: IR file missing: " + file.getFullPathName());
            return false;
        }

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
        {
            irReady.store (false, std::memory_order_release);
            DBG ("CabIR: failed to read IR: " + file.getFullPathName());
            return false;
        }

        convolution.loadImpulseResponse (file,
                                         juce::dsp::Convolution::Stereo::no,
                                         juce::dsp::Convolution::Trim::yes,
                                         (size_t) kMaxIrSamples,
                                         juce::dsp::Convolution::Normalise::yes);

        irReady.store (true, std::memory_order_release);
        DBG ("CabIR: queued IR " + file.getFullPathName()
             + " (" + juce::String ((int) reader->lengthInSamples) + " samples, max "
             + juce::String (kMaxIrSamples) + ")");
        return true;
    }

    juce::String getIrPath() const
    {
        return state.getProperty ("irPath").toString();
    }

    cab::SpeakerImpedance& getSpeakerImpedance() noexcept { return speakerZ; }
    const cab::SpeakerImpedance& getSpeakerImpedance() const noexcept { return speakerZ; }

private:
    void applyPresetFromParam()
    {
        const int idx = juce::jlimit (0, 3, (int) std::lround (getParam (ParamIDs::CabIR::impedancePreset, 3.0f)));
        const auto preset = static_cast<cab::ImpedancePreset> (idx);
        if (preset != speakerZ.getPreset())
            speakerZ.setPreset (preset);
    }

    void maybeReloadIrFromState()
    {
        const auto path = getIrPath();
        if (path.isNotEmpty())
            loadImpulseResponse (juce::File (path));
    }

    cab::SpeakerImpedance speakerZ;
    juce::dsp::Convolution convolution;
    std::atomic<bool> irReady { false };
    bool prepared = false;
};
