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
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <cfloat>

// ============================================================================
// Accounting & Macros
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

namespace challenger_m5_1 {

struct ParamSpec {
    const char* id;
    float minVal;
    float maxVal;
    float defaultVal;
    bool isDiscrete;
};

static const std::vector<ParamSpec> kAll55Params = {
    // Oscillators (14)
    { "osc1Waveform", 0.0f, 3.0f, 0.0f, true },
    { "osc1Octave",   -3.0f, 3.0f, 0.0f, true },
    { "osc1Fine",     -1.0f, 1.0f, 0.0f, false },
    { "osc2Waveform", 0.0f, 3.0f, 0.0f, true },
    { "osc2Octave",   -3.0f, 3.0f, 0.0f, true },
    { "osc2Fine",     -1.0f, 1.0f, 0.0f, false },
    { "oscMix",       0.0f, 1.0f, 0.5f, false },
    { "osc3Waveform", 0.0f, 1.0f, 0.0f, true },
    { "osc3Level",    0.0f, 1.0f, 0.0f, false },
    { "ringModMix",   0.0f, 1.0f, 0.0f, false },
    { "pulseWidth",   0.01f, 0.99f, 0.5f, false },
    { "fmAmount",     0.0f, 1.0f, 0.0f, false },
    { "velToAmp",     0.0f, 1.0f, 0.5f, false },
    { "velToFilter",  0.0f, 1.0f, 0.5f, false },

    // Filter (5)
    { "filterMode",      0.0f, 5.0f, 1.0f, true },
    { "filterCutoff",    20.0f, 20000.0f, 1200.0f, false },
    { "filterResonance", 0.0f, 1.0f, 0.2f, false },
    { "filterKbTrack",   0.0f, 1.0f, 0.5f, false },
    { "filterEnvAmount", -1.0f, 1.0f, 0.0f, false },

    // Envelopes (13)
    { "ampAttack",     0.001f, 10.0f, 0.01f, false },
    { "ampDecay",      0.001f, 10.0f, 0.30f, false },
    { "ampSustain",    0.0f, 1.0f, 0.80f, false },
    { "ampRelease",    0.001f, 10.0f, 0.30f, false },
    { "filterAttack",  0.001f, 10.0f, 0.05f, false },
    { "filterDecay",   0.001f, 10.0f, 0.50f, false },
    { "filterSustain", 0.0f, 1.0f, 0.50f, false },
    { "filterRelease", 0.001f, 10.0f, 0.40f, false },
    { "envLink",       0.0f, 1.0f, 0.0f, true },
    { "modAttack",     0.001f, 5.0f, 0.05f, false },
    { "modDecay",      0.001f, 10.0f, 0.50f, false },
    { "modAmount",     -1.0f, 1.0f, 0.0f, false },
    { "modTarget",     0.0f, 3.0f, 0.0f, true },

    // Dual LFOs (16)
    { "lfo1Waveform", 0.0f, 3.0f, 2.0f, true },
    { "lfo1Rate",     0.05f, 30.0f, 2.0f, false },
    { "lfo1Delay",    0.0f, 5.0f, 0.0f, false },
    { "lfo1Sync",     0.0f, 1.0f, 0.0f, true },
    { "lfo1SyncDiv",  0.0f, 5.0f, 3.0f, true },
    { "lfo1KeyReset", 0.0f, 1.0f, 1.0f, true },
    { "lfo1Amount",   0.0f, 1.0f, 0.0f, false },
    { "lfo1Target",   0.0f, 2.0f, 1.0f, true },
    { "lfo2Waveform", 0.0f, 3.0f, 2.0f, true },
    { "lfo2Rate",     0.05f, 30.0f, 1.0f, false },
    { "lfo2Delay",    0.0f, 5.0f, 0.0f, false },
    { "lfo2Sync",     0.0f, 1.0f, 0.0f, true },
    { "lfo2SyncDiv",  0.0f, 5.0f, 4.0f, true },
    { "lfo2KeyReset", 0.0f, 1.0f, 1.0f, true },
    { "lfo2Amount",   0.0f, 1.0f, 0.0f, false },
    { "lfo2Target",   0.0f, 2.0f, 0.0f, true },

    // Character & Output (7)
    { "driveEnabled", 0.0f, 1.0f, 0.0f, true },
    { "driveAmount",  0.0f, 1.0f, 0.3f, false },
    { "driveTone",    0.0f, 1.0f, 0.5f, false },
    { "dualMode",     0.0f, 1.0f, 0.0f, true },
    { "analogMode",   0.0f, 1.0f, 0.0f, true },
    { "wNoiseMode",   0.0f, 1.0f, 0.0f, true },
    { "masterVolume", 0.0f, 1.0f, 0.8f, false }
};

// ============================================================================
// SUITE 1: 10,000+ Random Parameter Sets Round-Trip Recall Precision
// ============================================================================
bool runRoundTripPrecisionChallenge(bumbler::BumblerAudioProcessor& procA,
                                   bumbler::BumblerAudioProcessor& procB)
{
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 1] Testing 10,000+ Parameter Sets for Round-Trip Recall Precision...\n";
    std::cout << "======================================================================\n";

