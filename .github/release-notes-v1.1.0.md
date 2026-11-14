# 🐝 Bumbler XD v1.1.0 — Near-Nyquist PolyBLEP4 Symmetry, `DBL_NT` Sub-Block Smoothing, MIDI Panic Compliance, `NaN`/`Inf` Immunity & Migrator Bounds Hardening

**Bumbler XD v1.1.0** is a comprehensive DSP accuracy, real-time state-machine, preset migration, and documentation compliance release for the C++20 / JUCE 8 homage to the classic **Wasp XT** synthesizer. Every synthesis block, modulation state transition, legacy binary/XML preset parser, and documentation equation has been audited end-to-end and verified across 24 C++20 CTest suites and 22 Python unit tests.

---

## ✨ What's New in v1.1.0

### 🎛️ Real-Time DSP Accuracy & Anti-Aliasing Fixes
- **Near-Nyquist `polyBlep4` Discontinuity Symmetry Above $f_s / 4$ (`source/dsp/BumblerCommon.h`)**:
  - **Root Cause**: The 4-point `polyBlep4(t, dt)` residual checked `t < 2 * dt` and returned before reaching the right-hand `t > 1 - 2 * dt` branch. At pitches above $f_s / 4$ ($dt > 0.25$, e.g., high octaves or extreme pitch modulation at `44.1 kHz`), the two-sample windows $[0, 2 \cdot dt)$ and $(1 - 2 \cdot dt, 1]$ overlapped and suppressed the trailing discontinuity correction, injecting DC bias and foldback aliasing.
  - **Fix**: Delegated $dt > 0.25$ cleanly to the non-overlapping 2-point `polyBlep(t, dt)` kernel (clamped at $dt \le 0.5$) while keeping $dt \le 0.25$ 100% bit-identical to all isolated PolyBLEP golden fixtures.
- **Zipper-Free `WaspFilterMode::DBL_NT` Sub-Block Smoothing (`source/dsp/WaspFilter.h/.cpp`)**:
  - **Root Cause**: `WaspFilter::processBlock()` linearly interpolated `mCurrentG` and `mCurrentK` across 16-sample sub-blocks for standard modes, but held the twin-notch pre-warped gains (`dblNtG1`, `dblNtG2`) constant across the entire host buffer in `DBL_NT` mode, causing audible zipper steps on large buffer sizes (`1024`–`4096+` samples).
  - **Fix**: Added `mCurrentDblG1`, `mCurrentDblG2`, and `mLastMode` transition snapping with 16-sample sub-block linear interpolation across `44.1`–`192 kHz`, eliminating block-boundary stepping without per-sample `std::tan()` calls.
- **Direct Module-Level IEEE-754 `NaN` / `±Inf` Immunity (`source/dsp/*`, `source/plugin/PluginProcessor.cpp`)**:
  - **Root Cause**: Several subordinate DSP entry points (`OnePoleDCBlocker`, `BumblerOscillator`, `BumblerTripleOscillatorSection` including `mOsc3` frequency clamping, `WaspFilter`, `BumblerADSR`, `BumblerModEnvelope`, `BumblerLFO`, `BumblerVoice`, and `CharacterCircuits`) assumed pre-sanitized caller inputs and could latch non-finite values in recursive filter or envelope registers if invoked directly by a host or C++20 library consumer.
  - **Fix**: Added bitwise finite checks (`isFiniteBitwise` / `isFiniteBitwiseDouble`, immune to `-ffast-math`) and upper-bound clamping across all DSP entry points and `PluginProcessor::applyParameterSnapshot()`, guaranteeing clean recovery without requiring `reset()`.

