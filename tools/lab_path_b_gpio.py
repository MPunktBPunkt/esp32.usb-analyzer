#!/usr/bin/env python3
"""Path B GPIO lab — Host bit-bangs synth FS line-states on free GPIOs.

Answers: can the Host ESP deliberately drive slow D+/D− *test* signals on
GPIO10/11 (not OTG 19/20), sample them back, and push L0ST to ingest?

Claim: path_b_gpio_lab — NOT live OTG PHY capture / NOT Path A.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 60) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--mode", default="txrx_l0st",
                    choices=("tx", "txrx", "txrx_l0st"))
    ap.add_argument("--rate", type=int, default=100000, help="target bitbang Hz")
    ap.add_argument("--dp", type=int, default=10)
    ap.add_argument("--dm", type=int, default=11)
    ap.add_argument("--l0st-host", default="192.168.178.187")
    ap.add_argument("--l0st-port", type=int, default=9760)
    ap.add_argument("--ingest", action="store_true",
                    help="start local ingest before l0st push")
    ap.add_argument("--decode", action="store_true",
                    help="run live pipeline after push")
    ap.add_argument(
        "--out",
        default="docs/evidence/LAB_PATH_B_GPIO_LAB_MEASURED.md",
    )
    args = ap.parse_args()

    ingest_proc = None
    if args.ingest or args.mode == "txrx_l0st":
        out_dir = ROOT / "logs" / "l0"
        out_dir.mkdir(parents=True, exist_ok=True)
        ingest_proc = subprocess.Popen(
            [
                sys.executable,
                str(ROOT / "tools" / "l0_path_b_ingest.py"),
                "--listen",
                f"0.0.0.0:{args.l0st_port}",
                "--out",
                str(out_dir),
                "--decode",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        time.sleep(0.6)

    st = get(f"http://{args.host}/api/status")
    print("host", st.get("version"), st.get("role"))
    q = urllib.parse.urlencode(
        {
            "mode": args.mode,
            "rate": args.rate,
            "dp": args.dp,
            "dm": args.dm,
            "l0st_host": args.l0st_host,
            "l0st_port": args.l0st_port,
        }
    )
    r = get(f"http://{args.host}/api/lab/path-b-gpio?{q}", timeout=120)
    print(json.dumps(r, indent=2))

    pipeline = None
    if args.decode and r.get("l0stOk"):
        time.sleep(0.3)
        try:
            pipeline = subprocess.check_output(
                [
                    sys.executable,
                    str(ROOT / "tools" / "lab_l0_live_pipeline.py"),
                    "--source",
                    "ingest",
                    "--ingest-dir",
                    str(ROOT / "logs" / "l0"),
                    "--out",
                    str(ROOT / "docs" / "evidence" / "LAB_L0_LIVE_PIPELINE_GPIO.md"),
                ],
                text=True,
                timeout=60,
            )
            print(pipeline)
        except Exception as e:
            print("pipeline note:", e)

    if ingest_proc:
        ingest_proc.terminate()
        try:
            ingest_proc.wait(timeout=3)
        except Exception:
            ingest_proc.kill()

    if not r.get("ok"):
        print("FAIL", r.get("err"))
        return 1
    if args.mode != "tx" and not r.get("loopbackOk"):
        print("FAIL loopback mismatches", r.get("mismatches"))
        return 1

    md = Path(args.out)
    md.parent.mkdir(parents=True, exist_ok=True)
    md.write_text(
        f"""# Lab MEASURED — Path B GPIO lab substitute (0.0.41)

**Tool:** `tools/lab_path_b_gpio.py` → `GET /api/lab/path-b-gpio`  
**Host FW:** `{st.get("version")}`  
**Pins:** DP=GPIO{r.get("dpPin")} · DM=GPIO{r.get("dmPin")} (**not** OTG 19/20)

## Question

Can the Host ESP deliberately drive slow synthetic FS line-states on free GPIOs
for decoder/ingest tests?

## Result

| Check | Value |
|-------|-------|
| Mode | {r.get("mode")} |
| Target rate | {r.get("rateHz")} Hz |
| Achieved rate | **{r.get("achievedRateHz")} Hz** |
| States TX/RX | {r.get("statesTx")} / {r.get("statesRx")} |
| Loopback mismatches | {r.get("mismatches")} · ok={r.get("loopbackOk")} |
| L0ST push | ok={r.get("l0stOk")} · bytes={r.get("l0stBytes")} → {r.get("l0stHost")}:{r.get("l0stPort")} |
| Duration | {r.get("durationUs")} µs |

## Verdict

- **Yes** — Host can bit-bang known SETUP/DATA0/ACK line-states on free GPIOs and
  self-sample them (digital loopback) for automated Path-B pipe tests.
- Rate is lab-slow (CPU bit-bang), not FS 12 MHz wire capture.
- Does **not** replace a Hi-Z passive tap on the real OTG bus.

**Claim:** `path_b_gpio_lab`  
**Not claimed:** live OTG D+/D− PHY · Path A · HU.
"""
    )
    print(f"wrote {md}")
    print("RESULT OK — path_b_gpio_lab")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