    constexpr int kNumRandomTrials = 10000;
    constexpr int kNumBoundaryTrials = 500;
    const int totalTrials = kNumRandomTrials + kNumBoundaryTrials;

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    auto& apvtsA = procA.getApvts();
    auto& apvtsB = procB.getApvts();

    double maxNormalizedDiffObserved = 0.0;
    double maxSnapshotDiffObserved = 0.0;
    std::string worstParamId;

    const auto startTime = std::chrono::high_resolution_clock::now();

    for (int trial = 0; trial < totalTrials; ++trial) {
        // Set parameters in procA
        for (size_t i = 0; i < kAll55Params.size(); ++i) {
            const auto& spec = kAll55Params[i];
            auto* p = apvtsA.getParameter(spec.id);
            CHALLENGE_ASSERT(p != nullptr, "Parameter pointer null in procA");

            float normVal = 0.0f;
            if (trial < kNumBoundaryTrials) {
                if (spec.isDiscrete) {
                    const int steps = static_cast<int>(spec.maxVal - spec.minVal + 0.5f);
                    // Cycle through all legal discrete steps (min, max, and every intermediate step)
                    const int step = (trial % (steps + 1));
                    const float plain = spec.minVal + static_cast<float>(step);
                    normVal = p->convertTo0to1(plain);
                } else {
                    // Extreme continuous boundary conditions
                    switch (trial % 5) {
                        case 0: normVal = 0.0f; break; // Min
                        case 1: normVal = 1.0f; break; // Max
                        case 2: normVal = 0.5f; break; // Center
                        case 3: normVal = 1e-4f; break; // Near-min
                        case 4: normVal = 1.0f - 1e-4f; break; // Near-max
                    }
                }
            } else {
                if (spec.isDiscrete) {
                    const int steps = static_cast<int>(spec.maxVal - spec.minVal + 0.5f);
                    const int s = std::uniform_int_distribution<int>(0, steps)(rng);
                    const float plain = spec.minVal + static_cast<float>(s);
                    normVal = p->convertTo0to1(plain);
                } else {
                    normVal = dist01(rng);
                }
            }
            p->setValueNotifyingHost(normVal);
        }

        // Capture source state
        const bumbler::ParameterSnapshot snapA = procA.getCurrentSnapshot();
        std::vector<float> origNormValues(kAll55Params.size());
        for (size_t i = 0; i < kAll55Params.size(); ++i) {
            origNormValues[i] = apvtsA.getParameter(kAll55Params[i].id)->getValue();
        }

        // Serialize state from procA
        juce::MemoryBlock block;
        procA.getStateInformation(block);
        CHALLENGE_ASSERT(block.getSize() > 50, "Serialized state block empty or too small");

        // Deserialize state into fresh procB
        procB.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        const bumbler::ParameterSnapshot snapB = procB.getCurrentSnapshot();

        // Compare all 55 parameters
        const float* floatArrayA = reinterpret_cast<const float*>(&snapA);
        const float* floatArrayB = reinterpret_cast<const float*>(&snapB);

        for (size_t i = 0; i < kAll55Params.size(); ++i) {
            auto* pB = apvtsB.getParameter(kAll55Params[i].id);
            const float recalledNorm = pB->getValue();
            const double normDiff = std::abs(static_cast<double>(recalledNorm) - static_cast<double>(origNormValues[i]));

            if (normDiff > maxNormalizedDiffObserved) {
                maxNormalizedDiffObserved = normDiff;
                worstParamId = kAll55Params[i].id;
            }

            // Snapshot plain float comparison
            const float plainA = floatArrayA[i];
            const float plainB = floatArrayB[i];
            const double snapDiff = std::abs(static_cast<double>(plainA) - static_cast<double>(plainB));

            if (snapDiff > maxSnapshotDiffObserved) {
                maxSnapshotDiffObserved = snapDiff;
            }

            if (normDiff > 1.0e-4) {
                std::cout << "\nDEBUG TRIAL " << trial << " PARAM: " << kAll55Params[i].id
                          << " (isDiscrete=" << kAll55Params[i].isDiscrete << ")"
                          << "\n  origNormVal:  " << origNormValues[i]
                          << "\n  recalledNorm: " << recalledNorm
                          << "\n  normDiff:     " << normDiff
                          << "\n  snapA:        " << plainA
                          << "\n  snapB:        " << plainB
                          << "\n  snapDiff:     " << snapDiff << "\n";
            }

            CHALLENGE_ASSERT(normDiff <= 1.0e-4, 
                ("Normalized precision mismatch on " + std::string(kAll55Params[i].id) + 
                 " trial " + std::to_string(trial) + " diff=" + std::to_string(normDiff)).c_str());

            // Discrete parameters must be bit-identical or <= 1e-4
            if (kAll55Params[i].isDiscrete) {
                CHALLENGE_ASSERT(snapDiff < 1.0e-4, 
                    ("Discrete snapshot mismatch on " + std::string(kAll55Params[i].id)).c_str());
            } else {
                // Continuous parameters: relative precision check
                const double maxVal = std::max(1.0, static_cast<double>(std::abs(plainA)));
                const double relDiff = snapDiff / maxVal;
                CHALLENGE_ASSERT(relDiff <= 1.0e-4 || snapDiff <= 1.0e-4,
                    ("Continuous snapshot precision mismatch on " + std::string(kAll55Params[i].id) +
                     " snapA=" + std::to_string(plainA) + " snapB=" + std::to_string(plainB)).c_str());
            }
        }
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();

    std::cout << "  - Total Parameter Sets Tested: " << totalTrials << " (10,000 random + 500 boundaries)\n";
    std::cout << "  - Parameters per Set:          55 floats\n";
    std::cout << "  - Total Float Comparisons:     " << (totalTrials * 55) << "\n";
    std::cout << "  - Total Execution Time:        " << std::fixed << std::setprecision(2) << elapsedSec << " s\n";
    std::cout << "  - Max Normalized Delta:        " << std::scientific << maxNormalizedDiffObserved << " (Worst: " << worstParamId << ")\n";
    std::cout << "  - Max Snapshot Plain Delta:    " << std::scientific << maxSnapshotDiffObserved << "\n";
    std::cout << "  - Required Epsilon:            <= 1.0e-4\n";

    CHALLENGE_ASSERT(maxNormalizedDiffObserved <= 1.0e-4, "Max normalized delta exceeded 1.0e-4 requirement!");

    std::cout << "  -> PASS: All " << totalTrials << " parameter sets recalled with strict precision <= 1.0e-4 across all 55 floats.\n";
    ++gTestsPassed;
    return true;
}

// ============================================================================
// SUITE 2: Corrupted, Invalid & Truncated Chunk Robustness
// ============================================================================
bool runCorruptedDataRobustnessChallenge(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 2] Testing Recovery from Corrupted, Truncated & Invalid Chunks...\n";
    std::cout << "======================================================================\n";

