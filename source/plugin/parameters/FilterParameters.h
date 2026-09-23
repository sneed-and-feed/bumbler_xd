#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../dsp/ParameterSnapshot.h"
#include "ParameterHelpers.h"
#include <vector>
#include <memory>
#include <atomic>

namespace bumbler {

// ============================================================================
// Parameter IDs (Type-safe APVTS identifiers) — Filter (5)
// ============================================================================
namespace ParamIDs {
    inline const juce::ParameterID filterMode      { "filterMode", 1 };
    inline const juce::ParameterID filterCutoff    { "filterCutoff", 1 };
    inline const juce::ParameterID filterResonance { "filterResonance", 1 };
    inline const juce::ParameterID filterKbTrack   { "filterKbTrack", 1 };
    inline const juce::ParameterID filterEnvAmount { "filterEnvAmount", 1 };
} // namespace ParamIDs

// ============================================================================
// Choice String Arrays — Filter
// ============================================================================
inline const juce::StringArray& getFilterModeChoices() {
    static const juce::StringArray choices { "LP12", "LP24", "LP+NT", "DBL.NT", "BP24", "HP24" };
    return choices;
}

// ============================================================================
// Cached Atomic Pointers — Filter
// ============================================================================
struct FilterAtomicPointers {
    std::atomic<float>* filterMode      { nullptr };
    std::atomic<float>* filterCutoff    { nullptr };
    std::atomic<float>* filterResonance { nullptr };
    std::atomic<float>* filterKbTrack   { nullptr };
    std::atomic<float>* filterEnvAmount { nullptr };

    void initialize(juce::AudioProcessorValueTreeState& apvts) noexcept {
        filterMode      = apvts.getRawParameterValue(ParamIDs::filterMode.getParamID());
        filterCutoff    = apvts.getRawParameterValue(ParamIDs::filterCutoff.getParamID());
        filterResonance = apvts.getRawParameterValue(ParamIDs::filterResonance.getParamID());
        filterKbTrack   = apvts.getRawParameterValue(ParamIDs::filterKbTrack.getParamID());
        filterEnvAmount = apvts.getRawParameterValue(ParamIDs::filterEnvAmount.getParamID());
    }

    void loadSnapshot(ParameterSnapshot& s) const noexcept {
        if (filterMode)      s.filterMode      = filterMode->load(std::memory_order_relaxed);
        if (filterCutoff)    s.filterCutoff    = filterCutoff->load(std::memory_order_relaxed);
        if (filterResonance) s.filterResonance = filterResonance->load(std::memory_order_relaxed);
        if (filterKbTrack)   s.filterKbTrack   = filterKbTrack->load(std::memory_order_relaxed);
        if (filterEnvAmount) s.filterEnvAmount = filterEnvAmount->load(std::memory_order_relaxed);
    }
};

// ============================================================================
// Layout Helpers — Filter (5)
// ============================================================================
inline void addFilterParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) {
    detail::addChoiceParameter(params, ParamIDs::filterMode, "Filter Mode", getFilterModeChoices(), 1); // Default LP24
    detail::addFloatSkewParameter(params, ParamIDs::filterCutoff, "Cutoff Frequency", 20.0f, 20000.0f, 1200.0f, 1000.0f, 0.1f, "Hz");
    detail::addFloatParameter(params, ParamIDs::filterResonance, "Resonance", 0.0f, 1.0f, 0.2f, 0.001f, "%");
    detail::addFloatParameter(params, ParamIDs::filterKbTrack,   "Keyboard Track", 0.0f, 1.0f, 0.5f, 0.001f, "%");
    detail::addFloatParameter(params, ParamIDs::filterEnvAmount, "Filter Env Amount", -1.0f, 1.0f, 0.0f, 0.001f, "%");
}

inline std::vector<std::unique_ptr<juce::RangedAudioParameter>> createFilterParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    addFilterParameters(params);
    return params;
}

} // namespace bumbler
