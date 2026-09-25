#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/ParameterSnapshot.h"
#include <map>
#include <vector>

namespace bumbler {

/**
 * MigratedPreset: In-memory representation of a migrated synthesizer patch.
 * Contains both a raw parameter ID map and a pre-compiled ParameterSnapshot
 * ready for instant lock-free injection into BumblerEngine.
 */
struct MigratedPreset {
    juce::String name;
    juce::String category;
    juce::String sourceFormat;
    juce::String sourceFilePath;
    std::map<juce::String, float> parameterMap;
    ParameterSnapshot snapshot;

    juce::String toXml() const;
};

/**
 * PresetMigrator: Universal clean-room legacy preset migration engine.
 *
 * Implements native C++ parsing and mathematical domain translation for:
 *   - VST 2.4 presets (.fxp) and banks (.fxb) ('CcnK' / 'FxCk' / 'FPCh' / 'FxBk' / 'FBCh')
 *   - FL Studio state files (.fst) (RIFF containers, nested VST chunks, raw dumps)
 *   - FL Studio project files (.flp) (scans event 0xC5 for Wasp/Wasp XT channels)
 *   - Native Bumbler XD XML presets (<BumblerXD ...> / <Parameters ...>)
 *
 * Runs 100% natively in C++20 with zero external dependencies, zero subprocesses,
 * and zero requirement for Python on the musician's machine.
 */
class PresetMigrator {
public:
    // Core binary parsers
    static std::vector<MigratedPreset> parseFxp(const void* data, size_t sizeBytes, const juce::String& filename = {});
    static std::vector<MigratedPreset> parseFst(const void* data, size_t sizeBytes, const juce::String& filename = {});
    static std::vector<MigratedPreset> parseFlp(const void* data, size_t sizeBytes, const juce::String& filename = {});
    static std::vector<MigratedPreset> parseXml(const juce::String& xmlText, const juce::String& filename = {});

    // High-level migration workflows
    static std::vector<MigratedPreset> migrateFile(const juce::File& file);
    static std::vector<MigratedPreset> migrateFiles(const juce::Array<juce::File>& files);
    static std::vector<MigratedPreset> migrateDirectory(const juce::File& dir, bool recursive = true);

    // Preset library storage
    static juce::File getUserPresetDirectory();
    static bool savePresetToXml(const MigratedPreset& preset, const juce::File& destinationFile);
    static juce::File saveToUserLibrary(const MigratedPreset& preset);
    static juce::Array<juce::File> scanUserPresets();

    // Mathematical domain mapping
    static float clamp(float val, float minVal, float maxVal) noexcept;
    static float scaleLogarithmic(float norm, float minVal, float maxVal) noexcept;
    static float scaleExponentialTime(float norm, float minS = 0.001f, float maxS = 10.0f) noexcept;
    static float scaleLfoRate(float norm, float minHz = 0.05f, float maxHz = 30.0f) noexcept;
    static float scaleBipolar(float norm, float minVal = -1.0f, float maxVal = 1.0f) noexcept;
    static float scaleDiscrete(float norm, int numSteps) noexcept;

    static float mapLegacyParamToApvts(const juce::String& paramId, float rawVal);
    static juce::String inferCategory(const juce::String& name, const ParameterSnapshot& snap);
    static ParameterSnapshot createDefaultSnapshot();
    static ParameterSnapshot mapFloatsToSnapshot(const std::vector<float>& floats, bool isClassicWasp32 = false);
    static std::map<juce::String, float> snapshotToMap(const ParameterSnapshot& snap);
};

} // namespace bumbler
