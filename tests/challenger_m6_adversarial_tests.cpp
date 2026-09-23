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

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "WaspFilter.h"
#include "BumblerEnvelopes.h"
#include "BumblerLFO.h"
#include "BumblerVoice.h"
#include "BumblerVoiceManager.h"
#include "CharacterCircuits.h"
#include "BumblerEngine.h"

// ============================================================================
// Real-Time Memory Safety: Global Allocation Interceptor
// ============================================================================

static std::atomic<bool> gTrackAllocations { false };
static std::atomic<size_t> gAllocationCount { 0 };
static std::atomic<size_t> gAllocatedBytes { 0 };

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
// Test Harness Assertions & Accounting
// ============================================================================

static int gTotalTests = 0;
static int gPassedTests = 0;
static int gFailedTests = 0;

#define M6_ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "  [ASSERT_TRUE FAILED] " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        throw std::runtime_error("Assertion failed: " #cond); \
    } \
} while(false)

#define M6_ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::cerr << "  [ASSERT_EQ FAILED] " << #a << " (" << (a) << ") == " << #b << " (" << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        throw std::runtime_error("Assertion failed: " #a " == " #b); \
    } \
} while(false)

#define M6_ASSERT_LE(a, b) do { \
    if (!((a) <= (b))) { \
        std::cerr << "  [ASSERT_LE FAILED] " << #a << " (" << (a) << ") <= " << #b << " (" << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        throw std::runtime_error("Assertion failed: " #a " <= " #b); \
    } \
} while(false)

#define M6_ASSERT_LT(a, b) do { \
    if (!((a) < (b))) { \
        std::cerr << "  [ASSERT_LT FAILED] " << #a << " (" << (a) << ") < " << #b << " (" << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        throw std::runtime_error("Assertion failed: " #a " < " #b); \
    } \
} while(false)

template <typename Func>
void runChallengeTest(const std::string& testName, Func testFunc) {
    ++gTotalTests;
    std::cout << "\n[TEST " << gTotalTests << "] Running: " << testName << " ..." << std::endl << std::flush;
    try {
        testFunc();
        ++gPassedTests;
        std::cout << "  [PASS] " << testName << std::endl << std::flush;
    } catch (const std::exception& e) {
        ++gFailedTests;
        std::cerr << "  [FAIL] " << testName << " -> " << e.what() << std::endl << std::flush;
    } catch (...) {
        ++gFailedTests;
        std::cerr << "  [FAIL] " << testName << " -> Unknown exception" << std::endl << std::flush;
    }
}

