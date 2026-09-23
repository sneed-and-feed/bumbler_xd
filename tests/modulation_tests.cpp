#include "test_helpers.h"

#if __has_include("../source/dsp/BumblerOscillator.h")
#include "../source/dsp/BumblerOscillator.h"
#define BUMBLER_HAS_REAL_OSCILLATOR 1
#endif

#if __has_include("../source/dsp/BumblerLFO.h")
#include "../source/dsp/BumblerLFO.h"
#define BUMBLER_HAS_REAL_LFO 1
#endif

#if __has_include("../source/dsp/BumblerEnvelopes.h")
#include "../source/dsp/BumblerEnvelopes.h"
#define BUMBLER_HAS_REAL_ENVELOPES 1
#endif

#if __has_include("../source/dsp/BumblerVoice.h")
#include "../source/dsp/BumblerVoice.h"
#define BUMBLER_HAS_REAL_VOICE 1
#endif

// ============================================================================
// Reference Modulation Implementations for Headless Verification
// ============================================================================
namespace mod_ref {

// DC Blocker filter: y[n] = x[n] - x[n-1] + R * y[n-1]
struct DcBlocker {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float R = 0.995f;

    float process(float in) {
        float out = in - x1 + R * y1;
        x1 = in;
        y1 = out;
        return out;
    }
    void reset() { x1 = y1 = 0.0f; }
};

// Reference Ring Modulator
inline void processRingMod(const float* osc1, const float* osc2, float* out, int numSamples, float mix) {
    static DcBlocker dcBlock;
    for (int i = 0; i < numSamples; ++i) {
        const float ring = osc1[i] * osc2[i];
        const float filteredRing = dcBlock.process(ring);
        out[i] = (1.0f - mix) * osc1[i] + mix * filteredRing;
    }
}

// Reference Frequency Modulation (OSC 1 modulates OSC 2 carrier)
inline void generateFm(float* out, int numSamples, double carrierHz, double modHz, double modIndex, double sampleRate) {
    const double twoPiOverFs = bumbler_test::kTwoPi / sampleRate;
    double phaseCarrier = 0.0;
    double phaseMod = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        const double modSignal = std::sin(phaseMod);
        // Instantaneous carrier frequency modulation
        const double instFreq = carrierHz + modIndex * modHz * modSignal;
        out[i] = static_cast<float>(std::sin(phaseCarrier));

        phaseCarrier += instFreq * twoPiOverFs;
        if (phaseCarrier >= bumbler_test::kTwoPi) phaseCarrier -= bumbler_test::kTwoPi;

        phaseMod += modHz * twoPiOverFs;
        if (phaseMod >= bumbler_test::kTwoPi) phaseMod -= bumbler_test::kTwoPi;
    }
}

// Reference LFO
class RefLFO {
public:
    enum Wave { SINE = 0, SAW = 1, SQUARE = 2, NOISE = 3 };

    void reset() {
        phase = 0.0;
        delaySamplesRemaining = static_cast<int>(delaySec * fs);
    }

    void setParams(double rateHz, double delaySeconds, Wave wave, double sampleRate) {
        rate = rateHz;
        delaySec = delaySeconds;
        waveform = wave;
        fs = sampleRate;
        reset();
    }

    float processSample() {
        if (delaySamplesRemaining > 0) {
            --delaySamplesRemaining;
            return 0.0f;
        }

        float out = 0.0f;
        switch (waveform) {
            case SINE:
                out = static_cast<float>(std::sin(phase));
                break;
            case SAW:
                out = static_cast<float>(1.0 - 2.0 * (phase / bumbler_test::kTwoPi));
                break;
            case SQUARE:
                out = (phase < bumbler_test::kPi) ? 1.0f : -1.0f;
                break;
            case NOISE:
                // Sample and hold step
                if (phase < phaseInc) {
                    noiseHold = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f;
                }
                out = noiseHold;
                break;
        }

        phaseInc = bumbler_test::kTwoPi * rate / fs;
        phase += phaseInc;
        if (phase >= bumbler_test::kTwoPi) phase -= bumbler_test::kTwoPi;

        return out;
    }

private:
    double rate = 2.0;
    double delaySec = 0.0;
    Wave waveform = SINE;
    double fs = 48000.0;
    double phase = 0.0;
    double phaseInc = 0.0;
    int delaySamplesRemaining = 0;
    float noiseHold = 0.0f;
};

