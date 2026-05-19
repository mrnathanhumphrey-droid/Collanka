#include "SubHarmonicExciter.h"

SubHarmonicExciter::SubHarmonicExciter() = default;

void SubHarmonicExciter::prepare(double sr, int /*blockSize*/)
{
    sampleRate_ = sr;
    ja_dt = 1.0f / static_cast<float>(sr);
    driveSm.prepare(sr); satSm.prepare(sr); bumpSm.prepare(sr); mixSm.prepare(sr);
    updatePreEmphasisCoefficients();
    updateDeEmphasisCoefficients();
    updateBumpCoefficients();
}

void SubHarmonicExciter::reset()
{
    preX1 = preY1 = 0.0f;
    M_prev = H_prev = 0.0f;
    deX1 = deY1 = 0.0f;
    bumpX1 = bumpX2 = bumpY1 = bumpY2 = 0.0f;
}

void SubHarmonicExciter::setDrive(float v)      { driveSm.set(std::max(0.0f, std::min(1.0f, v))); }
void SubHarmonicExciter::setSaturation(float v)  { satSm.set(std::max(0.0f, std::min(1.0f, v))); }
void SubHarmonicExciter::setBumpAmount(float v)  { bumpSm.set(std::max(0.0f, std::min(1.0f, v))); }
void SubHarmonicExciter::setMix(float v)         { mixSm.set(std::max(0.0f, std::min(1.0f, v))); }

void SubHarmonicExciter::triggerOnset()
{
    // Full state clear on note-on. Same rationale as TransientWaveshaper's
    // triggerOnset(): the render loop is gated by noteActive, so any state
    // left in pre/de-emphasis filters or the head-bump biquad freezes mid-
    // decay between notes and rings audibly when the next note's first
    // sample arrives. The previous 10% JA bleed left preX/preY/deX/deY and
    // the entire bumpX/bumpY chain stale — visible as a "mechanical pop".
    M_prev = 0.0f;
    H_prev = 0.0f;
    preX1 = preY1 = 0.0f;
    deX1 = deY1 = 0.0f;
    bumpX1 = bumpX2 = 0.0f;
    bumpY1 = bumpY2 = 0.0f;
    onsetSamples = ONSET_RAMP_SAMPLES;
}

void SubHarmonicExciter::setBumpFrequency(float hz)
{
    float f0 = std::max(30.0f, std::min(200.0f, hz * 1.2f));
    float A = std::pow(10.0f, (2.0f * bumpAmountVal) / 40.0f);
    float w0 = 2.0f * PI_F * f0 / static_cast<float>(sampleRate_);
    float cosw0 = std::cos(w0), sinw0 = std::sin(w0);
    float alphaQ = sinw0 / (2.0f * 1.8f);
    float a0 = 1.0f + alphaQ / A;
    bumpB0 = (1.0f + alphaQ * A) / a0;
    bumpB1 = (-2.0f * cosw0) / a0;
    bumpB2 = (1.0f - alphaQ * A) / a0;
    bumpA1 = (-2.0f * cosw0) / a0;
    bumpA2 = (1.0f - alphaQ / A) / a0;
}

void SubHarmonicExciter::updatePreEmphasisCoefficients()
{
    float fc = 400.0f, boostDB = 6.0f;
    float G = std::pow(10.0f, boostDB / 20.0f);
    float sqrtG = std::sqrt(G);
    float wc = std::tan(PI_F * fc / static_cast<float>(sampleRate_));
    float a0 = wc + 1.0f / sqrtG;
    preB0 = (wc + sqrtG) / a0;
    preB1 = (wc - sqrtG) / a0;
    preA1 = (wc - 1.0f / sqrtG) / a0;
}

