# E2E Test Infra: Bumbler XD Synthesizer

**Document ID**: `BUMBLER-TEST-INFRA-001`  
**Target Subsystems**: Native JUCE 8 DSP Core (`bumbler_dsp_core`), `BumblerEngine`, Standalone Executable, VST3 Plugin  
**Standard Compliance**: ISO C++20, IEEE-754 `/fp:precise`, JUCE 8.0.6, IEC 60268 Sound System Equipment  
**Status**: Authoritative Test Infrastructure Specification & 4-Tier Test Framework  

---

## 1. Test Philosophy

The testing methodology for **Bumbler XD** is governed by five core engineering principles:

1. **Opaque-Box, Requirement-Driven Verification**:  
   All test cases are derived directly from the acoustic requirements and mathematical properties defined in `ORIGINAL_REQUEST.md` and `PROJECT.md`. Tests evaluate observable acoustic output, frequency spectra, harmonic content, parameter response, and memory safety without coupling to internal private state.

2. **Zero Tolerance for Facades or Circumvention**:  
   No trivial "always-pass" tests, mock assertions, or tautological checks are permitted. Every test asserts against real DSP computation, true floating-point audio buffers, actual frequency-domain or time-domain metrics, and hardware-level allocation interceptors.

3. **Explicit Authoritative Expected Output Derivation**:  
   For every single test case across all tiers, expected values are derived from authoritative mathematical equations and physical laws:
   - *Filter Roll-Off Slopes*: Closed-form 2-pole (-12 dB/oct) and 4-pole (-24 dB/oct) transfer function magnitudes $|H(j\omega)|$.
   - *Frequency Modulation Sidebands*: Jacobi-Anger expansion and Bessel function distribution ratios $J_n(\beta)$.
   - *Ring Modulation Spectrum*: Analytical sum ($f_1 + f_2$) and difference ($|f_1 - f_2|$) frequencies with zero carrier bleed.
   - *Stereo Decorrelation*: Pearson cross-correlation coefficient $r = \frac{\sum L[n] R[n]}{\sqrt{\sum L[n]^2 \sum R[n]^2}} < 0.70$.
   - *Pitch Drift & Jitter*: Statistical sample variance $\sigma^2 > 0$ across repeated note triggers.
   - *Spectral Energy Distribution*: Discrete single-bin Fourier analysis $H_k = \frac{2}{N}\left|\sum x[n] e^{-j 2\pi k n / N}\right|$.

4. **Multi-Tier Orthogonal Partitioning**:  
   The verification space is structured into four progressive tiers plus adversarial stress testing:
   - **Tier 1 (Feature Coverage)**: Validates primary happy paths and baseline functionality for every feature F1 through F36 ($\ge 5$ tests per feature, $\ge 180$ tests).
   - **Tier 2 (Boundary & Corner Cases)**: Pushes every feature to mathematical extremes ($\ge 5$ tests per feature, $\ge 180$ tests), explicitly verifying multi-rate operation (44.1 kHz to 192 kHz), arbitrary buffer sizes (1 to 8,192 samples), zero heap allocations, denormal flushing, and stability under maximum resonance.
   - **Tier 3 (Cross-Feature Combinations)**: Systematically tests pairwise interactions between concurrent subsystems ($\ge 20$ interaction scenarios).
   - **Tier 4 (Real-World Application Scenarios)**: Exercises full-system production workloads, realistic musical presets, polyphonic chord progressions, velocity sweeps, and high-load stress testing ($\ge 20$ scenario assertions).
   - **Tier 5 (Adversarial Hardening)**: White-box extreme input testing, NaN/Inf poisoning, and parameter chaos sweeps.

5. **Self-Containment & Progressive Testability**:  
   Every test case is self-contained: it initializes its own memory, configures parameters, renders audio deterministically, and validates invariants without dependence on test execution order or external host state.

---

## 2. Feature Inventory Coverage Matrix (N = 36 Features)

