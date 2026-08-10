#pragma once

#include <JuceHeader.h>
#include <functional>

/** Two-option segmented control drawn as a pill with a sliding selection. */
class PillToggle final : public juce::Component
{
public:
    PillToggle() = default;

    void setOptions (juce::String leftLabel, juce::String rightLabel)
    {
        leftText = std::move (leftLabel);
        rightText = std::move (rightLabel);
        repaint();
    }

    /** 0 = left, 1 = right */
    void setSelectedIndex (int index, juce::NotificationType notification = juce::sendNotification)
    {
        const int clamped = juce::jlimit (0, 1, index);
        if (clamped == selected)
            return;

        selected = clamped;
        repaint();

        if (notification != juce::dontSendNotification && onChange)
            onChange();
    }

    int getSelectedIndex() const noexcept { return selected; }

    std::function<void()> onChange;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = bounds.getHeight() * 0.5f;

        g.setColour (juce::Colour (0xff2a2a2a));
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (juce::Colour (0xff555555));
        g.drawRoundedRectangle (bounds, radius, 1.0f);

        auto left = bounds.removeFromLeft (bounds.getWidth() * 0.5f);
        auto right = bounds;

        const auto selectedBounds = (selected == 0 ? left : right).reduced (2.0f);
        g.setColour (juce::Colour (0xff3d6b8a));
        g.fillRoundedRectangle (selectedBounds, selectedBounds.getHeight() * 0.5f);

        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.setColour (selected == 0 ? juce::Colours::white : juce::Colours::lightgrey);
        g.drawText (leftText, left.toNearestInt(), juce::Justification::centred, false);

        g.setColour (selected == 1 ? juce::Colours::white : juce::Colours::lightgrey);
        g.drawText (rightText, right.toNearestInt(), juce::Justification::centred, false);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()))
            return;

        setSelectedIndex (e.x < getWidth() / 2 ? 0 : 1);
    }

private:
    juce::String leftText { "A" };
    juce::String rightText { "B" };
    int selected = 0;
};
