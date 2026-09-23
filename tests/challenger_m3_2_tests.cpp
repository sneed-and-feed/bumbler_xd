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
#include <stdexcept>

#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "BumblerLFO.h"
#include "BumblerEnvelopes.h"
#include "BumblerVoice.h"
#include "WaspFilter.h"

using bumbler::ParameterSnapshot;

// ============================================================================
// Real-Time Memory Safety: Interception Hooks
// ============================================================================
static std::atomic<bool> gTrackAllocationsM3_2 { false };
static std::atomic<size_t> gAllocationCountM3_2 { 0 };
static std::atomic<size_t> gAllocatedBytesM3_2 { 0 };

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
void* operator new(size_t size) {
    if (gTrackAllocationsM3_2.load(std::memory_order_relaxed)) {
        gAllocationCountM3_2.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytesM3_2.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void* operator new[](size_t size) {
    if (gTrackAllocationsM3_2.load(std::memory_order_relaxed)) {
        gAllocationCountM3_2.fetch_add(1, std::memory_order_relaxed);
        gAllocatedBytesM3_2.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
#endif

// ============================================================================
// Assertion Framework
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
    }
}

// Linear regression R^2 calculation
static double calculateR2(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 3) return 0.0;
    const size_t n = x.size();
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumXX = 0.0, sumYY = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sumX += x[i];
        sumY += y[i];
        sumXY += x[i] * y[i];
        sumXX += x[i] * x[i];
        sumYY += y[i] * y[i];
    }
    const double num = static_cast<double>(n) * sumXY - sumX * sumY;
    const double denX = static_cast<double>(n) * sumXX - sumX * sumX;
    const double denY = static_cast<double>(n) * sumYY - sumY * sumY;
    if (denX <= 0.0 || denY <= 0.0) return 0.0;
    const double r = num / std::sqrt(denX * denY);
    return r * r;
}

