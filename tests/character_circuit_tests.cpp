#include "test_helpers.h"
#include "BumblerCommon.h"
#include "ParameterSnapshot.h"
#include "BumblerOscillator.h"
#include "BumblerVoice.h"
#include "BumblerVoiceManager.h"
#include "BumblerEngine.h"
#include "CharacterCircuits.h"

// ============================================================================
// 1. Dual Mode Stereo Widening & Beating Verification (r < 0.70)
// ============================================================================
void test_dual_mode_stereo_widening() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 4096;

    std::vector<float> monoIn(kSamples);
    bumbler_test::generateSine(monoIn.data(), kSamples, 440.0, kSampleRate, 0.8f);

    // A. Direct Verification of bumbler::CharacterCircuits
    {
        bumbler::CharacterCircuits circuits;
        circuits.prepare(kSampleRate, kSamples);

        bumbler::ParameterSnapshot params;
        params.driveEnabled = 0.0f;
        params.masterVolume = 1.0f;

        // 1. Dual Mode OFF: Left and Right must be identical (cross-correlation r >= 0.999)
        params.dualMode = 0.0f;
        std::vector<float> outL_off = monoIn;
        std::vector<float> outR_off = monoIn;
        circuits.reset();
        circuits.processStereo(outL_off.data(), outR_off.data(), kSamples, params);

        double rOff = bumbler_test::computeCrossCorrelation(outL_off.data(), outR_off.data(), kSamples);
        TEST_ASSERT_NEAR(rOff, 1.000, 0.001, "Real CharacterCircuits Dual OFF cross-correlation not 1.0");

        // 2. Dual Mode ON: Left and Right must be decorrelated (r < 0.70)
        params.dualMode = 1.0f;
        std::vector<float> outL_on = monoIn;
        std::vector<float> outR_on = monoIn;
        circuits.reset();
        circuits.processStereo(outL_on.data(), outR_on.data(), kSamples, params);

        double rOn = bumbler_test::computeCrossCorrelation(outL_on.data(), outR_on.data(), kSamples);
        TEST_ASSERT(rOn < 0.70, "Real CharacterCircuits Dual ON failed stereo widening: r = " + std::to_string(rOn));

        // 3. Peak ceiling check
        float peakL = bumbler_test::computePeak(outL_on.data(), kSamples);
        float peakR = bumbler_test::computePeak(outR_on.data(), kSamples);
        TEST_ASSERT(peakL <= 1.05f && peakR <= 1.05f, "Real CharacterCircuits Dual mode exceeded safety ceiling");
    }

    // B. Full Engine Verification of Dual Mode via BumblerEngine
    {
        bumbler::BumblerEngine engine;
        engine.prepare(kSampleRate, 512);

        bumbler::ParameterSnapshot engParams;
        engParams.osc1Waveform = 0.0f; // Sawtooth
        engParams.ampAttack = 0.001f;
        engParams.ampSustain = 1.0f;
        engParams.masterVolume = 0.8f;
        engParams.dualMode = 1.0f;

        engine.processMidiEvent(0x90, 60, 0.8f); // Note On C4

        std::vector<float> engL(kSamples, 0.0f);
        std::vector<float> engR(kSamples, 0.0f);

        for (int b = 0; b < kSamples; b += 512) {
            float* blockChannels[2] = { engL.data() + b, engR.data() + b };
            engine.renderBlock(blockChannels, 2, 512, engParams);
        }

        double engR_on = bumbler_test::computeCrossCorrelation(engL.data() + 512, engR.data() + 512, kSamples - 512);
        TEST_ASSERT(engR_on < 0.70, "BumblerEngine Dual mode failed to widen stereo field: r = " + std::to_string(engR_on));
    }
}

