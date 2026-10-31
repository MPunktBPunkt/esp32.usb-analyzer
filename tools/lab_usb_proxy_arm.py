#!/usr/bin/env python3
"""Arm Gate-4 MSC proxy: Device (.89) reads/writes via Host (.88) DUT.

Also applies DUT snapshot identity on the Device gadget (VID/PID + MSC inquiry /
capacity) and soft-reenumerates — not USB control MITM.

Wiring (required):
  PC ──OTG── Device ESP (.89, green)
  FS-DUT (Stick) ──OTG── Host ESP (.88, blue)
  WiFi TCP :9752 between ESPs

Phone often fails on S3 Host (HS-only). Use a Full-Speed USB stick.
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.parse
import urllib.request


def get(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=8) as r:
        return json.loads(r.read().decode())


def post(url: str, timeout: float = 30) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--off", action="store_true")
    ap.add_argument("--probe-blocks", type=int, default=8,
                    help="Host MSC probe blocks to fill inquiry/capacity cache")
    ap.add_argument("--wait", type=float, default=8.0,
                    help="Seconds to wait for Device identity apply")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    hdut = h.get("dut") or {}
    print("host ", h.get("version"), h.get("role"), (h.get("otg") or {}).get("state"),
          "dut", hdut.get("present"), hdut.get("id"), hdut.get("product"))
    print("device", d.get("version"), d.get("role"), (d.get("otg") or {}).get("state"),
          "gadget", (d.get("gadget") or {}).get("appliedId"),
          "ident", (d.get("gadget") or {}).get("identityApplied"))

    if args.off:
        r = post(f"http://{args.device}/api/lab/usb-proxy?enable=0")
        print("disarm", r)
        return 0

    if not hdut.get("present"):
        print("WARN: Host has no DUT — plug Full-Speed stick into BLUE (.88) OTG")
        return 2

    # Fill Host last_probe_ (inquiry/capacity) without blocking Device TCP path.
    probe_q = urllib.parse.urlencode({
        "dir": "read",
        "blocks": max(1, args.probe_blocks),
        "chunk": 1,
        "lba": 0,
    })
    try:
        pr = post(f"http://{args.host}/api/lab/usb-host-probe?{probe_q}", timeout=60)
        inq = pr.get("inquiry") or {}
        print("host-probe", "ok" if pr.get("ok") else "FAIL",
              "inq", inq.get("vendor"), inq.get("product"),
              "cap", pr.get("capacity_blocks"), "bs", pr.get("block_size"))
    except Exception as e:
        print("host-probe ERR", e)

    q = urllib.parse.urlencode({"enable": 1, "peer": args.host})
    r = post(f"http://{args.device}/api/lab/usb-proxy?{q}")
    print("arm", r)

    deadline = time.time() + max(1.0, args.wait)
    g = {}
    while time.time() < deadline:
        d2 = get(f"http://{args.device}/api/status")
        g = d2.get("gadget") or {}
        pd = g.get("proxiedDut") or {}
        if g.get("identityApplied") and pd.get("present"):
            break
        time.sleep(0.5)

    host_id = (hdut.get("id") or "").lower()
    applied = (g.get("appliedId") or "").lower()
    pd = g.get("proxiedDut") or {}
    print("proxy", g.get("proxy"), "peer", g.get("proxyPeer"))
    print("host_dut", host_id, "device_applied", applied,
          "identityApplied", g.get("identityApplied"),
          "blocks", g.get("blockCount"), "proxied", pd.get("id"),
          "mscReady", pd.get("mscReady"))

    if not g.get("proxy"):
        print("FAIL: proxy not armed")
        return 1
    if not g.get("identityApplied"):
        print("WARN: identity not applied yet — Host probe cache empty or DUT missing?")
        return 3
    if host_id and applied and host_id != applied:
        print("WARN: applied id mismatch host", host_id, "vs device", applied)
        return 4
    prod = ((h.get("dut") or {}).get("product") or "").upper()
    if host_id.startswith("303a:") and ("ANALYZER" in prod or "ESP" in prod):
        print("WARN: Host DUT looks like Device gadget (OTG loop) — "
              "for A4 use a real FS stick on BLUE, not OTG↔OTG")
    print("OK: DUT identity applied on gadget — PC should re-enum as", applied)
    print("claim: gadget_identity_from_dut_snapshot (not USB control MITM)")
    print("verify: python3 tools/lab_usb_proxy_verify.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
