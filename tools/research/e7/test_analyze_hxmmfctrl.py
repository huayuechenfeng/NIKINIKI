#!/usr/bin/env python3

import importlib.util
import struct
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("analyze_hxmmfctrl.py")
SPEC = importlib.util.spec_from_file_location("analyze_hxmmfctrl", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class HxMmfCtrlAnalysisTests(unittest.TestCase):
    def test_saved_error_requires_signed_positive_field90(self):
        for field90 in (0, 0x80000000, 0xFFFFFFFF):
            result = MODULE.select_error_fields(0x40024, field90, 2, 0x8007000E, 0, 4)
            self.assertEqual(result["selected"], 0x40024)
            self.assertEqual(result["saved94"], 0x8007000E)

    def test_state7_overwrites_saved_error_including_zero(self):
        for current in (0, 0x40024):
            result = MODULE.select_error_fields(current, 1, 7, 0x8007000E, 0, 4)
            self.assertEqual(result["saved94"], current)
            self.assertEqual(result["selected"], current)

    def test_other_state_reuses_only_nonzero_saved_error(self):
        result = MODULE.select_error_fields(0x40024, 1, 2, 0x8007000E, 0, 4)
        self.assertEqual(result["selected"], 0x8007000E)
        self.assertEqual(result["selected_from"], "previous_saved94")
        result = MODULE.select_error_fields(0x40024, 1, 2, 0, 0, 4)
        self.assertEqual(result["selected"], 0x40024)

    def test_severity4_returns_after_not_before_aggregate_write(self):
        result = MODULE.select_error_fields(0x40024, 0, 2, 0, 123, 4)
        self.assertEqual(result["aggregate80"], 0x40024)
        self.assertTrue(result["aggregate_written"])
        self.assertEqual(result["terminal"], "return_after_commit")

    def test_special_notification_bypasses_direct_aggregate_write(self):
        result = MODULE.select_error_fields(0x406A2, 1, 7, 3, 123, 4)
        self.assertEqual(result["saved94"], 0x406A2)
        self.assertEqual(result["aggregate80"], 123)
        self.assertFalse(result["aggregate_written"])

    def test_dispatch_mapping_rejects_unverified_code(self):
        with self.assertRaisesRegex(ValueError, "mapping mismatch"):
            MODULE.onerror_dispatch_map(bytes(0x41970), 0x8000)

    def test_parse_petran_code_reconstructs_little_endian_words(self):
        text = "000000: 11223344 AABBCCDD \n000008: 01020304 05060708 \n"
        self.assertEqual(
            MODULE.parse_petran_code(text, 16),
            bytes.fromhex("44332211DDCCBBAA0403020108070605"),
        )

    def test_parse_petran_code_rejects_gap(self):
        with self.assertRaisesRegex(ValueError, r"incomplete at code \+0x4"):
            MODULE.parse_petran_code("000000: 11223344 \n000008: AABBCCDD \n", 12)

    def test_hx_result_table_reports_partial_playback_pair(self):
        code = bytearray(
            MODULE.HX_TO_SYMBIAN_TABLE_OFFSET
            + MODULE.HX_TO_SYMBIAN_TABLE_ENTRIES * 8
        )
        for index in range(MODULE.HX_TO_SYMBIAN_TABLE_ENTRIES):
            struct.pack_into("<Ii", code, MODULE.HX_TO_SYMBIAN_TABLE_OFFSET + index * 8, index, -2)
        target_index = 38
        struct.pack_into(
            "<Ii",
            code,
            MODULE.HX_TO_SYMBIAN_TABLE_OFFSET + target_index * 8,
            MODULE.HX_PARTIAL_PLAYBACK,
            MODULE.SYMBIAN_PARTIAL_PLAYBACK,
        )
        report = MODULE.hx_result_table(bytes(code))
        self.assertEqual(
            report["partial_playback_matches"],
            [
                {
                    "index": target_index,
                    "hx_result": "0x00040024",
                    "symbian_error": -12017,
                }
            ],
        )


if __name__ == "__main__":
    unittest.main()
