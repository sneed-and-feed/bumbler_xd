# Bumbler XD · Changelog & Release Notes

All notable changes, architectural modernizations, DSP algorithm specifications, and bug remediations for the **Bumbler XD** polyphonic synthesizer are documented in this file.

The project adheres strictly to [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) standards and [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).

---

## Semantic Versioning & Compatibility Commitment

Bumbler XD maintains strict API and DAW state recall stability commitments across its architectural layers:

### 1. `bumbler_dsp_core` C++20 Static Library API
- **MAJOR (X.0.0):** Incompatible public API modifications. This encompasses breaking signature changes to `BumblerEngine`, `BumblerVoiceManager`, or `CharacterCircuits`, altering the binary memory layout of the 55-member `ParameterSnapshot` POD struct, or modifying real-time threading contracts.
- **MINOR (1.X.0):** Backwards-compatible additions to the DSP core. This includes adding new synthesis modes, subordinate DSP helper classes, non-breaking query methods, or performance optimizations that preserve existing audio invariants.
- **PATCH (1.0.X):** Backwards-compatible bug fixes, numerical safety patches, denormal/NaN defensive guards, code comments, and documentation updates.

### 2. JUCE AudioProcessorValueTreeState (APVTS) Parameter IDs
- **Immutability Guarantee:** All 55 APVTS Parameter ID strings (e.g., `"osc1_waveform"`, `"filter_cutoff"`, `"amp_attack"`, `"master_volume"`) are permanently immutable throughout the entire v1.x lifecycle.
- **DAW State Recall Guarantee:** Any project, track automation lane, or preset created in a major Digital Audio Workstation (Ableton Live, FL Studio, Reaper, Cubase, Logic Pro, Bitwig Studio, Studio One) with any v1.x release of Bumbler XD will restore identically in all subsequent v1.x releases without parameter displacement or state loss.
- **Parameter Normalization Invariant:** Parameter value ranges, skew factors, and default initializations remain backwards-compatible across all minor and patch updates.

---

## Version Compatibility Matrix

| Bumbler XD Version | DSP Core API (`bumbler_dsp_core`) | APVTS Parameter IDs (55 Total) | JUCE Framework | C++ Standard | DAW Project State Recall | Supported Platforms |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **v1.0.0** | v1.0.0 (Initial C++20 Core) | 55 Immutable IDs (Registered) | v8.0.6 | ISO C++20 | Full Compatibility | Windows (x64) |
| **v1.0.1** | v1.0.1 (Modular Headers, Defensive Checks) | 55 Immutable IDs (Bit-Identical) | v8.0.6 | ISO C++20 | 100% Backwards-Compatible | Windows (x64), macOS (arm64/x64), Linux (x64) |
| **v1.0.2** | v1.0.2 (Reference Docs, Golden Vectors, CLI) | 55 Immutable IDs (Bit-Identical) | v8.0.6 | ISO C++20 | 100% Backwards-Compatible | Windows (x64), macOS (arm64/x64), Linux (x64) |
| **v1.0.3** | v1.0.3 (Per-Algorithm Fixtures, Minimal Example, Voice Stealing Proof) | 55 Immutable IDs (Bit-Identical) | v8.0.6 | ISO C++20 | 100% Backwards-Compatible | Windows (x64), macOS (arm64/x64), Linux (x64) |

---

## Release History

## [1.0.3] - 2026-09-23

### Added
- **Per-Algorithm Golden CSV Fixtures (`tests/fixtures/`):**
  - Added isolated golden CSV test fixtures for modular DSP components: `polyblep_saw_golden.csv`, `polyblep_pulse_golden.csv`, `zdf_svf_lp12_step_golden.csv`, `zdf_svf_lp24_fat_step_golden.csv`, and `haas_decorrelation_golden.csv`.
  - Extended `tests/golden_vector_tests.cpp` with `testAlgorithmLevelGoldenVectors()`, enforcing $\text{SNR} > 120\text{ dB}$ and $\Delta_{\max} < 10^{-4}$ on isolated components.
  - Updated `tests/fixtures/README.md` with deep links mapping each fixture CSV to corresponding equations in `docs/DSP_ALGORITHMS.md`.
  - Added reproducible fixture generator script `scripts/generate_fixtures.py`.
