#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <string>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "BumblerVoice.h"
#include "BumblerVoiceManager.h"
#include "WaspFilter.h"

// ============================================================================
// Real-Time Memory Safety: Interception Hooks
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
// Test Framework Macros
// ============================================================================
static int gTotalTests = 0;
static int gPassedTests = 0;
static int gFailedTests = 0;

#define AUDITOR_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [AUDITOR FAILURE] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        throw std::runtime_error(std::string("Assertion failed: ") + #cond + " -> " + msg); \
    } \
} while (false)

static void runAuditorTest(const char* name, void (*fn)()) {
    ++gTotalTests;
    std::cout << "\n>>> [AUDITOR RUNNING] " << name << "..." << std::endl;
    try {
        fn();
        ++gPassedTests;
        std::cout << ">>> [AUDITOR PASSED ] " << name << std::endl;
    } catch (const std::exception& e) {
        ++gFailedTests;
        std::cerr << ">>> [AUDITOR FAILED ] " << name << " : " << e.what() << std::endl;
    } catch (...) {
        ++gFailedTests;
        std::cerr << ">>> [AUDITOR FAILED ] " << name << " : Unknown exception" << std::endl;
    }
}

// ============================================================================
// DSP Mathematical Measurement Utilities
// ============================================================================
static double measureGainAtFreq(
    bumbler::WaspFilter& filter,
    double testFreqHz,
    double cutoffHz,
    double reso,
    bumbler::WaspFilterMode mode,
    double sampleRate = 48000.0,
    int numSamples = 6000
) {
    filter.reset();
    const double phaseInc = 2.0 * bumbler::kPi * testFreqHz / sampleRate;
    constexpr int kDiscard = 3000;
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        float in = static_cast<float>(std::sin(phaseInc * i));
        float out = filter.processSample(in, cutoffHz, reso, mode);
        if (i >= kDiscard) {
            float a = std::abs(out);
            if (a > peak) peak = a;
        }
    }
    return static_cast<double>(peak);
}

static double toDb(double linear) {
    if (linear <= 1.0e-12) return -240.0;
    return 20.0 * std::log10(linear);
}

static double computeFourierBinPower(const float* buffer, int numSamples, double targetFreqHz, double sampleRate) {
    if (numSamples <= 0 || sampleRate <= 0.0) return 0.0;
    double realSum = 0.0;
    double imagSum = 0.0;
    const double omega = 2.0 * bumbler::kPi * targetFreqHz / sampleRate;

    for (int n = 0; n < numSamples; ++n) {
        const double angle = omega * n;
        realSum += buffer[n] * std::cos(angle);
        imagSum -= buffer[n] * std::sin(angle);
    }
    const double mag = (2.0 / numSamples) * std::sqrt(realSum * realSum + imagSum * imagSum);
    return mag * mag;
}

