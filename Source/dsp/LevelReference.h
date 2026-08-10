#pragma once

namespace LevelReference
{
    /** Post-trim gated RMS that models and captures are authored against. */
    inline constexpr float kReferenceRmsDb = -18.0f;

    /** Blocks quieter than this are ignored during gated RMS / calibration. */
    inline constexpr float kGateFloorDb = -50.0f;

    inline constexpr float kInputTrimMinDb = -24.0f;
    inline constexpr float kInputTrimMaxDb = 24.0f;

    inline constexpr float kMasterGainMinDb = -60.0f;
    inline constexpr float kMasterGainMaxDb = 12.0f;

    /** Default master ≈ previous linear 0.8 gain. */
    inline constexpr float kMasterGainDefaultDb = -1.9382f;

    inline constexpr float kCalibrateSeconds = 4.0f;
}
