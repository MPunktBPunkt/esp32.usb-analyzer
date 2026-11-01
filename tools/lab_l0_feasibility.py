#!/usr/bin/env python3
"""Gate 10A — L0 feasibility (HPRT Level-2 + Path A falsify).

Claim: gate10a_feasibility — NOT live D+/D− packets.
"""
from __future__ import annotations

import argparse
import json
import urllib.request


def get(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=10) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("present"))
    r = get(f"http://{args.host}/api/lab/l0-feasibility")
    print(json.dumps(r, indent=2))
    if not r.get("ok"):
        print("FAIL", r.get("err"))
        return 1
    if r.get("pathA_snifferOk"):
        print("FAIL: Path A unexpectedly claimed OK")
        return 1
    if not r.get("pathB_preferred"):
        print("FAIL: Path B should be preferred")
        return 1
    print("RESULT OK — Gate 10A: Path A falsified; HPRT Level-2 dumped")
    print("claim: gate10a_feasibility (not L0 packets)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
