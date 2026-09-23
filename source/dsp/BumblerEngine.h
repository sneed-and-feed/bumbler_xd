#pragma once

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerVoiceManager.h"

namespace bumbler {

/**
 * BumblerEngine: Mandated top-level core engine entry point for bumbler_dsp_core.
 * Conforms to PROJECT.md § Interface Contracts.
 */
class BumblerEngine {
public:
    BumblerEngine() noexcept = default;

    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void processMidiEvent(int status, int noteNumber, float velocity) noexcept;
    void renderBlock(float* const* outputChannels, int numChannels, int numSamples, const ParameterSnapshot& params) noexcept;

    void setHostBpm(float bpm) noexcept { mVoiceManager.setHostBpm(bpm); }
    [[nodiscard]] float getHostBpm() const noexcept { return mVoiceManager.getHostBpm(); }

    // Direct sub-component access
    [[nodiscard]] BumblerVoiceManager& getVoiceManager() noexcept { return mVoiceManager; }
    [[nodiscard]] const BumblerVoiceManager& getVoiceManager() const noexcept { return mVoiceManager; }
    [[nodiscard]] CharacterCircuits& getCharacterCircuits() noexcept { return mVoiceManager.getCharacterCircuits(); }
    [[nodiscard]] const CharacterCircuits& getCharacterCircuits() const noexcept { return mVoiceManager.getCharacterCircuits(); }

private:
    BumblerVoiceManager mVoiceManager;
};

} // namespace bumbler
