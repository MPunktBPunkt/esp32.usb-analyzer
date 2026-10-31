#!/usr/bin/env python3
"""Lab BCL framing over AOA vendor bulk (OTG loop) + host unit gate.

Happy path + DATA payload + header-only + optional series.
RX parse is on Host firmware (bclValidateFrame on echo bytes).

Claim: lab_bcl_on_aoa_bulk — NOT HU wire / NOT Etch/SAS.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


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


def run_host_units() -> int:
    script = Path(__file__).resolve().parent.parent / "tests/host/run_bcl_lab_frame_tests.sh"
    print("--- host unit BclLabFrame ---", flush=True)
    p = subprocess.run(["bash", str(script)], check=False)
    return p.returncode


def call_bcl(host: str, cmd: int, dest: str, send_init: bool,
             plhex: str | None) -> dict:
    q: dict[str, str | int] = {
        "cmd": cmd,
        "dest": dest,
        "init": 0 if not send_init else 1,
    }
    if plhex is not None:
        q["plhex"] = plhex
    return post(f"http://{host}/api/lab/aoa-bcl?{urllib.parse.urlencode(q)}",
                timeout=20)


def expect_ok(label: str, r: dict, cmd: int, dest: int, data_len: int | None) -> bool:
    print(f"--- {label} ---")
    print(json.dumps(r, indent=2))
    if not r.get("ok"):
        print("FAIL", label, r.get("err"))
        return False
    if int(r.get("cmd") or 0) != cmd:
        print("FAIL", label, "cmd", r.get("cmd"), "!=", cmd)
        return False
    if int(r.get("dest") or -1) != dest:
        print("FAIL", label, "dest", r.get("dest"), "!=", dest)
        return False
    if data_len is not None:
        got_len = r.get("dataLen")
        if got_len is None or int(got_len) != data_len:
            print("FAIL", label, "dataLen", got_len, "!=", data_len)
            return False
    if not r.get("parseOk"):
        print("FAIL", label, "parseOk false (RX validate)")
        return False
    print("PASS", label)
    return True


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--cmd", type=int, default=4, help="single-shot BCL cmd")
    ap.add_argument("--dest", default="0x0FA4")
    ap.add_argument("--no-init", action="store_true")
    ap.add_argument("--plhex", default=None,
                    help="explicit payload hex (empty string = header-only)")
    ap.add_argument("--series", action="store_true",
                    help="HANDSHAKE + DATA + OPEN empty + CLOSE empty")
    ap.add_argument("--unit-only", action="store_true",
                    help="only host unit tests (no hardware)")
    ap.add_argument("--skip-unit", action="store_true")
    args = ap.parse_args()

    if not args.skip_unit:
        rc = run_host_units()
        if rc != 0:
            return rc
    if args.unit_only:
        return 0

    h = get(f"http://{args.host}/api/status")
    if not (h.get("dut") or {}).get("present"):
        print("FAIL: need OTG DUT on host (loop cable)")
        return 2
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("id"))
    dest = int(str(args.dest), 0)
    send_init = not args.no_init
    ok = True

    if args.series:
        # Init once on first call; subsequent frames skip init.
        cases = [
            ("HANDSHAKE default payload", 4, None, 6, True),
            ("DATA plhex", 2, "cafebabe", 4, False),
            ("OPEN header-only", 1, "", 0, False),
            ("CLOSE header-only", 3, "", 0, False),
            ("DATAACK empty", 6, "", 0, False),
        ]
        first = True
        for label, cmd, plhex, dlen, use_init in cases:
            r = call_bcl(args.host, cmd, args.dest,
                         send_init=use_init and first and send_init,
                         plhex=plhex)
            first = False
            if not expect_ok(label, r, cmd, dest, dlen):
                ok = False
                break
    else:
        r = call_bcl(args.host, args.cmd, args.dest, send_init, args.plhex)
        dlen = None
        if args.plhex is not None:
            dlen = len(args.plhex) // 2
        if not expect_ok("single", r, args.cmd, dest, dlen):
            ok = False

    d = get(f"http://{args.device}/api/status")
    aoa = (d.get("gadget") or {}).get("aoa") or {}
    print("device bulkRx/Tx", aoa.get("bulkRx"), aoa.get("bulkTx"))

    if not ok:
        return 1
    print("RESULT OK — BCL on AOA bulk (lab; RX parse)")
    print("claim: lab_bcl_on_aoa_bulk (not HU wire)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
