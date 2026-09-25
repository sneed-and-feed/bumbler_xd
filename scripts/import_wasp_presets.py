#!/usr/bin/env python3
"""
scripts/import_wasp_presets.py
==============================
Legacy Wasp / Wasp XT Preset Migration Importer for Bumbler XD.

Converts legacy preset files (.fxp, .fxb, .fst, .flp) into:
  1) JUCE APVTS-compatible XML preset files (<BumblerXD ...>).
  2) C++ PresetDefinition structs suitable for source/plugin/parameters/PresetParameters.h.

Supported Legacy Formats:
  - .fxp / .fxb: VST 2.4 standard presets and banks ('CcnK' / 'FPCh' / 'FBCh' / 'FxCk' / 'FxBk').
  - .fst: FL Studio state files (RIFF/IFF containers, FL event streams, or raw plugin chunks).
  - .flp: FL Studio project files (scans event 0xC5 PluginData for Wasp/Wasp XT channels).

Pure Python standard library implementation with zero external dependencies.
"""

from __future__ import annotations

import argparse
import io
import math
import os
import struct
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple


# =============================================================================
# Parameter Specification & APVTS Blueprint (55 Parameters)
# =============================================================================

@dataclass(frozen=True)
class ParamSpec:
    id: str
    name: str
    group: str
    min_val: float
    max_val: float
    default_val: float
    unit: str
    is_discrete: bool
    choices: Tuple[str, ...] = ()


ALL_55_PARAMS: Tuple[ParamSpec, ...] = (
    # Group 1: Oscillators & Voice Management (14 parameters)
    ParamSpec("osc1Waveform",  "OSC 1 Waveform",      "Oscillators", 0.0, 3.0, 0.0, "",    True,  ("Saw", "Square", "Sine", "Noise")),
    ParamSpec("osc1Octave",    "OSC 1 Octave",        "Oscillators", -3.0, 3.0, 0.0, "oct", True),
    ParamSpec("osc1Fine",      "OSC 1 Fine",          "Oscillators", -1.0, 1.0, 0.0, "st",  False),
    ParamSpec("osc2Waveform",  "OSC 2 Waveform",      "Oscillators", 0.0, 3.0, 0.0, "",    True,  ("Saw", "Square", "Sine", "Noise")),
    ParamSpec("osc2Octave",    "OSC 2 Octave",        "Oscillators", -3.0, 3.0, 0.0, "oct", True),
    ParamSpec("osc2Fine",      "OSC 2 Fine",          "Oscillators", -1.0, 1.0, 0.0, "st",  False),
    ParamSpec("oscMix",        "OSC Mix",             "Oscillators", 0.0, 1.0, 0.5, "%",   False),
    ParamSpec("osc3Waveform",  "OSC 3 Waveform",      "Oscillators", 0.0, 1.0, 0.0, "",    True,  ("Square", "Saw")),
    ParamSpec("osc3Level",     "OSC 3 Level",         "Oscillators", 0.0, 1.0, 0.0, "%",   False),
    ParamSpec("ringModMix",    "Ring Mod Mix",        "Oscillators", 0.0, 1.0, 0.0, "%",   False),
    ParamSpec("pulseWidth",    "Pulse Width",         "Oscillators", 0.01, 0.99, 0.50, "%", False),
    ParamSpec("fmAmount",      "FM Amount",           "Oscillators", 0.0, 1.0, 0.0, "%",   False),
    ParamSpec("velToAmp",      "Velocity to Amp",     "Oscillators", 0.0, 1.0, 0.50, "%",  False),
    ParamSpec("velToFilter",   "Velocity to Filter",  "Oscillators", 0.0, 1.0, 0.50, "%",  False),

    # Group 2: 6-Mode Filter Section (5 parameters)
    ParamSpec("filterMode",      "Filter Mode",       "Filter", 0.0, 5.0, 1.0, "",    True,  ("LP12", "LP24", "LP+NT", "DBL.NT", "BP24", "HP24")),
    ParamSpec("filterCutoff",    "Cutoff Frequency",  "Filter", 20.0, 20000.0, 1200.0, "Hz", False),
    ParamSpec("filterResonance", "Resonance",         "Filter", 0.0, 1.0, 0.20, "%",  False),
    ParamSpec("filterKbTrack",   "Keyboard Track",    "Filter", 0.0, 1.0, 0.50, "%",  False),
    ParamSpec("filterEnvAmount", "Filter Env Amount", "Filter", -1.0, 1.0, 0.0, "%",  False),

    # Group 3: Envelopes (13 parameters)
    ParamSpec("ampAttack",     "Amp Attack",          "Envelopes", 0.001, 10.0, 0.01, "s", False),
    ParamSpec("ampDecay",      "Amp Decay",           "Envelopes", 0.001, 10.0, 0.30, "s", False),
    ParamSpec("ampSustain",    "Amp Sustain",         "Envelopes", 0.0, 1.0, 0.80, "%",   False),
    ParamSpec("ampRelease",    "Amp Release",         "Envelopes", 0.001, 10.0, 0.30, "s", False),
    ParamSpec("filterAttack",  "Filter Attack",       "Envelopes", 0.001, 10.0, 0.05, "s", False),
    ParamSpec("filterDecay",   "Filter Decay",        "Envelopes", 0.001, 10.0, 0.50, "s", False),
    ParamSpec("filterSustain", "Filter Sustain",      "Envelopes", 0.0, 1.0, 0.50, "%",   False),
    ParamSpec("filterRelease", "Filter Release",      "Envelopes", 0.001, 10.0, 0.40, "s", False),
    ParamSpec("envLink",       "Envelope Link",       "Envelopes", 0.0, 1.0, 0.0, "",    True),
    ParamSpec("modAttack",     "Mod Attack",          "Envelopes", 0.001, 5.0, 0.05, "s", False),
    ParamSpec("modDecay",      "Mod Decay",           "Envelopes", 0.001, 10.0, 0.50, "s", False),
    ParamSpec("modAmount",     "Mod Amount",          "Envelopes", -1.0, 1.0, 0.0, "%",  False),
    ParamSpec("modTarget",     "Mod Target",          "Envelopes", 0.0, 3.0, 0.0, "",    True,  ("PW", "Lfo1Amt", "Osc1Level", "Osc2Pitch")),

    # Group 4: Dual LFOs (16 parameters)
    ParamSpec("lfo1Waveform", "LFO 1 Waveform",       "LFO", 0.0, 3.0, 2.0, "",    True,  ("Saw", "Square", "Sine", "Noise")),
    ParamSpec("lfo1Rate",     "LFO 1 Rate",           "LFO", 0.05, 30.0, 2.0, "Hz", False),
    ParamSpec("lfo1Delay",    "LFO 1 Delay",          "LFO", 0.0, 5.0, 0.0, "s",   False),
    ParamSpec("lfo1Sync",     "LFO 1 Tempo Sync",     "LFO", 0.0, 1.0, 0.0, "",    True),
    ParamSpec("lfo1SyncDiv",  "LFO 1 Sync Division",  "LFO", 0.0, 5.0, 3.0, "",    True,  ("1/32", "1/16", "1/8", "1/4", "1/2", "1/1")),
    ParamSpec("lfo1KeyReset", "LFO 1 Key Reset",      "LFO", 0.0, 1.0, 1.0, "",    True),
    ParamSpec("lfo1Amount",   "LFO 1 Amount",         "LFO", 0.0, 1.0, 0.0, "%",   False),
    ParamSpec("lfo1Target",   "LFO 1 Target",         "LFO", 0.0, 2.0, 1.0, "",    True,  ("Osc12Pitch", "FilterCutoff", "PulseWidth")),
    ParamSpec("lfo2Waveform", "LFO 2 Waveform",       "LFO", 0.0, 3.0, 2.0, "",    True,  ("Saw", "Square", "Sine", "Noise")),
    ParamSpec("lfo2Rate",     "LFO 2 Rate",           "LFO", 0.05, 30.0, 1.0, "Hz", False),
    ParamSpec("lfo2Delay",    "LFO 2 Delay",          "LFO", 0.0, 5.0, 0.0, "s",   False),
    ParamSpec("lfo2Sync",     "LFO 2 Tempo Sync",     "LFO", 0.0, 1.0, 0.0, "",    True),
    ParamSpec("lfo2SyncDiv",  "LFO 2 Sync Division",  "LFO", 0.0, 5.0, 4.0, "",    True,  ("1/32", "1/16", "1/8", "1/4", "1/2", "1/1")),
    ParamSpec("lfo2KeyReset", "LFO 2 Key Reset",      "LFO", 0.0, 1.0, 1.0, "",    True),
    ParamSpec("lfo2Amount",   "LFO 2 Amount",         "LFO", 0.0, 1.0, 0.0, "%",   False),
    ParamSpec("lfo2Target",   "LFO 2 Target",         "LFO", 0.0, 2.0, 0.0, "",    True,  ("Osc1Pitch", "OscMix", "MasterAmp")),

    # Group 5: Character & Master Output (7 parameters)
    ParamSpec("driveEnabled", "Drive Enable",         "Output", 0.0, 1.0, 0.0, "",  True),
    ParamSpec("driveAmount",  "Drive Amount",         "Output", 0.0, 1.0, 0.30, "%", False),
    ParamSpec("driveTone",    "Drive Tone",           "Output", 0.0, 1.0, 0.50, "%", False),
    ParamSpec("dualMode",     "Dual Mode",            "Output", 0.0, 1.0, 0.0, "",  True),
    ParamSpec("analogMode",   "Analog Mode",          "Output", 0.0, 1.0, 0.0, "",  True),
    ParamSpec("wNoiseMode",   "Noise Type",           "Output", 0.0, 1.0, 0.0, "",  True,  ("Vintage Table", "White Noise")),
    ParamSpec("masterVolume", "Master Volume",        "Output", 0.0, 1.0, 0.80, "%", False),
)

