#!/usr/bin/env python3
"""Lab BCL 8-byte BE frame over AOA vendor bulk (OTG loop).

Sends Desk session-init (12 34 56 78) + HANDSHAKE frame (dest default 0x0FA4),
expects Device echo. Desk FACT header from technology-audit bmw-bcl.

Claim: lab_bcl_on_aoa_bulk — NOT HU wire / NOT Etch/SAS.
"""
from __future__ import annotations

import argparse
import json
import urllib.error
import urllib.parse
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
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--cmd", type=int, default=4, help="BCL command (4=HANDSHAKE)")
    ap.add_argument("--dest", default="0x0FA4", help="BCL dest (Etch lab default)")
    ap.add_argument("--no-init", action="store_true")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    if not (h.get("dut") or {}).get("present"):
        print("FAIL: need OTG DUT on host (loop cable)")
        return 2
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("id"))

    q = urllib.parse.urlencode({
        "cmd": args.cmd,
        "dest": args.dest,
        "init": 0 if args.no_init else 1,
    })
    r = post(f"http://{args.host}/api/lab/aoa-bcl?{q}", timeout=20)
    print(json.dumps(r, indent=2))

    d = get(f"http://{args.device}/api/status")
    aoa = (d.get("gadget") or {}).get("aoa") or {}
    print("device bulkRx/Tx", aoa.get("bulkRx"), aoa.get("bulkTx"))

    if not r.get("ok"):
        print("FAIL", r.get("err"))
        return 1
    if int(r.get("cmd") or 0) != args.cmd:
        print("FAIL cmd mismatch")
        return 1
    dest = int(str(args.dest), 0)
    if int(r.get("dest") or -1) != dest:
        print("FAIL dest mismatch", r.get("dest"), dest)
        return 1
    print("RESULT OK — BCL frame over AOA bulk (lab echo)")
    print("claim: lab_bcl_on_aoa_bulk (not HU wire)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
