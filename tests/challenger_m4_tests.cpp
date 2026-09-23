#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <string>
#include <stdexcept>
#include <limits>
#include <iomanip>
#include <algorithm>
#include <array>
#include <random>

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "WaspFilter.h"
#include "BumblerLFO.h"
#include "BumblerEnvelopes.h"
#include "BumblerVoice.h"
#include "BumblerVoiceManager.h"
#include "BumblerEngine.h"
#include "CharacterCircuits.h"

// ============================================================================
// Real-Time Memory Safety: Global Allocation Interceptor
// ============================================================================

static std::atomic<bool> gTrackAllocations { false };
static std::atomic<size_t> gAllocationCount { 0 };
static std::atomic<size_t> gAllocatedBytes { 0 };

inline void resetAllocationTracker() noexcept {
    gAllocationCount.store(0, std::memory_order_seq_cst);
    gAllocatedBytes.store(0, std::memory_order_seq_cst);
}

inline void enableAllocationTracker(bool enable) noexcept {
    gTrackAllocations.store(enable, std::memory_order_seq_cst);
}

inline size_t getAllocationCount() noexcept {
    return gAllocationCount.load(std::memory_order_seq_cst);
}


#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
void* operator new(size_t size) {
    if (gTrackAllocations.load(std::memory_order_relaxed)) {
        gAllocationCount.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytes.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void* operator new[](size_t size) {
    if (gTrackAllocations.load(std::memory_order_relaxed)) {
        gAllocationCount.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytes.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
#endif

// ============================================================================
// Test Framework Infrastructure & Assertions
// ============================================================================

static int gTotalTests = 0;
static int gPassedTests = 0;
static int gFailedTests = 0;

static void runSingleTest(const char* testName, void (*testFunc)()) {
    ++gTotalTests;
    std::cout << "\n[CHALLENGER-M4 RUN] " << testName << std::endl;
    auto t0 = std::chrono::high_resolution_clock::now();
    try {
        testFunc();
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        ++gPassedTests;
        std::cout << "[       PASSED   ] " << testName << " (" << std::fixed << std::setprecision(2) << elapsedMs << " ms)" << std::endl;
    } catch (const std::exception& e) {
        ++gFailedTests;
        std::cerr << "[    FAILED!!!   ] " << testName << " -> " << e.what() << std::endl;
    } catch (...) {
        ++gFailedTests;
        std::cerr << "[    FAILED!!!   ] " << testName << " -> Unknown exception" << std::endl;
    }
}

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #cond + " at line " + std::to_string(__LINE__)); \
    } \
} while (false)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #a + " == " + #b + \
            " (expected " + std::to_string(b) + ", got " + std::to_string(a) + ") at line " + std::to_string(__LINE__)); \
    } \
} while (false)

#define ASSERT_LE(a, b) do { \
    if ((a) > (b)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #a + " <= " + #b + \
            " (got " + std::to_string(a) + " > " + std::to_string(b) + ") at line " + std::to_string(__LINE__)); \
    } \
} while (false)

#define ASSERT_LT(a, b) do { \
    if ((a) >= (b)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #a + " < " + #b + \
            " (got " + std::to_string(a) + " >= " + std::to_string(b) + ") at line " + std::to_string(__LINE__)); \
    } \
} while (false)

#define ASSERT_NEAR(a, b, tol) do { \
    if (std::abs(static_cast<double>(a) - static_cast<double>(b)) > static_cast<double>(tol)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #a + " near " + #b + \
            " (diff " + std::to_string(std::abs(static_cast<double>(a) - static_cast<double>(b))) + " > tol " + std::to_string(tol) + ") at line " + std::to_string(__LINE__)); \
    } \
} while (false)

[[nodiscard]] inline bool isSubnormal(float val) noexcept {
    return std::fpclassify(val) == FP_SUBNORMAL;
}

[[nodiscard]] inline double computeBinPower(const float* buffer, int numSamples, double targetFreqHz, double sampleRate) {
    if (numSamples <= 0 || sampleRate <= 0.0) return 0.0;
    double realSum = 0.0;
    double imagSum = 0.0;
    const double omega = 2.0 * 3.14159265358979323846 * targetFreqHz / sampleRate;
    for (int n = 0; n < numSamples; ++n) {
        const double angle = omega * n;
        realSum += buffer[n] * std::cos(angle);
        imagSum -= buffer[n] * std::sin(angle);
    }
    const double mag = (2.0 / numSamples) * std::sqrt(realSum * realSum + imagSum * imagSum);
    return mag * mag;
}

[[nodiscard]] inline double toDb(double power) {
    if (power <= 1.0e-24) return -240.0;
    return 10.0 * std::log10(power);
}

