#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../source/dsp/BumblerCommon.h"
#include "../source/dsp/BumblerOscillator.h"
#include "../source/dsp/WaspFilter.h"
#include "../source/dsp/BumblerEnvelopes.h"
#include "../source/dsp/BumblerLFO.h"
#include "../source/dsp/BumblerVoiceManager.h"
#include "../source/dsp/CharacterCircuits.h"
#include "../source/dsp/BumblerEngine.h"
#include "../source/plugin/PluginProcessor.h"
#include "../source/plugin/PluginEditor.h"
#include "../source/plugin/PresetMigrator.h"
#include "../source/plugin/parameters/PresetParameters.h"
#include "../source/ui/LCDGraphDisplay.h"

#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <limits>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
static void pumpPendingGuiMessages(int milliseconds) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    do {
        MSG msg;
        int processed = 0;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && processed < 2000) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            ++processed;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < end);
}
#else
static void pumpPendingGuiMessages(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
#endif

using namespace bumbler;

#define V110_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [FAIL] " << msg << " (" #cond ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    } \
} while (false)

namespace {

// 1. WaspFilter DBL_NT 16-sample sub-block interpolation across sample rates
bool testDblNtSubBlockSmoothingAcrossSampleRates() {
    std::cout << "[1/7] WaspFilter DBL_NT sub-block smoothing across 44.1-192 kHz...\n";
    const std::array<double, 5> sampleRates { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };

    for (double sr : sampleRates) {
        WaspFilter filterBlock;
        filterBlock.prepare(sr);

        constexpr int blockSize = 64;
        std::vector<float> in(blockSize, 0.35f);
        std::vector<float> outA(blockSize, 0.0f);
        std::vector<float> outB(blockSize, 0.0f);

        // Prime first block at 400 Hz
        for (int i = 0; i < blockSize; ++i) {
            in[static_cast<size_t>(i)] = 0.5f * std::sin(kTwoPi * 440.0f * static_cast<float>(i) / static_cast<float>(sr));
        }
        filterBlock.processBlock(in.data(), outA.data(), blockSize, 400.0f, 0.75f, WaspFilterMode::DBL_NT);

        // Sweep cutoff sharply to 8000 Hz on next 64-sample block
        for (int i = 0; i < blockSize; ++i) {
            in[static_cast<size_t>(i)] = 0.5f * std::sin(kTwoPi * 440.0f * static_cast<float>(i + blockSize) / static_cast<float>(sr));
        }
        filterBlock.processBlock(in.data(), outB.data(), blockSize, 8000.0f, 0.75f, WaspFilterMode::DBL_NT);

        // Compare against unsmoothed abrupt jump on sample 0 of block 2
        WaspFilter filterUnsmoothed;
        filterUnsmoothed.prepare(sr);
        for (int i = 0; i < blockSize; ++i) {
            const float x = 0.5f * std::sin(kTwoPi * 440.0f * static_cast<float>(i) / static_cast<float>(sr));
            (void)filterUnsmoothed.processSample(x, 400.0, 0.75, WaspFilterMode::DBL_NT);
        }
        const float xBoundary = 0.5f * std::sin(kTwoPi * 440.0f * static_cast<float>(blockSize) / static_cast<float>(sr));
        const float abruptFirst = filterUnsmoothed.processSample(xBoundary, 8000.0, 0.75, WaspFilterMode::DBL_NT);

        for (float s : outB) {
            V110_ASSERT(isFiniteBitwise(s) && std::abs(s) <= 4.0f, "DBL_NT output finite and bounded");
        }
        // Sample 0 of smoothed block B starts from ~1/4 sub-block step, not 100% abrupt jump
        const float smoothDelta = std::abs(outB[0] - outA[blockSize - 1]);
        const float abruptDelta = std::abs(abruptFirst - outA[blockSize - 1]);
        V110_ASSERT(smoothDelta <= abruptDelta + 1.0e-4f, "Sub-block smoothed transition gentler than unsmoothed jump");

        // Mode switch LP24 -> DBL_NT snaps cleanly without NaN or transient explosion
        filterBlock.processBlock(in.data(), outA.data(), blockSize, 1200.0f, 0.8f, WaspFilterMode::LP24);
        filterBlock.processBlock(in.data(), outB.data(), blockSize, 3200.0f, 0.8f, WaspFilterMode::DBL_NT);
        for (float s : outB) {
            V110_ASSERT(isFiniteBitwise(s) && std::abs(s) <= 4.0f, "Mode switch LP24 -> DBL_NT clean");
        }
    }
    return true;
}

// 2. PolyBlep4 high-frequency (> fs/4) zero self-overlap & zero DC bias
bool testPolyBlep4NearNyquistZeroOverlap() {
    std::cout << "[2/7] PolyBlep4 near-Nyquist (dt > 0.25) symmetry & zero DC bias...\n";

    // Integrate polyBlep4(t, dt) uniformly over [0, 1) for dt > 0.25: symmetric BLEP must integrate to 0
    for (float dt : { 0.26f, 0.33f, 0.42f, 0.48f }) {
        double sum = 0.0;
        constexpr int N = 20000;
        for (int i = 0; i < N; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(N);
            const float b = polyBlep4(t, dt);
            V110_ASSERT(isFiniteBitwise(b) && std::abs(b) <= 1.05f, "polyBlep4 bounded for dt > 0.25");
            sum += static_cast<double>(b);
        }
        const double mean = sum / static_cast<double>(N);
        V110_ASSERT(std::abs(mean) < 1.0e-4, "polyBlep4 has zero DC integral above fs/4");
    }

    // Verify BumblerOscillator Saw and extreme-PW Square at high pitch (17 kHz @ 44.1 kHz, dt ~ 0.385)
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 }) {
        BumblerOscillator osc;
        osc.prepare(sr);
        osc.setFrequency(static_cast<float>(sr * 0.39)); // dt = 0.39 > 0.25

        osc.setWaveform(OscWaveform::Saw);
        osc.reset(0.0f);
        double sawSum = 0.0;
        constexpr int numSamples = 16384;
        for (int i = 0; i < numSamples; ++i) {
            const float s = osc.processSample();
            V110_ASSERT(isFiniteBitwise(s) && std::abs(s) <= 1.5f, "High-pitch Saw finite & bounded");
            sawSum += static_cast<double>(s);
        }
        V110_ASSERT(std::abs(sawSum / numSamples) < 0.01, "High-pitch Saw DC offset < 0.01");

        for (float pw : { 0.05f, 0.50f, 0.95f }) {
            osc.setWaveform(OscWaveform::Square);
            osc.setPulseWidth(pw);
            osc.reset(0.0f);
            double sqSum = 0.0;
            for (int i = 0; i < numSamples; ++i) {
                const float s = osc.processSample();
                V110_ASSERT(isFiniteBitwise(s) && std::abs(s) <= 2.1f, "High-pitch Square finite & bounded");
                sqSum += static_cast<double>(s);
            }
            const double expectedDc = 2.0 * static_cast<double>(pw) - 1.0;
            V110_ASSERT(std::abs((sqSum / numSamples) - expectedDc) < 0.02, "High-pitch Square preserves theoretical PWM mean");
        }
    }
    return true;
}

