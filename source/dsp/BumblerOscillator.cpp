#include "BumblerOscillator.h"

namespace bumbler {

// ============================================================================
// Vintage Cyclic Noise Table (1024 points, pseudo-random LFSR sequence, zero DC)
// ============================================================================
static const std::array<float, 1024> sVintageNoiseTable = []() {
    std::array<float, 1024> table {};
    // Deterministic 16-bit Galois LFSR (polynomial: x^16 + x^14 + x^13 + x^11 + 1)
    uint16_t lfsr = 0xACE1u;
    double sum = 0.0;
    for (size_t i = 0; i < 1024; ++i) {
        const unsigned bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
        lfsr = (lfsr >> 1) | static_cast<uint16_t>(bit << 15);
        const float val = (static_cast<float>(lfsr) / 32767.5f) - 1.0f;
        table[i] = val;
        sum += val;
    }
    // Remove residual DC mean for perfect zero DC balance
    const float dcMean = static_cast<float>(sum / 1024.0);
    for (size_t i = 0; i < 1024; ++i) {
        table[i] -= dcMean;
    }
    return table;
}();

// ============================================================================
// BumblerOscillator Implementation
// ============================================================================

void BumblerOscillator::prepare(double sampleRate) noexcept {
    mSampleRate = (sampleRate > 100.0) ? static_cast<float>(sampleRate) : 48000.0f;
    setFrequency(mFrequency);
}

void BumblerOscillator::reset(float initialPhase) noexcept {
    mPhase = wrap01(initialPhase);
    mVintageNoiseIndex = 0;
}

void BumblerOscillator::setFrequency(float freqHz) noexcept {
    mFrequency = std::clamp(freqHz, 5.0f, mSampleRate * 0.49f);
    mPhaseInc = mFrequency / mSampleRate;
}

void BumblerOscillator::seedRandom(uint32_t seed) noexcept {
    mPrngState = (seed != 0) ? seed : 0x12345678u;
    mVintageNoiseIndex = static_cast<size_t>(seed) & 1023u;
}

float BumblerOscillator::processSample(float phaseModRad) noexcept {
    return process(mWaveform, mPulseWidth, mNoiseMode, phaseModRad);
}

float BumblerOscillator::process(OscWaveform waveform, float pulseWidth, WNoiseMode noiseMode, float phaseModRad) noexcept {
    const float dt = mPhaseInc;
    const float t = wrap01(mPhase + phaseModRad * kInvTwoPi);
    float out = 0.0f;

    switch (waveform) {
        case OscWaveform::Saw: {
            out = (2.0f * t - 1.0f) - polyBlep4(t, dt);
            break;
        }

        case OscWaveform::Square: {
            const float pw = std::clamp(pulseWidth, 0.01f, 0.99f);
            out = (t < pw) ? 1.0f : -1.0f;
            out += polyBlep4(t, dt);
            out -= polyBlep4(wrap01(t - pw), dt);
            break;
        }

        case OscWaveform::Sine: {
            out = FastSinTable::sin01(t);
            break;
        }

        case OscWaveform::Noise: {
            if (noiseMode == WNoiseMode::Vintage) {
                out = sVintageNoiseTable[mVintageNoiseIndex];
                mVintageNoiseIndex = (mVintageNoiseIndex + 1) & 1023u;
            } else {
                mPrngState ^= mPrngState << 13;
                mPrngState ^= mPrngState >> 17;
                mPrngState ^= mPrngState << 5;
                out = static_cast<float>(static_cast<int32_t>(mPrngState)) * (1.0f / 2147483648.0f);
            }
            break;
        }
    }

    mPhase = wrap01(mPhase + dt);
    return flushDenormal(out);
}

// ============================================================================
// BumblerTripleOscillatorSection Implementation
// ============================================================================

void BumblerTripleOscillatorSection::prepare(double sampleRate) noexcept {
    mSampleRate = (sampleRate > 100.0) ? static_cast<float>(sampleRate) : 48000.0f;
    mOsc1.prepare(mSampleRate);
    mOsc2.prepare(mSampleRate);
    mOsc3.prepare(mSampleRate);
    mRingModDcBlocker.setSampleRate(mSampleRate, 10.0f);
}

void BumblerTripleOscillatorSection::reset(bool freeRunningPhase) noexcept {
    if (!freeRunningPhase) {
        mOsc1.reset(0.0f);
        mOsc2.reset(0.0f);
        mOsc3.reset(0.0f);
    }
    mRingModDcBlocker.reset();
    mLastOsc1 = 0.0f;
    mLastOsc2 = 0.0f;
    mLastOsc3 = 0.0f;
    mLastRingMod = 0.0f;
}

void BumblerTripleOscillatorSection::resetWithPhase(float phase1, float phase2, float phase3) noexcept {
    mOsc1.reset(phase1);
    mOsc2.reset(phase2);
    mOsc3.reset(phase3);
    mRingModDcBlocker.reset();
    mLastOsc1 = 0.0f;
    mLastOsc2 = 0.0f;
    mLastOsc3 = 0.0f;
    mLastRingMod = 0.0f;
}

void BumblerTripleOscillatorSection::seedVoiceRandom(uint32_t voiceIndex) noexcept {
    const uint32_t seed1 = 0x9E3779B9u * (voiceIndex + 1u) ^ 0x12345678u;
    const uint32_t seed2 = 0x85EBCA6Bu * (voiceIndex + 1u) ^ 0x9ABCDEF0u;
    const uint32_t seed3 = 0xC2B2AE35u * (voiceIndex + 1u) ^ 0xFEDCBA98u;
    mOsc1.seedRandom(seed1);
    mOsc2.seedRandom(seed2);
    mOsc3.seedRandom(seed3);
}

float BumblerTripleOscillatorSection::process(float baseFreqHz,
                                             const ParameterSnapshot& params,
                                             float extModPw,
                                             float extModPitch1,
                                             float extModPitch2,
                                             float extModOscMix,
                                             float extModOsc1Gain) noexcept {
    // 1. Effective Pulse Width clamped to safe duty cycle [0.01, 0.99]
    const float effPw = std::clamp(params.pulseWidth + extModPw, 0.01f, 0.99f);
    const auto noiseMode = (params.wNoiseMode >= 0.5f) ? WNoiseMode::White : WNoiseMode::Vintage;

    // 2. Compute pitch and process OSC 1
    const float semi1 = params.osc1Octave * 12.0f + params.osc1Fine + extModPitch1;
    const float f1 = std::clamp(baseFreqHz * semitonesToRatio(semi1), 5.0f, mSampleRate * 0.48f);
    mOsc1.setFrequency(f1);

    const auto wave1 = static_cast<OscWaveform>(std::clamp(static_cast<int>(params.osc1Waveform), 0, 3));
    const float rawOsc1Out = mOsc1.process(wave1, effPw, noiseMode, 0.0f);
    const float osc1Out = rawOsc1Out * std::clamp(extModOsc1Gain, 0.0f, 2.0f);
    mLastOsc1 = osc1Out;

    // 3. Compute pitch and process OSC 2 with Audio-Rate FM
    const float semi2 = params.osc2Octave * 12.0f + params.osc2Fine + extModPitch2;
    const float f2Base = std::clamp(baseFreqHz * semitonesToRatio(semi2), 5.0f, mSampleRate * 0.48f);
    mOsc2.setFrequency(f2Base);

    const float phaseMod2 = (params.fmAmount > 0.0001f)
                                ? (osc1Out * params.fmAmount * 4.0f * kPi)
                                : 0.0f;

    const auto wave2 = static_cast<OscWaveform>(std::clamp(static_cast<int>(params.osc2Waveform), 0, 3));
    const float osc2Out = mOsc2.process(wave2, effPw, noiseMode, phaseMod2);
    mLastOsc2 = osc2Out;

    // 4. Continuous Balance Crossfade between OSC 1 and OSC 2 (Equal-Power)
    const float mix = std::clamp(params.oscMix + extModOscMix, 0.0f, 1.0f);
    const float angle = mix * kHalfPi;
    const float g1 = std::cos(angle);
    const float g2 = std::sin(angle);
    const float oscMixOut = g1 * osc1Out + g2 * osc2Out;

    // 5. Ring Modulator with DC Blocker (Sum & Difference metallic sidebands)
    const float rawRingMod = osc1Out * osc2Out;
    const float cleanRingMod = mRingModDcBlocker.process(rawRingMod);
    mLastRingMod = cleanRingMod;

    // Blend Ring Mod according to ringModMix
    const float ringMix = std::clamp(params.ringModMix, 0.0f, 1.0f);
    const float osc12Out = (1.0f - ringMix) * oscMixOut + ringMix * cleanRingMod;

    // 6. Auxiliary OSC 3 (Square or Sawtooth, tracked to base pitch)
    float osc3Out = 0.0f;
    if (params.osc3Level > 0.001f) {
        mOsc3.setFrequency(baseFreqHz);
        const auto wave3 = (params.osc3Waveform >= 0.5f) ? OscWaveform::Saw : OscWaveform::Square;
        osc3Out = mOsc3.process(wave3, 0.5f, noiseMode, 0.0f) * params.osc3Level;
    }
    mLastOsc3 = osc3Out;

    // 7. Total combined oscillator signal
    return flushDenormal(osc12Out + osc3Out);
}

} // namespace bumbler
