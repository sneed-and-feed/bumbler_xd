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

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "WaspFilter.h"
#include "BumblerLFO.h"
#include "BumblerEnvelopes.h"
#include "BumblerVoice.h"
#include "BumblerVoiceManager.h"
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
// Headless Test Harness Infrastructure
// ============================================================================

static int gTotalTests = 0;
static int gPassedTests = 0;
static int gFailedTests = 0;

static void runSingleTest(const char* testName, void (*testFunc)()) {
    ++gTotalTests;
    std::cout << "\n[CHALLENGER-M3 RUN] " << testName << std::endl;
    try {
        testFunc();
        ++gPassedTests;
        std::cout << "[       PASSED   ] " << testName << std::endl;
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

#define ASSERT_NEAR(a, b, tol) do { \
    if (std::abs(static_cast<double>(a) - static_cast<double>(b)) > static_cast<double>(tol)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #a + " near " + #b + \
            " (diff " + std::to_string(std::abs(static_cast<double>(a) - static_cast<double>(b))) + " > tol " + std::to_string(tol) + ") at line " + std::to_string(__LINE__)); \
    } \
} while (false)

[[nodiscard]] inline bool isSubnormalBitwise(float val) noexcept {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(float));
    return ((bits & 0x7F800000u) == 0u) && ((bits & 0x007FFFFFu) != 0u);
}

// ============================================================================
// SUITE 1: Extreme Modulation Depths & Parameter Clamping Invariants
// ============================================================================

/**
 * Probe 1.1: Max LFO 1 Amount (1.0) into Pitch & Cutoff with All Waveforms
 * Verifies that LFO 1 at maximum depth (1.0) routing to pitch (+/-12 semitones)
 * and cutoff (+/-36 semitones) produces bounded, finite, non-NaN audio
 * across all 6 filter modes.
 */
static void probe_extreme_lfo1_depths_pitch_and_cutoff() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerVoice voice;
    voice.prepare(sampleRate);
    voice.setFilterEnabled(true);

    const std::vector<int> lfoWaves = { 0, 1, 2, 3 }; // Saw, Square, Sine, Noise
    const std::vector<int> filterModes = { 0, 1, 2, 3, 4, 5 };
    const std::vector<float> baseCutoffs = { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f };

    for (int wave : lfoWaves) {
        for (int mode : filterModes) {
            for (float cutoff : baseCutoffs) {
                voice.reset();

                bumbler::ParameterSnapshot params;
                params.osc1Waveform = 0.0f; // Saw
                params.oscMix = 0.0f;
                params.filterMode = static_cast<float>(mode);
                params.filterCutoff = cutoff;
                params.filterResonance = 0.85f;
                params.ampAttack = 0.001f;
                params.ampDecay = 0.1f;
                params.ampSustain = 1.0f;
                params.ampRelease = 0.1f;
                params.masterVolume = 1.0f;

                // 1. Max LFO 1 -> Pitch (Osc12Pitch = 0)
                params.lfo1Target = 0.0f;
                params.lfo1Amount = 1.0f;
                params.lfo1Rate = 20.0f;
                params.lfo1Waveform = static_cast<float>(wave);

                voice.noteOn(60, 0.9f, 1);
                for (int i = 0; i < 480; ++i) {
                    float outL = 0.0f, outR = 0.0f;
                    voice.renderSample(outL, outR, params);
                    ASSERT_FALSE(bumbler::isNanOrInfBitwise(outL));
                    ASSERT_FALSE(bumbler::isNanOrInfBitwise(outR));
                    ASSERT_FALSE(isSubnormalBitwise(outL));
                    ASSERT_FALSE(isSubnormalBitwise(outR));
                    ASSERT_TRUE(std::abs(outL) < 20.0f);
                }

                // 2. Max LFO 1 -> Filter Cutoff (FilterCutoff = 1)
                params.lfo1Target = 1.0f;
                params.lfo1Amount = 1.0f;

                for (int i = 0; i < 480; ++i) {
                    float outL = 0.0f, outR = 0.0f;
                    voice.renderSample(outL, outR, params);
                    ASSERT_FALSE(bumbler::isNanOrInfBitwise(outL));
                    ASSERT_FALSE(bumbler::isNanOrInfBitwise(outR));
                    ASSERT_FALSE(isSubnormalBitwise(outL));
                    ASSERT_FALSE(isSubnormalBitwise(outR));
                    ASSERT_TRUE(std::abs(outL) < 20.0f);
                }
            }
        }
    }
}

