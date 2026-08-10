#pragma once

#include <JuceHeader.h>
#include "dsp/Chain.h"
#include "dsp/InputCalibrator.h"
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
    InputCalibrator& getInputCalibrator() noexcept { return inputCalibrator; }

    int getSelectedSlot() const noexcept { return selectedSlot; }
    void setSelectedSlot (int index);

    void loadModuleIntoSlot (int slotIndex, const juce::String& typeId);
    void loadModuleIntoSelectedSlot (const juce::String& typeId);

    enum class CalibrationApplyResult
    {
        none,
        applied,
        tooQuiet
    };

    void startInputCalibration();
    CalibrationApplyResult applyPendingCalibrationTrim();

    float getInputPeakDb() const noexcept { return inputPeakDb.load (std::memory_order_relaxed); }
    float getInputRmsDb() const noexcept { return inputRmsDb.load (std::memory_order_relaxed); }
    float getOutputPeakDb() const noexcept { return outputPeakDb.load (std::memory_order_relaxed); }

    void addChangeListener (juce::ChangeListener* listener) { ChangeBroadcaster::addChangeListener (listener); }
    void removeChangeListener (juce::ChangeListener* listener) { ChangeBroadcaster::removeChangeListener (listener); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static void measureBufferLevels (const juce::AudioBuffer<float>& buffer,
                                     float& peakOut,
                                     float& rmsOut) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    Chain chain;
    InputCalibrator inputCalibrator;
    int selectedSlot = 0;
    std::atomic<float>* inputTrimParam = nullptr;
    std::atomic<float>* masterGainParam = nullptr;

    std::atomic<float> inputPeakDb { -100.0f };
    std::atomic<float> inputRmsDb { -100.0f };
    std::atomic<float> outputPeakDb { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpStudioAudioProcessor)
};
