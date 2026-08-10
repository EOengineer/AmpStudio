#pragma once

#include "../Block.h"
#include "../../util/ParamIDs.h"

/**
 * White-box Fender Champ 5F1 stub — identity audio; Volume param only.
 *
 * Electrical contexts (set by Chain before process):
 * - driveContext: upstream source Z (e.g. Tube Screamer low-Z vs guitar-ish).
 *   Future input network / first-stage HF can use this; boost feel is still
 *   mostly the upstream block's audio content.
 * - loadContext: downstream load (future cab ModuleCategory::cab). Prefer
 *   frequency-dependent speaker Z via ElectricalPort::response when available;
 *   power amp / OT / NFB should query loadContext.evaluate(f) while computing.
 *
 * Amp↔cab contract: a cab module publishes getInputLoad() with speaker Z(f);
 * this amp consumes it through loadContext. No co-process path yet — port
 * exchange is enough until a nonlinear simultaneous solve is required.
 */
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

    /** OT / speaker feed until a separate cab slot exists — resistive stand-in. */
    ElectricalPort getOutputPort() const override
    {
        return ElectricalPort::speakerResistive();
    }

    ElectricalPort getInputLoad() const override { return ElectricalPort::highZInput(); }

    void prepare (const juce::dsp::ProcessSpec&) override {}
    void reset() override {}

    void process (juce::AudioBuffer<float>&) override
    {
        // DSP intentionally deferred — learn white-box amp modeling here later.
        // When implementing: use getDriveContext() at the input network and
        // getLoadContext() (Z(f) when isFrequencyDependent) in the power section.
        juce::ignoreUnused (driveContext, loadContext);
    }
};