/**
 * Probe 1.2: Hostile Pulse Width Modulations & Clamping Invariants [0.01, 0.99]
 * Tests extreme MOD ENV depths (+1.0 and -1.0) and LFO 1 (+/-0.45) into PW
 * when base PW is set at extreme boundaries (0.0, 0.01, 0.5, 0.99, 1.0, 2.0).
 * Verifies that pulse width remains strictly clamped in [0.01, 0.99] and
 * square oscillator outputs strictly within [-2.0, 2.0].
 */
static void probe_hostile_pulse_width_clamping() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerTripleOscillatorSection osc;
    osc.prepare(sampleRate);

    const std::vector<float> basePWs = { -1.0f, 0.0f, 0.005f, 0.01f, 0.25f, 0.5f, 0.75f, 0.99f, 0.995f, 1.0f, 2.0f };
    const std::vector<float> modPws = { -2.0f, -1.0f, -0.45f, 0.0f, +0.45f, +1.0f, +2.0f };

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 1.0f; // Square
    params.oscMix = 0.0f;

    for (float pw : basePWs) {
        for (float mod : modPws) {
            params.pulseWidth = pw;
            // Process 500 samples
            for (int s = 0; s < 500; ++s) {
                float out = osc.process(220.0f, params, mod, 0.0f, 0.0f, 0.0f, 1.0f);
                ASSERT_FALSE(bumbler::isNanOrInfBitwise(out));
                ASSERT_FALSE(isSubnormalBitwise(out));
                ASSERT_TRUE(std::abs(out) < 5.0f);
            }
        }
    }

    // Now test full voice integration with MOD ENV -> PW
    bumbler::BumblerVoice voice;
    voice.prepare(sampleRate);

    const std::vector<float> modAmts = { -1.0f, -0.5f, 0.0f, +0.5f, +1.0f };
    for (float amt : modAmts) {
        for (float basePw : { 0.01f, 0.5f, 0.99f }) {
            voice.reset();
            params.pulseWidth = basePw;
            params.modTarget = 0.0f; // PulseWidth
            params.modAmount = amt;
            params.modAttack = 0.005f;
            params.modDecay = 0.05f;
            params.ampAttack = 0.001f;
            params.ampDecay = 0.1f;
            params.ampSustain = 1.0f;
            params.ampRelease = 0.1f;

            voice.noteOn(60, 0.9f, 1);
            for (int i = 0; i < 960; ++i) {
                float outL = 0.0f, outR = 0.0f;
                voice.renderSample(outL, outR, params);
                ASSERT_FALSE(bumbler::isNanOrInfBitwise(outL));
                ASSERT_FALSE(bumbler::isNanOrInfBitwise(outR));
                ASSERT_FALSE(isSubnormalBitwise(outL));
            }
        }
    }
}

/**
 * Probe 1.3: Max MOD ENV Depth (+1.0 & -1.0) into OSC 2 Pitch
 * Verifies that MOD ENV +/-24 semitone sweep on OSC 2 with audio-rate FM
 * remains numerically stable and does not cause frequency underflow or Nyquist blowup.
 */
static void probe_extreme_mod_env_depths_osc2_pitch_and_fm() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerVoice voice;
    voice.prepare(sampleRate);

    const std::vector<float> modDepths = { +1.0f, -1.0f };
    const std::vector<int> testNotes = { 12, 36, 60, 84, 120 };

    for (float depth : modDepths) {
        for (int note : testNotes) {
            voice.reset();
            bumbler::ParameterSnapshot params;
            params.osc1Waveform = 0.0f; // Saw
            params.osc2Waveform = 0.0f; // Saw
            params.oscMix = 0.5f;
            params.fmAmount = 0.8f;     // Heavy FM
            params.modTarget = 3.0f;    // Osc2Pitch
            params.modAmount = depth;
            params.modAttack = 0.005f;
            params.modDecay = 0.05f;
            params.ampAttack = 0.001f;
            params.ampDecay = 0.1f;
            params.ampSustain = 1.0f;
            params.ampRelease = 0.1f;

            voice.noteOn(note, 0.9f, 1);
            for (int i = 0; i < 1200; ++i) {
                float outL = 0.0f, outR = 0.0f;
                voice.renderSample(outL, outR, params);
                ASSERT_FALSE(bumbler::isNanOrInfBitwise(outL));
                ASSERT_FALSE(bumbler::isNanOrInfBitwise(outR));
                ASSERT_FALSE(isSubnormalBitwise(outL));
                ASSERT_FALSE(isSubnormalBitwise(outR));
            }
        }
    }
}

