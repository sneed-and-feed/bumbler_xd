#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>
#include "BumblerCommon.h"
#include "ParameterSnapshot.h"

namespace bumbler {

/**
 * BumblerLFO: Multi-waveform Low Frequency Oscillator for Bumbler XD.
 * Conforms to ORIGINAL_REQUEST.md § R3 and PROJECT.md § M3.
 * Remediated for BUG-M3-01: Phase accumulator (mPhase), increment (mPhaseInc),
 * and effective rate (mEffectiveRate) upgraded to IEEE-754 64-bit double precision
 * to eliminate low-frequency mantissa truncation drift (<0.0001% timing error).
 * Features:
 * - 4 Waveforms: Sawtooth, Square, Sine, Noise (sample-and-hold pseudo-random).
 * - Rate: 0.01 Hz to 30.0 Hz (free mode).
 * - Tempo Sync: Whole (1/1), 1/2, 1/4, 1/8, 1/16, 1/32 (synced to host BPM).
 * - Delay: Pre-modulation onset delay/ramp (0.0 to 5.0 seconds).
 * - Key-trigger Phase Reset: restarts phase at 0.0 on note-on.
 * - Lock-free, zero dynamic allocations, denormal-safe.
 */
class BumblerLFO {
public:
    using Waveform = LfoWaveform;

    // Compatible enum for test harnesses matching mod_ref::RefLFO
    enum RefWave {
        SINE = 0,
        SAW = 1,
        SQUARE = 2,
        NOISE = 3
    };

    enum class SyncDivision : int {
        Div_1_32 = 0,
        Div_1_16 = 1,
        Div_1_8  = 2,
        Div_1_4  = 3,
        Div_1_2  = 4,
        Div_1_1  = 5
    };

    BumblerLFO() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Sample rate configuration alias
    void setSampleRate(double sampleRate) noexcept { prepare(sampleRate); }

    // Parameter configuration
    void setRate(float rateHz) noexcept;
    void setDelay(float delaySeconds) noexcept;
    void setWaveform(LfoWaveform wave) noexcept;
    void setWaveform(int waveIndex) noexcept;
    void setTempoSync(bool syncEnabled, float syncDivisionIndex, float hostBpm = 120.0f) noexcept;
    void setKeyReset(bool resetEnabled) noexcept;
    void setAmount(float amount) noexcept;
    void setTempo(float hostBpm) noexcept;

    // Test helper compatibility API matching mod_ref::RefLFO
    void setParams(double rateHz, double delaySeconds, RefWave wave, double sampleRate) noexcept;
    void setParams(double rateHz, double delaySeconds, LfoWaveform wave, double sampleRate) noexcept;

    // Note trigger event (called on note-on)
    void noteTrigger() noexcept;

    // Direct sample evaluation: produces raw waveform in [-1.0, 1.0] (or 0 during delay)
    [[nodiscard]] float processSample() noexcept;

    // State queries (float overloads maintain strict backward compatibility)
    [[nodiscard]] float getPhase() const noexcept { return static_cast<float>(mPhase); }
    [[nodiscard]] double getPhaseDouble() const noexcept { return mPhase; }
    [[nodiscard]] float getLastOutput() const noexcept { return mLastOutput; }
    [[nodiscard]] float getEffectiveRateHz() const noexcept { return static_cast<float>(mEffectiveRate); }
    [[nodiscard]] double getEffectiveRateHzDouble() const noexcept { return mEffectiveRate; }
    [[nodiscard]] double getPhaseIncrement() const noexcept { return mPhaseInc; }
    [[nodiscard]] float getAmount() const noexcept { return mAmount; }
    [[nodiscard]] bool isInDelay() const noexcept { return mDelaySamplesRemaining > 0; }
    [[nodiscard]] bool isKeyResetEnabled() const noexcept { return mKeyReset; }
    [[nodiscard]] LfoWaveform getWaveform() const noexcept { return mWaveform; }

    void seedRandom(uint32_t seed) noexcept {
        mPrngState = (seed != 0u) ? seed : 0x543210FDu;
    }

private:
    [[nodiscard]] float nextRandomSample() noexcept;
    void updatePhaseIncrement() noexcept;

    double mSampleRate { 48000.0 };
    float mRateHz { 2.0f };
    float mDelaySeconds { 0.0f };
    LfoWaveform mWaveform { LfoWaveform::Sine };
    bool mTempoSync { false };
    float mSyncDivision { 3.0f };
    float mHostBpm { 120.0f };
    bool mKeyReset { true };
    float mAmount { 1.0f };

    // 64-bit IEEE-754 double precision variables (BUG-M3-01 Remediation)
    double mEffectiveRate { 2.0 };
    double mPhase { 0.0 };
    double mPhaseInc { 0.0 };
    int mDelaySamplesTotal { 0 };
    int mDelaySamplesRemaining { 0 };

    float mNoiseHeldSample { 0.0f };
    uint32_t mPrngState { 0x543210FDu };
    float mLastOutput { 0.0f };
};

} // namespace bumbler