All 36 features defined in `PROJECT.md § Feature Inventory` are mapped across the test tiers:

| # | Feature | Subsystem | Milestone | Tier 1 (Min 5) | Tier 2 (Min 5) | Tier 3 (Pairwise) | Tier 4 (Scenarios) |
|---|---------|-----------|:---------:|:--------------:|:--------------:|:-----------------:|:------------------:|
| **F1** | CMake & JUCE 8 Scaffold | Build / Arch | M1 | 5 | 5 | [X] | S01-S05 |
| **F2** | Polyphonic Voice Manager | Voice Allocation | M1 | 5 | 5 | [X] | S01, S04 |
| **F3** | OSC 1 Multi-waveform | Oscillator | M1 | 5 | 5 | [X] | S01, S02, S03 |
| **F4** | OSC 2 Multi-waveform | Oscillator | M1 | 5 | 5 | [X] | S01, S02, S04 |
| **F5** | OSC 3 Aux Sub-Osc | Oscillator | M1 | 5 | 5 | [X] | S01, S05 |
| **F6** | OSC Balance Slider | Mixer | M1 | 5 | 5 | [X] | S01, S03 |
| **F7** | Ring Modulator | Inter-Modulation | M1 | 5 | 5 | [X] | S04, S05 |
| **F8** | Pulse Width Modulation | Modulation | M1 | 5 | 5 | [X] | S02, S03 |
| **F9** | Frequency Modulation (FM) | Inter-Modulation | M1 | 5 | 5 | [X] | S02, S04 |
| **F10** | Velocity Sensitivity | Voice Dynamics | M1 | 5 | 5 | [X] | S01, S04 |
| **F11** | Filter 12dB Lowpass (LP) | 6-Mode Filter | M2 | 5 | 5 | [X] | S01, S03 |
| **F12** | Filter 24dB Lowpass (LP FAT) | 6-Mode Filter | M2 | 5 | 5 | [X] | S01, S02 |
| **F13** | Filter 12dB LP + Notch (LP+NT) | 6-Mode Filter | M2 | 5 | 5 | [X] | S03, S05 |
| **F14** | Filter Double Notch (DBL.NT) | 6-Mode Filter | M2 | 5 | 5 | [X] | S04, S05 |
| **F15** | Filter 24dB Bandpass (BP) | 6-Mode Filter | M2 | 5 | 5 | [X] | S03, S04 |
| **F16** | Filter 24dB Highpass (HP) | 6-Mode Filter | M2 | 5 | 5 | [X] | S02, S05 |
| **F17** | Filter Keyboard Tracking | Filter Modulation | M2 | 5 | 5 | [X] | S01, S02 |
| **F18** | Filter Bipolar Envelope Mod | Filter Modulation | M2 | 5 | 5 | [X] | S01, S03 |
| **F19** | LFO 1 Multi-waveform | LFO Engine | M3 | 5 | 5 | [X] | S02, S03 |
| **F20** | LFO 1 Routing Matrix | LFO Engine | M3 | 5 | 5 | [X] | S02, S04 |
| **F21** | LFO 2 Multi-waveform | LFO Engine | M3 | 5 | 5 | [X] | S01, S03 |
| **F22** | LFO 2 Routing Matrix | LFO Engine | M3 | 5 | 5 | [X] | S03, S05 |
| **F23** | MOD ENV (Attack-Decay) | Envelope Engine | M3 | 5 | 5 | [X] | S02, S04 |
| **F24** | Dual ADSR Envelopes | Envelope Engine | M3 | 5 | 5 | [X] | S01-S05 |
| **F25** | ADSR Link Toggle | Envelope Engine | M3 | 5 | 5 | [X] | S01, S03 |
| **F26** | Output Distortion & Drive | Output Circuits | M4 | 5 | 5 | [X] | S01, S02, S04 |
| **F27** | Distortion Tone Control | Output Circuits | M4 | 5 | 5 | [X] | S01, S04 |
| **F28** | Dual Mode (Voice Doubling) | Output Circuits | M4 | 5 | 5 | [X] | S02, S03, S05 |
| **F29** | Analog Mode (Pitch Drift) | Output Circuits | M4 | 5 | 5 | [X] | S01, S03, S05 |
| **F30** | W.Noise Generator | Output Circuits | M4 | 5 | 5 | [X] | S05 |
| **F31** | Master Output Volume | Output Circuits | M4 | 5 | 5 | [X] | S01-S05 |
| **F32** | APVTS & Automation | Plugin Wrapper | M5 | 5 | 5 | [X] | S01-S05 |
| **F33** | DAW State Recall | Plugin Wrapper | M5 | 5 | 5 | [X] | S01-S05 |
| **F34** | Skeuomorphic Wasp XT GUI | Presentation | M5 | 5 | 5 | [X] | S01-S05 |
| **F35** | E2E Acceptance Test Pass | Test Suite | M6 | 5 | 5 | [X] | All |
| **F36** | Adversarial Hardening | Stress Suite | M6 | 5 | 5 | [X] | All |

