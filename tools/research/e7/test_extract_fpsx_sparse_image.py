import importlib.util
import sys
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("extract_fpsx_sparse_image.py")
SPEC = importlib.util.spec_from_file_location("extract_fpsx_sparse_image", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ExtractFpsxSparseImageTests(unittest.TestCase):
    def test_tiny_binary_block(self) -> None:
        primary = b"\xB2" + (0).to_bytes(4, "big")
        header = (
            b"\x00" + b"\x00\x00" + b"\x00\x00" + b"\x00"
            + (3).to_bytes(4, "big") + (2).to_bytes(4, "big")
        )
        fpsx = primary + b"\x54\x01\x17" + bytes([len(header)]) + header + b"\xADabc"
        blocks = MODULE.parse_blocks(fpsx)
        self.assertEqual(len(blocks), 1)
        self.assertEqual(blocks[0].payload_size, 3)
        self.assertEqual(blocks[0].target_offset, 2)


if __name__ == "__main__":
    unittest.main()