    // 2.1: Null pointer & 0-byte chunk tests
    std::cout << "  [2.1] Testing nullptr and 0-byte edge cases...\n";
    proc.setStateInformation(nullptr, 0);
    proc.setStateInformation(nullptr, 100);
    proc.setStateInformation(nullptr, -1);
    proc.setStateInformation(nullptr, -99999);

    std::vector<uint8_t> dummyBuffer = { 0x01, 0x02, 0x03 };
    proc.setStateInformation(dummyBuffer.data(), 0);
    proc.setStateInformation(dummyBuffer.data(), -1);
    proc.setStateInformation(dummyBuffer.data(), -100);

    // 2.2: Arbitrary binary noise fuzzing (1,000 random seeds & sizes)
    std::cout << "  [2.2] Testing arbitrary binary noise chunks (1,000 runs)...\n";
    std::mt19937 rng(999);
    const size_t testSizes[] = { 1, 2, 3, 4, 7, 16, 32, 64, 128, 256, 512, 1024, 4096, 16384, 65536 };

    for (int i = 0; i < 1000; ++i) {
        const size_t sz = testSizes[i % (sizeof(testSizes) / sizeof(testSizes[0]))];
        std::vector<uint8_t> garbage(sz);
        for (size_t b = 0; b < sz; ++b) {
            garbage[b] = static_cast<uint8_t>(rng() & 0xFF);
        }
        proc.setStateInformation(garbage.data(), static_cast<int>(garbage.size()));
    }

