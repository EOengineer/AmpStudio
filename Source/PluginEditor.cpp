#include "PluginEditor.h"
#include "util/ModuleFactory.h"
#include "util/ParamIDs.h"
#include "dsp/cabs/CabIR.h"
#include "dsp/LevelReference.h"
#include "dsp/captures/NeuralCapture.h"
#include <cmath>

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

    loadIrButton.onClick = [this]
    {
        auto* block = audioProcessor.getChain().getBlock (audioProcessor.getSelectedSlot());
        auto* cab = dynamic_cast<CabIR*> (block);
        if (cab == nullptr)
            return;

        auto chooser = std::make_shared<juce::FileChooser> (
            "Select a cab impulse response (max 2048 samples used)",
            juce::File{},
            "*.wav;*.aif;*.aiff;*.flac");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [chooser, cab] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (file != juce::File{})
                                      cab->loadImpulseResponse (file);
                              });
    };
    addAndMakeVisible (loadIrButton);
    loadIrButton.setVisible (false);

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
    loadIrButton.setBounds (loadCaptureButton.getBounds());
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
    paramCombos.clear();
    paramToggles.clear();
    paramLabels.clear();
    bindings.clear();
    moduleParamsHost.removeAllChildren();

    auto* block = audioProcessor.getChain().getBlock (audioProcessor.getSelectedSlot());
    if (block == nullptr)
    {
        loadCaptureButton.setVisible (false);
        loadIrButton.setVisible (false);
        return;
    }

    loadCaptureButton.setVisible (block->getTypeId() == ModuleIds::neuralCapture);
    loadIrButton.setVisible (block->getTypeId() == ModuleIds::cabIR);

    enum class ControlKind { continuous, combo, pill };

    struct Spec
    {
        const char* id;
        const char* label;
        ControlKind kind = ControlKind::continuous;
    };

    std::vector<Spec> specs;

    if (block->getTypeId() == ModuleIds::tubeScreamer)
    {
        specs.push_back ({ ParamIDs::TubeScreamer::drive, "Drive" });
        specs.push_back ({ ParamIDs::TubeScreamer::tone,  "Tone" });
        specs.push_back ({ ParamIDs::TubeScreamer::level, "Level" });
        specs.push_back ({ ParamIDs::TubeScreamer::outputVariant, "808/9", ControlKind::pill });
        specs.push_back ({ ParamIDs::TubeScreamer::bassCap, "Bass", ControlKind::pill });
        specs.push_back ({ ParamIDs::TubeScreamer::diodeMode, "Diodes", ControlKind::combo });
    }
    else if (block->getTypeId() == ModuleIds::champ5F1)
    {
        specs.push_back ({ ParamIDs::Champ5F1::volume, "Volume" });
    }
    else if (block->getTypeId() == ModuleIds::cabIR)
    {
        specs.push_back ({ ParamIDs::CabIR::impedancePreset, "Z Curve", ControlKind::combo });
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
        label->setJustificationType (juce::Justification::centred);

        auto col = bounds.removeFromLeft (width);
        label->setBounds (col.removeFromTop (20));
        moduleParamsHost.addAndMakeVisible (label);

        const juce::String paramId (specs[i].id);
        const int bindingIndex = bindings.size();

        if (specs[i].kind == ControlKind::pill)
        {
            auto* toggle = paramToggles.add (new PillToggle());

            if (paramId == ParamIDs::TubeScreamer::outputVariant)
                toggle->setOptions ("808", "9");
            else if (paramId == ParamIDs::TubeScreamer::bassCap)
                toggle->setOptions ("Stock", "More");

            const int selected = juce::jlimit (0, 1,
                                               (int) std::lround (block->getParam (specs[i].id, 0.0f)));
            toggle->setSelectedIndex (selected, juce::dontSendNotification);

            bindings.add ({ specs[i].id, nullptr, nullptr, toggle });
            toggle->onChange = [this, bindingIndex]
            {
                applyBindingToBlock (bindingIndex);
            };

            auto toggleBounds = col.withSizeKeepingCentre (juce::jmin (col.getWidth() - 8, 110), 28);
            toggle->setBounds (toggleBounds);
            moduleParamsHost.addAndMakeVisible (toggle);
        }
        else if (specs[i].kind == ControlKind::combo)
        {
            auto* combo = paramCombos.add (new juce::ComboBox());
            combo->setJustificationType (juce::Justification::centred);

            if (paramId == ParamIDs::TubeScreamer::diodeMode)
            {
                combo->addItem ("Si/Si", 1);
                combo->addItem ("Asym Si", 2);
                combo->addItem ("Ge/Si", 3);
                combo->addItem ("LED", 4);
            }
            else if (paramId == ParamIDs::CabIR::impedancePreset)
            {
                combo->addItem ("Flat 8Ω", 1);
                combo->addItem ("Fender Dlx 1x12", 2);
                combo->addItem ("Marshall 4x12 GB", 3);
                combo->addItem ("Mesa 4x12 V30", 4);
            }

            const int selected = juce::jlimit (0, combo->getNumItems() - 1,
                                               (int) std::lround (block->getParam (specs[i].id, 0.0f)));
            combo->setSelectedItemIndex (selected, juce::dontSendNotification);

            bindings.add ({ specs[i].id, nullptr, combo, nullptr });
            combo->onChange = [this, bindingIndex]
            {
                applyBindingToBlock (bindingIndex);
            };

            combo->setBounds (col.reduced (4, 28));
            moduleParamsHost.addAndMakeVisible (combo);
        }
        else
        {
            auto* slider = paramSliders.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                                                               juce::Slider::TextBoxBelow));
            slider->setRange (0.0, 1.0, 0.01);
            slider->setValue (block->getParam (specs[i].id, 0.5f),
                              juce::dontSendNotification);

            bindings.add ({ specs[i].id, slider, nullptr, nullptr });
            slider->onValueChange = [this, bindingIndex]
            {
                applyBindingToBlock (bindingIndex);
            };

            slider->setBounds (col.reduced (8));
            moduleParamsHost.addAndMakeVisible (slider);
        }
    }

    moduleParamsTitle.setText ("Selected: " + block->getDisplayName(),
                               juce::dontSendNotification);
}

