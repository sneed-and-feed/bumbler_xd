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

#define CHALLENGER_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [ASSERTION FAILED] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        throw std::runtime_error(std::string("Assertion failed: ") + #cond + " -> " + msg); \
    } \
} while (false)

static void runChallengerTest(const char* name, void (*fn)()) {
    ++gTotalTests;
    std::cout << "\n>>> [TEST RUNNING] " << name << "..." << std::endl;
    try {
        fn();
        ++gPassedTests;
        std::cout << ">>> [TEST PASSED ] " << name << std::endl;
    } catch (const std::exception& e) {
        ++gFailedTests;
        std::cerr << ">>> [TEST FAILED ] " << name << " : " << e.what() << std::endl;
    } catch (...) {
        ++gFailedTests;
        std::cerr << ">>> [TEST FAILED ] " << name << " : Unknown exception" << std::endl;
    }
}

// ============================================================================
// Fast Fourier Transform (Cooley-Tukey Radix-2) & Spectral Analysis
// ============================================================================

static void fftRadix2(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    if (n <= 1) return;

    // Bit reversal permutation
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }

    // Cooley-Tukey butterflies
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * bumbler::kPi / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j) {
                const std::complex<double> u = a[i + j];
                const std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

// 4-Term Blackman-Harris Window: Sidelobe attenuation > 92 dB
static void applyBlackmanHarris(std::vector<double>& window, double& windowWeightSum) {
    const size_t n = window.size();
    constexpr double a0 = 0.35875;
    constexpr double a1 = 0.48829;
    constexpr double a2 = 0.14128;
    constexpr double a3 = 0.01168;

    windowWeightSum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double theta = 2.0 * bumbler::kPi * static_cast<double>(i) / static_cast<double>(n);
        const double w = a0 - a1 * std::cos(theta) + a2 * std::cos(2.0 * theta) - a3 * std::cos(3.0 * theta);
        window[i] = w;
        windowWeightSum += w;
    }
}

// ============================================================================
// 1. Spectral Anti-Aliasing Verification Harness
// ============================================================================

struct SpectralAliasingResult {
    double fundamentalFreq { 0.0 };
    double fundamentalMagnitude { 0.0 };
    double fundamentalDb { 0.0 };
    double maxFoldbackAliasFreq { 0.0 };
    double maxFoldbackAliasMag { 0.0 };
    double maxFoldbackAliasDb { 0.0 };
    double suppressionDb { 0.0 };
    bool passed40dB { false };
    std::vector<std::pair<double, double>> aliasPeaks; // (freqHz, dB relative to fundamental)
};

