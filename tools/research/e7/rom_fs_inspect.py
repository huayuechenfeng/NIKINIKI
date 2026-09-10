#!/usr/bin/env python3
"""Inspect a named ROM-FS entry in a Nokia CORE FPSX image.

This is a host-only, read-only helper.  It deliberately does not try to decode
pageable ROM data: a directory entry is reported as metadata unless its address
is below the image's pageable boundary and maps directly into the CORE image.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def align4(value: int) -> int:
    return (value + 3) & ~3


def parse(core: Path, needle: str) -> dict[str, object]:
    data = core.read_bytes()
    magic = data.find(b"EPOCARM5ROM")
    if magic < 0:
        raise ValueError("EPOCARM5ROM marker not found")

    header = magic + 0x100
    get = lambda offset: u32(data, header + offset)
    base = get(0x8C)
    rom_size = get(0x90)
    root = get(0x94)
    pageable_start = get(0x148)
    pageable_size = get(0x14C)
    compressed_unpaged_start = get(0x160)
    unpaged_compressed_size = get(0x164)
    unpaged_uncompressed_size = get(0x168)

    candidates: list[tuple[int, int, int]] = []
    for bias in range(-0x40, 0x81):
        root_list = header + (root - base) + bias
        if not 0 <= root_list <= len(data) - 12:
            continue
        count = u32(data, root_list)
        root_va = u32(data, root_list + 8)
        if not (1 <= count <= 8 and base <= root_va < base + rom_size):
            continue
        directory = header + (root_va - base) + bias
        if not 0 <= directory <= len(data) - 8:
            continue
        directory_size = u32(data, directory)
        sort_info = directory + 4 + align4(directory_size)
        if not (0 < directory_size < 0x1000000 and sort_info <= len(data) - 4):
            continue
        children = u16(data, sort_info) + u16(data, sort_info + 2)
        if children < 20000:
            candidates.append((bias, root_va, directory))

    if not candidates:
        raise ValueError("ROM-FS root mapping not validated")
    bias, root_va, _ = candidates[0]

    records: list[dict[str, object]] = []
    seen: set[int] = set()

    def parse_dir(virtual_address: int, prefix: str) -> None:
        physical = header + (virtual_address - base) + bias
        if physical in seen or not 0 <= physical <= len(data) - 8:
            return
        seen.add(physical)
        directory_size = u32(data, physical)
        sort_info = physical + 4 + align4(directory_size)
        if not (0 < directory_size and physical + 4 + directory_size <= len(data)):
            return
        if sort_info > len(data) - 4:
            return
        subdirs = u16(data, sort_info)
        files = u16(data, sort_info + 2)
        total = subdirs + files
        if total > 20000 or sort_info + 4 + 2 * total > len(data):
            return
        for index in range(total):
            relative = u16(data, sort_info + 4 + 2 * index) * 4
            entry = physical + 4 + relative
            if entry > len(data) - 10:
                continue
            size = u32(data, entry)
            address = u32(data, entry + 4)
            attributes = data[entry + 8]
            name_length = data[entry + 9]
            raw_name = data[entry + 10 : entry + 10 + 2 * name_length]
            try:
                name = raw_name.decode("utf-16le")
            except UnicodeDecodeError:
                name = "<invalid-utf16>"
            path = (prefix + "/" + name).replace("//", "/")
            if needle.casefold() in path.casefold():
                records.append(
                    {
                        "path": path,
                        "size": size,
                        "rom_entry_address": address,
                        "address_semantics": "TRomEntry.iAddressLin; not a code address",
                        "attributes": attributes,
                        "directory_entry_core_offset": entry,
                        "pageable": (address - base) >= pageable_start,
                    }
                )
            if attributes & 0x10 and base <= address < base + rom_size:
                parse_dir(address, path)

    parse_dir(root_va, "")
    return {
        "core": str(core),
        "core_bytes": len(data),
        "core_sha256": hashlib.sha256(data).hexdigest().upper(),
        "epocarm5rom_offset": magic,
        "rom_header_offset": header,
        "rom_base": base,
        "rom_size": rom_size,
        "rom_fs_bias": bias,
        "pageable_start": pageable_start,
        "pageable_size": pageable_size,
        "compressed_unpaged_start": compressed_unpaged_start,
        "unpaged_compressed_size": unpaged_compressed_size,
        "unpaged_uncompressed_size": unpaged_uncompressed_size,
        "query": needle,
        "matches": records,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("core", type=Path)
    parser.add_argument("needle")
    parser.add_argument(
        "--rom-image-out",
        type=Path,
        help=(
            "optionally emit the embedded ROM image beginning at "
            "EPOCARM5ROM, for readimage verification"
        ),
    )
    args = parser.parse_args()
    result = parse(args.core, args.needle)
    if args.rom_image_out is not None:
        data = args.core.read_bytes()
        start = int(result["epocarm5rom_offset"])
        args.rom_image_out.parent.mkdir(parents=True, exist_ok=True)
        args.rom_image_out.write_bytes(data[start:])
        result["emitted_rom_image"] = {
            "path": str(args.rom_image_out),
            "source_offset": start,
            "bytes": len(data) - start,
            "sha256": hashlib.sha256(data[start:]).hexdigest().upper(),
        }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
