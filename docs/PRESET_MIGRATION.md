# 🐝 Legacy Wasp & Wasp XT Preset Migration Guide & Interoperability Architecture

This document provides a comprehensive technical reference for migrating legacy preset patches from the discontinued **Wasp** and **Wasp XT** synthesizers to **Bumbler XD**. It covers legacy file container structures, mathematical parameter domain transformations, APVTS mapping specifications, CLI migration tooling, and the legal clean-room interoperability architecture.

---

## 📑 Table of Contents

1. [Architectural Overview](#1-architectural-overview)
2. [Supported Legacy Container Formats](#2-supported-legacy-container-formats)
   - [2.1 VST 2.4 Preset & Bank Files (.fxp, .fxb)](#21-vst-24-preset--bank-files-fxp-fxb)
   - [2.2 FL Studio State Files (.fst)](#22-fl-studio-state-files-fst)
   - [2.3 FL Studio Project Files (.flp)](#23-fl-studio-project-files-flp)
3. [Parameter Mapping Blueprint (55 APVTS IDs)](#3-parameter-mapping-blueprint-55-apvts-ids)
   - [3.1 Oscillators & Voice Architecture (14 Parameters)](#31-oscillators--voice-architecture-14-parameters)
   - [3.2 6-Mode Zero-Delay Feedback Filter (5 Parameters)](#32-6-mode-zero-delay-feedback-filter-5-parameters)
   - [3.3 Envelope Generators (13 Parameters)](#33-envelope-generators-13-parameters)
   - [3.4 Dual Low-Frequency Oscillators (16 Parameters)](#34-dual-low-frequency-oscillators-16-parameters)
   - [3.5 Character Circuits & Output Section (7 Parameters)](#35-character-circuits--output-section-7-parameters)
4. [Mathematical Transformation Curves](#4-mathematical-transformation-curves)
   - [4.1 Logarithmic Cutoff Frequency Scaling](#41-logarithmic-cutoff-frequency-scaling)
   - [4.2 Exponential Time Curve Scaling](#42-exponential-time-curve-scaling)
   - [4.3 Bipolar Envelope & Mod Amount Conversion](#43-bipolar-envelope--mod-amount-conversion)
5. [CLI Tooling & Usage Workflows](#5-cli-tooling--usage-workflows)
   - [5.1 Installation & Dependencies](#51-installation--dependencies)
   - [5.2 CLI Command Flags](#52-cli-command-flags)
   - [5.3 Practical Workflows & Examples](#53-practical-workflows--examples)
6. [Legal & Clean-Room Interoperability Architecture](#6-legal--clean-room-interoperability-architecture)
   - [6.1 Clean-Room Engineering Methodology](#61-clean-room-engineering-methodology)
   - [6.2 United States Legal Precedents](#62-united-states-legal-precedents)
   - [6.3 European Union Software Directive](#63-european-union-software-directive)
   - [6.4 Commercial Royalty-Free Status](#64-commercial-royalty-free-status)

---

## 1. Architectural Overview

The classic EDP Wasp (1978) and its software implementations by Richard Hoffmann (Wasp and Wasp XT) established a distinct synthesis architecture:
- Dual digital oscillators with anti-aliasing tables and pulse width modulation.
- A distinctive sub-oscillator and audio-rate FM / Ring Modulation.
- A signature multi-mode filter featuring double-notch and lowpass+notch formant modes.
- Dual LFOs with flexible modulation routing.
- Analog character circuits (saturation drive, dual detune mode, analog drift, and vintage noise).

**Bumbler XD** preserves this synthesis heritage through modern C++20 zero-delay feedback (ZDF) state-variable filters (SVF) and a lock-free, 55-parameter `AudioProcessorValueTreeState` (APVTS). 

The **Preset Migration Importer** (`scripts/import_wasp_presets.py`) provides automated binary parsing and mathematical parameter transformation to port legacy patch data into Bumbler XD's XML and C++ preset formats without requiring the original, discontinued 32-bit plugins.

---

## 2. Supported Legacy Container Formats

The migration importer supports four primary legacy container formats:

```text
Legacy Containers                   Bumbler XD Outputs
┌─────────────────────────┐         ┌───────────────────────────────┐
│ .fxp (VST 2.4 Preset)   │         │ JUCE APVTS XML Preset         │
│ .fxb (VST 2.4 Bank)     │ ──────> │ <BumblerXD ...>               │
│ .fst (FL Studio State)  │         │   <PARAM id="..." value="..."/>│
│ .flp (FL Studio Project)│         │ </BumblerXD>                  │
└─────────────────────────┘         ├───────────────────────────────┤
                                    │ C++ PresetDefinition          │
                                    │ (PresetParameters.h)          │
                                    └───────────────────────────────┘
```

### 2.1 VST 2.4 Preset & Bank Files (.fxp, .fxb)
VST 2.4 binary presets adhere to Steinberg's `CcnK` container specification:

| Field | Offset | Type | Description |
|---|---|---|---|
| `chunkMagic` | 0 | `char[4]` | Container magic (`'CcnK'`, `0x43636E4B`). |
| `byteSize` | 4 | `uint32_be` | Total size of subsequent data chunk. |
| `fxMagic` | 8 | `char[4]` | Sub-format identifier: `'FxCk'` (regular preset), `'FPCh'` (chunk preset), `'FxBk'` (regular bank), `'FBCh'` (chunk bank). |
| `version` | 12 | `int32_be` | VST format version (typically 1 or 2). |
| `fxID` | 16 | `char[4]` | 4-character plugin identifier (e.g. `'Wasp'`, `'WsXT'`). |
| `fxVersion` | 20 | `int32_be` | Plugin internal version integer. |
| `numParams` | 24 | `int32_be` | Count of automatable parameters. |
| `prName` | 28 | `char[28]` | Null-padded preset title. |
| `params` / `chunk` | 56 | Variable | 32-bit big-endian float array or opaque chunk payload. |

The importer handles both standard parameter streams (`FxCk`, `FxBk`) and chunk-based states (`FPCh`, `FBCh`), extracting parameter arrays using automated endianness detection.

### 2.2 FL Studio State Files (.fst)
FL Studio `.fst` files save generator or effect channel states. The importer supports:
1. **RIFF Containers**: Files beginning with `'RIFF'` holding `'FS_G'` (generator state) or `'FS_C'` (channel state) headers, scanning nested subchunks (`'data'`, `'plug'`, `'stat'`).
2. **IFF Containers**: Files beginning with `'FORM'` using big-endian chunk sizing.
3. **Embedded VST Chunks**: States wrapping native VST `CcnK` blocks.
4. **Raw State Dumps**: Unwrapped 32-bit float or byte-quantized knob buffers.

### 2.3 FL Studio Project Files (.flp)
FL Studio `.flp` project files are parsed natively:
1. Validates the `'FLhd'` project header (channel count, PPQ/beat division).
2. Traverses the `'FLdt'` event stream using variable-length integer (LEB128/varint) decoding for events `>= 192` (`0xC0`).
3. Tracks channel names and plugin identities (event `0xC4`, `FLP_PluginName` / `FLP_ChanName`).
4. Detects event `0xC5` (`197`, `FLP_PluginData`), identifying instances of Wasp or Wasp XT and extracting their serialized synthesis states.

---

## 3. Parameter Mapping Blueprint (55 APVTS IDs)

Bumbler XD organizes its synthesizer parameters into 5 logical functional groups:

### 3.1 Oscillators & Voice Architecture (14 Parameters)

| APVTS Parameter ID | Type | Range | Default | Unit | Legacy Mapping & Semantic Description |
|---|---|---|---|---|---|
| `osc1Waveform` | Choice | `0..3` | `0` | - | `0: Saw`, `1: Square`, `2: Sine`, `3: Noise`. Discrete step conversion. |
| `osc1Octave` | Float | `-3.0..3.0` | `0.0` | `oct` | Coarse tuning. Transposes oscillator 1 by octaves in integer increments. |
| `osc1Fine` | Float | `-1.0..1.0` | `0.0` | `st` | Fine tuning in semitones (-100 cents to +100 cents). |
| `osc2Waveform` | Choice | `0..3` | `0` | - | `0: Saw`, `1: Square`, `2: Sine`, `3: Noise`. |
| `osc2Octave` | Float | `-3.0..3.0` | `0.0` | `oct` | Coarse tuning for oscillator 2. |
| `osc2Fine` | Float | `-1.0..1.0` | `0.0` | `st` | Fine tuning for oscillator 2. |
| `oscMix` | Float | `0.0..1.0` | `0.5` | `%` | Crossfader between Osc 1 (`0.0`) and Osc 2 (`1.0`). |
| `osc3Waveform` | Choice | `0..1` | `0` | - | Sub-oscillator shape: `0: Square`, `1: Saw`. |
| `osc3Level` | Float | `0.0..1.0` | `0.0` | `%` | Sub-oscillator output level (inaudible at `0.0`). |
| `ringModMix` | Float | `0.0..1.0` | `0.0` | `%` | Ring modulation blend between Osc 1 and Osc 2. |
| `pulseWidth` | Float | `0.01..0.99`| `0.50`| `%` | Duty cycle for Square waveforms (`0.50` = symmetric square). |
| `fmAmount` | Float | `0.0..1.0` | `0.0` | `%` | Audio-rate Frequency Modulation (Osc 1 modulates Osc 2). |
| `velToAmp` | Float | `0.0..1.0` | `0.50`| `%` | Dynamic velocity scaling applied to Amplitude Envelope. |
| `velToFilter` | Float | `0.0..1.0` | `0.50`| `%` | Dynamic velocity scaling applied to Filter Envelope. |

### 3.2 6-Mode Zero-Delay Feedback Filter (5 Parameters)

| APVTS Parameter ID | Type | Range | Default | Unit | Legacy Mapping & Semantic Description |
|---|---|---|---|---|---|
| `filterMode` | Choice | `0..5` | `1` | - | `0: LP12`, `1: LP24`, `2: LP+NT`, `3: DBL.NT`, `4: BP24`, `5: HP24`. |
| `filterCutoff` | Float | `20.0..20000.0` | `1200.0` | `Hz` | Logarithmically scaled base cutoff frequency: $f = 20 \cdot 1000^{\text{norm}}$. |
| `filterResonance` | Float | `0.0..1.0` | `0.20` | `%` | Filter resonance / Q factor (self-oscillates above ~`0.85`). |
| `filterKbTrack` | Float | `0.0..1.0` | `0.50` | `%` | Keyboard pitch tracking depth (1:1 tracking at `1.0`). |
| `filterEnvAmount` | Float | `-1.0..1.0` | `0.0` | `%` | Bipolar filter envelope modulation depth (-100% to +100%). |

### 3.3 Envelope Generators (13 Parameters)

| APVTS Parameter ID | Type | Range | Default | Unit | Legacy Mapping & Semantic Description |
|---|---|---|---|---|---|
| `ampAttack` | Float | `0.001..10.0` | `0.01` | `s` | Amplitude Envelope Attack time (1 ms to 10 seconds). |
| `ampDecay` | Float | `0.001..10.0` | `0.30` | `s` | Amplitude Envelope Decay time. |
| `ampSustain` | Float | `0.0..1.0` | `0.80` | `%` | Amplitude Envelope Sustain level. |
| `ampRelease` | Float | `0.001..10.0` | `0.30` | `s` | Amplitude Envelope Release time. |
| `filterAttack` | Float | `0.001..10.0` | `0.05` | `s` | Filter Envelope Attack time. |
| `filterDecay` | Float | `0.001..10.0` | `0.50` | `s` | Filter Envelope Decay time. |
| `filterSustain` | Float | `0.0..1.0` | `0.50` | `%` | Filter Envelope Sustain level. |
| `filterRelease` | Float | `0.001..10.0` | `0.40` | `s` | Filter Envelope Release time. |
| `envLink` | Bool | `0.0 / 1.0` | `0.0` | - | Links Amp & Filter envelope decay/sustain controls. |
| `modAttack` | Float | `0.001..5.0` | `0.05` | `s` | Modulation Envelope Attack time (1 ms to 5 seconds). |
| `modDecay` | Float | `0.001..10.0` | `0.50` | `s` | Modulation Envelope Decay time. |
| `modAmount` | Float | `-1.0..1.0` | `0.0` | `%` | Bipolar Modulation Envelope depth. |
| `modTarget` | Choice | `0..3` | `0` | - | `0: PW`, `1: Lfo1Amt`, `2: Osc1Level`, `3: Osc2Pitch`. |

### 3.4 Dual Low-Frequency Oscillators (16 Parameters)

| APVTS Parameter ID | Type | Range | Default | Unit | Legacy Mapping & Semantic Description |
|---|---|---|---|---|---|
| `lfo1Waveform` | Choice | `0..3` | `2` | - | `0: Saw`, `1: Square`, `2: Sine`, `3: Noise (S&H)`. |
| `lfo1Rate` | Float | `0.05..30.0` | `2.0` | `Hz` | Exponentially scaled frequency (0.05 Hz to 30.0 Hz). |
| `lfo1Delay` | Float | `0.0..5.0` | `0.0` | `s` | Pre-modulation delay onset time. |
| `lfo1Sync` | Bool | `0.0 / 1.0` | `0.0` | - | Host tempo sync enable. |
| `lfo1SyncDiv` | Choice | `0..5` | `3` | - | `0: 1/32`, `1: 1/16`, `2: 1/8`, `3: 1/4`, `4: 1/2`, `5: 1/1`. |
| `lfo1KeyReset` | Bool | `0.0 / 1.0` | `1.0` | - | Phase re-triggering on note-on. |
| `lfo1Amount` | Float | `0.0..1.0` | `0.0` | `%` | Modulation depth. |
| `lfo1Target` | Choice | `0..2` | `1` | - | `0: Osc12Pitch`, `1: FilterCutoff`, `2: PulseWidth`. |
| `lfo2Waveform` | Choice | `0..3` | `2` | - | `0: Saw`, `1: Square`, `2: Sine`, `3: Noise (S&H)`. |
| `lfo2Rate` | Float | `0.05..30.0` | `1.0` | `Hz` | Exponentially scaled frequency. |
| `lfo2Delay` | Float | `0.0..5.0` | `0.0` | `s` | Pre-modulation delay onset time. |
| `lfo2Sync` | Bool | `0.0 / 1.0` | `0.0` | - | Host tempo sync enable. |
| `lfo2SyncDiv` | Choice | `0..5` | `4` | - | Musical division (default `1/2`). |
| `lfo2KeyReset` | Bool | `0.0 / 1.0` | `1.0` | - | Phase re-triggering on note-on. |
| `lfo2Amount` | Float | `0.0..1.0` | `0.0` | `%` | Modulation depth. |
| `lfo2Target` | Choice | `0..2` | `0` | - | `0: Osc1Pitch`, `1: OscMix`, `2: MasterAmp`. |

### 3.5 Character Circuits & Output Section (7 Parameters)

| APVTS Parameter ID | Type | Range | Default | Unit | Legacy Mapping & Semantic Description |
|---|---|---|---|---|---|
| `driveEnabled` | Bool | `0.0 / 1.0` | `0.0` | - | Engages the non-linear waveshaping distortion circuit. |
| `driveAmount` | Float | `0.0..1.0` | `0.30` | `%` | Drive saturation gain. |
| `driveTone` | Float | `0.0..1.0` | `0.50` | `%` | Pre/post distortion tilt equalization (`0.0` dark, `1.0` bright). |
| `dualMode` | Bool | `0.0 / 1.0` | `0.0` | - | Voice doubling with stereo Haas decorrelation. |
| `analogMode` | Bool | `0.0 / 1.0` | `0.0` | - | Free-running oscillator phase & analog drift. |
| `wNoiseMode` | Choice | `0..1` | `0` | - | `0: Vintage Table Noise`, `1: True White Noise`. |
| `masterVolume` | Float | `0.0..1.0` | `0.80` | `%` | Master output attenuation. |

---

## 4. Mathematical Transformation Curves

Legacy synthesizers typically expose parameters as normalized floating-point numbers in the range $[0.0, 1.0]$ or 8-bit integers $[0, 255]$. Bumbler XD's importer applies rigorous mathematical domain mappings:

### 4.1 Logarithmic Cutoff Frequency Scaling
Human pitch and frequency perception are logarithmic. A linear mapping causes 90% of the knob travel to bunch up in the ultra-high frequencies. Bumbler XD employs logarithmic scaling:

```math
f(\text{norm}) = 20.0 \cdot \left( \frac{20000.0}{20.0} \right)^{\text{norm}} = 20.0 \cdot 1000.0^{\text{norm}} \quad (\text{Hz})
```

- When $\text{norm} = 0.0$: $f = 20.0\text{ Hz}$ (Sub-audible floor).
- When $\text{norm} = 0.5$: $f = 20.0 \cdot \sqrt{1000} \approx 632.45\text{ Hz}$ (Acoustic center).
- When $\text{norm} = 1.0$: $f = 20000.0\text{ Hz}$ (Nyquist boundary).

If the parsed value is already in Hz $(> 1.0)$, it is clamped directly to $[20.0, 20000.0]\text{ Hz}$.

### 4.2 Exponential Time Curve Scaling
Envelope stage times (Attack, Decay, Release) are mapped across four decades of dynamic range from $1\text{ ms}$ $(0.001\text{ s})$ to $10\text{ s}$:

```math
t(\text{norm}) = t_{\text{min}} \cdot \left( \frac{t_{\text{max}}}{t_{\text{min}}} \right)^{\text{norm}} = 0.001 \cdot 10000.0^{\text{norm}} \quad (\text{seconds})
```

- $\text{norm} = 0.00 \implies 0.001\text{ s}$ (1 ms, snappy click/transient).
- $\text{norm} = 0.25 \implies 0.01\text{ s}$ (10 ms, punchy bass).
- $\text{norm} = 0.50 \implies 0.10\text{ s}$ (100 ms, pluck/stab).
- $\text{norm} = 0.75 \implies 1.00\text{ s}$ (1 s, standard decay).
- $\text{norm} = 1.00 \implies 10.0\text{ s}$ (10 s, ambient drone).

### 4.3 Bipolar Envelope & Mod Amount Conversion
Parameters with a neutral center position (such as Filter Envelope Depth and Modulation Depth) are mapped linearly from unipolar $[0.0, 1.0]$ to bipolar $[-1.0, +1.0]$:

```math
v_{\text{bipolar}}(\text{norm}) = 2.0 \cdot \text{norm} - 1.0
```

If the input is already signed negative, it is treated as a pre-scaled bipolar quantity and clamped to $[-1.0, 1.0]$.

---

## 5. CLI Tooling & Usage Workflows

### 5.1 Installation & Dependencies
The migration tool [`scripts/import_wasp_presets.py`](../scripts/import_wasp_presets.py) is implemented using Python 3.8+ standard libraries (`struct`, `xml.etree.ElementTree`, `pathlib`, `argparse`). It requires zero third-party packages or virtual environment activation.

### 5.2 CLI Command Flags
```text
python scripts/import_wasp_presets.py [-h] --input INPUT [INPUT ...]
                                     [--output-dir OUTPUT_DIR]
                                     [--export-cpp [EXPORT_CPP]]
                                     [--category CATEGORY]
                                     [--verbose]
                                     [--dry-run]
```

- `--input`, `-i`: One or more paths to files (`.fxp`, `.fxb`, `.fst`, `.flp`) or directories. Directories are scanned recursively.
- `--output-dir`, `-o`: Directory where XML preset files are written (defaults to `presets/migrated/`). Presets are organized into category subfolders.
- `--export-cpp`, `-c`: Optional path to output a compiled C++ `PresetDefinition` array header.
- `--category`: Overrides the category tag for all processed files (e.g. `Bass`, `Lead`, `Pad`). When omitted, presets are auto-classified based on name and acoustic parameter values.
- `--verbose`, `-v`: Enables detailed logging of file headers, chunk sizes, and mapped parameters.
- `--dry-run`: Runs full extraction and logging without writing files to disk.

### 5.3 Practical Workflows & Examples

#### Workflow A: Migrating a Directory of VST Presets
```bash
python scripts/import_wasp_presets.py \
  --input "C:/LegacyMusic/Wasp_Patches" \
  --output-dir "presets/migrated" \
  --verbose
```

#### Workflow B: Extracting Sounds from Legacy FLP Project Files
```bash
python scripts/import_wasp_presets.py \
  --input "D:/OldProjects/TranceTrack2004.flp" \
  --output-dir "presets/migrated"
```
The importer automatically extracts each Wasp channel from the project and names the preset after the channel rack track.

#### Workflow C: Compiling Factory Presets into C++ Source Code
```bash
python scripts/import_wasp_presets.py \
  --input "presets/homage" \
  --export-cpp "source/plugin/parameters/MigratedPresets.h"
```

---

## 6. Legal & Clean-Room Interoperability Architecture

### 6.1 Clean-Room Engineering Methodology
Bumbler XD and its migration tooling were authored under strict **clean-room software engineering standards**:
1. **Zero Proprietary Code**: No binary code, decompiled source listings, or copyrighted wavetable assets from Image-Line or Synapse Audio were copied or distributed.
2. **Functional Compatibility**: The migration tool acts exclusively as a data translator, parsing legacy file containers to preserve end-user artistic works and patch data.
3. **Independent Acoustic Emulation**: Bumbler XD's DSP algorithms (PolyBLEP oscillators, ZDF state-variable filters, and polynomial saturation) are original implementations derived from public mathematical literature (Välimäki, Zavalishin, Stilson & Smith).

### 6.2 United States Legal Precedents
In the United States, functional file formats, parameter definitions, and interoperability tools are protected under established copyright jurisprudence:
- **Baker v. Selden, 101 U.S. 99 (1879)**: The *idea/expression dichotomy* establishes that functional systems, blank forms, and operational mechanics cannot be copyrighted.
- **Sega Enterprises Ltd. v. Accolade, Inc., 977 F.2d 1510 (9th Cir. 1992)** & **Sony Computer Entertainment, Inc. v. Connectix Corp., 203 F.3d 596 (9th Cir. 2000)**: Developing software that reads proprietary data structures or interfaces for the purpose of achieving interoperability constitutes lawful Fair Use under 17 U.S.C. § 107.
- **Google LLC v. Oracle America, Inc., 141 S. Ct. 1163 (2021)**: The Supreme Court affirmed that declaring interfaces, parameter namespaces, and functional headers used to ensure interoperability are subject to robust fair use protection.

### 6.3 European Union Software Directive
In the European Union, interoperability without authorization of the original rightsholder is expressly guaranteed:
- **Directive 2009/24/EC, Article 6 (Decompilation & Interoperability)**: Reproduction of code and translation of its form are permitted when indispensable to obtain the information necessary to achieve the interoperability of an independently created computer program with other programs.

### 6.4 Commercial Royalty-Free Status
- All XML presets generated by `scripts/import_wasp_presets.py` and the 15 clean-room homage presets in `presets/homage/` are **100% royalty-free**.
- Musicians, composers, and producers may use them freely in commercial music recordings, film scoring, game audio, and sample libraries without licensing fees or attribution requirements.
