#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerVoice.h"

namespace bumbler {

class BumblerVoiceManager {
public:
    static constexpr int kMaxVoices = 16;

    BumblerVoiceManager() noexcept = default;

    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;

    // MIDI Note & Transport Dispatch
    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote, float velocity = 0.0f) noexcept;
    void setSustainPedal(bool pedalDown) noexcept;
    void setPitchBend(float semitones) noexcept;
    void allNotesOff(bool fastKill = false) noexcept;

    // Polyphony Configuration (1 to 16)
    void setPolyphonyLimit(int limit) noexcept;
    [[nodiscard]] int getPolyphonyLimit() const noexcept { return mMaxPolyphony; }

    // State Inspection
    [[nodiscard]] int getNumActiveVoices() const noexcept;
    [[nodiscard]] const BumblerVoice& getVoice(int index) const noexcept { return mVoices[static_cast<size_t>(index)]; }
    [[nodiscard]] BumblerVoice& getVoice(int index) noexcept { return mVoices[static_cast<size_t>(index)]; }

    // Filter Enablement
    void setFilterEnabled(bool enabled) noexcept {
        mFilterEnabled = enabled;
        for (auto& v : mVoices) {
            v.setFilterEnabled(enabled);
        }
    }
    [[nodiscard]] bool isFilterEnabled() const noexcept { return mFilterEnabled; }

    // Host Tempo Dispatch (BPM)
    void setHostBpm(float bpm) noexcept {
        mHostBpm = (bpm > 20.0f && bpm < 400.0f) ? bpm : 120.0f;
        for (auto& v : mVoices) {
            v.setHostBpm(mHostBpm);
        }
    }
    [[nodiscard]] float getHostBpm() const noexcept { return mHostBpm; }

    // Character Circuits Output Stage
    [[nodiscard]] CharacterCircuits& getCharacterCircuits() noexcept { return mCharacterCircuits; }
    [[nodiscard]] const CharacterCircuits& getCharacterCircuits() const noexcept { return mCharacterCircuits; }
    void setAnalogMode(bool enabled) noexcept {
        mAnalogMode = enabled;
        for (auto& v : mVoices) {
            v.setAnalogMode(enabled);
        }
    }
    [[nodiscard]] bool isAnalogMode() const noexcept { return mAnalogMode; }

    // Audio Rendering: Clears and accumulates up to 16 voices into outputChannels
    void renderBlock(float* const* outputChannels, int numChannels, int numSamples, const ParameterSnapshot& params) noexcept;

private:
    [[nodiscard]] BumblerVoice* allocateVoice(int midiNote) noexcept;

    double mSampleRate { 48000.0 };
    int mMaxBlockSize { 512 };
    int mMaxPolyphony { kMaxVoices };
    int mLastAllocatedIndex { 0 };
    uint64_t mSampleCounter { 0 };
    uint64_t mNoteTriggerCounter { 0 };

    bool mSustainPedal { false };
    float mPitchBendSemitones { 0.0f };
    bool mFilterEnabled { false };
    float mHostBpm { 120.0f };
    bool mAnalogMode { false };

    CharacterCircuits mCharacterCircuits;
    std::array<BumblerVoice, kMaxVoices> mVoices;
    std::array<float, 8192> mMonoScratchBuffer {};
};

} // namespace bumbler
