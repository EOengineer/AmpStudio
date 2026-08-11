#pragma once

#include "../../cabs/SpeakerImpedance.h"
#include "ChampDsp.h"
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace champ
{
/**
 * JUCE façade over ChampDsp (base sample rate).
 *
 * juce::dsp::Oversampling has muted this amp in-host while the identical
 * stages pass offline; 4× OS is deferred until this path is audible.
 *
 * Stripped vs full 5F1: Hi jack, no 5Y3. Unloaded OT is flat 8 Ω; a Cab IR
 * Z-curve is stamped via loadContext. NFB is Stock 22k by default
 * (Off = lifted resistor).
 */
class ChampEngine
{
public:
    static constexpr float kMillerC = ChampDsp::kMillerC;
    /** Matches Golden::kHashHfMax — Debug log always fires at or above this. */
    static constexpr float kDbgHashHfMax = 0.25f;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        const float fs = (float) std::max (spec.sampleRate, 1.0);
        dsp.prepare (fs);
        dbgBlocks = 0;
        dbgPrevOut = 0.0f;
#if JUCE_DEBUG
        capFs = fs;
        capMax = std::max (1, (int) std::lround (2.0 * (double) fs));
        capIn.assign ((size_t) capMax, 0.0f);
        capOut.assign ((size_t) capMax, 0.0f);
        capLen = 0;
        capArmed = false;
        capWritten = false;
#endif
    }

    void reset() { dsp.reset(); }

    void setVolume (float volume01) noexcept { dsp.setVolume (volume01); }

    /** Stock = 22k speaker → V1B cathode; Off = resistor lifted. */
    void setNfbEnabled (bool on) noexcept { dsp.setNfbEnabled (on); }

    void setComponentSet (const ComponentSet& c) { dsp.setComponentSet (c); }

    ComponentSet& getComponentSet() noexcept { return dsp.getComponentSet(); }
    const ComponentSet& getComponentSet() const noexcept { return dsp.getComponentSet(); }

    /** Cab Z(f) when present; unloaded / high-Z falls back to flat 8 Ω. */
    void setLoadContext (const ElectricalPort& load) noexcept
    {
        dsp.setSpeakerRlc (cab::resolveLoadRlc (load));
    }

    float getInputZohms() const noexcept { return dsp.getInputZohms(); }
    float getOutputZohms() const noexcept { return dsp.getOutputZohms(); }
    int getLatencySamples() const noexcept { return 0; }

    ChampDsp& getDsp() noexcept { return dsp; }
    const ChampDsp& getDsp() const noexcept { return dsp; }

    ChampTriodeStage& getV1a() noexcept { return dsp.getV1a(); }
    ChampTriodeStage& getV1b() noexcept { return dsp.getV1b(); }
    ChampPowerStage& getPower() noexcept { return dsp.getPower(); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0 || numCh <= 0)
            return;

        auto* left = buffer.getWritePointer (0);

        float inPeak = 0.0f;
        float outPeak = 0.0f;

#if JUCE_DEBUG
        const int holds0 = dsp.getPower().getHoldCount();
        double diffE = 0.0;
        double totE = 0.0;
        float maxDelta = 0.0f;
        float prev = dbgPrevOut;
#endif

        for (int i = 0; i < numSamples; ++i)
        {
            const float inS = left[i];
            inPeak = std::max (inPeak, std::abs (inS));
            const float outS = dsp.processSample (inS);
            left[i] = outS;
            outPeak = std::max (outPeak, std::abs (outS));

#if JUCE_DEBUG
            const float d = outS - prev;
            maxDelta = std::max (maxDelta, std::abs (d));
            diffE += (double) d * (double) d;
            totE += (double) outS * (double) outS;
            prev = outS;
            captureSample (inS, outS);
#endif
        }

        for (int ch = 1; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

#if JUCE_DEBUG
        dbgPrevOut = prev;
        const int holds = dsp.getPower().getHoldCount() - holds0;
        const float hfRatio = totE > 1.0e-20 ? (float) (diffE / totE) : 0.0f;
        const bool hashish = holds > 0 || hfRatio > kDbgHashHfMax;
        if (dbgBlocks < 32 || (dbgBlocks % 32) == 0 || hashish)
        {
            DBG ("Champ 5F1 block " << dbgBlocks
                 << " fs=" << capFs
                 << " inPeak=" << inPeak
                 << " outPeak=" << outPeak
                 << " vol=" << volumeFraction (dsp.getVolume())
                 << " nfb=" << (dsp.getNfbEnabled() ? "Stock" : "Off")
                 << " holds=" << holds
                 << " hf=" << hfRatio
                 << " dMax=" << maxDelta
                 << " Vp1=" << dsp.getV1a().getPlate()
                 << " Vp2=" << dsp.getV1b().getPlate()
                 << " Vs=" << dsp.getLastSpeakerV());
        }
        ++dbgBlocks;
        if (capLen >= capMax && ! capWritten)
            writeHostDumpWav();
#endif
    }

private:
#if JUCE_DEBUG
    void captureSample (float inS, float outS) noexcept
    {
        if (capWritten || capLen >= capMax || capMax <= 0)
            return;
        if (! capArmed)
        {
            if (std::abs (inS) <= 1.0e-3f)
                return;
            capArmed = true;
        }
        capIn[(size_t) capLen] = inS;
        capOut[(size_t) capLen] = outS;
        ++capLen;
    }

    void writeHostDumpWav()
    {
        capWritten = true;
        auto file = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                        .getChildFile ("champ_host_dump.wav");
        file.deleteFile();
        auto os = file.createOutputStream();
        if (os == nullptr)
        {
            DBG ("Champ dump: failed to create " << file.getFullPathName());
            return;
        }

        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            fmt.createWriterFor (os.release(), (double) capFs, 2, 16, {}, 0));
        if (writer == nullptr)
        {
            DBG ("Champ dump: writer failed");
            return;
        }

        juce::AudioBuffer<float> buf (2, capLen);
        buf.copyFrom (0, 0, capIn.data(), capLen);
        buf.copyFrom (1, 0, capOut.data(), capLen);
        // Champ digital full-scale is ±2; 16-bit WAV is ±1. Do not clip.
        buf.applyGain (0.5f);
        writer->writeFromAudioSampleBuffer (buf, 0, capLen);
        DBG ("Champ dump: wrote " << file.getFullPathName()
             << " samples=" << capLen
             << " fs=" << capFs
             << " (L=in R=out)");
    }
#endif

    ChampDsp dsp;
    int dbgBlocks = 0;
    float dbgPrevOut = 0.0f;
#if JUCE_DEBUG
    float capFs = 48000.0f;
    int capMax = 0;
    int capLen = 0;
    bool capArmed = false;
    bool capWritten = false;
    std::vector<float> capIn;
    std::vector<float> capOut;
#endif
};
} // namespace champ
