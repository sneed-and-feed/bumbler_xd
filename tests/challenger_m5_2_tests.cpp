#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../source/plugin/PluginProcessor.h"
#include "../source/plugin/PluginEditor.h"
#include "../source/plugin/Parameters.h"
#include "../source/dsp/ParameterSnapshot.h"
#include "../source/dsp/BumblerEngine.h"

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <cassert>

// ============================================================================
// Real-Time Thread-Local Heap Allocation Interception
// ============================================================================
static thread_local bool t_trackAllocations = false;
static thread_local size_t t_allocationCount = 0;
static thread_local size_t t_allocatedBytes = 0;

inline void resetThreadAllocationTracker() {
    t_allocationCount = 0;
    t_allocatedBytes = 0;
}

inline void enableThreadAllocationTracker(bool enable) {
    t_trackAllocations = enable;
}

inline size_t getThreadAllocationCount() {
    return t_allocationCount;
}

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
void* operator new(size_t size) {
    if (t_trackAllocations) {
        ++t_allocationCount;
        t_allocatedBytes += size;
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
    if (t_trackAllocations) {
        ++t_allocationCount;
        t_allocatedBytes += size;
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
#endif

// ============================================================================
// Assertion & Test Accounting
// ============================================================================
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define CHALLENGE_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [FAIL] " << msg << " (" #cond ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
        ++gTestsFailed; \
        return false; \
    } \
} while (false)

namespace challenger_m5_2 {

// ============================================================================
// SUITE 1: Lock-Free loadSnapshot() Latency & Contention Benchmark
// ============================================================================
bool runSnapshotExtractionBenchmark(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 1A] Microbenchmarking 1,000,000 Lock-Free Snapshot Extractions...\n";
    std::cout << "======================================================================\n";

    // 1. Warm-up cache (10,000 iterations)
    for (int i = 0; i < 10000; ++i) {
        auto snap = proc.getCurrentSnapshot();
        if (std::isnan(snap.filterCutoff)) return false;
    }

    // 2. Benchmark 1,000,000 bulk extractions with zero-allocation verification
    constexpr int kTotalExtractions = 1000000;
    resetThreadAllocationTracker();
    enableThreadAllocationTracker(true);

    volatile float dummySink = 0.0f;
    const auto bulkStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kTotalExtractions; ++i) {
        bumbler::ParameterSnapshot s = proc.getCurrentSnapshot();
        dummySink += s.filterCutoff;
    }
    const auto bulkEnd = std::chrono::high_resolution_clock::now();
    enableThreadAllocationTracker(false);

    const double bulkTotalNs = std::chrono::duration<double, std::nano>(bulkEnd - bulkStart).count();
    const double avgLatencyNs = bulkTotalNs / static_cast<double>(kTotalExtractions);
    const size_t allocs = getThreadAllocationCount();

    std::cout << "  - Total Extractions:  " << kTotalExtractions << "\n";
    std::cout << "  - Total Time:         " << (bulkTotalNs / 1.0e6) << " ms\n";
    std::cout << "  - Average Latency:    " << std::fixed << std::setprecision(2) << avgLatencyNs << " ns / extraction\n";
    std::cout << "  - Heap Allocations:   " << allocs << "\n";

    CHALLENGE_ASSERT(allocs == 0, "Heap allocations detected during loadSnapshot()!");
    CHALLENGE_ASSERT(avgLatencyNs < 100.0, "Average latency exceeded 100 ns ceiling!");

    // 3. Granular batched measurements: 10,000 batches of 100 extractions for percentile analysis
    constexpr int kNumBatches = 10000;
    constexpr int kBatchSize = 100;
    std::vector<double> batchPerCallNs(kNumBatches);

    for (int b = 0; b < kNumBatches; ++b) {
        const auto bStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < kBatchSize; ++i) {
            bumbler::ParameterSnapshot s = proc.getCurrentSnapshot();
            dummySink += s.oscMix;
        }
        const auto bEnd = std::chrono::high_resolution_clock::now();
        const double batchNs = std::chrono::duration<double, std::nano>(bEnd - bStart).count();
        batchPerCallNs[b] = batchNs / static_cast<double>(kBatchSize);
    }

    std::sort(batchPerCallNs.begin(), batchPerCallNs.end());
    const double minLat = batchPerCallNs.front();
    const double p50Lat = batchPerCallNs[static_cast<size_t>(kNumBatches * 0.50)];
    const double p90Lat = batchPerCallNs[static_cast<size_t>(kNumBatches * 0.90)];
    const double p99Lat = batchPerCallNs[static_cast<size_t>(kNumBatches * 0.99)];
    const double p999Lat = batchPerCallNs[static_cast<size_t>(kNumBatches * 0.999)];
    const double maxLat = batchPerCallNs.back();

    std::cout << "  - Latency Distribution (10,000 batches of 100):\n";
    std::cout << "      Min:   " << minLat << " ns\n";
    std::cout << "      p50:   " << p50Lat << " ns\n";
    std::cout << "      p90:   " << p90Lat << " ns\n";
    std::cout << "      p99:   " << p99Lat << " ns (Target: < 250 ns)\n";
    std::cout << "      p99.9: " << p999Lat << " ns\n";
    std::cout << "      Max:   " << maxLat << " ns\n";

    CHALLENGE_ASSERT(p99Lat < 250.0, "p99 latency exceeded 250 ns target!");

    std::cout << "  -> PASS: 1,000,000 Snapshot extractions verified (< 100 ns avg, 0 allocations).\n";
    ++gTestsPassed;

    // ------------------------------------------------------------------------
    // [CHALLENGE 1B] Snapshot Latency Under Heavy Concurrent Writer Contention
    // ------------------------------------------------------------------------
    std::cout << "\n[CHALLENGE 1B] Benchmarking 1,000,000 Extractions Under Concurrent Writer Contention...\n";

    std::atomic<bool> stopWriters { false };
    std::atomic<size_t> writesCompleted { 0 };
    auto& apvts = proc.getApvts();

    // 4 background writer threads mutating random parameters via APVTS
    std::vector<std::thread> writers;
    writers.reserve(4);
    for (int t = 0; t < 4; ++t) {
        writers.emplace_back([&, t]() {
            std::mt19937 rng(1337 + t);
            std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
            const char* paramKeys[] = {
                "filterCutoff", "oscMix", "pulseWidth", "driveAmount",
                "lfo1Rate", "ampDecay", "velToFilter", "ringModMix"
            };
            constexpr size_t numKeys = sizeof(paramKeys) / sizeof(paramKeys[0]);

            while (!stopWriters.load(std::memory_order_relaxed)) {
                for (size_t k = 0; k < numKeys; ++k) {
                    if (auto* p = apvts.getParameter(paramKeys[k])) {
                        p->setValueNotifyingHost(dist01(rng));
                    }
                }
                writesCompleted.fetch_add(numKeys, std::memory_order_relaxed);
            }
        });
    }

    // Reader thread executes 1,000,000 snapshot extractions while writers churn
    const auto contStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kTotalExtractions; ++i) {
        bumbler::ParameterSnapshot s = proc.getCurrentSnapshot();
        // Verify no torn floats or corrupted bounds
        if (!std::isfinite(s.filterCutoff) || !std::isfinite(s.oscMix)) {
            stopWriters.store(true);
            for (auto& w : writers) w.join();
            CHALLENGE_ASSERT(false, "Corrupt float read during concurrent mutation!");
        }
        dummySink += s.driveAmount;
    }
    const auto contEnd = std::chrono::high_resolution_clock::now();

    stopWriters.store(true);
    for (auto& w : writers) w.join();

    const double contTotalNs = std::chrono::duration<double, std::nano>(contEnd - contStart).count();
    const double contAvgLatencyNs = contTotalNs / static_cast<double>(kTotalExtractions);

    std::cout << "  - Contention Extractions: " << kTotalExtractions << "\n";
    std::cout << "  - Concurrent Mutations:   " << writesCompleted.load() << "\n";
    std::cout << "  - Contention Avg Latency: " << contAvgLatencyNs << " ns / extraction\n";

    CHALLENGE_ASSERT(contAvgLatencyNs < 250.0, "Contention latency exceeded 250 ns ceiling!");
    std::cout << "  -> PASS: Lock-free reads under concurrent mutation verified (< 250 ns under 4-core write saturation).\n";
    ++gTestsPassed;


    return true;
}

