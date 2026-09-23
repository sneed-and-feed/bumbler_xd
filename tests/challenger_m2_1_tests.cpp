#include "test_helpers.h"
#include <iomanip>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <iostream>

#include "WaspFilter.h"
#include "BumblerVoice.h"
#include "ParameterSnapshot.h"

// ============================================================================
// Global Real-Time Heap Allocation Interceptors for Challenger M2-1
// ============================================================================
void* operator new(size_t size) {
    if (gTrackAllocations.load(std::memory_order_relaxed)) {
        gAllocationCount.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytes.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

void* operator new[](size_t size) {
    if (gTrackAllocations.load(std::memory_order_relaxed)) {
        gAllocationCount.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytes.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p, size_t) noexcept {
    std::free(p);
}

// ============================================================================
// SUITE 1: Resonance at Self-Oscillation Limits (Q -> 1.0)
// ============================================================================

/**
 * Probe 1.1: DC Steps under Maximum Resonance (Q -> 1.0)
 * Feeds extreme DC steps into all 6 filter modes with resonance clamped at max (0.95, 0.99, 1.0, 1.5).
 * Validates that states do not explode and output remains finite, denormal-free, and bounded.
 */
void test_resonance_self_oscillation_dc_steps() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    const std::vector<bumbler::WaspFilterMode> modes = {
        bumbler::WaspFilterMode::LP12,
        bumbler::WaspFilterMode::LP24,
        bumbler::WaspFilterMode::LP_NT,
        bumbler::WaspFilterMode::DBL_NT,
        bumbler::WaspFilterMode::BP24,
        bumbler::WaspFilterMode::HP24
    };

    const std::vector<double> resoValues = { 0.95, 0.99, 1.0, 1.5 };
    const std::vector<double> cutoffs = { 60.0, 440.0, 1000.0, 5000.0, 15000.0 };

    for (auto mode : modes) {
        for (double reso : resoValues) {
            for (double fc : cutoffs) {
                bumbler::WaspFilter filter;
                filter.prepare(48000.0);

                // Phase 1: Sudden DC jump from 0.0 to +2.0
                for (int i = 0; i < 500; ++i) {
                    float out = filter.processSample(2.0f, fc, reso, mode);
                    TEST_ASSERT(!std::isnan(out) && !std::isinf(out),
                        "NaN/Inf detected on positive DC step, mode=" + std::to_string(static_cast<int>(mode)));
                    TEST_ASSERT(std::abs(out) < 25.0f,
                        "State explosion on positive DC step: out=" + std::to_string(out));
                }

                // Phase 2: Sudden DC reversal from +2.0 to -2.0
                for (int i = 0; i < 500; ++i) {
                    float out = filter.processSample(-2.0f, fc, reso, mode);
                    TEST_ASSERT(!std::isnan(out) && !std::isinf(out),
                        "NaN/Inf detected on negative DC step, mode=" + std::to_string(static_cast<int>(mode)));
                    TEST_ASSERT(std::abs(out) < 25.0f,
                        "State explosion on negative DC step: out=" + std::to_string(out));
                }

                // Phase 3: Extreme DC jump to +10.0 (high overdrive)
                for (int i = 0; i < 500; ++i) {
                    float out = filter.processSample(10.0f, fc, reso, mode);
                    TEST_ASSERT(!std::isnan(out) && !std::isinf(out),
                        "NaN/Inf detected on +10 DC step, mode=" + std::to_string(static_cast<int>(mode)));
                    TEST_ASSERT(std::abs(out) < 50.0f,
                        "State explosion on +10 DC step: out=" + std::to_string(out));
                }

                // Phase 4: Release back to 0.0 and verify ringing decay
                float earlyPeak = 0.0f;
                float lastOut = 0.0f;
                constexpr int kDecaySamples = 24000; // 0.5s allows low-frequency (60 Hz) poles to decay physically
                for (int i = 0; i < kDecaySamples; ++i) {
                    float out = filter.processSample(0.0f, fc, reso, mode);
                    TEST_ASSERT(!std::isnan(out) && !std::isinf(out),
                        "NaN/Inf detected during DC release decay");
                    TEST_ASSERT(std::fpclassify(out) != FP_SUBNORMAL,
                        "Denormal detected during DC release decay");
                    if (i < 200) {
                        earlyPeak = std::max(earlyPeak, std::abs(out));
                    }
                    lastOut = out;
                }
                // Filter must settle toward zero after 0.5 seconds of zero input
                TEST_ASSERT(std::abs(lastOut) < 0.1f,
                    "Filter failed to settle after DC release: lastOut=" + std::to_string(lastOut)
                    + ", earlyPeak=" + std::to_string(earlyPeak)
                    + ", mode=" + std::to_string(static_cast<int>(mode))
                    + ", fc=" + std::to_string(fc) + ", reso=" + std::to_string(reso));
            }
        }
    }
}

/**
 * Probe 1.2: High-Amplitude Square Waves at Resonant Frequency
 * Drives the filter with continuous high-amplitude square waves (up to 10.0) at the exact
 * cutoff frequency with max resonance to test non-linear state limiting and prevent divergence.
 */
void test_resonance_self_oscillation_high_amplitude_squares() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    const std::vector<bumbler::WaspFilterMode> modes = {
        bumbler::WaspFilterMode::LP12,
        bumbler::WaspFilterMode::LP24,
        bumbler::WaspFilterMode::BP24,
        bumbler::WaspFilterMode::HP24
    };

    const std::vector<double> testFreqs = { 80.0, 440.0, 1000.0, 3500.0, 10000.0 };
    const std::vector<float> amplitudes = { 1.0f, 2.5f, 5.0f, 10.0f };
    constexpr double fs = 48000.0;

    for (auto mode : modes) {
        for (double f : testFreqs) {
            for (float amp : amplitudes) {
                bumbler::WaspFilter filter;
                filter.prepare(fs);

                const double period = fs / f;
                float maxAbsOut = 0.0f;

                // Process 10,000 samples of high-amplitude square wave at resonant cutoff
                for (int i = 0; i < 10000; ++i) {
                    const float sq = (std::fmod(static_cast<double>(i), period) < (period * 0.5)) ? amp : -amp;
                    float out = filter.processSample(sq, f, 0.99, mode);

                    TEST_ASSERT(!std::isnan(out) && !std::isinf(out),
                        "NaN/Inf on resonant square wave, mode=" + std::to_string(static_cast<int>(mode))
                        + ", freq=" + std::to_string(f) + ", amp=" + std::to_string(amp));
                    TEST_ASSERT(std::fpclassify(out) != FP_SUBNORMAL,
                        "Subnormal detected on resonant square wave");

                    maxAbsOut = std::max(maxAbsOut, std::abs(out));
                }

                // Output must be strictly bounded by internal saturation/clamping
                TEST_ASSERT(maxAbsOut < 50.0f,
                    "Output peak exceeded safety bounds under resonant square wave: peak=" + std::to_string(maxAbsOut));
            }
        }
    }
}

/**
 * Probe 1.3: Impulse Response and Ringing Stability at Resonance Limit
 * Injects a single unit impulse (x[0] = 1.0, x[n>0] = 0.0) at max resonance (0.99).
 * Observes ringing across 48,000 samples (1 second) to verify unconditional stability.
 */
void test_resonance_impulse_ringing_stability() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    constexpr double fs = 48000.0;
    constexpr int numSamples = 48000;

    bumbler::WaspFilter filter;
    filter.prepare(fs);

    // Mode LP24 with cutoff 1000 Hz and resonance 0.99
    float impulseOut = filter.processSample(1.0f, 1000.0, 0.99, bumbler::WaspFilterMode::LP24);
    TEST_ASSERT(!std::isnan(impulseOut) && !std::isinf(impulseOut), "NaN/Inf on initial impulse");

    std::vector<float> ringBuffer(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        ringBuffer[i] = filter.processSample(0.0f, 1000.0, 0.99, bumbler::WaspFilterMode::LP24);
        TEST_ASSERT(!std::isnan(ringBuffer[i]) && !std::isinf(ringBuffer[i]),
            "NaN/Inf during resonant ringing at sample " + std::to_string(i));
        TEST_ASSERT(std::fpclassify(ringBuffer[i]) != FP_SUBNORMAL,
            "Denormal during resonant ringing at sample " + std::to_string(i));
    }

    // Verify stability: early ringing RMS vs late ringing RMS
    double earlyRms = bumbler_test::computeRms(ringBuffer.data(), 1000);
    double lateRms = bumbler_test::computeRms(ringBuffer.data() + 40000, 1000);

    TEST_ASSERT(earlyRms > 0.01, "Filter failed to resonate following impulse");
    TEST_ASSERT(lateRms < earlyRms, "Filter impulse ringing energy failed to decay (unstable/divergent)");
    TEST_ASSERT(lateRms < 0.01, "Filter failed to damp out after 40k samples: lateRms=" + std::to_string(lateRms));
}

