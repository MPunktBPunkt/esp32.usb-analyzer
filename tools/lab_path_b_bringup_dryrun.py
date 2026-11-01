#!/usr/bin/env python3
"""Path-B HW bring-up dry-run (no new hardware required).

Validates offline decoder/correlator + golden fixture + optional live Host
GPIO lab / ingest protocol so HW arrival is a plug-in step.

Claim: path_b_bringup_dryrun_lab
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.correlate import correlate  # noqa: E402
from usb_fs_l0.decoder import decode_nrzi_bits  # noqa: E402
from usb_fs_l0.gen_synth import synth_get_descriptor_states  # noqa: E402
from usb_fs_l0.sample_io import line_states_from_file, write_line_states  # noqa: E402


def get(url: str, timeout: float = 10) -> dict | None:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception:  # noqa: BLE001
        return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--skip-live", action="store_true")
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_PATH_B_BRINGUP_DRYRUN.md"),
    )
    args = ap.parse_args()

    checks: list[dict] = []

    # 1) Host unit tests
    ut = subprocess.run(
        [sys.executable, str(ROOT / "tests/host/test_usb_fs_l0.py")],
        capture_output=True,
        text=True,
        cwd=str(ROOT),
    )
    checks.append(
        {
            "name": "host_unit_usb_fs_l0",
            "ok": ut.returncode == 0,
            "detail": (ut.stdout + ut.stderr)[-400:],
        }
    )

    # 2) Golden fixture roundtrip
    fixture = ROOT / "tests/host/fixtures/synth_setup.l0samp"
    fixture.parent.mkdir(parents=True, exist_ok=True)
    states = synth_get_descriptor_states()
    write_line_states(fixture, 12_000_000, states)
    rate, got = line_states_from_file(fixture)
    pkts = decode_nrzi_bits(got)
    checks.append(
        {
            "name": "golden_fixture",
            "ok": rate == 12_000_000 and len(pkts) >= 3 and pkts[0]["pid"] == "SETUP",
            "packets": len(pkts),
            "path": str(fixture.relative_to(ROOT)),
        }
    )

    # 3) Correlator classification
    l1 = [
        {"type": "enum_info", "timestamp_us": 1, "note": "get_descriptor"},
        {"type": "enum_info", "timestamp_us": 2, "note": "ep0"},
    ]
    corr = correlate(pkts, l1, ignore_time=True)
    checks.append(
        {
            "name": "correlate_classify",
            "ok": corr["matched"] >= 1
            and "true_gap_n" in corr
            and "handshake_expected_n" in corr,
            "matched": corr["matched"],
            "true_gap_n": corr.get("true_gap_n"),
            "handshake_expected_n": corr.get("handshake_expected_n"),
        }
    )

    # 4) Optional live Host reachability / GPIO lab endpoint
    if not args.skip_live:
        st = get(f"http://{args.host}/api/status")
        live_ok = bool(st and st.get("version"))
        checks.append(
            {
                "name": "live_host",
                "ok": live_ok,
                "version": (st or {}).get("version"),
                "otg": (st or {}).get("otg"),
            }
        )

    # 5) Docs / BOM present
    bom = (ROOT / "docs/hardware/L0_CAPTURE_POC.md").is_file()
    proto = (ROOT / "tools/usb_fs_l0/path_b_protocol.md").is_file()
    checks.append({"name": "docs_bom_protocol", "ok": bom and proto})

    # Bring-up checklist when HW arrives
    checklist = [
        "Solder Hi-Z tap per docs/hardware/L0_CAPTURE_POC.md BOM",
        "Feed VP/VM digital into ESP GPIO10/11 (or sampler)",
        "Start: tools/l0_path_b_ingest.py --decode",
        "TX/RX: Host /api/lab/path-b-gpio-rx or external sampler → L0ST :9760",
        "Verify: tools/lab_l0_live_pipeline.py --source ingest",
        "Gap: tools/lab_l0_stack_gap.py or soft campaign with real .l0samp",
    ]

    all_ok = all(c.get("ok") for c in checks)
    out = {
        "ok": all_ok,
        "claim": "path_b_bringup_dryrun_lab",
        "checks": checks,
        "hw_arrival_checklist": checklist,
        "not_claimed": ["path_b_soldered", "live_wire_capture"],
    }

    md = Path(args.out)
    lines = [
        "# Lab MEASURED — Path B bring-up dry-run (no HW)",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**All checks:** {'PASS' if all_ok else 'FAIL'}",
        "",
        "## Checks",
        "",
    ]
    for c in checks:
        lines.append(f"- `{c['name']}`: {'ok' if c.get('ok') else 'FAIL'}")
    lines += [
        "",
        "## When HW arrives",
        "",
    ]
    for step in checklist:
        lines.append(f"1. {step}")
    lines += [
        "",
        "**Not claimed:** soldered Path B · live D+/D− FACT.",
        "",
        f"JSON: `{md.with_suffix('.json').name}`",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    md.with_suffix(".json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    print("wrote", md, "ok=", all_ok)
    for c in checks:
        print(f"  {c['name']}: {c.get('ok')}")
    return 0 if all_ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
