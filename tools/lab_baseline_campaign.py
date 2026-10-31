#!/usr/bin/env python3
"""Lab baseline campaign — MEASURED speed / integrity / timing limits.

Paths:
  A) USB OTG Host→Device MSC (cable loop)  — /api/lab/usb-host-probe
  B) WiFi TCP ESP↔ESP integrity             — /api/lab/integrity

Writes JSON report to docs/evidence/ (stdout summary always).
"""
from __future__ import annotations

import argparse
import json
import statistics
import time
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path


def post_json(url: str, timeout: float = 300) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def usb_probe(host: str, blocks: int, chunk: int) -> dict:
    q = urllib.parse.urlencode({"blocks": blocks, "chunk": chunk, "lba": 0})
    return post_json(f"http://{host}/api/lab/usb-host-probe?{q}")


def tcp_integrity(a: str, b: str, frames: int, frame_bytes: int) -> dict:
    q = urllib.parse.urlencode(
        {"peer": b, "frames": frames, "frame_bytes": frame_bytes}
    )
    return post_json(f"http://{a}/api/lab/integrity?{q}")


def status(ip: str) -> dict:
    with urllib.request.urlopen(f"http://{ip}/api/status", timeout=5) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument(
        "--out",
        default="",
        help="JSON report path (default docs/evidence/LAB_BASELINE_<ts>.json)",
    )
    args = ap.parse_args()

    root = Path(__file__).resolve().parents[1]
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out = Path(args.out) if args.out else root / "docs" / "evidence" / f"LAB_BASELINE_{ts}.json"
    out.parent.mkdir(parents=True, exist_ok=True)

    report: dict = {
        "classification": "MEASURED",
        "timestamp_utc": ts,
        "fw_claim": "usb-analyzer analyzer-node",
        "topology": "OTG Host(.88) ↔ USB-C ↔ Device(.89); WiFi TCP parallel",
        "not_claimed": [
            "Gate4 USB-over-WiFi proxy",
            "D+/D- wire capture",
            "USB High-Speed",
        ],
        "nodes": {},
        "usb_otg_msc": [],
        "tcp_wifi": [],
        "limits": {},
    }

    print("=== node status ===")
    for label, ip in (("host", args.host), ("device", args.device)):
        s = status(ip)
        report["nodes"][label] = {
            "ip": ip,
            "version": s.get("version"),
            "role": s.get("role"),
            "otg": s.get("otg"),
            "dut": s.get("dut"),
            "gadget": s.get("gadget"),
        }
        print(label, s.get("version"), s.get("role"), (s.get("otg") or {}).get("state"))

    # --- USB sweeps ---
    print("\n=== A) USB OTG MSC (cable) ===")
    print(f"{'blocks':>6} {'chunk':>5} {'ok':>4} {'pat':>4} {'failB':>6} "
          f"{'kB/s':>7} {'p50us':>8} {'p95us':>8} {'maxus':>8} {'ms':>6}")
    usb_cfgs = [
        (1, 1),
        (8, 1),
        (32, 1),
        (64, 1),
        (128, 1),
        (256, 1),
        (512, 1),
        (64, 2),
        (128, 2),
        (256, 4),
        (512, 4),
        (512, 8),
        (512, 8),  # repeat
        (512, 8),
    ]
    peak_usb = 0
    any_loss = False
    for blocks, chunk in usb_cfgs:
        try:
            r = usb_probe(args.host, blocks, chunk)
        except Exception as e:
            print(f"{blocks:>6} {chunk:>5} ERR {e}")
            report["usb_otg_msc"].append(
                {"blocks": blocks, "chunk": chunk, "error": str(e)}
            )
            continue
        rate = int(r.get("rate_kBs") or 0)
        peak_usb = max(peak_usb, rate)
        fail_b = int(r.get("pattern_fail_bytes") or 0)
        if fail_b or not r.get("pattern_ok", True):
            any_loss = True
        rtt = r.get("rtt_us") or {}
        row = {
            "blocks": blocks,
            "chunk": chunk,
            "ok": bool(r.get("ok")),
            "pattern_ok": bool(r.get("pattern_ok")),
            "pattern_fail_bytes": fail_b,
            "rate_kBs": rate,
            "bytes_read": r.get("bytes_read"),
            "duration_ms": r.get("duration_ms"),
            "transfers": r.get("transfers"),
            "rtt_us": rtt,
            "path": r.get("path"),
            "err": r.get("err"),
        }
        report["usb_otg_msc"].append(row)
        print(
            f"{blocks:>6} {chunk:>5} {str(row['ok']):>4} {str(row['pattern_ok']):>4} "
            f"{fail_b:>6} {rate:>7} {rtt.get('p50', 0):>8} {rtt.get('p95', 0):>8} "
            f"{rtt.get('max', 0):>8} {r.get('duration_ms', 0):>6}"
        )
        time.sleep(0.15)

    # sustained: 3× full capacity with chunk=8
    print("\n=== A2) USB sustained (3× 512 blocks, chunk=8) ===")
    sust = []
    for i in range(3):
        r = usb_probe(args.host, 512, 8)
        sust.append(r)
        print(
            f"  run{i+1}: ok={r.get('ok')} pattern={r.get('pattern_ok')} "
            f"failB={r.get('pattern_fail_bytes')} rate={r.get('rate_kBs')} "
            f"p50={((r.get('rtt_us') or {}).get('p50'))}"
        )
    report["usb_sustained"] = sust
    if sust:
        rates = [int(x.get("rate_kBs") or 0) for x in sust if x.get("ok")]
        fails = [int(x.get("pattern_fail_bytes") or 0) for x in sust]
        report["limits"]["usb_sustained_kBs"] = {
            "min": min(rates) if rates else None,
            "avg": round(statistics.fmean(rates), 1) if rates else None,
            "max": max(rates) if rates else None,
            "pattern_fail_bytes_total": sum(fails),
        }

    # --- TCP sweeps ---
    print("\n=== B) WiFi TCP integrity ===")
    print(f"{'fbytes':>6} {'frames':>6} {'ok':>4} {'kB/s':>7} {'p50us':>10} {'p95us':>10}")
    tcp_cfgs = [
        (64, 64),
        (256, 64),
        (1024, 64),
        (2048, 64),
        (4096, 64),
        (2048, 128),
    ]
    peak_tcp = 0
    for fbytes, frames in tcp_cfgs:
        try:
            r = tcp_integrity(args.host, args.device, frames, fbytes)
        except Exception as e:
            print(f"{fbytes:>6} {frames:>6} ERR {e}")
            report["tcp_wifi"].append(
                {"frame_bytes": fbytes, "frames": frames, "error": str(e)}
            )
            continue
        rate = int(r.get("rate_kBs") or 0)
        peak_tcp = max(peak_tcp, rate)
        rtt = r.get("rtt_us") or {}
        row = {
            "frame_bytes": fbytes,
            "frames": frames,
            "ok": bool(r.get("ok")),
            "frames_fail": r.get("frames_fail"),
            "rate_kBs": rate,
            "rtt_us": rtt,
            "path": r.get("path"),
            "err": r.get("err"),
        }
        report["tcp_wifi"].append(row)
        print(
            f"{fbytes:>6} {frames:>6} {str(row['ok']):>4} {rate:>7} "
            f"{rtt.get('p50', 0):>10} {rtt.get('p95', 0):>10}"
        )

    report["limits"]["usb_peak_kBs"] = peak_usb
    report["limits"]["tcp_peak_kBs"] = peak_tcp
    report["limits"]["usb_pattern_loss_observed"] = any_loss
    report["limits"]["notes"] = [
        "USB path = Full-Speed OTG cable MSC READ Host→Device (not MITM).",
        "Theoretical FS bulk ceiling ~1.2 MB/s class; lab stack+gadget overhead applies.",
        "TCP path = WiFi ESP↔ESP PUSH/ACK; RTT-dominated for small frames.",
        "pattern_fail_bytes>0 would be data corruption on USB lab pattern.",
    ]

    out.write_text(json.dumps(report, indent=2) + "\n")
    print(f"\n=== SUMMARY ===")
    print(f"USB peak:  {peak_usb} kB/s   pattern_loss={any_loss}")
    print(f"TCP peak:  {peak_tcp} kB/s")
    if "usb_sustained_kBs" in report["limits"]:
        print(f"USB sustained: {report['limits']['usb_sustained_kBs']}")
    print(f"report: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
