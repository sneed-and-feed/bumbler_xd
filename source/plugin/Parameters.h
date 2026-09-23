#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/ParameterSnapshot.h"
#include <array>
#include <vector>
#include <memory>
#include <string_view>
#include <cmath>

namespace bumbler {

// ============================================================================
// Parameter IDs (Type-safe APVTS identifiers)
// ============================================================================
namespace ParamIDs {
    // Oscillators (14)
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

    // Filter (5)
    inline const juce::ParameterID filterMode      { "filterMode", 1 };
    inline const juce::ParameterID filterCutoff    { "filterCutoff", 1 };
    inline const juce::ParameterID filterResonance { "filterResonance", 1 };
    inline const juce::ParameterID filterKbTrack   { "filterKbTrack", 1 };
    inline const juce::ParameterID filterEnvAmount { "filterEnvAmount", 1 };

    // Envelopes (13)
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

    // Dual LFOs (16)
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

    // Character & Output (7)
    inline const juce::ParameterID driveEnabled { "driveEnabled", 1 };
    inline const juce::ParameterID driveAmount  { "driveAmount", 1 };
    inline const juce::ParameterID driveTone    { "driveTone", 1 };
    inline const juce::ParameterID dualMode     { "dualMode", 1 };
    inline const juce::ParameterID analogMode   { "analogMode", 1 };
    inline const juce::ParameterID wNoiseMode   { "wNoiseMode", 1 };
    inline const juce::ParameterID masterVolume { "masterVolume", 1 };
}

// ============================================================================
// Choice String Arrays
// ============================================================================
inline const juce::StringArray& getOscWaveformChoices() {
    static const juce::StringArray choices { "Saw", "Square", "Sine", "Noise" };
    return choices;
}

inline const juce::StringArray& getOsc3WaveformChoices() {
    static const juce::StringArray choices { "Square", "Saw" };
    return choices;
}

inline const juce::StringArray& getFilterModeChoices() {
    static const juce::StringArray choices { "LP12", "LP24", "LP+NT", "DBL.NT", "BP24", "HP24" };
    return choices;
}

inline const juce::StringArray& getModTargetChoices() {
    static const juce::StringArray choices { "PW", "Lfo1Amt", "Osc1Level", "Osc2Pitch" };
    return choices;
}

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

inline const juce::StringArray& getNoiseModeChoices() {
    static const juce::StringArray choices { "Vintage Table", "White Noise" };
    return choices;
}

// ============================================================================
// Cached Atomic Pointers for Lock-Free Audio-Thread Snapshot Extraction
// ============================================================================
struct BumblerAtomicPointers {
    // Oscillators (14)
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

    // Filter (5)
    std::atomic<float>* filterMode      { nullptr };
    std::atomic<float>* filterCutoff    { nullptr };
    std::atomic<float>* filterResonance { nullptr };
    std::atomic<float>* filterKbTrack   { nullptr };
    std::atomic<float>* filterEnvAmount { nullptr };

    // Envelopes (13)
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

    // Dual LFOs (16)
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

    // Character & Output (7)
    std::atomic<float>* driveEnabled { nullptr };
    std::atomic<float>* driveAmount  { nullptr };
    std::atomic<float>* driveTone    { nullptr };
    std::atomic<float>* dualMode     { nullptr };
    std::atomic<float>* analogMode   { nullptr };
    std::atomic<float>* wNoiseMode   { nullptr };
    std::atomic<float>* masterVolume { nullptr };

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

        filterMode      = apvts.getRawParameterValue(ParamIDs::filterMode.getParamID());
        filterCutoff    = apvts.getRawParameterValue(ParamIDs::filterCutoff.getParamID());
        filterResonance = apvts.getRawParameterValue(ParamIDs::filterResonance.getParamID());
        filterKbTrack   = apvts.getRawParameterValue(ParamIDs::filterKbTrack.getParamID());
        filterEnvAmount = apvts.getRawParameterValue(ParamIDs::filterEnvAmount.getParamID());

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

