#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/ParameterSnapshot.h"

// Domain-specific parameter headers
#include "parameters/ParameterHelpers.h"
#include "parameters/OscillatorParameters.h"
#include "parameters/FilterParameters.h"
#include "parameters/EnvelopeParameters.h"
#include "parameters/LfoParameters.h"
#include "parameters/CharacterParameters.h"
#include "parameters/PresetParameters.h"

#include <array>
#include <vector>
#include <memory>
#include <atomic>

namespace bumbler {

// ============================================================================
// Cached Atomic Pointers for Lock-Free Audio-Thread Snapshot Extraction
// ============================================================================
struct BumblerAtomicPointers : public OscillatorAtomicPointers,
                               public FilterAtomicPointers,
                               public EnvelopeAtomicPointers,
                               public LfoAtomicPointers,
                               public CharacterAtomicPointers
{
    void initialize(juce::AudioProcessorValueTreeState& apvts) noexcept {
        OscillatorAtomicPointers::initialize(apvts);
        FilterAtomicPointers::initialize(apvts);
        EnvelopeAtomicPointers::initialize(apvts);
        LfoAtomicPointers::initialize(apvts);
        CharacterAtomicPointers::initialize(apvts);
    }

    [[nodiscard]] ParameterSnapshot loadSnapshot() const noexcept {
        ParameterSnapshot s;
        OscillatorAtomicPointers::loadSnapshot(s);
        FilterAtomicPointers::loadSnapshot(s);
        EnvelopeAtomicPointers::loadSnapshot(s);
        LfoAtomicPointers::loadSnapshot(s);
        CharacterAtomicPointers::loadSnapshot(s);
        return s;
    }
};

// ============================================================================
// APVTS ParameterLayout Factory
// ============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    addOscillatorParameters(params);
    addFilterParameters(params);
    addEnvelopeParameters(params);
    addLfoParameters(params);
    addCharacterParameters(params);

    return { params.begin(), params.end() };
}

} // namespace bumbler