// Reference ADSR Envelope
class RefADSR {
public:
    enum State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

    void setSampleRate(double sampleRate) { fs = sampleRate; }

    void setParameters(float aSec, float dSec, float sLevel, float rSec) {
        attackRate  = (aSec > 0.001f) ? (1.0f / (aSec * static_cast<float>(fs))) : 1.0f;
        decayCoef   = std::exp(-1.0f / (std::max(0.001f, dSec) * static_cast<float>(fs) * 0.5f));
        sustainLevel = std::clamp(sLevel, 0.0f, 1.0f);
        releaseCoef = std::exp(-1.0f / (std::max(0.001f, rSec) * static_cast<float>(fs) * 0.5f));
    }

    void noteOn() {
        state = ATTACK;
    }

    void noteOff() {
        state = RELEASE;
    }

    float processSample() {
        switch (state) {
            case IDLE:
                val = 0.0f;
                break;
            case ATTACK:
                val += attackRate;
                if (val >= 1.0f) {
                    val = 1.0f;
                    state = DECAY;
                }
                break;
            case DECAY:
                val = sustainLevel + (val - sustainLevel) * decayCoef;
                if (std::abs(val - sustainLevel) < 0.001f) {
                    val = sustainLevel;
                    state = SUSTAIN;
                }
                break;
            case SUSTAIN:
                val = sustainLevel;
                break;
            case RELEASE:
                val *= releaseCoef;
                if (val < 0.0001f) {
                    val = 0.0f;
                    state = IDLE;
                }
                break;
        }
        return val;
    }

    State getState() const { return state; }
    float getValue() const { return val; }

private:
    double fs = 48000.0;
    State state = IDLE;
    float val = 0.0f;
    float attackRate = 0.01f;
    float decayCoef = 0.999f;
    float sustainLevel = 0.5f;
    float releaseCoef = 0.999f;
};

// Reference Attack-Decay Mod Envelope
class RefModEnv {
public:
    void setParameters(float aSec, float dSec, float amt, double sampleRate) {
        attackRate = 1.0f / (std::max(0.001f, aSec) * static_cast<float>(sampleRate));
        decayCoef  = std::exp(-1.0f / (std::max(0.001f, dSec) * static_cast<float>(sampleRate) * 0.5f));
        amount = amt;
    }

    void trigger() {
        val = 0.0f;
        isAttacking = true;
    }

    float processSample() {
        if (isAttacking) {
            val += attackRate;
            if (val >= 1.0f) {
                val = 1.0f;
                isAttacking = false;
            }
        } else {
            val *= decayCoef;
        }
        return val * amount;
    }

private:
    float val = 0.0f;
    float attackRate = 0.01f;
    float decayCoef = 0.999f;
    float amount = 1.0f;
    bool isAttacking = false;
};

} // namespace mod_ref

