#include "SynthEngine.h"
#include <cmath>
#include <algorithm>

SynthEngine::SynthEngine() = default;

void SynthEngine::prepare(double newSampleRate, int blockSize)
{
    sampleRate = newSampleRate;

    oscillatorBank.prepare(sampleRate, blockSize);
    mixer.prepare(sampleRate, blockSize);
    filter.prepare(sampleRate, blockSize);
    collatzFormant.prepare(sampleRate, blockSize);
    filterEnvelope.prepare(sampleRate, blockSize);
    ampEnvelope.prepare(sampleRate, blockSize);
    pitchEnvelope.prepare(sampleRate, blockSize);
    portamento.prepare(sampleRate, blockSize);
    lfo.prepare(sampleRate, blockSize);
    modMatrix.prepare(sampleRate, blockSize);
    outputStage.prepare(sampleRate, blockSize);
    noteManager.prepare(sampleRate, blockSize);

    // Initialize parameter smoothers (~10ms smoothing time)
    smoothCutoff.prepare(sampleRate, 0.01f);
    smoothResonance.prepare(sampleRate, 0.01f);
    smoothOsc1Level.prepare(sampleRate, 0.01f);
    smoothOsc2Level.prepare(sampleRate, 0.01f);
    smoothOsc3Level.prepare(sampleRate, 0.01f);
    smoothNoiseLevel.prepare(sampleRate, 0.01f);
    smoothDrive.prepare(sampleRate, 0.01f);
    smoothMasterVol.prepare(sampleRate, 0.01f);
}

void SynthEngine::reset()
{
    oscillatorBank.reset();
    mixer.reset();
    filter.reset();
    collatzFormant.reset();
    filterEnvelope.reset();
    ampEnvelope.reset();
    pitchEnvelope.reset();
    portamento.reset();
    lfo.reset();
    modMatrix.reset();
    outputStage.reset();
    noteManager.reset();

    feedbackSample = 0.0f;

    smoothCutoff.snapTo(baseFilterCutoff);
    smoothResonance.snapTo(0.2f);
    smoothOsc1Level.snapTo(0.8f);
    smoothOsc2Level.snapTo(0.0f);
    smoothOsc3Level.snapTo(0.0f);
    smoothNoiseLevel.snapTo(0.0f);
    smoothDrive.snapTo(0.0f);
    smoothMasterVol.snapTo(0.8f);
}

// =============================================================================
// MIDI Event Handling
// =============================================================================

void SynthEngine::handleNoteOn(int noteNumber, float velocity)
{
    // Store previous note state for legato detection
    bool wasHeld = noteManager.isNoteActive();

    noteManager.noteOn(noteNumber, velocity);

    // Determine the note frequency
    double noteFreq = NoteManager::noteToFrequency(noteNumber);

    if (wasHeld)
    {
        // Legato: set portamento target (glide from previous note)
        portamento.setTarget(noteFreq, true);
    }
    else
    {
        // New note (not legato): snap to frequency or glide depending on settings
        portamento.setTarget(noteFreq, false);

        // Trigger envelopes on non-legato notes
        filterEnvelope.noteOn(velocity);
        ampEnvelope.noteOn(velocity);

        // Click fix: clear filter biquad state + reset all oscillator phases
        // on every non-legato note-on. Without this, stale filter state
        // rings audibly when the next note's first sample arrives, and a
        // random wavetable read position produces an audible pop because
        // the first sample is whatever value the wavetable had at the
        // previous note's release phase.
        filter.reset();
        oscillatorBank.resetPhases();
    }

    // Always trigger pitch envelope on every new note (for 808 punch / pluck snap)
    pitchEnvelope.trigger();

    // Bleed tape saturator magnetization state on note-on
    outputStage.triggerTapeOnset();

    // Flag for 808 bump frequency tracking
    noteFreqChanged = true;

    // Per-note Collatz formant retune (residue = noteNumber mod 2^k).
    // Frozen for note duration per spec.
    collatzFormant.retune(noteNumber, currentCollatzK);
}

