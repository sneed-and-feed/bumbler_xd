#pragma once

#include <type_traits>

namespace bumbler {

/**
 * ParameterSnapshot: POD plain-old-data structure holding current snapshot of all 55 parameters.
 * Guarantees zero heap allocation, lock-free thread safety, and trivial copyability.
 */
struct ParameterSnapshot {
    // ------------------------------------------------------------------------
    // Group 1: Oscillators & Voice Management (14 parameters)
    // ------------------------------------------------------------------------
    float osc1Waveform { 0.0f };  // 0=Saw, 1=Square, 2=Sine, 3=Noise
    float osc1Octave   { 0.0f };  // -3 to +3 octaves
    float osc1Fine     { 0.0f };  // -1 to +1 semitones (-100 to +100 cents)
    float osc2Waveform { 0.0f };  // 0=Saw, 1=Square, 2=Sine, 3=Noise
    float osc2Octave   { 0.0f };  // -3 to +3 octaves
    float osc2Fine     { 0.0f };  // -1 to +1 semitones (-100 to +100 cents)
    float oscMix       { 0.5f };  // 0.0 (OSC 1) to 1.0 (OSC 2)
    float osc3Waveform { 0.0f };  // 0=Square, 1=Saw
    float osc3Level    { 0.0f };  // 0.0 to 1.0
    float ringModMix   { 0.0f };  // 0.0 to 1.0 (Off / Blend / On)
    float pulseWidth   { 0.5f };  // 0.01 to 0.99
    float fmAmount     { 0.0f };  // 0.0 to 1.0 (OSC 1 mod OSC 2)
    float velToAmp     { 0.5f };  // 0.0 to 1.0 velocity depth to Amp ADSR
    float velToFilter  { 0.5f };  // 0.0 to 1.0 velocity depth to Filter ADSR

    // ------------------------------------------------------------------------
    // Group 2: 6-Mode Filter Section (5 parameters)
    // ------------------------------------------------------------------------
    float filterMode      { 1.0f };    // 0=LP12, 1=LP24, 2=LP+NT, 3=DBL.NT, 4=BP24, 5=HP24
    float filterCutoff    { 1200.0f }; // 20.0 to 20000.0 Hz
    float filterResonance { 0.2f };    // 0.0 to 1.0
    float filterKbTrack   { 0.5f };    // 0.0 to 1.0
    float filterEnvAmount { 0.0f };    // -1.0 to +1.0 (Bipolar)

    // ------------------------------------------------------------------------
    // Group 3: Envelopes (13 parameters)
    // ------------------------------------------------------------------------
    float ampAttack     { 0.01f }; // 0.001 to 10.0 seconds
    float ampDecay      { 0.30f }; // 0.001 to 10.0 seconds
    float ampSustain    { 0.80f }; // 0.0 to 1.0 level
    float ampRelease    { 0.30f }; // 0.001 to 10.0 seconds

    float filterAttack  { 0.05f }; // 0.001 to 10.0 seconds
    float filterDecay   { 0.50f }; // 0.001 to 10.0 seconds
    float filterSustain { 0.50f }; // 0.0 to 1.0 level
    float filterRelease { 0.40f }; // 0.001 to 10.0 seconds
    float envLink       { 0.0f };  // 0.0 or 1.0 (Link toggle)

    float modAttack     { 0.05f }; // 0.001 to 5.0 seconds
    float modDecay      { 0.50f }; // 0.001 to 10.0 seconds
    float modAmount     { 0.0f };  // -1.0 to +1.0 (Bipolar)
    float modTarget     { 0.0f };  // 0=PW, 1=LFO1Amt, 2=Osc1Level, 3=Osc2Pitch

    // ------------------------------------------------------------------------
    // Group 4: Dual LFOs (16 parameters: 8 for LFO 1, 8 for LFO 2)
    // ------------------------------------------------------------------------
    float lfo1Waveform { 2.0f }; // 0=Saw, 1=Square, 2=Sine, 3=Noise
    float lfo1Rate     { 2.0f }; // 0.05 to 30.0 Hz
    float lfo1Delay    { 0.0f }; // 0.0 to 5.0 seconds
    float lfo1Sync     { 0.0f }; // 0.0 (Free) or 1.0 (Sync)
    float lfo1SyncDiv  { 3.0f }; // Musical division (1/4 default)
    float lfo1KeyReset { 1.0f }; // 0.0 or 1.0
    float lfo1Amount   { 0.0f }; // 0.0 to 1.0
    float lfo1Target   { 1.0f }; // 0=Osc12Pitch, 1=FilterCutoff, 2=PulseWidth

    float lfo2Waveform { 2.0f }; // 0=Saw, 1=Square, 2=Sine, 3=Noise
    float lfo2Rate     { 1.0f }; // 0.05 to 30.0 Hz
    float lfo2Delay    { 0.0f }; // 0.0 to 5.0 seconds
    float lfo2Sync     { 0.0f }; // 0.0 (Free) or 1.0 (Sync)
    float lfo2SyncDiv  { 4.0f }; // Musical division (1/2 default)
    float lfo2KeyReset { 1.0f }; // 0.0 or 1.0
    float lfo2Amount   { 0.0f }; // 0.0 to 1.0
    float lfo2Target   { 0.0f }; // 0=Osc1Pitch, 1=OscMix, 2=MasterAmp

    // ------------------------------------------------------------------------
    // Group 5: Character & Master Output (7 parameters)
    // ------------------------------------------------------------------------
    float driveEnabled { 0.0f }; // 0.0 or 1.0
    float driveAmount  { 0.3f }; // 0.0 to 1.0
    float driveTone    { 0.5f }; // 0.0 (dark) to 1.0 (bright)
    float dualMode     { 0.0f }; // 0.0 or 1.0
    float analogMode   { 0.0f }; // 0.0 or 1.0
    float wNoiseMode   { 0.0f }; // 0=Vintage Table, 1=White Noise
    float masterVolume { 0.8f }; // 0.0 to 1.0
};

// POD contract static verification
static_assert(std::is_standard_layout_v<ParameterSnapshot>, "ParameterSnapshot must be standard layout");
static_assert(std::is_trivially_copyable_v<ParameterSnapshot>, "ParameterSnapshot must be trivially copyable");
static_assert(sizeof(ParameterSnapshot) == 55 * sizeof(float), "ParameterSnapshot must contain exactly 55 float parameters");

} // namespace bumbler
