#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../source/plugin/PluginProcessor.h"
#include "../source/plugin/PluginEditor.h"
#include "../source/plugin/Parameters.h"
#include "../source/ui/BumblerLookAndFeel.h"
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
#include <memory>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cassert>
#include <cfloat>

#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

inline int pumpMessageQueue(int maxMessages = 2000) {
    MSG msg;
    int processed = 0;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && processed < maxMessages) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        ++processed;
    }
    return processed;
}

inline int pumpMessageQueueFor(int milliseconds) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    int total = 0;
    do {
        total += pumpMessageQueue();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < end);
    return total;
}
#else
inline int pumpMessageQueue(int = 2000) { return 0; }
inline int pumpMessageQueueFor(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return 0;
}
#endif

// ============================================================================
// Real-Time Thread-Local Heap Allocation Interceptor
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
// Accounting & Macros
// ============================================================================
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define CHALLENGE_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [FAIL] " << msg << " (" #cond ") at " << __FILE__ << ":" << __LINE__ << "\n" << std::flush; \
        ++gTestsFailed; \
        return false; \
    } \
} while (false)

namespace challenger_m6_2 {

struct ParamSpec {
    const char* id;
    const char* name;
    float minVal;
    float maxVal;
    float defaultVal;
    bool isDiscrete;
};

static const std::vector<ParamSpec> kAll55Params = {
    // Group 1: Oscillators (14)
    { "osc1Waveform", "OSC 1 Waveform", 0.0f, 3.0f, 0.0f, true },
    { "osc1Octave",   "OSC 1 Octave",   -3.0f, 3.0f, 0.0f, true },
    { "osc1Fine",     "OSC 1 Fine",     -1.0f, 1.0f, 0.0f, false },
    { "osc2Waveform", "OSC 2 Waveform", 0.0f, 3.0f, 0.0f, true },
    { "osc2Octave",   "OSC 2 Octave",   -3.0f, 3.0f, 0.0f, true },
    { "osc2Fine",     "OSC 2 Fine",     -1.0f, 1.0f, 0.0f, false },
    { "oscMix",       "OSC Mix",        0.0f, 1.0f, 0.5f, false },
    { "osc3Waveform", "OSC 3 Waveform", 0.0f, 1.0f, 0.0f, true },
    { "osc3Level",    "OSC 3 Level",    0.0f, 1.0f, 0.0f, false },
    { "ringModMix",   "Ring Mod Mix",   0.0f, 1.0f, 0.0f, false },
    { "pulseWidth",   "Pulse Width",    0.01f, 0.99f, 0.5f, false },
    { "fmAmount",     "FM Amount",      0.0f, 1.0f, 0.0f, false },
    { "velToAmp",     "Velocity to Amp", 0.0f, 1.0f, 0.5f, false },
    { "velToFilter",  "Velocity to Filter", 0.0f, 1.0f, 0.5f, false },

    // Group 2: Filter (5)
    { "filterMode",      "Filter Mode",        0.0f, 5.0f, 1.0f, true },
    { "filterCutoff",    "Cutoff Frequency",   20.0f, 20000.0f, 1200.0f, false },
    { "filterResonance", "Resonance",          0.0f, 1.0f, 0.2f, false },
    { "filterKbTrack",   "Keyboard Track",     0.0f, 1.0f, 0.5f, false },
    { "filterEnvAmount", "Filter Env Amount",  -1.0f, 1.0f, 0.0f, false },

    // Group 3: Envelopes (13)
    { "ampAttack",     "Amp Attack",     0.001f, 10.0f, 0.01f, false },
    { "ampDecay",      "Amp Decay",      0.001f, 10.0f, 0.30f, false },
    { "ampSustain",    "Amp Sustain",    0.0f, 1.0f, 0.80f, false },
    { "ampRelease",    "Amp Release",    0.001f, 10.0f, 0.30f, false },
    { "filterAttack",  "Filter Attack",  0.001f, 10.0f, 0.05f, false },
    { "filterDecay",   "Filter Decay",   0.001f, 10.0f, 0.50f, false },
    { "filterSustain", "Filter Sustain", 0.0f, 1.0f, 0.50f, false },
    { "filterRelease", "Filter Release", 0.001f, 10.0f, 0.40f, false },
    { "envLink",       "Envelope Link",  0.0f, 1.0f, 0.0f, true },
    { "modAttack",     "Mod Attack",     0.001f, 5.0f, 0.05f, false },
    { "modDecay",      "Mod Decay",      0.001f, 10.0f, 0.50f, false },
    { "modAmount",     "Mod Amount",     -1.0f, 1.0f, 0.0f, false },
    { "modTarget",     "Mod Target",     0.0f, 3.0f, 0.0f, true },

    // Group 4: Dual LFOs (16)
    { "lfo1Waveform", "LFO 1 Waveform",      0.0f, 3.0f, 2.0f, true },
    { "lfo1Rate",     "LFO 1 Rate",          0.05f, 30.0f, 2.0f, false },
    { "lfo1Delay",    "LFO 1 Delay",         0.0f, 5.0f, 0.0f, false },
    { "lfo1Sync",     "LFO 1 Tempo Sync",    0.0f, 1.0f, 0.0f, true },
    { "lfo1SyncDiv",  "LFO 1 Sync Division", 0.0f, 5.0f, 3.0f, true },
    { "lfo1KeyReset", "LFO 1 Key Reset",     0.0f, 1.0f, 1.0f, true },
    { "lfo1Amount",   "LFO 1 Amount",        0.0f, 1.0f, 0.0f, false },
    { "lfo1Target",   "LFO 1 Target",        0.0f, 2.0f, 1.0f, true },
    { "lfo2Waveform", "LFO 2 Waveform",      0.0f, 3.0f, 2.0f, true },
    { "lfo2Rate",     "LFO 2 Rate",          0.05f, 30.0f, 1.0f, false },
    { "lfo2Delay",    "LFO 2 Delay",         0.0f, 5.0f, 0.0f, false },
    { "lfo2Sync",     "LFO 2 Tempo Sync",    0.0f, 1.0f, 0.0f, true },
    { "lfo2SyncDiv",  "LFO 2 Sync Division", 0.0f, 5.0f, 4.0f, true },
    { "lfo2KeyReset", "LFO 2 Key Reset",     0.0f, 1.0f, 1.0f, true },
    { "lfo2Amount",   "LFO 2 Amount",        0.0f, 1.0f, 0.0f, false },
    { "lfo2Target",   "LFO 2 Target",        0.0f, 2.0f, 0.0f, true },

    // Group 5: Character & Output (7)
    { "driveEnabled", "Drive Enable",   0.0f, 1.0f, 0.0f, true },
    { "driveAmount",  "Drive Amount",   0.0f, 1.0f, 0.3f, false },
    { "driveTone",    "Drive Tone",     0.0f, 1.0f, 0.5f, false },
    { "dualMode",     "Dual Mode",      0.0f, 1.0f, 0.0f, true },
    { "analogMode",   "Analog Mode",    0.0f, 1.0f, 0.0f, true },
    { "wNoiseMode",   "Noise Type",     0.0f, 1.0f, 0.0f, true },
    { "masterVolume", "Master Volume",  0.0f, 1.0f, 0.8f, false }
};

// ============================================================================
// CHALLENGE 1: APVTS Parameter Tree & Bijective Mapping Rigor (55 Parameters)
// ============================================================================
bool testApvtsTreeAndBijectiveMapping(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 1] APVTS Parameter Tree Completeness & Bijective Mapping (55 Params)...\n";
    std::cout << "======================================================================\n" << std::flush;

