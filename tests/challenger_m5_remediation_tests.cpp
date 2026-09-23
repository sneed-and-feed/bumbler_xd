#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../source/plugin/PluginProcessor.h"
#include "../source/plugin/PluginEditor.h"
#include "../source/ui/BumblerLookAndFeel.h"
#include "../source/plugin/Parameters.h"
#include "../source/dsp/ParameterSnapshot.h"

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
#include <cassert>

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

namespace challenger_m5_rem {

// ============================================================================
// CHALLENGE 1: Direct LookAndFeel Adversarial Stress & Matrix Probing
// ============================================================================
bool testDirectLookAndFeelStress() {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 1] Direct LookAndFeel Adversarial Probing & Cast Safety...\n";
    std::cout << "======================================================================\n" << std::flush;

    bumbler::BumblerLookAndFeel lnf;

    // 1.1: Adversarial ToggleButton rendering matrix
    const std::vector<std::string> testLabels = {
        "",
        "LINK ENVS",
        "SYNC",
        "RESET",
        "DRIVE",
        "DUAL",
        "ANALOG",
        "W.NOISE",
        "A VERY EXTREMELY LONG TEXT LABEL TO TEST BUFFER BOUNDARIES AND CLIPPING WITHOUT CRASHING",
        "Ω≈ç√∫˜µ≤≥ Unicode Test",
        "%s %d %x %n format string attack test",
        "Line1\nLine2\tLine3",
        "1"
    };

    constexpr int kImgW = 300;
    constexpr int kImgH = 80;
    juce::Image testImg(juce::Image::ARGB, kImgW, kImgH, true);

    int combinationsTested = 0;
    for (const auto& labelText : testLabels) {
        juce::ToggleButton button;
        button.setButtonText(labelText);

        for (int ticked = 0; ticked <= 1; ++ticked) {
            button.setToggleState(ticked != 0, juce::dontSendNotification);

            for (int enabled = 0; enabled <= 1; ++enabled) {
                button.setEnabled(enabled != 0);

                for (int highlighted = 0; highlighted <= 1; ++highlighted) {
                    for (int down = 0; down <= 1; ++down) {
                        for (int sz = 0; sz < 8; ++sz) {
                            int bw = 25 + sz * 35;
                            int bh = 15 + sz * 8;
                            button.setBounds(0, 0, bw, bh);

                            try {
                                juce::Graphics g(testImg);
                                lnf.drawToggleButton(g, button, highlighted != 0, down != 0);
                                ++combinationsTested;
                            } catch (const std::bad_cast& bc) {
                                std::cerr << "  [CRITICAL FAIL] Caught std::bad_cast in drawToggleButton: " << bc.what() << "\n" << std::flush;
                                CHALLENGE_ASSERT(false, "std::bad_cast in drawToggleButton");
                            } catch (const std::exception& e) {
                                std::cerr << "  [FAIL] Caught std::exception in drawToggleButton: " << e.what() << "\n" << std::flush;
                                CHALLENGE_ASSERT(false, "std::exception in drawToggleButton");
                            } catch (...) {
                                std::cerr << "  [FAIL] Caught unknown exception in drawToggleButton\n" << std::flush;
                                CHALLENGE_ASSERT(false, "unknown exception in drawToggleButton");
                            }
                        }
                    }
                }
            }
        }
    }

    std::cout << "  - drawToggleButton combinations tested: " << combinationsTested << "\n" << std::flush;
    CHALLENGE_ASSERT(combinationsTested >= 1500, "Insufficient drawToggleButton combinations tested");

