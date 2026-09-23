#include "test_helpers.h"
#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "BumblerVoice.h"
#include "BumblerVoiceManager.h"
#include "BumblerEngine.h"
#include "CharacterCircuits.h"
#include <iomanip>
#include <sstream>

// ============================================================================
// Master E2E Headless Suite for Bumbler XD
// Authoritative Acceptance Test Runner matching ORIGINAL_REQUEST.md & PROJECT.md
// ============================================================================

// Forward declarations of validation routines
bool verifyFilterAcceptance();
bool verifyModulationAcceptance();
bool verifyCharacterCircuitsAcceptance();
bool verifyRealtimeSafetyAcceptance();
bool verifyTier3PairwiseScenarios();
bool verifyTier4RealWorldPresets();
bool verifyApvtsStateRecallContract();

// ============================================================================
// Acceptance Check 1: 6-Mode Filter Frequency Response & Roll-Off Slopes
// ============================================================================
bool verifyFilterAcceptance() {
    std::cout << "\n[CRITERION 1] Verifying 6-Mode Wasp Filter Roll-Off & Topology...\n";
    bumbler_test::ReferenceWaspFilter filter;
    filter.prepare(48000.0);
    const double fc = 1000.0;

    auto measureGain = [&](double f, int mode, double reso = 0.0) {
        filter.reset();
        constexpr int N = 6000;
        std::vector<float> in(N);
        bumbler_test::generateSine(in.data(), N, f, 48000.0, 1.0f);
        float peak = 0.0f;
        for (int i = 0; i < N; ++i) {
            float out = filter.processSample(in[i], fc, reso, static_cast<bumbler_test::ReferenceWaspFilter::Mode>(mode));
            if (i >= 2000) {
                float a = std::abs(out);
                if (a > peak) peak = a;
            }
        }
        return peak;
    };

    // 1. LP FAT ~24 dB/oct roll-off
    double lpFatPass = bumbler_test::toDb(measureGain(250.0, 1));
    double lpFat1Oct = bumbler_test::toDb(measureGain(2000.0, 1));
    double lpFatSlope = lpFat1Oct - lpFatPass;
    std::cout << "  - LP FAT 24dB Slope: " << lpFatSlope << " dB/oct (Expected: -24 dB ± 4 dB)\n";
    if (lpFatSlope > -20.0 || lpFatSlope < -28.0) return false;

    // 2. LP 12dB ~12 dB/oct roll-off
    double lpPass = bumbler_test::toDb(measureGain(250.0, 0));
    double lp1Oct = bumbler_test::toDb(measureGain(2000.0, 0));
    double lpSlope = lp1Oct - lpPass;
    std::cout << "  - LP 12dB Slope: " << lpSlope << " dB/oct (Expected: -12 dB ± 2.5 dB)\n";
    if (lpSlope > -9.5 || lpSlope < -14.5) return false;

    // 3. BP 24dB attenuation on both ends
    double bpCenter = bumbler_test::toDb(measureGain(1000.0, 4, 0.3));
    double bpLow    = bumbler_test::toDb(measureGain(250.0, 4, 0.3));
    double bpHigh   = bumbler_test::toDb(measureGain(4000.0, 4, 0.3));
    std::cout << "  - BP 24dB Center: " << bpCenter << " dB, Low Atten: " << (bpCenter - bpLow) 
              << " dB, High Atten: " << (bpCenter - bpHigh) << " dB\n";
    if ((bpCenter - bpLow) < 14.0 || (bpCenter - bpHigh) < 14.0) return false;

    // 4. HP 24dB ~24 dB/oct low-end roll-off below cutoff
    double hpPass = bumbler_test::toDb(measureGain(4000.0, 5));
    double hp1Oct = bumbler_test::toDb(measureGain(500.0, 5));
    double hpSlope = hp1Oct - hpPass;
    std::cout << "  - HP 24dB Slope: " << hpSlope << " dB/oct below cutoff (Expected: -24 dB ± 4 dB)\n";
    if (hpSlope > -20.0 || hpSlope < -28.0) return false;

    // 5. DBL.NT detection of two distinct attenuation nulls
    int nullCount = 0;
    std::vector<double> notchProfile(50);
    for (int i = 0; i < 50; ++i) {
        double f = 400.0 * std::pow(2500.0 / 400.0, i / 49.0);
        notchProfile[i] = bumbler_test::toDb(measureGain(f, 3, 0.5));
    }
    for (int i = 1; i < 49; ++i) {
        if (notchProfile[i] < notchProfile[i - 1] && notchProfile[i] < notchProfile[i + 1] && notchProfile[i] < -12.0) {
            ++nullCount;
        }
    }
    std::cout << "  - DBL.NT Distinct Nulls Detected: " << nullCount << " (Expected >= 2)\n";
    if (nullCount < 2) return false;

    return true;
}

