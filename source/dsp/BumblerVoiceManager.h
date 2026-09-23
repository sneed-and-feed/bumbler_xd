#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerVoice.h"

namespace bumbler {

/**
 * BumblerVoiceManager: Manages polyphonic voice allocation, note lifecycle,
 * and audio accumulation for up to kMaxVoices (16) synthesizer voices.
 *
 * ============================================================================
 * Threading Model & Audio Thread Safety Guarantees:
 * ============================================================================
 * - BumblerVoiceManager is designed exclusively for synchronous, single-thread
 *   execution on the real-time audio thread.
 * - All methods (prepare, reset, noteOn, noteOff, setSustainPedal, setPitchBend,
 *   allNotesOff, setPolyphonyLimit, renderBlock) are strictly non-blocking,
 *   wait-free, and guaranteed to perform zero dynamic memory allocations.
 * - No mutexes, spinning primitives, condition variables, or operating system
 *   synchronization objects are acquired.
 * - Multi-threaded hosts must ensure sequential calling of MIDI dispatches and
 *   block rendering, or dispatch MIDI through lock-free SPSC queues.
 *
 * ============================================================================
 * Voice Allocation Invariants:
 * ============================================================================
 * Voice allocation employs a strict, deterministic 4-tier strategy:
 *
 * 1. Tier 1 (Same-Pitch Retriggering):
 *    If an active voice is already sounding the requested MIDI pitch, that voice
 *    is retriggered in-place. This preserves phase continuity, prevents duplicate
 *    voice buildup on rapid repeated notes, and conserves voice polyphony headroom.
 *
 * 2. Tier 2 (Free / Inactive Voice Acquisition):
 *    If the requested pitch is not currently active, the manager performs a round-robin
 *    scan starting from (mLastAllocatedIndex + 1) across the active polyphony pool
 *    [0, mMaxPolyphony - 1]. The first inactive voice encountered is allocated.
 *
 * 3. Tier 3 (Oldest Releasing Voice Stealing):
 *    If all voices in the active polyphony pool are active, the manager searches
 *    for voices currently in their Release envelope stage (isReleasing() == true).
 *    Among these, the voice with the lowest trigger sample timestamp is stolen.
 *
 * 4. Tier 4 (Oldest Held Voice Stealing / LRU Fallback):
 *    If all active voices are actively held (sustaining), the voice with the oldest
 *    trigger sample timestamp across the active polyphony pool is stolen.
 *
 * Invariant Constraints:
 * - Active voices are bounded strictly by mMaxPolyphony, clamped to [1, kMaxVoices].
 * - When polyphony limit is lowered, any active voices outside the new limit are
 *   instantly silenced via forceKill().
 * - Scratch buffer usage for mono downmixing is guarded and chunked to prevent
 *   buffer overruns regardless of host render block sizes.
 */
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
