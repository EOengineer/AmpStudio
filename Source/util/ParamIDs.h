#pragma once

namespace ParamIDs
{
    inline constexpr const char* inputTrimDb  = "inputTrimDb";
    inline constexpr const char* masterGainDb = "masterGainDb";

    namespace TubeScreamer
    {
        inline constexpr const char* drive = "drive";
        inline constexpr const char* tone  = "tone";
        inline constexpr const char* level = "level";
    }

    namespace Champ5F1
    {
        inline constexpr const char* volume = "volume";
    }

    namespace NeuralCapture
    {
        inline constexpr const char* inputGain  = "inputGain";
        inline constexpr const char* outputGain = "outputGain";
    }
}

namespace ModuleIds
{
    inline constexpr const char* bypass        = "bypass";
    inline constexpr const char* tubeScreamer  = "tubeScreamer";
    inline constexpr const char* champ5F1      = "champ5F1";
    inline constexpr const char* neuralCapture = "neuralCapture";
}