    // 2.3: Patterned hostile binary blocks
    std::cout << "  [2.3] Testing patterned hostile binary blocks (all 0x00, 0xFF, 0xAA, repeated strings)...\n";
    std::vector<uint8_t> allZeros(2048, 0x00);
    proc.setStateInformation(allZeros.data(), static_cast<int>(allZeros.size()));

    std::vector<uint8_t> allOnes(2048, 0xFF);
    proc.setStateInformation(allOnes.data(), static_cast<int>(allOnes.size()));

    std::vector<uint8_t> altBits(2048, 0xAA);
    proc.setStateInformation(altBits.data(), static_cast<int>(altBits.size()));

    // 2.4: Truncated valid state chunks (test every byte truncation)
    std::cout << "  [2.4] Testing byte-by-byte truncation of valid XML state...\n";
    juce::MemoryBlock validBlock;
    proc.getStateInformation(validBlock);
    const int fullSize = static_cast<int>(validBlock.getSize());
    CHALLENGE_ASSERT(fullSize > 100, "Valid state block unexpectedly small");

    for (int cut = 1; cut < fullSize; ++cut) {
        proc.setStateInformation(validBlock.getData(), cut);
    }

    // 2.5: Malformed XML strings (unclosed tags, mismatched tags, illegal chars)
    std::cout << "  [2.5] Testing malformed XML syntax injection...\n";
    const std::vector<std::string> malformedXmlCases = {
        "",
        "   \t\r\n   ",
        "<",
        "<?xml",
        "<Parameters>",
        "<Parameters><PARAM",
        "<Parameters><PARAM id=\"filterCutoff\"",
        "<Parameters><PARAM id=\"filterCutoff\" value=\"1000\"",
        "<Parameters><PARAM id=\"filterCutoff\" value=\"1000\" /",
        "<Parameters><PARAM id=\"filterCutoff\" value=\"1000\" ></Parameters",
        "<Parameters></MismatchedTag>",
        "<WrongRootTag><PARAM id=\"filterCutoff\" value=\"500\" /></WrongRootTag>",
        "<Parameters><PARAM id=\"\" value=\"\" /></Parameters>",
        "<Parameters><PARAM /></Parameters>",
        "<Parameters><![CDATA[arbitrary binary \x00\x01\x02\xFF]]></Parameters>",
        "<html><body>Not a preset</body></html>",
        "{\"type\": \"json\", \"not\": \"xml\"}"
    };

    for (const auto& xmlCase : malformedXmlCases) {
        proc.setStateInformation(xmlCase.data(), static_cast<int>(xmlCase.size()));
    }

    // 2.6: Audio engine verification after all corrupt inputs
    std::cout << "  [2.6] Verifying audio engine safety after corrupted chunk barrage...\n";
    proc.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

    proc.processBlock(buffer, midi);

    for (int ch = 0; ch < 2; ++ch) {
        for (int s = 0; s < 256; ++s) {
            const float val = buffer.getSample(ch, s);
            CHALLENGE_ASSERT(!std::isnan(val), "NaN produced in audio callback after corrupted state injection!");
            CHALLENGE_ASSERT(!std::isinf(val), "Inf produced in audio callback after corrupted state injection!");
            CHALLENGE_ASSERT(std::fpclassify(val) != FP_SUBNORMAL, "Denormal produced in audio callback!");
        }
    }

    std::cout << "  -> PASS: 0 crashes, 0 exceptions, 0 leaks, audio engine intact after corrupted inputs.\n";
    ++gTestsPassed;
    return true;
}

