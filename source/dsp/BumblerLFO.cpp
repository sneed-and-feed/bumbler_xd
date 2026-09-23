#include "BumblerLFO.h"

namespace bumbler {

void BumblerLFO::prepare(double sampleRate) noexcept {
    mSampleRate = (sampleRate > 100.0) ? sampleRate : 48000.0;
    reset();
}

void BumblerLFO::reset() noexcept {
    mPhase = 0.0;
    mDelaySamplesTotal = static_cast<int>(mDelaySeconds * static_cast<float>(mSampleRate));
    mDelaySamplesRemaining = mDelaySamplesTotal;
    mNoiseHeldSample = nextRandomSample();
    mLastOutput = 0.0f;
    updatePhaseIncrement();
}

void BumblerLFO::setRate(float rateHz) noexcept {
    mRateHz = std::clamp(rateHz, 0.01f, 30.0f);
    updatePhaseIncrement();
}

void BumblerLFO::setDelay(float delaySeconds) noexcept {
    mDelaySeconds = std::clamp(delaySeconds, 0.0f, 5.0f);
    mDelaySamplesTotal = static_cast<int>(mDelaySeconds * static_cast<float>(mSampleRate));
}

void BumblerLFO::setWaveform(LfoWaveform wave) noexcept {
    mWaveform = wave;
}

void BumblerLFO::setWaveform(int waveIndex) noexcept {
    mWaveform = static_cast<LfoWaveform>(std::clamp(waveIndex, 0, 3));
}

void BumblerLFO::setTempoSync(bool syncEnabled, float syncDivisionIndex, float hostBpm) noexcept {
    mTempoSync = syncEnabled;
    mSyncDivision = syncDivisionIndex;
    mHostBpm = (hostBpm > 20.0f && hostBpm < 400.0f) ? hostBpm : 120.0f;
    updatePhaseIncrement();
}

void BumblerLFO::setTempo(float hostBpm) noexcept {
    mHostBpm = (hostBpm > 20.0f && hostBpm < 400.0f) ? hostBpm : 120.0f;
    if (mTempoSync) {
        updatePhaseIncrement();
    }
}

void BumblerLFO::setKeyReset(bool resetEnabled) noexcept {
    mKeyReset = resetEnabled;
}

void BumblerLFO::setAmount(float amount) noexcept {
    mAmount = std::clamp(amount, 0.0f, 1.0f);
}

void BumblerLFO::setParams(double rateHz, double delaySeconds, RefWave wave, double sampleRate) noexcept {
    mSampleRate = (sampleRate > 100.0) ? sampleRate : 48000.0;
    mRateHz = std::clamp(static_cast<float>(rateHz), 0.01f, 30.0f);
    mDelaySeconds = std::clamp(static_cast<float>(delaySeconds), 0.0f, 5.0f);
    // Map RefWave: SINE=0 -> Sine, SAW=1 -> Saw, SQUARE=2 -> Square, NOISE=3 -> Noise
    switch (wave) {
        case SINE:   mWaveform = LfoWaveform::Sine; break;
        case SAW:    mWaveform = LfoWaveform::Saw; break;
        case SQUARE: mWaveform = LfoWaveform::Square; break;
        case NOISE:  mWaveform = LfoWaveform::Noise; break;
        default:     mWaveform = LfoWaveform::Sine; break;
    }
    mTempoSync = false;
    mAmount = 1.0f;
    reset();
}

void BumblerLFO::setParams(double rateHz, double delaySeconds, LfoWaveform wave, double sampleRate) noexcept {
    mSampleRate = (sampleRate > 100.0) ? sampleRate : 48000.0;
    mRateHz = std::clamp(static_cast<float>(rateHz), 0.01f, 30.0f);
    mDelaySeconds = std::clamp(static_cast<float>(delaySeconds), 0.0f, 5.0f);
    mWaveform = wave;
    mTempoSync = false;
    mAmount = 1.0f;
    reset();
}

void BumblerLFO::noteTrigger() noexcept {
    if (mKeyReset) {
        mPhase = 0.0;
        mDelaySamplesRemaining = mDelaySamplesTotal;
        mNoiseHeldSample = nextRandomSample();
    }
}

void BumblerLFO::updatePhaseIncrement() noexcept {
    if (mTempoSync) {
        double beatsPerCycle = 1.0; // 1/4 note default
        const int div = static_cast<int>(std::round(mSyncDivision));
        switch (div) {
            case 0: beatsPerCycle = 0.125; break; // 1/32 note
            case 1: beatsPerCycle = 0.25;  break; // 1/16 note
            case 2: beatsPerCycle = 0.5;   break; // 1/8 note
            case 3: beatsPerCycle = 1.0;   break; // 1/4 note
            case 4: beatsPerCycle = 2.0;   break; // 1/2 note
            case 5: beatsPerCycle = 4.0;   break; // 1/1 Whole note
            default: beatsPerCycle = 1.0;  break;
        }
        mEffectiveRate = static_cast<double>(mHostBpm) / (60.0 * beatsPerCycle);
    } else {
        mEffectiveRate = static_cast<double>(mRateHz);
    }

    mPhaseInc = (mSampleRate > 0.0) ? (mEffectiveRate / mSampleRate) : 0.0;
}

float BumblerLFO::nextRandomSample() noexcept {
    // 32-bit XorShift PRNG -> uniform float in [-1.0, 1.0]
    uint32_t x = mPrngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    mPrngState = x;
    return static_cast<float>(x) * (2.0f / 4294967295.0f) - 1.0f;
}

float BumblerLFO::processSample() noexcept {
    // 1. Pre-modulation delay window
    if (mDelaySamplesRemaining > 0) {
        --mDelaySamplesRemaining;
        mLastOutput = 0.0f;
        return 0.0f;
    }

    // 2. Synthesize waveform from normalized phase [0.0, 1.0)
    // Cast 64-bit double phase to 32-bit float for wavetable and analytical evaluation
    const float phaseF = static_cast<float>(mPhase);
    float out = 0.0f;
    switch (mWaveform) {
        case LfoWaveform::Saw:
            // Downward ramp: +1.0 at phase 0, -1.0 at phase 1
            out = 1.0f - 2.0f * phaseF;
            break;

        case LfoWaveform::Square:
            out = (phaseF < 0.5f) ? 1.0f : -1.0f;
            break;

        case LfoWaveform::Sine:
            out = FastSinTable::sin01(phaseF);
            break;

        case LfoWaveform::Noise:
            out = mNoiseHeldSample;
            break;
    }

    // 3. Advance phase accumulator in 64-bit double precision (prevents mantissa truncation)
    mPhase += mPhaseInc;
    if (mPhase >= 1.0) {
        mPhase -= 1.0;
        if (mWaveform == LfoWaveform::Noise) {
            mNoiseHeldSample = nextRandomSample();
        }
    }

    mLastOutput = out;
    return out;
}

} // namespace bumbler