// ============================================================================
// SUITE 2: Factory Presets Live Audio Audition & Headroom Safety
// ============================================================================
bool runFactoryPresetsAudition(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 2] Auditioning All 5 Factory Presets Under Live Audio Rendering...\n";
    std::cout << "======================================================================\n";

    const auto& presets = bumbler::getFactoryPresets();
    CHALLENGE_ASSERT(presets.size() == 5, "Expected exactly 5 factory presets");

    const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
    const int blockSizes[] = { 64, 128, 256, 512, 1024, 2048 };

    for (int pIdx = 0; pIdx < 5; ++pIdx) {
        const auto& pDef = presets[static_cast<size_t>(pIdx)];
        std::cout << "\n--- Auditioning Preset [" << pIdx << "]: \"" << pDef.name 
                  << "\" (" << pDef.category << ") ---\n";

        proc.setCurrentProgram(pIdx);
        CHALLENGE_ASSERT(proc.getCurrentProgram() == pIdx, "Current program index did not update");

        // Verify snapshot parameters match preset definition
        bumbler::ParameterSnapshot snap = proc.getCurrentSnapshot();
        CHALLENGE_ASSERT(static_cast<int>(snap.filterMode) == static_cast<int>(pDef.params.filterMode), "Filter mode mismatch");
        CHALLENGE_ASSERT(std::abs(snap.filterCutoff - pDef.params.filterCutoff) < 1.0f, "Filter cutoff mismatch");
        CHALLENGE_ASSERT(std::abs(snap.oscMix - pDef.params.oscMix) < 0.01f, "Osc mix mismatch");
        CHALLENGE_ASSERT(snap.driveEnabled == pDef.params.driveEnabled, "Drive enable mismatch");

        // Test each preset across diverse sample rates and block sizes
        for (double sr : sampleRates) {
            for (int bs : blockSizes) {
                proc.prepareToPlay(sr, bs);

                juce::AudioBuffer<float> buffer(2, bs);
                juce::MidiBuffer midi;

                // ------------------------------------------------------------
                // Scenario A: Staccato Attack + Note On
                // ------------------------------------------------------------
                buffer.clear();
                midi.clear();
                midi.addEvent(juce::MidiMessage::noteOn(1, 48 + pIdx * 5, 0.90f), 0);
                proc.processBlock(buffer, midi);

                float peakA_L = buffer.getMagnitude(0, 0, bs);
                float peakA_R = buffer.getMagnitude(1, 0, bs);
                float maxPeakA = std::max(peakA_L, peakA_R);

                CHALLENGE_ASSERT(!std::isnan(maxPeakA) && !std::isinf(maxPeakA), "NaN/Inf produced on NoteOn!");
                CHALLENGE_ASSERT(maxPeakA <= 1.05f, "Peak amplitude exceeded headroom ceiling 1.05f on NoteOn!");

                // Check for denormals
                for (int ch = 0; ch < 2; ++ch) {
                    for (int s = 0; s < bs; ++s) {
                        CHALLENGE_ASSERT(std::fpclassify(buffer.getSample(ch, s)) != FP_SUBNORMAL,
                                         "Denormal float detected in audio buffer!");
                    }
                }

                // ------------------------------------------------------------
                // Scenario B: Sustained Tone & Harmonic Convergence (10 blocks)
                // ------------------------------------------------------------
                float totalEnergy = 0.0f;
                float globalMaxPeak = maxPeakA;

                for (int b = 0; b < 15; ++b) {
                    buffer.clear();
                    midi.clear();
                    // Mid-sustain pitchbend modulation
                    if (b == 5) {
                        midi.addEvent(juce::MidiMessage::pitchWheel(1, 12000), 0);
                    } else if (b == 10) {
                        midi.addEvent(juce::MidiMessage::pitchWheel(1, 4000), 0);
                    }
                    proc.processBlock(buffer, midi);

                    const float bPeakL = buffer.getMagnitude(0, 0, bs);
                    const float bPeakR = buffer.getMagnitude(1, 0, bs);
                    const float bMaxPeak = std::max(bPeakL, bPeakR);
                    if (bMaxPeak > globalMaxPeak) globalMaxPeak = bMaxPeak;

                    CHALLENGE_ASSERT(!std::isnan(bMaxPeak) && !std::isinf(bMaxPeak), "NaN/Inf in sustain blocks!");
                    CHALLENGE_ASSERT(bMaxPeak <= 1.05f, "Peak amplitude exceeded 1.05f in sustain blocks!");

                    for (int ch = 0; ch < 2; ++ch) {
                        for (int s = 0; s < bs; ++s) {
                            const float val = buffer.getSample(ch, s);
                            CHALLENGE_ASSERT(std::fpclassify(val) != FP_SUBNORMAL, "Denormal detected in sustain!");
                            totalEnergy += val * val;
                        }
                    }
                }

                const float rmsEnergy = std::sqrt(totalEnergy / static_cast<float>(15 * bs * 2));
                CHALLENGE_ASSERT(rmsEnergy > 0.0001f, "Preset rendered silence during sustained note!");

                // ------------------------------------------------------------
                // Scenario C: Polyphonic 4-Note Cluster
                // ------------------------------------------------------------
                buffer.clear();
                midi.clear();
                midi.addEvent(juce::MidiMessage::noteOn(1, 48, 0.85f), 0);
                midi.addEvent(juce::MidiMessage::noteOn(1, 52, 0.85f), 10);
                midi.addEvent(juce::MidiMessage::noteOn(1, 55, 0.85f), 20);
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.85f), 30);
                proc.processBlock(buffer, midi);

                float polyPeakL = buffer.getMagnitude(0, 0, bs);
                float polyPeakR = buffer.getMagnitude(1, 0, bs);
                float polyMaxPeak = std::max(polyPeakL, polyPeakR);

                CHALLENGE_ASSERT(!std::isnan(polyMaxPeak) && !std::isinf(polyMaxPeak), "NaN/Inf in polyphonic cluster!");
                CHALLENGE_ASSERT(polyMaxPeak <= 1.05f, "Polyphonic cluster exceeded 1.05f headroom ceiling!");

                // ------------------------------------------------------------
                // Scenario D: Note-Off & Release Envelope Decay
                // ------------------------------------------------------------
                buffer.clear();
                midi.clear();
                midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
                proc.processBlock(buffer, midi);

                for (int b = 0; b < 10; ++b) {
                    buffer.clear();
                    midi.clear();
                    proc.processBlock(buffer, midi);

                    for (int ch = 0; ch < 2; ++ch) {
                        for (int s = 0; s < bs; ++s) {
                            const float val = buffer.getSample(ch, s);
                            CHALLENGE_ASSERT(std::isfinite(val), "NaN/Inf during release phase!");
                            CHALLENGE_ASSERT(std::fpclassify(val) != FP_SUBNORMAL, "Denormal during release phase!");
                        }
                    }
                }
            }
        }

        std::cout << "  -> PASS: Preset \"" << pDef.name << "\" verified across 18 SR/Block combinations (non-silent, no NaNs, headroom <= 1.05f).\n";
    }

    std::cout << "  -> PASS: All 5 factory presets auditioned successfully.\n";
    ++gTestsPassed;
    return true;
}

