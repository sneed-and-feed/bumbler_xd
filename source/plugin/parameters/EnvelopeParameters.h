#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../dsp/ParameterSnapshot.h"
#include "ParameterHelpers.h"
#include <vector>
#include <memory>
#include <atomic>

namespace bumbler {

// ============================================================================
// Parameter IDs (Type-safe APVTS identifiers) — Envelopes (13)
// ============================================================================
namespace ParamIDs {
    inline const juce::ParameterID ampAttack     { "ampAttack", 1 };
    inline const juce::ParameterID ampDecay      { "ampDecay", 1 };
    inline const juce::ParameterID ampSustain    { "ampSustain", 1 };
    inline const juce::ParameterID ampRelease    { "ampRelease", 1 };
    inline const juce::ParameterID filterAttack  { "filterAttack", 1 };
    inline const juce::ParameterID filterDecay   { "filterDecay", 1 };
    inline const juce::ParameterID filterSustain { "filterSustain", 1 };
    inline const juce::ParameterID filterRelease { "filterRelease", 1 };
    inline const juce::ParameterID envLink       { "envLink", 1 };
    inline const juce::ParameterID modAttack     { "modAttack", 1 };
    inline const juce::ParameterID modDecay      { "modDecay", 1 };
    inline const juce::ParameterID modAmount     { "modAmount", 1 };
    inline const juce::ParameterID modTarget     { "modTarget", 1 };
} // namespace ParamIDs

// ============================================================================
// Choice String Arrays — Envelopes
// ============================================================================
inline const juce::StringArray& getModTargetChoices() {
    static const juce::StringArray choices { "PW", "Lfo1Amt", "Osc1Level", "Osc2Pitch" };
    return choices;
}

// ============================================================================
// Cached Atomic Pointers — Envelopes
// ============================================================================
struct EnvelopeAtomicPointers {
    std::atomic<float>* ampAttack     { nullptr };
    std::atomic<float>* ampDecay      { nullptr };
    std::atomic<float>* ampSustain    { nullptr };
    std::atomic<float>* ampRelease    { nullptr };
    std::atomic<float>* filterAttack  { nullptr };
    std::atomic<float>* filterDecay   { nullptr };
    std::atomic<float>* filterSustain { nullptr };
    std::atomic<float>* filterRelease { nullptr };
    std::atomic<float>* envLink       { nullptr };
    std::atomic<float>* modAttack     { nullptr };
    std::atomic<float>* modDecay      { nullptr };
    std::atomic<float>* modAmount     { nullptr };
    std::atomic<float>* modTarget     { nullptr };

    void initialize(juce::AudioProcessorValueTreeState& apvts) noexcept {
        ampAttack     = apvts.getRawParameterValue(ParamIDs::ampAttack.getParamID());
        ampDecay      = apvts.getRawParameterValue(ParamIDs::ampDecay.getParamID());
        ampSustain    = apvts.getRawParameterValue(ParamIDs::ampSustain.getParamID());
        ampRelease    = apvts.getRawParameterValue(ParamIDs::ampRelease.getParamID());
        filterAttack  = apvts.getRawParameterValue(ParamIDs::filterAttack.getParamID());
        filterDecay   = apvts.getRawParameterValue(ParamIDs::filterDecay.getParamID());
        filterSustain = apvts.getRawParameterValue(ParamIDs::filterSustain.getParamID());
        filterRelease = apvts.getRawParameterValue(ParamIDs::filterRelease.getParamID());
        envLink       = apvts.getRawParameterValue(ParamIDs::envLink.getParamID());
        modAttack     = apvts.getRawParameterValue(ParamIDs::modAttack.getParamID());
        modDecay      = apvts.getRawParameterValue(ParamIDs::modDecay.getParamID());
        modAmount     = apvts.getRawParameterValue(ParamIDs::modAmount.getParamID());
        modTarget     = apvts.getRawParameterValue(ParamIDs::modTarget.getParamID());
    }

