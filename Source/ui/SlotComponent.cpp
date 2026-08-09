#include "SlotComponent.h"
#include "../PluginProcessor.h"
#include "../util/ModuleFactory.h"

SlotComponent::SlotComponent (AmpStudioAudioProcessor& processorToUse, int slotIndex)
    : processor (processorToUse), index (slotIndex)
{
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    leftButton.onClick = [this] { nudgeLeft(); };
    rightButton.onClick = [this] { nudgeRight(); };
    clearButton.onClick = [this] { clearSlot(); };

    addAndMakeVisible (leftButton);
    addAndMakeVisible (rightButton);
    addAndMakeVisible (clearButton);

    refreshFromChain();
}

void SlotComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    const auto fill = selected ? juce::Colour (0xff2f4f4f)
                               : juce::Colour (0xff1e1e1e);
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (dragOver ? juce::Colours::orange
                          : (selected ? juce::Colours::lightblue : juce::Colours::grey));
    g.drawRoundedRectangle (bounds, 6.0f, selected ? 2.0f : 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (11.0f);
    g.drawText ("#" + juce::String (index + 1),
                getLocalBounds().removeFromTop (18),
                juce::Justification::centred);
}

void SlotComponent::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (16);

    auto buttons = area.removeFromBottom (24);
    leftButton.setBounds (buttons.removeFromLeft (28));
    rightButton.setBounds (buttons.removeFromLeft (28));
    clearButton.setBounds (buttons.removeFromRight (28));

    nameLabel.setBounds (area);
}

void SlotComponent::mouseDown (const juce::MouseEvent&)
{
    processor.setSelectedSlot (index);
}

void SlotComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 6)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        juce::var dragDesc;
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("type", "chainSlot");
        obj->setProperty ("index", index);
        dragDesc = obj;

        container->startDragging (dragDesc, this);
    }
}

bool SlotComponent::isInterestedInDragSource (const SourceDetails& dragSourceDetails)
{
    if (auto* obj = dragSourceDetails.description.getDynamicObject())
        return obj->getProperty ("type").toString() == "chainSlot"
            || obj->getProperty ("type").toString() == "libraryModule";

    return false;
}

void SlotComponent::itemDropped (const SourceDetails& dragSourceDetails)
{
    dragOver = false;

    auto* obj = dragSourceDetails.description.getDynamicObject();
    if (obj == nullptr)
        return;

    const auto type = obj->getProperty ("type").toString();

    if (type == "chainSlot")
    {
        const int from = (int) obj->getProperty ("index");
        processor.getChain().moveBlock (from, index);
        processor.setSelectedSlot (index);
    }
    else if (type == "libraryModule")
    {
        const auto typeId = obj->getProperty ("typeId").toString();
        processor.loadModuleIntoSlot (index, typeId);
        processor.setSelectedSlot (index);
    }

    repaint();
}

void SlotComponent::itemDragEnter (const SourceDetails&)
{
    dragOver = true;
    repaint();
}

void SlotComponent::itemDragExit (const SourceDetails&)
{
    dragOver = false;
    repaint();
}

void SlotComponent::refreshFromChain()
{
    selected = (processor.getSelectedSlot() == index);

    if (auto* block = processor.getChain().getBlock (index))
    {
        nameLabel.setText (block->getDisplayName(), juce::dontSendNotification);
        const bool isEmpty = block->getTypeId() == ModuleIds::bypass;
        clearButton.setEnabled (! isEmpty);
    }
    else
    {
        nameLabel.setText ("Empty", juce::dontSendNotification);
        clearButton.setEnabled (false);
    }

    leftButton.setEnabled (index > 0);
    rightButton.setEnabled (index < Chain::numSlots - 1);
    repaint();
}

void SlotComponent::nudgeLeft()
{
    if (index <= 0)
        return;

    processor.getChain().moveBlock (index, index - 1);
    processor.setSelectedSlot (index - 1);
}

void SlotComponent::nudgeRight()
{
    if (index >= Chain::numSlots - 1)
        return;

    processor.getChain().moveBlock (index, index + 1);
    processor.setSelectedSlot (index + 1);
}

void SlotComponent::clearSlot()
{
    processor.getChain().clearBlock (index);
}