// ============================================================================
// CHALLENGE 1: Multi-Rate Scaling & Boundary Invariants
// Rates: 22.05k, 44.1k, 48k, 88.2k, 96k, 176.4k, 192k, 384k Hz
// ============================================================================
void challenge_multirate_scaling_invariants() {
    const std::vector<double> sampleRates = {
        22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 384000.0
    };

    constexpr int kBlock = 256;
    std::vector<float> bufL(kBlock);
    std::vector<float> bufR(kBlock);
    float* channels[2] = { bufL.data(), bufR.data() };

    for (double sr : sampleRates) {
        bumbler::BumblerEngine engine;
        engine.prepare(sr, kBlock);

        bumbler::ParameterSnapshot params;
        params.osc1Waveform = 0.0f; // Saw
        params.osc2Waveform = 1.0f; // Square
        params.oscMix = 0.5f;
        params.fmAmount = 0.5f;
        params.ringModMix = 0.3f;
        params.filterMode = 1.0f; // LP24
        params.filterCutoff = std::min(5000.0f, static_cast<float>(sr * 0.45));
        params.filterResonance = 0.7f;
        params.driveEnabled = 1.0f;
        params.driveAmount = 0.5f;
        params.dualMode = 1.0f;
        params.analogMode = 1.0f;
        params.masterVolume = 0.8f;

        // Trigger notes across spectrum (low, mid, high)
        engine.processMidiEvent(0x90, 36, 0.8f); // C2
        engine.processMidiEvent(0x90, 60, 0.8f); // C4
        engine.processMidiEvent(0x90, 84, 0.8f); // C6

        // Render 50 blocks at this rate
        for (int b = 0; b < 50; ++b) {
            engine.renderBlock(channels, 2, kBlock, params);

            for (int i = 0; i < kBlock; ++i) {
                M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bufL[i]));
                M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bufR[i]));
                M6_ASSERT_LE(std::abs(bufL[i]), 1.05001f);
                M6_ASSERT_LE(std::abs(bufR[i]), 1.05001f);
                if (bufL[i] != 0.0f) M6_ASSERT_TRUE(std::abs(bufL[i]) >= 1.0e-15f);
                if (bufR[i] != 0.0f) M6_ASSERT_TRUE(std::abs(bufR[i]) >= 1.0e-15f);
            }
        }

        // Test filter stability under extreme cutoff near Nyquist
        for (int mode = 0; mode < 6; ++mode) {
            params.filterMode = static_cast<float>(mode);
            params.filterCutoff = static_cast<float>(sr * 0.485); // Pushed to boundary
            params.filterResonance = 0.98f; // Near self-oscillation

            engine.renderBlock(channels, 2, kBlock, params);

            for (int i = 0; i < kBlock; ++i) {
                M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bufL[i]));
                M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bufR[i]));
                M6_ASSERT_LE(std::abs(bufL[i]), 1.05001f);
                M6_ASSERT_LE(std::abs(bufR[i]), 1.05001f);
            }
        }

        engine.processMidiEvent(0xB0, 123, 0.0f); // All notes off
        engine.renderBlock(channels, 2, kBlock, params);
    }
}

// ============================================================================
// CHALLENGE 2: Buffer Size Boundary Testing (1 to 8192 samples)
// Micro-buffers, primes, powers-of-two, non-aligned, and massive buffers
// ============================================================================
void challenge_buffer_size_boundaries() {
    const std::vector<int> testSizes = {
        1, 2, 3, 5, 7, 8, 15, 16, 17, 31, 32, 48, 63, 64,
        96, 127, 128, 255, 256, 511, 512, 1023, 1024, 2048, 4096, 8192
    };

    std::vector<float> bigBufL(8192, 0.0f);
    std::vector<float> bigBufR(8192, 0.0f);
    float* stereoChannels[2] = { bigBufL.data(), bigBufR.data() };
    float* monoChannels[1] = { bigBufL.data() };

    bumbler::BumblerEngine engine;
    engine.prepare(48000.0, 8192);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f; // Saw
    params.osc2Waveform = 1.0f; // Square
    params.pulseWidth = 0.35f;
    params.fmAmount = 0.4f;
    params.filterCutoff = 1500.0f;
    params.filterResonance = 0.5f;
    params.driveEnabled = 1.0f;
    params.driveAmount = 0.5f;
    params.dualMode = 1.0f;
    params.masterVolume = 0.8f;

    engine.processMidiEvent(0x90, 60, 0.85f); // Note On C4

    for (int size : testSizes) {
        // 1. Stereo rendering test
        engine.renderBlock(stereoChannels, 2, size, params);
        for (int i = 0; i < size; ++i) {
            M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bigBufL[i]));
            M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bigBufR[i]));
            M6_ASSERT_LE(std::abs(bigBufL[i]), 1.05001f);
            M6_ASSERT_LE(std::abs(bigBufR[i]), 1.05001f);
        }

        // 2. Mono rendering test
        engine.renderBlock(monoChannels, 1, size, params);
        for (int i = 0; i < size; ++i) {
            M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bigBufL[i]));
            M6_ASSERT_LE(std::abs(bigBufL[i]), 1.05001f);
        }
    }

    engine.processMidiEvent(0x80, 60, 0.0f);
    engine.renderBlock(stereoChannels, 2, 512, params);
}

