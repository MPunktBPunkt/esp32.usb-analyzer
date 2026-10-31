#!/usr/bin/env python3
"""Lab EP0 control negative / robustness series (OTG loop).

Caller-initiated Host control transfers: positives + expected STALL/fail cases.

Claim: usb_ctrl_neg_lab — NOT transparent SETUP MITM / L0.
"""
from __future__ import annotations

import argparse
import json
import urllib.error
import urllib.request


def get(url: str, timeout: float = 8) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


def post(url: str, timeout: float = 30) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")
        try:
            return json.loads(body)
        except Exception:
            return {"ok": False, "err": f"http_{e.code}", "body": body[:200]}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("id"))
    if not (h.get("dut") or {}).get("present"):
        print("FAIL: need OTG DUT")
        return 2

    r = post(f"http://{args.host}/api/lab/usb-ctrl-neg", timeout=30)
    print(json.dumps(r, indent=2))
    if not r.get("ok"):
        failed = [c for c in (r.get("detail") or []) if not c.get("pass")]
        print("FAIL cases:", [c.get("name") for c in failed])
        return 1
    print("RESULT OK — EP0 control neg series",
          r.get("passed"), "/", r.get("cases"))
    print("claim: usb_ctrl_neg_lab (not SETUP MITM)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
