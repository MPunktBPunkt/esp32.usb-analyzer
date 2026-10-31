#!/usr/bin/env python3
"""Gate-6: verify Device capture has real BOT CBW/CSW from TinyUSB class hooks.

Lab path (no FS stick / PC MITM needed):
  Host OTG ↔ Device OTG cable
  POST Host /api/lab/usb-host-probe
  GET Device /api/capture → bot_cbw/bot_csw with source=class_hook_mscd_xfer

Claim: class-hook BOT decode. NOT D+/D− wire, NOT transparent MITM.
"""
from __future__ import annotations

import argparse
import json
import urllib.parse
import urllib.request


def get(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=15) as r:
        return json.loads(r.read().decode())


def post(url: str, timeout: float = 90) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--blocks", type=int, default=8)
    ap.add_argument("--lba", type=int, default=0)
    ap.add_argument("--dir", choices=("read", "write"), default="read")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    print("host", h.get("version"), h.get("role"), (h.get("dut") or {}).get("id"))
    print("device", d.get("version"), d.get("role"),
          (d.get("otg") or {}).get("state"), "capture", d.get("capture"))

    before = int(((d.get("capture") or {}).get("seq")) or 0)

    q = urllib.parse.urlencode({
        "dir": args.dir,
        "blocks": args.blocks,
        "chunk": 1,
        "lba": args.lba,
    })
    pr = post(f"http://{args.host}/api/lab/usb-host-probe?{q}")
    print("probe", "ok" if pr.get("ok") else "FAIL",
          "rate", pr.get("rate_kBs"), "pattern", pr.get("pattern_ok"),
          pr.get("err") or "")
    if not pr.get("ok"):
        print("FAIL: need OTG Host↔Device cable for this verify")
        return 2

    batch = get(f"http://{args.device}/api/capture?since={before}&limit=128")
    events = batch.get("events") or []
    cbws = [e for e in events
            if e.get("event_type") == "bot_cbw"
            and (e.get("decoded") or {}).get("source") == "class_hook_mscd_xfer"]
    csws = [e for e in events
            if e.get("event_type") == "bot_csw"
            and (e.get("decoded") or {}).get("source") == "class_hook_mscd_xfer"]

    print("capture events", len(events), "class_hook_cbw", len(cbws),
          "class_hook_csw", len(csws))

    if not cbws or not csws:
        # show any bot_* for debug
        bots = [e for e in events if str(e.get("event_type", "")).startswith("bot_")]
        for e in bots[:6]:
            print("  bot", e.get("event_type"), (e.get("decoded") or {}).get("source"),
                  e.get("note"), "hex", (e.get("payload_hex") or "")[:20])
        print("FAIL: no class_hook_mscd_xfer CBW/CSW — patch not linked?")
        return 1

    want_op = 42 if args.dir == "write" else 40
    matched = False
    for e in cbws:
        dec = e.get("decoded") or {}
        hx = e.get("payload_hex") or ""
        print("CBW", "op", dec.get("scsi_opcode"), "lba", dec.get("lba"),
              "tl", dec.get("transfer_length"), "sig", dec.get("signature"),
              "hex", hx[:16], "...")
        if hx.startswith("55534243"):  # 'USBC' LE = 55 53 42 43
            op = int(dec.get("scsi_opcode") if dec.get("scsi_opcode") is not None else -1)
            lba = int(dec.get("lba") if dec.get("lba") is not None else -1)
            if op == want_op and lba == args.lba:
                matched = True
        else:
            print("WARN: CBW signature hex not USBC")

    for e in csws[:3]:
        dec = e.get("decoded") or {}
        hx = e.get("payload_hex") or ""
        print("CSW", "status", dec.get("csw_status"), "residue", dec.get("residue"),
              "sig", dec.get("signature"), "hex", hx[:16])
        if not hx.startswith("55534253"):  # USBS
            print("WARN: CSW signature hex not USBS")

    if not matched:
        print("FAIL: no CBW matching probe intent",
              f"dir={args.dir} op={want_op} lba={args.lba}")
        return 1

    print("RESULT OK — class-hook BOT CBW/CSW")
    print("claim: class_hook_mscd_xfer (not wire PHY, not transparent MITM)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
