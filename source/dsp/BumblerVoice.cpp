#include "BumblerVoice.h"

namespace bumbler {

void BumblerVoice::prepare(double sampleRate) noexcept {
    mSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    mOscSection.prepare(mSampleRate);
    mFilter.prepare(mSampleRate);
    mDualEnvelopes.prepare(mSampleRate);
    mModEnv.prepare(mSampleRate);
    mLfo1.prepare(mSampleRate);
    mLfo2.prepare(mSampleRate);
    reset();
}

void BumblerVoice::reset() noexcept {
    mActive = false;
    mIsHeld = false;
    mIsPedalLatched = false;
    mMidiNote = 60;
    mVelocity = 0.0f;
    mVelocityGain = 0.0f;
    mPitchBendSemitones = 0.0f;
    mTriggerSample = 0;

    mPitchDriftCents = 0.0f;
    mOscInitialPhaseRad = 0.0f;
    mAnalogMode = false;
    mOscSection.reset(false);
    mFilter.reset();

    mDualEnvelopes.reset();
    mModEnv.reset();
    mLfo1.reset();
    mLfo2.reset();

    mLastOutL = 0.0f;
    mLastOutR = 0.0f;
    mStealSampleL = 0.0f;
    mStealSampleR = 0.0f;
    mStealSamplesTotal = 0;
    mStealSamplesRemaining = 0;
}

void BumblerVoice::noteOn(int midiNote, float velocity, uint64_t triggerSample) noexcept {
    noteOn(midiNote, velocity, triggerSample, mAnalogMode);
}

void BumblerVoice::noteOn(int midiNote, float velocity, uint64_t triggerSample, bool analogMode) noexcept {
    // 5ms Hann De-Click Steal Crossfade:
    // If voice was active and producing non-zero sound, latch output for smooth fade-out
    if (mActive && (std::abs(mLastOutL) > 1.0e-5f || std::abs(mLastOutR) > 1.0e-5f)) {
        mStealSampleL = mLastOutL;
        mStealSampleR = mLastOutR;
        mStealSamplesTotal = std::max(1, static_cast<int>(0.005 * mSampleRate));
        mStealSamplesRemaining = mStealSamplesTotal;
    } else {
        mStealSamplesRemaining = 0;
    }

    mMidiNote = std::clamp(midiNote, 0, 127);
    mVelocity = std::clamp(velocity, 0.0f, 1.0f);
    mVelocityGain = mVelocity * mVelocity; // Quadratic velocity response
    mTriggerSample = triggerSample;
    mIsHeld = true;
    mIsPedalLatched = false;
    mActive = true;
    mAnalogMode = analogMode;

    // Reset oscillator phase (free-running randomized when analog mode is active, hard sync 0.0 when off)
    if (mAnalogMode) {
        mPitchDriftCents = static_cast<float>(mAnalogDrift.triggerNote(true));
        const double p1 = mAnalogDrift.getInitialPhase(true);
        const double p2 = mAnalogDrift.getInitialPhase(true);
        const double p3 = mAnalogDrift.getInitialPhase(true);
        mOscInitialPhaseRad = static_cast<float>(p1);
        mOscSection.resetWithPhase(
            wrap01(static_cast<float>(p1 * kInvTwoPi)),
            wrap01(static_cast<float>(p2 * kInvTwoPi)),
            wrap01(static_cast<float>(p3 * kInvTwoPi))
        );
    } else {
        mPitchDriftCents = static_cast<float>(mAnalogDrift.triggerNote(false));
        mOscInitialPhaseRad = 0.0f;
        mOscSection.reset(false);
    }

    // Reset filter states on note trigger to prevent residual transient pops
    mFilter.reset();

    // Start Attack stage for Dual ADSR envelopes
    mDualEnvelopes.noteOn();

    // Trigger MOD ENV (Attack-Decay)
    mModEnv.trigger();

    // Trigger key reset for LFO 1 & LFO 2
    mLfo1.noteTrigger();
    mLfo2.noteTrigger();
}

void BumblerVoice::noteOff(float /*velocity*/) noexcept {
    mIsHeld = false;
    mIsPedalLatched = false;
    if (mActive) {
        mDualEnvelopes.noteOff();
    }
}

void BumblerVoice::forceKill() noexcept {
    mActive = false;
    mIsHeld = false;
    mIsPedalLatched = false;
    mDualEnvelopes.forceKill();
    mModEnv.reset();
    mLfo1.reset();
    mLfo2.reset();
    mFilter.reset();
    mStealSamplesRemaining = 0;
    mLastOutL = 0.0f;
    mLastOutR = 0.0f;
}

void BumblerVoice::renderSample(float& outL, float& outR, const ParameterSnapshot& params) noexcept {
    if (!mActive && mStealSamplesRemaining <= 0) {
        mLastOutL = 0.0f;
        mLastOutR = 0.0f;
        return;
    }

    // 1. Update LFO & Envelope parameters from Snapshot
    mLfo1.setRate(params.lfo1Rate);
    mLfo1.setDelay(params.lfo1Delay);
    mLfo1.setWaveform(static_cast<LfoWaveform>(std::clamp(static_cast<int>(params.lfo1Waveform), 0, 3)));
    mLfo1.setTempoSync(params.lfo1Sync > 0.5f, params.lfo1SyncDiv, mHostBpm);
    mLfo1.setKeyReset(params.lfo1KeyReset > 0.5f);

    mLfo2.setRate(params.lfo2Rate);
    mLfo2.setDelay(params.lfo2Delay);
    mLfo2.setWaveform(static_cast<LfoWaveform>(std::clamp(static_cast<int>(params.lfo2Waveform), 0, 3)));
    mLfo2.setTempoSync(params.lfo2Sync > 0.5f, params.lfo2SyncDiv, mHostBpm);
    mLfo2.setKeyReset(params.lfo2KeyReset > 0.5f);

    mModEnv.setParameters(params.modAttack, params.modDecay, params.modAmount, mSampleRate);

    // 2. Step Envelopes & LFOs
    const float lfo1Raw = mLfo1.processSample();
    const float lfo2Raw = mLfo2.processSample();
    const float modEnvRaw = mModEnv.processSample();

    // 3. Modulation Matrix Destination Routing
    float modPw = 0.0f;
    float modPitch1 = 0.0f;
    float modPitch2 = 0.0f;
    float modCutoffSemitones = 0.0f;
    float modOscMix = 0.0f;
    float modOsc1Gain = 1.0f;
    float modMasterAmp = 1.0f;
    float effectiveLfo1Amount = params.lfo1Amount;

    // MOD ENV Destinations (0=PW, 1=LFO1Amt, 2=Osc1Level, 3=Osc2Pitch)
    const auto modDest = static_cast<ModEnvDestination>(std::clamp(static_cast<int>(params.modTarget), 0, 3));
    switch (modDest) {
        case ModEnvDestination::PulseWidth:
            modPw += modEnvRaw * 0.45f;
            break;
        case ModEnvDestination::Lfo1Amount:
            effectiveLfo1Amount = std::clamp(params.lfo1Amount + modEnvRaw, 0.0f, 1.0f);
            break;
        case ModEnvDestination::Osc1Level:
            modOsc1Gain = std::clamp(1.0f + modEnvRaw, 0.0f, 2.0f);
            break;
        case ModEnvDestination::Osc2Pitch:
            modPitch2 += modEnvRaw * 24.0f; // ±2 octaves
            break;
    }

    // LFO 1 Destinations (0=Osc12Pitch, 1=FilterCutoff, 2=PulseWidth)
    const float lfo1Signal = lfo1Raw * effectiveLfo1Amount;
    const auto lfo1Dest = static_cast<Lfo1Destination>(std::clamp(static_cast<int>(params.lfo1Target), 0, 2));
    switch (lfo1Dest) {
        case Lfo1Destination::Osc12Pitch:
            modPitch1 += lfo1Signal * 12.0f;
            modPitch2 += lfo1Signal * 12.0f;
            break;
        case Lfo1Destination::FilterCutoff:
            modCutoffSemitones += lfo1Signal * 36.0f;
            break;
        case Lfo1Destination::PulseWidth:
            modPw += lfo1Signal * 0.45f;
            break;
    }

    // LFO 2 Destinations (0=Osc1Pitch, 1=OscMix, 2=MasterAmp)
    const float lfo2Signal = lfo2Raw * params.lfo2Amount;
    const auto lfo2Dest = static_cast<Lfo2Destination>(std::clamp(static_cast<int>(params.lfo2Target), 0, 2));
    switch (lfo2Dest) {
        case Lfo2Destination::Osc1Pitch:
            modPitch1 += lfo2Signal * 12.0f;
            break;
        case Lfo2Destination::OscMix:
            modOscMix += lfo2Signal * 0.5f;
            break;
        case Lfo2Destination::MasterAmp:
            modMasterAmp = std::clamp(1.0f + lfo2Signal * 0.5f, 0.0f, 1.0f);
            break;
    }

    // 4. Frequency Calculations & Render 3-Oscillator Section
    mAnalogMode = (params.analogMode >= 0.5f);
    const float effDriftCents = mAnalogMode ? mPitchDriftCents : 0.0f;
    const float baseNote = static_cast<float>(mMidiNote) + mPitchBendSemitones + effDriftCents * 0.01f;
    const float fBase = midiNoteToHz(baseNote);
    const float oscOut = mOscSection.process(fBase, params, modPw, modPitch1, modPitch2, modOscMix, modOsc1Gain);

    // 5. Dual ADSR Envelopes Update & Filter Cutoff
    // Link Toggle Check: BumblerDualADSR handles envLink automatically
    mDualEnvelopes.update(params);
    const float rawFilterEnv = mDualEnvelopes.getFilterEnv().processSample();

    // Velocity scaling on Filter Envelope
    const float velFilterScaling = (1.0f - params.velToFilter) + params.velToFilter * mVelocityGain;
    const float effectiveFilterEnv = rawFilterEnv * velFilterScaling;

    // Cutoff modulation: Keyboard Tracking + Bipolar Envelope Amount + External LFO/Matrix
    const float effectiveCutoff = WaspFilter::calculateModulatedCutoff(
        params.filterCutoff,
        baseNote,
        params.filterKbTrack,
        params.filterEnvAmount,
        effectiveFilterEnv,
        static_cast<float>(mSampleRate),
        modCutoffSemitones
    );

    // 6. 6-Mode Wasp XT Filter Processing
    float filteredOut = oscOut;
    if (mFilterEnabled) {
        const auto mode = static_cast<WaspFilterMode>(static_cast<int>(params.filterMode));
        filteredOut = mFilter.processSample(oscOut, effectiveCutoff, params.filterResonance, mode);
    }

    // 7. Amplitude ADSR & Master Amplitude Mod
    const float ampEnvLevel = mDualEnvelopes.getAmpEnv().processSample();
    if (!mDualEnvelopes.getAmpEnv().isActive()) {
        mActive = false;
    }

    // 8. Velocity Gain Routing & Amplitude Modulation
    const float velScaling = (1.0f - params.velToAmp) + params.velToAmp * mVelocityGain;
    float voiceL = filteredOut * ampEnvLevel * velScaling * modMasterAmp;
    float voiceR = voiceL;

    // 9. Voice Steal 5ms Hann De-Click Crossfading
    if (mStealSamplesRemaining > 0) {
        const float progress = 1.0f - static_cast<float>(mStealSamplesRemaining) / static_cast<float>(mStealSamplesTotal);
        // Hann window: w = 0.5 * (1 + cos(pi * progress))
        const float w = 0.5f * (1.0f + std::cos(kPi * progress));
        voiceL = (1.0f - w) * voiceL + w * mStealSampleL;
        voiceR = (1.0f - w) * voiceR + w * mStealSampleR;
        --mStealSamplesRemaining;
    }

    mLastOutL = voiceL;
    mLastOutR = voiceR;

    outL += voiceL;
    outR += voiceR;
}

void BumblerVoice::renderBlockAccumulate(float* outL, float* outR, int numSamples, const ParameterSnapshot& params) noexcept {
    for (int i = 0; i < numSamples; ++i) {
        renderSample(outL[i], outR[i], params);
    }
}

} // namespace bumbler
