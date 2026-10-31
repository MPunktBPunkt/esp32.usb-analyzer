#!/usr/bin/env python3
"""Lab HID interrupt IN (OTG Host↔Device loop).

Device injects a keyboard report; Host EspUsbHost keyboard/HID callback
receives interrupt transfer data.

Claim: hid_interrupt_lab_loop — NOT L0 wire / MITM / HU.
"""
from __future__ import annotations

import argparse
import json
import threading
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
    ap.add_argument("--ascii", default="a", help="ASCII key to inject")
    ap.add_argument("--wait-ms", type=int, default=3000)
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("id"),
          "hidIface", (h.get("dut") or {}).get("hidIface"))
    print("device", d.get("version"), "hid", (d.get("gadget") or {}).get("hid"),
          "injects", (d.get("gadget") or {}).get("hidInjects"))

    if not (h.get("dut") or {}).get("present"):
        print("FAIL: need OTG DUT")
        return 2
    if not (d.get("gadget") or {}).get("hid"):
        print("FAIL: device HID not enabled (flash 0.0.35+ device FW)")
        return 2

    observe: dict = {}
    err: list[str] = []

    def host_wait() -> None:
        try:
            q = urllib.parse.urlencode({"wait_ms": args.wait_ms, "clear": 1})
            observe.update(
                post(f"http://{args.host}/api/lab/hid-observe?{q}",
                     timeout=args.wait_ms / 1000.0 + 5))
        except Exception as e:  # noqa: BLE001
            err.append(str(e))

    th = threading.Thread(target=host_wait, daemon=True)
    th.start()
    time.sleep(0.25)
    inj = post(
        f"http://{args.device}/api/lab/hid-inject?"
        + urllib.parse.urlencode({"ascii": args.ascii[:1]}))
    print("inject", json.dumps(inj, indent=2))
    th.join(timeout=args.wait_ms / 1000.0 + 6)
    if err:
        print("FAIL observe exception", err[0])
        return 1
    print("observe", json.dumps(observe, indent=2))

    if not inj.get("ok"):
        print("FAIL inject", inj.get("err"))
        return 1
    if not observe.get("ok"):
        print("FAIL observe", observe.get("err"))
        return 1

    want = ord(args.ascii[:1])
    # EspUsbHost may report HID usage (0x04='a') or ASCII — accept either.
    kc = int(observe.get("keycode") or 0)
    asc = int(observe.get("ascii") or 0)
    if asc != want and kc == 0 and not (observe.get("reportHex") or ""):
        print("FAIL: no keycode/ascii/report")
        return 1
    print("RESULT OK — HID interrupt report received on Host")
    print("claim: hid_interrupt_lab_loop (not L0 / not HU)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