void SynthEngine::handleNoteOff(int noteNumber)
{
    int prevCurrent = noteManager.getCurrentNote();
    noteManager.noteOff(noteNumber);

    if (!noteManager.isNoteActive())
    {
        // No notes held — release envelopes
        filterEnvelope.noteOff();
        ampEnvelope.noteOff();
    }
    else if (noteManager.getCurrentNote() != prevCurrent)
    {
        // Fell back to a previously held note — glide to it
        double newFreq = NoteManager::noteToFrequency(noteManager.getCurrentNote());
        portamento.setTarget(newFreq, true);
    }
}

void SynthEngine::handlePitchBend(int value)
{
    noteManager.pitchBend(value);
}

void SynthEngine::handleControlChange(int ccNumber, int ccValue)
{
    noteManager.controlChange(ccNumber, ccValue);
}

// =============================================================================
// Per-block Audio Rendering
// =============================================================================

void SynthEngine::process(float* leftChannel, float* rightChannel, int numSamples)
{
    noteManager.prepareForBlock();

    for (int i = 0; i < numSamples; ++i)
    {
        float outputSample = 0.0f;

        // Only process audio if a note is active or envelopes are still releasing
        if (noteManager.isNoteActive() || ampEnvelope.isActive())
        {
            // =====================================================
            // 1. PORTAMENTO → current frequency
            // =====================================================
            double currentFreq = portamento.tick();

            // Apply pitch bend
            float bendSemitones = noteManager.getPitchBendSemitones();
            if (std::abs(bendSemitones) > 0.001f)
            {
                double bendRatio = std::pow(2.0, static_cast<double>(bendSemitones) / 12.0);
                currentFreq *= bendRatio;
            }

            // =====================================================
            // 2. PITCH ENVELOPE → pitch multiplier
            // =====================================================
            float pitchMod = pitchEnvelope.tick();
            currentFreq *= static_cast<double>(pitchMod);

            // =====================================================
            // 3. LFO → LFO value
            // =====================================================
            float lfoValue = lfo.tick();

            // =====================================================
            // 4. MOD MATRIX → modulated parameter offsets
            // =====================================================
            float filterEnvValue = filterEnvelope.tick();
            float ampEnvValue = ampEnvelope.getLevel();

            ModMatrix::SourceValues modSources;
            modSources.lfo = lfoValue;
            modSources.filterEnv = filterEnvValue;
            modSources.ampEnv = ampEnvValue;
            modSources.velocity = noteManager.getCurrentVelocity();
            modSources.modWheel = noteManager.getModWheel();
            modSources.osc3LFO = osc3AsLFO ? oscillatorBank.getOsc3LFOValue() : 0.0f;

            ModMatrix::ModOutputs modOutputs = modMatrix.process(modSources);

            // Apply pitch modulation from mod matrix (LFO vibrato)
            if (std::abs(modOutputs.pitchMod) > 0.001f)
            {
                double pitchModRatio = std::pow(2.0, static_cast<double>(modOutputs.pitchMod) / 12.0);
                currentFreq *= pitchModRatio;
            }

            // 808 head bump frequency tracking (only on note change, not every sample)
            if (noteFreqChanged && currentMode == BassMode::Bass808)
            {
                outputStage.setTapeBumpFrequency(static_cast<float>(currentFreq));
                noteFreqChanged = false;
            }

            // =====================================================
            // 5. OSCILLATOR BANK → raw signal (Collatz wavetable)
            //    Per-sample WT POS = base + mod-matrix offset, clamped.
            // =====================================================
            float wtPos = baseWtPos + modOutputs.wtPosMod;
            if (wtPos < 0.0f) wtPos = 0.0f;
            else if (wtPos > 1.0f) wtPos = 1.0f;

            OscillatorBank::OscOutput oscOut = oscillatorBank.tick(currentFreq, osc3AsLFO, wtPos);

            // =====================================================
            // 6. MIXER → combined signal (with feedback)
            //    Smoothed per-sample levels for click-free changes
            // =====================================================
            mixer.setOsc1Level(smoothOsc1Level.tick());
            mixer.setOsc2Level(smoothOsc2Level.tick());
            mixer.setOsc3Level(smoothOsc3Level.tick());
            mixer.setNoiseLevel(smoothNoiseLevel.tick());

            float mixedSignal = mixer.process(oscOut, feedbackSample);

            // =====================================================
            // 7. MOOG LADDER FILTER → filtered signal
            // =====================================================
            // Smooth base cutoff, then add modulation on top
            float smoothedBase = smoothCutoff.tick();
            float modulatedCutoff = smoothedBase + modOutputs.filterCutoffMod;

            // Keyboard tracking: shift filter cutoff based on played note
            if (filterKeyTrackMode > 0 && noteManager.isNoteActive())
            {
                int note = noteManager.getCurrentNote();
                float noteOffset = static_cast<float>(note - 60);
                float keyTrackFactor = 0.0f;
                switch (filterKeyTrackMode)
                {
                    case 1: keyTrackFactor = 0.33f; break;
                    case 2: keyTrackFactor = 0.67f; break;
                    case 3: keyTrackFactor = 1.0f;  break;
                }
                double trackRatio = std::pow(2.0, static_cast<double>(noteOffset * keyTrackFactor) / 12.0);
                modulatedCutoff *= static_cast<float>(trackRatio);
            }

            // Clamp to valid range and apply
            float clampedCutoff = std::max(20.0f, std::min(20000.0f, modulatedCutoff));
            filter.setCutoff(clampedCutoff);
            filter.setResonance(smoothResonance.tick());

            float filteredSignal = filter.process(mixedSignal);

            // =====================================================
            // 7b. COLLATZ FORMANT FILTER (3 SVF bandpass)
            //     F1/F2/F3 frozen at note-on; Q and wet are live + mod-routable.
            // =====================================================
            float fQ   = baseFormantQ + modOutputs.formantQMod;
            float fWet = baseFormantWet + modOutputs.formantWetMod;
            collatzFormant.setQ(fQ);
            collatzFormant.setWet(fWet);
            filteredSignal = collatzFormant.process(filteredSignal);

            // =====================================================
            // 8. AMPLITUDE ENVELOPE × filtered signal
            // =====================================================
            float ampEnvLevel = ampEnvelope.tick();

            // Apply amplitude modulation from mod matrix (tremolo)
            float ampModFactor = 1.0f + modOutputs.ampMod;
            ampModFactor = std::max(0.0f, ampModFactor);

            float shapedOutput = filteredSignal * ampEnvLevel * ampModFactor;

            // Feedback tap: after VCA (amp envelope), before output stage.
            // This matches the Minimoog's feedback path.
            feedbackSample = shapedOutput;

            // =====================================================
            // 9. OUTPUT STAGE → final sample
            //    Drive and master volume smoothed per-sample
            // =====================================================
            outputStage.setDrive(smoothDrive.tick());
            outputStage.setMasterVolume(smoothMasterVol.tick());

            // Route per-sample modulation to the active driver
            outputStage.setDriverEnvelopeGain(ampEnvLevel);
            outputStage.setDriverLFOGain(lfoValue);

            outputSample = outputStage.process(shapedOutput);
        }
        else
        {
            feedbackSample = 0.0f;
        }

        // Write to output buffers (mono synth → duplicate to stereo)
        leftChannel[i] = outputSample;
        rightChannel[i] = outputSample;
    }

    // Apply stereo width to the entire block
    for (int i = 0; i < numSamples; ++i)
    {
        outputStage.applyStereoWidth(leftChannel[i], rightChannel[i]);
    }
}

