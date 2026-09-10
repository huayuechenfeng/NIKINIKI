#!/usr/bin/env python3
"""Make a narrow, reproducible MMF string/ROM-FS comparison.

This host-only helper does not decompress, rebuild, or execute firmware.  It
compares only MediaClientVideo ROM-FS metadata and a fixed set of
HxMmfCtrl/Real registration signatures in the supplied CORE/FPSX files.
Offsets are offsets in the original container, never runtime code addresses.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from rom_fs_inspect import parse as parse_rom_fs


PROBES = {
    "media_client_video_ascii": b"MediaClientVideo.dll",
    "media_client_video_utf16le": "MediaClientVideo.dll".encode("utf-16le"),
    "hx_mmf_ctrl_ascii": b"HxMmfCtrl",
    "hx_mmf_ctrl_utf16le": "HxMmfCtrl".encode("utf-16le"),
    "real_video_player_ascii": b"Real Video Player",
    "real_video_player_utf16le": "Real Video Player".encode("utf-16le"),
    "hx_impl_source_ascii": b"hxmmfctrlimpl.cpp",
    "hx_impl_source_utf16le": "hxmmfctrlimpl.cpp".encode("utf-16le"),
    "implementation_uid_0x101f8514_le": struct.pack("<I", 0x101F8514),
    "dll_uid3_0x101f8513_le": struct.pack("<I", 0x101F8513),
}


def offsets(data: bytes, needle: bytes, limit: int = 64) -> tuple[int, list[int]]:
    found: list[int] = []
    start = 0
    count = 0
    while True:
        at = data.find(needle, start)
        if at < 0:
            return count, found
        count += 1
        if len(found) < limit:
            found.append(at)
        start = at + 1


def ecom_clusters(data: bytes) -> list[dict[str, object]]:
    """Report UID occurrences near Hx/Real text without interpreting a record."""
    uid = PROBES["implementation_uid_0x101f8514_le"]
    result: list[dict[str, object]] = []
    _, uid_offsets = offsets(data, uid, 256)
    signatures = {
        key: value
        for key, value in PROBES.items()
        if key.startswith("hx_mmf_ctrl_") or key.startswith("real_video_player_")
    }
    for at in uid_offsets:
        lower = max(0, at - 0x400)
        upper = min(len(data), at + 0x800)
        window = data[lower:upper]
        hits = sorted(key for key, value in signatures.items() if value in window)
        if hits:
            strings: list[dict[str, object]] = []
            run_start = None
            for index, byte in enumerate(window + b"\x00"):
                if 0x20 <= byte <= 0x7E:
                    if run_start is None:
                        run_start = index
                elif run_start is not None:
                    if index - run_start >= 4:
                        strings.append(
                            {
                                "offset": lower + run_start,
                                "text": window[run_start:index].decode("ascii"),
                            }
                        )
                    run_start = None
            result.append(
                {
                    "uid_container_offset": at,
                    "window_start": lower,
                    "window_end": upper,
                    "nearby_signatures": hits,
                    "ascii_strings": strings,
                    "window_sha256": hashlib.sha256(window).hexdigest().upper(),
                }
            )
    return result


def inspect(label: str, path: Path) -> dict[str, object]:
    data = path.read_bytes()
    result: dict[str, object] = {
        "label": label,
        "path": str(path),
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest().upper(),
        "offset_semantics": "original container offsets; never runtime code addresses",
        "signatures": {},
    }
    try:
        result["rom_fs"] = parse_rom_fs(path, "MediaClientVideo.dll")
    except ValueError as error:
        result["rom_fs_error"] = str(error)
    signatures = result["signatures"]
    assert isinstance(signatures, dict)
    for name, needle in PROBES.items():
        count, found = offsets(data, needle)
        signatures[name] = {
            "count": count,
            "first_offsets": found,
            "offsets_truncated": count > len(found),
        }
    result["ecom_uid_text_clusters"] = ecom_clusters(data)
    return result


def label_path(value: str) -> tuple[str, Path]:
    if "=" not in value:
        raise argparse.ArgumentTypeError("input must be LABEL=PATH")
    label, path = value.split("=", 1)
    if not label or not path:
        raise argparse.ArgumentTypeError("input must be LABEL=PATH")
    return label, Path(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", nargs="+", type=label_path, metavar="LABEL=PATH")
    args = parser.parse_args()
    result = {"schema": 1, "images": [inspect(label, path) for label, path in args.input]}
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