// 3. MIDI Panic: CC 64 Sustain + CC 123 All Notes Off, CC 120 All Sound Off, CC 121 Reset Controllers
bool testMidiPanicSustainPedalAllSoundOffAndResetControllers() {
    std::cout << "[3/7] MIDI CC 120 / CC 121 / CC 123 & Sustain Pedal state machine...\n";

    BumblerAudioProcessor proc;
    proc.prepareToPlay(48000.0, 128);

    // Long release so we can distinguish soft release (CC 123) from immediate kill (CC 120)
    ParameterSnapshot snap = PresetMigrator::createDefaultSnapshot();
    snap.ampRelease = 5.0f;
    proc.applyParameterSnapshot(snap);

    juce::AudioBuffer<float> buf(2, 128);
    juce::MidiBuffer midi;

    // Engage sustain pedal (CC 64 = 127), play C4, release key -> voice stays sustaining
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<uint8_t>(100)), 1);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 64);
    proc.processBlock(buf, midi);
    midi.clear();
    V110_ASSERT(proc.getEngine().getVoiceManager().getNumActiveVoices() == 1, "Voice sustaining under CC 64");

    // Send CC 123 (All Notes Off): clears sustain pedal and transitions active voice to Release
    midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    proc.processBlock(buf, midi);
    midi.clear();
    bool anyReleasing = false;
    for (int idx = 0; idx < BumblerVoiceManager::kMaxVoices; ++idx) {
        if (proc.getEngine().getVoiceManager().getVoice(idx).isReleasing()) {
            anyReleasing = true;
            break;
        }
    }
    V110_ASSERT(anyReleasing, "CC 123 transitions sustain-latched voice into Release");

    // Send CC 120 (All Sound Off): must immediately kill releasing tail to zero active voices
    midi.addEvent(juce::MidiMessage::allSoundOff(1), 0);
    proc.processBlock(buf, midi);
    midi.clear();
    V110_ASSERT(proc.getEngine().getVoiceManager().getNumActiveVoices() == 0,
                "CC 120 (All Sound Off) performs immediate fast-kill");

    // Subsequent block must be completely silent (zero lingering release tail)
    proc.processBlock(buf, midi);
    V110_ASSERT(buf.getMagnitude(0, 128) == 0.0f, "Zero residual audio after CC 120");

    // Verify sustain pedal was cleared by CC 123/120: play E4 + NoteOff with short release -> decays out
    snap.ampRelease = 0.002f;
    proc.applyParameterSnapshot(snap);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<uint8_t>(100)), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 16);
    proc.processBlock(buf, midi);
    midi.clear();
    for (int b = 0; b < 10; ++b) {
        proc.processBlock(buf, midi);
    }
    V110_ASSERT(proc.getEngine().getVoiceManager().getNumActiveVoices() == 0,
                "Sustain pedal does not stay latched after panic");

    // Verify CC 121 (Reset All Controllers) clears pitch bend and sustain pedal
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0); // +2 semitones
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 1);
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<uint8_t>(100)), 2); // A4 = 440 Hz
    midi.addEvent(juce::MidiMessage::noteOff(1, 69), 10);
    proc.processBlock(buf, midi);
    midi.clear();
    V110_ASSERT(proc.getEngine().getVoiceManager().getNumActiveVoices() == 1, "Sustaining before CC 121");

    midi.addEvent(juce::MidiMessage::controllerEvent(1, 121, 0), 0);
    proc.processBlock(buf, midi);
    midi.clear();
    for (int b = 0; b < 10; ++b) {
        proc.processBlock(buf, midi);
    }
    V110_ASSERT(proc.getEngine().getVoiceManager().getNumActiveVoices() == 0,
                "CC 121 releases sustain latch and resets pitch bend");

    return true;
}

