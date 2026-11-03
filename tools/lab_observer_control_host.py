#!/usr/bin/env python3
"""Control-host Observer method — PC↔Stick transparency gate (checklist + optional probes).

OTG-loop observer baseline is MEASURED but ≠ transparent MITM. This tool:
  - emits a control-host checklist evidence stub
  - optionally verifies Analyzer Host sees a non-analyzer FS stick (pc_stick topology)
  - never claims transparency without a Diff vs direct PC capture

Claim: observer_control_host_method_lab
"""
from __future__ import annotations

import argparse
import json
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CHECKLIST = [
    {
        "id": "CH1",
        "item": "FS stick on Analyzer Host OTG; Device gadget disconnected or parked",
        "needed_for": "direct_host_baseline",
    },
    {
        "id": "CH2",
        "item": "Same stick on PC (usbmon/Wireshark); capture mount+READ reference",
        "needed_for": "pc_direct_oracle",
    },
    {
        "id": "CH3",
        "item": "MITM path: PC→Device-gadget ↔ WiFi/proxy ↔ Host→stick; compare CBW/CSW/data",
        "needed_for": "transparency_diff",
    },
    {
        "id": "CH4",
        "item": "Tag every rate with topology=pc_stick_mitm|otg_loop — never mix claims",
        "needed_for": "claim_discipline",
    },
    {
        "id": "CH5",
        "item": "Only after byte/order Diff pass: raise claim to transparent_mitm_lab",
        "needed_for": "promotion_gate",
    },
]


def get(url: str, timeout: float = 10) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument(
        "--expect-stick",
        action="store_true",
        help="Require Host DUT to be a non-analyzer mass-storage stick",
    )
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_OBSERVER_CONTROL_HOST_METHOD.md"),
    )
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    dut = h.get("dut") or {}
    vidpid = str(dut.get("id") or "")
    product = str(dut.get("product") or "")
    is_analyzer_dut = vidpid.upper() == "303A:1001" or "ANALYZER" in product.upper()

    probes = {
        "host_version": h.get("version"),
        "device_version": d.get("version"),
        "dut": dut,
        "is_analyzer_otg_loop": is_analyzer_dut,
        "device_otg": d.get("otg"),
    }

    ready_for_stick_path = bool(dut.get("present")) and not is_analyzer_dut
    if args.expect_stick and not ready_for_stick_path:
        ok = False
        err = "expected_non_analyzer_stick_on_host"
    else:
        ok = True
        err = None

    out = {
        "ok": ok,
        "err": err,
        "claim": "observer_control_host_method_lab",
        "timestamp_utc": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
        "topology_observed": "otg_loop_analyzer" if is_analyzer_dut else (
            "host_has_foreign_dut" if dut.get("present") else "no_dut"
        ),
        "checklist": CHECKLIST,
        "checklist_pass": False,
        "note": (
            "Checklist is the method contract. Transparency FACT only after CH1–CH5 "
            "with PC Diff — OTG-loop proxy Δ is insufficient."
        ),
        "probes": probes,
        "ready_for_stick_baseline": ready_for_stick_path,
        "not_claimed": [
            "transparent_mitm",
            "pc_stick_diff_done",
            "otg_loop_equals_mitm",
        ],
    }

    md = Path(args.out)
    lines = [
        "# Lab — Observer control-host method",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**Topology now:** {out['topology_observed']}",
        f"**Stick baseline ready:** {ready_for_stick_path}",
        "",
        "## Checklist (must all pass before transparency claim)",
        "",
    ]
    for c in CHECKLIST:
        lines.append(f"- [ ] **{c['id']}** {c['item']} _(for {c['needed_for']})_")
    lines += [
        "",
        "## Epistemik",
        "",
        "- OTG-loop observer MEASURED ≠ PC↔Stick MITM.",
        "- Promote claim only after Diff vs PC usbmon/Wireshark oracle.",
        "",
        "```json",
        json.dumps(out, indent=2, default=str),
        "```",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    md.with_suffix(".json").write_text(json.dumps(out, indent=2, default=str), encoding="utf-8")
    print(json.dumps({"ok": ok, "topology": out["topology_observed"],
                      "ready_for_stick_baseline": ready_for_stick_path, "err": err}, indent=2))
    print("WROTE", md)
    return 0 if ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