**Total Test Coverage**:
- Tier 1: 36 × 5 = 180 tests
- Tier 2: 36 × 5 = 180 tests
- Tier 3: 20 pairwise tests
- Tier 4: 20 musical & scenario stress tests
- **Total**: **400 distinct test cases**

---

## 3. Detailed Tier Specifications

### 3.1 Tier 1: Feature Coverage (N = 180 Tests)

Each feature F1 through F36 contains 5 isolated verification checks:

- **F1 (Scaffold & Build)**: C++20 language features, `/fp:precise` floating-point model, AVX2 vectorization support, static DSP library linkage, CTest runner registration.
- **F2 (Polyphonic Voice Manager)**: Single note allocation, 16-voice polyphony saturation, LRU oldest-voice stealing, note-off release transition, 5ms Hann de-click windowing.
- **F3 (OSC 1 Multi-waveform)**: Sawtooth spectral decay ($1/k$), Square wave odd-harmonic series ($1/k$), Sine wave purity ($THD < 0.05\%$), Noise energy distribution, Coarse/fine pitch tuning accuracy within 0.1 cents.
- **F4 (OSC 2 Multi-waveform)**: Waveform selection (Saw, Square, Sine, Noise), Coarse tuning across -3 to +3 octaves, Fine tuning across -100 to +100 cents, Anti-aliasing suppression above Nyquist, Amplitude stability.
- **F5 (OSC 3 Aux Sub-Osc)**: Square wave generation at 1 octave sub, Sawtooth generation at 1 octave sub, Zero level inaudibility ($<-96$ dBFS), Linear gain scaling up to 1.0, Phase alignment with OSC 1.
- **F6 (OSC Balance Slider)**: Mix at 0.0 delivers pure OSC 1, Mix at 1.0 delivers pure OSC 2, Mix at 0.5 delivers equal sum, Continuous monotonic gain transition, Zero distortion during crossfade.
- **F7 (Ring Modulator)**: Sum frequency generation ($f_1 + f_2$), Difference frequency generation ($|f_1 - f_2|$), Carrier suppression ($>40$ dB), DC offset suppression ($< 10^{-4}$), Enable/disable bypass fidelity.
- **F8 (Pulse Width Modulation)**: 50% duty square symmetry, 25% narrow pulse duty cycle, 75% wide pulse duty cycle, 1% extreme pulse boundary, Continuous PW modulation without DC spike.
- **F9 (Frequency Modulation)**: Carrier at $f_c$ with single sidebands at $f_c \pm f_m$, Second-order sidebands at $f_c \pm 2f_m$, Modulation index scaling, Carrier amplitude reduction matching $J_0(\beta)$, Enable/disable bypass.
- **F10 (Velocity Sensitivity)**: Velocity 127 maximum amplitude, Velocity 1 minimum audible amplitude, Velocity 64 intermediate scaling, Velocity routing to Filter cutoff depth, Zero velocity ignored.
- **F11 (Filter LP 12dB)**: Cutoff at 1 kHz with 0 dB passband, 12 dB/octave attenuation slope above 1 kHz, Resonance peak at cutoff, Phase response through cutoff, Stability under high resonance.
- **F12 (Filter LP FAT 24dB)**: 24 dB/octave attenuation slope above cutoff, 4-pole cascaded steepness, Saturation knee under high resonance, Bass preservation ("FAT" tuning), Monotonic attenuation above cutoff.
- **F13 (Filter LP+NT Cascade)**: Lowpass roll-off above 2 kHz, Notch attenuation dip ($>20$ dB) at center frequency, Combined transfer magnitude, Resonance peak shaping, Transient stability.
- **F14 (Filter Double Notch DBL.NT)**: Identification of first notch null $f_1$, Identification of second notch null $f_2$, Passband peak between twin notches, Attenuation depth ($>18$ dB each), Symmetrical tracking across cutoff sweeps.
- **F15 (Filter BP 24dB)**: Low-end attenuation below 500 Hz ($>12$ dB), High-end attenuation above 2 kHz ($>12$ dB), Unity gain passband at center frequency, Q factor sharpness scaling, Zero DC transmission.
- **F16 (Filter HP 24dB)**: 24 dB/octave attenuation slope below cutoff, 0 dB passband above cutoff, High resonance peaking at cutoff, Rejection of low-frequency rumble, Clean transient step response.
- **F17 (Filter Keyboard Tracking)**: Tracking ratio 1.0 shifts cutoff 1 semitone per MIDI note, Tracking ratio 0.5 shifts cutoff 0.5 semitones per note, Tracking ratio 0.0 leaves cutoff unchanged, Base note C4 (MIDI 60) reference, High-note clamping below Nyquist.
- **F18 (Filter Bipolar Envelope Mod)**: Positive envelope sweeps cutoff upward, Negative envelope sweeps cutoff downward, Zero modulation leaves cutoff at base frequency, Dynamic envelope tracking, Logarithmic frequency clamping.
- **F19 (LFO 1 Multi-waveform)**: Sine wave pure oscillation, Sawtooth linear ramp, Square alternating polarity, Sample & Hold pseudo-random noise, Frequency range 0.05 Hz to 30.0 Hz.
- **F20 (LFO 1 Routing Matrix)**: Routing to OSC 1+2 pitch creates vibrato, Routing to Filter cutoff creates wah/wobble, Routing to PW creates pulse width sweep, Non-selected destinations remain unmodulated, Amount control scales depth linearly.
- **F21 (LFO 2 Multi-waveform)**: Sine, Saw, Square, and Noise shapes, Rate control from 0.05 Hz to 30 Hz, Delay onset stage ramp-up, Phase consistency, Smooth restart.
- **F22 (LFO 2 Routing Matrix)**: Routing to OSC 1 pitch, Routing to OSC Mix balance, Routing to Master Amplitude (tremolo), Mutual exclusivity of unselected targets, Linear depth control.
- **F23 (MOD ENV Attack-Decay)**: Attack stage exponential rise, Decay stage exponential fall, Bipolar modulation amount ($\pm 1.0$), Routing to PW, Routing to OSC 2 pitch.
- **F24 (Dual ADSR Envelopes)**: Amp Attack time accuracy (1ms to 10s), Amp Decay & Sustain level hold, Amp Release tail time, Filter ADSR independent stage timing, Exponential decay curves.
- **F25 (ADSR Link Toggle)**: Link ON copies Amp edits to Filter, Link ON copies Filter edits to Amp, Link OFF decouples envelopes, Prevention of circular update loops, Real-time parameter sync.
- **F26 (Output Distortion & Drive)**: Drive toggle bypasses/activates saturation, Drive amount increases harmonic distortion (THD), Asymmetric soft-clipping knee, Dynamic range compression, Peak level bounded below $+1.05$.
- **F27 (Distortion Tone Control)**: Tone at 0.0 darkens spectrum (attenuates $>2$ kHz by $>12$ dB), Tone at 1.0 brightens spectrum (boosts $>3$ kHz), Tone at 0.5 flat neutral tilt, Smooth continuous sweep, Zero phase explosion.
- **F28 (Dual Mode Voice Doubling)**: Detuned unison voice generation, Left/Right phase decorrelation ($r < 0.70$), Chorus beating periodicity, Stereo spread width, Output gain compensation.
- **F29 (Analog Mode Pitch Drift)**: Random walk pitch drift across notes ($\pm 1.5$ to $4.0$ cents), Free-running oscillator phase on trigger, Non-zero statistical variance across identical notes, Deterministic seed reproducibility, Stability under rapid note retriggers.
- **F30 (W.Noise Generator)**: Vintage fixed-table mode has discrete spectral periodicity, White noise mode has flat Gaussian spectrum, Crest factor comparison, Table loop boundary smoothness, Toggle transitions without clicks.
- **F31 (Master Output Volume)**: Level 1.0 delivers unity gain, Level 0.0 delivers total silence ($-\infty$ dBFS), Logarithmic fader law, DC blocker prevents DC buildup, Soft limiter prevents hard digital clipping.
- **F32 (APVTS & Automation)**: All 55 parameter IDs registered, Range clamping and normalisation, Thread-safe lock-free atomic parameter caching, Sample-accurate parameter interpolation, Zero audio dropouts during automation.
- **F33 (DAW State Recall)**: XML serialization contains all 55 parameter values, Deserialization restores exact floating-point values, Corrupt XML fallback to default state, Binary chunk compatibility, Preset state independence.
- **F34 (Skeuomorphic Wasp XT GUI)**: UI instantiation without headless failure, Parameter attachment binding, LookAndFeel knob rendering bounds, LCD display curve drawing, Mix slider horizontal coordinate mapping.
- **F35 (E2E Acceptance Test Pass)**: Execution of all 6 acceptance criteria from `ORIGINAL_REQUEST.md`, Master suite passes with exit code 0, Quantitative metric validation, Zero memory leaks, Execution time under 30 seconds.
- **F36 (Adversarial Hardening)**: Rapid MIDI note bursts (100 notes/sec), Cutoff swept to $f_s/2$, Infinite resonance burst stability, Multi-rate audio stability (44.1k to 192k), Extreme buffer size scaling (1 to 8,192 samples).

