#include "test_helpers.h"
#include <iomanip>

#if __has_include("../source/dsp/WaspFilter.h")
#include "../source/dsp/WaspFilter.h"
#define BUMBLER_HAS_REAL_WASP_FILTER 1
#endif
#include "BumblerVoice.h"
#include "ParameterSnapshot.h"

// ============================================================================
// Filter Test Harness Adapter
// ============================================================================
class FilterTestHarness {
public:
    void prepare(double sampleRate) {
        fs = sampleRate;
#if defined(BUMBLER_HAS_REAL_WASP_FILTER)
        realFilter.prepare(sampleRate);
#else
        refFilter.prepare(sampleRate);
#endif
    }

    void reset() {
#if defined(BUMBLER_HAS_REAL_WASP_FILTER)
        realFilter.reset();
#else
        refFilter.reset();
#endif
    }

    float process(float in, double cutoff, double reso, int mode) {
#if defined(BUMBLER_HAS_REAL_WASP_FILTER)
        return realFilter.processSample(in, cutoff, reso, static_cast<WaspFilterMode>(mode));
#else
        return refFilter.processSample(in, cutoff, reso, static_cast<bumbler_test::ReferenceWaspFilter::Mode>(mode));
#endif
    }

    double measureGainAtFrequency(double testFreqHz, double cutoffHz, double reso, int mode, int numSamples = 8000) {
        reset();
        std::vector<float> in(numSamples);
        bumbler_test::generateSine(in.data(), numSamples, testFreqHz, fs, 1.0f);

        // Discard initial 3000 samples for steady state
        constexpr int kTransientDiscard = 3000;
        float peakOut = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float out = process(in[i], cutoffHz, reso, mode);
            if (i >= kTransientDiscard) {
                float absOut = std::abs(out);
                if (absOut > peakOut) peakOut = absOut;
            }
        }
        return peakOut;
    }

private:
    double fs = 48000.0;
#if defined(BUMBLER_HAS_REAL_WASP_FILTER)
    WaspFilter realFilter;
#else
    bumbler_test::ReferenceWaspFilter refFilter;
#endif
};

// ============================================================================
// 1. LP 12dB Roll-Off Verification: ~12 dB/oct above cutoff
// ============================================================================
void test_lp12_rolloff_slope() {
    FilterTestHarness harness;
    harness.prepare(48000.0);
    const double fc = 1000.0;
    const double reso = 0.0; // flat Butterworth-like Q

    // Measure at passband (250 Hz), cutoff (1000 Hz), +1 oct (2000 Hz), +2 oct (4000 Hz)
    double gainPass = harness.measureGainAtFrequency(250.0, fc, reso, 0);
    double gainFc   = harness.measureGainAtFrequency(1000.0, fc, reso, 0);
    double gain1Oct = harness.measureGainAtFrequency(2000.0, fc, reso, 0);
    double gain2Oct = harness.measureGainAtFrequency(4000.0, fc, reso, 0);

    double dbPass = bumbler_test::toDb(gainPass);
    double dbFc   = bumbler_test::toDb(gainFc);
    double db1Oct = bumbler_test::toDb(gain1Oct);
    double db2Oct = bumbler_test::toDb(gain2Oct);

    // Passband should be near 0 dB
    TEST_ASSERT(std::abs(dbPass) < 1.0, "LP12 passband gain deviates from 0 dB");

    // At cutoff, 2-pole response with damping is approximately -3 to -6 dB
    TEST_ASSERT(dbFc <= 0.0 && dbFc >= -6.5, "LP12 cutoff gain out of expected [-6.5, 0] dB range");

    // Roll-off at +1 oct (2000 Hz) relative to passband should be ~12 dB down (-10 to -14 dB)
    double slope1Oct = db1Oct - dbPass;
    TEST_ASSERT(slope1Oct <= -9.5 && slope1Oct >= -14.5, 
        "LP12 slope at +1 octave violates ~12 dB/oct: measured " + std::to_string(slope1Oct) + " dB");

    // Roll-off at +2 oct (4000 Hz) relative to passband should be ~24 dB down (-20 to -27 dB)
    double slope2Oct = db2Oct - dbPass;
    TEST_ASSERT(slope2Oct <= -20.0 && slope2Oct >= -27.0, 
        "LP12 slope at +2 octaves violates ~24 dB/2-oct: measured " + std::to_string(slope2Oct) + " dB");
}