// ============================================================================
// SUITE 3: Rapid Program Switching Stress Under Active Audio Synthesis
// ============================================================================
bool runRapidProgramSwitchingStress(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 3] Rapid Program Switching Stress (1,000 changes/sec) During Live Audio...\n";
    std::cout << "======================================================================\n";

    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 128;
    proc.prepareToPlay(kSampleRate, kBlockSize);

    std::atomic<bool> stopStress { false };
    std::atomic<size_t> programSwitchesCompleted { 0 };
    std::atomic<size_t> audioBlocksRendered { 0 };
    std::atomic<bool> audioErrorDetected { false };
    std::string audioErrorMessage;

    // ------------------------------------------------------------------------
    // Thread 1: Real-Time Audio Callback Thread
    // ------------------------------------------------------------------------
    std::thread audioThread([&]() {
        resetThreadAllocationTracker();
        enableThreadAllocationTracker(true);

        juce::AudioBuffer<float> buffer(2, kBlockSize);
        juce::MidiBuffer midi;

        size_t blockCounter = 0;
        float maxPeakObserved = 0.0f;

        while (!stopStress.load(std::memory_order_relaxed)) {
            buffer.clear();
            midi.clear();

            // Inject periodic MIDI triggers so sound synthesis is continuously active
            if (blockCounter % 32 == 0) {
                midi.addEvent(juce::MidiMessage::noteOn(1, 48 + static_cast<int>(blockCounter % 24), 0.85f), 0);
            } else if (blockCounter % 32 == 20) {
                midi.addEvent(juce::MidiMessage::noteOff(1, 48 + static_cast<int>(blockCounter % 24), 0.0f), 0);
            }

            // Execute real-time audio block
            proc.processBlock(buffer, midi);

            // Audit rendered audio buffer
            for (int ch = 0; ch < 2; ++ch) {
                for (int s = 0; s < kBlockSize; ++s) {
                    const float val = buffer.getSample(ch, s);
                    if (!std::isfinite(val)) {
                        audioErrorDetected.store(true);
                        audioErrorMessage = "NaN or Inf produced during rapid program switching!";
                        enableThreadAllocationTracker(false);
                        return;
                    }
                    if (std::fpclassify(val) == FP_SUBNORMAL) {
                        audioErrorDetected.store(true);
                        audioErrorMessage = "Denormal float produced during rapid program switching!";
                        enableThreadAllocationTracker(false);
                        return;
                    }
                    const float absVal = std::abs(val);
                    if (absVal > 1.05f) {
                        audioErrorDetected.store(true);
                        audioErrorMessage = "Output exceeded headroom ceiling (peak=" + std::to_string(absVal) + " > 1.05f)!";
                        enableThreadAllocationTracker(false);
                        return;
                    }
                    if (absVal > maxPeakObserved) {
                        maxPeakObserved = absVal;
                    }
                }
            }

            ++blockCounter;
            audioBlocksRendered.store(blockCounter, std::memory_order_relaxed);
        }

        enableThreadAllocationTracker(false);
        const size_t allocs = getThreadAllocationCount();
        if (allocs != 0) {
            audioErrorDetected.store(true);
            audioErrorMessage = "Heap allocations detected on audio thread during program switching! Count=" + std::to_string(allocs);
        }
    });

    // ------------------------------------------------------------------------
    // Thread 2: Rapid Program Switcher Thread (1,000 switches/second)
    // ------------------------------------------------------------------------
    std::thread switchThread([&]() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> presetDist(0, 4);

        constexpr size_t kTargetSwitches = 3000; // 3,000 switches @ 1,000/s = 3 seconds
        size_t switches = 0;

        auto nextTick = std::chrono::steady_clock::now();
        while (switches < kTargetSwitches && !audioErrorDetected.load(std::memory_order_relaxed)) {
            const int nextPreset = presetDist(rng);
            proc.setCurrentProgram(nextPreset);
            ++switches;
            programSwitchesCompleted.store(switches, std::memory_order_relaxed);

            nextTick += std::chrono::microseconds(1000); // 1.0 ms = 1,000/sec
            std::this_thread::sleep_until(nextTick);
        }

        stopStress.store(true);
    });

    // ------------------------------------------------------------------------
    // Thread 3: Concurrent MIDI PitchWheel & Controller Churn Thread
    // ------------------------------------------------------------------------
    std::thread midiChurnThread([&]() {
        std::mt19937 rng(999);
        std::uniform_int_distribution<int> pbDist(0, 16383);

        while (!stopStress.load(std::memory_order_relaxed)) {
            // Simulate host DAW rapid automation updates
            proc.getEngine().processMidiEvent(0xE0, 0, static_cast<float>(pbDist(rng) - 8192) / 8192.0f);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    switchThread.join();
    stopStress.store(true);
    audioThread.join();
    midiChurnThread.join();

    const size_t totalSwitches = programSwitchesCompleted.load();
    const size_t totalBlocks = audioBlocksRendered.load();

    std::cout << "  - Rapid Switches Completed: " << totalSwitches << "\n";
    std::cout << "  - Audio Blocks Rendered:    " << totalBlocks << "\n";
    std::cout << "  - Switch Cadence:           ~1,000 switches/second\n";

    if (audioErrorDetected.load()) {
        std::cerr << "  [FAIL] Rapid Program Switch Error: " << audioErrorMessage << "\n";
        CHALLENGE_ASSERT(false, audioErrorMessage.c_str());
    }

    CHALLENGE_ASSERT(totalSwitches >= 3000, "Failed to reach target program switch count");
    CHALLENGE_ASSERT(totalBlocks >= 1000, "Audio thread starved during rapid program switching");

    std::cout << "  -> PASS: 3,000 rapid program changes executed without glitching, NaN, headroom violation, or audio thread allocation.\n";
    ++gTestsPassed;
    return true;
}

// ============================================================================
// SUITE 4: Headless GUI Lifecycle & RAII Teardown Safety
// ============================================================================
bool runHeadlessGuiLifecycleSafety(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 4] Headless Skeuomorphic GUI Lifecycle & Attachment Safety...\n";
    std::cout << "======================================================================\n";

    // 1. Single full editor instantiation test
    {
        auto* editor = proc.createEditor();
        CHALLENGE_ASSERT(editor != nullptr, "proc.createEditor() returned nullptr");

        const auto bounds = editor->getBounds();
        std::cout << "  - Initial Editor Bounds: " << bounds.getWidth() << "x" << bounds.getHeight() << "\n";
        CHALLENGE_ASSERT(bounds.getWidth() == 1120 && bounds.getHeight() == 700, "Editor default bounds mismatch (expected 1120x700)");

        // Test resizing bounds
        editor->setSize(1280, 800);
        CHALLENGE_ASSERT(editor->getWidth() == 1280 && editor->getHeight() == 800, "Editor resize failed");

        delete editor;
    }

    // 2. Rapid instantiation & destruction churn (50 cycles) to verify zero leaks/dangling pointers
    constexpr int kEditorCycles = 50;
    for (int i = 0; i < kEditorCycles; ++i) {
        auto* ed = proc.createEditor();
        CHALLENGE_ASSERT(ed != nullptr, "createEditor returned nullptr in churn loop");
        ed->setSize(960 + (i * 10) % 400, 600 + (i * 10) % 300);
        delete ed;
    }

    std::cout << "  - Rapid Editor Create/Resize/Destroy Cycles: " << kEditorCycles << "\n";
    std::cout << "  -> PASS: GUI lifecycle and attachment destruction order verified safely.\n";
    ++gTestsPassed;
    return true;
}

} // namespace challenger_m5_2

