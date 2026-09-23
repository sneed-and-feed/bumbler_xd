#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/BumblerCommon.h"
#include "../dsp/ParameterSnapshot.h"
#include "../dsp/BumblerEngine.h"
#include "Parameters.h"
#include <atomic>

namespace bumbler {

class BumblerAudioProcessor : public juce::AudioProcessor {
public:
    BumblerAudioProcessor();
    ~BumblerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Bumbler XD"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Preset & Program Management
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void loadPreset(int index);

    // DAW State Recall
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return mApvts; }
    const juce::AudioProcessorValueTreeState& getApvts() const noexcept { return mApvts; }

    [[nodiscard]] ParameterSnapshot getCurrentSnapshot() const noexcept {
        return mAtomicPointers.loadSnapshot();
    }

    [[nodiscard]] BumblerEngine& getEngine() noexcept { return mEngine; }

private:
    juce::AudioProcessorValueTreeState mApvts;
    BumblerAtomicPointers mAtomicPointers;
    BumblerEngine mEngine;

    std::atomic<int> mCurrentProgramIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BumblerAudioProcessor)
};

} // namespace bumbler