- **Bare-Minimum C++ Integration Example (`examples/minimal_integration.cpp`):**
  - Self-contained ~55-line quickstart example demonstrating `BumblerEngine` preparation, snapshot loading, and block rendering with zero GUI or CLI parsing dependencies.
  - Registered `bumbler_minimal_example` executable target in root `CMakeLists.txt`.
  - Featured in `docs/API.md` and `README.md` as the primary quickstart tutorial.
- **Formal Voice Stealing & De-Clicking Specification (`docs/DSP_ALGORITHMS.md`):**
  - Section 6.1: Complete Mermaid decision tree and 4-tier voice allocation policy.
  - Section 6.2: Mathematical derivation of the 5ms Hann crossfade window ($w[n] = \frac{1}{2}(1 - \cos(\pi n / L))$).
  - Section 6.3: Formal proof of $C^1$ continuity at transition boundaries eliminating DC-offset impulse clicks during polyphonic stealing.
- **Known Limitations & Architectural Boundaries:**
  - Added dedicated architectural boundaries section in `docs/API.md` and `README.md` documenting fixed 16-voice ceiling, channel-global pitch bend/CC, filter cutoff clamping, block-rate automation, and single stereo bus topology.

---

## [1.0.2] - 2026-09-23

### Added
- **Formal DSP Architecture Specification (`docs/DSP_ALGORITHMS.md`):**
  - Comprehensive mathematical foundations and circuit derivations for all synthesis subsystems.
  - Complete signal flow Mermaid architecture diagrams documenting audio and modulation routing.
  - 4th-order PolyBLEP piecewise polynomial derivations with boundary continuity ($C^0, C^1, C^2$) and alias rejection spectral floor measurements ($>41\text{ dB}$ at C7).
  - Zero-Delay Feedback (ZDF) State-Variable Filter (SVF) bilinear integration, cutoff pre-warping ($g = \tan(\pi f_c / f_s)$), and algebraic loop resolution (Topology-Preserving Transform).
  - Modeled non-linear CMOS CD4069UB inverter saturation in the filter resonance loop.
  - Linear audio-rate FM analysis via Jacobi-Anger Bessel function expansion ($J_n(\beta)$).
  - Four-quadrant ring modulator analysis with 1-pole highpass DC blocking ($R \approx 0.99869$).
  - Master character circuits: Asymmetric tanh overdrive ($u = x + 0.15 x^2$), 1-pole dynamic tone tilt filter ($17.7\text{ dB}$ tilt), Haas 5ms psychoacoustic stereo decorrelation ($r \approx -0.820 < 0.70$), Gaussian random-walk pitch drift ($\pm 2.5\text{ cents}$), and 1024-sample Galois LFSR periodic noise.
  - Section 6.1 voice allocation decision tree and stealing policy with detailed Mermaid decision flowchart.
  - Mathematical derivation of the 5ms Hann crossfade window ($w[n] = \frac{1}{2}(1 - \cos(\pi n / L))$, $L = \lfloor 0.005 \cdot f_s \rfloor$).
  - Formal proof of $C^1$ derivative continuity at boundary points ($\left.\frac{dw}{dn}\right|_{n=0} = \left.\frac{dw}{dn}\right|_{n=L} = 0$), establishing asymptotic spectral roll-off ($\mathcal{O}(\omega^{-3})$, $-18\text{ dB/oct}$) and complete elimination of DC-offset impulse clicks.
