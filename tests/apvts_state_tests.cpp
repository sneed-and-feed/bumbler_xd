#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../source/plugin/PluginProcessor.h"
#include "../source/plugin/PluginEditor.h"
#include "../source/dsp/ParameterSnapshot.h"
#include "../source/dsp/BumblerEngine.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <sstream>
#include <chrono>

#define APVTS_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [FAIL] " << msg << " (" #cond ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    } \
} while (false)

namespace bumbler_apvts_tests {

// Full metadata record for 55 parameters
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

// 1. Parameter Registration & Default Values Test
bool testParameterRegistration(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "[Test 1] Verifying all 55 APVTS parameters registered & bounded...\n";
    auto& apvts = proc.getApvts();
    APVTS_ASSERT(kAll55Params.size() == 55, "Spec must list exactly 55 parameters");

    for (const auto& spec : kAll55Params) {
        auto* param = apvts.getParameter(spec.id);
        APVTS_ASSERT(param != nullptr, (std::string("Missing parameter: ") + spec.id).c_str());
        APVTS_ASSERT(param->paramID == spec.id, "Parameter ID mismatch");

        // Verify range bounds
        auto range = param->getNormalisableRange();
        APVTS_ASSERT(range.start <= spec.minVal + 1e-4f, "Range min mismatch");
        APVTS_ASSERT(range.end >= spec.maxVal - 1e-4f, "Range max mismatch");

        // Verify normalized range in [0, 1]
        float normVal = param->getValue();
        APVTS_ASSERT(normVal >= 0.0f && normVal <= 1.0f, "Normalized value out of bounds");
    }
    std::cout << "  -> PASS: All 55 parameters verified.\n";
    return true;
}

// 2. Parameter Normalization & Bijective Mapping Test
bool testParameterNormalization(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "[Test 2] Verifying normalization bijective mappings...\n";
    auto& apvts = proc.getApvts();

    for (const auto& spec : kAll55Params) {
        auto* param = apvts.getParameter(spec.id);
        if (spec.isDiscrete) {
            const int numSteps = static_cast<int>(spec.maxVal - spec.minVal + 1.5f);
            for (int s = 0; s < numSteps; ++s) {
                float plain = spec.minVal + static_cast<float>(s);
                float norm = param->convertTo0to1(plain);
                float plainBack = param->convertFrom0to1(norm);
                APVTS_ASSERT(std::abs(plain - plainBack) < 1e-4f,
                    (std::string("Non-bijective normalization on discrete ") + spec.id).c_str());
            }
        } else {
            const float testPoints[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
            for (float normIn : testPoints) {
                float plainVal = param->convertFrom0to1(normIn);
                float normOut = param->convertTo0to1(plainVal);
                float plainBack = param->convertFrom0to1(normOut);
                APVTS_ASSERT(std::abs(plainVal - plainBack) < 1e-3f,
                    (std::string("Non-bijective normalization on ") + spec.id).c_str());
            }
        }
    }
    std::cout << "  -> PASS: Normalization bijectivity verified.\n";
    return true;
}

// 3. Lock-Free ParameterSnapshot Extraction Test
bool testSnapshotExtraction(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "[Test 3] Verifying lock-free ParameterSnapshot extraction & performance...\n";
    auto& apvts = proc.getApvts();

    // Set known distinctive values
    if (auto* p = apvts.getParameter("filterCutoff")) p->setValueNotifyingHost(p->convertTo0to1(3333.0f));
    if (auto* p = apvts.getParameter("oscMix"))        p->setValueNotifyingHost(p->convertTo0to1(0.77f));
    if (auto* p = apvts.getParameter("pulseWidth"))    p->setValueNotifyingHost(p->convertTo0to1(0.33f));
    if (auto* p = apvts.getParameter("driveAmount"))   p->setValueNotifyingHost(p->convertTo0to1(0.65f));

    bumbler::ParameterSnapshot snap = proc.getCurrentSnapshot();
    APVTS_ASSERT(std::abs(snap.filterCutoff - 3333.0f) < 5.0f, "Cutoff snapshot extraction failed");
    APVTS_ASSERT(std::abs(snap.oscMix - 0.77f) < 0.01f, "OscMix snapshot extraction failed");
    APVTS_ASSERT(std::abs(snap.pulseWidth - 0.33f) < 0.01f, "PulseWidth snapshot extraction failed");
    APVTS_ASSERT(std::abs(snap.driveAmount - 0.65f) < 0.01f, "DriveAmount snapshot extraction failed");

    // Microbenchmark extraction speed
    constexpr int kIterations = 100000;
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        bumbler::ParameterSnapshot s = proc.getCurrentSnapshot();
        if (std::isnan(s.filterCutoff)) {
            return false;
        }
    }
    const auto end = std::chrono::high_resolution_clock::now();
    const double elapsedNs = std::chrono::duration<double, std::nano>(end - start).count();
    const double nsPerCall = elapsedNs / kIterations;

    std::cout << "  -> Lock-free snapshot latency: " << nsPerCall << " ns/call (Target: < 250 ns)\n";
    APVTS_ASSERT(nsPerCall < 250.0, "Snapshot extraction too slow");

    std::cout << "  -> PASS: ParameterSnapshot extraction verified.\n";
    return true;
}

// 4. Full 55-Parameter Round-Trip Serialization Test
bool testRoundTripSerialization(bumbler::BumblerAudioProcessor& procA, bumbler::BumblerAudioProcessor& procB) {
    std::cout << "[Test 4] Verifying 55-parameter round-trip state serialization...\n";
    auto& apvtsA = procA.getApvts();

    // Set pseudo-random values across all 55 parameters
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist01(0.05f, 0.95f);

    std::vector<float> originalNorm(kAll55Params.size());
    for (size_t i = 0; i < kAll55Params.size(); ++i) {
        auto* param = apvtsA.getParameter(kAll55Params[i].id);
        if (kAll55Params[i].isDiscrete) {
            int numSteps = static_cast<int>(kAll55Params[i].maxVal - kAll55Params[i].minVal + 0.5f);
            int step = std::uniform_int_distribution<int>(0, numSteps)(rng);
            float plain = kAll55Params[i].minVal + static_cast<float>(step);
            param->setValueNotifyingHost(param->convertTo0to1(plain));
        } else {
            float randVal = dist01(rng);
            param->setValueNotifyingHost(randVal);
        }
        originalNorm[i] = param->getValue();
    }

    // Save state
    juce::MemoryBlock block;
    procA.getStateInformation(block);
    APVTS_ASSERT(block.getSize() > 100, "State binary block too small");

    // Restore to fresh procB
    procB.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    auto& apvtsB = procB.getApvts();

    for (size_t i = 0; i < kAll55Params.size(); ++i) {
        auto* paramB = apvtsB.getParameter(kAll55Params[i].id);
        float restoredVal = paramB->getValue();
        if (std::abs(restoredVal - originalNorm[i]) >= 2e-3f) {
            std::cout << "\nMismatch on " << kAll55Params[i].id 
                      << " (isDiscrete=" << kAll55Params[i].isDiscrete << ")"
                      << ": originalNorm=" << originalNorm[i]
                      << ", restoredVal=" << restoredVal << "\n";
        }
        APVTS_ASSERT(std::abs(restoredVal - originalNorm[i]) < 2e-3f,
            (std::string("Roundtrip mismatch on ") + kAll55Params[i].id).c_str());
    }

    std::cout << "  -> PASS: All 55 parameters restored identically.\n";
    return true;
}

// 5. Corrupted Chunk & Fuzzing Recovery Test
bool testCorruptedChunkRecovery(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "[Test 5] Verifying corrupted state chunk resilience & fallback...\n";

    // Subcase 5A: Nullptr & 0 size
    proc.setStateInformation(nullptr, 0);

    // Subcase 5B: Binary garbage
    std::vector<uint8_t> garbage(1024);
    for (size_t i = 0; i < garbage.size(); ++i) garbage[i] = static_cast<uint8_t>(i ^ 0xAA);
    proc.setStateInformation(garbage.data(), static_cast<int>(garbage.size()));

    // Subcase 5C: Malformed XML string
    const std::string malformedXml = "<Parameters><brokenTag attr=\"val\"";
    proc.setStateInformation(malformedXml.data(), static_cast<int>(malformedXml.size()));

    // Subcase 5D: Malicious XML with NaNs & Infinities
    const std::string nanXml = "<Parameters filterCutoff=\"NaN\" driveAmount=\"Infinity\" oscMix=\"-999.0\" />";
    juce::MemoryBlock memNan(nanXml.data(), nanXml.size());
    proc.setStateInformation(memNan.getData(), static_cast<int>(memNan.getSize()));

    // Ensure parameters remain finite and safely bounded
    auto snap = proc.getCurrentSnapshot();
    APVTS_ASSERT(std::isfinite(snap.filterCutoff), "Cutoff became non-finite after NaN injection");
    APVTS_ASSERT(std::isfinite(snap.driveAmount), "Drive became non-finite after NaN injection");
    APVTS_ASSERT(snap.filterCutoff >= 20.0f && snap.filterCutoff <= 20000.0f, "Cutoff bounds violated");
    APVTS_ASSERT(snap.oscMix >= 0.0f && snap.oscMix <= 1.0f, "OscMix bounds violated");

    // Verify audio callback executes without NaN propagation
    proc.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();
    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 256; ++i) {
            APVTS_ASSERT(std::isfinite(buffer.getSample(ch, i)), "NaN escaped to audio buffer");
        }
    }

    std::cout << "  -> PASS: Zero crashes, zero NaNs on corrupted input.\n";
    return true;
}

