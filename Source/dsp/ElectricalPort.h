#pragma once

#include <JuceHeader.h>
#include <complex>

/**
 * Nominal electrical defaults for the chain adjacency contract.
 * Models may override; stubs that ignore drive/load still publish these.
 *
 * Levels (−18 dBFS) are a separate contract — ports describe Z interaction only.
 */
namespace ElectricalDefaults
{
    /** Op-amp / interface / Tube Screamer–style buffered source. */
    inline constexpr float kBufferedSourceOhms = 100.0f;

    /** Typical high-Z pedal / amp grid input, or electrically unloaded. */
    inline constexpr float kHighZInputOhms = 1.0e6f;

    /** Rough guitar pickup + cable order-of-magnitude (future pickup frontend). */
    inline constexpr float kGuitarishSourceOhms = 10.0e3f;

    /** Common speaker nominal (resistive stand-in until Z(f) exists). */
    inline constexpr float kSpeakerNominalOhms = 8.0f;
}

/**
 * Optional frequency-dependent impedance (cab speaker Z, reactive networks).
 * Owned by the Block that publishes the port; pointer in ElectricalPort is non-owning.
 */
class ImpedanceResponse
{
public:
    virtual ~ImpedanceResponse() = default;

    /** Complex Z at frequencyHz. Resistive models can return { R, 0 }. */
    virtual std::complex<float> evaluate (float frequencyHz) const = 0;
};

/**
 * Thevenin / load port exchanged between adjacent chain slots.
 * Start with resistiveOhms; set isFrequencyDependent + response for amp↔cab Z(f).
 */
struct ElectricalPort
{
    float resistiveOhms = ElectricalDefaults::kHighZInputOhms;
    bool isFrequencyDependent = false;

    /** Non-owning; valid while the publishing Block lives. */
    const ImpedanceResponse* response = nullptr;

    static ElectricalPort bufferedSource() noexcept
    {
        return { ElectricalDefaults::kBufferedSourceOhms, false, nullptr };
    }

    static ElectricalPort highZInput() noexcept
    {
        return { ElectricalDefaults::kHighZInputOhms, false, nullptr };
    }

    static ElectricalPort unloaded() noexcept
    {
        return highZInput();
    }

    static ElectricalPort interfaceSource() noexcept
    {
        return bufferedSource();
    }

    static ElectricalPort speakerResistive (float ohms = ElectricalDefaults::kSpeakerNominalOhms) noexcept
    {
        return { ohms, false, nullptr };
    }

    static ElectricalPort withResponse (float nominalOhms, const ImpedanceResponse* r) noexcept
    {
        return { nominalOhms, r != nullptr, r };
    }

    std::complex<float> evaluate (float frequencyHz) const
    {
        if (isFrequencyDependent && response != nullptr)
            return response->evaluate (frequencyHz);

        return { resistiveOhms, 0.0f };
    }
};
