#!/usr/bin/env python3
"""Synthetic offline log fixtures; no device or private log dependency."""
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "audit_hx_saved_fields", Path(__file__).with_name("audit_hx_saved_fields.py"))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SavedFieldAuditTests(unittest.TestCase):
    def audit_items(self, items):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.jsonl"
            path.write_text("\n".join(json.dumps(item) for item in items), encoding="utf-8")
            return MODULE.audit(path)

    def test_missing_is_unknown_not_zero_or_register_value(self):
        result = self.audit_items([
            {"label": "Registers.getm", "value": ["00000000"]},
            {"label": "hx_send_event_hit", "value": {
                "event_index": 2, "aggregate_hx_result_at_plus_0x80": 0x40024}},
        ])
        self.assertEqual(result["registers_saved"], 1)
        self.assertEqual(result["hits"][0]["line"], 2)
        self.assertEqual(result["hits"][0]["fields"], {
            "0x48": "UNKNOWN", "0x80": 0x40024, "0x90": "UNKNOWN", "0x94": "UNKNOWN"})

    def test_zero_fields_are_preserved_when_explicit(self):
        result = self.audit_items([{"label": "controller_dispatch_identity", "value": {
            "controller": 123, "state_plus_0x48": 7, "aggregate_plus_0x80": 0,
            "selection_state_plus_0x90": 0, "saved_result_plus_0x94": 0}}])
        self.assertEqual(result["hits"][0]["fields"], {
            "0x48": 7, "0x80": 0, "0x90": 0, "0x94": 0})

    def test_trace_and_raw_read_evidence_is_only_counted_not_inferred(self):
        result = self.audit_items([
            {"label": "Memory.get", "value": ["00000000"]},
            {"label": "event", "value": ["E", "Logging", "writeln", '"logger"',
                                              '"HXMMFCtrlImpl::GetDownloadID downloadID=0"']},
            {"label": "hx_on_error_hit", "value": {}},
        ])
        self.assertEqual(result["raw_memory_reply_lines"], [1])
        self.assertEqual(result["matching_trace_lines"], [2])
        self.assertEqual(result["onerror_hit_lines"], [3])
        self.assertEqual(result["hits"][0]["fields"]["0x90"], "UNKNOWN")


if __name__ == "__main__":
    unittest.main()