// =============================================================================
// Parameter Setters
// =============================================================================

// --- Oscillators (waveform setters are now no-ops; Collatz osc has no per-osc waveform) ---
void SynthEngine::setOsc1Waveform(int /*idx*/) {}
void SynthEngine::setOsc1Range(int idx)       { oscillatorBank.setOsc1Range(idx); }
void SynthEngine::setOsc1Level(float level)   { smoothOsc1Level.setTarget(level); }
void SynthEngine::setOsc1TuneSemi(int semi)   { oscillatorBank.setOsc1TuneSemitones(semi); }
void SynthEngine::setOsc1TuneCents(float c)   { oscillatorBank.setOsc1TuneCents(c); }

void SynthEngine::setOsc2Waveform(int /*idx*/) {}
void SynthEngine::setOsc2Range(int idx)       { oscillatorBank.setOsc2Range(idx); }
void SynthEngine::setOsc2Level(float level)   { smoothOsc2Level.setTarget(level); }
void SynthEngine::setOsc2Detune(float cents)  { oscillatorBank.setOsc2Detune(cents); }
void SynthEngine::setOsc2TuneSemi(int semi)   { oscillatorBank.setOsc2TuneSemitones(semi); }

void SynthEngine::setOsc3Mode(int mode)       { osc3AsLFO = (mode == 1); }
void SynthEngine::setOsc3Waveform(int /*idx*/) {}
void SynthEngine::setOsc3Range(int idx)       { oscillatorBank.setOsc3Range(idx); }
void SynthEngine::setOsc3Level(float level)   { smoothOsc3Level.setTarget(level); }
void SynthEngine::setOsc3TuneSemi(int semi)   { oscillatorBank.setOsc3TuneSemitones(semi); }
void SynthEngine::setOsc3LFORate(float hz)    { oscillatorBank.setOsc3LFOFrequency(static_cast<double>(hz)); }

