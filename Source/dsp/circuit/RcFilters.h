#pragma once

#include <cmath>
#include <complex>
#include <algorithm>

namespace circuit
{
/** Trapezoidal (bilinear) companion model for a capacitor to ground. */
struct CapacitorTrap
{
    float c = 0.0f;
    float sampleRate = 48000.0f;
    float geq = 0.0f;   // 2C/T
    float iEq = 0.0f;   // history current source
    float vPrev = 0.0f;

    void prepare (float capacitance, float fs) noexcept
    {
        c = capacitance;
        sampleRate = fs;
        const float T = 1.0f / std::max (fs, 1.0f);
        geq = 2.0f * c / T;
        reset();
    }

    void reset() noexcept
    {
        iEq = 0.0f;
        vPrev = 0.0f;
    }

    /** After solving new voltage across the cap, update history. */
    void advance (float vNew) noexcept
    {
        // i = geq*v - iEq; next iEq = geq*v + i
        const float i = geq * vNew - iEq;
        iEq = geq * vNew + i;
        vPrev = vNew;
    }
};

/** First-order LPF H = 1 / (1 + sRC), bilinear, from named R and C. */
class OnePoleLpRc
{
public:
    void prepare (float rOhms, float cFarads, float fs) noexcept
    {
        sampleRate = fs;
        const float rc = std::max (rOhms * cFarads, 1.0e-12f);
        const float T = 1.0f / std::max (fs, 1.0f);
        // Bilinear s = 2/T * (1-z^-1)/(1+z^-1)
        // y[n] = (x[n]+x[n-1] - (1 - 2rc/T)/(1+2rc/T) * y[n-1]) / (1 + something)
        const float a = 2.0f * rc / T;
        const float inv = 1.0f / (1.0f + a);
        b0 = inv;
        b1 = inv;
        a1 = (1.0f - a) * inv;
        reset();
    }

    void reset() noexcept
    {
        x1 = 0.0f;
        y1 = 0.0f;
    }

    float process (float x) noexcept
    {
        const float y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    /** Analytic continuous-time magnitude at freqHz (for verification). */
    static float magnitudeAt (float rOhms, float cFarads, float freqHz) noexcept
    {
        const float w = 2.0f * juceMathPi * freqHz;
        const float rc = rOhms * cFarads;
        return 1.0f / std::sqrt (1.0f + (w * rc) * (w * rc));
    }

    static constexpr float cornerHz (float rOhms, float cFarads) noexcept
    {
        return 1.0f / (2.0f * juceMathPi * rOhms * cFarads);
    }

private:
    static constexpr float juceMathPi = 3.14159265358979323846f;
    float sampleRate = 48000.0f;
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f;
    float x1 = 0.0f, y1 = 0.0f;
};

/**
 * TS-style tone shelf from Electrosmash / Keen component split:
 * H(s) = (1 + s C (R8 + T2)) / (1 + s C (R8 + T1)), T1+T2 = pot.
 * tone01: 0 = darker (+ side / more HF cut), 1 = brighter.
 */
class TsToneShelfRc
{
public:
    void prepare (float potOhms, float rSeriesOhms, float cFarads, float fs) noexcept
    {
        pot = potOhms;
        rSeries = rSeriesOhms;
        c = cFarads;
        sampleRate = fs;
        setTone (tone01);
    }

    void setTone (float tone01In) noexcept
    {
        tone01 = std::clamp (tone01In, 0.0f, 1.0f);
        // Geofex: toward (+) = more HF shunt on input (dark); toward (-) = treble
        const float t1 = tone01 * pot;         // bright side grows with tone
        const float t2 = (1.0f - tone01) * pot;
        const float rz = rSeries + t2;
        const float rp = rSeries + t1;
        const float T = 1.0f / std::max (sampleRate, 1.0f);
        // Bilinear of (1 + s C rz) / (1 + s C rp)
        const float az = 2.0f * c * rz / T;
        const float ap = 2.0f * c * rp / T;
        const float inv = 1.0f / (1.0f + ap);
        b0 = (1.0f + az) * inv;
        b1 = (1.0f - az) * inv;
        a1 = (1.0f - ap) * inv;
    }

    void reset() noexcept
    {
        x1 = 0.0f;
        y1 = 0.0f;
    }

    float process (float x) noexcept
    {
        const float y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    static float continuousMagnitude (float potOhms, float rSeries, float cFarads,
                                     float tone01, float freqHz) noexcept
    {
        const float t1 = std::clamp (tone01, 0.0f, 1.0f) * potOhms;
        const float t2 = (1.0f - std::clamp (tone01, 0.0f, 1.0f)) * potOhms;
        const float w = 2.0f * 3.14159265358979323846f * freqHz;
        const std::complex<float> s (0.0f, w);
        const auto num = 1.0f + s * cFarads * (rSeries + t2);
        const auto den = 1.0f + s * cFarads * (rSeries + t1);
        return std::abs (num / den);
    }

    /** ~3.2 kHz when Rseries dominates the wiper RC (Geofex). */
    static float seriesCornerHz (float rSeriesOhms, float cFarads) noexcept
    {
        return OnePoleLpRc::cornerHz (rSeriesOhms, cFarads);
    }

private:
    float pot = 20000.0f;
    float rSeries = 220.0f;
    float c = 0.22e-6f;
    float sampleRate = 48000.0f;
    float tone01 = 0.5f;
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f;
    float x1 = 0.0f, y1 = 0.0f;
};

/** Series RC impedance Z = R + 1/(sC); corner when 1/(ωC)=R. */
inline float seriesRcCornerHz (float rOhms, float cFarads) noexcept
{
    return OnePoleLpRc::cornerHz (rOhms, cFarads);
}

inline std::complex<float> seriesRcImpedance (float rOhms, float cFarads, float freqHz) noexcept
{
    const float w = 2.0f * 3.14159265358979323846f * freqHz;
    return std::complex<float> (rOhms, 0.0f) + std::complex<float> (0.0f, -1.0f / (w * cFarads));
}
} // namespace circuit