// ============================================================================
// Test 1: Arbitrary Unseen Frequencies & Dynamic S-Plane Mapping
// Verifies that filter response is computed analytically via ZDF s-plane equations,
// not via hardcoded test table entries or canned constants.
// ============================================================================
static void test_zdf_arbitrary_unseen_frequencies() {
    bumbler::WaspFilter filter;
    filter.prepare(48000.0);

    // Arbitrary prime cutoffs and test frequencies not present in any standard test
    const double fc = 1337.0;
    const double reso = 0.25;

    // Check LP12: 300 Hz (passband) vs 2674 Hz (+1 oct) vs 5348 Hz (+2 oct)
    double gPass = measureGainAtFreq(filter, 300.0, fc, reso, bumbler::WaspFilterMode::LP12);
    double g1Oct = measureGainAtFreq(filter, 2674.0, fc, reso, bumbler::WaspFilterMode::LP12);
    double g2Oct = measureGainAtFreq(filter, 5348.0, fc, reso, bumbler::WaspFilterMode::LP12);

    double dbPass = toDb(gPass);
    double db1Oct = toDb(g1Oct);
    double db2Oct = toDb(g2Oct);

    std::cout << "  LP12 at arbitrary fc=1337Hz: pass=" << dbPass << " dB, +1oct=" << db1Oct 
              << " dB, +2oct=" << db2Oct << " dB\n";

    AUDITOR_ASSERT(std::abs(dbPass) < 1.0, "LP12 passband gain deviates at arbitrary frequency");
    double slope1 = db1Oct - dbPass;
    AUDITOR_ASSERT(slope1 <= -9.5 && slope1 >= -14.5, "LP12 1-octave slope invalid at arbitrary frequency");
    double slope2 = db2Oct - dbPass;
    AUDITOR_ASSERT(slope2 <= -20.0 && slope2 >= -27.0, "LP12 2-octave slope invalid at arbitrary frequency");

    // Check LP24 at fc=751 Hz: 1502 Hz (+1 oct) should be ~24 dB down
    const double fc2 = 751.0;
    double gPass24 = measureGainAtFreq(filter, 150.0, fc2, 0.0, bumbler::WaspFilterMode::LP24);
    double g1Oct24 = measureGainAtFreq(filter, 1502.0, fc2, 0.0, bumbler::WaspFilterMode::LP24);
    double slope24 = toDb(g1Oct24) - toDb(gPass24);
    std::cout << "  LP24 at arbitrary fc=751Hz: 1-oct slope=" << slope24 << " dB\n";
    AUDITOR_ASSERT(slope24 <= -19.5 && slope24 >= -28.0, "LP24 slope invalid at arbitrary frequency");
}

// ============================================================================
// Test 2: CMOS Non-Linear Overdrive Harmonic Distortion in LP24
// Verifies that tanh(1.2 * lp1) / 1.2 is authentically running and producing
// odd harmonic distortion at high signal amplitudes.
// ============================================================================
static void test_cmos_nonlinear_saturation_lp24() {
    bumbler::WaspFilter filter;
    filter.prepare(48000.0);

    const double fc = 2000.0;
    const double reso = 0.1;
    const double testFreq = 300.0;
    constexpr int N = 8192;
    const double phaseInc = 2.0 * bumbler::kPi * testFreq / 48000.0;

    // Small input signal (linear regime, amplitude 0.05)
    filter.reset();
    std::vector<float> smallOut(N);
    for (int i = 0; i < N; ++i) {
        float in = 0.05f * static_cast<float>(std::sin(phaseInc * i));
        smallOut[i] = filter.processSample(in, fc, reso, bumbler::WaspFilterMode::LP24);
    }
    double fundPowerSmall = computeFourierBinPower(smallOut.data() + 2048, N - 2048, 300.0, 48000.0);
    double h3PowerSmall   = computeFourierBinPower(smallOut.data() + 2048, N - 2048, 900.0, 48000.0);
    double thdSmall = h3PowerSmall / fundPowerSmall;

    // Large input signal (saturation regime, amplitude 1.5)
    filter.reset();
    std::vector<float> largeOut(N);
    for (int i = 0; i < N; ++i) {
        float in = 1.5f * static_cast<float>(std::sin(phaseInc * i));
        largeOut[i] = filter.processSample(in, fc, reso, bumbler::WaspFilterMode::LP24);
    }
    double fundPowerLarge = computeFourierBinPower(largeOut.data() + 2048, N - 2048, 300.0, 48000.0);
    double h3PowerLarge   = computeFourierBinPower(largeOut.data() + 2048, N - 2048, 900.0, 48000.0);
    double thdLarge = h3PowerLarge / fundPowerLarge;

    std::cout << "  LP24 Small Signal (0.05) H3 THD: " << toDb(std::sqrt(thdSmall)) << " dB\n";
    std::cout << "  LP24 Large Signal (1.50) H3 THD: " << toDb(std::sqrt(thdLarge)) << " dB\n";

    // In linear regime, 3rd harmonic is negligible (< -46 dB)
    AUDITOR_ASSERT(thdSmall < 2.5e-5, "LP24 has unexpected distortion on small signal");
    // In saturating regime, 3rd harmonic is substantial (> -30 dB)
    AUDITOR_ASSERT(thdLarge > 1.0e-3, "LP24 lacks non-linear CMOS saturation harmonics on large signal");
}