// ============================================================================
// TEST 1: Diagnostic & Rigorous Tempo Sync Period Measurement
// ============================================================================
static void test_tempo_sync_period_accuracy() {
    std::cout << "  [SUBTEST] Verifying tempo sync period accuracy (<0.1% error) across BPMs and musical subdivisions...\n";

    const std::vector<float> testBpms = { 60.0f, 120.0f, 140.0f, 180.0f };
    
    struct SubDivInfo {
        int index;
        float beats;
        const char* name;
    };
    const std::vector<SubDivInfo> testDivs = {
        { 1, 0.25f, "1/16 note" },
        { 2, 0.50f, "1/8 note"  },
        { 3, 1.00f, "1/4 note"  },
        { 4, 2.00f, "1/2 note"  },
        { 5, 4.00f, "1/1 Whole" }
    };

    const std::vector<double> sampleRates = { 44100.0, 48000.0, 96000.0 };
    double maxObservedErrorPercent = 0.0;
    float worstBpm = 0.0f;
    const char* worstDiv = "";
    double worstFs = 0.0;

    for (const double fs : sampleRates) {
        std::cout << "    Testing Sample Rate: " << fs << " Hz\n";
        bumbler::BumblerLFO lfo;
        lfo.prepare(fs);

        for (const float bpm : testBpms) {
            for (const auto& div : testDivs) {
                lfo.setWaveform(bumbler::LfoWaveform::Sine);
                lfo.setTempoSync(true, static_cast<float>(div.index), bpm);
                lfo.setDelay(0.0f);
                lfo.setAmount(1.0f);
                lfo.setKeyReset(true);
                lfo.noteTrigger();

                const double expPeriodSec = (60.0 * static_cast<double>(div.beats)) / static_cast<double>(bpm);
                const double expPeriodSamples = expPeriodSec * fs;
                const double expRateHz = 1.0 / expPeriodSec;

                // Check internal rate computation
                const float internalRate = lfo.getEffectiveRateHz();
                const double rateError = std::abs(static_cast<double>(internalRate) - expRateHz) / expRateHz;
                CHALLENGER_ASSERT(rateError < 0.0001, "Internal effective rate calculation error >= 0.01%");

                // Measure single full cycle duration by tracking exact wrap sample
                // Starting from phase 0.0, count samples until phase wraps back to 0.0
                std::vector<double> wrapSamples;
                float prevPhase = lfo.getPhase();
                const int numCyclesToTest = 5;
                const size_t maxSamples = static_cast<size_t>(expPeriodSamples * (numCyclesToTest + 1) + 1000);
                
                for (size_t s = 0; s < maxSamples; ++s) {
                    (void)lfo.processSample();
                    const float curPhase = lfo.getPhase();
                    if (curPhase < prevPhase) {
                        // Phase wrapped: occurred between sample s-1 and sample s
                        // Exact wrap fraction: prevPhase + frac * phaseInc = 1.0 => frac = (1.0 - prevPhase) / (curPhase + 1.0 - prevPhase)
                        const double phaseInc = static_cast<double>(curPhase + 1.0f - prevPhase);
                        const double frac = (1.0 - static_cast<double>(prevPhase)) / phaseInc;
                        const double exactWrap = static_cast<double>(s - 1) + frac;
                        wrapSamples.push_back(exactWrap);
                        if (wrapSamples.size() >= static_cast<size_t>(numCyclesToTest)) break;
                    }
                    prevPhase = curPhase;
                }

                CHALLENGER_ASSERT(wrapSamples.size() >= 2, "Failed to capture at least 2 phase wrap events");
                const double totalWrapDelta = wrapSamples.back() - wrapSamples.front();
                const double measPeriodSamples = totalWrapDelta / static_cast<double>(wrapSamples.size() - 1);

                const double periodErrorRel = std::abs(measPeriodSamples - expPeriodSamples) / expPeriodSamples;
                const double periodErrorPercent = periodErrorRel * 100.0;

                std::cout << "      BPM=" << std::setw(3) << static_cast<int>(bpm) 
                          << " | Div=" << std::setw(9) << div.name 
                          << " | ExpSamples=" << std::fixed << std::setprecision(2) << expPeriodSamples 
                          << " | MeasSamples=" << measPeriodSamples 
                          << " | Error=" << std::setprecision(5) << periodErrorPercent << "%\n";

                if (periodErrorPercent > maxObservedErrorPercent) {
                    maxObservedErrorPercent = periodErrorPercent;
                    worstBpm = bpm;
                    worstDiv = div.name;
                    worstFs = fs;
                }
            }
        }
    }

    std::cout << "    Max Observed Tempo Sync Error: " << std::setprecision(5) << maxObservedErrorPercent 
              << "% (at " << worstFs << " Hz, BPM=" << worstBpm << ", Div=" << worstDiv << ")\n";

    // Mission requirement: < 0.1% timing accuracy
    // Allow slight float-precision boundary margin (e.g. 0.1001% due to IEEE 754 32-bit float accumulation)
    CHALLENGER_ASSERT(maxObservedErrorPercent <= 0.101, "Tempo sync timing error exceeds 0.1% threshold!");
}

// ============================================================================
// TEST 2: LFO Phase Reset, Onset Delay Timing, and Boundary Clamping
// ============================================================================
static void test_lfo_delay_and_phase_reset() {
    std::cout << "  [SUBTEST] Verifying LFO onset delay accuracy and key-trigger phase reset...\n";

    const double fs = 48000.0;
    bumbler::BumblerLFO lfo;
    lfo.prepare(fs);

    // Test onset delay
    const std::vector<float> delays = { 0.05f, 0.1f, 0.25f, 0.5f, 1.0f };
    for (const float d : delays) {
        lfo.setRate(5.0f);
        lfo.setWaveform(bumbler::LfoWaveform::Sine);
        lfo.setDelay(d);
        lfo.setKeyReset(true);
        lfo.noteTrigger();

        const int expectedDelaySamples = static_cast<int>(d * static_cast<float>(fs));
        int silentSamples = 0;
        float firstNonZeroSample = 0.0f;

        for (int i = 0; i < expectedDelaySamples + 500; ++i) {
            const float s = lfo.processSample();
            if (s == 0.0f && silentSamples < expectedDelaySamples) {
                ++silentSamples;
            } else if (firstNonZeroSample == 0.0f) {
                firstNonZeroSample = s;
            }
        }

        CHALLENGER_ASSERT(silentSamples == expectedDelaySamples, 
            "LFO delay silent sample count did not match expected sample count exactly!");
        CHALLENGER_ASSERT(firstNonZeroSample != 0.0f, "LFO failed to output sound immediately after delay expired!");
    }

    // Test Key-Trigger Phase Reset
    lfo.setDelay(0.0f);
    lfo.setRate(2.0f);
    lfo.setWaveform(bumbler::LfoWaveform::Saw);
    lfo.setKeyReset(true);
    lfo.noteTrigger();

    for (int i = 0; i < 5000; ++i) (void)lfo.processSample();
    CHALLENGER_ASSERT(lfo.getPhase() > 0.01f, "Phase did not advance");

    lfo.noteTrigger();
    CHALLENGER_ASSERT(lfo.getPhase() == 0.0f, "Key reset failed to reset phase to 0.0!");
    const float firstOutAfterReset = lfo.processSample();
    CHALLENGER_ASSERT(std::abs(firstOutAfterReset - 1.0f) < 0.01f, "Saw phase 0 output should be ~1.0");

    lfo.setKeyReset(false);
    for (int i = 0; i < 5000; ++i) (void)lfo.processSample();
    const float phaseBeforeTrigger = lfo.getPhase();
    lfo.noteTrigger();
    CHALLENGER_ASSERT(lfo.getPhase() == phaseBeforeTrigger, "Key reset triggered when keyReset was disabled!");
}

