#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../dsp/ParameterSnapshot.h"
#include "ParameterHelpers.h"
#include <vector>
#include <memory>
#include <atomic>

namespace bumbler {

// ============================================================================
// Parameter IDs (Type-safe APVTS identifiers) — Oscillators (14)
// ============================================================================
namespace ParamIDs {
    inline const juce::ParameterID osc1Waveform  { "osc1Waveform", 1 };
    inline const juce::ParameterID osc1Octave    { "osc1Octave", 1 };
    inline const juce::ParameterID osc1Fine      { "osc1Fine", 1 };
    inline const juce::ParameterID osc2Waveform  { "osc2Waveform", 1 };
    inline const juce::ParameterID osc2Octave    { "osc2Octave", 1 };
    inline const juce::ParameterID osc2Fine      { "osc2Fine", 1 };
    inline const juce::ParameterID oscMix        { "oscMix", 1 };
    inline const juce::ParameterID osc3Waveform  { "osc3Waveform", 1 };
    inline const juce::ParameterID osc3Level     { "osc3Level", 1 };
    inline const juce::ParameterID ringModMix    { "ringModMix", 1 };
    inline const juce::ParameterID pulseWidth    { "pulseWidth", 1 };
    inline const juce::ParameterID fmAmount      { "fmAmount", 1 };
    inline const juce::ParameterID velToAmp      { "velToAmp", 1 };
    inline const juce::ParameterID velToFilter   { "velToFilter", 1 };
} // namespace ParamIDs

// ============================================================================
// Choice String Arrays — Oscillators
// ============================================================================
inline const juce::StringArray& getOscWaveformChoices() {
    static const juce::StringArray choices { "Saw", "Square", "Sine", "Noise" };
    return choices;
}

inline const juce::StringArray& getOsc3WaveformChoices() {
    static const juce::StringArray choices { "Square", "Saw" };
    return choices;
}

// ============================================================================
// Cached Atomic Pointers — Oscillators
// ============================================================================
struct OscillatorAtomicPointers {
    std::atomic<float>* osc1Waveform  { nullptr };
    std::atomic<float>* osc1Octave    { nullptr };
    std::atomic<float>* osc1Fine      { nullptr };
    std::atomic<float>* osc2Waveform  { nullptr };
    std::atomic<float>* osc2Octave    { nullptr };
    std::atomic<float>* osc2Fine      { nullptr };
    std::atomic<float>* oscMix        { nullptr };
    std::atomic<float>* osc3Waveform  { nullptr };
    std::atomic<float>* osc3Level     { nullptr };
    std::atomic<float>* ringModMix    { nullptr };
    std::atomic<float>* pulseWidth    { nullptr };
    std::atomic<float>* fmAmount      { nullptr };
    std::atomic<float>* velToAmp      { nullptr };
    std::atomic<float>* velToFilter   { nullptr };

    void initialize(juce::AudioProcessorValueTreeState& apvts) noexcept {
        osc1Waveform  = apvts.getRawParameterValue(ParamIDs::osc1Waveform.getParamID());
        osc1Octave    = apvts.getRawParameterValue(ParamIDs::osc1Octave.getParamID());
        osc1Fine      = apvts.getRawParameterValue(ParamIDs::osc1Fine.getParamID());
        osc2Waveform  = apvts.getRawParameterValue(ParamIDs::osc2Waveform.getParamID());
        osc2Octave    = apvts.getRawParameterValue(ParamIDs::osc2Octave.getParamID());
        osc2Fine      = apvts.getRawParameterValue(ParamIDs::osc2Fine.getParamID());
        oscMix        = apvts.getRawParameterValue(ParamIDs::oscMix.getParamID());
        osc3Waveform  = apvts.getRawParameterValue(ParamIDs::osc3Waveform.getParamID());
        osc3Level     = apvts.getRawParameterValue(ParamIDs::osc3Level.getParamID());
        ringModMix    = apvts.getRawParameterValue(ParamIDs::ringModMix.getParamID());
        pulseWidth    = apvts.getRawParameterValue(ParamIDs::pulseWidth.getParamID());
        fmAmount      = apvts.getRawParameterValue(ParamIDs::fmAmount.getParamID());
        velToAmp      = apvts.getRawParameterValue(ParamIDs::velToAmp.getParamID());
        velToFilter   = apvts.getRawParameterValue(ParamIDs::velToFilter.getParamID());
    }

