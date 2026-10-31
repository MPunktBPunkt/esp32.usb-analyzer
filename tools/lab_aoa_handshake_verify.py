#!/usr/bin/env python3
"""Lab AOA control handshake on OTG Host↔Device loop (no phone / no HU).

Device (.89) enables EP0 AOA responder (51/52/53); Host (.88) runs accessory
handshake. Optional soft-reenum to 18D1:2D00 after START.

Lab A/B (hu-probe C1b-P0.5 pattern):
  A — responder ON  → 51/52/53 PASS + counters↑
  B — responder OFF → Host STALL/fail; counters stay flat

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


def aoa_counters(st: dict) -> tuple[int, int, int]:
    a = ((st.get("gadget") or {}).get("aoa") or {})
    return int(a.get("aoa51") or 0), int(a.get("aoa52") or 0), int(a.get("aoa53") or 0)


def capture_has_aoa(host: str) -> list[dict]:
    """Return decoded AOA events from Host capture (if any)."""
    try:
        batch = get(f"http://{host}/api/capture?since=0&limit=128")
    except Exception:
        return []
    out = []
    for ev in batch.get("events") or []:
        dec = ev.get("decoded") or {}
        if dec.get("protocol") == "aoa":
            out.append(ev)
        elif (ev.get("note") or "").startswith("aoa"):
            out.append(ev)
    return out


def run_handshake(host: str, profile: str, wait_reenum: bool) -> dict:
    hq = urllib.parse.urlencode({
        "profile": profile,
        "wait_reenum": 1 if wait_reenum else 0,
    })
    return post(f"http://{host}/api/lab/aoa-handshake?{hq}", timeout=45)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--profile", default="lab",
                    choices=("lab", "bmw_lab", "minimal"))
    ap.add_argument("--no-reenum", action="store_true",
                    help="skip post-53 soft-reenum wait")
    ap.add_argument("--no-ab", action="store_true",
                    help="skip Lab B (responder off) negative check")
    args = ap.parse_args()
    wait_reenum = not args.no_reenum

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

    failed = False

    # --- Lab A: responder ON ---
    q = urllib.parse.urlencode({
        "enable": 1,
        "reenum": 1 if wait_reenum else 0,
    })
    arm = post(f"http://{args.device}/api/lab/aoa-respond?{q}")
    print("Lab A arm", json.dumps(arm))
    if not arm.get("ok"):
        print("FAIL: aoa-respond", arm.get("err"))
        return 1

    time.sleep(0.2)
    before = get(f"http://{args.device}/api/status")
    c0 = aoa_counters(before)
    print("Lab A counters before", c0)

    hs = run_handshake(args.host, args.profile, wait_reenum)
    print("Lab A handshake", json.dumps({k: hs.get(k) for k in (
        "ok", "protocol", "protocolOk", "stringsOk", "startOk", "reenumOk",
        "dutBefore", "dutAfter", "duration_ms", "err", "profile")}, indent=2))

    time.sleep(0.3)
    after = get(f"http://{args.device}/api/status")
    c1 = aoa_counters(after)
    print("Lab A counters after", c1,
          "51ok=", ((after.get("gadget") or {}).get("aoa") or {}).get("aoa51Ok"),
          "last", ((after.get("gadget") or {}).get("aoa") or {}).get("lastString"),
          "id", (after.get("gadget") or {}).get("appliedId"))

    if not hs.get("ok"):
        print("FAIL Lab A: handshake", hs.get("err"))
        failed = True
    if not hs.get("protocolOk") or int(hs.get("protocol") or 0) < 1:
        print("FAIL Lab A: GET_PROTOCOL")
        failed = True
    if int(hs.get("stringsOk") or 0) != 6:
        print("FAIL Lab A: SEND_STRING", hs.get("stringsOk"))
        failed = True
    if not hs.get("startOk"):
        print("FAIL Lab A: START")
        failed = True
    if wait_reenum and not hs.get("reenumOk"):
        print("FAIL Lab A: re-enum", hs.get("dutAfter"), hs.get("err"))
        failed = True
    if c1[0] < c0[0] + 1 or c1[1] < c0[1] + 6 or c1[2] < c0[2] + 1:
        print("FAIL Lab A: device counters did not rise", c0, "→", c1)
        failed = True

    aoa_ev = capture_has_aoa(args.host)
    print("Lab A capture aoa events", len(aoa_ev))
    if aoa_ev:
        sample = aoa_ev[-1].get("decoded") or {"note": aoa_ev[-1].get("note")}
        print("  last", json.dumps(sample)[:200])

    # Restore lab identity before Lab B
    post(f"http://{args.device}/api/lab/aoa-respond?enable=0")
    time.sleep(1.5)  # soft-reenum back to 303a:1001

    if not args.no_ab:
        # --- Lab B: responder OFF — Host must fail; counters flat ---
        # Ensure DUT present again after reenum
        for _ in range(20):
            h2 = get(f"http://{args.host}/api/status")
            if (h2.get("dut") or {}).get("present"):
                break
            time.sleep(0.25)
        else:
            print("FAIL Lab B: DUT not back after disarm")
            failed = True

        post(f"http://{args.device}/api/lab/aoa-respond?enable=0")  # explicit off
        time.sleep(0.2)
        b0 = aoa_counters(get(f"http://{args.device}/api/status"))
        hs_b = run_handshake(args.host, args.profile, wait_reenum=False)
        time.sleep(0.2)
        b1 = aoa_counters(get(f"http://{args.device}/api/status"))
        print("Lab B handshake ok=", hs_b.get("ok"), "err=", hs_b.get("err"),
              "counters", b0, "→", b1)
        if hs_b.get("ok"):
            print("FAIL Lab B: expected Host fail with responder OFF")
            failed = True
        else:
            print("Lab B PASS — Host fail as expected")
        if b1 != b0:
            # Tiny rise possible if partial path; require no full 51+52+53
            if b1[0] > b0[0] and b1[1] >= b0[1] + 6 and b1[2] > b0[2]:
                print("FAIL Lab B: full handshake counted while respond OFF")
                failed = True
            else:
                print("Lab B note: counters", b0, "→", b1, "(partial ok if STALL)")

    post(f"http://{args.device}/api/lab/aoa-respond?enable=0")

    if failed:
        print("RESULT FAIL")
        return 1
    print("RESULT OK — Lab A handshake + Lab B negative")
    print("claim: aoa_control_handshake_lab (not HU/BCL/accessory bulk)")
    if hs.get("duration_ms") is not None:
        print(f"MEASURED Lab A duration_ms={hs.get('duration_ms')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
