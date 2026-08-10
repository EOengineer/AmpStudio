#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LibraryPanel.h"
#include "ui/ChainStrip.h"
#include "ui/LevelMeter.h"
#include "ui/PillToggle.h"

class AmpStudioAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            public juce::DragAndDropContainer,
                                            private juce::ChangeListener,
                                            private Chain::Listener,
                                            private juce::Timer
{
public:
    explicit AmpStudioAudioProcessorEditor (AmpStudioAudioProcessor&);
    ~AmpStudioAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void chainChanged() override;
    void timerCallback() override;
    void rebuildParamControls();
    void updateCalibrateStatus();
    void setStickyCalibrateStatus (const juce::String& text);

    AmpStudioAudioProcessor& audioProcessor;
    juce::String stickyCalibrateStatus;

    LibraryPanel libraryPanel;
    ChainStrip chainStrip;

    juce::Slider inputTrimSlider;
    juce::Label inputTrimLabel { {}, "Input Trim" };
    juce::TextButton calibrateButton { "Calibrate" };
    juce::Label calibrateStatusLabel;
    LevelMeter inputMeter;

    juce::Slider masterGainSlider;
    juce::Label masterGainLabel { {}, "Master" };
    LevelMeter outputMeter;

    juce::Label moduleParamsTitle { {}, "Selected Module" };
    juce::Component moduleParamsHost;
    juce::OwnedArray<juce::Slider> paramSliders;
    juce::OwnedArray<juce::ComboBox> paramCombos;
    juce::OwnedArray<PillToggle> paramToggles;
    juce::OwnedArray<juce::Label> paramLabels;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inputTrimAttachment;
    std::unique_ptr<SliderAttachment> masterGainAttachment;

    struct ParamBinding
    {
        juce::String paramId;
        juce::Slider* slider = nullptr;
        juce::ComboBox* combo = nullptr;
        PillToggle* toggle = nullptr;
    };

    juce::Array<ParamBinding> bindings;
    juce::TextButton loadCaptureButton { "Load Capture…" };
    juce::TextButton loadIrButton { "Load IR…" };

    void syncParamsFromBlock();
    void applyBindingToBlock (int bindingIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpStudioAudioProcessorEditor)
};
