#include "CollatzFormantFilter.h"
#include <cmath>
#include <algorithm>

namespace
{
    // Compressed Collatz: T(n) = (3n+1)/2 if odd else n/2.
    inline long long collatzStep(long long n)
    {
        return (n & 1LL) ? (3LL * n + 1LL) >> 1 : n >> 1;
    }

    // First iterate strictly below r. Returns max(1, ...) for r in {0, 1}.
    int aFinalForResidue(int r)
    {
        if (r <= 1) return 1;
        long long n = static_cast<long long>(r);
        for (int s = 0; s < 256; ++s)
        {
            n = collatzStep(n);
            if (n < static_cast<long long>(r)) return static_cast<int>(std::max<long long>(n, 1));
        }
        return std::max<int>(static_cast<int>(n), 1);
    }
}

void CollatzFormantFilter::SVFBandpass::setCoeffs(double sr, float freqHz, float Q)
{
    const double maxFc = sr * 0.49;
    const double fc    = std::min(static_cast<double>(std::max(20.0f, freqHz)), maxFc);
    const float  qSafe = std::max(0.5f, Q);
    g   = static_cast<float>(std::tan(3.14159265358979323846 * fc / sr));
    kFb = 1.0f / qSafe;
    a1  = 1.0f / (1.0f + g * (g + kFb));
    a2  = g * a1;
    a3  = g * a2;
}

void CollatzFormantFilter::prepare(double sampleRateHz, int /*blockSize*/)
{
    sampleRate = sampleRateHz;
    recomputeAllCoeffs();
}

void CollatzFormantFilter::reset()
{
    bp1.reset(); bp2.reset(); bp3.reset();
}

void CollatzFormantFilter::recomputeAllCoeffs()
{
    bp1.setCoeffs(sampleRate, F1, currentQ);
    bp2.setCoeffs(sampleRate, F2, currentQ);
    bp3.setCoeffs(sampleRate, F3, currentQ);
}

void CollatzFormantFilter::setQ(float q)
{
    currentQ = std::max(0.5f, std::min(50.0f, q));
    recomputeAllCoeffs();
}

void CollatzFormantFilter::setWet(float w)
{
    wet = std::max(0.0f, std::min(1.0f, w));
}

void CollatzFormantFilter::setAnchor(float hz)
{
    anchorHz = std::max(50.0f, std::min(2000.0f, hz));
    // Per spec: anchor only takes effect at next note-on retune, so do not
    // recompute F1/F2/F3 mid-note. Just stash the value.
}

void CollatzFormantFilter::retune(int residue, int k)
{
    const int N = 1 << k;
    const int r = ((residue % N) + N) % N;

    // Cache a_final_max at this k (only recompute when k changes)
    if (k != cachedK)
    {
        int maxAFinal = 1;
        for (int rr = 0; rr < N; ++rr)
        {
            int af = aFinalForResidue(rr);
            if (af > maxAFinal) maxAFinal = af;
        }
        cachedAFinalMax = std::max(maxAFinal, 2);
        cachedK = k;
    }

    int aFinal = aFinalForResidue(r);

    constexpr float Kh_local = 3.0f / 0.2876820724517809f; // 3 / log(4/3) ~= 10.428
    // Note: 0.2876820... = log(4/3). Hardcoded so the code does not call log at note-on.

    F1 = std::max(200.0f, std::min(1000.0f, anchorHz));

    float F2_raw = F1 * (1.0f + Kh_local / 10.0f);
    float F2_lo  = std::max(F1 * 1.3f, 600.0f);
    F2 = std::max(F2_lo, std::min(3000.0f, F2_raw));

    float aFinalRatio = std::log(static_cast<float>(std::max(aFinal, 1)))
                      / std::log(static_cast<float>(cachedAFinalMax));
    if (aFinalRatio < 0.0f) aFinalRatio = 0.0f;

    float F3_raw = F2 * (1.0f + aFinalRatio);
    float F3_lo  = std::max(F2 * 1.2f, 1500.0f);
    F3 = std::max(F3_lo, std::min(4500.0f, F3_raw));

    recomputeAllCoeffs();
}