PARAM_SPEC_MAP: Dict[str, ParamSpec] = {p.id: p for p in ALL_55_PARAMS}

# Standard Wasp XT legacy parameter order blueprint
WASP_XT_PARAM_ORDER: Tuple[str, ...] = tuple(p.id for p in ALL_55_PARAMS)

# Classic 2-Oscillator Wasp parameter order (32 legacy parameters subset)
CLASSIC_WASP_PARAM_ORDER: Tuple[str, ...] = (
    "osc1Waveform", "osc1Octave", "osc1Fine",
    "osc2Waveform", "osc2Octave", "osc2Fine", "oscMix",
    "ringModMix", "pulseWidth", "fmAmount",
    "velToAmp", "velToFilter",
    "filterMode", "filterCutoff", "filterResonance", "filterKbTrack", "filterEnvAmount",
    "ampAttack", "ampDecay", "ampSustain", "ampRelease",
    "filterAttack", "filterDecay", "filterSustain", "filterRelease", "envLink",
    "lfo1Waveform", "lfo1Rate", "lfo1Delay", "lfo1Amount", "lfo1Target",
    "masterVolume",
)

# 56 native parameters corresponding to Wasp XT Delphi form controls (Tags 0..55)
NATIVE_WASP_XT_PARAM_ORDER: Tuple[str, ...] = (
    "osc1Waveform",   # 00: Osc1ShapeSelect (0=Saw, 1=Square, 2=Sine, 3=Noise)
    "osc1Octave",     # 01: Osc1CoarseWheel
    "osc1Fine",       # 02: Osc1FineWheel
    "osc2Waveform",   # 03: Osc2ShapeSelect
    "osc2Octave",     # 04: Osc2CoarseWheel
    "osc2Fine",       # 05: Osc2FineWheel
    "osc3Waveform",   # 06: Osc3ShapeSelect (0=Square, 1=Saw)
    "osc3Level",      # 07: Osc3AmountWheel
    "oscMix",         # 08: OscMixSlider
    "pulseWidth",     # 09: PulseWidthWheel
    "fmAmount",       # 10: FmAmountWheel
    "ringModMix",     # 11: RingModBtn (0 or 1)
    "ampAttack",      # 12: AmpAttackWheel
    "ampDecay",       # 13: AmpDecayWheel
    "ampSustain",     # 14: AmpSustainLevelWheel
    "ampRelease",     # 15: AmpReleaseWheel
    "filterAttack",   # 16: FiltAttack
    "filterDecay",    # 17: FiltDecay
    "filterSustain",  # 18: FiltSustainLevel
    "filterRelease",  # 19: FiltRelease
    "filterKbTrack",  # 20: KbTrackWheel
    "filterMode",     # 21: FilterTypeSelect (0..5)
    "filterCutoff",   # 22: CutoffWheel
    "filterResonance",# 23: ResonanceWheel
    "filterEnvAmount",# 24: EnvAmtWheel
    "lfo1Waveform",   # 25: LFO1ShapeSelect (0..3)
    "lfo1Target",     # 26: LFO1DestSelect (0..2)
    "lfo1Amount",     # 27: LFO1AmountWheel
    "lfo1Rate",       # 28: LFO1FreqWheel
    "lfo1Sync",       # 29: LFO1SyncBtn
    "lfo1KeyReset",   # 30: LFO1ResetBtn
    "lfo2Waveform",   # 31: LFO2ShapeSelect (0..3)
    "lfo2Target",     # 32: LFO2DestSelect (0..2)
    "lfo2Amount",     # 33: LFO2AmountWheel
    "lfo2Rate",       # 34: LFO2FreqWheel
    "lfo2Sync",       # 35: LFO2SyncBtn
    "lfo2KeyReset",   # 36: LFO2ResetBtn
    "driveEnabled",   # 37: UseDistBtn
    "driveAmount",    # 38: DistDriveWheel
    "driveTone",      # 39: DistToneWheel
    "dualMode",       # 40: DualVoiceBtn
    "velToFilter",    # 41: VelocityFilterWheel
    "analogMode",     # 42: AnalogBtn
    "modAttack",      # 43: ModAttackWheel
    "modDecay",       # 44: ModDecayWheel
    "modAmount",      # 45: ModAmountWheel
    "modDest1",       # 46: ModDest1Btn (Filter Cutoff) -> target 0
    "modDest2",       # 47: ModDest2Btn (Osc1 Pitch) -> target 1
    "modDest3",       # 48: ModDest3Btn (Osc2 Pitch) -> target 1
    "modDest4",       # 49: ModDest4Btn (Pulse Width) -> target 2
    "masterVolume",   # 50: VolumeWheel
    "aftertouch",     # 51: AftertouchBtn
    "lfo1Delay",      # 52: LFO1DelayWheel
    "lfo2Delay",      # 53: LFO2DelayWheel
    "velToAmp",       # 54: VelocityAmpWheel
    "wNoiseMode",     # 55: WhiteNoiseBtn
)


# =============================================================================
# Mathematical Scaling & Domain Conversion Helpers
# =============================================================================

def clamp(val: float, min_val: float, max_val: float) -> float:
    """Clamps a floating-point value to the inclusive range [min_val, max_val]."""
    if math.isnan(val) or math.isinf(val):
        return min_val
    return max(min_val, min(max_val, val))


def scale_logarithmic(norm: float, min_val: float, max_val: float) -> float:
    """
    Logarithmically scales a normalized value in [0.0, 1.0] to [min_val, max_val].
    Used for frequency cutoff: freq = min_val * (max_val / min_val) ** norm.
    If input is already > 1.0, clamps directly to [min_val, max_val].
    """
    if norm > 1.0:
        return clamp(norm, min_val, max_val)
    n = clamp(norm, 0.0, 1.0)
    ratio = max_val / min_val
    return clamp(min_val * (ratio ** n), min_val, max_val)


def scale_exponential_time(norm: float, min_s: float = 0.001, max_s: float = 10.0) -> float:
    """
    Exponentially scales envelope times from [0.0, 1.0] to [min_s, max_s].
    At norm=0.0 -> 1 ms, norm=0.5 -> ~100 ms, norm=1.0 -> 10 s.
    If input is already in seconds (> 1.0), clamps to [min_s, max_s].
    """
    if norm > 1.0:
        return clamp(norm, min_s, max_s)
    n = clamp(norm, 0.0, 1.0)
    ratio = max_s / min_s
    return clamp(min_s * (ratio ** n), min_s, max_s)


def scale_lfo_rate(norm: float, min_hz: float = 0.05, max_hz: float = 30.0) -> float:
    """
    Exponentially scales LFO rate from [0.0, 1.0] to [min_hz, max_hz].
    """
    if norm > 1.0:
        return clamp(norm, min_hz, max_hz)
    n = clamp(norm, 0.0, 1.0)
    ratio = max_hz / min_hz
    return clamp(min_hz * (ratio ** n), min_hz, max_hz)


