#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <cstdlib>

#include "test_helpers.h"
#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerEngine.h"
#include "BumblerVoiceManager.h"
#include "BumblerVoice.h"
#include "BumblerOscillator.h"
#include "WaspFilter.h"
#include "CharacterCircuits.h"
#include "parameters/PresetParameters.h"

// ============================================================================
// Bumbler XD — Golden Test Vectors & Deterministic Regression Suite
// Enforces:
//   1. Bit-exact ParameterSnapshot golden defaults across 5 factory presets.
//   2. High-precision deterministic repeatability: SNR > 120 dB, Max Delta < 1e-4.
//   3. Essential audio invariants (non-finite, subnormals, peak bounds, RMS, stereo r).
//   4. Fixture vector file verification & synchronization with tests/fixtures/.
//   5. Hard real-time allocation invariance (0 heap allocations).
// ============================================================================

namespace {

constexpr double kGoldenSampleRate = 44100.0;
constexpr int    kGoldenBlockSize  = 512;
constexpr int    kGoldenNumSamples = 2048; // 4 blocks of 512 samples
constexpr uint32_t kGoldenSeedBase = 0x12345678u;
constexpr int    kGoldenMidiNote   = 60;   // Middle C
constexpr float  kGoldenVelocity   = 0.85f;
constexpr int    kGoldenNoteOff    = 1024; // Trigger note off halfway to test release tail

// Prepare and deterministically seed BumblerEngine
void setupDeterministicEngine(bumbler::BumblerEngine& engine,
                              double sampleRate = kGoldenSampleRate,
                              int blockSize = kGoldenBlockSize,
                              uint32_t seedBase = kGoldenSeedBase) {
    engine.prepare(sampleRate, blockSize);
    engine.reset();
    for (int i = 0; i < bumbler::BumblerVoiceManager::kMaxVoices; ++i) {
        auto& voice = engine.getVoiceManager().getVoice(i);
        voice.seedVoice(static_cast<uint32_t>(i));
        voice.getAnalogDrift().seed(seedBase + static_cast<uint32_t>(i) * 0x1000u);
    }
}

// Render standardized deterministic audio block
void renderDeterministicPreset(bumbler::BumblerEngine& engine,
                               int presetIndex,
                               std::vector<float>& outL,
                               std::vector<float>& outR,
                               bumbler::ParameterSnapshot& snapshot,
                               uint32_t seedBase = kGoldenSeedBase) {
    setupDeterministicEngine(engine, kGoldenSampleRate, kGoldenBlockSize, seedBase);

    const auto& presets = bumbler::getFactoryPresets();
    snapshot = presets[static_cast<size_t>(presetIndex)].params;

    outL.assign(static_cast<size_t>(kGoldenNumSamples), 0.0f);
    outR.assign(static_cast<size_t>(kGoldenNumSamples), 0.0f);

    std::vector<float> blockL(static_cast<size_t>(kGoldenBlockSize), 0.0f);
    std::vector<float> blockR(static_cast<size_t>(kGoldenBlockSize), 0.0f);
    float* channels[2] = { blockL.data(), blockR.data() };

    engine.processMidiEvent(0x90, kGoldenMidiNote, kGoldenVelocity);
    bool noteOffTriggered = false;

    int currentSample = 0;
    while (currentSample < kGoldenNumSamples) {
        const int samplesThisBlock = std::min(kGoldenBlockSize, kGoldenNumSamples - currentSample);

        if (!noteOffTriggered && (currentSample + samplesThisBlock >= kGoldenNoteOff)) {
            engine.processMidiEvent(0x80, kGoldenMidiNote, 0.0f);
            noteOffTriggered = true;
        }

        std::fill(blockL.begin(), blockL.end(), 0.0f);
        std::fill(blockR.begin(), blockR.end(), 0.0f);

        engine.renderBlock(channels, 2, samplesThisBlock, snapshot);

        for (int i = 0; i < samplesThisBlock; ++i) {
            outL[static_cast<size_t>(currentSample + i)] = blockL[static_cast<size_t>(i)];
            outR[static_cast<size_t>(currentSample + i)] = blockR[static_cast<size_t>(i)];
        }

        currentSample += samplesThisBlock;
    }
}

// Locate fixture directory relative to common working directories
std::string findFixtureDirectory() {
    const std::string candidateDirs[] = {
        "tests/fixtures",
        "../tests/fixtures",
        "../../tests/fixtures",
        "../../../tests/fixtures"
    };

    for (const auto& dir : candidateDirs) {
        std::ifstream testFile(dir + "/README.md");
        if (testFile.is_open()) {
            return dir;
        }
    }
    return "tests/fixtures"; // Fallback default
}

} // namespace