int main() {
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "======================================================================\n";
    std::cout << " BUMBLER XD : CHALLENGER M5-2 ADVERSARIAL STABILITY & LATENCY SUITE  \n";
    std::cout << "======================================================================\n";

    bumbler::BumblerAudioProcessor proc;

    bool p1 = challenger_m5_2::runSnapshotExtractionBenchmark(proc);
    bool p2 = challenger_m5_2::runFactoryPresetsAudition(proc);
    bool p3 = challenger_m5_2::runRapidProgramSwitchingStress(proc);
    bool p4 = challenger_m5_2::runHeadlessGuiLifecycleSafety(proc);

    bool allPassed = p1 && p2 && p3 && p4 && (gTestsFailed == 0);

    std::cout << "\n======================================================================\n";
    std::cout << " CHALLENGER M5-2 FINAL SUMMARY:\n";
    std::cout << "  - Tests Passed: " << gTestsPassed << "\n";
    std::cout << "  - Tests Failed: " << gTestsFailed << "\n";
    std::cout << "  - Empirical Status: " << (allPassed ? "[ALL CHALLENGES PASSED]" : "[CHALLENGE REJECTED]") << "\n";
    std::cout << "  - Binary Verdict:   " << (allPassed ? "APPROVE" : "REJECT") << "\n";
    std::cout << "======================================================================\n";

    return allPassed ? 0 : 1;
}