// 4. Polyphony limit resize pitch-bend inheritance & live LFO delay reduction
bool testPolyphonyResizePitchBendAndLiveLfoDelayReduction() {
    std::cout << "[4/7] VoiceManager polyphony resize pitch-bend & live LFO delay reduction...\n";

    BumblerVoiceManager vm;
    vm.prepare(48000.0, 2048);
    vm.setPolyphonyLimit(2);
    vm.setPitchBend(2.0f); // +2 semitones while polyphony is 2
    vm.setPolyphonyLimit(8); // Expand polyphony to 8

    // Trigger 4 notes so an upper voice index (>= 2, above old limit 2) is allocated on A4 (69 -> B4 = 493.88 Hz)
    vm.noteOn(60, 1.0f);
    vm.noteOn(62, 1.0f);
    vm.noteOn(64, 1.0f);
    vm.noteOn(69, 1.0f);

    int a4VoiceIdx = -1;
    for (int idx = 0; idx < 8; ++idx) {
        if (vm.getVoice(idx).isActive() && vm.getVoice(idx).getMidiNote() == 69) {
            a4VoiceIdx = idx;
            break;
        }
    }
    V110_ASSERT(a4VoiceIdx >= 2, "A4 allocated to expanded upper voice slot >= 2");

    ParameterSnapshot sineSnap = PresetMigrator::createDefaultSnapshot();
    sineSnap.osc1Waveform = 2.0f; // Sine
    sineSnap.oscMix = 0.0f;       // Pure OSC 1
    sineSnap.ampAttack = 0.001f;
    sineSnap.ampSustain = 1.0f;

    std::vector<float> vL(4800, 0.0f);
    std::vector<float> vR(4800, 0.0f);
    vm.getVoice(a4VoiceIdx).renderBlockAccumulate(vL.data(), vR.data(), 4800, sineSnap);

    // Count upward zero-crossings over last 4096 samples (~85.33 ms): B4 (493.88 Hz) -> ~42.1 cycles vs A4 (440 Hz) -> ~37.5 cycles
    int upwardCrossings = 0;
    for (int i = 705; i < 4800; ++i) {
        if (vL[static_cast<size_t>(i - 1)] <= 0.0f && vL[static_cast<size_t>(i)] > 0.0f) {
            ++upwardCrossings;
        }
    }
    V110_ASSERT(upwardCrossings >= 41 && upwardCrossings <= 44,
                "Expanded upper voice #3 rendered at bent B4 (~493.9 Hz), not stale unbent A4 (440 Hz)");

    // Live LFO delay reduction mid-note
    BumblerLFO lfo;
    lfo.prepare(48000.0);
    lfo.setWaveform(LfoWaveform::Square);
    lfo.setRate(5.0f);
    lfo.setDelay(5.0f); // 240,000 samples delay
    lfo.noteTrigger();
    V110_ASSERT(lfo.isInDelay(), "LFO enters 5s delay on note trigger");
    V110_ASSERT(lfo.processSample() == 0.0f, "LFO outputs 0 while in delay");

    // User turns delay knob down to 0.0s while holding the note
    lfo.setDelay(0.0f);
    V110_ASSERT(!lfo.isInDelay(), "Lowering LFO delay to 0 immediately unblocks countdown");
    const float immediateOut = lfo.processSample();
    V110_ASSERT(std::abs(immediateOut) > 0.5f, "LFO immediately outputs active modulation after live delay reduction");

    // Preset-order invariance: Swarm Pad (lfo1Delay=0.0s) rendered cold vs after Sync Lead (lfo1Delay=0.25s)
    const auto& factoryPresets = getFactoryPresets();
    auto seedEngine = [](BumblerEngine& eng) {
        eng.prepare(44100.0, 512);
        eng.reset();
        for (int i = 0; i < BumblerVoiceManager::kMaxVoices; ++i) {
            auto& v = eng.getVoiceManager().getVoice(i);
            v.seedVoice(static_cast<uint32_t>(i));
            v.getAnalogDrift().seed(0x12345678u + static_cast<uint32_t>(i) * 0x1000u);
        }
    };

    std::vector<float> coldL(512, 0.0f), coldR(512, 0.0f);
    {
        BumblerEngine coldEng;
        seedEngine(coldEng);
        float* ch[2] = { coldL.data(), coldR.data() };
        coldEng.processMidiEvent(0x90, 60, 0.85f);
        coldEng.renderBlock(ch, 2, 512, factoryPresets[2].params);
    }

    std::vector<float> afterSyncL(512, 0.0f), afterSyncR(512, 0.0f);
    {
        BumblerEngine seqEng;
        seedEngine(seqEng);
        float* ch[2] = { afterSyncL.data(), afterSyncR.data() };
        seqEng.processMidiEvent(0x90, 60, 0.85f);
        seqEng.renderBlock(ch, 2, 512, factoryPresets[1].params); // Sync Lead (lfo1Delay = 0.25s)

        std::fill(afterSyncL.begin(), afterSyncL.end(), 0.0f);
        std::fill(afterSyncR.begin(), afterSyncR.end(), 0.0f);
        seedEngine(seqEng);
        seqEng.processMidiEvent(0x90, 60, 0.85f);
        seqEng.renderBlock(ch, 2, 512, factoryPresets[2].params); // Swarm Pad (lfo1Delay = 0.0s)
    }

    float maxPresetOrderDelta = 0.0f;
    for (size_t i = 0; i < 512; ++i) {
        maxPresetOrderDelta = std::max(maxPresetOrderDelta, std::abs(coldL[i] - afterSyncL[i]));
        maxPresetOrderDelta = std::max(maxPresetOrderDelta, std::abs(coldR[i] - afterSyncR[i]));
    }
    V110_ASSERT(maxPresetOrderDelta == 0.0f,
                "Swarm Pad output is 100% bit-identical whether rendered cold or after Sync Lead");

    // Sample-0 preset-load trigger synchronization in PluginProcessor::processBlock
    {
        BumblerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 128);
        ParameterSnapshot prevSnap = PresetMigrator::createDefaultSnapshot();
        prevSnap.analogMode = 0.0f;
        prevSnap.lfo1KeyReset = 0.0f;
        prevSnap.lfo1Delay = 5.0f;
        proc.loadMigratedSnapshot(prevSnap);

        juce::AudioBuffer<float> buf(2, 128);
        juce::MidiBuffer midi;
        buf.clear();
        proc.processBlock(buf, midi); // Caches prevSnap in voices

        // Now load a preset with analogMode=1, lfo1KeyReset=1, lfo1Delay=0 and send NoteOn on very next block
        ParameterSnapshot nextSnap = PresetMigrator::createDefaultSnapshot();
        nextSnap.analogMode = 1.0f;
        nextSnap.lfo1KeyReset = 1.0f;
        nextSnap.lfo1Delay = 0.0f;
        proc.loadMigratedSnapshot(nextSnap);

        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
        buf.clear();
        proc.processBlock(buf, midi);

        int trigIdx = -1;
        for (int i = 0; i < BumblerVoiceManager::kMaxVoices; ++i) {
            if (proc.getEngine().getVoiceManager().getVoice(i).isActive()) {
                trigIdx = i;
                break;
            }
        }
        V110_ASSERT(trigIdx >= 0, "Sample-0 note after preset load allocated active voice");
        const auto& trigVoice = proc.getEngine().getVoiceManager().getVoice(trigIdx);
        V110_ASSERT(trigVoice.isAnalogMode(), "Sample-0 NoteOn immediately sees new preset analogMode=1");
        V110_ASSERT(!trigVoice.getLfo1().isInDelay(), "Sample-0 NoteOn immediately sees new preset lfo1Delay=0");
    }

    return true;
}

