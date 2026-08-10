#pragma once

#include <JuceHeader.h>
#include "../util/ModuleFactory.h"

class AmpStudioAudioProcessor;

class LibraryPanel final : public juce::Component
{
public:
    explicit LibraryPanel (AmpStudioAudioProcessor& processorToUse);

    void resized() override;

private:
    class ModuleListModel final : public juce::ListBoxModel
    {
    public:
        ModuleListModel (AmpStudioAudioProcessor& p, ModuleCategory categoryToShow);

        int getNumRows() override;
        void paintListBoxItem (int rowNumber, juce::Graphics& g,
                               int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        juce::var getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe) override;

    private:
        AmpStudioAudioProcessor& processor;
        ModuleCategory category;
        juce::Array<ModuleInfo> modules;
    };

    AmpStudioAudioProcessor& processor;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::ListBox fxList, ampList, cabList, captureList;
    ModuleListModel fxModel, ampModel, cabModel, captureModel;
    juce::Label title { {}, "Library" };
    juce::TextButton loadButton { "Load into selected slot" };

    void loadSelectedFromCurrentTab();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryPanel)
};