---

### 3.2 Tier 2: Boundary & Corner Cases (N = 180 Tests)

Tier 2 stresses the engine at physical, computational, and numerical limits:

1. **Multi-Rate Invariance**:  
   Audio processing is verified at sample rates: 44.1 kHz, 48 kHz, 88.2 kHz, 96 kHz, 176.4 kHz, 192 kHz. Filters, envelopes, and LFOs dynamically scale time constants:
   $$\alpha = 1 - e^{-\frac{1}{\tau \cdot f_s}}$$

2. **Buffer Size Stress**:  
   Processing blocks of size $1, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192$ samples. Verifies zero buffer overrun, zero pointer miscalculation, and correct voice state preservation across arbitrary block divisions.

3. **Zero Heap Allocations**:  
   Monitored via overloaded global `operator new`, `operator new[]`, `operator delete`, and `operator delete[]` with atomic counters. Any heap allocation occurring during `renderBlock()` or `process()` triggers an immediate test failure.

4. **Denormal Suppression & FTZ/DAZ Protection**:  
   Input signals decaying below $10^{-15}$ must be flushed to bit-exact $0.0\text{f}$ via hardware FTZ/DAZ registers (`_mm_setcsr(0x8040)`) and software guards, preventing x86 floating-point microcode stall traps.

