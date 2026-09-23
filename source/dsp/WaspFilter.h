#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>
#include "BumblerCommon.h"

namespace bumbler {

/**
 * WaspFilterMode: 6 distinct filter topologies matching vintage Wasp XT hardware.
 * LP12:   2-pole lowpass (~12 dB/oct roll-off)
 * LP24:   4-pole cascaded lowpass (~24 dB/oct roll-off) with analog tanh saturation
 * LP_NT:  2-pole lowpass cascaded with a notch filter for vocal vowel resonance
 * DBL_NT: Double notch filter with two distinct attenuation nulls (0.707 * fc and 1.414 * fc)
 * BP24:   4-pole cascaded bandpass filter attenuating low and high ends
 * HP24:   4-pole cascaded highpass filter (~24 dB/oct roll-off below cutoff)
 */
using WaspFilterMode = FilterMode;

/**
 * SvfStage: Single 2-pole Zero-Delay Feedback (ZDF) State-Variable Filter stage.
 * Uses bilinear trapezoidal integration (Topology-Preserving Transform) for
 * unconditional stability, authentic analog frequency mapping, and zero latency delay.
 */
struct SvfStage {
    double s1 { 0.0 };
    double s2 { 0.0 };

    void reset() noexcept {
        s1 = 0.0;
        s2 = 0.0;
    }

    /**
     * Executes one trapezoidal integration step.
     * in:     Input signal sample
     * g:      Prewarped integrator gain: tan(pi * fc / fs)
     * k:      Damping factor: 2.0 - 1.9 * resonance (k in (0.1, 2.0])
     * lp/bp/hp/notch: Output node voltages
     */
    inline void step(double in, double g, double k, double& lp, double& bp, double& hp, double& notch) noexcept {
        const double d = 1.0 + g * (g + k);
        hp = (in - (g + k) * s1 - s2) / d;

        const double v1 = g * hp;
        bp = v1 + s1;
        s1 = bp + v1;

        const double v2 = g * bp;
        lp = v2 + s2;
        s2 = lp + v2;

        notch = hp + lp;

        // Hardware denormal protection and soft limiting safeguard
        if (std::abs(s1) < 1.0e-15) s1 = 0.0;
        if (std::abs(s2) < 1.0e-15) s2 = 0.0;

        // State sanity clamping against adversarial numerical divergence
        s1 = std::clamp(s1, -12.0, 12.0);
        s2 = std::clamp(s2, -12.0, 12.0);
    }
};

/**
 * WaspFilter: Production-ready C++20 engine implementing the complete 6-mode Wasp XT filter.
 * Guarantees zero dynamic memory allocations in audio processing paths,
 * complete denormal immunity, smooth sub-block parameter interpolation,
 * and strict IEEE 754 finite output bounds.
 */
class WaspFilter {
public:
    WaspFilter() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    /**
     * Direct sample processing matching FilterTestHarness and filter accuracy verification.
     * in:       Audio input sample
     * cutoffHz: Cutoff frequency in Hz (clamped safely to [20 Hz, 0.48 * fs])
     * resonance: Resonance amount in [0.0, 1.0] (clamped safely to [0.0, 0.99])
     * mode:     One of the 6 Wasp XT filter modes
     */
    [[nodiscard]] float processSample(float in, double cutoffHz, double resonance, WaspFilterMode mode) noexcept;
    [[nodiscard]] float processSample(float in, float cutoffHz, float resonance, WaspFilterMode mode) noexcept {
        return processSample(in, static_cast<double>(cutoffHz), static_cast<double>(resonance), mode);
    }

    /**
     * Block processing with 16-sample sub-block parameter interpolation to eliminate zipper noise.
     */
    void processBlock(const float* input, float* output, int numSamples, float cutoffHz, float resonance, WaspFilterMode mode) noexcept;

    /**
     * Utility method: Calculates modulated cutoff frequency from base cutoff,
     * keyboard tracking (relative to MIDI Note 60 = C4), bipolar envelope amount (-1..+1),
     * and instantaneous filter envelope level.
     */
    [[nodiscard]] static float calculateModulatedCutoff(
        float baseCutoffHz,
        float midiNoteWithBend,
        float kbTrack,
        float envAmount,
        float envLevel,
        float sampleRate,
        float extModCutoffSemitones = 0.0f
    ) noexcept;

private:
    double mSampleRate { 48000.0 };
    SvfStage mStage1;
    SvfStage mStage2;

    // Sub-block parameter smoothing registers
    bool mFirstBlock { true };
    double mCurrentG { 0.1 };
    double mCurrentK { 2.0 };
};

} // namespace bumbler

// Global namespace exports for standalone test harnesses and legacy callers
using bumbler::WaspFilter;
using bumbler::WaspFilterMode;