def scale_bipolar(norm: float, min_val: float = -1.0, max_val: float = 1.0) -> float:
    """
    Scales a unipolar normalized value [0.0, 1.0] to bipolar range [-1.0, 1.0].
    If input is already negative or in [-1.0, 1.0] and raw bipolar is supplied, clamps safely.
    """
    if norm < 0.0:
        return clamp(norm, min_val, max_val)
    return clamp(norm * 2.0 - 1.0, min_val, max_val)


def scale_discrete(norm: float, num_steps: int) -> float:
    """
    Converts a normalized value [0.0, 1.0] or discrete step into a 0-indexed float.
    """
    if norm > 1.0:
        return float(clamp(int(round(norm)), 0, num_steps - 1))
    return float(clamp(int(round(norm * (num_steps - 1))), 0, num_steps - 1))


def map_legacy_param_to_apvts(param_id: str, raw_val: float) -> float:
    """
    Transforms a single raw / normalized legacy parameter value to its APVTS target range.
    """
    spec = PARAM_SPEC_MAP.get(param_id)
    if spec is None:
        return raw_val

    # 1. Oscillators
    if param_id in ("osc1Waveform", "osc2Waveform"):
        return scale_discrete(raw_val, 4)  # 0=Saw, 1=Square, 2=Sine, 3=Noise
    if param_id in ("osc1Octave", "osc2Octave"):
        if -3.0 <= raw_val <= 3.0 and (raw_val < 0.0 or raw_val.is_integer()):
            return clamp(round(raw_val), -3.0, 3.0)
        return float(clamp(int(round(raw_val * 6.0 - 3.0)), -3, 3))
    if param_id in ("osc1Fine", "osc2Fine"):
        if -1.0 <= raw_val <= 1.0 and raw_val < 0.0:
            return clamp(raw_val, -1.0, 1.0)
        return scale_bipolar(raw_val, -1.0, 1.0)
    if param_id == "oscMix":
        return clamp(raw_val, 0.0, 1.0)
    if param_id == "osc3Waveform":
        return 1.0 if raw_val >= 0.5 else 0.0  # 0=Square, 1=Saw
    if param_id in ("osc3Level", "ringModMix", "fmAmount", "velToAmp", "velToFilter"):
        return clamp(raw_val, 0.0, 1.0)
    if param_id == "pulseWidth":
        if 0.01 <= raw_val <= 0.99:
            return clamp(raw_val, 0.01, 0.99)
        return clamp(0.01 + raw_val * 0.98, 0.01, 0.99)

    # 2. Filter
    if param_id == "filterMode":
        return scale_discrete(raw_val, 6)  # 0..5
    if param_id == "filterCutoff":
        return scale_logarithmic(raw_val, 20.0, 20000.0)
    if param_id in ("filterResonance", "filterKbTrack"):
        return clamp(raw_val, 0.0, 1.0)
    if param_id == "filterEnvAmount":
        return scale_bipolar(raw_val, -1.0, 1.0)

    # 3. Envelopes
    if param_id in ("ampAttack", "ampDecay", "ampRelease",
                    "filterAttack", "filterDecay", "filterRelease",
                    "modDecay"):
        return scale_exponential_time(raw_val, 0.001, 10.0)
    if param_id == "modAttack":
        return scale_exponential_time(raw_val, 0.001, 5.0)
    if param_id in ("ampSustain", "filterSustain"):
        return clamp(raw_val, 0.0, 1.0)
    if param_id == "envLink":
        return 1.0 if raw_val >= 0.5 else 0.0
    if param_id == "modAmount":
        return scale_bipolar(raw_val, -1.0, 1.0)
    if param_id == "modTarget":
        return scale_discrete(raw_val, 4)

    # 4. LFOs
    if param_id in ("lfo1Waveform", "lfo2Waveform"):
        return scale_discrete(raw_val, 4)
    if param_id in ("lfo1Rate", "lfo2Rate"):
        return scale_lfo_rate(raw_val, 0.05, 30.0)
    if param_id in ("lfo1Delay", "lfo2Delay"):
        if raw_val > 1.0:
            return clamp(raw_val, 0.0, 5.0)
        return clamp(raw_val * 5.0, 0.0, 5.0)
    if param_id in ("lfo1Sync", "lfo2Sync", "lfo1KeyReset", "lfo2KeyReset"):
        return 1.0 if raw_val >= 0.5 else 0.0
    if param_id in ("lfo1SyncDiv", "lfo2SyncDiv"):
        return scale_discrete(raw_val, 6)
    if param_id in ("lfo1Amount", "lfo2Amount"):
        return clamp(raw_val, 0.0, 1.0)
    if param_id in ("lfo1Target", "lfo2Target"):
        return scale_discrete(raw_val, 3)

    # 5. Output / Character
    if param_id in ("driveEnabled", "dualMode", "analogMode", "wNoiseMode"):
        return 1.0 if raw_val >= 0.5 else 0.0
    if param_id in ("driveAmount", "driveTone", "masterVolume"):
        return clamp(raw_val, 0.0, 1.0)

    return raw_val


# =============================================================================
# Parsed Preset Container
# =============================================================================

@dataclass
class ParsedPreset:
    name: str
    category: str = "Synth"
    parameters: Dict[str, float] = field(default_factory=dict)
    source_format: str = "Unknown"
    source_file: str = ""

    def __post_init__(self) -> None:
        # Populate any missing parameters with APVTS default values
        for spec in ALL_55_PARAMS:
            if spec.id not in self.parameters:
                self.parameters[spec.id] = spec.default_val
            else:
                self.parameters[spec.id] = round(self.parameters[spec.id], 4)

    def auto_classify_category(self) -> str:
        """Heuristically infers preset category based on name and parameter features."""
        name_lower = self.name.lower()
        if any(kw in name_lower for kw in ("bass", "sub", "303", "reese", "low", "acid", "growl")):
            return "Bass"
        if any(kw in name_lower for kw in ("lead", "solo", "sync", "stab", "screamer", "saw", "hook")):
            return "Lead"
        if any(kw in name_lower for kw in ("pad", "string", "choir", "warm", "swell", "atmosphere", "drone", "ambient")):
            return "Pad"
        if any(kw in name_lower for kw in ("pluck", "bell", "chime", "harp", "key", "piano", "staccato")):
            return "Pluck"
        if any(kw in name_lower for kw in ("kick", "snare", "perc", "drum", "hat", "zap", "clap", "tom")):
            return "Percussion"
        if any(kw in name_lower for kw in ("fx", "laser", "noise", "sweep", "rise", "down", "space", "sfx")):
            return "FX"

        # Parameter acoustic heuristics
        cutoff = self.parameters.get("filterCutoff", 1200.0)
        amp_atk = self.parameters.get("ampAttack", 0.01)
        amp_rel = self.parameters.get("ampRelease", 0.30)
        amp_sus = self.parameters.get("ampSustain", 0.80)
        osc1_oct = self.parameters.get("osc1Octave", 0.0)

        if osc1_oct <= -1.0 and cutoff < 600.0 and amp_atk < 0.05:
            return "Bass"
        if amp_atk >= 0.40 or amp_rel >= 1.50:
            return "Pad"
        if amp_sus < 0.20 and self.parameters.get("ampDecay", 0.30) < 0.35:
            return "Pluck"
        if self.parameters.get("modTarget", 0.0) == 3.0 and self.parameters.get("modDecay", 0.50) < 0.10:
            return "Percussion"
        return "Lead" if cutoff > 2000.0 else "Synth"


# =============================================================================
# Legacy File Format Parsers
# =============================================================================

def parse_float_block(data: bytes, endianness: str = "<") -> List[float]:
    """Unpacks a raw block of 32-bit floats."""
    count = len(data) // 4
    if count == 0:
        return []
    fmt = f"{endianness}{count}f"
    try:
        floats = struct.unpack(fmt, data[:count * 4])
        # Validate that the floats are finite
        if all(math.isfinite(f) for f in floats):
            return list(floats)
    except struct.error:
        pass
    return []