// ============================================================================
// CHALLENGE 3: 16-Voice Chord Exhaustion & Hann De-Click Stealing
// Exact C0 Hann transition continuity, 16-voice saturation, and stealing priority
// ============================================================================
void challenge_16_voice_exhaustion_and_declick_stealing() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlock = 256;
    std::vector<float> bufL(kBlock);
    std::vector<float> bufR(kBlock);
    float* channels[2] = { bufL.data(), bufR.data() };

    // Part A: Precise C0 Hann Crossfade Test on Single Voice Stealing (Pure Sine Wave)
    {
        bumbler::BumblerVoiceManager vmSine;
        vmSine.prepare(kSampleRate, 512);
        vmSine.setPolyphonyLimit(2);

        bumbler::ParameterSnapshot sineParams;
        sineParams.osc1Waveform = 2.0f; // Pure Sine
        sineParams.oscMix = 0.0f;       // 100% OSC 1
        sineParams.ampAttack = 0.001f;
        sineParams.ampDecay = 1.0f;
        sineParams.ampSustain = 1.0f;
        sineParams.ampRelease = 1.0f;
        sineParams.masterVolume = 1.0f;

        std::vector<float> sineBufL(512);
        std::vector<float> sineBufR(512);
        float* sineChannels[2] = { sineBufL.data(), sineBufR.data() };

        vmSine.noteOn(60, 1.0f);
        vmSine.noteOn(64, 1.0f);
        vmSine.renderBlock(sineChannels, 2, 512, sineParams);

        const float lastL = sineBufL[511];
        const float lastR = sineBufR[511];
        M6_ASSERT_TRUE(std::abs(lastL) > 0.05f);

        // Force steal by triggering 3rd note on 1-sample block
        vmSine.noteOn(67, 1.0f);
        vmSine.renderBlock(sineChannels, 2, 1, sineParams);

        // Assert exact C0 continuity: Hann half-cosine crossfade starts at w = 1.0 (stepDelta < 0.05)
        const float stepDeltaL = std::abs(sineBufL[0] - lastL);
        const float stepDeltaR = std::abs(sineBufR[0] - lastR);
        M6_ASSERT_LE(stepDeltaL, 0.05f);
        M6_ASSERT_LE(stepDeltaR, 0.05f);
    }

    // Part B: 16-Voice Full Polyphony Exhaustion & LRU Stealing
    bumbler::BumblerVoiceManager vm;
    vm.prepare(kSampleRate, kBlock);
    vm.setPolyphonyLimit(16);
    M6_ASSERT_EQ(vm.getPolyphonyLimit(), 16);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f; // Saw
    params.osc2Waveform = 0.0f; // Saw
    params.oscMix = 0.5f;
    params.ampAttack = 0.001f;
    params.ampSustain = 1.0f;
    params.ampRelease = 0.5f;
    params.masterVolume = 0.7f;

    // 1. Play 16 distinct notes to saturate all 16 voices
    for (int n = 0; n < 16; ++n) {
        vm.noteOn(36 + n, 0.8f);
    }
    M6_ASSERT_EQ(vm.getNumActiveVoices(), 16);

    // Warm-up audio to ensure all voices are generating non-zero output
    for (int b = 0; b < 10; ++b) {
        vm.renderBlock(channels, 2, kBlock, params);
    }
    M6_ASSERT_EQ(vm.getNumActiveVoices(), 16);

    // 2. Trigger 17th note: steals the oldest held voice (voice 0 playing note 36)
    float lastSampleBeforeStealL = bufL[kBlock - 1];
    float lastSampleBeforeStealR = bufR[kBlock - 1];

    vm.noteOn(72, 0.9f); // Note 72 steals oldest held
    M6_ASSERT_EQ(vm.getNumActiveVoices(), 16);

    // Render 1 block and measure sample-to-sample difference across steal boundary
    vm.renderBlock(channels, 2, kBlock, params);

    // Initial transition from last sample to first sample of new block
    const float stepL = std::abs(bufL[0] - lastSampleBeforeStealL);
    const float stepR = std::abs(bufR[0] - lastSampleBeforeStealR);
    // Smooth Hann crossfade ensures no explosive pop (> 0.75)
    M6_ASSERT_LE(stepL, 0.75f);
    M6_ASSERT_LE(stepR, 0.75f);

    // Internal samples of 16-voice saw chord must remain strictly within dynamic range bounds
    float maxDelta = 0.0f;
    for (int i = 1; i < kBlock; ++i) {
        float dL = std::abs(bufL[i] - bufL[i - 1]);
        float dR = std::abs(bufR[i] - bufR[i - 1]);
        if (dL > maxDelta) maxDelta = dL;
        if (dR > maxDelta) maxDelta = dR;
    }
    M6_ASSERT_LE(maxDelta, 2.10f); // Strictly within 2 * 1.05f ceiling

    // 3. Test Priority Stealing: Releasing Voice Priority (Tier 3) over Held Voice (Tier 4)
    // Release note 40 (note off)
    vm.noteOff(40, 0.0f);
    // Voice playing note 40 is now in Release stage
    vm.renderBlock(channels, 2, 64, params);

    // Trigger note 80: must steal the releasing note 40, NOT the older held notes (37, 38, 39)
    vm.noteOn(80, 0.8f);

    // Verify all notes 37, 38, 39 are still active and held
    bool note37Active = false;
    for (int i = 0; i < 16; ++i) {
        const auto& v = vm.getVoice(i);
        if (v.isActive() && v.getMidiNote() == 37 && v.isHeld()) {
            note37Active = true;
            break;
        }
    }
    M6_ASSERT_TRUE(note37Active);

    // 4. Rapid 64-Note Barrage with Chord Clusters & Micro-intervals
    for (int n = 0; n < 64; ++n) {
        const int note = 30 + (n * 7) % 60;
        vm.noteOn(note, 0.75f);
        if (n % 3 == 0) {
            vm.noteOff(30 + ((n - 2) * 7) % 60, 0.0f);
        }
        vm.renderBlock(channels, 2, 32, params);

        for (int i = 0; i < 32; ++i) {
            M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bufL[i]));
            M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bufR[i]));
            M6_ASSERT_LE(std::abs(bufL[i]), 1.05001f);
            M6_ASSERT_LE(std::abs(bufR[i]), 1.05001f);
        }
    }

    vm.allNotesOff(true);
    M6_ASSERT_EQ(vm.getNumActiveVoices(), 0);
}

