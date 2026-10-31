#!/usr/bin/env python3
"""Lab performance + latency report.

MEASURED paths:
  A) ESP↔ESP TCP PUSH→ACK RTT (Gate 3 WiFi data plane)
  B) PC→ESP Device MSC sector read (USB gadget only — not through Host ESP)

Gate-4 MSC bulk proxy (Device→Host DUT) selftest: tools/lab_usb_proxy_verify.py
NOT claimed: transparent MITM / PC observer-effect baseline.
"""
from __future__ import annotations

import argparse
import json
import os
import statistics
import subprocess
import time
import urllib.parse
import urllib.request


def tcp_integrity(ip: str, peer: str, frames: int, frame_bytes: int) -> dict:
    q = urllib.parse.urlencode(
        {"peer": peer, "frames": frames, "frame_bytes": frame_bytes}
    )
    req = urllib.request.Request(
        f"http://{ip}/api/lab/integrity?{q}", method="POST", data=b""
    )
    with urllib.request.urlopen(req, timeout=300) as r:
        return json.loads(r.read().decode())


def find_analyzer_dev() -> str | None:
    # Prefer by-id; fall back to model scan.
    by_id = "/dev/disk/by-id"
    if os.path.isdir(by_id):
        for name in os.listdir(by_id):
            if "ANALYZER" in name.upper() or "MPB" in name.upper():
                return os.path.realpath(os.path.join(by_id, name))
    for name in os.listdir("/sys/block"):
        if not name.startswith("sd"):
            continue
        model = open(f"/sys/block/{name}/device/model", errors="ignore").read().strip()
        if "ANALYZER" in model.upper():
            return f"/dev/{name}"
    return None


def usb_msc_read_latencies(dev: str, samples: int = 100) -> dict:
    """Time 512-byte reads from MSC gadget (PC → Device ESP USB)."""
    lat_us: list[float] = []
    with open(dev, "rb", buffering=0) as f:
        for i in range(samples):
            f.seek(0)
            t0 = time.perf_counter_ns()
            data = f.read(512)
            t1 = time.perf_counter_ns()
            if len(data) != 512:
                raise RuntimeError(f"short read {len(data)}")
            lat_us.append((t1 - t0) / 1000.0)
    lat_us.sort()
    return {
        "path": "usb_pc_to_device_esp_msc",
        "device": dev,
        "samples": samples,
        "sector_bytes": 512,
        "rtt_us": {  # one-way-ish host syscall→USB→gadget callback→USB→host
            "min": round(lat_us[0], 1),
            "avg": round(statistics.fmean(lat_us), 1),
            "p50": round(lat_us[len(lat_us) // 2], 1),
            "p95": round(lat_us[int(len(lat_us) * 0.95)], 1),
            "max": round(lat_us[-1], 1),
        },
        "note": "NOT full MITM path; Device OTG only",
    }


def fmt_rtt(rtt: dict) -> str:
    return (
        f"min={rtt.get('min')}  avg={rtt.get('avg')}  "
        f"p50={rtt.get('p50')}  p95={rtt.get('p95')}  max={rtt.get('max')}  µs"
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", default="192.168.178.88")
    ap.add_argument("--b", default="192.168.178.89")
    ap.add_argument("--latency-frames", type=int, default=200)
    ap.add_argument("--usb-samples", type=int, default=100)
    args = ap.parse_args()

    print("=== CLAIM SCOPE ===")
    print("USB→USB through both ESPs: NOT AVAILABLE (Gate 4 proxy missing)")
    print("Measuring: (1) WiFi TCP ESP↔ESP RTT  (2) USB MSC PC→Device ESP")
    print()

    print("=== 1) TCP latency (small impulse 64 B) ===")
    r = tcp_integrity(args.a, args.b, args.latency_frames, 64)
    print(json.dumps({k: r[k] for k in r if k != "note"}, indent=2))
    if r.get("rtt_us"):
        print("RTT PUSH→ACK:", fmt_rtt(r["rtt_us"]))
        # one-way estimate ~ RTT/2 (symmetric WiFi assumption — INFERENCE)
        p50 = r["rtt_us"]["p50"]
        print(f"INFERENCE one-way ~ p50/2 = {p50 / 2:.0f} µs (not USB)")
    print()

    print("=== 2) TCP throughput sweep ===")
    print(f"{'size':>6} {'kB/s':>8} {'p50_us':>10} {'p95_us':>10} {'ok':>5}")
    for sz in (64, 256, 1024, 2048, 4096):
        frames = 128 if sz >= 1024 else 200
        try:
            x = tcp_integrity(args.a, args.b, frames, sz)
        except Exception as e:
            print(f"{sz:>6} ERR {e}")
            continue
        rtt = x.get("rtt_us") or {}
        print(
            f"{sz:>6} {x.get('rate_kBs', 0):>8} {rtt.get('p50', 0):>10} "
            f"{rtt.get('p95', 0):>10} {str(x.get('ok')):>5}"
        )
    print()

    print("=== 3) USB MSC read (PC → .89 Device gadget) ===")
    dev = find_analyzer_dev()
    if not dev:
        print("no ANALYZER MSC block device found")
    else:
        # may need root for raw read
        try:
            u = usb_msc_read_latencies(dev, args.usb_samples)
            print(json.dumps(u, indent=2))
            print("USB sector read:", fmt_rtt(u["rtt_us"]))
        except PermissionError:
            print(f"need read access to {dev} (sudo?) — trying dd timed samples")
            # fallback: sudo dd
            times = []
            for _ in range(min(args.usb_samples, 30)):
                t0 = time.perf_counter()
                subprocess.run(
                    ["dd", f"if={dev}", "of=/dev/null", "bs=512", "count=1", "status=none"],
                    check=False,
                    capture_output=True,
                )
                times.append((time.perf_counter() - t0) * 1e6)
            times.sort()
            print(
                {
                    "device": dev,
                    "samples": len(times),
                    "rtt_us": {
                        "min": round(times[0], 1),
                        "avg": round(statistics.fmean(times), 1),
                        "p50": round(times[len(times) // 2], 1),
                        "p95": round(times[int(len(times) * 0.95)], 1),
                        "max": round(times[-1], 1),
                    },
                    "note": "dd fallback; includes process overhead",
                }
            )
    print()
    print("=== SUMMARY ===")
    print("Impulse USB↔USB over both ESPs: unknown until Gate 4")
    print("Use TCP RTT above as WiFi hop budget; USB MSC as Device-side budget.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