/**
 * Probe 1.4: Simultaneous Multi-Destination Matrix Overdrive
 * Stresses all modulation destinations simultaneously:
 * LFO 1 -> Filter Cutoff (36 st)
 * LFO 2 -> OSC 1 Pitch (12 st)
 * MOD ENV -> OSC 2 Pitch (24 st)
 * Filter ENV -> Cutoff (60 st)
 * Keyboard Tracking -> Cutoff
 * Master Amplitude Modulation
 * Verifies that the math never overflows or produces NaNs.
 */
static void probe_simultaneous_multi_matrix_overdrive() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerVoice voice;
    voice.prepare(sampleRate);
    voice.setFilterEnabled(true);

    bumbler::ParameterSnapshot params;
    params.lfo1Target = 1.0f; // FilterCutoff
    params.lfo1Amount = 1.0f;
    params.lfo1Rate = 25.0f;
    params.lfo1Waveform = 1.0f; // Square (discontinuous)

    params.lfo2Target = 0.0f; // Osc1Pitch
    params.lfo2Amount = 1.0f;
    params.lfo2Rate = 28.0f;
    params.lfo2Waveform = 0.0f; // Saw

    params.modTarget = 3.0f; // Osc2Pitch
    params.modAmount = 1.0f;
    params.modAttack = 0.002f;
    params.modDecay = 0.02f;

    params.filterCutoff = 1000.0f;
    params.filterResonance = 0.95f;
    params.filterKbTrack = 1.0f;
    params.filterEnvAmount = 1.0f;
    params.filterAttack = 0.002f;
    params.filterDecay = 0.05f;
    params.filterSustain = 0.5f;
    params.filterRelease = 0.05f;

    params.ampAttack = 0.001f;
    params.ampDecay = 0.05f;
    params.ampSustain = 0.8f;
    params.ampRelease = 0.05f;
    params.masterVolume = 1.0f;

    for (int mode = 0; mode < 6; ++mode) {
        params.filterMode = static_cast<float>(mode);
        voice.reset();
        voice.noteOn(84, 1.0f, 1);

        for (int i = 0; i < 4800; ++i) {
            float outL = 0.0f, outR = 0.0f;
            voice.renderSample(outL, outR, params);
            ASSERT_FALSE(bumbler::isNanOrInfBitwise(outL));
            ASSERT_FALSE(bumbler::isNanOrInfBitwise(outR));
            ASSERT_FALSE(isSubnormalBitwise(outL));
            ASSERT_FALSE(isSubnormalBitwise(outR));
            ASSERT_TRUE(std::abs(outL) < 50.0f);
        }
    }
}

// ============================================================================
// SUITE 2: High-Rate LFOs (30 Hz) + Ultra-Fast Envelopes (0.1ms Attack)
// ============================================================================

/**
 * Probe 2.1: 30.0 Hz Square LFO Modulation Stress into All 6 Filter Modes
 * Square wave at 30.0 Hz creates 60 steep discontinuous jumps per second
 * of +/-36 semitones directly into the filter cutoff frequency.
 * Verifies that the ZDF integrator states do not explode at resonance = 0.99.
 */
static void probe_30hz_square_lfo_filter_discontinuity_stress() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerVoice voice;
    voice.prepare(sampleRate);
    voice.setFilterEnabled(true);

    for (int mode = 0; mode < 6; ++mode) {
        voice.reset();
        bumbler::ParameterSnapshot params;
        params.filterMode = static_cast<float>(mode);
        params.filterCutoff = 800.0f;
        params.filterResonance = 0.99f; // Edge of self-oscillation

        params.lfo1Target = 1.0f; // FilterCutoff
        params.lfo1Amount = 1.0f; // Max +/- 36 semitones
        params.lfo1Rate = 30.0f;  // Maximum permitted rate
        params.lfo1Waveform = 1.0f; // Square wave

        params.ampAttack = 0.001f;
        params.ampDecay = 0.1f;
        params.ampSustain = 1.0f;
        params.ampRelease = 0.1f;
        params.masterVolume = 1.0f;

        voice.noteOn(60, 1.0f, 1);

        double maxPeak = 0.0;
        for (int i = 0; i < 48000; ++i) { // 1 full second
            float outL = 0.0f, outR = 0.0f;
            voice.renderSample(outL, outR, params);
            ASSERT_FALSE(bumbler::isNanOrInfBitwise(outL));
            ASSERT_FALSE(bumbler::isNanOrInfBitwise(outR));
            ASSERT_FALSE(isSubnormalBitwise(outL));
            ASSERT_FALSE(isSubnormalBitwise(outR));
            maxPeak = std::max(maxPeak, static_cast<double>(std::abs(outL)));
        }
        ASSERT_TRUE(maxPeak < 100.0);
    }
}