        driveEnabled = apvts.getRawParameterValue(ParamIDs::driveEnabled.getParamID());
        driveAmount  = apvts.getRawParameterValue(ParamIDs::driveAmount.getParamID());
        driveTone    = apvts.getRawParameterValue(ParamIDs::driveTone.getParamID());
        dualMode     = apvts.getRawParameterValue(ParamIDs::dualMode.getParamID());
        analogMode   = apvts.getRawParameterValue(ParamIDs::analogMode.getParamID());
        wNoiseMode   = apvts.getRawParameterValue(ParamIDs::wNoiseMode.getParamID());
        masterVolume = apvts.getRawParameterValue(ParamIDs::masterVolume.getParamID());
    }

    [[nodiscard]] ParameterSnapshot loadSnapshot() const noexcept {
        ParameterSnapshot s;
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

        if (filterMode)      s.filterMode      = filterMode->load(std::memory_order_relaxed);
        if (filterCutoff)    s.filterCutoff    = filterCutoff->load(std::memory_order_relaxed);
        if (filterResonance) s.filterResonance = filterResonance->load(std::memory_order_relaxed);
        if (filterKbTrack)   s.filterKbTrack   = filterKbTrack->load(std::memory_order_relaxed);
        if (filterEnvAmount) s.filterEnvAmount = filterEnvAmount->load(std::memory_order_relaxed);

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

        if (driveEnabled) s.driveEnabled = driveEnabled->load(std::memory_order_relaxed);
        if (driveAmount)  s.driveAmount  = driveAmount->load(std::memory_order_relaxed);
        if (driveTone)    s.driveTone    = driveTone->load(std::memory_order_relaxed);
        if (dualMode)     s.dualMode     = dualMode->load(std::memory_order_relaxed);
        if (analogMode)   s.analogMode   = analogMode->load(std::memory_order_relaxed);
        if (wNoiseMode)   s.wNoiseMode   = wNoiseMode->load(std::memory_order_relaxed);
        if (masterVolume) s.masterVolume = masterVolume->load(std::memory_order_relaxed);

        return s;
    }
};

// ============================================================================
// Factory Presets: 5 Signature Sounds
// ============================================================================
struct PresetDefinition {
    const char* name;
    const char* category;
    ParameterSnapshot params;
};