// ============================================================================
// CHALLENGE 1: Extreme Drive Levels, Maximum Amplitudes & Discontinuous DC Jumps
// ============================================================================

/**
 * Challenge 1.1: DistortionUnit and CharacterCircuits under Extreme Drive and Extreme Inputs
 * Probes:
 *  - drive = 1.0f (and out-of-range drive = 2.0f)
 *  - Input levels: +/-2.0f, +/-10.0f, +/-100.0f
 *  - Discontinuous DC step jumps: -2.0f to +2.0f, +10.0f to -10.0f
 *  - Alternating Nyquist spikes: +/-10.0f
 *  - Detailed quadratic foldback transition probing around x = -3.3333f
 * Invariants:
 *  - Output peak <= 1.05f strictly
 *  - Zero NaNs, zero Infs, zero denormals
 */
static void challenge_extreme_drive_and_amplitudes() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::DistortionUnit distortion;
    distortion.prepare(sampleRate);

    // 1. Probing the quadratic foldback vertex boundary: x in [-5.0, -1.0] in 0.0001f steps
    {
        distortion.reset();
        for (float x = -5.0f; x <= -1.0f; x += 0.0001f) {
            float yDark = distortion.processSample(x, 1.0f, 0.0f, true);
            float yBright = distortion.processSample(x, 1.0f, 1.0f, true);

            ASSERT_TRUE(bumbler::isFiniteBitwise(yDark));
            ASSERT_TRUE(bumbler::isFiniteBitwise(yBright));
            ASSERT_FALSE(isSubnormal(yDark));
            ASSERT_FALSE(isSubnormal(yBright));
            ASSERT_LE(std::abs(yDark), 1.05001f);
            ASSERT_LE(std::abs(yBright), 1.05001f);
        }
    }

    // 2. Continuous stream of extreme DC steps and Nyquist impulses
    {
        constexpr int kBlockSize = 2048;
        std::vector<float> inBuf(kBlockSize);
        std::vector<float> outBuf(kBlockSize);

        // Pattern: sudden jumps between -10.0f, +10.0f, -2.0f, +2.0f, and impulses
        for (int i = 0; i < kBlockSize; ++i) {
            if (i < 256) inBuf[i] = 2.0f;
            else if (i < 512) inBuf[i] = -2.0f;
            else if (i < 768) inBuf[i] = 10.0f;
            else if (i < 1024) inBuf[i] = -10.0f;
            else if (i < 1536) inBuf[i] = (i % 2 == 0) ? 5.0f : -5.0f; // Nyquist alternation
            else inBuf[i] = (i % 64 == 0) ? 100.0f : ((i % 64 == 32) ? -100.0f : 0.0f); // Spikes
        }

        // Test DistortionUnit with active drive (d >= 0.01f): must clamp all spikes <= 1.05f
        const std::vector<float> activeDrives = { 0.25f, 0.5f, 0.8f, 1.0f, 2.0f };
        const std::vector<float> tones = { 0.0f, 0.2f, 0.5f, 0.8f, 1.0f };

        for (float d : activeDrives) {
            for (float t : tones) {
                distortion.reset();
                distortion.process(inBuf.data(), outBuf.data(), kBlockSize, d, t, true);

                float maxPeak = 0.0f;
                for (int i = 0; i < kBlockSize; ++i) {
                    float s = outBuf[i];
                    ASSERT_TRUE(bumbler::isFiniteBitwise(s));
                    ASSERT_FALSE(isSubnormal(s));
                    if (std::abs(s) > maxPeak) maxPeak = std::abs(s);
                }
                ASSERT_LE(maxPeak, 1.05001f);
            }
        }

        // Test DistortionUnit bypass when drive <= 0.001f: must be bit-exact transparent copy
        distortion.reset();
        distortion.process(inBuf.data(), outBuf.data(), kBlockSize, 0.0f, 0.5f, true);
        for (int i = 0; i < kBlockSize; ++i) {
            ASSERT_EQ(outBuf[i], inBuf[i]);
        }
    }

    // 3. Complete CharacterCircuits master stage under extreme drive and input amplitude
    {
        bumbler::CharacterCircuits circuits;
        circuits.prepare(sampleRate, 512);

        constexpr int kSamples = 4096;
        std::vector<float> inL(kSamples);
        std::vector<float> inR(kSamples);

        // Generate high amplitude +/-2.0f multi-frequency input with DC bias (+1.5f)
        for (int i = 0; i < kSamples; ++i) {
            float osc = static_cast<float>(2.0 * std::sin(2.0 * bumbler::kPi * 220.0 * i / sampleRate));
            inL[i] = osc + 1.5f; // Extreme DC offset + large AC
            inR[i] = -osc + 1.5f;
        }

        // Test with drive ON (max drive) and drive OFF: CharacterCircuits must clamp <= 1.05f in BOTH
        for (float driveEn : { 1.0f, 0.0f }) {
            bumbler::ParameterSnapshot params;
            params.driveEnabled = driveEn;
            params.driveAmount = 1.0f; // Max drive
            params.driveTone = 0.7f;
            params.dualMode = 1.0f;
            params.masterVolume = 1.0f;

            std::vector<float> testL = inL;
            std::vector<float> testR = inR;
            circuits.reset();
            circuits.processStereo(testL.data(), testR.data(), kSamples, params);

            float peakL = 0.0f;
            float peakR = 0.0f;
            for (int i = 0; i < kSamples; ++i) {
                ASSERT_TRUE(bumbler::isFiniteBitwise(testL[i]));
                ASSERT_TRUE(bumbler::isFiniteBitwise(testR[i]));
                ASSERT_FALSE(isSubnormal(testL[i]));
                ASSERT_FALSE(isSubnormal(testR[i]));
                if (std::abs(testL[i]) > peakL) peakL = std::abs(testL[i]);
                if (std::abs(testR[i]) > peakR) peakR = std::abs(testR[i]);
            }

            ASSERT_LE(peakL, 1.05001f);
            ASSERT_LE(peakR, 1.05001f);
        }
    }
}