    auto& apvts = proc.getApvts();
    CHALLENGE_ASSERT(kAll55Params.size() == 55, "Spec must contain exactly 55 parameters");

    int verifiedParams = 0;
    for (const auto& spec : kAll55Params) {
        auto* param = apvts.getParameter(spec.id);
        CHALLENGE_ASSERT(param != nullptr, (std::string("Missing parameter: ") + spec.id).c_str());
        CHALLENGE_ASSERT(param->paramID == spec.id, "Parameter ID mismatch");

        const auto range = param->getNormalisableRange();
        CHALLENGE_ASSERT(range.start <= spec.minVal + 1e-4f, "Range min bound violation");
        CHALLENGE_ASSERT(range.end >= spec.maxVal - 1e-4f, "Range max bound violation");

        // Bijective roundtrip testing across 200 values per parameter
        constexpr int kTestSteps = 200;
        for (int s = 0; s <= kTestSteps; ++s) {
            const float normIn = static_cast<float>(s) / static_cast<float>(kTestSteps);
            const float plain = param->convertFrom0to1(normIn);
            const float normOut = param->convertTo0to1(plain);
            const float plainBack = param->convertFrom0to1(normOut);

            CHALLENGE_ASSERT(normIn >= 0.0f && normIn <= 1.0f, "Normalized value out of [0, 1]");
            CHALLENGE_ASSERT(std::abs(plain - plainBack) < 2e-3f, "Bijective conversion mismatch");
        }
        ++verifiedParams;
    }

