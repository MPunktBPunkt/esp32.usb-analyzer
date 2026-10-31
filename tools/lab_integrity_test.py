#!/usr/bin/env python3
"""Gate 3: ESP↔ESP TCP integrity (byte-identical frames). Not USB MITM."""
from __future__ import annotations

import argparse
import json
import sys
import urllib.parse
import urllib.request


def run_integrity(ip: str, peer: str, frames: int, frame_bytes: int) -> dict:
    q = urllib.parse.urlencode(
        {"peer": peer, "frames": frames, "frame_bytes": frame_bytes}
    )
    url = f"http://{ip}/api/lab/integrity?{q}"
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=180) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", default="192.168.178.88", help="initiator IP")
    ap.add_argument("--b", default="192.168.178.89", help="peer IP")
    ap.add_argument("--frames", type=int, default=64)
    ap.add_argument("--frame-bytes", type=int, default=1024)
    ap.add_argument("--both-ways", action="store_true")
    args = ap.parse_args()

    print(f"==> {args.a} → {args.b}  frames={args.frames} size={args.frame_bytes}")
    try:
        r = run_integrity(args.a, args.b, args.frames, args.frame_bytes)
    except Exception as e:
        print("FAIL", e)
        return 1
    print(json.dumps(r, indent=2))
    ok = bool(r.get("ok"))
    if args.both_ways:
        print(f"==> {args.b} → {args.a}")
        try:
            r2 = run_integrity(args.b, args.a, args.frames, args.frame_bytes)
        except Exception as e:
            print("FAIL", e)
            return 1
        print(json.dumps(r2, indent=2))
        ok = ok and bool(r2.get("ok"))
    print("RESULT", "OK" if ok else "FAIL")
    return 0 if ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
