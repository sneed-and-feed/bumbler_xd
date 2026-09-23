#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <string>
#include <iomanip>
#include <algorithm>
#include <array>
#include <stdexcept>

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "BumblerVoice.h"
#include "BumblerVoiceManager.h"
#include "BumblerEngine.h"
#include "CharacterCircuits.h"
#include "test_helpers.h"

// ============================================================================
// Real-Time Memory Safety: Interception Hooks
// ============================================================================
static std::atomic<bool> gTrackAllocationsM4 { false };
static std::atomic<size_t> gAllocationCountM4 { 0 };
static std::atomic<size_t> gAllocatedBytesM4 { 0 };

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
void* operator new(size_t size) {
    if (gTrackAllocationsM4.load(std::memory_order_relaxed)) {
        gAllocationCountM4.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytesM4.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void* operator new[](size_t size) {
    if (gTrackAllocationsM4.load(std::memory_order_relaxed)) {
        gAllocationCountM4.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytesM4.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
#endif

// ============================================================================
// Test Framework Logging & Counters
// ============================================================================
static int gTotalTests = 0;
static int gPassedTests = 0;
static int gFailedTests = 0;

#define CHALLENGE_ASSERT(cond, msg) \
    do { \
        ++gTotalTests; \
        if (cond) { \
            ++gPassedTests; \
        } else { \
            ++gFailedTests; \
            std::cerr << "  [FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
        } \
    } while(0)

#define CHALLENGE_ASSERT_NEAR(val, target, eps, msg) \
    do { \
        ++gTotalTests; \
        double diff = std::abs((val) - (target)); \
        if (diff <= (eps)) { \
            ++gPassedTests; \
        } else { \
            ++gFailedTests; \
            std::cerr << "  [FAIL] Line " << __LINE__ << ": " << msg \
                      << " (val=" << (val) << ", target=" << (target) \
                      << ", diff=" << diff << ", eps=" << (eps) << ")" << std::endl; \
        } \
    } while(0)

// Helper: Normalized autocorrelation at lag k: sum(x[i]*x[i+k]) / sum(x[i]^2)
static double computeNormalizedAutocorr(const float* buffer, int numSamples, int lag) {
    if (buffer == nullptr || numSamples <= lag || lag < 0) return 0.0;
    const int N = numSamples - lag;
    double num = 0.0, denom = 0.0;
    for (int i = 0; i < N; ++i) {
        num += static_cast<double>(buffer[i]) * static_cast<double>(buffer[i + lag]);
        denom += static_cast<double>(buffer[i]) * static_cast<double>(buffer[i]);
    }
    if (denom <= 1.0e-15) return 0.0;
    return num / denom;
}

// Helper: Zero-crossing fundamental frequency estimator
static double measureAudioFrequency(const float* buffer, int numSamples, double sampleRate) {
    if (buffer == nullptr || numSamples < 64 || sampleRate <= 0.0) return 0.0;
    std::vector<double> crossings;
    crossings.reserve(256);
    for (int i = 1; i < numSamples; ++i) {
        if (buffer[i - 1] <= 0.0f && buffer[i] > 0.0f) {
            double y1 = buffer[i - 1], y2 = buffer[i];
            double frac = (y2 != y1) ? (-y1 / (y2 - y1)) : 0.0;
            crossings.push_back((i - 1) + frac);
        }
    }
    if (crossings.size() < 4) return 0.0;
    double totalCycles = static_cast<double>(crossings.size() - 1);
    double totalSamples = crossings.back() - crossings.front();
    if (totalSamples <= 0.0) return 0.0;
    return sampleRate / (totalSamples / totalCycles);
}

// ============================================================================
// CHALLENGE SUITE 1: Dual Mode Stereo Decorrelation Across Diverse Signals
// ============================================================================
void challenge_dual_mode_decorrelation() {
    std::cout << "\n=======================================================\n";
    std::cout << " [CHALLENGE 1] Dual Mode Stereo Decorrelation & Bounds\n";
    std::cout << "=======================================================\n";

    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 8192;

    bumbler::CharacterCircuits circuits;
    circuits.prepare(kSampleRate, kSamples);

    bumbler::ParameterSnapshot params;
    params.driveEnabled = 0.0f;
    params.masterVolume = 1.0f;

    struct SignalTestCase {
        std::string name;
        std::vector<float> data;
    };

    std::vector<SignalTestCase> testSignals;

    // 1. Sine 440 Hz (Standard A4 musical pitch)
    {
        SignalTestCase tc { "Sine 440 Hz (A4)", std::vector<float>(kSamples) };
        bumbler_test::generateSine(tc.data.data(), kSamples, 440.0, kSampleRate, 0.8f);
        testSignals.push_back(std::move(tc));
    }
    // 2. Sine 330 Hz (E4 musical pitch)
    {
        SignalTestCase tc { "Sine 330 Hz (E4)", std::vector<float>(kSamples) };
        bumbler_test::generateSine(tc.data.data(), kSamples, 330.0, kSampleRate, 0.8f);
        testSignals.push_back(std::move(tc));
    }
    // 3. Sine 587 Hz (D5 musical pitch)
    {
        SignalTestCase tc { "Sine 587 Hz (D5)", std::vector<float>(kSamples) };
        bumbler_test::generateSine(tc.data.data(), kSamples, 587.0, kSampleRate, 0.8f);
        testSignals.push_back(std::move(tc));
    }
    // 4. Sawtooth 220 Hz (A3 musical pitch)
    {
        SignalTestCase tc { "Sawtooth 220 Hz (A3)", std::vector<float>(kSamples) };
        double phase = 0.0;
        const double inc = 220.0 / kSampleRate;
        for (int i = 0; i < kSamples; ++i) {
            tc.data[i] = static_cast<float>(2.0 * phase - 1.0) * 0.8f;
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
        testSignals.push_back(std::move(tc));
    }
    // 5. Sawtooth 440 Hz (A4 musical pitch)
    {
        SignalTestCase tc { "Sawtooth 440 Hz (A4)", std::vector<float>(kSamples) };
        double phase = 0.0;
        const double inc = 440.0 / kSampleRate;
        for (int i = 0; i < kSamples; ++i) {
            tc.data[i] = static_cast<float>(2.0 * phase - 1.0) * 0.8f;
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
        testSignals.push_back(std::move(tc));
    }
    // 6. White Noise (Broadband stochastic signal)
    {
        SignalTestCase tc { "White Noise", std::vector<float>(kSamples) };
        uint32_t prng = 0xDEADBEEFu;
        for (int i = 0; i < kSamples; ++i) {
            prng ^= prng << 13; prng ^= prng >> 17; prng ^= prng << 5;
            tc.data[i] = static_cast<float>(static_cast<int32_t>(prng)) * (0.8f / 2147483648.0f);
        }
        testSignals.push_back(std::move(tc));
    }
    // 7. Impulses (Periodic train every 350 samples)
    {
        SignalTestCase tc { "Impulse Train (Period 350)", std::vector<float>(kSamples, 0.0f) };
        for (int i = 0; i < kSamples; i += 350) {
            tc.data[i] = 0.9f;
        }
        testSignals.push_back(std::move(tc));
    }
    // 8. Sparse Random Impulses
    {
        SignalTestCase tc { "Sparse Random Impulses", std::vector<float>(kSamples, 0.0f) };
        uint32_t prng = 0x543210ABu;
        for (int i = 0; i < kSamples; ++i) {
            prng ^= prng << 13; prng ^= prng >> 17; prng ^= prng << 5;
            if ((prng & 0x7Fu) == 0x01u) {
                tc.data[i] = (prng & 0x100u) ? 0.9f : -0.9f;
            }
        }
        testSignals.push_back(std::move(tc));
    }


    // Direct DualModeVoiceDoubler & CharacterCircuits verification across all signals
    for (const auto& sig : testSignals) {
        // A. Bypassed Mode: r MUST be identically 1.000
        params.dualMode = 0.0f;
        std::vector<float> offL = sig.data;
        std::vector<float> offR = sig.data;
        circuits.reset();
        circuits.processStereo(offL.data(), offR.data(), kSamples, params);

        double rOff = bumbler_test::computeCrossCorrelation(offL.data(), offR.data(), kSamples);
        std::cout << "  Signal: " << std::left << std::setw(28) << sig.name 
                  << " | Dual OFF r = " << std::fixed << std::setprecision(6) << rOff;
        CHALLENGE_ASSERT_NEAR(rOff, 1.000000, 1.0e-5, "Dual Mode OFF must produce r = 1.000 for signal: " + sig.name);

        // B. Active Mode: r MUST be < 0.70, peak output <= 1.05f
        params.dualMode = 1.0f;
        std::vector<float> onL = sig.data;
        std::vector<float> onR = sig.data;
        circuits.reset();
        circuits.processStereo(onL.data(), onR.data(), kSamples, params);

        // Skip initial transient samples corresponding to delay buffer fill
        constexpr int kWarmup = 300;
        double rOn = bumbler_test::computeCrossCorrelation(onL.data() + kWarmup, onR.data() + kWarmup, kSamples - kWarmup);
        float peakL = bumbler_test::computePeak(onL.data(), kSamples);
        float peakR = bumbler_test::computePeak(onR.data(), kSamples);
        float maxPeak = std::max(peakL, peakR);

        std::cout << " | Dual ON r = " << std::setw(9) << rOn 
                  << " | Peak = " << std::setw(7) << maxPeak << "\n";

        CHALLENGE_ASSERT(rOn < 0.70, "Dual Mode ON cross-correlation r must be < 0.70 for signal: " + sig.name + " (got " + std::to_string(rOn) + ")");
        CHALLENGE_ASSERT(maxPeak <= 1.05f, "Peak output exceeded 1.05f bound for signal: " + sig.name + " (got " + std::to_string(maxPeak) + ")");
    }

    // C. Extreme Hot Input Stress Test (Amplitude = 1.8f)
    {
        std::cout << "  Stress testing Dual Mode under extreme input overdrive (Amp = 1.8f)...\n";
        std::vector<float> hotL(kSamples), hotR(kSamples);
        bumbler_test::generateSine(hotL.data(), kSamples, 440.0, kSampleRate, 1.8f);
        hotR = hotL;

        params.dualMode = 1.0f;
        circuits.reset();
        circuits.processStereo(hotL.data(), hotR.data(), kSamples, params);

        float peakL = bumbler_test::computePeak(hotL.data(), kSamples);
        float peakR = bumbler_test::computePeak(hotR.data(), kSamples);
        CHALLENGE_ASSERT(peakL <= 1.05f && peakR <= 1.05f, "Hot input caused output to exceed 1.05f safety ceiling");
    }

    // D. End-to-End Engine Verification across multiple voice pitches
    {
        std::cout << "  Verifying End-to-End BumblerEngine Dual Mode across voice pitches...\n";
        bumbler::BumblerEngine engine;
        engine.prepare(kSampleRate, 512);

        const int testMidiNotes[] = { 48, 60, 72 }; // C3, C4, C5
        for (int note : testMidiNotes) {
            bumbler::ParameterSnapshot engParams;
            engParams.osc1Waveform = 0.0f; // Sawtooth
            engParams.ampAttack = 0.001f;
            engParams.ampSustain = 1.0f;
            engParams.masterVolume = 0.8f;
            engParams.dualMode = 1.0f;

            engine.reset();
            engine.processMidiEvent(0x90, note, 0.8f);

            std::vector<float> engL(kSamples, 0.0f);
            std::vector<float> engR(kSamples, 0.0f);

            for (int b = 0; b < kSamples; b += 512) {
                float* blockChannels[2] = { engL.data() + b, engR.data() + b };
                engine.renderBlock(blockChannels, 2, 512, engParams);
            }

            double rEng = bumbler_test::computeCrossCorrelation(engL.data() + 512, engR.data() + 512, kSamples - 512);
            float peakEng = std::max(bumbler_test::computePeak(engL.data(), kSamples),
                                     bumbler_test::computePeak(engR.data(), kSamples));

            std::cout << "    Midi Note " << note << ": Dual ON r = " << rEng << ", Peak = " << peakEng << "\n";
            CHALLENGE_ASSERT(rEng < 0.70, "BumblerEngine Dual ON r must be < 0.70 for note " + std::to_string(note));
            CHALLENGE_ASSERT(peakEng <= 1.05f, "BumblerEngine Dual ON peak exceeded 1.05f for note " + std::to_string(note));
        }
    }

    // E. Adversarial Physics Probe: Fixed 5ms Delay Comb Node Spectral Characteristics
    {
        std::cout << "  [Adversarial Physics Probe] Analyzing 5ms static delay comb nodes at k * 200 Hz:\n";
        const double combFreqs[] = { 200.0, 400.0, 600.0, 1000.0 };
        for (double f : combFreqs) {
            std::vector<float> probeL(kSamples), probeR(kSamples);
            bumbler_test::generateSine(probeL.data(), kSamples, f, kSampleRate, 0.8f);
            probeR = probeL;
            params.dualMode = 1.0f;
            circuits.reset();
            circuits.processStereo(probeL.data(), probeR.data(), kSamples, params);
            double rComb = bumbler_test::computeCrossCorrelation(probeL.data() + 300, probeR.data() + 300, kSamples - 300);
            std::cout << "    Comb Node f = " << std::setw(4) << static_cast<int>(f) 
                      << " Hz: Dual ON r = " << std::fixed << std::setprecision(6) << rComb 
                      << " (Exact period multiple: 5ms = " << (f * 0.005) << " cycles)\n";
        }
    }
}


// ============================================================================
// CHALLENGE SUITE 2: Analog Mode Pitch Drift & Phase Variance (1,000+ Triggers)
// ============================================================================
void challenge_analog_mode_statistics() {
    std::cout << "\n=======================================================\n";
    std::cout << " [CHALLENGE 2] Analog Mode Pitch Drift & Phase Variance\n";
    std::cout << "=======================================================\n";

    constexpr double kSampleRate = 48000.0;
    constexpr int kNumTriggers = 1200; // Requirement: 1,000+ note triggers

    std::cout << "  Sampling across N = " << kNumTriggers << " note trigger events on BumblerVoice...\n";

    bumbler::BumblerVoice voice;
    voice.prepare(kSampleRate);

    bumbler::ParameterSnapshot vParams;
    vParams.osc1Waveform = 2.0f; // Pure sine for accurate zero-crossing
    vParams.oscMix = 0.0f;
    vParams.ampAttack = 0.0005f;
    vParams.ampSustain = 1.0f;
    vParams.velToAmp = 0.0f;

    // ------------------------------------------------------------------------
    // Part A: Analog Mode ACTIVE (1,200 triggers)
    // ------------------------------------------------------------------------
    std::vector<double> driftCentsActive(kNumTriggers);
    std::vector<double> initialPhaseActive(kNumTriggers);
    std::vector<double> audioFreqCentsActive(kNumTriggers);
    std::vector<double> initialSampleActive(kNumTriggers);

    vParams.analogMode = 1.0f;

    for (int i = 0; i < kNumTriggers; ++i) {
        voice.reset();
        voice.setAnalogMode(true);
        // Note trigger
        voice.noteOn(69, 1.0f, static_cast<uint64_t>(i * 4096), true);

        // 1. Direct state queries
        driftCentsActive[i] = static_cast<double>(voice.getPitchDriftCents());
        initialPhaseActive[i] = static_cast<double>(voice.getOscInitialPhaseRad());

        // 2. Render initial audio sample
        float sL = 0.0f, sR = 0.0f;
        voice.renderSample(sL, sR, vParams);
        initialSampleActive[i] = static_cast<double>(sL);

        // 3. Render audio block to measure fundamental frequency (every 5th trigger to maintain fast test execution)
        if (i % 5 == 0) {
            constexpr int kBlockLen = 2048;
            std::vector<float> audioBlock(kBlockLen);
            audioBlock[0] = sL;
            for (int s = 1; s < kBlockLen; ++s) {
                float l = 0.0f, r = 0.0f;
                voice.renderSample(l, r, vParams);
                audioBlock[s] = l;
            }
            double f = measureAudioFrequency(audioBlock.data(), kBlockLen, kSampleRate);
            audioFreqCentsActive[i / 5] = 1200.0 * std::log2(f / 440.0);
        }
    }
    audioFreqCentsActive.resize(kNumTriggers / 5);

    double varDriftActive = bumbler_test::computeVariance(driftCentsActive);
    double varPhaseActive = bumbler_test::computeVariance(initialPhaseActive);
    double varFreqActive = bumbler_test::computeVariance(audioFreqCentsActive);
    double varSampleActive = bumbler_test::computeVariance(initialSampleActive);

    std::cout << "  [ACTIVE] Pitch Drift Variance:            " << varDriftActive << " (Threshold > 0.05)\n";
    std::cout << "  [ACTIVE] Initial Phase Variance:          " << varPhaseActive << " (Threshold > 0.1)\n";
    std::cout << "  [ACTIVE] Audio Measured Freq Variance:    " << varFreqActive << " cents^2\n";
    std::cout << "  [ACTIVE] Audio First Sample Variance:     " << varSampleActive << "\n";

    CHALLENGE_ASSERT(varDriftActive > 0.05, "Active pitch drift variance must be > 0.05 (got " + std::to_string(varDriftActive) + ")");
    CHALLENGE_ASSERT(varPhaseActive > 0.1, "Active initial phase variance must be > 0.1 (got " + std::to_string(varPhaseActive) + ")");
    CHALLENGE_ASSERT(varFreqActive > 0.05, "Active audio frequency variance must be > 0.05 cents^2 (got " + std::to_string(varFreqActive) + ")");
    CHALLENGE_ASSERT(varSampleActive > 0.1, "Active initial sample variance must be > 0.1 (got " + std::to_string(varSampleActive) + ")");

    // ------------------------------------------------------------------------
    // Part B: Analog Mode INACTIVE / BYPASSED (1,200 triggers)
    // ------------------------------------------------------------------------
    std::vector<double> driftCentsInactive(kNumTriggers);
    std::vector<double> initialPhaseInactive(kNumTriggers);
    std::vector<double> audioFreqCentsInactive(kNumTriggers);
    std::vector<double> initialSampleInactive(kNumTriggers);

    vParams.analogMode = 0.0f;

    for (int i = 0; i < kNumTriggers; ++i) {
        voice.reset();
        voice.setAnalogMode(false);
        voice.noteOn(69, 1.0f, static_cast<uint64_t>(i * 4096), false);

        driftCentsInactive[i] = static_cast<double>(voice.getPitchDriftCents());
        initialPhaseInactive[i] = static_cast<double>(voice.getOscInitialPhaseRad());

        float sL = 0.0f, sR = 0.0f;
        voice.renderSample(sL, sR, vParams);
        initialSampleInactive[i] = static_cast<double>(sL);

        if (i % 5 == 0) {
            constexpr int kBlockLen = 2048;
            std::vector<float> audioBlock(kBlockLen);
            audioBlock[0] = sL;
            for (int s = 1; s < kBlockLen; ++s) {
                float l = 0.0f, r = 0.0f;
                voice.renderSample(l, r, vParams);
                audioBlock[s] = l;
            }
            double f = measureAudioFrequency(audioBlock.data(), kBlockLen, kSampleRate);
            audioFreqCentsInactive[i / 5] = 1200.0 * std::log2(f / 440.0);
        }
    }
    audioFreqCentsInactive.resize(kNumTriggers / 5);

    double varDriftInactive = bumbler_test::computeVariance(driftCentsInactive);
    double varPhaseInactive = bumbler_test::computeVariance(initialPhaseInactive);
    double varFreqInactive = bumbler_test::computeVariance(audioFreqCentsInactive);
    double varSampleInactive = bumbler_test::computeVariance(initialSampleInactive);

    std::cout << "  [INACTIVE] Pitch Drift Variance:          " << varDriftInactive << " (Expected STRICTLY 0.0)\n";
    std::cout << "  [INACTIVE] Initial Phase Variance:        " << varPhaseInactive << " (Expected STRICTLY 0.0)\n";
    std::cout << "  [INACTIVE] Audio Measured Freq Variance:  " << varFreqInactive << " cents^2\n";
    std::cout << "  [INACTIVE] Audio First Sample Variance:   " << varSampleInactive << "\n";

    CHALLENGE_ASSERT(varDriftInactive == 0.0, "Inactive pitch drift variance must be strictly 0.0 (got " + std::to_string(varDriftInactive) + ")");
    CHALLENGE_ASSERT(varPhaseInactive == 0.0, "Inactive initial phase variance must be strictly 0.0 (got " + std::to_string(varPhaseInactive) + ")");
    CHALLENGE_ASSERT(varFreqInactive < 1.0e-5, "Inactive audio frequency variance must be < 1.0e-5 (got " + std::to_string(varFreqInactive) + ")");
    CHALLENGE_ASSERT(varSampleInactive == 0.0, "Inactive initial sample variance must be strictly 0.0 (got " + std::to_string(varSampleInactive) + ")");
}

// ============================================================================
// CHALLENGE SUITE 3: Distortion 3rd Harmonic Boost & Tone Spectral Tilt
// ============================================================================
void challenge_distortion_harmonics_and_tone() {
    std::cout << "\n=======================================================\n";
    std::cout << " [CHALLENGE 3] Distortion 3rd Harmonics & Tone Tilt\n";
    std::cout << "=======================================================\n";

    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 8192;
    constexpr double fFund = 1000.0;
    constexpr double f3rd = 3000.0;

    std::vector<float> sineIn(kSamples);
    bumbler_test::generateSine(sineIn.data(), kSamples, fFund, kSampleRate, 0.7f);

    bumbler::CharacterCircuits circuits;
    circuits.prepare(kSampleRate, kSamples);

    bumbler::ParameterSnapshot params;
    params.masterVolume = 1.0f;
    params.dualMode = 0.0f;

    // ------------------------------------------------------------------------
    // A. 3rd Harmonic Boost: drive=0.0 vs drive=1.0
    // ------------------------------------------------------------------------
    // 1. Clean Reference: driveEnabled=0.0, driveAmount=0.0
    params.driveEnabled = 0.0f;
    params.driveAmount = 0.0f;
    params.driveTone = 0.5f;

    std::vector<float> cleanL = sineIn;
    std::vector<float> cleanR = sineIn;
    circuits.reset();
    circuits.processStereo(cleanL.data(), cleanR.data(), kSamples, params);

    double clean3rdPower = bumbler_test::computeFourierBinPower(cleanL.data(), kSamples, f3rd, kSampleRate);

    // 2. Driven Output: driveEnabled=1.0, driveAmount=1.0
    params.driveEnabled = 1.0f;
    params.driveAmount = 1.0f;
    params.driveTone = 0.5f;

    std::vector<float> drivenL = sineIn;
    std::vector<float> drivenR = sineIn;
    circuits.reset();
    circuits.processStereo(drivenL.data(), drivenR.data(), kSamples, params);

    double driven3rdPower = bumbler_test::computeFourierBinPower(drivenL.data(), kSamples, f3rd, kSampleRate);
    float drivenPeak = bumbler_test::computePeak(drivenL.data(), kSamples);

    // Calculate boost in dB: 10 * log10(P_driven / P_clean)
    double boostDb = 10.0 * std::log10(driven3rdPower / std::max(clean3rdPower, 1.0e-12));

    std::cout << "  Clean 3rd Harmonic Power:   " << clean3rdPower << "\n";
    std::cout << "  Driven 3rd Harmonic Power:  " << driven3rdPower << "\n";
    std::cout << "  3rd Harmonic Boost:         " << boostDb << " dB (Threshold > 20 dB)\n";
    std::cout << "  Driven Peak Output:         " << drivenPeak << " (Ceiling <= 1.05)\n";

    CHALLENGE_ASSERT(boostDb > 20.0, "3rd harmonic boost must be > 20 dB between drive=0.0 and drive=1.0 (got " + std::to_string(boostDb) + " dB)");
    CHALLENGE_ASSERT(drivenPeak <= 1.05f, "Driven peak exceeded safety ceiling of 1.05f (got " + std::to_string(drivenPeak) + ")");

    // ------------------------------------------------------------------------
    // B. Tone Control Spectral Tilt at 3 kHz (tone=0.0 vs tone=1.0)
    // ------------------------------------------------------------------------
    params.driveEnabled = 1.0f;
    params.driveAmount = 1.0f;

    // Dark (tone = 0.0)
    params.driveTone = 0.0f;
    std::vector<float> darkL = sineIn, darkR = sineIn;
    circuits.reset();
    circuits.processStereo(darkL.data(), darkR.data(), kSamples, params);
    double darkMag3k = bumbler_test::computeFourierBinMagnitude(darkL.data(), kSamples, 3000.0, kSampleRate);

    // Bright (tone = 1.0)
    params.driveTone = 1.0f;
    std::vector<float> brightL = sineIn, brightR = sineIn;
    circuits.reset();
    circuits.processStereo(brightL.data(), brightR.data(), kSamples, params);
    double brightMag3k = bumbler_test::computeFourierBinMagnitude(brightL.data(), kSamples, 3000.0, kSampleRate);

    double tiltDb = 20.0 * std::log10(brightMag3k / std::max(darkMag3k, 1.0e-12));

    std::cout << "  Dark (Tone=0.0) 3 kHz Mag:   " << darkMag3k << "\n";
    std::cout << "  Bright (Tone=1.0) 3 kHz Mag: " << brightMag3k << "\n";
    std::cout << "  Tone Spectral Tilt (3 kHz): " << tiltDb << " dB (Threshold > 10 dB)\n";

    CHALLENGE_ASSERT(tiltDb > 10.0, "Tone spectral tilt must be > 10 dB at 3 kHz (got " + std::to_string(tiltDb) + " dB)");

    // Monotonic Tone Sweep Test across [0.0, 0.25, 0.50, 0.75, 1.0]
    std::cout << "  Sweeping Tone across [0.0, 0.25, 0.5, 0.75, 1.0] to test monotonic high-frequency response:\n";
    const float toneSteps[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    double lastMag = -1.0;
    bool isMonotonic = true;

    for (float t : toneSteps) {
        params.driveTone = t;
        std::vector<float> tL = sineIn, tR = sineIn;
        circuits.reset();
        circuits.processStereo(tL.data(), tR.data(), kSamples, params);
        double mag = bumbler_test::computeFourierBinMagnitude(tL.data(), kSamples, 3000.0, kSampleRate);
        std::cout << "    Tone = " << std::fixed << std::setprecision(2) << t << " -> 3 kHz Mag = " << mag << "\n";
        if (lastMag >= 0.0 && mag < lastMag - 1.0e-6) {
            isMonotonic = false;
        }
        lastMag = mag;
    }
    CHALLENGE_ASSERT(isMonotonic, "Tone control must monotonically increase 3 kHz spectral power");

    // ------------------------------------------------------------------------
    // C. Quadratic Saturation Negative Excursion Turnaround Guard Probing
    // ------------------------------------------------------------------------
    {
        std::cout << "  Probing quadratic turnaround guard on extreme negative inputs (-5.0f, -10.0f)...\n";
        bumbler::DistortionUnit dist;
        dist.prepare(kSampleRate);

        const float extremeInputs[] = { -3.3f, -3.4f, -4.0f, -5.0f, -10.0f, -50.0f };
        for (float inVal : extremeInputs) {
            float out = dist.processSample(inVal, 1.0f, 0.5f, true);
            CHALLENGE_ASSERT(!std::isnan(out) && !std::isinf(out), "Distortion produced NaN/Inf on extreme negative input");
            CHALLENGE_ASSERT(out >= -1.05f && out <= 1.05f, "Distortion exceeded ceiling bounds on extreme negative input");
        }
    }
}

// ============================================================================
// CHALLENGE SUITE 4: W.Noise Periodic Cyclicity vs True White Noise
// ============================================================================
void challenge_wnoise_autocorrelation() {
    std::cout << "\n=======================================================\n";
    std::cout << " [CHALLENGE 4] W.Noise Periodic Cyclicity vs White Noise\n";
    std::cout << "=======================================================\n";

    // ------------------------------------------------------------------------
    // Part A: Direct bumbler::NoiseEngine Verification
    // ------------------------------------------------------------------------
    {
        constexpr int kSamples = 4096;
        bumbler::NoiseEngine noiseEng;

        // 1. Vintage LFSR Mode (1024-sample period)
        std::vector<float> vintageBuf(kSamples);
        noiseEng.reset();
        noiseEng.render(vintageBuf.data(), kSamples, false);

        double r1024_vintage = computeNormalizedAutocorr(vintageBuf.data(), kSamples, 1024);
        double r2048_vintage = computeNormalizedAutocorr(vintageBuf.data(), kSamples, 2048);
        double r512_vintage = computeNormalizedAutocorr(vintageBuf.data(), kSamples, 512);

        // Check DC mean
        double sumDC = 0.0;
        for (float v : vintageBuf) sumDC += v;
        double dcMean = std::abs(sumDC / kSamples);

        std::cout << "  [NoiseEngine Vintage] R[1024] / R[0] = " << std::fixed << std::setprecision(6) << r1024_vintage << " (Target ~1.000)\n";
        std::cout << "  [NoiseEngine Vintage] R[2048] / R[0] = " << r2048_vintage << " (Target ~1.000)\n";
        std::cout << "  [NoiseEngine Vintage] R[512]  / R[0] = " << r512_vintage << " (Half-period, Target < 0.20)\n";
        std::cout << "  [NoiseEngine Vintage] DC Mean        = " << dcMean << " (Target < 1.0e-5)\n";

        CHALLENGE_ASSERT_NEAR(r1024_vintage, 1.000000, 0.001, "Vintage noise must have R[1024]/R[0] ~ 1.000");
        CHALLENGE_ASSERT_NEAR(r2048_vintage, 1.000000, 0.001, "Vintage noise must have R[2048]/R[0] ~ 1.000");
        CHALLENGE_ASSERT(std::abs(r512_vintage) < 0.20, "Vintage noise half-period lag 512 must not be strongly correlated");
        CHALLENGE_ASSERT(dcMean < 1.0e-5, "Vintage noise must have zero DC mean");

        // 2. White Noise Mode (XorShift32)
        constexpr int kWhiteSamples = 16384;
        std::vector<float> whiteBuf(kWhiteSamples);
        noiseEng.reset();
        noiseEng.render(whiteBuf.data(), kWhiteSamples, true);

        double r1024_white = std::abs(computeNormalizedAutocorr(whiteBuf.data(), kWhiteSamples, 1024));
        std::cout << "  [NoiseEngine White]   |R[1024]| / R[0] = " << r1024_white << " (Threshold < 0.15)\n";

        CHALLENGE_ASSERT(r1024_white < 0.15, "White noise lag-1024 correlation must be < 0.15 (got " + std::to_string(r1024_white) + ")");

        // Multi-seed check across 10 independent noise seeds
        double maxWhiteCorr = 0.0;
        for (uint32_t s = 1; s <= 10; ++s) {
            noiseEng.seed(s * 0x7654321u);
            noiseEng.render(whiteBuf.data(), kWhiteSamples, true);
            double r = std::abs(computeNormalizedAutocorr(whiteBuf.data(), kWhiteSamples, 1024));
            if (r > maxWhiteCorr) maxWhiteCorr = r;
        }
        std::cout << "  [NoiseEngine White]   Max |R[1024]| across 10 seeds = " << maxWhiteCorr << "\n";
        CHALLENGE_ASSERT(maxWhiteCorr < 0.15, "White noise across 10 seeds must remain < 0.15 at lag 1024");
    }

    // ------------------------------------------------------------------------
    // Part B: Direct bumbler::BumblerOscillator Verification
    // ------------------------------------------------------------------------
    {
        constexpr int kSamples = 4096;
        bumbler::BumblerOscillator osc;
        osc.prepare(48000.0);

        // Vintage Noise Oscillator
        std::vector<float> oscVintage(kSamples);
        osc.reset(0.0f);
        for (int i = 0; i < kSamples; ++i) {
            oscVintage[i] = osc.process(bumbler::OscWaveform::Noise, 0.5f, bumbler::WNoiseMode::Vintage, 0.0f);
        }
        double oscR1024_v = computeNormalizedAutocorr(oscVintage.data(), kSamples, 1024);
        std::cout << "  [BumblerOscillator Vintage] R[1024] / R[0] = " << oscR1024_v << " (Target ~1.000)\n";
        CHALLENGE_ASSERT_NEAR(oscR1024_v, 1.000000, 0.001, "Oscillator vintage noise R[1024]/R[0] must be ~ 1.000");

        // White Noise Oscillator
        constexpr int kOscWhiteSamples = 16384;
        std::vector<float> oscWhite(kOscWhiteSamples);
        osc.reset(0.0f);
        for (int i = 0; i < kOscWhiteSamples; ++i) {
            oscWhite[i] = osc.process(bumbler::OscWaveform::Noise, 0.5f, bumbler::WNoiseMode::White, 0.0f);
        }
        double oscR1024_w = std::abs(computeNormalizedAutocorr(oscWhite.data(), kOscWhiteSamples, 1024));
        std::cout << "  [BumblerOscillator White]   |R[1024]| / R[0] = " << oscR1024_w << " (Threshold < 0.15)\n";
        CHALLENGE_ASSERT(oscR1024_w < 0.15, "Oscillator white noise R[1024]/R[0] must be < 0.15");
    }
}

// ============================================================================
// CHALLENGE SUITE 5: Real-Time Memory Safety, DC Blocker & Volume Control
// ============================================================================
void challenge_realtime_safety_and_master() {
    std::cout << "\n=======================================================\n";
    std::cout << " [CHALLENGE 5] Real-Time Safety, DC Blocker & Volume\n";
    std::cout << "=======================================================\n";

    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 16384;

    bumbler::CharacterCircuits circuits;
    circuits.prepare(kSampleRate, 512);

    // 1. Zero Allocation Verification in CharacterCircuits::processStereo
    {
        std::vector<float> bufL(512, 0.5f);
        std::vector<float> bufR(512, 0.5f);
        bumbler::ParameterSnapshot params;
        params.driveEnabled = 1.0f;
        params.driveAmount = 0.8f;
        params.driveTone = 0.5f;
        params.dualMode = 1.0f;
        params.masterVolume = 1.0f;

        // Warm up
        circuits.processStereo(bufL.data(), bufR.data(), 512, params);

        gAllocationCountM4.store(0);
        gAllocatedBytesM4.store(0);
        gTrackAllocationsM4.store(true);

        for (int b = 0; b < 100; ++b) {
            circuits.processStereo(bufL.data(), bufR.data(), 512, params);
        }

        gTrackAllocationsM4.store(false);
        size_t allocs = gAllocationCountM4.load();
        std::cout << "  Allocations during 100 blocks of processStereo: " << allocs << "\n";
        CHALLENGE_ASSERT(allocs == 0, "CharacterCircuits::processStereo performed heap allocation in audio path");
    }

    // 2. DC Blocker Transient Rejection
    {
        std::vector<float> dcL(kSamples), dcR(kSamples);
        bumbler_test::generateSine(dcL.data(), kSamples, 1000.0, kSampleRate, 0.2f);
        for (int i = 0; i < kSamples; ++i) {
            dcL[i] += 0.5f; // Heavy +0.5f DC offset
            dcR[i] = dcL[i];
        }

        bumbler::ParameterSnapshot params;
        params.driveEnabled = 0.0f;
        params.dualMode = 0.0f;
        params.masterVolume = 1.0f;

        circuits.reset();
        circuits.processStereo(dcL.data(), dcR.data(), kSamples, params);

        // Measure residual DC over final 4096 samples
        double dcSum = 0.0;
        for (int i = kSamples - 4096; i < kSamples; ++i) dcSum += dcL[i];
        double residualDC = std::abs(dcSum / 4096.0);

        std::cout << "  Residual DC after DC Blocker: " << residualDC << " (Threshold < 1.0e-3)\n";
        CHALLENGE_ASSERT(residualDC < 1.0e-3, "DC blocker failed to eliminate +0.5f DC bias");
    }

    // 3. Master Volume Muting & Ceiling Clamp
    {
        std::vector<float> sineL(1024), sineR(1024);
        bumbler_test::generateSine(sineL.data(), 1024, 440.0, kSampleRate, 0.7f);
        sineR = sineL;

        bumbler::ParameterSnapshot params;
        params.driveEnabled = 0.0f;
        params.dualMode = 0.0f;

        // Master Volume = 0.0 (Silence)
        params.masterVolume = 0.0f;
        circuits.reset();
        circuits.processStereo(sineL.data(), sineR.data(), 1024, params);
        float peakZero = bumbler_test::computePeak(sineL.data(), 1024);
        std::cout << "  Master Volume = 0.0 Peak: " << peakZero << " (Target 0.0)\n";
        CHALLENGE_ASSERT(peakZero == 0.0f, "Master Volume = 0.0 did not achieve absolute silence");

        // Master Volume = 2.0 (Boost with ceiling check)
        bumbler_test::generateSine(sineL.data(), 1024, 440.0, kSampleRate, 0.8f);
        sineR = sineL;
        params.masterVolume = 2.0f;
        circuits.reset();
        circuits.processStereo(sineL.data(), sineR.data(), 1024, params);
        float peakBoost = std::max(bumbler_test::computePeak(sineL.data(), 1024),
                                   bumbler_test::computePeak(sineR.data(), 1024));
        std::cout << "  Master Volume = 2.0 Boosted Peak: " << peakBoost << " (Ceiling <= 1.05f)\n";
        CHALLENGE_ASSERT(peakBoost <= 1.05f, "Master Volume = 2.0 exceeded safety ceiling of 1.05f");
    }

    // 4. Denormal Immunity
    {
        std::vector<float> denormL(512, 1.0e-35f);
        std::vector<float> denormR(512, 1.0e-35f);
        bumbler::ParameterSnapshot params;
        params.driveEnabled = 0.0f;
        params.dualMode = 0.0f;
        params.masterVolume = 1.0f;

        circuits.processStereo(denormL.data(), denormR.data(), 512, params);
        bool hasSubL = bumbler_test::containsSubnormals(denormL.data(), 512);
        bool hasSubR = bumbler_test::containsSubnormals(denormR.data(), 512);

        std::cout << "  Denormals flushed from output: " << (!hasSubL && !hasSubR ? "YES" : "NO") << "\n";
        CHALLENGE_ASSERT(!hasSubL && !hasSubR, "Subnormal numbers escaped into output stream");
    }
}

// ============================================================================
// MAIN RUNNER & BINARY VERDICT
// ============================================================================
int main() {
    std::cout << "======================================================================\n";
    std::cout << "    Bumbler XD: Milestone 4 Empirical Acoustic & Statistical Suite    \n";
    std::cout << "          Adversarial Challenger Target: challenger_m4_2               \n";
    std::cout << "======================================================================\n";

    challenge_dual_mode_decorrelation();
    challenge_analog_mode_statistics();
    challenge_distortion_harmonics_and_tone();
    challenge_wnoise_autocorrelation();
    challenge_realtime_safety_and_master();

    std::cout << "\n======================================================================\n";
    std::cout << "                        CHALLENGE AUDIT SUMMARY                       \n";
    std::cout << "======================================================================\n";
    std::cout << "  Total Assertions Checked: " << gTotalTests << "\n";
    std::cout << "  Passed Assertions:        " << gPassedTests << "\n";
    std::cout << "  Failed Assertions:        " << gFailedTests << "\n";
    std::cout << "----------------------------------------------------------------------\n";

    if (gFailedTests == 0) {
        std::cout << "  VERDICT: [APPROVE] - All empirical acoustic & statistical\n";
        std::cout << "                       characteristics meet or exceed specifications.\n";
    } else {
        std::cout << "  VERDICT: [REJECT] - " << gFailedTests << " empirical assertions failed!\n";
    }
    std::cout << "======================================================================\n";

    return (gFailedTests == 0) ? 0 : 1;
}