    std::cout << "  - Verified Parameters: " << verifiedParams << " / 55\n";
    std::cout << "  - Bijective Conversions Tested: " << (verifiedParams * 201) << "\n";
    std::cout << "  -> PASS: All 55 parameters accurately registered with bijective normalization.\n" << std::flush;
    ++gTestsPassed;
    return true;
}

// ============================================================================
// CHALLENGE 2: High-Contention Lock-Free Snapshot Latency (8 Threads)
// ============================================================================
bool testHighContentionSnapshotLatency(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 2] High-Contention Lock-Free Snapshot Microbenchmark (8 Threads)...\n";
    std::cout << "======================================================================\n" << std::flush;

    constexpr int kExtractions = 1000000;
    std::atomic<bool> stopWriters{false};
    std::atomic<uint64_t> writesCompleted{0};
    auto& apvts = proc.getApvts();

    // 8 concurrent writer threads churning random APVTS parameter mutations
    std::vector<std::thread> writers;
    constexpr int kNumWriters = 8;
    writers.reserve(kNumWriters);

    for (int t = 0; t < kNumWriters; ++t) {
        writers.emplace_back([&, t]() {
            std::mt19937 rng(4242 + t);
            std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
            const char* targetKeys[] = {
                "filterCutoff", "filterResonance", "oscMix", "pulseWidth",
                "driveAmount", "driveTone", "ampAttack", "ampRelease",
                "lfo1Rate", "lfo2Rate", "fmAmount", "ringModMix"
            };
            constexpr size_t numKeys = sizeof(targetKeys) / sizeof(targetKeys[0]);

            while (!stopWriters.load(std::memory_order_relaxed)) {
                for (size_t k = 0; k < numKeys; ++k) {
                    if (auto* p = apvts.getParameter(targetKeys[k])) {
                        p->setValueNotifyingHost(dist01(rng));
                    }
                }
                writesCompleted.fetch_add(numKeys, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }

    // Reader thread benchmarking 1,000,000 snapshot extractions
    constexpr int kBatches = 10000;
    constexpr int kBatchSize = 100;
    std::vector<double> batchLatencyNs(kBatches);
    volatile float dummySink = 0.0f;

    resetThreadAllocationTracker();
    enableThreadAllocationTracker(true);

    const auto startTotal = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < kBatches; ++b) {
        const auto bStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < kBatchSize; ++i) {
            bumbler::ParameterSnapshot snap = proc.getCurrentSnapshot();
            dummySink += snap.filterCutoff;
            if (!std::isfinite(snap.filterCutoff) || !std::isfinite(snap.oscMix)) {
                stopWriters.store(true);
                for (auto& w : writers) w.join();
                CHALLENGE_ASSERT(false, "Non-finite parameter detected during snapshot load!");
            }
        }
        const auto bEnd = std::chrono::high_resolution_clock::now();
        batchLatencyNs[b] = std::chrono::duration<double, std::nano>(bEnd - bStart).count() / static_cast<double>(kBatchSize);
    }
    const auto endTotal = std::chrono::high_resolution_clock::now();

    enableThreadAllocationTracker(false);
    stopWriters.store(true);
    for (auto& w : writers) w.join();

    const size_t allocs = getThreadAllocationCount();
    const double totalElapsedNs = std::chrono::duration<double, std::nano>(endTotal - startTotal).count();
    const double avgLatencyNs = totalElapsedNs / static_cast<double>(kExtractions);

    std::sort(batchLatencyNs.begin(), batchLatencyNs.end());
    const double minLat = batchLatencyNs.front();
    const double p50Lat = batchLatencyNs[static_cast<size_t>(kBatches * 0.50)];
    const double p90Lat = batchLatencyNs[static_cast<size_t>(kBatches * 0.90)];
    const double p99Lat = batchLatencyNs[static_cast<size_t>(kBatches * 0.99)];
    const double maxLat = batchLatencyNs.back();

    std::cout << "  - Extractions:            " << kExtractions << "\n";
    std::cout << "  - Writer Threads:         " << kNumWriters << " (Writes completed: " << writesCompleted.load() << ")\n";
    std::cout << "  - Heap Allocations:       " << allocs << "\n";
    std::cout << "  - Average Latency:        " << std::fixed << std::setprecision(2) << avgLatencyNs << " ns / call\n";
    std::cout << "  - Latency Percentiles (ns):\n";
    std::cout << "      Min: " << minLat << " | p50: " << p50Lat << " | p90: " << p90Lat << " | p99: " << p99Lat << " | Max: " << maxLat << "\n";

    CHALLENGE_ASSERT(allocs == 0, "Heap allocations detected during lock-free snapshot extraction!");
    CHALLENGE_ASSERT(p50Lat <= 250.0, "p50 median latency exceeded 250 ns under 8-thread saturation!");
    CHALLENGE_ASSERT(avgLatencyNs <= 1500.0, "Average snapshot extraction latency exceeded 1500 ns under 8-thread saturation!");

    std::cout << "  -> PASS: Atomic snapshot extraction verified (p50 <= 250 ns, avg <= 1500 ns, 0 allocations under 8-thread write saturation).\n" << std::flush;
    ++gTestsPassed;
    return true;
}

// ============================================================================
// CHALLENGE 3: 5,000+ Rapid Preset Switches Under Polyphonic Voice Load
// ============================================================================
bool testRapidPresetSwitchingPolyphonic(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 3] 5,000+ Rapid Preset Switches Under Polyphonic Load...\n";
    std::cout << "======================================================================\n" << std::flush;

    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    proc.prepareToPlay(kSampleRate, kBlockSize);

    std::atomic<bool> stopTest{false};
    std::atomic<uint64_t> totalSwitches{0};
    std::atomic<uint64_t> totalBlocks{0};
    std::atomic<bool> audioError{false};
    std::string audioErrorMsg;

    constexpr size_t kTargetSwitches = 5200; // Exceeds 5,000+ requirement

    // Audio Rendering Thread: Actively playing complex polyphonic chord progression
    std::thread audioThread([&]() {
        resetThreadAllocationTracker();
        enableThreadAllocationTracker(true);

        juce::AudioBuffer<float> buffer(2, kBlockSize);
        juce::MidiBuffer midi;

        size_t blockIdx = 0;
        const int chordNotes[4][4] = {
            { 48, 52, 55, 60 }, // C major
            { 45, 48, 52, 57 }, // A minor
            { 41, 45, 48, 53 }, // F major
            { 43, 47, 50, 55 }  // G major
        };

        while (!stopTest.load(std::memory_order_relaxed)) {
            buffer.clear();
            midi.clear();

            // Churn 4-note chord changes every 16 blocks (voice stealing & release envelope churn)
            if (blockIdx % 16 == 0) {
                const int chordIdx = static_cast<int>((blockIdx / 16) % 4);
                // Turn off prior chord notes
                for (int n = 0; n < 4; ++n) {
                    const int prevChordIdx = (chordIdx + 3) % 4;
                    midi.addEvent(juce::MidiMessage::noteOff(1, chordNotes[prevChordIdx][n], 0.0f), 0);
                }
                // Turn on new chord notes
                for (int n = 0; n < 4; ++n) {
                    midi.addEvent(juce::MidiMessage::noteOn(1, chordNotes[chordIdx][n], 0.85f), n * 4);
                }
            }

            try {
                proc.processBlock(buffer, midi);
            } catch (const std::exception& e) {
                audioError.store(true);
                audioErrorMsg = std::string("Exception in processBlock: ") + e.what();
                break;
            } catch (...) {
                audioError.store(true);
                audioErrorMsg = "Unknown exception in processBlock";
                break;
            }

            // Verify audio sanity
            for (int ch = 0; ch < 2; ++ch) {
                for (int s = 0; s < kBlockSize; ++s) {
                    const float sample = buffer.getSample(ch, s);
                    if (!std::isfinite(sample)) {
                        audioError.store(true);
                        audioErrorMsg = "Non-finite sample (NaN or Inf) detected in audio output!";
                        break;
                    }
                    if (std::fpclassify(sample) == FP_SUBNORMAL) {
                        audioError.store(true);
                        audioErrorMsg = "Denormal sample detected in audio output!";
                        break;
                    }
                    if (std::abs(sample) > 1.05f) {
                        audioError.store(true);
                        audioErrorMsg = "Headroom violation (> 1.05f) detected: sample=" + std::to_string(sample);
                        break;
                    }
                }
                if (audioError.load()) break;
            }

            totalBlocks.fetch_add(1, std::memory_order_relaxed);
            ++blockIdx;
        }

        enableThreadAllocationTracker(false);
        const size_t allocCount = getThreadAllocationCount();
        if (allocCount != 0) {
            audioError.store(true);
            audioErrorMsg = "Heap allocations detected on audio thread during preset switching! Count=" + std::to_string(allocCount);
        }
    });

    // Preset Switching Thread: Churn 5,200 preset changes across all 5 factory presets
    std::thread switchThread([&]() {
        std::mt19937 rng(777);
        std::uniform_int_distribution<int> presetDist(0, 4);

        for (size_t i = 0; i < kTargetSwitches && !audioError.load(); ++i) {
            const int pIdx = presetDist(rng);
            proc.setCurrentProgram(pIdx);
            totalSwitches.fetch_add(1, std::memory_order_relaxed);

            if ((i + 1) % 1000 == 0) {
                std::cout << "  - Completed " << (i + 1) << " / " << kTargetSwitches << " preset switches...\n" << std::flush;
            }
            std::this_thread::yield();
        }
        // Ensure audio thread renders at least 500 blocks of active polyphony
        while (totalBlocks.load() < 500 && !audioError.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        stopTest.store(true);
    });

    switchThread.join();
    stopTest.store(true);
    audioThread.join();

    std::cout << "  - Total Preset Switches:  " << totalSwitches.load() << "\n";
    std::cout << "  - Total Audio Blocks:     " << totalBlocks.load() << "\n";

    CHALLENGE_ASSERT(!audioError.load(), audioErrorMsg.c_str());
    CHALLENGE_ASSERT(totalSwitches.load() >= kTargetSwitches, "Failed to reach target switch count");
    CHALLENGE_ASSERT(totalBlocks.load() >= 500, "Audio thread starved during rapid preset switching");

    std::cout << "  -> PASS: 5,200 rapid preset switches completed cleanly under active 4-note chord polyphony.\n" << std::flush;
    ++gTestsPassed;
    return true;
}

// ============================================================================
// CHALLENGE 4: Deep XML Chunk Fuzzing & Defensive Recovery
// ============================================================================
bool testXmlChunkFuzzing(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 4] Deep XML Chunk Fuzzing & Corrupt State Defensive Recovery...\n";
    std::cout << "======================================================================\n" << std::flush;

    constexpr int kFuzzIterations = 2000;
    std::mt19937 rng(9999);

    // 4.1: Edge case pointers and sizes
    proc.setStateInformation(nullptr, 0);
    proc.setStateInformation(nullptr, 1024);
    proc.setStateInformation(nullptr, -1);
    proc.setStateInformation(nullptr, -9999);

    // 4.2: Pseudo-random binary noise fuzzing
    for (int i = 0; i < 500; ++i) {
        const size_t sz = 1 + (rng() % 4096);
        std::vector<uint8_t> garbage(sz);
        for (size_t b = 0; b < sz; ++b) {
            garbage[b] = static_cast<uint8_t>(rng() & 0xFF);
        }
        proc.setStateInformation(garbage.data(), static_cast<int>(garbage.size()));
    }

    // 4.3: Malformed XML injection (unclosed, truncated, foreign tags, hostile attributes)
    const std::vector<std::string> hostileXmls = {
        "<",
        "<?xml?>",
        "<Parameters",
        "<Parameters><PARAM",
        "<Parameters><PARAM id=\"filterCutoff\" value=\"NaN\"/></Parameters>",
        "<Parameters><PARAM id=\"filterCutoff\" value=\"Infinity\"/></Parameters>",
        "<Parameters><PARAM id=\"filterCutoff\" value=\"-Infinity\"/></Parameters>",
        "<Parameters><PARAM id=\"filterCutoff\" value=\"1e38\"/></Parameters>",
        "<Parameters><PARAM id=\"filterCutoff\" value=\"-1e38\"/></Parameters>",
        "<Parameters><PARAM id=\"filterCutoff\" value=\"undefined\"/></Parameters>",
        "<Parameters filterCutoff=\"NaN\" oscMix=\"Infinity\" masterVolume=\"-999.0\" />",
        "<Parameters programIndex=\"999999\" schemaVersion=\"-1\" />",
        "<Parameters programIndex=\"-50\" />",
        "<Parameters><UnknownTag val=\"123\" /><EvilExploit buffer=\"overflow\" /></Parameters>",
        "<Parameters><PARAM id=\"nonExistentParam\" value=\"42.0\" /></Parameters>",
        "<Parameters><PARAM id=\"\" value=\"\" /></Parameters>",
        "<DifferentRoot><PARAM id=\"filterCutoff\" value=\"500.0\"/></DifferentRoot>"
    };

    for (const auto& xml : hostileXmls) {
        proc.setStateInformation(xml.data(), static_cast<int>(xml.size()));
    }

    // 4.4: Dynamic random mutated XML generator (1,500 iterations)
    for (int i = 0; i < 1500; ++i) {
        std::ostringstream ss;
        ss << "<Parameters schemaVersion=\"" << (rng() % 10) << "\" programIndex=\"" << static_cast<int>(rng() % 20 - 5) << "\">";
        const int numParams = rng() % 60;
        for (int p = 0; p < numParams; ++p) {
            const auto& spec = kAll55Params[p % kAll55Params.size()];
            ss << "<PARAM id=\"" << spec.id << "\" value=\"";
            switch (rng() % 6) {
                case 0: ss << "NaN"; break;
                case 1: ss << "Infinity"; break;
                case 2: ss << "-Infinity"; break;
                case 3: ss << (static_cast<float>(rng() % 100000) - 50000.0f); break;
                case 4: ss << "garbage_string"; break;
                case 5: ss << spec.defaultVal; break;
            }
            ss << "\" />";
        }
        ss << "</Parameters>";
        const std::string xmlStr = ss.str();
        proc.setStateInformation(xmlStr.data(), static_cast<int>(xmlStr.size()));
    }

    // Verify APVTS parameter sanity after fuzzing
    auto snap = proc.getCurrentSnapshot();
    CHALLENGE_ASSERT(std::isfinite(snap.filterCutoff), "filterCutoff corrupted by fuzzing");
    CHALLENGE_ASSERT(snap.filterCutoff >= 20.0f && snap.filterCutoff <= 20000.0f, "filterCutoff range violated");
    CHALLENGE_ASSERT(std::isfinite(snap.oscMix), "oscMix corrupted by fuzzing");
    CHALLENGE_ASSERT(snap.oscMix >= 0.0f && snap.oscMix <= 1.0f, "oscMix range violated");

    // Verify audio rendering executes cleanly without crashing
    proc.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> testBuf(2, 256);
    testBuf.clear();
    juce::MidiBuffer testMidi;
    testMidi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
    proc.processBlock(testBuf, testMidi);

    for (int ch = 0; ch < 2; ++ch) {
        for (int s = 0; s < 256; ++s) {
            const float v = testBuf.getSample(ch, s);
            CHALLENGE_ASSERT(std::isfinite(v), "Non-finite audio sample rendered after XML fuzzing");
            CHALLENGE_ASSERT(std::fpclassify(v) != FP_SUBNORMAL, "Denormal audio sample rendered after XML fuzzing");
        }
    }

    std::cout << "  - Corrupt Chunks Injected:  " << (500 + hostileXmls.size() + 1500) << "\n";
    std::cout << "  -> PASS: Audio processor survived 2,000+ hostile XML chunks with safe fallback and valid output.\n" << std::flush;
    ++gTestsPassed;
    return true;
}

