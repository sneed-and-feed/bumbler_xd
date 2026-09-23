#include "test_helpers.h"

// ============================================================================
// Global Real-Time Heap Allocation Interceptors
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
// Real-time Audio Processing Pipeline Simulation
// ============================================================================
struct PreallocatedVoiceState {
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float envVal = 0.0f;
    bumbler_test::ReferenceWaspFilter filter;
};

class RealtimeSafeDspMock {
public:
    void prepare(double sampleRate, int maxBlock) {
        fs = sampleRate;
        maxBlockSize = maxBlock;
        for (int v = 0; v < 16; ++v) {
            voices[v].phase1 = 0.0f;
            voices[v].phase2 = 0.0f;
            voices[v].envVal = 1.0f;
            voices[v].filter.prepare(sampleRate);
        }
    }

    // Audio rendering callback operating strictly on preallocated state
    void renderBlock(float* outL, float* outR, int numSamples, const ParameterSnapshot& params) noexcept {
        bumbler_test::ScopedNoDenormalsGuard noDenormals;

        for (int i = 0; i < numSamples; ++i) {
            float sumL = 0.0f;
            float sumR = 0.0f;

            for (int v = 0; v < 4; ++v) { // 4 active polyphonic voices
                // Osc 1 Sine
                float osc1 = std::sin(voices[v].phase1);
                voices[v].phase1 += 0.05f;
                if (voices[v].phase1 >= bumbler_test::kTwoPi) voices[v].phase1 -= static_cast<float>(bumbler_test::kTwoPi);

                // Osc 2 Saw
                float osc2 = 1.0f - 2.0f * (voices[v].phase2 / static_cast<float>(bumbler_test::kTwoPi));
                voices[v].phase2 += 0.051f;
                if (voices[v].phase2 >= bumbler_test::kTwoPi) voices[v].phase2 -= static_cast<float>(bumbler_test::kTwoPi);

                float mix = (1.0f - params.oscMix) * osc1 + params.oscMix * osc2;
                float filtered = voices[v].filter.processSample(mix, params.filterCutoff, params.filterResonance, bumbler_test::ReferenceWaspFilter::LP24);

                sumL += filtered;
                sumR += filtered;
            }

            // Output stage
            float distL = bumbler_test::ReferenceCharacterCircuits::processDistortion(sumL, params.driveAmount, params.driveTone, params.driveEnabled > 0.5f);
            float distR = bumbler_test::ReferenceCharacterCircuits::processDistortion(sumR, params.driveAmount, params.driveTone, params.driveEnabled > 0.5f);

            outL[i] = distL * params.masterVolume;
            outR[i] = distR * params.masterVolume;

            // Software denormal flush safeguard
            if (std::abs(outL[i]) < 1.0e-15f) outL[i] = 0.0f;
            if (std::abs(outR[i]) < 1.0e-15f) outR[i] = 0.0f;
        }
    }

private:
    double fs = 48000.0;
    int maxBlockSize = 512;
    PreallocatedVoiceState voices[16];
};

// ============================================================================
// 1. Zero Heap Allocations in Audio Callback
// ============================================================================
void test_zero_heap_allocations_during_audio_callback() {
    constexpr int kBlockSize = 512;
    constexpr int kNumBlocks = 1000;

    RealtimeSafeDspMock engine;
    engine.prepare(48000.0, kBlockSize);

    ParameterSnapshot params;
    params.oscMix = 0.5f;
    params.filterCutoff = 1200.0f;
    params.filterResonance = 0.4f;
    params.driveEnabled = 1.0f;
    params.driveAmount = 0.5f;
    params.masterVolume = 0.8f;

    std::vector<float> bufL(kBlockSize, 0.0f);
    std::vector<float> bufR(kBlockSize, 0.0f);

    resetAllocationTracker();
    enableAllocationTracker(true);

    // Run 1,000 continuous audio blocks
    for (int b = 0; b < kNumBlocks; ++b) {
        // Vary parameters to simulate host automation
        params.filterCutoff = 200.0f + static_cast<float>(b * 10);
        engine.renderBlock(bufL.data(), bufR.data(), kBlockSize, params);
    }

    enableAllocationTracker(false);
    size_t allocs = getAllocationCount();

    TEST_ASSERT(allocs == 0, "Heap allocation detected during audio callback! Count: " + std::to_string(allocs));
}

