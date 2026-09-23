#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../dsp/ParameterSnapshot.h"
#include "ParameterHelpers.h"
#include <vector>
#include <memory>
#include <atomic>

namespace bumbler {

// ============================================================================
// Parameter IDs (Type-safe APVTS identifiers) — Dual LFOs (16)
// ============================================================================
namespace ParamIDs {
    inline const juce::ParameterID lfo1Waveform { "lfo1Waveform", 1 };
    inline const juce::ParameterID lfo1Rate     { "lfo1Rate", 1 };
    inline const juce::ParameterID lfo1Delay    { "lfo1Delay", 1 };
    inline const juce::ParameterID lfo1Sync     { "lfo1Sync", 1 };
    inline const juce::ParameterID lfo1SyncDiv  { "lfo1SyncDiv", 1 };
    inline const juce::ParameterID lfo1KeyReset { "lfo1KeyReset", 1 };
    inline const juce::ParameterID lfo1Amount   { "lfo1Amount", 1 };
    inline const juce::ParameterID lfo1Target   { "lfo1Target", 1 };

    inline const juce::ParameterID lfo2Waveform { "lfo2Waveform", 1 };
    inline const juce::ParameterID lfo2Rate     { "lfo2Rate", 1 };
    inline const juce::ParameterID lfo2Delay    { "lfo2Delay", 1 };
    inline const juce::ParameterID lfo2Sync     { "lfo2Sync", 1 };
    inline const juce::ParameterID lfo2SyncDiv  { "lfo2SyncDiv", 1 };
    inline const juce::ParameterID lfo2KeyReset { "lfo2KeyReset", 1 };
    inline const juce::ParameterID lfo2Amount   { "lfo2Amount", 1 };
    inline const juce::ParameterID lfo2Target   { "lfo2Target", 1 };
} // namespace ParamIDs

// ============================================================================
// Choice String Arrays — Dual LFOs
// ============================================================================
inline const juce::StringArray& getLfoWaveformChoices() {
    static const juce::StringArray choices { "Saw", "Square", "Sine", "Noise" };
    return choices;
}

inline const juce::StringArray& getLfoSyncDivChoices() {
    static const juce::StringArray choices { "1/32", "1/16", "1/8", "1/4", "1/2", "1/1" };
    return choices;
}

inline const juce::StringArray& getLfo1TargetChoices() {
    static const juce::StringArray choices { "Osc12Pitch", "FilterCutoff", "PulseWidth" };
    return choices;
}

inline const juce::StringArray& getLfo2TargetChoices() {
    static const juce::StringArray choices { "Osc1Pitch", "OscMix", "MasterAmp" };
    return choices;
}

// ============================================================================
// Cached Atomic Pointers — Dual LFOs
// ============================================================================
struct LfoAtomicPointers {
    std::atomic<float>* lfo1Waveform { nullptr };
    std::atomic<float>* lfo1Rate     { nullptr };
    std::atomic<float>* lfo1Delay    { nullptr };
    std::atomic<float>* lfo1Sync     { nullptr };
    std::atomic<float>* lfo1SyncDiv  { nullptr };
    std::atomic<float>* lfo1KeyReset { nullptr };
    std::atomic<float>* lfo1Amount   { nullptr };
    std::atomic<float>* lfo1Target   { nullptr };

    std::atomic<float>* lfo2Waveform { nullptr };
    std::atomic<float>* lfo2Rate     { nullptr };
    std::atomic<float>* lfo2Delay    { nullptr };
    std::atomic<float>* lfo2Sync     { nullptr };
    std::atomic<float>* lfo2SyncDiv  { nullptr };
    std::atomic<float>* lfo2KeyReset { nullptr };
    std::atomic<float>* lfo2Amount   { nullptr };
    std::atomic<float>* lfo2Target   { nullptr };

    void initialize(juce::AudioProcessorValueTreeState& apvts) noexcept {
        lfo1Waveform = apvts.getRawParameterValue(ParamIDs::lfo1Waveform.getParamID());
        lfo1Rate     = apvts.getRawParameterValue(ParamIDs::lfo1Rate.getParamID());
        lfo1Delay    = apvts.getRawParameterValue(ParamIDs::lfo1Delay.getParamID());
        lfo1Sync     = apvts.getRawParameterValue(ParamIDs::lfo1Sync.getParamID());
        lfo1SyncDiv  = apvts.getRawParameterValue(ParamIDs::lfo1SyncDiv.getParamID());
        lfo1KeyReset = apvts.getRawParameterValue(ParamIDs::lfo1KeyReset.getParamID());
        lfo1Amount   = apvts.getRawParameterValue(ParamIDs::lfo1Amount.getParamID());
        lfo1Target   = apvts.getRawParameterValue(ParamIDs::lfo1Target.getParamID());

        lfo2Waveform = apvts.getRawParameterValue(ParamIDs::lfo2Waveform.getParamID());
        lfo2Rate     = apvts.getRawParameterValue(ParamIDs::lfo2Rate.getParamID());
        lfo2Delay    = apvts.getRawParameterValue(ParamIDs::lfo2Delay.getParamID());
        lfo2Sync     = apvts.getRawParameterValue(ParamIDs::lfo2Sync.getParamID());
        lfo2SyncDiv  = apvts.getRawParameterValue(ParamIDs::lfo2SyncDiv.getParamID());
        lfo2KeyReset = apvts.getRawParameterValue(ParamIDs::lfo2KeyReset.getParamID());
        lfo2Amount   = apvts.getRawParameterValue(ParamIDs::lfo2Amount.getParamID());
        lfo2Target   = apvts.getRawParameterValue(ParamIDs::lfo2Target.getParamID());
    }

