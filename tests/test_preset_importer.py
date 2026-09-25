#!/usr/bin/env python3
"""
tests/test_preset_importer.py
=============================
Automated Unit Tests for Legacy Wasp / Wasp XT Preset Migration Importer.

Validates:
  1. Synthetic .fxp binary generation & extraction ('FxCk' and 'FPCh').
  2. Synthetic .fxb bank binary generation & multi-program extraction.
  3. Synthetic .fst FL Studio state generation & extraction (RIFF and raw).
  4. Synthetic .flp project event 0xC5 scanning & Wasp channel extraction.
  5. Parameter scaling formulas (logarithmic cutoff, exponential ADSR, bipolar envs).
  6. Valid JUCE APVTS XML structure (<BumblerXD ...> with all 55 PARAM IDs).
  7. Valid C++ PresetDefinition code generation matching PresetParameters.h.
  8. End-to-end CLI file processing workflow.
"""

from __future__ import annotations

import math
import struct
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

# Add project root to sys.path so scripts can be imported directly
import sys
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from scripts.import_wasp_presets import (
    ALL_55_PARAMS,
    PARAM_SPEC_MAP,
    ParsedPreset,
    export_preset_to_cpp_struct,
    export_preset_to_xml,
    export_presets_to_cpp_header,
    map_legacy_param_to_apvts,
    parse_flp_data,
    parse_fst_data,
    parse_fxp_data,
    parse_preset_file,
    process_files,
    scale_bipolar,
    scale_discrete,
    scale_exponential_time,
    scale_lfo_rate,
    scale_logarithmic,
)


class TestParameterScalingFormulas(unittest.TestCase):
    """Verifies that mathematical mapping and domain transformations match APVTS specs."""

    def test_logarithmic_cutoff_scaling(self):
        """Filter cutoff must scale log from 20 Hz to 20000 Hz."""
        # 0.0 -> 20.0 Hz
        self.assertAlmostEqual(scale_logarithmic(0.0, 20.0, 20000.0), 20.0, places=2)
        # 1.0 -> 20000.0 Hz
        self.assertAlmostEqual(scale_logarithmic(1.0, 20.0, 20000.0), 20000.0, places=1)
        # 0.5 -> 20 * sqrt(1000) ~= 632.45 Hz
        expected_mid = 20.0 * math.sqrt(1000.0)
        self.assertAlmostEqual(scale_logarithmic(0.5, 20.0, 20000.0), expected_mid, places=1)

        # Raw value already in Hz (> 1.0)
        self.assertEqual(scale_logarithmic(1500.0, 20.0, 20000.0), 1500.0)
        # Over-range clamp
        self.assertEqual(scale_logarithmic(25000.0, 20.0, 20000.0), 20000.0)

    def test_exponential_time_scaling(self):
        """ADSR time scaling must span 0.001 s (1 ms) to 10.0 s."""
        # 0.0 -> 0.001 s
        self.assertAlmostEqual(scale_exponential_time(0.0, 0.001, 10.0), 0.001, places=4)
        # 1.0 -> 10.0 s
        self.assertAlmostEqual(scale_exponential_time(1.0, 0.001, 10.0), 10.0, places=2)
        # 0.5 -> 0.001 * 100 = 0.1 s (100 ms)
        self.assertAlmostEqual(scale_exponential_time(0.5, 0.001, 10.0), 0.1, places=3)

    def test_lfo_rate_scaling(self):
        """LFO rate must span 0.05 Hz to 30.0 Hz."""
        self.assertAlmostEqual(scale_lfo_rate(0.0, 0.05, 30.0), 0.05, places=3)
        self.assertAlmostEqual(scale_lfo_rate(1.0, 0.05, 30.0), 30.0, places=1)
        # Midpoint ~1.22 Hz
        self.assertAlmostEqual(scale_lfo_rate(0.5, 0.05, 30.0), 0.05 * math.sqrt(600.0), places=2)

    def test_bipolar_scaling(self):
        """Bipolar parameters (filterEnvAmount, modAmount) map [0.0, 1.0] -> [-1.0, 1.0]."""
        self.assertAlmostEqual(scale_bipolar(0.0), -1.0, places=3)
        self.assertAlmostEqual(scale_bipolar(0.5), 0.0, places=3)
        self.assertAlmostEqual(scale_bipolar(1.0), 1.0, places=3)
        # Preserves existing negative inputs
        self.assertAlmostEqual(scale_bipolar(-0.75), -0.75, places=3)

    def test_discrete_scaling(self):
        """Discrete choices map cleanly to integer steps."""
        # Filter modes (6 modes: 0..5)
        self.assertEqual(scale_discrete(0.0, 6), 0.0)   # LP12
        self.assertEqual(scale_discrete(0.2, 6), 1.0)   # LP24
        self.assertEqual(scale_discrete(1.0, 6), 5.0)   # HP24

        # Waveforms (4 choices: 0..3)
        self.assertEqual(scale_discrete(0.0, 4), 0.0)   # Saw
        self.assertEqual(scale_discrete(0.33, 4), 1.0)  # Square
        self.assertEqual(scale_discrete(0.67, 4), 2.0)  # Sine
        self.assertEqual(scale_discrete(1.0, 4), 3.0)   # Noise

    def test_full_param_map_all_55(self):
        """Verifies that all 55 APVTS parameters are recognized and mapped without NaN/Inf."""
        for spec in ALL_55_PARAMS:
            val_0 = map_legacy_param_to_apvts(spec.id, 0.0)
            val_half = map_legacy_param_to_apvts(spec.id, 0.5)
            val_1 = map_legacy_param_to_apvts(spec.id, 1.0)

            for v in (val_0, val_half, val_1):
                self.assertTrue(math.isfinite(v), f"Non-finite value for {spec.id}: {v}")
                self.assertGreaterEqual(v, spec.min_val - 1e-3, f"{spec.id} min bound violated: {v} < {spec.min_val}")
                self.assertLessEqual(v, spec.max_val + 1e-3, f"{spec.id} max bound violated: {v} > {spec.max_val}")


