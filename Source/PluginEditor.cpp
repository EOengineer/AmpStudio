#include "PluginEditor.h"
#include "util/ModuleFactory.h"
#include "util/ParamIDs.h"
#include "dsp/LevelReference.h"
#include "dsp/captures/NeuralCapture.h"

namespace
{
    void styleDbRotary (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        slider.setNumDecimalPlacesToDisplay (1);
        slider.textFromValueFunction = [] (double v)
        {
            return juce::String (v, 1) + " dB";
        };
        slider.valueFromTextFunction = [] (const juce::String& t)
        {
            return t.upToFirstOccurrenceOf ("dB", false, false).trim().getDoubleValue();
        };
    }
}

AmpStudioAudioProcessorEditor::AmpStudioAudioProcessorEditor (AmpStudioAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      libraryPanel (p),
      chainStrip (p)
{
    addAndMakeVisible (libraryPanel);
    addAndMakeVisible (chainStrip);

    inputTrimLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (inputTrimLabel);
    styleDbRotary (inputTrimSlider);
    addAndMakeVisible (inputTrimSlider);
    inputTrimAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), ParamIDs::inputTrimDb, inputTrimSlider);

    calibrateButton.onClick = [this]
    {
        stickyCalibrateStatus.clear();
        audioProcessor.startInputCalibration();
        updateCalibrateStatus();
    };
    addAndMakeVisible (calibrateButton);

    calibrateStatusLabel.setJustificationType (juce::Justification::centred);
    calibrateStatusLabel.setFont (juce::FontOptions (12.0f));
    calibrateStatusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (calibrateStatusLabel);

    inputMeter.setShowReferenceTick (true);
    addAndMakeVisible (inputMeter);

    masterGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (masterGainLabel);
    styleDbRotary (masterGainSlider);
    addAndMakeVisible (masterGainSlider);
    masterGainAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), ParamIDs::masterGainDb, masterGainSlider);

    outputMeter.setShowReferenceTick (false);
    addAndMakeVisible (outputMeter);

    moduleParamsTitle.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (moduleParamsTitle);
    addAndMakeVisible (moduleParamsHost);

    loadCaptureButton.onClick = [this]
    {
        auto* block = audioProcessor.getChain().getBlock (audioProcessor.getSelectedSlot());
        auto* capture = dynamic_cast<NeuralCapture*> (block);
        if (capture == nullptr)
            return;

        auto chooser = std::make_shared<juce::FileChooser> (
            "Select a capture file (placeholder)",
            juce::File{},
            "*");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [chooser, capture] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (file != juce::File{})
                                      capture->loadCapture (file);
                              });
    };
    addAndMakeVisible (loadCaptureButton);
    loadCaptureButton.setVisible (false);

    audioProcessor.addChangeListener (this);
    audioProcessor.getChain().addListener (this);

    setSize (1040, 560);
    rebuildParamControls();
    updateCalibrateStatus();
    startTimerHz (30);
}

AmpStudioAudioProcessorEditor::~AmpStudioAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.removeChangeListener (this);
    audioProcessor.getChain().removeListener (this);
}

void AmpStudioAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121212));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("AmpStudio", getLocalBounds().removeFromTop (40).reduced (16, 0),
                juce::Justification::centredLeft, true);
}

void AmpStudioAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (36);

    auto top = area.removeFromTop (area.getHeight() * 2 / 3);
    libraryPanel.setBounds (top.removeFromLeft (280));
    top.removeFromLeft (12);

    auto right = top;
    auto levelsArea = right.removeFromRight (220);
    right.removeFromRight (8);

    auto inputCol = levelsArea.removeFromLeft (110);
    auto outputCol = levelsArea;

    inputTrimLabel.setBounds (inputCol.removeFromTop (18));
    inputTrimSlider.setBounds (inputCol.removeFromTop (90).reduced (8, 0));
    calibrateButton.setBounds (inputCol.removeFromTop (28).reduced (4, 2));
    calibrateStatusLabel.setBounds (inputCol.removeFromTop (32).reduced (2, 0));
    inputMeter.setBounds (inputCol.reduced (28, 4));

    masterGainLabel.setBounds (outputCol.removeFromTop (18));
    masterGainSlider.setBounds (outputCol.removeFromTop (90).reduced (8, 0));
    outputCol.removeFromTop (28);
    outputCol.removeFromTop (32);
    outputMeter.setBounds (outputCol.reduced (28, 4));

    moduleParamsTitle.setBounds (right.removeFromTop (24));
    right.removeFromTop (4);
    loadCaptureButton.setBounds (right.removeFromBottom (28));
    right.removeFromBottom (4);
    moduleParamsHost.setBounds (right);

    area.removeFromTop (12);
    chainStrip.setBounds (area);
}