inline const std::array<PresetDefinition, 5>& getFactoryPresets() {
    static const std::array<PresetDefinition, 5> presets = []() {
        std::array<PresetDefinition, 5> p {};

        // --------------------------------------------------------------------
        // 1. Acid Bass (Classic 303 / Wasp squelch bass)
        // --------------------------------------------------------------------
        p[0].name = "Acid Bass";
        p[0].category = "Bass";
        p[0].params.osc1Waveform  = 0.0f;  // Saw
        p[0].params.osc1Octave    = -1.0f; // -1 oct
        p[0].params.osc1Fine      = 0.0f;
        p[0].params.osc2Waveform  = 1.0f;  // Square
        p[0].params.osc2Octave    = -1.0f;
        p[0].params.osc2Fine      = 0.05f;
        p[0].params.oscMix        = 0.35f; // Favor Saw
        p[0].params.osc3Waveform  = 0.0f;  // Square sub
        p[0].params.osc3Level     = 0.25f;
        p[0].params.ringModMix    = 0.0f;
        p[0].params.pulseWidth    = 0.50f;
        p[0].params.fmAmount      = 0.0f;
        p[0].params.velToAmp      = 0.70f;
        p[0].params.velToFilter   = 0.85f; // Dynamic accent

        p[0].params.filterMode      = 1.0f;   // LP24
        p[0].params.filterCutoff    = 380.0f; // Low base cutoff
        p[0].params.filterResonance = 0.78f;  // Squelch resonance
        p[0].params.filterKbTrack   = 0.60f;
        p[0].params.filterEnvAmount = 0.75f;  // Plucky envelope sweep

        p[0].params.ampAttack     = 0.002f;
        p[0].params.ampDecay      = 0.22f;
        p[0].params.ampSustain    = 0.0f;
        p[0].params.ampRelease    = 0.15f;
        p[0].params.filterAttack  = 0.002f;
        p[0].params.filterDecay   = 0.28f;
        p[0].params.filterSustain = 0.05f;
        p[0].params.filterRelease = 0.18f;
        p[0].params.envLink       = 0.0f;

        p[0].params.modAttack     = 0.01f;
        p[0].params.modDecay      = 0.20f;
        p[0].params.modAmount     = 0.0f;
        p[0].params.modTarget     = 0.0f;

        p[0].params.lfo1Waveform = 2.0f;
        p[0].params.lfo1Rate     = 0.5f;
        p[0].params.lfo1Amount   = 0.0f;
        p[0].params.lfo2Waveform = 2.0f;
        p[0].params.lfo2Rate     = 1.0f;
        p[0].params.lfo2Amount   = 0.0f;

        p[0].params.driveEnabled = 1.0f;  // Saturation bite
        p[0].params.driveAmount  = 0.45f;
        p[0].params.driveTone    = 0.65f;
        p[0].params.dualMode     = 0.0f;  // Centered mono bass
        p[0].params.analogMode   = 1.0f;
        p[0].params.wNoiseMode   = 0.0f;
        p[0].params.masterVolume = 0.82f;

        // --------------------------------------------------------------------
        // 2. Sync Lead (Aggressive piercing sync with FM & pitch laser dive)
        // --------------------------------------------------------------------
        p[1].name = "Sync Lead";
        p[1].category = "Lead";
        p[1].params.osc1Waveform  = 0.0f;  // Saw
        p[1].params.osc1Octave    = 0.0f;
        p[1].params.osc1Fine      = 0.0f;
        p[1].params.osc2Waveform  = 0.0f;  // Saw
        p[1].params.osc2Octave    = 1.0f;  // +1 oct
        p[1].params.osc2Fine      = 0.12f; // Detuned
        p[1].params.oscMix        = 0.50f;
        p[1].params.osc3Waveform  = 0.0f;
        p[1].params.osc3Level     = 0.0f;
        p[1].params.ringModMix    = 0.15f; // Metallic edge
        p[1].params.pulseWidth    = 0.50f;
        p[1].params.fmAmount      = 0.35f; // Audio-rate FM grit
        p[1].params.velToAmp      = 0.50f;
        p[1].params.velToFilter   = 0.40f;

        p[1].params.filterMode      = 2.0f;    // LP+NT (formant cascade)
        p[1].params.filterCutoff    = 2400.0f;
        p[1].params.filterResonance = 0.55f;
        p[1].params.filterKbTrack   = 0.75f;
        p[1].params.filterEnvAmount = 0.35f;

        p[1].params.ampAttack     = 0.005f;
        p[1].params.ampDecay      = 0.60f;
        p[1].params.ampSustain    = 0.75f;
        p[1].params.ampRelease    = 0.35f;
        p[1].params.filterAttack  = 0.02f;
        p[1].params.filterDecay   = 0.80f;
        p[1].params.filterSustain = 0.40f;
        p[1].params.filterRelease = 0.40f;
        p[1].params.envLink       = 0.0f;

        p[1].params.modAttack     = 0.005f;
        p[1].params.modDecay      = 0.30f;
        p[1].params.modAmount     = 0.40f;
        p[1].params.modTarget     = 3.0f;  // ModEnv -> OSC 2 pitch dive

        p[1].params.lfo1Waveform = 2.0f;  // Sine
        p[1].params.lfo1Rate     = 5.2f;  // Delayed vibrato
        p[1].params.lfo1Delay    = 0.25f;
        p[1].params.lfo1Amount   = 0.18f;
        p[1].params.lfo1Target   = 0.0f;  // OSC 1+2 pitch

        p[1].params.driveEnabled = 1.0f;
        p[1].params.driveAmount  = 0.35f;
        p[1].params.driveTone    = 0.75f; // Bright cutting bite
        p[1].params.dualMode     = 1.0f;  // Wide stereo spread
        p[1].params.analogMode   = 1.0f;
        p[1].params.masterVolume = 0.78f;

        // --------------------------------------------------------------------
        // 3. Swarm Pad (Lush double-notch choir pad with slow PWM sweep)
        // --------------------------------------------------------------------
        p[2].name = "Swarm Pad";
        p[2].category = "Pad";
        p[2].params.osc1Waveform  = 1.0f;  // Square
        p[2].params.osc1Octave    = 0.0f;
        p[2].params.osc1Fine      = -0.08f;
        p[2].params.osc2Waveform  = 0.0f;  // Saw
        p[2].params.osc2Octave    = 0.0f;
        p[2].params.osc2Fine      = 0.08f;
        p[2].params.oscMix        = 0.50f;
        p[2].params.osc3Waveform  = 1.0f;  // Saw sub-layer
        p[2].params.osc3Level     = 0.20f;
        p[2].params.pulseWidth    = 0.50f;
        p[2].params.velToAmp      = 0.30f;
        p[2].params.velToFilter   = 0.40f;

        p[2].params.filterMode      = 3.0f;    // DBL.NT (Double Notch)
        p[2].params.filterCutoff    = 1800.0f;
        p[2].params.filterResonance = 0.65f;
        p[2].params.filterKbTrack   = 0.50f;
        p[2].params.filterEnvAmount = 0.25f;

        p[2].params.ampAttack     = 0.85f;  // Slow blooming swell
        p[2].params.ampDecay      = 1.80f;
        p[2].params.ampSustain    = 0.85f;
        p[2].params.ampRelease    = 1.50f;
        p[2].params.filterAttack  = 1.20f;
        p[2].params.filterDecay   = 2.00f;
        p[2].params.filterSustain = 0.70f;
        p[2].params.filterRelease = 1.80f;

        p[2].params.modAttack     = 0.50f;
        p[2].params.modDecay      = 3.00f;
        p[2].params.modAmount     = 0.30f;
        p[2].params.modTarget     = 0.0f;  // ModEnv -> Pulse Width

        p[2].params.lfo1Waveform = 2.0f;  // Sine
        p[2].params.lfo1Rate     = 0.35f; // Slow PWM sweep
        p[2].params.lfo1Amount   = 0.45f;
        p[2].params.lfo1Target   = 2.0f;  // Pulse Width

        p[2].params.lfo2Waveform = 2.0f;  // Sine
        p[2].params.lfo2Rate     = 0.22f; // Slow OSC Mix pan
        p[2].params.lfo2Amount   = 0.30f;
        p[2].params.lfo2Target   = 1.0f;  // OSC Mix

        p[2].params.driveEnabled = 0.0f;
        p[2].params.dualMode     = 1.0f;  // Massive stereo chorusing
        p[2].params.analogMode   = 1.0f;
        p[2].params.masterVolume = 0.75f;

        // --------------------------------------------------------------------
        // 4. Percussion (Snappy analog drum/zap transient with noise & BP24)
        // --------------------------------------------------------------------
        p[3].name = "Percussion";
        p[3].category = "Percussion";
        p[3].params.osc1Waveform  = 2.0f;  // Sine thump
        p[3].params.osc1Octave    = -1.0f;
        p[3].params.osc2Waveform  = 3.0f;  // Noise snap
        p[3].params.oscMix        = 0.40f;
        p[3].params.ringModMix    = 0.30f; // Metallic click
        p[3].params.fmAmount      = 0.15f;
        p[3].params.velToAmp      = 0.85f;
        p[3].params.velToFilter   = 0.90f;

        p[3].params.filterMode      = 4.0f;   // BP24 (Bandpass)
        p[3].params.filterCutoff    = 850.0f;
        p[3].params.filterResonance = 0.70f;
        p[3].params.filterKbTrack   = 0.20f;
        p[3].params.filterEnvAmount = 0.80f;  // Filter plunge

        p[3].params.ampAttack     = 0.001f; // 1ms instant punch
        p[3].params.ampDecay      = 0.14f;
        p[3].params.ampSustain    = 0.0f;
        p[3].params.ampRelease    = 0.08f;
        p[3].params.filterAttack  = 0.001f;
        p[3].params.filterDecay   = 0.09f;
        p[3].params.filterSustain = 0.0f;
        p[3].params.filterRelease = 0.06f;

        p[3].params.modAttack     = 0.001f;
        p[3].params.modDecay      = 0.06f;
        p[3].params.modAmount     = 0.85f;
        p[3].params.modTarget     = 3.0f;  // Fast pitch dive on OSC 2

        p[3].params.driveEnabled = 1.0f;  // Crunchy transient drive
        p[3].params.driveAmount  = 0.55f;
        p[3].params.driveTone    = 0.60f;
        p[3].params.dualMode     = 0.0f;  // Tight mono punch
        p[3].params.analogMode   = 0.0f;  // Phase-locked transient
        p[3].params.masterVolume = 0.85f;

        // --------------------------------------------------------------------
        // 5. Vintage Drone (Dark, brooding HP24 atmospheric drone with S&H)
        // --------------------------------------------------------------------
        p[4].name = "Vintage Drone";
        p[4].category = "Atmosphere";
        p[4].params.osc1Waveform  = 0.0f;  // Saw
        p[4].params.osc1Octave    = -2.0f; // Sub rumble
        p[4].params.osc1Fine      = -0.05f;
        p[4].params.osc2Waveform  = 3.0f;  // Noise bed
        p[4].params.osc2Octave    = -1.0f;
        p[4].params.oscMix        = 0.60f;
        p[4].params.osc3Waveform  = 1.0f;  // Saw sub
        p[4].params.osc3Level     = 0.35f;
        p[4].params.ringModMix    = 0.20f;
        p[4].params.fmAmount      = 0.05f;
        p[4].params.velToAmp      = 0.20f;
        p[4].params.velToFilter   = 0.20f;

        p[4].params.filterMode      = 5.0f;   // HP24
        p[4].params.filterCutoff    = 220.0f; // Highpass hollow resonance
        p[4].params.filterResonance = 0.60f;
        p[4].params.filterKbTrack   = 0.30f;
        p[4].params.filterEnvAmount = -0.20f; // Inverted breathing

        p[4].params.ampAttack     = 2.50f;
        p[4].params.ampDecay      = 3.00f;
        p[4].params.ampSustain    = 1.00f;  // Infinite hold drone
        p[4].params.ampRelease    = 2.50f;
        p[4].params.filterAttack  = 3.00f;
        p[4].params.filterDecay   = 4.00f;
        p[4].params.filterSustain = 0.90f;
        p[4].params.filterRelease = 3.00f;

        p[4].params.modAttack     = 1.00f;
        p[4].params.modDecay      = 4.00f;
        p[4].params.modAmount     = 0.25f;
        p[4].params.modTarget     = 1.0f;  // ModEnv -> LFO 1 depth

        p[4].params.lfo1Waveform = 3.0f;  // Noise (Sample & Hold)
        p[4].params.lfo1Rate     = 0.80f; // Random cutoff drift
        p[4].params.lfo1Amount   = 0.25f;
        p[4].params.lfo1Target   = 1.0f;  // Filter cutoff

        p[4].params.lfo2Waveform = 2.0f;  // Sine
        p[4].params.lfo2Rate     = 0.08f; // Ultra-slow pitch warble
        p[4].params.lfo2Amount   = 0.15f;
        p[4].params.lfo2Target   = 0.0f;  // OSC 1 pitch

        p[4].params.driveEnabled = 1.0f;
        p[4].params.driveAmount  = 0.25f;
        p[4].params.driveTone    = 0.35f; // Dark vintage warmth
        p[4].params.dualMode     = 1.0f;  // Atmospheric stereo field
        p[4].params.analogMode   = 1.0f;  // Free-running analog drift
        p[4].params.masterVolume = 0.72f;

        return p;
    }();
    return presets;
}

