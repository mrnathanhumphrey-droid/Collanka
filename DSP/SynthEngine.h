#pragma once
#include "OscillatorBank.h"
#include "MixerSection.h"
#include "MoogLadderFilter.h"
#include "CollatzFormantFilter.h"
#include "EnvelopeGenerator.h"
#include "PitchEnvelope.h"
#include "PortamentoEngine.h"
#include "LFOEngine.h"
#include "ModMatrix.h"
#include "OutputStage.h"
#include "NoteManager.h"

/**
 * SynthEngine — Top-level DSP Orchestrator.
 *
 * Owns all DSP components and coordinates the per-sample render loop.
 * This is a pure C++ class with no JUCE dependencies — all JUCE interaction
 * happens through PluginProcessor, which calls updateParameters() each block
 * and process() to render audio.
 *
 * Signal flow per sample:
 *   1. Portamento → current frequency
 *   2. Pitch envelope → pitch multiplier
 *   3. LFO → LFO value
 *   4. Mod matrix → modulated parameter offsets
 *   5. Oscillator bank → raw oscillator signals
 *   6. Mixer → combined signal (with feedback)
 *   7. Moog ladder filter → filtered signal
 *   8. Amplitude envelope × filtered signal → shaped output
 *   9. Output stage (drive, EQ, width) → final sample
 *
 * Bass modes (Pluck, 808, Reese) configure parameter presets via
 * applyBassMode(), which sets sensible defaults for each archetype
 * while allowing user tweaking afterwards.
 */
class SynthEngine
{
public:
    enum class BassMode
    {
        Pluck = 0,
        Bass808,
        Reese
    };

    SynthEngine();

    void prepare(double sampleRate, int blockSize);
    void reset();

    /**
     * Render audio into a stereo buffer.
     * @param leftChannel   Pointer to left channel output samples.
     * @param rightChannel  Pointer to right channel output samples.
     * @param numSamples    Number of samples to render.
     */
    void process(float* leftChannel, float* rightChannel, int numSamples);

    // --- MIDI event forwarding (called from PluginProcessor) ---
    void handleNoteOn(int noteNumber, float velocity);
    void handleNoteOff(int noteNumber);
    void handlePitchBend(int value);
    void handleControlChange(int ccNumber, int ccValue);

    // --- Parameter update (called once per block from PluginProcessor) ---

    // Oscillators
    void setOsc1Waveform(int waveformIndex);
    void setOsc1Range(int rangeIndex);
    void setOsc1Level(float level);
    void setOsc1TuneSemi(int semitones);
    void setOsc1TuneCents(float cents);

    void setOsc2Waveform(int waveformIndex);
    void setOsc2Range(int rangeIndex);
    void setOsc2Level(float level);
    void setOsc2Detune(float cents);
    void setOsc2TuneSemi(int semitones);

    void setOsc3Mode(int mode);  // 0 = oscillator, 1 = LFO
    void setOsc3Waveform(int waveformIndex);
    void setOsc3Range(int rangeIndex);
    void setOsc3Level(float level);
    void setOsc3TuneSemi(int semitones);
    void setOsc3LFORate(float hz);

    void setNoiseLevel(float level);
    void setNoiseType(int type);

    // Mixer
    void setFeedbackAmount(float amount);

    // Filter
    void setFilterCutoff(float hz);
    void setFilterResonance(float r);
    void setFilterEnvAmount(float amount);
    void setFilterKeyTrack(int mode);       // 0=0%, 1=33%, 2=67%, 3=100%
    void setFilterSlope(int slope);         // 0=12dB, 1=24dB
    void setFilterSaturation(float amount);

    // Filter Envelope
    void setFilterEnvAttack(float seconds);
    void setFilterEnvDecay(float seconds);
    void setFilterEnvSustain(float level);
    void setFilterEnvRelease(float seconds);

    // Amplitude Envelope
    void setAmpEnvAttack(float seconds);
    void setAmpEnvDecay(float seconds);
    void setAmpEnvSustain(float level);
    void setAmpEnvRelease(float seconds);

    // Pitch Envelope
    void setPitchEnvAmount(float semitones);
    void setPitchEnvTime(float seconds);

    // Glide
    void setGlideEnabled(bool enabled);
    void setGlideTime(float seconds);
    void setGlideLegato(bool legatoOnly);

    // LFO
    void setLFORate(float hz);
    void setLFODepth(float depth);
    void setLFOWaveform(int waveformIndex);
    void setLFOTarget(int targetIndex);
    void setLFOSync(bool sync);
    void setTempoBPM(double bpm);

