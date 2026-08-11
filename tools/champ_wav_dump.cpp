// Write the champ_verify driven-hash stimulus through ChampDsp (no JUCE).
// Build: clang++ -std=c++17 -O2 -I Source tools/champ_wav_dump.cpp -o tools/champ_wav_dump
//        ./tools/champ_wav_dump
// Output: tools/champ_offline_dump.wav (gitignored)

#include "dsp/amps/champ/ChampDsp.h"
#include "dsp/amps/champ/ChampVerify.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
void writePcm16Wav (const char* path, const float* samples, int n, int fs)
{
    const int dataBytes = n * 2;
    const int fileBytes = 36 + dataBytes;
    FILE* f = std::fopen (path, "wb");
    if (f == nullptr)
    {
        std::perror (path);
        return;
    }

    auto u16 = [&] (uint16_t v)
    {
        const unsigned char b[2] { (unsigned char) (v & 0xff), (unsigned char) (v >> 8) };
        std::fwrite (b, 1, 2, f);
    };
    auto u32 = [&] (uint32_t v)
    {
        const unsigned char b[4] {
            (unsigned char) (v & 0xff),
            (unsigned char) ((v >> 8) & 0xff),
            (unsigned char) ((v >> 16) & 0xff),
            (unsigned char) ((v >> 24) & 0xff)
        };
        std::fwrite (b, 1, 4, f);
    };

    std::fwrite ("RIFF", 1, 4, f);
    u32 ((uint32_t) fileBytes);
    std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f);
    u32 (16);
    u16 (1);  // PCM
    u16 (1);  // mono
    u32 ((uint32_t) fs);
    u32 ((uint32_t) fs * 2);
    u16 (2);
    u16 (16);
    std::fwrite ("data", 1, 4, f);
    u32 ((uint32_t) dataBytes);

    for (int i = 0; i < n; ++i)
    {
        // Champ digital full-scale is ±2; 16-bit WAV is ±1. Do not clip.
        float x = std::clamp (samples[i] * 0.5f, -1.0f, 1.0f);
        const int s = (int) std::lround (x * 32767.0f);
        const int16_t v = (int16_t) std::clamp (s, -32767, 32767);
        const unsigned char b[2] {
            (unsigned char) ((uint16_t) v & 0xff),
            (unsigned char) ((uint16_t) v >> 8)
        };
        std::fwrite (b, 1, 2, f);
    }

    std::fclose (f);
}
} // namespace

int main()
{
    constexpr float kFs = 48000.0f;
    constexpr int kSeconds = 2;
    constexpr int kN = (int) kFs * kSeconds;

    champ::ChampDsp dsp;
    dsp.prepare (kFs);
    dsp.setVolume (1.0f);
    dsp.setNfbEnabled (true);
    dsp.setSpeakerRlc (cab::makePreset (cab::ImpedancePreset::flat8));
    dsp.reset();
    for (int i = 0; i < 256; ++i)
        dsp.processSample (0.0f);

    std::vector<float> y ((size_t) kN, 0.0f);
    for (int i = 0; i < kN; ++i)
        y[(size_t) i] = dsp.processSample (champ::drivenHashInput (i, kFs));

    const std::string path = "tools/champ_offline_dump.wav";
    writePcm16Wav (path.c_str(), y.data(), kN, (int) kFs);
    std::printf ("Wrote %s (%d samples @ %.0f Hz, vol 1, NFB Stock)\n",
                 path.c_str(), kN, (double) kFs);
    return 0;
}
