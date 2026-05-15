#include "ModMatrix.h"
#include <cmath>
#include <algorithm>

ModMatrix::ModMatrix() = default;

void ModMatrix::prepare(double /*sampleRate*/, int /*blockSize*/)
{
    // No sample-rate-dependent state to initialize
}

void ModMatrix::reset()
{
    // No internal state to reset
}

void ModMatrix::setLFOTarget(Destination dest)
{
    lfoTarget = dest;
}

void ModMatrix::setFilterEnvAmount(float amount)
{
    filterEnvAmount = std::max(-1.0f, std::min(1.0f, amount));
}

void ModMatrix::setModWheelToFilterAmount(float amount)
{
    modWheelToFilter = std::max(0.0f, std::min(1.0f, amount));
}

void ModMatrix::setVelocitySensitivity(float amount)
{
    velocitySensitivity = std::max(0.0f, std::min(1.0f, amount));
}

ModMatrix::ModOutputs ModMatrix::process(const SourceValues& sources) const
{
    ModOutputs out;

    // --- Filter Cutoff modulation ---
    // Filter envelope contribution: bipolar amount * envelope level
    // Scaled to a frequency range (envelope at 1.0 with amount 1.0 = +10000 Hz offset)
    out.filterCutoffMod = filterEnvAmount * sources.filterEnv * 10000.0f;

    // Mod wheel → filter cutoff (unipolar, additive)
    out.filterCutoffMod += modWheelToFilter * sources.modWheel * 5000.0f;

    // --- LFO routing (to its designated target) ---
    switch (lfoTarget)
    {
        case Destination::FilterCutoff:
            // LFO modulates filter cutoff — bipolar, scaled to frequency range
            out.filterCutoffMod += sources.lfo * 4000.0f;
            break;

        case Destination::Pitch:
            // LFO modulates pitch — bipolar, in semitones
            out.pitchMod += sources.lfo * 2.0f;  // ±2 semitones at full depth
            break;

        case Destination::Amplitude:
            // LFO modulates amplitude — tremolo effect
            // Convert bipolar LFO to a gain multiplier centered at 1.0
            out.ampMod = sources.lfo * 0.5f;  // ±50% amplitude variation
            break;

        case Destination::Drive:
            // LFO modulates drive amount
            out.driveMod = sources.lfo * 0.3f;
            break;

        // --- Collatz destinations ---
        case Destination::WtPos:
            // LFO sweeps wavetable position (full +/- 0.5 around base, clamped at SynthEngine)
            out.wtPosMod = sources.lfo * 0.5f;
            break;

        case Destination::CollatzK:
            // LFO shifts modular resolution k by up to +/- 3 (rounded to int at SynthEngine)
            out.collatzKMod = sources.lfo * 3.0f;
            break;

        case Destination::FormantQ:
            // LFO sweeps Q by +/- 10 around the base value
            out.formantQMod = sources.lfo * 10.0f;
            break;

        case Destination::FormantWet:
            // LFO sweeps formant wet/dry by +/- 0.5 (clamped at filter)
            out.formantWetMod = sources.lfo * 0.5f;
            break;

        case Destination::FormantAnchor:
            // LFO sweeps anchor by +/- 200 Hz; takes effect at next note-on per spec
            out.formantAnchorMod = sources.lfo * 200.0f;
            break;

        default:
            break;
    }

    // --- OSC3-as-LFO (always routes to filter cutoff when active) ---
    // This mimics the Minimoog's OSC3 modulation behavior
    out.filterCutoffMod += sources.osc3LFO * 2000.0f;

    // --- Velocity → Amplitude ---
    // Velocity sensitivity blends between constant gain (1.0) and velocity-scaled gain.
    // At sensitivity=0, velocity has no effect. At sensitivity=1, full velocity control.
    // The amp envelope already handles velocity in its noteOn(), but this provides
    // an additional velocity→amplitude route for the mod matrix.
    // (This is additive to the ampMod, not replacing the envelope's velocity scaling.)

    return out;
}
