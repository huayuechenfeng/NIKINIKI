#!/usr/bin/env python3
"""Inspect the same-device Helix media-platform E32 image.

Petran performs BytePair expansion separately.  This tool consumes its text
dump, verifies the DLL's custom name-to-ordinal table, and reports stable
code-relative fingerprints.  It never treats the linked base as a runtime
load address.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

from analyze_hxmmfctrl import all_offsets, parse_header, parse_petran_code


SYMBOL_TABLE_OFFSET = 0x3752C
SYMBOL_TABLE_ENTRIES = 8
EXPORT_LINE = re.compile(r"Ordinal\s+(\d+):\s*([0-9A-Fa-f]{8})")

KNOWN_BODIES = {
    "GetSymbolOrdinal": 0x4A8C,
    "FreeGlobal": 0x4AE4,
    "HXMediaPlatformOpen": 0x43F8,
    "HXCreateMediaPlatform": 0x4418,
    "HXMediaPlatformClose": 0x44F0,
    "CreateEngine": 0x46F8,
    "CloseEngine": 0x48C4,
    "SetDLLAccessPath": 0x43E0,
}


def c_string_at_linked(code: bytes, code_base: int, linked: int) -> str:
    offset = linked - code_base
    if not 0 <= offset < len(code):
        raise ValueError(f"string pointer 0x{linked:X} is outside code")
    end = code.find(b"\0", offset)
    if end < 0:
        raise ValueError(f"unterminated string at code +0x{offset:X}")
    return code[offset:end].decode("ascii", errors="strict")


def parse_exports(text: str) -> dict[int, int]:
    exports = {
        int(match.group(1)): int(match.group(2), 16)
        for match in EXPORT_LINE.finditer(text)
    }
    if not exports:
        raise ValueError("Petran dump contains no export ordinal rows")
    return exports


def symbol_map(code: bytes, code_base: int) -> list[dict[str, object]]:
    result = []
    for index in range(SYMBOL_TABLE_ENTRIES):
        offset = SYMBOL_TABLE_OFFSET + index * 8
        linked, ordinal = struct.unpack_from("<II", code, offset)
        result.append(
            {
                "name": c_string_at_linked(code, code_base, linked),
                "ordinal": ordinal,
                "name_linked_address": f"0x{linked:X}",
                "table_code_offset": f"0x{offset:X}",
            }
        )
    return result


def fingerprint(code: bytes, offset: int, size: int = 32) -> dict[str, object]:
    window = code[offset : offset + size]
    if len(window) != size:
        raise ValueError(f"fingerprint at +0x{offset:X} exceeds code")
    return {
        "code_offset": f"0x{offset:X}",
        "size": size,
        "sha256": hashlib.sha256(window).hexdigest().upper(),
        "bytes_hex": window.hex().upper(),
        "instruction_set": "ARM",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path)
    parser.add_argument("--petran-dump", type=Path, required=True)
    parser.add_argument("--extract-code", type=Path)
    args = parser.parse_args()

    image = args.dll.read_bytes()
    header = parse_header(image)
    petran_text = args.petran_dump.read_text(encoding="ascii", errors="strict")
    code = parse_petran_code(petran_text, int(header["code_size"]))
    if args.extract_code:
        args.extract_code.write_bytes(code)

    exports = parse_exports(petran_text)
    names = symbol_map(code, int(header["code_base"]))
    named_exports = []
    for item in names:
        ordinal = int(item["ordinal"])
        linked = exports.get(ordinal)
        if linked is None:
            raise ValueError(f"symbol table refers to missing ordinal {ordinal}")
        offset = linked - int(header["code_base"])
        named_exports.append(
            {
                **item,
                "export_linked_address": f"0x{linked:X}",
                "export_code_offset": f"0x{offset:X}",
                "export_fingerprint": fingerprint(code, offset),
                "body_fingerprint": fingerprint(code, KNOWN_BODIES[str(item["name"])]),
            }
        )

    constants = {}
    for name, value in {
        "hx_partial_playback": 0x00040024,
        "e_fail": 0x80004005,
        "e_outofmemory": 0x8007000E,
    }.items():
        constants[name] = [
            f"0x{offset:X}"
            for offset in all_offsets(code, struct.pack("<I", value))
        ]

    strings = {}
    for value in (
        b"HLX_MDF_VIDEO_SERV",
        b"DT_Codecs",
        b"DT_RCAPlugins",
        b"DT_Plugins",
        b"IHXMediaPlatform",
        b"IHXMediaPlatformQuery",
        b"IHXMediaPlatformKicker",
    ):
        strings[value.decode("ascii")] = [
            f"0x{offset:X}" for offset in all_offsets(code, value)
        ]

    report = {
        "header": header,
        "expanded_code": {
            "size": len(code),
            "sha256": hashlib.sha256(code).hexdigest().upper(),
        },
        "exports": {
            "count": len(exports),
            "ordinals": {str(key): f"0x{value:X}" for key, value in exports.items()},
            "custom_named_subset": named_exports,
        },
        "constants": constants,
        "strings": strings,
        "limitations": [
            "linked addresses are not runtime addresses",
            "absence of a literal does not exclude a computed or propagated value",
            "the custom table names eight of thirteen E32 exports",
        ],
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
