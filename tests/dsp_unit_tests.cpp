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
// Lightweight Headless Test Harness
// ============================================================================

static int gTotalTests = 0;
static int gPassedTests = 0;
static int gFailedTests = 0;

static void runSingleTest(const char* testName, void (*testFunc)()) {
    ++gTotalTests;
    std::cout << "[RUN     ] " << testName << std::endl;
    try {
        testFunc();
        ++gPassedTests;
        std::cout << "[      OK] " << testName << std::endl;
    } catch (const std::exception& e) {
        ++gFailedTests;
        std::cerr << "[  FAILED ] " << testName << " -> " << e.what() << std::endl;
    } catch (...) {
        ++gFailedTests;
        std::cerr << "[  FAILED ] " << testName << " -> Unknown exception" << std::endl;
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

// ============================================================================
// DSP Mathematical Verification Helpers
// ============================================================================

// Compute Discrete Fourier single-bin magnitude at target frequency f_target
static float computeFourierMagnitude(const float* buffer, int numSamples, double sampleRate, double fTarget) {
    double realSum = 0.0;
    double imagSum = 0.0;
    const double omega = 2.0 * 3.14159265358979323846 * fTarget / sampleRate;

    for (int n = 0; n < numSamples; ++n) {
        const double angle = omega * n;
        realSum += buffer[n] * std::cos(angle);
        imagSum += buffer[n] * std::sin(angle);
    }
    return static_cast<float>((2.0 / numSamples) * std::sqrt(realSum * realSum + imagSum * imagSum));
}

// ============================================================================
// Test Suite 1: Real-time Memory Safety & Zero Dynamic Allocation
// ============================================================================

static void test_realtime_zero_heap_allocation() {
    bumbler::BumblerVoiceManager vm;
    vm.prepare(48000.0, 512);

    bumbler::ParameterSnapshot params;
    params.ampAttack = 0.001f;
    params.ampRelease = 0.010f;

    std::vector<float> bufL(512, 0.0f);
    std::vector<float> bufR(512, 0.0f);
    float* channels[2] = { bufL.data(), bufR.data() };

    // Warm-up iteration
    vm.noteOn(60, 0.8f);
    vm.renderBlock(channels, 2, 512, params);

    // Arm allocation interception
    gAllocationCount.store(0);
    gAllocatedBytes.store(0);
    gTrackAllocations.store(true);

    for (int block = 0; block < 100; ++block) {
        if (block % 3 == 0) vm.noteOn(60 + (block % 16), 0.75f);
        if (block % 5 == 0) vm.noteOff(60 + (block % 16));
        vm.renderBlock(channels, 2, 512, params);
    }

    gTrackAllocations.store(false);

    ASSERT_EQ(gAllocationCount.load(), 0u);
    ASSERT_EQ(gAllocatedBytes.load(), 0u);
}

// ============================================================================
// Test Suite 2: Polyphonic Voice Allocation, Clamping & Same-Pitch Retrigger
// ============================================================================

static void test_voice_allocation_and_limits() {
    bumbler::BumblerVoiceManager vm;
    vm.prepare(48000.0, 512);
    vm.setPolyphonyLimit(4);

    ASSERT_EQ(vm.getNumActiveVoices(), 0);

    // Play 4 notes
    vm.noteOn(60, 0.8f);
    vm.noteOn(64, 0.8f);
    vm.noteOn(67, 0.8f);
    vm.noteOn(71, 0.8f);
    ASSERT_EQ(vm.getNumActiveVoices(), 4);

    // 5th note exceeds limit of 4 -> must steal one voice and remain at 4
    vm.noteOn(72, 0.8f);
    ASSERT_EQ(vm.getNumActiveVoices(), 4);

    // Same-pitch re-triggering -> should not increase voice count
    vm.noteOn(72, 0.9f);
    ASSERT_EQ(vm.getNumActiveVoices(), 4);

    // Note Off
    vm.noteOff(72);
    // Envelope is releasing, voice is still active until tail completes
    ASSERT_TRUE(vm.getNumActiveVoices() >= 1);

    // Fast kill
    vm.allNotesOff(true);
    ASSERT_EQ(vm.getNumActiveVoices(), 0);
}

// ============================================================================
// Test Suite 3: LRU Voice Stealing & 5ms Hann De-Click Crossfading
// ============================================================================

static void test_lru_voice_stealing_and_hann_declick() {
    bumbler::BumblerVoiceManager vm;
    vm.prepare(48000.0, 512);
    vm.setPolyphonyLimit(2);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 2.0f; // Sine
    params.ampAttack = 0.001f;
    params.ampDecay = 1.0f;
    params.ampSustain = 1.0f;
    params.ampRelease = 1.0f;

    std::vector<float> bufL(512, 0.0f);
    std::vector<float> bufR(512, 0.0f);
    float* channels[2] = { bufL.data(), bufR.data() };

    // Start 2 loud sustained notes
    vm.noteOn(60, 1.0f);
    vm.noteOn(64, 1.0f);
    vm.renderBlock(channels, 2, 512, params);

    // Check last sample before steal
    const float lastSampleBeforeSteal = bufL[511];
    ASSERT_TRUE(std::abs(lastSampleBeforeSteal) > 0.1f);

    // Force steal by triggering 3rd note on 1-sample block
    vm.noteOn(67, 1.0f);
    vm.renderBlock(channels, 2, 1, params);
    const float firstSampleAfterSteal = bufL[0];

    // Assert C0 continuity: step jump must be smooth and bounded (< 0.05)
    ASSERT_NEAR(firstSampleAfterSteal, lastSampleBeforeSteal, 0.05f);

    // Render through 5ms fade window (240 samples @ 48kHz)
    vm.renderBlock(channels, 2, 240, params);
    for (int i = 0; i < 240; ++i) {
        ASSERT_TRUE(bumbler::isFiniteBitwise(bufL[i]));
    }
}

// ============================================================================
// Test Suite 4: Waveform Spectral Purity & Fundamental Frequencies
// ============================================================================

static void test_waveform_spectral_purity_sine() {
    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 2.0f; // Sine
    params.oscMix = 0.0f;       // 100% OSC 1
    params.ampAttack = 0.001f;
    params.ampSustain = 1.0f;

    // Trigger A4 (MIDI 69 = 440 Hz)
    voice.noteOn(69, 1.0f, 0);

    const int kNumSamples = 48000;
    std::vector<float> outBuffer(kNumSamples, 0.0f);
    for (int i = 0; i < kNumSamples; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
        outBuffer[i] = l;
    }

    // Fundamental energy at 440 Hz
    const float fund = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 440.0);
    ASSERT_NEAR(fund, 1.0f, 0.05f);

    // 2nd Harmonic (880 Hz) suppression
    const float h2 = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 880.0);
    ASSERT_TRUE(h2 < 0.01f);

    // 3rd Harmonic (1320 Hz) suppression
    const float h3 = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 1320.0);
    ASSERT_TRUE(h3 < 0.01f);
}