// ============================================================================
// 2. Analog Mode Pitch Drift Variance Across Repeated Triggers
// ============================================================================
void test_analog_mode_pitch_drift_variance() {
    constexpr double kSampleRate = 48000.0;

    // A. Direct Verification of bumbler::AnalogVoiceDrift
    {
        bumbler::AnalogVoiceDrift driftGen;
        driftGen.seed(12345);

        // Analog OFF: 10 triggers produce zero drift
        std::vector<double> driftOff(10);
        for (int i = 0; i < 10; ++i) driftOff[i] = driftGen.triggerNote(false);
        double varOff = bumbler_test::computeVariance(driftOff);
        TEST_ASSERT(varOff == 0.0, "AnalogVoiceDrift OFF has non-zero drift variance: " + std::to_string(varOff));

        // Analog ON: 10 triggers produce variance > 0.05
        std::vector<double> driftOn(10);
        for (int i = 0; i < 10; ++i) driftOn[i] = driftGen.triggerNote(true);
        double varOn = bumbler_test::computeVariance(driftOn);
        TEST_ASSERT(varOn > 0.05, "AnalogVoiceDrift ON lacks drift variance: " + std::to_string(varOn));

        // Free-running phase variance > 0.1
        std::vector<double> phases(10);
        for (int i = 0; i < 10; ++i) phases[i] = driftGen.getInitialPhase(true);
        double phaseVar = bumbler_test::computeVariance(phases);
        TEST_ASSERT(phaseVar > 0.1, "AnalogVoiceDrift free-running phase not randomized");
    }

    // B. Authentic Voice Audio Verification on BumblerVoice
    {
        bumbler::BumblerVoice voice;
        voice.prepare(kSampleRate);

        bumbler::ParameterSnapshot vParams;
        vParams.osc1Waveform = 2.0f; // Pure sine
        vParams.oscMix = 0.0f;
        vParams.ampAttack = 0.001f;
        vParams.ampSustain = 1.0f;
        vParams.velToAmp = 0.0f;

        auto measureVoiceFreq = [&](bumbler::BumblerVoice& v, const bumbler::ParameterSnapshot& p) -> double {
            constexpr int kWarmup = 256;
            constexpr int kMeasure = 4096;
            for (int i = 0; i < kWarmup; ++i) { float sL = 0, sR = 0; v.renderSample(sL, sR, p); }
            std::vector<float> audio(kMeasure);
            for (int i = 0; i < kMeasure; ++i) { float sL = 0, sR = 0; v.renderSample(sL, sR, p); audio[i] = sL; }

            std::vector<double> crossings;
            crossings.reserve(64);
            for (size_t i = 1; i < audio.size(); ++i) {
                if (audio[i - 1] <= 0.0f && audio[i] > 0.0f) {
                    double y1 = audio[i - 1], y2 = audio[i];
                    double frac = -y1 / (y2 - y1);
                    crossings.push_back((i - 1) + frac);
                }
            }
            if (crossings.size() < 4) return 0.0;
            double totalCycles = static_cast<double>(crossings.size() - 1);
            double totalSamples = crossings.back() - crossings.front();
            return kSampleRate / (totalSamples / totalCycles);
        };

        // Measure pitch drift variance with Analog Mode OFF
        vParams.analogMode = 0.0f;
        std::vector<double> voiceCentsOff(10);
        for (int i = 0; i < 10; ++i) {
            voice.reset();
            voice.setAnalogMode(false);
            voice.noteOn(69, 1.0f, static_cast<uint64_t>(i * 4096));
            double f = measureVoiceFreq(voice, vParams);
            voiceCentsOff[i] = 1200.0 * std::log2(f / 440.0);
        }
        double voiceVarOff = bumbler_test::computeVariance(voiceCentsOff);
        TEST_ASSERT(voiceVarOff < 1.0e-5, "Real BumblerVoice Analog OFF has non-zero pitch drift variance: " + std::to_string(voiceVarOff));

        // Measure pitch drift variance with Analog Mode ON
        vParams.analogMode = 1.0f;
        std::vector<double> voiceCentsOn(10);
        std::vector<double> initSamples(10);
        for (int i = 0; i < 10; ++i) {
            voice.reset();
            voice.setAnalogMode(true);
            voice.noteOn(69, 1.0f, static_cast<uint64_t>(i * 4096));
            initSamples[i] = voice.getOscInitialPhaseRad(); // Record initial oscillator phase
            float sL = 0.0f, sR = 0.0f;
            voice.renderSample(sL, sR, vParams);
            double f = measureVoiceFreq(voice, vParams);
            voiceCentsOn[i] = 1200.0 * std::log2(f / 440.0);
        }
        double voiceVarOn = bumbler_test::computeVariance(voiceCentsOn);
        TEST_ASSERT(voiceVarOn > 0.05, "Real BumblerVoice Analog ON lacks pitch drift variance: " + std::to_string(voiceVarOn));

        double voicePhaseVar = bumbler_test::computeVariance(initSamples);
        TEST_ASSERT(voicePhaseVar > 0.1, "Real BumblerVoice Analog ON lacks free-running phase variance: " + std::to_string(voicePhaseVar));
    }
}