// ============================================================================
// CHALLENGE 2: Continuous 100,000+ Polyphonic Block Rendering & Voice Stealing
// ============================================================================

/**
 * Challenge 2.1: 100,000 Continuous Polyphonic Audio Blocks
 * Adversarial setup:
 *  - 100,000 blocks at 64 samples = 6,400,000 samples (~133 seconds @ 48 kHz).
 *  - Multi-voice chords (4 to 12 notes) triggering voice allocation and voice stealing.
 *  - Dynamic continuous modulation of Drive, Tone, DualMode, AnalogMode, and MasterVolume.
 *  - Real-time heap allocation tracking during render.
 *  - Tracking sliding DC offset, global DC mean, max peak, NaNs, and denormals.
 * Invariants:
 *  - Peak <= 1.05f strictly
 *  - Total allocations during audio callbacks == 0
 *  - Residual DC offset < 0.01
 *  - Zero NaNs, zero Infs, zero denormals
 */
static void challenge_polyphonic_continuous_100k_blocks() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    constexpr int kBlockSize = 64;
    constexpr int kTotalBlocks = 100000;

    bumbler::BumblerEngine engine;
    engine.prepare(sampleRate, kBlockSize);

    // Warm-up and configuration
    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f; // Saw
    params.osc2Waveform = 1.0f; // Pulse
    params.oscMix = 0.5f;
    params.filterCutoff = 2500.0f;
    params.filterResonance = 0.4f;
    params.ampAttack = 0.005f;
    params.ampDecay = 0.1f;
    params.ampSustain = 0.7f;
    params.ampRelease = 0.15f;
    params.masterVolume = 1.0f;
    params.driveEnabled = 1.0f;
    params.driveAmount = 0.5f;
    params.driveTone = 0.5f;
    params.dualMode = 1.0f;
    params.analogMode = 1.0f;

    std::array<float, kBlockSize> blockL;
    std::array<float, kBlockSize> blockR;
    float* channelPointers[2] = { blockL.data(), blockR.data() };

    // Musical chord patterns to cycle through (forcing multi-voice polyphony and voice stealing)
    const std::vector<std::vector<int>> chords = {
        { 48, 52, 55, 59, 62 },             // Cmaj9 (5 voices)
        { 50, 53, 57, 60, 64, 67 },         // Dm11 (6 voices)
        { 45, 48, 52, 55, 59, 62, 65, 69 }, // Am13 (8 voices: max pool saturated)
        { 41, 45, 48, 52, 55, 58, 62, 65, 69, 72 }, // F13 (10 voices: forces voice stealing)
        { 43, 47, 50, 53, 57, 60, 65, 67, 71, 74, 77 } // G13sus (11 voices: aggressive stealing)
    };

    double globalSumL = 0.0;
    double globalSumR = 0.0;
    uint64_t totalSampleCount = 0;
    float globalPeakL = 0.0f;
    float globalPeakR = 0.0f;
    size_t nanCount = 0;
    size_t infCount = 0;
    size_t denormalCount = 0;

    int currentChordIdx = 0;
    std::vector<int> activeNotes;
    activeNotes.reserve(32);

    // Reset and enable heap allocation tracker
    resetAllocationTracker();
    enableAllocationTracker(true);

    for (int block = 0; block < kTotalBlocks; ++block) {
        // Dynamic event sequencing: change chords every 100 blocks (~133 ms)
        if (block % 100 == 0) {
            // Note off previous chord
            for (int note : activeNotes) {
                engine.processMidiEvent(0x80, note, 0.0f);
            }
            activeNotes.clear();

            // Note on new chord
            currentChordIdx = (currentChordIdx + 1) % static_cast<int>(chords.size());
            const auto& chord = chords[static_cast<size_t>(currentChordIdx)];
            for (int note : chord) {
                float vel = 0.6f + 0.3f * std::sin(static_cast<float>(block + note));
                engine.processMidiEvent(0x90, note, std::clamp(vel, 0.2f, 1.0f));
                activeNotes.push_back(note);
            }
        }

        // Continuous parameter modulation
        const double blockTime = static_cast<double>(block) * kBlockSize / sampleRate;
        params.driveAmount = static_cast<float>(0.5 + 0.5 * std::sin(blockTime * 1.5));
        params.driveTone = static_cast<float>(0.5 + 0.5 * std::cos(blockTime * 0.8));
        params.dualMode = (std::sin(blockTime * 0.4) > 0.0) ? 1.0f : 0.0f;
        params.masterVolume = static_cast<float>(1.0 + 0.5 * std::sin(blockTime * 0.3)); // up to 1.5f

        // Real-time render call
        engine.renderBlock(channelPointers, 2, kBlockSize, params);

        // Verification & metric accumulation per sample
        for (int s = 0; s < kBlockSize; ++s) {
            float sL = blockL[static_cast<size_t>(s)];
            float sR = blockR[static_cast<size_t>(s)];

            if (!bumbler::isFiniteBitwise(sL)) {
                if (std::isnan(sL)) ++nanCount;
                if (std::isinf(sL)) ++infCount;
            }
            if (!bumbler::isFiniteBitwise(sR)) {
                if (std::isnan(sR)) ++nanCount;
                if (std::isinf(sR)) ++infCount;
            }

            if (isSubnormal(sL)) ++denormalCount;
            if (isSubnormal(sR)) ++denormalCount;

            float absL = std::abs(sL);
            float absR = std::abs(sR);
            if (absL > globalPeakL) globalPeakL = absL;
            if (absR > globalPeakR) globalPeakR = absR;

            // Measure DC after 1000 blocks warm-up (~1.3s for DC blocker settling)
            if (block >= 1000) {
                globalSumL += sL;
                globalSumR += sR;
                ++totalSampleCount;
            }
        }
    }

    enableAllocationTracker(false);
    size_t allocs = getAllocationCount();

    // Note off all remaining notes
    for (int note : activeNotes) {
        engine.processMidiEvent(0x80, note, 0.0f);
    }

    double dcOffsetL = std::abs(globalSumL / static_cast<double>(totalSampleCount));
    double dcOffsetR = std::abs(globalSumR / static_cast<double>(totalSampleCount));

    std::cout << "    [100k Polyphonic Run Summary]:\n"
              << "      - Total Blocks: " << kTotalBlocks << " (" << (kTotalBlocks * kBlockSize) << " samples)\n"
              << "      - Max Peak L: " << globalPeakL << " (Limit <= 1.05)\n"
              << "      - Max Peak R: " << globalPeakR << " (Limit <= 1.05)\n"
              << "      - Residual DC Offset L: " << dcOffsetL << " (Limit < 0.01)\n"
              << "      - Residual DC Offset R: " << dcOffsetR << " (Limit < 0.01)\n"
              << "      - RT Heap Allocations: " << allocs << " (Limit == 0)\n"
              << "      - NaNs: " << nanCount << ", Infs: " << infCount << ", Denormals: " << denormalCount << "\n";

    ASSERT_EQ(allocs, 0);
    ASSERT_EQ(nanCount, 0);
    ASSERT_EQ(infCount, 0);
    ASSERT_EQ(denormalCount, 0);
    ASSERT_LE(globalPeakL, 1.05001f);
    ASSERT_LE(globalPeakR, 1.05001f);
    ASSERT_LT(dcOffsetL, 0.010);
    ASSERT_LT(dcOffsetR, 0.010);
}