/**
 * Probe 1.4: LP24 Analog CMOS Tanh Saturation Behavior
 * Compares stage 1 vs stage 2 harmonic distortion under overdrive.
 * At high resonance and overdrive, tanh(1.2 * lp1) / 1.2 introduces odd harmonics (analog warmth)
 * while strictly bounding inter-stage drive into stage 2.
 */
void test_lp24_cmos_tanh_saturation_limiting() {
    constexpr double fs = 48000.0;
    constexpr int numSamples = 8192;

    bumbler::WaspFilter filterClean;
    filterClean.prepare(fs);

    bumbler::WaspFilter filterOverdriven;
    filterOverdriven.prepare(fs);

    std::vector<float> cleanInput(numSamples);
    std::vector<float> hotInput(numSamples);
    bumbler_test::generateSine(cleanInput.data(), numSamples, 1000.0, fs, 0.1f);
    bumbler_test::generateSine(hotInput.data(), numSamples, 1000.0, fs, 4.0f);

    std::vector<float> cleanOut(numSamples);
    std::vector<float> hotOut(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        cleanOut[i] = filterClean.processSample(cleanInput[i], 1000.0, 0.7, bumbler::WaspFilterMode::LP24);
        hotOut[i]   = filterOverdriven.processSample(hotInput[i], 1000.0, 0.7, bumbler::WaspFilterMode::LP24);
    }

    // Measure fundamental (1000 Hz) and 3rd harmonic (3000 Hz) power
    constexpr int discard = 2048;
    constexpr int evalLen = numSamples - discard;

    double cleanFund = bumbler_test::computeFourierBinMagnitude(cleanOut.data() + discard, evalLen, 1000.0, fs);
    double cleanHarm3 = bumbler_test::computeFourierBinMagnitude(cleanOut.data() + discard, evalLen, 3000.0, fs);

    double hotFund = bumbler_test::computeFourierBinMagnitude(hotOut.data() + discard, evalLen, 1000.0, fs);
    double hotHarm3 = bumbler_test::computeFourierBinMagnitude(hotOut.data() + discard, evalLen, 3000.0, fs);

    double cleanThd = (cleanFund > 1.0e-6) ? (cleanHarm3 / cleanFund) : 0.0;
    double hotThd   = (hotFund > 1.0e-6) ? (hotHarm3 / hotFund) : 0.0;

    // Overdriven signal must exhibit substantially higher 3rd harmonic generation due to tanh CMOS saturation
    TEST_ASSERT(hotThd > cleanThd * 3.0,
        "LP24 tanh CMOS saturation failed to produce expected odd harmonic distortion under overdrive: cleanThd="
        + std::to_string(cleanThd) + ", hotThd=" + std::to_string(hotThd));

    // And hot output peak must remain safely bounded
    float hotPeak = bumbler_test::computePeak(hotOut.data(), numSamples);
    TEST_ASSERT(hotPeak < 10.0f, "LP24 output peak blew up under overdrive: hotPeak=" + std::to_string(hotPeak));
}