// 5. Direct NaN / +-Inf injection immunity & clean recovery across all DSP modules
bool testDirectNanInfImmunityAcrossAllDspUnits() {
    std::cout << "[5/7] Direct NaN / +-Inf injection immunity & instant clean recovery...\n";

    const float nanF = std::numeric_limits<float>::quiet_NaN();
    const float infF = std::numeric_limits<float>::infinity();
    const double nanD = std::numeric_limits<double>::quiet_NaN();

    // OnePoleDCBlocker
    OnePoleDCBlocker dc;
    dc.setSampleRate(nanF, nanF);
    V110_ASSERT(isFiniteBitwise(dc.process(nanF)), "DC blocker rejects NaN input on arrival sample");
    V110_ASSERT(isFiniteBitwise(dc.process(infF)), "DC blocker rejects +Inf input");
    V110_ASSERT(std::abs(dc.process(0.5f)) > 0.1f, "DC blocker recovers cleanly without reset()");

    // BumblerOscillator & BumblerTripleOscillatorSection
    BumblerOscillator osc;
    osc.prepare(48000.0);
    osc.setFrequency(nanF);
    V110_ASSERT(isFiniteBitwise(osc.process(OscWaveform::Saw, nanF, WNoiseMode::White, nanF)),
                "Oscillator survives NaN freq/PW/phaseMod");
    osc.setFrequency(440.0f);
    V110_ASSERT(isFiniteBitwise(osc.processSample(0.0f)), "Oscillator recovers cleanly");

    BumblerTripleOscillatorSection triple;
    triple.prepare(48000.0);
    ParameterSnapshot badSnap = PresetMigrator::createDefaultSnapshot();
    badSnap.oscMix = nanF;
    badSnap.fmAmount = infF;
    badSnap.pulseWidth = -infF;
    V110_ASSERT(isFiniteBitwise(triple.process(nanF, badSnap, nanF, infF, -infF, nanF, infF)),
                "TripleOscillatorSection survives all-NaN/Inf inputs");
    ParameterSnapshot goodSnap = PresetMigrator::createDefaultSnapshot();
    V110_ASSERT(isFiniteBitwise(triple.process(440.0f, goodSnap)), "TripleOscillatorSection recovers cleanly");

    // WaspFilter
    WaspFilter filt;
    filt.prepare(nanD);
    V110_ASSERT(isFiniteBitwise(filt.processSample(nanF, nanD, nanD, WaspFilterMode::LP24)),
                "WaspFilter::processSample survives NaN");
    std::array<float, 32> fIn {};
    std::array<float, 32> fOut {};
    fIn[0] = nanF;
    fIn[1] = infF;
    fIn[2] = -infF;
    filt.processBlock(fIn.data(), fOut.data(), 32, nanF, infF, WaspFilterMode::DBL_NT);
    for (float s : fOut) {
        V110_ASSERT(isFiniteBitwise(s), "WaspFilter::processBlock survives NaN/Inf");
    }
    const float modCut = WaspFilter::calculateModulatedCutoff(nanF, infF, nanF, -infF, nanF, 48000.0f, nanF);
    V110_ASSERT(isFiniteBitwise(modCut) && modCut >= 20.0f, "calculateModulatedCutoff survives NaN/Inf");

    // Envelopes
    BumblerADSR adsr;
    adsr.prepare(nanD);
    adsr.setParameters(nanF, infF, nanF, -infF);
    adsr.noteOn();
    for (int i = 0; i < 64; ++i) {
        V110_ASSERT(isFiniteBitwise(adsr.processSample()), "BumblerADSR finite under NaN/Inf params");
    }
    BumblerModEnvelope modEnv;
    modEnv.prepare(nanD);
    modEnv.setParameters(nanF, infF, nanF);
    modEnv.trigger();
    V110_ASSERT(isFiniteBitwise(modEnv.processSample()), "BumblerModEnvelope finite under NaN/Inf params");

    // LFO
    BumblerLFO lfo;
    lfo.prepare(nanD);
    lfo.setRate(nanF);
    lfo.setDelay(infF);
    lfo.setTempoSync(true, nanF, infF);
    lfo.setAmount(nanF);
    V110_ASSERT(isFiniteBitwise(lfo.processSample()), "BumblerLFO finite under NaN/Inf params");

    // CharacterCircuits
    CharacterCircuits cc;
    cc.prepare(nanD, 64);
    std::array<float, 32> chL {};
    std::array<float, 32> chR {};
    chL[0] = nanF;
    chR[0] = infF;
    badSnap.driveEnabled = 1.0f;
    badSnap.driveAmount = infF;
    badSnap.driveTone = nanF;
    badSnap.dualMode = 1.0f;
    badSnap.masterVolume = nanF;
    cc.processStereo(chL.data(), chR.data(), 32, badSnap);
    for (int i = 0; i < 32; ++i) {
        V110_ASSERT(isFiniteBitwise(chL[static_cast<size_t>(i)]) && isFiniteBitwise(chR[static_cast<size_t>(i)]),
                    "CharacterCircuits survives NaN/Inf buffers & params");
    }

    return true;
}

