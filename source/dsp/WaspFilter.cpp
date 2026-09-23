#include "WaspFilter.h"

namespace bumbler {

void WaspFilter::prepare(double sampleRate) noexcept {
    mSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    reset();
}

void WaspFilter::reset() noexcept {
    mStage1.reset();
    mStage2.reset();
    mFirstBlock = true;
    mCurrentG = 0.1;
    mCurrentK = 2.0;
}

float WaspFilter::processSample(float in, double cutoffHz, double resonance, WaspFilterMode mode) noexcept {
    // 1. Safe Parameter Clamping:
    // Limit cutoff to [20.0 Hz, 0.48 * sampleRate] to guarantee that tan(pi * fc / fs) remains finite.
    const double fc = std::clamp(cutoffHz, 20.0, 0.48 * mSampleRate);
    const double reso = std::clamp(resonance, 0.0, 0.99);

    // 2. ZDF State-Variable Filter Coefficients
    const double g = std::tan(bumbler::kPi * fc / mSampleRate);
    const double k = 2.0 - 1.9 * reso; // k in (0.1, 2.0]

    double lp1 = 0.0, bp1 = 0.0, hp1 = 0.0, nt1 = 0.0;
    double lp2 = 0.0, bp2 = 0.0, hp2 = 0.0, nt2 = 0.0;

    const double inputD = static_cast<double>(in);

    // 3. 6 Discrete Filter Topologies
    switch (mode) {
        case WaspFilterMode::LP12: {
            // Mode 0: 2-pole lowpass (~12 dB/oct roll-off)
            mStage1.step(inputD, g, k, lp1, bp1, hp1, nt1);
            return static_cast<float>(flushDenormal(static_cast<float>(lp1)));
        }

        case WaspFilterMode::LP24: {
            // Mode 1: 4-pole cascaded lowpass (~24 dB/oct roll-off) with analog warmth saturation
            mStage1.step(inputD, g, k, lp1, bp1, hp1, nt1);
            // Analog soft saturation between cascaded stages (simulates 4069UB CMOS overdrive)
            const double stageIn = std::tanh(lp1 * 1.2) / 1.2;
            mStage2.step(stageIn, g, k, lp2, bp2, hp2, nt2);
            return static_cast<float>(flushDenormal(static_cast<float>(lp2)));
        }

        case WaspFilterMode::LP_NT: {
            // Mode 2: 2-pole lowpass cascaded with a notch filter at cutoff for vowel formant scoop
            mStage1.step(inputD, g, k, lp1, bp1, hp1, nt1);
            mStage2.step(lp1, g, k, lp2, bp2, hp2, nt2);
            return static_cast<float>(flushDenormal(static_cast<float>(nt2)));
        }

        case WaspFilterMode::DBL_NT: {
            // Mode 3: Double notch filter producing two distinct attenuation nulls
            // Notch 1 at 0.5 octaves below cutoff (fc * 1/sqrt(2))
            // Notch 2 at 0.5 octaves above cutoff (fc * sqrt(2))
            const double fc1 = std::clamp(fc * 0.7071067811865475, 20.0, 0.48 * mSampleRate);
            const double fc2 = std::clamp(fc * 1.4142135623730951, 20.0, 0.48 * mSampleRate);
            const double g1 = std::tan(bumbler::kPi * fc1 / mSampleRate);
            const double g2 = std::tan(bumbler::kPi * fc2 / mSampleRate);

            mStage1.step(inputD, g1, k, lp1, bp1, hp1, nt1);
            mStage2.step(nt1, g2, k, lp2, bp2, hp2, nt2);
            return static_cast<float>(flushDenormal(static_cast<float>(nt2)));
        }

        case WaspFilterMode::BP24: {
            // Mode 4: 4-pole cascaded bandpass with normalized unity passband gain (k * bp)
            mStage1.step(inputD, g, k, lp1, bp1, hp1, nt1);
            const double bpNorm1 = k * bp1;
            mStage2.step(bpNorm1, g, k, lp2, bp2, hp2, nt2);
            return static_cast<float>(flushDenormal(static_cast<float>(k * bp2)));
        }

        case WaspFilterMode::HP24: {
            // Mode 5: 4-pole cascaded highpass (~24 dB/oct roll-off below cutoff)
            mStage1.step(inputD, g, k, lp1, bp1, hp1, nt1);
            mStage2.step(hp1, g, k, lp2, bp2, hp2, nt2);
            return static_cast<float>(flushDenormal(static_cast<float>(hp2)));
        }

        default: {
            mStage1.step(inputD, g, k, lp1, bp1, hp1, nt1);
            return static_cast<float>(flushDenormal(static_cast<float>(lp1)));
        }
    }
}

void WaspFilter::processBlock(
    const float* input,
    float* output,
    int numSamples,
    float cutoffHz,
    float resonance,
    WaspFilterMode mode
) noexcept {
    if (input == nullptr || output == nullptr || numSamples <= 0) return;

    constexpr int kSubBlockSize = 16;
    int samplesRemaining = numSamples;
    int offset = 0;

    const double fc = std::clamp(static_cast<double>(cutoffHz), 20.0, 0.48 * mSampleRate);
    const double reso = std::clamp(static_cast<double>(resonance), 0.0, 0.99);
    const double targetG = std::tan(bumbler::kPi * fc / mSampleRate);
    const double targetK = 2.0 - 1.9 * reso;

    if (mFirstBlock) {
        mCurrentG = targetG;
        mCurrentK = targetK;
        mFirstBlock = false;
    }

    while (samplesRemaining > 0) {
        const int chunkSize = std::min(samplesRemaining, kSubBlockSize);
        const double stepG = (targetG - mCurrentG) / static_cast<double>(chunkSize);
        const double stepK = (targetK - mCurrentK) / static_cast<double>(chunkSize);

        for (int i = 0; i < chunkSize; ++i) {
            mCurrentG += stepG;
            mCurrentK += stepK;

            const double inD = static_cast<double>(input[offset + i]);
            double lp1 = 0.0, bp1 = 0.0, hp1 = 0.0, nt1 = 0.0;
            double lp2 = 0.0, bp2 = 0.0, hp2 = 0.0, nt2 = 0.0;
            float outVal = 0.0f;

            switch (mode) {
                case WaspFilterMode::LP12: {
                    mStage1.step(inD, mCurrentG, mCurrentK, lp1, bp1, hp1, nt1);
                    outVal = static_cast<float>(lp1);
                    break;
                }
                case WaspFilterMode::LP24: {
                    mStage1.step(inD, mCurrentG, mCurrentK, lp1, bp1, hp1, nt1);
                    const double stageIn = std::tanh(lp1 * 1.2) / 1.2;
                    mStage2.step(stageIn, mCurrentG, mCurrentK, lp2, bp2, hp2, nt2);
                    outVal = static_cast<float>(lp2);
                    break;
                }
                case WaspFilterMode::LP_NT: {
                    mStage1.step(inD, mCurrentG, mCurrentK, lp1, bp1, hp1, nt1);
                    mStage2.step(lp1, mCurrentG, mCurrentK, lp2, bp2, hp2, nt2);
                    outVal = static_cast<float>(nt2);
                    break;
                }
                case WaspFilterMode::DBL_NT: {
                    const double fc1 = std::clamp(fc * 0.7071067811865475, 20.0, 0.48 * mSampleRate);
                    const double fc2 = std::clamp(fc * 1.4142135623730951, 20.0, 0.48 * mSampleRate);
                    const double g1 = std::tan(bumbler::kPi * fc1 / mSampleRate);
                    const double g2 = std::tan(bumbler::kPi * fc2 / mSampleRate);
                    mStage1.step(inD, g1, mCurrentK, lp1, bp1, hp1, nt1);
                    mStage2.step(nt1, g2, mCurrentK, lp2, bp2, hp2, nt2);
                    outVal = static_cast<float>(nt2);
                    break;
                }
                case WaspFilterMode::BP24: {
                    mStage1.step(inD, mCurrentG, mCurrentK, lp1, bp1, hp1, nt1);
                    mStage2.step(mCurrentK * bp1, mCurrentG, mCurrentK, lp2, bp2, hp2, nt2);
                    outVal = static_cast<float>(mCurrentK * bp2);
                    break;
                }
                case WaspFilterMode::HP24: {
                    mStage1.step(inD, mCurrentG, mCurrentK, lp1, bp1, hp1, nt1);
                    mStage2.step(hp1, mCurrentG, mCurrentK, lp2, bp2, hp2, nt2);
                    outVal = static_cast<float>(hp2);
                    break;
                }
                default: {
                    mStage1.step(inD, mCurrentG, mCurrentK, lp1, bp1, hp1, nt1);
                    outVal = static_cast<float>(lp1);
                    break;
                }
            }

            output[offset + i] = flushDenormal(outVal);
        }

        mCurrentG = targetG;
        mCurrentK = targetK;
        offset += chunkSize;
        samplesRemaining -= chunkSize;
    }
}

float WaspFilter::calculateModulatedCutoff(
    float baseCutoffHz,
    float midiNoteWithBend,
    float kbTrack,
    float envAmount,
    float envLevel,
    float sampleRate,
    float extModCutoffSemitones
) noexcept {
    // 1. Keyboard Tracking relative to MIDI Note 60 (C4 = 261.63 Hz)
    const float kbSemitones = (midiNoteWithBend - 60.0f) * std::clamp(kbTrack, 0.0f, 1.0f);
    const float kbMultiplier = semitonesToRatio(kbSemitones);

    // 2. Bipolar Envelope Modulation (-1.0 to +1.0 scaling up to +/- 5 octaves = 60 semitones)
    const float clampedEnvAmount = std::clamp(envAmount, -1.0f, 1.0f);
    const float clampedEnvLevel = std::clamp(envLevel, 0.0f, 1.0f);
    const float envSemitones = clampedEnvAmount * clampedEnvLevel * 60.0f;
    const float envMultiplier = semitonesToRatio(envSemitones);

    // 3. External LFO/Matrix Cutoff Modulation
    const float extMultiplier = semitonesToRatio(extModCutoffSemitones);

    // 4. Combined Modulated Cutoff Clamping
    const float modulatedCutoff = baseCutoffHz * kbMultiplier * envMultiplier * extMultiplier;
    return std::clamp(modulatedCutoff, 20.0f, 0.48f * sampleRate);
}

} // namespace bumbler