// ============================================================================
// SUITE 2: Rapid Cutoff Modulation Stepping
// ============================================================================

/**
 * Probe 2.1: Single-Block Rapid Cutoff Modulation (20 Hz <-> 20 kHz)
 * Simulates DAW parameter automation jumping from 20 Hz to 20,000 Hz across standard buffer sizes.
 * Verifies that 16-sample sub-block stepping and trapezoidal integration maintain stability
 * with zero clicks, no NaN/Inf, and bounded sample-to-sample derivatives.
 */
void test_rapid_cutoff_modulation_single_block() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    constexpr double fs = 48000.0;
    const std::vector<int> bufferSizes = { 16, 32, 64, 128, 256, 512, 1024 };

    for (int bufSize : bufferSizes) {
        bumbler::WaspFilter filter;
        filter.prepare(fs);

        std::vector<float> in(bufSize);
        bumbler_test::generateSine(in.data(), bufSize, 440.0, fs, 0.8f);

        std::vector<float> outBlock(bufSize, 0.0f);

        // Block 1: Low cutoff 20 Hz (steady state)
        filter.processBlock(in.data(), outBlock.data(), bufSize, 20.0f, 0.5f, bumbler::WaspFilterMode::LP24);

        // Block 2: Extreme rapid step 20 Hz -> 20,000 Hz in this single buffer block
        std::vector<float> outJumpUp(bufSize, 0.0f);
        filter.processBlock(in.data(), outJumpUp.data(), bufSize, 20000.0f, 0.5f, bumbler::WaspFilterMode::LP24);

        // Check continuity and absence of click explosions:
        // Transition from last sample of Block 1 to first sample of Block 2
        float crossBlockDiff = std::abs(outJumpUp[0] - outBlock[bufSize - 1]);
        TEST_ASSERT(crossBlockDiff < 1.0f,
            "Click explosion across block boundary on 20Hz->20kHz jump: diff=" + std::to_string(crossBlockDiff)
            + ", bufSize=" + std::to_string(bufSize));

        // Intra-block first-difference derivative |out[i] - out[i-1]|
        float maxIntraDiff = 0.0f;
        for (int i = 0; i < bufSize; ++i) {
            TEST_ASSERT(!std::isnan(outJumpUp[i]) && !std::isinf(outJumpUp[i]),
                "NaN/Inf during rapid cutoff modulation (20Hz->20kHz) at sample " + std::to_string(i));
            if (i > 0) {
                float diff = std::abs(outJumpUp[i] - outJumpUp[i - 1]);
                maxIntraDiff = std::max(maxIntraDiff, diff);
            }
        }
        TEST_ASSERT(maxIntraDiff < 1.5f,
            "Discontinuity spike inside 16-sample sub-block ramp: maxDiff=" + std::to_string(maxIntraDiff));

        // Block 3: Extreme rapid step 20,000 Hz -> 20 Hz in this single buffer block
        std::vector<float> outJumpDown(bufSize, 0.0f);
        filter.processBlock(in.data(), outJumpDown.data(), bufSize, 20.0f, 0.5f, bumbler::WaspFilterMode::LP24);

        float crossBlockDiffDown = std::abs(outJumpDown[0] - outJumpUp[bufSize - 1]);
        TEST_ASSERT(crossBlockDiffDown < 1.0f,
            "Click explosion across block boundary on 20kHz->20Hz jump: diff=" + std::to_string(crossBlockDiffDown));

        for (int i = 0; i < bufSize; ++i) {
            TEST_ASSERT(!std::isnan(outJumpDown[i]) && !std::isinf(outJumpDown[i]),
                "NaN/Inf during rapid cutoff modulation (20kHz->20Hz) at sample " + std::to_string(i));
        }
    }
}