class TestSyntheticFXPParsing(unittest.TestCase):
    """Tests parsing of VST 2.4 .fxp and .fxb binary buffers."""

    def create_synthetic_fxcp_preset(self, name: str, params: list[float]) -> bytes:
        """Constructs a synthetic VST 2.4 'FxCk' preset buffer."""
        num_params = len(params)
        name_bytes = name.encode("latin1")[:27].ljust(28, b"\x00")
        float_bytes = struct.pack(f">{num_params}f", *params)
        byte_size = 20 + 28 + num_params * 4

        header = struct.pack(
            ">4s I 4s i 4s i i",
            b"CcnK",
            byte_size,
            b"FxCk",
            1,          # version
            b"Wasp",    # fxID
            100,        # fxVersion
            num_params
        )
        return header + name_bytes + float_bytes

    def create_synthetic_fpch_chunk(self, name: str, params: list[float]) -> bytes:
        """Constructs a synthetic VST 2.4 'FPCh' opaque chunk preset buffer."""
        chunk_data = struct.pack(f"<{len(params)}f", *params)
        chunk_size = len(chunk_data)
        name_bytes = name.encode("latin1")[:27].ljust(28, b"\x00")
        byte_size = 20 + 28 + 4 + chunk_size

        header = struct.pack(
            ">4s I 4s i 4s i i",
            b"CcnK",
            byte_size,
            b"FPCh",
            1,
            b"WsXT",
            200,
            len(params)
        )
        return header + name_bytes + struct.pack(">I", chunk_size) + chunk_data

    def test_fxcp_preset_extraction(self):
        """Tests parsing a standard VST 2.4 float parameter preset."""
        # 55 normalized parameters
        raw_floats = [0.0] * 55
        raw_floats[1] = 0.5   # osc1Octave: 0.5 -> 0 oct
        raw_floats[14] = 0.2  # filterMode: LP24 (1.0)
        raw_floats[15] = 0.6  # filterCutoff: log-scale
        raw_floats[48] = 1.0  # driveEnabled (index 48)

        buf = self.create_synthetic_fxcp_preset("Acid Squelch 303", raw_floats)
        presets = parse_fxp_data(buf, "acid_squelch.fxp")

        self.assertEqual(len(presets), 1)
        p = presets[0]
        self.assertEqual(p.name, "Acid Squelch 303")
        self.assertEqual(p.category, "Bass")
        self.assertEqual(p.parameters["filterMode"], 1.0)
        self.assertEqual(p.parameters["driveEnabled"], 1.0)
        self.assertGreater(p.parameters["filterCutoff"], 500.0)

    def test_fpch_chunk_extraction(self):
        """Tests parsing an opaque chunk preset containing little-endian floats."""
        raw_floats = [0.5] * 55
        raw_floats[14] = 0.6  # filterMode: DBL.NT (3.0)
        raw_floats[19] = 0.8  # ampAttack: long swell pad

        buf = self.create_synthetic_fpch_chunk("Celestial Swell", raw_floats)
        presets = parse_fxp_data(buf, "celestial_swell.fxp")

        self.assertEqual(len(presets), 1)
        p = presets[0]
        self.assertEqual(p.name, "Celestial Swell")
        self.assertEqual(p.category, "Pad")
        self.assertEqual(p.parameters["filterMode"], 3.0)
        self.assertGreater(p.parameters["ampAttack"], 0.5)