// ============================================================================
// 3. Output Drive Harmonic Saturation and Tone Control Spectral Tilt
// ============================================================================
void test_distortion_harmonics_and_tone() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 4096;
    constexpr double fFund = 1000.0;

    std::vector<float> sineIn(kSamples);
    bumbler_test::generateSine(sineIn.data(), kSamples, fFund, kSampleRate, 0.7f);

    bumbler::CharacterCircuits circuits;
    circuits.prepare(kSampleRate, kSamples);

    bumbler::ParameterSnapshot params;
    params.masterVolume = 1.0f;
    params.dualMode = 0.0f;

    // Test 1: Harmonic distortion increases with Drive
    params.driveEnabled = 0.0f;
    params.driveAmount = 0.0f;
    params.driveTone = 0.5f;

    std::vector<float> cleanL = sineIn;
    std::vector<float> cleanR = sineIn;
    circuits.reset();
    circuits.processStereo(cleanL.data(), cleanR.data(), kSamples, params);

    params.driveEnabled = 1.0f;
    params.driveAmount = 0.8f;
    std::vector<float> drivenL = sineIn;
    std::vector<float> drivenR = sineIn;
    circuits.reset();
    circuits.processStereo(drivenL.data(), drivenR.data(), kSamples, params);

    // Compute 3rd harmonic (3000 Hz) power
    double clean3rd = bumbler_test::computeFourierBinPower(cleanL.data(), kSamples, 3000.0, kSampleRate);
    double driven3rd = bumbler_test::computeFourierBinPower(drivenL.data(), kSamples, 3000.0, kSampleRate);

    TEST_ASSERT(clean3rd < 1.0e-5, "Real CharacterCircuits clean output contains unexpected 3rd harmonic");
    TEST_ASSERT(driven3rd > 0.01, "Real CharacterCircuits driven output lacks 3rd harmonic saturation: " + std::to_string(driven3rd));
    TEST_ASSERT(driven3rd > clean3rd * 100.0, "Drive failed to increase 3rd harmonic by > 20 dB");

    // Ceiling limit check
    float peakDriven = bumbler_test::computePeak(drivenL.data(), kSamples);
    TEST_ASSERT(peakDriven <= 1.05f, "Real CharacterCircuits distortion exceeded ceiling limit: " + std::to_string(peakDriven));

    // Test 2: Tone Control Spectral Tilt
    params.driveTone = 0.0f; // Dark
    std::vector<float> darkL = sineIn;
    std::vector<float> darkR = sineIn;
    circuits.reset();
    circuits.processStereo(darkL.data(), darkR.data(), kSamples, params);

    params.driveTone = 1.0f; // Bright
    std::vector<float> brightL = sineIn;
    std::vector<float> brightR = sineIn;
    circuits.reset();
    circuits.processStereo(brightL.data(), brightR.data(), kSamples, params);

    double dark3rd = bumbler_test::computeFourierBinPower(darkL.data(), kSamples, 3000.0, kSampleRate);
    double bright3rd = bumbler_test::computeFourierBinPower(brightL.data(), kSamples, 3000.0, kSampleRate);

    double dbDiff = bumbler_test::toDb(std::sqrt(bright3rd)) - bumbler_test::toDb(std::sqrt(dark3rd));
    TEST_ASSERT(dbDiff > 10.0, "Real CharacterCircuits tone control failed spectral tilt: dbDiff = " + std::to_string(dbDiff) + " dB");
}

