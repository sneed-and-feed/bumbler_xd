# 🎯 Bumbler XD — Golden Test Vectors & Regression Fixtures

**Specification ID**: `BUMBLER-TEST-FIXTURES-001`  
**Compliance**: ISO C++20, IEEE-754 `/fp:precise`, IEC 60268 Audio Equipment  
**Verification Harness**: `tests/golden_vector_tests.cpp` -> Target: `bumbler_golden_vector_tests`

---

## 1. Overview & Purpose

The Golden Test Vector framework guarantees bit-level deterministic regression protection across all core DSP subsystems of **Bumbler XD**. By evaluating rendered audio blocks of the 5 signature factory presets against deterministic golden numbers under fixed pseudo-random generator (PRNG) seeds, the suite enforces:

- **Signal-to-Noise Ratio (SNR)**: $> 120.0\text{ dB}$ across identical deterministic runs.
- **Maximum Sample Delta**: $\Delta_{\max} < 1.0 \times 10^{-4}$ ($< -80\text{ dBFS}$).
- **Zero Denormal Flushes**: Strict FTZ/DAZ subnormal interception ($0$ subnormals permitted).
- **Zero Non-Finite Values**: Guaranteed zero `NaN` or `Inf` floating-point poisoning.
- **Strict Real-Time Safety**: Zero heap allocations (`malloc`, `free`, `new`, `delete`) during audio processing blocks.
- **Spatial Mode Invariance**:
  - Presets with `dualMode == 0.0f` maintain mono phase coherence ($r \ge 0.9999$).
  - Presets with `dualMode == 1.0f` maintain stereo decorrelation ($r < 0.70$).

---

## 2. Standard Test Vector Setup

Every test vector is generated and evaluated under identical, standardized synthesis conditions:

| Parameter | Value | Description |
|---|---|---|
| **Sample Rate** | `44100.0 Hz` | Standard CD quality audio rate |
| **Block Size** | `512 samples` | Typical host real-time buffer size |
| **Vector Length** | `2048 samples` | Exactly 4 audio blocks ($~46.4\text{ ms}$) |
| **Base Seed** | `0x12345678` | Fixed PRNG seed for voice oscillators and analog drift |
| **MIDI Trigger** | Note 60 (C4), Velocity 0.85 | Standard Middle C note trigger |
| **Gate Duration** | 1024 samples | Note Off triggered at sample 1024 for release tail verification |

---

## 3. Preset Inventory & Golden Invariants

| Index | Name | Category | Filter Mode | Dual Mode | Analog Mode | Expected Correlation ($r$) | Peak Range | RMS Range |
|:---:|---|---|:---:|:---:|:---:|:---:|:---:|:---:|
| **0** | Acid Bass | Bass | LP24 | OFF (0.0) | ON (1.0) | $r \ge 0.9999$ (Mono) | $[0.05, 1.05]$ | $[0.01, 0.90]$ |
| **1** | Sync Lead | Lead | LP+NT | ON (1.0) | ON (1.0) | $r < 0.70$ (Wide) | $[0.05, 1.05]$ | $[0.01, 0.90]$ |
| **2** | Swarm Pad | Pad | DBL.NT | ON (1.0) | ON (1.0) | $r < 0.70$ (Wide) | $[0.008, 1.05]$ | $[0.003, 0.90]$ |
| **3** | Percussion | Percussion | BP24 | OFF (0.0) | OFF (0.0) | $r \ge 0.9999$ (Mono) | $[0.05, 1.05]$ | $[0.01, 0.90]$ |
| **4** | Vintage Drone | Atmosphere | HP24 | ON (1.0) | ON (1.0) | $r < 0.70$ (Wide) | $[0.008, 1.05]$ | $[0.003, 0.90]$ |

---

## 4. Fixture Data Files

- [`golden_reference_invariants.json`](golden_reference_invariants.json): Machine-readable schema declaring all exact parameter snapshot entries, tolerance limits, and invariant ranges for all 5 presets.
- `preset_<idx>_<name>.csv`: Comma-separated floating point sample vectors (`sample_index,left,right`) capturing rendered output. If missing, the test suite generates them deterministically on demand.

---

## 5. Running the Golden Vector Verification

```powershell
# Build the test target
cmake --build build --config Release --target bumbler_golden_vector_tests

# Run directly via CTest
ctest --test-dir build -C Release -R GoldenVectorTests --output-on-failure

# Or execute binary directly
./build/tests/Release/bumbler_golden_vector_tests.exe
```
