#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/LevelReference.h"
#include "util/ModuleFactory.h"

AmpStudioAudioProcessor::AmpStudioAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    inputTrimParam = apvts.getRawParameterValue (ParamIDs::inputTrimDb);
    masterGainParam = apvts.getRawParameterValue (ParamIDs::masterGainDb);
}

AmpStudioAudioProcessor::~AmpStudioAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout AmpStudioAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::inputTrimDb, 1 },
        "Input Trim",
        juce::NormalisableRange<float> (LevelReference::kInputTrimMinDb,
                                        LevelReference::kInputTrimMaxDb,
                                        0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::masterGainDb, 1 },
        "Master Gain",
        juce::NormalisableRange<float> (LevelReference::kMasterGainMinDb,
                                        LevelReference::kMasterGainMaxDb,
                                        0.1f),
        LevelReference::kMasterGainDefaultDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

const juce::String AmpStudioAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AmpStudioAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AmpStudioAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AmpStudioAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AmpStudioAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AmpStudioAudioProcessor::getNumPrograms() { return 1; }
int AmpStudioAudioProcessor::getCurrentProgram() { return 0; }
void AmpStudioAudioProcessor::setCurrentProgram (int) {}
const juce::String AmpStudioAudioProcessor::getProgramName (int) { return {}; }
void AmpStudioAudioProcessor::changeProgramName (int, const juce::String&) {}

void AmpStudioAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();
    chain.prepare (spec);
    inputCalibrator.prepare (sampleRate);
}

void AmpStudioAudioProcessor::releaseResources()
{
    chain.reset();
    inputCalibrator.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AmpStudioAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void AmpStudioAudioProcessor::measureBufferLevels (const juce::AudioBuffer<float>& buffer,
                                                   float& peakOut,
                                                   float& rmsOut) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
    {
        peakOut = 0.0f;
        rmsOut = 0.0f;
        return;
    }

    float peak = 0.0f;
    double sumSquares = 0.0;
    int gatedSamples = 0;
    const float gateLinear = juce::Decibels::decibelsToGain (LevelReference::kGateFloorDb);
    const float gateThreshSq = gateLinear * gateLinear;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = std::abs (data[i]);
            peak = juce::jmax (peak, s);
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += buffer.getSample (ch, i);
        mono /= (float) numChannels;

        const float s2 = mono * mono;
        if (s2 >= gateThreshSq)
        {
            sumSquares += (double) s2;
            ++gatedSamples;
        }
    }

    peakOut = peak;
    rmsOut = gatedSamples > 0 ? (float) std::sqrt (sumSquares / (double) gatedSamples) : 0.0f;
}

void AmpStudioAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const float trimDb = inputTrimParam != nullptr ? inputTrimParam->load() : 0.0f;
    buffer.applyGain (juce::Decibels::decibelsToGain (trimDb));

    float inPeak = 0.0f, inRms = 0.0f;
    measureBufferLevels (buffer, inPeak, inRms);
    inputPeakDb.store (juce::Decibels::gainToDecibels (inPeak, -100.0f), std::memory_order_relaxed);
    inputRmsDb.store (juce::Decibels::gainToDecibels (inRms, -100.0f), std::memory_order_relaxed);

    inputCalibrator.process (buffer, trimDb);

    chain.process (buffer);

    if (masterGainParam != nullptr)
        buffer.applyGain (juce::Decibels::decibelsToGain (masterGainParam->load()));

    float outPeak = 0.0f, outRms = 0.0f;
    measureBufferLevels (buffer, outPeak, outRms);
    juce::ignoreUnused (outRms);
    outputPeakDb.store (juce::Decibels::gainToDecibels (outPeak, -100.0f), std::memory_order_relaxed);
}

void AmpStudioAudioProcessor::startInputCalibration()
{
    inputCalibrator.start();
}

AmpStudioAudioProcessor::CalibrationApplyResult AmpStudioAudioProcessor::applyPendingCalibrationTrim()
{
    if (! inputCalibrator.consumeResultReady())
        return CalibrationApplyResult::none;

    const auto state = inputCalibrator.getState();
    auto result = CalibrationApplyResult::none;

    if (state == InputCalibrator::State::finishedOk)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::inputTrimDb)))
        {
            const float trimDb = inputCalibrator.getPendingTrimDb();
            const float norm = param->convertTo0to1 (trimDb);
            param->beginChangeGesture();
            param->setValueNotifyingHost (norm);
            param->endChangeGesture();
        }

        result = CalibrationApplyResult::applied;
    }
    else if (state == InputCalibrator::State::finishedTooQuiet)
    {
        result = CalibrationApplyResult::tooQuiet;
    }

    inputCalibrator.acknowledgeFinished();
    sendChangeMessage();
    return result;
}

bool AmpStudioAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AmpStudioAudioProcessor::createEditor()
{
    return new AmpStudioAudioProcessorEditor (*this);
}

void AmpStudioAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.appendChild (chain.toValueTree(), nullptr);
    state.setProperty ("selectedSlot", selectedSlot, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AmpStudioAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        selectedSlot = (int) state.getProperty ("selectedSlot", 0);

        auto chainTree = state.getChildWithName ("Chain");
        if (chainTree.isValid())
        {
            chain.fromValueTree (chainTree, [] (const juce::String& typeId)
                                 {
                                     return ModuleFactory::create (typeId);
                                 });
            state.removeChild (chainTree, nullptr);
        }

        state.removeProperty ("selectedSlot", nullptr);
        apvts.replaceState (state);
        sendChangeMessage();
    }
}

void AmpStudioAudioProcessor::setSelectedSlot (int index)
{
    if (! juce::isPositiveAndBelow (index, Chain::numSlots))
        return;

    selectedSlot = index;
    sendChangeMessage();
}

void AmpStudioAudioProcessor::loadModuleIntoSlot (int slotIndex, const juce::String& typeId)
{
    if (! juce::isPositiveAndBelow (slotIndex, Chain::numSlots))
        return;

    chain.setBlock (slotIndex, ModuleFactory::create (typeId));
    selectedSlot = slotIndex;
    sendChangeMessage();
}

void AmpStudioAudioProcessor::loadModuleIntoSelectedSlot (const juce::String& typeId)
{
    loadModuleIntoSlot (selectedSlot, typeId);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmpStudioAudioProcessor();
}
