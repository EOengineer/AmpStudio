#pragma once

#include "../Block.h"
#include "../../util/ParamIDs.h"
#include "SpeakerImpedance.h"
#include <array>
#include <vector>

/**
 * Cab IR block — acoustic IR convolution + publishes speaker Z(f) to upstream amp.
 *
 * Audio: amp speaker voltage → short FIR (max 2048 taps).
 * Electrical: getInputLoad() = SpeakerImpedance for the selected preset.
 */
class CabIR final : public Block
{
public:
    static constexpr int kMaxIrSamples = 2048;

    CabIR()
    {
        state.setProperty ("typeId", ModuleIds::cabIR, nullptr);
        state.setProperty ("irPath", juce::String(), nullptr);
        setParam (ParamIDs::CabIR::impedancePreset, 3.0f); // mesa4x12V30
        speakerZ.setPreset (cab::ImpedancePreset::mesa4x12V30);
    }

    juce::String getTypeId() const override { return ModuleIds::cabIR; }
    juce::String getDisplayName() const override { return "Cab IR"; }
    ModuleCategory getCategory() const override { return ModuleCategory::cab; }

    ElectricalPort getInputLoad() const override
    {
        return ElectricalPort::withResponse (speakerZ.nominalOhms(), &speakerZ);
    }

    ElectricalPort getOutputPort() const override
    {
        return ElectricalPort::bufferedSource();
    }

    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;
        delay.assign ((size_t) kMaxIrSamples, 0.0f);
        writePos = 0;
        applyPresetFromParam();
        maybeReloadIrFromState();
        prepared = true;
    }

    void reset() override
    {
        std::fill (delay.begin(), delay.end(), 0.0f);
        writePos = 0;
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        if (! prepared)
            return;

        applyPresetFromParam();

        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numSamples <= 0 || numCh <= 0)
            return;

        auto* left = buffer.getWritePointer (0);

        if (! irReady || ir.size() < 1)
        {
            // No IR loaded — dry passthrough (amp tone only).
            return;
        }

        const int irLen = (int) ir.size();

        for (int n = 0; n < numSamples; ++n)
        {
            delay[(size_t) writePos] = left[n];

            float y = 0.0f;
            int idx = writePos;
            for (int k = 0; k < irLen; ++k)
            {
                y += ir[(size_t) k] * delay[(size_t) idx];
                if (--idx < 0)
                    idx = kMaxIrSamples - 1;
            }

            left[n] = y;
            writePos = (writePos + 1) % kMaxIrSamples;
        }

        for (int ch = 1; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }

    /** Message-thread IR load. WAV/AIFF/etc via AudioFormatManager. */
    bool loadImpulseResponse (const juce::File& file)
    {
        state.setProperty ("irPath", file.getFullPathName(), nullptr);

        if (! file.existsAsFile())
        {
            irReady = false;
            ir.clear();
            DBG ("CabIR: IR file missing: " + file.getFullPathName());
            return false;
        }

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
        {
            irReady = false;
            ir.clear();
            DBG ("CabIR: failed to read IR: " + file.getFullPathName());
            return false;
        }

        const int srcLen = (int) reader->lengthInSamples;
        if (srcLen <= 0)
        {
            irReady = false;
            ir.clear();
            return false;
        }

        juce::AudioBuffer<float> raw (1, srcLen);
        reader->read (&raw, 0, srcLen, 0, true, false);

        std::vector<float> mono ((size_t) srcLen);
        for (int i = 0; i < srcLen; ++i)
            mono[(size_t) i] = raw.getSample (0, i);

        // Resample to host rate if needed (linear).
        const double fileRate = reader->sampleRate;
        std::vector<float> atHost;
        if (sampleRate > 0.0 && std::abs (fileRate - sampleRate) > 1.0)
        {
            const double ratio = sampleRate / fileRate;
            const int outLen = juce::jmax (1, (int) std::lround ((double) srcLen * ratio));
            atHost.resize ((size_t) outLen);
            for (int i = 0; i < outLen; ++i)
            {
                const double srcPos = (double) i / ratio;
                const int i0 = (int) srcPos;
                const int i1 = juce::jmin (i0 + 1, srcLen - 1);
                const float frac = (float) (srcPos - (double) i0);
                atHost[(size_t) i] = mono[(size_t) i0] * (1.0f - frac) + mono[(size_t) i1] * frac;
            }
        }
        else
        {
            atHost = std::move (mono);
        }

        if ((int) atHost.size() > kMaxIrSamples)
        {
            DBG ("CabIR: truncating IR from " + juce::String ((int) atHost.size())
                 + " to " + juce::String (kMaxIrSamples) + " samples");
            atHost.resize ((size_t) kMaxIrSamples);
        }

        ir = std::move (atHost);
        // Mild normalize so loud IRs don't explode.
        float peak = 0.0f;
        for (float s : ir)
            peak = juce::jmax (peak, std::abs (s));
        if (peak > 1.0e-6f && peak > 1.0f)
        {
            const float inv = 1.0f / peak;
            for (float& s : ir)
                s *= inv;
        }

        irReady = ! ir.empty();
        reset();
        return irReady;
    }

    juce::String getIrPath() const
    {
        return state.getProperty ("irPath").toString();
    }

    cab::SpeakerImpedance& getSpeakerImpedance() noexcept { return speakerZ; }
    const cab::SpeakerImpedance& getSpeakerImpedance() const noexcept { return speakerZ; }

private:
    void applyPresetFromParam()
    {
        const int idx = juce::jlimit (0, 3, (int) std::lround (getParam (ParamIDs::CabIR::impedancePreset, 3.0f)));
        const auto preset = static_cast<cab::ImpedancePreset> (idx);
        if (preset != speakerZ.getPreset())
            speakerZ.setPreset (preset);
    }

    void maybeReloadIrFromState()
    {
        const auto path = getIrPath();
        if (path.isNotEmpty())
            loadImpulseResponse (juce::File (path));
    }

    cab::SpeakerImpedance speakerZ;
    std::vector<float> ir;
    std::vector<float> delay;
    int writePos = 0;
    double sampleRate = 48000.0;
    bool irReady = false;
    bool prepared = false;
};
