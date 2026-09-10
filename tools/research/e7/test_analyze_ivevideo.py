import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).with_name("analyze_ivevideo.py")
SPEC = importlib.util.spec_from_file_location("analyze_ivevideo", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class IveVideoAnalysisTests(unittest.TestCase):
    def test_exact_phone_copy(self):
        target = MODULE.HERE.parents[2] / ".tmp/e7-mmf-runtime/ive/ivevideodecodehwdevice.dll"
        if not target.exists():
            self.skipTest("private same-device IVE copy is unavailable")
        result = MODULE.analyze(target)
        self.assertEqual(result["header"]["uid3"], 0x10204C1E)
        self.assertEqual(result["avc_implementation_uid"], 0x10204C21)
        self.assertEqual(result["initialize"]["code_address"], 0x80D26322)
        self.assertEqual(result["initialize"]["pointer"], 0x80D26323)
        self.assertEqual(result["policy_callback_failure"]["constructed_error"], -44)
        self.assertEqual(result["policy_callback_failure"]["required_bit"], 0x20)
        self.assertEqual(result["policy_callback_failure"]["entry_trace_id"], 0x88)
        self.assertEqual(result["policy_callback_failure"]["exit_trace_id"], 0x89)
        self.assertEqual(
            result["policy_callback_failure"]["name_basis"],
            "CIveVideoDecodeHwDevice::AccessDenied",
        )

    def test_two_forwarding_return_addresses_are_distinct_thumb_addresses(self):
        self.assertNotEqual(MODULE.SYNC_OBSERVER_RETURN_LR, MODULE.ASYNC_OBSERVER_RETURN_LR)
        self.assertEqual(MODULE.SYNC_OBSERVER_RETURN_LR & 1, 1)
        self.assertEqual(MODULE.ASYNC_OBSERVER_RETURN_LR & 1, 1)
        self.assertEqual(MODULE.POLICY_DENIED_OBSERVER_RETURN_LR & 1, 1)


if __name__ == "__main__":
    unittest.main()