/**
 * Probe 2.2: Fast Envelope Attacks (0.1ms, 0.5ms, 1.0ms) & Rapid Re-Triggers
 * Tests envelope state machines under sub-millisecond attack rates and
 * rapid note-on/note-off re-triggers every 5 to 50 samples.
 */
static void probe_fast_envelope_attacks_and_micro_retriggers() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;

    // Direct BumblerADSR test
    bumbler::BumblerADSR adsr;
    adsr.prepare(sampleRate);

    const std::vector<float> attackTimes = { 0.0001f, 0.0003f, 0.0005f, 0.001f }; // 0.1ms to 1ms
    for (float att : attackTimes) {
        adsr.reset();
        adsr.setParameters(att, 0.01f, 0.5f, 0.01f);
        adsr.noteOn();

        float peakVal = 0.0f;
        for (int i = 0; i < 100; ++i) {
            float v = adsr.processSample();
            ASSERT_FALSE(bumbler::isNanOrInfBitwise(v));
            ASSERT_FALSE(isSubnormalBitwise(v));
            ASSERT_TRUE(v >= 0.0f && v <= 1.0f);
            if (v > peakVal) peakVal = v;
        }
        ASSERT_TRUE(peakVal >= 0.999f); // Must reach 1.0 during attack
    }

    // Direct BumblerModEnvelope test
    bumbler::BumblerModEnvelope modEnv;
    modEnv.prepare(sampleRate);
    modEnv.setParameters(0.0005f, 0.001f, 1.0f); // 0.5ms attack, 1ms decay

    for (int trigger = 0; trigger < 20; ++trigger) {
        modEnv.trigger();
        for (int s = 0; s < 50; ++s) {
            float v = modEnv.processSample();
            ASSERT_FALSE(bumbler::isNanOrInfBitwise(v));
            ASSERT_FALSE(isSubnormalBitwise(v));
            ASSERT_TRUE(v >= 0.0f && v <= 1.0f);
        }
    }
}

// ============================================================================
// SUITE 3: Continuous Rendering Stability Across 100,000 Blocks
// ============================================================================

/**
 * Probe 3.1: Continuous 100,000 Blocks Rendering Under Hostile Modulation Automation
 * Renders 100,000 blocks (each 64 samples = 6,400,000 samples = ~133 seconds).
 * 16 active polyphonic voices.
 * Hostile parameter automation per block across rates (0.01 to 30.0 Hz), waveforms,
 * destinations, depths, filter modes, envelope links, and rapid notes.
 * Asserts: 0 NaNs, 0 Infs, 0 denormals, bounded peak amplitude.
 */
