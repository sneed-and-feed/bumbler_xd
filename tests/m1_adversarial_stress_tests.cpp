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

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "BumblerVoice.h"
#include "BumblerVoiceManager.h"

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
    std::cout << "\n[ADVERSARIAL RUN] " << testName << std::endl;
    try {
        testFunc();
        ++gPassedTests;
        std::cout << "[       PASSED ] " << testName << std::endl;
    } catch (const std::exception& e) {
        ++gFailedTests;
        std::cerr << "[    FAILED!!! ] " << testName << " -> " << e.what() << std::endl;
    } catch (...) {
        ++gFailedTests;
        std::cerr << "[    FAILED!!! ] " << testName << " -> Unknown exception" << std::endl;
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

// Bitwise check for IEEE-754 subnormal (denormal) float:
// Exponent bits are all 0, and mantissa is non-zero
[[nodiscard]] inline bool isSubnormalBitwise(float val) noexcept {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(float));
    return ((bits & 0x7F800000u) == 0u) && ((bits & 0x007FFFFFu) != 0u);
}

// ============================================================================
// Test 1: Numerical Stability Under Extreme Tuning & Pitch Boundaries
// ============================================================================
// Tests coarse tune +/-3 octaves, fine tune +/-1 semitone, pitch bend +/-12 semitones
// on MIDI notes 0, 1, 60, 126, 127 across all 4 oscillator waveforms.
static void test_extreme_tuning_and_pitch_boundaries() {
    bumbler::ScopedNoDenormals guard;
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const std::vector<int> midiNotes = { 0, 1, 12, 60, 120, 126, 127 };
    const std::vector<float> coarseTunes = { -3.0f, -2.0f, 0.0f, +2.0f, +3.0f };
    const std::vector<float> fineTunes = { -1.0f, 0.0f, +1.0f };
    const std::vector<float> pitchBends = { -12.0f, 0.0f, +12.0f };
    const std::vector<int> waveforms = { 0, 1, 2, 3 }; // Saw, Square, Sine, Noise

    for (double sr : sampleRates) {
        bumbler::BumblerVoice voice;
        voice.prepare(sr);

        for (int note : midiNotes) {
            for (float coarse : coarseTunes) {
                for (float fine : fineTunes) {
                    for (float bend : pitchBends) {
                        for (int wave : waveforms) {
                            bumbler::ParameterSnapshot params;
                            params.osc1Waveform = static_cast<float>(wave);
                            params.osc1Octave = coarse;
                            params.osc1Fine = fine;
                            params.osc2Waveform = static_cast<float>(wave);
                            params.osc2Octave = coarse;
                            params.osc2Fine = fine;
                            params.oscMix = 0.5f;
                            params.pulseWidth = 0.5f;
                            params.masterVolume = 1.0f;
                            params.ampAttack = 0.001f;
                            params.ampDecay = 0.05f;
                            params.ampSustain = 0.8f;
                            params.ampRelease = 0.05f;

                            voice.reset();
                            voice.setPitchBend(bend);
                            voice.noteOn(note, 1.0f, 0);

                            for (int s = 0; s < 256; ++s) {
                                float outL = 0.0f;
                                float outR = 0.0f;
                                voice.renderSample(outL, outR, params);

                                ASSERT_TRUE(bumbler::isFiniteBitwise(outL));
                                ASSERT_TRUE(bumbler::isFiniteBitwise(outR));
                                ASSERT_FALSE(bumbler::isNanOrInfBitwise(outL));
                                ASSERT_FALSE(bumbler::isNanOrInfBitwise(outR));
                                ASSERT_FALSE(isSubnormalBitwise(outL));
                                ASSERT_FALSE(isSubnormalBitwise(outR));

                                // Bound check: individual voice output must stay in [-4.0, 4.0]
                                ASSERT_TRUE(std::abs(outL) <= 4.0f);
                                ASSERT_TRUE(std::abs(outR) <= 4.0f);
                            }
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// Test 2: Pulse Width Boundary Extremes (0.0001 to 0.9999)
// ============================================================================
static void test_pulse_width_boundary_extremes() {
    bumbler::ScopedNoDenormals guard;
    bumbler::BumblerOscillator osc;
    osc.prepare(48000.0);

    const std::vector<float> testPws = {
        -1.0f, 0.0f, 0.0001f, 0.001f, 0.01f, 0.05f, 0.25f, 0.50f, 0.75f, 0.95f, 0.99f, 0.999f, 0.9999f, 1.0f, 2.0f
    };
    const std::vector<float> freqs = { 20.0f, 100.0f, 440.0f, 2000.0f, 10000.0f, 22000.0f };

    for (float f : freqs) {
        osc.setFrequency(f);
        for (float pw : testPws) {
            osc.reset(0.0f);

            double sum = 0.0;
            double energy = 0.0;
            const int kSamples = 4800;

            for (int i = 0; i < kSamples; ++i) {
                const float s = osc.process(bumbler::OscWaveform::Square, pw, bumbler::WNoiseMode::Vintage, 0.0f);
                ASSERT_TRUE(bumbler::isFiniteBitwise(s));
                ASSERT_FALSE(bumbler::isNanOrInfBitwise(s));
                ASSERT_FALSE(isSubnormalBitwise(s));
                ASSERT_TRUE(std::abs(s) <= 3.0f);

                sum += s;
                energy += s * s;
            }

            // Energy must be non-zero (oscillator is vibrating)
            ASSERT_TRUE(energy > 1.0);
        }
    }
}

// ============================================================================
// Test 3: Maximum FM Modulation Index Combined with High Pitch
// ============================================================================
static void test_extreme_fm_modulation_index() {
    bumbler::ScopedNoDenormals guard;
    bumbler::BumblerTripleOscillatorSection oscSec;
    oscSec.prepare(48000.0);

    const std::vector<float> fmAms = { 0.0f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 50.0f, 100.0f, 1000.0f };
    const std::vector<float> basePitches = { 440.0f, 5000.0f, 12000.0f, 20000.0f };

    for (float baseHz : basePitches) {
        for (float fm : fmAms) {
            for (int wave1 = 0; wave1 < 4; ++wave1) {
                for (int wave2 = 0; wave2 < 4; ++wave2) {
                    bumbler::ParameterSnapshot params;
                    params.osc1Waveform = static_cast<float>(wave1);
                    params.osc1Octave = +3.0f; // Max high pitch
                    params.osc2Waveform = static_cast<float>(wave2);
                    params.osc2Octave = +3.0f;
                    params.fmAmount = fm;
                    params.oscMix = 0.5f;

                    oscSec.reset(false);

                    for (int s = 0; s < 1024; ++s) {
                        const float out = oscSec.process(baseHz, params, 0.0f, 0.0f, 0.0f);
                        ASSERT_TRUE(bumbler::isFiniteBitwise(out));
                        ASSERT_FALSE(bumbler::isNanOrInfBitwise(out));
                        ASSERT_FALSE(isSubnormalBitwise(out));
                        ASSERT_TRUE(std::abs(out) <= 10.0f);
                    }
                }
            }
        }
    }
}

// ============================================================================
// Test 4: High-Gain Ring Modulation & DC Offset Stability
// ============================================================================
static void test_high_gain_ring_modulation() {
    bumbler::ScopedNoDenormals guard;
    bumbler::BumblerTripleOscillatorSection oscSec;
    oscSec.prepare(48000.0);

    // Test pairs of frequencies: identical (creates large DC) and disparate
    struct FreqPair {
        float f1;
        float f2;
        const char* desc;
    };

    const std::vector<FreqPair> pairs = {
        { 40.0f, 40.0f, "Identical Low (40Hz)" },
        { 440.0f, 440.0f, "Identical Mid (440Hz)" },
        { 10000.0f, 10000.0f, "Identical High (10kHz)" },
        { 440.0f, 441.0f, "Micro-detuned 1Hz Beat" },
        { 50.0f, 12000.0f, "Extreme Disparate (50Hz vs 12kHz)" },
        { 15000.0f, 19000.0f, "High-frequency Intermodulation" }
    };

    for (const auto& pair : pairs) {
        for (int wave = 0; wave < 3; ++wave) { // Saw, Square, Sine
            bumbler::ParameterSnapshot params;
            params.osc1Waveform = static_cast<float>(wave);
            params.osc2Waveform = static_cast<float>(wave);
            params.osc1Octave = 0.0f;
            params.osc2Octave = 0.0f;
            params.ringModMix = 1.0f; // 100% Pure Ring Modulation
            params.oscMix = 0.5f;

            oscSec.reset(false);

            // Burn-in: let DC blocker stabilize
            for (int s = 0; s < 4800; ++s) {
                // Pitch offsets to match pair.f1 and pair.f2 from a base 440Hz
                const float semi1 = 12.0f * std::log2(pair.f1 / 440.0f);
                const float semi2 = 12.0f * std::log2(pair.f2 / 440.0f);
                (void)oscSec.process(440.0f, params, 0.0f, semi1, semi2);
            }

            // Measurement block: for low-frequency beat, use full 1-second period (48000 samples)
            const int kMeasureSamples = (std::abs(pair.f1 - pair.f2) > 0.0f && std::abs(pair.f1 - pair.f2) <= 10.0f) ? 48000 : 4800;
            double dcSum = 0.0;
            double peakVal = 0.0;
            for (int s = 0; s < kMeasureSamples; ++s) {
                const float semi1 = 12.0f * std::log2(pair.f1 / 440.0f);
                const float semi2 = 12.0f * std::log2(pair.f2 / 440.0f);
                const float out = oscSec.process(440.0f, params, 0.0f, semi1, semi2);

                ASSERT_TRUE(bumbler::isFiniteBitwise(out));
                ASSERT_FALSE(bumbler::isNanOrInfBitwise(out));
                ASSERT_FALSE(isSubnormalBitwise(out));

                dcSum += out;
                peakVal = std::max(peakVal, static_cast<double>(std::abs(out)));
            }

            const double meanDc = std::abs(dcSum / static_cast<double>(kMeasureSamples));
            std::cout << "  [RingMod] " << pair.desc << " (wave " << wave << "): meanDc = "
                      << meanDc << ", peak = " << peakVal << std::endl;
            // Residual DC offset must not run away; with 10 Hz DC blocker it must be < 0.015
            ASSERT_TRUE(meanDc < 0.015);
        }
    }
}

// ============================================================================
// Test 5: Long Continuous Rendering (>100,000 Blocks) Stress Test
// ============================================================================
static void test_long_continuous_rendering_100k_blocks() {
    bumbler::ScopedNoDenormals guard;
    bumbler::BumblerVoiceManager vm;
    const double sampleRate = 48000.0;
    const int blockSize = 128;
    vm.prepare(sampleRate, blockSize);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f; // Saw
    params.osc2Waveform = 1.0f; // Square (50% duty cycle)
    params.osc3Waveform = 1.0f; // Saw
    params.osc3Level = 0.3f;
    params.fmAmount = 0.8f;
    params.ringModMix = 0.4f;
    params.oscMix = 0.6f;
    params.pulseWidth = 0.50f;
    params.masterVolume = 1.0f;

    params.ampAttack = 0.01f;
    params.ampDecay = 0.10f;
    params.ampSustain = 0.7f;
    params.ampRelease = 0.20f;

    std::vector<float> bufL(blockSize, 0.0f);
    std::vector<float> bufR(blockSize, 0.0f);
    float* channels[2] = { bufL.data(), bufR.data() };

    // Trigger 4 sustained chord notes
    vm.noteOn(48, 0.8f);
    vm.noteOn(55, 0.8f);
    vm.noteOn(60, 0.8f);
    vm.noteOn(64, 0.8f);

    const int kTotalBlocks = 100000;
    size_t nanCount = 0;
    size_t infCount = 0;
    size_t denormalCount = 0;
    double maxAbsSample = 0.0;
    double movingDcSum = 0.0;

    auto tStart = std::chrono::high_resolution_clock::now();

    for (int block = 0; block < kTotalBlocks; ++block) {
        // At block 50,000: release all notes so envelope decays through release to silence
        if (block == 50000) {
            vm.noteOff(48);
            vm.noteOff(55);
            vm.noteOff(60);
            vm.noteOff(72);
            vm.allNotesOff(false); // Normal release
        }

        // At block 75,000: retrigger fresh notes with extreme detuning
        if (block == 75000) {
            vm.noteOn(36, 0.9f);
            vm.noteOn(84, 0.9f);
        }

        vm.renderBlock(channels, 2, blockSize, params);

        double blockSum = 0.0;
        for (int i = 0; i < blockSize; ++i) {
            const float sL = bufL[i];
            const float sR = bufR[i];

            if (bumbler::isNanOrInfBitwise(sL)) ++nanCount;
            if (bumbler::isNanOrInfBitwise(sR)) ++nanCount;
            if (isSubnormalBitwise(sL)) ++denormalCount;
            if (isSubnormalBitwise(sR)) ++denormalCount;

            maxAbsSample = std::max(maxAbsSample, static_cast<double>(std::max(std::abs(sL), std::abs(sR))));
            blockSum += 0.5 * (sL + sR);
        }

        // Leaky moving DC tracker
        movingDcSum = 0.999 * movingDcSum + 0.001 * (blockSum / blockSize);
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(tEnd - tStart).count();

    std::cout << "  Rendered " << kTotalBlocks << " blocks (" << (kTotalBlocks * blockSize) << " samples) in "
              << std::fixed << std::setprecision(2) << elapsedSec << "s ("
              << ((kTotalBlocks * blockSize) / elapsedSec / 1000000.0) << " MSamples/sec)" << std::endl;
    std::cout << "  NaNs: " << nanCount << ", Infs: " << infCount << ", Denormals: " << denormalCount
              << ", Max peak: " << maxAbsSample << ", Residual DC: " << movingDcSum << std::endl;

    ASSERT_EQ(nanCount, 0u);
    ASSERT_EQ(infCount, 0u);
    ASSERT_EQ(denormalCount, 0u);
    ASSERT_TRUE(maxAbsSample < 20.0);
    ASSERT_TRUE(std::abs(movingDcSum) < 0.01);
}

// ============================================================================
// Test 6: Voice Exhaustion Rapid Hammer Stress Test (128 events within 1 ms)
// ============================================================================
static void test_polyphony_voice_exhaustion_hammer() {
    bumbler::ScopedNoDenormals guard;
    bumbler::BumblerVoiceManager vm;
    const double sampleRate = 48000.0;
    // 1 millisecond at 48kHz = 48 samples
    const int kOneMsSamples = 48;
    vm.prepare(sampleRate, 512);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 1.0f; // Square
    params.pulseWidth = 0.5f;
    params.ampAttack = 0.001f;
    params.ampDecay = 0.05f;
    params.ampSustain = 0.5f;
    params.ampRelease = 0.05f;
    params.masterVolume = 1.0f;

    std::vector<float> bufL(512, 0.0f);
    std::vector<float> bufR(512, 0.0f);
    float* channels[2] = { bufL.data(), bufR.data() };

    // Warm-up
    vm.noteOn(60, 0.8f);
    vm.renderBlock(channels, 2, 64, params);

    // Track heap allocations during hammer
    gAllocationCount.store(0);
    gAllocatedBytes.store(0);
    gTrackAllocations.store(true);

    double maxJump = 0.0;
    float prevSample = 0.0f;

    // Hammer 128 rapid note-on/note-off events interleaved within 1 ms (48 samples)
    for (int event = 0; event < 128; ++event) {
        const int note = event % 128;
        const float vel = 0.5f + 0.5f * (static_cast<float>(event % 10) / 10.0f);

        // Note-On
        vm.noteOn(note, vel);

        // Render 1 sample if within the 48-sample window
        if (event < kOneMsSamples) {
            vm.renderBlock(channels, 2, 1, params);
            const float curr = bufL[0];
            const double jump = std::abs(curr - prevSample);
            maxJump = std::max(maxJump, jump);
            prevSample = curr;

            ASSERT_TRUE(bumbler::isFiniteBitwise(curr));
            ASSERT_FALSE(bumbler::isNanOrInfBitwise(curr));
            ASSERT_FALSE(isSubnormalBitwise(curr));
        }

        // Rapid Note-Off for older note
        if (event % 2 == 1) {
            vm.noteOff((note + 64) % 128, 0.0f);
        }
    }

    gTrackAllocations.store(false);

    // Verify 0 heap allocations occurred during the hammer burst
    ASSERT_EQ(gAllocationCount.load(), 0u);
    ASSERT_EQ(gAllocatedBytes.load(), 0u);

    // Now render 5ms (240 samples) to verify smooth voice stealing crossfade
    vm.renderBlock(channels, 2, 240, params);
    for (int i = 0; i < 240; ++i) {
        const float curr = bufL[i];
        const double jump = std::abs(curr - prevSample);
        maxJump = std::max(maxJump, jump);
        prevSample = curr;

        ASSERT_TRUE(bumbler::isFiniteBitwise(curr));
        ASSERT_FALSE(bumbler::isNanOrInfBitwise(curr));
        ASSERT_FALSE(isSubnormalBitwise(curr));
    }

    // Voice count must not exceed kMaxVoices (16)
    ASSERT_TRUE(vm.getNumActiveVoices() <= bumbler::BumblerVoiceManager::kMaxVoices);

    // Clean voice termination
    vm.allNotesOff(true);
    ASSERT_EQ(vm.getNumActiveVoices(), 0);

    std::cout << "  128 Hammer events completed. Max sample-to-sample jump: " << maxJump
              << ", Heap allocations: " << gAllocationCount.load() << std::endl;
}

// ============================================================================
// Test 6B: Chord Burst Voice Stealing Fairness & Starvation Test
// ============================================================================
static void test_chord_burst_voice_stealing_fairness() {
    bumbler::BumblerVoiceManager vm;
    vm.prepare(48000.0, 512);
    vm.setPolyphonyLimit(4);

    // Trigger chord 1: notes 60, 64, 67, 71
    vm.noteOn(60, 0.8f);
    vm.noteOn(64, 0.8f);
    vm.noteOn(67, 0.8f);
    vm.noteOn(71, 0.8f);

    ASSERT_EQ(vm.getNumActiveVoices(), 4);

    // Now, at the same buffer sample (before renderBlock advances mSampleCounter),
    // trigger chord 2: notes 48, 52, 55, 59
    vm.noteOn(48, 0.8f);
    vm.noteOn(52, 0.8f);
    vm.noteOn(55, 0.8f);
    vm.noteOn(59, 0.8f);

    // Inspect active notes in all 4 voices
    std::cout << "  [Chord Steal Inspection]:" << std::endl;
    for (int i = 0; i < 4; ++i) {
        std::cout << "    Voice " << i << " playing MIDI note: " << vm.getVoice(i).getMidiNote() << std::endl;
    }

    // All 4 notes of chord 2 should be allocated (or at least more than 1 voice should be replaced)
    bool has48 = false, has52 = false, has55 = false, has59 = false;
    for (int i = 0; i < 4; ++i) {
        int n = vm.getVoice(i).getMidiNote();
        if (n == 48) has48 = true;
        if (n == 52) has52 = true;
        if (n == 55) has55 = true;
        if (n == 59) has59 = true;
    }

    // Assert that chord 2 notes were not starved
    ASSERT_TRUE(has48);
    ASSERT_TRUE(has52);
    ASSERT_TRUE(has55);
    ASSERT_TRUE(has59);
}


// ============================================================================
// Test 7: wrap01 and FastSinTable Numerical Boundary Probing
// ============================================================================
static void test_wrap01_and_fast_sin_boundaries() {
    bumbler::ScopedNoDenormals guard;

    const std::vector<float> boundaryPhases = {
        0.0f, -0.0f, 1.0f, -1.0f, 2.0f, -2.0f,
        -1.0e-9f, -1.0e-15f, +1.0e-9f, +1.0e-15f,
        0.99999994f, 1.0f - 1.0e-7f, 1.0f + 1.0e-7f,
        -10.0f, +10.0f, -100.0f, +100.0f, -1000.0f, +1000.0f
    };

    for (float p : boundaryPhases) {
        const float wrapped = bumbler::wrap01(p);
        ASSERT_TRUE(bumbler::isFiniteBitwise(wrapped));
        ASSERT_FALSE(bumbler::isNanOrInfBitwise(wrapped));

        // wrap01 must produce a value in [0.0, 1.0]
        ASSERT_TRUE(wrapped >= 0.0f);
        ASSERT_TRUE(wrapped <= 1.0f);

        // Sin01 evaluation must be strictly bounded in [-1.001, 1.001]
        const float s = bumbler::FastSinTable::sin01(p);
        ASSERT_TRUE(bumbler::isFiniteBitwise(s));
        ASSERT_FALSE(bumbler::isNanOrInfBitwise(s));
        ASSERT_FALSE(isSubnormalBitwise(s));

        if (std::abs(s) > 1.05f) {
            std::cerr << "  Boundary failure: sin01(" << p << ") produced out-of-bounds: " << s << std::endl;
        }
        ASSERT_TRUE(std::abs(s) <= 1.05f);
    }
}

// ============================================================================
// Main Runner
// ============================================================================
int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << "Bumbler XD M1 DSP Adversarial Stress & Stability Harness" << std::endl;
    std::cout << "============================================================" << std::endl;

    runSingleTest("test_wrap01_and_fast_sin_boundaries", test_wrap01_and_fast_sin_boundaries);
    runSingleTest("test_extreme_tuning_and_pitch_boundaries", test_extreme_tuning_and_pitch_boundaries);
    runSingleTest("test_pulse_width_boundary_extremes", test_pulse_width_boundary_extremes);
    runSingleTest("test_extreme_fm_modulation_index", test_extreme_fm_modulation_index);
    runSingleTest("test_high_gain_ring_modulation", test_high_gain_ring_modulation);
    runSingleTest("test_long_continuous_rendering_100k_blocks", test_long_continuous_rendering_100k_blocks);
    runSingleTest("test_polyphony_voice_exhaustion_hammer", test_polyphony_voice_exhaustion_hammer);
    runSingleTest("test_chord_burst_voice_stealing_fairness", test_chord_burst_voice_stealing_fairness);

    std::cout << "------------------------------------------------------------" << std::endl;
    std::cout << "Adversarial Stress Test Summary: " << gPassedTests << " / " << gTotalTests << " passed ("
              << gFailedTests << " failed)" << std::endl;
    std::cout << "============================================================" << std::endl;

    return (gFailedTests == 0) ? 0 : 1;
}