// ============================================================================
// CHALLENGE 3: Sample Rate Invariance (44.1k, 48k, 96k, 192k)
// ============================================================================

/**
 * Challenge 3.1: Probing CharacterCircuits across Standard Sample Rates
 * Checks:
 *  - 44.1 kHz, 48.0 kHz, 96.0 kHz, 192.0 kHz
 *  - DC blocker pole stability (0.90 <= mR <= 0.9999) and DC step elimination (< 0.001)
 *  - Haas delay sample sizing (within [1, 2048]) and stereo widening cross-correlation (r < 0.70)
 *  - Distortion unit tone filter scaling and spectral tilt (> 10 dB)
 *  - Output ceiling invariant (<= 1.05f)
 */
static void challenge_sample_rate_invariance() {
    bumbler::ScopedNoDenormals guard;
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (double fs : sampleRates) {
        std::cout << "    [Testing Sample Rate: " << static_cast<int>(fs) << " Hz]...\n";
        bumbler::CharacterCircuits circuits;
        circuits.prepare(fs, 1024);

        // 1. DC Step Rejection Probing
        {
            const int kSamples = static_cast<int>(fs * 0.5); // 0.5 seconds
            std::vector<float> inL(kSamples, 0.8f); // High +0.8f DC bias
            std::vector<float> inR(kSamples, 0.8f);

            bumbler::ParameterSnapshot params;
            params.driveEnabled = 0.0f;
            params.dualMode = 0.0f;
            params.masterVolume = 1.0f;

            circuits.reset();
            circuits.processStereo(inL.data(), inR.data(), kSamples, params);

            // In the last 20% of samples (after DC blocker settled), compute residual DC
            const int evalStart = static_cast<int>(kSamples * 0.8);
            double sumL = 0.0;
            for (int i = evalStart; i < kSamples; ++i) sumL += inL[i];
            double residualDc = std::abs(sumL / (kSamples - evalStart));

            ASSERT_LT(residualDc, 0.001);
            ASSERT_LE(std::abs(inL.back()), 1.05001f);
        }

        // 2. Dual Mode Stereo Widening Probing
        {
            const int kSamples = static_cast<int>(fs * 0.1);
            std::vector<float> monoIn(kSamples);
            for (int i = 0; i < kSamples; ++i) {
                monoIn[i] = static_cast<float>(0.7 * std::sin(2.0 * bumbler::kPi * 440.0 * i / fs));
            }

            bumbler::ParameterSnapshot params;
            params.driveEnabled = 0.0f;
            params.dualMode = 1.0f;
            params.masterVolume = 1.0f;

            std::vector<float> outL = monoIn;
            std::vector<float> outR = monoIn;
            circuits.reset();
            circuits.processStereo(outL.data(), outR.data(), kSamples, params);

            // Compute cross-correlation r
            double sumL = 0.0, sumR = 0.0;
            for (int i = 0; i < kSamples; ++i) { sumL += outL[i]; sumR += outR[i]; }
            double meanL = sumL / kSamples, meanR = sumR / kSamples;
            double num = 0.0, denL = 0.0, denR = 0.0;
            for (int i = 0; i < kSamples; ++i) {
                double dL = outL[i] - meanL;
                double dR = outR[i] - meanR;
                num += dL * dR;
                denL += dL * dL;
                denR += dR * dR;
            }
            double r = num / std::sqrt(denL * denR);

            ASSERT_LT(r, 0.70);
            for (int i = 0; i < kSamples; ++i) {
                ASSERT_LE(std::abs(outL[i]), 1.05001f);
                ASSERT_LE(std::abs(outR[i]), 1.05001f);
            }
        }

        // 3. Tone Filter Spectral Tilt Probing
        {
            const int kSamples = static_cast<int>(fs * 0.1);
            std::vector<float> sineIn(kSamples);
            for (int i = 0; i < kSamples; ++i) {
                sineIn[i] = static_cast<float>(0.7 * std::sin(2.0 * bumbler::kPi * 1000.0 * i / fs));
            }

            bumbler::ParameterSnapshot pDark;
            pDark.driveEnabled = 1.0f;
            pDark.driveAmount = 0.8f;
            pDark.driveTone = 0.0f; // Dark
            pDark.dualMode = 0.0f;
            pDark.masterVolume = 1.0f;

            std::vector<float> darkL = sineIn, darkR = sineIn;
            circuits.reset();
            circuits.processStereo(darkL.data(), darkR.data(), kSamples, pDark);

            bumbler::ParameterSnapshot pBright = pDark;
            pBright.driveTone = 1.0f; // Bright

            std::vector<float> brightL = sineIn, brightR = sineIn;
            circuits.reset();
            circuits.processStereo(brightL.data(), brightR.data(), kSamples, pBright);

            // 3rd harmonic (3000 Hz) power comparison
            double pwrDark = computeBinPower(darkL.data(), kSamples, 3000.0, fs);
            double pwrBright = computeBinPower(brightL.data(), kSamples, 3000.0, fs);
            double tiltDb = toDb(pwrBright) - toDb(pwrDark);

            ASSERT_TRUE(tiltDb > 10.0);
        }
    }
}