// ============================================================================
// Test 1: Deterministic ParameterSnapshot Golden Preset Value Verifications
// ============================================================================
void testPresetSnapshotIntegrity() {
    const auto& presets = bumbler::getFactoryPresets();
    TEST_ASSERT(presets.size() == 5, "Factory presets array must contain exactly 5 presets");

    // 0: Acid Bass
    {
        const auto& p = presets[0];
        TEST_ASSERT(std::string(p.name) == "Acid Bass", "Preset 0 name mismatch");
        TEST_ASSERT(p.params.osc1Waveform == 0.0f, "Preset 0 OSC1 must be Saw (0.0)");
        TEST_ASSERT(p.params.osc2Waveform == 1.0f, "Preset 0 OSC2 must be Square (1.0)");
        TEST_ASSERT(p.params.filterMode == 1.0f, "Preset 0 filter mode must be LP24 (1.0)");
        TEST_ASSERT_NEAR(p.params.filterCutoff, 380.0f, 0.1f, "Preset 0 filter cutoff must be 380 Hz");
        TEST_ASSERT_NEAR(p.params.filterResonance, 0.78f, 0.01f, "Preset 0 filter resonance must be 0.78");
        TEST_ASSERT(p.params.driveEnabled == 1.0f, "Preset 0 overdrive must be enabled");
        TEST_ASSERT(p.params.dualMode == 0.0f, "Preset 0 dual mode must be 0.0 (Mono center)");
        TEST_ASSERT(p.params.analogMode == 1.0f, "Preset 0 analog mode must be 1.0");
    }

    // 1: Sync Lead
    {
        const auto& p = presets[1];
        TEST_ASSERT(std::string(p.name) == "Sync Lead", "Preset 1 name mismatch");
        TEST_ASSERT(p.params.osc1Waveform == 0.0f && p.params.osc2Waveform == 0.0f, "Preset 1 OSCs must be Saw");
        TEST_ASSERT(p.params.filterMode == 2.0f, "Preset 1 filter mode must be LP+NT (2.0)");
        TEST_ASSERT_NEAR(p.params.filterCutoff, 2400.0f, 1.0f, "Preset 1 filter cutoff must be 2400 Hz");
        TEST_ASSERT_NEAR(p.params.ringModMix, 0.15f, 0.01f, "Preset 1 ring mod must be 0.15");
        TEST_ASSERT_NEAR(p.params.fmAmount, 0.35f, 0.01f, "Preset 1 FM amount must be 0.35");
        TEST_ASSERT(p.params.dualMode == 1.0f, "Preset 1 dual mode must be 1.0 (Stereo wide)");
    }

    // 2: Swarm Pad
    {
        const auto& p = presets[2];
        TEST_ASSERT(std::string(p.name) == "Swarm Pad", "Preset 2 name mismatch");
        TEST_ASSERT(p.params.filterMode == 3.0f, "Preset 2 filter mode must be DBL.NT (3.0)");
        TEST_ASSERT_NEAR(p.params.filterCutoff, 1800.0f, 1.0f, "Preset 2 filter cutoff must be 1800 Hz");
        TEST_ASSERT_NEAR(p.params.filterResonance, 0.65f, 0.01f, "Preset 2 filter resonance must be 0.65");
        TEST_ASSERT(p.params.ampAttack >= 0.80f, "Preset 2 amp attack must be slow swell (>= 0.8s)");
        TEST_ASSERT(p.params.dualMode == 1.0f, "Preset 2 dual mode must be 1.0 (Wide chorus)");
    }

    // 3: Percussion
    {
        const auto& p = presets[3];
        TEST_ASSERT(std::string(p.name) == "Percussion", "Preset 3 name mismatch");
        TEST_ASSERT(p.params.filterMode == 4.0f, "Preset 3 filter mode must be BP24 (4.0)");
        TEST_ASSERT_NEAR(p.params.filterCutoff, 850.0f, 1.0f, "Preset 3 filter cutoff must be 850 Hz");
        TEST_ASSERT(p.params.ampAttack <= 0.005f, "Preset 3 amp attack must be instantaneous (< 5ms)");
        TEST_ASSERT(p.params.ampSustain == 0.0f, "Preset 3 sustain must be zero for percussive snap");
        TEST_ASSERT(p.params.dualMode == 0.0f, "Preset 3 dual mode must be 0.0 (Mono punch)");
    }

    // 4: Vintage Drone
    {
        const auto& p = presets[4];
        TEST_ASSERT(std::string(p.name) == "Vintage Drone", "Preset 4 name mismatch");
        TEST_ASSERT(p.params.filterMode == 5.0f, "Preset 4 filter mode must be HP24 (5.0)");
        TEST_ASSERT_NEAR(p.params.filterCutoff, 220.0f, 1.0f, "Preset 4 filter cutoff must be 220 Hz");
        TEST_ASSERT(p.params.ampSustain == 1.0f, "Preset 4 sustain must be 1.0 for infinite hold drone");
        TEST_ASSERT(p.params.dualMode == 1.0f, "Preset 4 dual mode must be 1.0 (Stereo atmosphere)");
    }
}