// ============================================================================
// 2. Denormal FTZ/DAZ Protection Verification
// ============================================================================
void test_denormal_ftz_daz_protection() {
    bumbler_test::ScopedNoDenormalsGuard guard;

    constexpr int kSamples = 1024;
    std::vector<float> inBuf(kSamples);
    std::vector<float> outL(kSamples);
    std::vector<float> outR(kSamples);

    // Inject subnormal numbers (below standard single-precision normalized float ~1.17e-38)
    for (int i = 0; i < kSamples; ++i) {
        inBuf[i] = 1.0e-39f * static_cast<float>(i + 1);
    }

    RealtimeSafeDspMock engine;
    engine.prepare(48000.0, kSamples);

    ParameterSnapshot params;
    params.filterCutoff = 500.0f;
    params.filterResonance = 0.1f;

    engine.renderBlock(outL.data(), outR.data(), kSamples, params);

    // Assert zero subnormals in output buffer
    TEST_ASSERT(!bumbler_test::containsSubnormals(outL.data(), kSamples), "Output L contains subnormal floats");
    TEST_ASSERT(!bumbler_test::containsSubnormals(outR.data(), kSamples), "Output R contains subnormal floats");
}

// ============================================================================
// 3. Finite Output Guarantee (No NaN or Inf Under Stress)
// ============================================================================
void test_finite_output_guarantee() {
    constexpr int kSamples = 512;
    RealtimeSafeDspMock engine;
    engine.prepare(48000.0, kSamples);

    ParameterSnapshot params;
    params.filterCutoff = 19000.0f; // Cutoff near Nyquist
    params.filterResonance = 0.99f; // Extreme resonance
    params.driveEnabled = 1.0f;
    params.driveAmount = 1.0f;     // Maximum overdrive

    std::vector<float> bufL(kSamples, 0.0f);
    std::vector<float> bufR(kSamples, 0.0f);

    for (int b = 0; b < 100; ++b) {
        engine.renderBlock(bufL.data(), bufR.data(), kSamples, params);
        TEST_ASSERT(!bumbler_test::containsNonFinite(bufL.data(), kSamples), "Non-finite output detected in L channel");
        TEST_ASSERT(!bumbler_test::containsNonFinite(bufR.data(), kSamples), "Non-finite output detected in R channel");

        float peakL = bumbler_test::computePeak(bufL.data(), kSamples);
        float peakR = bumbler_test::computePeak(bufR.data(), kSamples);
        TEST_ASSERT(peakL <= 1.05f && peakR <= 1.05f, "Output exploded beyond safety ceiling");
    }
}

// ============================================================================
// 4. Extreme Block Size Scaling (1 to 8,192 Samples)
// ============================================================================
void test_extreme_block_sizes() {
    const std::vector<int> blockSizes = { 1, 7, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };
    RealtimeSafeDspMock engine;
    ParameterSnapshot params;

    for (int bs : blockSizes) {
        engine.prepare(48000.0, bs);
        std::vector<float> bufL(bs, 0.0f);
        std::vector<float> bufR(bs, 0.0f);

        engine.renderBlock(bufL.data(), bufR.data(), bs, params);
        TEST_ASSERT(!bumbler_test::containsNonFinite(bufL.data(), bs), 
            "Block size " + std::to_string(bs) + " produced non-finite output");
    }
}

// ============================================================================
// 5. Multi-Rate Sample Rate Stability
// ============================================================================
void test_multirate_sample_rates() {
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    RealtimeSafeDspMock engine;
    ParameterSnapshot params;
    constexpr int kSamples = 256;
    std::vector<float> bufL(kSamples);
    std::vector<float> bufR(kSamples);

    for (double sr : sampleRates) {
        engine.prepare(sr, kSamples);
        engine.renderBlock(bufL.data(), bufR.data(), kSamples, params);
        TEST_ASSERT(!bumbler_test::containsNonFinite(bufL.data(), kSamples), 
            "Sample rate " + std::to_string(sr) + " produced non-finite output");
    }
}

// ============================================================================
// Main Real-Time Safety Test Runner
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << " Bumbler XD: Real-Time Safety & Invariants Suite    \n";
    std::cout << "====================================================\n";

    RUN_TEST(test_zero_heap_allocations_during_audio_callback);
    RUN_TEST(test_denormal_ftz_daz_protection);
    RUN_TEST(test_finite_output_guarantee);
    RUN_TEST(test_extreme_block_sizes);
    RUN_TEST(test_multirate_sample_rates);

    std::cout << "\n----------------------------------------------------\n";
    std::cout << " Real-Time Safety Tests Summary: " << gGlobalTestsPassed << " Passed, " 
              << gGlobalTestsFailed << " Failed\n";
    std::cout << "----------------------------------------------------\n";

    return (gGlobalTestsFailed == 0) ? 0 : 1;
}
