#!/usr/bin/env python3
"""Validate the phone-copied E7 IVE AVC decoder and Initialize error routes.

The input remains read-only.  The module is a TRomImageHeader followed by the
mapped ROM bytes actually returned by the phone; the missing declared tail is
reported and never synthesized.
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

EXPECTED_SHA256 = "77E24AA6DD3382D07F2802159415B9F3B70D96790E8C70F637E75342DF6B3A87"
EXPECTED_UIDS = (0x10000079, 0x10009D8D, 0x10204C1E)
EXPECTED_CODE_ADDRESS = 0x80D25578
EXPECTED_CODE_SIZE = 0xD354
ROM_IMAGE_ADDRESS = EXPECTED_CODE_ADDRESS - ROM_IMAGE_HEADER_SIZE

AVC_IMPLEMENTATION_UID = 0x10204C21
AVC_TYPEINFO = 0x80D31348
AVC_VTABLE_ADDRESS_POINT = 0x80D317B4
INITIALIZE_VTABLE_OFFSET = 0x44
INITIALIZE_ADDRESS = 0x80D26322
RESOURCE_HELPER_ADDRESS = 0x80D2627E
SYNC_OBSERVER_CALL = 0x80D263A0
SYNC_OBSERVER_RETURN_LR = 0x80D263A5
ASYNC_OBSERVER_CALL = 0x80D273BA
ASYNC_OBSERVER_RETURN_LR = 0x80D273BD
# A separate MIveObserver callback does not forward a lower-layer return code:
# when decoder state bit 5 at this+0xBC is clear it constructs
# KErrHardwareNotAvailable (~43 == -44) and reports it to DevVideo.
POLICY_CALLBACK_ADDRESS = 0x80D273D2
POLICY_DENIED_OBSERVER_CALL = 0x80D2744E
POLICY_DENIED_OBSERVER_RETURN_LR = 0x80D27451
POLICY_STATE_FLAGS_OFFSET = 0xBC
POLICY_READY_BIT = 0x20
TRACE_ID_LITERAL_ADDRESS = 0x80D27794
TRACE_COMPONENT_UID_ADDRESS = 0x80D27798
TRACE_ID_LITERAL = 0x008A0086
TRACE_COMPONENT_UID = 0x10204C1E
ACCESS_DENIED_ENTRY_TRACE_ID = 0x88
ACCESS_DENIED_EXIT_TRACE_ID = 0x89

EXPECTED_WINDOWS = {
    "initialize": (INITIALIZE_ADDRESS, 0x120,
        "DC09EDADC00F7B40818169C61052D9D8961FB7F80E578754D7C72086FFD723E3"),
    "resource_helper": (RESOURCE_HELPER_ADDRESS, 0x6C,
        "F3BD92B20FF5B28EBBCDDE9CB78488B39A5ACF10C09E7F28BECE98ADF9073510"),
    "avc_vtable": (AVC_VTABLE_ADDRESS_POINT, 0x100,
        "EBD9852BF5A87FECAEDC48D80210E5F83D9637D56E380E6AB79F69A55B52FD56"),
    "import_stubs": (0x80D255D8, 0x180,
        "B2B4312E760CB110647B0494C15E394160A8292E128DB16A73D90079CBC11CD1"),
    "policy_callback": (POLICY_CALLBACK_ADDRESS, 0x90,
        "B185F2F774BD98B501969ABFFC341884B525560553943CFAF5059F106106FC7A"),
}

# Stubs called around the synchronous Initialize setup.  The first two targets
# match the RM-626 policy-client and the target SDK DSO ABI.  The subsequently
# supplied participating-phone copy is byte-identical, but a same-run module
# event is still required before converting any linked address dynamically.
LOWER_STUBS = {
    "policy_newl": 0x80D25718,
    "policy_request_ive_access_l": 0x80D25710,
    "resource_trap_setup": 0x80D25628,
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def file_offset(address: int) -> int:
    if address < EXPECTED_CODE_ADDRESS:
        raise ValueError(f"address 0x{address:08X} precedes mapped code")
    return ROM_IMAGE_HEADER_SIZE + address - EXPECTED_CODE_ADDRESS


def window(data: bytes, address: int, length: int) -> dict[str, object]:
    offset = file_offset(address)
    if offset + length > len(data):
        raise ValueError(f"window 0x{address:08X}+0x{length:X} is incomplete")
    part = data[offset:offset + length]
    return {
        "mapped_range": [address, address + length],
        "file_range": [offset, offset + length],
        "bytes": length,
        "sha256": sha256(part),
        "hex": part.hex().upper(),
    }


def analyze(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    header = parse_header(data)
    digest = sha256(data)
    if digest != EXPECTED_SHA256:
        raise ValueError(f"unexpected E7 IVE copy SHA-256 {digest}")
    if (header["uid1"], header["uid2"], header["uid3"]) != EXPECTED_UIDS:
        raise ValueError("E7 IVE UID tuple changed")
    if header["code_address"] != EXPECTED_CODE_ADDRESS or header["code_size"] != EXPECTED_CODE_SIZE:
        raise ValueError("E7 IVE code address/size changed")

    available = data[ROM_IMAGE_HEADER_SIZE:]
    available_end = EXPECTED_CODE_ADDRESS + len(available)
    declared_end = EXPECTED_CODE_ADDRESS + EXPECTED_CODE_SIZE
    verified = {}
    for name, (address, length, expected) in EXPECTED_WINDOWS.items():
        item = window(data, address, length)
        item["expected_sha256"] = expected
        item["matches_expected"] = item["sha256"] == expected
        if not item["matches_expected"]:
            raise ValueError(f"{name} fingerprint changed")
        verified[name] = item

    initialize_pointer = u32(data, file_offset(AVC_VTABLE_ADDRESS_POINT + INITIALIZE_VTABLE_OFFSET))
    if initialize_pointer != INITIALIZE_ADDRESS | 1:
        raise ValueError("AVC vtable Initialize slot changed")
    trace_id_literal = u32(data, file_offset(TRACE_ID_LITERAL_ADDRESS))
    trace_component_uid = u32(data, file_offset(TRACE_COMPONENT_UID_ADDRESS))
    if trace_id_literal != TRACE_ID_LITERAL or trace_component_uid != TRACE_COMPONENT_UID:
        raise ValueError("AccessDenied trace identity literals changed")

    stub_targets = {}
    for name, address in LOWER_STUBS.items():
        code = window(data, address, 8)["hex"]
        if code[:8] != "04F01FE5":
            raise ValueError(f"{name} is not an ARM import veneer")
        stub_targets[name] = {
            "stub": address,
            "target": u32(data, file_offset(address + 4)),
        }

    uid_bytes = struct.pack("<I", AVC_IMPLEMENTATION_UID)
    uid_offsets = [i for i in range(len(data)) if data.startswith(uid_bytes, i)]
    return {
        "input": str(path),
        "file_bytes": len(data),
        "file_sha256": digest,
        "representation": "TRomImageHeader plus available mapped ROM code bytes",
        "header": header,
        "declared_code_range": [EXPECTED_CODE_ADDRESS, declared_end],
        "available_code_range": [EXPECTED_CODE_ADDRESS, available_end],
        "available_code_bytes": len(available),
        "missing_declared_tail_bytes": max(0, EXPECTED_CODE_SIZE - len(available)),
        "avc_implementation_uid": AVC_IMPLEMENTATION_UID,
        "avc_uid_file_offsets": uid_offsets,
        "avc_typeinfo": AVC_TYPEINFO,
        "avc_vtable_address_point": AVC_VTABLE_ADDRESS_POINT,
        "initialize": {
            "vtable_offset": INITIALIZE_VTABLE_OFFSET,
            "pointer": initialize_pointer,
            "code_address": INITIALIZE_ADDRESS,
            "sync_observer_call": SYNC_OBSERVER_CALL,
            "sync_observer_return_lr": SYNC_OBSERVER_RETURN_LR,
            "async_observer_call": ASYNC_OBSERVER_CALL,
            "async_observer_return_lr": ASYNC_OBSERVER_RETURN_LR,
            "static_limit": (
                "both call sites forward an error to the DevVideo proxy; runtime LR is required "
                "to select the executed path"
            ),
        },
        "policy_callback_failure": {
            "name_basis": "CIveVideoDecodeHwDevice::AccessDenied",
            "code_address": POLICY_CALLBACK_ADDRESS,
            "state_flags_offset": POLICY_STATE_FLAGS_OFFSET,
            "required_bit": POLICY_READY_BIT,
            "observer_call": POLICY_DENIED_OBSERVER_CALL,
            "observer_return_lr": POLICY_DENIED_OBSERVER_RETURN_LR,
            "constructed_error": -44,
            "trace_component_uid": trace_component_uid,
            "entry_trace_id": (trace_id_literal & 0xFFFF) + 2,
            "exit_trace_id": (trace_id_literal & 0xFFFF) + 3,
            "instruction_evidence": (
                "LDR decoder+0xBC; LSLS #26 tests original bit 5; when clear, "
                "MVNS of 43 constructs -44 immediately before observer BLX"
            ),
            "semantic_limit": (
                "the function name is cross-identified by exact trace component/IDs and the target SDK "
                "generated trace dictionary; why policy selected AccessDenied remains unknown"
            ),
        },
        "lower_stubs": stub_targets,
        "verified_windows": verified,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = analyze(args.module)
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
