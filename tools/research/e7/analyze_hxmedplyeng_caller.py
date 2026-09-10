#!/usr/bin/env python3
"""Verify the bounded hxmedplyeng candidate for the E7 StateCtrl caller.

The phone-copied E32 file is never modified.  Petran expansion is supplied as
a separate text input.  Runtime addresses are used only to calculate and test
a candidate code offset; they are not treated as module-identity evidence.
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


RUNTIME_CALL_ADDRESS = 0x7CA3C2AC
RUNTIME_RETURN_ADDRESS = 0x7CA3C2B0
NEXT_OBSERVED_MODULE_BASE = 0x7CAC0000
RUNTIME_WINDOW = bytes.fromhex("0410A0E13CFF2FE1")
REQUIRED_BASE_ALIGNMENT = 0x10000
PLAYER_ON_ERROR_OFFSET = 0xEDAC
PARTIAL_GENERATOR_FUNCTION_OFFSET = 0x11EBC
PARTIAL_GENERATOR_CALL_OFFSET = 0x11F54
PARTIAL_LITERAL_OFFSET = 0x1206C
TARGET_HX_RESULT = 0x00040024


def u32(code: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(code):
        raise ValueError(f"word at +0x{offset:X} is outside code")
    return struct.unpack_from("<I", code, offset)[0]


def arm_words(code: bytes, start: int, end: int) -> list[dict[str, object]]:
    return [
        {"offset": f"0x{offset:X}", "word": f"0x{u32(code, offset):08X}"}
        for offset in range(start, end, 4)
    ]


def preceding_arm_prologues(code: bytes, call_offset: int, limit: int = 0x400) -> list[int]:
    """Return ARM STMDB sp!, {...,lr} candidates nearest first."""
    start = max(0, call_offset - limit) & ~3
    result = []
    for offset in range(start, call_offset, 4):
        word = u32(code, offset)
        if word & 0xFFFF0000 == 0xE92D0000 and word & (1 << 14):
            result.append(offset)
    return list(reversed(result))


def arm_branch_target(offset: int, word: int) -> int:
    """Decode an ARM B/BL immediate using the code-relative PC."""
    if word & 0x0E000000 != 0x0A000000:
        raise ValueError(f"word at +0x{offset:X} is not ARM B/BL")
    displacement = (word & 0x00FFFFFF) << 2
    if displacement & 0x02000000:
        displacement -= 0x04000000
    return offset + 8 + displacement


def analyze(image: bytes, petran_text: str) -> tuple[dict[str, object], bytes]:
    header = parse_header(image)
    code = parse_petran_code(petran_text, int(header["code_size"]))
    occurrences = all_offsets(code, RUNTIME_WINDOW)
    base_candidates = []
    for window_offset in occurrences:
        call_offset = window_offset + 4
        base = RUNTIME_CALL_ADDRESS - call_offset
        code_end = base + len(code)
        base_candidates.append(
            {
                "window_offset": f"0x{window_offset:X}",
                "call_offset": f"0x{call_offset:X}",
                "derived_base": f"0x{base:08X}",
                "base_64k_aligned": base % REQUIRED_BASE_ALIGNMENT == 0,
                "derived_code_end": f"0x{code_end:08X}",
                "ends_before_next_observed_module": code_end <= NEXT_OBSERVED_MODULE_BASE,
            }
        )
    plausible = [
        item
        for item in base_candidates
        if item["base_64k_aligned"] and item["ends_before_next_observed_module"]
    ]
    if len(plausible) != 1:
        raise ValueError(
            f"runtime window did not produce one bounded aligned base: {plausible}"
        )
    selected = plausible[0]
    selected_base = int(str(selected["derived_base"]), 16)
    call_offset = int(str(selected["call_offset"]), 16)
    return_offset = call_offset + 4
    window_offset = call_offset - 4
    actual_window = code[window_offset : window_offset + len(RUNTIME_WINDOW)]
    generator_call_word = u32(code, PARTIAL_GENERATOR_CALL_OFFSET)
    generator_target = arm_branch_target(PARTIAL_GENERATOR_CALL_OFFSET, generator_call_word)
    partial_literal_offsets = all_offsets(code, struct.pack("<I", TARGET_HX_RESULT))
    prologues = preceding_arm_prologues(code, call_offset)
    nearest = prologues[0] if prologues else None
    local_start = max(0, call_offset - 0x40) & ~3
    local_end = min(len(code), call_offset + 0x44) & ~3
    result: dict[str, object] = {
        "input": {
            "file_bytes": len(image),
            "file_sha256": hashlib.sha256(image).hexdigest().upper(),
            "representation": "E32Image V2 BytePair input; Petran-expanded code analyzed separately",
        },
        "header": header,
        "expanded_code": {
            "bytes": len(code),
            "sha256": hashlib.sha256(code).hexdigest().upper(),
        },
        "runtime_candidate": {
            "observed_call_address": f"0x{RUNTIME_CALL_ADDRESS:08X}",
            "observed_return_address": f"0x{RUNTIME_RETURN_ADDRESS:08X}",
            "candidate_base": f"0x{selected_base:08X}",
            "candidate_call_offset": f"0x{call_offset:X}",
            "candidate_return_offset": f"0x{return_offset:X}",
            "candidate_code_end": f"0x{selected_base + len(code):08X}",
            "next_observed_module_base": f"0x{NEXT_OBSERVED_MODULE_BASE:08X}",
            "gap_before_next_module": NEXT_OBSERVED_MODULE_BASE
            - (selected_base + len(code)),
            "range_covers_call": call_offset + 4 <= len(code),
            "base_candidates_from_every_byte_match": base_candidates,
            "plausible_aligned_candidate_count": len(plausible),
            "identity_limit": (
                "candidate base is derived from runtime bytes, 64 KiB alignment, and the "
                "next observed module; it is not yet a same-run load event"
            ),
        },
        "runtime_window_match": {
            "offset": f"0x{window_offset:X}",
            "expected_hex": RUNTIME_WINDOW.hex().upper(),
            "actual_hex": actual_window.hex().upper(),
            "matched": actual_window == RUNTIME_WINDOW,
            "occurrence_offsets": [f"0x{offset:X}" for offset in occurrences],
            "unique_in_module": len(occurrences) == 1,
            "unique_bounded_aligned_base": len(plausible) == 1,
            "call_word": f"0x{u32(code, call_offset):08X}",
            "call_decode": "BLX r12" if u32(code, call_offset) == 0xE12FFF3C else "UNKNOWN",
        },
        "bounded_function_search": {
            "nearest_arm_push_lr": f"0x{nearest:X}" if nearest is not None else None,
            "candidate_prologues_nearest_first": [f"0x{x:X}" for x in prologues],
            "limit": "prologue scan is a boundary candidate, not a symbol name",
        },
        "partial_playback_literal_generator": {
            "function_offset": f"0x{PARTIAL_GENERATOR_FUNCTION_OFFSET:X}",
            "call_offset": f"0x{PARTIAL_GENERATOR_CALL_OFFSET:X}",
            "call_return_offset": f"0x{PARTIAL_GENERATOR_CALL_OFFSET + 4:X}",
            "call_word": f"0x{generator_call_word:08X}",
            "call_target": f"0x{generator_target:X}",
            "calls_player_on_error": generator_target == PLAYER_ON_ERROR_OFFSET,
            "literal": f"0x{TARGET_HX_RESULT:08X}",
            "literal_offset": f"0x{PARTIAL_LITERAL_OFFSET:X}",
            "literal_occurrence_offsets": [f"0x{x:X}" for x in partial_literal_offsets],
            "literal_unique": partial_literal_offsets == [PARTIAL_LITERAL_OFFSET],
            "evidence_words": arm_words(code, 0x11F04, 0x11F5C),
            "bounded_semantics": (
                "for each selected entry, +0x2D0C4 supplies a result; a first nonzero "
                "result is retained in r5 and skips this report, otherwise a non-null "
                "entry+0xAC reaches severity=4, literal HXCode=0x00040024, userCode=0, "
                "two null string pointers, then calls player OnError +0xEDAC"
            ),
            "limit": (
                "entry type, +0x2D0C4 operation, and +0xAC object role remain unnamed; "
                "runtime caller LR must select this site before treating it as the observed source"
            ),
        },
        "local_arm_words": arm_words(code, local_start, local_end),
        "conclusion": (
            "STATIC_CANDIDATE_MATCH"
            if actual_window == RUNTIME_WINDOW and len(plausible) == 1
            else "CANDIDATE_NOT_UNIQUELY_MATCHED"
        ),
    }
    return result, code


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path)
    parser.add_argument("--petran-dump", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--extract-code", type=Path)
    args = parser.parse_args()

    result, code = analyze(
        args.dll.read_bytes(),
        args.petran_dump.read_text(encoding="ascii", errors="strict"),
    )
    if args.report is not None:
        if args.report.exists():
            raise FileExistsError(f"refusing to overwrite {args.report}")
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    if args.extract_code is not None:
        if args.extract_code.exists():
            raise FileExistsError(f"refusing to overwrite {args.extract_code}")
        args.extract_code.parent.mkdir(parents=True, exist_ok=True)
        args.extract_code.write_bytes(code)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
