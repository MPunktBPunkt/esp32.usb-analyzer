#!/usr/bin/env python3
"""Observer-effect baseline (Gate 11).

Modes:
  loop   — Host OTG MSC READ repeatability
  proxy  — direct Host USB READ vs Device→Host MSC-proxy (WiFi)
           On OTG-loop: uses Host proxy-source=pattern + Device identity=0
           to avoid Device↔Host USB recursion.

Claim: observer_effect_baseline_lab — NOT transparency FACT / NOT L0.
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.parse
import urllib.request


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


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--mode", choices=("loop", "proxy"), default="proxy")
    ap.add_argument("--blocks", type=int, default=32)
    ap.add_argument("--chunk", type=int, default=4)
    ap.add_argument("--proxy-blocks", type=int, default=8)
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
        "direct": {
            "rate_kBs_avg": sum(direct_rates) / len(direct_rates),
            "duration_ms_avg": sum(direct_ms) / len(direct_ms),
            "runs": args.runs,
        },
    }

    if args.mode == "proxy":
        # Host serves lab pattern over MSC-proxy (loop-safe).
        src = post(f"http://{args.host}/api/lab/proxy-source?mode=pattern")
        print("proxy-source", src)
        arm = post(
            f"http://{args.device}/api/lab/usb-proxy?"
            + urllib.parse.urlencode(
                {"enable": 1, "peer": args.host, "identity": 0, "reenum": 0}
            )
        )
        print("proxy-arm", arm)
        time.sleep(0.4)
        st_times: list[float] = []
        st_ok = True
        for i in range(args.runs):
            st = post(
                f"http://{args.device}/api/lab/usb-proxy-selftest?"
                + urllib.parse.urlencode(
                    {"lba": 0, "blocks": min(args.proxy_blocks, 8)}
                ),
                timeout=60,
            )
            print(f"proxy[{i}]", "ok=", st.get("ok"), "ms=", st.get("duration_ms"),
                  "err=", st.get("err"))
            if not st.get("ok"):
                st_ok = False
                result["proxy_last"] = st
                break
            st_times.append(float(st.get("duration_ms") or 0))
            time.sleep(0.1)
        # restore
        post(f"http://{args.device}/api/lab/usb-proxy?enable=0")
        post(f"http://{args.host}/api/lab/proxy-source?mode=dut")

        if not st_ok or not st_times:
            result["ok"] = False
            result["err"] = "proxy_path_fail"
            print(json.dumps(result, indent=2))
            return 1
        p_ms = sum(st_times) / len(st_times)
        d_ms = result["direct"]["duration_ms_avg"]
        # Normalize rough throughput estimate for proxy bytes
        proxy_bytes = min(args.proxy_blocks, 8) * 512
        proxy_kbs = (proxy_bytes / 1024.0) / (p_ms / 1000.0) if p_ms > 0 else 0
        result["proxy"] = {
            "ok": True,
            "source": "pattern",
            "identity": "skipped",
            "duration_ms_avg": p_ms,
            "rate_kBs_est": round(proxy_kbs, 1),
            "blocks": min(args.proxy_blocks, 8),
            "note": "wifi_msc_proxy_pattern_fill_otg_loop_safe",
        }
        result["delta"] = {
            "proxy_minus_direct_ms": p_ms - d_ms,
            "direct_kBs": result["direct"]["rate_kBs_avg"],
            "proxy_kBs_est": round(proxy_kbs, 1),
            "interpretation": (
                "positive_ms ⇒ proxy/WiFi slower than direct OTG USB; "
                "pattern source ≠ DUT MITM transparency"
            ),
        }
        result["note"] = "otg_loop_proxy_vs_direct_not_pc_stick_mitm"
    else:
        result["delta"] = {
            "rate_kBs_spread": max(direct_rates) - min(direct_rates),
            "interpretation": "OTG_loop_repeatability",
        }
        result["note"] = "loop_repeatability_only"

    print(json.dumps(result, indent=2))
    print("RESULT OK — observer-effect baseline")
    print("claim: observer_effect_baseline_lab (not transparency FACT)")
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
