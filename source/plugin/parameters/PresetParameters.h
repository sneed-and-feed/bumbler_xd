#pragma once

#include "../../dsp/ParameterSnapshot.h"
#include <array>

namespace bumbler {

// ============================================================================
// Factory Presets: 5 Signature Sounds
// ============================================================================
struct PresetDefinition {
    const char* name;
    const char* category;
    ParameterSnapshot params;
};

inline const std::array<PresetDefinition, 5>& getFactoryPresets() {
    static const std::array<PresetDefinition, 5> presets = []() {
        std::array<PresetDefinition, 5> p {};

        // --------------------------------------------------------------------
        // 1. Acid Bass (Classic 303 / Wasp squelch bass)
        // --------------------------------------------------------------------
        p[0].name = "Acid Bass";
        p[0].category = "Bass";
        p[0].params.osc1Waveform  = 0.0f;  // Saw
        p[0].params.osc1Octave    = -1.0f; // -1 oct
        p[0].params.osc1Fine      = 0.0f;
        p[0].params.osc2Waveform  = 1.0f;  // Square
        p[0].params.osc2Octave    = -1.0f;
        p[0].params.osc2Fine      = 0.05f;
        p[0].params.oscMix        = 0.35f; // Favor Saw
        p[0].params.osc3Waveform  = 0.0f;  // Square sub
        p[0].params.osc3Level     = 0.25f;
        p[0].params.ringModMix    = 0.0f;
        p[0].params.pulseWidth    = 0.50f;
        p[0].params.fmAmount      = 0.0f;
        p[0].params.velToAmp      = 0.70f;
        p[0].params.velToFilter   = 0.85f; // Dynamic accent

        p[0].params.filterMode      = 1.0f;   // LP24
        p[0].params.filterCutoff    = 380.0f; // Low base cutoff
        p[0].params.filterResonance = 0.78f;  // Squelch resonance
        p[0].params.filterKbTrack   = 0.60f;
        p[0].params.filterEnvAmount = 0.75f;  // Plucky envelope sweep

        p[0].params.ampAttack     = 0.002f;
        p[0].params.ampDecay      = 0.22f;
        p[0].params.ampSustain    = 0.0f;
        p[0].params.ampRelease    = 0.15f;
        p[0].params.filterAttack  = 0.002f;
        p[0].params.filterDecay   = 0.28f;
        p[0].params.filterSustain = 0.05f;
        p[0].params.filterRelease = 0.18f;
        p[0].params.envLink       = 0.0f;

        p[0].params.modAttack     = 0.01f;
        p[0].params.modDecay      = 0.20f;
        p[0].params.modAmount     = 0.0f;
        p[0].params.modTarget     = 0.0f;

        p[0].params.lfo1Waveform = 2.0f;
        p[0].params.lfo1Rate     = 0.5f;
        p[0].params.lfo1Amount   = 0.0f;
        p[0].params.lfo2Waveform = 2.0f;
        p[0].params.lfo2Rate     = 1.0f;
        p[0].params.lfo2Amount   = 0.0f;

        p[0].params.driveEnabled = 1.0f;  // Saturation bite
        p[0].params.driveAmount  = 0.45f;
        p[0].params.driveTone    = 0.65f;
        p[0].params.dualMode     = 0.0f;  // Centered mono bass
        p[0].params.analogMode   = 1.0f;
        p[0].params.wNoiseMode   = 0.0f;
        p[0].params.masterVolume = 0.82f;

        // --------------------------------------------------------------------
        // 2. Sync Lead (Aggressive piercing sync with FM & pitch laser dive)
        // --------------------------------------------------------------------
        p[1].name = "Sync Lead";
        p[1].category = "Lead";
        p[1].params.osc1Waveform  = 0.0f;  // Saw
        p[1].params.osc1Octave    = 0.0f;
        p[1].params.osc1Fine      = 0.0f;
        p[1].params.osc2Waveform  = 0.0f;  // Saw
        p[1].params.osc2Octave    = 1.0f;  // +1 oct
        p[1].params.osc2Fine      = 0.12f; // Detuned
        p[1].params.oscMix        = 0.50f;
        p[1].params.osc3Waveform  = 0.0f;
        p[1].params.osc3Level     = 0.0f;
        p[1].params.ringModMix    = 0.15f; // Metallic edge
        p[1].params.pulseWidth    = 0.50f;
        p[1].params.fmAmount      = 0.35f; // Audio-rate FM grit
        p[1].params.velToAmp      = 0.50f;
        p[1].params.velToFilter   = 0.40f;

        p[1].params.filterMode      = 2.0f;    // LP+NT (formant cascade)
        p[1].params.filterCutoff    = 2400.0f;
        p[1].params.filterResonance = 0.55f;
        p[1].params.filterKbTrack   = 0.75f;
        p[1].params.filterEnvAmount = 0.35f;

        p[1].params.ampAttack     = 0.005f;
        p[1].params.ampDecay      = 0.60f;
        p[1].params.ampSustain    = 0.75f;
        p[1].params.ampRelease    = 0.35f;
        p[1].params.filterAttack  = 0.02f;
        p[1].params.filterDecay   = 0.80f;
        p[1].params.filterSustain = 0.40f;
        p[1].params.filterRelease = 0.40f;
        p[1].params.envLink       = 0.0f;

        p[1].params.modAttack     = 0.005f;
        p[1].params.modDecay      = 0.30f;
        p[1].params.modAmount     = 0.40f;
        p[1].params.modTarget     = 3.0f;  // ModEnv -> OSC 2 pitch dive

        p[1].params.lfo1Waveform = 2.0f;  // Sine
        p[1].params.lfo1Rate     = 5.2f;  // Delayed vibrato
        p[1].params.lfo1Delay    = 0.25f;
        p[1].params.lfo1Amount   = 0.18f;
        p[1].params.lfo1Target   = 0.0f;  // OSC 1+2 pitch

        p[1].params.driveEnabled = 1.0f;
        p[1].params.driveAmount  = 0.35f;
        p[1].params.driveTone    = 0.75f; // Bright cutting bite
        p[1].params.dualMode     = 1.0f;  // Wide stereo spread
        p[1].params.analogMode   = 1.0f;
        p[1].params.masterVolume = 0.78f;

        // --------------------------------------------------------------------
        // 3. Swarm Pad (Lush double-notch choir pad with slow PWM sweep)
        // --------------------------------------------------------------------
        p[2].name = "Swarm Pad";
        p[2].category = "Pad";
        p[2].params.osc1Waveform  = 1.0f;  // Square
        p[2].params.osc1Octave    = 0.0f;
        p[2].params.osc1Fine      = -0.08f;
        p[2].params.osc2Waveform  = 0.0f;  // Saw
        p[2].params.osc2Octave    = 0.0f;
        p[2].params.osc2Fine      = 0.08f;
        p[2].params.oscMix        = 0.50f;
        p[2].params.osc3Waveform  = 1.0f;  // Saw sub-layer
        p[2].params.osc3Level     = 0.20f;
        p[2].params.pulseWidth    = 0.50f;
        p[2].params.velToAmp      = 0.30f;
        p[2].params.velToFilter   = 0.40f;

        p[2].params.filterMode      = 3.0f;    // DBL.NT (Double Notch)
        p[2].params.filterCutoff    = 1800.0f;
        p[2].params.filterResonance = 0.65f;
        p[2].params.filterKbTrack   = 0.50f;
        p[2].params.filterEnvAmount = 0.25f;

        p[2].params.ampAttack     = 0.85f;  // Slow blooming swell
        p[2].params.ampDecay      = 1.80f;
        p[2].params.ampSustain    = 0.85f;
        p[2].params.ampRelease    = 1.50f;
        p[2].params.filterAttack  = 1.20f;
        p[2].params.filterDecay   = 2.00f;
        p[2].params.filterSustain = 0.70f;
        p[2].params.filterRelease = 1.80f;

        p[2].params.modAttack     = 0.50f;
        p[2].params.modDecay      = 3.00f;
        p[2].params.modAmount     = 0.30f;
        p[2].params.modTarget     = 0.0f;  // ModEnv -> Pulse Width

        p[2].params.lfo1Waveform = 2.0f;  // Sine
        p[2].params.lfo1Rate     = 0.35f; // Slow PWM sweep
        p[2].params.lfo1Amount   = 0.45f;
        p[2].params.lfo1Target   = 2.0f;  // Pulse Width

        p[2].params.lfo2Waveform = 2.0f;  // Sine
        p[2].params.lfo2Rate     = 0.22f; // Slow OSC Mix pan
        p[2].params.lfo2Amount   = 0.30f;
        p[2].params.lfo2Target   = 1.0f;  // OSC Mix

        p[2].params.driveEnabled = 0.0f;
        p[2].params.dualMode     = 1.0f;  // Massive stereo chorusing
        p[2].params.analogMode   = 1.0f;
        p[2].params.masterVolume = 0.75f;

        // --------------------------------------------------------------------
        // 4. Percussion (Snappy analog drum/zap transient with noise & BP24)
        // --------------------------------------------------------------------
        p[3].name = "Percussion";
        p[3].category = "Percussion";
        p[3].params.osc1Waveform  = 2.0f;  // Sine thump
        p[3].params.osc1Octave    = -1.0f;
        p[3].params.osc2Waveform  = 3.0f;  // Noise snap
        p[3].params.oscMix        = 0.40f;
        p[3].params.ringModMix    = 0.30f; // Metallic click
        p[3].params.fmAmount      = 0.15f;
        p[3].params.velToAmp      = 0.85f;
        p[3].params.velToFilter   = 0.90f;

        p[3].params.filterMode      = 4.0f;   // BP24 (Bandpass)
        p[3].params.filterCutoff    = 850.0f;
        p[3].params.filterResonance = 0.70f;
        p[3].params.filterKbTrack   = 0.20f;
        p[3].params.filterEnvAmount = 0.80f;  // Filter plunge

        p[3].params.ampAttack     = 0.001f; // 1ms instant punch
        p[3].params.ampDecay      = 0.14f;
        p[3].params.ampSustain    = 0.0f;
        p[3].params.ampRelease    = 0.08f;
        p[3].params.filterAttack  = 0.001f;
        p[3].params.filterDecay   = 0.09f;
        p[3].params.filterSustain = 0.0f;
        p[3].params.filterRelease = 0.06f;

        p[3].params.modAttack     = 0.001f;
        p[3].params.modDecay      = 0.06f;
        p[3].params.modAmount     = 0.85f;
        p[3].params.modTarget     = 3.0f;  // Fast pitch dive on OSC 2

        p[3].params.driveEnabled = 1.0f;  // Crunchy transient drive
        p[3].params.driveAmount  = 0.55f;
        p[3].params.driveTone    = 0.60f;
        p[3].params.dualMode     = 0.0f;  // Tight mono punch
        p[3].params.analogMode   = 0.0f;  // Phase-locked transient
        p[3].params.masterVolume = 0.85f;

        // --------------------------------------------------------------------
        // 5. Vintage Drone (Dark, brooding HP24 atmospheric drone with S&H)
        // --------------------------------------------------------------------
        p[4].name = "Vintage Drone";
        p[4].category = "Atmosphere";
        p[4].params.osc1Waveform  = 0.0f;  // Saw
        p[4].params.osc1Octave    = -2.0f; // Sub rumble
        p[4].params.osc1Fine      = -0.05f;
        p[4].params.osc2Waveform  = 3.0f;  // Noise bed
        p[4].params.osc2Octave    = -1.0f;
        p[4].params.oscMix        = 0.60f;
        p[4].params.osc3Waveform  = 1.0f;  // Saw sub
        p[4].params.osc3Level     = 0.35f;
        p[4].params.ringModMix    = 0.20f;
        p[4].params.fmAmount      = 0.05f;
        p[4].params.velToAmp      = 0.20f;
        p[4].params.velToFilter   = 0.20f;

        p[4].params.filterMode      = 5.0f;   // HP24
        p[4].params.filterCutoff    = 220.0f; // Highpass hollow resonance
        p[4].params.filterResonance = 0.60f;
        p[4].params.filterKbTrack   = 0.30f;
        p[4].params.filterEnvAmount = -0.20f; // Inverted breathing

        p[4].params.ampAttack     = 2.50f;
        p[4].params.ampDecay      = 3.00f;
        p[4].params.ampSustain    = 1.00f;  // Infinite hold drone
        p[4].params.ampRelease    = 2.50f;
        p[4].params.filterAttack  = 3.00f;
        p[4].params.filterDecay   = 4.00f;
        p[4].params.filterSustain = 0.90f;
        p[4].params.filterRelease = 3.00f;

        p[4].params.modAttack     = 1.00f;
        p[4].params.modDecay      = 4.00f;
        p[4].params.modAmount     = 0.25f;
        p[4].params.modTarget     = 1.0f;  // ModEnv -> LFO 1 depth

        p[4].params.lfo1Waveform = 3.0f;  // Noise (Sample & Hold)
        p[4].params.lfo1Rate     = 0.80f; // Random cutoff drift
        p[4].params.lfo1Amount   = 0.25f;
        p[4].params.lfo1Target   = 1.0f;  // Filter cutoff

        p[4].params.lfo2Waveform = 2.0f;  // Sine
        p[4].params.lfo2Rate     = 0.08f; // Ultra-slow pitch warble
        p[4].params.lfo2Amount   = 0.15f;
        p[4].params.lfo2Target   = 0.0f;  // OSC 1 pitch

        p[4].params.driveEnabled = 1.0f;
        p[4].params.driveAmount  = 0.25f;
        p[4].params.driveTone    = 0.35f; // Dark vintage warmth
        p[4].params.dualMode     = 1.0f;  // Atmospheric stereo field
        p[4].params.analogMode   = 1.0f;  // Free-running analog drift
        p[4].params.masterVolume = 0.72f;

        return p;
    }();
    return presets;
}

} // namespace bumbler