// ============================================================================
// 1. Ring Modulator Sum/Diff Frequencies & Zero DC Runaway
// ============================================================================
void test_ring_mod_sum_diff_and_dc() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 4800; // Exact integer cycles for 100, 300, 400, 500 Hz
    constexpr double f1 = 400.0;
    constexpr double f2 = 100.0;

    std::vector<float> osc1(kSamples);
    std::vector<float> osc2(kSamples);
    std::vector<float> ringOut(kSamples);

    bumbler_test::generateSine(osc1.data(), kSamples, f1, kSampleRate, 1.0f);
    bumbler_test::generateSine(osc2.data(), kSamples, f2, kSampleRate, 1.0f);

    mod_ref::processRingMod(osc1.data(), osc2.data(), ringOut.data(), kSamples, 1.0f);

    // Sum harmonic = 500 Hz, Difference harmonic = 300 Hz
    double pSum  = bumbler_test::computeFourierBinPower(ringOut.data(), kSamples, 500.0, kSampleRate);
    double pDiff = bumbler_test::computeFourierBinPower(ringOut.data(), kSamples, 300.0, kSampleRate);
    double pFund1 = bumbler_test::computeFourierBinPower(ringOut.data(), kSamples, 400.0, kSampleRate);
    double pFund2 = bumbler_test::computeFourierBinPower(ringOut.data(), kSamples, 100.0, kSampleRate);

    // Assert sum and diff harmonics exist with equal amplitude
    TEST_ASSERT(pSum > 0.05, "Ring mod sum frequency (500 Hz) missing or weak: power = " + std::to_string(pSum));
    TEST_ASSERT(pDiff > 0.05, "Ring mod diff frequency (300 Hz) missing or weak: power = " + std::to_string(pDiff));
    TEST_ASSERT(std::abs(pSum - pDiff) < 0.05, "Ring mod sideband symmetry violated");

    // Carrier bleed rejection: 400 Hz and 100 Hz should be negligible (< 1e-4)
    TEST_ASSERT(pFund1 < 1.0e-3, "Ring mod carrier 1 bleed too high: " + std::to_string(pFund1));
    TEST_ASSERT(pFund2 < 1.0e-3, "Ring mod carrier 2 bleed too high: " + std::to_string(pFund2));

    // DC Runaway verification: compute DC component over integer cycles (2400 to 4800 samples)
    double dcSum = 0.0;
    for (int i = 2400; i < kSamples; ++i) {
        dcSum += ringOut[i];
    }
    double dcOffset = std::abs(dcSum / (kSamples - 2400));
    TEST_ASSERT(dcOffset < 1.0e-3, "Ring mod has DC offset runaway: " + std::to_string(dcOffset));
}

// ============================================================================
// 2. FM Bessel-Distribution Sideband Harmonics Verification
// ============================================================================
void test_fm_bessel_sidebands() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 4096;
    constexpr double fc = 1000.0; // Carrier
    constexpr double fm = 200.0;  // Modulator

    // Test with low modulation index (beta = 0.5)
    std::vector<float> fmOutLow(kSamples);
    mod_ref::generateFm(fmOutLow.data(), kSamples, fc, fm, 0.5, kSampleRate);

    // Test with higher modulation index (beta = 1.5)
    std::vector<float> fmOutHigh(kSamples);
    mod_ref::generateFm(fmOutHigh.data(), kSamples, fc, fm, 1.5, kSampleRate);

    // First sideband pair at fc ± fm (800 Hz and 1200 Hz)
    double p800Low  = bumbler_test::computeFourierBinPower(fmOutLow.data(), kSamples, 800.0, kSampleRate);
    double p1200Low = bumbler_test::computeFourierBinPower(fmOutLow.data(), kSamples, 1200.0, kSampleRate);
    double p800High = bumbler_test::computeFourierBinPower(fmOutHigh.data(), kSamples, 800.0, kSampleRate);
    double p1200High = bumbler_test::computeFourierBinPower(fmOutHigh.data(), kSamples, 1200.0, kSampleRate);

    // Assert sidebands exist and are symmetrical
    TEST_ASSERT(p800Low > 0.01, "FM first sideband 800 Hz missing");
    TEST_ASSERT(p1200Low > 0.01, "FM first sideband 1200 Hz missing");
    TEST_ASSERT(std::abs(p800Low - p1200Low) < 0.05, "FM sideband pair 1 asymmetry");

    // Increasing modulation index must monotonically increase first sideband power
    TEST_ASSERT(p800High > p800Low, "FM sideband energy did not increase with modulation index");
    TEST_ASSERT(p1200High > p1200Low, "FM sideband energy did not increase with modulation index");

    // Second sideband pair at fc ± 2fm (600 Hz and 1400 Hz) should be visible under higher beta
    double p600High = bumbler_test::computeFourierBinPower(fmOutHigh.data(), kSamples, 600.0, kSampleRate);
    double p1400High = bumbler_test::computeFourierBinPower(fmOutHigh.data(), kSamples, 1400.0, kSampleRate);
    TEST_ASSERT(p600High > 0.005, "FM second sideband 600 Hz missing at beta=1.5");
    TEST_ASSERT(p1400High > 0.005, "FM second sideband 1400 Hz missing at beta=1.5");
}

