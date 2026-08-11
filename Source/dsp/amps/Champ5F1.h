#pragma once

#include "../Block.h"
#include "../../util/ParamIDs.h"
#include "champ/ChampEngine.h"
#include "champ/ChampVerifyBootstrap.h"

/**
 * White-box Fender Champ 5F1 — Volume primary; NFB Stock/Off in Deep settings.
 *
 * Electrical contexts (set by Chain before process):
 * - driveContext: reserved for future input-network source Z.
 * - loadContext: OT secondary via cab::resolveLoadRlc (flat 8 Ω default;
 *   Cab IR publishes Z(f) when present). Acoustic IR stays in the cab slot.
 */
class Champ5F1 final : public Block
{
public:
    Champ5F1()
    {
        state.setProperty ("typeId", ModuleIds::champ5F1, nullptr);
        setParam (ParamIDs::Champ5F1::volume, 0.5f);
        setParam (ParamIDs::Champ5F1::nfb, 1.0f); // Stock 22k
    }

    juce::String getTypeId() const override { return ModuleIds::champ5F1; }
    juce::String getDisplayName() const override { return "Champ 5F1"; }
    ModuleCategory getCategory() const override { return ModuleCategory::amp; }

    ElectricalPort getOutputPort() const override
    {
        return ElectricalPort::speakerResistive (engine.getOutputZohms());
    }

    ElectricalPort getInputLoad() const override
    {
        return { engine.getInputZohms(), false, nullptr };
    }

    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        champ::logVerificationOnce();
        engine.prepare (spec);
        prepared = true;
    }

    void reset() override { engine.reset(); }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        if (! prepared)
            return;

        engine.setVolume (getParam (ParamIDs::Champ5F1::volume, 0.5f));
        engine.setNfbEnabled (getParam (ParamIDs::Champ5F1::nfb, 1.0f) >= 0.5f);
        engine.setLoadContext (loadContext);
        juce::ignoreUnused (driveContext);
        engine.process (buffer);
    }

    champ::ChampEngine& getEngine() noexcept { return engine; }
    const champ::ChampEngine& getEngine() const noexcept { return engine; }

private:
    champ::ChampEngine engine;
    bool prepared = false;
};
