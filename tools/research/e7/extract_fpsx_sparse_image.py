#!/usr/bin/env python3
"""Extract the sparse flash payload assembled by Nokia FPSX binary blocks.

Only the block types and fields needed by the E7 CORE evidence are accepted.
The extractor never modifies its input.  Output holes are zero-filled.  Blocks
are applied in file order, matching the reference extractor's sparse writes;
overlap and conflict counts are reported.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Block:
    index: int
    block_type: int
    file_offset: int
    header_size: int
    payload_offset: int
    payload_size: int
    target_offset: int | None


def be16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def parse_blocks(data: bytes) -> list[Block]:
    if len(data) < 5:
        raise ValueError("FPSX is shorter than its primary header")
    header_size = be32(data, 1)
    cursor = 5 + header_size
    if cursor > len(data):
        raise ValueError("FPSX primary header exceeds file")
    blocks: list[Block] = []
    index = 0
    while cursor < len(data):
        start = cursor
        if cursor + 4 > len(data):
            raise ValueError("truncated FPSX block preamble")
        _container_type, _padding, block_type, block_header_size = data[cursor : cursor + 4]
        cursor += 4
        header_end = cursor + block_header_size
        if header_end > len(data):
            raise ValueError(f"block {index} header exceeds file")
        header = data[cursor:header_end]
        cursor = header_end

        payload_size = 0
        target_offset: int | None = None
        if block_type == 0x17:
            if len(header) < 14:
                raise ValueError(f"binary block {index} header is too short")
            payload_size = be32(header, 6)
            target_offset = be32(header, 10)
        elif block_type in (0x27, 0x28):
            if len(header) < 45:
                raise ValueError(f"hash/certificate block {index} header is too short")
            payload_size = be32(header, 37)
            raw_target = be32(header, 41)
            target_offset = None if raw_target == 0xFFFFFFFF else raw_target
        elif block_type == 0x2E:
            if len(header) < 25:
                raise ValueError(f"user-area block {index} header is too short")
            payload_size = be32(header, 17)
            target_offset = be32(header, 21)
        elif block_type == 0x3A:
            payload_size = 0
            target_offset = None
        else:
            raise ValueError(f"unsupported FPSX block type 0x{block_type:02X} at 0x{start:X}")

        # Nokia's block framing contains one byte between the typed header and
        # payload.  It is outside both the header-size and payload-size fields.
        if cursor >= len(data):
            raise ValueError(f"block {index} is missing its framing byte")
        cursor += 1
        payload_offset = cursor
        if cursor + payload_size > len(data):
            raise ValueError(f"block {index} payload exceeds file")
        cursor += payload_size
        blocks.append(
            Block(
                index=index,
                block_type=block_type,
                file_offset=start,
                header_size=block_header_size,
                payload_offset=payload_offset,
                payload_size=payload_size,
                target_offset=target_offset,
            )
        )
        index += 1
    return blocks


def extract(path: Path) -> tuple[bytes, dict[str, object]]:
    data = path.read_bytes()
    blocks = parse_blocks(data)
    image_blocks = [
        block for block in blocks
        if block.target_offset is not None and block.payload_size
    ]
    if not image_blocks:
        raise ValueError("FPSX contains no sparse image blocks")
    image_size = max(
        int(block.target_offset) + block.payload_size for block in image_blocks
    )
    image = bytearray(image_size)
    written = bytearray(image_size)
    records: list[dict[str, object]] = []
    overlap_bytes = 0
    conflicting_overlap_bytes = 0
    for block in image_blocks:
        assert block.target_offset is not None
        start = block.target_offset
        end = start + block.payload_size
        payload = data[block.payload_offset : block.payload_offset + block.payload_size]
        for offset, value in enumerate(payload, start):
            if written[offset]:
                overlap_bytes += 1
                if image[offset] != value:
                    conflicting_overlap_bytes += 1
            image[offset] = value
            written[offset] = 1
        records.append(
            {
                "index": block.index,
                "type": block.block_type,
                "fpsx_file_offset": block.file_offset,
                "payload_file_range": [
                    block.payload_offset,
                    block.payload_offset + block.payload_size,
                ],
                "sparse_image_range": [start, end],
                "payload_sha256": hashlib.sha256(payload).hexdigest().upper(),
            }
        )
    image_bytes = bytes(image)
    magic = image_bytes.find(b"EPOCARM5ROM")
    if magic < 0:
        raise ValueError("assembled sparse image has no EPOCARM5ROM marker")
    report: dict[str, object] = {
        "source": str(path),
        "source_bytes": len(data),
        "source_sha256": hashlib.sha256(data).hexdigest().upper(),
        "block_count": len(blocks),
        "image_block_count": len(image_blocks),
        "sparse_image_bytes": len(image_bytes),
        "sparse_image_sha256": hashlib.sha256(image_bytes).hexdigest().upper(),
        "epocarm5rom_offset": magic,
        "overlap_bytes": overlap_bytes,
        "conflicting_overlap_bytes": conflicting_overlap_bytes,
        "overlap_semantics": "later FPSX block wins, in container order",
        "blocks": records,
    }
    return image_bytes, report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fpsx", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    image, report = extract(args.fpsx)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