/**
 * Probe 2.2: Rapid Cutoff Modulation on Arbitrary / Non-Power-of-Two Buffer Sizes
 * Verifies that the 16-sample sub-block stepping loop correctly handles buffers smaller than
 * 16 samples, prime buffer sizes, and non-multiples of 16 without boundary overruns or glitches.
 */
void test_rapid_cutoff_modulation_arbitrary_buffer_sizes() {
    constexpr double fs = 48000.0;
    const std::vector<int> oddSizes = { 1, 3, 5, 7, 15, 17, 31, 33, 63, 65, 127, 255 };

    for (int bufSize : oddSizes) {
        bumbler::WaspFilter filter;
        filter.prepare(fs);

        std::vector<float> in(bufSize);
        std::vector<float> out(bufSize);
        bumbler_test::generateSine(in.data(), bufSize, 880.0, fs, 0.9f);

        // Ping-pong cutoff 100 times on this buffer size
        for (int block = 0; block < 100; ++block) {
            const float targetCutoff = (block % 2 == 0) ? 30.0f : 18000.0f;
            const float targetReso   = (block % 2 == 0) ? 0.9f  : 0.1f;

            filter.processBlock(in.data(), out.data(), bufSize, targetCutoff, targetReso, bumbler::WaspFilterMode::LP24);

            for (int i = 0; i < bufSize; ++i) {
                TEST_ASSERT(!std::isnan(out[i]) && !std::isinf(out[i]),
                    "NaN/Inf in arbitrary buffer size test, bufSize=" + std::to_string(bufSize)
                    + ", sample=" + std::to_string(i));
                TEST_ASSERT(std::abs(out[i]) < 10.0f,
                    "State blowup in arbitrary buffer size test: out=" + std::to_string(out[i]));
            }
        }
    }
}

/**
 * Probe 2.3: Continuous Cutoff Ping-Pong Stress Across All 6 Modes
 * Alternates cutoff between 20 Hz and 20 kHz on every 32-sample block for 1,000 blocks
 * with high resonance. Tests trapezoidal integrator conditioning under high-frequency parameter slew.
 */