// ============================================================================
// Test 3: Double Notch (DBL_NT) Exact Spacing & Intermediate Peak
// Verifies that DBL_NT has two distinct attenuation nulls exactly at fc / sqrt(2)
// and fc * sqrt(2), and passband recovery in between.
// ============================================================================
static void test_twin_notch_spacing_and_symmetry() {
    bumbler::WaspFilter filter;
    filter.prepare(48000.0);

    const double fc = 1200.0;
    const double expectedF1 = fc * 0.7071067811865475; // ~848.5 Hz
    const double expectedF2 = fc * 1.4142135623730951; // ~1697.0 Hz

    // Evaluate response at 100 frequencies from 500 Hz to 2500 Hz
    constexpr int kSteps = 100;
    std::vector<double> freqs(kSteps);
    std::vector<double> dbs(kSteps);

    const double fStart = 500.0;
    const double fEnd = 2500.0;
    for (int i = 0; i < kSteps; ++i) {
        double f = fStart + (fEnd - fStart) * i / (kSteps - 1);
        freqs[i] = f;
        double g = measureGainAtFreq(filter, f, fc, 0.6, bumbler::WaspFilterMode::DBL_NT, 48000.0, 5000);
        dbs[i] = toDb(g);
    }

    // Find local minima
    std::vector<size_t> minima;
    for (size_t i = 1; i < kSteps - 1; ++i) {
        if (dbs[i] < dbs[i - 1] && dbs[i] < dbs[i + 1]) {
            minima.push_back(i);
        }
    }

    AUDITOR_ASSERT(minima.size() >= 2, "DBL_NT failed to exhibit at least 2 local minima");
    size_t min1 = minima[0];
    size_t min2 = minima[1];

    std::cout << "  DBL_NT Null 1 measured: " << freqs[min1] << " Hz (Expected ~" << expectedF1 
              << " Hz, Depth: " << dbs[min1] << " dB)\n";
    std::cout << "  DBL_NT Null 2 measured: " << freqs[min2] << " Hz (Expected ~" << expectedF2 
              << " Hz, Depth: " << dbs[min2] << " dB)\n";

    AUDITOR_ASSERT(std::abs(freqs[min1] - expectedF1) < 60.0, "DBL_NT null 1 frequency misaligned");
    AUDITOR_ASSERT(std::abs(freqs[min2] - expectedF2) < 80.0, "DBL_NT null 2 frequency misaligned");
    AUDITOR_ASSERT(dbs[min1] < -12.0, "DBL_NT null 1 depth insufficient");
    AUDITOR_ASSERT(dbs[min2] < -12.0, "DBL_NT null 2 depth insufficient");

    // Intermediate saddle point between notches should have higher gain than both notches
    double saddleDb = -100.0;
    for (size_t i = min1; i <= min2; ++i) {
        if (dbs[i] > saddleDb) saddleDb = dbs[i];
    }
    AUDITOR_ASSERT(saddleDb > dbs[min1] + 6.0 && saddleDb > dbs[min2] + 6.0, 
        "DBL_NT lacks recovery peak between twin nulls");
}

// ============================================================================
// Test 4: Extreme Cutoff Clamping & Nyquist Singularity Prevention
// Verifies tan(pi * fc / fs) does not divide by zero or overflow when fc -> fs/2.
// ============================================================================
static void test_multirate_and_nyquist_boundary_stability() {
    const std::vector<double> rates = { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0 };
    const std::vector<double> cutoffs = { -100.0, 0.0, 5.0, 20.0, 1000.0, 20000.0, 50000.0, 1000000.0 };
    const std::vector<double> resonances = { -0.5, 0.0, 0.5, 0.99, 1.0, 5.0 };

    bumbler::WaspFilter filter;
    for (double sr : rates) {
        filter.prepare(sr);
        for (double fc : cutoffs) {
            for (double reso : resonances) {
                for (int m = 0; m <= 5; ++m) {
                    auto mode = static_cast<bumbler::WaspFilterMode>(m);
                    // Process short impulse
                    filter.reset();
                    float out1 = filter.processSample(1.0f, fc, reso, mode);
                    float out2 = filter.processSample(0.0f, fc, reso, mode);
                    AUDITOR_ASSERT(!std::isnan(out1) && !std::isinf(out1), "Filter produced NaN/Inf on impulse");
                    AUDITOR_ASSERT(!std::isnan(out2) && !std::isinf(out2), "Filter produced NaN/Inf on impulse decay");
                    AUDITOR_ASSERT(std::abs(out1) <= 12.0f, "Filter impulse output exploded");
                }
            }
        }
    }
}

