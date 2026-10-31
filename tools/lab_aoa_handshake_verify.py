#!/usr/bin/env python3
"""Lab AOA control handshake on OTG Host↔Device loop (no phone / no HU).

Device (.89) enables EP0 AOA responder (51/52/53); Host (.88) runs accessory
handshake. Optional soft-reenum to 18D1:2D00 after START.

Desk FACT: technology-audit usb-aoa + hu-probe C1b lab (stage/respond rules).
Claim: aoa_control_handshake_lab — NOT HU wire, NOT BCL, NOT accessory bulk IF.
"""
from __future__ import annotations

import argparse
import json
import time
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
    ap.add_argument("--profile", default="lab",
                    choices=("lab", "bmw_lab", "minimal"))
    ap.add_argument("--no-reenum", action="store_true",
                    help="skip post-53 soft-reenum wait")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("id"),
          "present", (h.get("dut") or {}).get("present"))
    print("device", d.get("version"), "proxy",
          (d.get("gadget") or {}).get("proxy"),
          "aoa", (d.get("gadget") or {}).get("aoa"))

    if not (h.get("dut") or {}).get("present"):
        print("FAIL: need OTG Host↔Device cable (dut on .88)")
        return 2

    if (d.get("gadget") or {}).get("proxy"):
        print("WARN: MSC proxy ON — disarming")
        post(f"http://{args.device}/api/lab/usb-proxy?enable=0")

    # Enable Device AOA EP0 responder
    q = urllib.parse.urlencode({
        "enable": 1,
        "reenum": 0 if args.no_reenum else 1,
    })
    arm = post(f"http://{args.device}/api/lab/aoa-respond?{q}")
    print("arm", json.dumps(arm))
    if not arm.get("ok"):
        print("FAIL: aoa-respond", arm.get("err"))
        return 1

    time.sleep(0.2)
    before = get(f"http://{args.device}/api/status")
    aoa0 = ((before.get("gadget") or {}).get("aoa") or {})
    print("aoa before", aoa0.get("aoa51"), aoa0.get("aoa52"), aoa0.get("aoa53"))

    hq = urllib.parse.urlencode({
        "profile": args.profile,
        "wait_reenum": 0 if args.no_reenum else 1,
    })
    hs = post(f"http://{args.host}/api/lab/aoa-handshake?{hq}", timeout=45)
    print("handshake", json.dumps({k: hs.get(k) for k in (
        "ok", "protocol", "protocolOk", "stringsOk", "startOk", "reenumOk",
        "dutBefore", "dutAfter", "duration_ms", "err", "profile")}, indent=2))

    time.sleep(0.3)
    after = get(f"http://{args.device}/api/status")
    aoa1 = ((after.get("gadget") or {}).get("aoa") or {})
    print("aoa after", "51/52/53=", aoa1.get("aoa51"), aoa1.get("aoa52"),
          aoa1.get("aoa53"), "51ok=", aoa1.get("aoa51Ok"),
          "last", aoa1.get("lastStringIndex"), aoa1.get("lastString"))
    print("gadget id", (after.get("gadget") or {}).get("appliedId"))

    ok = bool(hs.get("ok"))
    if not hs.get("protocolOk") or int(hs.get("protocol") or 0) < 1:
        print("FAIL: GET_PROTOCOL")
        ok = False
    if int(hs.get("stringsOk") or 0) != 6:
        print("FAIL: SEND_STRING count", hs.get("stringsOk"))
        ok = False
    if not hs.get("startOk"):
        print("FAIL: START")
        ok = False
    if not args.no_reenum and not hs.get("reenumOk"):
        print("FAIL: re-enum to 18d1:2d0x", hs.get("dutAfter"), hs.get("err"))
        ok = False
    if (aoa1.get("aoa51") or 0) < 1 or (aoa1.get("aoa52") or 0) < 6 or (
            aoa1.get("aoa53") or 0) < 1:
        print("FAIL: device counters did not rise")
        ok = False

    # Disarm responder (restores lab VID if accessory)
    post(f"http://{args.device}/api/lab/aoa-respond?enable=0")

    if ok:
        print("RESULT OK — AOA 51→52×6→53 lab handshake")
        print("claim: aoa_control_handshake_lab (not HU/BCL/accessory bulk)")
        return 0
    print("RESULT FAIL")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
