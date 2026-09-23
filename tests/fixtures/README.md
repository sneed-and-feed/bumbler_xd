# 🎯 Bumbler XD — Golden Test Vectors & Regression Fixtures

**Specification ID**: `BUMBLER-TEST-FIXTURES-001`  
**Compliance**: ISO C++20, IEEE-754 `/fp:precise`, IEC 60268 Audio Equipment  
**Verification Harness**: `tests/golden_vector_tests.cpp` -> Target: `bumbler_golden_vector_tests`

---

## 1. Overview & Purpose

The Golden Test Vector framework guarantees bit-level deterministic regression protection across all core DSP subsystems of **Bumbler XD**. By evaluating rendered audio blocks of factory presets and isolated per-algorithm DSP components against deterministic golden numbers, the suite enforces:

- **Signal-to-Noise Ratio (SNR)**: $> 120.0\text{ dB}$ across identical deterministic runs on the same host, and $> 75.0\text{ dB}$ cross-platform against static CSV reference fixtures (accounting for transcendental runtime differences in `std::tan`, `std::tanh`, `std::exp`, and `std::pow` across CPU architectures like ARM64 vs x86_64).
- **Maximum Sample Delta**: $\Delta_{\max} < 1.0 \times 10^{-4}$ ($< -80\text{ dBFS}$).
- **Zero Denormal Flushes**: Strict FTZ/DAZ subnormal interception ($0$ subnormals permitted).
- **Zero Non-Finite Values**: Guaranteed zero `NaN` or `Inf` floating-point poisoning.
- **Strict Real-Time Safety**: Zero heap allocations (`malloc`, `free`, `new`, `delete`) during audio processing blocks.
- **Spatial Mode Invariance**:
  - Presets with `dualMode == 0.0f` maintain mono phase coherence ($r \ge 0.9999$).
  - Presets with `dualMode == 1.0f` maintain stereo decorrelation ($r < 0.70$).

---

## 2. Standard Test Vector Setup

Every preset test vector is generated and evaluated under standardized synthesis conditions:

| Parameter | Value | Description |
|---|---|---|
| Sample Rate | `44100.0 Hz` | Standard CD quality audio rate |
| Block Size | `512 samples` | Typical host real-time buffer size |
| Vector Length | `2048 samples` | Exactly 4 audio blocks ($~46.4\text{ ms}$) |
| Base Seed | `0x12345678` | Fixed PRNG seed for voice oscillators and analog drift |
| MIDI Trigger | Note 60 (C4), Velocity 0.85 | Standard Middle C note trigger |
| Gate Duration | 1024 samples | Note Off triggered at sample 1024 for release tail verification |

---

## 3. Factory Preset Inventory & Golden Invariants

| Index | Name | Category | Filter Mode | Dual Mode | Analog Mode | Expected Correlation ($r$) | Peak Range | RMS Range |
|:---:|---|---|:---:|:---:|:---:|:---:|:---:|:---:|
| 0 | Acid Bass | Bass | LP24 | OFF (0.0) | ON (1.0) | $r \ge 0.9999$ (Mono) | $[0.05, 1.05]$ | $[0.01, 0.90]$ |
| 1 | Sync Lead | Lead | LP+NT | ON (1.0) | ON (1.0) | $r < 0.70$ (Wide) | $[0.05, 1.05]$ | $[0.01, 0.90]$ |
| 2 | Swarm Pad | Pad | DBL.NT | ON (1.0) | ON (1.0) | $r < 0.70$ (Wide) | $[0.008, 1.05]$ | $[0.003, 0.90]$ |
| 3 | Percussion | Percussion | BP24 | OFF (0.0) | OFF (0.0) | $r \ge 0.9999$ (Mono) | $[0.05, 1.05]$ | $[0.01, 0.90]$ |
| 4 | Vintage Drone | Atmosphere | HP24 | ON (1.0) | ON (1.0) | $r < 0.70$ (Wide) | $[0.008, 1.05]$ | $[0.003, 0.90]$ |

---

## 4. Isolated Per-Algorithm Golden Fixtures

To complement end-to-end preset synthesis verification, the framework provides isolated, unit-level golden CSV fixtures for each critical DSP building block. Each fixture verifies a dedicated mathematical kernel isolated from envelopes, voice allocation, and modulation matrices.

