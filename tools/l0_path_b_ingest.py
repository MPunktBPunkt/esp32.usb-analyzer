#!/usr/bin/env python3
"""Path B — TCP ingest for FS line-state samples → .l0samp (+ optional decode).

Claim: path_b_ingest_lab — accepts external/sim sampler; NOT PHY solder MEASURED.
"""
from __future__ import annotations

import argparse
import json
import socket
import struct
import threading
import time
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.decoder import decode_nrzi_bits  # noqa: E402
from usb_fs_l0.sample_io import write_line_states  # noqa: E402
from usb_fs_l0.schema import batch_dict  # noqa: E402

MAGIC = 0x4C305354  # L0ST
TYPE_CHUNK = 1
TYPE_EOB = 2
TYPE_HELLO = 3


def recv_exact(conn: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("eof")
        buf += chunk
    return buf


def handle_client(conn: socket.socket, addr, out_dir: Path, decode: bool) -> None:
    print(f"[ingest] connect {addr}", flush=True)
    states: list[int] = []
    rate = 12_000_000
    try:
        while True:
            hdr = recv_exact(conn, 20)
            magic, ver, typ, rate_hz, seq, nbytes = struct.unpack("<IHHIII", hdr)
            if magic != MAGIC or ver != 1:
                print("[ingest] bad header", flush=True)
                break
            payload = recv_exact(conn, nbytes) if nbytes else b""
            if typ == TYPE_HELLO:
                print("[ingest] hello", payload[:200], flush=True)
                continue
            if typ == TYPE_CHUNK:
                rate = rate_hz or rate
                states.extend(payload)
                continue
            if typ == TYPE_EOB:
                rate = rate_hz or rate
                ts = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
                samp = out_dir / f"pathb_{ts}_seq{seq}.l0samp"
                write_line_states(samp, rate, states)
                print(f"[ingest] wrote {samp} samples={len(states)}", flush=True)
                if decode and states:
                    pkts = decode_nrzi_bits(states)
                    jpath = samp.with_suffix(".packets.json")
                    jpath.write_text(json.dumps(batch_dict(pkts), indent=2))
                    print(f"[ingest] decoded packets={len(pkts)} → {jpath}", flush=True)
                # latest symlink-ish copy
                latest = out_dir / "latest.l0samp"
                write_line_states(latest, rate, states)
                states = []
                continue
            print(f"[ingest] unknown type {typ}", flush=True)
    except ConnectionError:
        print(f"[ingest] disconnect {addr}", flush=True)
    finally:
        conn.close()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--listen", default="0.0.0.0:9760")
    ap.add_argument("--out", default=str(ROOT / "logs" / "l0"))
    ap.add_argument("--decode", action="store_true")
    args = ap.parse_args()
    host, port_s = args.listen.rsplit(":", 1)
    port = int(port_s)
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((host, port))
    srv.listen(4)
    print(f"[ingest] Path B listening on {host}:{port} out={out_dir}", flush=True)
    while True:
        conn, addr = srv.accept()
        threading.Thread(
            target=handle_client, args=(conn, addr, out_dir, args.decode), daemon=True
        ).start()


if __name__ == "__main__":
    raise SystemExit(main())
