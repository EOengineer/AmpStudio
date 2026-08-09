#pragma once

#include <JuceHeader.h>
#include "../dsp/Chain.h"

class AmpStudioAudioProcessor;

class SlotComponent final : public juce::Component,
                            public juce::DragAndDropTarget
{
public:
    SlotComponent (AmpStudioAudioProcessor& processorToUse, int slotIndex);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;

    void refreshFromChain();

private:
    void nudgeLeft();
    void nudgeRight();
    void clearSlot();

    AmpStudioAudioProcessor& processor;
    int index = 0;
    bool selected = false;
    bool dragOver = false;

    juce::TextButton leftButton { "<" };
    juce::TextButton rightButton { ">" };
    juce::TextButton clearButton { "X" };
    juce::Label nameLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotComponent)
};
