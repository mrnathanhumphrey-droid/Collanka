#pragma once
#include <cmath>
#include <algorithm>

/**
 * Three parallel SVF bandpass filters (TPT form, Vadim Zavalishin) whose centre
 * frequencies F1 / F2 / F3 are derived from a fixed anchor, the framework
 * constant Kh = 3 / log(4/3) ~= 10.428, and the residue's a_final value.
 *
 * Per spec: tuned at note-on and frozen for note duration. Q and wet are live.
 */
class CollatzFormantFilter
{
public:
    void prepare(double sampleRateHz, int /*blockSize*/);
    void reset();

    /** Recompute F1 / F2 / F3 from residue + anchor + a_final. Call at note-on. */
    void retune(int residue, int k);

    /** Live runtime params (per-block or per-sample callable). */
    void setQ(float q);
    void setWet(float w);
    void setEnabled(bool e) { enabled = e; }
    void setAnchor(float hz);

    /** Process one input sample. Returns dry*(1-wet) + sum(BPs)*wet. */
    inline float process(float input)
    {
        if (!enabled || wet < 1e-6f) return input;
        float bp = bp1.process(input) + bp2.process(input) + bp3.process(input);
        bp *= (1.0f / 3.0f);
        return input * (1.0f - wet) + bp * wet;
    }

    float getF1() const { return F1; }
    float getF2() const { return F2; }
    float getF3() const { return F3; }

private:
    struct SVFBandpass
    {
        float ic1eq = 0.0f, ic2eq = 0.0f;
        float g = 0.0f, kFb = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

        void setCoeffs(double sr, float freqHz, float Q);
        void reset() { ic1eq = ic2eq = 0.0f; }

        inline float process(float x)
        {
            float v3 = x - ic2eq;
            float v1 = a1 * ic1eq + a2 * v3;
            float v2 = ic2eq + a3 * ic1eq + a3 * v3;
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;
            return v1; // bandpass
        }
    };

    void recomputeAllCoeffs();

    double sampleRate = 44100.0;
    bool   enabled    = true;
    float  anchorHz   = 500.0f;
    float  currentQ   = 18.0f;
    float  wet        = 0.7f;

    float F1 = 500.0f, F2 = 1000.0f, F3 = 2000.0f;

    SVFBandpass bp1, bp2, bp3;

    // Cached a_final maximum at the current k (rebuilt when k changes).
    int   cachedK         = -1;
    int   cachedAFinalMax = 1;
};