// 6. Factory Presets Integrity Test
bool testFactoryPresetIntegrity(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "[Test 6] Verifying 5 Factory Presets Integrity & Audio Sanity...\n";

    APVTS_ASSERT(proc.getNumPrograms() == 5, "Must have exactly 5 factory presets");

    struct PresetValidation {
        const char* name;
        int expectedFilterMode;
        float minCutoff, maxCutoff;
        float minResonance;
        float driveExpected;
    };

    const PresetValidation presets[] = {
        { "Acid Bass",    1, 200.0f,  800.0f,   0.70f, 1.0f },
        { "Sync Lead",    2, 1500.0f, 4000.0f,  0.40f, 1.0f },
        { "Swarm Pad",    3, 1000.0f, 3000.0f,  0.50f, 0.0f },
        { "Percussion",   4, 600.0f,  2000.0f,  0.50f, 1.0f },
        { "Vintage Drone",5, 50.0f,   400.0f,   0.40f, 1.0f }
    };

    for (int pIdx = 0; pIdx < 5; ++pIdx) {
        const auto& pv = presets[pIdx];
        APVTS_ASSERT(proc.getProgramName(pIdx) == pv.name, "Preset name mismatch");

        proc.setCurrentProgram(pIdx);
        bumbler::ParameterSnapshot s = proc.getCurrentSnapshot();

        APVTS_ASSERT(static_cast<int>(s.filterMode) == pv.expectedFilterMode, "Filter mode mismatch");
        APVTS_ASSERT(s.filterCutoff >= pv.minCutoff && s.filterCutoff <= pv.maxCutoff, "Cutoff range mismatch");
        APVTS_ASSERT(s.filterResonance >= pv.minResonance, "Resonance too low for signature preset");
        APVTS_ASSERT(s.driveEnabled == pv.driveExpected, "Drive enable mismatch");

        // Render block with MIDI note on
        proc.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.85f), 0);
        proc.processBlock(buffer, midi);

        float peakL = buffer.getMagnitude(0, 0, 512);
        float peakR = buffer.getMagnitude(1, 0, 512);
        APVTS_ASSERT(peakL > 0.001f || peakR > 0.001f, "Preset rendered silence");
        APVTS_ASSERT(peakL <= 1.2f && peakR <= 1.2f, "Preset exceeded headroom ceiling");
    }

    std::cout << "  -> PASS: All 5 presets verified musically and numerically.\n";
    return true;
}