def map_float_list_to_preset(
    floats: Sequence[float],
    preset_name: str,
    source_format: str,
    source_file: str,
    param_order: Sequence[str] = WASP_XT_PARAM_ORDER
) -> ParsedPreset:
    """Maps a sequence of float parameters to a ParsedPreset using the parameter order."""
    params: Dict[str, float] = {}
    for i, pid in enumerate(param_order):
        if i < len(floats):
            params[pid] = map_legacy_param_to_apvts(pid, floats[i])
        else:
            params[pid] = PARAM_SPEC_MAP[pid].default_val

    preset = ParsedPreset(
        name=preset_name.strip() or "Migrated Preset",
        parameters=params,
        source_format=source_format,
        source_file=source_file
    )
    preset.category = preset.auto_classify_category()
    return preset


def map_native_wasp_floats_to_preset(
    floats: Sequence[float],
    preset_name: str,
    source_format: str,
    source_file: str,
    flags: int = 0
) -> ParsedPreset:
    """Maps 56 native Wasp XT float parameters (Tags 0..55) + flags to a ParsedPreset."""
    params: Dict[str, float] = {}
    for spec in ALL_55_PARAMS:
        params[spec.id] = spec.default_val

    for i, pid in enumerate(NATIVE_WASP_XT_PARAM_ORDER):
        if i >= len(floats):
            break
        raw_val = floats[i]
        if pid.startswith("modDest") or pid == "aftertouch":
            continue
        params[pid] = map_legacy_param_to_apvts(pid, raw_val)

    # Mod Destination Buttons (Tags 46..49)
    # 46: ModDest1Btn (Filter Cutoff) -> target 0
    # 47: ModDest2Btn (Osc1 Pitch)    -> target 1
    # 48: ModDest3Btn (Osc2 Pitch)    -> target 1
    # 49: ModDest4Btn (Pulse Width)   -> target 2
    if len(floats) > 49:
        if floats[46] >= 0.5:
            params["modTarget"] = 0.0  # Filter Cutoff
        elif floats[47] >= 0.5 or floats[48] >= 0.5:
            params["modTarget"] = 1.0  # Pitch
        elif floats[49] >= 0.5:
            params["modTarget"] = 2.0  # Pulse Width

    # Flags: Bit 0 or Bit 1 = envLink (AmpCoupleFltBtn)
    if (flags & 0x01) or (flags & 0x02):
        params["envLink"] = 1.0

    preset = ParsedPreset(
        name=preset_name.strip() or "Migrated Preset",
        parameters=params,
        source_format=source_format,
        source_file=source_file
    )
    preset.category = preset.auto_classify_category()
    return preset


def map_delphi_wasp_integers_to_preset(
    ints: Sequence[int],
    preset_name: str,
    source_format: str,
    source_file: str,
    flags: int = 0
) -> ParsedPreset:
    """Maps 56 Delphi Wasp XT integer parameters (Tags 0..55) + flags to a ParsedPreset."""
    params: Dict[str, float] = {}
    for spec in ALL_55_PARAMS:
        params[spec.id] = spec.default_val

    p = list(ints)
    if len(p) < 56:
        p.extend([0] * (56 - len(p)))
        if len(ints) <= 50:
            p[50] = 107  # Default master volume ~0.84

    # 0: Osc 1 Waveform (0=Saw, 1=Square, 2=Sine, 3=Noise)
    params["osc1Waveform"] = float(clamp(float(p[0]), 0.0, 3.0))
    # 1: Osc 1 Coarse (0..128, Def=64) -> -3..+3 octaves
    params["osc1Octave"] = float(clamp(round((p[1] / 128.0) * 6.0 - 3.0), -3.0, 3.0))
    # 2: Osc 1 Fine (0..128, Def=64) -> -1.0..+1.0
    params["osc1Fine"] = clamp((p[2] / 128.0) * 2.0 - 1.0, -1.0, 1.0)

    # 3: Osc 2 Waveform (0=Saw, 1=Square, 2=Sine, 3=Noise)
    params["osc2Waveform"] = float(clamp(float(p[3]), 0.0, 3.0))
    # 4: Osc 2 Coarse (0..128, Def=64) -> -3..+3 octaves
    params["osc2Octave"] = float(clamp(round((p[4] / 128.0) * 6.0 - 3.0), -3.0, 3.0))
    # 5: Osc 2 Fine (0..128, Def=64) -> -1.0..+1.0
    params["osc2Fine"] = clamp((p[5] / 128.0) * 2.0 - 1.0, -1.0, 1.0)

    # 6: Osc 3 Waveform (0=Square, 1=Saw)
    params["osc3Waveform"] = 1.0 if p[6] > 0 else 0.0
    # 7: Osc 3 Level (0..128)
    params["osc3Level"] = clamp(p[7] / 128.0, 0.0, 1.0)
    # 8: Osc Mix (0..128)
    params["oscMix"] = clamp(p[8] / 128.0, 0.0, 1.0)
    # 9: Pulse Width (0..128) -> 0.01..0.99
    params["pulseWidth"] = clamp(0.01 + (p[9] / 128.0) * 0.98, 0.01, 0.99)
    # 10: FM Amount (0..128)
    params["fmAmount"] = clamp(p[10] / 128.0, 0.0, 1.0)
    # 11: Ring Mod Mix (0 or 1)
    params["ringModMix"] = 1.0 if p[11] > 0 else 0.0

    # Envelopes: Amp ADSR (0..128)
    params["ampAttack"]  = scale_exponential_time(clamp(p[12] / 128.0, 0.0, 1.0), 0.001, 10.0)
    params["ampDecay"]   = scale_exponential_time(clamp(p[13] / 128.0, 0.0, 1.0), 0.001, 10.0)
    params["ampSustain"] = clamp(p[14] / 128.0, 0.0, 1.0)
    params["ampRelease"] = scale_exponential_time(clamp(p[15] / 128.0, 0.0, 1.0), 0.001, 10.0)

    # Envelopes: Filter ADSR (0..128)
    params["filterAttack"]  = scale_exponential_time(clamp(p[16] / 128.0, 0.0, 1.0), 0.001, 10.0)
    params["filterDecay"]   = scale_exponential_time(clamp(p[17] / 128.0, 0.0, 1.0), 0.001, 10.0)
    params["filterSustain"] = clamp(p[18] / 128.0, 0.0, 1.0)
    params["filterRelease"] = scale_exponential_time(clamp(p[19] / 128.0, 0.0, 1.0), 0.001, 10.0)

    # Filter Controls
    params["filterKbTrack"] = clamp(p[20] / 128.0, 0.0, 1.0)
    params["filterMode"] = float(clamp(float(p[21]), 0.0, 5.0))
    # Cutoff: Max 512!
    params["filterCutoff"] = scale_logarithmic(clamp(p[22] / 512.0, 0.0, 1.0), 20.0, 20000.0)
    params["filterResonance"] = clamp(p[23] / 128.0, 0.0, 1.0)
    # Env Amount: Bipolar -1..+1 (0..128, Def=64)
    params["filterEnvAmount"] = clamp((p[24] / 128.0) * 2.0 - 1.0, -1.0, 1.0)

    # LFO 1
    params["lfo1Waveform"] = float(clamp(float(p[25]), 0.0, 3.0))
    params["lfo1Target"] = float(clamp(float(p[26]), 0.0, 2.0))
    params["lfo1Amount"] = clamp(p[27] / 128.0, 0.0, 1.0)
    params["lfo1Rate"] = scale_lfo_rate(clamp(p[28] / 128.0, 0.0, 1.0), 0.05, 30.0)
    params["lfo1Sync"] = 1.0 if p[29] > 0 else 0.0
    params["lfo1KeyReset"] = 1.0 if p[30] > 0 else 0.0

    # LFO 2
    params["lfo2Waveform"] = float(clamp(float(p[31]), 0.0, 3.0))
    params["lfo2Target"] = float(clamp(float(p[32]), 0.0, 2.0))
    params["lfo2Amount"] = clamp(p[33] / 128.0, 0.0, 1.0)
    params["lfo2Rate"] = scale_lfo_rate(clamp(p[34] / 128.0, 0.0, 1.0), 0.05, 30.0)
    params["lfo2Sync"] = 1.0 if p[35] > 0 else 0.0
    params["lfo2KeyReset"] = 1.0 if p[36] > 0 else 0.0

    # Character & Voices
    params["driveEnabled"] = 1.0 if p[37] > 0 else 0.0
    params["driveAmount"] = clamp(p[38] / 128.0, 0.0, 1.0)
    params["driveTone"] = clamp(p[39] / 128.0, 0.0, 1.0)
    params["dualMode"] = 1.0 if p[40] > 0 else 0.0
    params["velToFilter"] = clamp(p[41] / 128.0, 0.0, 1.0)
    params["analogMode"] = 1.0 if p[42] > 0 else 0.0

    # Mod Envelope
    params["modAttack"] = scale_exponential_time(clamp(p[43] / 128.0, 0.0, 1.0), 0.001, 5.0)
    params["modDecay"] = scale_exponential_time(clamp(p[44] / 128.0, 0.0, 1.0), 0.001, 10.0)
    params["modAmount"] = clamp((p[45] / 128.0) * 2.0 - 1.0, -1.0, 1.0)

    # Mod Destination Buttons (Tags 46..49)
    if p[46] > 0:
        params["modTarget"] = 0.0  # Filter
    elif p[47] > 0 or p[48] > 0:
        params["modTarget"] = 1.0  # Pitch
    elif p[49] > 0:
        params["modTarget"] = 2.0  # Pulse Width
    else:
        params["modTarget"] = 0.0

    # Output & Delay
    params["masterVolume"] = clamp(p[50] / 128.0, 0.0, 1.0)
    params["lfo1Delay"] = clamp((p[52] / 128.0) * 5.0, 0.0, 5.0)
    params["lfo2Delay"] = clamp((p[53] / 128.0) * 5.0, 0.0, 5.0)
    params["velToAmp"] = clamp(p[54] / 128.0, 0.0, 1.0)
    params["wNoiseMode"] = 1.0 if p[55] > 0 else 0.0

    # Flags: Bit 0 or Bit 1 = envLink (AmpCoupleFltBtn)
    if (flags & 0x01) or (flags & 0x02):
        params["envLink"] = 1.0

    preset = ParsedPreset(
        name=preset_name.strip() or "Migrated Preset",
        parameters=params,
        source_format=source_format,
        source_file=source_file
    )
    preset.category = preset.auto_classify_category()
    return preset


