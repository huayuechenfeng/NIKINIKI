import importlib.util
from pathlib import Path
import unittest


PATH = Path(__file__).with_name("qt_mmf_internal_map.py")
SPEC = importlib.util.spec_from_file_location("qt_mmf_internal_map", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class QtMmfInternalMapTests(unittest.TestCase):
    def test_parse_address_accepts_hex_and_decimal(self):
        self.assertEqual(MODULE.parse_address("0x8000"), 0x8000)
        self.assertEqual(MODULE.parse_address("32768"), 0x8000)

    def test_observation_points_are_within_expected_code_window(self):
        offsets = [
            item["link_address"] - MODULE.LINK_CODE_BASE
            for item in MODULE.OBSERVATION_POINTS
        ]
        self.assertTrue(all(offset >= 0 for offset in offsets))
        self.assertEqual(len(offsets), len(set(offsets)))


if __name__ == "__main__":
    unittest.main()
