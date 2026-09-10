#!/usr/bin/env python3
"""Reconstruct one bounded pageable ROM-FS entry from an embedded Symbian ROM.

The input must begin at ``EPOCARM5ROM`` (as emitted by ``rom_fs_inspect.py``),
not at the beginning of a Nokia FPSX container.  The page table and declared
ROM geometry are validated before any output is written.  This helper exists
for offline research; a reconstructed firmware candidate is not a substitute
for a file copied from the phone that participated in a device run.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


ROM_HEADER_OFFSET = 0x100
PAGE_SIZE = 0x1000


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def bytepair_decode(source: bytes, output_limit: int = PAGE_SIZE) -> bytes:
    """Decode the Symbian byte-pair page format used by this ROM family."""
    if not source:
        raise ValueError("empty BytePair page")
    pair_count = source[0]
    cursor = 1
    left = list(range(256))
    right = list(range(256))
    marker = -1
    if pair_count:
        if cursor >= len(source):
            raise ValueError("truncated BytePair marker")
        marker = source[cursor]
        cursor += 1
        left[marker] = (~marker) & 0xFF
        if pair_count < 32:
            if cursor + 3 * pair_count > len(source):
                raise ValueError("truncated BytePair dictionary")
            for _ in range(pair_count):
                symbol, first, second = source[cursor : cursor + 3]
                cursor += 3
                left[symbol] = first
                right[symbol] = second
        else:
            if cursor + 32 > len(source):
                raise ValueError("truncated BytePair symbol mask")
            mask = source[cursor : cursor + 32]
            cursor += 32
            for symbol in range(256):
                if mask[symbol >> 3] & (1 << (symbol & 7)):
                    if cursor + 2 > len(source):
                        raise ValueError("truncated BytePair dictionary")
                    left[symbol], right[symbol] = source[cursor : cursor + 2]
                    cursor += 2

    if cursor >= len(source):
        raise ValueError("BytePair page has no token stream")
    output = bytearray()
    stack: list[int] = []
    token = source[cursor]
    cursor += 1
    first = left[token]

    # Faithful structured translation of Symbian's BytePairDecompress labels:
    # next -> not_single -> do_pair -> recurse.  In particular, a full output
    # page returns immediately after the next input token has been fetched.
    state = "next" if first == token else "not_single"
    while True:
        if state == "next":
            if cursor >= len(source):
                output.append(first)
                return bytes(output)
            token = source[cursor]
            cursor += 1
            output.append(first)
            if len(output) >= output_limit:
                return bytes(output)
            first = left[token]
            state = "next" if first == token else "not_single"
            continue

        if state == "not_single":
            if token == marker:
                if cursor >= len(source):
                    raise ValueError("truncated quoted BytePair literal")
                first = source[cursor]
                cursor += 1
                state = "next"
            else:
                state = "do_pair"
            continue

        if state == "do_pair":
            if len(stack) >= 256:
                raise ValueError("BytePair expansion stack overflow")
            second = right[token]
            token = first
            first = left[token]
            stack.append(second)
            state = "recurse"
            continue

        if token != first:
            state = "do_pair"
            continue
        if not stack:
            state = "next"
            continue
        token = stack.pop()
        output.append(first)
        if len(output) >= output_limit:
            raise ValueError("BytePair pair expansion exceeds page size")
        first = left[token]
        state = "recurse"


def reconstruct(rom_path: Path, address: int, size: int) -> tuple[bytes, dict[str, object]]:
    source_data = rom_path.read_bytes()
    rom_offset = source_data.find(b"EPOCARM5ROM")
    if rom_offset < 0:
        raise ValueError("input has no EPOCARM5ROM marker")
    data = source_data[rom_offset:]
    get = lambda offset: u32(data, ROM_HEADER_OFFSET + offset)
    base = get(0x8C)
    rom_size = get(0x90)
    pageable_start = get(0x148)
    pageable_size = get(0x14C)
    page_index = get(0x150)
    if size <= 0 or address < base or address + size > base + rom_size:
        raise ValueError("requested ROM entry is outside declared ROM geometry")
    relative_start = address - base
    relative_end = relative_start + size
    if not (
        pageable_start <= relative_start
        and relative_end <= pageable_start + pageable_size
    ):
        raise ValueError("requested entry is not wholly within pageable ROM")

    first_page = pageable_start // PAGE_SIZE
    start_page = relative_start // PAGE_SIZE
    end_page = (relative_end + PAGE_SIZE - 1) // PAGE_SIZE
    table = ROM_HEADER_OFFSET + page_index
    if table < ROM_HEADER_OFFSET or table + end_page * 8 > len(data):
        raise ValueError("page table does not cover requested entry")

    stream = ROM_HEADER_OFFSET + pageable_start
    for index in range(first_page, start_page):
        stream += u16(data, table + index * 8 + 4)

    pages = bytearray()
    page_records: list[dict[str, object]] = []
    for index in range(start_page, end_page):
        entry_offset = table + index * 8
        data_start, stored_size, compression, attributes = struct.unpack_from(
            "<I H B B", data, entry_offset
        )
        if stored_size <= 0 or stream + stored_size > len(data):
            raise ValueError(f"page 0x{index:X} has invalid stored extent")
        stored = data[stream : stream + stored_size]
        if compression == 0:
            decoded = stored
        elif compression == 1:
            decoded = bytepair_decode(stored)
        else:
            raise ValueError(
                f"page 0x{index:X} uses unsupported compression {compression}"
            )
        if len(decoded) != PAGE_SIZE:
            raise ValueError(
                f"page 0x{index:X} decoded to {len(decoded)}, expected 4096"
            )
        pages.extend(decoded)
        page_records.append(
            {
                "page_index": index,
                "virtual_range": [base + index * PAGE_SIZE,
                                  base + (index + 1) * PAGE_SIZE],
                "page_table_data_start": data_start,
                "stored_offset": stream,
                "stored_size": stored_size,
                "compression": compression,
                "attributes": attributes,
                "stored_sha256": hashlib.sha256(stored).hexdigest().upper(),
                "decoded_sha256": hashlib.sha256(decoded).hexdigest().upper(),
            }
        )
        stream += stored_size

    slice_start = relative_start - start_page * PAGE_SIZE
    result = bytes(pages[slice_start : slice_start + size])
    if len(result) != size:
        raise ValueError("reconstructed output length does not match request")
    report: dict[str, object] = {
        "source": str(rom_path),
        "source_bytes": len(source_data),
        "source_sha256": hashlib.sha256(source_data).hexdigest().upper(),
        "embedded_rom_offset": rom_offset,
        "embedded_rom_bytes": len(data),
        "rom_base": base,
        "rom_size": rom_size,
        "pageable_range": [base + pageable_start,
                           base + pageable_start + pageable_size],
        "page_index_offset": page_index,
        "requested_range": [address, address + size],
        "output_bytes": len(result),
        "output_sha256": hashlib.sha256(result).hexdigest().upper(),
        "pages": page_records,
        "identity_limit": (
            "Firmware candidate only. Device identity requires comparison "
            "with the corresponding file copied from the participating phone."
        ),
    }
    return result, report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("address", type=lambda value: int(value, 0))
    parser.add_argument("size", type=lambda value: int(value, 0))
    parser.add_argument("output", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result, report = reconstruct(args.rom, args.address, args.size)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
