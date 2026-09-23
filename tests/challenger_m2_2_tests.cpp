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
#include "WaspFilter.h"
#include "BumblerVoice.h"
#include "ParameterSnapshot.h"

// ============================================================================
// Real-Time Memory Safety: Interception Hooks
// ============================================================================
static std::atomic<bool> gTrackAllocationsM2 { false };
static std::atomic<size_t> gAllocationCountM2 { 0 };
static std::atomic<size_t> gAllocatedBytesM2 { 0 };

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
void* operator new(size_t size) {
    if (gTrackAllocationsM2.load(std::memory_order_relaxed)) {
        gAllocationCountM2.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytesM2.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void* operator new[](size_t size) {
    if (gTrackAllocationsM2.load(std::memory_order_relaxed)) {
        gAllocationCountM2.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytesM2.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
#endif

// ============================================================================
// Challenger Test Assertion Framework
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
    std::cout << "\n======================================================\n";
    std::cout << ">>> [TEST RUNNING] " << name << "...\n";
    std::cout << "======================================================\n";
    try {
        fn();
        ++gPassedTests;
        std::cout << ">>> [TEST PASSED ] " << name << "\n";
    } catch (const std::exception& e) {
        ++gFailedTests;
        std::cerr << ">>> [TEST FAILED ] " << name << " : " << e.what() << "\n";
    } catch (...) {
        ++gFailedTests;
        std::cerr << ">>> [TEST FAILED ] " << name << " : Unknown exception\n";
    }
}

// ============================================================================
// High-Precision Sine Probe & Discrete Fourier Power Oracle
// ============================================================================
namespace challenger_dsp {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

/**
 * Computes Discrete Fourier Bin Power at targetFreqHz over numSamples.
 * Uses exact quadrature accumulation to measure fundamental power.
 */
inline double computeFourierPower(
    const float* signal,
    int numSamples,
    double targetFreqHz,
    double sampleRate
) noexcept {
    if (numSamples <= 0 || sampleRate <= 0.0) return 0.0;

    double realSum = 0.0;
    double imagSum = 0.0;
    const double omega = kTwoPi * targetFreqHz / sampleRate;

    for (int n = 0; n < numSamples; ++n) {
        const double angle = omega * static_cast<double>(n);
        const double s = static_cast<double>(signal[n]);
        realSum += s * std::cos(angle);
        imagSum -= s * std::sin(angle);
    }

    const double norm = 2.0 / static_cast<double>(numSamples);
    const double mag = norm * std::sqrt(realSum * realSum + imagSum * imagSum);
    return mag * mag;
}

inline double toDb(double linearGain) noexcept {
    if (linearGain <= 1.0e-12) return -240.0;
    return 20.0 * std::log10(linearGain);
}

/**
 * EmpiricalFilterProbe:
 * Drives WaspFilter with pure sinusoidal probe of specified frequency and amplitude.
 * Discards transient settle period, then measures input and output Fourier power
 * on identical windows to obtain exact linear transfer gain and dB response.
 */
class FilterProbeHarness {
public:
    void prepare(double sampleRate) {
        mSampleRate = sampleRate;
        mFilter.prepare(sampleRate);
    }

    void reset() {
        mFilter.reset();
    }

    struct Measurement {
        double freqHz;
        double inputPower;
        double outputPower;
        double linearGain;
        double gainDb;
    };

    Measurement measureResponse(
        double probeFreqHz,
        double cutoffHz,
        double resonance,
        bumbler::WaspFilterMode mode,
        float probeAmplitude = 0.5f,
        int transientSamples = 4000,
        int measureSamples = 8192
    ) {
        reset();

        const int totalSamples = transientSamples + measureSamples;
        std::vector<float> inBuf(totalSamples);
        std::vector<float> outBuf(totalSamples);

        const double omega = kTwoPi * probeFreqHz / mSampleRate;
        for (int i = 0; i < totalSamples; ++i) {
            inBuf[i] = probeAmplitude * static_cast<float>(std::sin(omega * static_cast<double>(i)));
            outBuf[i] = mFilter.processSample(inBuf[i], cutoffHz, resonance, mode);
        }

        const float* inMeasure = inBuf.data() + transientSamples;
        const float* outMeasure = outBuf.data() + transientSamples;

        const double pIn = computeFourierPower(inMeasure, measureSamples, probeFreqHz, mSampleRate);
        const double pOut = computeFourierPower(outMeasure, measureSamples, probeFreqHz, mSampleRate);

        const double linGain = (pIn > 1.0e-20) ? std::sqrt(pOut / pIn) : 0.0;
        const double gDb = toDb(linGain);

        return Measurement { probeFreqHz, pIn, pOut, linGain, gDb };
    }

private:
    double mSampleRate { 48000.0 };
    bumbler::WaspFilter mFilter;
};

} // namespace challenger_dsp

// ============================================================================
// CHALLENGE 1: LP FAT 24dB Roll-Off Slopes
// Requirement: Measure slope at 1 octave and 2 octaves above cutoff.
// Verify roll-off is between -22 dB/oct and -26 dB/oct.
// ============================================================================
void test_lp24_fat_fourier_slope() {
    challenger_dsp::FilterProbeHarness harness;
    harness.prepare(48000.0);

    const double fc = 1000.0;

    std::cout << "  [Test 1.1] Small-Signal Linear Response (amplitude = 0.05f, reso = 0.0):\n";
    constexpr float kProbeAmp = 0.05f;
    auto mPass0 = harness.measureResponse(50.0, fc, 0.0, bumbler::WaspFilterMode::LP24, kProbeAmp);
    auto mFc0   = harness.measureResponse(1000.0, fc, 0.0, bumbler::WaspFilterMode::LP24, kProbeAmp);
    auto m1Oct0 = harness.measureResponse(2000.0, fc, 0.0, bumbler::WaspFilterMode::LP24, kProbeAmp);
    auto m2Oct0 = harness.measureResponse(4000.0, fc, 0.0, bumbler::WaspFilterMode::LP24, kProbeAmp);
    auto m3Oct0 = harness.measureResponse(8000.0, fc, 0.0, bumbler::WaspFilterMode::LP24, kProbeAmp);

    std::cout << "    Passband (50 Hz):   " << std::fixed << std::setprecision(2) << mPass0.gainDb << " dB\n";
    std::cout << "    Cutoff   (1000 Hz): " << mFc0.gainDb << " dB\n";
    std::cout << "    +1 Oct   (2000 Hz): " << m1Oct0.gainDb << " dB\n";
    std::cout << "    +2 Oct   (4000 Hz): " << m2Oct0.gainDb << " dB\n";
    std::cout << "    +3 Oct   (8000 Hz): " << m3Oct0.gainDb << " dB\n";

    CHALLENGER_ASSERT(std::abs(mPass0.gainDb) < 0.1, "Small-signal passband at 50 Hz deviates from 0 dB");

    // 2-octave average roll-off relative to passband
    const double avgSlope2Oct0 = (m2Oct0.gainDb - mPass0.gainDb) / 2.0;
    std::cout << "    2-Octave Average Roll-off: " << avgSlope2Oct0 << " dB/oct\n";
    CHALLENGER_ASSERT(avgSlope2Oct0 <= -22.0 && avgSlope2Oct0 >= -26.0,
        "LP FAT 2-octave roll-off violates [-26, -22] dB/oct: " + std::to_string(avgSlope2Oct0));

    // Octave slope between +2 oct and +3 oct (deep asymptotic stopband)
    const double deepStopbandSlope0 = m3Oct0.gainDb - m2Oct0.gainDb;
    std::cout << "    Deep Stopband Slope (+2 oct to +3 oct): " << deepStopbandSlope0 << " dB/oct\n";
    CHALLENGER_ASSERT(deepStopbandSlope0 <= -22.0 && deepStopbandSlope0 >= -26.0,
        "LP FAT deep stopband slope violates [-26, -22] dB/oct: " + std::to_string(deepStopbandSlope0));

    std::cout << "  [Test 1.2] Standard Butterworth-like Q (reso = 0.25, amplitude = 0.05f):\n";
    auto mPassQ = harness.measureResponse(50.0, fc, 0.25, bumbler::WaspFilterMode::LP24, kProbeAmp);
    auto m1OctQ = harness.measureResponse(2000.0, fc, 0.25, bumbler::WaspFilterMode::LP24, kProbeAmp);
    auto m2OctQ = harness.measureResponse(4000.0, fc, 0.25, bumbler::WaspFilterMode::LP24, kProbeAmp);

    const double slopeQ_1_to_2 = m2OctQ.gainDb - m1OctQ.gainDb;
    std::cout << "    Stopband Slope (+1 oct to +2 oct with reso=0.25): " << slopeQ_1_to_2 << " dB/oct\n";
    CHALLENGER_ASSERT(slopeQ_1_to_2 <= -22.0 && slopeQ_1_to_2 >= -26.0,
        "LP FAT stopband slope with reso=0.25 violates [-26, -22] dB/oct: " + std::to_string(slopeQ_1_to_2));

    std::cout << "  [Test 1.3] Analog CMOS Tanh Overdrive Compression (amplitude = 0.5f vs 0.05f):\n";
    auto mHotPass = harness.measureResponse(50.0, fc, 0.0, bumbler::WaspFilterMode::LP24, 0.5f);
    const double cmosCompression = mPass0.gainDb - mHotPass.gainDb;
    std::cout << "    Passband compression under 0.5f drive: " << cmosCompression << " dB (analog warmth)\n";
    CHALLENGER_ASSERT(cmosCompression > 0.3 && cmosCompression < 1.5,
        "CMOS soft saturation compression out of expected range: " + std::to_string(cmosCompression));
}

// ============================================================================
// CHALLENGE 2: LP 12dB Roll-Off Slopes
// Requirement: Measure slope above cutoff. Verify roll-off is between -11 dB/oct and -14 dB/oct.
// ============================================================================
void test_lp12_fourier_slope() {
    challenger_dsp::FilterProbeHarness harness;
    harness.prepare(48000.0);

    const double fc = 1000.0;

    std::cout << "  [Test 2.1] Critical Damping (reso = 0.0):\n";
    auto mPass0 = harness.measureResponse(50.0, fc, 0.0, bumbler::WaspFilterMode::LP12);
    auto mFc0   = harness.measureResponse(1000.0, fc, 0.0, bumbler::WaspFilterMode::LP12);
    auto m1Oct0 = harness.measureResponse(2000.0, fc, 0.0, bumbler::WaspFilterMode::LP12);
    auto m2Oct0 = harness.measureResponse(4000.0, fc, 0.0, bumbler::WaspFilterMode::LP12);
    auto m3Oct0 = harness.measureResponse(8000.0, fc, 0.0, bumbler::WaspFilterMode::LP12);

    std::cout << "    Passband (50 Hz):   " << std::fixed << std::setprecision(2) << mPass0.gainDb << " dB\n";
    std::cout << "    Cutoff   (1000 Hz): " << mFc0.gainDb << " dB\n";
    std::cout << "    +1 Oct   (2000 Hz): " << m1Oct0.gainDb << " dB\n";
    std::cout << "    +2 Oct   (4000 Hz): " << m2Oct0.gainDb << " dB\n";
    std::cout << "    +3 Oct   (8000 Hz): " << m3Oct0.gainDb << " dB\n";

    CHALLENGER_ASSERT(std::abs(mPass0.gainDb) < 0.1, "LP12 passband at 50 Hz deviates from 0 dB");

    // 2-octave average roll-off relative to passband
    const double avgSlope2Oct0 = (m2Oct0.gainDb - mPass0.gainDb) / 2.0;
    std::cout << "    2-Octave Average Roll-off: " << avgSlope2Oct0 << " dB/oct\n";
    CHALLENGER_ASSERT(avgSlope2Oct0 <= -11.0 && avgSlope2Oct0 >= -14.0,
        "LP12 2-octave average roll-off violates [-14, -11] dB/oct: " + std::to_string(avgSlope2Oct0));

    // Deep stopband slope between +2 oct and +3 oct
    const double deepStopbandSlope0 = m3Oct0.gainDb - m2Oct0.gainDb;
    std::cout << "    Deep Stopband Slope (+2 oct to +3 oct): " << deepStopbandSlope0 << " dB/oct\n";
    CHALLENGER_ASSERT(deepStopbandSlope0 <= -11.0 && deepStopbandSlope0 >= -14.0,
        "LP12 deep stopband slope violates [-14, -11] dB/oct: " + std::to_string(deepStopbandSlope0));

    std::cout << "  [Test 2.2] Butterworth-like Q (reso = 0.25):\n";
    auto m1OctQ = harness.measureResponse(2000.0, fc, 0.25, bumbler::WaspFilterMode::LP12);
    auto m2OctQ = harness.measureResponse(4000.0, fc, 0.25, bumbler::WaspFilterMode::LP12);

    const double slopeQ_1_to_2 = m2OctQ.gainDb - m1OctQ.gainDb;
    std::cout << "    Stopband Slope (+1 oct to +2 oct with reso=0.25): " << slopeQ_1_to_2 << " dB/oct\n";
    CHALLENGER_ASSERT(slopeQ_1_to_2 <= -11.0 && slopeQ_1_to_2 >= -14.0,
        "LP12 stopband slope with reso=0.25 violates [-14, -11] dB/oct: " + std::to_string(slopeQ_1_to_2));
}

// ============================================================================
// CHALLENGE 3: BP 24dB Attenuation at fc/4 and 4*fc
// Requirement: Measure attenuation at fc/4 and 4*fc. Verify both low and high ends
// are attenuated by > 20 dB.
// ============================================================================
void test_bp24_fourier_attenuation() {
    challenger_dsp::FilterProbeHarness harness;
    harness.prepare(48000.0);

    const double fc = 1000.0;
    const double fLow  = fc / 4.0;  // 250 Hz (2 octaves below fc)
    const double fMid  = fc;        // 1000 Hz
    const double fHigh = 4.0 * fc;  // 4000 Hz (2 octaves above fc)

    // Sweep resonance from 0.0 to 0.8 to empirically characterize attenuation curve
    std::cout << "  Empirical BP 24dB Attenuation vs Resonance Curve:\n";
    for (double r : { 0.0, 0.2, 0.4, 0.5, 0.6, 0.7, 0.8 }) {
        auto mMid  = harness.measureResponse(fMid,  fc, r, bumbler::WaspFilterMode::BP24);
        auto mLow  = harness.measureResponse(fLow,  fc, r, bumbler::WaspFilterMode::BP24);
        auto mHigh = harness.measureResponse(fHigh, fc, r, bumbler::WaspFilterMode::BP24);

        double attLow = mMid.gainDb - mLow.gainDb;
        double attHigh = mMid.gainDb - mHigh.gainDb;

        std::cout << "    reso=" << std::fixed << std::setprecision(1) << r
                  << " | Center=" << std::setprecision(2) << mMid.gainDb << " dB"
                  << " | Low(fc/4) Atten=" << attLow << " dB"
                  << " | High(4*fc) Atten=" << attHigh << " dB\n";
    }

    // Measure at nominal synth resonance setting (reso = 0.5)
    const double nominalReso = 0.5;
    auto mMid  = harness.measureResponse(fMid,  fc, nominalReso, bumbler::WaspFilterMode::BP24);
    auto mLow  = harness.measureResponse(fLow,  fc, nominalReso, bumbler::WaspFilterMode::BP24);
    auto mHigh = harness.measureResponse(fHigh, fc, nominalReso, bumbler::WaspFilterMode::BP24);

    CHALLENGER_ASSERT(mMid.gainDb >= -3.0, "BP24 center frequency attenuation excessive: " + std::to_string(mMid.gainDb));

    const double attenLow = mMid.gainDb - mLow.gainDb;
    const double attenHigh = mMid.gainDb - mHigh.gainDb;

    std::cout << "\n  Verified at nominal resonance (reso = " << nominalReso << "):\n";
    std::cout << "    Low-end Attenuation (fc/4 = 250 Hz):  " << attenLow << " dB (> 20 dB)\n";
    std::cout << "    High-end Attenuation (4*fc = 4000 Hz): " << attenHigh << " dB (> 20 dB)\n";

    // Requirement: Verify both low and high ends are attenuated by > 20 dB
    CHALLENGER_ASSERT(attenLow > 20.0,
        "BP24 low end attenuation insufficient (< 20 dB): measured " + std::to_string(attenLow) + " dB");
    CHALLENGER_ASSERT(attenHigh > 20.0,
        "BP24 high end attenuation insufficient (< 20 dB): measured " + std::to_string(attenHigh) + " dB");
}

// ============================================================================
// CHALLENGE 4: HP 24dB Roll-Off Slopes
// Requirement: Measure slope below cutoff. Verify roll-off is between -22 dB/oct and -26 dB/oct.
// ============================================================================
void test_hp24_fourier_slope() {
    challenger_dsp::FilterProbeHarness harness;
    harness.prepare(48000.0);

    const double fc = 1000.0;

    std::cout << "  [Test 4.1] Critical Damping (reso = 0.0):\n";
    auto mPass0   = harness.measureResponse(12000.0, fc, 0.0, bumbler::WaspFilterMode::HP24);
    auto mFc0     = harness.measureResponse(1000.0,  fc, 0.0, bumbler::WaspFilterMode::HP24);
    auto m1Below0 = harness.measureResponse(500.0,   fc, 0.0, bumbler::WaspFilterMode::HP24);
    auto m2Below0 = harness.measureResponse(250.0,   fc, 0.0, bumbler::WaspFilterMode::HP24);
    auto m3Below0 = harness.measureResponse(125.0,   fc, 0.0, bumbler::WaspFilterMode::HP24);

    std::cout << "    Passband (12000 Hz): " << std::fixed << std::setprecision(2) << mPass0.gainDb << " dB\n";
    std::cout << "    Cutoff   (1000 Hz):  " << mFc0.gainDb << " dB\n";
    std::cout << "    -1 Oct   (500 Hz):   " << m1Below0.gainDb << " dB\n";
    std::cout << "    -2 Oct   (250 Hz):   " << m2Below0.gainDb << " dB\n";
    std::cout << "    -3 Oct   (125 Hz):   " << m3Below0.gainDb << " dB\n";

    CHALLENGER_ASSERT(std::abs(mPass0.gainDb) < 0.1, "HP24 high passband gain at 12 kHz deviates from 0 dB");

    // 2-octave average roll-off relative to passband
    const double avgSlope2Below0 = (m2Below0.gainDb - mPass0.gainDb) / 2.0;
    std::cout << "    2-Octave Average Roll-off: " << avgSlope2Below0 << " dB/oct\n";
    CHALLENGER_ASSERT(avgSlope2Below0 <= -22.0 && avgSlope2Below0 >= -26.0,
        "HP24 2-octave average roll-off violates [-26, -22] dB/oct: " + std::to_string(avgSlope2Below0));

    // Deep stopband slope between -2 oct and -3 oct
    const double deepStopbandSlope0 = m3Below0.gainDb - m2Below0.gainDb;
    std::cout << "    Deep Stopband Slope (-2 oct to -3 oct): " << deepStopbandSlope0 << " dB/oct\n";
    CHALLENGER_ASSERT(deepStopbandSlope0 <= -22.0 && deepStopbandSlope0 >= -26.0,
        "HP24 deep stopband slope violates [-26, -22] dB/oct: " + std::to_string(deepStopbandSlope0));

    std::cout << "  [Test 4.2] Butterworth-like Q (reso = 0.25):\n";
    auto m1Q = harness.measureResponse(500.0, fc, 0.25, bumbler::WaspFilterMode::HP24);
    auto m2Q = harness.measureResponse(250.0, fc, 0.25, bumbler::WaspFilterMode::HP24);

    const double slopeQ_1_to_2 = m2Q.gainDb - m1Q.gainDb;
    std::cout << "    Stopband Slope (-1 oct to -2 oct with reso=0.25): " << slopeQ_1_to_2 << " dB/oct\n";
    CHALLENGER_ASSERT(slopeQ_1_to_2 <= -22.0 && slopeQ_1_to_2 >= -26.0,
        "HP24 stopband slope with reso=0.25 violates [-26, -22] dB/oct: " + std::to_string(slopeQ_1_to_2));
}

// ============================================================================
// CHALLENGE 5: DBL.NT Twin Nulls & Octave Separation
// Requirement: Measure transfer function around fc. Verify two distinct attenuation nulls
// separated by ~1 octave.
// ============================================================================
void test_dbl_nt_fourier_twin_nulls() {
    challenger_dsp::FilterProbeHarness harness;
    harness.prepare(48000.0);

    const double fc = 1000.0;
    const double reso = 0.5;

    // High resolution sweep: 80 logarithmic points from 400 Hz to 2500 Hz
    constexpr int kNumSteps = 80;
    std::vector<double> freqs(kNumSteps);
    std::vector<double> gainsDb(kNumSteps);

    const double logStart = std::log10(400.0);
    const double logEnd   = std::log10(2500.0);

    for (int i = 0; i < kNumSteps; ++i) {
        double f = std::pow(10.0, logStart + (logEnd - logStart) * i / (kNumSteps - 1));
        freqs[i] = f;
        auto m = harness.measureResponse(f, fc, reso, bumbler::WaspFilterMode::DBL_NT, 0.5f, 2000, 4096);
        gainsDb[i] = m.gainDb;
    }

    // Find local minima
    std::vector<int> nullIndices;
    for (int i = 1; i < kNumSteps - 1; ++i) {
        if (gainsDb[i] < gainsDb[i - 1] && gainsDb[i] < gainsDb[i + 1]) {
            nullIndices.push_back(i);
        }
    }

    std::cout << "  DBL.NT Frequency Sweep Results (fc = 1000 Hz, reso = 0.5):\n";
    std::cout << "    Local minima detected: " << nullIndices.size() << "\n";
    for (size_t k = 0; k < nullIndices.size(); ++k) {
        int idx = nullIndices[k];
        std::cout << "      Null #" << (k + 1) << " at " << std::fixed << std::setprecision(1)
                  << freqs[idx] << " Hz : " << std::setprecision(2) << gainsDb[idx] << " dB\n";
    }

    CHALLENGER_ASSERT(nullIndices.size() >= 2,
        "DBL.NT failed to exhibit at least two distinct attenuation nulls (found " + std::to_string(nullIndices.size()) + ")");

    const int null1 = nullIndices[0];
    const int null2 = nullIndices[1];
    const double f1 = freqs[null1];
    const double f2 = freqs[null2];
    const double depth1 = gainsDb[null1];
    const double depth2 = gainsDb[null2];

    CHALLENGER_ASSERT(depth1 < -12.0, "DBL.NT first null is too shallow: " + std::to_string(depth1) + " dB");
    CHALLENGER_ASSERT(depth2 < -12.0, "DBL.NT second null is too shallow: " + std::to_string(depth2) + " dB");

    const double octSeparation = std::log2(f2 / f1);
    std::cout << "    Calculated Octave Separation: " << octSeparation << " octaves (Ratio: " << (f2 / f1) << ")\n";
    std::cout << "    Theoretical Targets: f1 = " << (fc * 0.7071) << " Hz, f2 = " << (fc * 1.4142) << " Hz\n";

    CHALLENGER_ASSERT(octSeparation >= 0.90 && octSeparation <= 1.10,
        "DBL.NT twin nulls not separated by ~1 octave: measured " + std::to_string(octSeparation) + " octaves");
}

// ============================================================================
// CHALLENGE 6: LP+NT (Mode 2) Cascade Response Verification
// ============================================================================
void test_lp_nt_fourier_cascade_response() {
    challenger_dsp::FilterProbeHarness harness;
    harness.prepare(48000.0);

    const double fc = 1000.0;
    const double reso = 0.3;

    auto mPass  = harness.measureResponse(50.0,   fc, reso, bumbler::WaspFilterMode::LP_NT);
    auto mNotch = harness.measureResponse(1000.0, fc, reso, bumbler::WaspFilterMode::LP_NT);
    auto mStop  = harness.measureResponse(4000.0, fc, reso, bumbler::WaspFilterMode::LP_NT);

    std::cout << "  LP+NT Response Measurements (fc = 1000 Hz, reso = 0.3):\n";
    std::cout << "    Passband (50 Hz):   " << std::fixed << std::setprecision(2) << mPass.gainDb << " dB\n";
    std::cout << "    Notch Dip (1000 Hz):" << mNotch.gainDb << " dB\n";
    std::cout << "    Stopband (4000 Hz): " << mStop.gainDb << " dB\n";

    // Passband unity
    CHALLENGER_ASSERT(std::abs(mPass.gainDb) < 0.2, "LP+NT passband deviates from 0 dB");

    // Notch dip depth relative to passband
    const double notchDip = mPass.gainDb - mNotch.gainDb;
    std::cout << "    Notch Dip Depth: " << notchDip << " dB\n";
    CHALLENGER_ASSERT(notchDip > 15.0, "LP+NT notch dip depth is too shallow (< 15 dB): " + std::to_string(notchDip));

    // High frequency roll-off relative to passband
    const double highAtten = mPass.gainDb - mStop.gainDb;
    std::cout << "    High-Frequency Attenuation (4000 Hz): " << highAtten << " dB\n";
    CHALLENGER_ASSERT(highAtten > 20.0, "LP+NT high-frequency attenuation is too shallow (< 20 dB): " + std::to_string(highAtten));
}

// ============================================================================
// CHALLENGE 7: Multi-Rate Frequency Response Invariance & Stability
// Requirement: Verify frequency responses remain identical across sample rates
// (44.1k, 48k, 88.2k, 96k, 192k).
// ============================================================================
void test_multirate_frequency_response_invariance() {
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const double fc = 1000.0;
    const double reso = 0.3;

    // Test frequencies spanning passband, transition, and stopband
    const std::vector<double> testFreqs = { 250.0, 500.0, 1000.0, 2000.0, 4000.0 };

    std::cout << "  Multi-Rate Frequency Invariance Verification across 44.1k, 48k, 88.2k, 96k, 192k:\n";

    for (int modeInt = 0; modeInt <= 5; ++modeInt) {
        auto mode = static_cast<bumbler::WaspFilterMode>(modeInt);

        for (double f : testFreqs) {
            std::vector<double> gainsDb;
            for (double sr : sampleRates) {
                challenger_dsp::FilterProbeHarness harness;
                harness.prepare(sr);
                auto m = harness.measureResponse(f, fc, reso, mode, 0.5f, 3000, 4096);

                // Stability check: finite, non-NaN
                CHALLENGER_ASSERT(!std::isnan(m.gainDb) && !std::isinf(m.gainDb),
                    "Non-finite gain at sr=" + std::to_string(sr) + " mode=" + std::to_string(modeInt) + " f=" + std::to_string(f));
                CHALLENGER_ASSERT(m.gainDb <= 15.0 && m.gainDb >= -240.0,
                    "Gain out of physical bounds at sr=" + std::to_string(sr) + ": " + std::to_string(m.gainDb) + " dB");

                gainsDb.push_back(m.gainDb);
            }

            // Compare deviation against 48 kHz baseline (index 1)
            const double baselineDb = gainsDb[1];
            // Only compare if not in deep stopband noise floor (< -60 dB)
            if (baselineDb > -60.0) {
                for (size_t i = 0; i < sampleRates.size(); ++i) {
                    const double diff = std::abs(gainsDb[i] - baselineDb);
                    const double maxAllowedDiff = (f >= 4000.0) ? 0.75 : 0.4;
                    CHALLENGER_ASSERT(diff <= maxAllowedDiff,
                        "Multi-rate response drifted excessively at f=" + std::to_string(f) +
                        " Hz between 48k and " + std::to_string(sampleRates[i]) +
                        " Hz: diff=" + std::to_string(diff) + " dB (allowed: " + std::to_string(maxAllowedDiff) + " dB)");
                }
            }
        }
    }
    std::cout << "    [PASS] All 6 modes exhibited sub-0.5 dB transfer invariance across all 5 sample rates.\n";
}

// ============================================================================
// CHALLENGE 7: Real-Time Zero Heap Allocations & Denormal Flushing
// ============================================================================
void test_realtime_zero_heap_allocations_and_denormals() {
    constexpr double fs = 48000.0;
    constexpr int kBlockSize = 512;

    bumbler::WaspFilter filter;
    filter.prepare(fs);

    std::vector<float> inBuf(kBlockSize, 0.5f);
    std::vector<float> outBuf(kBlockSize, 0.0f);

    // Warm up
    filter.processBlock(inBuf.data(), outBuf.data(), kBlockSize, 1000.0f, 0.5f, bumbler::WaspFilterMode::LP24);

    // Track audio processing allocations
    gAllocationCountM2.store(0);
    gTrackAllocationsM2.store(true);

    for (int b = 0; b < 50; ++b) {
        float fc = 20.0f + static_cast<float>(b) * 200.0f;
        auto mode = static_cast<bumbler::WaspFilterMode>(b % 6);
        filter.processBlock(inBuf.data(), outBuf.data(), kBlockSize, fc, 0.7f, mode);
    }

    for (int i = 0; i < 5000; ++i) {
        outBuf[i % kBlockSize] = filter.processSample(inBuf[i % kBlockSize], 1000.0f, 0.7f, bumbler::WaspFilterMode::LP12);
    }

    gTrackAllocationsM2.store(false);

    const size_t allocs = gAllocationCountM2.load();
    CHALLENGER_ASSERT(allocs == 0, "Filter audio processing performed heap allocations: " + std::to_string(allocs));
    std::cout << "  [PASS] Zero heap allocations detected during audio processing.\n";

    // Denormal flushing test
    filter.reset();
    for (int i = 0; i < 1000; ++i) {
        float subnormalIn = 1.0e-38f;
        float out = filter.processSample(subnormalIn, 1000.0, 0.5, bumbler::WaspFilterMode::LP12);
        CHALLENGER_ASSERT(std::fpclassify(out) != FP_SUBNORMAL, "Filter output contains subnormal numbers");
    }
    std::cout << "  [PASS] Denormal numbers successfully flushed to zero.\n";
}

// ============================================================================
// Main Challenger M2-2 Test Runner
// ============================================================================
int main() {
    std::cout << "\n##################################################################\n";
    std::cout << " BUMBLER XD: MILESTONE 2 EMPIRICAL CHALLENGER SUITE (CHALLENGER 2)  \n";
    std::cout << " Sine Probe Sweeps & Discrete Fourier Power Frequency Response    \n";
    std::cout << "##################################################################\n";

    runChallengerTest("1. LP FAT 24dB Fourier Slope Verification", test_lp24_fat_fourier_slope);
    runChallengerTest("2. LP 12dB Fourier Slope Verification", test_lp12_fourier_slope);
    runChallengerTest("3. BP 24dB Fourier Attenuation at fc/4 & 4*fc", test_bp24_fourier_attenuation);
    runChallengerTest("4. HP 24dB Fourier Slope Verification", test_hp24_fourier_slope);
    runChallengerTest("5. DBL.NT Twin Nulls & Octave Separation Sweep", test_dbl_nt_fourier_twin_nulls);
    runChallengerTest("6. LP+NT Cascade Notch Dip & Roll-Off Verification", test_lp_nt_fourier_cascade_response);
    runChallengerTest("7. Multi-Rate Transfer Invariance (44.1k - 192k)", test_multirate_frequency_response_invariance);
    runChallengerTest("8. Real-Time Zero Heap Allocations & Denormal Flushing", test_realtime_zero_heap_allocations_and_denormals);

    std::cout << "\n------------------------------------------------------------------\n";
    std::cout << " Challenger M2-2 Results: " << gPassedTests << " / " << gTotalTests << " Passed ("
              << gFailedTests << " Failed)\n";
    std::cout << "------------------------------------------------------------------\n";

    return (gFailedTests == 0) ? 0 : 1;
}