| Fixture File | Algorithm Component | Test Parameters | Target Columns | Primary Equation & Mathlib Reference |
|---|---|---|---|---|
| [`polyblep_saw_golden.csv`](polyblep_saw_golden.csv) | 4th-Order PolyBLEP Saw Wave | $f_0 = 440\text{ Hz}$, $f_s = 44100\text{ Hz}$, $N=100$ | `sample_idx, phase, naive_saw, polyblep_residual, output` | [Section 2.2 & 2.3](../../docs/DSP_ALGORITHMS.md#22-piecewise-4th-order-residual-derivation) — Eq: $y_{\mathrm{saw}}(t) = (2t - 1) - \mathrm{polyBlep4}(t, dt)$ |
| [`polyblep_pulse_golden.csv`](polyblep_pulse_golden.csv) | 4th-Order PolyBLEP Pulse Wave | $f_0 = 440\text{ Hz}$, $f_s = 44100\text{ Hz}$, $w \in \{0.50, 0.25\}$, $N=100$ | `sample_idx, phase, naive_pulse_50, polyblep_residual_50, output_50, naive_pulse_25, polyblep_residual_25, output_25` | [Section 2.3](../../docs/DSP_ALGORITHMS.md#23-waveform-syntheses--discontinuity-corrections) — Eq: $y_{\mathrm{pulse}}(t, w) = y_{\mathrm{naive}} + R(t, dt) - R(t-w, dt)$ |
| [`zdf_svf_lp12_step_golden.csv`](zdf_svf_lp12_step_golden.csv) | ZDF SVF 2-Pole Lowpass Step Response | $f_c = 1000\text{ Hz}$, $R = 0.707$, input $= 1.0$, $f_s = 44100\text{ Hz}$, $N=100$ | `sample_idx, input, lp, bp, hp, notch, output` | [Sections 3.1–3.4](../../docs/DSP_ALGORITHMS.md#32-bilinear-integration--cutoff-pre-warping) — Mode 0 (LP12), $g = \tan(\pi f_c / f_s)$, $d = 1 + g(g+k)$ |
| [`zdf_svf_lp24_fat_step_golden.csv`](zdf_svf_lp24_fat_step_golden.csv) | ZDF SVF 4-Pole Lowpass FAT Step with CMOS Saturation | $f_c = 1000\text{ Hz}$, reso $= 0.75$, input $= 1.0$, $f_s = 44100\text{ Hz}$, $N=100$ | `sample_idx, input, stage1_lp, stage1_sat, stage2_lp, output` | [Sections 3.4 & 3.5](../../docs/DSP_ALGORITHMS.md#mode-1-lp24-fat-4-pole-cascaded-lowpass-with-saturation) — Mode 1 (LP24 FAT), $v_{\mathrm{inter}} = \frac{\tanh(1.2 v_{\mathrm{lp1}})}{1.2}$ |
| [`haas_decorrelation_golden.csv`](haas_decorrelation_golden.csv) | Haas 5ms Psychoacoustic Stereo Decorrelator | Unit impulse $x[0]=1.0$, $D=240$ samples ($5\text{ ms}$ at $f_s = 48000\text{ Hz}$), $N=300$ | `sample_idx, mono_in, delayed, out_left, out_right` | [Section 5.3](../../docs/DSP_ALGORITHMS.md#53-haas-5ms-psychoacoustic-stereo-decorrelator) — Eq: $y_L = \frac{x + 0.6(x - x_{\mathrm{del}})}{\sqrt{2}}$, $y_R = \frac{x_{\mathrm{del}} - 0.6(x - x_{\mathrm{del}})}{\sqrt{2}}$ |

### 4.1 Detailed Algorithm Cross-Links & Invariants

#### 1. PolyBLEP 4th-Order Sawtooth (`polyblep_saw_golden.csv`)
- **Documentation**: [DSP_ALGORITHMS.md §2.2](../../docs/DSP_ALGORITHMS.md#22-piecewise-4th-order-residual-derivation) and [§2.3](../../docs/DSP_ALGORITHMS.md#23-waveform-syntheses--discontinuity-corrections).
- **Core Formula**:
  $$y_{\mathrm{saw}}(t) = (2.0 \cdot t - 1.0) - \mathrm{polyBlep4}(t, dt)$$
- **Boundary Splines**: Evaluated over $t \in [0, 2dt)$ and $t \in (1 - 2dt, 1]$ with optimal coefficients $c = 0.095$, $a_1 = 1.240$, $a_3 = -0.480$, $a_4 = 0.145$.
- **Invariant**: Maximum sample delta $< 10^{-4}$, SNR $> 120.0\text{ dB}$, discontinuity at $t = 0$ cancelled ($y[0] = 0.0$).

#### 2. PolyBLEP 4th-Order Pulse / Square (`polyblep_pulse_golden.csv`)
- **Documentation**: [DSP_ALGORITHMS.md §2.3](../../docs/DSP_ALGORITHMS.md#23-waveform-syntheses--discontinuity-corrections).
- **Core Formula**:
  $$y_{\mathrm{pulse}}(t, w) = y_{\mathrm{naive\_pulse}}(t, w) + \mathrm{polyBlep4}(t, dt) - \mathrm{polyBlep4}(\mathrm{wrap}_{01}(t - w), dt)$$
- **Dual Duty Cycles**: Evaluated at standard square $w = 0.50$ and asymmetric pulse $w = 0.25$.
- **Invariant**: Corrects both positive edge at $t = 0$ and negative edge at $t = w$. Maximum delta $< 10^{-4}$, SNR $> 120.0\text{ dB}$.

#### 3. ZDF SVF 2-Pole Lowpass Step Response (`zdf_svf_lp12_step_golden.csv`)
- **Documentation**: [DSP_ALGORITHMS.md §3.1–3.4](../../docs/DSP_ALGORITHMS.md#31-analog-state-variable-prototype).
- **Core Formulation**: Bilinear pre-warping $g = \tan(\pi f_c / f_s)$, damping factor $k = 2.0 - 1.9 \cdot R$.
- **Implicit Loop Resolution**:
  $$d = 1.0 + g \cdot (g + k)$$
  $$v_{\mathrm{hp}}[n] = \frac{v_{\mathrm{in}}[n] - (g + k) \cdot s_1[n-1] - s_2[n-1]}{d}$$
- **Invariant**: Step response with $v_{\mathrm{in}}[n] = 1.0$ matches trapezoidal integrator states with zero delay. Output SNR $> 120.0\text{ dB}$.

#### 4. ZDF SVF 4-Pole Lowpass FAT Step Response (`zdf_svf_lp24_fat_step_golden.csv`)
- **Documentation**: [DSP_ALGORITHMS.md §3.4 (Mode 1)](../../docs/DSP_ALGORITHMS.md#mode-1-lp24-fat-4-pole-cascaded-lowpass-with-saturation) and [§3.5](../../docs/DSP_ALGORITHMS.md#35-non-linear-cmos-4069ub-inverter-saturation).
- **CMOS 4069UB Saturation Curve**: Inter-stage transfer function:
  $$v_{\mathrm{inter}}[n] = \frac{\tanh(1.2 \cdot v_{\mathrm{lp1}}[n])}{1.2}$$
- **Invariant**: Soft saturates resonance peaks before cascading into Stage 2. Maximum sample delta $< 10^{-4}$, SNR $> 120.0\text{ dB}$.

#### 5. Haas 5ms Psychoacoustic Stereo Decorrelator (`haas_decorrelation_golden.csv`)
- **Documentation**: [DSP_ALGORITHMS.md §5.3](../../docs/DSP_ALGORITHMS.md#53-haas-5ms-psychoacoustic-stereo-decorrelator).
- **Delay Buffer Sizing**: $D = \lfloor 0.005 \cdot f_s + 0.5 \rfloor = 240\text{ samples}$ (at $f_s = 48000\text{ Hz}$).
- **Cross-Matrix Formulation**:
  $$d[n] = x[n] - x_{\mathrm{del}}[n]$$
  $$y_L[n] = \frac{1}{\sqrt{2}} \left(x[n] + 0.6 \cdot d[n]\right), \quad y_R[n] = \frac{1}{\sqrt{2}} \left(x_{\mathrm{del}}[n] - 0.6 \cdot d[n]\right)$$
- **Invariant**: Unit impulse at $n = 0$ emerges at $y_L[0] = 1.05$ (soft-clipping ceiling) and $y_R[0] \approx -0.424264$, followed by the delayed Haas response at $n = 240$ with $y_L[240] \approx -0.424264$ and $y_R[240] = 1.05$. SNR $> 120.0\text{ dB}$.

---

## 5. Fixture Data Files Inventory

- [`golden_reference_invariants.json`](golden_reference_invariants.json): Machine-readable schema declaring all exact parameter snapshot entries, tolerance limits, and invariant ranges for all 5 factory presets.
- `preset_<idx>_<name>.csv`: Comma-separated floating point sample vectors (`sample_index,left,right`) capturing rendered output across 2048 samples.
- `polyblep_saw_golden.csv`: 100 samples of isolated 4th-order PolyBLEP saw wave at 440 Hz ($f_s = 44100\text{ Hz}$).
- `polyblep_pulse_golden.csv`: 100 samples of isolated 4th-order PolyBLEP pulse wave at 440 Hz for PW=0.50 and PW=0.25 ($f_s = 44100\text{ Hz}$).
- `zdf_svf_lp12_step_golden.csv`: 100 samples of isolated ZDF 2-pole Lowpass step response ($f_c = 1000\text{ Hz}$, $R = 0.707$, input = 1.0).
- `zdf_svf_lp24_fat_step_golden.csv`: 100 samples of isolated ZDF 4-pole Lowpass FAT step response with CMOS 4069UB saturation ($f_c = 1000\text{ Hz}$, $\text{resonance} = 0.75$, input = 1.0).
- `haas_decorrelation_golden.csv`: 300 samples of isolated Haas 5ms delay buffer response for a unit impulse input ($f_s = 48000\text{ Hz}$, delay = 240 samples).

---

## 6. Running the Golden Vector Verification

```powershell
# Build the test target
cmake --build build --config Release --target bumbler_golden_vector_tests

# Run directly via CTest
ctest --test-dir build -C Release -R GoldenVectorTests --output-on-failure

# Or execute binary directly
./build/tests/Release/bumbler_golden_vector_tests.exe
```
