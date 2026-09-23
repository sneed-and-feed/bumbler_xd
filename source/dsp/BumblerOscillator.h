#pragma once

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"

namespace bumbler {

/**
 * BumblerOscillator: Single anti-aliased oscillator core.
 * Generates Sawtooth, Square/Pulse with variable PW, Sine, and Noise (Vintage LFSR or White).
 * Features PolyBLEP anti-aliasing for Saw and Pulse.
 */
class BumblerOscillator {
public:
    BumblerOscillator() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset(float initialPhase = 0.0f) noexcept;

    // Setters
    void setFrequency(float freqHz) noexcept;
    void setWaveform(OscWaveform waveform) noexcept { mWaveform = waveform; }
    void setPulseWidth(float pulseWidth) noexcept { mPulseWidth = std::clamp(pulseWidth, 0.01f, 0.99f); }
    void setWhiteNoise(bool isWhite) noexcept { mNoiseMode = isWhite ? WNoiseMode::White : WNoiseMode::Vintage; }

    // Direct accessors
    [[nodiscard]] float getFrequency() const noexcept { return mFrequency; }
    [[nodiscard]] float getPhase() const noexcept { return mPhase; }
    void setPhase(float phase) noexcept { mPhase = wrap01(phase); }
    [[nodiscard]] float getPhaseIncrement() const noexcept { return mPhaseInc; }
    void setPhaseIncrement(float inc) noexcept { mPhaseInc = inc; }

    // Seed PRNG state for voice-independent white noise
    void seedRandom(uint32_t seed) noexcept;

    // Process a single sample with optional phase modulation (in radians)
    [[nodiscard]] float processSample(float phaseModRad = 0.0f) noexcept;

    // Flexible process method matching explorer blueprint
    [[nodiscard]] float process(OscWaveform waveform, float pulseWidth, WNoiseMode noiseMode, float phaseModRad = 0.0f) noexcept;

private:
    float mSampleRate { 48000.0f };
    float mFrequency { 440.0f };
    float mPhase { 0.0f };
    float mPhaseInc { 0.0f };
    OscWaveform mWaveform { OscWaveform::Saw };
    float mPulseWidth { 0.5f };
    WNoiseMode mNoiseMode { WNoiseMode::Vintage };

    // Vintage noise cyclic index
    size_t mVintageNoiseIndex { 0 };

    // Voice-local XorShift32 PRNG state
    uint32_t mPrngState { 0x12345678u };
};

/**
 * BumblerTripleOscillatorSection: High-level generator managing OSC 1, OSC 2, OSC 3 (aux),
 * continuous balance mixing, Ring Modulator with DC blocker, and audio-rate FM.
 */
class BumblerTripleOscillatorSection {
public:
    BumblerTripleOscillatorSection() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset(bool freeRunningPhase = false) noexcept;
    void resetWithPhase(float phase1, float phase2, float phase3) noexcept;

    /**
     * Renders next sample of the combined 3-oscillator section.
     * @param baseFreqHz Pitch-tracked base frequency of the voice
     * @param params Snapshot containing all oscillator parameters
     * @param extModPw External modulation applied to Pulse Width (from LFO/ModEnv)
     * @param extModPitch1 External pitch offset in semitones for OSC 1
     * @param extModPitch2 External pitch offset in semitones for OSC 2
     */
    [[nodiscard]] float process(float baseFreqHz,
                                const ParameterSnapshot& params,
                                float extModPw = 0.0f,
                                float extModPitch1 = 0.0f,
                                float extModPitch2 = 0.0f,
                                float extModOscMix = 0.0f,
                                float extModOsc1Gain = 1.0f) noexcept;

    // Diagnostics / state inspection
    [[nodiscard]] float getLastOsc1() const noexcept { return mLastOsc1; }
    [[nodiscard]] float getLastOsc2() const noexcept { return mLastOsc2; }
    [[nodiscard]] float getLastOsc3() const noexcept { return mLastOsc3; }
    [[nodiscard]] float getLastRingMod() const noexcept { return mLastRingMod; }

    void seedVoiceRandom(uint32_t voiceIndex) noexcept;

private:
    float mSampleRate { 48000.0f };

    BumblerOscillator mOsc1;
    BumblerOscillator mOsc2;
    BumblerOscillator mOsc3; // Aux oscillator (Square / Saw)

    OnePoleDCBlocker mRingModDcBlocker;

    float mLastOsc1 { 0.0f };
    float mLastOsc2 { 0.0f };
    float mLastOsc3 { 0.0f };
    float mLastRingMod { 0.0f };
};

} // namespace bumbler
