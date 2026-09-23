#pragma once

#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>
#include <cstring>
#include "BumblerCommon.h"
#include "ParameterSnapshot.h"

namespace bumbler {

// ============================================================================
// 1. Distortion Unit: Asymmetric Non-Linear Saturation & 1-Pole Tone Tilt
// ============================================================================
class DistortionUnit {
public:
    DistortionUnit() noexcept = default;

    void prepare(double sampleRate) noexcept {
        mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        mSampleRateScale = 48000.0f / mSampleRate;
        reset();
    }

    void reset() noexcept {
        mFilterStateL = 0.0f;
        mFilterStateR = 0.0f;
    }

    /**
     * Process a single audio sample (inlined for real-time DSP performance).
     */
    [[nodiscard]] inline float processSample(float in, float drive, float tone, bool enabled) noexcept {
        if (!enabled || drive <= 0.001f) {
            return in;
        }

        const float preGain = 1.0f + drive * 8.0f;
        const float x = in * preGain;

        // Asymmetric non-linear saturation curve producing rich 2nd and 3rd harmonics.
        // Clamp to prevent quadratic turnaround inversion on extreme negative excursions.
        const float u = (x >= -3.3333f) ? (x + 0.15f * x * x) : -1.6667f;
        const float sat = std::tanh(u);

        // 1-pole Tone filter: alpha scales from dark (0.05) to bright (0.90)
        const float toneNorm = std::clamp(tone, 0.0f, 1.0f);
        const float alpha = std::clamp((0.05f + 0.85f * toneNorm) * mSampleRateScale, 0.005f, 0.99f);

        mFilterStateL += alpha * (sat - mFilterStateL);
        mFilterStateL = flushDenormal(mFilterStateL);

        const float shaped = (1.0f - toneNorm) * mFilterStateL + toneNorm * sat;
        return std::clamp(shaped, -1.05f, 1.05f);
    }

    /**
     * Mono block processing conforming to character_circuit_tests.cpp API.
     */
    void process(const float* in, float* out, int numSamples, float drive, float tone, bool enabled) noexcept;

    /**
     * Stereo in-place block processing with dual-channel filter state isolation.
     */
    void processBlock(float* bufferL, float* bufferR, int numSamples, float drive, float tone, bool enabled) noexcept;

    /**
     * Stereo in-place processing alias.
     */
    void processStereo(float* outL, float* outR, int numSamples, float drive, float tone, bool enabled) noexcept {
        processBlock(outL, outR, numSamples, drive, tone, enabled);
    }

private:
    float mSampleRate { 48000.0f };
    float mSampleRateScale { 1.0f };
    float mFilterStateL { 0.0f };
    float mFilterStateR { 0.0f };
};

// ============================================================================
// 2. Dual Mode: Voice Doubler, Haas Decorrelation & Stereo Widening Matrix
// ============================================================================
class DualModeVoiceDoubler {
public:
    static constexpr size_t kMaxDelaySamples = 4096;
    static constexpr size_t kMask = kMaxDelaySamples - 1;

    DualModeVoiceDoubler() noexcept = default;

    void prepare(double sampleRate) noexcept {
        mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        // 5ms Haas decorrelation delay
        mDelaySamples = std::clamp(static_cast<int>(0.005f * mSampleRate + 0.5f), 1, 2048);
        reset();
    }

    void reset() noexcept {
        mDelayRing.fill(0.0f);
        mRingIdx = 0;
    }

    /**
     * Mono-in to Stereo-out block processing.
     */
    void process(const float* monoIn, float* outL, float* outR, int numSamples, bool enabled, double sampleRate = 0.0) noexcept;

    /**
     * Stereo in-place block processing.
     */
    void processBlock(float* bufferL, float* bufferR, int numSamples, bool enabled) noexcept;

    /**
     * Stereo processing alias.
     */
    void processStereo(float* outL, float* outR, int numSamples, bool enabled) noexcept {
        processBlock(outL, outR, numSamples, enabled);
    }

private:
    float mSampleRate { 48000.0f };
    int mDelaySamples { 240 };
    int mRingIdx { 0 };
    std::array<float, kMaxDelaySamples> mDelayRing {};
};

using DualModeProcessor = DualModeVoiceDoubler;

// Standalone function matching tests/character_circuit_tests.cpp
inline void processDualMode(const float* monoIn, float* outL, float* outR, int numSamples, bool enabled, double sampleRate = 48000.0) noexcept {
    static DualModeVoiceDoubler sDualProc;
    sDualProc.process(monoIn, outL, outR, numSamples, enabled, sampleRate);
}

// ============================================================================
// 3. Analog Voice Drift Generator: Random Walk Pitch Jitter & Free-Running Phase
// ============================================================================
class AnalogVoiceDrift {
public:
    AnalogVoiceDrift() noexcept {
        seed(0x12345678u);
    }

    void seed(uint32_t s) noexcept {
        mPrngState = (s != 0) ? s : 0xACE14321u;
    }

    void reset() noexcept {
        mDriftCents = 0.0f;
    }