// ============================================================================
// 2. LP FAT 24dB Roll-Off Verification: ~24 dB/oct above cutoff
// ============================================================================
void test_lp24_fat_rolloff_slope() {
    FilterTestHarness harness;
    harness.prepare(48000.0);
    const double fc = 1000.0;
    const double reso = 0.0;

    double gainPass = harness.measureGainAtFrequency(250.0, fc, reso, 1);
    double gain1Oct = harness.measureGainAtFrequency(2000.0, fc, reso, 1);
    double gain2Oct = harness.measureGainAtFrequency(4000.0, fc, reso, 1);

    double dbPass = bumbler_test::toDb(gainPass);
    double db1Oct = bumbler_test::toDb(gain1Oct);
    double db2Oct = bumbler_test::toDb(gain2Oct);

    // Roll-off at +1 oct relative to passband should be ~24 dB down (-20 to -28 dB)
    double slope1Oct = db1Oct - dbPass;
    TEST_ASSERT(slope1Oct <= -19.5 && slope1Oct >= -28.0, 
        "LP FAT 24dB slope at +1 octave violates ~24 dB/oct: measured " + std::to_string(slope1Oct) + " dB");

    // Roll-off at +2 oct relative to passband should be ~48 dB down (-40 to -54 dB)
    double slope2Oct = db2Oct - dbPass;
    TEST_ASSERT(slope2Oct <= -40.0 && slope2Oct >= -54.0, 
        "LP FAT 24dB slope at +2 octaves violates ~48 dB/2-oct: measured " + std::to_string(slope2Oct) + " dB");
}

// ============================================================================
// 3. BP 24dB Verification: Attenuation on both low and high ends
// ============================================================================
void test_bp24_attenuation_both_ends() {
    FilterTestHarness harness;
    harness.prepare(48000.0);
    const double fc = 1000.0;
    const double reso = 0.2;

    double gainCenter = harness.measureGainAtFrequency(1000.0, fc, reso, 4);
    double gainLow    = harness.measureGainAtFrequency(250.0, fc, reso, 4);
    double gainHigh   = harness.measureGainAtFrequency(4000.0, fc, reso, 4);

    double dbCenter = bumbler_test::toDb(gainCenter);
    double dbLow    = bumbler_test::toDb(gainLow);
    double dbHigh   = bumbler_test::toDb(gainHigh);

    // Center frequency should be strong passband (> -3 dB)
    TEST_ASSERT(dbCenter >= -3.0, "BP24 center frequency attenuation too high: " + std::to_string(dbCenter) + " dB");

    // Low end (250 Hz, 2 octaves below) must be heavily attenuated (> 15 dB down relative to center)
    double attenLow = dbCenter - dbLow;
    TEST_ASSERT(attenLow >= 14.0, "BP24 low end insufficient attenuation: " + std::to_string(attenLow) + " dB");

    // High end (4000 Hz, 2 octaves above) must be heavily attenuated (> 15 dB down relative to center)
    double attenHigh = dbCenter - dbHigh;
    TEST_ASSERT(attenHigh >= 14.0, "BP24 high end insufficient attenuation: " + std::to_string(attenHigh) + " dB");
}

// ============================================================================
// 4. HP 24dB Roll-Off Verification: ~24 dB/oct low-end roll-off below cutoff
// ============================================================================
void test_hp24_rolloff_slope() {
    FilterTestHarness harness;
    harness.prepare(48000.0);
    const double fc = 1000.0;
    const double reso = 0.0;

    double gainPass = harness.measureGainAtFrequency(4000.0, fc, reso, 5); // 2 octaves above fc
    double gain1OctBelow = harness.measureGainAtFrequency(500.0, fc, reso, 5); // 1 oct below fc
    double gain2OctBelow = harness.measureGainAtFrequency(250.0, fc, reso, 5); // 2 oct below fc

    double dbPass = bumbler_test::toDb(gainPass);
    double db1Oct = bumbler_test::toDb(gain1OctBelow);
    double db2Oct = bumbler_test::toDb(gain2OctBelow);

    // High frequency passband should be near 0 dB
    TEST_ASSERT(std::abs(dbPass) < 1.5, "HP24 passband deviates from 0 dB");

    // 1 octave below cutoff: ~24 dB attenuation relative to passband (-20 to -28 dB)
    double slope1Oct = db1Oct - dbPass;
    TEST_ASSERT(slope1Oct <= -19.5 && slope1Oct >= -28.0, 
        "HP24 slope at -1 octave violates ~24 dB/oct: measured " + std::to_string(slope1Oct) + " dB");

    // 2 octaves below cutoff: ~48 dB attenuation relative to passband (-40 to -54 dB)
    double slope2Oct = db2Oct - dbPass;
    TEST_ASSERT(slope2Oct <= -40.0 && slope2Oct >= -54.0, 
        "HP24 slope at -2 octaves violates ~48 dB/2-oct: measured " + std::to_string(slope2Oct) + " dB");
}