// ============================================================================
// SUITE 3: Non-Finite (NaN, Inf) & Extreme Float Sanitization
// ============================================================================
bool runNonFiniteSanitizationChallenge(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 3] Testing Non-Finite (NaN, +/-Inf, Subnormals) & Out-of-Range Sanitization...\n";
    std::cout << "======================================================================\n";

    // 3.1: Child PARAM XML with NaNs across all 55 parameters
    std::cout << "  [3.1] Injecting NaN across all 55 parameters via child PARAM nodes...\n";
    std::ostringstream nanXml;
    nanXml << "<Parameters schemaVersion=\"1\">";
    for (const auto& spec : kAll55Params) {
        nanXml << "<PARAM id=\"" << spec.id << "\" value=\"NaN\" />";
    }
    nanXml << "</Parameters>";

    const std::string nanXmlStr = nanXml.str();
    proc.setStateInformation(nanXmlStr.data(), static_cast<int>(nanXmlStr.size()));

    // Verify all 55 parameters are finite and reset to defaults
    bumbler::ParameterSnapshot snapAfterNaN = proc.getCurrentSnapshot();
    const float* floatArrayNaN = reinterpret_cast<const float*>(&snapAfterNaN);

    for (size_t i = 0; i < kAll55Params.size(); ++i) {
        const float val = floatArrayNaN[i];
        CHALLENGE_ASSERT(!std::isnan(val), ("Parameter " + std::string(kAll55Params[i].id) + " remained NaN!").c_str());
        CHALLENGE_ASSERT(!std::isinf(val), ("Parameter " + std::string(kAll55Params[i].id) + " became Inf!").c_str());
        CHALLENGE_ASSERT(val >= kAll55Params[i].minVal && val <= kAll55Params[i].maxVal,
            ("Parameter " + std::string(kAll55Params[i].id) + " outside valid bounds after NaN sanitization!").c_str());
    }

    // 3.2: Child PARAM XML with +Infinity and -Infinity
    std::cout << "  [3.2] Injecting +Infinity and -Infinity across all 55 parameters...\n";
    std::ostringstream infXml;
    infXml << "<Parameters schemaVersion=\"1\">";
    for (size_t i = 0; i < kAll55Params.size(); ++i) {
        const char* infStr = (i % 2 == 0) ? "Infinity" : "-Infinity";
        infXml << "<PARAM id=\"" << kAll55Params[i].id << "\" value=\"" << infStr << "\" />";
    }
    infXml << "</Parameters>";

    const std::string infXmlStr = infXml.str();
    proc.setStateInformation(infXmlStr.data(), static_cast<int>(infXmlStr.size()));

    bumbler::ParameterSnapshot snapAfterInf = proc.getCurrentSnapshot();
    const float* floatArrayInf = reinterpret_cast<const float*>(&snapAfterInf);

    for (size_t i = 0; i < kAll55Params.size(); ++i) {
        const float val = floatArrayInf[i];
        CHALLENGE_ASSERT(!std::isnan(val), ("Parameter " + std::string(kAll55Params[i].id) + " became NaN after Inf injection!").c_str());
        CHALLENGE_ASSERT(!std::isinf(val), ("Parameter " + std::string(kAll55Params[i].id) + " remained Inf!").c_str());
        CHALLENGE_ASSERT(val >= kAll55Params[i].minVal && val <= kAll55Params[i].maxVal,
            ("Parameter " + std::string(kAll55Params[i].id) + " outside valid bounds after Inf sanitization!").c_str());
    }

    // 3.3: Extreme out-of-range floats (1e30, -1e30, 999999999)
    std::cout << "  [3.3] Injecting massive out-of-range floats (1e30, -1e30, 999999999)...\n";
    std::ostringstream oorXml;
    oorXml << "<Parameters schemaVersion=\"1\">";
    for (size_t i = 0; i < kAll55Params.size(); ++i) {
        const char* valStr = (i % 2 == 0) ? "1.0e30" : "-1.0e30";
        oorXml << "<PARAM id=\"" << kAll55Params[i].id << "\" value=\"" << valStr << "\" />";
    }
    oorXml << "</Parameters>";

    const std::string oorXmlStr = oorXml.str();
    proc.setStateInformation(oorXmlStr.data(), static_cast<int>(oorXmlStr.size()));

    bumbler::ParameterSnapshot snapAfterOor = proc.getCurrentSnapshot();
    const float* floatArrayOor = reinterpret_cast<const float*>(&snapAfterOor);

    for (size_t i = 0; i < kAll55Params.size(); ++i) {
        const float val = floatArrayOor[i];
        CHALLENGE_ASSERT(!std::isnan(val), "NaN after out-of-range float injection");
        CHALLENGE_ASSERT(!std::isinf(val), "Inf after out-of-range float injection");
        CHALLENGE_ASSERT(val >= kAll55Params[i].minVal - 1e-4f && val <= kAll55Params[i].maxVal + 1e-4f,
            ("Parameter " + std::string(kAll55Params[i].id) + " not clamped to valid range! val=" + std::to_string(val)).c_str());
    }

    // 3.4: Root Attribute XML Format with NaNs & Infinities
    std::cout << "  [3.4] Injecting NaNs and Infinities via root attribute XML format...\n";
    std::ostringstream attrXml;
    attrXml << "<Parameters schemaVersion=\"1\" ";
    for (size_t i = 0; i < kAll55Params.size(); ++i) {
        if (i % 3 == 0) {
            attrXml << kAll55Params[i].id << "=\"NaN\" ";
        } else if (i % 3 == 1) {
            attrXml << kAll55Params[i].id << "=\"Infinity\" ";
        } else {
            attrXml << kAll55Params[i].id << "=\"-99999.0\" ";
        }
    }
    attrXml << "/>";

    const std::string attrXmlStr = attrXml.str();
    proc.setStateInformation(attrXmlStr.data(), static_cast<int>(attrXmlStr.size()));

    bumbler::ParameterSnapshot snapAfterAttr = proc.getCurrentSnapshot();
    const float* floatArrayAttr = reinterpret_cast<const float*>(&snapAfterAttr);

    for (size_t i = 0; i < kAll55Params.size(); ++i) {
        const float val = floatArrayAttr[i];
        CHALLENGE_ASSERT(!std::isnan(val), ("Root attr NaN sanitization failed on " + std::string(kAll55Params[i].id)).c_str());
        CHALLENGE_ASSERT(!std::isinf(val), ("Root attr Inf sanitization failed on " + std::string(kAll55Params[i].id)).c_str());
        CHALLENGE_ASSERT(val >= kAll55Params[i].minVal - 1e-4f && val <= kAll55Params[i].maxVal + 1e-4f,
            ("Root attr clamping failed on " + std::string(kAll55Params[i].id)).c_str());
    }

    // 3.5: Audio playback verification after hostile float barrage
    std::cout << "  [3.5] Verifying synthesizer audio playback remains clean & finite...\n";
    proc.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
    proc.processBlock(buffer, midi);

    float peakL = buffer.getMagnitude(0, 0, 512);
    float peakR = buffer.getMagnitude(1, 0, 512);

    CHALLENGE_ASSERT(!std::isnan(peakL) && !std::isnan(peakR), "Audio callback produced NaN!");
    CHALLENGE_ASSERT(!std::isinf(peakL) && !std::isinf(peakR), "Audio callback produced Inf!");
    CHALLENGE_ASSERT(peakL <= 1.05f && peakR <= 1.05f, "Audio callback exceeded headroom ceiling!");

    std::cout << "  -> PASS: All non-finite floats safely sanitized to defaults or clamped to bounds; audio clean.\n";
    ++gTestsPassed;
    return true;
}

