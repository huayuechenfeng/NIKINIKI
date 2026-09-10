#!/usr/bin/env python3
"""Verify the E7 HXMMFStateCtrl OnError forwarding boundary.

This intentionally checks only the instruction and vtable words needed to
interpret the caller observed at HxMmfCtrl code+0x684C.  It is not a general
HxMmfCtrl disassembler and it never writes to the input image.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


EXPECTED_CODE_SHA256 = (
    "F2E371650460097CEC108CEACA10B9187B8578D2360BBFF6523688671CB864DE"
)

# ARM words from the same-phone expanded HxMmfCtrl code.  These cover the
# adjusted-interface entry, the state implementation dispatch, the observer
# dispatch whose return address was observed dynamically, and its epilogue.
EXPECTED_WORDS = {
    0x6798: 0xE92D47FC,  # push {r2-r10,lr}
    0x679C: 0xE2404014,  # r4 = interface_this - 0x14
    0x67A0: 0xE1A06002,  # preserve HXCode
    0x67A4: 0xE1A07001,  # preserve severity
    0x67A8: 0xE1A08003,  # preserve userCode
    0x6800: 0xE5940054,  # state implementation at state+0x54
    0x6810: 0xE591C058,  # implementation virtual slot +0x58
    0x6818: 0xE12FFF3C,  # call implementation
    0x681C: 0xE1A05000,  # preserve implementation return
    0x6820: 0xE5940094,  # observer interface at state+0x94
    0x6828: 0x13550000,  # require nonzero implementation return
    0x683C: 0xE591C01C,  # observer virtual slot +0x1c
    0x6840: 0xE1A02006,  # forward original HXCode
    0x6844: 0xE1A01007,  # forward original severity
    0x6848: 0xE12FFF3C,  # call observer OnError
    0x684C: 0xE1A00005,  # observed LR returns here
    0x6850: 0xE8BD87FC,  # return implementation result
}

# GCC-style vtable groups: offset-to-top, common typeinfo, then function slots.
EXPECTED_VTABLE_WORDS = {
    0x3D89C: 0x00000000,
    0x3D8A0: 0x00043DC0,
    0x3D8FC: 0x0000E6E0,  # primary vptr + 0x58
    0x3D99C: 0xFFFFFFEC,  # adjusted subobject is +0x14 from object top
    0x3D9A0: 0x00043DC0,
    0x3D9B0: 0x0000E798,  # adjusted vptr + 0x0c
}


def word_at(code: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(code):
        raise ValueError(f"word 0x{offset:X} outside code length 0x{len(code):X}")
    return struct.unpack_from("<I", code, offset)[0]


def analyze(code: bytes, *, require_identity: bool = True) -> dict:
    digest = hashlib.sha256(code).hexdigest().upper()
    if require_identity and digest != EXPECTED_CODE_SHA256:
        raise ValueError(
            f"code SHA-256 mismatch: expected {EXPECTED_CODE_SHA256}, got {digest}"
        )

    mismatches = []
    for offset, expected in {**EXPECTED_WORDS, **EXPECTED_VTABLE_WORDS}.items():
        actual = word_at(code, offset)
        if actual != expected:
            mismatches.append(
                {
                    "offset": f"0x{offset:X}",
                    "expected": f"0x{expected:08X}",
                    "actual": f"0x{actual:08X}",
                }
            )
    if mismatches:
        raise ValueError(f"instruction/vtable mismatch: {mismatches}")

    return {
        "input_sha256": digest,
        "input_size": len(code),
        "verified": True,
        "instruction_set": "ARM",
        "adjusted_entry": {
            "code_offset": "0x6798",
            "interface_adjustment": "state_controller = r0 - 0x14",
            "parameters": {
                "r1": "severity",
                "r2": "original HXCode",
                "r3": "userCode",
                "stack_0": "userString",
                "stack_4": "moreInfoURL",
            },
            "vtable": {
                "offset_to_top_word": "code+0x3D99C = -0x14",
                "typeinfo_word": "code+0x3D9A0 = linked 0x43DC0",
                "vptr": "code+0x3D9A4",
                "slot": "+0x0C -> linked 0xE798 -> code+0x6798",
            },
        },
        "primary_equivalent": {
            "code_offset": "0x66E0",
            "vptr": "code+0x3D8A4",
            "slot": "+0x58 -> linked 0xE6E0 -> code+0x66E0",
            "same_typeinfo": True,
        },
        "forwarding": {
            "state_implementation": "[state+0x54], virtual slot +0x58",
            "observer_interface": "[state+0x94], virtual slot +0x1C",
            "observer_call_offset": "0x6848",
            "observer_return_offset": "0x684C",
            "hx_code_forwarded_from": "preserved original r2 via r6",
            "conversion_before_observer_call": False,
        },
        "dynamic_interpretation": {
            "observed_return_offset": "0x684C",
            "proves_adjusted_body_executed": True,
            "next_observation": "break at code+0x6798 and record LR plus state=r0-0x14",
        },
        "limits": [
            "the upstream caller and the instruction that created HXCode 0x00040024 remain unknown",
            "state+0x94 here is an observer pointer, not HXMMFCtrlImpl's saved-result field at controller+0x94",
            "linked pointers and code offsets are not future runtime addresses",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("code", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = analyze(args.code.read_bytes())
    text = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    if args.output:
        if args.output.exists():
            raise FileExistsError(f"refusing to overwrite {args.output}")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