void SynthEngine::setNoiseLevel(float level)  { smoothNoiseLevel.setTarget(level); }
void SynthEngine::setNoiseType(int type)      { oscillatorBank.setNoiseType(static_cast<NoiseGenerator::NoiseType>(type)); }

// --- Mixer ---
void SynthEngine::setFeedbackAmount(float amount)  { mixer.setFeedbackAmount(amount); }

// --- Filter ---
void SynthEngine::setFilterCutoff(float hz)
{
    baseFilterCutoff = hz;
    smoothCutoff.setTarget(hz);
}

void SynthEngine::setFilterResonance(float r)       { smoothResonance.setTarget(r); }

void SynthEngine::setFilterEnvAmount(float amount)
{
    filterEnvAmountParam = amount;
    modMatrix.setFilterEnvAmount(amount);
}

void SynthEngine::setFilterKeyTrack(int mode)        { filterKeyTrackMode = mode; }
void SynthEngine::setFilterSlope(int slope)          { filter.setSlope(slope == 0 ? MoogLadderFilter::Slope::TwoPole : MoogLadderFilter::Slope::FourPole); }
void SynthEngine::setFilterSaturation(float amount)  { filter.setSaturation(amount); }

// --- Filter Envelope ---
void SynthEngine::setFilterEnvAttack(float s)   { filterEnvelope.setAttack(s); }
void SynthEngine::setFilterEnvDecay(float s)     { filterEnvelope.setDecay(s); }
void SynthEngine::setFilterEnvSustain(float l)   { filterEnvelope.setSustain(l); }
void SynthEngine::setFilterEnvRelease(float s)   { filterEnvelope.setRelease(s); }

// --- Amplitude Envelope ---
void SynthEngine::setAmpEnvAttack(float s)      { ampEnvelope.setAttack(s); }
void SynthEngine::setAmpEnvDecay(float s)        { ampEnvelope.setDecay(s); }
void SynthEngine::setAmpEnvSustain(float l)      { ampEnvelope.setSustain(l); }
void SynthEngine::setAmpEnvRelease(float s)      { ampEnvelope.setRelease(s); }

// --- Pitch Envelope ---
void SynthEngine::setPitchEnvAmount(float semi)  { pitchEnvelope.setAmount(semi); }
void SynthEngine::setPitchEnvTime(float s)       { pitchEnvelope.setDecayTime(s); }

