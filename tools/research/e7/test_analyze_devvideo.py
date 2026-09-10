import importlib.util
from pathlib import Path
import unittest


PATH = Path(__file__).with_name("analyze_devvideo.py")
SPEC = importlib.util.spec_from_file_location("analyze_devvideo", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class AnalyzeDevVideoTests(unittest.TestCase):
    def test_target_phone_copy_mapping(self):
        target = PATH.parents[3] / ".tmp/e7-mmf-runtime/devvideo/DevVideo.dll"
        if not target.exists():
            self.skipTest("private phone copy is not present")
        result = MODULE.analyze(target)
        callback = result["proxy_initialize_complete"]
        self.assertEqual(callback["pointer"], 0x80605AB9)
        self.assertEqual(callback["body_address"], 0x80605A70)
        self.assertEqual(callback["instruction_set"], "Thumb")
        self.assertEqual(result["missing_declared_tail_bytes"], 0x78)
        self.assertTrue(result["live_pid_70569_relation"]["both_within_available_code"])
        self.assertTrue(all(
            item["matches_expected"]
            for item in callback["verified_windows"].values()
        ))

    def test_file_offset_rejects_address_before_code(self):
        with self.assertRaises(ValueError):
            MODULE.file_offset({"code_address": 0x1000}, 0x0FFF)


if __name__ == "__main__":
    unittest.main()
