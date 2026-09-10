#!/usr/bin/env python3
"""Validate a phone-copied Symbian ROM module without modifying it.

The input is treated as a TRomImageHeader followed by whatever mapped code
bytes the file service returned.  In particular, the tool does not assume
that the copy contains the complete declared code extent or the DLL reference
table.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


ROM_IMAGE_HEADER_SIZE = 0x78
KDYNAMIC_LIBRARY_UID = 0x10000079


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def mapped_address(header: dict[str, int], file_offset: int) -> int:
    if file_offset < ROM_IMAGE_HEADER_SIZE:
        raise ValueError("ROM header bytes are not part of the mapped code")
    return header["code_address"] + file_offset - ROM_IMAGE_HEADER_SIZE


def parse_header(data: bytes) -> dict[str, int]:
    if len(data) < ROM_IMAGE_HEADER_SIZE:
        raise ValueError("file is shorter than the 0x78-byte TRomImageHeader")
    names = (
        "uid1",
        "uid2",
        "uid3",
        "uid_checksum",
        "entry_point",
        "code_address",
        "data_address",
        "code_size",
        "text_size",
        "data_size",
        "bss_size",
        "heap_min",
        "heap_max",
        "stack_size",
        "dll_ref_table",
        "export_count",
        "export_dir",
    )
    result = dict(zip(names, struct.unpack_from("<17I", data, 0)))
    result.update(
        {
            "secure_id": u32(data, 0x44),
            "flags": u32(data, 0x58),
            "priority": u32(data, 0x5C),
            "data_bss_linear_base": u32(data, 0x60),
            "next_extension": u32(data, 0x64),
            "hardware_variant": u32(data, 0x68),
            "total_data_size": u32(data, 0x6C),
            "module_version": u32(data, 0x70),
            "exception_descriptor": u32(data, 0x74),
        }
    )
    return result


def analyze(path: Path, related: Path | None = None) -> dict[str, object]:
    data = path.read_bytes()
    header = parse_header(data)
    if header["uid1"] != KDYNAMIC_LIBRARY_UID:
        raise ValueError(
            f"UID1 0x{header['uid1']:08X} is not a Symbian dynamic library"
        )
    code_size = header["code_size"]
    if code_size <= 0 or header["code_address"] + code_size > 0x100000000:
        raise ValueError("declared code range is empty or overflows 32-bit space")

    available = len(data) - ROM_IMAGE_HEADER_SIZE
    export_offset = (
        header["export_dir"] - header["code_address"] + ROM_IMAGE_HEADER_SIZE
    )
    available_exports = 0
    if 0 <= export_offset < len(data):
        available_exports = min(
            header["export_count"], (len(data) - export_offset) // 4
        )

    result: dict[str, object] = {
        "path": str(path),
        "file_bytes": len(data),
        "file_sha256": sha256(data),
        "representation": "TRomImageHeader plus mapped ROM code bytes",
        "rom_image_header_bytes": ROM_IMAGE_HEADER_SIZE,
        "header": header,
        "abi": {
            "eabi": bool(header["flags"] & 0x8),
            "eka2": bool(header["flags"] & 0x20),
        },
        "declared_code_range": [
            header["code_address"],
            header["code_address"] + code_size,
        ],
        "available_mapped_code_range": [
            header["code_address"],
            header["code_address"] + available,
        ],
        "available_mapped_code_bytes": available,
        "missing_declared_code_tail_bytes": max(0, code_size - available),
        "complete_declared_code_extent": available >= code_size,
        "export_directory_file_offset": export_offset,
        "available_export_entries": available_exports,
        "missing_export_entries": max(
            0, header["export_count"] - available_exports
        ),
    }

    if related is not None:
        related_data = related.read_bytes()
        related_header = parse_header(related_data)
        related_start = related_header["code_address"]
        related_end = related_start + related_header["code_size"]
        pointers = []
        for offset in range(ROM_IMAGE_HEADER_SIZE, len(data) - 3):
            value = u32(data, offset)
            if related_start <= (value & ~1) < related_end:
                pointers.append(
                    {
                        "source_file_offset": offset,
                        "source_mapped_address": mapped_address(header, offset),
                        "target": value,
                    }
                )
        result["related_module_reference_scan"] = {
            "related_path": str(related),
            "related_uid3": related_header["uid3"],
            "related_declared_code_range": [related_start, related_end],
            "pointer_hits_at_any_byte_alignment": pointers,
            "limitation": (
                "No hit does not exclude ordinal imports, dynamic loading, or "
                "references in the unavailable file tail."
            ),
        }
    return result


def verify_e7_prepare_handler(data: bytes, header: dict[str, int]) -> dict[str, object]:
    """Verify the bounded E7 handler windows used by the observation plan."""
    body_start = 0x74D8
    prepare_start = 0x752E
    thunk_start = 0x76B2
    body_end = thunk_start
    expected = {
        "body_prefix": (body_start, body_start + 64,
                        "FB7F6BAB83A761C63AF60E5DFCA76BBAFBBF07DD8D8EA9D8151D452DBC6E0CDA"),
        "body": (body_start, body_end,
                 "711FD616ECD6384247AF85AB2AE6444F1FDD3CF103BE6B513737571D0232B2C3"),
        "prepare_window": (prepare_start, 0x7546,
                           "713F0E328713C20F26225E0ADE8E646C9D119A69CDB902FAE563F5FCDADA8A59"),
        "interface_thunk": (thunk_start, thunk_start + 4,
                            "9EFB250D43DBAE2B0049A6B345F999A1CE6F0D25223EB3717FFF5463F7D7C821"),
    }
    windows = {}
    for name, (start, end, expected_hash) in expected.items():
        actual = sha256(data[start:end])
        windows[name] = {
            "file_range": [start, end],
            "code_relative_range": [
                start - ROM_IMAGE_HEADER_SIZE,
                end - ROM_IMAGE_HEADER_SIZE,
            ],
            "mapped_range": [
                mapped_address(header, start),
                mapped_address(header, end),
            ],
            "sha256": actual,
            "matches_expected": actual == expected_hash,
        }
    return {
        "all_windows_match": all(
            bool(item["matches_expected"]) for item in windows.values()
        ),
        "windows": windows,
        "prepare_event_uid_file_offset": 0x99F8,
        "prepare_event_uid": u32(data, 0x99F8),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    parser.add_argument("--related-module", type=Path)
    parser.add_argument(
        "--verify-e7-prepare-handler",
        action="store_true",
        help="verify only the documented E7 MediaClientVideo handler windows",
    )
    args = parser.parse_args()
    result = analyze(args.module, args.related_module)
    if args.verify_e7_prepare_handler:
        data = args.module.read_bytes()
        result["e7_prepare_handler"] = verify_e7_prepare_handler(
            data, result["header"]
        )
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
