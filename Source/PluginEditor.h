#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LibraryPanel.h"
#include "ui/ChainStrip.h"

class AmpStudioAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            public juce::DragAndDropContainer,
                                            private juce::ChangeListener,
                                            private Chain::Listener
{
public:
    explicit AmpStudioAudioProcessorEditor (AmpStudioAudioProcessor&);
    ~AmpStudioAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void chainChanged() override;
    void rebuildParamControls();

    AmpStudioAudioProcessor& audioProcessor;

    LibraryPanel libraryPanel;
    ChainStrip chainStrip;
    juce::Slider masterGainSlider;
    juce::Label masterGainLabel { {}, "Master" };
    juce::Label moduleParamsTitle { {}, "Selected Module" };
    juce::Component moduleParamsHost;
    juce::OwnedArray<juce::Slider> paramSliders;
    juce::OwnedArray<juce::Label> paramLabels;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> masterGainAttachment;

    struct ParamBinding
    {
        juce::String paramId;
        juce::Slider* slider = nullptr;
    };

    juce::Array<ParamBinding> bindings;
    juce::TextButton loadCaptureButton { "Load Capture…" };

    void syncSlidersFromBlock();
    void applySliderToBlock (int bindingIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpStudioAudioProcessorEditor)
};
