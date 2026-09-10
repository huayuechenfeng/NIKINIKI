#!/usr/bin/env python3
"""Verify the bounded E7 Qt MMF mapping and print runtime addresses.

This tool is deliberately host-only.  It has no CODA transport, process launch,
breakpoint, install, or device-write capability.  A runtime CodeAddress may be
supplied only to perform arithmetic after the candidate fingerprints match.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


LINK_CODE_BASE = 0x8000
CANDIDATE_SHA256 = "44EE2383965365B40D35CEE9EB0FB8D5C9A7A18F0343533A3CE953C87B8D86B4"
CODE_SHA256 = "9FAEC742872CD5F8377D28827500A5C6D4159933F10E840434AA1653FF6F7792"

FINGERPRINTS = (
    {
        "name": "applyPendingChanges",
        "link_address": 0xE868,
        "length": 64,
        "sha256": "C316DF978DC6E221E7F04EF78D21047D8ADF4DA2A31F604D972AF085A05A2134",
    },
    {
        "name": "doPlay",
        "link_address": 0xF1A0,
        "length": 32,
        "sha256": "1CE9B30E1C30118D22A4CC59618DA62D56CE48760A155CB3D0B2D89111FFFF5D",
    },
    {
        "name": "MvpuoPrepareComplete",
        "link_address": 0xF4D2,
        "length": 64,
        "sha256": "2C2447CC82E9C0EB514ED8416717BA6503168D63263DB61A8CA761F83C25B76B",
    },
)

OBSERVATION_POINTS = (
    {
        "name": "prepare_entry",
        "link_address": 0xF4D2,
        "meaning": "r0=full session this; r1=TInt aError before the prologue copies them",
    },
    {
        "name": "apply_pending_entry",
        "link_address": 0xE868,
        "meaning": "r0=session; r1=force; require the same session and force==1",
    },
    {
        "name": "after_apply_pending",
        "link_address": 0xF6CC,
        "meaning": "applyPendingChanges(true) returned; r4=session; [sp+0x38]=pre-apply leave code",
    },
    {
        "name": "mmf_play_entry",
        "link_address": 0xF1A0,
        "meaning": "r0=session; [r0+0x78]=CVideoPlayerUtility2 immediately before Play",
    },
)

SESSION_MEMBERS = {
    "base_error": 0x38,
    "ws_session": 0x6C,
    "screen_device": 0x70,
    "player_utility": 0x78,
    "dummy_window_object": 0x8C,
    "video_output_control": 0x90,
    "video_output_display": 0x94,
    "display_window": 0x98,
    "pending_changes": 0xAC,
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def parse_address(value: str) -> int:
    return int(value, 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--candidate",
        type=Path,
        required=True,
        help="path to the privately supplied Qt MMF engine DLL",
    )
    parser.add_argument(
        "--code",
        type=Path,
        required=True,
        help="path to its privately supplied decompressed code image",
    )
    parser.add_argument(
        "--code-address",
        type=parse_address,
        help="CODA Shared Library CodeAddress; arithmetic only, no device connection",
    )
    args = parser.parse_args()

    candidate = args.candidate.read_bytes()
    code = args.code.read_bytes()
    failures: list[str] = []

    candidate_digest = sha256(candidate)
    code_digest = sha256(code)
    if candidate_digest != CANDIDATE_SHA256:
        failures.append("candidate SHA-256 mismatch")
    if code_digest != CODE_SHA256:
        failures.append("decompressed code SHA-256 mismatch")

    checked_fingerprints = []
    for item in FINGERPRINTS:
        offset = item["link_address"] - LINK_CODE_BASE
        actual = sha256(code[offset : offset + item["length"]])
        matched = actual == item["sha256"]
        if not matched:
            failures.append(f"{item['name']} fingerprint mismatch")
        checked_fingerprints.append(
            {
                **item,
                "code_offset": offset,
                "actual_sha256": actual,
                "matched": matched,
            }
        )

    points = []
    for item in OBSERVATION_POINTS:
        point = {
            **item,
            "code_offset": item["link_address"] - LINK_CODE_BASE,
            "thumb_symbol": item["link_address"] | 1,
        }
        if args.code_address is not None:
            point["runtime_instruction_address"] = (
                args.code_address + point["code_offset"]
            )
            point["runtime_instruction_address_hex"] = hex(
                point["runtime_instruction_address"]
            )
        points.append(point)

    result = {
        "status": "MATCH" if not failures else "MISMATCH",
        "scope": "host-only; supplied runtime base is not an identity proof",
        "candidate": {
            "path": str(args.candidate),
            "bytes": len(candidate),
            "sha256": candidate_digest,
        },
        "code": {
            "path": str(args.code),
            "bytes": len(code),
            "link_code_base": LINK_CODE_BASE,
            "sha256": code_digest,
        },
        "fingerprints": checked_fingerprints,
        "observation_points": points,
        "session_members": SESSION_MEMBERS,
        "failures": failures,
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
