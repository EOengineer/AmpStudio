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
        /** 0 = TS808 output R, 1 = TS9 output R */
        inline constexpr const char* outputVariant = "tsOutputVariant";
        /** 0 = 0.047µ Zi C, 1 = 0.1µ more-bass */
        inline constexpr const char* bassCap = "tsBassCap";
        /** 0 Si/Si, 1 asym Si, 2 Ge/Si, 3 LED */
        inline constexpr const char* diodeMode = "tsDiodeMode";
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