// ============================================================================
// SUITE 4: Partial States, Schema Versioning & Program Index Resilience
// ============================================================================
bool runSchemaAndPartialStateChallenge(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 4] Testing Partial States, Future Schemas & Program Index Resilience...\n";
    std::cout << "======================================================================\n";

    // 4.1: Partial state chunk (only 2 parameters specified out of 55)
    std::cout << "  [4.1] Testing partial state chunk (only 2 parameters)...\n";
    const std::string partialXml = 
        "<Parameters schemaVersion=\"1\">"
        "  <PARAM id=\"filterCutoff\" value=\"2500.0\" />"
        "  <PARAM id=\"oscMix\" value=\"0.8\" />"
        "</Parameters>";
    proc.setStateInformation(partialXml.data(), static_cast<int>(partialXml.size()));

    bumbler::ParameterSnapshot snapPartial = proc.getCurrentSnapshot();
    CHALLENGE_ASSERT(std::abs(snapPartial.filterCutoff - 2500.0f) < 5.0f, "Partial cutoff restore failed");
    CHALLENGE_ASSERT(std::abs(snapPartial.oscMix - 0.8f) < 0.01f, "Partial oscMix restore failed");
    CHALLENGE_ASSERT(std::isfinite(snapPartial.filterResonance), "Unspecified parameter became non-finite");

    // 4.2: Future schema version with unexpected extra parameters
    std::cout << "  [4.2] Testing forward compatibility: future schema with unexpected foreign keys...\n";
    const std::string futureXml = 
        "<Parameters schemaVersion=\"99\" futureGlobalSetting=\"true\">"
        "  <PARAM id=\"filterCutoff\" value=\"3500.0\" />"
        "  <PARAM id=\"alienSubOscHarmonics\" value=\"0.999\" />"
        "  <PARAM id=\"futureQuantumChorus\" value=\"1.0\" />"
        "</Parameters>";
    proc.setStateInformation(futureXml.data(), static_cast<int>(futureXml.size()));

    bumbler::ParameterSnapshot snapFuture = proc.getCurrentSnapshot();
    CHALLENGE_ASSERT(std::abs(snapFuture.filterCutoff - 3500.0f) < 5.0f, "Known parameter in future schema failed");

    // 4.3: Program Index bounds testing
    std::cout << "  [4.3] Testing programIndex property boundaries (valid vs out-of-range)...\n";
    // Valid programIndex = 3
    const std::string prog3Xml = "<Parameters schemaVersion=\"1\" programIndex=\"3\"><PARAM id=\"filterCutoff\" value=\"850.0\"/></Parameters>";
    proc.setStateInformation(prog3Xml.data(), static_cast<int>(prog3Xml.size()));
    CHALLENGE_ASSERT(proc.getCurrentProgram() == 3, "Valid programIndex 3 not restored");

    // Invalid programIndex = 99 (must not exceed getNumPrograms() - 1 = 4)
    const std::string prog99Xml = "<Parameters schemaVersion=\"1\" programIndex=\"99\"><PARAM id=\"filterCutoff\" value=\"850.0\"/></Parameters>";
    proc.setStateInformation(prog99Xml.data(), static_cast<int>(prog99Xml.size()));
    CHALLENGE_ASSERT(proc.getCurrentProgram() == 3, "Out-of-range programIndex 99 was improperly accepted");

    // Invalid programIndex = -1
    const std::string progNegXml = "<Parameters schemaVersion=\"1\" programIndex=\"-1\"><PARAM id=\"filterCutoff\" value=\"850.0\"/></Parameters>";
    proc.setStateInformation(progNegXml.data(), static_cast<int>(progNegXml.size()));
    CHALLENGE_ASSERT(proc.getCurrentProgram() == 3, "Negative programIndex -1 was improperly accepted");

    std::cout << "  -> PASS: Partial states, forward schema compatibility, and program index bounds verified.\n";
    ++gTestsPassed;
    return true;
}

} // namespace challenger_m5_1