// ============================================================================
// Test 5: Adversarial State Divergence & Denormal Immunity
// Feeds alternating rail impulses and subnormals to verify state bounds clamping.
// ============================================================================
static void test_adversarial_numerical_stress_and_denormals() {
    bumbler::WaspFilter filter;
    filter.prepare(48000.0);

    // 1. Extreme rail impulse train
    for (int m = 0; m <= 5; ++m) {
        auto mode = static_cast<bumbler::WaspFilterMode>(m);
        filter.reset();
        for (int i = 0; i < 1000; ++i) {
            float in = (i % 2 == 0) ? 50.0f : -50.0f;
            float out = filter.processSample(in, 1000.0, 0.95, mode);
            AUDITOR_ASSERT(bumbler::isFiniteBitwise(out), "Filter blew up on extreme rail impulses");
            AUDITOR_ASSERT(std::abs(out) <= 500.0f, "Filter state escaped soft limiter bounds");
        }
    }

    // 2. Subnormal inputs
    for (int m = 0; m <= 5; ++m) {
        auto mode = static_cast<bumbler::WaspFilterMode>(m);
        filter.reset();
        for (int i = 0; i < 500; ++i) {
            float subnormal = 1.0e-38f;
            float out = filter.processSample(subnormal, 1000.0, 0.5, mode);
            AUDITOR_ASSERT(std::fpclassify(out) != FP_SUBNORMAL, "Filter generated subnormal float");
        }
    }

    // 3. Steady-state decay to absolute zero
    filter.reset();
    filter.processSample(1.0f, 1000.0, 0.5, bumbler::WaspFilterMode::LP24);
    for (int i = 0; i < 20000; ++i) {
        filter.processSample(0.0f, 1000.0, 0.5, bumbler::WaspFilterMode::LP24);
    }
    float zeroCheck = filter.processSample(0.0f, 1000.0, 0.5, bumbler::WaspFilterMode::LP24);
    AUDITOR_ASSERT(std::abs(zeroCheck) == 0.0f, "Filter failed to flush denormals to exact zero");
}

// ============================================================================
// Test 6: processBlock Sub-Blocking Consistency
// Compares block processing against sample-by-sample processing.
// ============================================================================
static void test_process_block_vs_process_sample_equivalence() {
    bumbler::WaspFilter filterSample;
    bumbler::WaspFilter filterBlock;
    filterSample.prepare(48000.0);
    filterBlock.prepare(48000.0);

    constexpr int N = 256;
    std::vector<float> input(N);
    const double phaseInc = 2.0 * bumbler::kPi * 440.0 / 48000.0;
    for (int i = 0; i < N; ++i) {
        input[i] = static_cast<float>(std::sin(phaseInc * i));
    }

    std::vector<float> outSample(N);
    std::vector<float> outBlock(N);

    // Constant parameters: sample and block processing must be mathematically identical
    for (int i = 0; i < N; ++i) {
        outSample[i] = filterSample.processSample(input[i], 1000.0, 0.3, bumbler::WaspFilterMode::LP12);
    }
    filterBlock.processBlock(input.data(), outBlock.data(), N, 1000.0f, 0.3f, bumbler::WaspFilterMode::LP12);

    for (int i = 0; i < N; ++i) {
        AUDITOR_ASSERT(std::abs(outSample[i] - outBlock[i]) < 1.0e-5f, 
            "processBlock deviated from processSample on constant parameters");
    }
}

