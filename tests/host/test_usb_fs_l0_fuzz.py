#!/usr/bin/env python3
"""Property-style fuzz for usb_fs_l0 decoder / sample_io / correlator.

Stdlib-only (no hypothesis dep). Claim: usb_fs_l0_fuzz_lab — not live wire.
"""
from __future__ import annotations

import random
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.correlate import correlate  # noqa: E402
from usb_fs_l0.decoder import (  # noqa: E402
    build_data,
    build_handshake,
    build_token,
    decode_nrzi_bits,
    decode_packet_bytes,
    packet_bytes_to_nrzi_states,
)
from usb_fs_l0.gen_synth import synth_get_descriptor_states  # noqa: E402
from usb_fs_l0.nrzi import J, SE0  # noqa: E402
from usb_fs_l0.sample_io import (  # noqa: E402
    MAGIC,
    write_line_states,
    line_states_from_file,
    read_l0samp,
)


SEED = 0x10F0
N = 80


class TestDecoderFuzz(unittest.TestCase):
    def setUp(self) -> None:
        self.rng = random.Random(SEED)

    def test_random_bytes_never_crash(self) -> None:
        for _ in range(N):
            n = self.rng.randint(0, 64)
            raw = bytes(self.rng.getrandbits(8) for _ in range(n))
            d = decode_packet_bytes(raw)
            self.assertIn("pid", d)
            self.assertIn("raw_valid" in d or "crc_ok" in d or "note" in d, (True,))

    def test_truncated_valid_packets(self) -> None:
        tok = build_token(0x2D, 1, 0)
        data = build_data(0xC3, bytes(range(8)))
        ack = build_handshake(0xD2)
        for pkt in (tok, data, ack):
            for cut in range(len(pkt)):
                d = decode_packet_bytes(pkt[:cut])
                self.assertIsInstance(d, dict)

    def test_bad_crc_flagged(self) -> None:
        data = bytearray(build_data(0xC3, bytes([1, 2, 3, 4])))
        data[-1] ^= 0xFF
        d = decode_packet_bytes(bytes(data))
        self.assertEqual(d["pid"], "DATA0")
        self.assertFalse(d["crc_ok"])

    def test_illegal_pid_complement(self) -> None:
        d = decode_packet_bytes(bytes([0x01]))  # not a valid PID complement
        self.assertFalse(d.get("raw_valid", True) is True and d.get("note") == "")
        self.assertIn("bad_pid", d.get("note", "") + d.get("pid", ""))

    def test_nrzi_noise_and_double_eop(self) -> None:
        for _ in range(N // 2):
            states = [self.rng.choice([0, 1, 2, 3]) for _ in range(self.rng.randint(0, 200))]
            # sprinkle double SE0
            if states:
                i = self.rng.randrange(len(states))
                states[i:i] = [SE0, SE0, SE0]
            pkts = decode_nrzi_bits(states)
            self.assertIsInstance(pkts, list)

    def test_missing_eop_still_safe(self) -> None:
        pkt = build_token(0x2D, 3, 1)
        states = packet_bytes_to_nrzi_states(pkt, idle=J)
        # strip trailing SE0/idle
        while states and states[-1] in (J, SE0):
            states.pop()
        pkts = decode_nrzi_bits(states)
        self.assertIsInstance(pkts, list)

    def test_roundtrip_fuzz_tokens(self) -> None:
        for _ in range(40):
            addr = self.rng.randint(0, 127)
            endp = self.rng.randint(0, 15)
            pid = self.rng.choice([0x2D, 0x69, 0xE1])
            pkt = build_token(pid, addr, endp)
            states = packet_bytes_to_nrzi_states(pkt)
            # pad idle
            states = [J] * 4 + states + [J] * 4
            pkts = decode_nrzi_bits(states)
            self.assertGreaterEqual(len(pkts), 1)
            self.assertTrue(pkts[0]["crc_ok"])


class TestSampleIoFuzz(unittest.TestCase):
    def setUp(self) -> None:
        self.rng = random.Random(SEED + 1)

    def test_garbage_l0samp_raises(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "bad.l0samp"
            for blob in (b"", b"XXXX" + b"\x00" * 20, MAGIC + b"\x00" * 3):
                p.write_bytes(blob)
                with self.assertRaises(Exception):
                    read_l0samp(p)
            # valid header, unsupported fmt → line_states_from_file rejects
            p.write_bytes(
                MAGIC
                + struct.pack("<III", 1, 12_000_000, 99)
                + struct.pack("<I", 4)
                + b"abcd"
            )
            with self.assertRaises(ValueError):
                line_states_from_file(p)

    def test_partial_payload_truncation(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "trunc.l0samp"
            # claim 100 bytes but write 2
            header = struct.pack("<4sIII", MAGIC, 1, 12_000_000, 1)
            p.write_bytes(header + struct.pack("<I", 100) + b"\x01\x02")
            rate, fmt, payload = read_l0samp(p)
            self.assertEqual(rate, 12_000_000)
            self.assertEqual(len(payload), 2)  # truncated read is bounded

    def test_roundtrip_random_states(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "rnd.l0samp"
            states = [self.rng.randint(0, 3) for _ in range(self.rng.randint(1, 500))]
            write_line_states(p, 12_000_000, states)
            rate, got = line_states_from_file(p)
            self.assertEqual(rate, 12_000_000)
            self.assertEqual(got, states)


class TestCorrelateFuzz(unittest.TestCase):
    def test_empty_and_noise_l1(self) -> None:
        l0 = decode_nrzi_bits(synth_get_descriptor_states())
        r = correlate(l0, [], ignore_time=True)
        self.assertEqual(r["matched"], 0)
        self.assertGreater(r["stack_gap_n"], 0)
        noise = [{"type": f"x{i}", "timestamp_us": i, "note": "noise"} for i in range(20)]
        r2 = correlate(l0, noise, ignore_time=True)
        self.assertIsInstance(r2["unmatched_l0"], list)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