static void test_waveform_spectral_series_sawtooth() {
    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f; // Sawtooth
    params.oscMix = 0.0f;
    params.ampAttack = 0.001f;
    params.ampSustain = 1.0f;

    voice.noteOn(69, 1.0f, 0); // 440 Hz

    const int kNumSamples = 48000;
    std::vector<float> outBuffer(kNumSamples, 0.0f);
    for (int i = 0; i < kNumSamples; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
        outBuffer[i] = l;
    }

    const float h1 = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 440.0);
    const float h2 = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 880.0);
    const float h3 = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 1320.0);

    // Theoretical fundamental amplitude of bipolar [-1, 1] saw is 2/pi (~0.637)
    ASSERT_NEAR(h1, static_cast<float>(2.0 / 3.141592653589793), 0.05f);
    // Relative harmonic roll-off series: H2/H1 = 1/2, H3/H1 = 1/3
    ASSERT_NEAR(h2 / h1, 0.50f, 0.05f);
    ASSERT_NEAR(h3 / h1, 0.333f, 0.05f);
}

static void test_waveform_spectral_series_square() {
    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 1.0f; // Square
    params.pulseWidth = 0.5f;   // 50% symmetrical
    params.oscMix = 0.0f;
    params.ampAttack = 0.001f;
    params.ampSustain = 1.0f;

    voice.noteOn(69, 1.0f, 0); // 440 Hz

    const int kNumSamples = 48000;
    std::vector<float> outBuffer(kNumSamples, 0.0f);
    for (int i = 0; i < kNumSamples; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
        outBuffer[i] = l;
    }

    const float h1 = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 440.0);
    const float h2 = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 880.0);
    const float h3 = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 1320.0);

    // Theoretical fundamental amplitude of bipolar [-1, 1] square is 4/pi (~1.273)
    ASSERT_NEAR(h1, static_cast<float>(4.0 / 3.141592653589793), 0.05f);
    ASSERT_TRUE(h2 < 0.03f);       // Even harmonic suppressed > 30 dB
    // Odd harmonic roll-off series: H3/H1 = 1/3
    ASSERT_NEAR(h3 / h1, 0.333f, 0.05f);
}

// ============================================================================
// Test Suite 5: Ring Modulation Sum & Difference Frequencies
// ============================================================================

static void test_ring_modulation_sum_and_diff() {
    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 2.0f; // Sine
    params.osc2Waveform = 2.0f; // Sine
    params.ringModMix = 1.0f;   // 100% Ring Mod
    params.ampAttack = 0.001f;
    params.ampSustain = 1.0f;

    // OSC 1 = 440 Hz (Note 69), OSC 2 coarse = -1 octave (220 Hz)
    params.osc2Octave = -1.0f;
    voice.noteOn(69, 1.0f, 0);

    const int kNumSamples = 48000;
    std::vector<float> outBuffer(kNumSamples, 0.0f);
    for (int i = 0; i < kNumSamples; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
        outBuffer[i] = l;
    }

    // Difference frequency: 440 - 220 = 220 Hz
    // Sum frequency: 440 + 220 = 660 Hz
    const float fDiff = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 220.0);
    const float fSum  = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 660.0);

    ASSERT_TRUE(fDiff > 0.30f);
    ASSERT_TRUE(fSum > 0.30f);

    // Mean DC offset must be negligible (zero DC runaway)
    double dcSum = 0.0;
    for (int i = 1000; i < kNumSamples; ++i) dcSum += outBuffer[i];
    ASSERT_NEAR(static_cast<float>(dcSum / (kNumSamples - 1000)), 0.0f, 0.005f);
}

