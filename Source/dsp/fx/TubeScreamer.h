#pragma once

#include "../Block.h"
#include "../../util/ParamIDs.h"

/**
 * White-box Tube Screamer stub — identity audio; Drive/Tone/Level params only.
 *
 * Electrical: low-Z buffered output (op-amp). When placed before an amp, the
 * dominant boost interaction is serial audio (level / mids / clip); the amp may
 * also read driveContext for input-network authenticity vs a high-Z guitar source.
 */
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

    ElectricalPort getOutputPort() const override { return ElectricalPort::bufferedSource(); }
    ElectricalPort getInputLoad() const override  { return ElectricalPort::highZInput(); }

    void prepare (const juce::dsp::ProcessSpec&) override {}
    void reset() override {}

    void process (juce::AudioBuffer<float>&) override
    {
        // DSP intentionally deferred — learn white-box modeling here later.
    }
};
