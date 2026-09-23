#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <numeric>
#include <string>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <new>
#include <algorithm>
#include <random>

// ============================================================================
// Real-Time Heap Allocation Tracking Hooks
// ============================================================================
#ifndef BUMBLER_ALLOCATION_TRACKING_DEFINED
#define BUMBLER_ALLOCATION_TRACKING_DEFINED

static std::atomic<bool> gTrackAllocations { false };
static std::atomic<size_t> gAllocationCount { 0 };
static std::atomic<size_t> gAllocatedBytes { 0 };

inline void resetAllocationTracker() {
    gAllocationCount.store(0, std::memory_order_seq_cst);
    gAllocatedBytes.store(0, std::memory_order_seq_cst);
}

inline void enableAllocationTracker(bool enable) {
    gTrackAllocations.store(enable, std::memory_order_seq_cst);
}

inline size_t getAllocationCount() {
    return gAllocationCount.load(std::memory_order_seq_cst);
}

#endif // BUMBLER_ALLOCATION_TRACKING_DEFINED

// ============================================================================
// Test Assertion & Runner Macros
// ============================================================================
static int gGlobalTestsPassed = 0;
static int gGlobalTestsFailed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++gGlobalTestsFailed; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_NEAR(actual, expected, tol, msg) \
    do { \
        const double diff = std::abs(static_cast<double>(actual) - static_cast<double>(expected)); \
        if (diff > static_cast<double>(tol)) { \
            std::cerr << "  [FAIL] " << msg << " (actual: " << (actual) \
                      << ", expected: " << (expected) \
                      << ", diff: " << diff << " > tol: " << (tol) \
                      << " at " << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++gGlobalTestsFailed; \
            return; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUNNING] " << #fn << "..." << std::endl; \
        const int prevFailures = gGlobalTestsFailed; \
        fn(); \
        if (gGlobalTestsFailed == prevFailures) { \
            std::cout << "  [PASS] " << #fn << "\n"; \
            ++gGlobalTestsPassed; \
        } \
    } while (0)

// ============================================================================
// ParameterSnapshot Interface Contract (PROJECT.md § Interface Contracts)
// ============================================================================
#if __has_include("../source/dsp/ParameterSnapshot.h")
#include "../source/dsp/ParameterSnapshot.h"
using bumbler::ParameterSnapshot;
#else
struct ParameterSnapshot {
    // Oscillators
    float osc1Waveform = 0.0f; // 0=saw, 1=square, 2=sine, 3=noise
    float osc1Octave = 0.0f;   // -3 to +3
    float osc1Fine = 0.0f;     // -1 to +1 semitone
    float osc2Waveform = 0.0f;
    float osc2Octave = 0.0f;
    float osc2Fine = 0.0f;
    float oscMix = 0.5f;       // 0.0 (OSC1) to 1.0 (OSC2)
    float osc3Waveform = 0.0f; // 0=square, 1=saw
    float osc3Level = 0.0f;    // 0.0 to 1.0
    float ringModMix = 0.0f;   // 0.0 to 1.0
    float pulseWidth = 0.5f;   // 0.01 to 0.99
    float fmAmount = 0.0f;     // 0.0 to 1.0

    // Filter
    float filterMode = 0.0f;   // 0=LP12, 1=LP24, 2=LP+NT, 3=DBL.NT, 4=BP24, 5=HP24
    float filterCutoff = 1000.0f; // 20 Hz to 20 kHz
    float filterResonance = 0.1f; // 0.0 to 1.0
    float filterKbTrack = 0.0f;   // 0.0 to 1.0
    float filterEnvAmount = 0.0f; // -1.0 to +1.0

    // Envelopes
    float ampAttack = 0.01f, ampDecay = 0.2f, ampSustain = 0.8f, ampRelease = 0.3f;
    float filterAttack = 0.01f, filterDecay = 0.2f, filterSustain = 0.5f, filterRelease = 0.3f;
    float envLink = 0.0f; // 0 or 1
    float modAttack = 0.01f, modDecay = 0.2f, modAmount = 0.0f, modTarget = 0.0f;

    // LFOs
    float lfo1Waveform = 0.0f, lfo1Rate = 2.0f, lfo1Delay = 0.0f, lfo1Sync = 0.0f, lfo1KeyReset = 0.0f, lfo1Amount = 0.0f, lfo1Target = 0.0f;
    float lfo2Waveform = 0.0f, lfo2Rate = 2.0f, lfo2Delay = 0.0f, lfo2Sync = 0.0f, lfo2KeyReset = 0.0f, lfo2Amount = 0.0f, lfo2Target = 0.0f;

    // Character & Output
    float driveEnabled = 0.0f, driveAmount = 0.0f, driveTone = 0.5f;
    float dualMode = 0.0f, analogMode = 0.0f, wNoiseMode = 0.0f;
    float masterVolume = 0.7f;
};
#endif

// ============================================================================
// Core DSP Math & Analysis Oracles
// ============================================================================
namespace bumbler_test {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

// Discrete Single-Bin Fourier Power (DFT bin evaluation)
inline double computeFourierBinPower(const float* buffer, int numSamples, double targetFreqHz, double sampleRate) {
    if (numSamples <= 0 || sampleRate <= 0.0) return 0.0;
    double realSum = 0.0;
    double imagSum = 0.0;
    const double omega = kTwoPi * targetFreqHz / sampleRate;

    for (int n = 0; n < numSamples; ++n) {
        const double angle = omega * n;
        realSum += buffer[n] * std::cos(angle);
        imagSum -= buffer[n] * std::sin(angle);
    }

    const double mag = (2.0 / numSamples) * std::sqrt(realSum * realSum + imagSum * imagSum);
    return mag * mag; // Power
}

inline double computeFourierBinMagnitude(const float* buffer, int numSamples, double targetFreqHz, double sampleRate) {
    return std::sqrt(computeFourierBinPower(buffer, numSamples, targetFreqHz, sampleRate));
}

// Peak and RMS Measurement
inline float computePeak(const float* buffer, int numSamples) {
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        const float absVal = std::abs(buffer[i]);
        if (absVal > peak) peak = absVal;
    }
    return peak;
}

inline double computeRms(const float* buffer, int numSamples) {
    if (numSamples <= 0) return 0.0;
    double sumSq = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        sumSq += static_cast<double>(buffer[i]) * static_cast<double>(buffer[i]);
    }
    return std::sqrt(sumSq / numSamples);
}

