import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).with_name("analyze_ive_policy.py")
SPEC = importlib.util.spec_from_file_location("analyze_ive_policy", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class IvePolicyAnalysisTests(unittest.TestCase):
    def test_candidate_pair_and_mode_one_path(self):
        root = MODULE.HERE.parents[2]
        private = root / ".tmp/e7-mmf-runtime/ive-policy"
        client = private / "ivepolicyserverclient.rom-candidate.dll"
        server = private / "ivepolicyserver.rom-candidate.exe"
        if not client.exists() or not server.exists():
            self.skipTest("private E7 firmware candidates are unavailable")
        rcam = private / "rcam.rom-candidate.dll"
        result = MODULE.analyze(client, server, rcam=rcam if rcam.exists() else None)
        self.assertEqual(result["client"]["header"]["uid3"], 0x10204C26)
        self.assertEqual(result["server"]["header"]["uid3"], 0x10204C27)
        self.assertEqual(result["request_contract"]["ipc_function"], 1)
        self.assertEqual(
            result["server_decision"]["mode_1_rules"],
            ["CIveRuleClientProcess", "CIveRuleNumOfActiveClients", "CIveRuleResource"],
        )
        self.assertEqual(
            result["server_decision"]["resource_path"]["allocator_return_observation_offset"],
            0x208A,
        )
        self.assertEqual(
            result["server_decision"]["resource_path"]["common_rule_return_observation_offset"],
            0xD98,
        )
        points = result["dynamic_contract"]["minimal_points"]
        self.assertEqual(len(points), 1)
        self.assertEqual(points[0]["code_offset"], 0x12E0)
        self.assertIn("do not observe those points again", result["dynamic_contract"]["phase_context"])
        if rcam.exists():
            imports = result["direct_rcam_boundary"]["server_imports"]
            self.assertEqual(imports[0]["ordinal"], 15)
            self.assertIn("SetClientInfo", imports[0]["name"])
            calls = result["direct_rcam_boundary"]["setclient_calls"]
            self.assertEqual([item["ordinal"] for item in calls], [15, 16])
            self.assertEqual(calls[0]["common_call_link_lr"], 0x80BC97A1)
            self.assertEqual(calls[1]["common_call_link_lr"], 0x80BC9775)
            lower = result["direct_rcam_boundary"]["shared_lower_control"]
            self.assertEqual(lower["rcam_control_target"], 0x80484F6D)
            self.assertEqual(lower["target_sdk_ordinal"], 491)

    def test_setclient_abi_sources_and_nonleaf_lr_are_frozen(self):
        self.assertEqual(MODULE.RCAM_SETCLIENT_CALLS[15]["policy_call"], 0x80D235D6)
        self.assertEqual(MODULE.RCAM_SETCLIENT_CALLS[16]["policy_call"], 0x80D235E4)
        self.assertEqual(MODULE.RCAM_SETCLIENT_CALLS[15]["caller_link_lr"], 0x80D235DB)
        self.assertEqual(MODULE.RCAM_SETCLIENT_CALLS[16]["caller_link_lr"], 0x80D235E9)
        self.assertEqual(MODULE.RCAM_SETCLIENT_CALLS[15]["common_call_link_lr"], 0x80BC97A1)
        self.assertEqual(MODULE.RCAM_SETCLIENT_CALLS[16]["common_call_link_lr"], 0x80BC9775)
        self.assertEqual(
            MODULE.RCAM_ARGUMENT_SOURCES["explicit_1_r1"]["payload"],
            "[current+0x04] original client PID",
        )
        self.assertEqual(
            MODULE.RCAM_ARGUMENT_SOURCES["explicit_1_r1"]["source_stack_copy"],
            "[allocator sp+0x54]",
        )
        self.assertEqual(
            MODULE.RCAM_ARGUMENT_SOURCES["explicit_2_r2"]["source_stack_copy"],
            "[allocator sp+0x58]",
        )
        self.assertEqual(
            MODULE.RCAM_ARGUMENT_SOURCES["explicit_3_r3"]["source_stack_copy"],
            "[allocator sp+0x50]",
        )
        self.assertEqual(
            MODULE.RCAM_ARGUMENT_SOURCES["final_unsigned"]["payload"],
            "[current+0x10] camera handle",
        )
        self.assertEqual(
            MODULE.RCAM_ARGUMENT_SOURCES["final_unsigned"]["source_stack_copy"],
            "[allocator sp+0x3C]",
        )

    def test_dynamic_offsets_are_module_relative_and_bounded(self):
        self.assertGreater(MODULE.CLIENT_REQUEST_ENTRY_OFFSET, 0)
        self.assertGreater(MODULE.SERVER_RESOURCE_RETURN_OFFSET, 0)
        self.assertLess(MODULE.SERVER_RESOURCE_RETURN_OFFSET, MODULE.EXPECTED["server"]["code_size"])
        self.assertLess(MODULE.SERVER_DENY_CALL_OFFSET, MODULE.EXPECTED["server"]["code_size"])

    def test_phone_copy_provenance_preserves_dynamic_identity_gate(self):
        root = MODULE.HERE.parents[2]
        private = root / ".tmp/e7-mmf-runtime/ive-policy"
        client = private / "ivepolicyserverclient.rom-candidate.dll"
        server = private / "ivepolicyserver.rom-candidate.exe"
        if not client.exists() or not server.exists():
            self.skipTest("private E7 firmware candidates are unavailable")
        result = MODULE.analyze(client, server, identity_source="participating-phone-copy")
        self.assertEqual(result["server"]["identity_source"], "participating-phone-copy")
        self.assertIn("same-run", result["server"]["identity_limit"])
        self.assertIn("same-run loaded-module", result["limits"][0])


if __name__ == "__main__":
    unittest.main()