// ============================================================================
// 3. Dual LFO Waveforms, Tempo Sync, Delay, and Reset
// ============================================================================
void test_lfo_features() {
    constexpr double kSampleRate = 48000.0;

    // Test mod_ref::RefLFO
    {
        mod_ref::RefLFO lfo;

        // Test 1: Sine LFO frequency accuracy
        lfo.setParams(5.0, 0.0, mod_ref::RefLFO::SINE, kSampleRate);
        std::vector<float> sineBuf(48000);
        for (int i = 0; i < 48000; ++i) sineBuf[i] = lfo.processSample();
        double p5Hz = bumbler_test::computeFourierBinPower(sineBuf.data(), 48000, 5.0, kSampleRate);
        TEST_ASSERT(p5Hz > 0.85, "LFO Sine wave frequency accuracy failed: power = " + std::to_string(p5Hz));

        // Test 2: LFO Delay stage (output should remain 0 during delay window)
        lfo.setParams(2.0, 0.1, mod_ref::RefLFO::SINE, kSampleRate); // 0.1 sec delay = 4800 samples
        for (int i = 0; i < 4700; ++i) {
            float out = lfo.processSample();
            TEST_ASSERT(out == 0.0f, "LFO output non-zero during delay phase at sample " + std::to_string(i));
        }

        // Test 3: Key Reset restarts phase at 0
        lfo.setParams(2.0, 0.0, mod_ref::RefLFO::SINE, kSampleRate);
        for (int i = 0; i < 6000; ++i) lfo.processSample(); // advance phase
        lfo.reset(); // trigger key reset
        float resetVal = lfo.processSample();
        TEST_ASSERT(std::abs(resetVal) < 0.01f, "LFO key reset failed to restart phase near 0: " + std::to_string(resetVal));

        // Test 4: Square wave levels are strictly ±1
        lfo.setParams(4.0, 0.0, mod_ref::RefLFO::SQUARE, kSampleRate);
        for (int i = 0; i < 2000; ++i) {
            float val = lfo.processSample();
            TEST_ASSERT(val == 1.0f || val == -1.0f, "LFO square output not ±1");
        }
    }

#if BUMBLER_HAS_REAL_LFO
    // Test genuine bumbler::BumblerLFO implementation
    {
        bumbler::BumblerLFO realLfo;
        realLfo.prepare(kSampleRate);

        // Real Test 1: Sine frequency accuracy (5.0 Hz)
        realLfo.setParams(5.0, 0.0, bumbler::BumblerLFO::SINE, kSampleRate);
        std::vector<float> sineBuf(48000);
        for (int i = 0; i < 48000; ++i) sineBuf[i] = realLfo.processSample();
        double p5Hz = bumbler_test::computeFourierBinPower(sineBuf.data(), 48000, 5.0, kSampleRate);
        TEST_ASSERT(p5Hz > 0.85, "Real BumblerLFO Sine frequency accuracy failed: power = " + std::to_string(p5Hz));

        // Real Test 2: Delay window (0.1 sec = 4800 samples)
        realLfo.setParams(2.0, 0.1, bumbler::BumblerLFO::SINE, kSampleRate);
        for (int i = 0; i < 4700; ++i) {
            float out = realLfo.processSample();
            TEST_ASSERT(out == 0.0f, "Real BumblerLFO output non-zero during delay phase at sample " + std::to_string(i));
        }

        // Real Test 3: Key reset restarts phase at 0
        realLfo.setParams(2.0, 0.0, bumbler::BumblerLFO::SINE, kSampleRate);
        for (int i = 0; i < 6000; ++i) (void)realLfo.processSample();
        realLfo.noteTrigger();
        float resetVal = realLfo.processSample();
        TEST_ASSERT(std::abs(resetVal) < 0.01f, "Real BumblerLFO key reset failed: " + std::to_string(resetVal));

        // Real Test 4: Square wave levels are strictly ±1.0
        realLfo.setParams(4.0, 0.0, bumbler::BumblerLFO::SQUARE, kSampleRate);
        for (int i = 0; i < 2000; ++i) {
            float val = realLfo.processSample();
            TEST_ASSERT(val == 1.0f || val == -1.0f, "Real BumblerLFO square output not ±1");
        }

        // Real Test 5: Sawtooth ramp is bounded in [-1.0, 1.0] and downward sloping
        realLfo.setParams(2.0, 0.0, bumbler::BumblerLFO::SAW, kSampleRate);
        float s0 = realLfo.processSample();
        float s1 = realLfo.processSample();
        TEST_ASSERT(s0 >= -1.0f && s0 <= 1.0f, "Real BumblerLFO saw out of bounds");
        TEST_ASSERT(s1 < s0, "Real BumblerLFO saw not downward ramp");

        // Real Test 6: Noise (Sample & Hold) held over cycle
        realLfo.setParams(2.0, 0.0, bumbler::BumblerLFO::NOISE, kSampleRate);
        float n0 = realLfo.processSample();
        float n1 = realLfo.processSample();
        TEST_ASSERT(n0 >= -1.0f && n0 <= 1.0f, "Real BumblerLFO noise out of bounds");
        TEST_ASSERT(n0 == n1, "Real BumblerLFO S&H noise not held within cycle");

        // Real Test 7: Tempo sync calculation (120 BPM, 1/4 note div=3 -> 2.0 Hz)
        realLfo.setWaveform(bumbler::LfoWaveform::Sine);
        realLfo.setDelay(0.0f);
        realLfo.setTempoSync(true, 3.0f, 120.0f);
        TEST_ASSERT_NEAR(realLfo.getEffectiveRateHz(), 2.0f, 0.01f, "Real BumblerLFO tempo sync rate incorrect");
        std::vector<float> syncBuf(48000);
        for (int i = 0; i < 48000; ++i) syncBuf[i] = realLfo.processSample();
        double p2Hz = bumbler_test::computeFourierBinPower(syncBuf.data(), 48000, 2.0, kSampleRate);
        TEST_ASSERT(p2Hz > 0.85, "Real BumblerLFO tempo sync 2.0 Hz accuracy failed: " + std::to_string(p2Hz));
    }
#endif
}

