#pragma once
#include "IBassDriver.h"
#include <cmath>
#include <algorithm>

/**
 * TransientWaveshaper — Pluck-voiced bass driver.
 *
 * Retains the CP3 asymmetric clip (correct topology for Pluck) but gates
 * the drive amount by the amplitude envelope level per sample. This makes
 * saturation track the attack transient — high drive on the initial pluck,
 * tapering to clean on the decay tail. Hardware-accurate behavior.
 *
 * Head bump is disabled (no-op). JA hysteresis retained but with envelope-
 * scaled input gain so it only colors the transient.
 *
 * Pre/de-emphasis retained for spectral shaping.
 */
class TransientWaveshaper final : public IBassDriver
{
public:
    TransientWaveshaper();

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    float processSample(float input) override;
    void triggerOnset() override;
    void setDrive(float drive) override;
    void setSaturation(float sat) override;
    void setBumpAmount(float) override { /* no-op: bump disabled for Pluck */ }
    void setMix(float mix) override;
    void setBumpFrequency(float) override { /* no-op */ }
    void setEnvelopeGain(float env) override;
    void setLFOGain(float) override { /* no-op */ }

private:
    static constexpr float PI_F = 3.14159265358979323846f;

    void updatePreEmphasisCoefficients();
    void updateDeEmphasisCoefficients();

    double sampleRate_ = 44100.0;

    // CP3 asymmetric shaper (same as MoogTapeSaturator)
    float alpha = 2.0f;
    float beta = 1.44f;
    float cpDrive = 1.0f;

    // Envelope gating — multiplies cpDrive per sample
    float envelopeLevel = 0.0f;

    // Pre-emphasis
    float preB0 = 1.0f, preB1 = 0.0f, preA1 = 0.0f;
    float preX1 = 0.0f, preY1 = 0.0f;

    // JA hysteresis (envelope-scaled input)
    float M_prev = 0.0f, H_prev = 0.0f;
    float ja_a = 1.5f, ja_Ms = 1.0f, ja_k = 0.5f;
    float ja_inputGain = 1.0f;
    float ja_dt = 1.0f / 44100.0f;

    static constexpr int ONSET_RAMP_SAMPLES = 96;
    int onsetSamples = 0;

    // De-emphasis (no bump)
    float deB0 = 1.0f, deB1 = 0.0f, deA1 = 0.0f;
    float deX1 = 0.0f, deY1 = 0.0f;

    // DC blocker on the wet output. CP3 asymmetric clip (alpha=2 vs
    // beta=1.44) puts ~5-8% DC offset on a symmetric input. When the VCA
    // opens that DC gets multiplied by an attacking envelope ramp →
    // audible sub-audio thump on every note-on.
    // R = exp(-2*pi*fc/sr); fc ~= 35 Hz at 44.1 kHz gives R ~= 0.995.
    float dcX1 = 0.0f, dcY1 = 0.0f;
    static constexpr float DC_BLOCK_R = 0.995f;

    struct Smooth
    {
        float current = 0.0f, target = 0.0f, coeff = 0.999f;
        void prepare(double sr, float timeMs = 20.0f)
        {
            float samples = static_cast<float>(sr) * timeMs * 0.001f;
            coeff = (samples > 1.0f) ? std::exp(-2.302585f / samples) : 0.0f;
        }
        void set(float v) { target = v; }
        void snap(float v) { current = v; target = v; }
        float tick() { current = current * coeff + target * (1.0f - coeff); return current; }
    };

    Smooth driveSm, satSm, mixSm;
};