inline double toDb(double linearAmplitude) {
    if (linearAmplitude <= 1.0e-12) return -240.0;
    return 20.0 * std::log10(linearAmplitude);
}

// Pearson Cross-Correlation Coefficient r in [-1, +1]
inline double computeCrossCorrelation(const float* left, const float* right, int numSamples) {
    if (numSamples <= 0) return 1.0;
    double sumL = 0.0, sumR = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        sumL += left[i];
        sumR += right[i];
    }
    const double meanL = sumL / numSamples;
    const double meanR = sumR / numSamples;

    double num = 0.0, denomL = 0.0, denomR = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        const double dL = left[i] - meanL;
        const double dR = right[i] - meanR;
        num += dL * dR;
        denomL += dL * dL;
        denomR += dR * dR;
    }

    const double denom = std::sqrt(denomL * denomR);
    if (denom <= 1.0e-12) return 1.0;
    return num / denom;
}

// Statistical Variance
inline double computeVariance(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    double mean = 0.0;
    for (double v : values) mean += v;
    mean /= values.size();

    double var = 0.0;
    for (double v : values) {
        const double diff = v - mean;
        var += diff * diff;
    }
    return var / (values.size() - 1);
}

// Sine wave generator
inline void generateSine(float* buffer, int numSamples, double freqHz, double sampleRate, float amplitude = 1.0f) {
    const double phaseInc = kTwoPi * freqHz / sampleRate;
    for (int i = 0; i < numSamples; ++i) {
        buffer[i] = amplitude * static_cast<float>(std::sin(phaseInc * i));
    }
}

// Subnormal / Denormal detector
inline bool containsSubnormals(const float* buffer, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        if (std::fpclassify(buffer[i]) == FP_SUBNORMAL) return true;
    }
    return false;
}

inline bool containsNonFinite(const float* buffer, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        if (std::isnan(buffer[i]) || std::isinf(buffer[i])) return true;
    }
    return false;
}

// Scoped FTZ / DAZ Guard
struct ScopedNoDenormalsGuard {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    unsigned int originalMxcsr;
    ScopedNoDenormalsGuard() {
        originalMxcsr = _mm_getcsr();
        _mm_setcsr(originalMxcsr | 0x8040); // Bit 15 FTZ, Bit 6 DAZ
    }
    ~ScopedNoDenormalsGuard() {
        _mm_setcsr(originalMxcsr);
    }
#else
    ScopedNoDenormalsGuard() {}
    ~ScopedNoDenormalsGuard() {}
#endif
};

// ============================================================================
// Authoritative Reference Mathematical Oracle Models
// ============================================================================

// 1. Chamberlin / State Variable Filter (SVF) Reference Model
class ReferenceWaspFilter {
public:
    enum Mode {
        LP12 = 0,
        LP24 = 1,
        LP_NT = 2,
        DBL_NT = 3,
        BP24 = 4,
        HP24 = 5
    };

    void prepare(double sampleRate) {
        fs = sampleRate;
        reset();
    }

    void reset() {
        s1_a = s2_a = 0.0;
        s1_b = s2_b = 0.0;
        s1_c = s2_c = 0.0;
    }

