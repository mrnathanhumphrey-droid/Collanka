#pragma once
#include <juce_core/juce_core.h>

namespace ParamIDs
{
    // Oscillator 1
    inline constexpr const char* osc1Waveform    = "osc1_waveform";
    inline constexpr const char* osc1Range       = "osc1_range";
    inline constexpr const char* osc1Level       = "osc1_level";
    inline constexpr const char* osc1TuneSemi    = "osc1_tune_semi";
    inline constexpr const char* osc1TuneCents   = "osc1_tune_cents";

    // Oscillator 2
    inline constexpr const char* osc2Waveform    = "osc2_waveform";
    inline constexpr const char* osc2Range       = "osc2_range";
    inline constexpr const char* osc2Level       = "osc2_level";
    inline constexpr const char* osc2Detune      = "osc2_detune";
    inline constexpr const char* osc2TuneSemi    = "osc2_tune_semi";

    // Oscillator 3
    inline constexpr const char* osc3Mode        = "osc3_mode";
    inline constexpr const char* osc3Waveform    = "osc3_waveform";
    inline constexpr const char* osc3Range       = "osc3_range";
    inline constexpr const char* osc3Level       = "osc3_level";
    inline constexpr const char* osc3TuneSemi    = "osc3_tune_semi";
    inline constexpr const char* osc3LFORate     = "osc3_lfo_rate";

    // Noise
    inline constexpr const char* noiseLevel      = "noise_level";
    inline constexpr const char* noiseType       = "noise_type";

    // Mixer
    inline constexpr const char* feedbackAmount  = "feedback_amount";

    // Filter
    inline constexpr const char* filterCutoff    = "filter_cutoff";
    inline constexpr const char* filterResonance = "filter_resonance";
    inline constexpr const char* filterEnvAmount = "filter_env_amount";
    inline constexpr const char* filterKeyTrack  = "filter_key_track";
    inline constexpr const char* filterSlope     = "filter_slope";
    inline constexpr const char* filterSaturation = "filter_saturation";

    // Filter Envelope
    inline constexpr const char* envFAttack      = "env_f_attack";
    inline constexpr const char* envFDecay       = "env_f_decay";
    inline constexpr const char* envFSustain     = "env_f_sustain";
    inline constexpr const char* envFRelease     = "env_f_release";

    // Amplitude Envelope
    inline constexpr const char* envAAttack      = "env_a_attack";
    inline constexpr const char* envADecay       = "env_a_decay";
    inline constexpr const char* envASustain     = "env_a_sustain";
    inline constexpr const char* envARelease     = "env_a_release";

    // Pitch Envelope
    inline constexpr const char* pitchEnvAmount  = "pitch_env_amount";
    inline constexpr const char* pitchEnvTime    = "pitch_env_time";

    // Glide / Portamento
    inline constexpr const char* glideOn         = "glide_on";
    inline constexpr const char* glideTime       = "glide_time";
    inline constexpr const char* glideLegato     = "glide_legato";

    // LFO
    inline constexpr const char* lfoRate         = "lfo_rate";
    inline constexpr const char* lfoDepth        = "lfo_depth";
    inline constexpr const char* lfoWaveform     = "lfo_waveform";
    inline constexpr const char* lfoTarget       = "lfo_target";
    inline constexpr const char* lfoSync         = "lfo_sync";

    // Drive Mode
    inline constexpr const char* driveMode       = "drive_mode";

    // Tape Saturation / Driver params
    inline constexpr const char* tapeDrive       = "tape_drive";
    inline constexpr const char* tapeSaturation  = "tape_saturation";
    inline constexpr const char* tapeBump        = "tape_bump";
    inline constexpr const char* tapeMix         = "tape_mix";

    // BillyWonka Bass EQ
    inline constexpr const char* eqEnabled       = "eq_enabled";
    inline constexpr const char* eqHPFFreq       = "eq_hpf_freq";
    inline constexpr const char* eqSubFreq       = "eq_sub_freq";
    inline constexpr const char* eqSubGain       = "eq_sub_gain";
    inline constexpr const char* eqFundFreq      = "eq_fund_freq";
    inline constexpr const char* eqFundGain      = "eq_fund_gain";
    inline constexpr const char* eqFundQ         = "eq_fund_q";
    inline constexpr const char* eqMudFreq       = "eq_mud_freq";
    inline constexpr const char* eqMudGain       = "eq_mud_gain";
    inline constexpr const char* eqMudQ          = "eq_mud_q";

    // Output Stage
    inline constexpr const char* driveAmount     = "drive_amount";
    inline constexpr const char* stereoWidth     = "stereo_width";
    inline constexpr const char* masterVolume    = "master_volume";

    // Compressor
    inline constexpr const char* compThreshold   = "compThreshold";
    inline constexpr const char* compAttack      = "compAttack";
    inline constexpr const char* compRelease     = "compRelease";
    inline constexpr const char* compOpticalRatio = "compOpticalRatio";
    inline constexpr const char* compParallelMix = "compParallelMix";
    inline constexpr const char* compOutputGain  = "compOutputGain";

    // Bass Reverb
    inline constexpr const char* reverbEnabled   = "reverbEnabled";
    inline constexpr const char* reverbMode      = "reverbMode";
    inline constexpr const char* reverbMix       = "reverbMix";
    inline constexpr const char* reverbDecay     = "reverbDecay";
    inline constexpr const char* reverbTone      = "reverbTone";

    // Mode
    inline constexpr const char* bassMode        = "bass_mode";

    // Collatz wavetable
    inline constexpr const char* collatzK             = "collatz_k";
    inline constexpr const char* collatzWavetableRule = "collatz_wavetable_rule";
    inline constexpr const char* wtPos                = "wt_pos";

    // Collatz formant
    inline constexpr const char* formantEnabled  = "formant_enabled";
    inline constexpr const char* formantQ        = "formant_q";
    inline constexpr const char* formantWet      = "formant_wet";
    inline constexpr const char* formantAnchorHz = "formant_anchor_hz";
}