// ============================================================================
// 4. W.Noise Fixed-Table vs True White Noise Comparison
// ============================================================================
void test_wnoise_table_vs_white_noise() {
    constexpr int kSamples = 2048;

    // A. Verification via bumbler::NoiseEngine
    {
        bumbler::NoiseEngine noiseEng;
        std::vector<float> vintageNoise(kSamples);
        std::vector<float> whiteNoise(kSamples);

        noiseEng.reset();
        noiseEng.render(vintageNoise.data(), kSamples, false); // Vintage table (1024 period)

        noiseEng.reset();
        noiseEng.render(whiteNoise.data(), kSamples, true); // True white noise

        double numVintage = 0.0, denomVintage = 0.0;
        for (int i = 0; i < 1024; ++i) {
            numVintage += vintageNoise[i] * vintageNoise[i + 1024];
            denomVintage += vintageNoise[i] * vintageNoise[i];
        }
        double autoCorrVintage = numVintage / denomVintage;
        TEST_ASSERT_NEAR(autoCorrVintage, 1.000, 0.01, "Real NoiseEngine vintage table failed 1024-sample periodicity");

        double numWhite = 0.0, denomWhite = 0.0;
        for (int i = 0; i < 1024; ++i) {
            numWhite += whiteNoise[i] * whiteNoise[i + 1024];
            denomWhite += whiteNoise[i] * whiteNoise[i];
        }
        double autoCorrWhite = std::abs(numWhite / denomWhite);
        TEST_ASSERT(autoCorrWhite < 0.15, "Real NoiseEngine white noise exhibits spurious lag-1024 correlation");
    }

    // B. Direct Verification of bumbler::BumblerOscillator
    {
        bumbler::BumblerOscillator osc;
        osc.prepare(48000.0);

        std::vector<float> oscVintage(kSamples);
        std::vector<float> oscWhite(kSamples);

        osc.reset(0.0f);
        for (int i = 0; i < kSamples; ++i) {
            oscVintage[i] = osc.process(bumbler::OscWaveform::Noise, 0.5f, bumbler::WNoiseMode::Vintage, 0.0f);
        }

        osc.reset(0.0f);
        for (int i = 0; i < kSamples; ++i) {
            oscWhite[i] = osc.process(bumbler::OscWaveform::Noise, 0.5f, bumbler::WNoiseMode::White, 0.0f);
        }

        double numV = 0.0, denomV = 0.0;
        for (int i = 0; i < 1024; ++i) {
            numV += oscVintage[i] * oscVintage[i + 1024];
            denomV += oscVintage[i] * oscVintage[i];
        }
        double rV = numV / denomV;
        TEST_ASSERT_NEAR(rV, 1.000, 0.01, "BumblerOscillator vintage noise failed 1024-sample periodicity");

        double numW = 0.0, denomW = 0.0;
        for (int i = 0; i < 1024; ++i) {
            numW += oscWhite[i] * oscWhite[i + 1024];
            denomW += oscWhite[i] * oscWhite[i];
        }
        double rW = std::abs(numW / denomW);
        TEST_ASSERT(rW < 0.15, "BumblerOscillator white noise exhibits spurious lag-1024 correlation: " + std::to_string(rW));
    }
}

// ============================================================================
// 5. Master Volume & DC Blocker Verification
// ============================================================================
void test_master_volume_and_dc_blocker() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 16384;

    bumbler::CharacterCircuits circuits;
    circuits.prepare(kSampleRate, kSamples);

    // Test DC Rejection on severe +0.5f DC offset input
    std::vector<float> dcInL(kSamples);
    std::vector<float> dcInR(kSamples);
    bumbler_test::generateSine(dcInL.data(), kSamples, 1000.0, kSampleRate, 0.2f);
    for (int i = 0; i < kSamples; ++i) {
        dcInL[i] += 0.5f; // Add +0.5 DC bias
        dcInR[i] = dcInL[i];
    }

    bumbler::ParameterSnapshot params;
    params.driveEnabled = 0.0f;
    params.dualMode = 0.0f;
    params.masterVolume = 1.0f;

    circuits.reset();
    circuits.processStereo(dcInL.data(), dcInR.data(), kSamples, params);

    // Discard initial 8192 transient samples and measure DC mean
    double dcMean = 0.0;
    for (int i = 8192; i < kSamples; ++i) dcMean += dcInL[i];
    dcMean = std::abs(dcMean / (kSamples - 8192));
    TEST_ASSERT(dcMean < 1.0e-3, "CharacterCircuits DC blocker failed to eliminate DC bias: " + std::to_string(dcMean));

    // Test Master Volume scaling
    std::vector<float> sineL(1024), sineR(1024);
    bumbler_test::generateSine(sineL.data(), 1024, 440.0, kSampleRate, 0.5f);
    sineR = sineL;

    params.masterVolume = 0.0f;
    circuits.reset();
    circuits.processStereo(sineL.data(), sineR.data(), 1024, params);
    float peakSilent = bumbler_test::computePeak(sineL.data(), 1024);
    TEST_ASSERT(peakSilent == 0.0f, "CharacterCircuits masterVolume=0.0 not silent");
}

// ============================================================================
// Main Character Circuits Test Runner
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << " Bumbler XD: Vintage Character Circuits Test Suite  \n";
    std::cout << "====================================================\n";

    RUN_TEST(test_dual_mode_stereo_widening);
    RUN_TEST(test_analog_mode_pitch_drift_variance);
    RUN_TEST(test_distortion_harmonics_and_tone);
    RUN_TEST(test_wnoise_table_vs_white_noise);
    RUN_TEST(test_master_volume_and_dc_blocker);

    std::cout << "\n----------------------------------------------------\n";
    std::cout << " Character Circuits Tests Summary: " << gGlobalTestsPassed << " Passed, " 
              << gGlobalTestsFailed << " Failed\n";
    std::cout << "----------------------------------------------------\n";

    return (gGlobalTestsFailed == 0) ? 0 : 1;
}
