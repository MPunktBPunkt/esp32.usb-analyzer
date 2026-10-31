#!/usr/bin/env python3
"""Arm Gate-4 MSC proxy: Device (.89) reads/writes via Host (.88) DUT.

Wiring (required):
  PC ──OTG── Device ESP (.89, green)
  FS-DUT (Stick) ──OTG── Host ESP (.88, blue)
  WiFi TCP :9752 between ESPs

Phone often fails on S3 Host (HS-only). Use a Full-Speed USB stick.
"""
from __future__ import annotations

import argparse
import json
import urllib.parse
import urllib.request


def get(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=8) as r:
        return json.loads(r.read().decode())


def post(url: str) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--off", action="store_true")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    print("host ", h.get("version"), h.get("role"), (h.get("otg") or {}).get("state"),
          "dut", (h.get("dut") or {}).get("present"), (h.get("dut") or {}).get("id"))
    print("device", d.get("version"), d.get("role"), (d.get("otg") or {}).get("state"),
          "gadget", d.get("gadget"))

    if args.off:
        r = post(f"http://{args.device}/api/lab/usb-proxy?enable=0")
        print("disarm", r)
        return 0

    q = urllib.parse.urlencode({"enable": 1, "peer": args.host})
    r = post(f"http://{args.device}/api/lab/usb-proxy?{q}")
    print("arm", r)

    d2 = get(f"http://{args.device}/api/status")
    g = d2.get("gadget") or {}
    print("proxy", g.get("proxy"), "peer", g.get("proxyPeer"))

    dut = (h.get("dut") or {}).get("present")
    if not dut:
        print("WARN: Host has no DUT — plug Full-Speed stick into BLUE (.88) OTG")
        return 2
    print("OK: DUT present — PC MSC I/O on green Device should proxy to stick")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
