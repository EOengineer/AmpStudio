#pragma once

#include <JuceHeader.h>
#include "ElectricalPort.h"

enum class ModuleCategory
{
    bypass,
    fx,
    amp,
    /** Future cab / IR modules — publish speaker load Z(f) for amp coupling. */
    cab,
    capture
};

/** Base interface for every chain module. Stubs and future models share this API. */
class Block
{
public:
    virtual ~Block() = default;

    virtual juce::String getTypeId() const = 0;
    virtual juce::String getDisplayName() const = 0;
    virtual ModuleCategory getCategory() const = 0;

    virtual void prepare (const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;

    /** Pass-through for stubs; real DSP fills this in later. */
    virtual void process (juce::AudioBuffer<float>& buffer) = 0;

    /**
     * Source port this block presents to the next active slot (Thevenin Zout).
     * Default: buffered low-Z (interface / modern FX isolation).
     */
    virtual ElectricalPort getOutputPort() const { return ElectricalPort::bufferedSource(); }

    /**
     * Load port this block presents to the previous active slot (Zin).
     * Default: high-Z input (~1 MΩ).
     * Cabs should override with speaker Z (resistive nominal and/or ImpedanceResponse).
     */
    virtual ElectricalPort getInputLoad() const { return ElectricalPort::highZInput(); }

    /** Upstream source from the previous active slot (or chain interface). */
    virtual void setDriveContext (const ElectricalPort& upstreamSource) { driveContext = upstreamSource; }

    /**
     * Downstream load from the next active slot (or unloaded at chain end).
     * Amps use this for power-section ↔ cab interaction when a cab follows.
     */
    virtual void setLoadContext (const ElectricalPort& downstreamLoad) { loadContext = downstreamLoad; }

    const ElectricalPort& getDriveContext() const noexcept { return driveContext; }
    const ElectricalPort& getLoadContext() const noexcept { return loadContext; }

    /** Empty slots are skipped when resolving electrical neighbors. */
    bool isElectricallyTransparent() const noexcept
    {
        return getCategory() == ModuleCategory::bypass;
    }

    /** Module-local parameter tree (survives reorder with the instance). */
    juce::ValueTree& getState() noexcept { return state; }
    const juce::ValueTree& getState() const noexcept { return state; }

    float getParam (const juce::Identifier& id, float defaultValue = 0.0f) const
    {
        return (float) state.getProperty (id, defaultValue);
    }

    void setParam (const juce::Identifier& id, float value)
    {
        state.setProperty (id, value, nullptr);
    }

protected:
    juce::ValueTree state { "BlockState" };

    /** Set by Chain before process(); models may read in process(). */
    ElectricalPort driveContext = ElectricalPort::interfaceSource();
    ElectricalPort loadContext = ElectricalPort::unloaded();
};

using BlockPtr = std::unique_ptr<Block>;
