#pragma once

#include "../Block.h"
#include "../../util/ParamIDs.h"

/** White-box Tube Screamer stub — identity audio; Drive/Tone/Level params only. */
class TubeScreamer final : public Block
{
public:
    TubeScreamer()
    {
        state.setProperty ("typeId", ModuleIds::tubeScreamer, nullptr);
        setParam (ParamIDs::TubeScreamer::drive, 0.5f);
        setParam (ParamIDs::TubeScreamer::tone,  0.5f);
        setParam (ParamIDs::TubeScreamer::level, 0.7f);
    }

    juce::String getTypeId() const override { return ModuleIds::tubeScreamer; }
    juce::String getDisplayName() const override { return "Tube Screamer"; }
    ModuleCategory getCategory() const override { return ModuleCategory::fx; }

    void prepare (const juce::dsp::ProcessSpec&) override {}
    void reset() override {}

    void process (juce::AudioBuffer<float>&) override
    {
        // DSP intentionally deferred — learn white-box modeling here later.
    }
};