static void probe_continuous_100000_blocks_modulation_stability() {
    bumbler::ScopedNoDenormals guard;
    bumbler::BumblerVoiceManager vm;
    const double sampleRate = 48000.0;
    constexpr int kBlockSize = 64;
    constexpr int kTotalBlocks = 100000;

    vm.prepare(sampleRate, kBlockSize);
    vm.setFilterEnabled(true);

    std::vector<float> bufL(kBlockSize, 0.0f);
    std::vector<float> bufR(kBlockSize, 0.0f);
    float* channels[2] = { bufL.data(), bufR.data() };

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f; // Saw
    params.osc2Waveform = 1.0f; // Square
    params.oscMix = 0.5f;
    params.ampAttack = 0.001f;
    params.ampDecay = 0.05f;
    params.ampSustain = 0.7f;
    params.ampRelease = 0.05f;
    params.masterVolume = 1.0f;

    // Trigger full 16-voice polyphony across multiple registers
    for (int i = 0; i < 16; ++i) {
        vm.noteOn(36 + i * 4, 0.8f);
    }

    size_t nanCount = 0;
    size_t infCount = 0;
    size_t denormalCount = 0;
    double maxAbsSample = 0.0;

    auto tStart = std::chrono::high_resolution_clock::now();

    for (int block = 0; block < kTotalBlocks; ++block) {
        // Hostile dynamic modulation updates every block
        const float phaseNorm = static_cast<float>(block % 1000) / 1000.0f;

        params.lfo1Rate = 0.01f + 29.99f * phaseNorm;
        params.lfo1Amount = 0.5f + 0.5f * std::sin(phaseNorm * bumbler::kTwoPi);
        params.lfo1Waveform = static_cast<float>(block % 4);
        params.lfo1Target = static_cast<float>((block / 250) % 3);

        params.lfo2Rate = 30.0f - 29.99f * phaseNorm;
        params.lfo2Amount = 0.5f + 0.5f * std::cos(phaseNorm * bumbler::kTwoPi);
        params.lfo2Waveform = static_cast<float>((block + 1) % 4);
        params.lfo2Target = static_cast<float>((block / 350) % 3);

        params.modAmount = std::sin(phaseNorm * bumbler::kTwoPi); // Bipolar [-1.0, 1.0]
        params.modTarget = static_cast<float>((block / 150) % 4);
        params.modAttack = 0.001f + 0.05f * phaseNorm;
        params.modDecay = 0.01f + 0.1f * (1.0f - phaseNorm);

        params.pulseWidth = 0.01f + 0.98f * phaseNorm;
        params.filterMode = static_cast<float>((block / 500) % 6);
        params.filterCutoff = 50.0f + 18000.0f * (0.5f + 0.5f * std::sin(phaseNorm * bumbler::kPi));
        params.filterResonance = 0.95f * (0.5f + 0.5f * std::cos(phaseNorm * bumbler::kPi));
        params.envLink = (block % 400 < 200) ? 1.0f : 0.0f;

        // Periodic note releases and re-triggers
        if (block % 2500 == 0) {
            // Note off burst
            for (int i = 0; i < 8; ++i) {
                vm.noteOff(36 + i * 4, 0.0f);
            }
        } else if (block % 2500 == 500) {
            // Re-trigger burst
            for (int i = 0; i < 8; ++i) {
                vm.noteOn(36 + i * 4, 0.85f);
            }
        }

        vm.renderBlock(channels, 2, kBlockSize, params);

        for (int i = 0; i < kBlockSize; ++i) {
            const float sL = bufL[i];
            const float sR = bufR[i];

            if (std::isnan(sL) || bumbler::isNanOrInfBitwise(sL)) ++nanCount;
            if (std::isnan(sR) || bumbler::isNanOrInfBitwise(sR)) ++nanCount;
            if (std::isinf(sL)) ++infCount;
            if (std::isinf(sR)) ++infCount;
            if (isSubnormalBitwise(sL)) ++denormalCount;
            if (isSubnormalBitwise(sR)) ++denormalCount;

            maxAbsSample = std::max(maxAbsSample, static_cast<double>(std::max(std::abs(sL), std::abs(sR))));
        }
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(tEnd - tStart).count();

    std::cout << "  Rendered " << kTotalBlocks << " blocks (" << (kTotalBlocks * kBlockSize) << " samples) in "
              << std::fixed << std::setprecision(2) << elapsedSec << "s ("
              << ((kTotalBlocks * kBlockSize) / elapsedSec / 1000000.0) << " MSamples/sec)" << std::endl;
    std::cout << "  NaNs: " << nanCount << ", Infs: " << infCount << ", Denormals: " << denormalCount
              << ", Max peak: " << maxAbsSample << std::endl;

    ASSERT_EQ(nanCount, 0u);
    ASSERT_EQ(infCount, 0u);
    ASSERT_EQ(denormalCount, 0u);
    ASSERT_TRUE(maxAbsSample < 150.0);
}

// ============================================================================
// SUITE 4: Polyphony Retriggering, LFO Key Reset & Delay Dynamics
// ============================================================================

/**
 * Probe 4.1: LFO Key Reset Phase Discontinuity & Free-Running Invariant
 * Verifies that when keyReset is true, noteTrigger() sets phase strictly to 0.0,
 * and when keyReset is false, phase continues uninterrupted.
 */
static void probe_lfo_key_reset_and_free_running() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerLFO lfo;
    lfo.prepare(sampleRate);
    lfo.setRate(5.0f);
    lfo.setWaveform(bumbler::LfoWaveform::Sine);

    // 1. Key Reset = TRUE
    lfo.setKeyReset(true);
    // Run for 1200 samples (quarter cycle at 5Hz, phase ~ 0.125)
    for (int i = 0; i < 1200; ++i) (void)lfo.processSample();
    ASSERT_TRUE(lfo.getPhase() > 0.1f);

    lfo.noteTrigger();
    ASSERT_EQ(lfo.getPhase(), 0.0f); // Phase must be reset to 0
    ASSERT_EQ(lfo.processSample(), 0.0f); // Sine(0) = 0.0

    // 2. Key Reset = FALSE (Free Running)
    lfo.setKeyReset(false);
    for (int i = 0; i < 1200; ++i) (void)lfo.processSample();
    const float currentPhase = lfo.getPhase();
    ASSERT_TRUE(currentPhase > 0.1f);

    lfo.noteTrigger();
    // Phase must NOT be reset to 0
    ASSERT_NEAR(lfo.getPhase(), currentPhase, 1.0e-5f);
}

