#!/usr/bin/env python3
"""Lab USB bus Suspend/Resume (OTG Host↔Device loop).

Host drives DWC HPRT port suspend/resume; post-resume GET_DESCRIPTOR smoke.
Device TinyUSB SUSPEND/RESUME is observed if present, but HPRT poke on this
S3 OTG loop often yields STOPPED (waiting) instead of SUSPEND — treated as
optional evidence, not a hard gate.

Claim: usb_bus_suspend_resume_lab — HPRT stack, NOT L0 PHY / MITM.
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
    ap.add_argument("--hold-ms", type=int, default=150)
    ap.add_argument("--require-device-events", action="store_true",
                    help="fail if Device TinyUSB suspend/resume counts do not bump")
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d0 = get(f"http://{args.device}/api/status")
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("id"))
    print("device", d0.get("version"), "otg", (d0.get("otg") or {}).get("state"),
          "suspCnt", (d0.get("gadget") or {}).get("suspendCount"),
          "resCnt", (d0.get("gadget") or {}).get("resumeCount"))

    if not (h.get("dut") or {}).get("present"):
        print("FAIL: need OTG DUT")
        return 2

    s0 = int((d0.get("gadget") or {}).get("suspendCount") or 0)
    r0 = int((d0.get("gadget") or {}).get("resumeCount") or 0)

    q = urllib.parse.urlencode({"hold_ms": args.hold_ms, "mode": "cycle"})
    r = post(f"http://{args.host}/api/lab/usb-suspend-resume?{q}", timeout=30)
    print("cycle", json.dumps(r, indent=2))
    if not r.get("ok"):
        print("FAIL host cycle", r.get("err"))
        return 1
    if not r.get("postTrafficOk"):
        print("FAIL: post-resume EP0 smoke failed")
        return 1

    time.sleep(0.25)
    d1 = get(f"http://{args.device}/api/status")
    s1 = int((d1.get("gadget") or {}).get("suspendCount") or 0)
    r1 = int((d1.get("gadget") or {}).get("resumeCount") or 0)
    otg1 = (d1.get("otg") or {}).get("state")
    print("device after", "otg", otg1,
          "suspended", (d1.get("otg") or {}).get("suspended"),
          "suspCnt", s1, "resCnt", r1)

    device_events = (s1 > s0) and (r1 > r0)
    if device_events:
        print("NOTE: Device TinyUSB suspend/resume counts increased (MEASURED)")
    else:
        print("NOTE: Device TinyUSB suspend/resume UNKNOWN on this HPRT path "
              f"(often STOPPED/waiting; susp {s0}->{s1} res {r0}->{r1})")

    if args.require_device_events and not device_events:
        print("FAIL: --require-device-events and Device counts did not bump")
        return 1

    # HPRT poke can leave gadget in waiting — soft-reenum to restore lab posture.
    if otg1 != "connected":
        rr = post(f"http://{args.device}/api/lab/soft-reenum")
        print("soft-reenum", rr)
        wait_dut(args.host, False, timeout_s=3.0)
        if not wait_dut(args.host, True, timeout_s=8.0):
            print("FAIL: DUT did not return after soft-reenum recovery")
            return 1
        time.sleep(0.3)
        d2 = get(f"http://{args.device}/api/status")
        print("device recovered", (d2.get("otg") or {}).get("state"))

    print("RESULT OK — bus suspend/resume lab (Host HPRT + post EP0)")
    print("claim: usb_bus_suspend_resume_lab (not L0; Device TinyUSB optional)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
