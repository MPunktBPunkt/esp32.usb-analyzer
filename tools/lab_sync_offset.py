#!/usr/bin/env python3
"""Gate-8: cross-node sync marker offset estimate (WiFi only — no stick).

Hits both ESPs /api/lab/sync-marker (or controller /api/v1/sync) and reports
median offset_us ± residual. Claim: cross_node_offset_estimate — NOT hard µs lock.
"""
from __future__ import annotations

import argparse
import json
import statistics
import time
import urllib.parse
import urllib.request


def post(url: str) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=8) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--controller", default="",
                    help="If set, POST /api/v1/sync on controller instead")
    ap.add_argument("--samples", type=int, default=7)
    args = ap.parse_args()
    n = max(1, min(args.samples, 21))

    if args.controller:
        body = json.dumps({"samples": n}).encode()
        req = urllib.request.Request(
            f"http://{args.controller}/api/v1/sync",
            data=body,
            method="POST",
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=60) as r:
            j = json.loads(r.read().decode())
        print(json.dumps(j, indent=2))
        return 0 if j.get("ok") else 1

    base = int(time.time_ns() & 0x7FFFFFFF)
    offsets = []
    for i in range(n):
        nonce = base + i
        q = urllib.parse.urlencode({"nonce": nonce})
        h = post(f"http://{args.host}/api/lab/sync-marker?{q}")
        d = post(f"http://{args.device}/api/lab/sync-marker?{q}")
        if not (h.get("ok") and d.get("ok")):
            print("FAIL marker", h, d)
            return 1
        off = int(h["timestamp_us"]) - int(d["timestamp_us"])
        offsets.append(off)
        print(f"sample {i} nonce={nonce} host={h['timestamp_us']} "
              f"device={d['timestamp_us']} offset_us={off}")
        time.sleep(0.05)

    offsets.sort()
    mid = offsets[len(offsets) // 2]
    residual = max(offsets) - min(offsets)
    mean = int(statistics.mean(offsets))
    print(f"RESULT OK offset_us≈{mid} (mean={mean}) residual_us={residual} "
          f"samples={n}")
    print("claim: cross_node_offset_estimate (not hard µs lock)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
