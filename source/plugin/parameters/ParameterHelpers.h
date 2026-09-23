#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace bumbler {
namespace detail {

inline void addFloatParameter(
    std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
    const juce::ParameterID& id,
    const char* name,
    float min,
    float max,
    float def,
    float step = 0.001f,
    const char* unit = "")
{
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        id, name, juce::NormalisableRange<float>(min, max, step), def,
        unit[0] != '\0' ? juce::AudioParameterFloatAttributes().withLabel(unit)
                        : juce::AudioParameterFloatAttributes()));
}

inline void addFloatSkewParameter(
    std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
    const juce::ParameterID& id,
    const char* name,
    float min,
    float max,
    float def,
    float centre,
    float step = 0.001f,
    const char* unit = "")
{
    juce::NormalisableRange<float> range(min, max, step);
    range.setSkewForCentre(centre);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        id, name, range, def,
        unit[0] != '\0' ? juce::AudioParameterFloatAttributes().withLabel(unit)
                        : juce::AudioParameterFloatAttributes()));
}

inline void addChoiceParameter(
    std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
    const juce::ParameterID& id,
    const char* name,
    const juce::StringArray& choices,
    int def)
{
    params.push_back(std::make_unique<juce::AudioParameterChoice>(id, name, choices, def));
}

inline void addBoolParameter(
    std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
    const juce::ParameterID& id,
    const char* name,
    bool def)
{
    params.push_back(std::make_unique<juce::AudioParameterBool>(id, name, def));
}

} // namespace detail
} // namespace bumbler