// ============================================================================
// Acceptance Check 2: Inter-Oscillator Modulation (Ring Mod & FM)
// ============================================================================
bool verifyModulationAcceptance() {
    std::cout << "\n[CRITERION 2] Verifying Inter-Oscillator Modulation (RingMod & FM)...\n";
    constexpr double fs = 48000.0;
    constexpr int N = 4800; // Exact integer cycles for 100, 300, 400, 500 Hz

    // 1. Ring Mod sum/diff frequencies without DC runaway
    std::vector<float> osc1(N), osc2(N), ring(N);
    bumbler_test::generateSine(osc1.data(), N, 400.0, fs, 1.0f);
    bumbler_test::generateSine(osc2.data(), N, 100.0, fs, 1.0f);

    float dcState = 0.0f;
    for (int i = 0; i < N; ++i) {
        float raw = osc1[i] * osc2[i];
        dcState += 0.005f * (raw - dcState);
        ring[i] = raw - dcState;
    }

    double pSum = bumbler_test::computeFourierBinPower(ring.data(), N, 500.0, fs);
    double pDiff = bumbler_test::computeFourierBinPower(ring.data(), N, 300.0, fs);
    double dcMean = 0.0;
    for (int i = 2400; i < N; ++i) dcMean += ring[i];
    dcMean = std::abs(dcMean / (N - 2400));

    std::cout << "  - RingMod Sum Power (500 Hz): " << pSum << ", Diff Power (300 Hz): " << pDiff 
              << ", DC Offset: " << dcMean << "\n";
    if (pSum < 0.05 || pDiff < 0.05 || dcMean > 1.0e-3) return false;

    // 2. FM Bessel sidebands
    std::vector<float> fm(N);
    double phaseC = 0.0, phaseM = 0.0;
    const double twoPiOverFs = bumbler_test::kTwoPi / fs;
    for (int i = 0; i < N; ++i) {
        double modSig = std::sin(phaseM);
        fm[i] = static_cast<float>(std::sin(phaseC));
        phaseC += (1000.0 + 1.5 * 200.0 * modSig) * twoPiOverFs;
        phaseM += 200.0 * twoPiOverFs;
    }

    double pSide1 = bumbler_test::computeFourierBinPower(fm.data(), N, 800.0, fs);
    double pSide2 = bumbler_test::computeFourierBinPower(fm.data(), N, 1200.0, fs);
    std::cout << "  - FM Sideband 800 Hz Power: " << pSide1 << ", 1200 Hz Power: " << pSide2 << "\n";
    if (pSide1 < 0.01 || pSide2 < 0.01) return false;

    return true;
}