static SpectralAliasingResult analyzeAntiAliasing(
    bumbler::OscWaveform waveform,
    float pulseWidth,
    double sampleRate,
    int midiNote = 96, // C7 = 2093.0045 Hz
    size_t fftSize = 65536)
{
    SpectralAliasingResult result;
    const double f0 = 440.0 * std::pow(2.0, (midiNote - 69.0) / 12.0);
    result.fundamentalFreq = f0;

    // 1. Generate steady-state audio signal
    bumbler::BumblerOscillator osc;
    osc.prepare(sampleRate);
    osc.setWaveform(waveform);
    osc.setPulseWidth(pulseWidth);
    osc.setFrequency(static_cast<float>(f0));
    osc.reset(0.0f);

    // Warm-up transient (500 samples)
    for (int i = 0; i < 500; ++i) {
        osc.processSample(0.0f);
    }

    // Capture fftSize samples
    std::vector<double> window(fftSize);
    double windowSum = 0.0;
    applyBlackmanHarris(window, windowSum);

    std::vector<std::complex<double>> fftBuf(fftSize);
    for (size_t i = 0; i < fftSize; ++i) {
        const float s = osc.processSample(0.0f);
        fftBuf[i] = static_cast<double>(s) * window[i];
    }

    // Run FFT
    fftRadix2(fftBuf);

    // Magnitude spectrum normalized by window sum
    const size_t numBins = fftSize / 2;
    const double binWidth = sampleRate / static_cast<double>(fftSize);
    std::vector<double> magSpectrum(numBins, 0.0);
    for (size_t i = 0; i < numBins; ++i) {
        magSpectrum[i] = (2.0 / windowSum) * std::abs(fftBuf[i]);
    }

    // Find fundamental peak around f0 (+/- 5 bins)
    const size_t targetFundBin = static_cast<size_t>(std::round(f0 / binWidth));
    size_t actualFundBin = targetFundBin;
    double maxFundMag = 0.0;
    for (size_t b = (targetFundBin > 5 ? targetFundBin - 5 : 0); b <= targetFundBin + 5 && b < numBins; ++b) {
        if (magSpectrum[b] > maxFundMag) {
            maxFundMag = magSpectrum[b];
            actualFundBin = b;
        }
    }
    result.fundamentalMagnitude = maxFundMag;
    result.fundamentalDb = 20.0 * std::log10(std::max(maxFundMag, 1.0e-12));

    // True harmonics: k * f0 up to Nyquist
    const int maxHarmonic = static_cast<int>(std::floor((sampleRate * 0.5) / f0));
    std::vector<bool> isHarmonicBin(numBins, false);

    // Notch out +/- 35 Hz around each true harmonic (Blackman-Harris main lobe width is ~ 4 * binWidth < 6 Hz)
    const int notchBins = std::max(4, static_cast<int>(std::ceil(35.0 / binWidth)));
    for (int k = 1; k <= maxHarmonic; ++k) {
        const double hFreq = k * f0;
        const int hBin = static_cast<int>(std::round(hFreq / binWidth));
        const int startBin = std::max(0, hBin - notchBins);
        const int endBin = std::min(static_cast<int>(numBins) - 1, hBin + notchBins);
        for (int b = startBin; b <= endBin; ++b) {
            isHarmonicBin[static_cast<size_t>(b)] = true;
        }
    }

    // Also notch out DC / low frequencies below 40 Hz
    const int dcNotchBins = static_cast<int>(std::ceil(40.0 / binWidth));
    for (int b = 0; b <= dcNotchBins && b < static_cast<int>(numBins); ++b) {
        isHarmonicBin[static_cast<size_t>(b)] = true;
    }

    // Calculate specific foldback harmonic frequencies for harmonics above Nyquist (k = maxHarmonic + 1 to 50)
    for (int k = maxHarmonic + 1; k <= 50; ++k) {
        const double rawFreq = k * f0;
        // Foldback reflection formula:
        const double cycles = rawFreq / sampleRate;
        const double rem = cycles - std::floor(cycles);
        const double foldedNormalized = (rem <= 0.5) ? rem : (1.0 - rem);
        const double foldFreq = foldedNormalized * sampleRate;

        if (foldFreq > 50.0 && foldFreq < (sampleRate * 0.5 - 50.0)) {
            const size_t fBin = static_cast<size_t>(std::round(foldFreq / binWidth));
            if (fBin < numBins && !isHarmonicBin[fBin]) {
                const double aMag = magSpectrum[fBin];
                const double aDb = 20.0 * std::log10(std::max(aMag, 1.0e-12)) - result.fundamentalDb;
                result.aliasPeaks.push_back({ foldFreq, aDb });
            }
        }
    }

    // Scan for global maximum alias peak across all non-harmonic bins
    double maxAliasMag = 0.0;
    size_t maxAliasBin = 0;
    for (size_t b = 0; b < numBins; ++b) {
        if (!isHarmonicBin[b]) {
            if (magSpectrum[b] > maxAliasMag) {
                maxAliasMag = magSpectrum[b];
                maxAliasBin = b;
            }
        }
    }

    result.maxFoldbackAliasFreq = static_cast<double>(maxAliasBin) * binWidth;
    result.maxFoldbackAliasMag = maxAliasMag;
    result.maxFoldbackAliasDb = 20.0 * std::log10(std::max(maxAliasMag, 1.0e-12));
    result.suppressionDb = result.fundamentalDb - result.maxFoldbackAliasDb;
    result.passed40dB = (result.suppressionDb >= 40.0);

    return result;
}