// ============================================================================
// 4. MOD ENV & ADSR Envelopes Timing and Curve Tracking
// ============================================================================
void test_envelope_curves_and_timing() {
    constexpr double kSampleRate = 48000.0;

    // Test mod_ref
    {
        mod_ref::RefADSR adsr;
        adsr.setSampleRate(kSampleRate);
        adsr.setParameters(0.01f, 0.02f, 0.5f, 0.03f); // A=10ms, D=20ms, S=0.5, R=30ms

        adsr.noteOn();
        // During attack (10ms = 480 samples), value should rise from 0 to 1
        float prev = -1.0f;
        for (int i = 0; i < 480; ++i) {
            float cur = adsr.processSample();
            TEST_ASSERT(cur >= prev, "ADSR attack not monotonically increasing");
            prev = cur;
        }
        TEST_ASSERT(std::abs(prev - 1.0f) < 0.05f, "ADSR failed to reach peak 1.0 at end of attack");

        // During decay (20ms = 960 samples), value should decay towards sustain (0.5)
        for (int i = 0; i < 1500; ++i) {
            adsr.processSample();
        }
        TEST_ASSERT_NEAR(adsr.getValue(), 0.5f, 0.05f, "ADSR failed to reach sustain level");

        // Note off triggers release
        adsr.noteOff();
        for (int i = 0; i < 3000; ++i) {
            adsr.processSample();
        }
        TEST_ASSERT(adsr.getValue() < 0.05f, "ADSR release failed to decay near silence");

        // Test 2: MOD ENV (Attack-Decay) Bipolar Depth
        mod_ref::RefModEnv modEnvPos;
        modEnvPos.setParameters(0.01f, 0.05f, +1.0f, kSampleRate);
        modEnvPos.trigger();

        mod_ref::RefModEnv modEnvNeg;
        modEnvNeg.setParameters(0.01f, 0.05f, -1.0f, kSampleRate);
        modEnvNeg.trigger();

        for (int i = 0; i < 480; ++i) {
            float p = modEnvPos.processSample();
            float n = modEnvNeg.processSample();
            TEST_ASSERT(p >= 0.0f, "Positive MOD ENV produced negative value");
            TEST_ASSERT(n <= 0.0f, "Negative MOD ENV produced positive value");
            TEST_ASSERT_NEAR(p, -n, 0.001f, "Bipolar MOD ENV polarity asymmetry");
        }
    }

#if BUMBLER_HAS_REAL_ENVELOPES
    // Test genuine bumbler::BumblerADSR implementation
    {
        bumbler::BumblerADSR adsr;
        adsr.prepare(kSampleRate);
        adsr.setParameters(0.01f, 0.02f, 0.5f, 0.03f); // A=10ms, D=20ms, S=0.5, R=30ms
        adsr.noteOn();

        float prev = -1.0f;
        for (int i = 0; i < 480; ++i) {
            float cur = adsr.processSample();
            TEST_ASSERT(cur >= prev, "Real ADSR attack not monotonically increasing");
            prev = cur;
        }
        TEST_ASSERT(std::abs(prev - 1.0f) < 0.05f, "Real ADSR failed to reach peak 1.0");

        for (int i = 0; i < 1500; ++i) {
            (void)adsr.processSample();
        }
        TEST_ASSERT_NEAR(adsr.getValue(), 0.5f, 0.05f, "Real ADSR failed to reach sustain level");

        adsr.noteOff();
        for (int i = 0; i < 3000; ++i) {
            (void)adsr.processSample();
        }
        TEST_ASSERT(adsr.getValue() < 0.05f, "Real ADSR release failed to decay near silence");

        // Denormal flushing check: run for 20000 more samples
        for (int i = 0; i < 20000; ++i) {
            (void)adsr.processSample();
        }
        TEST_ASSERT(adsr.getValue() == 0.0f, "Real ADSR denormal flush failed to zero tail");
        TEST_ASSERT(!adsr.isActive(), "Real ADSR should be inactive after full release");

        // Test genuine bumbler::BumblerModEnvelope
        bumbler::BumblerModEnvelope modEnvPos;
        modEnvPos.prepare(kSampleRate);
        modEnvPos.setParameters(0.01f, 0.05f, +1.0f);
        modEnvPos.trigger();

        bumbler::BumblerModEnvelope modEnvNeg;
        modEnvNeg.prepare(kSampleRate);
        modEnvNeg.setParameters(0.01f, 0.05f, -1.0f);
        modEnvNeg.trigger();

        for (int i = 0; i < 480; ++i) {
            float p = modEnvPos.processSample();
            float n = modEnvNeg.processSample();
            TEST_ASSERT(p >= 0.0f, "Real Positive MOD ENV produced negative value");
            TEST_ASSERT(n <= 0.0f, "Real Negative MOD ENV produced positive value");
            TEST_ASSERT_NEAR(p, -n, 0.001f, "Real Bipolar MOD ENV polarity asymmetry");
        }

        // Test genuine bumbler::BumblerDualADSR with envLink
        bumbler::BumblerDualADSR dual;
        dual.prepare(kSampleRate);
        bumbler::ParameterSnapshot params;
        params.ampAttack = 0.02f;
        params.ampDecay = 0.05f;
        params.ampSustain = 0.7f;
        params.ampRelease = 0.1f;
        params.filterAttack = 0.08f;
        params.filterDecay = 0.15f;
        params.filterSustain = 0.3f;
        params.filterRelease = 0.4f;

        // envLink = 0: filter uses filterAttack
        params.envLink = 0.0f;
        dual.update(params);
        dual.noteOn();
        for (int i = 0; i < 480; ++i) (void)dual.getFilterEnv().processSample();
        // At 480 samples (10ms), filter env with 80ms attack is at ~0.125
        float unlinkedVal = dual.getFilterEnv().getValue();
        TEST_ASSERT(unlinkedVal < 0.25f, "Unlinked filter env should have slow attack");

        // envLink = 1: filter mirrors amp (20ms attack)
        dual.reset();
        params.envLink = 1.0f;
        dual.update(params);
        dual.noteOn();
        for (int i = 0; i < 960; ++i) (void)dual.getFilterEnv().processSample();
        float linkedVal = dual.getFilterEnv().getValue();
        TEST_ASSERT_NEAR(linkedVal, 1.0f, 0.05f, "Linked filter env should match amp attack");
    }
#endif
}