// --- Glide ---
void SynthEngine::setGlideEnabled(bool on)      { portamento.setEnabled(on); }
void SynthEngine::setGlideTime(float s)          { portamento.setGlideTime(s); }
void SynthEngine::setGlideLegato(bool leg)       { portamento.setLegatoMode(leg); }

// --- LFO ---
void SynthEngine::setLFORate(float hz)           { lfo.setRate(hz); }
void SynthEngine::setLFODepth(float d)           { lfo.setDepth(d); }
void SynthEngine::setLFOWaveform(int idx)        { lfo.setWaveform(static_cast<LFOEngine::Waveform>(idx)); }
void SynthEngine::setLFOTarget(int idx)          { modMatrix.setLFOTarget(static_cast<ModMatrix::Destination>(idx)); }
void SynthEngine::setLFOSync(bool sync)          { lfo.setTempoSync(sync); }
void SynthEngine::setTempoBPM(double bpm)        { lfo.setTempoBPM(bpm); }

// --- Tape Saturation ---
void SynthEngine::setTapeDrive(float drive)      { outputStage.setTapeDrive(drive); }
void SynthEngine::setTapeSaturation(float sat)   { outputStage.setTapeSaturation(sat); }
void SynthEngine::setTapeBump(float bump)        { outputStage.setTapeBump(bump); }
void SynthEngine::setTapeMix(float mix)          { outputStage.setTapeMix(mix); }

// --- Drive Mode ---
void SynthEngine::setDriveMode(int modeIndex)
{
    outputStage.setDriveMode(static_cast<DriveMode>(std::max(0, std::min(4, modeIndex))));
}

// --- Compressor ---
void SynthEngine::setCompThreshold(float dBFS)         { outputStage.setCompThreshold(dBFS); }
void SynthEngine::setCompTransientAttack(float ms)     { outputStage.setCompTransientAttack(ms); }
void SynthEngine::setCompTransientRelease(float ms)    { outputStage.setCompTransientRelease(ms); }
void SynthEngine::setCompOpticalRatio(float ratio)     { outputStage.setCompOpticalRatio(ratio); }
void SynthEngine::setCompParallelMix(float mix)        { outputStage.setCompParallelMix(mix); }
void SynthEngine::setCompOutputGain(float dB)          { outputStage.setCompOutputGain(dB); }

// --- Bass Reverb ---
void SynthEngine::setReverbEnabled(bool on)            { outputStage.setReverbEnabled(on); }
void SynthEngine::setReverbMode(int modeIndex)         { outputStage.setReverbMode(modeIndex); }
void SynthEngine::setReverbMix(float mix)              { outputStage.setReverbMix(mix); }
void SynthEngine::setReverbDecay(float decay)          { outputStage.setReverbDecay(decay); }
void SynthEngine::setReverbTone(float tone)            { outputStage.setReverbTone(tone); }

// --- BillyWonka Bass EQ ---
void SynthEngine::setEQEnabled(bool on)          { outputStage.setEQEnabled(on); }
void SynthEngine::setHPFFreq(float hz)           { outputStage.setHPFFreq(hz); }
void SynthEngine::setSubFreq(float hz)           { outputStage.setSubFreq(hz); }
void SynthEngine::setSubGain(float dB)           { outputStage.setSubGain(dB); }
void SynthEngine::setFundFreq(float hz)          { outputStage.setFundFreq(hz); }
void SynthEngine::setFundGain(float dB)          { outputStage.setFundGain(dB); }
void SynthEngine::setFundQ(float q)              { outputStage.setFundQ(q); }
void SynthEngine::setMudFreq(float hz)           { outputStage.setMudFreq(hz); }
void SynthEngine::setMudGain(float dB)           { outputStage.setMudGain(dB); }
void SynthEngine::setMudQ(float q)               { outputStage.setMudQ(q); }