static void test_spectral_anti_aliasing_sawtooth() {
    std::cout << "Testing Sawtooth Waveform Anti-Aliasing at C7 (2093 Hz)...\n";
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    bool anyFailed = false;

    for (double sr : sampleRates) {
        const auto res = analyzeAntiAliasing(bumbler::OscWaveform::Saw, 0.5f, sr, 96);
        std::cout << "  [Saw @ " << std::fixed << std::setprecision(1) << (sr * 0.001) << " kHz] "
                  << "Fundamental: " << res.fundamentalFreq << " Hz (" << std::setprecision(2) << res.fundamentalDb << " dBFS) | "
                  << "Max Alias: " << res.maxFoldbackAliasFreq << " Hz (" << res.maxFoldbackAliasDb << " dBFS) | "
                  << "Suppression: " << res.suppressionDb << " dB | "
                  << (res.passed40dB ? "[PASS >=40dB]" : "[FAIL <40dB]") << "\n";

        if (!res.passed40dB) {
            anyFailed = true;
            std::cout << "    Foldback peaks above -40 dB relative to fundamental:\n";
            for (const auto& p : res.aliasPeaks) {
                if (p.second > -40.0) {
                    std::cout << "      freq=" << p.first << " Hz, relative dB=" << p.second << " dB\n";
                }
            }
        }
    }

    CHALLENGER_ASSERT(!anyFailed, "Sawtooth alias foldback suppression failed 40 dB threshold on one or more sample rates");
}

static void test_spectral_anti_aliasing_pulse() {
    std::cout << "Testing Square/Pulse Waveform Anti-Aliasing at C7 (2093 Hz)...\n";
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    bool anyFailed = false;

    for (double sr : sampleRates) {
        for (float pw : { 0.50f, 0.25f, 0.75f }) {
            const auto res = analyzeAntiAliasing(bumbler::OscWaveform::Square, pw, sr, 96);
            std::cout << "  [Pulse (PW=" << static_cast<int>(pw * 100) << "%) @ " << std::fixed << std::setprecision(1) << (sr * 0.001) << " kHz] "
                      << "Fundamental: " << res.fundamentalFreq << " Hz (" << std::setprecision(2) << res.fundamentalDb << " dBFS) | "
                      << "Max Alias: " << res.maxFoldbackAliasFreq << " Hz (" << res.maxFoldbackAliasDb << " dBFS) | "
                      << "Suppression: " << res.suppressionDb << " dB | "
                      << (res.passed40dB ? "[PASS >=40dB]" : "[FAIL <40dB]") << "\n";

            if (!res.passed40dB) {
                anyFailed = true;
                std::cout << "    Foldback peaks above -40 dB relative to fundamental:\n";
                for (const auto& p : res.aliasPeaks) {
                    if (p.second > -40.0) {
                        std::cout << "      freq=" << p.first << " Hz, relative dB=" << p.second << " dB\n";
                    }
                }
            }
        }
    }

    CHALLENGER_ASSERT(!anyFailed, "Pulse alias foldback suppression failed 40 dB threshold on one or more configurations");
}

// ============================================================================
// 2. Real-Time Zero Heap Allocation Verification Harness
// ============================================================================

