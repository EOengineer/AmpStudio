#pragma once

#include "../Block.h"
#include "../../util/ParamIDs.h"

/** White-box Fender Champ 5F1 stub — identity audio; Volume param only. */
class Champ5F1 final : public Block
{
public:
    Champ5F1()
    {
        state.setProperty ("typeId", ModuleIds::champ5F1, nullptr);
        setParam (ParamIDs::Champ5F1::volume, 0.5f);
    }

    juce::String getTypeId() const override { return ModuleIds::champ5F1; }
    juce::String getDisplayName() const override { return "Champ 5F1"; }
    ModuleCategory getCategory() const override { return ModuleCategory::amp; }

    void prepare (const juce::dsp::ProcessSpec&) override {}
    void reset() override {}

    void process (juce::AudioBuffer<float>&) override
    {
        // DSP intentionally deferred — learn white-box amp modeling here later.
    }
};
