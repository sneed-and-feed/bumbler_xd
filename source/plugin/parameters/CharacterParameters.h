#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../dsp/ParameterSnapshot.h"
#include "ParameterHelpers.h"
#include <vector>
#include <memory>
#include <atomic>

namespace bumbler {

// ============================================================================
// Parameter IDs (Type-safe APVTS identifiers) — Character & Output (7)
// ============================================================================
namespace ParamIDs {
    inline const juce::ParameterID driveEnabled { "driveEnabled", 1 };
    inline const juce::ParameterID driveAmount  { "driveAmount", 1 };
    inline const juce::ParameterID driveTone    { "driveTone", 1 };
    inline const juce::ParameterID dualMode     { "dualMode", 1 };
    inline const juce::ParameterID analogMode   { "analogMode", 1 };
    inline const juce::ParameterID wNoiseMode   { "wNoiseMode", 1 };
    inline const juce::ParameterID masterVolume { "masterVolume", 1 };
} // namespace ParamIDs

// ============================================================================
// Choice String Arrays — Character & Output
// ============================================================================
inline const juce::StringArray& getNoiseModeChoices() {
    static const juce::StringArray choices { "Vintage Table", "White Noise" };
    return choices;
}

// ============================================================================
// Cached Atomic Pointers — Character & Output
// ============================================================================
struct CharacterAtomicPointers {
    std::atomic<float>* driveEnabled { nullptr };
    std::atomic<float>* driveAmount  { nullptr };
    std::atomic<float>* driveTone    { nullptr };
    std::atomic<float>* dualMode     { nullptr };
    std::atomic<float>* analogMode   { nullptr };
    std::atomic<float>* wNoiseMode   { nullptr };
    std::atomic<float>* masterVolume { nullptr };

    void initialize(juce::AudioProcessorValueTreeState& apvts) noexcept {
        driveEnabled = apvts.getRawParameterValue(ParamIDs::driveEnabled.getParamID());
        driveAmount  = apvts.getRawParameterValue(ParamIDs::driveAmount.getParamID());
        driveTone    = apvts.getRawParameterValue(ParamIDs::driveTone.getParamID());
        dualMode     = apvts.getRawParameterValue(ParamIDs::dualMode.getParamID());
        analogMode   = apvts.getRawParameterValue(ParamIDs::analogMode.getParamID());
        wNoiseMode   = apvts.getRawParameterValue(ParamIDs::wNoiseMode.getParamID());
        masterVolume = apvts.getRawParameterValue(ParamIDs::masterVolume.getParamID());
    }

    void loadSnapshot(ParameterSnapshot& s) const noexcept {
        if (driveEnabled) s.driveEnabled = driveEnabled->load(std::memory_order_relaxed);
        if (driveAmount)  s.driveAmount  = driveAmount->load(std::memory_order_relaxed);
        if (driveTone)    s.driveTone    = driveTone->load(std::memory_order_relaxed);
        if (dualMode)     s.dualMode     = dualMode->load(std::memory_order_relaxed);
        if (analogMode)   s.analogMode   = analogMode->load(std::memory_order_relaxed);
        if (wNoiseMode)   s.wNoiseMode   = wNoiseMode->load(std::memory_order_relaxed);
        if (masterVolume) s.masterVolume = masterVolume->load(std::memory_order_relaxed);
    }
};

// ============================================================================
// Layout Helpers — Character & Output (7)
// ============================================================================
inline void addCharacterParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) {
    detail::addBoolParameter(params,      ParamIDs::driveEnabled, "Drive Enable", false);
    detail::addFloatParameter(params,     ParamIDs::driveAmount,  "Drive Amount", 0.0f, 1.0f, 0.30f, 0.001f, "%");
    detail::addFloatParameter(params,     ParamIDs::driveTone,    "Drive Tone", 0.0f, 1.0f, 0.50f, 0.001f, "%");
    detail::addBoolParameter(params,      ParamIDs::dualMode,     "Dual Mode", false);
    detail::addBoolParameter(params,      ParamIDs::analogMode,   "Analog Mode", false);
    detail::addChoiceParameter(params,    ParamIDs::wNoiseMode,   "Noise Type", getNoiseModeChoices(), 0);
    detail::addFloatSkewParameter(params, ParamIDs::masterVolume, "Master Volume", 0.0f, 1.0f, 0.80f, 0.5f, 0.001f, "%");
}

inline std::vector<std::unique_ptr<juce::RangedAudioParameter>> createCharacterParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    addCharacterParameters(params);
    return params;
}

} // namespace bumbler