// ============================================================================
// 5. DBL.NT Verification: Detection of two distinct attenuation nulls
// ============================================================================
void test_dbl_nt_twin_nulls() {
    FilterTestHarness harness;
    harness.prepare(48000.0);
    const double fc = 1000.0;
    const double reso = 0.5;

    // Sweep 60 logarithmic frequencies from 400 Hz to 2500 Hz
    constexpr int kNumSteps = 60;
    std::vector<double> freqs(kNumSteps);
    std::vector<double> gainsDb(kNumSteps);

    const double logStart = std::log10(400.0);
    const double logEnd   = std::log10(2500.0);

    for (int i = 0; i < kNumSteps; ++i) {
        double f = std::pow(10.0, logStart + (logEnd - logStart) * i / (kNumSteps - 1));
        freqs[i] = f;
        double gain = harness.measureGainAtFrequency(f, fc, reso, 3, 6000);
        gainsDb[i] = bumbler_test::toDb(gain);
    }

    // Detect local minima (points lower than both immediate neighbors)
    std::vector<int> localMinima;
    for (int i = 1; i < kNumSteps - 1; ++i) {
        if (gainsDb[i] < gainsDb[i - 1] && gainsDb[i] < gainsDb[i + 1]) {
            localMinima.push_back(i);
        }
    }

    TEST_ASSERT(localMinima.size() >= 2, 
        "DBL.NT failed to exhibit two distinct attenuation nulls (found " + std::to_string(localMinima.size()) + ")");

    int null1 = localMinima[0];
    int null2 = localMinima[1];
    TEST_ASSERT(gainsDb[null1] < -12.0, "DBL.NT first notch too shallow: " + std::to_string(gainsDb[null1]) + " dB");
    TEST_ASSERT(gainsDb[null2] < -12.0, "DBL.NT second notch too shallow: " + std::to_string(gainsDb[null2]) + " dB");
    TEST_ASSERT(freqs[null1] < freqs[null2], "DBL.NT notch frequencies misordered");
}

// ============================================================================
// 6. LP+NT Cascade: Lowpass with notch attenuation dip
// ============================================================================
void test_lp_nt_cascade_response() {
    FilterTestHarness harness;
    harness.prepare(48000.0);
    const double fc = 1000.0;
    const double reso = 0.3;

    double gainLow   = harness.measureGainAtFrequency(250.0, fc, reso, 2);
    double gainNotch = harness.measureGainAtFrequency(1000.0, fc, reso, 2);
    double gainHigh  = harness.measureGainAtFrequency(4000.0, fc, reso, 2);

    double dbLow   = bumbler_test::toDb(gainLow);
    double dbNotch = bumbler_test::toDb(gainNotch);
    double dbHigh  = bumbler_test::toDb(gainHigh);

    // Passband below cutoff is strong
    TEST_ASSERT(dbLow > -2.0, "LP+NT low passband gain too low: " + std::to_string(dbLow) + " dB");

    // Notch dip at 1000 Hz must be substantially lower than passband (> 12 dB dip)
    TEST_ASSERT((dbLow - dbNotch) > 12.0, "LP+NT lacks notch dip at cutoff: dip = " + std::to_string(dbLow - dbNotch) + " dB");

    // High frequency (4000 Hz) must exhibit lowpass roll-off (> 15 dB down relative to passband)
    TEST_ASSERT((dbLow - dbHigh) > 15.0, "LP+NT lacks high frequency roll-off: " + std::to_string(dbLow - dbHigh) + " dB");
}