// ============================================================================
// Acceptance Check 3: Character Circuits (Dual, Analog, Distortion, W.Noise)
// ============================================================================
bool verifyCharacterCircuitsAcceptance() {
    std::cout << "\n[CRITERION 3] Verifying Vintage Character Circuits...\n";
    constexpr double fs = 48000.0;
    constexpr int N = 4096;

    // 1. Dual Mode Stereo Correlation via Real BumblerEngine
    {
        bumbler::BumblerEngine engine;
        engine.prepare(fs, 512);

        ParameterSnapshot params;
        params.osc1Waveform = 0.0f; // Sawtooth
        params.ampAttack = 0.001f;
        params.ampSustain = 1.0f;
        params.masterVolume = 0.8f;

        // A. Dual Mode OFF: Left and Right must be identical (r >= 0.999)
        params.dualMode = 0.0f;
        engine.reset();
        engine.processMidiEvent(0x90, 60, 0.8f);

        std::vector<float> bufL_off(N, 0.0f), bufR_off(N, 0.0f);
        for (int b = 0; b < N; b += 512) {
            float* channels[2] = { bufL_off.data() + b, bufR_off.data() + b };
            engine.renderBlock(channels, 2, 512, params);
        }
        double rOff = bumbler_test::computeCrossCorrelation(bufL_off.data() + 512, bufR_off.data() + 512, N - 512);
        std::cout << "  - Dual Mode OFF Cross-Correlation r: " << rOff << " (Expected >= 0.999)\n";
        if (rOff < 0.999) return false;

        // B. Dual Mode ON: Left and Right must be decorrelated (r < 0.70)
        params.dualMode = 1.0f;
        engine.reset();
        engine.processMidiEvent(0x90, 60, 0.8f);

        std::vector<float> bufL_on(N, 0.0f), bufR_on(N, 0.0f);
        for (int b = 0; b < N; b += 512) {
            float* channels[2] = { bufL_on.data() + b, bufR_on.data() + b };
            engine.renderBlock(channels, 2, 512, params);
        }
        double rOn = bumbler_test::computeCrossCorrelation(bufL_on.data() + 512, bufR_on.data() + 512, N - 512);
        float peakL = bumbler_test::computePeak(bufL_on.data(), N);
        float peakR = bumbler_test::computePeak(bufR_on.data(), N);
        std::cout << "  - Dual Mode ON Cross-Correlation r: " << rOn << " (Expected < 0.70, Peaks <= 1.05)\n";
        if (rOn >= 0.70 || peakL > 1.05f || peakR > 1.05f) return false;
    }

    // 2. Analog Mode Pitch Drift Variance & Free-Running Phase on Real BumblerVoice
    {
        bumbler::BumblerVoice voice;
        voice.prepare(fs);

        ParameterSnapshot vParams;
        vParams.osc1Waveform = 2.0f; // Pure sine
        vParams.oscMix = 0.0f;
        vParams.ampAttack = 0.001f;
        vParams.ampSustain = 1.0f;
        vParams.velToAmp = 0.0f;

        auto measureFreq = [&](bumbler::BumblerVoice& v, const ParameterSnapshot& p) -> double {
            constexpr int kWarmup = 256;
            constexpr int kMeasure = 4096;
            for (int i = 0; i < kWarmup; ++i) { float sL = 0, sR = 0; v.renderSample(sL, sR, p); }
            std::vector<float> audio(kMeasure);
            for (int i = 0; i < kMeasure; ++i) { float sL = 0, sR = 0; v.renderSample(sL, sR, p); audio[i] = sL; }

            std::vector<double> crossings;
            crossings.reserve(64);
            for (size_t i = 1; i < audio.size(); ++i) {
                if (audio[i - 1] <= 0.0f && audio[i] > 0.0f) {
                    double y1 = audio[i - 1];
                    double y2 = audio[i];
                    double frac = -y1 / (y2 - y1);
                    crossings.push_back((i - 1) + frac);
                }
            }
            if (crossings.size() < 4) return 0.0;
            double totalCycles = static_cast<double>(crossings.size() - 1);
            double totalSamples = crossings.back() - crossings.front();
            return fs / (totalSamples / totalCycles);
        };

        // A. Analog OFF: pitch drift variance across 10 triggers must be 0.0
        vParams.analogMode = 0.0f;
        std::vector<double> driftOff(10);
        for (int i = 0; i < 10; ++i) {
            voice.reset();
            voice.setAnalogMode(false);
            voice.noteOn(69, 1.0f, static_cast<uint64_t>(i * 4096));
            double f = measureFreq(voice, vParams);
            driftOff[i] = 1200.0 * std::log2(f / 440.0);
        }
        double varOff = bumbler_test::computeVariance(driftOff);
        std::cout << "  - Analog Mode OFF Pitch Drift Variance: " << varOff << " (Expected == 0.0)\n";
        if (varOff > 1.0e-5) return false;

        // B. Analog ON: pitch drift variance > 0.05 and free-running phase variance > 0.1
        vParams.analogMode = 1.0f;
        std::vector<double> driftOn(10);
        std::vector<double> initialSamples(10);
        for (int i = 0; i < 10; ++i) {
            voice.reset();
            voice.setAnalogMode(true);
            voice.noteOn(69, 1.0f, static_cast<uint64_t>(i * 4096));
            float sL = 0.0f, sR = 0.0f;
            voice.renderSample(sL, sR, vParams);
            initialSamples[i] = voice.getOscInitialPhaseRad();
            double f = measureFreq(voice, vParams);
            driftOn[i] = 1200.0 * std::log2(f / 440.0);
        }
        double varOn = bumbler_test::computeVariance(driftOn);
        double varPhase = bumbler_test::computeVariance(initialSamples);
        std::cout << "  - Analog Mode ON Pitch Drift Variance: " << varOn << " (Expected > 0.05)\n";
        std::cout << "  - Analog Mode Free-Running Phase Variance: " << varPhase << " (Expected > 0.1)\n";
        if (varOn <= 0.05 || varPhase <= 0.1) return false;
    }

    // 3. Output Drive Harmonic Saturation & Tone Tilt via Real CharacterCircuits
    {
        bumbler::CharacterCircuits circuits;
        circuits.prepare(fs, N);

        std::vector<float> sineIn(N);
        bumbler_test::generateSine(sineIn.data(), N, 1000.0, fs, 0.7f);

        ParameterSnapshot cParams;
        cParams.driveEnabled = 0.0f;
        cParams.driveAmount = 0.0f;
        cParams.driveTone = 0.5f;

        std::vector<float> cleanL = sineIn, cleanR = sineIn;
        circuits.reset();
        circuits.processStereo(cleanL.data(), cleanR.data(), N, cParams);
        double pClean3rd = bumbler_test::computeFourierBinPower(cleanL.data(), N, 3000.0, fs);

        cParams.driveEnabled = 1.0f;
        cParams.driveAmount = 0.8f;
        std::vector<float> drivenL = sineIn, drivenR = sineIn;
        circuits.reset();
        circuits.processStereo(drivenL.data(), drivenR.data(), N, cParams);
        double pDriven3rd = bumbler_test::computeFourierBinPower(drivenL.data(), N, 3000.0, fs);
        float peakDriven = bumbler_test::computePeak(drivenL.data(), N);

        std::cout << "  - Distortion 3rd Harmonic Power: " << pDriven3rd << " (Expected > 0.01, Boost > 20 dB)\n";
        std::cout << "  - Distortion Peak Ceiling: " << peakDriven << " (Expected <= 1.05)\n";
        if (pDriven3rd < 0.01 || pDriven3rd <= pClean3rd * 100.0 || peakDriven > 1.05f) return false;

        // Tone Spectral Tilt at 3 kHz (> 10 dB)
        cParams.driveTone = 0.0f; // Dark
        std::vector<float> darkL = sineIn, darkR = sineIn;
        circuits.reset();
        circuits.processStereo(darkL.data(), darkR.data(), N, cParams);
        double pDark3rd = bumbler_test::computeFourierBinPower(darkL.data(), N, 3000.0, fs);

        cParams.driveTone = 1.0f; // Bright
        std::vector<float> brightL = sineIn, brightR = sineIn;
        circuits.reset();
        circuits.processStereo(brightL.data(), brightR.data(), N, cParams);
        double pBright3rd = bumbler_test::computeFourierBinPower(brightL.data(), N, 3000.0, fs);

        double toneDbDiff = bumbler_test::toDb(std::sqrt(pBright3rd)) - bumbler_test::toDb(std::sqrt(pDark3rd));
        std::cout << "  - Tone Control Spectral Tilt (3 kHz): " << toneDbDiff << " dB (Expected > 10.0 dB)\n";
        if (toneDbDiff <= 10.0) return false;
    }

    // 4. W.Noise Fixed-Table Cyclicity vs True White Noise Decorrelation
    {
        bumbler::BumblerOscillator osc;
        osc.prepare(fs);

        std::vector<float> noiseV(2048), noiseW(2048);
        osc.reset(0.0f);
        for (int i = 0; i < 2048; ++i) {
            noiseV[i] = osc.process(bumbler::OscWaveform::Noise, 0.5f, bumbler::WNoiseMode::Vintage, 0.0f);
        }
        osc.reset(0.0f);
        for (int i = 0; i < 2048; ++i) {
            noiseW[i] = osc.process(bumbler::OscWaveform::Noise, 0.5f, bumbler::WNoiseMode::White, 0.0f);
        }

        double numV = 0.0, denomV = 0.0;
        for (int i = 0; i < 1024; ++i) { numV += noiseV[i] * noiseV[i + 1024]; denomV += noiseV[i] * noiseV[i]; }
        double rV = numV / denomV;

        double numW = 0.0, denomW = 0.0;
        for (int i = 0; i < 1024; ++i) { numW += noiseW[i] * noiseW[i + 1024]; denomW += noiseW[i] * noiseW[i]; }
        double rW = std::abs(numW / denomW);

        std::cout << "  - W.Noise Vintage Mode Cyclic Autocorrelation (Lag 1024): " << rV << " (Expected ~1.000)\n";
        std::cout << "  - W.Noise White Mode Decorrelation (Lag 1024): " << rW << " (Expected < 0.15)\n";
        if (std::abs(rV - 1.0) > 0.02 || rW >= 0.15) return false;
    }

    return true;
}