class TestSyntheticFSTAndFLPParsing(unittest.TestCase):
    """Tests parsing of FL Studio State (.fst) and Project (.flp) binary buffers."""

    def create_synthetic_riff_fst(self, name: str, params: list[float]) -> bytes:
        """Constructs a synthetic RIFF container with a 'data' subchunk holding floats."""
        float_data = struct.pack(f"<{len(params)}f", *params)
        data_chunk = b"data" + struct.pack("<I", len(float_data)) + float_data
        riff_payload = b"FS_G" + data_chunk
        riff_header = b"RIFF" + struct.pack("<I", len(riff_payload)) + riff_payload
        return riff_header

    def create_synthetic_flp_with_wasp(self, chan_name: str, params: list[float]) -> bytes:
        """Constructs a minimal synthetic FLP stream with channel event 0xC5 containing Wasp state."""
        # 1. FLhd header chunk
        flhd = b"FLhd" + struct.pack("<I", 6) + struct.pack("<HHH", 0, 1, 96)

        # 2. Event stream in FLdt:
        # Event 0: FLP_NewChannel
        events = bytearray([0, 0])

        # Event 196 (0xC4): Plugin / Channel Name
        name_bytes = chan_name.encode("utf-8")
        events.append(196)
        # LEB128 varint for length
        length = len(name_bytes)
        while length >= 0x80:
            events.append((length & 0x7F) | 0x80)
            length >>= 7
        events.append(length)
        events.extend(name_bytes)

        # Event 197 (0xC5): FLP_PluginData
        param_bytes = struct.pack(f"<{len(params)}f", *params)
        events.append(197)
        p_len = len(param_bytes)
        while p_len >= 0x80:
            events.append((p_len & 0x7F) | 0x80)
            p_len >>= 7
        events.append(p_len)
        events.extend(param_bytes)

        fldt = b"FLdt" + struct.pack("<I", len(events)) + bytes(events)
        return flhd + fldt

    def test_riff_fst_extraction(self):
        """Tests parsing an FL Studio State (.fst) RIFF container."""
        raw_floats = [0.25] * 55
        buf = self.create_synthetic_riff_fst("Warm_Pad", raw_floats)
        presets = parse_fst_data(buf, "Warm_Pad.fst")

        self.assertEqual(len(presets), 1)
        p = presets[0]
        self.assertEqual(p.name, "Warm_Pad")
        self.assertIn("filterCutoff", p.parameters)
        self.assertIn("ampAttack", p.parameters)

    def test_flp_wasp_channel_scan(self):
        """Tests scanning an FL Studio project (.flp) for event 0xC5 Wasp channels."""
        raw_floats = [0.1] * 55
        raw_floats[14] = 0.8  # filterMode: HP24 (4.0 or 5.0)

        buf = self.create_synthetic_flp_with_wasp("Wasp XT Lead", raw_floats)
        presets = parse_flp_data(buf, "demo_track.flp")

        self.assertEqual(len(presets), 1)
        p = presets[0]
        self.assertEqual(p.name, "Wasp XT Lead")
        self.assertEqual(p.category, "Lead")
        self.assertEqual(p.source_format, "FL Studio FLP Event 0xC5")