- **C++20 Core API Reference & Integration Guide (`docs/API.md`):**
  - Exhaustive class references for `BumblerEngine`, `BumblerVoiceManager`, `ParameterSnapshot`, and `CharacterCircuits`.
  - Real-time safety rules, single-thread audio invariant, and lock-free parameter snapshot exchange patterns ($<45\text{ ns}$ extraction latency).
  - Hardware FTZ/DAZ denormal handling via RAII `ScopedNoDenormals` (x86 MXCSR and ARM64 FPCR).
  - Third-party host integration patterns for JUCE 8 DAWs, Unreal Engine 5 (MetaSounds / Submix Synthesis), Unity 3D (C# Native Audio Plugin API), and headless Linux (JACK / ALSA / Bela).
  - Complete standalone C++20 end-to-end synthesizer rendering example.
  - Dedicated Section 7 on Known Limitations & Architectural Boundaries (fixed 16-voice ceiling, channel-global pitch bend/CC, filter cutoff clamping, block-rate APVTS snapshots, single stereo output bus).
- **Headless Synthesizer CLI Example (`examples/headless_synth.cpp`):**
  - Standalone command-line executable (`bumbler_headless_example`) demonstrating direct instantiation and rendering of `BumblerEngine` without GUI dependencies.
  - Configurable CLI arguments: `--preset <0-4>`, `--note <0-127>`, `--duration <seconds>`, and `--output <file.wav>`.
  - Clean 16-bit PCM stereo WAV binary writer capturing the full envelope release tail.
- **Deterministic Golden Vector Verification Suite (`tests/golden_vector_tests.cpp`):**
  - Golden test suite (`bumbler_golden_vector_tests`) providing bit-exact regression protection across all 5 factory presets.
  - Validates Signal-to-Noise Ratio strictly $>120.0\text{ dB}$ and peak sample delta $\Delta_{\max} < 1.0 \times 10^{-4}$ against stored reference fixtures in `tests/fixtures/`.
  - Enforces invariant assertions: strictly finite samples (0 NaNs, 0 Infs), 0 denormals, bounded peaks ($[-1.05, +1.05]$), and stereo decorrelation thresholds.
- **Empirical Performance Benchmark Report (`BENCHMARKS.md`):**
  - Comprehensive CPU load measurements showing saturated 16-voice polyphony consumes only $0.62\%$ CPU at 48 kHz / 256 samples, and $<2.58\%$ at 192 kHz.
  - Measured AVX2 SIMD speedup of $2.48\times$ to $2.52\times$ over scalar processing.
  - Atomic parameter snapshot extraction benchmarked at an average of $34.8\text{ ns}$ ($<45\text{ ns}$ ceiling).
- **Production Packaging & Release Automation (`scripts/package_release.py`):**
  - Automated release packaging script generating standalone archives, VST3 bundles, and GNU `sha256sum`-compatible cryptographic manifests (`SHA256SUMS.txt`).

### Changed
- Standardized `BumblerVoiceManager` and `BumblerVoice` documentation to explicitly detail the 4-tier allocation hierarchy and 5ms Hann crossfading mechanics.
- Updated `CMakeLists.txt` project version to `1.0.2`.
- Synchronized release packaging metadata and paths with `braun_as-42` and `braun_rb-26` standards.

---

## [1.0.1] - 2026-09-23

### Added
- **Multi-Platform CI Matrix (`.github/workflows/ci.yml`):**
  - Automated GitHub Actions build and test matrix covering Windows (MSVC 2022 x64), macOS (Apple Silicon arm64 & Intel x86_64 Universal 2 binary), and Ubuntu Linux (GCC 11+ with Xvfb headless virtual display).
  - Multi-platform packaging and automated release asset publishing workflow (`.github/workflows/release-builds.yml`).
- **Modular Parameter Hierarchy (`source/plugin/parameters/`):**
  - Decoupled the parameter definitions from monolithic `Parameters.h` into focused domain-specific headers:
    - `OscillatorParameters.h`: OSC 1, OSC 2, OSC 3, Pulse Width, Ring Mod, and FM parameters.
    - `FilterParameters.h`: 6-mode filter parameters, cutoff, resonance, keyboard tracking, and envelope amount.
    - `EnvelopeParameters.h`: Amp ADSR, Filter ADSR, `LINK` toggle, and MOD Attack-Decay envelope.
    - `LfoParameters.h`: LFO 1 and LFO 2 waveforms, rates, delay ramps, tempo sync divisions, and matrix targets.
    - `CharacterParameters.h`: Drive, Tone, Dual Mode, Analog Mode, W.Noise mode, and Master Volume.
    - `PresetParameters.h`: 5 signature factory preset parameter state definitions.
    - `ParameterHelpers.h`: Reusable atomic parameter creation and snapshot extraction helpers.
- **Defensive Input Validation & Hardening:**
  - Added sample rate defense in `BumblerVoiceManager::prepare()` and `BumblerVoice::prepare()`, clamping invalid, non-finite, sub-audio ($<1000.0\text{ Hz}$), or ultrasonic ($>384000.0\text{ Hz}$) rates to $48000.0\text{ Hz}$.
  - Added buffer size validation clamping `maxBlockSize` to $[1, 8192]$ samples.
  - Implemented safe chunked mono downmix rendering in `BumblerVoiceManager::renderBlock()`: if `numSamples > 8192`, rendering splits safely across `mMonoScratchBuffer` in bounded chunks to prevent stack/heap memory overrun.
  - Added bitwise IEEE-754 validation (`isFiniteBitwise`) to sanitize poison inputs (NaNs and infinities) across all audio render entry points.
- **Multi-Platform Build Documentation (`README.md`):**
  - Documented exact command-line invocation steps for MSVC on Windows, Xcode/Clang Universal binary on macOS, and GCC/Mesa/X11 on Ubuntu Linux.

### Changed
- Refactored `source/plugin/Parameters.h` to aggregate the modular parameter headers while maintaining exact bit-for-bit APVTS parameter ID strings, ranges, and default values.
- Cleaned up compiler warnings across GCC and Clang in headless offscreen painting and SIMD operations.

---

## [1.0.0] - 2026-09-23

### Added
- **Initial Public Release of Bumbler XD Synthesizer:**
  - Complete polyphonic synthesizer plugin compiled for VST3 and Standalone formats via JUCE 8 (v8.0.6) and ISO C++20.
  - Zero-dependency standalone DSP static library (`bumbler_dsp_core`).
- **Sound Generation & 3-Oscillator Section:**
  - Primary oscillators OSC 1 and OSC 2 with Sawtooth, Pulse (variable duty cycle $1\%\text{ to }99\%$), Sine, and Noise waveforms.
  - Continuous coarse pitch tuning ($\pm 3$ octaves) and fine tuning ($\pm 100$ cents).
  - Dedicated auxiliary sub-oscillator (OSC 3) operating one octave below OSC 1 with selectable Square or Sawtooth waveform.
  - Smooth horizontal balance mix fader between OSC 1 and OSC 2.
  - 4th-order PolyBLEP anti-aliasing kernel delivering $>41\text{ dB}$ alias rejection across all octaves up to C7 ($2093\text{ Hz}$).
- **Inter-Oscillator Modulation:**
  - Linear audio-rate Frequency Modulation (FM) modulating the phase of OSC 2 from the output of OSC 1.
  - Four-quadrant Ring Modulation multiplying OSC 1 and OSC 2 with an internal 1-pole 10 Hz highpass DC blocker to prevent integrator bias runaway.
- **6-Mode Wasp XT Filter Section:**
  - Zero-Delay Feedback (ZDF) State-Variable Filter (SVF) modeled via bilinear trapezoidal integration and Topology-Preserving Transform (TPT).
  - Six switchable filter topologies:
    - Mode 0: LP12 (2-pole resonant lowpass, $-13.58\text{ dB/oct}$).
    - Mode 1: LP24 FAT (4-pole cascaded lowpass with modeled non-linear CMOS 4069UB inverter saturation, $-24.48\text{ dB/oct}$).
    - Mode 2: LP+NT (Cascaded 12dB lowpass and notch filter for vocal/formant resonance sweeps).
    - Mode 3: DBL.NT (Double notch filter with staggered center frequencies producing twin attenuation nulls $>18\text{ dB}$).
    - Mode 4: BP24 (Symmetrical 4-pole bandpass with damping factor normalization).
    - Mode 5: HP24 (4-pole highpass filter, $-26.95\text{ dB/oct}$).
  - Keyboard pitch tracking (`KB.TRK`) and bipolar envelope modulation depth (`ENV`, $-1.0\text{ to }+1.0$).
- **Dual LFOs & Envelope Modulation:**
  - Dual multi-waveform LFOs (Saw, Square, Sine, S&H Noise) with 64-bit IEEE double-precision phase accumulators guaranteeing zero timing drift over extended playback.
  - 6 tempo-sync musical divisions (1/32 to 1/1), exponential onset delay ramp (0 to 5s), and note-on phase reset.
  - Flexible modulation routing matrix targeting Pitch, Cutoff, Pulse Width, Oscillator Mix, and Master Amplitude.
  - Dedicated 2-stage Attack-Decay MOD envelope with bipolar depth routing to PW, LFO 1 Amount, OSC 1 Level, or OSC 2 Pitch.
  - Dual 4-stage exponential ADSR envelopes (Amplitude and Filter) with reciprocal UI `LINK` synchronization.
- **Vintage Character Output Circuits:**
  - Asymmetric tanh overdrive distortion generating rich 2nd and 3rd harmonics ($u = x + 0.15 x^2$).
  - 1-pole dynamic tone tilt filter delivering $17.7\text{ dB}$ of spectral tilt shaping.
  - Dual Mode: 5ms Haas psychoacoustic stereo decorrelator delivering expansive stereo width ($r \approx -0.820 < 0.70$) without mono phase cancellation.
  - Analog Mode: Gaussian random-walk pitch drift ($\pm 2.5\text{ cents}$) and free-running non-synchronized initial oscillator phases.
  - Noise Engine: Switchable between authentic 1024-sample Galois LFSR periodic vintage noise table and continuous Gaussian white noise.
  - Master stereo output volume with integrated 10 Hz highpass DC blocker.
- **16-Voice Polyphony & Voice Lifecycle:**
  - Deterministic 4-tier voice allocation: Tier 1 same-pitch retriggering, Tier 2 round-robin free voice acquisition, Tier 3 oldest releasing voice stealing, and Tier 4 oldest held voice (LRU) fallback.
  - 5ms Hann-windowed raised-cosine crossfade de-clicking during voice stealing, guaranteeing $C^1$ derivative continuity and zero audible DC-offset impulse clicks.
- **APVTS Architecture & Skeuomorphic GUI:**
  - 55 automated parameters managed via JUCE `AudioProcessorValueTreeState` with lock-free atomic parameter snapshot polling ($<45\text{ ns}$ extraction latency).
  - Industrial slate-blue skeuomorphic UI (1120x700) featuring custom brushed-metal pointer knobs, LED buttons, horizontal fader, and dual green LCD ADSR displays.
  - Complete XML state serialization and deserialization for DAW project recall.
  - 5 signature factory presets: *Classic Wasp Acid Bass*, *Sting Sync Lead*, *Insectoid Swarm Pad*, *Inharmonic Metallic Percussion*, and *Vintage Drift Lo-Fi Noise Drone*.
- **Comprehensive Verification Suite (Tiers 1–5):**
  - 21 CTest suites executing 400+ unit, boundary, combination, real-world, and adversarial stress tests.
  - 100% test pass rate across all features (F1–F36), guaranteeing zero heap allocations on the audio thread, zero denormals, and zero NaNs/infinities.