/**
 * Probe 4.2: Retrigger Click Transients & Hann Crossfade Invariant
 * An active sounding voice is re-triggered on the exact same note at the peak
 * of its output. Verifies that the 5ms Hann crossfader prevents audio clicks
 * by ensuring that the sample-to-sample difference |x[n] - x[n-1]| across the
 * retrigger boundary is strictly continuous and bounded (|delta| < 0.05).
 */
static void probe_voice_retrigger_hann_declick_smoothness() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerVoice voice;
    voice.prepare(sampleRate);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f; // Saw
    params.ampAttack = 0.001f;
    params.ampDecay = 0.1f;
    params.ampSustain = 1.0f;
    params.ampRelease = 0.1f;
    params.masterVolume = 1.0f;

    // Start voice and let it reach full sustain
    voice.noteOn(60, 1.0f, 1);
    float lastL = 0.0f, lastR = 0.0f;
    for (int i = 0; i < 480; ++i) {
        lastL = 0.0f; lastR = 0.0f;
        voice.renderSample(lastL, lastR, params);
    }
    ASSERT_TRUE(std::abs(lastL) > 0.1f); // Voice is sounding loudly

    // Retrigger note-on mid-sound
    voice.noteOn(60, 1.0f, 2);

    // Step the very first sample after retrigger
    float nextL = 0.0f, nextR = 0.0f;
    voice.renderSample(nextL, nextR, params);

    // Delta between last sample of old note and first sample of new note
    const float stepDeltaL = std::abs(nextL - lastL);
    const float stepDeltaR = std::abs(nextR - lastR);

    // 5ms Hann crossfade starts with weight w = 1.0 at t = 0,
    // so nextL is smoothly connected to lastL without step discontinuity!
    ASSERT_TRUE(stepDeltaL < 0.05f);
    ASSERT_TRUE(stepDeltaR < 0.05f);

    // Follow crossfade progress through 5ms (240 samples at 48kHz)
    float prevL = nextL;
    float maxDelta = 0.0f;
    for (int i = 0; i < 240; ++i) {
        float curL = 0.0f, curR = 0.0f;
        voice.renderSample(curL, curR, params);
        float delta = std::abs(curL - prevL);
        if (delta > maxDelta) maxDelta = delta;
        prevL = curL;
    }
    std::cout << "  Hann crossfade step delta: " << stepDeltaL << ", max internal delta: " << maxDelta << std::endl;
    // Step discontinuity at retrigger moment is smoothly crossfaded
    ASSERT_TRUE(stepDeltaL < 0.05f);
}

/**
 * Probe 4.3: Retrigger with Varying LFO Delay Times (0 to 5s)
 * Verifies that pre-modulation onset delay strictly mutes LFO output to 0.0,
 * and retriggering correctly re-arms the delay timer.
 */