void SubHarmonicExciter::updateDeEmphasisCoefficients()
{
    float fc = 400.0f, cutDB = -4.2f;
    float G = std::pow(10.0f, cutDB / 20.0f);
    float sqrtG = std::sqrt(G);
    float wc = std::tan(PI_F * fc / static_cast<float>(sampleRate_));
    float a0 = wc + 1.0f / sqrtG;
    deB0 = (wc + sqrtG) / a0;
    deB1 = (wc - sqrtG) / a0;
    deA1 = (wc - 1.0f / sqrtG) / a0;
}

void SubHarmonicExciter::updateBumpCoefficients()
{
    float fc = 80.0f, Q = 1.8f;
    float A = std::pow(10.0f, bumpGainDB / 40.0f);
    float w0 = 2.0f * PI_F * fc / static_cast<float>(sampleRate_);
    float sinw0 = std::sin(w0), cosw0 = std::cos(w0);
    float alphaQ = sinw0 / (2.0f * Q);
    float a0 = 1.0f + alphaQ / A;
    bumpB0 = (1.0f + alphaQ * A) / a0;
    bumpB1 = (-2.0f * cosw0) / a0;
    bumpB2 = (1.0f - alphaQ * A) / a0;
    bumpA1 = (-2.0f * cosw0) / a0;
    bumpA2 = (1.0f - alphaQ / A) / a0;
}

float SubHarmonicExciter::processSample(float input)
{
    float driveVal = driveSm.tick();
    float satVal = satSm.tick();
    float bumpVal = bumpSm.tick();
    float mixVal = mixSm.tick();

    polyDrive = 1.0f + driveVal * 6.0f;
    ja_inputGain = polyDrive;
    ja_a = 3.0f - satVal * 2.5f;
    ja_a = std::max(0.5f, ja_a);
    bumpAmountVal = bumpVal;

    // Smoothstep onset ramp on the driver INPUT (same fix as
    // TransientWaveshaper in v3.0.14): gates the input upstream of pre-
    // emphasis so the +6dB shelf doesn't step-respond on sample 0. The
    // shelf's b0 ≈ 1.96 produces a near-2x burst from a step input, which
    // is broadcast as a high-frequency transient through tanh and the
    // head-bump biquad — perceived as "feedback/mic-pop" at note-on.
    float progress = (onsetSamples > 0)
        ? 1.0f - static_cast<float>(onsetSamples) / static_cast<float>(ONSET_RAMP_SAMPLES)
        : 1.0f;
    if (onsetSamples > 0) --onsetSamples;
    float onsetRamp = progress * progress * (3.0f - 2.0f * progress);

    float gatedInput = input * onsetRamp;
    float dry = gatedInput;

    // Stage 1: Even-order polynomial soft shaper (symmetric — no DC offset)
    // f(x) = x / (1 + α|x|) — generates even harmonics via amplitude-dependent gain
    float x = gatedInput * polyDrive;
    float stage1 = x / (1.0f + polyAlpha * std::abs(x));

    // Stage 2: Pre-emphasis
    float preOut = preB0 * stage1 + preB1 * preX1 - preA1 * preY1;
    preX1 = stage1;
    preY1 = preOut;

    // Stage 3: anhysteretic curve (JA integration collapsed in v3.0.13 —
    // see git history). Onset ramp is now upstream of pre-emphasis.
    float H = preOut * ja_inputGain;
    float stage3 = std::tanh(H / ja_a);
    H_prev = H;
    M_prev = stage3 * ja_Ms;

    // Stage 4: De-emphasis + head bump
    float deOut = deB0 * stage3 + deB1 * deX1 - deA1 * deY1;
    deX1 = stage3;
    deY1 = deOut;

    float bumpIn = deOut;
    float bumpOut = bumpB0 * bumpIn + bumpB1 * bumpX1 + bumpB2 * bumpX2
                    - bumpA1 * bumpY1 - bumpA2 * bumpY2;
    bumpX2 = bumpX1; bumpX1 = bumpIn;
    bumpY2 = bumpY1; bumpY1 = bumpOut;

    float stage4 = deOut + bumpVal * (bumpOut - deOut);

    return dry + mixVal * (stage4 - dry);
}
