#pragma once

#include <JuceHeader.h>
#include "LevelReference.h"

/** Gated-RMS listen window that solves for input trim to hit the reference. */
class InputCalibrator
{
public:
    enum class State
    {
        idle,
        listening,
        finishedOk,
        finishedTooQuiet
    };

    void prepare (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        state.store (State::idle, std::memory_order_relaxed);
        sumSquares = 0.0;
        gatedSamples = 0;
        samplesRemaining = 0;
        pendingTrimDb.store (0.0f, std::memory_order_relaxed);
        resultReady.store (false, std::memory_order_relaxed);
    }

    /** Arm a ~4 s capture. Call from the message thread. */
    void start() noexcept
    {
        sumSquares = 0.0;
        gatedSamples = 0;
        samplesRemaining = (int) std::llround (sampleRateHz * (double) LevelReference::kCalibrateSeconds);
        resultReady.store (false, std::memory_order_relaxed);
        state.store (State::listening, std::memory_order_release);
    }

    State getState() const noexcept
    {
        return state.load (std::memory_order_acquire);
    }

    bool isListening() const noexcept
    {
        return getState() == State::listening;
    }

    /** True once after a listen window completes (ok or too quiet). */
    bool consumeResultReady() noexcept
    {
        return resultReady.exchange (false, std::memory_order_acq_rel);
    }

    float getPendingTrimDb() const noexcept
    {
        return pendingTrimDb.load (std::memory_order_acquire);
    }

    /**
     * Accumulate post-trim audio while listening.
     * @param currentTrimDb trim already applied to the buffer
     */
    void process (const juce::AudioBuffer<float>& buffer, float currentTrimDb) noexcept
    {
        if (state.load (std::memory_order_acquire) != State::listening)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0)
            return;

        const float gateLinear = juce::Decibels::decibelsToGain (LevelReference::kGateFloorDb);
        const float gateThreshSq = gateLinear * gateLinear;

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

        samplesRemaining -= numSamples;
        if (samplesRemaining > 0)
            return;

        constexpr int kMinGatedSamples = 2048;
        if (gatedSamples < kMinGatedSamples || sumSquares <= 0.0)
        {
            state.store (State::finishedTooQuiet, std::memory_order_release);
            resultReady.store (true, std::memory_order_release);
            return;
        }

        const float measuredRms = (float) std::sqrt (sumSquares / (double) gatedSamples);
        const float measuredRmsDb = juce::Decibels::gainToDecibels (measuredRms, -100.0f);
        const float deltaDb = LevelReference::kReferenceRmsDb - measuredRmsDb;
        const float newTrim = juce::jlimit (LevelReference::kInputTrimMinDb,
                                            LevelReference::kInputTrimMaxDb,
                                            currentTrimDb + deltaDb);

        pendingTrimDb.store (newTrim, std::memory_order_release);
        state.store (State::finishedOk, std::memory_order_release);
        resultReady.store (true, std::memory_order_release);
    }

    void acknowledgeFinished() noexcept
    {
        const auto s = state.load (std::memory_order_acquire);
        if (s == State::finishedOk || s == State::finishedTooQuiet)
            state.store (State::idle, std::memory_order_release);
    }

private:
    double sampleRateHz = 44100.0;
    double sumSquares = 0.0;
    int gatedSamples = 0;
    int samplesRemaining = 0;

    std::atomic<State> state { State::idle };
    std::atomic<float> pendingTrimDb { 0.0f };
    std::atomic<bool> resultReady { false };
};
