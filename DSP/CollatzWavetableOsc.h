#pragma once
#include <cmath>
#include <algorithm>

class CollatzWavetableOsc
{
public:
    static constexpr int FRAME_SIZE  = 2048;
    static constexpr int NUM_FRAMES  = 256;

    void prepare(double sampleRateHz, int /*blockSize*/)
    {
        sampleRate = sampleRateHz;
        updatePhaseInc();
    }

    void reset() { phase = 0.0; }

    void setFrequency(double freqHz)
    {
        frequency = freqHz;
        updatePhaseInc();
    }

    /** Tick from a 256-frame x 2048-sample bank with continuous WT-POS in [0, 1]. */
    inline float tick(const float* bank, float wtPos)
    {
        if (bank == nullptr) return 0.0f;

        float pos = std::min(std::max(wtPos, 0.0f), 1.0f) * static_cast<float>(NUM_FRAMES - 1);
        int frameA = static_cast<int>(pos);
        int frameB = (frameA < NUM_FRAMES - 1) ? frameA + 1 : frameA;
        float frameFrac = pos - static_cast<float>(frameA);

        const float* fa = bank + frameA * FRAME_SIZE;
        const float* fb = bank + frameB * FRAME_SIZE;

        int idxA = static_cast<int>(phase);
        int idxB = (idxA + 1) & (FRAME_SIZE - 1);
        float sampleFrac = static_cast<float>(phase - static_cast<double>(idxA));

        float sa = fa[idxA] + (fa[idxB] - fa[idxA]) * sampleFrac;
        float sb = fb[idxA] + (fb[idxB] - fb[idxA]) * sampleFrac;
        float out = sa + (sb - sa) * frameFrac;

        phase += phaseInc;
        if (phase >= static_cast<double>(FRAME_SIZE)) phase -= static_cast<double>(FRAME_SIZE);
        if (phase < 0.0)                              phase += static_cast<double>(FRAME_SIZE);

        return out;
    }

    double getFrequency() const { return frequency; }

private:
    void updatePhaseInc()
    {
        phaseInc = (sampleRate > 0.0) ? (frequency * static_cast<double>(FRAME_SIZE) / sampleRate) : 0.0;
    }

    double sampleRate = 44100.0;
    double frequency  = 440.0;
    double phase      = 0.0;
    double phaseInc   = 0.0;
};