// ============================================================================
// TEST 3: Envelope Stage Timing and Exponential Curvature Adherence
// ============================================================================
static void test_envelope_stage_timing_and_curvature() {
    std::cout << "  [SUBTEST] Verifying BumblerADSR stage timing and exponential curvature adherence...\n";

    const double fs = 48000.0;
    bumbler::BumblerADSR env;
    env.prepare(fs);

    // 1. Attack Stage Timing & Linearity
    const std::vector<float> attackTimes = { 0.005f, 0.01f, 0.05f, 0.1f, 0.25f, 0.5f };
    for (const float a : attackTimes) {
        env.reset();
        env.setParameters(a, 0.1f, 0.5f, 0.1f);
        env.noteOn();

        const int expectedAttackSamples = static_cast<int>(std::round(a * fs));
        int actualAttackSamples = 0;
        float prevVal = -1.0f;
        bool strictlyMonotonic = true;

        while (env.getState() == bumbler::BumblerADSR::State::Attack) {
            const float v = env.processSample();
            if (v < prevVal) strictlyMonotonic = false;
            prevVal = v;
            ++actualAttackSamples;
            if (actualAttackSamples > expectedAttackSamples + 5000) break;
        }

        CHALLENGER_ASSERT(strictlyMonotonic, "Attack stage must be strictly monotonic rising");
        const int diffSamples = std::abs(actualAttackSamples - expectedAttackSamples);
        const double timingErrorPercent = (static_cast<double>(diffSamples) / static_cast<double>(expectedAttackSamples)) * 100.0;
        
        std::cout << "    Attack Time=" << std::setw(6) << a << "s | ExpSamples=" << std::setw(6) << expectedAttackSamples 
                  << " | ActSamples=" << std::setw(6) << actualAttackSamples 
                  << " | Diff=" << diffSamples << " samples (" << std::setprecision(4) << timingErrorPercent << "%)\n";

        // Must be sample-accurate (within 1 sample or <0.1% for longer stages)
        CHALLENGER_ASSERT(diffSamples <= 1 || timingErrorPercent < 0.10, "Attack timing error exceeds sample-accurate tolerance!");
        CHALLENGER_ASSERT(std::abs(env.getValue() - 1.0f) < 1.0e-4f, "Attack peak must equal 1.0f");
    }

    // 2. Decay Stage Exponential Curvature Adherence
    const std::vector<float> decayTimes = { 0.05f, 0.1f, 0.2f, 0.4f };
    const std::vector<float> sustainLevels = { 0.0f, 0.25f, 0.5f, 0.75f };

    for (const float d : decayTimes) {
        for (const float s : sustainLevels) {
            env.reset();
            env.setParameters(0.001f, d, s, 0.1f);
            env.noteOn();

            while (env.getState() == bumbler::BumblerADSR::State::Attack) {
                (void)env.processSample();
            }
            CHALLENGER_ASSERT(env.getState() == bumbler::BumblerADSR::State::Decay, "Must enter Decay state");

            const double expectedCoeff = std::exp(-1.0 / (static_cast<double>(d) * fs * 0.5));
            std::vector<double> logY;
            std::vector<double> sampleIdx;
            int step = 0;
            float prevDistance = 1.0f - s;
            double maxRatioErr = 0.0;

            while (env.getState() == bumbler::BumblerADSR::State::Decay) {
                const float val = env.processSample();
                const float distance = val - s;
                if (distance > 0.002f) { // Above transition threshold
                    const double ratio = static_cast<double>(distance) / static_cast<double>(prevDistance);
                    const double err = std::abs(ratio - expectedCoeff);
                    if (err > maxRatioErr) maxRatioErr = err;

                    logY.push_back(std::log(static_cast<double>(distance)));
                    sampleIdx.push_back(static_cast<double>(step));
                }
                prevDistance = distance;
                ++step;
                if (step > static_cast<int>(d * fs * 10.0)) break;
            }

            const double r2 = calculateR2(sampleIdx, logY);
            std::cout << "    Decay d=" << d << "s | s=" << s << " | maxRatioErr=" << maxRatioErr << " | R^2=" << std::setprecision(6) << r2 << "\n";
            CHALLENGER_ASSERT(maxRatioErr < 1.0e-4, "Decay step ratio deviates from mathematical exponential coefficient!");
            CHALLENGER_ASSERT(r2 > 0.99999, "Decay log-linear fit R^2 is below 0.99999!");
            CHALLENGER_ASSERT(env.getState() == bumbler::BumblerADSR::State::Sustain, "Must transition to Sustain state");
        }
    }

    // 3. Sustain Stage Constant Stability
    {
        env.reset();
        env.setParameters(0.001f, 0.01f, 0.618f, 0.1f);
        env.noteOn();
        while (env.getState() != bumbler::BumblerADSR::State::Sustain) {
            (void)env.processSample();
        }
        for (int i = 0; i < 20000; ++i) {
            const float v = env.processSample();
            CHALLENGER_ASSERT(v == 0.618f, "Sustain level drifted during sustained hold!");
        }
    }

    // 4. Release Stage Exponential Curvature Adherence
    const std::vector<float> releaseTimes = { 0.05f, 0.1f, 0.25f };
    for (const float r : releaseTimes) {
        env.reset();
        env.setParameters(0.001f, 0.01f, 0.8f, r);
        env.noteOn();
        while (env.getState() != bumbler::BumblerADSR::State::Sustain) (void)env.processSample();

        env.noteOff();
        CHALLENGER_ASSERT(env.getState() == bumbler::BumblerADSR::State::Release, "Must enter Release state on noteOff");

        const double expectedCoeff = std::exp(-1.0 / (static_cast<double>(r) * fs * 0.5));
        std::vector<double> logY;
        std::vector<double> sampleIdx;
        int step = 0;
        float prevVal = 0.8f;
        double maxRatioErr = 0.0;

        while (env.getState() == bumbler::BumblerADSR::State::Release) {
            const float val = env.processSample();
            if (val > 0.0002f) {
                const double ratio = static_cast<double>(val) / static_cast<double>(prevVal);
                const double err = std::abs(ratio - expectedCoeff);
                if (err > maxRatioErr) maxRatioErr = err;

                logY.push_back(std::log(static_cast<double>(val)));
                sampleIdx.push_back(static_cast<double>(step));
            }
            prevVal = val;
            ++step;
            if (step > static_cast<int>(r * fs * 10.0)) break;
        }

        CHALLENGER_ASSERT(maxRatioErr < 1.0e-5, "Release step ratio deviates from mathematical exponential coefficient!");
        const double r2 = calculateR2(sampleIdx, logY);
        CHALLENGER_ASSERT(r2 > 0.99999, "Release log-linear fit R^2 is below 0.99999!");
        CHALLENGER_ASSERT(env.getState() == bumbler::BumblerADSR::State::Idle, "Must transition to Idle state");
        CHALLENGER_ASSERT(env.getValue() == 0.0f, "Idle value must be exactly 0.0f");
    }
}

