#!/usr/bin/env python3
"""Heap soak — soft-reenum + MSC with freelist / fragmentation hints.

Target: ≥1000 cycles or ≥1h (whichever limit hits first via --cycles / --max-s).
Records heap, heapMax (largest block), psram; classifies drift as leak-ish vs churn.

Claim: heap_soak_lab — NOT physical unplug / L0 / MITM.
"""
from __future__ import annotations

import argparse
import json
import statistics
import time
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 45) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def status(ip: str) -> dict:
    return get(f"http://{ip}/api/status", timeout=8)


def slope(xs: list[float]) -> float | None:
    n = len(xs)
    if n < 4:
        return None
    # simple linear regression vs index
    xm = (n - 1) / 2.0
    ym = sum(xs) / n
    num = sum((i - xm) * (xs[i] - ym) for i in range(n))
    den = sum((i - xm) ** 2 for i in range(n))
    return num / den if den else None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--cycles", type=int, default=1000)
    ap.add_argument("--max-s", type=float, default=3600.0)
    ap.add_argument("--settle-s", type=float, default=1.5)
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_HEAP_SOAK_MEASURED.md"),
    )
    args = ap.parse_args()

    t_start = time.time()
    rows: list[dict] = []
    fail_enum = 0
    fail_msc = 0
    h_heap: list[float] = []
    d_heap: list[float] = []
    h_max: list[float] = []
    d_max: list[float] = []

    h0 = status(args.host)
    d0 = status(args.device)
    print(
        "heap_soak start",
        h0.get("version"),
        d0.get("version"),
        "cycles",
        args.cycles,
        "max_s",
        args.max_s,
        flush=True,
    )

    for i in range(args.cycles):
        if time.time() - t_start >= args.max_s:
            print(f"max-s hit after {i} cycles", flush=True)
            break
        rr = get(f"http://{args.device}/api/lab/soft-reenum")
        time.sleep(args.settle_s)
        attached = False
        for _ in range(20):
            hs = status(args.host)
            otg = hs.get("otg") or {}
            if otg.get("state") == "device_attached" or otg.get("devices", 0) >= 1:
                attached = True
                break
            time.sleep(0.25)
        if not attached:
            fail_enum += 1
            rows.append({"i": i, "ok": False, "err": "no_attach", "reenum": rr})
            continue

        probe = get(
            f"http://{args.host}/api/lab/usb-host-probe?blocks=2&chunk=1&dir=read&lba=0"
        )
        hs = status(args.host)
        ds = status(args.device)
        ok = bool(probe.get("ok"))
        if not ok:
            fail_msc += 1
        hh = float(hs.get("heap") or 0)
        dh = float(ds.get("heap") or 0)
        hm = float(hs.get("heapMax") or 0)
        dm = float(ds.get("heapMax") or 0)
        h_heap.append(hh)
        d_heap.append(dh)
        h_max.append(hm)
        d_max.append(dm)
        row = {
            "i": i,
            "ok": ok,
            "host_heap": hh,
            "device_heap": dh,
            "host_heapMax": hm,
            "device_heapMax": dm,
            "host_psram": hs.get("psram"),
            "device_psram": ds.get("psram"),
            "host_diagRam": hs.get("diagRam"),
            "device_diagRam": ds.get("diagRam"),
            "rate_kBs": probe.get("rate_kBs"),
        }
        rows.append(row)
        if (i + 1) % 25 == 0 or i == 0:
            print(
                f"[{i+1}/{args.cycles}] ok={ok} heapH={hh:.0f}/{hm:.0f} "
                f"heapD={dh:.0f}/{dm:.0f}",
                flush=True,
            )

    wall = time.time() - t_start
    heal = get(
        f"http://{args.host}/api/lab/usb-host-probe?blocks=4&chunk=2&dir=read&lba=0"
    )

    def drift(xs: list[float]) -> float:
        return (max(xs) - min(xs)) if xs else 0.0

    # Classification: declining free heap + declining largest block → leak-ish;
    # free heap churn with stable heapMax → fragmentation/churn.
    hslope = slope(h_heap)
    dslope = slope(d_heap)
    hmslope = slope(h_max)
    dmslope = slope(d_max)

    def classify(free_slope: float | None, max_slope: float | None) -> str:
        if free_slope is None:
            return "insufficient_samples"
        # bytes per cycle; negative = shrinking free
        if free_slope < -20 and (max_slope is not None and max_slope < -10):
            return "leak_like_free_and_largest_block_falling"
        if free_slope < -20 and (max_slope is None or abs(max_slope) < 5):
            return "fragmentation_or_churn_free_falling_largest_stable"
        if abs(free_slope) < 5:
            return "stable"
        return "churn_no_clear_leak"

    n = len([r for r in rows if r.get("ok") is not None])
    out = {
        "ok": fail_enum == 0
        and fail_msc <= max(1, n // 20)
        and bool(heal.get("ok"))
        and n >= min(50, args.cycles),
        "claim": "heap_soak_lab",
        "timestamp_utc": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
        "cycles_requested": args.cycles,
        "cycles_done": n,
        "wall_s": round(wall, 1),
        "fail_enum": fail_enum,
        "fail_msc": fail_msc,
        "heal_ok": bool(heal.get("ok")),
        "host": {
            "heap_drift": drift(h_heap),
            "heapMax_drift": drift(h_max),
            "heap_slope_per_cycle": hslope,
            "heapMax_slope_per_cycle": hmslope,
            "class": classify(hslope, hmslope),
            "heap_p50": statistics.median(h_heap) if h_heap else None,
            "heapMax_p50": statistics.median(h_max) if h_max else None,
        },
        "device": {
            "heap_drift": drift(d_heap),
            "heapMax_drift": drift(d_max),
            "heap_slope_per_cycle": dslope,
            "heapMax_slope_per_cycle": dmslope,
            "class": classify(dslope, dmslope),
            "heap_p50": statistics.median(d_heap) if d_heap else None,
            "heapMax_p50": statistics.median(d_max) if d_max else None,
        },
        "module_hints": {
            "host_modules": h0.get("modules"),
            "device_modules": d0.get("modules"),
            "fields": ["heap", "heapMax", "psram", "diagRam"],
            "note": "heapMax=largest free block; falling free+heapMax ⇒ leak-like",
        },
        "rows_tail": rows[-20:],
        "not_claimed": ["physical_unplug", "l0_wire", "mitm", "absolute_leak_proof"],
    }

    md = Path(args.out)
    lines = [
        f"# Lab MEASURED — Heap soak · {h0.get('version')}",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**Cycles:** {n}/{args.cycles} · wall={out['wall_s']}s · "
        f"fail_enum={fail_enum} · fail_msc={fail_msc}",
        f"**Host class:** {out['host']['class']} · drift heap/heapMax="
        f"{out['host']['heap_drift']:.0f}/{out['host']['heapMax_drift']:.0f}",
        f"**Device class:** {out['device']['class']} · drift heap/heapMax="
        f"{out['device']['heap_drift']:.0f}/{out['device']['heapMax_drift']:.0f}",
        f"**Heal MSC:** {out['heal_ok']}",
        "",
        "## Epistemik",
        "",
        "- Soft-reenum soak approximates alloc churn; not VBUS unplug.",
        "- `heapMax` (largest block) separates leak-like from fragmentation/churn.",
        "- **Not claimed:** absolute leak proof · L0 · MITM.",
        "",
        f"JSON: `{md.with_suffix('.json').name}`",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    # omit full rows from JSON to keep size sane — keep summary + tail
    slim = dict(out)
    slim["rows_count"] = n
    md.with_suffix(".json").write_text(json.dumps(slim, indent=2, default=str), encoding="utf-8")
    print(json.dumps({k: slim[k] for k in ("ok", "cycles_done", "wall_s", "host", "device", "heal_ok")}, indent=2, default=str))
    print("WROTE", md, "ok=", out["ok"])
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
