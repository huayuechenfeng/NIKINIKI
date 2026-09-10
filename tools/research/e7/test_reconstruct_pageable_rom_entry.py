import hashlib
import importlib.util
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("reconstruct_pageable_rom_entry.py")
SPEC = importlib.util.spec_from_file_location("reconstruct_pageable_rom_entry", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ReconstructPageableRomEntryTests(unittest.TestCase):
    def test_e7_decoder_candidate_matches_phone_copy(self) -> None:
        root = Path(__file__).resolve().parents[3]
        rom = (
            root / ".tmp" / "e7-mmf-runtime" / "ive-policy"
            / "e7-fpsx-sparse.img"
        )
        phone = (
            root / ".tmp" / "e7-mmf-runtime" / "ive"
            / "ivevideodecodehwdevice.dll"
        )
        if not rom.exists() or not phone.exists():
            self.skipTest("private E7 ROM/phone evidence is unavailable")
        rebuilt, report = MODULE.reconstruct(rom, 0x80D25500, 54100)
        self.assertEqual(rebuilt, phone.read_bytes())
        self.assertEqual(
            report["output_sha256"],
            "77E24AA6DD3382D07F2802159415B9F3B70D96790E8C70F637E75342DF6B3A87",
        )
        self.assertEqual(len(rebuilt), 54100)

    def test_bytepair_empty_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "empty"):
            MODULE.bytepair_decode(b"")


if __name__ == "__main__":
    unittest.main()
