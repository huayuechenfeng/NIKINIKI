#!/usr/bin/env python3
"""Inspect the E7 HxMmfCtrl E32 image and a Petran-expanded code dump.

The input DLL is never modified.  Petran is used separately because the SDK
tool understands Symbian BytePair compression; this script turns its textual
``-dump c`` output into exact little-endian code bytes and records only
reproducible offsets/xrefs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


CODE_LINE = re.compile(r"^([0-9A-Fa-f]{6}):\s+((?:[0-9A-Fa-f]{8}\s+)+)")

# These points were identified in the E7 copy by ARM disassembly and vtable/
# caller data flow.  Addresses in reports are always code-relative; a future
# runtime address must use that run's CodeAddress + code_offset.
KNOWN_POINTS = {
    "controller_constructor": (0xE64, 32),
    "controller_destructor": (0x1108, 32),
    "base_controller_constructor": (0xA4E4, 32),
    "get_download_id": (0x3204, 32),
    "mvpc_prepare": (0x13CC, 32),
    "on_error": (0x3A98, 32),
    "on_error_adjusted_interface": (0x3E10, 32),
    "on_error_selection": (0x3B64, 32),
    "on_error_adjusted_selection": (0x3EDC, 32),
    "on_error_aggregate_commit": (0x3BD0, 32),
    "on_error_adjusted_aggregate_commit": (0x3F48, 32),
    "on_prepare_complete": (0x4054, 32),
    "hx_result_mapper": (0xA7A4, 32),
    "send_event": (0xE5D8, 32),
    "media_platform_init_caller": (0xD238, 32),
    "init_resources": (0xD48C, 32),
    "media_platform_loader": (0xEE00, 32),
    "init_ihx_player": (0x14080, 32),
}

HX_TO_SYMBIAN_TABLE_OFFSET = 0x3A528
HX_TO_SYMBIAN_TABLE_ENTRIES = 117
HX_PARTIAL_PLAYBACK = 0x00040024
SYMBIAN_PARTIAL_PLAYBACK = -12017


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def parse_header(data: bytes) -> dict[str, int | str]:
    if len(data) < 0x80:
        raise ValueError("file is too short for an E32Image header")
    if data[0x10:0x14] != b"EPOC":
        raise ValueError("missing EPOC signature at +0x10")
    fields = {
        "uid1": u32(data, 0x00),
        "uid2": u32(data, 0x04),
        "uid3": u32(data, 0x08),
        "module_version": u32(data, 0x18),
        "compression_type": u32(data, 0x1C),
        "flags": u32(data, 0x2C),
        "code_size": u32(data, 0x30),
        "data_size": u32(data, 0x34),
        "entry_point": u32(data, 0x48),
        "code_base": u32(data, 0x4C),
        "data_base": u32(data, 0x50),
        "dll_ref_count": u32(data, 0x54),
        "export_dir_offset": u32(data, 0x58),
        "export_count": u32(data, 0x5C),
        "text_size": u32(data, 0x60),
        "code_offset": u32(data, 0x64),
        "data_offset": u32(data, 0x68),
        "import_offset": u32(data, 0x6C),
        "code_reloc_offset": u32(data, 0x70),
        "data_reloc_offset": u32(data, 0x74),
        "uncompressed_size_after_header": u32(data, 0x7C),
    }
    fields["sha256"] = hashlib.sha256(data).hexdigest().upper()
    fields["file_size"] = len(data)
    return fields


def parse_petran_code(text: str, expected_size: int) -> bytes:
    chunks: dict[int, bytes] = {}
    for line in text.splitlines():
        match = CODE_LINE.match(line)
        if not match:
            continue
        offset = int(match.group(1), 16)
        words = match.group(2).split()
        chunks[offset] = b"".join(struct.pack("<I", int(word, 16)) for word in words)
    if not chunks:
        raise ValueError("no Petran code rows found")
    code = bytearray(expected_size)
    covered = bytearray(expected_size)
    for offset, chunk in chunks.items():
        if offset >= expected_size:
            continue
        chunk = chunk[: expected_size - offset]
        code[offset : offset + len(chunk)] = chunk
        covered[offset : offset + len(chunk)] = b"\x01" * len(chunk)
    try:
        first_gap = covered.index(0)
    except ValueError:
        first_gap = -1
    if first_gap >= 0:
        raise ValueError(f"Petran code dump is incomplete at code +0x{first_gap:X}")
    return bytes(code)


def all_offsets(data: bytes, needle: bytes) -> list[int]:
    found: list[int] = []
    start = 0
    while True:
        offset = data.find(needle, start)
        if offset < 0:
            return found
        found.append(offset)
        start = offset + 1


def aligned_pointer_xrefs(code: bytes, target: int) -> list[int]:
    needle = struct.pack("<I", target)
    return [offset for offset in all_offsets(code, needle) if offset % 4 == 0]


def point_fingerprints(code: bytes, code_base: int) -> dict[str, object]:
    result: dict[str, object] = {}
    for name, (offset, size) in KNOWN_POINTS.items():
        window = code[offset : offset + size]
        if len(window) != size:
            raise ValueError(f"{name} window exceeds expanded code")
        result[name] = {
            "code_offset": f"0x{offset:X}",
            "linked_address": f"0x{code_base + offset:X}",
            "instruction_set": "ARM",
            "size": size,
            "sha256": hashlib.sha256(window).hexdigest().upper(),
            "bytes_hex": window.hex().upper(),
        }
    return result


def hx_result_table(code: bytes) -> dict[str, object]:
    size = HX_TO_SYMBIAN_TABLE_ENTRIES * 8
    raw = code[HX_TO_SYMBIAN_TABLE_OFFSET : HX_TO_SYMBIAN_TABLE_OFFSET + size]
    if len(raw) != size:
        raise ValueError("HX_RESULT mapping table exceeds expanded code")
    entries = [struct.unpack_from("<Ii", raw, index * 8) for index in range(HX_TO_SYMBIAN_TABLE_ENTRIES)]
    matches = [
        {"index": index, "hx_result": f"0x{hx:08X}", "symbian_error": symbian}
        for index, (hx, symbian) in enumerate(entries)
        if hx == HX_PARTIAL_PLAYBACK or symbian == SYMBIAN_PARTIAL_PLAYBACK
    ]
    return {
        "code_offset": f"0x{HX_TO_SYMBIAN_TABLE_OFFSET:X}",
        "entry_count": HX_TO_SYMBIAN_TABLE_ENTRIES,
        "entry_size": 8,
        "sha256": hashlib.sha256(raw).hexdigest().upper(),
        "partial_playback_matches": matches,
    }


def onerror_dispatch_map(code: bytes, code_base: int) -> dict[str, object]:
    """Validate E7 construction/destruction interface stores and dispatch slot."""
    expected_words = {
        0x10D0: code_base + 0x3D560,  # shared ctor/dtor vtable literal
        0xE68: 0xEB00259D,  # ctor calls base ctor +0xa4e4
        0xE6C: 0xE59F125C,  # ldr r1,[pc,#604] -> +0x10d0
        0xE70: 0xE5801000,
        0xE74: 0xE2811F61,
        0xE78: 0xE5801018,
        0xE7C: 0xE2811020,
        0xE80: 0xE580101C,
        0x1110: 0xE51F0048,  # ldr r0,[pc,#-72] -> literal above
        0x1114: 0xE5840000,  # str r0,[r4]
        0x1118: 0xE2800F61,  # add r0,r0,#0x184
        0x111C: 0xE5840018,  # str r0,[r4,#0x18]
        0x1120: 0xE2800020,  # add r0,r0,#0x20
        0x1124: 0xE584001C,  # str r0,[r4,#0x1c]
        0x3D560 + 0x88: code_base + 0x3A98,
        0x3D704 + 0x1C: code_base + 0x3E10,
        0x3E10: 0xE92D47F0,  # save 32 bytes
        0x3E14: 0xE240401C,  # r4 = interface this - 0x1c
        0x3E18: 0xE1A08001,  # preserve severity
        0x3E1C: 0xE1A07002,  # preserve HXCode
        0x3E20: 0xE59D6020,  # original stack[0] userString
        0x3E24: 0xE59DA024,  # original stack[4] moreInfoURL
        0x3E34: 0xE1A05003,  # preserve userCode
        0x3EDC: 0xE5940090,
        0x3EE8: 0xE5940048,
        0x3EF0: 0x05845094,
        0x3EF8: 0xE5940094,
        0x3F04: 0xE1A05000,
        0x3F48: 0xE5845080,
    }
    for offset, expected in expected_words.items():
        actual = u32(code, offset)
        if actual != expected:
            raise ValueError(f"OnError mapping mismatch +0x{offset:X}: {actual:08X} != {expected:08X}")
    return {
        "verified_words": {f"0x{k:X}": f"0x{v:08X}" for k, v in expected_words.items()},
        "primary_vtable_code_offset": "0x3D560",
        "interface_member_offset": "0x1C",
        "interface_vtable_code_offset": "0x3D704",
        "interface_onerror_slot_offset": "0x1C",
        "interface_onerror_code_offset": "0x3E10",
        "controller": "entry r0 - 0x1C; body r4 after prologue",
        "error_selection": "+0x90, +0x48 and previous +0x94 affect value written to +0x80",
        "limits": "static structure only; same-run vtable/slot reads validate dispatch identity, not execution or breakpoint effectiveness",
    }


def select_error_fields(hx_code: int, field90: int, state48: int, saved94: int,
                        aggregate80: int, severity: int) -> dict:
    """Offline model of selection/commit only, after the existing scheduler call.

    This does not simulate the player or calls/reentrancy. Inputs are synthetic or
    explicitly observed selection-time snapshots, never inferred from Prepare=0.
    """
    for value in (hx_code, field90, state48, saved94, aggregate80, severity):
        if not 0 <= value <= 0xFFFFFFFF:
            raise ValueError("all inputs must be raw unsigned 32-bit words")
    signed90 = field90 if field90 < 0x80000000 else field90 - 0x100000000
    selected = hx_code
    selected_from = "current_hx_code"
    if signed90 > 0:
        if state48 == 7:
            saved94 = hx_code
        elif saved94 != 0:
            selected = saved94
            selected_from = "previous_saved94"
    if selected == 0x000406A2:
        return dict(selected=selected, selected_from=selected_from, saved94=saved94,
                    aggregate80=aggregate80, aggregate_written=False,
                    terminal="special_notification_tailcall; its side effects are outside this model")
    return dict(selected=selected, selected_from=selected_from, saved94=saved94,
                aggregate80=selected, aggregate_written=True,
                terminal="return_after_commit" if severity == 4 else "continue_event_dispatch")


def error_field_lifecycle(code: bytes) -> dict:
    expected_words = {
        # Ctor subobject return-value identity is established by these helpers.
        0xA520: 0xE3A05000, 0xA530: 0xE2840030,
        0xFF20: 0xE1A04000, 0xFF68: 0xE1A00004,
        0xA53C: 0xE2404030, 0xA54C: 0xE3A00007,
        0xA550: 0xE5840048, 0xA554: 0xE2840050,
        0xA55C: 0xEBFFF598, 0x7BC8: 0xEB0068A0,
        0x7BD0: 0xE1A04000, 0x7BDC: 0xE1A00004,
        0x21E50: 0xE59F1DE0, 0x21E68: 0xE12FFF1E,
        0xA57C: 0xE5805030, 0xA58C: 0xE5805040, 0xA590: 0xE5805044,
        0xA594: 0xE2404050,
        # GetDownloadID same-object write and custom-command call.
        0x320C: 0xE1A05000, 0x3260: 0xE59D0008,
        0x3268: 0xE5900000, 0x326C: 0xE5850090, 0x3618: 0xEBFFFEF9,
        # Adjusted body signed gate, overwrite/reuse branches and commit.
        0x3EBC: 0xE1A05007, 0x3EC8: 0xEBFFFEEB, 0x3ED8: 0xEB001A80,
        0x3EDC: 0xE5940090, 0x3EE0: 0xE3500000, 0x3EE4: 0xDA00000F,
        0x3EE8: 0xE5940048, 0x3EEC: 0xE3500007,
        0x3EF0: 0x05845094, 0x3EF4: 0x0A00000B,
        0x3EF8: 0xE5940094, 0x3EFC: 0xE3500000,
        0x3F00: 0x0A000008, 0x3F04: 0xE1A05000,
        0x3D70: 0x000406A2, 0x3F28: 0xE51F01C0,
        0x3F2C: 0xE1550000, 0x3F38: 0x08BD47F0,
        0x3F40: 0x0A0018CC, 0x3F44: 0xE3580004,
        0x3F48: 0xE5845080, 0x3F4C: 0x08BD87F0,
        # Primary body has identical selection instructions (relative addresses differ).
        0x3B64: 0xE5940090, 0x3B6C: 0xDA00000F,
        0x3B78: 0x05845094, 0x3B80: 0xE5940094,
        0x3B8C: 0xE1A05000, 0x3BD0: 0xE5845080,
        # State=7 is also written by StopL; Prepare sets 2 only after its call returns 0.
        0x1370: 0xE3A01007, 0x1374: 0xE5841048,
        0x16CC: 0xE1B05000, 0x16D0: 0x03A00002, 0x16D4: 0x05840048,
        0x4094: 0xE5942080, 0x40A8: 0xE12FFF3C,
        0x40AC: 0xE3A00003, 0x40B0: 0xE5840048,
        0x43A4: 0xE3A00007, 0x43A8: 0xE5840048,
        0xE6FC: 0xE3A00000, 0xE700: 0xE5840080,
        # Old claimed constructor is reached via deleting destructor, then base dtor.
        0x11C8: 0xEBFFFFCE, 0x11D0: 0xEA000DB7, 0x11C0: 0xEA00251B,
    }
    for offset, expected in expected_words.items():
        actual = u32(code, offset)
        if actual != expected:
            raise ValueError(f"error lifecycle mismatch +0x{offset:X}: {actual:08X} != {expected:08X}")
    return {
        "verified_words": {f"0x{k:X}": f"0x{v:08X}" for k, v in expected_words.items()},
        "initialization": {"state48": 7, "aggregate80": 0, "field90": 0, "saved94": 0,
                           "stores": ["+0xA550", "+0xA57C", "+0xA58C", "+0xA590"]},
        "field90_writer": "+0x326C in GetDownloadID; this recovered from entry r0",
        "saved94_writers": ["+0xA590 base ctor alias [controller+0x50+0x44]=0",
                            "+0x3B78 primary OnError conditional store",
                            "+0x3EF0 adjusted OnError conditional store"],
        "clear_limit": "no dedicated post-event saved94 clear recovered; SendEvent explicitly clears +0x80 only; indirect callbacks/bulk writes are not excluded",
        "selection_gate": "int32(field90)>0; state48==7 saves current HXCode, otherwise nonzero saved94 replaces current code",
        "pre_selection_call": "+0xA8E0 ResumeScheduler, except two HXCode cases; possible reentrancy/side effects not dynamically known",
        "scope": "same-phone Hx only; no plugin or driver assumptions; compile-time semantic map not runtime occurrence",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dll", type=Path)
    parser.add_argument("--petran-code-dump", type=Path)
    parser.add_argument("--extract-code", type=Path)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--report", type=Path, help="write a new local JSON report, refusing overwrite")
    args = parser.parse_args()

    image = args.dll.read_bytes()
    header = parse_header(image)
    report: dict[str, object] = {"header": header}

    if args.petran_code_dump:
        code = parse_petran_code(
            args.petran_code_dump.read_text(encoding="ascii", errors="strict"),
            int(header["code_size"]),
        )
        report["expanded_code"] = {
            "size": len(code),
            "sha256": hashlib.sha256(code).hexdigest().upper(),
        }
        report["known_points"] = point_fingerprints(code, int(header["code_base"]))
        report["hx_result_mapping_table"] = hx_result_table(code)
        report["onerror_dispatch_map"] = onerror_dispatch_map(code, int(header["code_base"]))
        report["error_field_lifecycle"] = error_field_lifecycle(code)
        if args.extract_code:
            args.extract_code.write_bytes(code)

        constants = {
            "prepare_event_uid": 0x101F7F86,
            "partial_playback_signed": -12017,
            "partial_playback_u32": (-12017) & 0xFFFFFFFF,
        }
        report["constants"] = {
            **constants,
            "prepare_event_uid_offsets": [
                f"0x{x:X}" for x in all_offsets(code, struct.pack("<I", constants["prepare_event_uid"]))
            ],
            "partial_playback_offsets": [
                f"0x{x:X}" for x in all_offsets(code, struct.pack("<I", constants["partial_playback_u32"]))
            ],
        }

        named_strings = [
            b"HXMMFCtrlImpl::MvpcPrepare()",
            b"HXMMFCtrlImpl::OnPrepareComplete()",
            b"HXMMFCtrlImpl::OnError()",
            b"HXMMFPlayCtrl::InitIHXPlayerL",
            b"hxmedpltfm.dll",
            b"HXMediaPlatformOpen",
            b"HXCreateMediaPlatform",
            b"CreateEngine",
            b"CHXMMFDevSound::Init()",
            b"CHXMMFDevSound::InitializeComplete(): err = %d",
            b"HLX_MDF_VIDEO_SERV",
        ]
        strings: dict[str, object] = {}
        for value in named_strings:
            offsets = all_offsets(code, value)
            entries = []
            for offset in offsets:
                linked = int(header["code_base"]) + offset
                entries.append(
                    {
                        "code_offset": f"0x{offset:X}",
                        "linked_address": f"0x{linked:X}",
                        "absolute_pointer_xrefs": [
                            f"0x{x:X}" for x in aligned_pointer_xrefs(code, linked)
                        ],
                    }
                )
            strings[value.decode("ascii")] = entries
        report["named_strings"] = strings

    if args.report:
        if args.report.resolve() in (args.dll.resolve(), args.petran_code_dump.resolve() if args.petran_code_dump else None):
            raise ValueError("report must not overwrite an input")
        with args.report.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2, sort_keys=True)
            stream.write("\n")
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"file_sha256={header['sha256']}")
        print(f"uid3=0x{int(header['uid3']):08X}")
        print(f"compression=0x{int(header['compression_type']):08X}")
        print(f"code_size=0x{int(header['code_size']):X}")
        print(f"code_base=0x{int(header['code_base']):X}")
        if "expanded_code" in report:
            expanded = report["expanded_code"]
            assert isinstance(expanded, dict)
            print(f"expanded_code_sha256={expanded['sha256']}")
            for name, entries in report["named_strings"].items():  # type: ignore[union-attr]
                print(f"{name}: {entries}")
            print(f"constants={report['constants']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