// --- Output ---
void SynthEngine::setDrive(float amount)         { smoothDrive.setTarget(amount); }
void SynthEngine::setStereoWidth(float w)        { outputStage.setStereoWidth(w); }
void SynthEngine::setMasterVolume(float l)       { smoothMasterVol.setTarget(l); }

// --- Collatz wavetable + formant ---
void SynthEngine::setCollatzBank(const float* bankData) { oscillatorBank.setCollatzBank(bankData); }
void SynthEngine::setCollatzK(int k)                    { currentCollatzK = std::max(3, std::min(9, k)); }
void SynthEngine::setWtPos(float pos)                   { baseWtPos = std::max(0.0f, std::min(1.0f, pos)); }

void SynthEngine::setFormantEnabled(bool e)             { collatzFormant.setEnabled(e); }
void SynthEngine::setFormantQ(float q)                  { baseFormantQ = q; }
void SynthEngine::setFormantWet(float w)                { baseFormantWet = w; }
void SynthEngine::setFormantAnchorHz(float hz)          { collatzFormant.setAnchor(hz); }

// =============================================================================
// Bass Mode Presets
// =============================================================================

void SynthEngine::setBassMode(BassMode mode)
{
    if (mode != currentMode)
    {
        currentMode = mode;
        outputStage.setBassMode(static_cast<int>(mode));
        applyBassMode(mode);
    }
}

