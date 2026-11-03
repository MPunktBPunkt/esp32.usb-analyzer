#!/usr/bin/env python3
"""Release gate — run capability audit before tagging FW.

Exit 0 only if lab_capability_audit.py returns 0.
Writes a short gate stamp under docs/evidence/.

Usage:
  python3 tools/release_gate_capability.py
  python3 tools/release_gate_capability.py --skip-heavy
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--skip-heavy", action="store_true")
    args = ap.parse_args()

    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    cmd = [
        sys.executable,
        str(ROOT / "tools/lab_capability_audit.py"),
        "--host",
        args.host,
        "--device",
        args.device,
    ]
    if args.skip_heavy:
        cmd.append("--skip-heavy")

    print("release_gate:", " ".join(cmd), flush=True)
    p = subprocess.run(cmd, cwd=str(ROOT))
    stamp = {
        "ok": p.returncode == 0,
        "claim": "release_gate_capability",
        "timestamp_utc": ts,
        "returncode": p.returncode,
        "cmd": cmd,
        "rule": "Do not tag/release analyzer FW unless capability audit is green on OTG loop.",
        "not_claimed": ["path_b_phy", "hu_wire", "usb_if"],
    }
    out = ROOT / "docs/evidence" / f"LAB_RELEASE_GATE_{ts}.json"
    out.write_text(json.dumps(stamp, indent=2), encoding="utf-8")
    latest = ROOT / "docs/evidence/LAB_RELEASE_GATE_MEASURED.json"
    latest.write_text(out.read_text(encoding="utf-8"), encoding="utf-8")
    md = ROOT / "docs/evidence/LAB_RELEASE_GATE_MEASURED.md"
    md.write_text(
        "\n".join(
            [
                f"# Release gate — capability audit · {ts}",
                "",
                f"**Claim:** `{stamp['claim']}`",
                f"**ok:** {stamp['ok']} · returncode={p.returncode}",
                "",
                "Rule: FW tag only if OTG-loop capability audit is green.",
                "",
                f"JSON: `{out.name}`",
                "",
            ]
        ),
        encoding="utf-8",
    )
    print("WROTE", latest, "ok=", stamp["ok"])
    return p.returncode


if __name__ == "__main__":
    raise SystemExit(main())