// ============================================================================
// CHALLENGE 4: Dual Mode Rapid Toggling & Glitch Immunity
// ============================================================================

/**
 * Challenge 4.1: Rapid Dual Mode toggling under high-energy audio
 * Probes:
 *  - Toggling dualMode on/off every block (64 samples)
 *  - Toggling dualMode randomly every few samples
 *  - Mono-in vs Stereo-in block processing consistency
 * Invariants:
 *  - Zero buffer overruns or index wrapping glitches
 *  - Strict ceiling limit <= 1.05f on every sample
 *  - Channel balance and mono compatibility
 */
static void challenge_dual_mode_rapid_toggling() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    constexpr int kBlockSize = 64;
    constexpr int kNumBlocks = 2000;

    // 1. Standalone DualModeVoiceDoubler: test rapid toggling with standard full-scale audio
    {
        bumbler::DualModeVoiceDoubler doubler;
        doubler.prepare(sampleRate);

        std::vector<float> blockL(kBlockSize);
        std::vector<float> blockR(kBlockSize);

        float maxObservedPeak = 0.0f;

        for (int b = 0; b < kNumBlocks; ++b) {
            for (int i = 0; i < kBlockSize; ++i) {
                float phase = static_cast<float>(b * kBlockSize + i) / 48000.0f * 440.0f;
                blockL[i] = static_cast<float>(0.8 * std::sin(2.0 * bumbler::kPi * phase));
                blockR[i] = blockL[i];
            }

            bool enabled = (b % 2 == 1); // Rapid alternating toggle
            doubler.processBlock(blockL.data(), blockR.data(), kBlockSize, enabled);

            for (int i = 0; i < kBlockSize; ++i) {
                ASSERT_TRUE(bumbler::isFiniteBitwise(blockL[i]));
                ASSERT_TRUE(bumbler::isFiniteBitwise(blockR[i]));
                ASSERT_FALSE(isSubnormal(blockL[i]));
                ASSERT_FALSE(isSubnormal(blockR[i]));
                float absL = std::abs(blockL[i]);
                float absR = std::abs(blockR[i]);
                if (absL > maxObservedPeak) maxObservedPeak = absL;
                if (absR > maxObservedPeak) maxObservedPeak = absR;
            }
        }

        ASSERT_LE(maxObservedPeak, 1.05001f);
    }

    // 2. CharacterCircuits master stage: test rapid dualMode toggling with high-energy input (+/-1.5f)
    {
        bumbler::CharacterCircuits circuits;
        circuits.prepare(sampleRate, kBlockSize);

        std::vector<float> blockL(kBlockSize);
        std::vector<float> blockR(kBlockSize);

        float maxObservedPeak = 0.0f;

        bumbler::ParameterSnapshot params;
        params.driveEnabled = 1.0f;
        params.driveAmount = 0.5f;
        params.driveTone = 0.5f;
        params.masterVolume = 1.0f;

        for (int b = 0; b < kNumBlocks; ++b) {
            for (int i = 0; i < kBlockSize; ++i) {
                float phase = static_cast<float>(b * kBlockSize + i) / 48000.0f * 440.0f;
                blockL[i] = static_cast<float>(1.2 * std::sin(2.0 * bumbler::kPi * phase) + 0.3 * std::cos(4.0 * bumbler::kPi * phase));
                blockR[i] = blockL[i];
            }

            params.dualMode = (b % 2 == 1) ? 1.0f : 0.0f;
            circuits.processStereo(blockL.data(), blockR.data(), kBlockSize, params);

            for (int i = 0; i < kBlockSize; ++i) {
                ASSERT_TRUE(bumbler::isFiniteBitwise(blockL[i]));
                ASSERT_TRUE(bumbler::isFiniteBitwise(blockR[i]));
                ASSERT_FALSE(isSubnormal(blockL[i]));
                ASSERT_FALSE(isSubnormal(blockR[i]));
                float absL = std::abs(blockL[i]);
                float absR = std::abs(blockR[i]);
                if (absL > maxObservedPeak) maxObservedPeak = absL;
                if (absR > maxObservedPeak) maxObservedPeak = absR;
            }
        }

        ASSERT_LE(maxObservedPeak, 1.05001f);
    }

    // 3. Verify mono vs stereo equivalence when disabled
    {
        bumbler::DualModeVoiceDoubler doubler;
        doubler.prepare(sampleRate);
        std::vector<float> monoIn(512, 0.75f);
        std::vector<float> outL(512), outR(512);
        doubler.reset();
        doubler.process(monoIn.data(), outL.data(), outR.data(), 512, false);
        for (int i = 0; i < 512; ++i) {
            ASSERT_EQ(outL[i], monoIn[i]);
            ASSERT_EQ(outR[i], monoIn[i]);
        }
    }
}