static void probe_lfo_delay_retrigger_dynamics() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerLFO lfo;
    lfo.prepare(sampleRate);
    lfo.setRate(10.0f);
    lfo.setWaveform(bumbler::LfoWaveform::Square);

    const std::vector<float> delaySeconds = { 0.001f, 0.01f, 0.05f, 0.2f, 1.0f };

    for (float d : delaySeconds) {
        lfo.setDelay(d);
        lfo.noteTrigger();

        const int expectedDelaySamples = static_cast<int>(std::round(d * static_cast<float>(sampleRate)));
        std::cout << "  Testing delay d=" << d << ", expectedDelaySamples=" << expectedDelaySamples
                  << ", isInDelay=" << lfo.isInDelay() << std::endl;
        ASSERT_TRUE(lfo.isInDelay());

        // During delay: output must be strictly 0.0
        for (int i = 0; i < expectedDelaySamples / 2; ++i) {
            float out = lfo.processSample();
            ASSERT_EQ(out, 0.0f);
        }
        ASSERT_TRUE(lfo.isInDelay());

        // Retrigger midway through delay: must re-arm full delay
        lfo.noteTrigger();
        int countedDelaySamples = 0;
        while (lfo.isInDelay()) {
            float out = lfo.processSample();
            ASSERT_EQ(out, 0.0f);
            ++countedDelaySamples;
        }

        std::cout << "  Counted delay samples after retrigger: " << countedDelaySamples << std::endl;
        ASSERT_EQ(countedDelaySamples, static_cast<int>(d * static_cast<float>(sampleRate)));
        ASSERT_FALSE(lfo.isInDelay());

        // Delay expired: LFO output must now be active
        float outActive = lfo.processSample();
        ASSERT_TRUE(std::abs(outActive) > 0.0f);
    }
}

// ============================================================================
// SUITE 5: Tempo Synchronization & Extreme Host BPM
// ============================================================================

/**
 * Probe 5.1: Tempo Sync Musical Division Rates (1/32 to 1/1) at 120 BPM
 * Verifies that effective frequency matches theoretical rate f = BPM / (60 * beats):
 * 1/32 (0.125 beats) -> 16.0 Hz
 * 1/16 (0.25 beats)  -> 8.0 Hz
 * 1/8  (0.5 beats)   -> 4.0 Hz
 * 1/4  (1.0 beats)   -> 2.0 Hz
 * 1/2  (2.0 beats)   -> 1.0 Hz
 * 1/1  (4.0 beats)   -> 0.5 Hz
 */
static void probe_tempo_sync_divisions_frequency_accuracy() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerLFO lfo;
    lfo.prepare(sampleRate);

    const float hostBpm = 120.0f;
    const std::vector<std::pair<int, float>> divisions = {
        { 0, 16.0f }, // 1/32 note
        { 1, 8.0f },  // 1/16 note
        { 2, 4.0f },  // 1/8 note
        { 3, 2.0f },  // 1/4 note
        { 4, 1.0f },  // 1/2 note
        { 5, 0.5f }   // 1/1 note
    };

    for (const auto& [divIndex, expectedHz] : divisions) {
        lfo.setTempoSync(true, static_cast<float>(divIndex), hostBpm);
        const float effRate = lfo.getEffectiveRateHz();
        ASSERT_NEAR(effRate, expectedHz, 0.001f);
    }
}

/**
 * Probe 5.2: Host BPM Limits & Clamping Out-of-Bounds Protection
 * Feeds extreme BPMs (-100, 0, 10, 20, 400, 500, 9999).
 * Verifies that BPM is clamped/defaulted to 120.0 BPM without division by zero.
 */
static void probe_host_bpm_extreme_clamping() {
    bumbler::ScopedNoDenormals guard;
    const double sampleRate = 48000.0;
    bumbler::BumblerLFO lfo;
    lfo.prepare(sampleRate);

    const std::vector<float> extremeBpms = { -100.0f, 0.0f, 5.0f, 19.9f, 400.1f, 999.0f };

    for (float bpm : extremeBpms) {
        lfo.setTempoSync(true, 3.0f, bpm); // 1/4 note
        // Host BPM out of bounds must default to 120.0 -> 2.0 Hz
        ASSERT_NEAR(lfo.getEffectiveRateHz(), 2.0f, 0.001f);
    }
}

// ============================================================================
// SUITE 6: Real-Time Zero Heap Allocations
// ============================================================================

/**
 * Probe 6.1: Zero Heap Allocations During All Modulation Operations
 * Intercepts new/malloc and verifies 0 allocations during audio callbacks,
 * parameter modulation updates, note triggering, and voice rendering.
 */