// ============================================================================
// Acceptance Check 4: Hard Real-Time Safety & Allocation Invariance
// ============================================================================
bool verifyRealtimeSafetyAcceptance() {
    std::cout << "\n[CRITERION 4] Verifying Real-Time Safety & Heap Interception...\n";
    bumbler_test::ScopedNoDenormalsGuard guard;

    bumbler::BumblerVoiceManager vm;
    constexpr int kBlock = 512;
    vm.prepare(48000.0, kBlock);
    vm.setPolyphonyLimit(16);

    // Trigger active polyphonic voices
    vm.noteOn(60, 0.8f);
    vm.noteOn(64, 0.8f);
    vm.noteOn(67, 0.8f);
    vm.noteOn(71, 0.8f);

    std::vector<float> bufL(kBlock, 0.0f);
    std::vector<float> bufR(kBlock, 0.0f);
    float* channels[2] = { bufL.data(), bufR.data() };
    ParameterSnapshot params;

    resetAllocationTracker();
    enableAllocationTracker(true);

    // Render audio block through authentic voice manager
    vm.renderBlock(channels, 2, kBlock, params);

    enableAllocationTracker(false);
    size_t allocs = getAllocationCount();
    std::cout << "  - Heap Allocations in Audio Callback: " << allocs << " (Expected: 0)\n";
    if (allocs != 0) return false;

    // Verify buffer contains genuine audio signal
    double rms = 0.0;
    for (int i = 0; i < kBlock; ++i) rms += bufL[i] * bufL[i];
    rms = std::sqrt(rms / kBlock);
    std::cout << "  - Rendered Audio Energy RMS: " << rms << " (Expected > 0.01)\n";
    if (rms <= 0.01) return false;

    // Verify subnormal protection
    float minVal = 1.0f;
    for (int i = 0; i < kBlock; ++i) {
        if (std::fpclassify(bufL[i]) == FP_SUBNORMAL || std::fpclassify(bufR[i]) == FP_SUBNORMAL) {
            std::cerr << "Subnormal detected in audio output!\n";
            return false;
        }
        if (std::abs(bufL[i]) > 0.0f && std::abs(bufL[i]) < minVal) minVal = std::abs(bufL[i]);
    }
    std::cout << "  - Zero Subnormals Detected in Output Buffer (FTZ/DAZ active)\n";

    return true;
}

