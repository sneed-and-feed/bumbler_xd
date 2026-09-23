#pragma once

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerVoiceManager.h"

namespace bumbler {

/**
 * BumblerEngine: Mandated top-level core engine entry point for bumbler_dsp_core.
 * Conforms to PROJECT.md § Interface Contracts.
 *
 * ============================================================================
 * Threading Model & Audio Thread Concurrency Guarantees:
 * ============================================================================
 * BumblerEngine is strictly designed for single-thread real-time audio execution:
 * - renderBlock() and processMidiEvent() are expected to be called sequentially
 *   from the same real-time audio thread (such as inside JUCE's processBlock callback).
 * - All audio processing, voice lifecycle state mutations, and modulation evaluations
 *   are non-blocking, wait-free, and 100% allocation-free (zero malloc/new/free/delete).
 * - No internal locks, mutexes, condition variables, or blocking synchronization
 *   primitives are utilized.
 *
 * Host Multi-Threading & Asynchronous MIDI Note:
 * External hosts or plugin wrappers that employ multi-threaded MIDI dispatch
 * (e.g., asynchronous MIDI input on a dedicated background or hardware thread)
 * MUST NOT call processMidiEvent() concurrently with renderBlock(). Such hosts
 * MUST synchronize calls externally, or buffer events into a lock-free Single-Producer
 * Single-Consumer (SPSC) ring buffer / FIFO on the MIDI thread and drain it sequentially
 * on the real-time audio thread immediately prior to calling renderBlock().
 */
class BumblerEngine {
public:
    BumblerEngine() noexcept = default;

    /**
     * Prepares the engine and all subordinate voice and circuit components.
     * Defensively validates sampleRate and clamps maxBlockSize to safe ranges.
     */
    void prepare(double sampleRate, int maxBlockSize) noexcept;

    /**
     * Resets all voices, filter states, character circuits, and controllers.
     */
    void reset() noexcept;

    /**
     * Processes a single MIDI event. Expected to be called sequentially on the
     * audio thread ahead of or interleaved with sample rendering blocks.
     */
    void processMidiEvent(int status, int noteNumber, float velocity) noexcept;

    /**
     * Renders synthesized audio into the supplied output channel buffers.
     * Guaranteed real-time safe (bounded execution, zero allocations, no denormals).
     */
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
