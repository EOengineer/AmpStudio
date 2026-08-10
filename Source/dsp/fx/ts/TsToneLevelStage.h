#pragma once

#include "../../circuit/RcFilters.h"
#include "TsComponents.h"
#include <algorithm>

namespace ts
{
/** Active tone shelf + level pot (post-clip LPF lives in the oversampled island). */
class TsToneLevelStage
{
public:
    void prepare (float sampleRateHz, const ComponentSet& comps) noexcept
    {
        fs = sampleRateHz;
        tone.prepare (comps.tonePotR, comps.toneSeriesR, comps.toneC, fs);
        reset();
    }

    void reset() noexcept { tone.reset(); }

    void setTone (float tone01) noexcept { tone.setTone (tone01); }

    void setLevel (float level01) noexcept { levelGain = audioTaperGain (level01); }

    float processSample (float x) noexcept
    {
        return tone.process (x) * levelGain;
    }

    circuit::TsToneShelfRc& getTone() noexcept { return tone; }

    /** Continuous-time post-LP helpers for Geofex verification (not used in process). */
    static float postLpCornerHz (const ComponentSet& c) noexcept
    {
        return circuit::OnePoleLpRc::cornerHz (c.postLpR, c.postLpC);
    }

private:
    float fs = 48000.0f;
    float levelGain = audioTaperGain (0.7f);
    circuit::TsToneShelfRc tone;
};

/**
 * Output emitter-follower stand-in + series/shunt into loadContext.
 * Vout = Vin * (Zshunt||Zload) / (Rseries + Zshunt||Zload)
 */
class TsOutputBuffer
{
public:
    void prepare (const ComponentSet& comps) noexcept
    {
        seriesR = comps.outSeriesR;
        shuntR = comps.outShuntR;
    }

    void setLoadOhms (float loadOhms) noexcept
    {
        loadR = std::max (loadOhms, 1.0f);
    }

    float processSample (float vin) noexcept
    {
        const float zParallel = (shuntR * loadR) / (shuntR + loadR);
        const float gain = zParallel / (seriesR + zParallel);
        return vin * gain;
    }

    float theveninZoutOhms() const noexcept { return seriesR; }

    float getSeriesR() const noexcept { return seriesR; }
    float getShuntR() const noexcept { return shuntR; }

    static float dividerGain (float series, float shunt, float load) noexcept
    {
        const float zParallel = (shunt * load) / (shunt + load);
        return zParallel / (series + zParallel);
    }

private:
    float seriesR = Comp::kOutSeries808;
    float shuntR = Comp::kOutShunt808;
    float loadR = 1.0e6f;
};
} // namespace ts