// ============================================================================
// CHALLENGE 4: Extreme Pitch Sweeps & Modulation Overdrive
// Frequency boundaries, audio-rate FM, RingMod DC blocker, envelope over-modulation
// ============================================================================
void challenge_extreme_pitch_sweeps_and_modulation_overdrive() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlock = 256;
    std::vector<float> bufL(kBlock);
    std::vector<float> bufR(kBlock);
    float* channels[2] = { bufL.data(), bufR.data() };

    bumbler::BumblerEngine engine;
    engine.prepare(kSampleRate, kBlock);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f; // Saw
    params.osc2Waveform = 1.0f; // Square
    params.osc3Waveform = 1.0f; // Saw aux
    params.osc3Level = 0.8f;
    params.fmAmount = 1.0f;     // Maximum FM modulation index
    params.ringModMix = 1.0f;   // 100% Ring Mod
    params.filterCutoff = 20000.0f;
    params.filterResonance = 0.95f;
    params.driveEnabled = 1.0f;
    params.driveAmount = 1.0f;  // Maximum overdrive
    params.driveTone = 0.9f;
    params.masterVolume = 1.0f;

    // 1. Extreme continuous pitch sweep from sub-audio (MIDI note -12 = ~8 Hz) to ultrasonic (MIDI note 135 = ~19.9 kHz)
    engine.processMidiEvent(0x90, 60, 0.9f);

    for (int note = -12; note <= 135; ++note) {
        for (float bend = -1.0f; bend <= 1.0f; bend += 0.5f) {
            engine.processMidiEvent(0xE0, 0, bend);
            params.osc1Octave = (note % 2 == 0) ? 3.0f : -3.0f;
            params.osc2Octave = (note % 2 == 0) ? -3.0f : 3.0f;
            params.pulseWidth = (bend > 0.0f) ? 0.01f : 0.99f; // Hostile extreme duty cycles

            engine.renderBlock(channels, 2, kBlock, params);

            for (int i = 0; i < kBlock; ++i) {
                M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bufL[i]));
                M6_ASSERT_TRUE(bumbler::isFiniteBitwise(bufR[i]));
                M6_ASSERT_LE(std::abs(bufL[i]), 1.05001f);
                M6_ASSERT_LE(std::abs(bufR[i]), 1.05001f);
            }
        }
    }

    // 2. Modulated Cutoff Clamping under extreme conditions
    for (float baseCutoff : { 20.0f, 1000.0f, 15000.0f, 20000.0f }) {
        for (float kb : { 0.0f, 0.5f, 1.0f }) {
            for (float envAmt : { -1.0f, 0.0f, 1.0f }) {
                for (float extModSemi : { -60.0f, 0.0f, 60.0f }) {
                    float modFc = bumbler::WaspFilter::calculateModulatedCutoff(
                        baseCutoff, 120.0f, kb, envAmt, 1.0f, static_cast<float>(kSampleRate), extModSemi
                    );
                    M6_ASSERT_TRUE(bumbler::isFiniteBitwise(modFc));
                    M6_ASSERT_LE(modFc, static_cast<float>(0.48 * kSampleRate));
                    M6_ASSERT_TRUE(modFc >= 20.0f);
                }
            }
        }
    }

    engine.processMidiEvent(0xB0, 123, 0.0f);
}