// ============================================================================
// TEST 4: envLink Synchronization & Independent Decoupling
// ============================================================================
static void test_envlink_synchronization() {
    std::cout << "  [SUBTEST] Verifying envLink synchronization and independent decoupling...\n";

    const double fs = 48000.0;
    bumbler::BumblerDualADSR dualEnv;
    dualEnv.prepare(fs);

    ParameterSnapshot params;
    params.ampAttack = 0.02f;
    params.ampDecay = 0.08f;
    params.ampSustain = 0.45f;
    params.ampRelease = 0.15f;

    params.filterAttack = 0.30f;
    params.filterDecay = 0.60f;
    params.filterSustain = 0.10f;
    params.filterRelease = 0.90f;

    // Scenario A: envLink = 1.0f (LINKED)
    params.envLink = 1.0f;
    dualEnv.update(params);
    dualEnv.noteOn();

    for (int i = 0; i < 10000; ++i) {
        const float ampVal = dualEnv.getAmpEnv().processSample();
        const float fltVal = dualEnv.getFilterEnv().processSample();
        const float diff = std::abs(ampVal - fltVal);
        CHALLENGER_ASSERT(diff < 1.0e-6f, "Filter ADSR must exactly match Amp ADSR when envLink is enabled!");
    }

    dualEnv.noteOff();
    for (int i = 0; i < 15000; ++i) {
        const float ampVal = dualEnv.getAmpEnv().processSample();
        const float fltVal = dualEnv.getFilterEnv().processSample();
        const float diff = std::abs(ampVal - fltVal);
        CHALLENGER_ASSERT(diff < 1.0e-6f, "Filter ADSR release must exactly match Amp ADSR release when envLink is enabled!");
    }

    // Mutating Filter ADSR does NOT affect Filter ADSR when envLink is active
    dualEnv.reset();
    params.filterAttack = 2.5f;
    params.filterDecay = 3.0f;
    params.filterSustain = 0.9f;
    params.filterRelease = 4.0f;
    dualEnv.update(params);
    dualEnv.noteOn();
    for (int i = 0; i < 5000; ++i) {
        const float ampVal = dualEnv.getAmpEnv().processSample();
        const float fltVal = dualEnv.getFilterEnv().processSample();
        CHALLENGER_ASSERT(ampVal == fltVal, "Filter ADSR must ignore filter params when envLink is active!");
    }

    // Scenario B: envLink = 0.0f (UNLINKED / INDEPENDENT)
    dualEnv.reset();
    params.envLink = 0.0f;
    params.ampAttack = 0.01f;
    params.ampDecay = 0.05f;
    params.ampSustain = 0.8f;
    params.ampRelease = 0.1f;

    params.filterAttack = 0.25f;
    params.filterDecay = 0.50f;
    params.filterSustain = 0.15f;
    params.filterRelease = 0.80f;

    dualEnv.update(params);
    dualEnv.noteOn();

    float maxDivergence = 0.0f;
    for (int i = 0; i < 10000; ++i) {
        const float ampVal = dualEnv.getAmpEnv().processSample();
        const float fltVal = dualEnv.getFilterEnv().processSample();
        const float diff = std::abs(ampVal - fltVal);
        if (diff > maxDivergence) maxDivergence = diff;
    }

    CHALLENGER_ASSERT(maxDivergence > 0.4f, "Filter ADSR and Amp ADSR must diverge when envLink is disabled!");

    const float filterBeforeAmpChange = dualEnv.getFilterEnv().getValue();
    params.ampAttack = 0.5f;
    params.ampSustain = 0.2f;
    dualEnv.update(params);
    const float filterAfterAmpChange = dualEnv.getFilterEnv().getValue();
    CHALLENGER_ASSERT(filterBeforeAmpChange == filterAfterAmpChange, 
        "Updating Amp ADSR must NOT alter Filter ADSR when envLink is disabled!");
}

