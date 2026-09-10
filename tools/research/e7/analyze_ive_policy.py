#!/usr/bin/env python3
"""Validate the bounded E7 IVE policy client/server reconstruction.

The inputs may be firmware-derived candidates or read-only files copied from
the participating phone.  This tool deliberately validates only the code
windows used by the Prepare -12017 investigation and records the remaining
dynamic boundary.
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
HEADER_SIZE = ROM_MODULE.ROM_IMAGE_HEADER_SIZE
parse_header = ROM_MODULE.parse_header

EXPECTED = {
    "client": {
        "sha256": "CAB0774617B89E21A9327FC45A7A77AEAB6C9289933859A137753A87CCED5F73",
        "uids": (0x10000079, 0x1000008D, 0x10204C26),
        "code_address": 0x80D20F18,
        "code_size": 0x136C,
        "windows": {
            "request_public": (0x80D211A6, 0x5A, "DC3957C719F2D457C38B7E3CF004AC4B16D0A0A64885D5FE79826A44EB20AD7C"),
            "request_packaging": (0x80D21460, 0x11C, "D9AEF505047BACC0E64F4728A6C8EB6135EE740EE1AF04ED015D90FCF9933AA0"),
            "runl_dispatch": (0x80D216C4, 0xA0, "B580FC2F4EC737E82336914B10D0BABF46DC76D3F6F8F793A1BF88DC5E3AF9B0"),
        },
    },
    "server": {
        "sha256": "2DFFC361DDDB0BCC8B4DF339E9FD4FC6731726698CF4594B3B30AF6526839DC1",
        "uids": (0x1000007A, 0x00000000, 0x10204C27),
        "code_address": 0x80D22308,
        "code_size": 0x31B8,
        "windows": {
            "execute_decision": (0x80D22BD6, 0x1E4, "7D0A5B4D0C62F37349CE6B5F537D73E3554336CEAE3BA86B4E9EA04AF9227173"),
            "policy_select_and_evaluate": (0x80D23076, 0x14A, "8DD4F3A63E95BBE29A0D176B62C0AC70E7F5CA08EA70E63251788695339DE819"),
            "resource_manager_construction": (0x80D232C0, 0xE4, "E0AB78EFCECAA896979E88340DD1C2C95A9F054CBD50AEE5458854A4FC350DAA"),
            "resource_allocator": (0x80D23548, 0xDA, "384B72DF70CDD7B2AF88CE19760CD02FCCDA9C3D63CBA33CAEDF6DC04D4090ED"),
            "resource_rule": (0x80D24374, 0x30, "0717AF5C68B0E34C879F27112E6D0FCCE959C1361412F9A0D404422E4C6962DE"),
        },
    },
}

EXPECTED_RCAM = {
    "sha256": "641537952D4E1EC95C0A0B3F3D6A5961C71BF62CAA8DDB4E9CAD1E5B5F74962B",
    "uids": (0x10000079, 0x10005A33, 0x00000000),
    "code_address": 0x80BC8FD8,
    "code_size": 0xF70,
    "available_export_count": 30,
}

RCAM_IMPORTS = {
    0x80D224E0: {
        "target": 0x80BC9779,
        "ordinal": 15,
        "name": "RCam::SetClientInfo(TCamArg, TCamArg, TCamArg, TCamResourceLevel&, unsigned)",
        "basis": "target equals reconstructed rcam ordinal 15 export; target SDK rcam DSO names ordinal 15",
    },
    0x80D224E8: {
        "target": 0x80BC9753,
        "ordinal": 16,
        "name": "RCam::SetClientInfo(TCamArg, TCamArg, TCamArg, unsigned)",
        "basis": "target equals reconstructed rcam ordinal 16 export; target SDK rcam DSO names ordinal 16",
    },
    0x80D224F0: {
        "target": 0x80BC97A5,
        "ordinal": 23,
        "name": "RCam::RemoveClientInfo(TCamArg, TCamArg)",
        "basis": "target equals reconstructed rcam ordinal 23 export; target SDK rcam DSO names ordinal 23",
    },
    0x80D224F8: {
        "target": 0x80BC97ED,
        "ordinal": 33,
        "name": "RCam::GetClientsWithResourceLevel(..., TCamResourceLevel&, unsigned)",
        "basis": "SDK ordinal/signature plus caller's extra resource-level argument; export entry lies in unavailable tail",
    },
    0x80D22500: {
        "target": 0x80BC97C5,
        "ordinal": 34,
        "name": "RCam::GetClientsWithResourceLevel(..., unsigned)",
        "basis": "SDK ordinal/signature plus caller's shorter argument list; export entry lies in unavailable tail",
    },
}

# The policy caller is Thumb, the import veneers are ARM, and both RCam
# implementations return to Thumb. ``caller_link_lr`` is the value placed in
# LR by the policy BLX and consumed by the RCam pop-to-PC epilogue.
# ``common_call_link_lr`` is likewise consumed by the common RCam helper.  At
# the policy join, both overloads instead expose residue from the same lower
# RBusLogicalChannel::DoControl path, so LR is corroboration, not an overload
# discriminator.
RCAM_SETCLIENT_CALLS = {
    15: {
        "policy_call": 0x80D235D6,
        "policy_stub": 0x80D224E0,
        "caller_link_lr": 0x80D235DB,
        "caller_return_pc": 0x80D235DA,
        "join_pc": 0x80D235E8,
        "rcam_entry": 0x80BC9778,
        "rcam_window_length": 0x2C,
        "common_call_link_lr": 0x80BC97A1,
        "selected_when": "current +0x14 is nonzero",
        "post_call_stack": {
            "sp+0x00": "pointer to the 0x24-byte resource-level copy at sp+0x18",
            "sp+0x04": "camera handle / final unsigned argument",
        },
    },
    16: {
        "policy_call": 0x80D235E4,
        "policy_stub": 0x80D224E8,
        "caller_link_lr": 0x80D235E9,
        "caller_return_pc": 0x80D235E8,
        "join_pc": 0x80D235E8,
        "rcam_entry": 0x80BC9752,
        "rcam_window_length": 0x26,
        "common_call_link_lr": 0x80BC9775,
        "selected_when": "current +0x14 is zero",
        "post_call_stack": {
            "sp+0x00": "camera handle / final unsigned argument",
            "sp+0x04": "not an argument of ordinal 16; do not interpret",
        },
    },
}

RCAM_ARGUMENT_SOURCES = {
    "implicit_this_r0": {
        "call_value": "[allocator sp+0x4C]",
        "origin": "allocator primary this + 8; embedded RCam object",
    },
    "explicit_1_r1": {
        "abi_type": "RCam::TCamArg by value (EABI passes its address)",
        "call_value": "allocator sp+0x10",
        "source_stack_copy": "[allocator sp+0x54]",
        "payload": "[current+0x04] original client PID",
    },
    "explicit_2_r2": {
        "abi_type": "RCam::TCamArg by value (EABI passes its address)",
        "call_value": "allocator sp+0x0C",
        "source_stack_copy": "[allocator sp+0x58]",
        "payload": "[current+0x48] policy token",
    },
    "explicit_3_r3": {
        "abi_type": "RCam::TCamArg by value (EABI passes its address)",
        "call_value": "allocator sp+0x08",
        "source_stack_copy": "[allocator sp+0x50]",
        "payload": "[current+0x54] mapped resource code; mode 1 maps to 5",
    },
    "ordinal_15_stack_0": {
        "abi_type": "RCam::TCamResourceLevel&",
        "call_value": "allocator sp+0x18",
        "source_stack_copy": "[allocator sp+0x18..+0x3B]",
        "payload": "0x24-byte copy of current+0x18..+0x3B",
    },
    "final_unsigned": {
        "abi_type": "unsigned int",
        "ordinal_15_call_value": "[allocator sp+0x04]",
        "ordinal_16_call_value": "[allocator sp+0x00]",
        "source_stack_copy": "[allocator sp+0x3C]",
        "payload": "[current+0x10] camera handle",
    },
}

CLIENT_REQUEST_ENTRY_OFFSET = 0x28E
CLIENT_RUNL_OFFSET = 0x7AC
SERVER_EXECUTE_DECISION_OFFSET = 0x8CE
SERVER_POLICY_EVALUATE_OFFSET = 0xD6E
SERVER_COMMON_RULE_RETURN_OFFSET = 0xD98
SERVER_RESOURCE_ALLOCATOR_OFFSET = 0x1240
SERVER_RESOURCE_RULE_OFFSET = 0x206C
SERVER_RESOURCE_RETURN_OFFSET = 0x208A
SERVER_DENY_CALL_OFFSET = 0xA4A


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def code_slice(data: bytes, code_address: int, address: int, length: int) -> bytes:
    offset = HEADER_SIZE + address - code_address
    if offset < HEADER_SIZE or offset + length > len(data):
        raise ValueError(f"mapped window 0x{address:08X}+0x{length:X} is incomplete")
    return data[offset:offset + length]


def analyze_one(path: Path, role: str, identity_source: str) -> dict[str, object]:
    spec = EXPECTED[role]
    data = path.read_bytes()
    header = parse_header(data)
    digest = sha256(data)
    if digest != spec["sha256"]:
        raise ValueError(f"unexpected {role} candidate SHA-256 {digest}")
    if (header["uid1"], header["uid2"], header["uid3"]) != spec["uids"]:
        raise ValueError(f"{role} UID tuple changed")
    if header["code_address"] != spec["code_address"] or header["code_size"] != spec["code_size"]:
        raise ValueError(f"{role} code address/size changed")

    verified = {}
    for name, (address, length, expected_hash) in spec["windows"].items():
        part = code_slice(data, spec["code_address"], address, length)
        actual_hash = sha256(part)
        if actual_hash != expected_hash:
            raise ValueError(f"{role} {name} fingerprint changed")
        verified[name] = {
            "code_address": address,
            "code_offset": address - spec["code_address"],
            "length": length,
            "sha256": actual_hash,
        }

    available = len(data) - HEADER_SIZE
    return {
        "path": str(path),
        "file_bytes": len(data),
        "file_sha256": digest,
        "representation": "TRomImageHeader plus reconstructed mapped ROM bytes",
        "identity_source": identity_source,
        "identity_limit": (
            "participating-phone Z: copy byte-identical to the validated RM-626 SW111.040.1511 "
            "candidate; same-run module load/range/fingerprint remains required for dynamic use"
            if identity_source == "participating-phone-copy"
            else "RM-626 SW111.040.1511 firmware candidate only; compare the corresponding "
            "participating-phone copy before using these offsets dynamically"
        ),
        "header": header,
        "abi": {"eabi": bool(header["flags"] & 0x8), "eka2": bool(header["flags"] & 0x20)},
        "declared_code_range": [header["code_address"], header["code_address"] + header["code_size"]],
        "available_mapped_code_range": [header["code_address"], header["code_address"] + available],
        "available_mapped_code_bytes": available,
        "missing_declared_code_tail_bytes": max(0, header["code_size"] - available),
        "verified_windows": verified,
    }


def compare_window(path: Path, address: int, code_address: int, length: int) -> str:
    data = path.read_bytes()
    return sha256(code_slice(data, code_address, address, length))


def analyze_rcam(path: Path, server_data: bytes) -> dict[str, object]:
    data = path.read_bytes()
    header = parse_header(data)
    digest = sha256(data)
    if digest != EXPECTED_RCAM["sha256"]:
        raise ValueError(f"unexpected rcam candidate SHA-256 {digest}")
    if (header["uid1"], header["uid2"], header["uid3"]) != EXPECTED_RCAM["uids"]:
        raise ValueError("rcam UID tuple changed")
    if header["code_address"] != EXPECTED_RCAM["code_address"] or header["code_size"] != EXPECTED_RCAM["code_size"]:
        raise ValueError("rcam code address/size changed")

    export_offset = HEADER_SIZE + header["export_dir"] - header["code_address"]
    available_exports = min(header["export_count"], max(0, (len(data) - export_offset) // 4))
    if available_exports != EXPECTED_RCAM["available_export_count"]:
        raise ValueError("rcam available export count changed")
    exports = {ordinal: u32(data, export_offset + 4 * (ordinal - 1)) for ordinal in range(1, available_exports + 1)}

    server_base = EXPECTED["server"]["code_address"]
    imports = []
    for stub, expected in RCAM_IMPORTS.items():
        stub_bytes = code_slice(server_data, server_base, stub, 8)
        if stub_bytes[:4] != bytes.fromhex("04F01FE5"):
            raise ValueError(f"server stub 0x{stub:08X} is not an ARM import veneer")
        target = u32(stub_bytes, 4)
        if target != expected["target"]:
            raise ValueError(f"server rcam import target at 0x{stub:08X} changed")
        if expected["ordinal"] <= available_exports and exports[expected["ordinal"]] != target:
            raise ValueError(f"rcam ordinal {expected['ordinal']} export target changed")
        imports.append({"server_stub": stub, **expected})
    setclient_calls = []
    for ordinal, call in RCAM_SETCLIENT_CALLS.items():
        rcam_window = code_slice(
            data,
            EXPECTED_RCAM["code_address"],
            call["rcam_entry"],
            call["rcam_window_length"],
        )
        setclient_calls.append(
            {
                "ordinal": ordinal,
                **call,
                "rcam_window_sha256": sha256(rcam_window),
                "lr_limit": (
                    "common_call_link_lr is restored into PC by the overload epilogue and is not "
                    "the LR register value visible at the policy join"
                ),
            }
        )
    return {
        "path": str(path),
        "file_bytes": len(data),
        "file_sha256": digest,
        "identity_limit": "same-firmware candidate; not a participating-phone copy",
        "header": header,
        "available_export_entries": available_exports,
        "missing_export_entries": header["export_count"] - available_exports,
        "server_imports": imports,
        "setclient_calls": setclient_calls,
        "argument_sources": RCAM_ARGUMENT_SOURCES,
        "shared_lower_control": {
            "rcam_common": 0x80BC9302,
            "rcam_common_length": 0x1C,
            "rcam_common_sha256": sha256(code_slice(
                data, EXPECTED_RCAM["code_address"], 0x80BC9302, 0x1C
            )),
            "rcam_control_veneer": 0x80BC9088,
            "rcam_control_veneer_bytes": code_slice(
                data, EXPECTED_RCAM["code_address"], 0x80BC9088, 8
            ).hex(),
            "rcam_control_target": 0x80484F6D,
            "target_sdk_dso": "euser.dso",
            "target_sdk_ordinal": 491,
            "target_sdk_symbol": "RBusLogicalChannel::DoControl(int, void*, void*)",
            "call_abi": {
                "r0": "embedded RBusLogicalChannel at RCam this + 4",
                "r1": "DoControl function 0",
                "r2": "subcommand 7 encoded as the first pointer-sized argument",
                "r3": "pointer to the bounded SetClientInfo request block",
            },
            "join_lr": 0x80484F75,
            "join_lr_role": (
                "shared deeper-call residue observed on the participating E7; it confirms the "
                "common lower control path but cannot distinguish ordinal 15 from 16"
            ),
        },
        "lr_correction": (
            "0x80D235DB/0x80D235E9 are incoming Thumb link values consumed by the RCam "
            "pop-to-PC return. 0x80BC97A1/0x80BC9775 are then consumed by the overload "
            "epilogues after both call the same helper at 0x80BC9302. LR at policy+0x12E0 "
            "therefore cannot classify the overload; use the verified policy branch plus "
            "the distinct outgoing-stack ABI, and use LR only to corroborate the shared "
            "lower control path."
        ),
        "conclusion": (
            "CIveResourceManager's direct allocation boundary calls the two RCam::SetClientInfo overloads; "
            "the actual nonzero return in the failing session remains UNKNOWN"
        ),
    }


def analyze(client: Path, server: Path, comparison_client: Path | None = None,
            comparison_server: Path | None = None, rcam: Path | None = None,
            identity_source: str = "firmware-candidate") -> dict[str, object]:
    server_data = server.read_bytes()
    result = {
        "client": analyze_one(client, "client", identity_source),
        "server": analyze_one(server, "server", identity_source),
        "request_contract": {
            "public_request_code_offset": CLIENT_REQUEST_ENTRY_OFFSET,
            "public_request_abi": {
                "r0": "CIvePolicy this",
                "r1": "original client process id",
                "r2": "TIveMode",
                "r3": "camera handle",
                "stack_0": "waiting flag",
                "stack_4": "RCam::TCamera selector",
                "stack_8": "optional 0x24-byte TCamResourceLevel pointer",
            },
            "ipc_payload": {
                "+0x00": "original process id",
                "+0x04": "TIveMode",
                "+0x08": "camera handle",
                "+0x0C": "camera selector",
                "+0x10": "resource-level-present flag",
                "+0x14": "request control field; semantic name not proved",
                "+0x18..+0x3B": "0x24-byte resource level when present",
            },
            "ipc_function": 1,
            "runl_code_offset": CLIENT_RUNL_OFFSET,
            "response": (
                "when active-object iStatus is zero, selector 0 invokes Granted(token) and "
                "selector 1 invokes Denied(); Denied is therefore a policy response, not a transport error"
            ),
        },
        "server_decision": {
            "execute_decision_code_offset": SERVER_EXECUTE_DECISION_OFFSET,
            "policy_evaluate_code_offset": SERVER_POLICY_EVALUATE_OFFSET,
            "trace_identity": {
                "component_uid": 0x10204C27,
                "entry_id": 0x17,
                "exit_id": 0x18,
                "granted_flow_id": 0x09,
                "denied_flow_id": 0x0A,
                "basis": "target SDK generated trace dictionary plus candidate instructions/literals",
            },
            "mode_1_rules": ["CIveRuleClientProcess", "CIveRuleNumOfActiveClients", "CIveRuleResource"],
            "mode_1_excluded_rules": [
                "CIveRuleSenderCapability", "CIveRuleNumOfCamera", "CIveRuleMatchCamera", "CIveRulePreEmption"
            ],
            "decision_fields": {
                "+0x04": "original client PID used by CIveRuleClientProcess",
                "+0x08": "second client identity word; exact semantic name unproved",
                "+0x44": "grant/deny decision marker",
                "+0x48": "generated token",
                "+0x50": "resource-granted marker set with +0x44 by CIveRuleResource",
                "+0x54": "mapped resource code; TIveMode 1 maps to 5",
                "+0x90": "rule-loop short-circuit marker; exact semantic name unproved",
            },
            "resource_path": {
                "common_rule_return_observation_offset": SERVER_COMMON_RULE_RETURN_OFFSET,
                "rule_code_offset": SERVER_RESOURCE_RULE_OFFSET,
                "allocator_code_offset": SERVER_RESOURCE_ALLOCATOR_OFFSET,
                "allocator_return_observation_offset": SERVER_RESOURCE_RETURN_OFFSET,
                "denied_observer_call_offset": SERVER_DENY_CALL_OFFSET,
                "object_link": (
                    "ResourceManager primary object owns a secondary MIveResourceAllocator at +4; "
                    "DecisionEngine receives that +4 pointer, CIveRuleResource stores it, and its "
                    "vslot 0 thunk subtracts 4 before branching to the allocator at +0x1240"
                ),
                "pseudo": (
                    "map mode 1 to resource 5; call MIveResourceAllocator(current); "
                    "allocator return 0 sets current+0x44 and +0x50 to 1, nonzero leaves +0x44 at 0; "
                    "ExecuteDecision later emits Denied when +0x44 remains 0"
                ),
            },
        },
        "dynamic_contract": {
            "phase_context": (
                "the two policy rule-return points already proved rule indexes 0/1 returned zero, Resource ran, "
                "its allocator returned -2, and the wrapper at index 2 returned zero; do not observe those points again"
            ),
            "precondition": (
                "before modifying code, compare the live allocator, policy import veneers, and both RCam overload "
                "windows byte-for-byte with the bounded candidate fingerprints; do not reuse an old PID or object"
            ),
            "minimal_points": [
                {
                    "module": "ivepolicyserver.exe",
                    "absolute_candidate_address": 0x80D235E8,
                    "code_offset": 0x12E0,
                    "original_halfword": "0x0006 (Thumb LSLS r6,r0,#0 / MOVS alias)",
                    "read": (
                        "original r0, live LR, policy PID/TID/SID, r5 current, fixed 0x5C-byte allocator stack, "
                        "and fixed 0x58-byte current object"
                    ),
                    "answers": "which SetClientInfo overload returned -2 and the inputs retained for that call frame",
                }
            ],
            "classification": (
                "after live policy/RCam windows pass, ordinal 15 requires current+0x14 nonzero, "
                "stack0==sp+0x18 and stack1==camera; ordinal 16 requires current+0x14 zero and "
                "stack0==camera. LR is not an overload discriminator because both paths share "
                "RCam common control and RBusLogicalChannel::DoControl"
            ),
            "media_owner": "the observer process creates the media; CODA must not start a second process while the sentinel is active",
            "capability_stop": (
                "on MOVS-gate, fingerprint, instance association, snapshot, overflow, restore, or cleanup failure, "
                "stop and retain the handler when needed; do not add points or scan unrelated dependencies"
            ),
        },
        "limits": [
            (
                "The supplied files are participating-phone copies and match the firmware candidates byte-for-byte, "
                "but the same-run loaded-module identity and base remain dynamic facts."
                if identity_source == "participating-phone-copy"
                else "The firmware candidates are not yet participating-phone copies."
            ),
            "The completed policy observation proves allocator -2, but not which SetClientInfo overload returned it or that call's values.",
            "Do not enter RCam IPC/server internals until the one return observer identifies the overload and bounded inputs.",
            "No permission, priority, lifetime, window, DLL replacement, or patch conclusion follows yet.",
        ],
    }

    if rcam is not None:
        result["direct_rcam_boundary"] = analyze_rcam(rcam, server_data)

    if (comparison_client is None) != (comparison_server is None):
        raise ValueError("comparison client and server must be supplied together")
    if comparison_client is not None and comparison_server is not None:
        comparison_specs = {
            "client_request_public": (comparison_client, 0x80D113F8, 0x80D11686, 0x5A),
            "client_request_packaging": (comparison_client, 0x80D113F8, 0x80D11940, 0x11C),
            "server_execute_decision": (comparison_server, 0x80D127E8, 0x80D130B6, 0x1E4),
            "server_policy_select_and_evaluate": (comparison_server, 0x80D127E8, 0x80D13556, 0x14A),
            "server_resource_manager_construction": (comparison_server, 0x80D127E8, 0x80D137A0, 0xE4),
            "server_resource_allocator": (comparison_server, 0x80D127E8, 0x80D13A28, 0xDA),
            "server_resource_rule": (comparison_server, 0x80D127E8, 0x80D14854, 0x30),
        }
        result["comparison_603"] = {
            name: {
                "sha256": compare_window(path, address, base, length),
                "matches_e7": compare_window(path, address, base, length) in {
                    item["sha256"]
                    for role in (result["client"], result["server"])
                    for item in role["verified_windows"].values()
                },
            }
            for name, (path, base, address, length) in comparison_specs.items()
        }
        result["comparison_603"]["limit"] = (
            "RM-779 SW111.020.0310 is a structural aid only, not the known-good SW113 phone and not a dynamic control"
        )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    parser.add_argument("server", type=Path)
    parser.add_argument("--comparison-client", type=Path)
    parser.add_argument("--comparison-server", type=Path)
    parser.add_argument("--rcam", type=Path)
    parser.add_argument(
        "--identity-source",
        choices=("firmware-candidate", "participating-phone-copy"),
        default="firmware-candidate",
    )
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = analyze(
        args.client,
        args.server,
        args.comparison_client,
        args.comparison_server,
        args.rcam,
        args.identity_source,
    )
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
