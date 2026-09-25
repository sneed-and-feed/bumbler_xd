# 🎹 Bumbler XD Preset Architecture & Sound Library

This document details the preset architecture, factory programs, and clean-room homage sound library for **Bumbler XD**.

---

## 📑 Table of Contents

1. [Preset Architecture & APVTS Recall](#1-preset-architecture--apvts-recall)
2. [Factory Programs (Embedded in Firmware/Binary)](#2-factory-programs-embedded-in-firmwarebinary)
3. [Clean-Room Homage Preset Collection (15 Presets)](#3-clean-room-homage-preset-collection-15-presets)
   - [3.1 Bass Presets](#31-bass-presets)
   - [3.2 Lead Presets](#32-lead-presets)
   - [3.3 Pad & Atmosphere Presets](#33-pad--atmosphere-presets)
   - [3.4 Pluck Presets](#34-pluck-presets)
   - [3.5 Percussion Presets](#35-percussion-presets)
   - [3.6 Special Effects (FX) Presets](#36-special-effects-fx-presets)
4. [DAW Preset Loading & Installation](#4-daw-preset-loading--installation)
5. [Legacy Preset Migration](#5-legacy-preset-migration)

---

## 1. Preset Architecture & APVTS Recall

Bumbler XD manages sound state using JUCE's `AudioProcessorValueTreeState` (APVTS) combined with a lock-free snapshot extraction architecture:

```text
                  ┌──────────────────────────────────────────┐
                  │       DAW Host / UI / Preset File        │
                  └────────────────────┬─────────────────────┘
                                       │ XML / State Information
                                       ▼
                  ┌──────────────────────────────────────────┐
                  │      AudioProcessorValueTreeState        │
                  │              (55 Parameters)             │
                  └────────────────────┬─────────────────────┘
                                       │ Atomic float updates
                                       ▼
┌────────────────────────────────────────────────────────────────────────────┐
│                    Lock-Free Audio Thread (<45 ns)                         │
│  ParameterSnapshot snap = mAtomicPointers.loadSnapshot();                  │
│  mEngine.renderNextBlock(buffer, snap);                                    │
└────────────────────────────────────────────────────────────────────────────┘
```

- **Thread-Safety Guarantee**: Preset changes are applied on the message thread and read atomically on the audio thread with `std::memory_order_relaxed`. Zero heap allocations occur in `processBlock()`.
- **Defensive XML Validation**: The state restoration routine (`setStateInformation`) sanitizes incoming XML against `NaN`, `Infinity`, and out-of-range bounds, safely falling back to parameter defaults on corrupted chunks.
- **Root Tag Compatibility**: Accepts both standard `<Parameters ...>` and native `<BumblerXD ...>` XML tags.

---

## 2. Factory Programs (Embedded in Firmware/Binary)

Bumbler XD features 5 built-in factory presets declared in [`source/plugin/parameters/PresetParameters.h`](../source/plugin/parameters/PresetParameters.h):

| Program # | Name | Category | Primary Filter | Description |
|---|---|---|---|---|
| **0** | `Acid Bass` | Bass | `LP24` (380 Hz) | Classic 303/Wasp squelch bass with 78% resonance, high dynamic velocity accent, and 45% drive saturation. |
| **1** | `Sync Lead` | Lead | `LP+NT` (2400 Hz) | Aggressive piercing sync lead with audio-rate FM grit, delayed pitch vibrato, and transient laser dive. |
| **2** | `Swarm Pad` | Pad | `DBL.NT` (1800 Hz) | Lush double-notch choir pad with slow PWM sweep, timbral panning, and wide stereo chorusing. |
| **3** | `Percussion` | Percussion | `BP24` (850 Hz) | Snappy analog drum/zap transient with noise snap, 1ms instant attack, fast pitch plunge, and drive crunch. |
| **4** | `Vintage Drone`| Atmosphere | `HP24` (220 Hz) | Dark, brooding highpass atmospheric drone with Sample & Hold cutoff wander and sub-bass rumble. |

---

## 3. Clean-Room Homage Preset Collection (15 Presets)

Located in [`presets/homage/`](../presets/homage/), these 15 presets provide clean-room, royalty-free acoustic homages capturing the synthesis architecture of the classic EDP Wasp and Wasp XT:

### 3.1 Bass Presets
- **[`Acid Squelch Bass`](../presets/homage/Bass/Acid_Squelch_Bass.xml)**: Signature 24 dB lowpass squelch tuned to 340 Hz with 82% resonance, 220ms snappy decay, and saturating drive.
- **[`Sub Rumble Bass`](../presets/homage/Bass/Sub_Rumble_Bass.xml)**: Deep -2 octave sine fundamental with 12 dB lowpass slope and warm analog drive for clean low-end retention.
- **[`Reese Grime Bass`](../presets/homage/Bass/Reese_Grime_Bass.xml)**: Detuned dual-saw beat frequencies with double-notch comb filtering modulated by LFO 1 and stereo Haas decorrelation.

### 3.2 Lead Presets
- **[`Piercing Sync Lead`](../presets/homage/Lead/Piercing_Sync_Lead.xml)**: High-register saw with 35% audio-rate FM, formant cascade filtering, and delayed vibrato.
- **[`Screamer Rave Stab`](../presets/homage/Lead/Screamer_Rave_Stab.xml)**: 90s rave stab with resonant 24 dB bandpass filter and high-gain distortion presence.
- **[`Formant Vocal Lead`](../presets/homage/Lead/Formant_Vocal_Lead.xml)**: Vowel-like formant lead using ModEnv-driven pulse width modulation.

### 3.3 Pad & Atmosphere Presets
- **[`Celestial Choir Pad`](../presets/homage/Pad/Celestial_Choir_Pad.xml)**: 1.2-second blooming swell with double-notch sweeping and lush stereo chorusing.
- **[`Dark Atmosphere Drone`](../presets/homage/Pad/Dark_Atmosphere_Drone.xml)**: Sci-fi atmospheric bed with 24 dB highpass hollow resonance and random Sample & Hold filter drift.
- **[`Vintage String Machine`](../presets/homage/Pad/Vintage_String_Machine.xml)**: 1970s analog string ensemble with detuned saws and BBD-style LFO pitch chorusing.

### 3.4 Pluck Presets
- **[`Chime Glass Pluck`](../presets/homage/Pluck/Chime_Glass_Pluck.xml)**: Crystalline bell pluck featuring ring modulation and resonant bandpass filter.
- **[`Retro Trance Pluck`](../presets/homage/Pluck/Retro_Trance_Pluck.xml)**: Crisp trance pluck with snappy 24 dB filter plunge, sub-oscillator support, and velocity dynamics.

### 3.5 Percussion Presets
- **[`Electro Zap Kick`](../presets/homage/Percussion/Electro_Zap_Kick.xml)**: Heavy analog electronic kick with 45ms pitch laser plunge and saturated drive clipping.
- **[`Snappy Noise Snare`](../presets/homage/Percussion/Snappy_Noise_Snare.xml)**: Tuned sine fundamental combined with 70% white noise burst and 24 dB bandpass body.

### 3.6 Special Effects (FX) Presets
- **[`SciFi Laser Dive`](../presets/homage/FX/SciFi_Laser_Dive.xml)**: Extreme pitch dive with high-depth audio FM screech and resonant filter sweep.
- **[`Cosmic Riser Sweep`](../presets/homage/FX/Cosmic_Riser_Sweep.xml)**: 3.5-second rising filter sweep with ramp LFO stutter modulation for track builds and EDM drops.

---

## 4. DAW Preset Loading & Installation

### Loading XML Presets in Your DAW
Bumbler XD presets use the JUCE APVTS-compatible XML format (`<BumblerXD ...>`).
- **FL Studio**: Plugin wrapper menu (top left) \(\rightarrow\) **"Load state..."** or **"Presets" \(\rightarrow\) "Load preset..."** \(\rightarrow\) select any `.xml` preset file.
- **Ableton Live**: On the VST3 device header, click the **Folder icon** or right-click the plugin name \(\rightarrow\) **"Load Preset (.xml)"**.
- **REAPER**: In the FX window, click the **"+"** button next to the preset dropdown \(\rightarrow\) **"Import patch/bank..."** or **"Load preset from file..."**.
- **Bitwig / Studio One / Cubase**: Use the plugin wrapper header preset menu \(\rightarrow\) **"Import VST3 Preset / XML State"**.

### Installing into System Preset Folders
To have presets appear automatically in host preset browsers, copy the preset folders to:
- **Windows**: `%USERPROFILE%\Documents\Bumbler XD\Presets\` or `%APPDATA%\VST3 Presets\Bumbler Audio\Bumbler XD\`
- **macOS**: `~/Library/Audio/Presets/Bumbler Audio/Bumbler XD/`
- **Linux**: `~/.vst3/presets/Bumbler Audio/Bumbler XD/`

---

## 5. Legacy Preset Migration & Musician QOL Workflows

Bumbler XD provides multiple effortless ways to migrate legacy patches from the discontinued Wasp and Wasp XT plugins (`.fxp`, `.fxb`, `.fst`, `.flp`) into modern APVTS presets:

### Method 1: Instant In-Plugin Drag-and-Drop (Recommended for DAWs)
1. Open Bumbler XD in your DAW or Standalone.
2. Drag any legacy preset file (`.fxp`, `.fxb`, `.fst`, `.flp`, or `.xml`) or an entire preset folder from your file manager directly onto the plugin window.
3. Bumbler XD displays an active drop zone, converts the file on-the-fly, saves it to `%USERPROFILE%\Documents\Bumbler XD\Presets\Migrated\`, updates the preset dropdown under **MIGRATED & USER PRESETS**, and immediately loads the sound so you can play it on your keyboard!

### Method 2: In-Plugin "MIGRATE..." Header Button
1. Click the **`MIGRATE...`** button in the plugin header next to the preset dropdown.
2. Choose **"Migrate Preset File(s)..."** or **"Migrate Entire Folder of Presets..."**.
3. Select your files; Bumbler XD imports and loads them automatically.

### Method 3: Desktop Graphical Migrator Tool (Batch Conversion)
For musicians migrating large sound libraries without launching a DAW:
- **Windows**: Double-click **`MigratePresets.bat`** in the Bumbler XD folder.
- **Cross-Platform**: Run `python scripts/migrator_gui.py` or `python scripts/import_wasp_presets.py --gui`.
- Provides an intuitive dark-themed GUI with file/folder selection, category auto-detection, a live conversion log, and an "Open Output Folder" shortcut.

### Method 4: Automated CLI Migration Tool
```bash
python scripts/import_wasp_presets.py --input "path/to/legacy_patches" --output-dir "presets/migrated"
```

For complete technical specifications on legacy binary parsing, mathematical parameter conversion curves, and legal clean-room interoperability, refer to:
- [**Legacy Preset Migration Guide (`docs/PRESET_MIGRATION.md`)**](PRESET_MIGRATION.md)
- [**Migration Importer Tooling (`scripts/README.md`)**](../scripts/README.md)
