#!/usr/bin/env python3
"""Experiment E1.1 / Gate 10A — GPIO19/20 poll while OTG owns PHY.

Baseline MSC → concurrent GPIO peek sample (+ MSC traffic) → post MSC.
Documents: bus stability, edge counts, reachable sample rate, poll jitter.

Claim: gate10a_e11_gpio_sample — NOT live FS L0 packet capture.
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.parse
import urllib.request
from pathlib import Path


def get(url: str, timeout: float = 30) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


def post(url: str, timeout: float = 180) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def msc_probe(host: str, blocks: int = 64, chunk: int = 4) -> dict:
    q = urllib.parse.urlencode(
        {"blocks": blocks, "lba": 0, "chunk": chunk, "dir": "read"}
    )
    return post(f"http://{host}/api/lab/usb-host-probe?{q}")


def capture_count(host: str) -> int:
    try:
        batch = get(f"http://{host}/api/capture?limit=200", timeout=10)
        ev = batch.get("events") or batch.get("items") or []
        if isinstance(ev, list):
            return len(ev)
        return int(batch.get("count") or 0)
    except Exception:
        return -1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--samples", type=int, default=200000)
    ap.add_argument("--traffic-blocks", type=int, default=64)
    ap.add_argument(
        "--out",
        default="docs/evidence/LAB_GATE10A_E11_GPIO_SAMPLE.md",
    )
    args = ap.parse_args()

    st = get(f"http://{args.host}/api/status")
    print(
        "host",
        st.get("version"),
        "dut",
        (st.get("dut") or {}).get("present"),
        "role",
        st.get("role"),
    )
    cap0 = capture_count(args.host)
    print("==> baseline MSC")
    base = msc_probe(args.host, blocks=32)
    print(
        "baseline",
        "ok" if base.get("ok") else "FAIL",
        f"rate={base.get('rate_kBs')} kB/s",
    )
    if not base.get("ok"):
        print("FAIL: baseline MSC required for E1.1")
        return 1

    print("==> E1.1 gpio sample (concurrent traffic)")
    q = urllib.parse.urlencode(
        {
            "samples": args.samples,
            "traffic_blocks": args.traffic_blocks,
        }
    )
    t0 = time.time()
    r = get(f"http://{args.host}/api/lab/e11-gpio-sample?{q}", timeout=180)
    wall = time.time() - t0
    print(json.dumps(r, indent=2))
    print(f"wall={wall:.2f}s")

    print("==> post MSC")
    post_m = msc_probe(args.host, blocks=32)
    print(
        "post",
        "ok" if post_m.get("ok") else "FAIL",
        f"rate={post_m.get('rate_kBs')} kB/s",
    )
    cap1 = capture_count(args.host)

    if not r.get("ok"):
        print("FAIL", r.get("err"))
        return 1
    if r.get("pathA_usable"):
        print("FAIL: pathA_usable unexpectedly true")
        return 1

    rate = int(r.get("rateKsps") or 0)
    jitter = int(r.get("jitterPpNs") or 0)
    edges = int(r.get("transitionsDm") or 0) + int(r.get("transitionsDp") or 0)
    destructive = bool(r.get("destructive"))
    bus_stable = bool(r.get("busStable")) and bool(post_m.get("ok"))

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    md = f"""# Lab MEASURED — Gate 10A / E1.1 GPIO sample (0.0.40)

**Tool:** `tools/lab_e11_gpio_sample.py` → `GET /api/lab/e11-gpio-sample`  
**Topology:** OTG Host↔Device loop · peek-only GPIO19/20 (no remux) · APP-CPU poll + MSC on PRO-CPU  
**Host FW:** `{st.get("version")}`

## Protocol

1. Baseline MSC READ (bus healthy)
2. GPIO19/20 poll burst (`samples={args.samples}`) concurrent with MSC (`traffic_blocks={args.traffic_blocks}`)
3. Post MSC READ (bus still healthy?)

## Results

| Check | Result |
|-------|--------|
| Baseline MSC | **{"OK" if base.get("ok") else "FAIL"}** · {base.get("rate_kBs")} kB/s |
| Sample rate | **{rate} ksps** ({r.get("samples")} samples / {r.get("durationUs")} µs) |
| Poll jitter (pp) | **{jitter} ns** (dt min/avg/max cy: {r.get("sampleDtMinCy")}/{r.get("sampleDtAvgCy")}/{r.get("sampleDtMaxCy")} @ {r.get("cpuMhz")} MHz) |
| Edges D−/D+ | {r.get("transitionsDm")} / {r.get("transitionsDp")} (sum={edges}) |
| Meaningful edges | {r.get("meaningfulEdges")} |
| Concurrent traffic | ok={r.get("trafficOk")} · {r.get("trafficRateKBs")} kB/s |
| Bus stable after | **{bus_stable}** · destructive={destructive} · postProbe={r.get("postProbeOk")} · post MSC={post_m.get("ok")} ({post_m.get("rate_kBs")} kB/s) |
| Path A usable | **{r.get("pathA_usable")}** — {r.get("pathA_reason")} |
| L1 capture window | before={cap0} after={cap1} events (batch) |

## Verdict

- Peek-only GPIO poll on internal PHY pins is **{"destructive" if destructive else "not bus-destructive"}** in this run.
- Achieved rate **{rate} ksps** vs FS need ≥**24000 ksps** (2× oversample) → **Path A falsified as FS sniffer**.
- Prefer **Path B** (external tap/PHY). Soft remux of GPIO19/20 away from USB was **not** tested (known to kill OTG).

**Claim:** `gate10a_e11_gpio_sample` · experiment **E1.1** · gate **10A**  
**Not claimed:** live D+/D− packet capture / MITM / HU.
"""
    out.write_text(md)
    print(f"wrote {out}")
    print("RESULT OK — E1.1 measured; Path A not usable as FS L0")
    print("claim: gate10a_e11_gpio_sample")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
