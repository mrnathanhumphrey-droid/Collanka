# BW BASS — Groove Engine R&B

A Moog Minimoog-inspired **monophonic bass synthesizer** built for R&B, Neo-Soul, Hip-Hop, and Trap production.

VST3 / AU / Standalone. Free. Built in C++17 on JUCE 8.

> **Status:** v3.0.3 — per-mode drivers, BassEQ, BassReverb, AI MIDI playback.

---

## Why another bass synth?

Most "808" plugins are a sample, a sub, and a tilt EQ. Most analog-style synths give you one saturator and call it a day. **BW BASS** is built around the idea that R&B-family bass actually wants three different drive characters depending on what kind of bass you're writing — and a real Moog ladder filter under all of it.

Three voicing modes, each routed through its own purpose-built saturator:

| Mode      | Default driver       | Character                                   |
|-----------|----------------------|---------------------------------------------|
| **Pluck** | Transient Waveshaper | Envelope-gated CP3, fast-attack neo-soul    |
| **808**   | SubHarmonic Exciter  | Even-order polynomial, trap/club sub punch  |
| **Reese** | Symmetric Folder     | Odd-harmonic fold, dark Reese growl         |

Driver assignment is `Auto` by default (follows the mode) but can be locked independently — Reese voicing through tape, Pluck through the subharmonic, etc.

---

## Features

**Voice**
- 3 oscillators (sine / triangle / saw / reverse saw / square / pulse), per-osc range, level, semi/cents tuning, OSC2 detune
- OSC3 doubles as a second LFO (oscillator/LFO mode switch)
- White / Pink noise generator with level
- Mixer feedback path

**Filter**
- Moog 4-pole ladder (12 dB / 24 dB slopes)
- Cutoff, resonance, envelope amount, keyboard tracking, drive-into-filter saturation

**Envelopes**
- Filter ADSR
- Amplitude ADSR
- Pitch envelope with amount + time (the 808 pitch-drop)

**Modulation**
- LFO with rate, depth, waveform (tri/square/sine), tempo sync, target routing (filter / pitch / amp)
- Glide / portamento with legato mode

**Drive section (per-mode)**
- `Auto` — driver follows the active bass mode
- `Tape` — full 4-stage Moog tape chain (drive, saturation, head bump, mix)
- `SubHarmonic` — even-order polynomial exciter
- `Transient` — envelope-gated CP3 waveshaper
- `Symmetric` — odd-harmonic folder

**Output chain**
- Billy Wonka Bass EQ — HPF + sub shelf + fundamental peak + mud cut, all musically-tuned for low-end work
- Optical-style RnB compressor (threshold / attack / release / ratio / parallel mix / makeup)
- BassReverb with four spaces (Room / Plate / Spring / Hall) — mix, decay, tone
- Master drive, stereo width, master volume

**Presets**
- 20 factory presets across 808 / Reese / Pluck / Hybrid families
- Standard DAW-side preset save/load via the host

**AI Assist tab (optional)**
- Local Node server bridges the plugin to the Claude API
- Two endpoints: `/chat` (sound-design Q&A with current params as context) and `/apply` (Claude proposes parameter changes you can accept)
- Bass-line MIDI generation: AI writes a sequence, plays it back through the synth via a lock-free SPSC queue (no glitches, no message-thread audio)
- Bring your own `ANTHROPIC_API_KEY`. Off by default; nothing leaves your machine without it.

---

## Install

> No prebuilt releases are posted on GitHub yet. Build from source (see below) — the CI workflow also produces Windows + macOS artifacts on every push if you fork it.

Once you have a build, drop the artefacts here:

### Windows
- `Groove Engine RnB.vst3` → `C:\Program Files\Common Files\VST3\`
- `Groove Engine RnB.exe` → anywhere (standalone)

Rescan in your DAW. Plugin shows up as **BW BASS** under *BillyWonkaMusic*.

### macOS (Universal — Intel + Apple Silicon)
- `Groove Engine RnB.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
- `Groove Engine RnB.component` → `~/Library/Audio/Plug-Ins/Components/`
- `Groove Engine RnB.app` → `/Applications/`

Restart your DAW and rescan.

**Tested in:** Ableton Live, Logic Pro, FL Studio, Reaper.

---

## Build from source

Requirements:
- CMake 3.22+
- A C++17 compiler (MSVC 2022 / Clang / Xcode 14+)
- Ninja recommended on Windows

```bash
git clone --recurse-submodules https://github.com/mrnathanhumphrey-droid/BillyWonkaVST.git
cd BillyWonkaVST
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE 8.0.12 is vendored in `JUCE/`, so no separate JUCE install is needed.

Build outputs land in `build/GrooveEngineRnB_artefacts/Release/`.

### AI Assist server (optional)
```bash
cd MCEServer
npm install
export ANTHROPIC_API_KEY=sk-ant-...
npm start
```
Server listens on `http://localhost:9150`. Plugin's AI Assist tab connects automatically.

---

## Architecture

Pure-C++ DSP core (`DSP/`) with zero JUCE dependency, wrapped by a thin JUCE `AudioProcessor` (`Source/`). UI in JUCE `Component`s with a custom look-and-feel (`UI/`). The MCE bridge (`MCEServer/`) is its own Node process.

Per-block signal flow:
```
MIDI → NoteManager
        ↓
    OscBank (3 osc + noise)
        ↓
    Mixer (+ feedback)
        ↓
    Moog ladder filter (env-modulated, key-tracked)
        ↓
    Amp ADSR
        ↓
    Pitch envelope / portamento / LFO routing
        ↓
    Bass EQ
        ↓
    Drive (Tape / SubHarmonic / Transient / Symmetric)
        ↓
    RnB Compressor → BassReverb → Output stage
```

---

## Changelog

- **v3.0.3** — -6 dB gain compensation on the Symmetric Folder (Reese driver)
- **v3.0.2** — VST3 `moduleinfo.json` trailing-comma fix (was blocking some scanners)
- **v3.0.1** — Driver audibility fix + preset path scanning across platforms
- **v3.0.0** — JUCE 8.0.12, per-mode drivers, BassEQ, BassReverb, AI MIDI playback

---

## License

Source code: [MIT](LICENSE).
JUCE is used under its [own license](JUCE/LICENSE.md).

---

## Credits

Built by [Billy Wonka Music](https://github.com/mrnathanhumphrey-droid).
Built on [JUCE 8](https://juce.com/).