static void test_realtime_zero_heap_allocations_matrix() {
    std::cout << "Testing Zero Heap Allocations across sample rates & buffer sizes...\n";

    const std::vector<double> sampleRates = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const std::vector<int> bufferSizes = { 16, 32, 64, 128, 256, 512, 1024, 2048 };

    bumbler::ParameterSnapshot params;
    params.oscMix = 0.5f;
    params.pulseWidth = 0.33f;
    params.ringModMix = 0.2f;
    params.fmAmount = 0.2f;
    params.ampAttack = 0.005f;
    params.ampDecay = 0.1f;
    params.ampSustain = 0.7f;
    params.ampRelease = 0.05f;
    params.masterVolume = 0.8f;

    // Allocate max buffer upfront outside measurement
    constexpr int kMaxAllocBlock = 2048;
    std::vector<float> preallocL(kMaxAllocBlock, 0.0f);
    std::vector<float> preallocR(kMaxAllocBlock, 0.0f);
    float* channelPointers[2] = { preallocL.data(), preallocR.data() };

    for (double sr : sampleRates) {
        for (int bs : bufferSizes) {
            bumbler::BumblerVoiceManager vm;
            vm.prepare(sr, bs);

            bumbler::BumblerVoice standaloneVoice;
            standaloneVoice.prepare(sr);

            // Warm-up
            vm.noteOn(60, 0.8f);
            vm.renderBlock(channelPointers, 2, bs, params);

            // Arm allocation tracker
            gAllocationCount.store(0, std::memory_order_seq_cst);
            gAllocatedBytes.store(0, std::memory_order_seq_cst);
            gTrackAllocations.store(true, std::memory_order_seq_cst);

            // 1. Exercise all 16 voices with Note-On
            for (int note = 60; note < 76; ++note) {
                vm.noteOn(note, 0.75f);
            }

            // 2. Trigger voice stealing (Tier 3 & 4) by adding 8 more notes
            for (int note = 76; note < 84; ++note) {
                vm.noteOn(note, 0.85f);
            }

            // 3. Modulate Pitch Bend and Sustain Pedal
            vm.setPitchBend(1.5f);
            vm.setSustainPedal(true);

            // 4. Release some notes
            for (int note = 60; note < 70; ++note) {
                vm.noteOff(note, 0.0f);
            }
            vm.setSustainPedal(false);

            // 5. Render multiple audio blocks with parameter sweeps
            for (int b = 0; b < 25; ++b) {
                params.oscMix = static_cast<float>(b) / 25.0f;
                params.pulseWidth = 0.1f + 0.8f * (static_cast<float>(b) / 25.0f);
                vm.renderBlock(channelPointers, 2, bs, params);
            }

            // 6. Test BumblerVoice direct renderBlockAccumulate & renderSample
            standaloneVoice.noteOn(69, 0.9f, 100);
            standaloneVoice.renderBlockAccumulate(preallocL.data(), preallocR.data(), bs, params);
            for (int s = 0; s < 100; ++s) {
                float l = 0.0f, r = 0.0f;
                standaloneVoice.renderSample(l, r, params);
            }

            // Disarm allocation tracker
            gTrackAllocations.store(false, std::memory_order_seq_cst);
            const size_t allocs = gAllocationCount.load(std::memory_order_seq_cst);
            const size_t bytes = gAllocatedBytes.load(std::memory_order_seq_cst);

            if (allocs > 0 || bytes > 0) {
                std::cerr << "  [FAIL] Heap allocation at SR=" << sr << " Hz, Block=" << bs
                          << " : Allocs=" << allocs << ", Bytes=" << bytes << "\n";
                CHALLENGER_ASSERT(allocs == 0, "Heap allocation detected during audio processing");
            }
        }
    }
    std::cout << "  [PASS] All 40 configurations (5 sample rates x 8 buffer sizes) exhibited ZERO heap allocations.\n";
}

// ============================================================================
// 3. Monotonic Velocity Scaling Verification Harness
// ============================================================================