    [[nodiscard]] double triggerNote(bool analogEnabled) noexcept {
        if (!analogEnabled) {
            mDriftCents = 0.0f;
            return 0.0;
        }
        // XorShift32 fast uniform PRNG in [-1.0, 1.0]
        mPrngState ^= mPrngState << 13;
        mPrngState ^= mPrngState >> 17;
        mPrngState ^= mPrngState << 5;
        const double randNormal = static_cast<double>(static_cast<int32_t>(mPrngState)) * (1.0 / 2147483648.0);
        mDriftCents = static_cast<float>(randNormal * 2.5); // ±2.5 cents, Var ≈ 2.08 > 0.05
        return static_cast<double>(mDriftCents);
    }

    [[nodiscard]] double getInitialPhase(bool analogEnabled) noexcept {
        if (!analogEnabled) {
            return 0.0;
        }
        mPrngState ^= mPrngState << 13;
        mPrngState ^= mPrngState >> 17;
        mPrngState ^= mPrngState << 5;
        const double rand01 = static_cast<double>(mPrngState) * (1.0 / 4294967296.0);
        return rand01 * static_cast<double>(kTwoPi);
    }

    [[nodiscard]] float getDriftCents() const noexcept { return mDriftCents; }

private:
    uint32_t mPrngState { 0xACE14321u };
    float mDriftCents { 0.0f };
};

// ============================================================================
// 4. Vintage Noise Engine: LFSR Table Loop vs True White Noise
// ============================================================================
class NoiseEngine {
public:
    NoiseEngine() noexcept {
        reset();
    }

    void reset() noexcept {
        mTableIdx = 0;
        mPrngState = 0x87654321u;
    }

    void seed(uint32_t s) noexcept {
        mPrngState = (s != 0) ? s : 0x87654321u;
        mTableIdx = static_cast<int>(s & 1023u);
    }

    void render(float* buffer, int numSamples, bool isWhiteNoise) noexcept {
        if (buffer == nullptr || numSamples <= 0) return;

        if (!isWhiteNoise) {
            for (int i = 0; i < numSamples; ++i) {
                buffer[i] = sVintageTable[static_cast<size_t>(mTableIdx & 1023)];
                mTableIdx = (mTableIdx + 1) & 1023;
            }
        } else {
            for (int i = 0; i < numSamples; ++i) {
                mPrngState ^= mPrngState << 13;
                mPrngState ^= mPrngState >> 17;
                mPrngState ^= mPrngState << 5;
                buffer[i] = static_cast<float>(static_cast<int32_t>(mPrngState)) * (1.0f / 2147483648.0f);
            }
        }
    }

private:
    static inline const std::array<float, 1024> sVintageTable = []() {
        std::array<float, 1024> table {};
        uint16_t lfsr = 0xACE1u;
        double sum = 0.0;
        for (size_t i = 0; i < 1024; ++i) {
            const unsigned bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
            lfsr = (lfsr >> 1) | static_cast<uint16_t>(bit << 15);
            const float val = (static_cast<float>(lfsr) / 32767.5f) - 1.0f;
            table[i] = val;
            sum += val;
        }
        const float dcMean = static_cast<float>(sum / 1024.0);
        for (size_t i = 0; i < 1024; ++i) {
            table[i] -= dcMean;
        }
        return table;
    }();

    int mTableIdx { 0 };
    uint32_t mPrngState { 0x87654321u };
};

// ============================================================================
// 5. Complete Character Circuits Master Output Stage
// ============================================================================
class CharacterCircuits {
public:
    CharacterCircuits() noexcept = default;

    void prepare(double sampleRate, int maxBlockSize = 512) noexcept {
        mSampleRate = sampleRate;
        mMaxBlockSize = maxBlockSize;
        mDistortion.prepare(sampleRate);
        mDualMode.prepare(sampleRate);
        mDcBlockerL.setSampleRate(static_cast<float>(sampleRate), 10.0f);
        mDcBlockerR.setSampleRate(static_cast<float>(sampleRate), 10.0f);
        reset();
    }

    void reset() noexcept {
        mDistortion.reset();
        mDualMode.reset();
        mDcBlockerL.reset();
        mDcBlockerR.reset();
    }

    void processStereo(float* outL, float* outR, int numSamples, const ParameterSnapshot& params) noexcept;
    void processBlock(float* const* channels, int numChannels, int numSamples, const ParameterSnapshot& params) noexcept;

    // Component accessors
    [[nodiscard]] DistortionUnit& getDistortion() noexcept { return mDistortion; }
    [[nodiscard]] const DistortionUnit& getDistortion() const noexcept { return mDistortion; }

    [[nodiscard]] DualModeVoiceDoubler& getDualMode() noexcept { return mDualMode; }
    [[nodiscard]] const DualModeVoiceDoubler& getDualMode() const noexcept { return mDualMode; }

    [[nodiscard]] AnalogVoiceDrift& getAnalogDrift() noexcept { return mAnalogDrift; }
    [[nodiscard]] const AnalogVoiceDrift& getAnalogDrift() const noexcept { return mAnalogDrift; }

    [[nodiscard]] NoiseEngine& getNoiseEngine() noexcept { return mNoiseEngine; }
    [[nodiscard]] const NoiseEngine& getNoiseEngine() const noexcept { return mNoiseEngine; }

private:
    double mSampleRate { 48000.0 };
    int mMaxBlockSize { 512 };

    DistortionUnit mDistortion;
    DualModeVoiceDoubler mDualMode;
    AnalogVoiceDrift mAnalogDrift;
    NoiseEngine mNoiseEngine;

    OnePoleDCBlocker mDcBlockerL;
    OnePoleDCBlocker mDcBlockerR;
};

} // namespace bumbler
