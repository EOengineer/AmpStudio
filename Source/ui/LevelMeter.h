#pragma once

#include <JuceHeader.h>
#include "../dsp/LevelReference.h"

/** Simple vertical peak meter with an optional reference tick mark. */
class LevelMeter final : public juce::Component
{
public:
    LevelMeter() = default;

    void setShowReferenceTick (bool shouldShow) noexcept { showReferenceTick = shouldShow; }

    void setLevels (float peakDb, float rmsDb) noexcept
    {
        targetPeakDb = juce::jlimit (-60.0f, 6.0f, peakDb);
        targetRmsDb = juce::jlimit (-60.0f, 6.0f, rmsDb);
    }

    void updateBallistics (float deltaSeconds) noexcept
    {
        const float release = std::exp (-deltaSeconds / 0.30f);

        if (targetPeakDb > displayPeakDb)
            displayPeakDb = targetPeakDb;
        else
            displayPeakDb = targetPeakDb + (displayPeakDb - targetPeakDb) * release;

        if (targetRmsDb > displayRmsDb)
            displayRmsDb = targetRmsDb;
        else
            displayRmsDb = targetRmsDb + (displayRmsDb - targetRmsDb) * release;

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (0xff1e1e1e));
        g.fillRoundedRectangle (bounds, 3.0f);

        const auto meterArea = bounds.reduced (3.0f);
        const float peakNorm = dbToNorm (displayPeakDb);
        const float rmsNorm = dbToNorm (displayRmsDb);

        const float peakH = meterArea.getHeight() * peakNorm;
        g.setColour (displayPeakDb >= -0.5f ? juce::Colour (0xffe74c3c)
                                            : juce::Colour (0xff3dba6c));
        g.fillRect (juce::Rectangle<float> (meterArea.getX(),
                                            meterArea.getBottom() - peakH,
                                            meterArea.getWidth(),
                                            peakH));

        const float rmsH = meterArea.getHeight() * rmsNorm;
        g.setColour (juce::Colour (0x8839a0d4));
        g.fillRect (juce::Rectangle<float> (meterArea.getX(),
                                            meterArea.getBottom() - rmsH,
                                            meterArea.getWidth() * 0.45f,
                                            rmsH));

        if (showReferenceTick)
        {
            const float y = meterArea.getBottom()
                          - meterArea.getHeight() * dbToNorm (LevelReference::kReferenceRmsDb);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawLine (meterArea.getX(), y, meterArea.getRight(), y, 1.2f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
    }

private:
    static float dbToNorm (float db) noexcept
    {
        constexpr float minDb = -60.0f;
        constexpr float maxDb = 0.0f;
        return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    }

    bool showReferenceTick = false;
    float targetPeakDb = -60.0f;
    float targetRmsDb = -60.0f;
    float displayPeakDb = -60.0f;
    float displayRmsDb = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};
