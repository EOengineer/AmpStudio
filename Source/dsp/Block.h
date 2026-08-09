#pragma once

#include <JuceHeader.h>

enum class ModuleCategory
{
    bypass,
    fx,
    amp,
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
};

using BlockPtr = std::unique_ptr<Block>;
