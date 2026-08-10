#include "LibraryPanel.h"
#include "../PluginProcessor.h"

LibraryPanel::ModuleListModel::ModuleListModel (AmpStudioAudioProcessor& p, ModuleCategory categoryToShow)
    : processor (p), category (categoryToShow)
{
    modules = ModuleFactory::getModulesForCategory (category);
}

int LibraryPanel::ModuleListModel::getNumRows()
{
    return modules.size();
}

void LibraryPanel::ModuleListModel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                                      int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, modules.size()))
        return;

    if (rowIsSelected)
        g.fillAll (juce::Colours::darkgrey);

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText (modules.getReference (rowNumber).displayName,
                8, 0, width - 16, height,
                juce::Justification::centredLeft, true);
}

void LibraryPanel::ModuleListModel::listBoxItemClicked (int, const juce::MouseEvent&)
{
}

void LibraryPanel::ModuleListModel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (! juce::isPositiveAndBelow (row, modules.size()))
        return;

    processor.loadModuleIntoSelectedSlot (modules.getReference (row).typeId);
}

juce::var LibraryPanel::ModuleListModel::getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe)
{
    if (rowsToDescribe.size() == 0)
        return {};

    const int row = rowsToDescribe[0];
    if (! juce::isPositiveAndBelow (row, modules.size()))
        return {};

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("type", "libraryModule");
    obj->setProperty ("typeId", modules.getReference (row).typeId);
    return obj;
}

LibraryPanel::LibraryPanel (AmpStudioAudioProcessor& processorToUse)
    : processor (processorToUse),
      fxModel (processorToUse, ModuleCategory::fx),
      ampModel (processorToUse, ModuleCategory::amp),
      cabModel (processorToUse, ModuleCategory::cab),
      captureModel (processorToUse, ModuleCategory::capture)
{
    title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (title);

    fxList.setModel (&fxModel);
    ampList.setModel (&ampModel);
    cabList.setModel (&cabModel);
    captureList.setModel (&captureModel);

    fxList.setRowHeight (28);
    ampList.setRowHeight (28);
    cabList.setRowHeight (28);
    captureList.setRowHeight (28);

    tabs.addTab ("FX", juce::Colours::darkslategrey, &fxList, false);
    tabs.addTab ("Amps", juce::Colours::darkslateblue, &ampList, false);
    tabs.addTab ("Cabs", juce::Colours::darkcyan, &cabList, false);
    tabs.addTab ("Captures", juce::Colours::darkolivegreen, &captureList, false);
    addAndMakeVisible (tabs);

    loadButton.onClick = [this] { loadSelectedFromCurrentTab(); };
    addAndMakeVisible (loadButton);
}

void LibraryPanel::resized()
{
    auto area = getLocalBounds();
    title.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);
    loadButton.setBounds (area.removeFromBottom (32));
    area.removeFromBottom (6);
    tabs.setBounds (area);
}

void LibraryPanel::loadSelectedFromCurrentTab()
{
    auto* list = dynamic_cast<juce::ListBox*> (tabs.getCurrentContentComponent());
    if (list == nullptr)
        return;

    const int row = list->getSelectedRow();
    auto* model = dynamic_cast<ModuleListModel*> (list->getListBoxModel());
    if (model == nullptr || row < 0)
        return;

    ModuleCategory category = ModuleCategory::fx;
    switch (tabs.getCurrentTabIndex())
    {
        case 0: category = ModuleCategory::fx; break;
        case 1: category = ModuleCategory::amp; break;
        case 2: category = ModuleCategory::cab; break;
        case 3: category = ModuleCategory::capture; break;
        default: break;
    }

    const auto modules = ModuleFactory::getModulesForCategory (category);
    if (juce::isPositiveAndBelow (row, modules.size()))
        processor.loadModuleIntoSelectedSlot (modules.getReference (row).typeId);
}
