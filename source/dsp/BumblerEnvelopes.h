#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>
#include "BumblerCommon.h"
#include "ParameterSnapshot.h"

namespace bumbler {

/**
 * BumblerADSR: 4-stage exponential envelope generator.
 * Used for Amplitude and Filter Cutoff modulations.
 * Conforms to ORIGINAL_REQUEST.md § R3 and PROJECT.md § M3.
 */
class BumblerADSR {
public:
    enum class State : uint8_t {
        Idle = 0,
        Attack = 1,
        Decay = 2,
        Sustain = 3,
        Release = 4
    };

    // Test-harness compatible enum matching mod_ref::RefADSR
    enum RefState {
        IDLE = 0,
        ATTACK = 1,
        DECAY = 2,
        SUSTAIN = 3,
        RELEASE = 4
    };

    BumblerADSR() noexcept = default;

    void prepare(double sampleRate) noexcept {
        mSampleRate = (sampleRate > 100.0) ? sampleRate : 48000.0;
        reset();
    }

    void setSampleRate(double sampleRate) noexcept {
        prepare(sampleRate);
    }

    void reset() noexcept {
        mState = State::Idle;
        mValue = 0.0f;
        mCachedAttack = -1.0f;
        mCachedDecay = -1.0f;
        mCachedRelease = -1.0f;
    }

    void noteOn() noexcept {
        mState = State::Attack;
    }

    void noteOff() noexcept {
        if (mState != State::Idle) {
            mState = State::Release;
        }
    }

    void forceKill() noexcept {
        mState = State::Idle;
        mValue = 0.0f;
    }

    void setParameters(float aSec, float dSec, float sLevel, float rSec) noexcept {
        const float att = std::clamp(aSec, 0.0001f, 10.0f);
        if (std::abs(att - mCachedAttack) > 1.0e-5f) {
            mCachedAttack = att;
            mAttackRate = (att > 0.0005f) ? (1.0f / (att * static_cast<float>(mSampleRate))) : 1.0f;
        }

        const float dec = std::clamp(dSec, 0.001f, 10.0f);
        if (std::abs(dec - mCachedDecay) > 1.0e-5f) {
            mCachedDecay = dec;
            mDecayCoeff = std::exp(-1.0f / (dec * static_cast<float>(mSampleRate) * 0.5f));
        }

        mSustainLevel = std::clamp(sLevel, 0.0f, 1.0f);

        const float rel = std::clamp(rSec, 0.001f, 10.0f);
        if (std::abs(rel - mCachedRelease) > 1.0e-5f) {
            mCachedRelease = rel;
            mReleaseCoeff = std::exp(-1.0f / (rel * static_cast<float>(mSampleRate) * 0.5f));
        }
    }

    void update(float aSec, float dSec, float sLevel, float rSec, double sampleRate = 0.0) noexcept {
        if (sampleRate > 100.0) {
            mSampleRate = sampleRate;
        }
        setParameters(aSec, dSec, sLevel, rSec);
    }

    [[nodiscard]] float processSample() noexcept {
        switch (mState) {
            case State::Attack:
                mValue += mAttackRate;
                if (mValue >= 1.0f) {
                    mValue = 1.0f;
                    mState = State::Decay;
                }
                break;

            case State::Decay:
                mValue = mSustainLevel + (mValue - mSustainLevel) * mDecayCoeff;
                if (std::abs(mValue - mSustainLevel) < 0.001f) {
                    mValue = mSustainLevel;
                    mState = State::Sustain;
                }
                break;

            case State::Sustain:
                mValue = mSustainLevel;
                break;

            case State::Release:
                mValue *= mReleaseCoeff;
                if (mValue < 0.0001f) {
                    mValue = 0.0f;
                    mState = State::Idle;
                }
                break;

            case State::Idle:
            default:
                mValue = 0.0f;
                break;
        }
        return flushDenormal(mValue);
    }

    [[nodiscard]] float step() noexcept { return processSample(); }
    [[nodiscard]] float getValue() const noexcept { return mValue; }
    [[nodiscard]] float getLevel() const noexcept { return mValue; }
    [[nodiscard]] State getState() const noexcept { return mState; }
    [[nodiscard]] bool isActive() const noexcept { return mState != State::Idle; }

private:
    double mSampleRate { 48000.0 };
    State mState { State::Idle };
    float mValue { 0.0f };
    float mAttackRate { 0.01f };
    float mDecayCoeff { 0.999f };
    float mSustainLevel { 0.8f };
    float mReleaseCoeff { 0.999f };

