#pragma once

#include "../ElectricalPort.h"
#include "SpeakerRlc.h"

namespace cab
{
/**
 * ImpedanceResponse owned by CabIR (and readable by Champ via dynamic_cast).
 * Exposes RLC coeffs for time-domain OT secondary stamps.
 */
class SpeakerImpedance final : public ImpedanceResponse
{
public:
    SpeakerImpedance() { setPreset (ImpedancePreset::mesa4x12V30); }

    void setPreset (ImpedancePreset preset) noexcept
    {
        currentPreset = preset;
        rlc = makePreset (preset);
    }

    ImpedancePreset getPreset() const noexcept { return currentPreset; }
    const SpeakerRlc& getRlc() const noexcept { return rlc; }
    bool isSyntheticPlaceholder() const noexcept { return rlc.syntheticPlaceholder; }

    std::complex<float> evaluate (float frequencyHz) const override
    {
        return evaluateRlc (rlc, frequencyHz);
    }

    float nominalOhms() const noexcept { return rlc.nominalOhms; }

    static constexpr bool kSyntheticPlaceholderDefault = true; // TODO(measured-z)

private:
    ImpedancePreset currentPreset = ImpedancePreset::mesa4x12V30;
    SpeakerRlc rlc = makePreset (ImpedancePreset::mesa4x12V30);
};

/** Resolve speaker load for an amp: cab SpeakerImpedance, else resistive 8 Ω. */
inline SpeakerRlc resolveLoadRlc (const ElectricalPort& loadContext) noexcept
{
    if (loadContext.isFrequencyDependent && loadContext.response != nullptr)
    {
        if (auto* z = dynamic_cast<const SpeakerImpedance*> (loadContext.response))
            return z->getRlc();
    }

    // Unloaded (~1 MΩ) or unknown — do NOT use as speaker; fall back to flat 8 Ω.
    if (loadContext.resistiveOhms > 1000.0f)
        return makePreset (ImpedancePreset::flat8);

    SpeakerRlc flat = makePreset (ImpedancePreset::flat8);
    flat.re = loadContext.resistiveOhms;
    flat.nominalOhms = loadContext.resistiveOhms;
    return flat;
}
} // namespace cab