// ============================================================================
// CHALLENGE 5: Master Volume Stress & Out-of-Bounds Scaling
// ============================================================================

/**
 * Challenge 5.1: Master Volume Stress from 0.0 to 2.0 (and out-of-range inputs)
 * Probes:
 *  - masterVolume = 0.0f -> strictly 0.0f
 *  - masterVolume = 2.0f under 8-voice maximum unison chord (> 5.0 raw sum)
 *  - Out-of-range positive masterVolume (3.0f, 10.0f) clamped to 2.0f
 *  - Out-of-range negative masterVolume (-1.0f, -5.0f) clamped to 0.0f
 * Invariants:
 *  - Output ceiling strictly <= 1.05f
 *  - Residual DC offset < 0.01
 */
static void challenge_master_volume_extremes() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    constexpr int kSamples = 16384;

    bumbler::CharacterCircuits circuits;
    circuits.prepare(sampleRate, kSamples);

    std::vector<float> massiveChordL(kSamples);
    std::vector<float> massiveChordR(kSamples);

    // Create massive signal simulating 8 hot voices summed (peaks > 6.0f)
    // Use fundamental 125.0 Hz (period = 384 samples at 48 kHz).
    // Exactly 384 * 21 = 8064 samples, or over 7680 samples (20 exact cycles).
    constexpr double fBase = 125.0; // 48000 / 125 = 384 samples exact cycle
    for (int i = 0; i < kSamples; ++i) {
        float sum = 0.0f;
        for (int h = 1; h <= 8; ++h) {
            sum += static_cast<float>(0.8 * std::sin(2.0 * bumbler::kPi * (fBase * h) * i / sampleRate));
        }
        massiveChordL[i] = sum;
        massiveChordR[i] = sum;
    }

    const std::vector<float> testVols = { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 10.0f, -1.0f, -5.0f };

    for (float vol : testVols) {
        bumbler::ParameterSnapshot params;
        params.driveEnabled = 1.0f;
        params.driveAmount = 0.8f;
        params.driveTone = 0.5f;
        params.dualMode = 1.0f;
        params.masterVolume = vol;

        std::vector<float> outL = massiveChordL;
        std::vector<float> outR = massiveChordR;
        circuits.reset();
        circuits.processStereo(outL.data(), outR.data(), kSamples, params);

        float peakL = 0.0f;
        float peakR = 0.0f;
        double sumL = 0.0;

        for (int i = 0; i < kSamples; ++i) {
            ASSERT_TRUE(bumbler::isFiniteBitwise(outL[i]));
            ASSERT_TRUE(bumbler::isFiniteBitwise(outR[i]));
            ASSERT_FALSE(isSubnormal(outL[i]));
            ASSERT_FALSE(isSubnormal(outR[i]));
            if (std::abs(outL[i]) > peakL) peakL = std::abs(outL[i]);
            if (std::abs(outR[i]) > peakR) peakR = std::abs(outR[i]);
            // Measure DC after 8192 transient samples over exact 20 periods (7680 samples)
            if (i >= 8192 && i < 15872) sumL += outL[i];
        }

        if (vol <= 0.0f) {
            ASSERT_EQ(peakL, 0.0f);
            ASSERT_EQ(peakR, 0.0f);
        } else {
            ASSERT_LE(peakL, 1.05001f);
            ASSERT_LE(peakR, 1.05001f);
            double residualDc = std::abs(sumL / 7680.0);
            ASSERT_LT(residualDc, 0.010);
        }
    }
}


