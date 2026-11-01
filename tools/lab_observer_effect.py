#!/usr/bin/env python3
"""Gate 11 starter — observer-effect baseline (control metrics Δ).

Compares Host direct OTG MSC READ vs Device MSC-proxy selftest path to the
same DUT (when wired for proxy) OR vs Host probe alone on OTG loop.

Modes:
  loop   — OTG Host↔Device cable: direct Host probe only (+ optional 2nd probe)
  proxy  — Host has FS DUT; Device proxy-selftest vs Host usb-host-read

Claim: observer_effect_baseline_lab — NOT transparent MITM / NOT L0.
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.parse
import urllib.request
import zlib


def get(url: str, timeout: float = 10) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


def post(url: str, timeout: float = 60) -> dict:
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


def crc32_hex(data: bytes) -> str:
    return f"{zlib.crc32(data) & 0xffffffff:08x}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--mode", choices=("loop", "proxy"), default="loop")
    ap.add_argument("--blocks", type=int, default=32)
    ap.add_argument("--chunk", type=int, default=4)
    ap.add_argument("--runs", type=int, default=3)
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    print("host", h.get("version"), "dut", (h.get("dut") or {}).get("id"))
    print("device", d.get("version"), "otg", (d.get("otg") or {}).get("state"))

    direct_rates: list[float] = []
    direct_ms: list[float] = []
    for i in range(args.runs):
        q = urllib.parse.urlencode(
            {"blocks": args.blocks, "chunk": args.chunk, "dir": "read", "lba": 0}
        )
        r = post(f"http://{args.host}/api/lab/usb-host-probe?{q}")
        print(f"direct[{i}]", "ok=", r.get("ok"), "kB/s=", r.get("rate_kBs"),
              "ms=", r.get("duration_ms"))
        if not r.get("ok"):
            print("FAIL direct probe", r.get("err"))
            return 1
        direct_rates.append(float(r.get("rate_kBs") or 0))
        direct_ms.append(float(r.get("duration_ms") or 0))
        time.sleep(0.15)

    result: dict = {
        "ok": True,
        "mode": args.mode,
        "claim": "observer_effect_baseline_lab",
        "note": "stack_path_delta_not_l0_not_transparency_fact",
        "direct": {
            "rate_kBs_avg": sum(direct_rates) / len(direct_rates),
            "duration_ms_avg": sum(direct_ms) / len(direct_ms),
            "runs": args.runs,
        },
    }

    if args.mode == "proxy":
        # Arm is caller's job; try selftest vs host-read
        peer = args.host
        post(
            f"http://{args.device}/api/lab/usb-proxy?"
            + urllib.parse.urlencode({"enable": 1, "peer": peer}),
            timeout=20,
        )
        time.sleep(0.5)
        st = post(
            f"http://{args.device}/api/lab/usb-proxy-selftest?"
            + urllib.parse.urlencode({"lba": 0, "blocks": min(args.blocks, 8)}),
            timeout=60,
        )
        print("proxy_selftest", json.dumps(st, indent=2)[:500])
        hr = post(
            f"http://{args.host}/api/lab/usb-host-read?"
            + urllib.parse.urlencode({"lba": 0, "blocks": min(args.blocks, 8)}),
            timeout=60,
        )
        print("host_read", "ok=", hr.get("ok"), "bytes=", hr.get("bytes"))
        result["proxy"] = {
            "selftest_ok": bool(st.get("ok")),
            "host_read_ok": bool(hr.get("ok")),
            "selftest_ms": st.get("duration_ms"),
            "match": st.get("ok") and hr.get("ok"),
        }
        if not (st.get("ok") and hr.get("ok")):
            result["ok"] = False
            result["err"] = "proxy_path_incomplete"
            print(json.dumps(result, indent=2))
            print("FAIL proxy path — check MITM wiring (DUT on Host, PC/gadget on Device)")
            return 1
        # Rough Δ: proxy path slower than direct (expected)
        d_ms = result["direct"]["duration_ms_avg"]
        p_ms = float(st.get("duration_ms") or 0)
        result["delta"] = {
            "proxy_minus_direct_ms": p_ms - d_ms,
            "interpretation": "positive_ms_means_proxy_slower_than_direct_otg",
        }
    else:
        # loop: second probe series as repeatability baseline
        result["delta"] = {
            "rate_kBs_spread": max(direct_rates) - min(direct_rates),
            "interpretation": "OTG_loop_repeatability_not_pc_stick_vs_mitm",
        }

    print(json.dumps(result, indent=2))
    print("RESULT OK — observer-effect baseline recorded")
    print("claim: observer_effect_baseline_lab (not transparency FACT)")
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
