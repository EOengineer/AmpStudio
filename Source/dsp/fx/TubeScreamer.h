#pragma once

#include "../Block.h"
#include "../../util/ParamIDs.h"
#include "ts/TsEngine.h"
#include "ts/TsVerifyBootstrap.h"
#include <cmath>

/**
 * White-box Tube Screamer — component-parameterized engine with oversampled
 * MNA clipping island. Drive / Tone / Level plus mod params (808/9, bass, diodes).
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
        setParam (ParamIDs::TubeScreamer::outputVariant, 0.0f); // 808
        setParam (ParamIDs::TubeScreamer::bassCap, 0.0f);       // stock
        setParam (ParamIDs::TubeScreamer::diodeMode, 0.0f);     // Si/Si
    }

    juce::String getTypeId() const override { return ModuleIds::tubeScreamer; }
    juce::String getDisplayName() const override { return "Tube Screamer"; }
    ModuleCategory getCategory() const override { return ModuleCategory::fx; }

    ElectricalPort getOutputPort() const override
    {
        return { engine.getOutputZohms(), false, nullptr };
    }

    ElectricalPort getInputLoad() const override
    {
        return { engine.getInputZohms(), false, nullptr };
    }

    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        ts::logVerificationOnce();
        applyModsFromParams();
        engine.prepare (spec);
        prepared = true;
    }

    void reset() override { engine.reset(); }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        if (! prepared)
            return;

        applyModsFromParams();
        engine.setParams (getParam (ParamIDs::TubeScreamer::drive, 0.5f),
                          getParam (ParamIDs::TubeScreamer::tone, 0.5f),
                          getParam (ParamIDs::TubeScreamer::level, 0.7f));
        engine.setLoadOhms (loadContext.resistiveOhms);
        engine.process (buffer);
    }

    ts::TsEngine& getEngine() noexcept { return engine; }
    const ts::TsEngine& getEngine() const noexcept { return engine; }

private:
    void applyModsFromParams()
    {
        auto comps = engine.getComponentSet();
        const int outVar = juce::jlimit (0, 1, (int) std::lround (getParam (ParamIDs::TubeScreamer::outputVariant, 0.0f)));
        const int bass = juce::jlimit (0, 1, (int) std::lround (getParam (ParamIDs::TubeScreamer::bassCap, 0.0f)));
        const int diode = juce::jlimit (0, 3, (int) std::lround (getParam (ParamIDs::TubeScreamer::diodeMode, 0.0f)));

        const auto newOut = (outVar == 0) ? ts::OutputVariant::ts808 : ts::OutputVariant::ts9;
        const auto newBass = (bass == 0) ? ts::BassCap::stock : ts::BassCap::moreBass;
        const auto newDiode = static_cast<ts::DiodeMode> (diode);

        const bool changed = newOut != comps.outputVariant
                          || newBass != comps.bassCap
                          || newDiode != comps.diodeMode;

        if (changed)
        {
            comps.applyOutputVariant (newOut);
            comps.applyBassCap (newBass);
            comps.diodeMode = newDiode;
            engine.setComponentSet (comps);
        }
    }

    ts::TsEngine engine;
    bool prepared = false;
};