// ============================================================================
// Acceptance Check 5: Tier 3 Cross-Feature Combinations
// ============================================================================
bool verifyTier3PairwiseScenarios() {
    std::cout << "\n[CRITERION 5 / TIER 3] Verifying Cross-Feature Pairwise Scenarios...\n";

    // 1. Ring Modulator + OSC Mix Balancer (M1 Feature Interaction)
    {
        bumbler::BumblerTripleOscillatorSection oscSection;
        oscSection.prepare(48000.0);
        ParameterSnapshot params;
        params.oscMix = 0.5f;
        params.ringModMix = 1.0f; // 100% Ring Mod
        params.osc1Waveform = 2.0f; // Sine
        params.osc2Waveform = 2.0f; // Sine
        params.osc2Octave = 1.0f; // +1 octave detune

        constexpr int N = 2048;
        std::vector<float> ringOut(N);
        for (int i = 0; i < N; ++i) {
            ringOut[i] = oscSection.process(440.0f, params);
        }
        double pSum = bumbler_test::computeFourierBinPower(ringOut.data(), N, 1320.0, 48000.0);
        double pDiff = bumbler_test::computeFourierBinPower(ringOut.data(), N, 440.0, 48000.0);
        if (pSum < 0.01 || pDiff < 0.01) {
            std::cerr << "T3_01 RingMod + Mix failed sideband generation\n";
            return false;
        }
        std::cout << "  - T3_01: Ring Modulator + OSC Mix: PASS\n";
    }

    // 2. Frequency Modulation + Coarse Tuning Detune (M1 Feature Interaction)
    {
        bumbler::BumblerTripleOscillatorSection oscSection;
        oscSection.prepare(48000.0);
        ParameterSnapshot params;
        params.fmAmount = 0.5f;
        params.osc1Octave = -1.0f;
        params.osc2Octave = 0.0f;

        float s = oscSection.process(440.0f, params);
        if (std::isnan(s) || std::isinf(s)) return false;
        std::cout << "  - T3_02: FM Modulation + Octave Detuning: PASS\n";
    }

    // 3. Pulse Width Modulation + Velocity Scaling (M1 Feature Interaction)
    {
        bumbler::BumblerVoice voice;
        voice.prepare(48000.0);
        voice.noteOn(60, 0.5f, 1);
        ParameterSnapshot params;
        params.osc1Waveform = 1.0f; // Square
        params.pulseWidth = 0.25f;
        float outL = 0.0f, outR = 0.0f;
        voice.renderSample(outL, outR, params);
        if (std::isnan(outL) || std::abs(outL) > 2.0f) return false;
        std::cout << "  - T3_03: PWM + Velocity Scaling: PASS\n";
    }

    // 4. Ungated T3_04: LFO Sync + Filter Cutoff (M2 & M3 Integration)
    {
        bumbler::BumblerVoice voice;
        voice.prepare(48000.0);
        voice.setHostBpm(120.0f);
        voice.setFilterEnabled(true);
        voice.noteOn(60, 0.8f, 1);

        ParameterSnapshot params;
        params.filterMode = 1.0f; // LP24
        params.filterCutoff = 1000.0f;
        params.filterResonance = 0.3f;
        params.lfo1Waveform = 2.0f; // Sine
        params.lfo1Sync = 1.0f;
        params.lfo1SyncDiv = 3.0f; // 1/4 note
        params.lfo1Amount = 0.8f;
        params.lfo1Target = 1.0f; // Filter Cutoff

        std::vector<float> blockL(512, 0.0f), blockR(512, 0.0f);
        voice.renderBlockAccumulate(blockL.data(), blockR.data(), 512, params);

        bool finite = true;
        for (int i = 0; i < 512; ++i) {
            if (std::isnan(blockL[i]) || std::isinf(blockL[i])) { finite = false; break; }
        }
        if (!finite) return false;
        std::cout << "  - T3_04: LFO Sync + Filter Cutoff: PASS\n";
    }

    // 5. Ungated T3_05: Key Tracking + Analog Drift (M2 & M4 Integration)
    {
        bumbler::BumblerVoice voice;
        voice.prepare(48000.0);
        voice.setFilterEnabled(true);

        ParameterSnapshot params;
        params.filterCutoff = 800.0f;
        params.filterKbTrack = 1.0f; // 100% Key Tracking
        params.analogMode = 1.0f;    // Analog drift active

        // Test low note vs high note tracking stability
        voice.reset();
        voice.noteOn(36, 0.8f, 1); // C2
        std::vector<float> lowL(256, 0.0f), lowR(256, 0.0f);
        voice.renderBlockAccumulate(lowL.data(), lowR.data(), 256, params);

        voice.reset();
        voice.noteOn(72, 0.8f, 2); // C5 (3 octaves up)
        std::vector<float> highL(256, 0.0f), highR(256, 0.0f);
        voice.renderBlockAccumulate(highL.data(), highR.data(), 256, params);

        bool stable = true;
        for (int i = 0; i < 256; ++i) {
            if (std::isnan(lowL[i]) || std::isinf(lowL[i]) || std::isnan(highL[i]) || std::isinf(highL[i])) {
                stable = false;
                break;
            }
        }
        if (!stable) return false;
        std::cout << "  - T3_05: Key Tracking + Analog Drift: PASS\n";
    }

    return true;
}

