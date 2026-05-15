# Collonka

A **Collatz-organized Moog-shaped monophonic bass synthesizer**. Direct fork of [BW BASS / Groove Engine R&B](https://github.com/mrnathanhumphrey-droid/BillyWonkaVST), with two surgical replacements:

1. **Oscillators** — replaced with Collatz-derived 256-frame wavetable banks (K_k eigenvalue spectra, α_det-phased harmonics, prefix-iteration parity). WT POS sweeps the residue-class structural axis.
2. **Formants** — additional 3-band SVF formant stage after the Moog ladder, with center frequencies F1/F2/F3 derived from Kh, anchor, and a_final(r).

Everything else stays identical to BW BASS — Moog ladder, all four drivers (Tape / SubHarmonic / Transient / Symmetric), three voicing modes (Pluck / 808 / Reese), mod matrix, ADSR, BillyWonka Bass EQ, RnB Compressor, BassReverb, master section.

VST3 / AU / Standalone. C++17 on JUCE 8.

> **Status:** Forked from BW BASS v3.0.3. Stage 1 — empty fork rebuild. Collatz components incoming.

---

## Build from source

Requirements:
- CMake 3.22+
- A C++17 compiler (MSVC 2022 / Clang / Xcode 14+)
- Ninja recommended on Windows

```bash
git clone https://github.com/mrnathanhumphrey-droid/BillyWonkaVST.git Collonka
cd Collonka
git clone --depth 1 --branch 8.0.12 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Windows, just run `build_config.bat` then `build_compile.bat` from `C:\Collonka\`.

Build outputs land in `build/Collonka_artefacts/Release/`.

---

## Architecture

Pure-C++ DSP core (`DSP/`) with zero JUCE dependency, wrapped by a thin JUCE `AudioProcessor` (`Source/`). UI in JUCE `Component`s with a custom look-and-feel (`UI/`). The MCE bridge (`MCEServer/`) is its own Node process.

Per-block signal flow:
```
MIDI → NoteManager
        ↓
    Collatz wavetable osc bank (residue-frame morph)
        ↓
    Mixer (+ feedback)
        ↓
    Moog ladder filter (env-modulated, key-tracked)
        ↓
    Collatz formant filter (3× parallel SVF, F1/F2/F3 from Kh + a_final)
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

## License

Source code: [MIT](LICENSE).
JUCE is used under its [own license](JUCE/LICENSE.md).

---

## Credits

Forked from [BW BASS](https://github.com/mrnathanhumphrey-droid/BillyWonkaVST) by Billy Wonka Music.
Built on [JUCE 8](https://juce.com/).
