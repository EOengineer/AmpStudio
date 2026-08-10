#pragma once

#include "Block.h"
#include "../util/ParamIDs.h"

/** Empty slot — electrically transparent; Chain skips it when resolving neighbors. */
class BypassBlock final : public Block
{
public:
    BypassBlock()
    {
        state.setProperty ("typeId", ModuleIds::bypass, nullptr);
    }

    juce::String getTypeId() const override { return ModuleIds::bypass; }
    juce::String getDisplayName() const override { return "Empty"; }
    ModuleCategory getCategory() const override { return ModuleCategory::bypass; }

    void prepare (const juce::dsp::ProcessSpec&) override {}
    void reset() override {}
    void process (juce::AudioBuffer<float>&) override {}
};
