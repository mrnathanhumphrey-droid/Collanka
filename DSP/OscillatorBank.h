#pragma once
#include "CollatzWavetableOsc.h"
#include "NoiseGenerator.h"

/**
 * Three-slot Collatz wavetable bank inspired by the Minimoog's oscillator
 * section, but each oscillator now reads from a shared 256-frame Collatz
 * wavetable bank rather than a band-limited basic shape.
 *
 *  - OSC1 / OSC2 / OSC3 each have their own range / level / detune / coarse
 *    tune, so detuned stacking still produces the Moog character.
 *  - WT POS, collatzK, and collatzWavetableRule are global: all three slots
 *    read the same frame at the same time.
 *  - OSC3 retains its LFO mode (slow phase, used as a mod source).
 *  - Per-osc waveform selectors are now no-ops (kept on the API surface so
 *    the rest of the codebase compiles unchanged).
 */
class OscillatorBank
{
public:
    OscillatorBank();

    void prepare(double sampleRate, int blockSize);
    void reset();

    /** Reset only the three wavetable read phases (not noise generator state).
     *  Call on every non-legato note-on so the new note starts at phase 0 on
     *  every osc — eliminates the random-phase-at-onset pop. */
    void resetPhases();

    struct OscOutput
    {
        float osc1  = 0.0f;
        float osc2  = 0.0f;
        float osc3  = 0.0f;
        float noise = 0.0f;
    };

    /**
     * Render one sample from all three Collatz oscillators.
     *
     * @param baseFreqHz  Note frequency after portamento and pitch envelope.
     * @param osc3AsLFO   If true, OSC3 runs at its own fixed LFO frequency.
     * @param wtPos       Wavetable position in [0, 1] (per-sample, can be modulated).
     */
    OscOutput tick(double baseFreqHz, bool osc3AsLFO, float wtPos);

    /** Swap the active wavetable bank pointer (called when collatzK / Rule change). */
    void setCollatzBank(const float* bankData) { currentBank = bankData; }

    /** No-op: kept to preserve API. Waveform is no longer per-osc. */
    enum class WaveformDummy { Sine = 0, Triangle, Sawtooth, ReverseSaw, Square, Pulse };
    void setOsc1Waveform(WaveformDummy) {}
    void setOsc2Waveform(WaveformDummy) {}
    void setOsc3Waveform(WaveformDummy) {}

    // --- OSC1 parameters ---
    void setOsc1Range(int rangeIndex);
    void setOsc1TuneSemitones(int semitones);
    void setOsc1TuneCents(float cents);

    // --- OSC2 parameters ---
    void setOsc2Range(int rangeIndex);
    void setOsc2TuneSemitones(int semitones);
    void setOsc2Detune(float cents);

    // --- OSC3 parameters ---
    void setOsc3Range(int rangeIndex);
    void setOsc3TuneSemitones(int semitones);
    void setOsc3LFOFrequency(double freqHz);

    // --- Noise ---
    void setNoiseType(NoiseGenerator::NoiseType type);

    /** OSC3 LFO output (last sample), used as a mod source. */
    float getOsc3LFOValue() const { return lastOsc3Value; }

private:
    static double rangeToMultiplier(int rangeIndex);
    static double tuneToRatio(int semitones, float cents);

    CollatzWavetableOsc osc1, osc2, osc3;
    NoiseGenerator      noise;

    const float* currentBank = nullptr;

    int   osc1Range = 2;  int   osc1Semi = 0;  float osc1Cents = 0.0f;
    int   osc2Range = 2;  int   osc2Semi = 0;  float osc2Detune = 0.0f;
    int   osc3Range = 2;  int   osc3Semi = 0;  double osc3LFOFreq = 1.0;

    float  lastOsc3Value     = 0.0f;
    double currentSampleRate = 44100.0;
};