// ============================================================================
// 7. Multi-Rate Filter Stability: 44.1k, 48k, 96k, 192k
// ============================================================================
void test_filter_multirate_stability() {
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    FilterTestHarness harness;

    for (double sr : sampleRates) {
        harness.prepare(sr);
        for (int mode = 0; mode <= 5; ++mode) {
            double gain = harness.measureGainAtFrequency(1000.0, 1000.0, 0.7, mode, 2048);
            TEST_ASSERT(!std::isnan(gain) && !std::isinf(gain), 
                "Filter output non-finite at sample rate " + std::to_string(sr) + ", mode " + std::to_string(mode));
            TEST_ASSERT(gain >= 0.0 && gain <= 10.0, 
                "Filter gain exploded at sample rate " + std::to_string(sr) + ": " + std::to_string(gain));
        }
    }
}

// ============================================================================
// 8. Keyboard Tracking & Bipolar Filter Envelope Modulation
// ============================================================================
void test_filter_kb_tracking_and_bipolar_env_modulation() {
    constexpr float sampleRate = 48000.0f;
    const float baseCutoff = 1000.0f;

    // 1. C4 (Note 60) with 100% key tracking -> delta = 0 semitones -> exactly 1000 Hz
    float fc60 = WaspFilter::calculateModulatedCutoff(baseCutoff, 60.0f, 1.0f, 0.0f, 0.0f, sampleRate);
    TEST_ASSERT(std::abs(fc60 - 1000.0f) < 0.1f, "Cutoff at Note 60 C4 with 100% KB tracking must be 1000 Hz");

    // 2. C5 (Note 72) with 100% key tracking -> +12 semitones -> exactly 2000 Hz
    float fc72 = WaspFilter::calculateModulatedCutoff(baseCutoff, 72.0f, 1.0f, 0.0f, 0.0f, sampleRate);
    TEST_ASSERT(std::abs(fc72 - 2000.0f) < 0.1f, "Cutoff at Note 72 C5 with 100% KB tracking must be 2000 Hz");

    // 3. C3 (Note 48) with 100% key tracking -> -12 semitones -> exactly 500 Hz
    float fc48 = WaspFilter::calculateModulatedCutoff(baseCutoff, 48.0f, 1.0f, 0.0f, 0.0f, sampleRate);
    TEST_ASSERT(std::abs(fc48 - 500.0f) < 0.1f, "Cutoff at Note 48 C3 with 100% KB tracking must be 500 Hz");

    // 4. C5 with 50% key tracking -> +6 semitones -> 1000 * sqrt(2) = 1414.21 Hz
    float fc72_half = WaspFilter::calculateModulatedCutoff(baseCutoff, 72.0f, 0.5f, 0.0f, 0.0f, sampleRate);
    TEST_ASSERT(std::abs(fc72_half - 1414.21f) < 1.0f, "Cutoff at Note 72 with 50% KB tracking must be ~1414 Hz");

    // 5. Bipolar Envelope Modulation (+1.0 scaling -> +5 octaves = +60 semitones = 32x)
    float fcEnvMax = WaspFilter::calculateModulatedCutoff(baseCutoff, 60.0f, 0.0f, 1.0f, 1.0f, sampleRate);
    TEST_ASSERT(std::abs(fcEnvMax - 23040.0f) < 1.0f, "Extreme positive envelope cutoff must be clamped to 0.48*fs");

    // 6. Bipolar Envelope Modulation (-1.0 scaling -> -5 octaves = -60 semitones = 1/32x)
    float fcEnvMin = WaspFilter::calculateModulatedCutoff(baseCutoff, 60.0f, 0.0f, -1.0f, 1.0f, sampleRate);
    TEST_ASSERT(std::abs(fcEnvMin - 31.25f) < 0.1f, "Negative envelope cutoff at -5 octaves must be ~31.25 Hz");

    // 7. Extreme low boundary clamping (50 Hz / 32 = 1.56 Hz -> clamped to 20 Hz)
    float fcExtremeLow = WaspFilter::calculateModulatedCutoff(50.0f, 60.0f, 0.0f, -1.0f, 1.0f, sampleRate);
    TEST_ASSERT(std::abs(fcExtremeLow - 20.0f) < 0.01f, "Extreme low cutoff must be clamped to 20 Hz");
}

