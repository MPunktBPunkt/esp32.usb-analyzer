#!/usr/bin/env python3
"""Store-and-Forward discrimination — Mode-A/TCP vs USB MSC under concurrent load.

Proves the residual gap that SF-0+ is meant to close. Does **not** implement
PSRAM rings (see docs/STORE_FORWARD.md SF-0..5).

Measures:
  1) USB MSC peak (Host probe)
  2) TCP integrity peak (pipeline if available)
  3) Concurrent: Host MSC probe while Device runs integrity TX (best-effort)

Claim: sf_discrimination_lab — NOT STORE_FORWARD implemented / NOT coex root-cause FACT.
"""
from __future__ import annotations

import argparse
import json
import threading
import time
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 120) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def post(url: str, timeout: float = 180) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--usb-blocks", type=int, default=64)
    ap.add_argument("--usb-chunk", type=int, default=8)
    ap.add_argument("--tcp-bytes", type=int, default=2048)
    ap.add_argument("--tcp-count", type=int, default=64)
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_SF_DISCRIMINATION_MEASURED.md"),
    )
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status", timeout=8)
    d = get(f"http://{args.device}/api/status", timeout=8)

    # 1) USB alone
    q_usb = urllib.parse.urlencode(
        {"blocks": args.usb_blocks, "chunk": args.usb_chunk, "dir": "read", "lba": 0}
    )
    usb = post(f"http://{args.host}/api/lab/usb-host-probe?{q_usb}", timeout=120)

    # 2) TCP alone (pipeline)
    q_tcp = urllib.parse.urlencode(
        {
            "peer": args.device,
            "bytes": args.tcp_bytes,
            "count": args.tcp_count,
            "pipeline": 1,
            "window": 8,
        }
    )
    # integrity may be GET on some builds
    tcp = get(f"http://{args.host}/api/lab/integrity?{q_tcp}", timeout=180)
    if tcp.get("err"):
        tcp = post(f"http://{args.host}/api/lab/integrity?{q_tcp}", timeout=180)

    # 3) Concurrent: start TCP on background thread, MSC in foreground
    tcp_conc: dict = {}
    usb_conc: dict = {}

    def tcp_job() -> None:
        nonlocal tcp_conc
        tcp_conc = get(f"http://{args.host}/api/lab/integrity?{q_tcp}", timeout=180)
        if tcp_conc.get("err"):
            tcp_conc = post(f"http://{args.host}/api/lab/integrity?{q_tcp}", timeout=180)

    th = threading.Thread(target=tcp_job, daemon=True)
    th.start()
    time.sleep(0.15)
    usb_conc = post(f"http://{args.host}/api/lab/usb-host-probe?{q_usb}", timeout=120)
    th.join(timeout=200)

    usb_k = float(usb.get("rate_kBs") or 0)
    tcp_k = float(tcp.get("rate_kBs") or tcp.get("rate") or 0)
    usb_c = float(usb_conc.get("rate_kBs") or 0)
    tcp_c = float(tcp_conc.get("rate_kBs") or tcp_conc.get("rate") or 0)
    gap = usb_k - tcp_k

    # SF warranted when TCP ≪ USB in isolation (known) OR concurrent degrades USB hard
    sf_warranted = (tcp_k > 0 and usb_k > 0 and tcp_k < 0.6 * usb_k) or (
        usb_k > 0 and usb_c > 0 and usb_c < 0.7 * usb_k
    )

    out = {
        "ok": bool(usb.get("ok")) and (bool(tcp.get("ok")) or tcp_k > 0),
        "claim": "sf_discrimination_lab",
        "timestamp_utc": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
        "nodes": {
            "host": h.get("version"),
            "device": d.get("version"),
        },
        "usb_alone_kBs": usb_k,
        "tcp_alone_kBs": tcp_k,
        "usb_concurrent_kBs": usb_c,
        "tcp_concurrent_kBs": tcp_c,
        "gap_usb_minus_tcp_kBs": round(gap, 1),
        "sf_warranted": sf_warranted,
        "usb_alone": {k: usb.get(k) for k in ("ok", "rate_kBs", "duration_ms", "err")},
        "tcp_alone": {k: tcp.get(k) for k in ("ok", "rate_kBs", "rate", "gaps", "err")},
        "usb_concurrent": {k: usb_conc.get(k) for k in ("ok", "rate_kBs", "duration_ms", "err")},
        "tcp_concurrent": {
            k: tcp_conc.get(k) for k in ("ok", "rate_kBs", "rate", "gaps", "err")
        },
        "next": "SF-0 Dual PSRAM rings + block-ready (USB task never calls TCP) — docs/STORE_FORWARD.md",
        "not_claimed": [
            "store_forward_implemented",
            "coex_root_cause",
            "psram_rings",
            "transparent_mitm",
        ],
    }

    md = Path(args.out)
    lines = [
        f"# Lab MEASURED — SF discrimination · {h.get('version')}",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**USB alone:** {usb_k:.1f} kB/s · **TCP alone:** {tcp_k:.1f} kB/s · gap={gap:.1f}",
        f"**Concurrent USB/TCP:** {usb_c:.1f} / {tcp_c:.1f} kB/s",
        f"**SF warranted (heuristic):** {sf_warranted}",
        "",
        "## Epistemik",
        "",
        "- This measures the Mode-A residual vs USB Soft-Ceiling — contract for SF-0+.",
        "- Does **not** implement STORE_FORWARD rings or prove coex root cause.",
        "- See `docs/STORE_FORWARD.md` gates SF-0…SF-5.",
        "",
        f"JSON: `{md.with_suffix('.json').name}`",
        "",
        "```json",
        json.dumps(out, indent=2, default=str),
        "```",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    md.with_suffix(".json").write_text(json.dumps(out, indent=2, default=str), encoding="utf-8")
    print(json.dumps({k: out[k] for k in (
        "ok", "usb_alone_kBs", "tcp_alone_kBs", "usb_concurrent_kBs",
        "tcp_concurrent_kBs", "sf_warranted")}, indent=2))
    print("WROTE", md)
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