// ============================================================================
// Test 7: Voice Integration: Dynamic Spectral Attenuation
// Proves that BumblerVoice physically routes through WaspFilter when enabled.
// ============================================================================
static void test_voice_filter_physical_spectral_filtering() {
    bumbler::BumblerVoice voiceUnfiltered;
    bumbler::BumblerVoice voiceFiltered;

    voiceUnfiltered.prepare(48000.0);
    voiceFiltered.prepare(48000.0);

    voiceUnfiltered.setFilterEnabled(false);
    voiceFiltered.setFilterEnabled(true);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 0.0f; // Sawtooth (rich harmonic content)
    params.oscMix = 0.0f;       // 100% OSC 1
    params.ampAttack = 0.001f;
    params.ampSustain = 1.0f;
    params.velToAmp = 0.0f;

    // Filter set to steep LP24 with low cutoff at 300 Hz
    params.filterMode = 1.0f;   // LP24
    params.filterCutoff = 300.0f;
    params.filterResonance = 0.0f;
    params.filterKbTrack = 0.0f;
    params.filterEnvAmount = 0.0f;

    // Play Note 60 (C4 = 261.63 Hz)
    voiceUnfiltered.noteOn(60, 1.0f, 1);
    voiceFiltered.noteOn(60, 1.0f, 1);

    constexpr int kWarmup = 512;
    constexpr int N = 4096;
    for (int i = 0; i < kWarmup; ++i) {
        float l = 0.0f, r = 0.0f;
        voiceUnfiltered.renderSample(l, r, params);
        l = 0.0f; r = 0.0f;
        voiceFiltered.renderSample(l, r, params);
    }

    std::vector<float> bufUnfiltered(N);
    std::vector<float> bufFiltered(N);
    for (int i = 0; i < N; ++i) {
        float l1 = 0.0f, r1 = 0.0f;
        voiceUnfiltered.renderSample(l1, r1, params);
        bufUnfiltered[i] = l1;

        float l2 = 0.0f, r2 = 0.0f;
        voiceFiltered.renderSample(l2, r2, params);
        bufFiltered[i] = l2;
    }

    // 4th Harmonic: 261.63 * 4 = 1046.5 Hz
    double p4Unfiltered = computeFourierBinPower(bufUnfiltered.data(), N, 1046.5, 48000.0);
    double p4Filtered   = computeFourierBinPower(bufFiltered.data(), N, 1046.5, 48000.0);
    double atten4 = toDb(p4Filtered) - toDb(p4Unfiltered);

    // 8th Harmonic: 261.63 * 8 = 2093.0 Hz
    double p8Unfiltered = computeFourierBinPower(bufUnfiltered.data(), N, 2093.0, 48000.0);
    double p8Filtered   = computeFourierBinPower(bufFiltered.data(), N, 2093.0, 48000.0);
    double atten8 = toDb(p8Filtered) - toDb(p8Unfiltered);

    std::cout << "  Voice 4th Harmonic Attenuation: " << atten4 << " dB (Expected < -20 dB)\n";
    std::cout << "  Voice 8th Harmonic Attenuation: " << atten8 << " dB (Expected < -40 dB)\n";

    AUDITOR_ASSERT(atten4 < -18.0, "Voice filter failed to attenuate 4th harmonic");
    AUDITOR_ASSERT(atten8 < -35.0, "Voice filter failed to attenuate 8th harmonic");
}

// ============================================================================
// Test 8: Voice Filter Modulation (KB Track, Bipolar Env, Link Toggle)
// ============================================================================
static void test_voice_filter_modulation_and_link() {
    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);
    voice.setFilterEnabled(true);

    bumbler::ParameterSnapshot params;
    params.filterCutoff = 1000.0f;
    params.filterMode = 0.0f; // LP12
    params.ampAttack = 0.001f;
    params.ampSustain = 1.0f;
    params.filterAttack = 0.010f; // 10ms
    params.filterSustain = 0.0f;
    params.filterEnvAmount = 1.0f; // Full positive modulation
    params.envLink = 0.0f;

    // Trigger Note 60
    voice.noteOn(60, 1.0f, 1);
    for (int i = 0; i < 480; ++i) { // 10ms = 480 samples
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
    }
    AUDITOR_ASSERT(voice.getFilterEnvelopeLevel() >= 0.90f, "Filter envelope did not reach peak on attack");

    // Test Link: with envLink = 1.0, filter envelope uses amp attack (1ms = 48 samples)
    params.envLink = 1.0f;
    voice.noteOn(60, 1.0f, 2);
    for (int i = 0; i < 48; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
    }
    AUDITOR_ASSERT(voice.getFilterEnvelopeLevel() >= 0.90f, "Linked filter envelope failed to mirror fast amp attack");
}