void SynthEngine::applyBassMode(BassMode mode)
{
    switch (mode)
    {
        case BassMode::Pluck:
        {
            // PLUCK: Triangle + Square, percussive envelope, filter snap
            setOsc1Waveform(1);  // Triangle
            setOsc1Range(2);     // 8'
            setOsc1Level(0.8f);
            setOsc1TuneSemi(0);

            setOsc2Waveform(4);  // Square
            setOsc2Range(3);     // 4' (one octave above OSC1)
            setOsc2Level(0.5f);
            setOsc2Detune(0.0f);
            setOsc2TuneSemi(-12);

            setOsc3Level(0.0f);
            setNoiseLevel(0.08f);

            setFilterCutoff(2000.0f);
            setFilterResonance(0.2f);
            setFilterSlope(0);   // 12 dB/oct — softer, warmer
            setFilterSaturation(0.15f);
            setFilterEnvAmount(0.7f);

            setFilterEnvAttack(0.001f);
            setFilterEnvDecay(0.15f);
            setFilterEnvSustain(0.0f);
            setFilterEnvRelease(0.08f);

            setAmpEnvAttack(0.001f);
            setAmpEnvDecay(0.15f);
            setAmpEnvSustain(0.0f);
            setAmpEnvRelease(0.06f);

            setPitchEnvAmount(3.0f);
            setPitchEnvTime(0.04f);

            setGlideEnabled(true);
            setGlideTime(0.06f);
            setGlideLegato(true);

            setFeedbackAmount(0.0f);
            setDrive(0.05f);

            // Driver: Pluck — envelope-gated CP3, no bump
            setTapeMix(0.6f);
            setTapeDrive(0.3f);
            setTapeBump(0.0f);

            // EQ: Pluck — tight HPF, mild sub boost, gentle fundamental
            setHPFFreq(35.0f);
            setSubFreq(60.0f);   setSubGain(2.0f);
            setFundFreq(90.0f);  setFundGain(1.0f);  setFundQ(1.2f);
            setMudFreq(300.0f);  setMudGain(-2.0f);  setMudQ(1.0f);
            break;
        }

        case BassMode::Bass808:
        {
            // 808: Sine oscillators, long decay, pitch punch
            setOsc1Waveform(0);  // Sine
            setOsc1Range(1);     // 16' (low)
            setOsc1Level(0.9f);
            setOsc1TuneSemi(0);

            setOsc2Level(0.0f);

            setOsc3Mode(0);      // Oscillator mode (not LFO)
            setOsc3Waveform(0);  // Sine
            setOsc3Range(0);     // 32' (sub octave)
            setOsc3Level(0.4f);
            setOsc3TuneSemi(-12);

            setNoiseLevel(0.0f);

            setFilterCutoff(200.0f);
            setFilterResonance(0.1f);
            setFilterSlope(1);   // 24 dB/oct — tight control
            setFilterSaturation(0.1f);
            setFilterEnvAmount(0.1f);

            setFilterEnvAttack(0.001f);
            setFilterEnvDecay(0.5f);
            setFilterEnvSustain(0.3f);
            setFilterEnvRelease(0.2f);

            setAmpEnvAttack(0.001f);
            setAmpEnvDecay(0.5f);
            setAmpEnvSustain(0.8f);
            setAmpEnvRelease(0.2f);

            setPitchEnvAmount(8.0f);
            setPitchEnvTime(0.015f);  // 15ms exponential punch

            setGlideEnabled(true);
            setGlideTime(0.1f);
            setGlideLegato(false);  // Always glide for 808 slides

            setFeedbackAmount(0.0f);
            setDrive(0.1f);

            // Driver: 808 — soft even-order poly, note-tracked bump
            setTapeBump(0.8f);
            setTapeMix(0.8f);
            setTapeDrive(0.4f);

            // EQ: 808 — deep HPF, strong sub boost, punchy fundamental, aggressive mud cut
            setHPFFreq(25.0f);
            setSubFreq(55.0f);   setSubGain(4.0f);
            setFundFreq(80.0f);  setFundGain(3.0f);  setFundQ(2.0f);
            setMudFreq(250.0f);  setMudGain(-4.0f);  setMudQ(1.5f);
            break;
        }

        case BassMode::Reese:
        {
            // REESE: Detuned sawtooths, full sustain, LFO on filter
            setOsc1Waveform(2);  // Sawtooth
            setOsc1Range(2);     // 8'
            setOsc1Level(0.7f);
            setOsc1TuneSemi(0);

            setOsc2Waveform(2);  // Sawtooth
            setOsc2Range(2);     // 8'
            setOsc2Level(0.7f);
            setOsc2Detune(30.0f);  // +0.3 semitones = 30 cents
            setOsc2TuneSemi(0);

            // Sub sine for low-end control
            setOsc3Mode(0);
            setOsc3Waveform(0);  // Sine
            setOsc3Range(1);     // 16' (one octave below)
            setOsc3Level(0.3f);

            setNoiseLevel(0.0f);

            setFilterCutoff(400.0f);
            setFilterResonance(0.25f);
            setFilterSlope(1);   // 24 dB/oct
            setFilterSaturation(0.2f);
            setFilterEnvAmount(0.2f);

            setFilterEnvAttack(0.01f);
            setFilterEnvDecay(0.3f);
            setFilterEnvSustain(0.5f);
            setFilterEnvRelease(0.15f);

            setAmpEnvAttack(0.005f);
            setAmpEnvDecay(0.1f);
            setAmpEnvSustain(1.0f);  // Full sustain
            setAmpEnvRelease(0.08f);

            setPitchEnvAmount(0.0f);  // No pitch transient
            setPitchEnvTime(0.05f);

            setGlideEnabled(true);
            setGlideTime(0.18f);
            setGlideLegato(true);

            // LFO modulates filter for the characteristic Reese wobble
            setLFORate(1.2f);
            setLFODepth(0.3f);
            setLFOWaveform(0);   // Triangle
            setLFOTarget(0);     // Filter cutoff

            setFeedbackAmount(0.0f);
            setDrive(0.08f);

            // Driver: Reese — symmetric fold, no JA, no bump
            setTapeMix(0.0f);
            setTapeBump(0.0f);
            setTapeDrive(0.15f);

            // EQ: Reese — moderate HPF, mild sub, warm fundamental, clean mud cut
            setHPFFreq(40.0f);
            setSubFreq(60.0f);   setSubGain(1.0f);
            setFundFreq(120.0f); setFundGain(2.0f);  setFundQ(1.8f);
            setMudFreq(350.0f);  setMudGain(-3.0f);  setMudQ(1.0f);
            break;
        }
    }
}
