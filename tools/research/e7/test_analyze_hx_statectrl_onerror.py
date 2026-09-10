import importlib.util
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("analyze_hx_statectrl_onerror.py")
SPEC = importlib.util.spec_from_file_location("analyze_hx_statectrl_onerror", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class StateCtrlOnErrorAnalysisTests(unittest.TestCase):
    def synthetic_code(self):
        size = max((*MODULE.EXPECTED_WORDS, *MODULE.EXPECTED_VTABLE_WORDS)) + 4
        code = bytearray(size)
        all_words = {**MODULE.EXPECTED_WORDS, **MODULE.EXPECTED_VTABLE_WORDS}
        for offset, value in all_words.items():
            code[offset : offset + 4] = value.to_bytes(4, "little")
        return code

    def test_expected_structure(self):
        report = MODULE.analyze(bytes(self.synthetic_code()), require_identity=False)
        self.assertTrue(report["verified"])
        self.assertEqual(report["adjusted_entry"]["code_offset"], "0x6798")
        self.assertFalse(report["forwarding"]["conversion_before_observer_call"])

    def test_instruction_mismatch_fails_closed(self):
        code = self.synthetic_code()
        code[0x6848] ^= 1
        with self.assertRaisesRegex(ValueError, "instruction/vtable mismatch"):
            MODULE.analyze(bytes(code), require_identity=False)

    def test_short_input_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "outside code length"):
            MODULE.analyze(b"\0" * 16, require_identity=False)


if __name__ == "__main__":
    unittest.main()