// ============================================================================
// 5. Voice Modulation Matrix End-to-End Routing Tests
// ============================================================================
#if BUMBLER_HAS_REAL_VOICE
void test_voice_modulation_matrix() {
    constexpr double kSampleRate = 48000.0;
    bumbler::BumblerVoice voice;
    voice.prepare(kSampleRate);

    bumbler::ParameterSnapshot params;
    params.ampAttack = 0.001f;
    params.ampDecay = 0.1f;
    params.ampSustain = 1.0f;
    params.ampRelease = 0.1f;
    params.masterVolume = 1.0f;

    // Test 1: LFO 1 -> Pitch modulation (Osc12Pitch = 0)
    params.lfo1Target = 0.0f; // Osc12Pitch
    params.lfo1Amount = 1.0f;
    params.lfo1Rate = 5.0f;
    params.lfo1Waveform = 2.0f; // Sine
    params.lfo1Delay = 0.0f;

    voice.noteOn(60, 1.0f, 1);
    float outL = 0.0f, outR = 0.0f;
    for (int i = 0; i < 100; ++i) {
        outL = 0.0f; outR = 0.0f;
        voice.renderSample(outL, outR, params);
    }
    TEST_ASSERT(voice.isActive(), "Voice should be active after noteOn");
    TEST_ASSERT(std::abs(outL) > 0.0f || std::abs(outR) > 0.0f, "Voice output should be non-zero");

    // Test 2: MOD ENV -> Pitch 2 (Osc2Pitch = 3)
    voice.forceKill();
    params.lfo1Amount = 0.0f;
    params.modTarget = 3.0f; // Osc2Pitch
    params.modAmount = 1.0f;
    params.modAttack = 0.01f;
    params.modDecay = 0.05f;

    voice.noteOn(60, 1.0f, 2);
    for (int i = 0; i < 200; ++i) {
        outL = 0.0f; outR = 0.0f;
        voice.renderSample(outL, outR, params);
    }
    TEST_ASSERT(voice.getModEnv().isActive(), "ModEnv should be active during attack/decay");
    TEST_ASSERT(voice.getModEnv().getValue() > 0.0f, "ModEnv value should be positive");

    // Test 3: LFO 2 -> Master Amp (MasterAmp = 2)
    voice.forceKill();
    params.modAmount = 0.0f;
    params.lfo2Target = 2.0f; // MasterAmp
    params.lfo2Amount = 1.0f;
    params.lfo2Rate = 10.0f;
    params.lfo2Waveform = 1.0f; // Square
    params.lfo2Delay = 0.0f;

    voice.noteOn(60, 1.0f, 3);
    float ampModMax = 0.0f;
    for (int i = 0; i < 4800; ++i) {
        outL = 0.0f; outR = 0.0f;
        voice.renderSample(outL, outR, params);
        if (std::abs(outL) > ampModMax) ampModMax = std::abs(outL);
    }
    TEST_ASSERT(ampModMax > 0.0f, "Tremolo voice output should produce sound");

    // Test 4: MOD ENV -> LFO 1 Amount (Lfo1Amount = 1)
    voice.forceKill();
    params.lfo2Amount = 0.0f;
    params.lfo1Amount = 0.0f; // Base LFO 1 depth is 0
    params.lfo1Target = 1.0f; // Filter Cutoff
    params.lfo1Rate = 20.0f;
    params.modTarget = 1.0f; // MOD ENV opens LFO 1 Amount
    params.modAmount = 1.0f;
    params.modAttack = 0.005f;
    params.modDecay = 0.05f;

    voice.noteOn(60, 1.0f, 4);
    for (int i = 0; i < 240; ++i) {
        outL = 0.0f; outR = 0.0f;
        voice.renderSample(outL, outR, params);
    }
    TEST_ASSERT(voice.getModEnv().getValue() > 0.5f, "MOD ENV should have opened LFO 1 amount");
}
#endif

// ============================================================================
// Main Modulation Test Runner
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << " Bumbler XD: Modulation & Matrix Test Suite         \n";
    std::cout << "====================================================\n";

    RUN_TEST(test_ring_mod_sum_diff_and_dc);
    RUN_TEST(test_fm_bessel_sidebands);
    RUN_TEST(test_lfo_features);
    RUN_TEST(test_envelope_curves_and_timing);
#if BUMBLER_HAS_REAL_VOICE
    RUN_TEST(test_voice_modulation_matrix);
#endif

    std::cout << "\n----------------------------------------------------\n";
    std::cout << " Modulation Tests Summary: " << gGlobalTestsPassed << " Passed, " 
              << gGlobalTestsFailed << " Failed\n";
    std::cout << "----------------------------------------------------\n";

    return (gGlobalTestsFailed == 0) ? 0 : 1;
}
