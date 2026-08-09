#pragma once

#include <JuceHeader.h>
#include "dsp/Chain.h"
#include "util/ParamIDs.h"

class AmpStudioAudioProcessorEditor;

class AmpStudioAudioProcessor final : public juce::AudioProcessor,
                                      private juce::ChangeBroadcaster
{
public:
    AmpStudioAudioProcessor();
    ~AmpStudioAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    Chain& getChain() noexcept { return chain; }

    int getSelectedSlot() const noexcept { return selectedSlot; }
    void setSelectedSlot (int index);

    void loadModuleIntoSlot (int slotIndex, const juce::String& typeId);
    void loadModuleIntoSelectedSlot (const juce::String& typeId);

    void addChangeListener (juce::ChangeListener* listener) { ChangeBroadcaster::addChangeListener (listener); }
    void removeChangeListener (juce::ChangeListener* listener) { ChangeBroadcaster::removeChangeListener (listener); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    Chain chain;
    int selectedSlot = 0;
    std::atomic<float>* masterGainParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpStudioAudioProcessor)
};