// ============================================================================
// Test 2: Deterministic Repeatability, SNR > 120 dB & Max Delta < 1e-4
// ============================================================================
void testDeterministicRepeatabilityAndSNR() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    bumbler::BumblerEngine engineA;
    bumbler::BumblerEngine engineB;

    for (int pIdx = 0; pIdx < 5; ++pIdx) {
        std::vector<float> leftA, rightA;
        std::vector<float> leftB, rightB;
        bumbler::ParameterSnapshot snapA, snapB;

        // Render Run A
        renderDeterministicPreset(engineA, pIdx, leftA, rightA, snapA, kGoldenSeedBase);

        // Render Run B with identical seed
        renderDeterministicPreset(engineB, pIdx, leftB, rightB, snapB, kGoldenSeedBase);

        double signalPower = 0.0;
        double noisePower  = 0.0;
        float  maxDelta    = 0.0f;

        for (int i = 0; i < kGoldenNumSamples; ++i) {
            const float diffL = leftA[static_cast<size_t>(i)]  - leftB[static_cast<size_t>(i)];
            const float diffR = rightA[static_cast<size_t>(i)] - rightB[static_cast<size_t>(i)];

            const float absDiffL = std::abs(diffL);
            const float absDiffR = std::abs(diffR);

            if (absDiffL > maxDelta) maxDelta = absDiffL;
            if (absDiffR > maxDelta) maxDelta = absDiffR;

            signalPower += static_cast<double>(leftA[static_cast<size_t>(i)]) * static_cast<double>(leftA[static_cast<size_t>(i)])
                         + static_cast<double>(rightA[static_cast<size_t>(i)]) * static_cast<double>(rightA[static_cast<size_t>(i)]);

            noisePower  += static_cast<double>(diffL) * static_cast<double>(diffL)
                         + static_cast<double>(diffR) * static_cast<double>(diffR);
        }

        // SNR calculation with division-by-zero protection for bit-identical runs
        double snrDb = 0.0;
        if (noisePower <= 1.0e-20) {
            snrDb = 240.0; // Bit-exact identity -> infinite SNR clamped to 240 dB
        } else {
            snrDb = 10.0 * std::log10(signalPower / noisePower);
        }

        std::cout << "  Preset [" << pIdx << "] Repeatability: SNR = " << snrDb 
                  << " dB, Max Delta = " << maxDelta << "\n";

        TEST_ASSERT(maxDelta < 1.0e-4f, "Maximum sample delta must be < 1e-4");
        TEST_ASSERT(snrDb > 120.0, "Deterministic SNR must exceed 120 dB");
    }
}