    float processSample(float input, double cutoffHz, double resonance, Mode mode) {
        // Clamp cutoff to safe bounds [20 Hz, 0.48 * fs]
        const double fc = std::clamp(cutoffHz, 20.0, 0.48 * fs);
        const double reso = std::clamp(resonance, 0.0, 0.99);

        // SVF coefficients
        const double g = std::tan(kPi * fc / fs);
        const double k = 2.0 - 1.9 * reso; // damping: k in (0.1, 2.0]

        auto svfStep = [&](double in, double& s1, double& s2, double& lp, double& bp, double& hp, double& notch) {
            const double d = 1.0 + g * (g + k);
            hp = (in - (g + k) * s1 - s2) / d;
            const double v1 = g * hp;
            bp = v1 + s1;
            s1 = bp + v1;
            const double v2 = g * bp;
            lp = v2 + s2;
            s2 = lp + v2;
            notch = hp + lp;
        };

        double lp1, bp1, hp1, nt1;
        svfStep(input, s1_a, s2_a, lp1, bp1, hp1, nt1);

        switch (mode) {
            case LP12:
                return static_cast<float>(lp1);

            case LP24: {
                // Cascaded 2nd stage
                double lp2, bp2, hp2, nt2;
                // Soft saturating non-linearity between stages for analog warmth
                const double stageIn = std::tanh(lp1 * 1.2) / 1.2;
                svfStep(stageIn, s1_b, s2_b, lp2, bp2, hp2, nt2);
                return static_cast<float>(lp2);
            }

            case LP_NT: {
                // Cascade LP12 with Notch
                double lp2, bp2, hp2, nt2;
                svfStep(lp1, s1_b, s2_b, lp2, bp2, hp2, nt2);
                return static_cast<float>(nt2);
            }

            case DBL_NT: {
                // Two notches at f1 = fc * 0.707 and f2 = fc * 1.414
                const double fc1 = std::clamp(fc * 0.7071, 20.0, 0.48 * fs);
                const double fc2 = std::clamp(fc * 1.4142, 20.0, 0.48 * fs);
                const double g1 = std::tan(kPi * fc1 / fs);
                const double g2 = std::tan(kPi * fc2 / fs);
                const double d1 = 1.0 + g1 * (g1 + k);
                const double d2 = 1.0 + g2 * (g2 + k);

                const double hp_a = (input - (g1 + k) * s1_a - s2_a) / d1;
                const double v1_a = g1 * hp_a;
                const double bp_a = v1_a + s1_a;
                s1_a = bp_a + v1_a;
                const double v2_a = g1 * bp_a;
                const double lp_a = v2_a + s2_a;
                s2_a = lp_a + v2_a;
                const double nt_a = hp_a + lp_a;

                const double hp_b = (nt_a - (g2 + k) * s1_b - s2_b) / d2;
                const double v1_b = g2 * hp_b;
                const double bp_b = v1_b + s1_b;
                s1_b = bp_b + v1_b;
                const double v2_b = g2 * bp_b;
                const double lp_b = v2_b + s2_b;
                s2_b = lp_b + v2_b;
                return static_cast<float>(hp_b + lp_b);
            }

            case BP24: {
                // Cascade of two 2-pole Bandpass stages with unity gain normalization (k * bp)
                double lp2, bp2, hp2, nt2;
                const double bpNorm1 = k * bp1;
                svfStep(bpNorm1, s1_b, s2_b, lp2, bp2, hp2, nt2);
                return static_cast<float>(k * bp2);
            }

            case HP24: {
                // Cascade of two 2-pole Highpass stages
                double lp2, bp2, hp2, nt2;
                svfStep(hp1, s1_b, s2_b, lp2, bp2, hp2, nt2);
                return static_cast<float>(hp2);
            }
        }
        return static_cast<float>(lp1);
    }

private:
    double fs = 48000.0;
    double s1_a = 0.0, s2_a = 0.0;
    double s1_b = 0.0, s2_b = 0.0;
    double s1_c = 0.0, s2_c = 0.0;
};

// 2. Reference Character Circuit Models
class ReferenceCharacterCircuits {
public:
    // Asymmetric overdrive saturator + tone tilt
    static float processDistortion(float input, float drive, float tone, bool enabled) {
        if (!enabled || drive <= 0.001f) return input;
        const float gain = 1.0f + drive * 9.0f;
        const float x = input * gain;
        // Asymmetric soft-clipping saturation
        const float sat = std::tanh(x + 0.1f * x * x);

        // Tone filter: 0.0 = dark lowpass, 1.0 = bright high-tilt
        // Simple 1-pole tone approximation
        static float toneState = 0.0f;
        const float alpha = 0.1f + 0.8f * tone;
        toneState += alpha * (sat - toneState);
        const float shaped = (1.0f - tone) * toneState + tone * sat;
        return std::clamp(shaped, -1.05f, 1.05f);
    }

    // Dual mode stereo widening: returns pair of (L, R)
    static std::pair<float, float> processDualMode(float monoInput, float detunePhaseL, float detunePhaseR, bool enabled) {
        if (!enabled) {
            return { monoInput, monoInput };
        }
        // Voice doubling with slight detune phase modulation and pan spread
        const float modL = std::sin(detunePhaseL);
        const float modR = std::cos(detunePhaseR);
        const float outL = monoInput * (0.85f + 0.15f * modL);
        const float outR = monoInput * (0.85f + 0.15f * modR);
        return { outL * 0.7071f, outR * 0.7071f };
    }
};

} // namespace bumbler_test