5. **Numerical Stability & Boundedness**:  
   Output samples are bounded within $[-1.05, +1.05]$ via soft-clipping saturation. Output buffers are scanned with `std::isnan()`, `std::isinf()`, and `std::fpclassify() == FP_SUBNORMAL` asserting zero abnormal values.

6. **Parameter Clamping & Extremes**:  
   Parameters set to minimum, maximum, and out-of-bounds values (e.g. Cutoff $= 0$ Hz, $100$ kHz; Resonance $= -1.0, +10.0$; PW $= -0.5, +1.5$; Drive $= 100.0$) must be cleanly clamped without audio blowups or exceptions.

---

### 3.3 Tier 3: Cross-Feature Combinations (N = 20 Pairwise Scenarios)

Pairwise interaction scenarios verify that concurrent subsystems operate without unwanted non-linear artifacts or phase destruction:

1. `T3_01`: Ring Modulator (F7) + High-Resonance 24dB LP FAT Filter (F12)
2. `T3_02`: Frequency Modulation (F9) + Dual Mode Stereo Detuning (F28)
3. `T3_03`: Pulse Width Modulation (F8) + MOD ENV Sweep (F23)
4. `T3_04`: LFO 1 Tempo Sync (F19) + LFO 1 Key Reset (F19) + Cutoff Routing (F20)
5. `T3_05`: Filter Keyboard Tracking (F17) + Analog Mode Pitch Drift (F29)
6. `T3_06`: Dual ADSR Envelope Link (F25) + Velocity Modulation (F10)
7. `T3_07`: Output Overdrive (F26) + Tone Control Sweeps (F27) + Master DC Blocker (F31)
8. `T3_08`: OSC 3 Sub-Oscillator (F5) + DBL.NT Double Notch Filter (F14)
9. `T3_09`: W.Noise Table Mode (F30) + 24dB Highpass Filter (F16)
10. `T3_10`: 16-Voice Polyphony Saturation (F2) + Dual Mode (F28) CPU Stress
11. `T3_11`: Bipolar Filter Envelope Mod (F18) + Extreme Cutoff Clamping at Nyquist
12. `T3_12`: OSC Mix Balance Sweeps (F6) + Ring Mod Carrier Bleed Rejection (F7)
13. `T3_13`: LFO 2 Amplitude Tremolo (F22) + Amp ADSR Release Tail (F24)
14. `T3_14`: FM Modulator (F9) + OSC 1 Coarse Detune (+3 Octaves)
15. `T3_15`: Analog Free-Running Phase (F29) + Fast Legato Voice Stealing (F2)
16. `T3_16`: Distortion Drive Saturation (F26) + 12dB LP+NT Notch Resonance (F13)
17. `T3_17`: LFO 1 PW Modulation (F20) + Minimum Pulse Width Limit (F8)
18. `T3_18`: MOD ENV to Pitch (F23) + Dual Mode Chorus Beating (F28)
19. `T3_19`: APVTS Continuous Automation Sweeps (F32) + Real-Time Safety Guard (F36)
20. `T3_20`: DAW State Restore During Audio Playback (F33) + Voice Silence Ramp

