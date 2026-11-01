#!/usr/bin/env python3
"""Host unit tests — USB FS L0 offline decoder + correlator (Gate 10B–F)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.correlate import correlate  # noqa: E402
from usb_fs_l0.crc import crc16_usb, token_crc5_ok  # noqa: E402
from usb_fs_l0.decoder import (  # noqa: E402
    build_data,
    build_token,
    decode_nrzi_bits,
    decode_packet_bytes,
)
from usb_fs_l0.gen_synth import synth_get_descriptor_states  # noqa: E402
from usb_fs_l0.sample_io import line_states_from_file, write_line_states  # noqa: E402


class TestCrc(unittest.TestCase):
    def test_token_crc5_roundtrip(self) -> None:
        pkt = build_token(0x2D, 5, 0)
        d = decode_packet_bytes(pkt)
        self.assertEqual(d["pid"], "SETUP")
        self.assertEqual(d["addr"], 5)
        self.assertEqual(d["endp"], 0)
        self.assertTrue(d["crc_ok"])
        self.assertTrue(token_crc5_ok(5, 0, d["crc_value"]))

    def test_data_crc16(self) -> None:
        payload = bytes([0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00])
        pkt = build_data(0xC3, payload)
        d = decode_packet_bytes(pkt)
        self.assertEqual(d["pid"], "DATA0")
        self.assertTrue(d["crc_ok"])
        self.assertEqual(d["payload_len"], 8)
        self.assertEqual(crc16_usb(payload), d["crc_value"])


class TestNrziRoundtrip(unittest.TestCase):
    def test_synth_setup_data_ack(self) -> None:
        states = synth_get_descriptor_states()
        pkts = decode_nrzi_bits(states)
        names = [p["pid"] for p in pkts]
        self.assertEqual(names[:3], ["SETUP", "DATA0", "ACK"])
        self.assertTrue(pkts[0]["crc_ok"])
        self.assertTrue(pkts[1]["crc_ok"])
        self.assertEqual(pkts[0]["addr"], 1)
        self.assertEqual(pkts[1]["payload_len"], 8)


class TestSampleIo(unittest.TestCase):
    def test_write_read(self) -> None:
        path = ROOT / "tests/host/fixtures/synth_setup.l0samp"
        path.parent.mkdir(parents=True, exist_ok=True)
        states = synth_get_descriptor_states()
        write_line_states(path, 12_000_000, states)
        rate, got = line_states_from_file(path)
        self.assertEqual(rate, 12_000_000)
        self.assertEqual(got, states)


class TestCorrelate(unittest.TestCase):
    def test_synth_l0_to_l1(self) -> None:
        states = synth_get_descriptor_states()
        l0 = decode_nrzi_bits(states)
        l1 = [
            {"type": "enum_info", "timestamp_us": 0, "note": "get_descriptor"},
            {"type": "enum_info", "timestamp_us": 1, "note": "ep0"},
            {"type": "bot_cbw", "timestamp_us": 99, "note": "unrelated"},
        ]
        r = correlate(l0, l1, window_us=10)
        self.assertGreaterEqual(r["matched"], 1)
        kinds = [m["kind"] for m in r["matches"]]
        self.assertIn("ep0", kinds)
        # Real ESP timestamps vs ordinal L0 → need ignore_time
        l1_real = [
            {"type": "enum_info", "timestamp_us": 9_000_000, "note": "get_descriptor"},
            {"type": "enum_info", "timestamp_us": 9_000_100, "note": "ep0"},
            {"type": "enum_info", "timestamp_us": 9_000_200, "note": "status"},
        ]
        r2 = correlate(l0, l1_real, ignore_time=True)
        assert "unmatched_l0" in r2 and "stack_gap_n" in r2
        self.assertGreaterEqual(r2["matched"], 1)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