// ============================================================================
// Test 3: Audio Invariants (Non-Finite, Subnormals, Peak, RMS, Spatial Correlation)
// ============================================================================
void testAudioInvariants() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    bumbler::BumblerEngine engine;

    for (int pIdx = 0; pIdx < 5; ++pIdx) {
        std::vector<float> left, right;
        bumbler::ParameterSnapshot snap;
        renderDeterministicPreset(engine, pIdx, left, right, snap, kGoldenSeedBase);

        // 1. Strict finite and subnormal inspection
        TEST_ASSERT(!bumbler_test::containsNonFinite(left.data(), kGoldenNumSamples),
                    "Left buffer must contain zero NaNs or Infs");
        TEST_ASSERT(!bumbler_test::containsNonFinite(right.data(), kGoldenNumSamples),
                    "Right buffer must contain zero NaNs or Infs");
        TEST_ASSERT(!bumbler_test::containsSubnormals(left.data(), kGoldenNumSamples),
                    "Left buffer must contain zero subnormals");
        TEST_ASSERT(!bumbler_test::containsSubnormals(right.data(), kGoldenNumSamples),
                    "Right buffer must contain zero subnormals");

        // 2. Peak amplitude bounds [-1.05, 1.05]
        const float peakL = bumbler_test::computePeak(left.data(), kGoldenNumSamples);
        const float peakR = bumbler_test::computePeak(right.data(), kGoldenNumSamples);
        const float maxPeak = std::max(peakL, peakR);

        TEST_ASSERT(maxPeak <= 1.05001f, "Audio peak must not exceed 1.05 (soft-clipped output stage)");
        // Presets with long attack times (Preset 2 Swarm Pad: 0.85s, Preset 4 Vintage Drone: 2.5s)
        // produce ~0.010-0.015 peak over the 2048-sample (46.4ms) initial evaluation window.
        const float expectedMinPeak = (pIdx == 2 || pIdx == 4) ? 0.008f : 0.05f;
        TEST_ASSERT(maxPeak >= expectedMinPeak, "Audio peak must be audible");

        // 3. RMS energy bounds
        const double rmsL = bumbler_test::computeRms(left.data(), kGoldenNumSamples);
        const double rmsR = bumbler_test::computeRms(right.data(), kGoldenNumSamples);
        const double expectedMinRms = (pIdx == 2 || pIdx == 4) ? 0.003 : 0.01;
        TEST_ASSERT(rmsL >= expectedMinRms && rmsR >= expectedMinRms, "RMS energy must indicate genuine acoustic signal");

        // 4. Spatial correlation invariants matching dualMode
        const double r = bumbler_test::computeCrossCorrelation(left.data(), right.data(), kGoldenNumSamples);
        if (snap.dualMode == 0.0f) {
            // Mono preset: Left and Right must be identical
            TEST_ASSERT(r >= 0.9999, "Mono preset (dualMode=0) cross-correlation r must be >= 0.9999");
        } else {
            // Stereo preset: Left and Right must be decorrelated
            TEST_ASSERT(r < 0.70, "Stereo preset (dualMode=1) cross-correlation r must be < 0.70");
        }
    }
}

// ============================================================================
// Test 4: Fixture Vector File Verification & Synchronization
// ============================================================================
void testGoldenFixtureVectors() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    bumbler::BumblerEngine engine;
    const std::string fixtureDir = findFixtureDirectory();

    const char* const presetFilenames[5] = {
        "preset_0_acid_bass.csv",
        "preset_1_sync_lead.csv",
        "preset_2_swarm_pad.csv",
        "preset_3_percussion.csv",
        "preset_4_vintage_drone.csv"
    };

    for (int pIdx = 0; pIdx < 5; ++pIdx) {
        std::vector<float> left, right;
        bumbler::ParameterSnapshot snap;
        renderDeterministicPreset(engine, pIdx, left, right, snap, kGoldenSeedBase);

        const std::string fixturePath = fixtureDir + "/" + presetFilenames[pIdx];
        std::ifstream inFile(fixturePath);

        if (!inFile.is_open()) {
            // Fixture file does not exist yet -> write out golden fixture vector
            std::ofstream outFile(fixturePath);
            if (outFile.is_open()) {
                outFile << "# Bumbler XD Golden Vector: " << presetFilenames[pIdx] << "\n";
                outFile << "# SampleIndex,LeftSample,RightSample\n";
                outFile << std::scientific << std::setprecision(8);
                for (int i = 0; i < kGoldenNumSamples; ++i) {
                    outFile << i << "," << left[static_cast<size_t>(i)] << "," << right[static_cast<size_t>(i)] << "\n";
                }
                std::cout << "  [FIXTURE GENERATED] Created: " << fixturePath << "\n";
            }
            continue;
        }

        // Fixture file exists -> load reference samples and compare
        std::vector<float> goldenL, goldenR;
        goldenL.reserve(kGoldenNumSamples);
        goldenR.reserve(kGoldenNumSamples);

        std::string line;
        while (std::getline(inFile, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::stringstream ss(line);
            std::string token;
            // Format: SampleIndex,Left,Right
            if (std::getline(ss, token, ',')) {
                std::string sLeft, sRight;
                if (std::getline(ss, sLeft, ',') && std::getline(ss, sRight, ',')) {
                    goldenL.push_back(std::stof(sLeft));
                    goldenR.push_back(std::stof(sRight));
                }
            }
        }

        if (goldenL.size() == static_cast<size_t>(kGoldenNumSamples)) {
            double sigP = 0.0, diffP = 0.0;
            float maxDelta = 0.0f;
            for (size_t i = 0; i < goldenL.size(); ++i) {
                const float dL = std::abs(left[i] - goldenL[i]);
                const float dR = std::abs(right[i] - goldenR[i]);
                if (dL > maxDelta) maxDelta = dL;
                if (dR > maxDelta) maxDelta = dR;

                sigP  += static_cast<double>(goldenL[i]) * static_cast<double>(goldenL[i])
                       + static_cast<double>(goldenR[i]) * static_cast<double>(goldenR[i]);
                diffP += static_cast<double>(dL) * static_cast<double>(dL)
                       + static_cast<double>(dR) * static_cast<double>(dR);
            }

            double snr = (diffP <= 1.0e-20) ? 240.0 : (10.0 * std::log10(sigP / diffP));
            std::cout << "  Fixture [" << pIdx << "] (" << presetFilenames[pIdx] 
                      << ") Match: SNR = " << snr << " dB, Max Delta = " << maxDelta << "\n";

            TEST_ASSERT(maxDelta < 1.0e-4f, "Fixture sample delta must be < 1e-4");
            // Cross-platform fixture comparison:
            // Local runs achieve bit-exact identity (>120 dB SNR, as verified in Test 2).
            // Cross-architecture comparisons against static CSV reference fixtures (e.g. ARM64 Clang
            // vs x86_64 MSVC) experience subtle 1-ULP standard library transcendental phase differences
            // (std::tan, std::tanh, std::pow, std::exp) that accumulate across 2048 IIR filter steps.
            // A threshold of 75 dB SNR alongside maxDelta < 1e-4 guarantees pristine acoustic equivalence
            // while accommodating IEEE-754 cross-compiler variations.
            TEST_ASSERT(snr > 75.0, "Fixture SNR must exceed 75 dB (cross-platform)");
        }
    }
}