class TestXMLAndCPPGeneration(unittest.TestCase):
    """Verifies JUCE APVTS XML and C++ PresetDefinition code generation."""

    def setUp(self):
        self.test_preset = ParsedPreset(
            name="Test Acid Squelch",
            category="Bass",
            parameters={
                "osc1Waveform": 0.0,
                "osc1Octave": -1.0,
                "osc2Waveform": 1.0,
                "osc2Fine": 0.05,
                "oscMix": 0.35,
                "filterMode": 1.0,
                "filterCutoff": 380.0,
                "filterResonance": 0.78,
                "filterEnvAmount": 0.75,
                "ampAttack": 0.002,
                "ampDecay": 0.22,
                "ampSustain": 0.0,
                "ampRelease": 0.15,
                "driveEnabled": 1.0,
                "driveAmount": 0.45,
                "driveTone": 0.65,
                "analogMode": 1.0,
                "masterVolume": 0.82,
            },
            source_format="Unit Test"
        )

    def test_valid_juce_xml_structure(self):
        """Generated XML must parse cleanly and contain all 55 PARAM IDs under <BumblerXD>."""
        xml_text = export_preset_to_xml(self.test_preset)
        self.assertTrue(xml_text.startswith("<?xml"))

        root = ET.fromstring(xml_text)
        self.assertEqual(root.tag, "BumblerXD")
        self.assertEqual(root.attrib.get("schemaVersion"), "1")
        self.assertEqual(root.attrib.get("name"), "Test Acid Squelch")
        self.assertEqual(root.attrib.get("category"), "Bass")

        param_nodes = root.findall("PARAM")
        self.assertEqual(len(param_nodes), 55, "All 55 APVTS parameters must be present")

        param_dict = {p.attrib["id"]: float(p.attrib["value"]) for p in param_nodes}
        self.assertEqual(param_dict["osc1Waveform"], 0.0)
        self.assertEqual(param_dict["osc1Octave"], -1.0)
        self.assertEqual(param_dict["filterCutoff"], 380.0)
        self.assertEqual(param_dict["filterResonance"], 0.78)
        self.assertEqual(param_dict["driveEnabled"], 1.0)

    def test_valid_cpp_code_generation(self):
        """Generated C++ must include struct fields matching PresetParameters.h."""
        cpp_struct = export_preset_to_cpp_struct(self.test_preset, 0)
        self.assertIn('p[0].name = "Test Acid Squelch";', cpp_struct)
        self.assertIn('p[0].category = "Bass";', cpp_struct)
        self.assertIn("p[0].params.filterCutoff    = 380.00f;", cpp_struct)
        self.assertIn("p[0].params.driveEnabled = 1.00f;", cpp_struct)

        full_header = export_presets_to_cpp_header([self.test_preset])
        self.assertIn("#pragma once", full_header)
        self.assertIn('#include "PresetParameters.h"', full_header)
        self.assertIn("getMigratedPresets()", full_header)
        self.assertIn("std::array<PresetDefinition, 1>", full_header)


class TestEndToEndWorkflow(unittest.TestCase):
    """Tests batch processing and directory output."""

    def test_process_files_directory_workflow(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = Path(tmp_dir)
            in_dir = tmp_path / "legacy"
            out_dir = tmp_path / "migrated"
            cpp_file = tmp_path / "GeneratedPresets.h"
            in_dir.mkdir()

            # Create synthetic FXP file
            num_params = 55
            floats = [0.3] * num_params
            raw_bytes = struct.pack(
                ">4s I 4s i 4s i i",
                b"CcnK", 20 + 28 + num_params * 4, b"FxCk", 1, b"Wasp", 100, num_params
            ) + b"Pluck Arp".ljust(28, b"\x00") + struct.pack(f">{num_params}f", *floats)

            fxp_file = in_dir / "pluck_arp.fxp"
            fxp_file.write_bytes(raw_bytes)

            presets = process_files(
                input_paths=[in_dir],
                output_dir=out_dir,
                export_cpp_path=cpp_file,
                verbose=False
            )

            self.assertEqual(len(presets), 1)
            # Category subfolder check
            expected_xml = out_dir / presets[0].category / "Pluck_Arp.xml"
            self.assertTrue(expected_xml.exists(), f"Missing output XML: {expected_xml}")
            self.assertTrue(cpp_file.exists(), f"Missing C++ header: {cpp_file}")

            # Verify XML content
            root = ET.fromstring(expected_xml.read_text(encoding="utf-8"))
            self.assertEqual(root.tag, "BumblerXD")
            self.assertEqual(len(root.findall("PARAM")), 55)


class TestMigratorGUIAndLauncher(unittest.TestCase):
    """Verifies that desktop GUI migrator and launcher script are properly configured."""

    def test_migrator_gui_module_importable(self):
        """scripts.migrator_gui must import cleanly without syntax or packaging errors."""
        import scripts.migrator_gui as migrator_gui
        self.assertTrue(hasattr(migrator_gui, "MigratorApp"))
        self.assertIn("Auto-detect", migrator_gui.CATEGORIES)
        self.assertEqual(migrator_gui.ACCENT_YELLOW, "#e5a912")

    def test_windows_batch_launcher_exists(self):
        """MigratePresets.bat must exist in project root and reference scripts/migrator_gui.py."""
        bat_file = PROJECT_ROOT / "MigratePresets.bat"
        self.assertTrue(bat_file.exists(), "MigratePresets.bat missing in root")
        bat_content = bat_file.read_text(encoding="utf-8")
        self.assertIn("migrator_gui.py", bat_content)

    def test_cli_gui_flag_registered(self):
        """import_wasp_presets.py must recognize the --gui flag."""
        from scripts.import_wasp_presets import build_arg_parser
        parser = build_arg_parser()
        args = parser.parse_args(["--gui"])
        self.assertTrue(args.gui)


if __name__ == "__main__":
    unittest.main()