    float mCachedAttack { -1.0f };
    float mCachedDecay { -1.0f };
    float mCachedRelease { -1.0f };
};

/**
 * BumblerModEnvelope: Dedicated 2-stage Attack-Decay envelope with bipolar depth.
 * Conforms to ORIGINAL_REQUEST.md § R3.
 */
class BumblerModEnvelope {
public:
    BumblerModEnvelope() noexcept = default;

    void prepare(double sampleRate) noexcept {
        mSampleRate = (sampleRate > 100.0) ? sampleRate : 48000.0;
        reset();
    }

    void reset() noexcept {
        mValue = 0.0f;
        mIsAttacking = false;
        mIsActive = false;
    }

    void trigger() noexcept {
        mValue = 0.0f;
        mIsAttacking = true;
        mIsActive = true;
    }

    void setParameters(float aSec, float dSec, float amount, double sampleRate = 0.0) noexcept {
        if (sampleRate > 100.0) mSampleRate = sampleRate;
        const float att = std::max(0.0005f, aSec);
        const float dec = std::max(0.001f, dSec);
        mAttackRate = 1.0f / (att * static_cast<float>(mSampleRate));
        mDecayCoeff = std::exp(-1.0f / (dec * static_cast<float>(mSampleRate) * 0.5f));
        mAmount = std::clamp(amount, -1.0f, 1.0f);
    }

    [[nodiscard]] float processSample() noexcept {
        if (!mIsActive) return 0.0f;

        if (mIsAttacking) {
            mValue += mAttackRate;
            if (mValue >= 1.0f) {
                mValue = 1.0f;
                mIsAttacking = false;
            }
        } else {
            mValue *= mDecayCoeff;
            if (mValue < 0.0001f) {
                mValue = 0.0f;
                mIsActive = false;
            }
        }
        return flushDenormal(mValue * mAmount);
    }

    [[nodiscard]] float getValue() const noexcept { return mValue * mAmount; }
    [[nodiscard]] float getUnscaledValue() const noexcept { return mValue; }
    [[nodiscard]] bool isActive() const noexcept { return mIsActive; }
    [[nodiscard]] float getAmount() const noexcept { return mAmount; }

private:
    double mSampleRate { 48000.0 };
    float mValue { 0.0f };
    float mAttackRate { 0.01f };
    float mDecayCoeff { 0.999f };
    float mAmount { 0.0f };
    bool mIsAttacking { false };
    bool mIsActive { false };
};

/**
 * BumblerDualADSR: Synchronized wrapper managing Amplitude and Filter ADSRs.
 * Implements the envLink toggle behavior.
 */
class BumblerDualADSR {
public:
    BumblerDualADSR() noexcept = default;

    void prepare(double sampleRate) noexcept {
        mAmpEnv.prepare(sampleRate);
        mFilterEnv.prepare(sampleRate);
    }

    void reset() noexcept {
        mAmpEnv.reset();
        mFilterEnv.reset();
    }

    void noteOn() noexcept {
        mAmpEnv.noteOn();
        mFilterEnv.noteOn();
    }

    void noteOff() noexcept {
        mAmpEnv.noteOff();
        mFilterEnv.noteOff();
    }

    void forceKill() noexcept {
        mAmpEnv.forceKill();
        mFilterEnv.forceKill();
    }

    void update(const ParameterSnapshot& params) noexcept {
        mAmpEnv.setParameters(params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
        if (params.envLink > 0.5f) {
            // envLink active: Filter ADSR mirrors Amplitude ADSR
            mFilterEnv.setParameters(params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
        } else {
            mFilterEnv.setParameters(params.filterAttack, params.filterDecay, params.filterSustain, params.filterRelease);
        }
    }

    [[nodiscard]] BumblerADSR& getAmpEnv() noexcept { return mAmpEnv; }
    [[nodiscard]] const BumblerADSR& getAmpEnv() const noexcept { return mAmpEnv; }
    [[nodiscard]] BumblerADSR& getFilterEnv() noexcept { return mFilterEnv; }
    [[nodiscard]] const BumblerADSR& getFilterEnv() const noexcept { return mFilterEnv; }

private:
    BumblerADSR mAmpEnv;
    BumblerADSR mFilterEnv;
};

} // namespace bumbler
