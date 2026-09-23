#include "CharacterCircuits.h"

namespace bumbler {

// ============================================================================
// DistortionUnit Implementation
// ============================================================================
void DistortionUnit::process(const float* in, float* out, int numSamples, float drive, float tone, bool enabled) noexcept {
    if (in == nullptr || out == nullptr || numSamples <= 0) return;

    if (!enabled || drive <= 0.001f) {
        if (in != out) {
            std::memcpy(out, in, static_cast<size_t>(numSamples) * sizeof(float));
        }
        return;
    }

    const float preGain = 1.0f + drive * 8.0f;
    const float toneNorm = std::clamp(tone, 0.0f, 1.0f);
    const float alpha = std::clamp((0.05f + 0.85f * toneNorm) * mSampleRateScale, 0.005f, 0.99f);

    for (int i = 0; i < numSamples; ++i) {
        const float x = in[i] * preGain;
        const float u = (x >= -3.3333f) ? (x + 0.15f * x * x) : -1.6667f;
        const float sat = std::tanh(u);

        mFilterStateL += alpha * (sat - mFilterStateL);
        mFilterStateL = flushDenormal(mFilterStateL);

        const float shaped = (1.0f - toneNorm) * mFilterStateL + toneNorm * sat;
        out[i] = std::clamp(shaped, -1.05f, 1.05f);
    }
}

void DistortionUnit::processBlock(float* bufferL, float* bufferR, int numSamples, float drive, float tone, bool enabled) noexcept {
    if (!enabled || drive <= 0.001f || numSamples <= 0 || bufferL == nullptr || bufferR == nullptr) return;

    const float preGain = 1.0f + drive * 8.0f;
    const float toneNorm = std::clamp(tone, 0.0f, 1.0f);
    const float alpha = std::clamp((0.05f + 0.85f * toneNorm) * mSampleRateScale, 0.005f, 0.99f);

    for (int i = 0; i < numSamples; ++i) {
        // Left Channel
        const float xL = bufferL[i] * preGain;
        const float uL = (xL >= -3.3333f) ? (xL + 0.15f * xL * xL) : -1.6667f;
        const float satL = std::tanh(uL);
        mFilterStateL += alpha * (satL - mFilterStateL);
        mFilterStateL = flushDenormal(mFilterStateL);
        const float shapedL = (1.0f - toneNorm) * mFilterStateL + toneNorm * satL;
        bufferL[i] = std::clamp(shapedL, -1.05f, 1.05f);

        // Right Channel
        const float xR = bufferR[i] * preGain;
        const float uR = (xR >= -3.3333f) ? (xR + 0.15f * xR * xR) : -1.6667f;
        const float satR = std::tanh(uR);
        mFilterStateR += alpha * (satR - mFilterStateR);
        mFilterStateR = flushDenormal(mFilterStateR);
        const float shapedR = (1.0f - toneNorm) * mFilterStateR + toneNorm * satR;
        bufferR[i] = std::clamp(shapedR, -1.05f, 1.05f);
    }
}

// ============================================================================
// DualModeVoiceDoubler Implementation
// ============================================================================
void DualModeVoiceDoubler::process(const float* monoIn, float* outL, float* outR, int numSamples, bool enabled, double sampleRate) noexcept {
    if (monoIn == nullptr || outL == nullptr || outR == nullptr || numSamples <= 0) return;

    if (sampleRate > 100.0 && std::abs(sampleRate - static_cast<double>(mSampleRate)) > 1.0) {
        prepare(sampleRate);
    }

    if (!enabled) {
        for (int i = 0; i < numSamples; ++i) {
            outL[i] = monoIn[i];
            outR[i] = monoIn[i];
        }
        return;
    }

    constexpr float kInvSqrt2 = 0.7071067811865475f;

    for (int i = 0; i < numSamples; ++i) {
        mDelayRing[static_cast<size_t>(mRingIdx)] = monoIn[i];
        const int readIdx = (mRingIdx - mDelaySamples + static_cast<int>(kMaxDelaySamples)) & static_cast<int>(kMask);
        const float delayed = mDelayRing[static_cast<size_t>(readIdx)];
        mRingIdx = (mRingIdx + 1) & static_cast<int>(kMask);

        const float diff = monoIn[i] - delayed;
        const float rawL = (monoIn[i] + 0.6f * diff) * kInvSqrt2;
        const float rawR = (delayed - 0.6f * diff) * kInvSqrt2;

        outL[i] = std::clamp(rawL, -1.05f, 1.05f);
        outR[i] = std::clamp(rawR, -1.05f, 1.05f);
    }
}

void DualModeVoiceDoubler::processBlock(float* bufferL, float* bufferR, int numSamples, bool enabled) noexcept {
    if (!enabled || bufferL == nullptr || bufferR == nullptr || numSamples <= 0) return;

    constexpr float kInvSqrt2 = 0.7071067811865475f;

    for (int i = 0; i < numSamples; ++i) {
        const float mono = 0.5f * (bufferL[i] + bufferR[i]);

        mDelayRing[static_cast<size_t>(mRingIdx)] = mono;
        const int readIdx = (mRingIdx - mDelaySamples + static_cast<int>(kMaxDelaySamples)) & static_cast<int>(kMask);
        const float delayed = mDelayRing[static_cast<size_t>(readIdx)];
        mRingIdx = (mRingIdx + 1) & static_cast<int>(kMask);

        const float diff = mono - delayed;
        const float rawL = (bufferL[i] + 0.6f * diff) * kInvSqrt2;
        const float rawR = (delayed - 0.6f * diff) * kInvSqrt2;

        bufferL[i] = std::clamp(rawL, -1.05f, 1.05f);
        bufferR[i] = std::clamp(rawR, -1.05f, 1.05f);
    }
}

// ============================================================================
// CharacterCircuits Implementation (Unified Output Chain)
// ============================================================================
void CharacterCircuits::processStereo(float* outL, float* outR, int numSamples, const ParameterSnapshot& params) noexcept {
    if (outL == nullptr || outR == nullptr || numSamples <= 0) return;

    const bool driveOn = (params.driveEnabled >= 0.5f);
    const bool dualOn = (params.dualMode >= 0.5f);
    const float masterVol = std::clamp(params.masterVolume, 0.0f, 2.0f);

    // 1. Distortion & Tone Filter (stereo independent)
    if (driveOn) {
        mDistortion.processBlock(outL, outR, numSamples, params.driveAmount, params.driveTone, true);
    }

    // 2. Dual Mode (voice doubling & Haas decorrelation stereo widening)
    if (dualOn) {
        mDualMode.processBlock(outL, outR, numSamples, true);
    }

    // 3. Master Volume, Saturation Ceiling, DC Blocker & Denormal Flushing
    for (int i = 0; i < numSamples; ++i) {
        float sL = std::clamp(outL[i] * masterVol, -1.02f, 1.02f);
        float sR = std::clamp(outR[i] * masterVol, -1.02f, 1.02f);

        float yL = mDcBlockerL.process(sL);
        float yR = mDcBlockerR.process(sR);

        yL = flushDenormal(yL);
        yR = flushDenormal(yR);

        outL[i] = std::clamp(yL, -1.05f, 1.05f);
        outR[i] = std::clamp(yR, -1.05f, 1.05f);
    }
}

void CharacterCircuits::processBlock(float* const* channels, int numChannels, int numSamples, const ParameterSnapshot& params) noexcept {
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0) return;

    if (numChannels == 1) {
        float* outM = channels[0];
        const bool driveOn = (params.driveEnabled >= 0.5f);
        const float masterVol = std::clamp(params.masterVolume, 0.0f, 2.0f);

        // 1. Distortion & Tone Filter
        if (driveOn) {
            mDistortion.process(outM, outM, numSamples, params.driveAmount, params.driveTone, true);
        }

        // 2. Master Volume, Saturation Ceiling, and DC Blocker
        for (int i = 0; i < numSamples; ++i) {
            float s = std::clamp(outM[i] * masterVol, -1.02f, 1.02f);
            float y = mDcBlockerL.process(s);
            y = flushDenormal(y);
            outM[i] = std::clamp(y, -1.05f, 1.05f);
        }
        return;
    }

    processStereo(channels[0], channels[1], numSamples, params);
}

} // namespace bumbler