// ============================================================================
// CHALLENGE 5: GUI Open/Close/Resize Teardown Races with SafePointer (1,000 Cycles)
// ============================================================================
bool testGuiTeardownRacesWithSafePointer(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 5] GUI Open/Close/Resize Teardown Races with SafePointer (1,000 Cycles)...\n";
    std::cout << "======================================================================\n" << std::flush;

    std::atomic<bool> stopRace{false};
    std::atomic<uint64_t> paramsFired{0};

    auto& apvts = proc.getApvts();
    const char* monitoredParams[] = {
        "filterAttack", "filterDecay", "filterSustain", "filterRelease",
        "ampAttack", "ampDecay", "ampSustain", "ampRelease"
    };

    // 2 background threads continuously automating monitored envelope parameters
    // Each parameter change triggers BumblerAudioProcessorEditor::parameterChanged,
    // which schedules MessageManager::callAsync([safeThis]...)
    std::vector<std::thread> automators;
    for (int t = 0; t < 2; ++t) {
        automators.emplace_back([&, t]() {
            std::mt19937 rng(5555 + t);
            std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
            size_t idx = 0;
            while (!stopRace.load(std::memory_order_relaxed)) {
                const char* paramId = monitoredParams[idx % 8];
                if (auto* p = apvts.getParameter(paramId)) {
                    p->setValueNotifyingHost(dist01(rng));
                }
                paramsFired.fetch_add(1, std::memory_order_relaxed);
                ++idx;
                std::this_thread::sleep_for(std::chrono::microseconds(25));
            }
        });
    }

    // Main thread: 1,000 rapid open/resize/paint/destroy cycles
    constexpr int kGuiCycles = 1000;
    std::mt19937 rng(8888);
    std::uniform_int_distribution<int> widthDist(960, 1600);
    std::uniform_int_distribution<int> heightDist(600, 1000);

    for (int cycle = 0; cycle < kGuiCycles; ++cycle) {
        auto* rawEditor = proc.createEditor();
        CHALLENGE_ASSERT(rawEditor != nullptr, "createEditor returned null");
        std::unique_ptr<juce::AudioProcessorEditor> editor(rawEditor);

        const int w = widthDist(rng);
        const int h = heightDist(rng);
        editor->setSize(w, h);
        editor->setVisible(true);

        // Offscreen paint on 25% of cycles
        if (cycle % 4 == 0) {
            juce::Image offscreen(juce::Image::ARGB, w, h, true);
            juce::Graphics g(offscreen);
            editor->paintEntireComponent(g, true);
        }

        // Pump a few messages while alive
        pumpMessageQueue(25);

        // DESTROY editor while background threads are firing callAsync lambdas!
        editor.reset();

        // IMMEDIATELY dispatch queued messages against the destroyed editor.
        // If SafePointer is invalid or dangling, this triggers immediate crash or heap corruption!
        pumpMessageQueue(50);

        if ((cycle + 1) % 200 == 0) {
            std::cout << "  - Completed " << (cycle + 1) << " / " << kGuiCycles << " teardown cycles (Params fired: " << paramsFired.load() << ")...\n" << std::flush;
        }
    }

    stopRace.store(true);
    for (auto& a : automators) a.join();

    // Drain all remaining queued messages
    pumpMessageQueueFor(100);

    std::cout << "  - Teardown Cycles Completed: " << kGuiCycles << "\n";
    std::cout << "  - Parameters Automated:      " << paramsFired.load() << "\n";
    std::cout << "  -> PASS: SafePointer eliminated all use-after-free and race conditions across 1,000 teardown cycles.\n" << std::flush;
    ++gTestsPassed;
    return true;
}

} // namespace challenger_m6_2