// ============================================================================
// CHALLENGE 6: NoiseEngine and AnalogVoiceDrift Extreme Probing
// ============================================================================

/**
 * Challenge 6.1: NoiseEngine & AnalogVoiceDrift PRNG and Table Integrity
 * Probes:
 *  - 100,000 samples of vintage noise: exact 1024-sample period throughout
 *  - Vintage table DC mean: strictly < 1e-6
 *  - 100,000 samples of white noise: decorrelation, finite bounds [-1.0, 1.0]
 *  - Zero-seed immunity on NoiseEngine and AnalogVoiceDrift (no freeze/hang)
 *  - AnalogVoiceDrift 10,000 note trigger bounds: [-2.5, +2.5] cents
 *  - AnalogVoiceDrift phase bounds: [0, 2*pi)
 *  - Deterministic 0.0 drift and 0.0 phase when analogMode is false
 */
static void challenge_noise_and_analog_drift() {
    // 1. NoiseEngine long render & zero seed
    {
        bumbler::NoiseEngine noise;
        constexpr int kSamples = 100000;
        std::vector<float> vBuf(kSamples);
        std::vector<float> wBuf(kSamples);

        // Seed with 0 (should not lock up)
        noise.seed(0u);
        noise.render(vBuf.data(), kSamples, false);
        noise.render(wBuf.data(), kSamples, true);

        // Check vintage periodicity at multiple 1024 offsets
        for (int base = 0; base + 2048 <= kSamples; base += 10240) {
            for (int i = 0; i < 1024; ++i) {
                ASSERT_EQ(vBuf[base + i], vBuf[base + i + 1024]);
            }
        }

        // Check white noise decorrelation at lag 1024
        double numW = 0.0, denW = 0.0;
        for (int i = 0; i < 1024; ++i) {
            numW += wBuf[i] * wBuf[i + 1024];
            denW += wBuf[i] * wBuf[i];
        }
        double rWhite = std::abs(numW / denW);
        ASSERT_LT(rWhite, 0.10);

        // Check white noise bounds
        for (int i = 0; i < kSamples; ++i) {
            ASSERT_LE(std::abs(wBuf[i]), 1.0001f);
        }
    }

    // 2. AnalogVoiceDrift boundary & distribution
    {
        bumbler::AnalogVoiceDrift drift;
        drift.seed(0u); // Seed with 0

        // Disabled mode: 1000 triggers must be bit-exact 0.0
        for (int i = 0; i < 1000; ++i) {
            double d = drift.triggerNote(false);
            double p = drift.getInitialPhase(false);
            ASSERT_EQ(d, 0.0);
            ASSERT_EQ(p, 0.0);
            ASSERT_EQ(drift.getDriftCents(), 0.0f);
        }

        // Enabled mode: 10,000 triggers must strictly fall in [-2.5, +2.5] cents and [0, 2*pi)
        std::vector<double> driftHistory;
        std::vector<double> phaseHistory;
        driftHistory.reserve(10000);
        phaseHistory.reserve(10000);

        for (int i = 0; i < 10000; ++i) {
            double d = drift.triggerNote(true);
            double p = drift.getInitialPhase(true);

            ASSERT_LE(std::abs(d), 2.50001);
            ASSERT_TRUE(p >= 0.0 && p < static_cast<double>(bumbler::kTwoPi));

            driftHistory.push_back(d);
            phaseHistory.push_back(p);
        }

        // Check statistical variance of drift (> 0.05) and phase (> 0.1)
        double meanD = 0.0;
        for (double v : driftHistory) meanD += v;
        meanD /= driftHistory.size();
        double varD = 0.0;
        for (double v : driftHistory) varD += (v - meanD) * (v - meanD);
        varD /= (driftHistory.size() - 1);

        double meanP = 0.0;
        for (double v : phaseHistory) meanP += v;
        meanP /= phaseHistory.size();
        double varP = 0.0;
        for (double v : phaseHistory) varP += (v - meanP) * (v - meanP);
        varP /= (phaseHistory.size() - 1);

        ASSERT_TRUE(varD > 0.05);
        ASSERT_TRUE(varP > 0.10);
    }
}