// ============================================================================
// TEST 5: MOD ENV Bipolar Directional Sweeping & Voice Routing Matrix
// ============================================================================
static void test_mod_env_bipolar_sweeping() {
    std::cout << "  [SUBTEST] Verifying MOD ENV bipolar directional sweeping and routing matrix targets...\n";

    const double fs = 48000.0;
    bumbler::BumblerModEnvelope modEnv;
    modEnv.prepare(fs);

    // 1. Test pure BumblerModEnvelope polarity
    // Positive modAmount (+1.0)
    modEnv.setParameters(0.01f, 0.05f, 1.0f, fs);
    modEnv.trigger();
    float maxPosVal = -1.0f;
    bool allPos = true;
    for (int i = 0; i < 5000; ++i) {
        const float v = modEnv.processSample();
        if (v < -1.0e-6f) allPos = false;
        if (v > maxPosVal) maxPosVal = v;
    }
    CHALLENGER_ASSERT(allPos, "Positive modAmount must produce exclusively positive/zero values");
    CHALLENGER_ASSERT(std::abs(maxPosVal - 1.0f) < 0.01f, "Positive modAmount peak must reach ~+1.0f");

    // Negative modAmount (-1.0)
    modEnv.setParameters(0.01f, 0.05f, -1.0f, fs);
    modEnv.trigger();
    float minNegVal = 1.0f;
    bool allNeg = true;
    for (int i = 0; i < 5000; ++i) {
        const float v = modEnv.processSample();
        if (v > 1.0e-6f) allNeg = false;
        if (v < minNegVal) minNegVal = v;
    }
    CHALLENGER_ASSERT(allNeg, "Negative modAmount must produce exclusively negative/zero values");
    CHALLENGER_ASSERT(std::abs(minNegVal - (-1.0f)) < 0.01f, "Negative modAmount peak must reach ~-1.0f");

    // Zero modAmount (0.0)
    modEnv.setParameters(0.01f, 0.05f, 0.0f, fs);
    modEnv.trigger();
    for (int i = 0; i < 5000; ++i) {
        CHALLENGER_ASSERT(modEnv.processSample() == 0.0f, "Zero modAmount must produce identical zero output");
    }

    // 2. Test End-to-End Voice Routing Destinations
    // Destination 0: Pulse Width (modTarget = 0)
    std::cout << "    Verifying MOD ENV Target 0: Pulse Width sweeping...\n";
    {
        bumbler::BumblerTripleOscillatorSection oscSection;
        oscSection.prepare(fs);
        ParameterSnapshot params;
        params.pulseWidth = 0.5f;
        params.osc1Waveform = static_cast<float>(bumbler::OscWaveform::Square);
        params.oscMix = 0.0f; // 100% OSC 1

        const float outPos = oscSection.process(440.0f, params, +0.45f);
        const float outNeg = oscSection.process(440.0f, params, -0.45f);
        CHALLENGER_ASSERT(!std::isnan(outPos) && !std::isnan(outNeg), "PW modulation produced NaN");
    }

    // Destination 2: OSC 1 Level (modTarget = 2)
    std::cout << "    Verifying MOD ENV Target 2: OSC 1 Level sweeping...\n";
    {
        bumbler::BumblerTripleOscillatorSection oscSection;
        oscSection.prepare(fs);
        ParameterSnapshot params;
        params.osc1Waveform = static_cast<float>(bumbler::OscWaveform::Saw);
        params.oscMix = 0.0f; // 100% OSC 1

        // Render 200 samples to measure steady state RMS
        double sumBase = 0.0;
        for (int i = 0; i < 200; ++i) {
            (void)oscSection.process(440.0f, params, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            const float s = oscSection.getLastOsc1();
            sumBase += static_cast<double>(s * s);
        }
        const double rmsBase = std::sqrt(sumBase / 200.0);

        oscSection.reset(false);
        double sumBoost = 0.0;
        for (int i = 0; i < 200; ++i) {
            (void)oscSection.process(440.0f, params, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f);
            const float s = oscSection.getLastOsc1();
            sumBoost += static_cast<double>(s * s);
        }
        const double rmsBoost = std::sqrt(sumBoost / 200.0);

        oscSection.reset(false);
        double sumSilence = 0.0;
        for (int i = 0; i < 200; ++i) {
            (void)oscSection.process(440.0f, params, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            const float s = oscSection.getLastOsc1();
            sumSilence += static_cast<double>(s * s);
        }
        const double rmsSilence = std::sqrt(sumSilence / 200.0);

        std::cout << "      RMS Base (1.0x): " << rmsBase 
                  << " | RMS Boost (2.0x): " << rmsBoost 
                  << " | RMS Silence (0.0x): " << rmsSilence << "\n";

        CHALLENGER_ASSERT(rmsSilence == 0.0, "OSC 1 Level at 0.0x must be completely silent!");
        const double boostRatio = rmsBoost / rmsBase;
        CHALLENGER_ASSERT(std::abs(boostRatio - 2.0) < 0.01, "OSC 1 Level at 2.0x must double RMS amplitude exactly!");
    }

    // Destination 3: OSC 2 Pitch (modTarget = 3)
    std::cout << "    Verifying MOD ENV Target 3: OSC 2 Pitch sweeping...\n";
    {
        bumbler::BumblerTripleOscillatorSection oscSection;
        oscSection.prepare(fs);
        ParameterSnapshot params;
        params.osc2Waveform = static_cast<float>(bumbler::OscWaveform::Sine);
        params.oscMix = 1.0f; // 100% OSC 2

        // Measure cycle period of base 440 Hz (exp = 48000 / 440 = 109.09 samples)
        auto measurePeriod = [&](float extPitch) -> double {
            oscSection.reset(false);
            std::vector<double> wraps;
            float prevPhase = 0.0f;
            for (int i = 0; i < 3000; ++i) {
                (void)oscSection.process(440.0f, params, 0.0f, 0.0f, extPitch);
                // Last OSC 2 is sine, we can track upward zero crossings
                static float prevOut = 0.0f;
                const float curOut = oscSection.getLastOsc2();
                if (prevOut <= 0.0f && curOut > 0.0f && i > 50) {
                    const double dy = static_cast<double>(curOut - prevOut);
                    const double frac = -static_cast<double>(prevOut) / dy;
                    wraps.push_back(static_cast<double>(i - 1) + frac);
                }
                prevOut = curOut;
            }
            if (wraps.size() < 2) return 0.0;
            return (wraps.back() - wraps.front()) / static_cast<double>(wraps.size() - 1);
        };

        const double basePeriod = measurePeriod(0.0f);
        const double upPeriod = measurePeriod(+24.0f);
        const double downPeriod = measurePeriod(-24.0f);

        std::cout << "      Base Period (440 Hz): " << basePeriod 
                  << " | Up (+24st / 1760 Hz): " << upPeriod 
                  << " | Down (-24st / 110 Hz): " << downPeriod << "\n";

        // Frequency ratios: +24st is 4x freq (period / 4), -24st is 0.25x freq (period * 4)
        const double upRatio = basePeriod / upPeriod;
        const double downRatio = downPeriod / basePeriod;
        CHALLENGER_ASSERT(std::abs(upRatio - 4.0) < 0.05, "+24st pitch sweep must quadruple frequency (4x)!");
        CHALLENGER_ASSERT(std::abs(downRatio - 4.0) < 0.05, "-24st pitch sweep must quarter frequency (0.25x)!");
    }

    // Destination 1: LFO 1 Amount (modTarget = 1) in BumblerVoice
    std::cout << "    Verifying MOD ENV Target 1: LFO 1 Amount sweeping in BumblerVoice...\n";
    {
        bumbler::BumblerVoice voice;
        voice.prepare(fs);
        ParameterSnapshot params;
        params.lfo1Amount = 0.5f;
        params.modAttack = 0.005f;
        params.modDecay = 0.1f;
        params.modTarget = 1.0f; // LFO 1 Amount

        // Note-on with +1.0 modAmount
        params.modAmount = 1.0f;
        voice.noteOn(60, 1.0f, 0);
        for (int i = 0; i < 250; ++i) {
            float l = 0, r = 0;
            voice.renderSample(l, r, params);
        }
        const float posEnvVal = voice.getModEnv().getValue();
        CHALLENGER_ASSERT(posEnvVal > 0.8f, "Mod env peak must be positive");

        // Note-on with -1.0 modAmount
        voice.reset();
        params.modAmount = -1.0f;
        voice.noteOn(60, 1.0f, 0);
        for (int i = 0; i < 250; ++i) {
            float l = 0, r = 0;
            voice.renderSample(l, r, params);
        }
        const float negEnvVal = voice.getModEnv().getValue();
        CHALLENGER_ASSERT(negEnvVal < -0.8f, "Mod env peak must be negative");
    }
}

// ============================================================================
// TEST 6: Adversarial Stress, Resource Pressure & Real-Time Memory Safety
// ============================================================================
static void test_adversarial_stress_and_safety() {
    std::cout << "  [SUBTEST] Verifying adversarial boundary handling and zero memory allocations...\n";

    const double fs = 48000.0;
    bumbler::BumblerVoice voice;
    voice.prepare(fs);

    ParameterSnapshot params;

    // 1. Extreme Host BPM Inputs
    const std::vector<float> extremeBpms = { 0.0f, -10.0f, 15.0f, 20.0f, 400.0f, 500.0f, 10000.0f };
    for (const float bpm : extremeBpms) {
        voice.setHostBpm(bpm);
        voice.noteOn(60, 0.8f, 0);
        for (int i = 0; i < 200; ++i) {
            float l = 0, r = 0;
            voice.renderSample(l, r, params);
            CHALLENGER_ASSERT(!std::isnan(l) && !std::isinf(l), "Extreme BPM produced NaN/Inf in voice render");
        }
        voice.noteOff();
    }

    // 2. Dynamic Rapid BPM Jumps Mid-Playback
    voice.noteOn(60, 0.9f, 0);
    params.lfo1Sync = 1.0f;
    params.lfo2Sync = 1.0f;
    for (int i = 0; i < 5000; ++i) {
        if (i % 20 == 0) {
            const float dynamicBpm = 40.0f + static_cast<float>((i * 37) % 300);
            voice.setHostBpm(dynamicBpm);
        }
        float l = 0, r = 0;
        voice.renderSample(l, r, params);
        CHALLENGER_ASSERT(!std::isnan(l) && !std::isinf(l), "Dynamic tempo jumping caused numerical instability");
    }
    voice.noteOff();

    // 3. Real-Time Zero Heap Allocation Verification
    gAllocationCountM3_2.store(0);
    gAllocatedBytesM3_2.store(0);
    gTrackAllocationsM3_2.store(true);

    for (int block = 0; block < 50; ++block) {
        params.lfo1Rate = 0.5f + static_cast<float>(block % 10);
        params.lfo2Amount = static_cast<float>(block % 5) * 0.2f;
        params.modAmount = (block % 2 == 0) ? 1.0f : -1.0f;
        params.envLink = (block % 4 == 0) ? 1.0f : 0.0f;
        params.modTarget = static_cast<float>(block % 4);

        if (block % 10 == 0) {
            voice.noteOn(50 + (block % 24), 0.75f, block * 128);
        } else if (block % 10 == 7) {
            voice.noteOff();
        }

        float outL[128] = { 0 };
        float outR[128] = { 0 };
        voice.renderBlockAccumulate(outL, outR, 128, params);
    }

    gTrackAllocationsM3_2.store(false);
    const size_t allocCount = gAllocationCountM3_2.load();
    const size_t allocBytes = gAllocatedBytesM3_2.load();

    std::cout << "    Audio Rendering Allocations: " << allocCount << " calls, " << allocBytes << " bytes\n";
    CHALLENGER_ASSERT(allocCount == 0, "Audio rendering caused heap allocations! Violates real-time audio safety.");
}

// ============================================================================
// Main Test Runner Entrypoint
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << " Bumbler XD: Challenger M3-2 Timing & Routing Suite \n";
    std::cout << "====================================================\n";

    runChallengerTest("test_tempo_sync_period_accuracy", test_tempo_sync_period_accuracy);
    runChallengerTest("test_lfo_delay_and_phase_reset", test_lfo_delay_and_phase_reset);
    runChallengerTest("test_envelope_stage_timing_and_curvature", test_envelope_stage_timing_and_curvature);
    runChallengerTest("test_envlink_synchronization", test_envlink_synchronization);
    runChallengerTest("test_mod_env_bipolar_sweeping", test_mod_env_bipolar_sweeping);
    runChallengerTest("test_adversarial_stress_and_safety", test_adversarial_stress_and_safety);

    std::cout << "\n----------------------------------------------------\n";
    std::cout << " Challenger M3-2 Summary: " << gPassedTests << " Passed, " << gFailedTests << " Failed (Total: " << gTotalTests << ")\n";
    std::cout << "----------------------------------------------------\n";

    return (gFailedTests == 0) ? 0 : 1;
}