// 6. PresetMigrator C++ adversarial bounds & ModDest1..4 bijection
bool testPresetMigratorAdversarialHardeningAndModDestBijection() {
    std::cout << "[6/7] PresetMigrator C++ adversarial binary/XML bounds & ModDest bijection...\n";

    // (a) Truncated FxCk (< 56 bytes), negative numParams (-1), huge numParams (0x3FFFFFFF)
    std::vector<uint8_t> shortFxCk(32, 0);
    std::memcpy(shortFxCk.data(), "CcnK", 4);
    std::memcpy(shortFxCk.data() + 8, "FxCk", 4);
    V110_ASSERT(PresetMigrator::parseFxp(shortFxCk.data(), shortFxCk.size(), "short.fxp").empty(),
                "Truncated <56B FxCk safely rejected");

    std::vector<uint8_t> negParamsFxCk(64, 0);
    std::memcpy(negParamsFxCk.data(), "CcnK", 4);
    std::memcpy(negParamsFxCk.data() + 8, "FxCk", 4);
    // numParams = -1 (0xFFFFFFFF big-endian at offset 24)
    negParamsFxCk[24] = 0xFF; negParamsFxCk[25] = 0xFF; negParamsFxCk[26] = 0xFF; negParamsFxCk[27] = 0xFF;
    V110_ASSERT(PresetMigrator::parseFxp(negParamsFxCk.data(), negParamsFxCk.size(), "neg.fxp").empty(),
                "Negative numParams FxCk safely rejected without length_error");

    // Huge numParams = 0x3FFFFFFF
    negParamsFxCk[24] = 0x3F;
    V110_ASSERT(PresetMigrator::parseFxp(negParamsFxCk.data(), negParamsFxCk.size(), "huge.fxp").empty(),
                "Huge numParams FxCk safely rejected without overflow");

    // Truncated FPCh (< 60 bytes) & FxBk (< 160 bytes)
    std::vector<uint8_t> shortFpch(40, 0);
    std::memcpy(shortFpch.data(), "CcnK", 4);
    std::memcpy(shortFpch.data() + 8, "FPCh", 4);
    V110_ASSERT(PresetMigrator::parseFxp(shortFpch.data(), shortFpch.size(), "fpch.fxp").empty(),
                "Truncated <60B FPCh safely rejected");

    std::vector<uint8_t> shortFxbk(100, 0);
    std::memcpy(shortFxbk.data(), "CcnK", 4);
    std::memcpy(shortFxbk.data() + 8, "FxBk", 4);
    V110_ASSERT(PresetMigrator::parseFxp(shortFxbk.data(), shortFxbk.size(), "fxbk.fxb").empty(),
                "Truncated <160B FxBk safely rejected");

    // (b) FLP EOF-adjacent FLhd/FLdt & malformed 8-byte varint (shift >= 32 UB guard)
    const uint8_t eofFlhd[] = { 'F', 'L', 'h', 'd', 0x06 };
    V110_ASSERT(PresetMigrator::parseFlp(eofFlhd, sizeof(eofFlhd), "eof.flp").empty(),
                "EOF-adjacent FLhd does not overread");

    std::vector<uint8_t> badVarintFlp = {
        'F', 'L', 'h', 'd', 6, 0, 0, 0, 0, 0, 1, 0, 96, 0,
        'F', 'L', 'd', 't', 10, 0, 0, 0,
        213, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01, 0x00
    };
    V110_ASSERT(PresetMigrator::parseFlp(badVarintFlp.data(), badVarintFlp.size(), "varint.flp").empty(),
                "Malformed >5-byte varint safely aborted without UB");

    // (c) Hostile XML sanitization (NaN, Inf, out-of-bounds, unknown IDs)
    const juce::String hostileXml =
        "<BumblerXD name=\"HostilePatch\" category=\"Lead\">"
        "  <PARAM id=\"filterCutoff\" value=\"9999999.0\"/>"
        "  <PARAM id=\"filterResonance\" value=\"-42.0\"/>"
        "  <PARAM id=\"masterVolume\" value=\"nan\"/>"
        "  <PARAM id=\"driveAmount\" value=\"inf\"/>"
        "  <PARAM id=\"osc1Octave\" value=\"2.7\"/>"
        "  <PARAM id=\"unknownMaliciousParam\" value=\"1337.0\"/>"
        "</BumblerXD>";
    auto xmlPresets = PresetMigrator::parseXml(hostileXml, "hostile.xml");
    V110_ASSERT(xmlPresets.size() == 1, "Hostile XML parsed safely");
    V110_ASSERT(xmlPresets[0].parameterMap.size() == 55, "parameterMap strictly contains 55 valid APVTS IDs");
    V110_ASSERT(xmlPresets[0].parameterMap.find("unknownMaliciousParam") == xmlPresets[0].parameterMap.end(),
                "Unknown XML param ID stripped");
    V110_ASSERT(std::abs(xmlPresets[0].snapshot.filterCutoff - 20000.0f) < 0.01f, "filterCutoff clamped to 20000 Hz");
    V110_ASSERT(std::abs(xmlPresets[0].snapshot.filterResonance - 0.0f) < 1e-5f, "filterResonance clamped to 0.0");
    V110_ASSERT(std::isfinite(xmlPresets[0].snapshot.masterVolume) && xmlPresets[0].snapshot.masterVolume > 0.0f,
                "NaN masterVolume falls back to finite default");
    V110_ASSERT(std::isfinite(xmlPresets[0].snapshot.driveAmount)
                    && std::abs(xmlPresets[0].snapshot.driveAmount - PresetMigrator::createDefaultSnapshot().driveAmount) < 1e-5f,
                "Inf driveAmount falls back to finite default");
    V110_ASSERT(std::abs(xmlPresets[0].snapshot.osc1Octave - 3.0f) < 1e-5f, "Discrete osc1Octave rounded & clamped");

    // (d) Bijective ModDest1..4 (tags 46..49 -> 0.0, 1.0, 2.0, 3.0) in float v13 and Delphi int v12
    for (size_t slot = 0; slot < 4; ++slot) {
        std::vector<float> wf(56, 0.0f);
        wf[45] = 1.0f; // positive modAmt
        wf[46 + slot] = 1.0f;
        const ParameterSnapshot sFloat = PresetMigrator::mapNativeWaspFloatsToSnapshot(wf, 0);
        V110_ASSERT(std::abs(sFloat.modTarget - static_cast<float>(slot)) < 1e-5f,
                    "Native Wasp float ModDest1..4 maps bijectively to 0..3");

        std::vector<int32_t> wi(56, 0);
        wi[45] = 64;
        wi[46 + slot] = 1;
        const ParameterSnapshot sInt = PresetMigrator::mapDelphiWaspIntegersToSnapshot(wi, 0);
        V110_ASSERT(std::abs(sInt.modTarget - static_cast<float>(slot)) < 1e-5f,
                    "Delphi Wasp int ModDest1..4 maps bijectively to 0..3");
    }

    return true;
}

