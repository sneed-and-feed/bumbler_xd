# 🐝 Bumbler XD

> A faithful, high-precision C++20 / JUCE 8 software homage to the legendary, long-discontinued **Wasp XT** synthesizer plugin.

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![JUCE](https://img.shields.io/badge/JUCE-8.0.6-orange.svg)
![Build](https://img.shields.io/badge/Build-Passing-brightgreen.svg)
![CTest](https://img.shields.io/badge/Tests-21%2F21%20Passed-success.svg)
![Formats](https://img.shields.io/badge/Formats-VST3%20%7C%20Standalone-purple.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

---

## Overview

**Bumbler XD** faithfully recreates the gritty, analog-digital hybrid sound character and raw edge of the classic Wasp XT synthesizer. Built from the ground up for modern 64-bit DAWs, it features sample-accurate automation, lock-free real-time audio threads, Zero-Delay Feedback (ZDF) filter topologies with modeled CMOS inverter saturation, and an authentic vintage skeuomorphic industrial rack control surface.

---

## Features

### 🎛️ Sound Generation & 3-Oscillator Engine
- **OSC 1 & OSC 2:** Multi-waveform generators supporting Sawtooth, Pulse/Square (with variable Pulse Width), Sine, and Noise/Wavetable. Coarse tuning ($\pm 3$ octaves) and fine tuning ($\pm 1$ semitone).
- **OSC Mix:** Continuous horizontal balance slider smoothly blending OSC 1 and OSC 2.
- **OSC 3:** Dedicated auxiliary sub-oscillator (Square or Sawtooth) with independent volume amount.
- **Inter-Oscillator Modulation:**
  - **Ring Modulator:** Modulates OSC 1 and OSC 2 with internal highpass DC blocking to eliminate bias runaway, producing signature metallic and inharmonic timbres.
  - **Frequency Modulation (FM):** Linear audio-rate modulation of OSC 2 by OSC 1 with Bessel-harmonic sideband expansion.
- **4th-Order PolyBLEP Anti-Aliasing:** Produces $>41\text{ dB}$ alias rejection across all frequencies up to C7 ($2093\text{ Hz}$).
- **16-Voice Polyphony:** Dynamic LRU voice allocation with smooth Hann-windowed crossfade de-clicking during voice stealing.

### 🎚️ 6-Mode Wasp Filter Section
Modeled using Zero-Delay Feedback (ZDF) State-Variable Filters with trapezoidal integration:
- **LP (12 dB Lowpass):** 2-pole resonant lowpass filter ($-13.58\text{ dB/oct}$).
- **LP FAT (24 dB Lowpass):** 4-pole cascaded lowpass with warm low-end boost and modeled non-linear **CMOS 4069UB inverter saturation** in the resonance loop ($-24.48\text{ dB/oct}$).
- **LP+NT (12 dB Lowpass + Notch Cascade):** Signature scooped resonant sweeps.
- **DBL.NT (Double Notch):** Produces twin attenuation nulls for complex vocal/comb-like textures.
- **BP (24 dB Bandpass):** Symmetrical bandpass with high-Q selectivity.
- **HP (24 dB Highpass):** Steep highpass filter ($-26.95\text{ dB/oct}$).
- **Modulation:** Keyboard tracking (`KB.TRK`) and bipolar envelope modulation depth (`ENV`).

### 🌊 Dual LFOs & Envelopes
- **Dual Multi-Waveform LFOs:** Sawtooth, Square, Sine, and Sample & Hold Noise waveforms.
  - **64-bit IEEE Double-Precision Phase Accumulation:** $0.00000\%$ timing drift over 384,000 samples at 96 kHz.
  - 6 tempo-sync musical divisions, exponential delay onset ramp, and note-on phase reset.
  - **LFO 1 Destinations:** Pitch (OSC 1+2), Filter Cutoff, or Pulse Width (PW).
  - **LFO 2 Destinations:** OSC 1 Pitch, OSC Mix ratio, or Master Amplitude.
- **Dual ADSR Envelopes:** Independent 4-stage curves for Amp and Filter with reciprocal UI `LINK` toggle.
- **MOD ENV:** Dedicated Attack-Decay envelope with bipolar depth $(-1.0 \text{ to } +1.0)$ routable to PW, LFO 1 Amount, OSC 1 Level, or OSC 2 Pitch.

### 📻 Vintage Character & Output Circuits
- **Overdrive / Distortion:** Asymmetric non-linear tanh saturation generating rich 2nd and 3rd harmonics, combined with a 1-pole dynamic Tone tilt filter ($17.70\text{ dB}$ spectral tilt range).
- **Dual Mode:** 5ms Haas psychoacoustic stereo decorrelation producing wide chorus widening ($r = -0.820 < 0.70$).
- **Analog Mode:** Gaussian random-walk pitch drift ($\pm 2.5\text{ cents}$) and free-running, non-synchronized oscillator phase on note triggers.
- **W.Noise Mode:** Switchable between authentic vintage 1024-sample Galois LFSR fixed noise loop and continuous white noise.
- **Master Bus:** Highpass 10 Hz DC-blocking filter and master output volume attenuation.

### 🖥️ APVTS & Skeuomorphic UI
- **55 Automated Parameters:** JUCE `AudioProcessorValueTreeState` (APVTS) with lock-free atomic parameter snapshot polling ($<45\text{ ns}$ latency, 0 audio thread allocations).
- **Vintage Control Panel:** Industrial slate-blue chassis, vintage brushed-metal pointer knobs, LED buttons, horizontal mix fader, and dual green LCD ADSR displays.
- **Factory Presets:**
  1. *Wasp Classic Lead*
  2. *Fat Square Bass*
  3. *Acid Reso Sweep*
  4. *Haas Stereo Pluck*
  5. *Cosmic FM Drone*

---

## Building from Source

### Prerequisites
- **CMake:** 3.22 or higher
- **C++ Compiler:** MSVC (Visual Studio 2022) with C++20 support (Windows), Clang 15+ (macOS/Linux)
- **Git**

### Build Commands
```bash
# Clone the repository
git clone https://github.com/sneed-and-feed/bumbler_xd.git
cd bumbler_xd

# Configure CMake (fetches JUCE 8 automatically)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build VST3, Standalone, and Test Suites
cmake --build build --config Release --parallel

# Run CTest verification suite (21 test suites)
ctest --test-dir build -C Release --output-on-failure
```

---

## Verification & Test Results

Bumbler XD includes a 21-suite automated CTest harness verifying all DSP topologies, acoustic properties, and real-time safety constraints:

| Category | Suites | Status | Key Metrics |
|---|---|---|---|
| **DSP Core & Polyphony** | `DspUnitTests`, `e2e_suite` | 100% Pass | 0 audio thread allocations, LRU voice stealing |
| **Filter Accuracy** | `FilterAccuracyTests` | 100% Pass | LP24 FAT: -24.48 dB/oct, LP12: -13.58 dB/oct, HP24: -26.95 dB/oct |
| **Inter-Osc Modulation** | `ModulationTests` | 100% Pass | FM Bessel sideband spread, RingMod zero DC bias |
| **Character Circuits** | `CharacterCircuitTests` | 100% Pass | Haas Dual mode $r = -0.820 < 0.70$, Analog drift $\pm 2.5$ cents |
| **Real-Time Safety** | `RealtimeSafetyTests` | 100% Pass | 0 heap allocations across 40 buffer/rate permutations, DAZ/FTZ active |
| **Adversarial Stress** | `ChallengerM1–M6` | 100% Pass | $>41\text{ dB}$ PolyBLEP alias rejection, $0.00000\%$ LFO tempo drift |

---

## License

This project is licensed under the [MIT License](LICENSE).