### 🎹 MIDI Panic, Voice Lifecycle & Preset-Switch State Synchronization
- **MIDI `CC 120` (All Sound Off), `CC 123` (All Notes Off), `CC 121` (Reset All Controllers) & `CC 64` Sustain Voice-Stealing Priority (`source/plugin/PluginProcessor.cpp`, `source/dsp/BumblerEngine.cpp`, `source/dsp/BumblerVoice.h`, `source/dsp/BumblerVoiceManager.h/.cpp`)**:
  - **Root Cause**: `PluginProcessor::processBlock()` checked `if (msg.isAllNotesOff() || msg.isAllSoundOff())` and forwarded both to `CC 123`, shadowing `CC 120` fast-kill (`allNotesOff(true)`). Additionally, `allNotesOff()` did not clear `mSustainPedal` or reset post-voice master `CharacterCircuits` (`mDcBlockerL`/`R`, `DistortionUnit` tone filter, and 5ms Haas `DualModeVoiceDoubler` ring buffer) on `fastKill`, leaving a decaying tail after `All Sound Off`. Under `CC 64` sustain pedal, `noteOff()` also left `mIsHeld == true` when setting `mIsPedalLatched = true`, causing 17th-note polyphonic voice allocation to steal physically held bass roots ahead of released pedal-latched arpeggio keys.
  - **Fix**: Separated `CC 120` (`allNotesOff(true)` + `mCharacterCircuits.reset()`) from `CC 123` (`allNotesOff(false)`), cleared `mSustainPedal = false` across all 16 voices on panic, added `CC 121` (`Reset All Controllers`) handling to reset sustain pedal and pitch bend, cleared `mIsHeld = false` on pedal-latched `noteOff()`, and added a dedicated 5-tier voice-stealing stage prioritizing released pedal-latched voices ahead of physically held keys.
- **Sample-0 Preset-Load Trigger Sync, Polyphony Resize Sync & Live LFO Delay Unblocking (`source/plugin/PluginProcessor.cpp`, `source/dsp/BumblerEngine.h`, `source/dsp/BumblerVoiceManager.cpp`, `source/dsp/BumblerVoice.cpp`, `source/dsp/BumblerLFO.cpp`)**:
  - **Root Cause**: `PluginProcessor::processBlock()` previously dispatched incoming MIDI events before loading the atomic `ParameterSnapshot`, and `BumblerLFO::setDelay()` did not clamp `mDelaySamplesRemaining` when `mDelaySamplesTotal` decreased. Consequently, switching from a preset with `lfo1Delay > 0` (e.g., `Sync Lead` `0.25s`) to one with `lfo1Delay = 0` (`Swarm Pad`) leaked stale LFO delay and `analogMode`/`keyReset` state onto the first note (also previously captured in `tests/fixtures/preset_2_swarm_pad.csv`), and lowering LFO delay mid-note waited out the old countdown. Furthermore, `setSustainPedal()` and `setPitchBend()` only iterated `0 .. mMaxPolyphony - 1`, leaving upper voices out of sync when polyphony was later expanded.
  - **Fix**: Added wait-free `syncTriggerParameters()` ahead of MIDI dispatch in `processBlock()`, clamped `mDelaySamplesRemaining` to `mDelaySamplesTotal` in `BumblerLFO::setDelay()`, regenerated `preset_2_swarm_pad.csv` with verified preset-order invariance (`Max Delta = 0`), and propagated controller state across all 16 preallocated voices.

### 🛡️ Legacy Preset Migrator Hardening, Build Metadata, UI Polish & GFM Math Compliance
- **Adversarial Binary & XML Hardening + `ModDest1..4` Bijection (`source/plugin/PresetMigrator.cpp`, `scripts/import_wasp_presets.py`)**:
  - Eliminated buffer overreads and signed/unsigned 32-bit wraparounds across truncated or hostile `.fxp`/`.fxb` (`FxCk`, `FPCh`, `FxBk`), `.fst` RIFF chunks, and `.flp` streams (including malformed $>5$-byte varints and EOF-adjacent `FLhd`/`FLdt` headers).
  - Hardened C++ `parseXml()` and Python `parse_xml_preset()` against unknown parameter IDs, non-numeric strings, `NaN`/`Inf`, and out-of-range values across all 55 APVTS parameter specs, and fixed `ModDest1..4` (`Tags 46..49 -> 0..3`) bijective mapping in both v13 float and v12 Delphi integer decoders.