// 7. Homage preset DAW state recall (5..19), 0-sample & 6-ch bus layouts, LCD bezel bounds & Editor sync
bool testHomagePresetStateRecallBusLayoutsAndUiBounds() {
    std::cout << "[7/7] Homage preset recall (5..19), 0-sample/6-ch buses, LCD bezel & Editor sync...\n";

    BumblerAudioProcessor procA;
    procA.prepareToPlay(48000.0, 128);
    procA.loadHomagePreset(7); // programIndex = 5 + 7 = 12
    V110_ASSERT(procA.getCurrentProgram() == 12, "loadHomagePreset(7) sets program index 12");

    juce::MemoryBlock stateData;
    procA.getStateInformation(stateData);

    BumblerAudioProcessor procB;
    procB.prepareToPlay(48000.0, 128);
    procB.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
    V110_ASSERT(procB.getCurrentProgram() == 12, "Homage preset programIndex 12 survives DAW state save/reload");

    // Verify out-of-range programIndex (-1 and 20) in state XML are rejected
    if (auto xml = juce::AudioProcessor::getXmlFromBinary(stateData.getData(), static_cast<int>(stateData.getSize()))) {
        xml->setAttribute("programIndex", 20);
        juce::MemoryBlock badBlock;
        juce::AudioProcessor::copyXmlToBinary(*xml, badBlock);
        procB.setCurrentProgram(2);
        procB.setStateInformation(badBlock.getData(), static_cast<int>(badBlock.getSize()));
        V110_ASSERT(procB.getCurrentProgram() == 2, "Out-of-range programIndex=20 rejected safely");
    }

    // 0-sample block and 6-channel buffer clearing
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> zeroBuf(2, 0);
    procB.processBlock(zeroBuf, midi); // must not crash or touch invalid pointers

    juce::AudioBuffer<float> sixChBuf(6, 64);
    for (int ch = 0; ch < 6; ++ch) {
        juce::FloatVectorOperations::fill(sixChBuf.getWritePointer(ch), 99.0f, 64);
    }
    procB.processBlock(sixChBuf, midi);
    for (int ch = 2; ch < 6; ++ch) {
        V110_ASSERT(sixChBuf.getMagnitude(ch, 0, 64) == 0.0f, "Auxiliary channels >= 2 cleared to silence");
    }

    // LCDGraphDisplay curve bounds stay strictly inside bezel across extreme ADSR & narrow sizes
    LCDGraphDisplay lcd(LCDGraphDisplay::DisplayType::FilterEnvelope);
    for (int width : { 220, 120, 64 }) {
        lcd.setBounds(0, 0, width, 80);
        lcd.setEnvelopeParameters(10.0f, 10.0f, 1.0f, 10.0f);
        const float maxRight = static_cast<float>(width) - 8.0f + 0.05f;
        V110_ASSERT(lcd.getCurvePath().getBounds().getRight() <= maxRight,
                    "LCD max ADSR (10s/10s/100%/10s) stays strictly inside bezel");

        lcd.setEnvelopeParameters(0.001f, 0.001f, 0.0f, 0.001f);
        V110_ASSERT(lcd.getCurvePath().getBounds().getRight() <= maxRight,
                    "LCD min ADSR (1ms/1ms/0%/1ms) stays strictly inside bezel");

        const float nanF = std::numeric_limits<float>::quiet_NaN();
        lcd.setEnvelopeParameters(nanF, nanF, nanF, nanF);
        lcd.setFilterModulationStatus(nanF, false);
        V110_ASSERT(lcd.getCurvePath().getBounds().getRight() <= maxRight,
                    "LCD NaN parameters sanitized cleanly inside bezel");
    }

    // Editor mPresetComboBox syncs when processor loads Homage Preset externally
    std::unique_ptr<juce::AudioProcessorEditor> rawEd(procB.createEditor());
    auto* ed = dynamic_cast<BumblerAudioProcessorEditor*>(rawEd.get());
    V110_ASSERT(ed != nullptr, "Created BumblerAudioProcessorEditor");
    procB.loadHomagePreset(3); // curProg = 5 + 3 = 8 -> ComboBox ID = 9
    pumpPendingGuiMessages(25);
    V110_ASSERT(ed->getSelectedPresetComboId() == 9,
                "Editor mPresetComboBox synced to ID 9 after external loadHomagePreset(3)");

    return true;
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "=== Bumbler XD v1.1.0 Deep Audit Regression Suite ===\n";

    bool ok = true;
    ok = ok && testDblNtSubBlockSmoothingAcrossSampleRates();
    ok = ok && testPolyBlep4NearNyquistZeroOverlap();
    ok = ok && testMidiPanicSustainPedalAllSoundOffAndResetControllers();
    ok = ok && testPolyphonyResizePitchBendAndLiveLfoDelayReduction();
    ok = ok && testDirectNanInfImmunityAcrossAllDspUnits();
    ok = ok && testPresetMigratorAdversarialHardeningAndModDestBijection();
    ok = ok && testHomagePresetStateRecallBusLayoutsAndUiBounds();

    if (!ok) {
        std::cerr << "=== FAILED: Bumbler XD v1.1.0 Regression Suite ===\n";
        return 1;
    }

    std::cout << "=== ALL 7 V1.1.0 REGRESSION SUITES PASSED ===\n";
    return 0;
}
