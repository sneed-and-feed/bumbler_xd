#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "WaspFilter.h"
#include "BumblerLFO.h"
#include "BumblerEnvelopes.h"
#include "CharacterCircuits.h"

namespace bumbler {

using EnvelopeStage = BumblerADSR::State;
using VoiceAdsr = BumblerADSR;

class BumblerVoice {
public:
    BumblerVoice() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Trigger & Note State
    void noteOn(int midiNote, float velocity, uint64_t triggerSample) noexcept;
    void noteOn(int midiNote, float velocity, uint64_t triggerSample, bool analogMode) noexcept;
    void noteOff(float velocity = 0.0f) noexcept;
    void forceKill() noexcept;

    // State Queries
    [[nodiscard]] bool isActive() const noexcept { return mActive; }
    [[nodiscard]] bool isHeld() const noexcept { return mIsHeld; }
    [[nodiscard]] bool isReleasing() const noexcept {
        return mActive && (mDualEnvelopes.getAmpEnv().getState() == BumblerADSR::State::Release);
    }
    [[nodiscard]] bool isPedalLatched() const noexcept { return mIsPedalLatched; }
    void setPedalLatched(bool latched) noexcept { mIsPedalLatched = latched; }

    [[nodiscard]] int getMidiNote() const noexcept { return mMidiNote; }
    [[nodiscard]] float getVelocity() const noexcept { return mVelocity; }
    [[nodiscard]] uint64_t getTriggerSample() const noexcept { return mTriggerSample; }
    [[nodiscard]] float getEnvelopeLevel() const noexcept { return mDualEnvelopes.getAmpEnv().getValue(); }
    [[nodiscard]] float getFilterEnvelopeLevel() const noexcept { return mDualEnvelopes.getFilterEnv().getValue(); }

    void setPitchBend(float semitones) noexcept { mPitchBendSemitones = semitones; }
    void setHostBpm(float bpm) noexcept {
        mHostBpm = (bpm > 20.0f && bpm < 400.0f) ? bpm : 120.0f;
        mLfo1.setTempo(mHostBpm);
        mLfo2.setTempo(mHostBpm);
    }

    void seedVoice(uint32_t voiceIndex) noexcept {
        mOscSection.seedVoiceRandom(voiceIndex);
        mLfo1.seedRandom(0x13579BDFu * (voiceIndex + 1u) ^ 0xA5A5A5A5u);
        mLfo2.seedRandom(0x2468ACE0u * (voiceIndex + 1u) ^ 0x5A5A5A5Au);
    }

    // Filter Enablement & Access
    void setFilterEnabled(bool enabled) noexcept { mFilterEnabled = enabled; }
    [[nodiscard]] bool isFilterEnabled() const noexcept { return mFilterEnabled; }
    [[nodiscard]] WaspFilter& getFilter() noexcept { return mFilter; }
    [[nodiscard]] const WaspFilter& getFilter() const noexcept { return mFilter; }

    // Modulation Component Access
    [[nodiscard]] BumblerLFO& getLfo1() noexcept { return mLfo1; }
    [[nodiscard]] const BumblerLFO& getLfo1() const noexcept { return mLfo1; }
    [[nodiscard]] BumblerLFO& getLfo2() noexcept { return mLfo2; }
    [[nodiscard]] const BumblerLFO& getLfo2() const noexcept { return mLfo2; }
    [[nodiscard]] BumblerModEnvelope& getModEnv() noexcept { return mModEnv; }
    [[nodiscard]] const BumblerModEnvelope& getModEnv() const noexcept { return mModEnv; }
    [[nodiscard]] BumblerADSR& getAmpEnv() noexcept { return mDualEnvelopes.getAmpEnv(); }
    [[nodiscard]] const BumblerADSR& getAmpEnv() const noexcept { return mDualEnvelopes.getAmpEnv(); }
    [[nodiscard]] BumblerADSR& getFilterEnv() noexcept { return mDualEnvelopes.getFilterEnv(); }
    [[nodiscard]] const BumblerADSR& getFilterEnv() const noexcept { return mDualEnvelopes.getFilterEnv(); }
    [[nodiscard]] BumblerDualADSR& getDualEnvelopes() noexcept { return mDualEnvelopes; }
    [[nodiscard]] const BumblerDualADSR& getDualEnvelopes() const noexcept { return mDualEnvelopes; }

    // Character & Analog Accessors
    [[nodiscard]] float getPitchDriftCents() const noexcept { return mPitchDriftCents; }
    [[nodiscard]] float getOscInitialPhaseRad() const noexcept { return mOscInitialPhaseRad; }
    [[nodiscard]] AnalogVoiceDrift& getAnalogDrift() noexcept { return mAnalogDrift; }
    [[nodiscard]] const AnalogVoiceDrift& getAnalogDrift() const noexcept { return mAnalogDrift; }
    void setAnalogMode(bool enabled) noexcept { mAnalogMode = enabled; }
    [[nodiscard]] bool isAnalogMode() const noexcept { return mAnalogMode; }
    void setAnalogDrift(float cents) noexcept { mPitchDriftCents = cents; }
    void setInitialPhase(float phase) noexcept { mOscInitialPhaseRad = phase; }

    // Direct sample & block rendering (accumulates into stereo output buffers)
    void renderSample(float& outL, float& outR, const ParameterSnapshot& params) noexcept;
    void renderBlockAccumulate(float* outL, float* outR, int numSamples, const ParameterSnapshot& params) noexcept;

private:
    // Sample Rate & Clocks
    double mSampleRate { 48000.0 };
    uint64_t mTriggerSample { 0 };
    float mHostBpm { 120.0f };

    // Voice State
    bool mActive { false };
    bool mIsHeld { false };
    bool mIsPedalLatched { false };
    int mMidiNote { 60 };
    float mVelocity { 0.8f };
    float mVelocityGain { 0.8f };
    float mPitchBendSemitones { 0.0f };

    // Character Circuits: Analog Pitch Drift & Free-Running Phase
    AnalogVoiceDrift mAnalogDrift;
    float mPitchDriftCents { 0.0f };
    float mOscInitialPhaseRad { 0.0f };
    bool mAnalogMode { false };

    // Oscillator Section
    BumblerTripleOscillatorSection mOscSection;

    // Filter Section (6-Mode Wasp XT Filter)
    WaspFilter mFilter;
    bool mFilterEnabled { false };

    // Modulation Sources (Dual LFOs & Dedicated MOD ENV)
    BumblerLFO mLfo1;
    BumblerLFO mLfo2;
    BumblerModEnvelope mModEnv;

    // Dual ADSR Envelopes (Amplitude & Filter Cutoff with envLink)
    BumblerDualADSR mDualEnvelopes;

    // Voice Steal 5ms Hann De-Click Crossfader
    float mLastOutL { 0.0f };
    float mLastOutR { 0.0f };
    float mStealSampleL { 0.0f };
    float mStealSampleR { 0.0f };
    int mStealSamplesTotal { 0 };
    int mStealSamplesRemaining { 0 };
};

} // namespace bumbler