- **Preset Recall (`-1..19`), `ChangeBroadcaster` Combo Sync, Windows PE `VERSIONINFO` & `MigratePresets.bat` Stub Guard (`source/plugin/PluginProcessor.h/.cpp`, `source/plugin/PluginEditor.h/.cpp`, `source/ui/LCDGraphDisplay.h/.cpp`, `CMakeLists.txt`, `scripts/package_release.py`, `MigratePresets.bat`)**:
  - Expanded `programIndex` persistence in `setStateInformation()` to `-1..19` so both Homage Presets (`5..19`) and custom/migrated patches (`-1`) restore accurately on DAW project reload, wired `juce::ChangeBroadcaster` / `ChangeListener` (`syncPresetComboSelection()`) so `mPresetComboBox` updates deterministically even when parameters match the previous patch, updated the header subtitle to `"WASP XT HOMAGE | v1.1.0"`, and normalized `LCDGraphDisplay::buildEnvelopePath()` stage widths so long ADSR curves never clip outside the LCD bezel.
  - Added explicit `Info.txt` / `CMakeLists.txt` build dependencies on JUCE's generated `BumblerXD_resources.rc` plus automated `ctypes.windll.version` PE `VERSIONINFO` verification (`1.1.0`) in `scripts/package_release.py`, and hardened `MigratePresets.bat` to probe `py -3 -c "import tkinter"` / `python -c "import tkinter"` before `pythonw` so Windows `WindowsApps` Microsoft Store execution aliases never silently swallow GUI launches.
- **Documentation & 100% GFM / KaTeX Math Compliance (`scripts/audit_gfm_math.py`, `docs/`, `tests/`, `README.md`, `CHANGELOG.md`, `BENCHMARKS.md`, `TEST_READY.md`)**:
  - Fixed ` ```math ` fence state tracking in `scripts/audit_gfm_math.py` and resolved all 57 CommonMark/KaTeX formatting issues across the repository (`0` audit warnings).

---

## 📦 Release Packages & SHA-256 Checksums

> **Note:** macOS (Universal 2 `arm64`/`x86_64`) and Linux (`x86_64`) builds are automatically generated and attached by the GitHub Actions CI/CD release runners upon tag push. Windows x64 packages (Full Bundle + VST3-only) are provided below:

| Package | Contents | Size | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| **`BUMBLER_XD-v1.1.0-Windows-x64.zip`** | Standalone (`Bumbler XD.exe`), VST3 (`Bumbler XD.vst3`), `MigratePresets.bat`, Python GUI/CLI Migrator Scripts, `README.md`, `LICENSE` | `6,363,860 bytes` (~6.07 MB) | `142f5e689160a0b4e65ec96c9861514c2f9bfcbb7e53e735a5c72aa497c649a4` |
| **`BUMBLER_XD-v1.1.0-VST3-Windows-x64.zip`** | VST3 Plugin Bundle (`Bumbler XD.vst3`), `README.md`, `LICENSE` | `2,938,458 bytes` (~2.80 MB) | `08d0cb58d70d0fda64d03ef612174773460175b0bd632f6dd84684d8fa901581` |

### `SHA256SUMS.txt`
```text
08d0cb58d70d0fda64d03ef612174773460175b0bd632f6dd84684d8fa901581  BUMBLER_XD-v1.1.0-VST3-Windows-x64.zip
142f5e689160a0b4e65ec96c9861514c2f9bfcbb7e53e735a5c72aa497c649a4  BUMBLER_XD-v1.1.0-Windows-x64.zip
```

---

## ✅ Verification Summary
- **Compiler**: MSVC 17.14 (`Release`, ISO C++20, `/W4 /O2 /fp:precise /utf-8 /arch:AVX2`) — **0 errors, 0 project warnings**; Windows PE `VERSIONINFO` verified at `1.1.0` for both `Bumbler XD.exe` and `Bumbler XD.vst3`.
- **Automated CTest Verification**: **24 / 24 test suites passed (100%)**, including deterministic Golden Vector regression tests (`SNR = 240 dB`, `maxDelta = 0` across all 5 factory preset fixtures and all 7 isolated algorithm fixtures), real-time zero-allocation interceptors, 500-cycle concurrent automation/GUI teardown stress suites, and the new 7-part `V110RegressionTests` suite.
- **Python Preset Migrator Suite**: **22 / 22 `pytest` unit tests passed (100%)**.
- **GFM / KaTeX Math Linter**: **`scripts/audit_gfm_math.py` passed with 0 issues** across all repository Markdown documents.