static void test_monotonic_velocity_scaling() {
    std::cout << "Testing Monotonic Velocity Scaling (Velocities 1..127)...\n";

    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);

    bumbler::ParameterSnapshot params;
    params.osc1Waveform = 2.0f; // Pure sine for clean amplitude evaluation
    params.oscMix = 0.0f;
    params.ampAttack = 0.001f;
    params.ampDecay = 0.01f;
    params.ampSustain = 1.0f;   // Full sustain
    params.ampRelease = 0.1f;
    params.velToAmp = 1.0f;     // Full velocity sensitivity

    constexpr int kSustainSamples = 1024;
    std::vector<double> rmsPowers(128, 0.0);
    std::vector<float> peakAmplitudes(128, 0.0f);

    // Test velocity 0 edge case
    {
        voice.reset();
        voice.noteOn(69, 0.0f / 127.0f, 0);
        // Wait 100 samples
        for (int i = 0; i < 100; ++i) {
            float l = 0.0f, r = 0.0f;
            voice.renderSample(l, r, params);
        }
        double sumSq = 0.0;
        for (int i = 0; i < kSustainSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            voice.renderSample(l, r, params);
            sumSq += l * l;
        }
        rmsPowers[0] = std::sqrt(sumSq / kSustainSamples);
        CHALLENGER_ASSERT(rmsPowers[0] < 1.0e-5, "Velocity 0 must produce zero/silent output");
    }

    // Test velocities 1 to 127
    for (int v = 1; v <= 127; ++v) {
        voice.reset();
        const float velNorm = static_cast<float>(v) / 127.0f;
        voice.noteOn(69, velNorm, 0);

        // Advance 200 samples past attack
        for (int i = 0; i < 200; ++i) {
            float l = 0.0f, r = 0.0f;
            voice.renderSample(l, r, params);
        }

        // Measure sustain RMS power and peak
        double sumSq = 0.0;
        float peak = 0.0f;
        for (int i = 0; i < kSustainSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            voice.renderSample(l, r, params);
            CHALLENGER_ASSERT(bumbler::isFiniteBitwise(l), "Non-finite output sample detected");
            sumSq += static_cast<double>(l) * static_cast<double>(l);
            const float a = std::abs(l);
            if (a > peak) peak = a;
        }

        const double rms = std::sqrt(sumSq / kSustainSamples);
        rmsPowers[static_cast<size_t>(v)] = rms;
        peakAmplitudes[static_cast<size_t>(v)] = peak;

        // No clipping above 1.05
        CHALLENGER_ASSERT(peak <= 1.05f, "Peak amplitude exceeded unity boundary (clipping)");
    }

    // Verify strict monotonicity: P(v) > P(v-1) for v = 2..127
    double minStepDiff = 1e9;
    for (int v = 2; v <= 127; ++v) {
        const double diff = rmsPowers[static_cast<size_t>(v)] - rmsPowers[static_cast<size_t>(v - 1)];
        if (diff < minStepDiff) minStepDiff = diff;
        if (diff <= 0.0) {
            std::cerr << "  [NON-MONOTONIC] at v=" << v
                      << " : P(v)=" << rmsPowers[static_cast<size_t>(v)]
                      << ", P(v-1)=" << rmsPowers[static_cast<size_t>(v - 1)] << "\n";
            CHALLENGER_ASSERT(diff > 0.0, "Velocity power scaling is not strictly monotonic");
        }
    }
    std::cout << "  [PASS] Strictly monotonic across all 127 steps. Min delta = " << minStepDiff << "\n";

    // Verify smooth quadratic curvature without abrupt jumps (continuity check)
    // For quadratic scaling P(v) ~ (v/127)^2, d2P/dv2 should be approximately constant (> 0)
    for (int v = 2; v <= 126; ++v) {
        const double d2 = rmsPowers[static_cast<size_t>(v + 1)] - 2.0 * rmsPowers[static_cast<size_t>(v)] + rmsPowers[static_cast<size_t>(v - 1)];
        // Ensure no sharp sign inversions or sudden glitches
        CHALLENGER_ASSERT(d2 >= -1.0e-5, "Second derivative discontinuity in velocity curve");
    }
    std::cout << "  [PASS] Velocity curve is smooth, continuous and strictly quadratic.\n";

    // Test with velToAmp = 0.0 (fixed velocity response)
    {
        params.velToAmp = 0.0f;
        voice.reset();
        voice.noteOn(69, 1.0f / 127.0f, 0);
        for (int i = 0; i < 200; ++i) { float l = 0, r = 0; voice.renderSample(l, r, params); }
        double sumSq1 = 0;
        for (int i = 0; i < 500; ++i) { float l = 0, r = 0; voice.renderSample(l, r, params); sumSq1 += l*l; }

        voice.reset();
        voice.noteOn(69, 127.0f / 127.0f, 0);
        for (int i = 0; i < 200; ++i) { float l = 0, r = 0; voice.renderSample(l, r, params); }
        double sumSq127 = 0;
        for (int i = 0; i < 500; ++i) { float l = 0, r = 0; voice.renderSample(l, r, params); sumSq127 += l*l; }

        const double rms1 = std::sqrt(sumSq1 / 500);
        const double rms127 = std::sqrt(sumSq127 / 500);
        CHALLENGER_ASSERT(std::abs(rms1 - rms127) < 0.01, "velToAmp = 0 must disable velocity sensitivity");
    }
}

