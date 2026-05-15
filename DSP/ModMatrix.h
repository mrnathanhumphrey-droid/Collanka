#pragma once

/**
 * Modulation Matrix.
 *
 * Routes modulation sources to destinations with configurable amounts.
 * Keeps the modulation routing decoupled from the individual DSP components,
 * allowing flexible patching without refactoring core modules.
 *
 * Sources: LFO, Filter Envelope, Amplitude Envelope, Pitch Envelope,
 *          Velocity, Mod Wheel, OSC3-as-LFO.
 *
 * Destinations: Filter Cutoff, Pitch, Amplitude, Drive, plus the Collatz
 *               additions: WT POS, Collatz K, Formant Q, Formant Wet,
 *               Formant Anchor.
 */
class ModMatrix
{
public:
    enum class Source
    {
        LFO = 0,
        FilterEnvelope,
        AmpEnvelope,
        PitchEnvelope,
        Velocity,
        ModWheel,
        Osc3LFO,
        NumSources
    };

    enum class Destination
    {
        FilterCutoff = 0,
        Pitch,
        Amplitude,
        Drive,
        WtPos,
        CollatzK,
        FormantQ,
        FormantWet,
        FormantAnchor,
        NumDestinations
    };

    struct SourceValues
    {
        float lfo = 0.0f;
        float filterEnv = 0.0f;
        float ampEnv = 0.0f;
        float pitchEnv = 0.0f;
        float velocity = 1.0f;
        float modWheel = 0.0f;
        float osc3LFO = 0.0f;
    };

    struct ModOutputs
    {
        float filterCutoffMod  = 0.0f;
        float pitchMod         = 0.0f;
        float ampMod           = 0.0f;
        float driveMod         = 0.0f;
        // Collatz additions
        float wtPosMod         = 0.0f;
        float collatzKMod      = 0.0f;
        float formantQMod      = 0.0f;
        float formantWetMod    = 0.0f;
        float formantAnchorMod = 0.0f;
    };

    ModMatrix();

    void prepare(double sampleRate, int blockSize);
    void reset();

    ModOutputs process(const SourceValues& sources) const;

    void setLFOTarget(Destination dest);
    void setFilterEnvAmount(float amount);
    void setModWheelToFilterAmount(float amount);
    void setVelocitySensitivity(float amount);

private:
    Destination lfoTarget = Destination::FilterCutoff;
    float filterEnvAmount     = 0.5f;
    float modWheelToFilter    = 0.0f;
    float velocitySensitivity = 0.5f;
};
