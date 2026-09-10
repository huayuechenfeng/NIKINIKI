import importlib.util
import struct
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("analyze_mdfvidrender.py")
SPEC = importlib.util.spec_from_file_location("analyze_mdfvidrender", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class MdfVidRenderAnalysisTests(unittest.TestCase):
    def test_backward_arm_branch(self):
        self.assertEqual(MODULE.arm_branch_target(0x1C1C, 0xEAFFFEB3), 0x16F0)

    def test_runtime_observations_share_candidate_base(self):
        vtable_offset = 0x2A9A4
        first_offset = 0x12E8
        second_offset = 0x1C18
        self.assertEqual(MODULE.RUNTIME_VTABLE - vtable_offset, 0x7C980000)
        self.assertEqual(MODULE.RUNTIME_FIRST_METHOD - first_offset, 0x7C980000)
        self.assertEqual(MODULE.RUNTIME_SECOND_METHOD - second_offset, 0x7C980000)

    def test_expected_vtable_slots(self):
        table = b"".join(
            struct.pack("<I", value) for value in MODULE.EXPECTED_VTABLE_PREFIX
        )
        self.assertEqual(
            struct.unpack_from("<I", table, MODULE.FIRST_SLOT)[0], 0x92E8
        )
        self.assertEqual(
            struct.unpack_from("<I", table, MODULE.SECOND_SLOT)[0], 0x9C18
        )

    def test_second_body_pin_includes_both_raw_return_gates(self):
        self.assertEqual(MODULE.SECOND_BODY_OFFSET, 0x16F0)
        self.assertEqual(MODULE.SECOND_BODY_WORDS[0x1770], 0xE12FFF33)
        self.assertEqual(MODULE.SECOND_BODY_WORDS[0x1774], 0xE2505000)
        self.assertEqual(MODULE.SECOND_BODY_WORDS[0x192C], 0xEB001E25)
        self.assertEqual(MODULE.SECOND_BODY_WORDS[0x1930], 0xE1A05000)
        self.assertEqual(MODULE.SECOND_BODY_WORDS[0x1C0C], 0xE1A00005)

    def test_init_helper_pin_keeps_common_join_and_local_codes_distinct(self):
        self.assertEqual(MODULE.INIT_HELPER_OFFSET, 0x91C8)
        self.assertEqual(MODULE.INIT_HELPER_WORDS[0x9644], 0xE2505000)
        self.assertEqual(MODULE.INIT_HELPER_WORDS[0x9764], 0xE51F02F0)

    def test_inner_init_pin_keeps_mapper_return_path(self):
        self.assertEqual(MODULE.INIT_INNER_OFFSET, 0x754C)
        self.assertEqual(MODULE.INIT_INNER_WORDS[0x7E14], 0xEB002B05)
        self.assertEqual(MODULE.INIT_INNER_WORDS[0x7E18], 0xE1A06000)
        self.assertEqual(MODULE.INIT_INNER_WORDS[0x7E4C], 0xE1A00006)

    def test_exception_map_distinguishes_explicit_and_default_efail(self):
        mapping = dict(MODULE.EXPECTED_EXCEPTION_MAP)
        self.assertEqual(mapping[0xFFFFFFFE], 0x80004005)
        self.assertNotIn(0xFFFF1234, mapping)

    def test_create_init_pin_separates_trap_and_later_call(self):
        self.assertEqual(MODULE.CREATE_INIT_WORDS[0x10668], 0xEBFFFFBB)
        self.assertEqual(MODULE.CREATE_INIT_WORDS[0x10698], 0xE3500000)
        self.assertEqual(MODULE.CREATE_INIT_WORDS[0x106B4], 0xEBFFFF76)
        self.assertEqual(MODULE.CREATE_INIT_WORDS[0x106B8], 0xE58D0018)
        self.assertEqual(MODULE.CREATE_INIT_WORDS[0x1049C], 0xEBFFC043)


if __name__ == "__main__":
    unittest.main()
