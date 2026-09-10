#!/usr/bin/env python3
"""Validate the phone-copied E7 DevVideo ROM image and its proxy callback.

The input remains read-only.  ``--code-out`` writes only the mapped bytes that
are actually present after the 0x78-byte TRomImageHeader; a missing declared
tail is reported and is never synthesized.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import struct
from pathlib import Path


HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location(
    "analyze_rom_module_copy", HERE / "analyze_rom_module_copy.py"
)
ROM_MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(ROM_MODULE)
ROM_IMAGE_HEADER_SIZE = ROM_MODULE.ROM_IMAGE_HEADER_SIZE
parse_header = ROM_MODULE.parse_header


EXPECTED_SHA256 = "6F0CBB47A4F136609F23AAFFA4BDB03E0B5F5D2ABACF64114950E84C62FB0BC2"
EXPECTED_UIDS = (0x10000079, 0x1000008D, 0x101F9ED6)
EXPECTED_CODE_ADDRESS = 0x80602998
EXPECTED_CODE_SIZE = 0x7A00
LIVE_PRIMARY_VPTR = 0x806091D0
LIVE_PROXY_VPTR = 0x80609214
INITIALIZE_COMPLETE_SLOT = 10
CALLBACK_THUNK_ADDRESS = 0x80605AB8
CALLBACK_BODY_ADDRESS = 0x80605A70
DECODER_HANDLER_ADDRESS = 0x8060578A
POSTPROC_HANDLER_ADDRESS = 0x8060573E
EXPECTED_WINDOWS = {
    "callback_thunk": (CALLBACK_THUNK_ADDRESS, 0x04,
        "4EFB2B0C77645DBAB4DE091DC7F63B47436B5969E7C8340A094F73C1F2628A2A"),
    "callback_body": (CALLBACK_BODY_ADDRESS, 0x48,
        "2ADD2EBF5CB417A55F3F38E111E5A2A80D95945C392BCD6CC489DA1EC4153BA3"),
    "decoder_handler": (DECODER_HANDLER_ADDRESS, 0x50,
        "52923F63488941E260EA9E613373A004571BBC89C854EE39B622A894A2C37B7A"),
    "postprocessor_handler": (POSTPROC_HANDLER_ADDRESS, 0x4C,
        "3FAAF03FBD4E3BEC3E3058DB49780ED01B9C0F4D3624273FBFACFF3C112BA7FF"),
    "primary_and_proxy_vtables": (LIVE_PRIMARY_VPTR, 0x74,
        "A2A743BD4E0F0189834F669C20850398C38B0085260548013C75DA51046A258E"),
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def file_offset(header: dict[str, int], address: int) -> int:
    start = header["code_address"]
    if address < start:
        raise ValueError(f"address 0x{address:08X} precedes mapped code")
    return ROM_IMAGE_HEADER_SIZE + address - start


def read_word(data: bytes, header: dict[str, int], address: int) -> int:
    offset = file_offset(header, address)
    if offset < ROM_IMAGE_HEADER_SIZE or offset + 4 > len(data):
        raise ValueError(f"word at 0x{address:08X} is outside available bytes")
    return u32(data, offset)


def bounded_window(
    data: bytes, header: dict[str, int], address: int, length: int
) -> dict[str, object]:
    offset = file_offset(header, address)
    if offset < ROM_IMAGE_HEADER_SIZE or offset + length > len(data):
        raise ValueError(
            f"window 0x{address:08X}+0x{length:X} is outside available bytes"
        )
    window = data[offset : offset + length]
    return {
        "mapped_range": [address, address + length],
        "file_range": [offset, offset + length],
        "bytes": length,
        "sha256": sha256(window),
        "hex": window.hex().upper(),
    }


def analyze(path: Path, code_out: Path | None = None) -> dict[str, object]:
    data = path.read_bytes()
    header = parse_header(data)
    digest = sha256(data)
    uids = (header["uid1"], header["uid2"], header["uid3"])
    if digest != EXPECTED_SHA256:
        raise ValueError(f"unexpected target copy SHA-256 {digest}")
    if uids != EXPECTED_UIDS:
        raise ValueError("target UID tuple does not match the supplied E7 DevVideo")
    if header["code_address"] != EXPECTED_CODE_ADDRESS:
        raise ValueError("target code address does not match the supplied E7 DevVideo")
    if header["code_size"] != EXPECTED_CODE_SIZE:
        raise ValueError("target code size does not match the supplied E7 DevVideo")

    available = data[ROM_IMAGE_HEADER_SIZE:]
    available_end = header["code_address"] + len(available)
    declared_end = header["code_address"] + header["code_size"]
    if not (header["code_address"] <= LIVE_PRIMARY_VPTR < available_end):
        raise ValueError("PID 70569 primary vptr is outside available mapped bytes")
    if not (header["code_address"] <= LIVE_PROXY_VPTR < available_end):
        raise ValueError("PID 70569 proxy vptr is outside available mapped bytes")

    callback_slot_address = LIVE_PROXY_VPTR + 4 * INITIALIZE_COMPLETE_SLOT
    callback_pointer = read_word(data, header, callback_slot_address)
    callback_code_address = callback_pointer & ~1
    if callback_pointer & 1 == 0:
        raise ValueError("proxy initialize-complete slot is not a Thumb pointer")
    if not (header["code_address"] <= callback_code_address < available_end):
        raise ValueError("proxy initialize-complete target is outside available code")
    if callback_code_address != CALLBACK_THUNK_ADDRESS:
        raise ValueError("proxy initialize-complete target changed")

    windows = {}
    for name, (address, length, expected) in EXPECTED_WINDOWS.items():
        window = bounded_window(data, header, address, length)
        window["expected_sha256"] = expected
        window["matches_expected"] = window["sha256"] == expected
        if not window["matches_expected"]:
            raise ValueError(f"{name} fingerprint changed")
        windows[name] = window
    if windows["callback_thunk"]["hex"] != "001FD9E7":
        raise ValueError("callback thunk is not SUBS r0,#4; B callback body")

    if code_out is not None:
        if code_out.exists():
            raise FileExistsError(f"refusing to overwrite {code_out}")
        code_out.parent.mkdir(parents=True, exist_ok=True)
        code_out.write_bytes(available)

    return {
        "input": str(path),
        "file_bytes": len(data),
        "file_sha256": digest,
        "representation": "TRomImageHeader plus available mapped ROM code bytes",
        "header": header,
        "declared_code_range": [header["code_address"], declared_end],
        "available_code_range": [header["code_address"], available_end],
        "available_code_bytes": len(available),
        "available_code_sha256": sha256(available),
        "missing_declared_tail_bytes": max(0, header["code_size"] - len(available)),
        "live_pid_70569_relation": {
            "primary_vptr": LIVE_PRIMARY_VPTR,
            "proxy_vptr": LIVE_PROXY_VPTR,
            "both_within_available_code": True,
            "limitation": (
                "This links the supplied file to the live object addresses; "
                "it does not prove every ROM byte outside the bounded windows."
            ),
        },
        "proxy_initialize_complete": {
            "declaration_slot": INITIALIZE_COMPLETE_SLOT,
            "slot_address": callback_slot_address,
            "slot_file_offset": file_offset(header, callback_slot_address),
            "pointer": callback_pointer,
            "instruction_set": "Thumb",
            "code_address": callback_code_address,
            "body_address": CALLBACK_BODY_ADDRESS,
            "decoder_handler_address": DECODER_HANDLER_ADDRESS,
            "postprocessor_handler_address": POSTPROC_HANDLER_ADDRESS,
            "vtable_window": bounded_window(data, header, LIVE_PROXY_VPTR, 0x30),
            "code_window": bounded_window(data, header, callback_code_address, 0x80),
            "verified_windows": windows,
            "routing": (
                "thunk subtracts 4 from proxy this and branches to body; body saves "
                "r1=device and r2=error, compares device with primary+0x0C decoder "
                "then primary+0x10 postprocessor, and forwards unchanged error to "
                "the selected handler"
            ),
        },
        "code_output": str(code_out) if code_out is not None else None,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    parser.add_argument("--code-out", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = analyze(args.module, args.code_out)
    text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.report is None:
        print(text, end="")
    else:
        if args.report.exists():
            raise FileExistsError(f"refusing to overwrite {args.report}")
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