// ============================================================================
// Test 9: Zero Real-Time Heap Allocations
// ============================================================================
static void test_zero_heap_allocations() {
    bumbler::WaspFilter filter;
    filter.prepare(48000.0);

    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);
    voice.setFilterEnabled(true);

    bumbler::ParameterSnapshot params;
    params.ampAttack = 0.001f;
    params.filterCutoff = 1000.0f;
    params.filterResonance = 0.5f;

    std::vector<float> inBlock(512, 0.5f);
    std::vector<float> outBlock(512, 0.0f);

    // Warm-up
    voice.noteOn(60, 0.8f, 1);
    float l = 0.0f, r = 0.0f;
    voice.renderSample(l, r, params);
    filter.processBlock(inBlock.data(), outBlock.data(), 512, 1000.0f, 0.5f, bumbler::WaspFilterMode::LP24);

    // Arm interceptor
    gAllocationCount.store(0);
    gAllocatedBytes.store(0);
    gTrackAllocations.store(true);

    for (int iter = 0; iter < 100; ++iter) {
        // Sample processing
        for (int i = 0; i < 128; ++i) {
            float s = filter.processSample(0.5f, 1200.0f, 0.7f, bumbler::WaspFilterMode::DBL_NT);
            (void)s;
            voice.renderSample(l, r, params);
        }
        // Block processing
        filter.processBlock(inBlock.data(), outBlock.data(), 512, 1000.0f, 0.5f, bumbler::WaspFilterMode::LP24);
    }

    gTrackAllocations.store(false);

    std::cout << "  Heap Allocations Count during 50k+ filter/voice sample ops: " 
              << gAllocationCount.load() << "\n";
    AUDITOR_ASSERT(gAllocationCount.load() == 0, "Heap allocations detected in audio processing path");
    AUDITOR_ASSERT(gAllocatedBytes.load() == 0, "Heap bytes allocated in audio processing path");
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main() {
    std::cout << "======================================================================\n";
    std::cout << " Bumbler XD : Milestone 2 Forensic Auditor Integrity Verification Suite \n";
    std::cout << "======================================================================\n";

    runAuditorTest("test_zdf_arbitrary_unseen_frequencies", test_zdf_arbitrary_unseen_frequencies);
    runAuditorTest("test_cmos_nonlinear_saturation_lp24", test_cmos_nonlinear_saturation_lp24);
    runAuditorTest("test_twin_notch_spacing_and_symmetry", test_twin_notch_spacing_and_symmetry);
    runAuditorTest("test_multirate_and_nyquist_boundary_stability", test_multirate_and_nyquist_boundary_stability);
    runAuditorTest("test_adversarial_numerical_stress_and_denormals", test_adversarial_numerical_stress_and_denormals);
    runAuditorTest("test_process_block_vs_process_sample_equivalence", test_process_block_vs_process_sample_equivalence);
    runAuditorTest("test_voice_filter_physical_spectral_filtering", test_voice_filter_physical_spectral_filtering);
    runAuditorTest("test_voice_filter_modulation_and_link", test_voice_filter_modulation_and_link);
    runAuditorTest("test_zero_heap_allocations", test_zero_heap_allocations);

    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << " Forensic Auditor Summary: " << gPassedTests << " / " << gTotalTests << " Passed ("
              << gFailedTests << " Failed)\n";
    std::cout << "======================================================================\n";

    return (gFailedTests == 0) ? 0 : 1;
}