def parse_fxp_data(data: bytes, filename: str = "") -> List[ParsedPreset]:
    """
    Parses VST 2.4 FXP preset or FXB bank files.
    Standard 'CcnK' header format:
      - 'FxCk': Regular preset (32-bit float parameters)
      - 'FPCh': Opaque chunk preset
      - 'FxBk': Regular bank (multiple parameter programs)
      - 'FBCh': Opaque chunk bank
    """
    if len(data) < 28:
        raise ValueError(f"File too small for FXP header ({len(data)} bytes)")

    chunk_magic = data[0:4]
    if chunk_magic != b"CcnK":
        raise ValueError(f"Invalid FXP magic: expected 'CcnK', got {chunk_magic!r}")

    byte_size, fx_magic, version = struct.unpack(">I4si", data[4:16])
    fx_id = data[16:20].decode("latin1", errors="replace")
    fx_version = struct.unpack(">i", data[20:24])[0]

    presets: List[ParsedPreset] = []

    if fx_magic == b"FxCk":
        # Regular Preset with Float Parameters
        num_params = struct.unpack(">i", data[24:28])[0]
        pr_name_raw = data[28:56]
        pr_name = pr_name_raw.split(b"\x00")[0].decode("latin1", errors="replace").strip()
        float_bytes = data[56:56 + num_params * 4]
        floats = struct.unpack(f">{num_params}f", float_bytes)
        presets.append(map_float_list_to_preset(floats, pr_name or Path(filename).stem, "VST 2.4 FXP (FxCk)", filename))

    elif fx_magic == b"FPCh":
        # Opaque Chunk Preset
        num_params = struct.unpack(">i", data[24:28])[0]
        pr_name_raw = data[28:56]
        pr_name = pr_name_raw.split(b"\x00")[0].decode("latin1", errors="replace").strip()
        chunk_size = struct.unpack(">I", data[56:60])[0]
        chunk_data = data[60:60 + chunk_size]

        # Scan chunk data for float parameters (try little-endian, then big-endian)
        floats = parse_float_block(chunk_data, "<")
        if not floats or len(floats) < 10:
            floats = parse_float_block(chunk_data, ">")

        if not floats and len(chunk_data) >= 55:
            # Possible raw byte knobs (0..255)
            floats = [b / 255.0 for b in chunk_data[:55]]

        presets.append(map_float_list_to_preset(floats, pr_name or Path(filename).stem, "VST 2.4 FXP (FPCh)", filename))

    elif fx_magic == b"FxBk":
        # Regular Bank
        num_programs = struct.unpack(">i", data[20:24])[0]
        num_params = struct.unpack(">i", data[156:160])[0] if len(data) >= 160 else 55
        offset = 160
        for p in range(num_programs):
            if offset + 28 + num_params * 4 > len(data):
                break
            pr_name_raw = data[offset:offset + 28]
            pr_name = pr_name_raw.split(b"\x00")[0].decode("latin1", errors="replace").strip()
            offset += 28
            float_bytes = data[offset:offset + num_params * 4]
            floats = struct.unpack(f">{num_params}f", float_bytes)
            offset += num_params * 4
            presets.append(map_float_list_to_preset(
                floats, pr_name or f"{Path(filename).stem}_{p + 1:02d}", "VST 2.4 FXB (FxBk)", filename
            ))

    elif fx_magic == b"FBCh":
        # Opaque Bank Chunk
        num_programs = struct.unpack(">i", data[20:24])[0]
        chunk_size = struct.unpack(">I", data[156:160])[0] if len(data) >= 160 else len(data) - 160
        chunk_data = data[160:160 + chunk_size]
        floats = parse_float_block(chunk_data, "<")
        if not floats:
            floats = parse_float_block(chunk_data, ">")
        presets.append(map_float_list_to_preset(floats, Path(filename).stem, "VST 2.4 FXB (FBCh)", filename))

    else:
        raise ValueError(f"Unknown VST fxMagic: {fx_magic!r}")

    return presets