// 7. GUI Editor Instantiation & Off-Screen Paint Verification
bool test_editor_instantiation_and_offscreen_paint(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "[Test 7] Verifying Editor instantiation, attachment binding & off-screen paint...\n";

    // 1. Create editor instance via AudioProcessor API
    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditorIfNeeded());
    APVTS_ASSERT(editor != nullptr, "createEditorIfNeeded returned null");

    // Standard rack dimensions
    editor->setSize(1120, 700);
    editor->setVisible(true);

    // 2. Create ARGB offscreen buffer
    juce::Image offscreen(juce::Image::ARGB, 1120, 700, true);

    // 3. Paint entire component tree (LookAndFeel, controls, LCD graphs, buttons)
    bool paintSucceeded = false;
    try {
        juce::Graphics g(offscreen);
        editor->paintEntireComponent(g, true);
        paintSucceeded = true;
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] Caught exception during paint: " << e.what() << "\n";
        return false;
    } catch (...) {
        std::cerr << "  [FAIL] Caught unknown non-std exception during paint\n";
        return false;
    }
    APVTS_ASSERT(paintSucceeded, "paintEntireComponent failed");

    // 4. Verify image was rendered into (not completely blank/transparent)
    bool hasNonZeroPixels = false;
    const juce::Image::BitmapData bm(offscreen, juce::Image::BitmapData::readOnly);
    for (int y = 0; y < bm.height && !hasNonZeroPixels; y += 10) {
        for (int x = 0; x < bm.width; x += 10) {
            auto pixel = bm.getPixelColour(x, y);
            if (pixel.getAlpha() > 0 && (pixel.getRed() > 0 || pixel.getGreen() > 0 || pixel.getBlue() > 0)) {
                hasNonZeroPixels = true;
                break;
            }
        }
    }
    APVTS_ASSERT(hasNonZeroPixels, "Rendered image is completely empty/blank");

    // 5. Clean up editor cleanly
    editor.reset();

    std::cout << "  -> PASS: Editor instantiated, painted offscreen, and cleaned up cleanly.\n";
    return true;
}

} // namespace bumbler_apvts_tests