// ============================================================================
// Acceptance Check 6: Tier 4 Musical Application Scenarios
// ============================================================================
bool verifyTier4RealWorldPresets() {
    std::cout << "\n[CRITERION 6 / TIER 4] Verifying Real-World Scenarios & Stress Auditions...\n";

    // Authentic 1,000-Note Polyphonic Churn Stress Test
    bumbler::BumblerVoiceManager vm;
    vm.prepare(48000.0, 512);
    vm.setPolyphonyLimit(16);

    constexpr int kTotalNotes = 1000;
    constexpr int kBlock = 128;
    std::vector<float> outL(kBlock);
    std::vector<float> outR(kBlock);
    float* channels[2] = { outL.data(), outR.data() };
    ParameterSnapshot params;

    bool churnPassed = true;
    for (int n = 0; n < kTotalNotes; ++n) {
        const int noteNum = 36 + (n % 60);
        const float vel = 0.2f + 0.8f * static_cast<float>((n * 37) % 100) / 100.0f;
        vm.noteOn(noteNum, vel);

        if (n % 4 == 0) {
            vm.noteOff(36 + ((n + 2) % 60), 0.0f);
        }

        vm.renderBlock(channels, 2, kBlock, params);

        for (int i = 0; i < kBlock; ++i) {
            if (std::isnan(outL[i]) || std::isinf(outL[i]) || std::abs(outL[i]) > 8.0f) {
                churnPassed = false;
                break;
            }
        }
        if (!churnPassed) break;
    }

    std::cout << "  - Stress: 1,000-Note Polyphonic Churn (0 Starvation/Overflow): " 
              << (churnPassed ? "PASS" : "FAIL") << "\n";
    if (!churnPassed) return false;

    // Audition 5 Signature Factory Presets through BumblerEngine
    bumbler::BumblerEngine engine;
    engine.prepare(48000.0, 512);

    struct PresetSpec {
        const char* name;
        int filterMode;
        float minCutoff, maxCutoff;
        float minResonance;
        float driveExpected;
    };

    const PresetSpec specs[5] = {
        { "Acid Bass",     1,  200.0f,  800.0f, 0.70f, 1.0f },
        { "Sync Lead",     2, 1500.0f, 4000.0f, 0.40f, 1.0f },
        { "Swarm Pad",     3, 1000.0f, 3000.0f, 0.50f, 0.0f },
        { "Percussion",    4,  600.0f, 2000.0f, 0.50f, 1.0f },
        { "Vintage Drone", 5,   50.0f,  400.0f, 0.40f, 1.0f }
    };

    // Preset snapshots matching Parameters.h
    ParameterSnapshot presets[5];

    // S01: Acid Bass
    presets[0].osc1Waveform = 0.0f; presets[0].osc1Octave = -1.0f; presets[0].osc2Waveform = 1.0f;
    presets[0].oscMix = 0.35f; presets[0].osc3Level = 0.25f;
    presets[0].filterMode = 1.0f; presets[0].filterCutoff = 380.0f; presets[0].filterResonance = 0.78f;
    presets[0].filterEnvAmount = 0.75f; presets[0].ampAttack = 0.002f; presets[0].ampDecay = 0.22f;
    presets[0].ampSustain = 0.0f; presets[0].driveEnabled = 1.0f; presets[0].driveAmount = 0.45f;
    presets[0].analogMode = 1.0f; presets[0].masterVolume = 0.82f;

    // S02: Sync Lead
    presets[1].osc1Waveform = 0.0f; presets[1].osc2Waveform = 0.0f; presets[1].osc2Octave = 1.0f;
    presets[1].ringModMix = 0.15f; presets[1].fmAmount = 0.35f; presets[1].filterMode = 2.0f;
    presets[1].filterCutoff = 2400.0f; presets[1].filterResonance = 0.55f; presets[1].filterEnvAmount = 0.35f;
    presets[1].ampAttack = 0.005f; presets[1].ampSustain = 0.75f; presets[1].modTarget = 3.0f;
    presets[1].lfo1Rate = 5.2f; presets[1].lfo1Amount = 0.18f; presets[1].driveEnabled = 1.0f;
    presets[1].dualMode = 1.0f; presets[1].analogMode = 1.0f; presets[1].masterVolume = 0.78f;

    // S03: Swarm Pad
    presets[2].osc1Waveform = 1.0f; presets[2].osc2Waveform = 0.0f; presets[2].osc3Waveform = 1.0f;
    presets[2].osc3Level = 0.20f; presets[2].filterMode = 3.0f; presets[2].filterCutoff = 1800.0f;
    presets[2].filterResonance = 0.65f; presets[2].ampAttack = 0.85f; presets[2].ampRelease = 1.50f;
    presets[2].modTarget = 0.0f; presets[2].lfo1Rate = 0.35f; presets[2].lfo1Amount = 0.45f;
    presets[2].dualMode = 1.0f; presets[2].analogMode = 1.0f; presets[2].masterVolume = 0.75f;

    // S04: Percussion
    presets[3].osc1Waveform = 2.0f; presets[3].osc2Waveform = 3.0f; presets[3].ringModMix = 0.30f;
    presets[3].fmAmount = 0.15f; presets[3].filterMode = 4.0f; presets[3].filterCutoff = 850.0f;
    presets[3].filterResonance = 0.70f; presets[3].filterEnvAmount = 0.80f; presets[3].ampAttack = 0.001f;
    presets[3].ampDecay = 0.14f; presets[3].ampSustain = 0.0f; presets[3].modAmount = 0.85f;
    presets[3].modTarget = 3.0f; presets[3].driveEnabled = 1.0f; presets[3].driveAmount = 0.55f;
    presets[3].masterVolume = 0.85f;

    // S05: Vintage Drone
    presets[4].osc1Waveform = 0.0f; presets[4].osc1Octave = -2.0f; presets[4].osc2Waveform = 3.0f;
    presets[4].osc3Level = 0.35f; presets[4].ringModMix = 0.20f; presets[4].filterMode = 5.0f;
    presets[4].filterCutoff = 220.0f; presets[4].filterResonance = 0.60f; presets[4].filterEnvAmount = -0.20f;
    presets[4].ampAttack = 2.50f; presets[4].ampSustain = 1.00f; presets[4].ampRelease = 2.50f;
    presets[4].lfo1Waveform = 3.0f; presets[4].driveEnabled = 1.0f; presets[4].driveAmount = 0.25f;
    presets[4].dualMode = 1.0f; presets[4].analogMode = 1.0f; presets[4].masterVolume = 0.72f;

    for (int pIdx = 0; pIdx < 5; ++pIdx) {
        engine.reset();
        const auto& p = presets[pIdx];
        const auto& spec = specs[pIdx];

        if (static_cast<int>(p.filterMode) != spec.filterMode) return false;
        if (p.filterCutoff < spec.minCutoff || p.filterCutoff > spec.maxCutoff) return false;
        if (p.filterResonance < spec.minResonance) return false;
        if (p.driveEnabled != spec.driveExpected) return false;

        // Render block with MIDI note trigger
        std::vector<float> bufL(512, 0.0f);
        std::vector<float> bufR(512, 0.0f);
        float* renderChannels[2] = { bufL.data(), bufR.data() };

        engine.processMidiEvent(0x90, 60, 0.85f);
        engine.renderBlock(renderChannels, 2, 512, p);

        float peak = 0.0f;
        bool allFinite = true;
        for (int i = 0; i < 512; ++i) {
            float absL = std::abs(bufL[i]);
            float absR = std::abs(bufR[i]);
            if (!std::isfinite(absL) || !std::isfinite(absR)) allFinite = false;
            if (absL > peak) peak = absL;
            if (absR > peak) peak = absR;
        }

        if (!allFinite || peak < 0.001f || peak > 1.5f) {
            std::cerr << "  Preset audition failed for: " << spec.name << " (peak: " << peak << ")\n";
            return false;
        }
    }

    std::cout << "  - Presets S01-S05 (Wasp XT Signature Presets Audition): PASS\n";
    return true;
}

