#!/usr/bin/env python3
"""Offline inventory of actually saved Hx controller fields; never uses CODA.

Reads explicit local JSONL logs and emits whitelisted field evidence with line
numbers. Missing fields are UNKNOWN, never zero. Register blobs are counted but
not reinterpreted as object memory; quiet Memory.get replies cannot be recovered.
"""
import argparse
import hashlib
import json
from pathlib import Path

FIELD_KEYS = {
    "0x48": ("state_plus_0x48",),
    "0x80": ("aggregate_plus_0x80", "aggregate_hx_result_at_plus_0x80", "aggregate_before_plus_0x80"),
    "0x90": ("selection_state_plus_0x90",),
    "0x94": ("saved_result_plus_0x94",),
}
HIT_LABELS = {"hx_send_event_hit", "hx_on_error_hit", "controller_dispatch_identity"}


def audit(path):
    data = path.read_bytes()
    report = dict(name=path.parent.name, sha256=hashlib.sha256(data).hexdigest().upper(),
                  bytes=len(data), hits=[], registers_saved=0, raw_memory_reply_lines=[],
                  onerror_hit_lines=[], matching_trace_lines=[])
    for line, raw in enumerate(data.decode("utf-8-sig").splitlines(), 1):
        item = json.loads(raw)
        label, value = item.get("label"), item.get("value")
        if label == "Registers.getm":
            report["registers_saved"] += 1
        if label == "Memory.get":
            report["raw_memory_reply_lines"].append(line)
        if label == "hx_on_error_hit":
            report["onerror_hit_lines"].append(line)
        if label in HIT_LABELS:
            fields = {}
            for field, keys in FIELD_KEYS.items():
                found = [(key, value[key]) for key in keys if key in value]
                fields[field] = found[0][1] if found else "UNKNOWN"
            report["hits"].append(dict(line=line, label=label, thread=value.get("thread"),
                                       controller=value.get("controller"), event=value.get("event_index"),
                                       raw_hx_result=value.get("raw_hx_result", "UNKNOWN"), fields=fields))
        if isinstance(value, list) and value[:3] == ["E", "Logging", "writeln"]:
            message = json.loads(value[-1])
            if any(token in message for token in ("GetDownloadID", "HXMMFCtrlImpl::OnError", "changed from")):
                report["matching_trace_lines"].append(line)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve() in {path.resolve() for path in args.logs}:
        parser.error("output cannot overwrite source evidence")
    reports = [audit(path) for path in args.logs]
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump({"logs": reports, "limit": "no missing object field is inferred from registers or API status"},
                  stream, indent=2)
        stream.write("\n")
    for report in reports:
        prepares = [hit for hit in report["hits"] if hit["event"] == 2]
        print(json.dumps(dict(name=report["name"], prepare=prepares,
                              onerror_hits=len(report["onerror_hit_lines"]),
                              raw_memory_replies=report["raw_memory_reply_lines"],
                              matching_trace_lines=report["matching_trace_lines"])))


if __name__ == "__main__":
    main()
