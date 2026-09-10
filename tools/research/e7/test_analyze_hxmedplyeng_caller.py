import importlib.util
from pathlib import Path
import struct
import unittest


MODULE_PATH = Path(__file__).with_name("analyze_hxmedplyeng_caller.py")
SPEC = importlib.util.spec_from_file_location("analyze_hxmedplyeng_caller", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class HxMedPlyEngCallerTests(unittest.TestCase):
    def test_arm_prologue_candidates_are_nearest_first(self):
        code = bytearray(0x80)
        struct.pack_into("<I", code, 0x10, 0xE92D4010)
        struct.pack_into("<I", code, 0x30, 0xE92D47F0)
        self.assertEqual(MODULE.preceding_arm_prologues(bytes(code), 0x60), [0x30, 0x10])

    def test_arm_words_keep_code_relative_offsets(self):
        code = struct.pack("<III", 1, 2, 3)
        self.assertEqual(
            MODULE.arm_words(code, 4, 12),
            [
                {"offset": "0x4", "word": "0x00000002"},
                {"offset": "0x8", "word": "0x00000003"},
            ],
        )

    def test_u32_rejects_out_of_range_word(self):
        with self.assertRaises(ValueError):
            MODULE.u32(b"\0\0\0", 0)

    def test_runtime_window_constants_describe_arm_call(self):
        self.assertEqual(MODULE.RUNTIME_WINDOW[-4:], struct.pack("<I", 0xE12FFF3C))
        self.assertEqual(MODULE.RUNTIME_RETURN_ADDRESS - MODULE.RUNTIME_CALL_ADDRESS, 4)

    def test_partial_generator_call_targets_player_on_error(self):
        word = 0xEBFFF394
        self.assertEqual(
            MODULE.arm_branch_target(MODULE.PARTIAL_GENERATOR_CALL_OFFSET, word),
            MODULE.PLAYER_ON_ERROR_OFFSET,
        )

    def test_arm_branch_decoder_rejects_non_branch(self):
        with self.assertRaisesRegex(ValueError, "not ARM B/BL"):
            MODULE.arm_branch_target(0x100, 0xE1A00000)


if __name__ == "__main__":
    unittest.main()
