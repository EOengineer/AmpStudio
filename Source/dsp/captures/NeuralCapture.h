#pragma once

#include "../Block.h"
#include "../../util/ParamIDs.h"

/**
 * Black-box / neural capture placeholder — identity audio.
 * Load API is a no-op until we pick a capture format together.
 *
 * Electrical: publishes buffered I/O ports and ignores drive/load contexts.
 * Captures bake whatever loading was present at training time; true Z
 * interaction cannot be retrofitted through the adjacency contract.
 */
class NeuralCapture final : public Block
{
public:
    NeuralCapture()
    {
        state.setProperty ("typeId", ModuleIds::neuralCapture, nullptr);
        state.setProperty ("capturePath", juce::String(), nullptr);
        setParam (ParamIDs::NeuralCapture::inputGain,  0.5f);
        setParam (ParamIDs::NeuralCapture::outputGain, 0.5f);
    }

    juce::String getTypeId() const override { return ModuleIds::neuralCapture; }
    juce::String getDisplayName() const override { return "Neural Capture"; }
    ModuleCategory getCategory() const override { return ModuleCategory::capture; }

    ElectricalPort getOutputPort() const override { return ElectricalPort::bufferedSource(); }
    ElectricalPort getInputLoad() const override  { return ElectricalPort::highZInput(); }

    void prepare (const juce::dsp::ProcessSpec&) override {}
    void reset() override {}

    void process (juce::AudioBuffer<float>&) override
    {
        // DSP intentionally deferred — black-box / neural path later.
    }

    /** Placeholder for future capture loading (message thread only). */
    bool loadCapture (const juce::File& file)
    {
        state.setProperty ("capturePath", file.getFullPathName(), nullptr);
        // No-op load until format is chosen.
        return file.existsAsFile();
    }

    juce::String getCapturePath() const
    {
        return state.getProperty ("capturePath").toString();
    }
};