    void loadSnapshot(ParameterSnapshot& s) const noexcept {
        if (lfo1Waveform) s.lfo1Waveform = lfo1Waveform->load(std::memory_order_relaxed);
        if (lfo1Rate)     s.lfo1Rate     = lfo1Rate->load(std::memory_order_relaxed);
        if (lfo1Delay)    s.lfo1Delay    = lfo1Delay->load(std::memory_order_relaxed);
        if (lfo1Sync)     s.lfo1Sync     = lfo1Sync->load(std::memory_order_relaxed);
        if (lfo1SyncDiv)  s.lfo1SyncDiv  = lfo1SyncDiv->load(std::memory_order_relaxed);
        if (lfo1KeyReset) s.lfo1KeyReset = lfo1KeyReset->load(std::memory_order_relaxed);
        if (lfo1Amount)   s.lfo1Amount   = lfo1Amount->load(std::memory_order_relaxed);
        if (lfo1Target)   s.lfo1Target   = lfo1Target->load(std::memory_order_relaxed);

        if (lfo2Waveform) s.lfo2Waveform = lfo2Waveform->load(std::memory_order_relaxed);
        if (lfo2Rate)     s.lfo2Rate     = lfo2Rate->load(std::memory_order_relaxed);
        if (lfo2Delay)    s.lfo2Delay    = lfo2Delay->load(std::memory_order_relaxed);
        if (lfo2Sync)     s.lfo2Sync     = lfo2Sync->load(std::memory_order_relaxed);
        if (lfo2SyncDiv)  s.lfo2SyncDiv  = lfo2SyncDiv->load(std::memory_order_relaxed);
        if (lfo2KeyReset) s.lfo2KeyReset = lfo2KeyReset->load(std::memory_order_relaxed);
        if (lfo2Amount)   s.lfo2Amount   = lfo2Amount->load(std::memory_order_relaxed);
        if (lfo2Target)   s.lfo2Target   = lfo2Target->load(std::memory_order_relaxed);
    }
};

// ============================================================================
// Layout Helpers — Dual LFOs (16)
// ============================================================================
inline void addLfoParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) {
    detail::addChoiceParameter(params,    ParamIDs::lfo1Waveform, "LFO 1 Waveform", getLfoWaveformChoices(), 2);
    detail::addFloatSkewParameter(params, ParamIDs::lfo1Rate,     "LFO 1 Rate", 0.05f, 30.0f, 2.0f, 2.0f, 0.01f, "Hz");
    detail::addFloatParameter(params,     ParamIDs::lfo1Delay,    "LFO 1 Delay", 0.0f, 5.0f, 0.0f, 0.01f, "s");
    detail::addBoolParameter(params,      ParamIDs::lfo1Sync,     "LFO 1 Tempo Sync", false);
    detail::addChoiceParameter(params,    ParamIDs::lfo1SyncDiv,  "LFO 1 Sync Division", getLfoSyncDivChoices(), 3); // 1/4 default
    detail::addBoolParameter(params,      ParamIDs::lfo1KeyReset, "LFO 1 Key Reset", true);
    detail::addFloatParameter(params,     ParamIDs::lfo1Amount,   "LFO 1 Amount", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    detail::addChoiceParameter(params,    ParamIDs::lfo1Target,   "LFO 1 Target", getLfo1TargetChoices(), 1);

    detail::addChoiceParameter(params,    ParamIDs::lfo2Waveform, "LFO 2 Waveform", getLfoWaveformChoices(), 2);
    detail::addFloatSkewParameter(params, ParamIDs::lfo2Rate,     "LFO 2 Rate", 0.05f, 30.0f, 1.0f, 2.0f, 0.01f, "Hz");
    detail::addFloatParameter(params,     ParamIDs::lfo2Delay,    "LFO 2 Delay", 0.0f, 5.0f, 0.0f, 0.01f, "s");
    detail::addBoolParameter(params,      ParamIDs::lfo2Sync,     "LFO 2 Tempo Sync", false);
    detail::addChoiceParameter(params,    ParamIDs::lfo2SyncDiv,  "LFO 2 Sync Division", getLfoSyncDivChoices(), 4); // 1/2 default
    detail::addBoolParameter(params,      ParamIDs::lfo2KeyReset, "LFO 2 Key Reset", true);
    detail::addFloatParameter(params,     ParamIDs::lfo2Amount,   "LFO 2 Amount", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    detail::addChoiceParameter(params,    ParamIDs::lfo2Target,   "LFO 2 Target", getLfo2TargetChoices(), 0);
}

inline std::vector<std::unique_ptr<juce::RangedAudioParameter>> createLfoParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    addLfoParameters(params);
    return params;
}

} // namespace bumbler