    void loadSnapshot(ParameterSnapshot& s) const noexcept {
        if (osc1Waveform)  s.osc1Waveform  = osc1Waveform->load(std::memory_order_relaxed);
        if (osc1Octave)    s.osc1Octave    = osc1Octave->load(std::memory_order_relaxed);
        if (osc1Fine)      s.osc1Fine      = osc1Fine->load(std::memory_order_relaxed);
        if (osc2Waveform)  s.osc2Waveform  = osc2Waveform->load(std::memory_order_relaxed);
        if (osc2Octave)    s.osc2Octave    = osc2Octave->load(std::memory_order_relaxed);
        if (osc2Fine)      s.osc2Fine      = osc2Fine->load(std::memory_order_relaxed);
        if (oscMix)        s.oscMix        = oscMix->load(std::memory_order_relaxed);
        if (osc3Waveform)  s.osc3Waveform  = osc3Waveform->load(std::memory_order_relaxed);
        if (osc3Level)     s.osc3Level     = osc3Level->load(std::memory_order_relaxed);
        if (ringModMix)    s.ringModMix    = ringModMix->load(std::memory_order_relaxed);
        if (pulseWidth)    s.pulseWidth    = pulseWidth->load(std::memory_order_relaxed);
        if (fmAmount)      s.fmAmount      = fmAmount->load(std::memory_order_relaxed);
        if (velToAmp)      s.velToAmp      = velToAmp->load(std::memory_order_relaxed);
        if (velToFilter)   s.velToFilter   = velToFilter->load(std::memory_order_relaxed);
    }
};

// ============================================================================
// Layout Helpers — Oscillators (14)
// ============================================================================
inline void addOscillatorParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) {
    detail::addChoiceParameter(params, ParamIDs::osc1Waveform, "OSC 1 Waveform", getOscWaveformChoices(), 0);
    detail::addFloatParameter(params, ParamIDs::osc1Octave,   "OSC 1 Octave", -3.0f, 3.0f, 0.0f, 1.0f, "oct");
    detail::addFloatParameter(params, ParamIDs::osc1Fine,     "OSC 1 Fine", -1.0f, 1.0f, 0.0f, 0.01f, "st");
    detail::addChoiceParameter(params, ParamIDs::osc2Waveform, "OSC 2 Waveform", getOscWaveformChoices(), 0);
    detail::addFloatParameter(params, ParamIDs::osc2Octave,   "OSC 2 Octave", -3.0f, 3.0f, 0.0f, 1.0f, "oct");
    detail::addFloatParameter(params, ParamIDs::osc2Fine,     "OSC 2 Fine", -1.0f, 1.0f, 0.0f, 0.01f, "st");
    detail::addFloatParameter(params, ParamIDs::oscMix,       "OSC Mix", 0.0f, 1.0f, 0.5f, 0.001f, "%");
    detail::addChoiceParameter(params, ParamIDs::osc3Waveform, "OSC 3 Waveform", getOsc3WaveformChoices(), 0);
    detail::addFloatParameter(params, ParamIDs::osc3Level,    "OSC 3 Level", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    detail::addFloatParameter(params, ParamIDs::ringModMix,   "Ring Mod Mix", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    detail::addFloatParameter(params, ParamIDs::pulseWidth,   "Pulse Width", 0.01f, 0.99f, 0.5f, 0.001f, "%");
    detail::addFloatParameter(params, ParamIDs::fmAmount,     "FM Amount", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    detail::addFloatParameter(params, ParamIDs::velToAmp,     "Velocity to Amp", 0.0f, 1.0f, 0.5f, 0.001f, "%");
    detail::addFloatParameter(params, ParamIDs::velToFilter,  "Velocity to Filter", 0.0f, 1.0f, 0.5f, 0.001f, "%");
}

inline std::vector<std::unique_ptr<juce::RangedAudioParameter>> createOscillatorParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    addOscillatorParameters(params);
    return params;
}

} // namespace bumbler