---

### 3.4 Tier 4: Real-World Application Scenarios (N = 20 Musical & Stress Tests)

Tier 4 validates full-system performance under realistic studio workloads:

1. **Preset S01: "Classic Wasp Acid Bass"**:
   - OSC 1 Saw, OSC 2 Saw (-1 Oct, fine detuned +5 cents), LP FAT 24dB filter at 650 Hz with 75% resonance, fast punchy Amp/Filter ADSR (Attack 2ms, Decay 180ms, Sustain 10%, Release 80ms), moderate Output Drive (35%).
   - Verified: Low-end punch preservation, aggressive resonant sweep, bounded peak headroom.
2. **Preset S02: "Sting Sync Lead"**:
   - OSC 1 Square, OSC 2 Square with FM engaged, LFO 1 modulating Cutoff at 4.5 Hz, Dual Mode active for stereo width.
   - Verified: Bessel sideband clarity, rich stereo imaging ($r < 0.65$), singing sustain.
3. **Preset S03: "Insectoid Swarm Pad"**:
   - OSC 1 Sine, OSC 2 Saw (+1 Oct), OSC 3 Sub Saw, LP+NT filter with moderate resonance, slow LFO 1 sweeping PW, long Amp ADSR (Attack 1.2s, Release 2.5s).
   - Verified: Vowel-like notch movement, smooth envelope tails, zero denormals during release.