    void loadSnapshot(ParameterSnapshot& s) const noexcept {
        if (ampAttack)     s.ampAttack     = ampAttack->load(std::memory_order_relaxed);
        if (ampDecay)      s.ampDecay      = ampDecay->load(std::memory_order_relaxed);
        if (ampSustain)    s.ampSustain    = ampSustain->load(std::memory_order_relaxed);
        if (ampRelease)    s.ampRelease    = ampRelease->load(std::memory_order_relaxed);
        if (filterAttack)  s.filterAttack  = filterAttack->load(std::memory_order_relaxed);
        if (filterDecay)   s.filterDecay   = filterDecay->load(std::memory_order_relaxed);
        if (filterSustain) s.filterSustain = filterSustain->load(std::memory_order_relaxed);
        if (filterRelease) s.filterRelease = filterRelease->load(std::memory_order_relaxed);
        if (envLink)       s.envLink       = envLink->load(std::memory_order_relaxed);
        if (modAttack)     s.modAttack     = modAttack->load(std::memory_order_relaxed);
        if (modDecay)      s.modDecay      = modDecay->load(std::memory_order_relaxed);
        if (modAmount)     s.modAmount     = modAmount->load(std::memory_order_relaxed);
        if (modTarget)     s.modTarget     = modTarget->load(std::memory_order_relaxed);
    }
};

// ============================================================================
// Layout Helpers — Envelopes (13)
// ============================================================================
inline void addEnvelopeParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) {
    detail::addFloatSkewParameter(params, ParamIDs::ampAttack,  "Amp Attack", 0.001f, 10.0f, 0.01f, 0.5f, 0.001f, "s");
    detail::addFloatSkewParameter(params, ParamIDs::ampDecay,   "Amp Decay", 0.001f, 10.0f, 0.30f, 1.0f, 0.001f, "s");
    detail::addFloatParameter(params,     ParamIDs::ampSustain, "Amp Sustain", 0.0f, 1.0f, 0.80f, 0.001f, "%");
    detail::addFloatSkewParameter(params, ParamIDs::ampRelease, "Amp Release", 0.001f, 10.0f, 0.30f, 1.2f, 0.001f, "s");

    detail::addFloatSkewParameter(params, ParamIDs::filterAttack,  "Filter Attack", 0.001f, 10.0f, 0.05f, 0.5f, 0.001f, "s");
    detail::addFloatSkewParameter(params, ParamIDs::filterDecay,   "Filter Decay", 0.001f, 10.0f, 0.50f, 1.0f, 0.001f, "s");
    detail::addFloatParameter(params,     ParamIDs::filterSustain, "Filter Sustain", 0.0f, 1.0f, 0.50f, 0.001f, "%");
    detail::addFloatSkewParameter(params, ParamIDs::filterRelease, "Filter Release", 0.001f, 10.0f, 0.40f, 1.2f, 0.001f, "s");
    detail::addBoolParameter(params,      ParamIDs::envLink,       "Envelope Link", false);

    detail::addFloatSkewParameter(params, ParamIDs::modAttack, "Mod Attack", 0.001f, 5.0f, 0.05f, 0.5f, 0.001f, "s");
    detail::addFloatSkewParameter(params, ParamIDs::modDecay,  "Mod Decay", 0.001f, 10.0f, 0.50f, 0.5f, 0.001f, "s");
    detail::addFloatParameter(params,     ParamIDs::modAmount, "Mod Amount", -1.0f, 1.0f, 0.0f, 0.001f, "%");
    detail::addChoiceParameter(params,    ParamIDs::modTarget, "Mod Target", getModTargetChoices(), 0);
}

inline std::vector<std::unique_ptr<juce::RangedAudioParameter>> createEnvelopeParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    addEnvelopeParameters(params);
    return params;
}

} // namespace bumbler