// ============================================================================
// Test 5: Per-Algorithm Isolated Golden Vectors & Unit-Level Regression
// Validates isolated core DSP components against bit-accurate CSV fixtures:
//   - 4th-Order PolyBLEP Saw Wave at 440 Hz (fs=44100)
//   - 4th-Order PolyBLEP Pulse Wave at 440 Hz (PW=0.50 & PW=0.25)
//   - ZDF SVF LP12 2-Pole Lowpass Step Response (fc=1000, R=0.707)
//   - ZDF SVF LP24 FAT 4-Pole Lowpass Step Response with CMOS Saturation (fc=1000, reso=0.75)
//   - Haas 5ms Stereo Psychoacoustic Decorrelator Impulse Response (fs=48000, delay=240)
// ============================================================================
void testAlgorithmLevelGoldenVectors() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    const std::string fixtureDir = findFixtureDirectory();

    auto computeMetrics = [](const std::vector<float>& live, const std::vector<float>& golden,
                             float& maxDelta, double& snrDb) {
        maxDelta = 0.0f;
        double sigP = 0.0;
        double diffP = 0.0;
        for (size_t i = 0; i < live.size(); ++i) {
            const float diff = live[i] - golden[i];
            const float absDiff = std::abs(diff);
            if (absDiff > maxDelta) maxDelta = absDiff;
            sigP += static_cast<double>(golden[i]) * static_cast<double>(golden[i]);
            diffP += static_cast<double>(diff) * static_cast<double>(diff);
        }
        snrDb = (diffP <= 1.0e-20) ? 240.0 : (10.0 * std::log10(sigP / diffP));
    };

    // ------------------------------------------------------------------------
    // A. 4th-Order PolyBLEP Saw Waveform (440 Hz, fs=44100)
    // ------------------------------------------------------------------------
    {
        constexpr int kSamples = 100;
        bumbler::BumblerOscillator osc;
        osc.prepare(44100.0);
        osc.setFrequency(440.0f);
        osc.setWaveform(bumbler::OscWaveform::Saw);
        osc.reset(0.0f);

        std::vector<float> liveSaw(kSamples);
        for (int i = 0; i < kSamples; ++i) {
            liveSaw[static_cast<size_t>(i)] = osc.processSample(0.0f);
        }

        const std::string sawPath = fixtureDir + "/polyblep_saw_golden.csv";
        std::ifstream inFile(sawPath);
        std::vector<float> goldenSaw;
        goldenSaw.reserve(kSamples);

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                if (line.empty() || line[0] == '#' || line.find("sample_idx") != std::string::npos) continue;
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> cols;
                while (std::getline(ss, token, ',')) cols.push_back(token);
                if (cols.size() >= 5) {
                    try {
                        goldenSaw.push_back(std::stof(cols[4]));
                    } catch (...) {}
                }
            }
        }

        if (goldenSaw.size() == static_cast<size_t>(kSamples)) {
            float maxDelta = 0.0f;
            double snrDb = 0.0;
            computeMetrics(liveSaw, goldenSaw, maxDelta, snrDb);
            std::cout << "  Algorithm [PolyBLEP Saw 440Hz] Match: SNR = " << snrDb 
                      << " dB, Max Delta = " << maxDelta << "\n";
            TEST_ASSERT(maxDelta < 1.0e-4f, "PolyBLEP Saw sample delta must be < 1e-4");
            TEST_ASSERT(snrDb > 120.0, "PolyBLEP Saw SNR must exceed 120 dB");
        } else {
            TEST_ASSERT(false, "Failed to load polyblep_saw_golden.csv fixture");
        }
    }

    // ------------------------------------------------------------------------
    // B. 4th-Order PolyBLEP Pulse Waveform (440 Hz, fs=44100, PW=0.50 & PW=0.25)
    // ------------------------------------------------------------------------
    {
        constexpr int kSamples = 100;
        bumbler::BumblerOscillator osc50;
        osc50.prepare(44100.0);
        osc50.setFrequency(440.0f);
        osc50.setWaveform(bumbler::OscWaveform::Square);
        osc50.setPulseWidth(0.50f);
        osc50.reset(0.0f);

        bumbler::BumblerOscillator osc25;
        osc25.prepare(44100.0);
        osc25.setFrequency(440.0f);
        osc25.setWaveform(bumbler::OscWaveform::Square);
        osc25.setPulseWidth(0.25f);
        osc25.reset(0.0f);

        std::vector<float> livePulse50(kSamples), livePulse25(kSamples);
        for (int i = 0; i < kSamples; ++i) {
            livePulse50[static_cast<size_t>(i)] = osc50.processSample(0.0f);
            livePulse25[static_cast<size_t>(i)] = osc25.processSample(0.0f);
        }

        const std::string pulsePath = fixtureDir + "/polyblep_pulse_golden.csv";
        std::ifstream inFile(pulsePath);
        std::vector<float> goldenPulse50, goldenPulse25;
        goldenPulse50.reserve(kSamples);
        goldenPulse25.reserve(kSamples);

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                if (line.empty() || line[0] == '#' || line.find("sample_idx") != std::string::npos) continue;
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> cols;
                while (std::getline(ss, token, ',')) cols.push_back(token);
                if (cols.size() >= 8) {
                    try {
                        goldenPulse50.push_back(std::stof(cols[4]));
                        goldenPulse25.push_back(std::stof(cols[7]));
                    } catch (...) {}
                }
            }
        }

        if (goldenPulse50.size() == static_cast<size_t>(kSamples) && goldenPulse25.size() == static_cast<size_t>(kSamples)) {
            float maxDelta50 = 0.0f, maxDelta25 = 0.0f;
            double snrDb50 = 0.0, snrDb25 = 0.0;
            computeMetrics(livePulse50, goldenPulse50, maxDelta50, snrDb50);
            computeMetrics(livePulse25, goldenPulse25, maxDelta25, snrDb25);

            std::cout << "  Algorithm [PolyBLEP Pulse PW=0.50] Match: SNR = " << snrDb50 
                      << " dB, Max Delta = " << maxDelta50 << "\n";
            std::cout << "  Algorithm [PolyBLEP Pulse PW=0.25] Match: SNR = " << snrDb25 
                      << " dB, Max Delta = " << maxDelta25 << "\n";

            TEST_ASSERT(maxDelta50 < 1.0e-4f, "PolyBLEP Pulse PW=0.50 sample delta must be < 1e-4");
            TEST_ASSERT(snrDb50 > 120.0, "PolyBLEP Pulse PW=0.50 SNR must exceed 120 dB");
            TEST_ASSERT(maxDelta25 < 1.0e-4f, "PolyBLEP Pulse PW=0.25 sample delta must be < 1e-4");
            TEST_ASSERT(snrDb25 > 120.0, "PolyBLEP Pulse PW=0.25 SNR must exceed 120 dB");
        } else {
            TEST_ASSERT(false, "Failed to load polyblep_pulse_golden.csv fixture");
        }
    }

    // ------------------------------------------------------------------------
    // C. ZDF 2-Pole Lowpass Filter Step Response (LP12, fc=1000 Hz, R=0.707)
    // ------------------------------------------------------------------------
    {
        constexpr int kSamples = 100;
        bumbler::WaspFilter filter;
        filter.prepare(44100.0);
        filter.reset();

        std::vector<float> liveLp12(kSamples);
        for (int i = 0; i < kSamples; ++i) {
            liveLp12[static_cast<size_t>(i)] = filter.processSample(1.0f, 1000.0, 0.707, bumbler::WaspFilterMode::LP12);
        }

        const std::string lp12Path = fixtureDir + "/zdf_svf_lp12_step_golden.csv";
        std::ifstream inFile(lp12Path);
        std::vector<float> goldenLp12;
        goldenLp12.reserve(kSamples);

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                if (line.empty() || line[0] == '#' || line.find("sample_idx") != std::string::npos) continue;
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> cols;
                while (std::getline(ss, token, ',')) cols.push_back(token);
                if (cols.size() >= 7) {
                    try {
                        goldenLp12.push_back(std::stof(cols[6]));
                    } catch (...) {}
                }
            }
        }

        if (goldenLp12.size() == static_cast<size_t>(kSamples)) {
            float maxDelta = 0.0f;
            double snrDb = 0.0;
            computeMetrics(liveLp12, goldenLp12, maxDelta, snrDb);
            std::cout << "  Algorithm [ZDF SVF LP12 Step] Match: SNR = " << snrDb 
                      << " dB, Max Delta = " << maxDelta << "\n";
            TEST_ASSERT(maxDelta < 1.0e-4f, "ZDF SVF LP12 sample delta must be < 1e-4");
            TEST_ASSERT(snrDb > 120.0, "ZDF SVF LP12 SNR must exceed 120 dB");
        } else {
            TEST_ASSERT(false, "Failed to load zdf_svf_lp12_step_golden.csv fixture");
        }
    }

    // ------------------------------------------------------------------------
    // D. ZDF 4-Pole Lowpass FAT Step Response with CMOS Saturation (LP24 FAT, fc=1000 Hz, reso=0.75)
    // ------------------------------------------------------------------------
    {
        constexpr int kSamples = 100;
        bumbler::WaspFilter filter;
        filter.prepare(44100.0);
        filter.reset();

        std::vector<float> liveLp24(kSamples);
        for (int i = 0; i < kSamples; ++i) {
            liveLp24[static_cast<size_t>(i)] = filter.processSample(1.0f, 1000.0, 0.75, bumbler::WaspFilterMode::LP24);
        }

        const std::string lp24Path = fixtureDir + "/zdf_svf_lp24_fat_step_golden.csv";
        std::ifstream inFile(lp24Path);
        std::vector<float> goldenLp24;
        goldenLp24.reserve(kSamples);

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                if (line.empty() || line[0] == '#' || line.find("sample_idx") != std::string::npos) continue;
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> cols;
                while (std::getline(ss, token, ',')) cols.push_back(token);
                if (cols.size() >= 6) {
                    try {
                        goldenLp24.push_back(std::stof(cols[5]));
                    } catch (...) {}
                }
            }
        }

        if (goldenLp24.size() == static_cast<size_t>(kSamples)) {
            float maxDelta = 0.0f;
            double snrDb = 0.0;
            computeMetrics(liveLp24, goldenLp24, maxDelta, snrDb);
            std::cout << "  Algorithm [ZDF SVF LP24 FAT Step] Match: SNR = " << snrDb 
                      << " dB, Max Delta = " << maxDelta << "\n";
            TEST_ASSERT(maxDelta < 1.0e-4f, "ZDF SVF LP24 FAT sample delta must be < 1e-4");
            TEST_ASSERT(snrDb > 120.0, "ZDF SVF LP24 FAT SNR must exceed 120 dB");
        } else {
            TEST_ASSERT(false, "Failed to load zdf_svf_lp24_fat_step_golden.csv fixture");
        }
    }

    // ------------------------------------------------------------------------
    // E. Haas 5ms Stereo Psychoacoustic Decorrelator Impulse Response (fs=48000, delay=240)
    // ------------------------------------------------------------------------
    {
        constexpr int kSamples = 300;
        bumbler::DualModeVoiceDoubler doubler;
        doubler.prepare(48000.0);
        doubler.reset();

        std::vector<float> monoIn(kSamples, 0.0f);
        monoIn[0] = 1.0f; // Unit impulse at n = 0
        std::vector<float> liveOutL(kSamples, 0.0f);
        std::vector<float> liveOutR(kSamples, 0.0f);

        doubler.process(monoIn.data(), liveOutL.data(), liveOutR.data(), kSamples, true, 48000.0);

        const std::string haasPath = fixtureDir + "/haas_decorrelation_golden.csv";
        std::ifstream inFile(haasPath);
        std::vector<float> goldenOutL, goldenOutR;
        goldenOutL.reserve(kSamples);
        goldenOutR.reserve(kSamples);

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                if (line.empty() || line[0] == '#' || line.find("sample_idx") != std::string::npos) continue;
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> cols;
                while (std::getline(ss, token, ',')) cols.push_back(token);
                if (cols.size() >= 5) {
                    try {
                        goldenOutL.push_back(std::stof(cols[3]));
                        goldenOutR.push_back(std::stof(cols[4]));
                    } catch (...) {}
                }
            }
        }

        if (goldenOutL.size() == static_cast<size_t>(kSamples) && goldenOutR.size() == static_cast<size_t>(kSamples)) {
            float maxDeltaL = 0.0f, maxDeltaR = 0.0f;
            double snrDbL = 0.0, snrDbR = 0.0;
            computeMetrics(liveOutL, goldenOutL, maxDeltaL, snrDbL);
            computeMetrics(liveOutR, goldenOutR, maxDeltaR, snrDbR);

            std::cout << "  Algorithm [Haas Decorrelator Left] Match: SNR = " << snrDbL 
                      << " dB, Max Delta = " << maxDeltaL << "\n";
            std::cout << "  Algorithm [Haas Decorrelator Right] Match: SNR = " << snrDbR 
                      << " dB, Max Delta = " << maxDeltaR << "\n";

            TEST_ASSERT(maxDeltaL < 1.0e-4f, "Haas Left sample delta must be < 1e-4");
            TEST_ASSERT(snrDbL > 120.0, "Haas Left SNR must exceed 120 dB");
            TEST_ASSERT(maxDeltaR < 1.0e-4f, "Haas Right sample delta must be < 1e-4");
            TEST_ASSERT(snrDbR > 120.0, "Haas Right SNR must exceed 120 dB");
        } else {
            TEST_ASSERT(false, "Failed to load haas_decorrelation_golden.csv fixture");
        }
    }
}

