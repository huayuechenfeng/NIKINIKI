#!/usr/bin/env python3
"""Inspect/rewrite Annex-B H.264 and compare device frames with PC goldens."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import zlib
from collections import Counter
from pathlib import Path
from typing import Any


HIGH_PROFILES = {44, 83, 86, 100, 110, 118, 122, 128, 134, 135, 138, 139, 244}
PROFILE_NAMES = {
    66: "Baseline",
    77: "Main",
    88: "Extended",
    100: "High",
    110: "High 10",
    122: "High 4:2:2",
    244: "High 4:4:4 Predictive",
}
SLICE_NAMES = {0: "P", 1: "B", 2: "I", 3: "SP", 4: "SI"}


class H264ParseError(ValueError):
    pass


class BitReader:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.position = 0

    @property
    def remaining(self) -> int:
        return len(self.data) * 8 - self.position

    def read_bits(self, count: int) -> int:
        if count < 0 or self.remaining < count:
            raise H264ParseError("truncated RBSP")
        value = 0
        for _ in range(count):
            byte_index = self.position >> 3
            bit_index = 7 - (self.position & 7)
            value = (value << 1) | ((self.data[byte_index] >> bit_index) & 1)
            self.position += 1
        return value

    def read_bit(self) -> int:
        return self.read_bits(1)

    def read_ue(self) -> int:
        zeroes = 0
        while True:
            if self.remaining <= 0:
                raise H264ParseError("truncated Exp-Golomb value")
            if self.read_bit() == 1:
                break
            zeroes += 1
            if zeroes > 31:
                raise H264ParseError("Exp-Golomb value is too large")
        suffix = self.read_bits(zeroes) if zeroes else 0
        return (1 << zeroes) - 1 + suffix

    def read_se(self) -> int:
        code_num = self.read_ue()
        value = (code_num + 1) >> 1
        return -value if code_num % 2 == 0 else value


def unsigned_exp_golomb_bits(value: int) -> str:
    if value < 0:
        raise ValueError("unsigned Exp-Golomb value cannot be negative")
    payload = f"{value + 1:b}"
    return "0" * (len(payload) - 1) + payload


def bits_to_bytes(value: str) -> bytes:
    if len(value) % 8:
        raise ValueError("bit string is not byte-aligned")
    return bytes(int(value[offset : offset + 8], 2) for offset in range(0, len(value), 8))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def annex_b_nal_units(data: bytes) -> list[bytes]:
    starts: list[tuple[int, int]] = []
    cursor = 0
    limit = len(data)
    while cursor + 3 <= limit:
        if cursor + 4 <= limit and data[cursor : cursor + 4] == b"\x00\x00\x00\x01":
            starts.append((cursor, 4))
            cursor += 4
        elif data[cursor : cursor + 3] == b"\x00\x00\x01":
            starts.append((cursor, 3))
            cursor += 3
        else:
            cursor += 1
    if not starts:
        raise H264ParseError("no Annex-B start code found")

    units: list[bytes] = []
    for index, (start, prefix) in enumerate(starts):
        end = starts[index + 1][0] if index + 1 < len(starts) else limit
        unit = data[start + prefix : end].rstrip(b"\x00")
        if unit:
            units.append(unit)
    if not units:
        raise H264ParseError("Annex-B stream has no NAL payload")
    return units


def ebsp_to_rbsp(ebsp: bytes) -> bytes:
    rbsp = bytearray()
    zeroes = 0
    for value in ebsp:
        if zeroes >= 2 and value == 0x03:
            zeroes = 0
            continue
        rbsp.append(value)
        zeroes = zeroes + 1 if value == 0 else 0
    return bytes(rbsp)


def rbsp_to_ebsp(rbsp: bytes) -> bytes:
    ebsp = bytearray()
    zeroes = 0
    for value in rbsp:
        if zeroes >= 2 and value <= 0x03:
            ebsp.append(0x03)
            zeroes = 0
        ebsp.append(value)
        zeroes = zeroes + 1 if value == 0 else 0
    return bytes(ebsp)


def skip_scaling_list(bits: BitReader, size: int) -> None:
    last_scale = 8
    next_scale = 8
    for _ in range(size):
        if next_scale != 0:
            delta_scale = bits.read_se()
            next_scale = (last_scale + delta_scale + 256) & 255
        if next_scale != 0:
            last_scale = next_scale


def skip_hrd(bits: BitReader) -> None:
    cpb_count_minus1 = bits.read_ue()
    bits.read_bits(4)
    bits.read_bits(4)
    for _ in range(cpb_count_minus1 + 1):
        bits.read_ue()
        bits.read_ue()
        bits.read_bit()
    bits.read_bits(5)
    bits.read_bits(5)
    bits.read_bits(5)
    bits.read_bits(5)


def parse_vui(bits: BitReader) -> dict[str, Any]:
    result: dict[str, Any] = {
        "max_num_reorder_frames": None,
        "max_dec_frame_buffering": None,
        "timing": None,
    }
    if bits.read_bit():
        aspect_ratio_idc = bits.read_bits(8)
        if aspect_ratio_idc == 255:
            bits.read_bits(16)
            bits.read_bits(16)
    if bits.read_bit():
        bits.read_bit()
    if bits.read_bit():
        bits.read_bits(3)
        bits.read_bit()
        if bits.read_bit():
            bits.read_bits(8)
            bits.read_bits(8)
            bits.read_bits(8)
    if bits.read_bit():
        bits.read_ue()
        bits.read_ue()
    if bits.read_bit():
        num_units_in_tick = bits.read_bits(32)
        time_scale = bits.read_bits(32)
        fixed_frame_rate = bool(bits.read_bit())
        result["timing"] = {
            "num_units_in_tick": num_units_in_tick,
            "time_scale": time_scale,
            "fixed_frame_rate": fixed_frame_rate,
            "nominal_fps": (
                time_scale / (2.0 * num_units_in_tick) if num_units_in_tick else None
            ),
        }
    nal_hrd = bool(bits.read_bit())
    if nal_hrd:
        skip_hrd(bits)
    vcl_hrd = bool(bits.read_bit())
    if vcl_hrd:
        skip_hrd(bits)
    if nal_hrd or vcl_hrd:
        bits.read_bit()
    bits.read_bit()
    if bits.read_bit():
        bits.read_bit()
        bits.read_ue()
        bits.read_ue()
        bits.read_ue()
        bits.read_ue()
        result["max_num_reorder_frames"] = bits.read_ue()
        result["max_dec_frame_buffering"] = bits.read_ue()
    return result


def locate_vui_dpb_field(bits: BitReader) -> tuple[int, int, int]:
    if bits.read_bit():
        aspect_ratio_idc = bits.read_bits(8)
        if aspect_ratio_idc == 255:
            bits.read_bits(16)
            bits.read_bits(16)
    if bits.read_bit():
        bits.read_bit()
    if bits.read_bit():
        bits.read_bits(3)
        bits.read_bit()
        if bits.read_bit():
            bits.read_bits(8)
            bits.read_bits(8)
            bits.read_bits(8)
    if bits.read_bit():
        bits.read_ue()
        bits.read_ue()
    if bits.read_bit():
        bits.read_bits(32)
        bits.read_bits(32)
        bits.read_bit()
    nal_hrd = bool(bits.read_bit())
    if nal_hrd:
        skip_hrd(bits)
    vcl_hrd = bool(bits.read_bit())
    if vcl_hrd:
        skip_hrd(bits)
    if nal_hrd or vcl_hrd:
        bits.read_bit()
    bits.read_bit()
    if not bits.read_bit():
        raise H264ParseError("SPS VUI has no bitstream_restriction_flag")
    bits.read_bit()
    bits.read_ue()
    bits.read_ue()
    bits.read_ue()
    bits.read_ue()
    bits.read_ue()
    start = bits.position
    old_value = bits.read_ue()
    end = bits.position
    if bits.remaining < 1 or bits.read_bit() != 1:
        raise H264ParseError("invalid SPS rbsp_trailing_bits")
    while bits.remaining:
        if bits.read_bit() != 0:
            raise H264ParseError("non-zero SPS padding after rbsp_stop_one_bit")
    return start, end, old_value


def locate_sps_dpb_field(nal: bytes) -> tuple[bytes, int, int, int]:
    if not nal or (nal[0] & 0x1F) != 7:
        raise H264ParseError("NAL is not an SPS")
    rbsp = ebsp_to_rbsp(nal[1:])
    bits = BitReader(rbsp)
    profile_idc = bits.read_bits(8)
    bits.read_bits(8)
    bits.read_bits(8)
    bits.read_ue()
    chroma_format_idc = 1
    if profile_idc in HIGH_PROFILES:
        chroma_format_idc = bits.read_ue()
        if chroma_format_idc == 3:
            bits.read_bit()
        bits.read_ue()
        bits.read_ue()
        bits.read_bit()
        if bits.read_bit():
            scaling_count = 12 if chroma_format_idc == 3 else 8
            for index in range(scaling_count):
                if bits.read_bit():
                    skip_scaling_list(bits, 16 if index < 6 else 64)
    bits.read_ue()
    pic_order_cnt_type = bits.read_ue()
    if pic_order_cnt_type == 0:
        bits.read_ue()
    elif pic_order_cnt_type == 1:
        bits.read_bit()
        bits.read_se()
        bits.read_se()
        for _ in range(bits.read_ue()):
            bits.read_se()
    elif pic_order_cnt_type != 2:
        raise H264ParseError(f"invalid pic_order_cnt_type {pic_order_cnt_type}")
    bits.read_ue()
    bits.read_bit()
    bits.read_ue()
    bits.read_ue()
    frame_mbs_only_flag = bits.read_bit()
    if not frame_mbs_only_flag:
        bits.read_bit()
    bits.read_bit()
    if bits.read_bit():
        bits.read_ue()
        bits.read_ue()
        bits.read_ue()
        bits.read_ue()
    if not bits.read_bit():
        raise H264ParseError("SPS has no VUI parameters")
    start, end, old_value = locate_vui_dpb_field(bits)
    return rbsp, start, end, old_value


def locate_sps_ref_field(nal: bytes) -> tuple[bytes, int, int, int]:
    if not nal or (nal[0] & 0x1F) != 7:
        raise H264ParseError("NAL is not an SPS")
    rbsp = ebsp_to_rbsp(nal[1:])
    bits = BitReader(rbsp)
    profile_idc = bits.read_bits(8)
    bits.read_bits(8)
    bits.read_bits(8)
    bits.read_ue()
    chroma_format_idc = 1
    if profile_idc in HIGH_PROFILES:
        chroma_format_idc = bits.read_ue()
        if chroma_format_idc == 3:
            bits.read_bit()
        bits.read_ue()
        bits.read_ue()
        bits.read_bit()
        if bits.read_bit():
            scaling_count = 12 if chroma_format_idc == 3 else 8
            for index in range(scaling_count):
                if bits.read_bit():
                    skip_scaling_list(bits, 16 if index < 6 else 64)
    bits.read_ue()
    pic_order_cnt_type = bits.read_ue()
    if pic_order_cnt_type == 0:
        bits.read_ue()
    elif pic_order_cnt_type == 1:
        bits.read_bit()
        bits.read_se()
        bits.read_se()
        for _ in range(bits.read_ue()):
            bits.read_se()
    elif pic_order_cnt_type != 2:
        raise H264ParseError(f"invalid pic_order_cnt_type {pic_order_cnt_type}")
    start = bits.position
    old_value = bits.read_ue()
    end = bits.position
    return rbsp, start, end, old_value


def rewrite_sps_ref_nal(nal: bytes, value: int) -> tuple[bytes, int]:
    if value < 0:
        raise ValueError("max_num_ref_frames cannot be negative")
    rbsp, start, end, old_value = locate_sps_ref_field(nal)
    source_bits = "".join(f"{byte:08b}" for byte in rbsp)
    stop_position = source_bits.rfind("1")
    if stop_position < end or any(bit != "0" for bit in source_bits[stop_position + 1 :]):
        raise H264ParseError("invalid SPS rbsp_trailing_bits")
    syntax_bits = (
        source_bits[:start]
        + unsigned_exp_golomb_bits(value)
        + source_bits[end:stop_position]
    )
    rewritten_bits = syntax_bits + "1"
    rewritten_bits += "0" * ((8 - len(rewritten_bits) % 8) % 8)
    rewritten_rbsp = bits_to_bytes(rewritten_bits)
    return bytes([nal[0]]) + rbsp_to_ebsp(rewritten_rbsp), old_value


def rewrite_sps_dpb_nal(nal: bytes, value: int) -> tuple[bytes, int]:
    rbsp, start, end, old_value = locate_sps_dpb_field(nal)
    source_bits = "".join(f"{byte:08b}" for byte in rbsp)
    syntax_bits = source_bits[:start] + unsigned_exp_golomb_bits(value)
    rewritten_bits = syntax_bits + "1"
    rewritten_bits += "0" * ((8 - len(rewritten_bits) % 8) % 8)
    rewritten_rbsp = bits_to_bytes(rewritten_bits)
    return bytes([nal[0]]) + rbsp_to_ebsp(rewritten_rbsp), old_value


def annex_b_start_positions(data: bytes) -> list[tuple[int, int]]:
    starts: list[tuple[int, int]] = []
    cursor = 0
    while cursor + 3 <= len(data):
        if cursor + 4 <= len(data) and data[cursor : cursor + 4] == b"\x00\x00\x00\x01":
            starts.append((cursor, 4))
            cursor += 4
        elif data[cursor : cursor + 3] == b"\x00\x00\x01":
            starts.append((cursor, 3))
            cursor += 3
        else:
            cursor += 1
    return starts


def rewrite_annex_b_sps_dpb(input_path: Path, output_path: Path, value: int) -> dict[str, Any]:
    data = input_path.read_bytes()
    starts = annex_b_start_positions(data)
    if not starts:
        raise H264ParseError("no Annex-B start code found")
    output = bytearray(data[: starts[0][0]])
    old_values: list[int] = []
    rewritten_count = 0
    for index, (start, prefix_size) in enumerate(starts):
        end = starts[index + 1][0] if index + 1 < len(starts) else len(data)
        prefix = data[start : start + prefix_size]
        payload_with_padding = data[start + prefix_size : end]
        payload = payload_with_padding.rstrip(b"\x00")
        padding = payload_with_padding[len(payload) :]
        if payload and (payload[0] & 0x1F) == 7:
            payload, old_value = rewrite_sps_dpb_nal(payload, value)
            old_values.append(old_value)
            rewritten_count += 1
        output.extend(prefix)
        output.extend(payload)
        output.extend(padding)
    if rewritten_count == 0:
        raise H264ParseError("stream has no SPS to rewrite")
    output_path.write_bytes(bytes(output))
    inspection = inspect_annex_b(output_path)
    parsed_values = {entry["max_dec_frame_buffering"] for entry in inspection["sps"]}
    if parsed_values != {value}:
        raise H264ParseError(f"rewritten SPS DPB values are {sorted(parsed_values)}, expected {value}")
    input_non_sps = hashlib.sha256()
    output_non_sps = hashlib.sha256()
    for unit in annex_b_nal_units(data):
        if (unit[0] & 0x1F) != 7:
            input_non_sps.update(unit)
    for unit in annex_b_nal_units(bytes(output)):
        if (unit[0] & 0x1F) != 7:
            output_non_sps.update(unit)
    return {
        "schema_version": 1,
        "operation": "raise_max_dec_frame_buffering",
        "input": str(input_path),
        "output": str(output_path),
        "input_sha256": sha256_file(input_path),
        "output_sha256": sha256_file(output_path),
        "sps_rewritten": rewritten_count,
        "old_values": old_values,
        "new_value": value,
        "non_sps_identical": input_non_sps.digest() == output_non_sps.digest(),
        "non_sps_sha256": input_non_sps.hexdigest().upper(),
    }


def rewrite_annex_b_sps_refs(input_path: Path, output_path: Path, value: int) -> dict[str, Any]:
    if input_path.resolve() == output_path.resolve():
        raise ValueError("diagnostic SPS rewrite requires a distinct destination")
    data = input_path.read_bytes()
    starts = annex_b_start_positions(data)
    if not starts:
        raise H264ParseError("no Annex-B start code found")
    output = bytearray(data[: starts[0][0]])
    old_values: list[int] = []
    rewritten_count = 0
    input_nal_types: list[int] = []
    output_nal_types: list[int] = []
    for index, (start, prefix_size) in enumerate(starts):
        end = starts[index + 1][0] if index + 1 < len(starts) else len(data)
        prefix = data[start : start + prefix_size]
        payload_with_padding = data[start + prefix_size : end]
        payload = payload_with_padding.rstrip(b"\x00")
        padding = payload_with_padding[len(payload) :]
        if payload:
            input_nal_types.append(payload[0] & 0x1F)
        if payload and (payload[0] & 0x1F) == 7:
            payload, old_value = rewrite_sps_ref_nal(payload, value)
            old_values.append(old_value)
            rewritten_count += 1
        if payload:
            output_nal_types.append(payload[0] & 0x1F)
        output.extend(prefix)
        output.extend(payload)
        output.extend(padding)
    if rewritten_count == 0:
        raise H264ParseError("stream has no SPS to rewrite")
    output_path.write_bytes(bytes(output))

    input_inspection = inspect_annex_b(input_path)
    output_inspection = inspect_annex_b(output_path)
    parsed_values = {entry["max_num_ref_frames"] for entry in output_inspection["sps"]}
    if parsed_values != {value}:
        raise H264ParseError(
            f"rewritten SPS reference values are {sorted(parsed_values)}, expected {value}"
        )
    input_sps_without_ref = []
    for entry in input_inspection["sps"]:
        normalized = dict(entry)
        normalized.pop("max_num_ref_frames", None)
        input_sps_without_ref.append(normalized)
    output_sps_without_ref = []
    for entry in output_inspection["sps"]:
        normalized = dict(entry)
        normalized.pop("max_num_ref_frames", None)
        output_sps_without_ref.append(normalized)
    only_ref_semantic_changed = input_sps_without_ref == output_sps_without_ref

    input_non_sps = hashlib.sha256()
    output_non_sps = hashlib.sha256()
    for unit in annex_b_nal_units(data):
        if (unit[0] & 0x1F) != 7:
            input_non_sps.update(unit)
    for unit in annex_b_nal_units(bytes(output)):
        if (unit[0] & 0x1F) != 7:
            output_non_sps.update(unit)
    non_sps_identical = input_non_sps.digest() == output_non_sps.digest()
    if not non_sps_identical or input_nal_types != output_nal_types or not only_ref_semantic_changed:
        raise H264ParseError("diagnostic SPS rewrite changed data outside max_num_ref_frames")
    return {
        "schema_version": 1,
        "operation": "diagnostic_lower_max_num_ref_frames",
        "input": str(input_path),
        "output": str(output_path),
        "input_sha256": sha256_file(input_path),
        "output_sha256": sha256_file(output_path),
        "sps_rewritten": rewritten_count,
        "old_values": old_values,
        "new_value": value,
        "only_max_num_ref_frames_semantic_changed": only_ref_semantic_changed,
        "non_sps_identical": non_sps_identical,
        "non_sps_sha256": input_non_sps.hexdigest().upper(),
        "nal_type_sequence_identical": input_nal_types == output_nal_types,
        "input_sps": input_inspection["sps"],
        "output_sps": output_inspection["sps"],
        "pps_identical": input_inspection["pps"] == output_inspection["pps"],
        "access_unit_count_identical": (
            input_inspection["access_unit_count"] == output_inspection["access_unit_count"]
        ),
    }


def append_h264_eos(input_path: Path, output_path: Path) -> dict[str, Any]:
    data = input_path.read_bytes()
    nal_types = [unit[0] & 0x1F for unit in annex_b_nal_units(data)]
    if 10 in nal_types or 11 in nal_types:
        raise H264ParseError("input already contains end_of_sequence/end_of_stream NAL")
    output_path.write_bytes(data + b"\x00\x00\x00\x01\x0b\x80")
    inspection = inspect_annex_b(output_path)
    if inspection["nal_unit_counts"].get("11") != 1:
        raise H264ParseError("failed to append exactly one end_of_stream NAL")
    return {
        "schema_version": 1,
        "operation": "append_end_of_stream_nal",
        "input": str(input_path),
        "output": str(output_path),
        "input_sha256": sha256_file(input_path),
        "output_sha256": sha256_file(output_path),
        "nal_hex": "000000010B80",
    }


def parse_sps(nal: bytes) -> dict[str, Any]:
    if not nal or (nal[0] & 0x1F) != 7:
        raise H264ParseError("NAL is not an SPS")
    bits = BitReader(ebsp_to_rbsp(nal[1:]))
    profile_idc = bits.read_bits(8)
    constraints = bits.read_bits(8)
    level_idc = bits.read_bits(8)
    sps_id = bits.read_ue()
    chroma_format_idc = 1
    separate_colour_plane_flag = 0
    bit_depth_luma_minus8 = 0
    bit_depth_chroma_minus8 = 0

    if profile_idc in HIGH_PROFILES:
        chroma_format_idc = bits.read_ue()
        if chroma_format_idc == 3:
            separate_colour_plane_flag = bits.read_bit()
        bit_depth_luma_minus8 = bits.read_ue()
        bit_depth_chroma_minus8 = bits.read_ue()
        bits.read_bit()
        if bits.read_bit():
            scaling_count = 12 if chroma_format_idc == 3 else 8
            for index in range(scaling_count):
                if bits.read_bit():
                    skip_scaling_list(bits, 16 if index < 6 else 64)

    log2_max_frame_num_minus4 = bits.read_ue()
    pic_order_cnt_type = bits.read_ue()
    if pic_order_cnt_type == 0:
        bits.read_ue()
    elif pic_order_cnt_type == 1:
        bits.read_bit()
        bits.read_se()
        bits.read_se()
        for _ in range(bits.read_ue()):
            bits.read_se()
    elif pic_order_cnt_type != 2:
        raise H264ParseError(f"invalid pic_order_cnt_type {pic_order_cnt_type}")

    max_num_ref_frames = bits.read_ue()
    gaps_in_frame_num_value_allowed_flag = bool(bits.read_bit())
    pic_width_in_mbs_minus1 = bits.read_ue()
    pic_height_in_map_units_minus1 = bits.read_ue()
    frame_mbs_only_flag = bits.read_bit()
    if not frame_mbs_only_flag:
        bits.read_bit()
    direct_8x8_inference_flag = bool(bits.read_bit())
    crop_left = crop_right = crop_top = crop_bottom = 0
    if bits.read_bit():
        crop_left = bits.read_ue()
        crop_right = bits.read_ue()
        crop_top = bits.read_ue()
        crop_bottom = bits.read_ue()
    vui = parse_vui(bits) if bits.read_bit() else {
        "max_num_reorder_frames": None,
        "max_dec_frame_buffering": None,
        "timing": None,
    }

    chroma_array_type = 0 if separate_colour_plane_flag else chroma_format_idc
    if chroma_array_type == 0:
        crop_unit_x = 1
        crop_unit_y = 2 - frame_mbs_only_flag
    else:
        sub_width_c = 2 if chroma_array_type in (1, 2) else 1
        sub_height_c = 2 if chroma_array_type == 1 else 1
        crop_unit_x = sub_width_c
        crop_unit_y = sub_height_c * (2 - frame_mbs_only_flag)
    coded_width = (pic_width_in_mbs_minus1 + 1) * 16
    coded_height = (2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16
    display_width = coded_width - (crop_left + crop_right) * crop_unit_x
    display_height = coded_height - (crop_top + crop_bottom) * crop_unit_y

    return {
        "profile_idc": profile_idc,
        "profile_name": PROFILE_NAMES.get(profile_idc, f"Profile {profile_idc}"),
        "constraint_flags": constraints,
        "level_idc": level_idc,
        "sps_id": sps_id,
        "chroma_format_idc": chroma_format_idc,
        "bit_depth_luma_minus8": bit_depth_luma_minus8,
        "bit_depth_chroma_minus8": bit_depth_chroma_minus8,
        "log2_max_frame_num_minus4": log2_max_frame_num_minus4,
        "pic_order_cnt_type": pic_order_cnt_type,
        "max_num_ref_frames": max_num_ref_frames,
        "gaps_in_frame_num_value_allowed_flag": gaps_in_frame_num_value_allowed_flag,
        "frame_mbs_only_flag": bool(frame_mbs_only_flag),
        "direct_8x8_inference_flag": direct_8x8_inference_flag,
        "coded_width": coded_width,
        "coded_height": coded_height,
        "display_width": display_width,
        "display_height": display_height,
        "crop": [crop_left, crop_right, crop_top, crop_bottom],
        **vui,
    }


def parse_pps(nal: bytes) -> dict[str, Any]:
    if not nal or (nal[0] & 0x1F) != 8:
        raise H264ParseError("NAL is not a PPS")
    bits = BitReader(ebsp_to_rbsp(nal[1:]))
    pps_id = bits.read_ue()
    sps_id = bits.read_ue()
    entropy_coding_mode_flag = bool(bits.read_bit())
    bottom_field_pic_order_in_frame_present_flag = bool(bits.read_bit())
    slice_groups_minus1 = bits.read_ue()
    if slice_groups_minus1 > 0:
        map_type = bits.read_ue()
        if map_type == 0:
            for _ in range(slice_groups_minus1 + 1):
                bits.read_ue()
        elif map_type == 2:
            for _ in range(slice_groups_minus1):
                bits.read_ue()
                bits.read_ue()
        elif map_type in (3, 4, 5):
            bits.read_bit()
            bits.read_ue()
        elif map_type == 6:
            map_units_minus1 = bits.read_ue()
            slice_group_count = slice_groups_minus1 + 1
            width = max(1, (slice_group_count - 1).bit_length())
            for _ in range(map_units_minus1 + 1):
                bits.read_bits(width)
        else:
            raise H264ParseError(f"invalid slice_group_map_type {map_type}")
    num_ref_idx_l0_default_active_minus1 = bits.read_ue()
    num_ref_idx_l1_default_active_minus1 = bits.read_ue()
    weighted_pred_flag = bits.read_bit()
    weighted_bipred_idc = bits.read_bits(2)
    return {
        "pps_id": pps_id,
        "sps_id": sps_id,
        "entropy_coding_mode_flag": entropy_coding_mode_flag,
        "bottom_field_pic_order_in_frame_present_flag": (
            bottom_field_pic_order_in_frame_present_flag
        ),
        "num_slice_groups_minus1": slice_groups_minus1,
        "num_ref_idx_l0_default_active_minus1": num_ref_idx_l0_default_active_minus1,
        "num_ref_idx_l1_default_active_minus1": num_ref_idx_l1_default_active_minus1,
        "weighted_pred_flag": weighted_pred_flag,
        "weighted_bipred_idc": weighted_bipred_idc,
    }


def parse_slice_summary(nal: bytes) -> tuple[int, str] | None:
    nal_type = nal[0] & 0x1F
    if nal_type not in (1, 5):
        return None
    bits = BitReader(ebsp_to_rbsp(nal[1:]))
    first_mb_in_slice = bits.read_ue()
    slice_type = bits.read_ue() % 5
    return first_mb_in_slice, SLICE_NAMES.get(slice_type, str(slice_type))


def inspect_annex_b(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    units = annex_b_nal_units(data)
    nal_counts = Counter(str(unit[0] & 0x1F) for unit in units)
    sps_entries = [parse_sps(unit) for unit in units if (unit[0] & 0x1F) == 7]
    pps_entries = [parse_pps(unit) for unit in units if (unit[0] & 0x1F) == 8]
    if not sps_entries:
        raise H264ParseError("stream has no SPS")
    if not pps_entries:
        raise H264ParseError("stream has no PPS")

    unique_sps: list[dict[str, Any]] = []
    for entry in sps_entries:
        if entry not in unique_sps:
            unique_sps.append(entry)
    unique_pps: list[dict[str, Any]] = []
    for entry in pps_entries:
        if entry not in unique_pps:
            unique_pps.append(entry)

    slices = Counter()
    access_units = 0
    for unit in units:
        summary = parse_slice_summary(unit)
        if summary is None:
            continue
        first_mb, name = summary
        if first_mb == 0:
            slices[name] += 1
            access_units += 1

    primary_sps = unique_sps[0]
    primary_pps = unique_pps[0]
    return {
        "schema_version": 1,
        "file": path.name,
        "size": path.stat().st_size,
        "sha256": sha256_file(path),
        "nal_unit_counts": dict(sorted(nal_counts.items(), key=lambda item: int(item[0]))),
        "aud_count": int(nal_counts.get("9", 0)),
        "access_unit_count": access_units,
        "slice_type_counts": dict(sorted(slices.items())),
        "sps_occurrences": len(sps_entries),
        "pps_occurrences": len(pps_entries),
        "sps": unique_sps,
        "pps": unique_pps,
        "stream": {
            "profile_idc": primary_sps["profile_idc"],
            "profile_name": primary_sps["profile_name"],
            "level_idc": primary_sps["level_idc"],
            "width": primary_sps["display_width"],
            "height": primary_sps["display_height"],
            "max_num_ref_frames": primary_sps["max_num_ref_frames"],
            "max_num_reorder_frames": primary_sps["max_num_reorder_frames"],
            "max_dec_frame_buffering": primary_sps["max_dec_frame_buffering"],
            "weighted_pred_flag": primary_pps["weighted_pred_flag"],
            "weighted_bipred_idc": primary_pps["weighted_bipred_idc"],
        },
    }


def read_exact(stream: Any, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = stream.read(size - len(data))
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


def build_golden_crc(
    ffmpeg: str,
    path: Path,
    width: int,
    height: int,
    expected_frames: int | None,
) -> dict[str, Any]:
    if width <= 0 or height <= 0 or width % 2 or height % 2:
        raise ValueError("golden CRC currently requires positive even YUV420 dimensions")
    command = [
        ffmpeg,
        "-hide_banner",
        "-v",
        "error",
        "-err_detect",
        "explode",
        "-i",
        str(path),
        "-map",
        "0:v:0",
        "-vsync",
        "0",
        "-pix_fmt",
        "yuv420p",
        "-f",
        "rawvideo",
        "pipe:1",
    ]
    # A deliberately inconsistent SPS diagnostic can emit enough decoder
    # warnings to fill an OS pipe while raw frames are still being consumed.
    # Spool stderr to a temporary file so golden generation cannot deadlock.
    with tempfile.TemporaryFile() as stderr_stream:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=stderr_stream,
        )
        assert process.stdout is not None
        y_size = width * height
        chroma_size = (width // 2) * (height // 2)
        frame_size = y_size + 2 * chroma_size
        frames: list[dict[str, Any]] = []
        aggregate = hashlib.sha256()
        while True:
            frame = read_exact(process.stdout, frame_size)
            if not frame:
                break
            if len(frame) != frame_size:
                process.kill()
                raise RuntimeError(
                    f"partial decoded frame: expected {frame_size} bytes, got {len(frame)}"
                )
            aggregate.update(frame)
            y_plane = frame[:y_size]
            u_plane = frame[y_size : y_size + chroma_size]
            v_plane = frame[y_size + chroma_size :]
            frames.append(
                {
                    "index": len(frames),
                    "crc32": f"{zlib.crc32(frame) & 0xFFFFFFFF:08X}",
                    "y_crc32": f"{zlib.crc32(y_plane) & 0xFFFFFFFF:08X}",
                    "u_crc32": f"{zlib.crc32(u_plane) & 0xFFFFFFFF:08X}",
                    "v_crc32": f"{zlib.crc32(v_plane) & 0xFFFFFFFF:08X}",
                }
            )
        return_code = process.wait()
        stderr_stream.seek(0)
        stderr = stderr_stream.read().decode("utf-8", errors="replace")
    if return_code != 0:
        raise RuntimeError(f"FFmpeg decode failed ({return_code}): {stderr.strip()}")
    if expected_frames is not None and len(frames) != expected_frames:
        raise RuntimeError(
            f"decoded frame count mismatch: expected {expected_frames}, got {len(frames)}"
        )
    return {
        "schema_version": 1,
        "file": path.name,
        "source_sha256": sha256_file(path),
        "pixel_format": "yuv420p",
        "width": width,
        "height": height,
        "frame_size": frame_size,
        "frame_count": len(frames),
        "aggregate_sha256": aggregate.hexdigest().upper(),
        "frames": frames,
    }


def parse_event_fields(parts: list[str]) -> dict[str, str]:
    fields: dict[str, str] = {}
    for item in parts:
        # SUMMARY fields are tab-separated, while stage detail fields emitted
        # by the Qt 4 probe are space-separated inside one TSV column.
        for token in item.split():
            if "=" not in token:
                continue
            key, value = token.split("=", 1)
            fields[key] = value
    return fields


def analyze_device_results(results_path: Path, matrix_path: Path) -> dict[str, Any]:
    events_path = results_path / "events.tsv"
    manifest_path = matrix_path / "manifest.json"
    if not events_path.is_file():
        raise ValueError(f"device events.tsv is missing: {events_path}")
    if not manifest_path.is_file():
        raise ValueError(f"matrix manifest.json is missing: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest_case_list = manifest.get("cases", [])
    manifest_cases = {case["id"]: case for case in manifest_case_list}
    if not manifest_cases or len(manifest_cases) != len(manifest_case_list):
        raise ValueError("matrix manifest has no cases or contains duplicate case IDs")
    case_order = manifest.get("case_order") or [case["id"] for case in manifest_case_list]
    if set(case_order) != set(manifest_cases) or len(case_order) != len(manifest_cases):
        raise ValueError("matrix case_order does not match its cases")
    fixed = manifest.get("fixed", {})
    width = int(fixed.get("width", 640))
    height = int(fixed.get("height", 360))
    if width <= 0 or height <= 0 or width % 2 or height % 2:
        raise ValueError("matrix has invalid YUV420 dimensions")

    summaries: dict[str, dict[str, str]] = {}
    picture_events: dict[str, list[dict[str, str]]] = {}
    for line_number, line in enumerate(
        events_path.read_text(encoding="utf-8", errors="replace").splitlines(), 1
    ):
        if line_number == 1 and line.startswith("utc\tcase\tevent\t"):
            continue
        parts = line.split("\t")
        if len(parts) < 4:
            continue
        case_id = parts[1]
        event = parts[2]
        fields = parse_event_fields(parts[3:])
        if event == "SUMMARY":
            summaries[case_id] = fields
        elif event == "PICTURE":
            picture_events.setdefault(case_id, []).append(fields)

    analyzed_cases: list[dict[str, Any]] = []
    for case_id in case_order:
        case = manifest_cases[case_id]
        admission = case.get("admission", {})
        admission_h264 = admission.get("h264", case["h264"])
        summary = summaries.get(case_id)
        result: dict[str, Any] = {
            "id": case_id,
            "legal_stream": bool(case.get("legal_stream", True)),
            "diagnostic_only": bool(case.get("diagnostic_only", False)),
            "expected_sha256": case["h264"]["sha256"],
            "expected_header_sha256": admission_h264["sha256"],
            "admission_split": admission_h264["sha256"].upper()
            != case["h264"]["sha256"].upper(),
            "summary_present": summary is not None,
            "sha256_match": False,
            "header_sha256_match": False,
            "probe_verdict": "NO_SUMMARY",
            "crc_status": "NOT_AVAILABLE",
            "picture_crc_status": "NOT_AVAILABLE",
            "picture_events_checked": 0,
            "picture_crc_matches": 0,
            "picture_crc_mismatches": [],
            "raw_crc_status": "NOT_AVAILABLE",
            "raw_frames_checked": 0,
            "raw_frame_matches": [],
        }
        if summary is None:
            analyzed_cases.append(result)
            continue
        result["sha256_match"] = (
            summary.get("sha", "").upper() == case["h264"]["sha256"].upper()
        )
        result["header_sha256_match"] = (
            summary.get("header_sha", summary.get("sha", "")).upper()
            == admission_h264["sha256"].upper()
        )
        result["header_refs"] = int(
            summary.get(
                "header_refs", str(admission.get("max_num_ref_frames", 0))
            )
        )
        result["probe_verdict"] = summary.get("verdict", "MISSING")
        result["stages"] = {
            key: summary.get(key, "MISSING")
            for key in (
                "select",
                "input",
                "header",
                "configure",
                "output_list",
                "output_set",
                "destination",
                "initialize",
                "first_picture",
                "fatal",
                "stream_end",
            )
        }
        result["access_units_written"] = int(summary.get("au_written", "0"))
        result["pictures_output"] = int(summary.get("pictures_output", "0"))
        result["lower_evidence"] = summary.get("lower_evidence", "MISSING")

        output_data = int(summary.get("output_data", "0"))
        output_pattern = int(summary.get("output_pattern", "0"))
        output_layout = int(summary.get("output_layout", "0"))
        first_bytes = int(summary.get("first_bytes", "0"))
        tight_yuv420_bytes = width * height * 3 // 2
        if result["pictures_output"] <= 0:
            result["crc_status"] = "NO_PICTURE"
            analyzed_cases.append(result)
            continue
        if not (
            output_data == 0x04000000
            and output_pattern in (1, 2, 4)
            and output_layout == 1
            and first_bytes == tight_yuv420_bytes
        ):
            result["crc_status"] = "UNSUPPORTED_OUTPUT_LAYOUT"
            analyzed_cases.append(result)
            continue

        golden_path = matrix_path / case["golden"]["h264_metadata"]
        golden = json.loads(golden_path.read_text(encoding="utf-8"))
        logged_pictures = picture_events.get(case_id, [])
        picture_mismatches: list[dict[str, Any]] = []
        picture_matches = 0
        for ordinal, event_fields in enumerate(logged_pictures):
            try:
                frame_index = int(event_fields.get("index", "-1"))
                frame_bytes = int(event_fields.get("bytes", "-1"))
            except ValueError:
                frame_index = -1
                frame_bytes = -1
            actual_crc = event_fields.get("crc", "").upper()
            expected_crc = (
                golden["frames"][frame_index]["crc32"].upper()
                if 0 <= frame_index < len(golden["frames"])
                else "OUT_OF_RANGE"
            )
            matches = (
                frame_index == ordinal
                and frame_bytes == tight_yuv420_bytes
                and actual_crc == expected_crc
            )
            if matches:
                picture_matches += 1
            else:
                picture_mismatches.append(
                    {
                        "ordinal": ordinal,
                        "index": frame_index,
                        "bytes": frame_bytes,
                        "actual_crc32": actual_crc,
                        "expected_crc32": expected_crc,
                    }
                )
        result["picture_events_checked"] = len(logged_pictures)
        result["picture_crc_matches"] = picture_matches
        result["picture_crc_mismatches"] = picture_mismatches
        if not logged_pictures:
            result["picture_crc_status"] = "EVENTS_MISSING"
        elif (
            len(logged_pictures) == result["pictures_output"]
            and picture_matches == len(logged_pictures)
        ):
            result["picture_crc_status"] = "MATCH"
        else:
            result["picture_crc_status"] = "MISMATCH"

        diagnostic_decode = case.get("pc_fake_decode")
        if diagnostic_decode and diagnostic_decode.get("metadata"):
            diagnostic_path = matrix_path / diagnostic_decode["metadata"]
            diagnostic_golden = json.loads(diagnostic_path.read_text(encoding="utf-8"))
            diagnostic_matches = 0
            diagnostic_mismatches: list[dict[str, Any]] = []
            for ordinal, event_fields in enumerate(logged_pictures):
                try:
                    frame_index = int(event_fields.get("index", "-1"))
                except ValueError:
                    frame_index = -1
                actual_crc = event_fields.get("crc", "").upper()
                expected_crc = (
                    diagnostic_golden["frames"][frame_index]["crc32"].upper()
                    if 0 <= frame_index < len(diagnostic_golden["frames"])
                    else "OUT_OF_RANGE"
                )
                if frame_index == ordinal and actual_crc == expected_crc:
                    diagnostic_matches += 1
                else:
                    diagnostic_mismatches.append(
                        {
                            "ordinal": ordinal,
                            "index": frame_index,
                            "actual_crc32": actual_crc,
                            "expected_crc32": expected_crc,
                        }
                    )
            result["pc_diagnostic_crc_matches"] = diagnostic_matches
            result["pc_diagnostic_crc_mismatches"] = diagnostic_mismatches
            result["pc_diagnostic_crc_status"] = (
                "MATCH"
                if logged_pictures and diagnostic_matches == len(logged_pictures)
                else "MISMATCH"
            )

        checks: list[dict[str, Any]] = []
        for frame_index in range(3):
            raw_path = results_path / f"{case_id}-frame{frame_index:03d}.raw"
            if not raw_path.is_file():
                continue
            raw = raw_path.read_bytes()
            actual_crc = f"{zlib.crc32(raw) & 0xFFFFFFFF:08X}"
            expected_crc = golden["frames"][frame_index]["crc32"]
            checks.append(
                {
                    "index": frame_index,
                    "file": raw_path.name,
                    "size": len(raw),
                    "actual_crc32": actual_crc,
                    "expected_crc32": expected_crc,
                    "match": len(raw) == tight_yuv420_bytes
                    and actual_crc == expected_crc,
                }
            )
        result["raw_frames_checked"] = len(checks)
        result["raw_frame_matches"] = checks
        if not checks:
            result["raw_crc_status"] = "FRAME_DUMPS_MISSING"
        elif all(check["match"] for check in checks):
            result["raw_crc_status"] = "MATCH"
        else:
            result["raw_crc_status"] = "MISMATCH"

        if (
            result["picture_crc_status"] == "MATCH"
            and result["raw_crc_status"] == "MATCH"
        ):
            result["crc_status"] = "MATCH"
        elif (
            result["picture_crc_status"] == "MISMATCH"
            or result["raw_crc_status"] == "MISMATCH"
        ):
            result["crc_status"] = "CORRUPT_OR_LAYOUT_MISMATCH"
        elif result["picture_crc_status"] == "MATCH":
            result["crc_status"] = "MATCH_EVENT_LOG_ONLY"
        elif result["raw_crc_status"] == "MATCH":
            result["crc_status"] = "MATCH_RAW_DUMPS_ONLY"
        else:
            result["crc_status"] = "CRC_EVIDENCE_MISSING"
        analyzed_cases.append(result)

    research_outcome = "UNCLASSIFIED"
    analyzed_by_id = {case["id"]: case for case in analyzed_cases}
    if manifest.get("matrix") == "H264_HEADER_SUBMIT_SPLIT_R4":
        native_r7 = analyzed_by_id.get("R7_NATIVE", {})
        split_r7 = analyzed_by_id.get("FAKE_HEADER_ORIGINAL_R7", {})
        fake_control = analyzed_by_id.get("FAKE_HEADER_FAKE_REF3", {})
        native_correct = (
            native_r7.get("sha256_match")
            and native_r7.get("header_sha256_match")
            and not native_r7.get("admission_split")
            and native_r7.get("stages", {}).get("header") == "OK"
            and native_r7.get("stages", {}).get("configure") == "OK"
            and native_r7.get("stages", {}).get("initialize") == "OK"
            and native_r7.get("access_units_written")
            == int(manifest_cases.get("R7_NATIVE", {}).get("access_units", 0))
            and native_r7.get("pictures_output", 0) > 0
            and native_r7.get("crc_status") == "MATCH"
        )
        if native_correct:
            native_r7["analysis_verdict"] = "ORIGINAL_R7_DIRECT_DECODE_CORRECT"
        elif native_r7.get("stages", {}).get("header") == "-5":
            native_r7["analysis_verdict"] = "ORIGINAL_R7_HEADER_REJECT_CONFIRMED"
        split_correct = (
            split_r7.get("sha256_match")
            and split_r7.get("header_sha256_match")
            and split_r7.get("admission_split")
            and split_r7.get("stages", {}).get("header") == "OK"
            and split_r7.get("stages", {}).get("configure") == "OK"
            and split_r7.get("stages", {}).get("initialize") == "OK"
            and split_r7.get("access_units_written")
            == int(manifest_cases.get("FAKE_HEADER_ORIGINAL_R7", {}).get("access_units", 0))
            and split_r7.get("pictures_output", 0) > 0
            and split_r7.get("crc_status") == "MATCH"
        )
        fake_corrupt = (
            fake_control.get("pictures_output", 0) > 0
            and fake_control.get("crc_status") == "CORRUPT_OR_LAYOUT_MISMATCH"
        )
        if split_correct:
            split_r7["analysis_verdict"] = (
                "ORIGINAL_R7_DECODE_CORRECT_AFTER_HEADER_SPLIT"
            )
            research_outcome = "HOST_ADMISSION_GATE_ISOLATED_R7_DECODE_CORRECT"
        if native_correct:
            research_outcome = "ORIGINAL_R7_DIRECT_DECODE_CORRECT"
        if fake_corrupt:
            fake_control["analysis_verdict"] = (
                "ALL_SPS_FAKE_CORRUPTION_CONTROL_CONFIRMED"
            )

    return {
        "schema_version": 1,
        "events_file": str(events_path),
        "events_sha256": sha256_file(events_path),
        "matrix_manifest_sha256": sha256_file(manifest_path),
        "matrix": manifest.get("matrix", "UNKNOWN"),
        "summary_case_count": len(summaries),
        "complete": len(summaries) == len(manifest_cases),
        "cases": analyzed_cases,
        "research_outcome": research_outcome,
    }


def write_json(result: dict[str, Any], output: str | None) -> None:
    text = json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    if output:
        Path(output).write_text(text, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(text)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    inspect_parser = subparsers.add_parser("inspect", help="inspect an Annex-B H.264 stream")
    inspect_parser.add_argument("input")
    inspect_parser.add_argument("--output")

    golden_parser = subparsers.add_parser(
        "golden", help="decode a stream and emit per-frame YUV420 CRC32 values"
    )
    golden_parser.add_argument("input")
    golden_parser.add_argument("--ffmpeg", default="ffmpeg")
    golden_parser.add_argument("--width", type=int, required=True)
    golden_parser.add_argument("--height", type=int, required=True)
    golden_parser.add_argument("--expected-frames", type=int)
    golden_parser.add_argument("--output")

    device_parser = subparsers.add_parser(
        "device", help="validate a returned Nokia 603 probe result directory"
    )
    device_parser.add_argument("results")
    device_parser.add_argument("--matrix", required=True)
    device_parser.add_argument("--output")

    rewrite_parser = subparsers.add_parser(
        "rewrite-sps-dpb",
        help="legally raise VUI max_dec_frame_buffering in every SPS",
    )
    rewrite_parser.add_argument("input")
    rewrite_parser.add_argument("destination")
    rewrite_parser.add_argument("--value", type=int, required=True)
    rewrite_parser.add_argument("--report")

    rewrite_refs_parser = subparsers.add_parser(
        "rewrite-sps-refs",
        help="diagnostically rewrite max_num_ref_frames in every SPS",
    )
    rewrite_refs_parser.add_argument("input")
    rewrite_refs_parser.add_argument("destination")
    rewrite_refs_parser.add_argument("--value", type=int, required=True)
    rewrite_refs_parser.add_argument("--report")

    eos_parser = subparsers.add_parser(
        "append-eos", help="append one H.264 end_of_stream NAL unit"
    )
    eos_parser.add_argument("input")
    eos_parser.add_argument("destination")
    eos_parser.add_argument("--report")

    args = parser.parse_args()
    try:
        if args.command == "inspect":
            result = inspect_annex_b(Path(args.input).resolve())
        elif args.command == "golden":
            result = build_golden_crc(
                args.ffmpeg,
                Path(args.input).resolve(),
                args.width,
                args.height,
                args.expected_frames,
            )
        elif args.command == "device":
            result = analyze_device_results(
                Path(args.results).resolve(), Path(args.matrix).resolve()
            )
        elif args.command == "rewrite-sps-dpb":
            result = rewrite_annex_b_sps_dpb(
                Path(args.input).resolve(), Path(args.destination).resolve(), args.value
            )
        elif args.command == "rewrite-sps-refs":
            result = rewrite_annex_b_sps_refs(
                Path(args.input).resolve(), Path(args.destination).resolve(), args.value
            )
        else:
            result = append_h264_eos(
                Path(args.input).resolve(), Path(args.destination).resolve()
            )
        output = getattr(args, "report", None) or getattr(args, "output", None)
        write_json(result, output)
        return 0
    except (H264ParseError, OSError, RuntimeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