void AmpStudioAudioProcessorEditor::syncParamsFromBlock()
{
    auto* block = audioProcessor.getChain().getBlock (audioProcessor.getSelectedSlot());
    if (block == nullptr)
        return;

    for (auto& binding : bindings)
    {
        if (binding.slider != nullptr)
        {
            binding.slider->setValue (block->getParam (binding.paramId, 0.5f),
                                      juce::dontSendNotification);
        }
        else if (binding.combo != nullptr)
        {
            const int selected = juce::jlimit (0, binding.combo->getNumItems() - 1,
                                               (int) std::lround (block->getParam (binding.paramId, 0.0f)));
            binding.combo->setSelectedItemIndex (selected, juce::dontSendNotification);
        }
        else if (binding.toggle != nullptr)
        {
            const int selected = juce::jlimit (0, 1,
                                               (int) std::lround (block->getParam (binding.paramId, 0.0f)));
            binding.toggle->setSelectedIndex (selected, juce::dontSendNotification);
        }
    }
}

void AmpStudioAudioProcessorEditor::applyBindingToBlock (int bindingIndex)
{
    if (! juce::isPositiveAndBelow (bindingIndex, bindings.size()))
        return;

    auto* block = audioProcessor.getChain().getBlock (audioProcessor.getSelectedSlot());
    if (block == nullptr)
        return;

    const auto& binding = bindings.getReference (bindingIndex);
    if (binding.slider != nullptr)
        block->setParam (binding.paramId, (float) binding.slider->getValue());
    else if (binding.combo != nullptr)
        block->setParam (binding.paramId, (float) binding.combo->getSelectedItemIndex());
    else if (binding.toggle != nullptr)
        block->setParam (binding.paramId, (float) binding.toggle->getSelectedIndex());
}