// ============================================================================
// APVTS ParameterLayout Factory
// ============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Helper lambdas
    auto addFloat = [&](const juce::ParameterID& id, const char* name, float min, float max, float def, float step = 0.001f, const char* unit = "") {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            id, name, juce::NormalisableRange<float>(min, max, step), def,
            unit[0] != '\0' ? juce::AudioParameterFloatAttributes().withLabel(unit) : juce::AudioParameterFloatAttributes()));
    };

    auto addFloatSkew = [&](const juce::ParameterID& id, const char* name, float min, float max, float def, float centre, float step = 0.001f, const char* unit = "") {
        juce::NormalisableRange<float> range(min, max, step);
        range.setSkewForCentre(centre);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            id, name, range, def,
            unit[0] != '\0' ? juce::AudioParameterFloatAttributes().withLabel(unit) : juce::AudioParameterFloatAttributes()));
    };

    auto addChoice = [&](const juce::ParameterID& id, const char* name, const juce::StringArray& choices, int def) {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(id, name, choices, def));
    };

    auto addBool = [&](const juce::ParameterID& id, const char* name, bool def) {
        params.push_back(std::make_unique<juce::AudioParameterBool>(id, name, def));
    };

    // Group 1: Oscillators & Voice Management (14)
    addChoice(ParamIDs::osc1Waveform, "OSC 1 Waveform", getOscWaveformChoices(), 0);
    addFloat(ParamIDs::osc1Octave,   "OSC 1 Octave", -3.0f, 3.0f, 0.0f, 1.0f, "oct");
    addFloat(ParamIDs::osc1Fine,     "OSC 1 Fine", -1.0f, 1.0f, 0.0f, 0.01f, "st");
    addChoice(ParamIDs::osc2Waveform, "OSC 2 Waveform", getOscWaveformChoices(), 0);
    addFloat(ParamIDs::osc2Octave,   "OSC 2 Octave", -3.0f, 3.0f, 0.0f, 1.0f, "oct");
    addFloat(ParamIDs::osc2Fine,     "OSC 2 Fine", -1.0f, 1.0f, 0.0f, 0.01f, "st");
    addFloat(ParamIDs::oscMix,       "OSC Mix", 0.0f, 1.0f, 0.5f, 0.001f, "%");
    addChoice(ParamIDs::osc3Waveform, "OSC 3 Waveform", getOsc3WaveformChoices(), 0);
    addFloat(ParamIDs::osc3Level,    "OSC 3 Level", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    addFloat(ParamIDs::ringModMix,   "Ring Mod Mix", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    addFloat(ParamIDs::pulseWidth,   "Pulse Width", 0.01f, 0.99f, 0.5f, 0.001f, "%");
    addFloat(ParamIDs::fmAmount,     "FM Amount", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    addFloat(ParamIDs::velToAmp,     "Velocity to Amp", 0.0f, 1.0f, 0.5f, 0.001f, "%");
    addFloat(ParamIDs::velToFilter,  "Velocity to Filter", 0.0f, 1.0f, 0.5f, 0.001f, "%");

    // Group 2: Filter (5)
    addChoice(ParamIDs::filterMode, "Filter Mode", getFilterModeChoices(), 1); // Default LP24
    addFloatSkew(ParamIDs::filterCutoff, "Cutoff Frequency", 20.0f, 20000.0f, 1200.0f, 1000.0f, 0.1f, "Hz");
    addFloat(ParamIDs::filterResonance, "Resonance", 0.0f, 1.0f, 0.2f, 0.001f, "%");
    addFloat(ParamIDs::filterKbTrack,   "Keyboard Track", 0.0f, 1.0f, 0.5f, 0.001f, "%");
    addFloat(ParamIDs::filterEnvAmount, "Filter Env Amount", -1.0f, 1.0f, 0.0f, 0.001f, "%");

    // Group 3: Envelopes (13)
    addFloatSkew(ParamIDs::ampAttack,  "Amp Attack", 0.001f, 10.0f, 0.01f, 0.5f, 0.001f, "s");
    addFloatSkew(ParamIDs::ampDecay,   "Amp Decay", 0.001f, 10.0f, 0.30f, 0.5f, 0.001f, "s");
    addFloat(ParamIDs::ampSustain,     "Amp Sustain", 0.0f, 1.0f, 0.80f, 0.001f, "%");
    addFloatSkew(ParamIDs::ampRelease, "Amp Release", 0.001f, 10.0f, 0.30f, 0.5f, 0.001f, "s");

    addFloatSkew(ParamIDs::filterAttack,  "Filter Attack", 0.001f, 10.0f, 0.05f, 0.5f, 0.001f, "s");
    addFloatSkew(ParamIDs::filterDecay,   "Filter Decay", 0.001f, 10.0f, 0.50f, 0.5f, 0.001f, "s");
    addFloat(ParamIDs::filterSustain,     "Filter Sustain", 0.0f, 1.0f, 0.50f, 0.001f, "%");
    addFloatSkew(ParamIDs::filterRelease, "Filter Release", 0.001f, 10.0f, 0.40f, 0.5f, 0.001f, "s");
    addBool(ParamIDs::envLink, "Envelope Link", false);

    addFloatSkew(ParamIDs::modAttack, "Mod Attack", 0.001f, 5.0f, 0.05f, 0.5f, 0.001f, "s");
    addFloatSkew(ParamIDs::modDecay,  "Mod Decay", 0.001f, 10.0f, 0.50f, 0.5f, 0.001f, "s");
    addFloat(ParamIDs::modAmount,     "Mod Amount", -1.0f, 1.0f, 0.0f, 0.001f, "%");
    addChoice(ParamIDs::modTarget,    "Mod Target", getModTargetChoices(), 0);

    // Group 4: Dual LFOs (16)
    addChoice(ParamIDs::lfo1Waveform, "LFO 1 Waveform", getLfoWaveformChoices(), 2);
    addFloatSkew(ParamIDs::lfo1Rate,  "LFO 1 Rate", 0.05f, 30.0f, 2.0f, 2.0f, 0.01f, "Hz");
    addFloat(ParamIDs::lfo1Delay,     "LFO 1 Delay", 0.0f, 5.0f, 0.0f, 0.01f, "s");
    addBool(ParamIDs::lfo1Sync,       "LFO 1 Tempo Sync", false);
    addChoice(ParamIDs::lfo1SyncDiv,  "LFO 1 Sync Division", getLfoSyncDivChoices(), 3); // 1/4 default
    addBool(ParamIDs::lfo1KeyReset,   "LFO 1 Key Reset", true);
    addFloat(ParamIDs::lfo1Amount,    "LFO 1 Amount", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    addChoice(ParamIDs::lfo1Target,   "LFO 1 Target", getLfo1TargetChoices(), 1);

    addChoice(ParamIDs::lfo2Waveform, "LFO 2 Waveform", getLfoWaveformChoices(), 2);
    addFloatSkew(ParamIDs::lfo2Rate,  "LFO 2 Rate", 0.05f, 30.0f, 1.0f, 2.0f, 0.01f, "Hz");
    addFloat(ParamIDs::lfo2Delay,     "LFO 2 Delay", 0.0f, 5.0f, 0.0f, 0.01f, "s");
    addBool(ParamIDs::lfo2Sync,       "LFO 2 Tempo Sync", false);
    addChoice(ParamIDs::lfo2SyncDiv,  "LFO 2 Sync Division", getLfoSyncDivChoices(), 4); // 1/2 default
    addBool(ParamIDs::lfo2KeyReset,   "LFO 2 Key Reset", true);
    addFloat(ParamIDs::lfo2Amount,    "LFO 2 Amount", 0.0f, 1.0f, 0.0f, 0.001f, "%");
    addChoice(ParamIDs::lfo2Target,   "LFO 2 Target", getLfo2TargetChoices(), 0);

    // Group 5: Character & Output (7)
    addBool(ParamIDs::driveEnabled, "Drive Enable", false);
    addFloat(ParamIDs::driveAmount, "Drive Amount", 0.0f, 1.0f, 0.30f, 0.001f, "%");
    addFloat(ParamIDs::driveTone,   "Drive Tone", 0.0f, 1.0f, 0.50f, 0.001f, "%");
    addBool(ParamIDs::dualMode,     "Dual Mode", false);
    addBool(ParamIDs::analogMode,   "Analog Mode", false);
    addChoice(ParamIDs::wNoiseMode, "Noise Type", getNoiseModeChoices(), 0);
    addFloatSkew(ParamIDs::masterVolume, "Master Volume", 0.0f, 1.0f, 0.80f, 0.5f, 0.001f, "%");

    return { params.begin(), params.end() };
}

} // namespace bumbler