void test_rapid_cutoff_ping_pong_lfo_stress() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    constexpr double fs = 48000.0;
    constexpr int blockSize = 32;
    constexpr int totalBlocks = 1000;

    const std::vector<bumbler::WaspFilterMode> modes = {
        bumbler::WaspFilterMode::LP12,
        bumbler::WaspFilterMode::LP24,
        bumbler::WaspFilterMode::LP_NT,
        bumbler::WaspFilterMode::DBL_NT,
        bumbler::WaspFilterMode::BP24,
        bumbler::WaspFilterMode::HP24
    };

    std::vector<float> in(blockSize);
    std::vector<float> out(blockSize);
    bumbler_test::generateSine(in.data(), blockSize, 440.0, fs, 1.0f);

    for (auto mode : modes) {
        bumbler::WaspFilter filter;
        filter.prepare(fs);

        for (int b = 0; b < totalBlocks; ++b) {
            const float cutoff = (b % 2 == 0) ? 20.0f : 20000.0f;
            const float reso   = (b % 4 < 2)  ? 0.95f : 0.2f;

            filter.processBlock(in.data(), out.data(), blockSize, cutoff, reso, mode);

            for (int i = 0; i < blockSize; ++i) {
                TEST_ASSERT(!std::isnan(out[i]) && !std::isinf(out[i]),
                    "Ping-pong stress NaN/Inf detected in mode " + std::to_string(static_cast<int>(mode)));
                TEST_ASSERT(std::abs(out[i]) < 20.0f,
                    "Ping-pong stress state divergence in mode " + std::to_string(static_cast<int>(mode))
                    + ": out=" + std::to_string(out[i]));
            }
        }
    }
}

/**
 * Probe 2.4: Trapezoidal Integrator Boundary Stability (Extreme g and k)
 * Verifies that prewarped integrator gain g = tan(pi * fc / fs) and damping k = 2.0 - 1.9 * reso
 * remain strictly positive, denominator d = 1 + g(g+k) >= 1.0, and Nyquist alternating pulses
 * (+1, -1, +1, -1) do not induce divide-by-zero or sign-alternating divergence.
 */
void test_trapezoidal_integration_extreme_g_stability() {
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (double sr : sampleRates) {
        bumbler::WaspFilter filter;
        filter.prepare(sr);

        // Test boundary 1: Minimum cutoff 20 Hz
        const double fcMin = 20.0;
        const double gMin = std::tan(bumbler::kPi * fcMin / sr);
        TEST_ASSERT(gMin > 0.0 && gMin < 0.01, "gMin out of expected range: " + std::to_string(gMin));

        // Test boundary 2: Maximum cutoff 0.48 * sr
        const double fcMax = 0.48 * sr;
        const double gMax = std::tan(bumbler::kPi * fcMax / sr);
        TEST_ASSERT(gMax > 10.0 && !std::isinf(gMax), "gMax out of expected range: " + std::to_string(gMax));

        // Verify denominator d = 1.0 + g * (g + k) for extreme g and k
        for (double reso : { 0.0, 0.99 }) {
            const double k = 2.0 - 1.9 * reso;
            const double dMin = 1.0 + gMin * (gMin + k);
            const double dMax = 1.0 + gMax * (gMax + k);
            TEST_ASSERT(dMin >= 1.0, "Denominator dMin < 1.0: " + std::to_string(dMin));
            TEST_ASSERT(dMax > 100.0 && !std::isinf(dMax), "Denominator dMax invalid: " + std::to_string(dMax));
        }

        // Pass Nyquist test pattern (+1, -1, +1, -1...) at max cutoff and max resonance
        for (int i = 0; i < 2048; ++i) {
            float inSample = (i % 2 == 0) ? 1.0f : -1.0f;
            float out = filter.processSample(inSample, fcMax, 0.99, bumbler::WaspFilterMode::LP24);
            TEST_ASSERT(!std::isnan(out) && !std::isinf(out), "Nyquist pulse test produced NaN/Inf");
            TEST_ASSERT(std::abs(out) < 20.0f, "Nyquist pulse test state explosion: " + std::to_string(out));
        }
    }
}

// ============================================================================
// SUITE 3: Extreme Keyboard Tracking and Bipolar Envelope Modulation
// ============================================================================

/**
 * Probe 3.1: MIDI Note 0 (8.18 Hz) and Note 127 (12543.85 Hz) with +/-100% Envelope Modulation
 * Verifies that the modulated cutoff remains strictly clamped within [20 Hz, 0.48 * fs]
 * across all standard audio sample rates (44.1k, 48k, 88.2k, 96k, 192k).
 */
