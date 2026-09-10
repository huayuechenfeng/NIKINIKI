#!/usr/bin/env python3
"""Verify the bounded E7 mdfvidrender candidate and its failing vslot.

The input DLL is never modified.  Petran expansion is supplied separately.
PID 36450 runtime addresses are used only to test one candidate relocation;
future runs must obtain their own module base.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_hxmmfctrl import all_offsets, parse_header, parse_petran_code


RUNTIME_VTABLE = 0x7C9AA9A4
RUNTIME_FIRST_METHOD = 0x7C9812E8
RUNTIME_SECOND_METHOD = 0x7C981C18
EXPECTED_VTABLE_PREFIX = (
    0x00008D28,
    0x00008D84,
    0x00008E00,
    0x00009208,
    0x000092E8,
    0x000093D0,
    0x00009C18,
    0x00009F78,
    0x0000A398,
)
FIRST_SLOT = 0x10
SECOND_SLOT = 0x18

# The second vslot is an adjustor thunk to +0x16F0.  These words pin the
# bounded return-value data flow without relying on symbols from another ROM.
SECOND_BODY_OFFSET = 0x16F0
SECOND_BODY_WORDS = {
    0x1724: 0xE51F9B04,  # ldr r9, [pc, #-0xb04] -> literal +0xc28
    0x1728: 0xE3560000,  # cmp r6, #0
    0x1730: 0x01A05009,  # moveq r5, r9
    0x1770: 0xE12FFF33,  # blx r3; target came from arg1 vtable +0x30
    0x1774: 0xE2505000,  # subs r5, r0, #0
    0x1778: 0xBA000045,  # blt +0x1894 (cleanup)
    0x192C: 0xEB001E25,  # bl +0x91c8
    0x1930: 0xE1A05000,  # mov r5, r0
    0x1968: 0xE3550000,  # cmp r5, #0
    0x196C: 0xBA000092,  # blt +0x1bbc (cleanup)
    0x1C0C: 0xE1A00005,  # mov r0, r5
    0x1C14: 0xE8BD83F0,  # pop {..., pc}
}
INIT_HELPER_OFFSET = 0x91C8
INIT_HELPER_WORDS = {
    0x91C8: 0xE92D4FFF,
    0x9384: 0xEBFFF45D,  # BL +0x6500
    0x9388: 0xE1A05000,  # r5 := raw return
    0x94BC: 0xE35A0000,
    0x94C4: 0xE1A0000A,
    0x94C8: 0xEB002558,  # BL +0x12A30; early mapped return
    0x94CC: 0xE28DD024,  # common epilogue
    0x94E8: 0xEBFFF268,  # BL +0x5E90
    0x94EC: 0xE1A0A000,
    0x956C: 0xE59F5298,  # local 0x80004001, not E_FAIL
    0x9574: 0xE3550000,
    0x9640: 0xEBFFFE9A,  # BL +0x90B0
    0x9644: 0xE2505000,
    0x9660: 0xE12FFF33,  # object +0x60, vslot +0x0c
    0x9664: 0xE2505000,
    0x9680: 0xEBFFFDA2,  # BL +0x8D10
    0x9684: 0xE2505000,
    0x96A0: 0xEBFFFA79,  # BL +0x808C
    0x96A4: 0xE2505000,
    0x96CC: 0xEBFFF79E,  # BL +0x754C
    0x96D0: 0xE1A05000,
    0x96FC: 0xE51F59F8,  # local 0x8007000e, not E_FAIL
    0x9760: 0xE1A05000,
    0x9764: 0xE51F02F0,  # common final error-report join
    0x9788: 0xE1A00005,
    0x978C: 0xEAFFFF4E,
}
INIT_INNER_OFFSET = 0x754C
INIT_INNER_WORDS = {
    0x754C: 0xE92D4FF0,  # function entry, ARM
    0x79A8: 0xE3A08001,  # r8 is overwritten; it is not an entry-input witness at exit
    0x7D1C: 0xEB00223A,  # BL +0x1060C
    0x7D20: 0xE3500000,  # test its return
    0x7DC4: 0xE12FFF33,  # interface call
    0x7DC8: 0xE2506000,  # r6 := raw return
    0x7DE0: 0xEB002B12,  # late BL +0x12A30 mapping
    0x7E04: 0xE5951000,  # work-object status selector
    0x7E0C: 0x15956040,  # r6 := [r5+0x40]
    0x7E14: 0xEB002B05,  # BL +0x12A30 mapping
    0x7E18: 0xE1A06000,  # r6 := mapped r0
    0x7E1C: 0xE1A00005,  # common result/cleanup point
    0x7E4C: 0xE1A00006,  # return r6
}
EXCEPTION_MAP_OFFSET = 0x12A30
EXCEPTION_MAP_WORDS = {
    0x12A30: 0xE59F103C,
    0x12A34: 0xE1A02000,  # r2 := raw r0
    0x12A38: 0xE59F0038,  # default E_FAIL
    0x12A4C: 0xE5914000,  # table key
    0x12A50: 0xE1540002,
    0x12A5C: 0xE5910004,  # mapped value
    0x12A78: 0x80004005,
}
EXCEPTION_MAP_TABLE_OFFSET = 0x28C30
CREATE_INIT_OFFSET = 0x1060C
CREATE_INIT_WORDS = {
    0x1060C: 0xE92D43F3,  # save r0/r1 and callee-saved registers
    0x10610: 0xE24DD01C,  # 0x1c local frame
    0x10614: 0xE1A04001,  # r4 := work input
    0x10618: 0xE1A08000,  # r8 := client input
    0x10658: 0xE58D6018,  # initialize captured TRAP result to zero
    0x10668: 0xEBFFFFBB,  # protected BL +0x1055c
    0x10680: 0xE5850000,  # exception handler stores leave code to [sp+0x18]
    0x10694: 0xE59D0018,  # load captured TRAP result
    0x10698: 0xE3500000,  # first comparison of captured result
    0x1069C: 0x1A000008,  # nonzero bypasses later create/init call
    0x106AC: 0xE58D4004,  # parameter block receives work input
    0x106B0: 0xE58D6014,  # parameter block tail zero
    0x106B4: 0xEBFFFF76,  # direct BL +0x10494
    0x106B8: 0xE58D0018,  # store direct return after observation point
    0x106BC: 0xE3500000,  # test selected result
    0x106E0: 0xE59D0018,  # return selected raw Symbian error
    0x106E8: 0xE8BD83F0,
    0x10494: 0xE2800004,  # wrapper adjusts client to embedded session handle
    0x1049C: 0xEBFFC043,  # direct BL to import trampoline +0x5b0
    0x104A0: 0xE8BD8010,  # wrapper preserves imported r0 result
    0x5B0: 0xE51FF004,   # E32 import trampoline
    0x5B4: 0x0000060B,   # ordinal literal; provider/symbol not recovered here
}
EXPECTED_EXCEPTION_MAP = (
    (0x00000000, 0x00000000),
    (0xFFFFFFFE, 0x80004005),
    (0xFFFFFFFA, 0x80070057),
    (0xFFFFFFFC, 0x8007000E),
    (0xFFFFFFFB, 0x8004000E),
    (0xFFFFFFFF, 0x80040089),
    (0xFFFFFFDE, 0x80040044),
    (0xFFFFFFEE, 0x80040007),
    (0xFFFFFFDF, 0x8004004C),
    (0xFFFFFFDC, 0x8004004D),
    (0xFFFFFFF7, 0x8004000D),
    (0xFFFFFFFD, 0x80040088),
    (0xFFFFFFD3, 0x80040FCA),
    (0xFFFFFFEC, 0x80040091),
    (0xFFFFFFD2, 0x800400CE),
    (0xFFFFFFD1, 0x8004000E),
)


def u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError(f"word at +0x{offset:X} is outside code")
    return struct.unpack_from("<I", data, offset)[0]


def arm_branch_target(offset: int, word: int) -> int:
    if word & 0x0E000000 != 0x0A000000:
        raise ValueError(f"word at +0x{offset:X} is not ARM B/BL")
    displacement = (word & 0x00FFFFFF) << 2
    if displacement & 0x02000000:
        displacement -= 0x04000000
    return offset + 8 + displacement


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def analyze(image: bytes, petran_text: str) -> tuple[dict[str, object], bytes]:
    header = parse_header(image)
    code = parse_petran_code(petran_text, int(header["code_size"]))
    table = b"".join(struct.pack("<I", value) for value in EXPECTED_VTABLE_PREFIX)
    table_offsets = [offset for offset in all_offsets(code, table) if offset % 4 == 0]
    if len(table_offsets) != 1:
        raise ValueError(f"expected one aligned renderer vtable, got {table_offsets}")
    vtable_offset = table_offsets[0]

    code_base = int(header["code_base"])
    first_linked = u32(code, vtable_offset + FIRST_SLOT)
    second_linked = u32(code, vtable_offset + SECOND_SLOT)
    first_offset = first_linked - code_base
    second_offset = second_linked - code_base
    if min(first_offset, second_offset) < 0:
        raise ValueError("vtable method pointer precedes linked code base")

    bases = {
        "vtable": RUNTIME_VTABLE - vtable_offset,
        "first_method": RUNTIME_FIRST_METHOD - first_offset,
        "second_method": RUNTIME_SECOND_METHOD - second_offset,
    }
    if len(set(bases.values())) != 1:
        raise ValueError(f"runtime observations imply inconsistent bases: {bases}")
    candidate_base = next(iter(bases.values()))
    if candidate_base & 0xFFFF:
        raise ValueError(f"derived runtime base is not 64 KiB aligned: 0x{candidate_base:X}")
    if candidate_base + len(code) <= RUNTIME_VTABLE:
        raise ValueError("runtime vtable is outside the derived code range")

    thunks: dict[str, dict[str, object]] = {}
    for name, offset in (("first", first_offset), ("second", second_offset)):
        adjust = u32(code, offset)
        branch = u32(code, offset + 4)
        if adjust != 0xE2400004:
            raise ValueError(f"{name} method is not the expected this-4 adjustor thunk")
        body = arm_branch_target(offset + 4, branch)
        thunks[name] = {
            "code_offset": f"0x{offset:X}",
            "runtime_address_pid36450": f"0x{candidate_base + offset:08X}",
            "adjust_word": f"0x{adjust:08X}",
            "branch_word": f"0x{branch:08X}",
            "body_offset": f"0x{body:X}",
            "body_runtime_address_pid36450": f"0x{candidate_base + body:08X}",
            "thunk_16_sha256": sha256(code[offset : offset + 16]),
            "body_32_sha256": sha256(code[body : body + 32]),
        }

    if int(thunks["second"]["body_offset"], 16) != SECOND_BODY_OFFSET:
        raise ValueError("second vslot does not branch to the verified +0x16F0 body")
    for offset, expected in SECOND_BODY_WORDS.items():
        actual = u32(code, offset)
        if actual != expected:
            raise ValueError(
                f"second body word mismatch at +0x{offset:X}: "
                f"0x{actual:08X} != 0x{expected:08X}"
            )
    e_fail_literal = u32(code, 0xC28)
    if e_fail_literal != 0x80004005:
        raise ValueError(f"unexpected +0xC28 literal: 0x{e_fail_literal:08X}")
    for offset, expected in INIT_HELPER_WORDS.items():
        actual = u32(code, offset)
        if actual != expected:
            raise ValueError(
                f"init helper word mismatch at +0x{offset:X}: "
                f"0x{actual:08X} != 0x{expected:08X}"
            )
    for group_name, words in (
        ("inner init", INIT_INNER_WORDS),
        ("exception map", EXCEPTION_MAP_WORDS),
        ("create and init", CREATE_INIT_WORDS),
    ):
        for offset, expected in words.items():
            actual = u32(code, offset)
            if actual != expected:
                raise ValueError(
                    f"{group_name} word mismatch at +0x{offset:X}: "
                    f"0x{actual:08X} != 0x{expected:08X}"
                )
    exception_map = tuple(
        (u32(code, EXCEPTION_MAP_TABLE_OFFSET + index * 8),
         u32(code, EXCEPTION_MAP_TABLE_OFFSET + index * 8 + 4))
        for index in range(len(EXPECTED_EXCEPTION_MAP))
    )
    if exception_map != EXPECTED_EXCEPTION_MAP:
        raise ValueError("exception-to-HX_RESULT table does not match")
    if u32(code, 0x980C) != 0x80004001 or u32(code, 0x8D0C) != 0x8007000E:
        raise ValueError("init helper local error literals do not match")

    e_fail = struct.pack("<I", 0x80004005)
    e_fail_offsets = [offset for offset in all_offsets(code, e_fail) if offset % 4 == 0]
    result: dict[str, object] = {
        "input": {
            "bytes": len(image),
            "sha256": sha256(image),
            "representation": "E32Image V2 BytePair input; Petran-expanded code analyzed separately",
        },
        "header": header,
        "expanded_code": {"bytes": len(code), "sha256": sha256(code)},
        "pid36450_identity": {
            "candidate_base": f"0x{candidate_base:08X}",
            "code_end": f"0x{candidate_base + len(code):08X}",
            "vtable_offset": f"0x{vtable_offset:X}",
            "runtime_vtable": f"0x{RUNTIME_VTABLE:08X}",
            "runtime_first_method": f"0x{RUNTIME_FIRST_METHOD:08X}",
            "runtime_second_method": f"0x{RUNTIME_SECOND_METHOD:08X}",
            "derived_bases": {key: f"0x{value:08X}" for key, value in bases.items()},
            "vtable_prefix_words": [f"0x{value:08X}" for value in EXPECTED_VTABLE_PREFIX],
            "vtable_prefix_36_sha256": sha256(table),
            "evidence": (
                "one unique aligned vtable in this DLL maps the observed vtable and both "
                "observed method pointers to the same 64 KiB runtime base"
            ),
            "limit": (
                "PID 36450 did not record this module's load event or runtime code bytes; "
                "a future run still needs a same-run load range and code fingerprint"
            ),
        },
        "thunks": thunks,
        "second_method_data_flow": {
            "body_offset": "0x16F0",
            "local_e_fail_literal": {
                "literal_offset": "0xC28",
                "value": "0x80004005",
                "guard_offsets": ["0x1728", "0x1730"],
                "condition": "arg1 (r6) is null",
                "pid36450_excluded": True,
                "exclusion_evidence": (
                    "the already recorded outer-call reconstruction supplied "
                    "arg1=0x2291B258, which is non-null"
                ),
            },
            "remaining_negative_return_gates_for_pid36450": [
                {
                    "call_offset": "0x1770",
                    "return_observation_offset": "0x1774",
                    "call": "BLX r3 loaded from arg1 vtable slot +0x30",
                    "mapping": "r5 := raw r0; negative r5 takes cleanup and is returned",
                },
                {
                    "call_offset": "0x192C",
                    "callee_offset": "0x91C8",
                    "return_observation_offset": "0x1930",
                    "call": "direct BL to same-module helper +0x91C8",
                    "mapping": (
                        "r5 := raw r0; a later compare at +0x1968 sends negative "
                        "r5 to cleanup without replacing it"
                    ),
                },
            ],
            "return_offsets": ["0x1C0C", "0x1C14"],
            "conclusion": (
                "for PID 36450's non-null arg1, the observed 0x80004005 was not "
                "created by the local null-argument branch; it is the raw negative "
                "return from one of the two listed calls"
            ),
            "runtime_limit": (
                "which gate actually returned 0x80004005 is not present in PID "
                "36450's saved registers and remains a dynamic question"
            ),
        },
        "init_helper_data_flow": {
            "entry_offset": "0x91C8",
            "embedded_name_evidence": "nearby trace strings identify Init and 'Error in Init'",
            "symbol_limit": "no matching symbol/map; CMdfVideoAdapter::Init is a semantic label, not a recovered export",
            "local_negative_literals": [
                {"offset": "0x980C", "value": "0x80004001"},
                {"offset": "0x8D0C", "value": "0x8007000E"},
            ],
            "local_e_fail_literal": False,
            "e_fail_conclusion": (
                "the observed 0x80004005 is not either local literal; it is a "
                "nested call return or an exception-to-HX_RESULT mapping"
            ),
            "negative_result_candidates": [
                {"call": "+0x6500", "capture_or_lr": "+0x9388 / LR +0x9394 after cleanup"},
                {"call": "+0x12A30 early exception mapping", "capture_or_lr": "+0x94C8 / epilogue +0x94CC"},
                {"call": "+0x90B0", "capture_or_lr": "+0x9644"},
                {"call": "[this+0x60] vslot +0x0C", "capture_or_lr": "+0x9664"},
                {"call": "+0x8D10", "capture_or_lr": "+0x9684"},
                {"call": "+0x808C", "capture_or_lr": "+0x96A4"},
                {"call": "+0x754C", "capture_or_lr": "+0x96DC after cleanup"},
                {"call": "+0x12A30 late exception mapping", "capture_or_lr": "+0x9760"},
            ],
            "common_failure_join": {
                "offset": "0x9764",
                "reads": "r5 raw selected failure and LR provenance; r4 is the adapter object",
                "fingerprint_16_sha256": sha256(code[0x9760:0x9770]),
                "exception": (
                    "the early +0x94C8 mapping returns through +0x94CC without "
                    "visiting +0x9764; retain the already verified outer return to distinguish it"
                ),
            },
        },
        "inner_init_754c_data_flow": {
            "entry_offset": "0x754C",
            "runtime_observation": (
                "PID36684 directly observed E_FAIL in r0 at caller +0x96D0; "
                "PID36727 then hit +0x7E1C with r6=E_FAIL and LR=+0x7E18"
            ),
            "actual_selected_path": (
                "+0x7E14 called +0x12A30 and +0x7E18 copied its returned E_FAIL to r6"
            ),
            "not_the_source": (
                "+0x7DFC is a later shared service/report call on other paths; a cleanup/report "
                "return address must not be named as the generator"
            ),
            "entry_input_limit": (
                "r8 is overwritten with 1 at +0x79A8, so PID36727's r8=1 is not the entry r1"
            ),
            "remaining_source_boundary": (
                "the raw r0 entering +0x12A30 is not yet observed; it is either an explicit "
                "table key (notably -2 maps to E_FAIL) or an unlisted value using default E_FAIL"
            ),
            "unique_next_point": {
                "offset": "0x12A30",
                "caller_lr": "mdf base +0x7E18",
                "reads": "raw r0 before the first mapper instruction; r4/r5 and renderer frame only for association",
            },
        },
        "create_and_init_1060c_data_flow": {
            "entry_offset": "0x1060C",
            "inputs": "r0 client is preserved in r8; r1 work is preserved in r4",
            "trap_path": {
                "protected_call": "+0x10668 BL +0x1055C",
                "exception_store": "+0x10680 stores the caught leave value to [sp+0x18]",
                "observation_offset": "0x10698",
                "observation_semantics": (
                    "r0 has just been loaded from [sp+0x18] and has not yet been compared; "
                    "a nonzero value is bounded to a leave inside protected +0x1055C, not TRAP itself"
                ),
            },
            "later_call_path": {
                "guard": "+0x1069C skips this path when the captured TRAP result is nonzero",
                "call": "+0x106B4 BL +0x10494",
                "observation_offset": "0x106B8",
                "observation_semantics": "r0 is the direct return and has not yet been stored or compared",
                "arguments": "r0=r8 client, r1=18, r2=sp+4; [sp+4]=work and [sp+0x14]=0",
                "wrapper": (
                    "+0x10494 adds 4 to the client pointer, calls import trampoline +0x5B0, "
                    "and returns its r0 unchanged"
                ),
                "import_limit": (
                    "the trampoline literal is 0x60B; this analysis does not infer a provider DLL "
                    "or public symbol without the matching import mapping"
                ),
            },
            "return": "+0x106E0 reloads [sp+0x18] and returns it unchanged",
            "function_224_sha256": sha256(code[0x1060C:0x106EC]),
            "wrapper_16_sha256": sha256(code[0x10494:0x104A4]),
            "unique_dynamic_points": ["0x10698", "0x106B8"],
        },
        "exception_to_hx_result_map": {
            "entry_offset": "0x12A30",
            "table_offset": "0x28C30",
            "entries": [
                {"raw": f"0x{raw:08X}", "mapped": f"0x{mapped:08X}"}
                for raw, mapped in exception_map
            ],
            "default": "0x80004005",
            "e_fail_ambiguity": (
                "raw 0xFFFFFFFE (-2/KErrGeneral) has an explicit E_FAIL entry; any raw value "
                "absent from the 16-entry table also returns default E_FAIL"
            ),
            "instruction_fingerprint_68_sha256": sha256(code[0x12A30:0x12A74]),
            "runtime_fingerprint_limit": (
                "+0x12A74 is a relocatable table pointer and is excluded from the runtime instruction fingerprint"
            ),
            "table_128_sha256": sha256(code[EXCEPTION_MAP_TABLE_OFFSET:EXCEPTION_MAP_TABLE_OFFSET + 128]),
        },
        "aligned_0x80004005_literals": [f"0x{offset:X}" for offset in e_fail_offsets],
    }
    return result, code


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path)
    parser.add_argument("--petran-code-dump", type=Path, required=True)
    parser.add_argument("--extract-code", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    result, code = analyze(
        args.dll.read_bytes(),
        args.petran_code_dump.read_text(encoding="ascii", errors="strict"),
    )
    if args.extract_code is not None:
        if args.extract_code.exists():
            raise FileExistsError(f"refusing to overwrite {args.extract_code}")
        args.extract_code.write_bytes(code)
    text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.report is not None:
        if args.report.exists():
            raise FileExistsError(f"refusing to overwrite {args.report}")
        args.report.write_text(text, encoding="utf-8")
    print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
