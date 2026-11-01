#!/usr/bin/env python3
"""Live L0 decode + L1 correlate pipeline (Gate 10F live path).

Sources:
  sim     — Path B sampler sim → ingest latest (or in-process synth)
  ingest  — read logs/l0/latest.l0samp
  file    — explicit .l0samp

Always pulls ESP L1 capture (host) and runs correlator.

Claim: l0_live_pipeline_lab — sim/ingest MEASURED; live PHY still needs Path B HW.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.correlate import correlate  # noqa: E402
from usb_fs_l0.decoder import decode_nrzi_bits  # noqa: E402
from usb_fs_l0.gen_synth import synth_get_descriptor_states  # noqa: E402
from usb_fs_l0.sample_io import line_states_from_file  # noqa: E402
from usb_fs_l0.schema import batch_dict  # noqa: E402


def get_json(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=10) as r:
        return json.loads(r.read().decode())


def post_empty(url: str) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", choices=("sim", "ingest", "file"), default="sim")
    ap.add_argument("--file", default="")
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--ingest-dir", default=str(ROOT / "logs" / "l0"))
    ap.add_argument("--out", default=str(ROOT / "docs/evidence/LAB_L0_LIVE_PIPELINE_MEASURED.md"))
    ap.add_argument("--trigger-ep0", action="store_true",
                    help="fire Host usb-enum-observe before capture pull")
    args = ap.parse_args()

    # 1) Optionally poke Host stack so L1 has fresh enum markers
    if args.trigger_ep0:
        try:
            r = post_empty(f"http://{args.host}/api/lab/usb-enum-observe?ep0=1")
            print("trigger ep0", r.get("ok"), r.get("err"))
        except Exception as e:  # noqa: BLE001
            print("trigger ep0 warn", e)

    # 2) Obtain L0 line states
    if args.source == "file" or (args.source == "ingest" and args.file):
        path = Path(args.file or (Path(args.ingest_dir) / "latest.l0samp"))
        rate, states = line_states_from_file(path)
        src_note = f"file:{path}"
    elif args.source == "ingest":
        latest = Path(args.ingest_dir) / "latest.l0samp"
        if not latest.is_file():
            print("FAIL: no latest.l0samp — start ingest + sampler_sim first")
            return 2
        rate, states = line_states_from_file(latest)
        src_note = f"ingest:{latest}"
    else:
        # in-process sim (+ optional push to ingest if running)
        states = synth_get_descriptor_states()
        rate = 12_000_000
        src_note = "sim_inprocess"
        try:
            subprocess.run(
                [sys.executable, str(ROOT / "tools/l0_path_b_sampler_sim.py")],
                check=False, timeout=5, capture_output=True,
            )
        except Exception:
            pass

    packets = decode_nrzi_bits(states)
    print(f"L0 packets={len(packets)} rate={rate} src={src_note}")

    # 3) L1 capture from Host
    try:
        cap = get_json(f"http://{args.host}/api/capture?limit=48")
    except Exception as e:  # noqa: BLE001
        print("FAIL L1 capture", e)
        return 1
    events = cap.get("events") or []
    print(f"L1 events={len(events)}")

    # 4) Correlate — synth/ingest L0 clocks are ordinal; ESP L1 uses µs uptime.
    ignore_time = args.source in ("sim", "ingest") or src_note.startswith("sim")
    result = correlate(packets, events, ignore_time=ignore_time)
    result["source"] = src_note
    result["l0_rate_hz"] = rate
    result["claim"] = "l0_live_pipeline_lab"
    print(json.dumps(result, indent=2))

    out = {
        "ok": result.get("matched", 0) >= 1,
        "pipeline": result,
        "l0_batch": batch_dict(packets),
        "l1_n": len(events),
    }
    out_json = Path(args.ingest_dir) / "last_pipeline.json"
    out_json.parent.mkdir(parents=True, exist_ok=True)
    out_json.write_text(json.dumps(out, indent=2))

    # evidence markdown
    md = Path(args.out)
    md.write_text(
        "# Lab MEASURED — L0 live pipeline (0.0.39)\n\n"
        f"**Source:** `{src_note}`  \n"
        f"**L0 packets:** {len(packets)}  \n"
        f"**L1 events:** {len(events)}  \n"
        f"**Matched:** {result.get('matched')}  \n\n"
        "Claim: `l0_live_pipeline_lab` — Path B **sim/ingest** MEASURED.  \n"
        "**Not claimed:** live D+/D− PHY tap until external front-end soldered.\n"
    )
    if result.get("matched", 0) < 1:
        print("FAIL: no L0↔L1 matches")
        return 1
    print("RESULT OK — live pipeline")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
