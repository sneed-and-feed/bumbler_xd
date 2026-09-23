# 📦 Bumbler XD Automation, Packaging & Preset Migration Scripts

This directory contains development automation, distribution packaging, and legacy preset migration utilities for **Bumbler XD**.

---

## 🛠️ Script Overview

| Script | Language | Dependencies | Primary Output | Description |
|---|---|---|---|---|
| [`import_wasp_presets.py`](import_wasp_presets.py) | Python 3.8+ | Standard Library | `presets/migrated/**/*.xml`, `PresetParameters.h` snippet | Legacy Wasp & Wasp XT preset migration tool (`.fxp`, `.fxb`, `.fst`, `.flp`). |
| [`generate_homage_presets.py`](generate_homage_presets.py) | Python 3.8+ | Standard Library | `presets/homage/**/*.xml` | Generates the 15 clean-room royalty-free homage preset library. |
| [`package_release.py`](package_release.py) | Python 3.8+ | Standard Library | `releases/*.zip`, `releases/SHA256SUMS.txt` | Packages release archives and computes cryptographic SHA-256 digests. |

---

## 🐝 `import_wasp_presets.py` (Preset Migration Importer)

### 1. Purpose

The [`import_wasp_presets.py`](import_wasp_presets.py) utility enables producers and engineers to migrate legacy preset patches and project tracks from the discontinued **Wasp** and **Wasp XT** synthesizers into Bumbler XD's modern APVTS architecture.

### 2. Supported Legacy File Formats

- **`.fxp` (VST 2.4 Preset)**: Parses standard `CcnK` container headers, handling both `FxCk` (regular 32-bit big-endian float parameter streams) and `FPCh` (opaque chunk presets).
- **`.fxb` (VST 2.4 Bank)**: Extracts multi-program banks (`FxBk` and `FBCh`), generating individual XML files for each contained preset.
- **`.fst` (FL Studio State File)**: Parses RIFF chunk containers (`FS_G`, `FS_C`, `data`, `plug`), IFF chunks, and embedded event streams to extract generator plugin state.
- **`.flp` (FL Studio Project File)**: Scans the event stream for event `0xC5` (`FLP_PluginData`) on channels identifying as Wasp or Wasp XT, extracting channel sound design states.

### 3. CLI Command Options

```text
usage: import_wasp_presets.py [-h] --input INPUT [INPUT ...]
                              [--output-dir OUTPUT_DIR]
                              [--export-cpp [EXPORT_CPP]]
                              [--category CATEGORY] [--verbose] [--dry-run]

Options:
  --input, -i INPUT [INPUT ...]
                        Path to legacy preset file(s) or directory (.fxp, .fxb, .fst, .flp)
  --output-dir, -o OUTPUT_DIR
                        Destination directory for generated JUCE APVTS XML presets (default: presets/migrated)
  --export-cpp, -c [EXPORT_CPP]
                        Generate C++ PresetDefinition header (defaults to source/plugin/parameters/MigratedPresets.h)
  --category CATEGORY   Override category for all imported presets (e.g., Bass, Lead, Pad, Pluck, Percussion, FX)
  --verbose, -v         Enable detailed diagnostic logging
  --dry-run             Parse input files and report statistics without writing output files
```

### 4. Migration Usage Examples

#### Single Preset Migration
```bash
# Convert a single legacy VST 2.4 preset file into an APVTS XML preset
python scripts/import_wasp_presets.py --input "path/to/Acid_Lead.fxp" --output-dir "presets/migrated" --verbose
```

#### Batch Directory Migration with Category Auto-Classification
```bash
# Recursively scan a folder of legacy .fxp, .fst, and .flp files and organize by category
python scripts/import_wasp_presets.py --input "C:/Legacy_Presets/Wasp_Patches" --output-dir "presets/migrated"
```

#### Generate C++ Preset Definitions for Hardcoding into Firmware / Plugin Binary
```bash
# Export C++ structs directly for inclusion into PresetParameters.h
python scripts/import_wasp_presets.py --input "presets/homage" --export-cpp "source/plugin/parameters/MigratedPresets.h"
```

---

## 🎨 `generate_homage_presets.py` (Clean-Room Homage Generator)

### 1. Purpose

The [`generate_homage_presets.py`](generate_homage_presets.py) script generates the 15 clean-room royalty-free homage presets in `presets/homage/`. It programmatically builds all 15 preset specifications covering:
- **Bass**: Acid Squelch Bass, Sub Rumble Bass, Reese Grime Bass
- **Lead**: Piercing Sync Lead, Screamer Rave Stab, Formant Vocal Lead
- **Pad**: Celestial Choir Pad, Dark Atmosphere Drone, Vintage String Machine
- **Pluck**: Chime Glass Pluck, Retro Trance Pluck
- **Percussion**: Electro Zap Kick, Snappy Noise Snare
- **FX**: Sci-Fi Laser Dive, Cosmic Riser Sweep

### 2. Usage

```bash
# Regenerate or verify all 15 homage presets
python scripts/generate_homage_presets.py
```

Outputs are placed in `presets/homage/<Category>/<Preset_Name>.xml`.

---

## 📦 `package_release.py` (Release Packaging)

### 1. Purpose

Automates the creation of release distribution archives from compiled build artifacts produced by CMake and JUCE 8. It packages standalone executables and VST3 plugin bundles, includes necessary project documentation and licensing, and computes cryptographically secure SHA-256 digests.

### 2. Prerequisites & Build

```bash
# Build standalone and VST3 targets in Release mode
cmake --build build --config Release --parallel
```

### 3. Packaging Execution

```bash
python scripts/package_release.py
```

All packaged files are staged into the `releases/` directory:
- `releases/BUMBLER_XD-v1.0.5-Windows-x64.zip` (Full distribution package)
- `releases/BUMBLER_XD-v1.0.5-VST3-Windows-x64.zip` (VST3-only package)
- `releases/SHA256SUMS.txt` (Cryptographic checksum manifest)

---

## 🧪 Automated Testing

To run the automated Python test suite for the preset migration importer and file parsers:

```bash
python tests/test_preset_importer.py -v
```