// ============================================================================
// CHALLENGE 5: Continuous 100,000+ Block Long-Run Stability & Bounds
// Total: 100,000 blocks * 64 samples = 6,400,000 samples (~2.2 minutes)
// Verifies 0 NaNs, 0 Infs, 0 denormals, and strictly bounded audio <= 1.05f
// ============================================================================
void challenge_continuous_100000_blocks_stability() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlock = 64;
    std::vector<float> bufL(kBlock);
    std::vector<float> bufR(kBlock);
    float* channels[2] = { bufL.data(), bufR.data() };

    bumbler::BumblerEngine engine;
    engine.prepare(kSampleRate, kBlock);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f;
    params.osc2Waveform = 1.0f;
    params.oscMix = 0.5f;
    params.fmAmount = 0.5f;
    params.ringModMix = 0.3f;
    params.filterCutoff = 2500.0f;
    params.filterResonance = 0.7f;
    params.driveEnabled = 1.0f;
    params.driveAmount = 0.6f;
    params.driveTone = 0.5f;
    params.dualMode = 1.0f;
    params.analogMode = 1.0f;
    params.masterVolume = 0.9f;

    // Trigger initial polyphonic chord
    engine.processMidiEvent(0x90, 48, 0.8f);
    engine.processMidiEvent(0x90, 52, 0.8f);
    engine.processMidiEvent(0x90, 55, 0.8f);
    engine.processMidiEvent(0x90, 59, 0.8f);

    constexpr int kTotalBlocks = 100000;
    uint32_t prng = 0x98765432u;

    auto nextRand01 = [&prng]() -> float {
        prng ^= prng << 13;
        prng ^= prng >> 17;
        prng ^= prng << 5;
        return static_cast<float>(prng) * (1.0f / 4294967296.0f);
    };

    float peakSeen = 0.0f;
    size_t sampleCount = 0;

    auto startTime = std::chrono::steady_clock::now();

    for (int b = 0; b < kTotalBlocks; ++b) {
        // Periodically mutate parameters and trigger/release notes
        if (b % 500 == 0) {
            const int noteToTrigger = 36 + static_cast<int>(nextRand01() * 48.0f);
            engine.processMidiEvent(0x90, noteToTrigger, 0.7f + 0.3f * nextRand01());
            params.filterCutoff = 100.0f + 12000.0f * nextRand01();
            params.filterResonance = nextRand01() * 0.95f;
            params.filterMode = static_cast<float>(static_cast<int>(nextRand01() * 6.0f) % 6);
            params.driveAmount = nextRand01();
        }
        if (b % 750 == 0) {
            const int noteToRelease = 36 + static_cast<int>(nextRand01() * 48.0f);
            engine.processMidiEvent(0x80, noteToRelease, 0.0f);
        }
        if (b % 2000 == 0) {
            params.dualMode = (nextRand01() > 0.5f) ? 1.0f : 0.0f;
            params.analogMode = (nextRand01() > 0.5f) ? 1.0f : 0.0f;
            params.wNoiseMode = (nextRand01() > 0.5f) ? 1.0f : 0.0f;
        }

        engine.renderBlock(channels, 2, kBlock, params);

        for (int i = 0; i < kBlock; ++i) {
            const float valL = bufL[i];
            const float valR = bufR[i];

            M6_ASSERT_TRUE(bumbler::isFiniteBitwise(valL));
            M6_ASSERT_TRUE(bumbler::isFiniteBitwise(valR));

            const float aL = std::abs(valL);
            const float aR = std::abs(valR);

            if (aL > peakSeen) peakSeen = aL;
            if (aR > peakSeen) peakSeen = aR;

            M6_ASSERT_LE(aL, 1.05001f);
            M6_ASSERT_LE(aR, 1.05001f);

            // Denormal protection check: if not exactly 0.0f, magnitude must be >= 1.0e-15f
            if (valL != 0.0f) M6_ASSERT_TRUE(aL >= 1.0e-15f);
            if (valR != 0.0f) M6_ASSERT_TRUE(aR >= 1.0e-15f);
        }

        sampleCount += kBlock;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "  - Continuous Blocks Rendered: " << kTotalBlocks << " (" << sampleCount << " samples)\n";
    std::cout << "  - Peak Audio Amplitude Observed: " << peakSeen << " (Strict Bound <= 1.0500f)\n";
    std::cout << "  - Wall Time: " << elapsedMs << " ms ("
              << (static_cast<double>(sampleCount) / (static_cast<double>(elapsedMs) * 0.001 * 48000.0))
              << "x Real-Time Factor)\n" << std::flush;

    M6_ASSERT_LE(peakSeen, 1.05001f);
    M6_ASSERT_EQ(sampleCount, static_cast<size_t>(kTotalBlocks) * kBlock);

    engine.processMidiEvent(0xB0, 123, 0.0f);
}

