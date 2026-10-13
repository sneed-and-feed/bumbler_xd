# 🐝 Bumbler XD v1.0.7 — Real-Time DSP Hot-Path Optimizations, State Recall Hardening & Preset Migrator Enhancements

**Bumbler XD v1.0.7** is a stability, performance, and interoperability release for the C++20 / JUCE 8 homage to the classic **Wasp XT** synthesizer. It eliminates redundant per-sample transcendental math in the active voice rendering loop, hardens DAW state recall against partial or malformed XML chunks, coalesces high-frequency parameter automation UI updates, and expands legacy FL Studio `.fst` / Delphi chunk preset migration support.

---

## ✨ What's New in v1.0.7

### ⚡ Real-Time DSP Hot-Path Optimizations
- **`BumblerModEnvelope` & `BumblerADSR` Coefficient Caching (`source/dsp/BumblerEnvelopes.h`)**:
  - Added attack and decay parameter caching (`mCachedAttack`, `mCachedDecay`) and sample-rate change cache invalidation to `BumblerModEnvelope::setParameters()` and `BumblerADSR::update()`, eliminating per-sample `std::exp()` calls across active voices while maintaining 100% bit-exact output against all golden test vectors.
- **`BumblerLFO` Phase Increment Change Detection (`source/dsp/BumblerLFO.cpp`)**:
  - Added change-detection guards (`!=`) and `std::isfinite()` BPM validation to `BumblerLFO::setRate()`, `setDelay()`, `setTempoSync()`, and `setTempo()`, eliminating redundant per-sample `updatePhaseIncrement()` divisions and `std::round()` evaluations when LFO parameters are unchanged.
- **Twin-Notch (`DBL_NT`) Block Pre-Warping (`source/dsp/WaspFilter.cpp`)**:
  - Hoisted `WaspFilterMode::DBL_NT` dual-notch cutoff clamping and `std::tan()` frequency pre-warping (`dblNtG1`, `dblNtG2`) out of the inner sample loop in `WaspFilter::processBlock()`.
- **Voice Steal De-Click Completion & Early Loop Exit (`source/dsp/BumblerVoice.h/.cpp`, `source/dsp/BumblerVoiceManager.h/.cpp`)**:
  - Added `BumblerVoice::isStealing()` so voices finishing a 5ms Hann de-click crossfade are guaranteed to render to completion, and added immediate mid-block loop exit in `BumblerVoice::renderBlockAccumulate()` once a voice completes both its envelope release and steal crossfade.

### 🛡️ DAW State Recall, Multichannel Safety & UI Coalescing
- **Partial & Foreign-Tag XML State Recall Hardening (`source/plugin/PluginProcessor.cpp`)**:
  - Prevented partial `<PARAM>` XML chunks or XML chunks containing foreign child tags (with fewer than 55 `<PARAM>` nodes) from overwriting the internal APVTS `ValueTree` structure and dropping unspecified parameter nodes during `setStateInformation()`.
  - Added explicit `id` and `value` attribute existence checks on `<PARAM>` elements prior to float conversion and range clamping.
- **Multichannel Host Output Clearing & Playhead BPM Guard (`source/plugin/PluginProcessor.cpp`)**:
  - Explicitly clears any auxiliary host output channels beyond stereo (`ch >= 2`) in `processBlock()` and guards host transport BPM queries with `std::isfinite()`.
- **Lock-Free Atomic Coalescing of Editor LCD Updates (`source/plugin/PluginEditor.h/.cpp`)**:
  - Added `std::atomic<bool> mLcdUpdatePending` compare-and-exchange coalescing in `BumblerAudioProcessorEditor::parameterChanged()` to prevent message-queue flooding under rapid multi-threaded DAW automation while preserving `SafePointer` lifecycle safety.

### 🎹 Legacy Preset Migrator Enhancements & Build Polish
- **Native FL Studio `.fst` & Delphi Integer Chunk Support (`source/plugin/PresetMigrator.cpp`, `scripts/import_wasp_presets.py`)**:
  - Added support for native FL Studio Wasp XT `.fst` presets, chunk event `0xD5`, and 32-bit Delphi integer parameter chunks while ignoring Fruity Wrapper metadata event `212`.
- **Zero-Warning `/W4` Compilation (`tests/`)**:
  - Resolved all remaining MSVC `/W4` warnings (`C4834` `[[nodiscard]]` and `C4189`) across all 23 CTest targets.

---

## 📦 Release Packages & SHA-256 Checksums

> **Note:** macOS (Universal 2 `arm64`/`x86_64`) and Linux (`x86_64`) builds are automatically generated and attached by the GitHub Actions CI/CD release runners upon tag push. Manual Windows x64 packages (Full Bundle + VST3-only) are provided below:

| Package | Contents | Size | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| **`BUMBLER_XD-v1.0.7-Windows-x64.zip`** | Standalone (`Bumbler XD.exe`), VST3 (`Bumbler XD.vst3`), `MigratePresets.bat`, Python GUI/CLI Migrator Scripts, `README.md`, `LICENSE` | `6,358,226 bytes` (~6.06 MB) | `3f029f8c4503c2e6dd2c2d295eecc859ecd63d68e37aa5774fb4a9ad3535cf3d` |
| **`BUMBLER_XD-v1.0.7-VST3-Windows-x64.zip`** | VST3 Plugin Bundle (`Bumbler XD.vst3`), `README.md`, `LICENSE` | `2,936,832 bytes` (~2.80 MB) | `337002939ce4bc4d693df0d19248fc5793153298fc6a79cd39e929fb5ab4bd53` |

### `SHA256SUMS.txt`
```text
337002939ce4bc4d693df0d19248fc5793153298fc6a79cd39e929fb5ab4bd53  BUMBLER_XD-v1.0.7-VST3-Windows-x64.zip
3f029f8c4503c2e6dd2c2d295eecc859ecd63d68e37aa5774fb4a9ad3535cf3d  BUMBLER_XD-v1.0.7-Windows-x64.zip
```

---

## ✅ Verification Summary
- **Compiler**: MSVC 17.14 (`Release`, ISO C++20, `/W4 /O2 /fp:precise /arch:AVX2`) — **0 errors, 0 project warnings**.
- **Automated CTest Verification**: **23 / 23 test suites passed (100%)**, including deterministic Golden Vector regression tests (`SNR > 120 dB`, `maxDelta < 1e-4`), real-time zero-allocation interceptors, and 500-cycle concurrent automation/GUI teardown stress suites.
- **Python Preset Migrator Suite**: **18 / 18 unit tests passed (100%)**.
