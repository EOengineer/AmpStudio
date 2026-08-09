#include "ChainStrip.h"
#include "../PluginProcessor.h"

ChainStrip::ChainStrip (AmpStudioAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (title);

    for (int i = 0; i < Chain::numSlots; ++i)
    {
        auto* slot = slots.add (new SlotComponent (processor, i));
        addAndMakeVisible (slot);
    }

    processor.getChain().addListener (this);
}

ChainStrip::~ChainStrip()
{
    processor.getChain().removeListener (this);
}

void ChainStrip::resized()
{
    auto area = getLocalBounds();
    title.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);

    const int gap = 8;
    const int slotWidth = slots.isEmpty() ? 0
        : (area.getWidth() - gap * (slots.size() - 1)) / slots.size();

    for (int i = 0; i < slots.size(); ++i)
    {
        slots[i]->setBounds (area.removeFromLeft (slotWidth));
        if (i + 1 < slots.size())
            area.removeFromLeft (gap);
    }
}

void ChainStrip::refresh()
{
    for (auto* slot : slots)
        slot->refreshFromChain();
}

void ChainStrip::chainChanged()
{
    refresh();
}