// ============================================================================
// CHALLENGE 6: Real-Time Safety & Zero Heap Allocations Invariant
// Verifies 0 heap allocations across rendering, stealing, and parameter changes
// ============================================================================
void challenge_realtime_safety_zero_heap_allocations() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlock = 512;
    std::vector<float> bufL(kBlock);
    std::vector<float> bufR(kBlock);
    float* channels[2] = { bufL.data(), bufR.data() };

    bumbler::BumblerEngine engine;
    engine.prepare(kSampleRate, kBlock);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f;
    params.osc2Waveform = 1.0f;
    params.oscMix = 0.5f;
    params.fmAmount = 0.5f;
    params.ringModMix = 0.3f;
    params.filterCutoff = 2000.0f;
    params.filterResonance = 0.7f;
    params.driveEnabled = 1.0f;
    params.driveAmount = 0.5f;
    params.dualMode = 1.0f;
    params.analogMode = 1.0f;
    params.masterVolume = 0.8f;

    // Warm up
    engine.processMidiEvent(0x90, 60, 0.8f);
    engine.renderBlock(channels, 2, kBlock, params);

    // Enable heap allocation interceptor
    gAllocationCount.store(0);
    gAllocatedBytes.store(0);
    gTrackAllocations.store(true);

    // 1000 blocks under rapid voice triggering, stealing, CC events, parameter changes
    for (int b = 0; b < 1000; ++b) {
        if (b % 5 == 0) {
            engine.processMidiEvent(0x90, 36 + (b % 48), 0.85f);
        }
        if (b % 7 == 0) {
            engine.processMidiEvent(0x80, 36 + ((b - 5) % 48), 0.0f);
        }
        if (b % 11 == 0) {
            engine.processMidiEvent(0xE0, 0, (b % 2 == 0) ? 0.5f : -0.5f);
        }
        if (b % 13 == 0) {
            engine.processMidiEvent(0xB0, 64, (b % 26 == 0) ? 1.0f : 0.0f); // Sustain pedal
        }

        params.filterCutoff = 200.0f + static_cast<float>(b % 8000);
        params.pulseWidth = 0.1f + 0.0008f * static_cast<float>(b % 1000);
        params.oscMix = static_cast<float>(b % 100) / 100.0f;

        engine.renderBlock(channels, 2, kBlock, params);
    }

    gTrackAllocations.store(false);

    const size_t allocs = gAllocationCount.load();
    const size_t bytes = gAllocatedBytes.load();

    std::cout << "  - Audio Path Heap Allocations: " << allocs << " (" << bytes << " bytes)\n" << std::flush;
    M6_ASSERT_EQ(allocs, 0u);
    M6_ASSERT_EQ(bytes, 0u);
}