void AmpStudioAudioProcessorEditor::timerCallback()
{
    using ApplyResult = AmpStudioAudioProcessor::CalibrationApplyResult;
    switch (audioProcessor.applyPendingCalibrationTrim())
    {
        case ApplyResult::applied:
            setStickyCalibrateStatus ("Calibrated to "
                                      + juce::String (LevelReference::kReferenceRmsDb, 0)
                                      + " dBFS");
            break;
        case ApplyResult::tooQuiet:
            setStickyCalibrateStatus ("Too quiet — try again");
            break;
        case ApplyResult::none:
            break;
    }

    inputMeter.setLevels (audioProcessor.getInputPeakDb(), audioProcessor.getInputRmsDb());
    outputMeter.setLevels (audioProcessor.getOutputPeakDb(), -100.0f);
    inputMeter.updateBallistics (1.0f / 30.0f);
    outputMeter.updateBallistics (1.0f / 30.0f);

    updateCalibrateStatus();
}

void AmpStudioAudioProcessorEditor::setStickyCalibrateStatus (const juce::String& text)
{
    stickyCalibrateStatus = text;
}

void AmpStudioAudioProcessorEditor::updateCalibrateStatus()
{
    using State = InputCalibrator::State;
    const auto state = audioProcessor.getInputCalibrator().getState();

    if (state == State::listening)
    {
        calibrateStatusLabel.setText ("Play hard for ~4 s…", juce::dontSendNotification);
        calibrateButton.setEnabled (false);
        return;
    }

    calibrateButton.setEnabled (true);

    if (stickyCalibrateStatus.isNotEmpty())
    {
        calibrateStatusLabel.setText (stickyCalibrateStatus, juce::dontSendNotification);
        return;
    }

    calibrateStatusLabel.setText ("Target "
                                      + juce::String (LevelReference::kReferenceRmsDb, 0)
                                      + " dBFS RMS",
                                  juce::dontSendNotification);
}

void AmpStudioAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    chainStrip.refresh();
    rebuildParamControls();
}

void AmpStudioAudioProcessorEditor::chainChanged()
{
    chainStrip.refresh();
    rebuildParamControls();
}

void AmpStudioAudioProcessorEditor::rebuildParamControls()
{
    paramSliders.clear();
    paramLabels.clear();
    bindings.clear();
    moduleParamsHost.removeAllChildren();

    auto* block = audioProcessor.getChain().getBlock (audioProcessor.getSelectedSlot());
    if (block == nullptr)
    {
        loadCaptureButton.setVisible (false);
        return;
    }

    loadCaptureButton.setVisible (block->getTypeId() == ModuleIds::neuralCapture);

    struct Spec { const char* id; const char* label; };
    std::vector<Spec> specs;

    if (block->getTypeId() == ModuleIds::tubeScreamer)
    {
        specs.push_back ({ ParamIDs::TubeScreamer::drive, "Drive" });
        specs.push_back ({ ParamIDs::TubeScreamer::tone,  "Tone" });
        specs.push_back ({ ParamIDs::TubeScreamer::level, "Level" });
    }
    else if (block->getTypeId() == ModuleIds::champ5F1)
    {
        specs.push_back ({ ParamIDs::Champ5F1::volume, "Volume" });
    }
    else if (block->getTypeId() == ModuleIds::neuralCapture)
    {
        specs.push_back ({ ParamIDs::NeuralCapture::inputGain,  "Input" });
        specs.push_back ({ ParamIDs::NeuralCapture::outputGain, "Output" });
    }

    auto bounds = moduleParamsHost.getLocalBounds();
    const int width = juce::jmax (80, specs.empty() ? 80 : bounds.getWidth() / (int) specs.size());

    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto* label = paramLabels.add (new juce::Label ({}, specs[i].label));
        auto* slider = paramSliders.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                                                           juce::Slider::TextBoxBelow));
        label->setJustificationType (juce::Justification::centred);
        slider->setRange (0.0, 1.0, 0.01);
        slider->setValue (block->getParam (specs[i].id, 0.5f),
                          juce::dontSendNotification);

        const int bindingIndex = bindings.size();
        bindings.add ({ specs[i].id, slider });

        slider->onValueChange = [this, bindingIndex]
        {
            applySliderToBlock (bindingIndex);
        };

        auto col = bounds.removeFromLeft (width);
        label->setBounds (col.removeFromTop (20));
        slider->setBounds (col.reduced (8));

        moduleParamsHost.addAndMakeVisible (label);
        moduleParamsHost.addAndMakeVisible (slider);
    }

    moduleParamsTitle.setText ("Selected: " + block->getDisplayName(),
                               juce::dontSendNotification);
}

void AmpStudioAudioProcessorEditor::syncSlidersFromBlock()
{
    auto* block = audioProcessor.getChain().getBlock (audioProcessor.getSelectedSlot());
    if (block == nullptr)
        return;

    for (auto& binding : bindings)
        if (binding.slider != nullptr)
            binding.slider->setValue (block->getParam (binding.paramId, 0.5f),
                                      juce::dontSendNotification);
}

void AmpStudioAudioProcessorEditor::applySliderToBlock (int bindingIndex)
{
    if (! juce::isPositiveAndBelow (bindingIndex, bindings.size()))
        return;

    auto* block = audioProcessor.getChain().getBlock (audioProcessor.getSelectedSlot());
    if (block == nullptr)
        return;

    const auto& binding = bindings.getReference (bindingIndex);
    if (binding.slider != nullptr)
        block->setParam (binding.paramId, (float) binding.slider->getValue());
}