    // Per-mode driver (routed through active IBassDriver variant)
    void setTapeDrive(float drive);
    void setTapeSaturation(float sat);
    void setTapeBump(float bump);
    void setTapeMix(float mix);

    // Drive mode selection
    void setDriveMode(int modeIndex);  // 0=Auto, 1=Tape, 2=SubHarmonic, 3=Transient, 4=Symmetric

    // Compressor
    void setCompThreshold(float dBFS);
    void setCompTransientAttack(float ms);
    void setCompTransientRelease(float ms);
    void setCompOpticalRatio(float ratio);
    void setCompParallelMix(float mix);
    void setCompOutputGain(float dB);

    // Bass Reverb
    void setReverbEnabled(bool on);
    void setReverbMode(int modeIndex);
    void setReverbMix(float mix);
    void setReverbDecay(float decay);
    void setReverbTone(float tone);

    // BillyWonka Bass EQ
    void setEQEnabled(bool on);
    void setHPFFreq(float hz);
    void setSubFreq(float hz);     void setSubGain(float dB);
    void setFundFreq(float hz);    void setFundGain(float dB);
    void setFundQ(float q);
    void setMudFreq(float hz);     void setMudGain(float dB);
    void setMudQ(float q);

    // Output
    void setDrive(float amount);
    void setStereoWidth(float width);
    void setMasterVolume(float level);

    // Bass Mode
    void setBassMode(BassMode mode);

    /** Get current bass mode for UI feedback. */
    BassMode getBassMode() const { return currentMode; }

    // --- Collatz wavetable + formant ---
    /** Swap active wavetable bank (called from PluginProcessor on K / Rule change). */
    void setCollatzBank(const float* bankData);
    /** Modular resolution k (3..9). Stored for formant retune; bank itself is set separately. */
    void setCollatzK(int k);
    /** Wavetable position in [0, 1]; per-block target, smoothed if needed at higher level. */
    void setWtPos(float pos);

    void setFormantEnabled(bool enabled);
    void setFormantQ(float q);
    void setFormantWet(float wet);
    void setFormantAnchorHz(float hz);

private:
    /** Apply parameter preset for the selected bass mode. */
    void applyBassMode(BassMode mode);

    // DSP Components (all owned, no heap allocation)
    OscillatorBank oscillatorBank;
    MixerSection mixer;
    MoogLadderFilter filter;
    CollatzFormantFilter collatzFormant;
    EnvelopeGenerator filterEnvelope;
    EnvelopeGenerator ampEnvelope;
    PitchEnvelope pitchEnvelope;
    PortamentoEngine portamento;
    LFOEngine lfo;
    ModMatrix modMatrix;
    OutputStage outputStage;
    NoteManager noteManager;

    // State
    double sampleRate = 44100.0;
    BassMode currentMode = BassMode::Pluck;

    // Feedback delay line (1 sample)
    float feedbackSample = 0.0f;

    // Note frequency change tracking (for 808 bump retune)
    bool noteFreqChanged = false;

    // Parameter cache for per-sample modulation
    float baseFilterCutoff = 2000.0f;
    float filterEnvAmountParam = 0.5f;
    int filterKeyTrackMode = 0;
    bool osc3AsLFO = false;

    // Collatz parameter cache (block-rate base values; modulation applied per-sample in process())
    int   currentCollatzK   = 6;
    float baseWtPos         = 0.0f;
    float baseFormantQ      = 18.0f;
    float baseFormantWet    = 0.7f;

    // =========================================================================
    // Per-sample parameter smoother (one-pole IIR, no JUCE dependency)
    // =========================================================================
    struct SmoothedParam
    {
        float current = 0.0f;
        float target = 0.0f;
        float coeff = 0.995f;  // Smoothing coefficient (set in prepare)

        void prepare(double sr, float timeSeconds = 0.01f)
        {
            // Coefficient for ~99% convergence in timeSeconds
            float samples = static_cast<float>(sr) * timeSeconds;
            coeff = (samples > 1.0f) ? std::exp(-2.302585f / samples) : 0.0f;
        }

        void setTarget(float v) { target = v; }
        void snapTo(float v)    { current = v; target = v; }

        float tick()
        {
            current = current * coeff + target * (1.0f - coeff);
            return current;
        }
    };

    SmoothedParam smoothCutoff;
    SmoothedParam smoothResonance;
    SmoothedParam smoothOsc1Level;
    SmoothedParam smoothOsc2Level;
    SmoothedParam smoothOsc3Level;
    SmoothedParam smoothNoiseLevel;
    SmoothedParam smoothDrive;
    SmoothedParam smoothMasterVol;
};
