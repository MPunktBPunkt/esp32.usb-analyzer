#!/usr/bin/env python3
"""Lab USB enum / control-state outcome (OTG Host↔Device loop).

Measures post-enum stack outcome: address, bConfigurationValue, optional
caller-initiated GET_DESCRIPTOR + GET_CONFIGURATION. Optional Device soft-reenum
cycle (Detach→Attach).

Claim: usb_enum_outcome_lab — NOT mid-enum SETUP MITM / L0 / SET_ADDRESS wire.
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


def wait_dut(host: str, present: bool, timeout_s: float = 8.0) -> bool:
    t0 = time.time()
    while time.time() - t0 < timeout_s:
        h = get(f"http://{host}/api/status")
        got = bool((h.get("dut") or {}).get("present"))
        if got == present:
            return True
        time.sleep(0.15)
    return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--no-ep0", action="store_true",
                    help="skip GET_DESCRIPTOR/GET_CONFIGURATION probe")
    ap.add_argument("--reenum", action="store_true",
                    help="Device soft-reenum + second Host observe")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("id"),
          "cfg", (h.get("dut") or {}).get("configurationValue"),
          "hidIface", (h.get("dut") or {}).get("hidIface"))
    print("device", d.get("version"), "otg", (d.get("otg") or {}).get("state"),
          "hid", (d.get("gadget") or {}).get("hid"))

    if not (h.get("dut") or {}).get("present"):
        print("FAIL: need OTG DUT on host")
        return 2

    q = urllib.parse.urlencode({"ep0": 0 if args.no_ep0 else 1})
    r1 = post(f"http://{args.host}/api/lab/usb-enum-observe?{q}")
    print("observe1", json.dumps(r1, indent=2))
    if not r1.get("ok"):
        print("FAIL observe1", r1.get("err"))
        return 1
    if int(r1.get("address") or 0) == 0 or int(r1.get("configurationValue") or 0) == 0:
        print("FAIL: address/config not Assigned/Configured outcome")
        return 1

    if args.reenum:
        rr = post(f"http://{args.device}/api/lab/soft-reenum")
        print("soft-reenum", rr)
        if not rr.get("ok"):
            print("FAIL soft-reenum")
            return 1
        # Soft-reenum may briefly drop DUT.
        wait_dut(args.host, False, timeout_s=3.0)
        if not wait_dut(args.host, True, timeout_s=8.0):
            print("FAIL: DUT did not reappear after soft-reenum")
            return 1
        time.sleep(0.3)
        r2 = post(f"http://{args.host}/api/lab/usb-enum-observe?{q}")
        print("observe2", json.dumps(r2, indent=2))
        if not r2.get("ok"):
            print("FAIL observe2", r2.get("err"))
            return 1

    print("RESULT OK — enum outcome (address+config) lab")
    print("claim: usb_enum_outcome_lab (not SETUP MITM / not L0)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