void test_midi_note_0_and_127_clamping() {
    const std::vector<float> sampleRates = { 44100.0f, 48000.0f, 88200.0f, 96000.0f, 192000.0f };
    const std::vector<float> baseCutoffs = { 20.0f, 100.0f, 440.0f, 1000.0f, 5000.0f, 15000.0f, 20000.0f };

    for (float sr : sampleRates) {
        const float nyquistClamp = 0.48f * sr;

        for (float baseFc : baseCutoffs) {
            // Case A: MIDI Note 0 (C-1) with 100% Key Tracking and -100% Envelope modulation
            // Mathematically: baseFc * 2^(-60/12) * 2^(-60/12) = baseFc / 1024.
            // For baseFc = 1000 Hz -> ~0.976 Hz. Must be strictly clamped to 20.0 Hz.
            float fcLow = bumbler::WaspFilter::calculateModulatedCutoff(
                baseFc,
                0.0f,    // Note 0
                1.0f,    // 100% KB tracking
                -1.0f,   // -100% Bipolar Envelope
                1.0f,    // Full Envelope Level
                sr
            );
            TEST_ASSERT(fcLow >= 20.0f,
                "Note 0 downward modulation dropped below 20 Hz: fcLow=" + std::to_string(fcLow) + ", sr=" + std::to_string(sr));
            TEST_ASSERT(fcLow <= nyquistClamp,
                "Note 0 downward modulation exceeded nyquist clamp: fcLow=" + std::to_string(fcLow));

            // Case B: MIDI Note 127 (G9) with 100% Key Tracking and +100% Envelope modulation
            // Mathematically: baseFc * 2^((127-60)/12) * 2^(+60/12) = baseFc * 47.9 * 32 = baseFc * 1532.8.
            // For baseFc = 1000 Hz -> ~1.53 MHz. Must be strictly clamped to 0.48 * sr.
            float fcHigh = bumbler::WaspFilter::calculateModulatedCutoff(
                baseFc,
                127.0f,  // Note 127
                1.0f,    // 100% KB tracking
                1.0f,    // +100% Bipolar Envelope
                1.0f,    // Full Envelope Level
                sr
            );
            TEST_ASSERT(fcHigh <= nyquistClamp + 0.001f,
                "Note 127 upward modulation exceeded 0.48*sr: fcHigh=" + std::to_string(fcHigh)
                + ", nyquistClamp=" + std::to_string(nyquistClamp));
            TEST_ASSERT(fcHigh >= 20.0f,
                "Note 127 upward modulation fell below 20 Hz: fcHigh=" + std::to_string(fcHigh));

            // Verify exact upper clamp equality when modulation pushes far past Nyquist
            if (baseFc >= 1000.0f) {
                TEST_ASSERT(std::abs(fcHigh - nyquistClamp) < 0.01f,
                    "Modulated cutoff failed to clamp exactly at 0.48*sr: fcHigh=" + std::to_string(fcHigh)
                    + ", expected=" + std::to_string(nyquistClamp));
            }
        }
    }
}

/**
 * Probe 3.2: Adversarial & Hostile Out-of-Range Modulation Inputs
 * Injects hostile inputs into calculateModulatedCutoff: negative MIDI notes, pitch bends,
 * out-of-range envelope depths, negative envelope levels, and extreme sample rates.
 * Verifies that the calculation NEVER produces NaN, Inf, or unclamped frequencies.
 */