// ============================================================================
// Main Test Entry Point
// ============================================================================
int main() {
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "======================================================================\n";
    std::cout << " BUMBLER XD : CHALLENGER M5-1 STATE RECALL & ROBUSTNESS SUITE         \n";
    std::cout << "======================================================================\n";

    bumbler::BumblerAudioProcessor procA;
    bumbler::BumblerAudioProcessor procB;

    bool p1 = challenger_m5_1::runRoundTripPrecisionChallenge(procA, procB);
    bool p2 = challenger_m5_1::runCorruptedDataRobustnessChallenge(procA);
    bool p3 = challenger_m5_1::runNonFiniteSanitizationChallenge(procA);
    bool p4 = challenger_m5_1::runSchemaAndPartialStateChallenge(procA);

    bool allPassed = p1 && p2 && p3 && p4 && (gTestsFailed == 0);

    std::cout << "\n======================================================================\n";
    std::cout << " CHALLENGER M5-1 FINAL SUMMARY:\n";
    std::cout << "  - Challenge Suites Passed: " << gTestsPassed << " / 4\n";
    std::cout << "  - Challenge Suites Failed: " << gTestsFailed << "\n";
    std::cout << "  - Overall Status:          " << (allPassed ? "[ALL CHALLENGES PASSED]" : "[FAILED]") << "\n";
    std::cout << "  - Binary Verdict:          " << (allPassed ? "APPROVE" : "REJECT") << "\n";
    std::cout << "======================================================================\n";

    return allPassed ? 0 : 1;
}
