import importlib.util
import struct
import sys
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("analyze_hxmedpltfm.py")
sys.path.insert(0, str(MODULE_PATH.parent))
SPEC = importlib.util.spec_from_file_location("analyze_hxmedpltfm", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class HxMediaPlatformAnalysisTests(unittest.TestCase):
    def test_parse_exports(self):
        text = "Ordinal     3:  0000c6dc\nOrdinal 8: 0000c3e0\n"
        self.assertEqual(MODULE.parse_exports(text), {3: 0xC6DC, 8: 0xC3E0})

    def test_symbol_map(self):
        code = bytearray(MODULE.SYMBOL_TABLE_OFFSET + 16)
        code_base = 0x8000
        code.extend(b"First\0Second\0")
        first = code_base + MODULE.SYMBOL_TABLE_OFFSET + 16
        second = first + 6
        struct.pack_into("<II", code, MODULE.SYMBOL_TABLE_OFFSET, first, 3)
        struct.pack_into("<II", code, MODULE.SYMBOL_TABLE_OFFSET + 8, second, 4)
        old_count = MODULE.SYMBOL_TABLE_ENTRIES
        MODULE.SYMBOL_TABLE_ENTRIES = 2
        try:
            result = MODULE.symbol_map(bytes(code), code_base)
        finally:
            MODULE.SYMBOL_TABLE_ENTRIES = old_count
        self.assertEqual([(item["name"], item["ordinal"]) for item in result],
                         [("First", 3), ("Second", 4)])


if __name__ == "__main__":
    unittest.main()