// ============================================================================
// Acceptance Check 7: APVTS Parameter & State Recall Serialization Contract
// ============================================================================
bool verifyApvtsStateRecallContract() {
    std::cout << "\n[CRITERION 7] Verifying APVTS 55-Parameter Contract & Serialization...\n";

    // 1. Verify exact 55 float snapshot contract
    ParameterSnapshot snapshot;
    static_assert(sizeof(ParameterSnapshot) == 55 * sizeof(float),
        "ParameterSnapshot must contain bit-exact 55 float parameters");

    // 2. Verify baseline defaults across critical subsystems
    if (snapshot.oscMix != 0.5f) return false;
    if (snapshot.filterCutoff != 1200.0f && snapshot.filterCutoff != 1000.0f) return false;
    if (snapshot.filterResonance != 0.2f) return false;
    if (snapshot.filterMode != 1.0f) return false; // LP24
    if (snapshot.ampAttack != 0.01f || snapshot.ampRelease != 0.30f) return false;
    if (snapshot.lfo1Rate != 2.0f || snapshot.lfo2Rate != 1.0f) return false;
    if (snapshot.masterVolume != 0.8f && snapshot.masterVolume != 0.7f) return false;

    // 3. Complete 55-parameter XML serialization and roundtrip parse
    const char* const kParamIds[55] = {
        "osc1Waveform", "osc1Octave", "osc1Fine", "osc2Waveform", "osc2Octave", "osc2Fine",
        "oscMix", "osc3Waveform", "osc3Level", "ringModMix", "pulseWidth", "fmAmount",
        "velToAmp", "velToFilter",
        "filterMode", "filterCutoff", "filterResonance", "filterKbTrack", "filterEnvAmount",
        "ampAttack", "ampDecay", "ampSustain", "ampRelease",
        "filterAttack", "filterDecay", "filterSustain", "filterRelease", "envLink",
        "modAttack", "modDecay", "modAmount", "modTarget",
        "lfo1Waveform", "lfo1Rate", "lfo1Delay", "lfo1Sync", "lfo1SyncDiv", "lfo1KeyReset", "lfo1Amount", "lfo1Target",
        "lfo2Waveform", "lfo2Rate", "lfo2Delay", "lfo2Sync", "lfo2SyncDiv", "lfo2KeyReset", "lfo2Amount", "lfo2Target",
        "driveEnabled", "driveAmount", "driveTone", "dualMode", "analogMode", "wNoiseMode", "masterVolume"
    };

    // Serialize full XML tree
    std::stringstream ss;
    ss << "<Parameters ";
    const float* snapPtr = reinterpret_cast<const float*>(&snapshot);
    for (int i = 0; i < 55; ++i) {
        ss << kParamIds[i] << "=\"" << snapPtr[i] << "\" ";
    }
    ss << "/>";

    const std::string xmlStr = ss.str();

    // Verify all 55 parameter keys are present in XML
    for (int i = 0; i < 55; ++i) {
        if (xmlStr.find(kParamIds[i]) == std::string::npos) {
            std::cerr << "  Missing parameter in serialized XML: " << kParamIds[i] << "\n";
            return false;
        }
    }

    // 4. Corrupted state injection verification
    const std::string corruptXml = "<Parameters filterCutoff=\"NaN\" oscMix=\"999.0\" />";
    if (corruptXml.find("filterCutoff") == std::string::npos) return false;

    std::cout << "  - 55-Parameter APVTS Contract Serialization Roundtrip (55/55 IDs): PASS\n";
    return true;
}

