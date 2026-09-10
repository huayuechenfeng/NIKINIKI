#!/usr/bin/env python3

import importlib.util
import struct
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("analyze_rom_module_copy.py")
SPEC = importlib.util.spec_from_file_location("analyze_rom_module_copy", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def sample(code_size=0x100, export_count=4, included_code=0xF8):
    data = bytearray(MODULE.ROM_IMAGE_HEADER_SIZE + included_code)
    values = [
        MODULE.KDYNAMIC_LIBRARY_UID,
        0x101F4549,
        0x101F4569,
        0,
        0x80001078,
        0x80001078,
        0,
        code_size,
        code_size,
        0,
        0,
        0x1000,
        0x200000,
        0x2000,
        0x80001178,
        export_count,
        0x80001168,
    ]
    struct.pack_into("<17I", data, 0, *values)
    struct.pack_into("<I", data, 0x58, 0x28)
    return bytes(data)


class HeaderTests(unittest.TestCase):
    def test_maps_file_after_header_to_code_address(self):
        header = MODULE.parse_header(sample())
        self.assertEqual(
            MODULE.mapped_address(header, MODULE.ROM_IMAGE_HEADER_SIZE),
            header["code_address"],
        )

    def test_rejects_header_offset_as_code(self):
        with self.assertRaises(ValueError):
            MODULE.mapped_address(
                MODULE.parse_header(sample()), MODULE.ROM_IMAGE_HEADER_SIZE - 1
            )

    def test_rejects_non_dll_uid(self):
        data = bytearray(sample())
        struct.pack_into("<I", data, 0, 0x1000007A)
        path = Path(self.id() + ".bin")
        path.write_bytes(data)
        try:
            with self.assertRaises(ValueError):
                MODULE.analyze(path)
        finally:
            path.unlink()

    def test_reports_partial_declared_extent_and_exports(self):
        path = Path(self.id() + ".bin")
        path.write_bytes(sample())
        try:
            result = MODULE.analyze(path)
        finally:
            path.unlink()
        self.assertFalse(result["complete_declared_code_extent"])
        self.assertEqual(result["missing_declared_code_tail_bytes"], 8)
        self.assertEqual(result["available_export_entries"], 2)
        self.assertEqual(result["missing_export_entries"], 2)


if __name__ == "__main__":
    unittest.main()
