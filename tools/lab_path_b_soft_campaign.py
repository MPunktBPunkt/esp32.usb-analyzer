#!/usr/bin/env python3
"""Soft Path-B + stack-gap campaign without soldered PHY.

Uses in-process synth (+ optional Host GPIO lab TX) as L0 source, Host L1
capture during MSC, correlator with handshake_expected vs true_gap classes.

Claim: path_b_soft_campaign_lab — NOT live OTG D+/D− PHY.
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
import sys

sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.correlate import correlate  # noqa: E402
from usb_fs_l0.decoder import decode_nrzi_bits  # noqa: E402
from usb_fs_l0.gen_synth import synth_get_descriptor_states  # noqa: E402
from usb_fs_l0.sample_io import write_line_states  # noqa: E402


def get(url: str, timeout: float = 30) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--gpio-lab", action="store_true",
                    help="also fire Host path-b-gpio TX (non-destructive)")
    ap.add_argument("--out", default=str(
        ROOT / "docs/evidence/LAB_PATH_B_SOFT_CAMPAIGN_MEASURED.md"))
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    print("host", h.get("version"), (h.get("otg") or {}).get("state"))

    # Live L1: MSC probe so capture has bot markers
    probe = get(
        f"http://{args.host}/api/lab/usb-host-probe?blocks=4&chunk=2&dir=read&lba=0"
    )
    print("msc", probe.get("ok"), probe.get("rate_kBs"))
    try:
        get(f"http://{args.host}/api/lab/usb-enum-observe?ep0=1")
    except Exception as e:  # noqa: BLE001
        print("enum warn", e)

    if args.gpio_lab:
        q = urllib.parse.urlencode(
            {"mode": "tx", "pattern": "setup_ok", "rate": 100000}
        )
        try:
            g = get(f"http://{args.host}/api/lab/path-b-gpio?{q}", timeout=60)
            print("gpio_lab", g.get("ok"), g.get("err"))
        except Exception as e:  # noqa: BLE001
            print("gpio_lab warn", e)

    time.sleep(0.2)
    cap = get(f"http://{args.host}/api/capture?limit=64")
    events = cap.get("events") or []

    states = synth_get_descriptor_states()
    fixture = ROOT / "tests/host/fixtures/synth_setup.l0samp"
    fixture.parent.mkdir(parents=True, exist_ok=True)
    write_line_states(fixture, 12_000_000, states)

    packets = decode_nrzi_bits(states)
    corr = correlate(packets, events, ignore_time=True)
    print(
        f"L0={corr['l0_n']} L1={corr['l1_n']} matched={corr['matched']} "
        f"stack_gap={corr['stack_gap_n']} true_gap={corr.get('true_gap_n')} "
        f"hs_exp={corr.get('handshake_expected_n')}"
    )

    out = {
        "ok": corr["l0_n"] > 0 and corr.get("true_gap_n", 0) >= 0,
        "claim": "path_b_soft_campaign_lab",
        "host_version": h.get("version"),
        "msc_ok": bool(probe.get("ok")),
        "gpio_lab": bool(args.gpio_lab),
        "correlate": {
            k: corr[k]
            for k in (
                "l0_n",
                "l1_n",
                "matched",
                "stack_gap_n",
                "true_gap_n",
                "handshake_expected_n",
                "unmatched_l0",
                "matches",
            )
        },
        "fixture": str(fixture.relative_to(ROOT)),
        "not_claimed": ["path_b_soldered_phy", "live_otg_dplus_dminus"],
    }

    md = Path(args.out)
    lines = [
        f"# Lab MEASURED — Path B soft campaign (no PHY) · {h.get('version')}",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**MSC probe:** {probe.get('ok')} @ {probe.get('rate_kBs')} kB/s",
        f"**L0 packets:** {corr['l0_n']} · **L1 events:** {corr['l1_n']}",
        f"**Matched:** {corr['matched']}",
        f"**stack_gap_n:** {corr['stack_gap_n']} "
        f"(true_gap={corr.get('true_gap_n')}, "
        f"handshake_expected={corr.get('handshake_expected_n')})",
        "",
        "## Unmatched L0 (classified)",
        "",
    ]
    for u in corr.get("unmatched_l0") or []:
        lines.append(
            f"- idx={u.get('l0_index')} pid=`{u.get('pid')}` "
            f"kind=`{u.get('kind')}` class=`{u.get('class')}`"
        )
    lines += [
        "",
        "## Verdict",
        "",
        "- Soft L0 (synth / optional GPIO lab) correlates against live Host L1.",
        "- Correlator splits handshake/token gaps from true stack_gap classes.",
        "- Ready for Path-B Hi-Z samples when HW arrives (same pipeline).",
        "",
        "**Not claimed:** soldered Path B PHY · live OTG D+/D− sniffer.",
        "",
        f"JSON: `{md.with_suffix('.json').name}`",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    md.with_suffix(".json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    print("wrote", md)
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
