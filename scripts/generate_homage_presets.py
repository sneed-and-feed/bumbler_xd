#!/usr/bin/env python3
"""
scripts/generate_homage_presets.py
==================================
Generates the 15 Clean-Room Royalty-Free Homage Presets for Bumbler XD.

Categories:
  - Bass (3): Acid Squelch Bass, Sub Rumble Bass, Reese Grime Bass
  - Lead (3): Piercing Sync Lead, Screamer Rave Stab, Formant Vocal Lead
  - Pad (3): Celestial Choir Pad, Dark Atmosphere Drone, Vintage String Machine
  - Pluck (2): Chime Glass Pluck, Retro Trance Pluck
  - Percussion (2): Electro Zap Kick, Snappy Noise Snare
  - FX (2): Sci-Fi Laser Dive, Cosmic Riser Sweep
"""

from __future__ import annotations

import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from scripts.import_wasp_presets import ParsedPreset, export_preset_to_xml


HOMAGE_PRESETS_DATA = [
    # -------------------------------------------------------------------------
    # BASS
    # -------------------------------------------------------------------------
    {
        "name": "Acid Squelch Bass",
        "category": "Bass",
        "params": {
            "osc1Waveform": 0.0,   # Saw
            "osc1Octave": -1.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 1.0,   # Square
            "osc2Octave": -1.0,
            "osc2Fine": 0.03,
            "oscMix": 0.35,        # Favor Saw
            "osc3Waveform": 0.0,   # Square sub
            "osc3Level": 0.25,
            "ringModMix": 0.0,
            "pulseWidth": 0.50,
            "fmAmount": 0.0,
            "velToAmp": 0.70,
            "velToFilter": 0.85,   # Dynamic accent squelch
            "filterMode": 1.0,     # LP24
            "filterCutoff": 340.0, # Low base cutoff
            "filterResonance": 0.82,# Signature squelch
            "filterKbTrack": 0.65,
            "filterEnvAmount": 0.80,# Snappy sweep
            "ampAttack": 0.002,
            "ampDecay": 0.22,
            "ampSustain": 0.0,
            "ampRelease": 0.14,
            "filterAttack": 0.002,
            "filterDecay": 0.26,
            "filterSustain": 0.05,
            "filterRelease": 0.16,
            "envLink": 0.0,
            "modAttack": 0.01,
            "modDecay": 0.20,
            "modAmount": 0.0,
            "modTarget": 0.0,
            "lfo1Waveform": 2.0,
            "lfo1Rate": 0.5,
            "lfo1Amount": 0.0,
            "lfo2Waveform": 2.0,
            "lfo2Rate": 1.0,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,   # Saturation bite
            "driveAmount": 0.48,
            "driveTone": 0.65,
            "dualMode": 0.0,       # Centered mono
            "analogMode": 1.0,     # Free-running drift
            "wNoiseMode": 0.0,
            "masterVolume": 0.82,
        }
    },
    {
        "name": "Sub Rumble Bass",
        "category": "Bass",
        "params": {
            "osc1Waveform": 2.0,   # Sine sub
            "osc1Octave": -2.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 0.0,   # Saw subtle grit
            "osc2Octave": -1.0,
            "osc2Fine": 0.0,
            "oscMix": 0.15,
            "osc3Waveform": 1.0,   # Saw sub
            "osc3Level": 0.35,
            "ringModMix": 0.0,
            "pulseWidth": 0.50,
            "fmAmount": 0.0,
            "velToAmp": 0.60,
            "velToFilter": 0.40,
            "filterMode": 0.0,     # LP12 warm slope
            "filterCutoff": 220.0,
            "filterResonance": 0.25,
            "filterKbTrack": 0.80,
            "filterEnvAmount": 0.20,
            "ampAttack": 0.005,
            "ampDecay": 0.60,
            "ampSustain": 0.90,
            "ampRelease": 0.25,
            "filterAttack": 0.01,
            "filterDecay": 0.40,
            "filterSustain": 0.60,
            "filterRelease": 0.20,
            "envLink": 0.0,
            "modAttack": 0.05,
            "modDecay": 0.50,
            "modAmount": 0.0,
            "modTarget": 0.0,
            "lfo1Waveform": 2.0,
            "lfo1Rate": 0.3,
            "lfo1Amount": 0.0,
            "lfo2Waveform": 2.0,
            "lfo2Rate": 0.5,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.30,   # Warm tube roundness
            "driveTone": 0.35,
            "dualMode": 0.0,       # Rigid mono center
            "analogMode": 1.0,
            "wNoiseMode": 0.0,
            "masterVolume": 0.85,
        }
    },
    {
        "name": "Reese Grime Bass",
        "category": "Bass",
        "params": {
            "osc1Waveform": 0.0,   # Saw
            "osc1Octave": -1.0,
            "osc1Fine": -0.10,     # Detuned
            "osc2Waveform": 0.0,   # Saw
            "osc2Octave": -1.0,
            "osc2Fine": 0.10,      # Detuned
            "oscMix": 0.50,
            "osc3Waveform": 0.0,   # Square sub
            "osc3Level": 0.30,
            "ringModMix": 0.0,
            "pulseWidth": 0.50,
            "fmAmount": 0.0,
            "velToAmp": 0.50,
            "velToFilter": 0.50,
            "filterMode": 3.0,     # DBL.NT (Double Notch)
            "filterCutoff": 950.0,
            "filterResonance": 0.68,
            "filterKbTrack": 0.40,
            "filterEnvAmount": 0.30,
            "ampAttack": 0.015,
            "ampDecay": 0.80,
            "ampSustain": 0.85,
            "ampRelease": 0.35,
            "filterAttack": 0.02,
            "filterDecay": 0.90,
            "filterSustain": 0.70,
            "filterRelease": 0.30,
            "envLink": 0.0,
            "modAttack": 0.05,
            "modDecay": 0.50,
            "modAmount": 0.0,
            "modTarget": 0.0,
            "lfo1Waveform": 2.0,   # Sine
            "lfo1Rate": 0.45,      # Slow notch comb sweep
            "lfo1Amount": 0.35,
            "lfo1Target": 1.0,     # Filter Cutoff
            "lfo2Waveform": 2.0,
            "lfo2Rate": 0.25,
            "lfo2Amount": 0.15,
            "lfo2Target": 0.0,     # Osc 1 pitch warble
            "driveEnabled": 1.0,
            "driveAmount": 0.55,   # Grimy aggressive distortion
            "driveTone": 0.50,
            "dualMode": 1.0,       # Wide stereo reese
            "analogMode": 1.0,
            "wNoiseMode": 0.0,
            "masterVolume": 0.78,
        }
    },

    # -------------------------------------------------------------------------
    # LEAD
    # -------------------------------------------------------------------------
    {
        "name": "Piercing Sync Lead",
        "category": "Lead",
        "params": {
            "osc1Waveform": 0.0,   # Saw
            "osc1Octave": 0.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 0.0,   # Saw
            "osc2Octave": 1.0,     # +1 oct
            "osc2Fine": 0.08,
            "oscMix": 0.50,
            "osc3Waveform": 0.0,
            "osc3Level": 0.0,
            "ringModMix": 0.15,    # Metallic ring
            "pulseWidth": 0.50,
            "fmAmount": 0.35,      # Audio-rate FM bite
            "velToAmp": 0.50,
            "velToFilter": 0.40,
            "filterMode": 2.0,     # LP+NT (formant cascade)
            "filterCutoff": 2600.0,
            "filterResonance": 0.58,
            "filterKbTrack": 0.70,
            "filterEnvAmount": 0.35,
            "ampAttack": 0.005,
            "ampDecay": 0.50,
            "ampSustain": 0.80,
            "ampRelease": 0.30,
            "filterAttack": 0.02,
            "filterDecay": 0.70,
            "filterSustain": 0.50,
            "filterRelease": 0.35,
            "envLink": 0.0,
            "modAttack": 0.005,
            "modDecay": 0.25,
            "modAmount": 0.35,     # Pitch laser dive
            "modTarget": 3.0,      # ModEnv -> Osc 2 Pitch
            "lfo1Waveform": 2.0,
            "lfo1Rate": 5.5,       # Delayed vibrato
            "lfo1Delay": 0.30,
            "lfo1Amount": 0.20,
            "lfo1Target": 0.0,     # Osc 1+2 Pitch
            "lfo2Waveform": 2.0,
            "lfo2Rate": 1.0,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.40,
            "driveTone": 0.75,     # Cutting bite
            "dualMode": 1.0,       # Wide stereo
            "analogMode": 1.0,
            "masterVolume": 0.78,
        }
    },
    {
        "name": "Screamer Rave Stab",
        "category": "Lead",
        "params": {
            "osc1Waveform": 1.0,   # Square
            "osc1Octave": 0.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 0.0,   # Saw
            "osc2Octave": 0.0,
            "osc2Fine": 0.15,      # Detuned rave spread
            "oscMix": 0.50,
            "osc3Waveform": 0.0,
            "osc3Level": 0.10,
            "ringModMix": 0.0,
            "pulseWidth": 0.35,
            "fmAmount": 0.0,
            "velToAmp": 0.75,
            "velToFilter": 0.80,
            "filterMode": 4.0,     # BP24 resonant bandpass
            "filterCutoff": 1850.0,
            "filterResonance": 0.72,
            "filterKbTrack": 0.85,
            "filterEnvAmount": 0.45,
            "ampAttack": 0.002,
            "ampDecay": 0.35,
            "ampSustain": 0.40,
            "ampRelease": 0.20,
            "filterAttack": 0.002,
            "filterDecay": 0.28,
            "filterSustain": 0.20,
            "filterRelease": 0.18,
            "envLink": 0.0,
            "modAttack": 0.002,
            "modDecay": 0.15,
            "modAmount": 0.30,
            "modTarget": 3.0,      # Transient pitch snap
            "lfo1Waveform": 2.0,
            "lfo1Rate": 2.0,
            "lfo1Amount": 0.0,
            "lfo2Waveform": 2.0,
            "lfo2Rate": 1.0,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.65,   # Screaming rave saturation
            "driveTone": 0.85,     # Aggressive presence
            "dualMode": 1.0,       # Wide unison
            "analogMode": 1.0,
            "masterVolume": 0.80,
        }
    },
    {
        "name": "Formant Vocal Lead",
        "category": "Lead",
        "params": {
            "osc1Waveform": 1.0,   # Square
            "osc1Octave": 0.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 1.0,   # Square
            "osc2Octave": 1.0,
            "osc2Fine": 0.05,
            "oscMix": 0.45,
            "osc3Waveform": 0.0,
            "osc3Level": 0.0,
            "ringModMix": 0.05,
            "pulseWidth": 0.40,
            "fmAmount": 0.10,
            "velToAmp": 0.60,
            "velToFilter": 0.65,
            "filterMode": 2.0,     # LP+NT (Vocal formant filter)
            "filterCutoff": 1400.0,
            "filterResonance": 0.75,
            "filterKbTrack": 0.50,
            "filterEnvAmount": 0.50,
            "ampAttack": 0.01,
            "ampDecay": 0.60,
            "ampSustain": 0.75,
            "ampRelease": 0.30,
            "filterAttack": 0.03,
            "filterDecay": 0.55,
            "filterSustain": 0.45,
            "filterRelease": 0.30,
            "envLink": 0.0,
            "modAttack": 0.04,     # Talking vowel transition
            "modDecay": 0.45,
            "modAmount": 0.45,
            "modTarget": 0.0,      # ModEnv -> Pulse Width
            "lfo1Waveform": 2.0,
            "lfo1Rate": 4.8,
            "lfo1Delay": 0.20,
            "lfo1Amount": 0.15,
            "lfo1Target": 1.0,     # Filter Cutoff
            "lfo2Waveform": 2.0,
            "lfo2Rate": 0.5,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.30,
            "driveTone": 0.60,
            "dualMode": 1.0,
            "analogMode": 1.0,
            "masterVolume": 0.78,
        }
    },

    # -------------------------------------------------------------------------
    # PAD
    # -------------------------------------------------------------------------
    {
        "name": "Celestial Choir Pad",
        "category": "Pad",
        "params": {
            "osc1Waveform": 1.0,   # Square
            "osc1Octave": 0.0,
            "osc1Fine": -0.06,
            "osc2Waveform": 0.0,   # Saw
            "osc2Octave": 0.0,
            "osc2Fine": 0.06,
            "oscMix": 0.50,
            "osc3Waveform": 1.0,   # Saw sub
            "osc3Level": 0.20,
            "ringModMix": 0.0,
            "pulseWidth": 0.50,
            "fmAmount": 0.0,
            "velToAmp": 0.30,
            "velToFilter": 0.40,
            "filterMode": 3.0,     # DBL.NT (Double Notch)
            "filterCutoff": 1650.0,
            "filterResonance": 0.65,
            "filterKbTrack": 0.50,
            "filterEnvAmount": 0.25,
            "ampAttack": 1.20,     # Blooming swell
            "ampDecay": 2.50,
            "ampSustain": 0.85,
            "ampRelease": 2.00,
            "filterAttack": 1.50,
            "filterDecay": 3.00,
            "filterSustain": 0.70,
            "filterRelease": 2.20,
            "envLink": 0.0,
            "modAttack": 0.80,
            "modDecay": 3.00,
            "modAmount": 0.25,
            "modTarget": 0.0,
            "lfo1Waveform": 2.0,   # Sine
            "lfo1Rate": 0.28,      # Slow PWM sweep
            "lfo1Amount": 0.40,
            "lfo1Target": 2.0,     # Pulse Width
            "lfo2Waveform": 2.0,
            "lfo2Rate": 0.18,      # Gentle mix panning
            "lfo2Amount": 0.25,
            "lfo2Target": 1.0,     # Osc Mix
            "driveEnabled": 0.0,
            "dualMode": 1.0,       # Majestic stereo spread
            "analogMode": 1.0,
            "masterVolume": 0.75,
        }
    },
    {
        "name": "Dark Atmosphere Drone",
        "category": "Pad",
        "params": {
            "osc1Waveform": 0.0,   # Saw
            "osc1Octave": -2.0,    # Sub rumble
            "osc1Fine": -0.04,
            "osc2Waveform": 3.0,   # Noise bed
            "osc2Octave": -1.0,
            "oscMix": 0.60,
            "osc3Waveform": 1.0,
            "osc3Level": 0.35,
            "ringModMix": 0.20,    # Metallic undertone
            "pulseWidth": 0.50,
            "fmAmount": 0.05,
            "velToAmp": 0.20,
            "velToFilter": 0.20,
            "filterMode": 5.0,     # HP24 (Highpass resonance)
            "filterCutoff": 260.0,
            "filterResonance": 0.65,
            "filterKbTrack": 0.30,
            "filterEnvAmount": -0.25,# Inverted breathing
            "ampAttack": 2.50,
            "ampDecay": 3.50,
            "ampSustain": 1.00,    # Infinite hold drone
            "ampRelease": 3.00,
            "filterAttack": 3.00,
            "filterDecay": 4.00,
            "filterSustain": 0.90,
            "filterRelease": 3.00,
            "envLink": 0.0,
            "modAttack": 1.00,
            "modDecay": 4.00,
            "modAmount": 0.25,
            "modTarget": 1.0,      # ModEnv -> Lfo1 Amt
            "lfo1Waveform": 3.0,   # Sample & Hold Noise
            "lfo1Rate": 0.75,      # Random cutoff wander
            "lfo1Amount": 0.30,
            "lfo1Target": 1.0,     # Filter Cutoff
            "lfo2Waveform": 2.0,   # Sine
            "lfo2Rate": 0.08,      # Subterranean pitch warble
            "lfo2Amount": 0.12,
            "lfo2Target": 0.0,     # Osc 1 Pitch
            "driveEnabled": 1.0,
            "driveAmount": 0.25,
            "driveTone": 0.30,     # Dark vintage warmth
            "dualMode": 1.0,
            "analogMode": 1.0,
            "wNoiseMode": 0.0,     # Vintage Table Noise
            "masterVolume": 0.74,
        }
    },
    {
        "name": "Vintage String Machine",
        "category": "Pad",
        "params": {
            "osc1Waveform": 0.0,   # Saw
            "osc1Octave": 0.0,
            "osc1Fine": -0.08,
            "osc2Waveform": 0.0,   # Saw
            "osc2Octave": 0.0,
            "osc2Fine": 0.08,
            "oscMix": 0.50,
            "osc3Waveform": 1.0,
            "osc3Level": 0.15,
            "ringModMix": 0.0,
            "pulseWidth": 0.50,
            "fmAmount": 0.0,
            "velToAmp": 0.40,
            "velToFilter": 0.30,
            "filterMode": 1.0,     # LP24
            "filterCutoff": 3200.0,
            "filterResonance": 0.30,
            "filterKbTrack": 0.85,
            "filterEnvAmount": 0.20,
            "ampAttack": 0.40,     # String ensemble attack
            "ampDecay": 1.20,
            "ampSustain": 0.80,
            "ampRelease": 0.85,
            "filterAttack": 0.50,
            "filterDecay": 1.50,
            "filterSustain": 0.70,
            "filterRelease": 0.90,
            "envLink": 0.0,
            "modAttack": 0.20,
            "modDecay": 1.00,
            "modAmount": 0.0,
            "modTarget": 0.0,
            "lfo1Waveform": 2.0,   # Sine
            "lfo1Rate": 5.8,       # Analog ensemble chorus rate
            "lfo1Amount": 0.08,
            "lfo1Target": 0.0,     # Osc 1+2 Pitch chorusing
            "lfo2Waveform": 2.0,
            "lfo2Rate": 0.8,
            "lfo2Amount": 0.0,
            "driveEnabled": 0.0,
            "dualMode": 1.0,       # Wide stereo string spread
            "analogMode": 1.0,
            "masterVolume": 0.76,
        }
    },

    # -------------------------------------------------------------------------
    # PLUCK
    # -------------------------------------------------------------------------
    {
        "name": "Chime Glass Pluck",
        "category": "Pluck",
        "params": {
            "osc1Waveform": 2.0,   # Sine
            "osc1Octave": 0.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 0.0,   # Saw
            "osc2Octave": 2.0,     # +2 oct
            "osc2Fine": 0.02,
            "oscMix": 0.45,
            "osc3Waveform": 0.0,
            "osc3Level": 0.0,
            "ringModMix": 0.40,    # Bell metallic harmonics
            "pulseWidth": 0.50,
            "fmAmount": 0.15,
            "velToAmp": 0.80,
            "velToFilter": 0.85,
            "filterMode": 4.0,     # BP24 (Bandpass shimmer)
            "filterCutoff": 2200.0,
            "filterResonance": 0.65,
            "filterKbTrack": 0.80,
            "filterEnvAmount": 0.60,
            "ampAttack": 0.001,    # Instant attack
            "ampDecay": 0.32,
            "ampSustain": 0.0,
            "ampRelease": 0.22,
            "filterAttack": 0.001,
            "filterDecay": 0.26,
            "filterSustain": 0.0,
            "filterRelease": 0.18,
            "envLink": 0.0,
            "modAttack": 0.001,
            "modDecay": 0.15,
            "modAmount": 0.25,     # Pitch sparkle
            "modTarget": 3.0,      # Osc 2 pitch
            "lfo1Waveform": 2.0,
            "lfo1Rate": 4.0,
            "lfo1Amount": 0.0,
            "lfo2Waveform": 2.0,
            "lfo2Rate": 1.0,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.20,
            "driveTone": 0.70,     # Crystalline brilliance
            "dualMode": 1.0,
            "analogMode": 1.0,
            "masterVolume": 0.80,
        }
    },
    {
        "name": "Retro Trance Pluck",
        "category": "Pluck",
        "params": {
            "osc1Waveform": 0.0,   # Saw
            "osc1Octave": 0.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 1.0,   # Square
            "osc2Octave": 0.0,
            "osc2Fine": 0.10,      # Detuned
            "oscMix": 0.50,
            "osc3Waveform": 0.0,   # Sub
            "osc3Level": 0.15,
            "ringModMix": 0.0,
            "pulseWidth": 0.50,
            "fmAmount": 0.0,
            "velToAmp": 0.70,
            "velToFilter": 0.75,
            "filterMode": 1.0,     # LP24
            "filterCutoff": 750.0, # Snappy base cutoff
            "filterResonance": 0.55,
            "filterKbTrack": 0.60,
            "filterEnvAmount": 0.70,# Hard pluck plunge
            "ampAttack": 0.001,
            "ampDecay": 0.28,
            "ampSustain": 0.05,
            "ampRelease": 0.18,
            "filterAttack": 0.001,
            "filterDecay": 0.22,
            "filterSustain": 0.0,
            "filterRelease": 0.16,
            "envLink": 0.0,
            "modAttack": 0.001,
            "modDecay": 0.10,
            "modAmount": 0.0,
            "modTarget": 0.0,
            "lfo1Waveform": 2.0,
            "lfo1Rate": 2.0,
            "lfo1Amount": 0.0,
            "lfo2Waveform": 2.0,
            "lfo2Rate": 1.0,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.35,
            "driveTone": 0.70,
            "dualMode": 1.0,       # Wide trance pluck
            "analogMode": 1.0,
            "masterVolume": 0.82,
        }
    },

    # -------------------------------------------------------------------------
    # PERCUSSION
    # -------------------------------------------------------------------------
    {
        "name": "Electro Zap Kick",
        "category": "Percussion",
        "params": {
            "osc1Waveform": 2.0,   # Sine thump
            "osc1Octave": -1.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 2.0,   # Sine
            "osc2Octave": -1.0,
            "osc2Fine": 0.0,
            "oscMix": 0.50,
            "osc3Waveform": 0.0,
            "osc3Level": 0.0,
            "ringModMix": 0.0,
            "pulseWidth": 0.50,
            "fmAmount": 0.10,      # Transient FM click
            "velToAmp": 0.90,
            "velToFilter": 0.90,
            "filterMode": 1.0,     # LP24
            "filterCutoff": 450.0,
            "filterResonance": 0.40,
            "filterKbTrack": 0.10,
            "filterEnvAmount": 0.85,
            "ampAttack": 0.001,    # Instant attack
            "ampDecay": 0.18,
            "ampSustain": 0.0,
            "ampRelease": 0.08,
            "filterAttack": 0.001,
            "filterDecay": 0.09,
            "filterSustain": 0.0,
            "filterRelease": 0.06,
            "envLink": 0.0,
            "modAttack": 0.001,
            "modDecay": 0.045,     # Ultra-fast zap pitch plunge
            "modAmount": 0.90,
            "modTarget": 3.0,      # Osc 2 pitch
            "lfo1Waveform": 2.0,
            "lfo1Rate": 1.0,
            "lfo1Amount": 0.0,
            "lfo2Waveform": 2.0,
            "lfo2Rate": 1.0,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.60,   # Heavy transient saturation
            "driveTone": 0.55,
            "dualMode": 0.0,       # Centered punch
            "analogMode": 0.0,     # Phase-locked attack
            "masterVolume": 0.86,
        }
    },
    {
        "name": "Snappy Noise Snare",
        "category": "Percussion",
        "params": {
            "osc1Waveform": 2.0,   # Sine tuned body
            "osc1Octave": 0.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 3.0,   # Noise snap
            "osc2Octave": 0.0,
            "oscMix": 0.70,        # Favor noise snap
            "osc3Waveform": 0.0,
            "osc3Level": 0.0,
            "ringModMix": 0.15,
            "pulseWidth": 0.50,
            "fmAmount": 0.0,
            "velToAmp": 0.85,
            "velToFilter": 0.90,
            "filterMode": 4.0,     # BP24 Bandpass snare body
            "filterCutoff": 1250.0,
            "filterResonance": 0.55,
            "filterKbTrack": 0.20,
            "filterEnvAmount": 0.65,
            "ampAttack": 0.001,
            "ampDecay": 0.16,
            "ampSustain": 0.0,
            "ampRelease": 0.10,
            "filterAttack": 0.001,
            "filterDecay": 0.12,
            "filterSustain": 0.0,
            "filterRelease": 0.08,
            "envLink": 0.0,
            "modAttack": 0.001,
            "modDecay": 0.035,
            "modAmount": 0.70,
            "modTarget": 3.0,      # Body pitch snap
            "lfo1Waveform": 2.0,
            "lfo1Rate": 1.0,
            "lfo1Amount": 0.0,
            "lfo2Waveform": 2.0,
            "lfo2Rate": 1.0,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.45,
            "driveTone": 0.65,
            "dualMode": 0.0,
            "analogMode": 0.0,
            "wNoiseMode": 1.0,     # True White Noise
            "masterVolume": 0.84,
        }
    },

    # -------------------------------------------------------------------------
    # FX
    # -------------------------------------------------------------------------
    {
        "name": "SciFi Laser Dive",
        "category": "FX",
        "params": {
            "osc1Waveform": 0.0,   # Saw
            "osc1Octave": 1.0,
            "osc1Fine": 0.0,
            "osc2Waveform": 1.0,   # Square
            "osc2Octave": 0.0,
            "osc2Fine": 0.20,
            "oscMix": 0.50,
            "osc3Waveform": 0.0,
            "osc3Level": 0.0,
            "ringModMix": 0.35,    # Metallic ring
            "pulseWidth": 0.50,
            "fmAmount": 0.55,      # Deep FM screech
            "velToAmp": 0.50,
            "velToFilter": 0.50,
            "filterMode": 2.0,     # LP+NT
            "filterCutoff": 3500.0,
            "filterResonance": 0.80,
            "filterKbTrack": 0.0,
            "filterEnvAmount": 0.75,
            "ampAttack": 0.002,
            "ampDecay": 0.80,
            "ampSustain": 0.0,
            "ampRelease": 0.35,
            "filterAttack": 0.005,
            "filterDecay": 0.60,
            "filterSustain": 0.0,
            "filterRelease": 0.30,
            "envLink": 0.0,
            "modAttack": 0.002,
            "modDecay": 0.65,      # Deep laser dive
            "modAmount": 0.95,
            "modTarget": 3.0,      # Osc 2 pitch dive
            "lfo1Waveform": 2.0,
            "lfo1Rate": 8.0,
            "lfo1Amount": 0.25,
            "lfo1Target": 1.0,     # Filter Cutoff warble
            "lfo2Waveform": 2.0,
            "lfo2Rate": 0.5,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.50,
            "driveTone": 0.75,
            "dualMode": 1.0,
            "analogMode": 1.0,
            "masterVolume": 0.80,
        }
    },
    {
        "name": "Cosmic Riser Sweep",
        "category": "FX",
        "params": {
            "osc1Waveform": 0.0,   # Saw
            "osc1Octave": 0.0,
            "osc1Fine": -0.05,
            "osc2Waveform": 3.0,   # Noise wash
            "osc2Octave": 0.0,
            "oscMix": 0.60,
            "osc3Waveform": 1.0,
            "osc3Level": 0.20,
            "ringModMix": 0.20,
            "pulseWidth": 0.50,
            "fmAmount": 0.10,
            "velToAmp": 0.30,
            "velToFilter": 0.40,
            "filterMode": 4.0,     # BP24 rising bandpass
            "filterCutoff": 400.0,
            "filterResonance": 0.75,
            "filterKbTrack": 0.0,
            "filterEnvAmount": 0.85,
            "ampAttack": 2.00,     # Rising build
            "ampDecay": 3.00,
            "ampSustain": 0.80,
            "ampRelease": 1.50,
            "filterAttack": 3.50,  # Long rising filter sweep
            "filterDecay": 2.00,
            "filterSustain": 0.90,
            "filterRelease": 1.20,
            "envLink": 0.0,
            "modAttack": 1.50,
            "modDecay": 2.50,
            "modAmount": 0.40,
            "modTarget": 0.0,
            "lfo1Waveform": 0.0,   # Saw (Ramping LFO)
            "lfo1Rate": 8.0,       # Stutter build
            "lfo1Amount": 0.30,
            "lfo1Target": 1.0,     # Filter Cutoff
            "lfo2Waveform": 2.0,
            "lfo2Rate": 0.2,
            "lfo2Amount": 0.0,
            "driveEnabled": 1.0,
            "driveAmount": 0.45,
            "driveTone": 0.70,
            "dualMode": 1.0,
            "analogMode": 1.0,
            "wNoiseMode": 1.0,     # White Noise wash
            "masterVolume": 0.78,
        }
    },
]


def generate_all_homage_presets(output_dir: Path) -> list[Path]:
    """Generates all 15 homage presets into output_dir organized by category."""
    output_dir.mkdir(parents=True, exist_ok=True)
    created_paths: list[Path] = []

    for item in HOMAGE_PRESETS_DATA:
        preset = ParsedPreset(
            name=item["name"],
            category=item["category"],
            parameters=item["params"],
            source_format="Clean-Room Homage Sound Design",
            source_file="scripts/generate_homage_presets.py"
        )

        cat_dir = output_dir / preset.category
        cat_dir.mkdir(parents=True, exist_ok=True)

        filename = f"{preset.name.replace(' ', '_')}.xml"
        file_path = cat_dir / filename

        xml_str = export_preset_to_xml(preset)
        file_path.write_text(xml_str, encoding="utf-8")
        created_paths.append(file_path)
        print(f"  [CREATED] {file_path.relative_to(PROJECT_ROOT)}")

    return created_paths


if __name__ == "__main__":
    target_dir = PROJECT_ROOT / "presets" / "homage"
    print(f"Generating 15 Clean-Room Homage Presets into {target_dir}...")
    paths = generate_all_homage_presets(target_dir)
    print(f"Successfully generated {len(paths)} homage preset XML files.")