4. **Preset S04: "Inharmonic Metallic Percussion"**:
   - OSC 1 Sine (440 Hz), OSC 2 Sine (277 Hz), Ring Modulator at 100%, 24dB Bandpass filter at 1.2 kHz, MOD ENV snapping pitch down.
   - Verified: Crisp clangorous sidebands ($163\text{ Hz}, 717\text{ Hz}$), zero DC drift, fast decay gating.
5. **Preset S05: "Vintage Drift Lo-Fi Noise Drone"**:
   - W.Noise generator in vintage table mode, HP 24dB filter at 150 Hz, Analog mode enabled with maximum drift, Overdrive Tone at 0.25 (dark/muddy).
   - Verified: Authentic cyclic noise undertones, randomized organic pitch drift, warm saturation.
6. **Stress Workload: 1,000-Note Polyphonic Churn**:
   - Rapid MIDI Note-On and Note-Off events injected at 120 notes per second across 16 voices.
   - Verified: Zero voice starvation, clean Hann window de-click voice stealing, zero memory leaks.
7. **Stress Workload: 1,000,000-Sample Continuous Burn-In**:
   - Continuous audio rendering over 1,000,000 samples ($~22.7$ seconds at 44.1 kHz) with dynamic parameter automation.
   - Verified: Bounded output, zero NaNs, memory allocation count remains bit-exact 0.

---

## 4. Mathematical Oracles & Validation Recipes

### 4.1 Filter Roll-Off Slope Verification
To verify `ORIGINAL_REQUEST.md` acceptance criteria:
1. Configure filter at $f_c = 1000\text{ Hz}$, $Q = 0.7071$, sample rate $f_s = 48000\text{ Hz}$.
2. Inject steady-state sine probes at frequencies $f = [250, 500, 1000, 2000, 4000]\text{ Hz}$.
3. Discard initial 2,048 transient samples; calculate steady-state peak amplitude $A(f)$ over 4,096 samples.
4. Convert to decibels: $L(f) = 20\log_{10}(A(f))$.
5. Assertions:
   - **LP FAT (24 dB/oct)**: $L(2000) - L(1000) \in [-27.0, -21.0]\text{ dB}$; $L(4000) - L(1000) \in [-52.0, -44.0]\text{ dB}$.
   - **LP 12dB**: $L(2000) - L(1000) \in [-14.0, -10.0]\text{ dB}$; $L(4000) - L(1000) \in [-27.0, -21.0]\text{ dB}$.
   - **BP 24dB**: Passband peak at $1000\text{ Hz} \ge -1.0\text{ dB}$; $L(500) < -10.0\text{ dB}$; $L(2000) < -10.0\text{ dB}$.
   - **HP 24dB**: $L(500) - L(1000) \in [-27.0, -21.0]\text{ dB}$; $L(250) - L(1000) \in [-52.0, -44.0]\text{ dB}$.
   - **DBL.NT**: Sweep 100 logarithmically spaced frequencies from $200\text{ Hz}$ to $5000\text{ Hz}$. Detect two distinct local minima with attenuation deeper than $-18.0\text{ dB}$ separated by a passband peak.