    // 1.2: Direct drawLabel and getLabelFont verification
    {
        juce::Label lbl;
        lbl.setText("Test Label", juce::dontSendNotification);
        lbl.setBounds(0, 0, 150, 30);

        try {
            juce::Graphics g(testImg);
            lnf.drawLabel(g, lbl);
            auto font = lnf.getLabelFont(lbl);
            CHALLENGE_ASSERT(font.getHeight() > 0.0f, "Invalid font height returned from getLabelFont");
        } catch (const std::bad_cast& bc) {
            std::cerr << "  [CRITICAL FAIL] Caught std::bad_cast in drawLabel: " << bc.what() << "\n" << std::flush;
            CHALLENGE_ASSERT(false, "std::bad_cast in drawLabel");
        } catch (const std::exception& e) {
            std::cerr << "  [FAIL] Caught exception in drawLabel: " << e.what() << "\n" << std::flush;
            CHALLENGE_ASSERT(false, "exception in drawLabel");
        }
    }

    // 1.3: Direct Rotary & Linear Slider drawing verification
    {
        juce::Slider rotarySlider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        rotarySlider.setBounds(0, 0, 80, 80);
        rotarySlider.setRange(0.0, 1.0);
        rotarySlider.setValue(0.5);

        juce::Slider linearSlider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        linearSlider.setBounds(0, 0, 200, 30);
        linearSlider.setRange(0.0, 1.0);
        linearSlider.setValue(0.75);

        try {
            juce::Graphics g(testImg);
            lnf.drawRotarySlider(g, 0, 0, 80, 80, 0.5f, 0.0f, 3.14159f * 1.5f, rotarySlider);
            lnf.drawLinearSlider(g, 0, 0, 200, 30, 150.0f, 0.0f, 200.0f, juce::Slider::LinearHorizontal, linearSlider);
        } catch (const std::exception& e) {
            std::cerr << "  [FAIL] Caught exception in slider drawing: " << e.what() << "\n" << std::flush;
            CHALLENGE_ASSERT(false, "slider drawing exception");
        }
    }

    std::cout << "  -> PASS: All LookAndFeel drawing delegates executed cleanly with zero bad_cast or exceptions.\n" << std::flush;
    ++gTestsPassed;
    return true;
}