void test_adversarial_out_of_range_modulation_inputs() {
    const std::vector<float> hostileNotes = { -120.0f, -50.0f, 0.0f, 60.0f, 127.0f, 250.0f, 500.0f };
    const std::vector<float> hostileKbTracks = { -10.0f, -1.0f, 0.0f, 0.5f, 1.0f, 2.0f, 10.0f };
    const std::vector<float> hostileEnvAmts = { -50.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 50.0f };
    const std::vector<float> hostileEnvLevels = { -5.0f, -1.0f, 0.0f, 0.5f, 1.0f, 2.0f, 10.0f };
    const std::vector<float> hostileBaseFcs = { -1000.0f, 0.0f, 10.0f, 1000.0f, 50000.0f, 1.0e8f };
    const std::vector<float> sampleRates = { 8000.0f, 44100.0f, 48000.0f, 192000.0f, 384000.0f };

    for (float sr : sampleRates) {
        const float nyquistClamp = 0.48f * sr;

        for (float note : hostileNotes) {
            for (float kb : hostileKbTracks) {
                for (float envAmt : hostileEnvAmts) {
                    for (float envLvl : hostileEnvLevels) {
                        for (float baseFc : hostileBaseFcs) {
                            float fc = bumbler::WaspFilter::calculateModulatedCutoff(
                                baseFc, note, kb, envAmt, envLvl, sr
                            );

                            TEST_ASSERT(!std::isnan(fc) && !std::isinf(fc),
                                "Hostile modulation input generated NaN/Inf: fc=" + std::to_string(fc));
                            TEST_ASSERT(fc >= 20.0f,
                                "Hostile modulation input violated minimum 20 Hz clamp: fc=" + std::to_string(fc));
                            TEST_ASSERT(fc <= nyquistClamp + 0.01f,
                                "Hostile modulation input violated maximum 0.48*sr clamp: fc=" + std::to_string(fc)
                                + ", clamp=" + std::to_string(nyquistClamp));
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// SUITE 4: Voice & Engine Filter Integration
// ============================================================================

/**
 * Probe 4.1: Voice Stealing during Maximum Resonance Self-Oscillation
 * Spawns a voice, triggers note-on with high resonance (0.95), allows self-oscillation
 * to develop full energy, then steals the voice immediately.
 * Validates that the 5ms Hann crossfading eliminates clicks and produces no audio anomalies.
 */
void test_voice_filter_high_resonance_note_stealing() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    constexpr double fs = 48000.0;

    bumbler::BumblerVoice voice;
    voice.prepare(fs);
    voice.setFilterEnabled(true);

    bumbler::ParameterSnapshot params;
    params.filterMode = 1.0f; // LP24
    params.filterCutoff = 1000.0f;
    params.filterResonance = 0.95f;
    params.filterKbTrack = 0.5f;
    params.ampAttack = 0.001f;
    params.ampDecay = 0.5f;
    params.ampSustain = 0.8f;
    params.ampRelease = 0.5f;

    // Trigger initial note
    voice.noteOn(60, 1.0f, 1);
    TEST_ASSERT(voice.isActive(), "Voice must be active after noteOn");

    // Render 500 samples so resonance builds up
    for (int i = 0; i < 500; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
        TEST_ASSERT(!std::isnan(l) && !std::isnan(r), "NaN detected during pre-steal resonance");
    }

    // Voice steal event: re-trigger on the same voice while filter is hot
    voice.noteOn(72, 1.0f, 2);

    // Crossfade window is 5ms = 240 samples at 48k
    float prevL = 0.0f;
    float maxDerivative = 0.0f;

    for (int i = 0; i < 480; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);

        TEST_ASSERT(!std::isnan(l) && !std::isinf(l), "NaN/Inf during resonant voice steal");
        TEST_ASSERT(!std::isnan(r) && !std::isinf(r), "NaN/Inf during resonant voice steal (R)");

        if (i > 0) {
            float diff = std::abs(l - prevL);
            maxDerivative = std::max(maxDerivative, diff);
        }
        prevL = l;
    }

    // Hann crossfade must prevent hard discontinuities
    TEST_ASSERT(maxDerivative < 1.0f,
        "Voice stealing under self-oscillation produced a click discontinuity: maxDiff=" + std::to_string(maxDerivative));
}

/**
 * Probe 4.2: Dynamic Filter Mode Switching Under Active Audio Load
 * Switches filterMode through all 6 modes dynamically on consecutive samples and short blocks
 * while processing active audio with resonance.
 * Validates that mode switching does not cause internal filter state divergence or pops.
 */
void test_voice_filter_dynamic_mode_switching() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    constexpr double fs = 48000.0;

    bumbler::WaspFilter filter;
    filter.prepare(fs);

    constexpr int kSamples = 6000;
    std::vector<float> in(kSamples);
    bumbler_test::generateSine(in.data(), kSamples, 500.0, fs, 0.8f);

    // Cycle through modes 0..5 every 10 samples
    for (int i = 0; i < kSamples; ++i) {
        auto mode = static_cast<bumbler::WaspFilterMode>((i / 10) % 6);
        float out = filter.processSample(in[i], 1000.0, 0.85, mode);

        TEST_ASSERT(!std::isnan(out) && !std::isinf(out),
            "NaN/Inf during dynamic filter mode switching at sample " + std::to_string(i));
        TEST_ASSERT(std::abs(out) < 25.0f,
            "State divergence during dynamic filter mode switching: out=" + std::to_string(out));
    }
}

/**
 * Probe 4.3: Real-Time Audio Path Zero Heap Allocations Under Stress
 * Executes WaspFilter::processBlock, WaspFilter::processSample, and BumblerVoice::renderSample
 * under full stress (rapid parameter stepping, extreme resonance, mode changes) with the
 * allocation tracker active.
 * Verifies STRICTLY 0 heap allocations on the audio path.
 */
void test_filter_realtime_zero_heap_allocations() {
    constexpr double fs = 48000.0;
    constexpr int kBufSize = 256;

    bumbler::WaspFilter filter;
    filter.prepare(fs);

    bumbler::BumblerVoice voice;
    voice.prepare(fs);
    voice.setFilterEnabled(true);

    bumbler::ParameterSnapshot params;
    params.filterCutoff = 2000.0f;
    params.filterResonance = 0.95f;
    params.filterMode = 1.0f;
    params.envLink = 1.0f;

    std::vector<float> inBuf(kBufSize, 0.5f);
    std::vector<float> outBuf(kBufSize, 0.0f);

    voice.noteOn(60, 0.9f, 1);

    // Warm-up to ensure any static init is complete
    filter.processBlock(inBuf.data(), outBuf.data(), kBufSize, 1000.0f, 0.5f, bumbler::WaspFilterMode::LP24);
    float dummyL = 0.0f, dummyR = 0.0f;
    voice.renderSample(dummyL, dummyR, params);

    // ================= START ALLOCATION TRACKING =================
    resetAllocationTracker();
    enableAllocationTracker(true);

    // 1. Process 100 blocks of processBlock with varying parameters
    for (int b = 0; b < 100; ++b) {
        float fc = 20.0f + static_cast<float>(b) * 190.0f;
        auto mode = static_cast<bumbler::WaspFilterMode>(b % 6);
        filter.processBlock(inBuf.data(), outBuf.data(), kBufSize, fc, 0.9f, mode);
    }

    // 2. Process 10,000 samples of processSample
    for (int i = 0; i < 10000; ++i) {
        [[maybe_unused]] float s = filter.processSample(0.5f, 1500.0, 0.95, bumbler::WaspFilterMode::LP_NT);
    }

    // 3. Process 10,000 samples of BumblerVoice::renderSample
    for (int i = 0; i < 10000; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
    }

    enableAllocationTracker(false);
    // ================= STOP ALLOCATION TRACKING ==================

    size_t allocs = getAllocationCount();
    TEST_ASSERT(allocs == 0,
        "Audio path violated zero-allocation invariant: " + std::to_string(allocs) + " heap allocations detected!");
}

// ============================================================================
// Main Adversarial Challenger M2-1 Test Runner
// ============================================================================
int main() {
    std::cout << "==================================================================\n";
    std::cout << " Bumbler XD: Milestone 2 Adversarial Filter Stress Probe Suite    \n";
    std::cout << " (teamwork_preview_challenger_m2_1)                               \n";
    std::cout << "==================================================================\n";

    std::cout << "\n--- SUITE 1: Resonance at Self-Oscillation Limits (Q -> 1.0) ---\n";
    RUN_TEST(test_resonance_self_oscillation_dc_steps);
    RUN_TEST(test_resonance_self_oscillation_high_amplitude_squares);
    RUN_TEST(test_resonance_impulse_ringing_stability);
    RUN_TEST(test_lp24_cmos_tanh_saturation_limiting);

    std::cout << "\n--- SUITE 2: Rapid Cutoff Modulation Stepping ---\n";
    RUN_TEST(test_rapid_cutoff_modulation_single_block);
    RUN_TEST(test_rapid_cutoff_modulation_arbitrary_buffer_sizes);
    RUN_TEST(test_rapid_cutoff_ping_pong_lfo_stress);
    RUN_TEST(test_trapezoidal_integration_extreme_g_stability);

    std::cout << "\n--- SUITE 3: Extreme Keyboard Tracking and Bipolar Envelope ---\n";
    RUN_TEST(test_midi_note_0_and_127_clamping);
    RUN_TEST(test_adversarial_out_of_range_modulation_inputs);

    std::cout << "\n--- SUITE 4: Voice & Engine Filter Integration ---\n";
    RUN_TEST(test_voice_filter_high_resonance_note_stealing);
    RUN_TEST(test_voice_filter_dynamic_mode_switching);
    RUN_TEST(test_filter_realtime_zero_heap_allocations);

    std::cout << "\n==================================================================\n";
    std::cout << " Challenger M2-1 Summary: " << gGlobalTestsPassed << " Passed, "
              << gGlobalTestsFailed << " Failed\n";
    std::cout << "==================================================================\n";

    return (gGlobalTestsFailed == 0) ? 0 : 1;
}
