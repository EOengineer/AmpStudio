#include "PluginProcessor.h"
#include "PluginEditor.h"
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
    masterGainParam = apvts.getRawParameterValue (ParamIDs::masterGain);
}

AmpStudioAudioProcessor::~AmpStudioAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout AmpStudioAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::masterGain, 1 },
        "Master Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        0.8f));
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
}

void AmpStudioAudioProcessor::releaseResources()
{
    chain.reset();
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

void AmpStudioAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    chain.process (buffer);

    if (masterGainParam != nullptr)
        buffer.applyGain (masterGainParam->load());
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
