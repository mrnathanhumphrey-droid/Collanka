#include "OscillatorBank.h"
#include <cmath>

OscillatorBank::OscillatorBank() = default;

void OscillatorBank::prepare(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    osc1.prepare(sampleRate, blockSize);
    osc2.prepare(sampleRate, blockSize);
    osc3.prepare(sampleRate, blockSize);
    noise.prepare(sampleRate, blockSize);
}

void OscillatorBank::reset()
{
    osc1.reset();
    osc2.reset();
    osc3.reset();
    noise.reset();
    lastOsc3Value = 0.0f;
}

void OscillatorBank::resetPhases()
{
    // Phase-only reset: clears the read position in all three Collatz
    // wavetable oscillators so a new note starts at frame index 0,
    // eliminating the click that comes from the first sample being
    // whatever value the wavetable held at the previous note's release
    // phase. Noise generator state is intentionally untouched so the
    // pink/white character is continuous across note boundaries.
    osc1.reset();
    osc2.reset();
    osc3.reset();
}

double OscillatorBank::rangeToMultiplier(int rangeIndex)
{
    switch (rangeIndex)
    {
        case 0: return 0.25;
        case 1: return 0.5;
        case 2: return 1.0;
        case 3: return 2.0;
        case 4: return 4.0;
        default: return 1.0;
    }
}

double OscillatorBank::tuneToRatio(int semitones, float cents)
{
    double totalSemi = static_cast<double>(semitones) + static_cast<double>(cents) / 100.0;
    return std::pow(2.0, totalSemi / 12.0);
}

OscillatorBank::OscOutput
OscillatorBank::tick(double baseFreqHz, bool osc3AsLFO, float wtPos)
{
    OscOutput out;

    double f1 = baseFreqHz * rangeToMultiplier(osc1Range) * tuneToRatio(osc1Semi, osc1Cents);
    osc1.setFrequency(f1);
    out.osc1 = osc1.tick(currentBank, wtPos);

    double f2 = baseFreqHz * rangeToMultiplier(osc2Range) * tuneToRatio(osc2Semi, osc2Detune);
    osc2.setFrequency(f2);
    out.osc2 = osc2.tick(currentBank, wtPos);

    if (osc3AsLFO)
    {
        osc3.setFrequency(osc3LFOFreq);
    }
    else
    {
        double f3 = baseFreqHz * rangeToMultiplier(osc3Range) * tuneToRatio(osc3Semi, 0.0f);
        osc3.setFrequency(f3);
    }
    out.osc3 = osc3.tick(currentBank, wtPos);
    lastOsc3Value = out.osc3;

    out.noise = noise.tick();
    return out;
}

void OscillatorBank::setOsc1Range(int r)            { osc1Range = r; }
void OscillatorBank::setOsc1TuneSemitones(int s)    { osc1Semi = s; }
void OscillatorBank::setOsc1TuneCents(float c)      { osc1Cents = c; }

void OscillatorBank::setOsc2Range(int r)            { osc2Range = r; }
void OscillatorBank::setOsc2TuneSemitones(int s)    { osc2Semi = s; }
void OscillatorBank::setOsc2Detune(float c)         { osc2Detune = c; }

void OscillatorBank::setOsc3Range(int r)            { osc3Range = r; }
void OscillatorBank::setOsc3TuneSemitones(int s)    { osc3Semi = s; }
void OscillatorBank::setOsc3LFOFrequency(double hz) { osc3LFOFreq = hz; }

void OscillatorBank::setNoiseType(NoiseGenerator::NoiseType t) { noise.setType(t); }
