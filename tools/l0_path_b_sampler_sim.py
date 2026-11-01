#!/usr/bin/env python3
"""Path B sampler simulator — pushes synth FS SETUP/DATA/ACK line-states to ingest.

Stands in for unsoldered front-end so the ingest→decode pipe is MEASURED end-to-end.
Claim: path_b_sampler_sim — NOT live D+/D− PHY.
"""
from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.gen_synth import synth_get_descriptor_states  # noqa: E402

MAGIC = 0x4C305354


def send_frame(sock: socket.socket, typ: int, rate: int, seq: int, payload: bytes) -> None:
    hdr = struct.pack("<IHHIII", MAGIC, 1, typ, rate, seq, len(payload))
    sock.sendall(hdr + payload)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="127.0.0.1:9760")
    ap.add_argument("--rate", type=int, default=12_000_000)
    args = ap.parse_args()
    host, port_s = args.target.rsplit(":", 1)
    states = synth_get_descriptor_states()
    payload = bytes(s & 0xFF for s in states)
    with socket.create_connection((host, int(port_s)), timeout=5) as s:
        hello = json.dumps({"sampler": "sim", "note": "synth_setup_data_ack"}).encode()
        send_frame(s, 3, args.rate, 0, hello)
        # chunk then end-of-burst
        send_frame(s, 1, args.rate, 1, payload)
        send_frame(s, 2, args.rate, 1, b"")
    print(f"OK pushed {len(states)} line-states → {args.target}")
    print("claim: path_b_sampler_sim (not live PHY)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