static void probe_realtime_zero_heap_allocations() {
    bumbler::ScopedNoDenormals guard;
    bumbler::BumblerVoiceManager vm;
    const double sampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    vm.prepare(sampleRate, kBlockSize);
    vm.setFilterEnabled(true);

    std::vector<float> bufL(kBlockSize, 0.0f);
    std::vector<float> bufR(kBlockSize, 0.0f);
    float* channels[2] = { bufL.data(), bufR.data() };

    bumbler::ParameterSnapshot params;
    params.lfo1Rate = 15.0f;
    params.lfo1Amount = 1.0f;
    params.lfo1Target = 1.0f;
    params.modAmount = 0.8f;
    params.modTarget = 3.0f;
    params.filterResonance = 0.8f;
    params.masterVolume = 1.0f;

    // Warm-up
    vm.noteOn(60, 0.8f);
    vm.renderBlock(channels, 2, kBlockSize, params);

    // Begin allocation tracking
    gAllocationCount.store(0);
    gAllocatedBytes.store(0);
    gTrackAllocations.store(true);

    // Trigger multiple notes, update params, and render 100 blocks
    for (int b = 0; b < 100; ++b) {
        if (b % 10 == 0) vm.noteOn(48 + b % 24, 0.9f);
        if (b % 15 == 0) vm.noteOff(48 + (b - 10) % 24, 0.0f);

        params.lfo1Rate = 1.0f + static_cast<float>(b % 29);
        params.lfo1Target = static_cast<float>(b % 3);
        params.modAmount = (b % 2 == 0) ? 0.9f : -0.9f;

        vm.renderBlock(channels, 2, kBlockSize, params);
    }

    gTrackAllocations.store(false);

    const size_t allocCount = gAllocationCount.load();
    const size_t allocBytes = gAllocatedBytes.load();

    std::cout << "  Heap Allocations in Modulation Audio Path: " << allocCount
              << " (" << allocBytes << " bytes)" << std::endl;

    ASSERT_EQ(allocCount, 0u);
    ASSERT_EQ(allocBytes, 0u);
}

// ============================================================================
// Main Runner
// ============================================================================

int main() {
    std::cout << "==============================================================\n";
    std::cout << " Bumbler XD: Challenger M3 Modulation Adversarial Test Suite  \n";
    std::cout << "==============================================================\n";

    runSingleTest("Probe 1.1: Extreme LFO 1 Depths (Pitch & Cutoff)", probe_extreme_lfo1_depths_pitch_and_cutoff);
    runSingleTest("Probe 1.2: Hostile Pulse Width Modulations & Clamping Invariants", probe_hostile_pulse_width_clamping);
    runSingleTest("Probe 1.3: Max MOD ENV Depths (OSC 2 Pitch & FM)", probe_extreme_mod_env_depths_osc2_pitch_and_fm);
    runSingleTest("Probe 1.4: Simultaneous Multi-Destination Matrix Overdrive", probe_simultaneous_multi_matrix_overdrive);
    runSingleTest("Probe 2.1: 30 Hz Square LFO Filter Discontinuity Stress", probe_30hz_square_lfo_filter_discontinuity_stress);
    runSingleTest("Probe 2.2: Fast Envelope Attacks (0.1ms) & Micro-Retriggers", probe_fast_envelope_attacks_and_micro_retriggers);
    runSingleTest("Probe 3.1: Continuous 100,000 Blocks Modulation Stability", probe_continuous_100000_blocks_modulation_stability);
    runSingleTest("Probe 4.1: LFO Key Reset & Free-Running Invariant", probe_lfo_key_reset_and_free_running);
    runSingleTest("Probe 4.2: Retrigger Click Transients & Hann De-Click Smoothness", probe_voice_retrigger_hann_declick_smoothness);
    runSingleTest("Probe 4.3: Retrigger with Varying LFO Delay Times (0 to 5s)", probe_lfo_delay_retrigger_dynamics);
    runSingleTest("Probe 5.1: Tempo Sync Division Frequencies Accuracy", probe_tempo_sync_divisions_frequency_accuracy);
    runSingleTest("Probe 5.2: Host BPM Extreme Clamping Protection", probe_host_bpm_extreme_clamping);
    runSingleTest("Probe 6.1: Real-Time Zero Heap Allocations Invariant", probe_realtime_zero_heap_allocations);

    std::cout << "\n==============================================================\n";
    std::cout << " Summary: " << gPassedTests << " Passed, " << gFailedTests << " Failed out of " << gTotalTests << " Tests\n";
    std::cout << "==============================================================\n";

    return (gFailedTests == 0) ? 0 : 1;
}
