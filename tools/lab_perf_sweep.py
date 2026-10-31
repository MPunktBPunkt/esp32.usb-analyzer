#!/usr/bin/env python3
"""Gate 3/4 performance sweep — ESP↔ESP TCP bulk (not USB)."""
from __future__ import annotations

import argparse
import json
import urllib.parse
import urllib.request


def run(ip: str, peer: str, frames: int, frame_bytes: int, window: int) -> dict:
    q = urllib.parse.urlencode(
        {
            "peer": peer,
            "frames": frames,
            "frame_bytes": frame_bytes,
            "window": window,
        }
    )
    req = urllib.request.Request(
        f"http://{ip}/api/lab/integrity?{q}", method="POST", data=b""
    )
    with urllib.request.urlopen(req, timeout=300) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", default="192.168.178.88")
    ap.add_argument("--b", default="192.168.178.89")
    ap.add_argument("--frames", type=int, default=128)
    args = ap.parse_args()

    print(f"sweep {args.a} → {args.b} frames={args.frames}")
    print(f"{'size':>6} {'win':>4} {'ok':>4} {'ms':>8} {'kB/s':>8} {'acked':>6}")
    for sz in (1024, 2048, 4096):
        for win in (1, 8, 16, 32):
            try:
                r = run(args.a, args.b, args.frames, sz, win)
            except Exception as e:
                print(f"{sz:>6} {win:>4} ERR {e}")
                continue
            print(
                f"{sz:>6} {win:>4} {str(r.get('ok')):>4} "
                f"{r.get('duration_ms', 0):>8} {r.get('rate_kBs', 0):>8} "
                f"{r.get('frames_acked', 0):>6}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