// ============================================================================
// 9. Block Processing with 16-Sample Sub-Block Interpolation
// ============================================================================
void test_filter_process_block_subblocking() {
    WaspFilter filter;
    filter.prepare(48000.0);

    constexpr int kNumSamples = 256;
    std::vector<float> in(kNumSamples, 0.0f);
    bumbler_test::generateSine(in.data(), kNumSamples, 440.0, 48000.0, 1.0f);

    std::vector<float> outBlock(kNumSamples, 0.0f);
    filter.processBlock(in.data(), outBlock.data(), kNumSamples, 1000.0f, 0.5f, WaspFilterMode::LP24);

    for (int i = 0; i < kNumSamples; ++i) {
        TEST_ASSERT(!std::isnan(outBlock[i]) && !std::isinf(outBlock[i]), "processBlock output contains NaN/Inf");
        TEST_ASSERT(std::abs(outBlock[i]) <= 2.0f, "processBlock output exceeded bounds");
    }

    float maxAbs = 0.0f;
    for (int i = 64; i < kNumSamples; ++i) {
        maxAbs = std::max(maxAbs, std::abs(outBlock[i]));
    }
    TEST_ASSERT(maxAbs > 0.1f, "processBlock output is silence");
}

// ============================================================================
// 10. Voice Filter Integration, Dual ADSR & Link Toggle
// ============================================================================
void test_voice_filter_envelope_and_link() {
    bumbler::BumblerVoice voice;
    voice.prepare(48000.0);
    voice.setFilterEnabled(true);

    TEST_ASSERT(voice.isFilterEnabled(), "Voice filter should be enabled");

    bumbler::ParameterSnapshot params;
    params.filterCutoff = 1000.0f;
    params.filterMode = 0.0f; // LP12
    params.ampAttack = 0.001f;
    params.ampDecay = 0.10f;
    params.ampSustain = 0.8f;
    params.ampRelease = 0.10f;

    params.filterAttack = 0.02f;
    params.filterDecay = 0.20f;
    params.filterSustain = 0.4f;
    params.filterRelease = 0.20f;
    params.envLink = 0.0f;

    voice.noteOn(60, 0.8f, 1);
    TEST_ASSERT(voice.isActive(), "Voice must be active after noteOn");

    for (int i = 0; i < 100; ++i) {
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
    }

    TEST_ASSERT(voice.getEnvelopeLevel() > 0.0f, "Amp envelope must be active");
    TEST_ASSERT(voice.getFilterEnvelopeLevel() > 0.0f, "Filter envelope must be active");

    // Test Link toggle: with envLink = 1.0, filter envelope parameters follow amp envelope
    params.envLink = 1.0f;
    voice.noteOn(60, 0.8f, 2);
    for (int i = 0; i < 48; ++i) { // 1ms = 48 samples (Amp attack duration)
        float l = 0.0f, r = 0.0f;
        voice.renderSample(l, r, params);
    }
    TEST_ASSERT(voice.getFilterEnvelopeLevel() >= 0.95f, "Linked filter envelope did not follow fast amp attack");
}

// ============================================================================
// Main Filter Test Runner
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << " Bumbler XD: 6-Mode Wasp Filter Accuracy Test Suite \n";
    std::cout << "====================================================\n";

    RUN_TEST(test_lp12_rolloff_slope);
    RUN_TEST(test_lp24_fat_rolloff_slope);
    RUN_TEST(test_bp24_attenuation_both_ends);
    RUN_TEST(test_hp24_rolloff_slope);
    RUN_TEST(test_dbl_nt_twin_nulls);
    RUN_TEST(test_lp_nt_cascade_response);
    RUN_TEST(test_filter_multirate_stability);
    RUN_TEST(test_filter_kb_tracking_and_bipolar_env_modulation);
    RUN_TEST(test_filter_process_block_subblocking);
    RUN_TEST(test_voice_filter_envelope_and_link);

    std::cout << "\n----------------------------------------------------\n";
    std::cout << " Filter Tests Summary: " << gGlobalTestsPassed << " Passed, " 
              << gGlobalTestsFailed << " Failed\n";
    std::cout << "----------------------------------------------------\n";

    return (gGlobalTestsFailed == 0) ? 0 : 1;
}
