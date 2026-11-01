#!/usr/bin/env python3
"""Hotplug / soak campaign on OTG loop (soft-reenum + MSC probe).

Cycles: soft-reenum → wait → MSC READ → record heap/mount/recovery.

Claim: hotplug_soak_lab — NOT L0 / true physical unplug / MITM.
"""
from __future__ import annotations

import argparse
import json
import statistics
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 45) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        try:
            return json.loads(e.read().decode())
        except Exception:
            return {"ok": False, "err": f"http_{e.code}"}
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def status(ip: str) -> dict:
    return get(f"http://{ip}/api/status", timeout=8)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--cycles", type=int, default=30)
    ap.add_argument("--settle-s", type=float, default=1.8)
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_HOTPLUG_SOAK_MEASURED.md"),
    )
    args = ap.parse_args()

    rows: list[dict] = []
    fail_enum = 0
    fail_msc = 0
    recovery_ms: list[float] = []
    heap_host: list[int] = []
    heap_dev: list[int] = []

    h0 = status(args.host)
    d0 = status(args.device)
    print("start", h0.get("version"), d0.get("version"), "cycles", args.cycles)

    for i in range(args.cycles):
        t0 = time.time()
        # Soft disconnect/reconnect on Device gadget
        rr = get(f"http://{args.device}/api/lab/soft-reenum")
        time.sleep(args.settle_s)
        # Wait until Host sees DUT
        attached = False
        for _ in range(20):
            hs = status(args.host)
            st = (hs.get("otg") or {}).get("state")
            if st == "device_attached" or (hs.get("otg") or {}).get("devices", 0) >= 1:
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
        dt = (time.time() - t0) * 1000.0
        recovery_ms.append(dt)
        hs = status(args.host)
        ds = status(args.device)
        heap_host.append(int(hs.get("heap") or 0))
        heap_dev.append(int(ds.get("heap") or 0))
        ok = bool(probe.get("ok"))
        if not ok:
            fail_msc += 1
        rows.append(
            {
                "i": i,
                "ok": ok,
                "recovery_ms": round(dt, 1),
                "rate_kBs": probe.get("rate_kBs"),
                "host_heap": hs.get("heap"),
                "device_heap": ds.get("heap"),
                "device_mount": (ds.get("otg") or {}).get("state"),
                "mountCount": (ds.get("gadget") or {}).get("mountCount")
                if isinstance(ds.get("gadget"), dict)
                else ds.get("mountCount"),
            }
        )
        print(
            f"[{i+1}/{args.cycles}] ok={ok} {dt:.0f}ms "
            f"heapH={hs.get('heap')} heapD={ds.get('heap')}"
        )

    # Final heal probe
    heal = get(
        f"http://{args.host}/api/lab/usb-host-probe?blocks=4&chunk=2&dir=read&lba=0"
    )

    def drift(xs: list[int]) -> int:
        if not xs:
            return 0
        return max(xs) - min(xs)

    out = {
        "ok": fail_enum == 0 and fail_msc <= max(1, args.cycles // 20) and bool(heal.get("ok")),
        "claim": "hotplug_soak_lab",
        "cycles": args.cycles,
        "fail_enum": fail_enum,
        "fail_msc": fail_msc,
        "recovery_ms_p50": statistics.median(recovery_ms) if recovery_ms else None,
        "recovery_ms_p95": (
            statistics.quantiles(recovery_ms, n=20)[18]
            if len(recovery_ms) >= 20
            else (max(recovery_ms) if recovery_ms else None)
        ),
        "host_heap_drift": drift(heap_host),
        "device_heap_drift": drift(heap_dev),
        "heal_ok": bool(heal.get("ok")),
        "rows": rows,
        "not_claimed": ["physical_unplug", "l0_wire", "mitm"],
    }

    md = Path(args.out)
    lines = [
        f"# Lab MEASURED — Hotplug/Soak · {h0.get('version')}",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**Cycles:** {args.cycles} · fail_enum={fail_enum} · fail_msc={fail_msc}",
        f"**Recovery ms p50/p95:** {out['recovery_ms_p50']} / {out['recovery_ms_p95']}",
        f"**Heap drift Host/Device:** {out['host_heap_drift']} / {out['device_heap_drift']}",
        f"**Final heal MSC:** {out['heal_ok']}",
        "",
        "## Verdict",
        "",
        "- Soft-reenum approximates hotplug on OTG loop (not physical unplug).",
        "- Pass if enum failures=0 and MSC fails ≤5% and final heal ok.",
        "",
        "**Not claimed:** physical VBUS unplug · L0 · MITM.",
        "",
        f"JSON: `{md.with_suffix('.json').name}`",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    md.with_suffix(".json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    print("wrote", md, "ok=", out["ok"])
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
