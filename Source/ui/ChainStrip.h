#pragma once

#include <JuceHeader.h>
#include "SlotComponent.h"
#include "../dsp/Chain.h"

class AmpStudioAudioProcessor;

class ChainStrip final : public juce::Component,
                         private Chain::Listener
{
public:
    explicit ChainStrip (AmpStudioAudioProcessor& processorToUse);
    ~ChainStrip() override;

    void resized() override;
    void refresh();

private:
    void chainChanged() override;

    AmpStudioAudioProcessor& processor;
    juce::OwnedArray<SlotComponent> slots;
    juce::Label title { {}, "Signal Chain" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainStrip)
};