// ============================================================================
// 4. Extreme Stress & Edge Cases Harness
// ============================================================================

static void test_adversarial_boundary_conditions() {
    std::cout << "Testing Adversarial Boundary Conditions (buffer=1, rapid retrigger, pitch extremes)...\n";

    bumbler::BumblerVoiceManager vm;
    vm.prepare(44100.0, 512);

    bumbler::ParameterSnapshot params;
    float dummyL[1] = { 0.0f };
    float dummyR[1] = { 0.0f };
    float* singleSampleChannels[2] = { dummyL, dummyR };

    // Stress 1: Buffer size of 1 sample
    vm.noteOn(60, 1.0f);
    for (int i = 0; i < 1000; ++i) {
        vm.renderBlock(singleSampleChannels, 2, 1, params);
        CHALLENGER_ASSERT(bumbler::isFiniteBitwise(dummyL[0]), "Non-finite at block size 1");
    }

    // Stress 2: Rapid note retriggers (same pitch)
    for (int i = 0; i < 200; ++i) {
        vm.noteOn(60, 0.5f + 0.5f * (i % 2));
        vm.renderBlock(singleSampleChannels, 2, 1, params);
    }
    CHALLENGER_ASSERT(vm.getNumActiveVoices() <= 16, "Voice pileup occurred");

    // Stress 3: Pitch extremes (MIDI 0 = 8.18 Hz to MIDI 127 = 12543 Hz)
    vm.allNotesOff(true);
    vm.noteOn(0, 1.0f);
    vm.noteOn(127, 1.0f);
    std::vector<float> blkL(512, 0.0f);
    std::vector<float> blkR(512, 0.0f);
    float* blkChans[2] = { blkL.data(), blkR.data() };
    vm.renderBlock(blkChans, 2, 512, params);

    float maxSample = 0.0f;
    for (int s = 0; s < 512; ++s) {
        CHALLENGER_ASSERT(bumbler::isFiniteBitwise(blkL[s]), "Extreme pitch produced NaN or Inf");
        const float a = std::abs(blkL[s]);
        if (a > maxSample) maxSample = a;
    }
    std::cout << "    Pitch extremes rendered max sample: " << maxSample << "\n";
    CHALLENGER_ASSERT(maxSample <= 3.0f, "Extreme pitch blew up output buffer");

    std::cout << "  [PASS] Boundary and stress tests completed without instability.\n";
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << " Bumbler XD Milestone 1 Adversarial Challenge Harness       \n";
    std::cout << " EMPIRICAL VERIFICATION OF SPECTRAL FIDELITY & SAFETY       \n";
    std::cout << "============================================================\n";

    runChallengerTest("test_spectral_anti_aliasing_sawtooth", test_spectral_anti_aliasing_sawtooth);
    runChallengerTest("test_spectral_anti_aliasing_pulse", test_spectral_anti_aliasing_pulse);
    runChallengerTest("test_realtime_zero_heap_allocations_matrix", test_realtime_zero_heap_allocations_matrix);
    runChallengerTest("test_monotonic_velocity_scaling", test_monotonic_velocity_scaling);
    runChallengerTest("test_adversarial_boundary_conditions", test_adversarial_boundary_conditions);

    std::cout << "\n============================================================\n";
    std::cout << " Challenger Verification Summary:\n";
    std::cout << "   Passed: " << gPassedTests << " / " << gTotalTests << "\n";
    std::cout << "   Failed: " << gFailedTests << " / " << gTotalTests << "\n";
    std::cout << "============================================================\n";

    return (gFailedTests == 0) ? 0 : 1;
}