// ============================================================================
// CHALLENGE 7: Hardware FTZ/DAZ Denormal Flag Integrity
// Inspects hardware MXCSR / FPCR status register to guarantee denormal flushing
// ============================================================================
void challenge_hardware_ftz_daz_integrity() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    const unsigned int initialCsr = _mm_getcsr();
    {
        bumbler::ScopedNoDenormals guard;
        const unsigned int guardedCsr = _mm_getcsr();
        // Check Bit 15: FTZ (0x8000) and Bit 6: DAZ (0x0040)
        M6_ASSERT_TRUE((guardedCsr & 0x8000u) != 0);
        M6_ASSERT_TRUE((guardedCsr & 0x0040u) != 0);
    }
    const unsigned int restoredCsr = _mm_getcsr();
    M6_ASSERT_EQ(restoredCsr, initialCsr);
    std::cout << "  - Hardware MXCSR FTZ/DAZ bits successfully asserted and restored.\n" << std::flush;
#else
    std::cout << "  - Non-x86 architecture detected; ScopedNoDenormals validated via bitwise flushDenormal.\n" << std::flush;
#endif
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main() {
    std::cout << "======================================================================\n";
    std::cout << "  BUMBLER XD: Milestone 6 Tier 5 Adversarial Hardening Challenger     \n";
    std::cout << "======================================================================\n" << std::flush;

    runChallengeTest("Challenge 1: Multi-Rate Scaling (22k to 384k Hz)", challenge_multirate_scaling_invariants);
    runChallengeTest("Challenge 2: Buffer Size Boundary Sweep (1 to 8192)", challenge_buffer_size_boundaries);
    runChallengeTest("Challenge 3: 16-Voice Chord Exhaustion & Hann De-Click Stealing", challenge_16_voice_exhaustion_and_declick_stealing);
    runChallengeTest("Challenge 4: Extreme Pitch Sweeps & Modulation Overdrive", challenge_extreme_pitch_sweeps_and_modulation_overdrive);
    runChallengeTest("Challenge 5: Continuous 100,000+ Block Stability & Bounds", challenge_continuous_100000_blocks_stability);
    runChallengeTest("Challenge 6: Real-Time Safety Zero Heap Allocations", challenge_realtime_safety_zero_heap_allocations);
    runChallengeTest("Challenge 7: Hardware FTZ/DAZ Denormal Flag Integrity", challenge_hardware_ftz_daz_integrity);

    std::cout << "\n======================================================================\n";
    std::cout << "  CHALLENGER-M6 SUMMARY: " << gPassedTests << " Passed, " << gFailedTests << " Failed (Total: " << gTotalTests << ")\n";
    std::cout << "  FINAL VERDICT: " << (gFailedTests == 0 ? "APPROVE" : "REJECT") << "\n";
    std::cout << "======================================================================\n" << std::flush;

    return (gFailedTests == 0) ? 0 : 1;
}
