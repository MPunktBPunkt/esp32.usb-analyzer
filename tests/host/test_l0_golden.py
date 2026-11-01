#!/usr/bin/env python3
"""Golden .l0samp regression — decode must match pinned expected JSON.

Claim: l0_golden_regress_lab — synth fixtures, not live Path-B PHY.
"""
from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.decoder import (  # noqa: E402
    build_data,
    build_handshake,
    build_token,
    decode_nrzi_bits,
    packet_bytes_to_nrzi_states,
)
from usb_fs_l0.gen_synth import synth_get_descriptor_states  # noqa: E402
from usb_fs_l0.nrzi import J  # noqa: E402
from usb_fs_l0.sample_io import line_states_from_file, write_line_states  # noqa: E402

FIX = ROOT / "tests/host/fixtures"


def _summary(pkts: list[dict]) -> list[dict]:
    out = []
    for p in pkts:
        out.append(
            {
                "pid": p.get("pid"),
                "crc_ok": p.get("crc_ok"),
                "addr": p.get("addr"),
                "endp": p.get("endp"),
                "payload_len": p.get("payload_len")
                if p.get("payload_len") is not None
                else len(p.get("payload") or b""),
                "note": p.get("note"),
            }
        )
    return out


def ensure_fixtures() -> None:
    FIX.mkdir(parents=True, exist_ok=True)

    # 1) happy SETUP+DATA0+ACK
    p1 = FIX / "synth_setup.l0samp"
    e1 = FIX / "synth_setup.expected.json"
    st = synth_get_descriptor_states()
    write_line_states(p1, 12_000_000, st)
    e1.write_text(
        json.dumps({"packets": _summary(decode_nrzi_bits(st))}, indent=2) + "\n",
        encoding="utf-8",
    )

    # 2) bad DATA CRC
    p2 = FIX / "data_bad_crc.l0samp"
    e2 = FIX / "data_bad_crc.expected.json"
    data = bytearray(build_data(0xC3, bytes([0x80, 0x06, 0, 1, 0, 0, 0x12, 0])))
    data[-1] ^= 0xFF
    st2 = packet_bytes_to_nrzi_states(bytes(data), idle=J)
    st2 = [J] * 8 + st2 + [J] * 8
    write_line_states(p2, 12_000_000, st2)
    e2.write_text(
        json.dumps({"packets": _summary(decode_nrzi_bits(st2))}, indent=2) + "\n",
        encoding="utf-8",
    )

    # 3) glitch: SE0 noise between packets
    p3 = FIX / "glitch_se0.l0samp"
    e3 = FIX / "glitch_se0.expected.json"
    tok = packet_bytes_to_nrzi_states(build_token(0x2D, 1, 0))
    ack = packet_bytes_to_nrzi_states(build_handshake(0xD2))
    st3 = [J] * 4 + tok + [0, 0, 0, 0] + [J] * 4 + ack + [J] * 4
    write_line_states(p3, 12_000_000, st3)
    e3.write_text(
        json.dumps({"packets": _summary(decode_nrzi_bits(st3))}, indent=2) + "\n",
        encoding="utf-8",
    )


class TestL0Golden(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        ensure_fixtures()

    def test_all_goldens(self) -> None:
        pairs = list(FIX.glob("*.expected.json"))
        self.assertGreaterEqual(len(pairs), 3)
        for exp_path in pairs:
            samp = exp_path.with_suffix("").with_suffix(".l0samp")
            # synth_setup.expected.json → stem handling
            if not samp.is_file():
                samp = FIX / (exp_path.name.replace(".expected.json", ".l0samp"))
            self.assertTrue(samp.is_file(), msg=str(samp))
            rate, states = line_states_from_file(samp)
            self.assertEqual(rate, 12_000_000)
            got = _summary(decode_nrzi_bits(states))
            exp = json.loads(exp_path.read_text(encoding="utf-8"))["packets"]
            self.assertEqual(got, exp, msg=f"drift in {samp.name}")


if __name__ == "__main__":
    ensure_fixtures()
    raise SystemExit(unittest.main(verbosity=2))
