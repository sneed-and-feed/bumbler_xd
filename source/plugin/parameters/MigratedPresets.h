// ============================================================================
// Auto-Generated Presets from Legacy Wasp / Wasp XT Preset Migration Importer
// Generated for Bumbler XD Synthesizer
// ============================================================================
#pragma once

#include "PresetParameters.h"
#include <array>

namespace bumbler {

inline const std::array<PresetDefinition, 15>& getMigratedPresets() {
    static const std::array<PresetDefinition, 15> presets = []() {
        std::array<PresetDefinition, 15> p {};

        // --------------------------------------------------------------------
        // 1. Acid Squelch Bass (Bass)
        // --------------------------------------------------------------------
        p[0].name = "Acid Squelch Bass";
        p[0].category = "Bass";
        p[0].params.osc1Waveform  = 0.00f;
        p[0].params.osc1Octave    = -1.00f;
        p[0].params.osc1Fine      = 0.0000f;
        p[0].params.osc2Waveform  = 1.00f;
        p[0].params.osc2Octave    = -1.00f;
        p[0].params.osc2Fine      = 0.0300f;
        p[0].params.oscMix        = 0.3500f;
        p[0].params.osc3Waveform  = 0.00f;
        p[0].params.osc3Level     = 0.2500f;
        p[0].params.ringModMix    = 0.0000f;
        p[0].params.pulseWidth    = 0.5000f;
        p[0].params.fmAmount      = 0.0000f;
        p[0].params.velToAmp      = 0.7000f;
        p[0].params.velToFilter   = 0.8500f;

        p[0].params.filterMode      = 1.00f;
        p[0].params.filterCutoff    = 340.00f;
        p[0].params.filterResonance = 0.8200f;
        p[0].params.filterKbTrack   = 0.6500f;
        p[0].params.filterEnvAmount = 0.8000f;

        p[0].params.ampAttack     = 0.0020f;
        p[0].params.ampDecay      = 0.2200f;
        p[0].params.ampSustain    = 0.0000f;
        p[0].params.ampRelease    = 0.1400f;
        p[0].params.filterAttack  = 0.0020f;
        p[0].params.filterDecay   = 0.2600f;
        p[0].params.filterSustain = 0.0500f;
        p[0].params.filterRelease = 0.1600f;
        p[0].params.envLink       = 0.00f;

        p[0].params.modAttack     = 0.0100f;
        p[0].params.modDecay      = 0.2000f;
        p[0].params.modAmount     = 0.0000f;
        p[0].params.modTarget     = 0.00f;

        p[0].params.lfo1Waveform = 2.00f;
        p[0].params.lfo1Rate     = 0.5000f;
        p[0].params.lfo1Delay    = 0.0000f;
        p[0].params.lfo1Sync     = 0.00f;
        p[0].params.lfo1SyncDiv  = 3.00f;
        p[0].params.lfo1KeyReset = 1.00f;
        p[0].params.lfo1Amount   = 0.0000f;
        p[0].params.lfo1Target   = 1.00f;

        p[0].params.lfo2Waveform = 2.00f;
        p[0].params.lfo2Rate     = 1.0000f;
        p[0].params.lfo2Delay    = 0.0000f;
        p[0].params.lfo2Sync     = 0.00f;
        p[0].params.lfo2SyncDiv  = 4.00f;
        p[0].params.lfo2KeyReset = 1.00f;
        p[0].params.lfo2Amount   = 0.0000f;
        p[0].params.lfo2Target   = 0.00f;

        p[0].params.driveEnabled = 1.00f;
        p[0].params.driveAmount  = 0.4800f;
        p[0].params.driveTone    = 0.6500f;
        p[0].params.dualMode     = 0.00f;
        p[0].params.analogMode   = 1.00f;
        p[0].params.wNoiseMode   = 0.00f;
        p[0].params.masterVolume = 0.8200f;

        // --------------------------------------------------------------------
        // 2. Reese Grime Bass (Bass)
        // --------------------------------------------------------------------
        p[1].name = "Reese Grime Bass";
        p[1].category = "Bass";
        p[1].params.osc1Waveform  = 0.00f;
        p[1].params.osc1Octave    = -1.00f;
        p[1].params.osc1Fine      = -0.1000f;
        p[1].params.osc2Waveform  = 0.00f;
        p[1].params.osc2Octave    = -1.00f;
        p[1].params.osc2Fine      = 0.1000f;
        p[1].params.oscMix        = 0.5000f;
        p[1].params.osc3Waveform  = 0.00f;
        p[1].params.osc3Level     = 0.3000f;
        p[1].params.ringModMix    = 0.0000f;
        p[1].params.pulseWidth    = 0.5000f;
        p[1].params.fmAmount      = 0.0000f;
        p[1].params.velToAmp      = 0.5000f;
        p[1].params.velToFilter   = 0.5000f;

        p[1].params.filterMode      = 3.00f;
        p[1].params.filterCutoff    = 950.00f;
        p[1].params.filterResonance = 0.6800f;
        p[1].params.filterKbTrack   = 0.4000f;
        p[1].params.filterEnvAmount = 0.3000f;

        p[1].params.ampAttack     = 0.0150f;
        p[1].params.ampDecay      = 0.8000f;
        p[1].params.ampSustain    = 0.8500f;
        p[1].params.ampRelease    = 0.3500f;
        p[1].params.filterAttack  = 0.0200f;
        p[1].params.filterDecay   = 0.9000f;
        p[1].params.filterSustain = 0.7000f;
        p[1].params.filterRelease = 0.3000f;
        p[1].params.envLink       = 0.00f;

        p[1].params.modAttack     = 0.0500f;
        p[1].params.modDecay      = 0.5000f;
        p[1].params.modAmount     = 0.0000f;
        p[1].params.modTarget     = 0.00f;

        p[1].params.lfo1Waveform = 2.00f;
        p[1].params.lfo1Rate     = 0.4500f;
        p[1].params.lfo1Delay    = 0.0000f;
        p[1].params.lfo1Sync     = 0.00f;
        p[1].params.lfo1SyncDiv  = 3.00f;
        p[1].params.lfo1KeyReset = 1.00f;
        p[1].params.lfo1Amount   = 0.3500f;
        p[1].params.lfo1Target   = 1.00f;

        p[1].params.lfo2Waveform = 2.00f;
        p[1].params.lfo2Rate     = 0.2500f;
        p[1].params.lfo2Delay    = 0.0000f;
        p[1].params.lfo2Sync     = 0.00f;
        p[1].params.lfo2SyncDiv  = 4.00f;
        p[1].params.lfo2KeyReset = 1.00f;
        p[1].params.lfo2Amount   = 0.1500f;
        p[1].params.lfo2Target   = 0.00f;

        p[1].params.driveEnabled = 1.00f;
        p[1].params.driveAmount  = 0.5500f;
        p[1].params.driveTone    = 0.5000f;
        p[1].params.dualMode     = 1.00f;
        p[1].params.analogMode   = 1.00f;
        p[1].params.wNoiseMode   = 0.00f;
        p[1].params.masterVolume = 0.7800f;

        // --------------------------------------------------------------------
        // 3. Sub Rumble Bass (Bass)
        // --------------------------------------------------------------------
        p[2].name = "Sub Rumble Bass";
        p[2].category = "Bass";
        p[2].params.osc1Waveform  = 2.00f;
        p[2].params.osc1Octave    = -2.00f;
        p[2].params.osc1Fine      = 0.0000f;
        p[2].params.osc2Waveform  = 0.00f;
        p[2].params.osc2Octave    = -1.00f;
        p[2].params.osc2Fine      = 0.0000f;
        p[2].params.oscMix        = 0.1500f;
        p[2].params.osc3Waveform  = 1.00f;
        p[2].params.osc3Level     = 0.3500f;
        p[2].params.ringModMix    = 0.0000f;
        p[2].params.pulseWidth    = 0.5000f;
        p[2].params.fmAmount      = 0.0000f;
        p[2].params.velToAmp      = 0.6000f;
        p[2].params.velToFilter   = 0.4000f;

        p[2].params.filterMode      = 0.00f;
        p[2].params.filterCutoff    = 220.00f;
        p[2].params.filterResonance = 0.2500f;
        p[2].params.filterKbTrack   = 0.8000f;
        p[2].params.filterEnvAmount = 0.2000f;

        p[2].params.ampAttack     = 0.0050f;
        p[2].params.ampDecay      = 0.6000f;
        p[2].params.ampSustain    = 0.9000f;
        p[2].params.ampRelease    = 0.2500f;
        p[2].params.filterAttack  = 0.0100f;
        p[2].params.filterDecay   = 0.4000f;
        p[2].params.filterSustain = 0.6000f;
        p[2].params.filterRelease = 0.2000f;
        p[2].params.envLink       = 0.00f;

        p[2].params.modAttack     = 0.0500f;
        p[2].params.modDecay      = 0.5000f;
        p[2].params.modAmount     = 0.0000f;
        p[2].params.modTarget     = 0.00f;

        p[2].params.lfo1Waveform = 2.00f;
        p[2].params.lfo1Rate     = 0.3000f;
        p[2].params.lfo1Delay    = 0.0000f;
        p[2].params.lfo1Sync     = 0.00f;
        p[2].params.lfo1SyncDiv  = 3.00f;
        p[2].params.lfo1KeyReset = 1.00f;
        p[2].params.lfo1Amount   = 0.0000f;
        p[2].params.lfo1Target   = 1.00f;

        p[2].params.lfo2Waveform = 2.00f;
        p[2].params.lfo2Rate     = 0.5000f;
        p[2].params.lfo2Delay    = 0.0000f;
        p[2].params.lfo2Sync     = 0.00f;
        p[2].params.lfo2SyncDiv  = 4.00f;
        p[2].params.lfo2KeyReset = 1.00f;
        p[2].params.lfo2Amount   = 0.0000f;
        p[2].params.lfo2Target   = 0.00f;

        p[2].params.driveEnabled = 1.00f;
        p[2].params.driveAmount  = 0.3000f;
        p[2].params.driveTone    = 0.3500f;
        p[2].params.dualMode     = 0.00f;
        p[2].params.analogMode   = 1.00f;
        p[2].params.wNoiseMode   = 0.00f;
        p[2].params.masterVolume = 0.8500f;

        // --------------------------------------------------------------------
        // 4. Cosmic Riser Sweep (FX)
        // --------------------------------------------------------------------
        p[3].name = "Cosmic Riser Sweep";
        p[3].category = "FX";
        p[3].params.osc1Waveform  = 0.00f;
        p[3].params.osc1Octave    = 0.00f;
        p[3].params.osc1Fine      = -0.0500f;
        p[3].params.osc2Waveform  = 3.00f;
        p[3].params.osc2Octave    = 0.00f;
        p[3].params.osc2Fine      = 0.0000f;
        p[3].params.oscMix        = 0.6000f;
        p[3].params.osc3Waveform  = 1.00f;
        p[3].params.osc3Level     = 0.2000f;
        p[3].params.ringModMix    = 0.2000f;
        p[3].params.pulseWidth    = 0.5000f;
        p[3].params.fmAmount      = 0.1000f;
        p[3].params.velToAmp      = 0.3000f;
        p[3].params.velToFilter   = 0.4000f;

        p[3].params.filterMode      = 4.00f;
        p[3].params.filterCutoff    = 400.00f;
        p[3].params.filterResonance = 0.7500f;
        p[3].params.filterKbTrack   = 0.0000f;
        p[3].params.filterEnvAmount = 0.8500f;

        p[3].params.ampAttack     = 2.0000f;
        p[3].params.ampDecay      = 3.0000f;
        p[3].params.ampSustain    = 0.8000f;
        p[3].params.ampRelease    = 1.5000f;
        p[3].params.filterAttack  = 3.5000f;
        p[3].params.filterDecay   = 2.0000f;
        p[3].params.filterSustain = 0.9000f;
        p[3].params.filterRelease = 1.2000f;
        p[3].params.envLink       = 0.00f;

        p[3].params.modAttack     = 1.5000f;
        p[3].params.modDecay      = 2.5000f;
        p[3].params.modAmount     = 0.4000f;
        p[3].params.modTarget     = 0.00f;

        p[3].params.lfo1Waveform = 0.00f;
        p[3].params.lfo1Rate     = 8.0000f;
        p[3].params.lfo1Delay    = 0.0000f;
        p[3].params.lfo1Sync     = 0.00f;
        p[3].params.lfo1SyncDiv  = 3.00f;
        p[3].params.lfo1KeyReset = 1.00f;
        p[3].params.lfo1Amount   = 0.3000f;
        p[3].params.lfo1Target   = 1.00f;

        p[3].params.lfo2Waveform = 2.00f;
        p[3].params.lfo2Rate     = 0.2000f;
        p[3].params.lfo2Delay    = 0.0000f;
        p[3].params.lfo2Sync     = 0.00f;
        p[3].params.lfo2SyncDiv  = 4.00f;
        p[3].params.lfo2KeyReset = 1.00f;
        p[3].params.lfo2Amount   = 0.0000f;
        p[3].params.lfo2Target   = 0.00f;

        p[3].params.driveEnabled = 1.00f;
        p[3].params.driveAmount  = 0.4500f;
        p[3].params.driveTone    = 0.7000f;
        p[3].params.dualMode     = 1.00f;
        p[3].params.analogMode   = 1.00f;
        p[3].params.wNoiseMode   = 1.00f;
        p[3].params.masterVolume = 0.7800f;

        // --------------------------------------------------------------------
        // 5. SciFi Laser Dive (FX)
        // --------------------------------------------------------------------
        p[4].name = "SciFi Laser Dive";
        p[4].category = "FX";
        p[4].params.osc1Waveform  = 0.00f;
        p[4].params.osc1Octave    = 1.00f;
        p[4].params.osc1Fine      = 0.0000f;
        p[4].params.osc2Waveform  = 1.00f;
        p[4].params.osc2Octave    = 0.00f;
        p[4].params.osc2Fine      = 0.2000f;
        p[4].params.oscMix        = 0.5000f;
        p[4].params.osc3Waveform  = 0.00f;
        p[4].params.osc3Level     = 0.0000f;
        p[4].params.ringModMix    = 0.3500f;
        p[4].params.pulseWidth    = 0.5000f;
        p[4].params.fmAmount      = 0.5500f;
        p[4].params.velToAmp      = 0.5000f;
        p[4].params.velToFilter   = 0.5000f;

        p[4].params.filterMode      = 2.00f;
        p[4].params.filterCutoff    = 3500.00f;
        p[4].params.filterResonance = 0.8000f;
        p[4].params.filterKbTrack   = 0.0000f;
        p[4].params.filterEnvAmount = 0.7500f;

        p[4].params.ampAttack     = 0.0020f;
        p[4].params.ampDecay      = 0.8000f;
        p[4].params.ampSustain    = 0.0000f;
        p[4].params.ampRelease    = 0.3500f;
        p[4].params.filterAttack  = 0.0050f;
        p[4].params.filterDecay   = 0.6000f;
        p[4].params.filterSustain = 0.0000f;
        p[4].params.filterRelease = 0.3000f;
        p[4].params.envLink       = 0.00f;

        p[4].params.modAttack     = 0.0020f;
        p[4].params.modDecay      = 0.6500f;
        p[4].params.modAmount     = 0.9500f;
        p[4].params.modTarget     = 3.00f;

        p[4].params.lfo1Waveform = 2.00f;
        p[4].params.lfo1Rate     = 8.0000f;
        p[4].params.lfo1Delay    = 0.0000f;
        p[4].params.lfo1Sync     = 0.00f;
        p[4].params.lfo1SyncDiv  = 3.00f;
        p[4].params.lfo1KeyReset = 1.00f;
        p[4].params.lfo1Amount   = 0.2500f;
        p[4].params.lfo1Target   = 1.00f;

        p[4].params.lfo2Waveform = 2.00f;
        p[4].params.lfo2Rate     = 0.5000f;
        p[4].params.lfo2Delay    = 0.0000f;
        p[4].params.lfo2Sync     = 0.00f;
        p[4].params.lfo2SyncDiv  = 4.00f;
        p[4].params.lfo2KeyReset = 1.00f;
        p[4].params.lfo2Amount   = 0.0000f;
        p[4].params.lfo2Target   = 0.00f;

        p[4].params.driveEnabled = 1.00f;
        p[4].params.driveAmount  = 0.5000f;
        p[4].params.driveTone    = 0.7500f;
        p[4].params.dualMode     = 1.00f;
        p[4].params.analogMode   = 1.00f;
        p[4].params.wNoiseMode   = 0.00f;
        p[4].params.masterVolume = 0.8000f;

        // --------------------------------------------------------------------
        // 6. Formant Vocal Lead (Lead)
        // --------------------------------------------------------------------
        p[5].name = "Formant Vocal Lead";
        p[5].category = "Lead";
        p[5].params.osc1Waveform  = 1.00f;
        p[5].params.osc1Octave    = 0.00f;
        p[5].params.osc1Fine      = 0.0000f;
        p[5].params.osc2Waveform  = 1.00f;
        p[5].params.osc2Octave    = 1.00f;
        p[5].params.osc2Fine      = 0.0500f;
        p[5].params.oscMix        = 0.4500f;
        p[5].params.osc3Waveform  = 0.00f;
        p[5].params.osc3Level     = 0.0000f;
        p[5].params.ringModMix    = 0.0500f;
        p[5].params.pulseWidth    = 0.4000f;
        p[5].params.fmAmount      = 0.1000f;
        p[5].params.velToAmp      = 0.6000f;
        p[5].params.velToFilter   = 0.6500f;

        p[5].params.filterMode      = 2.00f;
        p[5].params.filterCutoff    = 1400.00f;
        p[5].params.filterResonance = 0.7500f;
        p[5].params.filterKbTrack   = 0.5000f;
        p[5].params.filterEnvAmount = 0.5000f;

        p[5].params.ampAttack     = 0.0100f;
        p[5].params.ampDecay      = 0.6000f;
        p[5].params.ampSustain    = 0.7500f;
        p[5].params.ampRelease    = 0.3000f;
        p[5].params.filterAttack  = 0.0300f;
        p[5].params.filterDecay   = 0.5500f;
        p[5].params.filterSustain = 0.4500f;
        p[5].params.filterRelease = 0.3000f;
        p[5].params.envLink       = 0.00f;

        p[5].params.modAttack     = 0.0400f;
        p[5].params.modDecay      = 0.4500f;
        p[5].params.modAmount     = 0.4500f;
        p[5].params.modTarget     = 0.00f;

        p[5].params.lfo1Waveform = 2.00f;
        p[5].params.lfo1Rate     = 4.8000f;
        p[5].params.lfo1Delay    = 0.2000f;
        p[5].params.lfo1Sync     = 0.00f;
        p[5].params.lfo1SyncDiv  = 3.00f;
        p[5].params.lfo1KeyReset = 1.00f;
        p[5].params.lfo1Amount   = 0.1500f;
        p[5].params.lfo1Target   = 1.00f;

        p[5].params.lfo2Waveform = 2.00f;
        p[5].params.lfo2Rate     = 0.5000f;
        p[5].params.lfo2Delay    = 0.0000f;
        p[5].params.lfo2Sync     = 0.00f;
        p[5].params.lfo2SyncDiv  = 4.00f;
        p[5].params.lfo2KeyReset = 1.00f;
        p[5].params.lfo2Amount   = 0.0000f;
        p[5].params.lfo2Target   = 0.00f;

        p[5].params.driveEnabled = 1.00f;
        p[5].params.driveAmount  = 0.3000f;
        p[5].params.driveTone    = 0.6000f;
        p[5].params.dualMode     = 1.00f;
        p[5].params.analogMode   = 1.00f;
        p[5].params.wNoiseMode   = 0.00f;
        p[5].params.masterVolume = 0.7800f;

        // --------------------------------------------------------------------
        // 7. Piercing Sync Lead (Lead)
        // --------------------------------------------------------------------
        p[6].name = "Piercing Sync Lead";
        p[6].category = "Lead";
        p[6].params.osc1Waveform  = 0.00f;
        p[6].params.osc1Octave    = 0.00f;
        p[6].params.osc1Fine      = 0.0000f;
        p[6].params.osc2Waveform  = 0.00f;
        p[6].params.osc2Octave    = 1.00f;
        p[6].params.osc2Fine      = 0.0800f;
        p[6].params.oscMix        = 0.5000f;
        p[6].params.osc3Waveform  = 0.00f;
        p[6].params.osc3Level     = 0.0000f;
        p[6].params.ringModMix    = 0.1500f;
        p[6].params.pulseWidth    = 0.5000f;
        p[6].params.fmAmount      = 0.3500f;
        p[6].params.velToAmp      = 0.5000f;
        p[6].params.velToFilter   = 0.4000f;

        p[6].params.filterMode      = 2.00f;
        p[6].params.filterCutoff    = 2600.00f;
        p[6].params.filterResonance = 0.5800f;
        p[6].params.filterKbTrack   = 0.7000f;
        p[6].params.filterEnvAmount = 0.3500f;

        p[6].params.ampAttack     = 0.0050f;
        p[6].params.ampDecay      = 0.5000f;
        p[6].params.ampSustain    = 0.8000f;
        p[6].params.ampRelease    = 0.3000f;
        p[6].params.filterAttack  = 0.0200f;
        p[6].params.filterDecay   = 0.7000f;
        p[6].params.filterSustain = 0.5000f;
        p[6].params.filterRelease = 0.3500f;
        p[6].params.envLink       = 0.00f;

        p[6].params.modAttack     = 0.0050f;
        p[6].params.modDecay      = 0.2500f;
        p[6].params.modAmount     = 0.3500f;
        p[6].params.modTarget     = 3.00f;

        p[6].params.lfo1Waveform = 2.00f;
        p[6].params.lfo1Rate     = 5.5000f;
        p[6].params.lfo1Delay    = 0.3000f;
        p[6].params.lfo1Sync     = 0.00f;
        p[6].params.lfo1SyncDiv  = 3.00f;
        p[6].params.lfo1KeyReset = 1.00f;
        p[6].params.lfo1Amount   = 0.2000f;
        p[6].params.lfo1Target   = 0.00f;

        p[6].params.lfo2Waveform = 2.00f;
        p[6].params.lfo2Rate     = 1.0000f;
        p[6].params.lfo2Delay    = 0.0000f;
        p[6].params.lfo2Sync     = 0.00f;
        p[6].params.lfo2SyncDiv  = 4.00f;
        p[6].params.lfo2KeyReset = 1.00f;
        p[6].params.lfo2Amount   = 0.0000f;
        p[6].params.lfo2Target   = 0.00f;

        p[6].params.driveEnabled = 1.00f;
        p[6].params.driveAmount  = 0.4000f;
        p[6].params.driveTone    = 0.7500f;
        p[6].params.dualMode     = 1.00f;
        p[6].params.analogMode   = 1.00f;
        p[6].params.wNoiseMode   = 0.00f;
        p[6].params.masterVolume = 0.7800f;

        // --------------------------------------------------------------------
        // 8. Screamer Rave Stab (Lead)
        // --------------------------------------------------------------------
        p[7].name = "Screamer Rave Stab";
        p[7].category = "Lead";
        p[7].params.osc1Waveform  = 1.00f;
        p[7].params.osc1Octave    = 0.00f;
        p[7].params.osc1Fine      = 0.0000f;
        p[7].params.osc2Waveform  = 0.00f;
        p[7].params.osc2Octave    = 0.00f;
        p[7].params.osc2Fine      = 0.1500f;
        p[7].params.oscMix        = 0.5000f;
        p[7].params.osc3Waveform  = 0.00f;
        p[7].params.osc3Level     = 0.1000f;
        p[7].params.ringModMix    = 0.0000f;
        p[7].params.pulseWidth    = 0.3500f;
        p[7].params.fmAmount      = 0.0000f;
        p[7].params.velToAmp      = 0.7500f;
        p[7].params.velToFilter   = 0.8000f;

        p[7].params.filterMode      = 4.00f;
        p[7].params.filterCutoff    = 1850.00f;
        p[7].params.filterResonance = 0.7200f;
        p[7].params.filterKbTrack   = 0.8500f;
        p[7].params.filterEnvAmount = 0.4500f;

        p[7].params.ampAttack     = 0.0020f;
        p[7].params.ampDecay      = 0.3500f;
        p[7].params.ampSustain    = 0.4000f;
        p[7].params.ampRelease    = 0.2000f;
        p[7].params.filterAttack  = 0.0020f;
        p[7].params.filterDecay   = 0.2800f;
        p[7].params.filterSustain = 0.2000f;
        p[7].params.filterRelease = 0.1800f;
        p[7].params.envLink       = 0.00f;

        p[7].params.modAttack     = 0.0020f;
        p[7].params.modDecay      = 0.1500f;
        p[7].params.modAmount     = 0.3000f;
        p[7].params.modTarget     = 3.00f;

        p[7].params.lfo1Waveform = 2.00f;
        p[7].params.lfo1Rate     = 2.0000f;
        p[7].params.lfo1Delay    = 0.0000f;
        p[7].params.lfo1Sync     = 0.00f;
        p[7].params.lfo1SyncDiv  = 3.00f;
        p[7].params.lfo1KeyReset = 1.00f;
        p[7].params.lfo1Amount   = 0.0000f;
        p[7].params.lfo1Target   = 1.00f;

        p[7].params.lfo2Waveform = 2.00f;
        p[7].params.lfo2Rate     = 1.0000f;
        p[7].params.lfo2Delay    = 0.0000f;
        p[7].params.lfo2Sync     = 0.00f;
        p[7].params.lfo2SyncDiv  = 4.00f;
        p[7].params.lfo2KeyReset = 1.00f;
        p[7].params.lfo2Amount   = 0.0000f;
        p[7].params.lfo2Target   = 0.00f;

        p[7].params.driveEnabled = 1.00f;
        p[7].params.driveAmount  = 0.6500f;
        p[7].params.driveTone    = 0.8500f;
        p[7].params.dualMode     = 1.00f;
        p[7].params.analogMode   = 1.00f;
        p[7].params.wNoiseMode   = 0.00f;
        p[7].params.masterVolume = 0.8000f;

        // --------------------------------------------------------------------
        // 9. Celestial Choir Pad (Pad)
        // --------------------------------------------------------------------
        p[8].name = "Celestial Choir Pad";
        p[8].category = "Pad";
        p[8].params.osc1Waveform  = 1.00f;
        p[8].params.osc1Octave    = 0.00f;
        p[8].params.osc1Fine      = -0.0600f;
        p[8].params.osc2Waveform  = 0.00f;
        p[8].params.osc2Octave    = 0.00f;
        p[8].params.osc2Fine      = 0.0600f;
        p[8].params.oscMix        = 0.5000f;
        p[8].params.osc3Waveform  = 1.00f;
        p[8].params.osc3Level     = 0.2000f;
        p[8].params.ringModMix    = 0.0000f;
        p[8].params.pulseWidth    = 0.5000f;
        p[8].params.fmAmount      = 0.0000f;
        p[8].params.velToAmp      = 0.3000f;
        p[8].params.velToFilter   = 0.4000f;

        p[8].params.filterMode      = 3.00f;
        p[8].params.filterCutoff    = 1650.00f;
        p[8].params.filterResonance = 0.6500f;
        p[8].params.filterKbTrack   = 0.5000f;
        p[8].params.filterEnvAmount = 0.2500f;

        p[8].params.ampAttack     = 1.2000f;
        p[8].params.ampDecay      = 2.5000f;
        p[8].params.ampSustain    = 0.8500f;
        p[8].params.ampRelease    = 2.0000f;
        p[8].params.filterAttack  = 1.5000f;
        p[8].params.filterDecay   = 3.0000f;
        p[8].params.filterSustain = 0.7000f;
        p[8].params.filterRelease = 2.2000f;
        p[8].params.envLink       = 0.00f;

        p[8].params.modAttack     = 0.8000f;
        p[8].params.modDecay      = 3.0000f;
        p[8].params.modAmount     = 0.2500f;
        p[8].params.modTarget     = 0.00f;

        p[8].params.lfo1Waveform = 2.00f;
        p[8].params.lfo1Rate     = 0.2800f;
        p[8].params.lfo1Delay    = 0.0000f;
        p[8].params.lfo1Sync     = 0.00f;
        p[8].params.lfo1SyncDiv  = 3.00f;
        p[8].params.lfo1KeyReset = 1.00f;
        p[8].params.lfo1Amount   = 0.4000f;
        p[8].params.lfo1Target   = 2.00f;

        p[8].params.lfo2Waveform = 2.00f;
        p[8].params.lfo2Rate     = 0.1800f;
        p[8].params.lfo2Delay    = 0.0000f;
        p[8].params.lfo2Sync     = 0.00f;
        p[8].params.lfo2SyncDiv  = 4.00f;
        p[8].params.lfo2KeyReset = 1.00f;
        p[8].params.lfo2Amount   = 0.2500f;
        p[8].params.lfo2Target   = 1.00f;

        p[8].params.driveEnabled = 0.00f;
        p[8].params.driveAmount  = 0.3000f;
        p[8].params.driveTone    = 0.5000f;
        p[8].params.dualMode     = 1.00f;
        p[8].params.analogMode   = 1.00f;
        p[8].params.wNoiseMode   = 0.00f;
        p[8].params.masterVolume = 0.7500f;

        // --------------------------------------------------------------------
        // 10. Dark Atmosphere Drone (Pad)
        // --------------------------------------------------------------------
        p[9].name = "Dark Atmosphere Drone";
        p[9].category = "Pad";
        p[9].params.osc1Waveform  = 0.00f;
        p[9].params.osc1Octave    = -2.00f;
        p[9].params.osc1Fine      = -0.0400f;
        p[9].params.osc2Waveform  = 3.00f;
        p[9].params.osc2Octave    = -1.00f;
        p[9].params.osc2Fine      = 0.0000f;
        p[9].params.oscMix        = 0.6000f;
        p[9].params.osc3Waveform  = 1.00f;
        p[9].params.osc3Level     = 0.3500f;
        p[9].params.ringModMix    = 0.2000f;
        p[9].params.pulseWidth    = 0.5000f;
        p[9].params.fmAmount      = 0.0500f;
        p[9].params.velToAmp      = 0.2000f;
        p[9].params.velToFilter   = 0.2000f;

        p[9].params.filterMode      = 5.00f;
        p[9].params.filterCutoff    = 260.00f;
        p[9].params.filterResonance = 0.6500f;
        p[9].params.filterKbTrack   = 0.3000f;
        p[9].params.filterEnvAmount = -0.2500f;

        p[9].params.ampAttack     = 2.5000f;
        p[9].params.ampDecay      = 3.5000f;
        p[9].params.ampSustain    = 1.0000f;
        p[9].params.ampRelease    = 3.0000f;
        p[9].params.filterAttack  = 3.0000f;
        p[9].params.filterDecay   = 4.0000f;
        p[9].params.filterSustain = 0.9000f;
        p[9].params.filterRelease = 3.0000f;
        p[9].params.envLink       = 0.00f;

        p[9].params.modAttack     = 1.0000f;
        p[9].params.modDecay      = 4.0000f;
        p[9].params.modAmount     = 0.2500f;
        p[9].params.modTarget     = 1.00f;

        p[9].params.lfo1Waveform = 3.00f;
        p[9].params.lfo1Rate     = 0.7500f;
        p[9].params.lfo1Delay    = 0.0000f;
        p[9].params.lfo1Sync     = 0.00f;
        p[9].params.lfo1SyncDiv  = 3.00f;
        p[9].params.lfo1KeyReset = 1.00f;
        p[9].params.lfo1Amount   = 0.3000f;
        p[9].params.lfo1Target   = 1.00f;

        p[9].params.lfo2Waveform = 2.00f;
        p[9].params.lfo2Rate     = 0.0800f;
        p[9].params.lfo2Delay    = 0.0000f;
        p[9].params.lfo2Sync     = 0.00f;
        p[9].params.lfo2SyncDiv  = 4.00f;
        p[9].params.lfo2KeyReset = 1.00f;
        p[9].params.lfo2Amount   = 0.1200f;
        p[9].params.lfo2Target   = 0.00f;

        p[9].params.driveEnabled = 1.00f;
        p[9].params.driveAmount  = 0.2500f;
        p[9].params.driveTone    = 0.3000f;
        p[9].params.dualMode     = 1.00f;
        p[9].params.analogMode   = 1.00f;
        p[9].params.wNoiseMode   = 0.00f;
        p[9].params.masterVolume = 0.7400f;

        // --------------------------------------------------------------------
        // 11. Vintage String Machine (Pad)
        // --------------------------------------------------------------------
        p[10].name = "Vintage String Machine";
        p[10].category = "Pad";
        p[10].params.osc1Waveform  = 0.00f;
        p[10].params.osc1Octave    = 0.00f;
        p[10].params.osc1Fine      = -0.0800f;
        p[10].params.osc2Waveform  = 0.00f;
        p[10].params.osc2Octave    = 0.00f;
        p[10].params.osc2Fine      = 0.0800f;
        p[10].params.oscMix        = 0.5000f;
        p[10].params.osc3Waveform  = 1.00f;
        p[10].params.osc3Level     = 0.1500f;
        p[10].params.ringModMix    = 0.0000f;
        p[10].params.pulseWidth    = 0.5000f;
        p[10].params.fmAmount      = 0.0000f;
        p[10].params.velToAmp      = 0.4000f;
        p[10].params.velToFilter   = 0.3000f;

        p[10].params.filterMode      = 1.00f;
        p[10].params.filterCutoff    = 3200.00f;
        p[10].params.filterResonance = 0.3000f;
        p[10].params.filterKbTrack   = 0.8500f;
        p[10].params.filterEnvAmount = 0.2000f;

        p[10].params.ampAttack     = 0.4000f;
        p[10].params.ampDecay      = 1.2000f;
        p[10].params.ampSustain    = 0.8000f;
        p[10].params.ampRelease    = 0.8500f;
        p[10].params.filterAttack  = 0.5000f;
        p[10].params.filterDecay   = 1.5000f;
        p[10].params.filterSustain = 0.7000f;
        p[10].params.filterRelease = 0.9000f;
        p[10].params.envLink       = 0.00f;

        p[10].params.modAttack     = 0.2000f;
        p[10].params.modDecay      = 1.0000f;
        p[10].params.modAmount     = 0.0000f;
        p[10].params.modTarget     = 0.00f;

        p[10].params.lfo1Waveform = 2.00f;
        p[10].params.lfo1Rate     = 5.8000f;
        p[10].params.lfo1Delay    = 0.0000f;
        p[10].params.lfo1Sync     = 0.00f;
        p[10].params.lfo1SyncDiv  = 3.00f;
        p[10].params.lfo1KeyReset = 1.00f;
        p[10].params.lfo1Amount   = 0.0800f;
        p[10].params.lfo1Target   = 0.00f;

        p[10].params.lfo2Waveform = 2.00f;
        p[10].params.lfo2Rate     = 0.8000f;
        p[10].params.lfo2Delay    = 0.0000f;
        p[10].params.lfo2Sync     = 0.00f;
        p[10].params.lfo2SyncDiv  = 4.00f;
        p[10].params.lfo2KeyReset = 1.00f;
        p[10].params.lfo2Amount   = 0.0000f;
        p[10].params.lfo2Target   = 0.00f;

        p[10].params.driveEnabled = 0.00f;
        p[10].params.driveAmount  = 0.3000f;
        p[10].params.driveTone    = 0.5000f;
        p[10].params.dualMode     = 1.00f;
        p[10].params.analogMode   = 1.00f;
        p[10].params.wNoiseMode   = 0.00f;
        p[10].params.masterVolume = 0.7600f;

        // --------------------------------------------------------------------
        // 12. Electro Zap Kick (Percussion)
        // --------------------------------------------------------------------
        p[11].name = "Electro Zap Kick";
        p[11].category = "Percussion";
        p[11].params.osc1Waveform  = 2.00f;
        p[11].params.osc1Octave    = -1.00f;
        p[11].params.osc1Fine      = 0.0000f;
        p[11].params.osc2Waveform  = 2.00f;
        p[11].params.osc2Octave    = -1.00f;
        p[11].params.osc2Fine      = 0.0000f;
        p[11].params.oscMix        = 0.5000f;
        p[11].params.osc3Waveform  = 0.00f;
        p[11].params.osc3Level     = 0.0000f;
        p[11].params.ringModMix    = 0.0000f;
        p[11].params.pulseWidth    = 0.5000f;
        p[11].params.fmAmount      = 0.1000f;
        p[11].params.velToAmp      = 0.9000f;
        p[11].params.velToFilter   = 0.9000f;

        p[11].params.filterMode      = 1.00f;
        p[11].params.filterCutoff    = 450.00f;
        p[11].params.filterResonance = 0.4000f;
        p[11].params.filterKbTrack   = 0.1000f;
        p[11].params.filterEnvAmount = 0.8500f;

        p[11].params.ampAttack     = 0.0010f;
        p[11].params.ampDecay      = 0.1800f;
        p[11].params.ampSustain    = 0.0000f;
        p[11].params.ampRelease    = 0.0800f;
        p[11].params.filterAttack  = 0.0010f;
        p[11].params.filterDecay   = 0.0900f;
        p[11].params.filterSustain = 0.0000f;
        p[11].params.filterRelease = 0.0600f;
        p[11].params.envLink       = 0.00f;

        p[11].params.modAttack     = 0.0010f;
        p[11].params.modDecay      = 0.0450f;
        p[11].params.modAmount     = 0.9000f;
        p[11].params.modTarget     = 3.00f;

        p[11].params.lfo1Waveform = 2.00f;
        p[11].params.lfo1Rate     = 1.0000f;
        p[11].params.lfo1Delay    = 0.0000f;
        p[11].params.lfo1Sync     = 0.00f;
        p[11].params.lfo1SyncDiv  = 3.00f;
        p[11].params.lfo1KeyReset = 1.00f;
        p[11].params.lfo1Amount   = 0.0000f;
        p[11].params.lfo1Target   = 1.00f;

        p[11].params.lfo2Waveform = 2.00f;
        p[11].params.lfo2Rate     = 1.0000f;
        p[11].params.lfo2Delay    = 0.0000f;
        p[11].params.lfo2Sync     = 0.00f;
        p[11].params.lfo2SyncDiv  = 4.00f;
        p[11].params.lfo2KeyReset = 1.00f;
        p[11].params.lfo2Amount   = 0.0000f;
        p[11].params.lfo2Target   = 0.00f;

        p[11].params.driveEnabled = 1.00f;
        p[11].params.driveAmount  = 0.6000f;
        p[11].params.driveTone    = 0.5500f;
        p[11].params.dualMode     = 0.00f;
        p[11].params.analogMode   = 0.00f;
        p[11].params.wNoiseMode   = 0.00f;
        p[11].params.masterVolume = 0.8600f;

        // --------------------------------------------------------------------
        // 13. Snappy Noise Snare (Percussion)
        // --------------------------------------------------------------------
        p[12].name = "Snappy Noise Snare";
        p[12].category = "Percussion";
        p[12].params.osc1Waveform  = 2.00f;
        p[12].params.osc1Octave    = 0.00f;
        p[12].params.osc1Fine      = 0.0000f;
        p[12].params.osc2Waveform  = 3.00f;
        p[12].params.osc2Octave    = 0.00f;
        p[12].params.osc2Fine      = 0.0000f;
        p[12].params.oscMix        = 0.7000f;
        p[12].params.osc3Waveform  = 0.00f;
        p[12].params.osc3Level     = 0.0000f;
        p[12].params.ringModMix    = 0.1500f;
        p[12].params.pulseWidth    = 0.5000f;
        p[12].params.fmAmount      = 0.0000f;
        p[12].params.velToAmp      = 0.8500f;
        p[12].params.velToFilter   = 0.9000f;

        p[12].params.filterMode      = 4.00f;
        p[12].params.filterCutoff    = 1250.00f;
        p[12].params.filterResonance = 0.5500f;
        p[12].params.filterKbTrack   = 0.2000f;
        p[12].params.filterEnvAmount = 0.6500f;

        p[12].params.ampAttack     = 0.0010f;
        p[12].params.ampDecay      = 0.1600f;
        p[12].params.ampSustain    = 0.0000f;
        p[12].params.ampRelease    = 0.1000f;
        p[12].params.filterAttack  = 0.0010f;
        p[12].params.filterDecay   = 0.1200f;
        p[12].params.filterSustain = 0.0000f;
        p[12].params.filterRelease = 0.0800f;
        p[12].params.envLink       = 0.00f;

        p[12].params.modAttack     = 0.0010f;
        p[12].params.modDecay      = 0.0350f;
        p[12].params.modAmount     = 0.7000f;
        p[12].params.modTarget     = 3.00f;

        p[12].params.lfo1Waveform = 2.00f;
        p[12].params.lfo1Rate     = 1.0000f;
        p[12].params.lfo1Delay    = 0.0000f;
        p[12].params.lfo1Sync     = 0.00f;
        p[12].params.lfo1SyncDiv  = 3.00f;
        p[12].params.lfo1KeyReset = 1.00f;
        p[12].params.lfo1Amount   = 0.0000f;
        p[12].params.lfo1Target   = 1.00f;

        p[12].params.lfo2Waveform = 2.00f;
        p[12].params.lfo2Rate     = 1.0000f;
        p[12].params.lfo2Delay    = 0.0000f;
        p[12].params.lfo2Sync     = 0.00f;
        p[12].params.lfo2SyncDiv  = 4.00f;
        p[12].params.lfo2KeyReset = 1.00f;
        p[12].params.lfo2Amount   = 0.0000f;
        p[12].params.lfo2Target   = 0.00f;

        p[12].params.driveEnabled = 1.00f;
        p[12].params.driveAmount  = 0.4500f;
        p[12].params.driveTone    = 0.6500f;
        p[12].params.dualMode     = 0.00f;
        p[12].params.analogMode   = 0.00f;
        p[12].params.wNoiseMode   = 1.00f;
        p[12].params.masterVolume = 0.8400f;

        // --------------------------------------------------------------------
        // 14. Chime Glass Pluck (Pluck)
        // --------------------------------------------------------------------
        p[13].name = "Chime Glass Pluck";
        p[13].category = "Pluck";
        p[13].params.osc1Waveform  = 2.00f;
        p[13].params.osc1Octave    = 0.00f;
        p[13].params.osc1Fine      = 0.0000f;
        p[13].params.osc2Waveform  = 0.00f;
        p[13].params.osc2Octave    = 2.00f;
        p[13].params.osc2Fine      = 0.0200f;
        p[13].params.oscMix        = 0.4500f;
        p[13].params.osc3Waveform  = 0.00f;
        p[13].params.osc3Level     = 0.0000f;
        p[13].params.ringModMix    = 0.4000f;
        p[13].params.pulseWidth    = 0.5000f;
        p[13].params.fmAmount      = 0.1500f;
        p[13].params.velToAmp      = 0.8000f;
        p[13].params.velToFilter   = 0.8500f;

        p[13].params.filterMode      = 4.00f;
        p[13].params.filterCutoff    = 2200.00f;
        p[13].params.filterResonance = 0.6500f;
        p[13].params.filterKbTrack   = 0.8000f;
        p[13].params.filterEnvAmount = 0.6000f;

        p[13].params.ampAttack     = 0.0010f;
        p[13].params.ampDecay      = 0.3200f;
        p[13].params.ampSustain    = 0.0000f;
        p[13].params.ampRelease    = 0.2200f;
        p[13].params.filterAttack  = 0.0010f;
        p[13].params.filterDecay   = 0.2600f;
        p[13].params.filterSustain = 0.0000f;
        p[13].params.filterRelease = 0.1800f;
        p[13].params.envLink       = 0.00f;

        p[13].params.modAttack     = 0.0010f;
        p[13].params.modDecay      = 0.1500f;
        p[13].params.modAmount     = 0.2500f;
        p[13].params.modTarget     = 3.00f;

        p[13].params.lfo1Waveform = 2.00f;
        p[13].params.lfo1Rate     = 4.0000f;
        p[13].params.lfo1Delay    = 0.0000f;
        p[13].params.lfo1Sync     = 0.00f;
        p[13].params.lfo1SyncDiv  = 3.00f;
        p[13].params.lfo1KeyReset = 1.00f;
        p[13].params.lfo1Amount   = 0.0000f;
        p[13].params.lfo1Target   = 1.00f;

        p[13].params.lfo2Waveform = 2.00f;
        p[13].params.lfo2Rate     = 1.0000f;
        p[13].params.lfo2Delay    = 0.0000f;
        p[13].params.lfo2Sync     = 0.00f;
        p[13].params.lfo2SyncDiv  = 4.00f;
        p[13].params.lfo2KeyReset = 1.00f;
        p[13].params.lfo2Amount   = 0.0000f;
        p[13].params.lfo2Target   = 0.00f;

        p[13].params.driveEnabled = 1.00f;
        p[13].params.driveAmount  = 0.2000f;
        p[13].params.driveTone    = 0.7000f;
        p[13].params.dualMode     = 1.00f;
        p[13].params.analogMode   = 1.00f;
        p[13].params.wNoiseMode   = 0.00f;
        p[13].params.masterVolume = 0.8000f;

        // --------------------------------------------------------------------
        // 15. Retro Trance Pluck (Pluck)
        // --------------------------------------------------------------------
        p[14].name = "Retro Trance Pluck";
        p[14].category = "Pluck";
        p[14].params.osc1Waveform  = 0.00f;
        p[14].params.osc1Octave    = 0.00f;
        p[14].params.osc1Fine      = 0.0000f;
        p[14].params.osc2Waveform  = 1.00f;
        p[14].params.osc2Octave    = 0.00f;
        p[14].params.osc2Fine      = 0.1000f;
        p[14].params.oscMix        = 0.5000f;
        p[14].params.osc3Waveform  = 0.00f;
        p[14].params.osc3Level     = 0.1500f;
        p[14].params.ringModMix    = 0.0000f;
        p[14].params.pulseWidth    = 0.5000f;
        p[14].params.fmAmount      = 0.0000f;
        p[14].params.velToAmp      = 0.7000f;
        p[14].params.velToFilter   = 0.7500f;

        p[14].params.filterMode      = 1.00f;
        p[14].params.filterCutoff    = 750.00f;
        p[14].params.filterResonance = 0.5500f;
        p[14].params.filterKbTrack   = 0.6000f;
        p[14].params.filterEnvAmount = 0.7000f;

        p[14].params.ampAttack     = 0.0010f;
        p[14].params.ampDecay      = 0.2800f;
        p[14].params.ampSustain    = 0.0500f;
        p[14].params.ampRelease    = 0.1800f;
        p[14].params.filterAttack  = 0.0010f;
        p[14].params.filterDecay   = 0.2200f;
        p[14].params.filterSustain = 0.0000f;
        p[14].params.filterRelease = 0.1600f;
        p[14].params.envLink       = 0.00f;

        p[14].params.modAttack     = 0.0010f;
        p[14].params.modDecay      = 0.1000f;
        p[14].params.modAmount     = 0.0000f;
        p[14].params.modTarget     = 0.00f;

        p[14].params.lfo1Waveform = 2.00f;
        p[14].params.lfo1Rate     = 2.0000f;
        p[14].params.lfo1Delay    = 0.0000f;
        p[14].params.lfo1Sync     = 0.00f;
        p[14].params.lfo1SyncDiv  = 3.00f;
        p[14].params.lfo1KeyReset = 1.00f;
        p[14].params.lfo1Amount   = 0.0000f;
        p[14].params.lfo1Target   = 1.00f;

        p[14].params.lfo2Waveform = 2.00f;
        p[14].params.lfo2Rate     = 1.0000f;
        p[14].params.lfo2Delay    = 0.0000f;
        p[14].params.lfo2Sync     = 0.00f;
        p[14].params.lfo2SyncDiv  = 4.00f;
        p[14].params.lfo2KeyReset = 1.00f;
        p[14].params.lfo2Amount   = 0.0000f;
        p[14].params.lfo2Target   = 0.00f;

        p[14].params.driveEnabled = 1.00f;
        p[14].params.driveAmount  = 0.3500f;
        p[14].params.driveTone    = 0.7000f;
        p[14].params.dualMode     = 1.00f;
        p[14].params.analogMode   = 1.00f;
        p[14].params.wNoiseMode   = 0.00f;
        p[14].params.masterVolume = 0.8200f;

        return p;
    }();
    return presets;
}

} // namespace bumbler