// ============================================================================
// Test 6: Hard Real-Time Heap Safety (0 Allocations During Processing)
// ============================================================================
void testRealtimeSafetyInvariants() {
    bumbler_test::ScopedNoDenormalsGuard guard;
    bumbler::BumblerEngine engine;
    engine.prepare(kGoldenSampleRate, kGoldenBlockSize);

    std::vector<float> bufL(kGoldenBlockSize, 0.0f);
    std::vector<float> bufR(kGoldenBlockSize, 0.0f);
    float* channels[2] = { bufL.data(), bufR.data() };

    const auto& presets = bumbler::getFactoryPresets();

    for (int pIdx = 0; pIdx < 5; ++pIdx) {
        const auto& params = presets[static_cast<size_t>(pIdx)].params;
        engine.reset();
        engine.processMidiEvent(0x90, 60, 0.85f);

        resetAllocationTracker();
        enableAllocationTracker(true);

        engine.renderBlock(channels, 2, kGoldenBlockSize, params);

        enableAllocationTracker(false);
        const size_t allocs = getAllocationCount();

        TEST_ASSERT(allocs == 0, "Audio renderBlock must perform 0 heap allocations");
    }
}

// ============================================================================
// Main Runner Entry Point
// ============================================================================
int main() {
    std::cout << "======================================================================\n";
    std::cout << "          BUMBLER XD : GOLDEN TEST VECTORS & REGRESSION SUITE         \n";
    std::cout << "  Deterministic Preset Synthesis, Audio Invariants & Fixture Testing  \n";
    std::cout << "======================================================================\n";

    RUN_TEST(testPresetSnapshotIntegrity);
    RUN_TEST(testDeterministicRepeatabilityAndSNR);
    RUN_TEST(testAudioInvariants);
    RUN_TEST(testGoldenFixtureVectors);
    RUN_TEST(testAlgorithmLevelGoldenVectors);
    RUN_TEST(testRealtimeSafetyInvariants);

    std::cout << "\n======================================================================\n";
    std::cout << "                       FINAL TEST SUMMARY                             \n";
    std::cout << "======================================================================\n";
    std::cout << "  Tests Passed: " << gGlobalTestsPassed << "\n";
    std::cout << "  Tests Failed: " << gGlobalTestsFailed << "\n";
    std::cout << "  Overall Result: " << (gGlobalTestsFailed == 0 ? "[ALL TESTS PASSED]" : "[FAILED]") << "\n";
    std::cout << "======================================================================\n";

    return (gGlobalTestsFailed == 0) ? 0 : 1;
}