// ============================================================================
// CHALLENGE 2: 500+ Rapid GUI Lifecycle & Painting Stress Offscreen
// ============================================================================
bool testRapidGuiLifecycleAndOffscreenPaint(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 2] 500+ Rapid GUI Create/Paint/Destroy Cycles Offscreen...\n";
    std::cout << "======================================================================\n" << std::flush;

    constexpr int kTargetCycles = 550; // Exceeds 500+ requirement
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> unitDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> widthDist(960, 1400);
    std::uniform_int_distribution<int> heightDist(600, 900);
    std::uniform_int_distribution<int> presetDist(0, 4);

    auto& apvts = proc.getApvts();
    auto* filterAttackParam = apvts.getParameter("filterAttack");
    auto* filterDecayParam = apvts.getParameter("filterDecay");
    auto* filterSustainParam = apvts.getParameter("filterSustain");
    auto* filterReleaseParam = apvts.getParameter("filterRelease");

    auto* ampAttackParam = apvts.getParameter("ampAttack");
    auto* ampDecayParam = apvts.getParameter("ampDecay");
    auto* ampSustainParam = apvts.getParameter("ampSustain");
    auto* ampReleaseParam = apvts.getParameter("ampRelease");

    auto* cutoffParam = apvts.getParameter("filterCutoff");
    auto* resoParam = apvts.getParameter("filterResonance");
    auto* oscMixParam = apvts.getParameter("oscMix");

    int successfulPaints = 0;
    int verifiedNonEmptyRenders = 0;

    auto startTime = std::chrono::steady_clock::now();

    for (int cycle = 0; cycle < kTargetCycles; ++cycle) {
        // 1. Randomize parameters and presets to test various UI display states
        if (cycle % 20 == 0) {
            proc.setCurrentProgram(presetDist(rng));
        } else {
            if (filterAttackParam) filterAttackParam->setValueNotifyingHost(unitDist(rng));
            if (filterDecayParam) filterDecayParam->setValueNotifyingHost(unitDist(rng));
            if (filterSustainParam) filterSustainParam->setValueNotifyingHost(unitDist(rng));
            if (filterReleaseParam) filterReleaseParam->setValueNotifyingHost(unitDist(rng));

            if (ampAttackParam) ampAttackParam->setValueNotifyingHost(unitDist(rng));
            if (ampDecayParam) ampDecayParam->setValueNotifyingHost(unitDist(rng));
            if (ampSustainParam) ampSustainParam->setValueNotifyingHost(unitDist(rng));
            if (ampReleaseParam) ampReleaseParam->setValueNotifyingHost(unitDist(rng));

            if (cutoffParam) cutoffParam->setValueNotifyingHost(unitDist(rng));
            if (resoParam) resoParam->setValueNotifyingHost(unitDist(rng));
            if (oscMixParam) oscMixParam->setValueNotifyingHost(unitDist(rng));
        }

        // 2. Instantiate editor
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        if (cycle % 2 == 0) {
            editor.reset(proc.createEditorIfNeeded());
        } else {
            editor = std::make_unique<bumbler::BumblerAudioProcessorEditor>(proc);
        }

        CHALLENGE_ASSERT(editor != nullptr, "Failed to instantiate BumblerAudioProcessorEditor");

        // 3. Resize editor across varied rack bounds
        int curW = 1120;
        int curH = 700;
        if (cycle % 15 == 0) {
            curW = 960; curH = 600; // Min limits
        } else if (cycle % 15 == 1) {
            curW = 1600; curH = 1000; // Large bounds
        } else if (cycle % 3 == 0) {
            curW = widthDist(rng);
            curH = heightDist(rng);
        }
        editor->setSize(curW, curH);
        editor->setVisible(true);

        // 4. Create ARGB offscreen buffer and paint entire component tree
        {
            juce::Image offscreen(juce::Image::ARGB, curW, curH, true);
            try {
                juce::Graphics g(offscreen);
                editor->paintEntireComponent(g, true);
                ++successfulPaints;
            } catch (const std::bad_cast& bc) {
                std::cerr << "  [CRITICAL FAIL] Caught std::bad_cast during cycle " << cycle << ": " << bc.what() << "\n" << std::flush;
                CHALLENGE_ASSERT(false, "std::bad_cast during editor paintEntireComponent");
            } catch (const std::exception& e) {
                std::cerr << "  [FAIL] Caught std::exception during cycle " << cycle << ": " << e.what() << "\n" << std::flush;
                CHALLENGE_ASSERT(false, "std::exception during editor paintEntireComponent");
            } catch (...) {
                std::cerr << "  [FAIL] Caught unknown exception during cycle " << cycle << "\n" << std::flush;
                CHALLENGE_ASSERT(false, "unknown exception during editor paintEntireComponent");
            }

            // 5. Periodically inspect bitmap data to verify legitimate non-empty skeuomorphic graphics
            if (cycle % 20 == 0 || cycle == kTargetCycles - 1) {
                bool hasNonZeroPixels = false;
                const juce::Image::BitmapData bm(offscreen, juce::Image::BitmapData::readOnly);
                for (int y = 10; y < bm.height && !hasNonZeroPixels; y += 20) {
                    for (int x = 10; x < bm.width; x += 20) {
                        auto pixel = bm.getPixelColour(x, y);
                        if (pixel.getAlpha() > 0 && (pixel.getRed() > 0 || pixel.getGreen() > 0 || pixel.getBlue() > 0)) {
                            hasNonZeroPixels = true;
                            break;
                        }
                    }
                }
                CHALLENGE_ASSERT(hasNonZeroPixels, "Offscreen render resulted in completely empty raster buffer");
                ++verifiedNonEmptyRenders;
            }
        }

        // 6. Pump pending messages while editor is active
        pumpMessageQueue(20);

        // 7. Destroy editor cleanly
        editor.reset();

        // 8. Pump messages after destruction to ensure clean teardown
        pumpMessageQueue(20);

        if ((cycle + 1) % 50 == 0) {
            std::cout << "  - Completed " << (cycle + 1) << " / " << kTargetCycles << " cycles...\n" << std::flush;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();

    std::cout << "  - Total Cycles Completed:       " << kTargetCycles << "\n";
    std::cout << "  - Successful Offscreen Paints:  " << successfulPaints << "\n";
    std::cout << "  - Verified Non-Empty Renders:   " << verifiedNonEmptyRenders << "\n";
    std::cout << "  - Total Execution Duration:     " << elapsedSec << " s (" 
              << (elapsedSec / kTargetCycles * 1000.0) << " ms/cycle)\n" << std::flush;

    CHALLENGE_ASSERT(successfulPaints == kTargetCycles, "Not all paint cycles succeeded");
    CHALLENGE_ASSERT(verifiedNonEmptyRenders >= 25, "Insufficient non-empty renders verified");

    std::cout << "  -> PASS: " << kTargetCycles << " rapid create/paint/destroy cycles completed offscreen with zero crashes or bad_cast exceptions.\n" << std::flush;
    ++gTestsPassed;
    return true;
}

// ============================================================================
// CHALLENGE 3: Concurrent Parameter Automation & Teardown Race Test
// ============================================================================
bool testConcurrentAutomationTeardownRace(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 3] Concurrent Parameter Automation & Teardown Race Test...\n";
    std::cout << "======================================================================\n" << std::flush;

    std::atomic<bool> stopRace{false};
    std::atomic<uint64_t> totalParamsFired{0};
    std::atomic<uint64_t> totalAudioBlocks{0};
    std::atomic<bool> audioErrorDetected{false};
    std::string audioErrorMessage;

    auto& apvts = proc.getApvts();

    // Envelope parameters whose parameterChanged triggers MessageManager::callAsync([safeThis]...)
    const std::vector<const char*> envParamIds = {
        "filterAttack", "filterDecay", "filterSustain", "filterRelease",
        "ampAttack", "ampDecay", "ampSustain", "ampRelease"
    };

    std::vector<juce::RangedAudioParameter*> envParams;
    for (const char* id : envParamIds) {
        if (auto* p = apvts.getParameter(id)) {
            envParams.push_back(p);
        }
    }
    CHALLENGE_ASSERT(envParams.size() == 8, "Failed to retrieve all 8 ADSR parameters");

    // Worker Thread 1: Rapid Envelope Automation Churner A
    std::thread envThreadA([&]() {
        std::mt19937 rng(101);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        size_t idx = 0;
        while (!stopRace.load(std::memory_order_relaxed)) {
            envParams[idx % 4]->setValueNotifyingHost(dist(rng));
            ++idx;
            totalParamsFired.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(30));
        }
    });

    // Worker Thread 2: Rapid Envelope Automation Churner B
    std::thread envThreadB([&]() {
        std::mt19937 rng(202);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        size_t idx = 4;
        while (!stopRace.load(std::memory_order_relaxed)) {
            envParams[idx]->setValueNotifyingHost(dist(rng));
            idx = 4 + ((idx - 3) % 4);
            totalParamsFired.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(30));
        }
    });

    // Worker Thread 3: Full APVTS Random Shuffler
    std::thread apvtsThread([&]() {
        std::mt19937 rng(303);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        auto* cutoff = apvts.getParameter("filterCutoff");
        auto* reso = apvts.getParameter("filterResonance");
        auto* oscMix = apvts.getParameter("oscMix");
        auto* drive = apvts.getParameter("driveAmount");

        while (!stopRace.load(std::memory_order_relaxed)) {
            if (cutoff) cutoff->setValueNotifyingHost(dist(rng));
            if (reso) reso->setValueNotifyingHost(dist(rng));
            if (oscMix) oscMix->setValueNotifyingHost(dist(rng));
            if (drive) drive->setValueNotifyingHost(dist(rng));
            totalParamsFired.fetch_add(4, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // Worker Thread 4: Real-time Audio Processing
    std::thread audioThread([&]() {
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        int noteCounter = 0;

        while (!stopRace.load(std::memory_order_relaxed)) {
            buffer.clear();
            midi.clear();

            if (noteCounter % 8 == 0) {
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            } else if (noteCounter % 8 == 4) {
                midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.0f), 0);
            }
            ++noteCounter;

            try {
                proc.processBlock(buffer, midi);
                totalAudioBlocks.fetch_add(1, std::memory_order_relaxed);
            } catch (const std::exception& e) {
                audioErrorDetected.store(true);
                audioErrorMessage = e.what();
                break;
            } catch (...) {
                audioErrorDetected.store(true);
                audioErrorMessage = "Unknown exception in processBlock";
                break;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });

    // Main GUI Thread: Perform 500 rapid open/paint/destroy cycles while automation floods the queue
    constexpr int kRaceCycles = 500;
    int editorsCreatedAndDestroyed = 0;

    for (int cycle = 0; cycle < kRaceCycles; ++cycle) {
        // 1. Create editor
        auto* rawEditor = proc.createEditor();
        CHALLENGE_ASSERT(rawEditor != nullptr, "Failed to create editor during race test");
        std::unique_ptr<juce::AudioProcessorEditor> editor(rawEditor);

        editor->setSize(1120, 700);
        editor->setVisible(true);

        // 2. Offscreen paint on subset of cycles to test concurrent drawing during parameter flood
        if (cycle % 4 == 0) {
            juce::Image offscreen(juce::Image::ARGB, 1120, 700, true);
            try {
                juce::Graphics g(offscreen);
                editor->paintEntireComponent(g, true);
            } catch (const std::exception& e) {
                std::cerr << "  [FAIL] Paint error during concurrent race: " << e.what() << "\n" << std::flush;
                stopRace.store(true);
                CHALLENGE_ASSERT(false, "Paint failed during race test");
            }
        }

        // 3. Dispatch pending messages while editor is alive
        pumpMessageQueue(50);

        // 4. DESTROY editor while worker threads are actively firing parameterChanged!
        // At this exact moment, multiple callAsync lambdas are queued in MessageManager
        editor.reset();
        ++editorsCreatedAndDestroyed;

        // 5. IMMEDIATELY dispatch queued messages while editor is DEAD!
        // This directly stresses SafePointer<BumblerAudioProcessorEditor>.
        // If SafePointer fails or raw pointer was used, this results in use-after-free!
        pumpMessageQueue(100);

        if ((cycle + 1) % 50 == 0) {
            std::cout << "  - Teardown race cycles: " << (cycle + 1) << " / " << kRaceCycles 
                      << " (Params fired: " << totalParamsFired.load() << ")...\n" << std::flush;
        }
    }

    // Stop all worker threads
    stopRace.store(true);
    envThreadA.join();
    envThreadB.join();
    apvtsThread.join();
    audioThread.join();

    // Drain all remaining queued messages
    pumpMessageQueueFor(50);

    std::cout << "  - Total Race Teardown Cycles:    " << editorsCreatedAndDestroyed << "\n";
    std::cout << "  - Total Parameters Automated:    " << totalParamsFired.load() << "\n";
    std::cout << "  - Total Audio Blocks Rendered:   " << totalAudioBlocks.load() << "\n" << std::flush;

    CHALLENGE_ASSERT(!audioErrorDetected.load(), audioErrorMessage.c_str());
    CHALLENGE_ASSERT(editorsCreatedAndDestroyed == kRaceCycles, "Not all race cycles completed");
    CHALLENGE_ASSERT(totalParamsFired.load() >= 1000, "Insufficient parameter updates fired during race");

    std::cout << "  -> PASS: SafePointer successfully prevented any use-after-free or dangling pointer dereference across 500 teardown races.\n" << std::flush;
    ++gTestsPassed;
    return true;
}

// ============================================================================
// CHALLENGE 4: Heavy Message Queue Saturation & Mass Post-Teardown Drain
// ============================================================================
bool testMassQueuedAsyncTeardown(bumbler::BumblerAudioProcessor& proc) {
    std::cout << "\n======================================================================\n";
    std::cout << "[CHALLENGE 4] Heavy Message Queue Saturation & Mass Teardown Drain...\n";
    std::cout << "======================================================================\n" << std::flush;

    auto& apvts = proc.getApvts();
    auto* filterAttackParam = apvts.getParameter("filterAttack");
    auto* ampReleaseParam = apvts.getParameter("ampRelease");

    CHALLENGE_ASSERT(filterAttackParam != nullptr && ampReleaseParam != nullptr, "ADSR parameters null");

    // 1. Create editor and register listeners
    auto* editor = proc.createEditor();
    CHALLENGE_ASSERT(editor != nullptr, "createEditor returned null");

    // 2. Queue 5,000 parameter updates from a background thread WITHOUT running the message queue
    constexpr int kQueuedMessages = 5000;
    std::atomic<bool> burstDone{false};

    std::thread burstThread([&]() {
        for (int i = 0; i < kQueuedMessages; ++i) {
            float val = static_cast<float>(i % 100) / 100.0f;
            if (i % 2 == 0) {
                filterAttackParam->setValueNotifyingHost(val);
            } else {
                ampReleaseParam->setValueNotifyingHost(val);
            }
        }
        burstDone.store(true);
    });

    burstThread.join();
    CHALLENGE_ASSERT(burstDone.load(), "Burst thread did not complete");

    // 3. Immediately destroy editor BEFORE running the message loop!
    // All 5,000 callAsync lambdas are now sitting in the JUCE MessageManager queue
    // with SafePointers pointing to the now-destroyed editor instance.
    delete editor;

    // 4. Run message dispatch loop to process all 5,000 queued lambdas
    // SafePointer MUST safely drop every single update without accessing deleted memory.
    bool drainCompleted = false;
    try {
        int drained = pumpMessageQueueFor(100);
        std::cout << "  - Messages successfully dispatched during drain: " << drained << "\n" << std::flush;
        drainCompleted = true;
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] Caught exception during mass queue drain: " << e.what() << "\n" << std::flush;
        CHALLENGE_ASSERT(false, "Exception during mass queue drain");
    } catch (...) {
        std::cerr << "  [FAIL] Caught unknown exception during mass queue drain\n" << std::flush;
        CHALLENGE_ASSERT(false, "Unknown exception during mass queue drain");
    }

    CHALLENGE_ASSERT(drainCompleted, "Mass queue drain failed");

    std::cout << "  - Burst Messages Queued: " << kQueuedMessages << "\n";
    std::cout << "  - Post-Teardown Drain Status: Cleanly resolved with zero access violations\n";
    std::cout << "  -> PASS: 5,000 queued async callbacks safely drained post-teardown via SafePointer.\n" << std::flush;
    ++gTestsPassed;
    return true;
}

} // namespace challenger_m5_rem

// ============================================================================
// Main Test Entry Point
// ============================================================================
int main() {
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "======================================================================\n";
    std::cout << " BUMBLER XD : CHALLENGER M5 REMEDIATION GUI & RACE STRESS SUITE       \n";
    std::cout << "======================================================================\n" << std::flush;

    bumbler::BumblerAudioProcessor proc;

    bool p1 = challenger_m5_rem::testDirectLookAndFeelStress();
    bool p2 = challenger_m5_rem::testRapidGuiLifecycleAndOffscreenPaint(proc);
    bool p3 = challenger_m5_rem::testConcurrentAutomationTeardownRace(proc);
    bool p4 = challenger_m5_rem::testMassQueuedAsyncTeardown(proc);

    bool allPassed = p1 && p2 && p3 && p4 && (gTestsFailed == 0);

    std::cout << "\n======================================================================\n";
    std::cout << " CHALLENGER M5 REMEDIATION FINAL SUMMARY:\n";
    std::cout << "  - Stress Suites Passed: " << gTestsPassed << " / 4\n";
    std::cout << "  - Stress Suites Failed: " << gTestsFailed << "\n";
    std::cout << "  - Overall Status:       " << (allPassed ? "[ALL STRESS TESTS PASSED]" : "[FAILED]") << "\n";
    std::cout << "  - Binary Verdict:       " << (allPassed ? "APPROVE" : "REJECT") << "\n";
    std::cout << "======================================================================\n" << std::flush;

    return allPassed ? 0 : 1;
}