### 4.2 Inter-Oscillator Modulation Verification
1. **Ring Modulation**:
   - OSC 1 set to $400\text{ Hz}$ sine, OSC 2 set to $100\text{ Hz}$ sine.
   - Render 4,096 samples. Compute Fourier bin powers at:
     - Difference: $|400 - 100| = 300\text{ Hz}$
     - Sum: $400 + 100 = 500\text{ Hz}$
     - Carrier fundamentals: $100\text{ Hz}$ and $400\text{ Hz}$
   - Assert: $P(300) + P(500) > 0.90 \cdot P_{\text{total}}$; carrier bleed $P(100), P(400) < -40\text{ dBFS}$; DC offset $|\bar{x}| < 1.0\times 10^{-4}$.
2. **Frequency Modulation (FM)**:
   - Carrier OSC 2 at $1000\text{ Hz}$, Modulator OSC 1 at $200\text{ Hz}$.
   - Sideband frequencies at $f_c \pm k f_m$ ($800\text{ Hz}, 1200\text{ Hz}, 600\text{ Hz}, 1400\text{ Hz}$).
   - Assert: Increasing FM amount monotonically raises power in first-order sidebands ($800, 1200\text{ Hz}$) matching $J_1(\beta)$.

### 4.3 Character Circuits Verification
1. **Dual Mode Stereo Widening**:
   - Render 4,096 samples with Dual Mode OFF. Assert $L[n] == R[n]$ (correlation $r = 1.000$).
   - Enable Dual Mode. Compute Pearson correlation:
     $$r = \frac{\sum_{n=0}^{N-1} L[n] R[n]}{\sqrt{\sum L[n]^2 \sum R[n]^2}}$$
   - Assert $r < 0.70$ and verify amplitude beating periodicity matching voice detuning.
2. **Analog Mode Pitch Drift**:
   - Trigger 10 identical MIDI Note 60 events with Analog Mode OFF. Assert identical waveform output ($\sigma^2 = 0.0$).
   - Enable Analog Mode. Measure zero-crossing intervals across the 10 triggers. Assert pitch variance $\sigma^2 > 0.005$ semitones$^2$ and non-identical initial phase.
3. **Output Overdrive & Tone Control**:
   - Drive at 0.0 vs Drive at 1.0: Assert THD increases by $>15\text{ dB}$.
   - Tone at 0.0 (muddy) vs Tone at 1.0 (bright): Energy above 2 kHz is attenuated by $>15\text{ dB}$ at Tone 0.0 relative to Tone 1.0.
4. **Real-Time Safety & Memory**:
   - Execute 1,000 blocks with `operator new` hook active. Assert `gAllocationCount == 0`.
   - Assert zero occurrences of `std::isnan()` or `std::isinf()`.

---

## 5. Test Suite Architecture in `tests/`

The test suite is organized into modular native C++20 test runners registered with CMake and CTest:

1. `tests/filter_accuracy_tests.cpp`: Frequency response sweeps and roll-off slope validations for all 6 filter modes.
2. `tests/modulation_tests.cpp`: Ring modulation sum/difference, FM Bessel sidebands, LFO routing matrix, and envelope curves.
3. `tests/character_circuit_tests.cpp`: Dual mode stereo decorrelation, Analog pitch drift variance, Distortion saturation, Tone shaping, and W.Noise spectra.
4. `tests/realtime_safety_tests.cpp`: Real-time heap allocation interceptor, denormal FTZ/DAZ protection, and multi-rate/block-size stability.
5. `tests/e2e_headless_suite.cpp`: Master acceptance test runner aggregating all tiers and certifying compliance with `ORIGINAL_REQUEST.md`.

---

## 6. Verification Method

To execute the complete E2E test suite:

```powershell
# 1. Configure CMake with CTest enabled
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DBUMBLER_BUILD_TESTS=ON

# 2. Build test executables
cmake --build build --config Release --target bumbler_e2e_tests --parallel

# 3. Run CTest
ctest --test-dir build -C Release --output-on-failure
```
