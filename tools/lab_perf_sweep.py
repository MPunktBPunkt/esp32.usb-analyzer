#!/usr/bin/env python3
"""Gate 3/4 performance sweep — ESP↔ESP TCP bulk (not USB).

Compares ack-each / windowed barrier / sliding-window pipeline.
Claim: tcp_wifi_esp_esp throughput — NOT USB MITM / L0.
"""
from __future__ import annotations

import argparse
import json
import urllib.parse
import urllib.request


def run(
    ip: str,
    peer: str,
    frames: int,
    frame_bytes: int,
    window: int,
    pipeline: bool,
) -> dict:
    q = urllib.parse.urlencode(
        {
            "peer": peer,
            "frames": frames,
            "frame_bytes": frame_bytes,
            "window": window,
            "pipeline": 1 if pipeline else 0,
        }
    )
    req = urllib.request.Request(
        f"http://{ip}/api/lab/integrity?{q}", method="POST", data=b""
    )
    with urllib.request.urlopen(req, timeout=180) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", default="192.168.178.88")
    ap.add_argument("--b", default="192.168.178.89")
    ap.add_argument("--frames", type=int, default=64)
    ap.add_argument("--quick", action="store_true",
                    help="only sweet-spot sizes/windows")
    args = ap.parse_args()

    print(f"sweep {args.a} → {args.b} frames={args.frames}")
    print(f"{'size':>6} {'win':>4} {'pipe':>4} {'ok':>4} {'ms':>8} "
          f"{'kB/s':>8} {'acked':>6} {'mode':>10}")

    if args.quick:
        cases = [
            (2048, 1, False),
            (2048, 8, False),
            (2048, 8, True),
            (4096, 8, False),
            (4096, 8, True),
            (4096, 12, True),
            (4096, 16, True),
        ]
    else:
        cases = []
        for sz in (1024, 2048, 4096):
            for win in (1, 8, 16, 32):
                cases.append((sz, win, False))
                if win > 1:
                    cases.append((sz, win, True))

    for sz, win, pipe in cases:
        try:
            r = run(args.a, args.b, args.frames, sz, win, pipe)
        except Exception as e:
            print(f"{sz:>6} {win:>4} {int(pipe):>4} ERR {e}")
            continue
        print(
            f"{sz:>6} {win:>4} {int(pipe):>4} {str(r.get('ok')):>4} "
            f"{r.get('duration_ms', 0):>8} {r.get('rate_kBs', 0):>8} "
            f"{r.get('frames_acked', 0):>6} {str(r.get('mode') or ''):>10}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