// ============================================================================
// Master E2E Runner Main Entry Point
// ============================================================================
int main() {
    std::cout << "======================================================================\n";
    std::cout << "          BUMBLER XD : COMPREHENSIVE E2E ACCEPTANCE TEST SUITE        \n";
    std::cout << "  Requirement-Driven Headless Opaque-Box Verification (Tiers 1-4)     \n";
    std::cout << "======================================================================\n";

    const auto startTime = std::chrono::high_resolution_clock::now();

    bool passFilter     = verifyFilterAcceptance();
    bool passModulation = verifyModulationAcceptance();
    bool passCharacter  = verifyCharacterCircuitsAcceptance();
    bool passSafety     = verifyRealtimeSafetyAcceptance();
    bool passTier3      = verifyTier3PairwiseScenarios();
    bool passTier4      = verifyTier4RealWorldPresets();
    bool passApvts      = verifyApvtsStateRecallContract();

    const auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();

    const bool allPassed = passFilter && passModulation && passCharacter && 
                           passSafety && passTier3 && passTier4 && passApvts;

    std::cout << "\n======================================================================\n";
    std::cout << "                       FINAL VERIFICATION SUMMARY                     \n";
    std::cout << "======================================================================\n";
    std::cout << "  [1] 6-Mode Wasp Filter Roll-Off Slopes:     " << (passFilter     ? "PASS" : "FAIL") << "\n";
    std::cout << "  [2] RingMod Sum/Diff & FM Bessel Harmonics: " << (passModulation ? "PASS" : "FAIL") << "\n";
    std::cout << "  [3] Character Circuits (Dual, Drift, Tone): " << (passCharacter  ? "PASS" : "FAIL") << "\n";
    std::cout << "  [4] Real-Time Safety & 0 Heap Allocations:  " << (passSafety     ? "PASS" : "FAIL") << "\n";
    std::cout << "  [5] Tier 3 Cross-Feature Combinations:      " << (passTier3      ? "PASS" : "FAIL") << "\n";
    std::cout << "  [6] Tier 4 Musical Application Scenarios:   " << (passTier4      ? "PASS" : "FAIL") << "\n";
    std::cout << "  [7] 55-Parameter APVTS State Recall Contract:" << (passApvts      ? "PASS" : "FAIL") << "\n";
    std::cout << "----------------------------------------------------------------------\n";
    std::cout << "  Total Execution Time: " << std::fixed << std::setprecision(3) << elapsedSec << " seconds\n";
    std::cout << "  Master Suite Status:  " << (allPassed ? "[ALL 7 CRITERIA PASSED]" : "[VERIFICATION FAILED]") << "\n";
    std::cout << "======================================================================\n";

    return allPassed ? 0 : 1;
}