def parse_native_wasp_chunk(
    chunk: bytes,
    default_name: str = "",
    filename: str = ""
) -> List[ParsedPreset]:
    """
    Parses a native Wasp / Wasp XT chunk (Delphi FruityPlug or raw float block).
    Supports:
      1. Embedded 'CcnK' VST fxp
      2. Versioned chunk with 56 floats + 1 byte flags (e.g. version 13, 229 bytes)
      3. Versioned chunk with 41 or 52 floats (older Wasp XT versions, 169 or 213 bytes)
      4. Raw 55 or 56 float block
      5. Multi-offset scanning for 32..56 normalized floats in range [-3.5, 3.5]
      6. Raw byte knobs fallback (0..255)
    """
    if len(chunk) < 32:
        return []

    p_name = default_name.strip() or (Path(filename).stem if filename else "Migrated Preset")

    # 1. Embedded CcnK check
    ccnk_idx = chunk.find(b"CcnK")
    if ccnk_idx != -1:
        try:
            sub = parse_fxp_data(chunk[ccnk_idx:], filename)
            if sub:
                for p in sub:
                    if not p.name or p.name.startswith("Patch ") or p.name == "Migrated Preset":
                        p.name = p_name
                return sub
        except Exception:
            pass

    # 2. Native FL Studio Wasp XT chunk (Delphi SaveRestoreState):
    # 4-byte version (10..13) + 224 bytes (56 dwords) + 1 byte flags = 229 bytes
    if len(chunk) >= 229:
        version = struct.unpack("<I", chunk[:4])[0]
        if 1 <= version <= 32:
            uints = struct.unpack("<56I", chunk[4:4 + 224])
            is_delphi = all(u <= 2048 for u in uints)
            flags = chunk[4 + 224]
            if is_delphi:
                ints = struct.unpack("<56i", chunk[4:4 + 224])
                return [map_delphi_wasp_integers_to_preset(
                    ints, p_name, f"FL Studio Wasp XT Native (State v{version})", filename, flags
                )]
            else:
                floats = parse_float_block(chunk[4:4 + 224], "<")
                if len(floats) == 56:
                    return [map_native_wasp_floats_to_preset(
                        floats, p_name, f"FL Studio Wasp XT Native (State v{version})", filename, flags
                    )]

    # 3. Native FL Studio older version: 164 bytes (41 dwords) or 208 bytes (52 dwords)
    if len(chunk) >= 169:
        version = struct.unpack("<I", chunk[:4])[0]
        if 1 <= version <= 32:
            num_dwords = 52 if len(chunk) >= 213 else 41
            uints = struct.unpack(f"<{num_dwords}I", chunk[4:4 + num_dwords * 4])
            is_delphi = all(u <= 2048 for u in uints)
            flags = chunk[4 + num_dwords * 4] if len(chunk) >= 4 + num_dwords * 4 + 1 else 0
            if is_delphi:
                ints = struct.unpack(f"<{num_dwords}i", chunk[4:4 + num_dwords * 4])
                return [map_delphi_wasp_integers_to_preset(
                    ints, p_name, f"FL Studio Wasp XT Native (State v{version})", filename, flags
                )]
            else:
                floats = parse_float_block(chunk[4:4 + num_dwords * 4], "<")
                if len(floats) == num_dwords:
                    return [map_native_wasp_floats_to_preset(
                        floats, p_name, f"FL Studio Wasp XT Native (State v{version})", filename, flags
                    )]

    # 4. Raw 55 or 56 floats at offset 0
    if len(chunk) >= 55 * 4:
        floats = parse_float_block(chunk, "<")
        if not floats:
            floats = parse_float_block(chunk, ">")
        if len(floats) >= 55:
            if len(floats) >= 56:
                return [map_native_wasp_floats_to_preset(
                    floats[:56], p_name, "FL Studio FST (Raw Floats)", filename, 0
                )]
            else:
                return [map_float_list_to_preset(
                    floats[:55], p_name, "FL Studio FST (Raw Floats)", filename, WASP_XT_PARAM_ORDER
                )]

    # 5. Multi-offset scanning for 32..56 normalized floats in range [-3.5, 3.5]
    for off in range(0, min(68, len(chunk) - 32 * 4 + 1), 4):
        count = min(56, (len(chunk) - off) // 4)
        if count < 32:
            continue
        try:
            cand = list(struct.unpack(f"<{count}f", chunk[off:off + count * 4]))
            valid_cnt = sum(1 for f in cand if math.isfinite(f) and -3.5 <= f <= 3.5)
            if valid_cnt >= (count * 85) // 100:
                if len(cand) >= 56:
                    return [map_native_wasp_floats_to_preset(
                        cand[:56], p_name, "FL Studio FST (Scanned Block)", filename, 0
                    )]
                else:
                    return [map_float_list_to_preset(
                        cand, p_name, "FL Studio FST (Scanned Block)", filename,
                        CLASSIC_WASP_PARAM_ORDER if len(cand) <= 36 else WASP_XT_PARAM_ORDER
                    )]
        except struct.error:
            pass

    # 6. Raw byte knob fallback (0..255)
    if len(chunk) >= 32:
        count = min(len(chunk), 56)
        byte_floats = [b / 255.0 for b in chunk[:count]]
        if len(byte_floats) >= 56:
            return [map_native_wasp_floats_to_preset(
                byte_floats, p_name, "FL Studio FST (Raw Bytes)", filename, 0
            )]
        else:
            return [map_float_list_to_preset(
                byte_floats, p_name, "FL Studio FST (Raw Bytes)", filename,
                CLASSIC_WASP_PARAM_ORDER if len(byte_floats) <= 36 else WASP_XT_PARAM_ORDER
            )]

    return []


def parse_fst_data(data: bytes, filename: str = "") -> List[ParsedPreset]:
    """
    Parses FL Studio State (.fst) files.
    Supports:
      - Direct embedded VST CcnK chunks
      - Embedded FLhd/FLdt event streams (modern FL Studio .fst)
      - RIFF containers with subchunks ('data', 'plug', 'stat', etc.)
      - Raw plugin chunks via parse_native_wasp_chunk
    """
    if len(data) < 8:
        raise ValueError("File too small for FST state")

    base_name = Path(filename).stem if filename else "Migrated FST"

    # 1. Direct embedded VST CcnK check
    ccnk_idx = data.find(b"CcnK")
    if ccnk_idx != -1:
        try:
            return parse_fxp_data(data[ccnk_idx:], filename)
        except Exception:
            pass

    # 2. Embedded FLhd / FLdt Stream
    flhd_idx = data.find(b"FLhd")
    if flhd_idx != -1:
        flp_out = parse_flp_data(data[flhd_idx:], filename)
        if flp_out:
            return flp_out

    # 3. RIFF Container
    if data.startswith(b"RIFF") and len(data) >= 12:
        pos = 12
        while pos + 8 <= len(data):
            chunk_len = struct.unpack("<I", data[pos + 4:pos + 8])[0]
            pos += 8
            if pos + chunk_len > len(data):
                break
            chunk_payload = data[pos:pos + chunk_len]
            pos += (chunk_len + 1) & ~1  # 2-byte alignment

            sub = parse_native_wasp_chunk(chunk_payload, base_name, filename)
            if sub:
                return sub

    # 4. Raw file scan fallback
    fallback = parse_native_wasp_chunk(data, base_name, filename)
    if fallback:
        return fallback

    raise ValueError(f"Unable to parse .fst state: unrecognized format in '{filename}'")


def read_varint_from_buffer(buf: bytes, pos: int) -> Tuple[int, int]:
    """
    Decodes an FLP variable-length integer (LEB128/varint) from buf starting at pos.
    Returns (decoded_value, new_pos).
    """
    result = 0
    shift = 0
    while pos < len(buf):
        b = buf[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        shift += 7
        if not (b & 0x80):
            break
    return result, pos


def parse_flp_data(data: bytes, filename: str = "") -> List[ParsedPreset]:
    """
    Parses FL Studio project (.flp) files and FLhd-based .fst preset streams.
    Scans for event 213 (0xD5 PluginID.Data) and event 197 (0xC5 FLP_PluginData)
    identifying as Wasp or Wasp XT.
    """
    flhd_idx = data.find(b"FLhd")
    if flhd_idx == -1:
        raise ValueError("Invalid FLP: missing 'FLhd' header")

    pos = flhd_idx
    header_len = struct.unpack("<I", data[pos + 4:pos + 8])[0]
    pos += 8 + header_len

    if pos + 8 > len(data):
        raise ValueError("Truncated FLP file")

    data_magic = data[pos:pos + 4]
    if data_magic != b"FLdt":
        fldt_idx = data.find(b"FLdt", pos)
        if fldt_idx == -1:
            raise ValueError("Invalid FLP: missing 'FLdt' data chunk")
        pos = fldt_idx

    fldt_len = struct.unpack("<I", data[pos + 4:pos + 8])[0]
    pos += 8
    end_pos = min(len(data), pos + fldt_len)

    presets: List[ParsedPreset] = []
    current_chan_name = ""
    current_plugin_name = ""
    chan_counter = 0
    is_fst_file = filename.lower().endswith(".fst")
    file_stem = Path(filename).stem if filename else ""

    while pos < end_pos:
        cmd = data[pos]
        pos += 1

        if cmd < 64:  # 1-byte event
            if pos >= end_pos:
                break
            pos += 1
            if cmd == 0:  # FLP_NewChannel
                chan_counter += 1
                current_chan_name = f"Channel {chan_counter}"
                current_plugin_name = ""

        elif cmd < 128:  # 2-byte event
            pos += 2

        elif cmd < 192:  # 4-byte event
            pos += 4

        else:  # >= 192: Variable length chunk
            chunk_len, pos = read_varint_from_buffer(data, pos)
            if pos + chunk_len > end_pos:
                break
            chunk_bytes = data[pos:pos + chunk_len]
            pos += chunk_len

            # String events:
            # 192 = Channel Title / Name
            # 196 = Plugin Name / Sample Path
            # 201 = PluginID.InternalName (e.g. "Wasp XT", "Wasp", "Fruity Wrapper")
            # 203 = PluginID.Name (e.g. "Wasp XT")
            if cmd in (192, 196, 201, 203):
                try:
                    str_val = chunk_bytes.split(b"\x00")[0].decode("utf-8", errors="replace").strip()
                    if str_val:
                        if cmd == 192 and (not current_chan_name or current_chan_name.startswith("Channel ")):
                            current_chan_name = str_val
                        if cmd in (196, 201, 203):
                            current_plugin_name = str_val
                except Exception:
                    pass

            # Note: 212 = Wrapper GUI settings, must NOT be parsed as plugin chunk
            elif cmd in (213, 197):
                is_wasp = (
                    "wasp" in current_plugin_name.lower() or
                    "wasp" in current_chan_name.lower() or
                    "wasp" in filename.lower() or
                    is_fst_file or
                    b"Wasp" in chunk_bytes or
                    b"CcnK" in chunk_bytes
                )

                if is_fst_file and file_stem and not file_stem.startswith("test_") and file_stem.lower() != "migrated fst":
                    p_name = file_stem
                elif current_chan_name and not current_chan_name.startswith("Channel "):
                    p_name = current_chan_name
                elif current_plugin_name:
                    p_name = current_plugin_name
                else:
                    p_name = f"Wasp_Chan_{chan_counter or 1}"

                sub = parse_native_wasp_chunk(chunk_bytes, p_name, filename)
                if sub:
                    if is_wasp or not filename.lower().endswith(".flp"):
                        if filename.lower().endswith(".flp"):
                            for p in sub:
                                p.source_format = f"FL Studio FLP Event 0x{cmd:02X}"
                        presets.extend(sub)

    return presets


def parse_xml_preset(data: bytes, filename: str = "") -> List[ParsedPreset]:
    """Parses a native Bumbler XD or APVTS XML preset file."""
    try:
        root = ET.fromstring(data.decode("utf-8", errors="replace"))
        name = root.attrib.get("name", Path(filename).stem)
        category = root.attrib.get("category", "Synth")
        params: Dict[str, float] = {}
        for param_node in root.findall("PARAM"):
            pid = param_node.attrib.get("id")
            val = param_node.attrib.get("value")
            if pid and val:
                try:
                    params[pid] = float(val)
                except ValueError:
                    pass
        return [ParsedPreset(
            name=name,
            category=category,
            parameters=params,
            source_format="Bumbler XD APVTS XML",
            source_file=filename
        )]
    except Exception as exc:
        raise ValueError(f"Failed to parse XML preset '{filename}': {exc}")


def parse_preset_file(file_path: Path) -> List[ParsedPreset]:
    """
    High-level entry point: parses any legacy preset file (.fxp, .fxb, .fst, .flp)
    or native XML preset based on extension and magic bytes.
    """
    data = file_path.read_bytes()
    ext = file_path.suffix.lower()

    if ext == ".xml":
        return parse_xml_preset(data, file_path.name)
    elif ext in (".fxp", ".fxb") or data.startswith(b"CcnK"):
        return parse_fxp_data(data, file_path.name)
    elif ext == ".fst":
        return parse_fst_data(data, file_path.name)
    elif ext == ".flp" or data.startswith(b"FLhd"):
        return parse_flp_data(data, file_path.name)
    else:
        # Heuristic probe
        if b"CcnK" in data:
            return parse_fxp_data(data[data.find(b"CcnK"):], file_path.name)
        if b"FLhd" in data:
            return parse_flp_data(data, file_path.name)
        if data.startswith(b"RIFF"):
            return parse_fst_data(data, file_path.name)
        if data.strip().startswith(b"<?xml") or data.strip().startswith(b"<BumblerXD"):
            return parse_xml_preset(data, file_path.name)
        return parse_fst_data(data, file_path.name)


# =============================================================================
# Exporters: JUCE XML & C++ PresetDefinition
# =============================================================================

def format_float_clean(val: float) -> str:
    """Formats float concisely without trailing zeros where possible."""
    if abs(val - round(val)) < 1e-4:
        return f"{int(round(val))}.0"
    return f"{val:.4f}".rstrip("0").rstrip(".")


def export_preset_to_xml(preset: ParsedPreset) -> str:
    """
    Generates a JUCE APVTS-compatible XML preset representation:
      <BumblerXD schemaVersion="1" name="..." category="...">
        <PARAM id="..." value="..."/>
      </BumblerXD>
    """
    root = ET.Element("BumblerXD", {
        "schemaVersion": "1",
        "name": preset.name,
        "category": preset.category,
        "sourceFormat": preset.source_format,
    })

    # Order parameters logically according to ALL_55_PARAMS
    for spec in ALL_55_PARAMS:
        val = preset.parameters.get(spec.id, spec.default_val)
        ET.SubElement(root, "PARAM", {
            "id": spec.id,
            "value": format_float_clean(val),
        })

    # Pretty-print indentation
    ET.indent(root, space="  ", level=0)
    xml_str = ET.tostring(root, encoding="utf-8", xml_declaration=True).decode("utf-8")
    return xml_str + "\n"


def export_preset_to_cpp_struct(preset: ParsedPreset, index: int = 0) -> str:
    """
    Generates C++ code suitable for PresetParameters.h:
      p[index].name = "...";
      p[index].category = "...";
      p[index].params.osc1Waveform = ...f;
      ...
    """
    p = preset.parameters
    lines = [
        f"        // --------------------------------------------------------------------",
        f"        // {index + 1}. {preset.name} ({preset.category})",
        f"        // --------------------------------------------------------------------",
        f'        p[{index}].name = "{preset.name}";',
        f'        p[{index}].category = "{preset.category}";',
        f"        p[{index}].params.osc1Waveform  = {p['osc1Waveform']:.2f}f;",
        f"        p[{index}].params.osc1Octave    = {p['osc1Octave']:.2f}f;",
        f"        p[{index}].params.osc1Fine      = {p['osc1Fine']:.4f}f;",
        f"        p[{index}].params.osc2Waveform  = {p['osc2Waveform']:.2f}f;",
        f"        p[{index}].params.osc2Octave    = {p['osc2Octave']:.2f}f;",
        f"        p[{index}].params.osc2Fine      = {p['osc2Fine']:.4f}f;",
        f"        p[{index}].params.oscMix        = {p['oscMix']:.4f}f;",
        f"        p[{index}].params.osc3Waveform  = {p['osc3Waveform']:.2f}f;",
        f"        p[{index}].params.osc3Level     = {p['osc3Level']:.4f}f;",
        f"        p[{index}].params.ringModMix    = {p['ringModMix']:.4f}f;",
        f"        p[{index}].params.pulseWidth    = {p['pulseWidth']:.4f}f;",
        f"        p[{index}].params.fmAmount      = {p['fmAmount']:.4f}f;",
        f"        p[{index}].params.velToAmp      = {p['velToAmp']:.4f}f;",
        f"        p[{index}].params.velToFilter   = {p['velToFilter']:.4f}f;",
        f"",
        f"        p[{index}].params.filterMode      = {p['filterMode']:.2f}f;",
        f"        p[{index}].params.filterCutoff    = {p['filterCutoff']:.2f}f;",
        f"        p[{index}].params.filterResonance = {p['filterResonance']:.4f}f;",
        f"        p[{index}].params.filterKbTrack   = {p['filterKbTrack']:.4f}f;",
        f"        p[{index}].params.filterEnvAmount = {p['filterEnvAmount']:.4f}f;",
        f"",
        f"        p[{index}].params.ampAttack     = {p['ampAttack']:.4f}f;",
        f"        p[{index}].params.ampDecay      = {p['ampDecay']:.4f}f;",
        f"        p[{index}].params.ampSustain    = {p['ampSustain']:.4f}f;",
        f"        p[{index}].params.ampRelease    = {p['ampRelease']:.4f}f;",
        f"        p[{index}].params.filterAttack  = {p['filterAttack']:.4f}f;",
        f"        p[{index}].params.filterDecay   = {p['filterDecay']:.4f}f;",
        f"        p[{index}].params.filterSustain = {p['filterSustain']:.4f}f;",
        f"        p[{index}].params.filterRelease = {p['filterRelease']:.4f}f;",
        f"        p[{index}].params.envLink       = {p['envLink']:.2f}f;",
        f"",
        f"        p[{index}].params.modAttack     = {p['modAttack']:.4f}f;",
        f"        p[{index}].params.modDecay      = {p['modDecay']:.4f}f;",
        f"        p[{index}].params.modAmount     = {p['modAmount']:.4f}f;",
        f"        p[{index}].params.modTarget     = {p['modTarget']:.2f}f;",
        f"",
        f"        p[{index}].params.lfo1Waveform = {p['lfo1Waveform']:.2f}f;",
        f"        p[{index}].params.lfo1Rate     = {p['lfo1Rate']:.4f}f;",
        f"        p[{index}].params.lfo1Delay    = {p['lfo1Delay']:.4f}f;",
        f"        p[{index}].params.lfo1Sync     = {p['lfo1Sync']:.2f}f;",
        f"        p[{index}].params.lfo1SyncDiv  = {p['lfo1SyncDiv']:.2f}f;",
        f"        p[{index}].params.lfo1KeyReset = {p['lfo1KeyReset']:.2f}f;",
        f"        p[{index}].params.lfo1Amount   = {p['lfo1Amount']:.4f}f;",
        f"        p[{index}].params.lfo1Target   = {p['lfo1Target']:.2f}f;",
        f"",
        f"        p[{index}].params.lfo2Waveform = {p['lfo2Waveform']:.2f}f;",
        f"        p[{index}].params.lfo2Rate     = {p['lfo2Rate']:.4f}f;",
        f"        p[{index}].params.lfo2Delay    = {p['lfo2Delay']:.4f}f;",
        f"        p[{index}].params.lfo2Sync     = {p['lfo2Sync']:.2f}f;",
        f"        p[{index}].params.lfo2SyncDiv  = {p['lfo2SyncDiv']:.2f}f;",
        f"        p[{index}].params.lfo2KeyReset = {p['lfo2KeyReset']:.2f}f;",
        f"        p[{index}].params.lfo2Amount   = {p['lfo2Amount']:.4f}f;",
        f"        p[{index}].params.lfo2Target   = {p['lfo2Target']:.2f}f;",
        f"",
        f"        p[{index}].params.driveEnabled = {p['driveEnabled']:.2f}f;",
        f"        p[{index}].params.driveAmount  = {p['driveAmount']:.4f}f;",
        f"        p[{index}].params.driveTone    = {p['driveTone']:.4f}f;",
        f"        p[{index}].params.dualMode     = {p['dualMode']:.2f}f;",
        f"        p[{index}].params.analogMode   = {p['analogMode']:.2f}f;",
        f"        p[{index}].params.wNoiseMode   = {p['wNoiseMode']:.2f}f;",
        f"        p[{index}].params.masterVolume = {p['masterVolume']:.4f}f;",
    ]
    return "\n".join(lines)


def export_presets_to_cpp_header(presets: List[ParsedPreset]) -> str:
    """
    Generates a full C++ header snippet containing all migrated presets.
    """
    n = len(presets)
    sections = [
        "// ============================================================================",
        "// Auto-Generated Presets from Legacy Wasp / Wasp XT Preset Migration Importer",
        "// Generated for Bumbler XD Synthesizer",
        "// ============================================================================",
        '#pragma once',
        '',
        '#include "PresetParameters.h"',
        '#include <array>',
        '',
        'namespace bumbler {',
        '',
        f'inline const std::array<PresetDefinition, {n}>& getMigratedPresets() {{',
        f'    static const std::array<PresetDefinition, {n}> presets = []() {{',
        f'        std::array<PresetDefinition, {n}> p {{}};',
        '',
    ]

    for idx, preset in enumerate(presets):
        sections.append(export_preset_to_cpp_struct(preset, idx))
        sections.append("")

    sections.extend([
        "        return p;",
        "    }();",
        "    return presets;",
        "}",
        "",
        "} // namespace bumbler",
        "",
    ])
    return "\n".join(sections)


# =============================================================================
# CLI Interface & Batch Processing
# =============================================================================

def sanitize_filename(name: str) -> str:
    """Replaces unsafe filename characters with underscores."""
    safe = "".join(c if c.isalnum() or c in ("-", "_") else "_" for c in name.strip())
    return safe or "preset"


def process_files(
    input_paths: Sequence[Path],
    output_dir: Path,
    export_cpp_path: Optional[Path] = None,
    default_category: Optional[str] = None,
    verbose: bool = False,
    dry_run: bool = False
) -> List[ParsedPreset]:
    """Processes all specified input files and writes converted outputs."""
    all_presets: List[ParsedPreset] = []

    for path in input_paths:
        if path.is_dir():
            files = sorted([p for p in path.rglob("*") if p.suffix.lower() in (".fxp", ".fxb", ".fst", ".flp", ".xml")])
            if verbose:
                print(f"[INFO] Discovered {len(files)} preset file(s) in '{path}'")
            for f in files:
                all_presets.extend(process_single_file(f, default_category, verbose))
        elif path.is_file():
            all_presets.extend(process_single_file(path, default_category, verbose))
        else:
            print(f"[WARN] Input path does not exist: {path}", file=sys.stderr)

    if not dry_run:
        output_dir.mkdir(parents=True, exist_ok=True)
        for preset in all_presets:
            cat_dir = output_dir / preset.category
            cat_dir.mkdir(parents=True, exist_ok=True)
            out_file = cat_dir / f"{sanitize_filename(preset.name)}.xml"
            xml_content = export_preset_to_xml(preset)
            out_file.write_text(xml_content, encoding="utf-8")
            if verbose:
                print(f"[EXPORT] Wrote XML preset: {out_file}")

        if export_cpp_path:
            cpp_content = export_presets_to_cpp_header(all_presets)
            export_cpp_path.parent.mkdir(parents=True, exist_ok=True)
            export_cpp_path.write_text(cpp_content, encoding="utf-8")
            print(f"[EXPORT] Wrote C++ definitions: {export_cpp_path}")

    return all_presets


def process_single_file(
    file_path: Path,
    default_category: Optional[str] = None,
    verbose: bool = False
) -> List[ParsedPreset]:
    """Parses a single legacy file and returns extracted presets."""
    try:
        presets = parse_preset_file(file_path)
        for p in presets:
            if default_category:
                p.category = default_category
            if verbose:
                print(f"  [PARSED] '{p.name}' ({p.category}) from '{file_path.name}' via {p.source_format}")
        return presets
    except Exception as exc:
        print(f"[ERROR] Failed to parse '{file_path}': {exc}", file=sys.stderr)
        return []


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Legacy Wasp & Wasp XT Preset Migration Importer for Bumbler XD",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--gui",
        action="store_true",
        help="Launch graphical desktop migration assistant"
    )
    parser.add_argument(
        "--input", "-i",
        required=False,
        nargs="+",
        help="Path to legacy preset file(s) or directory (.fxp, .fxb, .fst, .flp)"
    )
    parser.add_argument(
        "positional_inputs",
        nargs="*",
        help="Optional positional path(s) to legacy preset file(s) or directory"
    )
    parser.add_argument(
        "--output-dir", "-o",
        default="presets/migrated",
        help="Destination directory for generated JUCE APVTS XML preset files"
    )
    parser.add_argument(
        "--export-cpp", "-c",
        nargs="?",
        const="source/plugin/parameters/MigratedPresets.h",
        default=None,
        help="Path to generate C++ PresetDefinition header (defaults to source/plugin/parameters/MigratedPresets.h if flag passed without value)"
    )
    parser.add_argument(
        "--category",
        default=None,
        help="Override category for all imported presets (e.g., Bass, Lead, Pad, Pluck, Percussion, FX)"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable detailed diagnostic logging"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse input files and report statistics without writing output files"
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    # If invoked directly without arguments in an interactive terminal / GUI environment, launch GUI
    if argv is None:
        effective_argv = sys.argv[1:]
        is_interactive = bool(sys.stdin and hasattr(sys.stdin, "isatty") and sys.stdin.isatty())
        if len(effective_argv) == 0 and is_interactive:
            try:
                from scripts.migrator_gui import launch_gui
            except ImportError:
                from migrator_gui import launch_gui
            launch_gui()
            return 0
    else:
        effective_argv = list(argv)

    parser = build_arg_parser()
    args = parser.parse_args(effective_argv)

    raw_inputs = list(args.input or []) + list(args.positional_inputs or [])

    if args.gui:
        try:
            from scripts.migrator_gui import launch_gui
        except ImportError:
            from migrator_gui import launch_gui
        launch_gui(initial_inputs=raw_inputs if raw_inputs else None)
        return 0

    if not raw_inputs:
        parser.error("the following arguments are required: --input/-i or positional input paths")

    input_paths = [Path(p) for p in raw_inputs]
    output_dir = Path(args.output_dir)
    export_cpp_path = Path(args.export_cpp) if args.export_cpp else None

    presets = process_files(
        input_paths=input_paths,
        output_dir=output_dir,
        export_cpp_path=export_cpp_path,
        default_category=args.category,
        verbose=args.verbose,
        dry_run=args.dry_run
    )

    print(f"Successfully processed {len(presets)} legacy preset(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