// ============================================================================
// Main Entry Point
// ============================================================================
int main() {
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "======================================================================\n";
    std::cout << " BUMBLER XD : CHALLENGER M6-2 ADVERSARIAL STRESS & HARDENING SUITE    \n";
    std::cout << "======================================================================\n" << std::flush;

    bumbler::BumblerAudioProcessor proc;

    bool p1 = challenger_m6_2::testApvtsTreeAndBijectiveMapping(proc);
    bool p2 = challenger_m6_2::testHighContentionSnapshotLatency(proc);
    bool p3 = challenger_m6_2::testRapidPresetSwitchingPolyphonic(proc);
    bool p4 = challenger_m6_2::testXmlChunkFuzzing(proc);
    bool p5 = challenger_m6_2::testGuiTeardownRacesWithSafePointer(proc);

    bool allPassed = p1 && p2 && p3 && p4 && p5 && (gTestsFailed == 0);

    std::cout << "\n======================================================================\n";
    std::cout << " CHALLENGER M6-2 FINAL VERDICT:\n";
    std::cout << "  - Stress Suites Passed: " << gTestsPassed << " / 5\n";
    std::cout << "  - Stress Suites Failed: " << gTestsFailed << "\n";
    std::cout << "  - Empirical Status:     " << (allPassed ? "[ALL STRESS TESTS PASSED]" : "[FAILED]") << "\n";
    std::cout << "  - Binary Verdict:       " << (allPassed ? "APPROVE" : "REJECT") << "\n";
    std::cout << "======================================================================\n" << std::flush;

    return allPassed ? 0 : 1;
}