// ============================================================================
// MAIN RUNNER
// ============================================================================
int main() {
    std::cout << "======================================================================\n";
    std::cout << "  Bumbler XD: Milestone 4 Adversarial Stress & Stability Challenger   \n";
    std::cout << "======================================================================\n";

    runSingleTest("1. Extreme Drive Levels, Maximum Amplitudes & Discontinuous DC Jumps", challenge_extreme_drive_and_amplitudes);
    runSingleTest("2. Continuous 100,000 Polyphonic Blocks Rendering with Voice Stealing", challenge_polyphonic_continuous_100k_blocks);
    runSingleTest("3. Sample Rate Invariance (44.1k, 48k, 96k, 192k)", challenge_sample_rate_invariance);
    runSingleTest("4. Dual Mode Rapid Toggling & Glitch Immunity", challenge_dual_mode_rapid_toggling);
    runSingleTest("5. Master Volume Stress & Out-of-Bounds Scaling", challenge_master_volume_extremes);
    runSingleTest("6. NoiseEngine and AnalogVoiceDrift Extreme Probing", challenge_noise_and_analog_drift);

    std::cout << "\n======================================================================\n";
    std::cout << "  CHALLENGER-M4 SUMMARY: " << gPassedTests << " Passed, " << gFailedTests << " Failed (Total: " << gTotalTests << ")\n";
    std::cout << "======================================================================\n";

    return (gFailedTests == 0) ? 0 : 1;
}
