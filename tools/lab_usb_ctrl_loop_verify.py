#!/usr/bin/env python3
"""Gate-4 lab: EP0 GET_DESCRIPTOR forward over WiFi (OTG Host↔Device loop).

Device asks Host to GET_DESCRIPTOR(DEVICE) against OTG DUT (= loop gadget),
compares to local gadget descriptor. Proxy MSC should be OFF.

Claim: ep0_get_desc_forward_lab_loop — NOT USB control MITM / transparency.
"""
from __future__ import annotations

import argparse
import json
import urllib.parse
import urllib.request


def get(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=8) as r:
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
    ap.add_argument("--device", default="192.168.178.89")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("id"),
          (h.get("dut") or {}).get("present"))
    print("device", d.get("version"), "proxy",
          (d.get("gadget") or {}).get("proxy"))

    if not (h.get("dut") or {}).get("present"):
        print("FAIL: need OTG Host↔Device cable (dut on .88)")
        return 2
    if (d.get("gadget") or {}).get("proxy"):
        print("WARN: MSC proxy is ON — disarming for ctrl-forward lab")
        post(f"http://{args.device}/api/lab/usb-proxy?enable=0")

    q = urllib.parse.urlencode({"peer": args.host, "desc": "device"})
    r = post(f"http://{args.device}/api/lab/usb-ctrl-forward?{q}")
    print("ctrl-device", json.dumps({k: r.get(k) for k in (
        "ok", "actual", "match_local_desc", "crc32", "duration_ms", "err",
        "hex")}, indent=2)[:600])

    if not r.get("ok"):
        print("FAIL:", r.get("err"))
        return 1
    if not r.get("match_local_desc"):
        print("FAIL: device descriptor mismatch")
        return 1
    hx = r.get("hex") or ""
    if not hx.startswith("1201"):
        print("WARN: unexpected device descriptor prefix", hx[:8])

    # String product (index 2)
    qs = urllib.parse.urlencode({"peer": args.host, "desc": "string", "index": 2})
    rs = post(f"http://{args.device}/api/lab/usb-ctrl-forward?{qs}")
    print("ctrl-string2", "ok" if rs.get("ok") else "FAIL",
          "decoded", rs.get("decoded"), "match", rs.get("match_local_desc"))
    if not rs.get("ok") or not rs.get("match_local_desc"):
        print("FAIL: string descriptor mismatch", rs.get("err"))
        return 1

    print("RESULT OK — EP0 device+string forward matches local gadget")
    print("claim: ep0_get_desc_forward_lab_loop (not USB control MITM)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