int main() {
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "======================================================================\n";
    std::cout << "      BUMBLER XD : APVTS PARAMETER & STATE RECALL TEST SUITE          \n";
    std::cout << "======================================================================\n";

    bumbler::BumblerAudioProcessor procA;
    bumbler::BumblerAudioProcessor procB;

    bool p1 = bumbler_apvts_tests::testParameterRegistration(procA);
    bool p2 = bumbler_apvts_tests::testParameterNormalization(procA);
    bool p3 = bumbler_apvts_tests::testSnapshotExtraction(procA);
    bool p4 = bumbler_apvts_tests::testRoundTripSerialization(procA, procB);
    bool p5 = bumbler_apvts_tests::testCorruptedChunkRecovery(procA);
    bool p6 = bumbler_apvts_tests::testFactoryPresetIntegrity(procA);
    bool p7 = bumbler_apvts_tests::test_editor_instantiation_and_offscreen_paint(procA);

    bool allPassed = p1 && p2 && p3 && p4 && p5 && p6 && p7;
    std::cout << "\n======================================================================\n";
    std::cout << "  APVTS Suite Status: " << (allPassed ? "[ALL TESTS PASSED]" : "[FAILED]") << "\n";
    std::cout << "======================================================================\n";

    return allPassed ? 0 : 1;
}