// ============================================================================
// Test Suite 6: Frequency Modulation (FM Bessel Sidebands)
// ============================================================================

static void test_frequency_modulation_bessel_sidebands() {
    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 2.0f; // Sine modulator
    params.osc2Waveform = 2.0f; // Sine carrier
    params.oscMix = 1.0f;       // Listen to OSC 2
    params.ampAttack = 0.001f;
    params.ampSustain = 1.0f;

    // Carrier = 880 Hz (+1 oct on Note 69), Modulator = 440 Hz
    params.osc2Octave = 1.0f;
    params.fmAmount = 0.5f;

    voice.noteOn(69, 1.0f, 0);

    const int kNumSamples = 48000;
    std::vector<float> outBuffer(kNumSamples, 0.0f);
    for (int i = 0; i < kNumSamples; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
        outBuffer[i] = l;
    }

    // Sidebands expected at fc +/- fm (880 +/- 440 = 440 Hz and 1320 Hz)
    const float sbLower = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 440.0);
    const float sbUpper = computeFourierMagnitude(outBuffer.data(), kNumSamples, 48000.0, 1320.0);

    ASSERT_TRUE(sbLower > 0.15f);
    ASSERT_TRUE(sbUpper > 0.15f);
}

// ============================================================================
// Test Suite 7: Pulse Width Duty Cycle Modulation
// ============================================================================

static void test_pulse_width_duty_cycle() {
    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 1.0f; // Square
    params.pulseWidth = 0.25f;  // 25% duty cycle
    params.oscMix = 0.0f;
    params.ampAttack = 0.001f;
    params.ampSustain = 1.0f;

    voice.noteOn(69, 1.0f, 0); // 440 Hz

    const int kNumSamples = 48000;
    int positiveCount = 0;
    for (int i = 0; i < kNumSamples; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
        if (l > 0.0f) ++positiveCount;
    }

    const float measuredDuty = static_cast<float>(positiveCount) / static_cast<float>(kNumSamples);
    ASSERT_NEAR(measuredDuty, 0.25f, 0.03f);
}

// ============================================================================
// Test Suite 8: Multi-Rate & Variable Block Size Stability
// ============================================================================

static void test_multirate_and_block_sizes() {
    const std::vector<double> rates = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const std::vector<int> blockSizes = { 1, 16, 64, 128, 256, 512, 1024, 2048 };

    bumbler::ParameterSnapshot params;

    for (double sr : rates) {
        for (int bs : blockSizes) {
            bumbler::BumblerVoiceManager vm;
            vm.prepare(sr, bs);
            vm.noteOn(60, 0.8f);

            std::vector<float> bufL(bs, 0.0f);
            std::vector<float> bufR(bs, 0.0f);
            float* channels[2] = { bufL.data(), bufR.data() };

            for (int iter = 0; iter < 10; ++iter) {
                vm.renderBlock(channels, 2, bs, params);
                for (int s = 0; s < bs; ++s) {
                    ASSERT_TRUE(bumbler::isFiniteBitwise(bufL[s]));
                    ASSERT_TRUE(bumbler::isFiniteBitwise(bufR[s]));
                }
            }
        }
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << "Bumbler XD M1 DSP Headless Unit Test Suite" << std::endl;
    std::cout << "============================================================" << std::endl;

    runSingleTest("test_realtime_zero_heap_allocation", test_realtime_zero_heap_allocation);
    runSingleTest("test_voice_allocation_and_limits", test_voice_allocation_and_limits);
    runSingleTest("test_lru_voice_stealing_and_hann_declick", test_lru_voice_stealing_and_hann_declick);
    runSingleTest("test_waveform_spectral_purity_sine", test_waveform_spectral_purity_sine);
    runSingleTest("test_waveform_spectral_series_sawtooth", test_waveform_spectral_series_sawtooth);
    runSingleTest("test_waveform_spectral_series_square", test_waveform_spectral_series_square);
    runSingleTest("test_ring_modulation_sum_and_diff", test_ring_modulation_sum_and_diff);
    runSingleTest("test_frequency_modulation_bessel_sidebands", test_frequency_modulation_bessel_sidebands);
    runSingleTest("test_pulse_width_duty_cycle", test_pulse_width_duty_cycle);
    runSingleTest("test_multirate_and_block_sizes", test_multirate_and_block_sizes);

    std::cout << "------------------------------------------------------------" << std::endl;
    std::cout << "Test Summary: " << gPassedTests << " / " << gTotalTests << " passed ("
              << gFailedTests << " failed)" << std::endl;
    std::cout << "============================================================" << std::endl;

    return (gFailedTests == 0) ? 0 : 1;
}
