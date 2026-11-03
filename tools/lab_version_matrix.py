#!/usr/bin/env python3
"""Archive a clean lab version matrix (Analyzer Host/Device + optional peers).

Captures live /api/status (+ controller health) into docs/evidence/.
Optional: record local git HEAD / VERSION without flashing.

Claim: lab_version_matrix — NOT a rebuild proof unless --note includes rebuild.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 8) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def git_head(path: Path) -> dict:
    if not (path / ".git").exists():
        return {"path": str(path), "err": "no_git"}
    def run(args: list[str]) -> str:
        p = subprocess.run(args, cwd=str(path), capture_output=True, text=True)
        return (p.stdout or "").strip() if p.returncode == 0 else ""

    return {
        "path": str(path),
        "head": run(["git", "rev-parse", "HEAD"])[:12],
        "branch": run(["git", "rev-parse", "--abbrev-ref", "HEAD"]),
        "dirty": bool(run(["git", "status", "--porcelain"])),
        "describe": run(["git", "describe", "--always", "--dirty"]),
    }


def node_snap(ip: str) -> dict:
    s = get(f"http://{ip}/api/status")
    if s.get("err"):
        return {"ip": ip, "ok": False, "err": s.get("err")}
    return {
        "ip": ip,
        "ok": True,
        "name": s.get("name"),
        "version": s.get("version") or s.get("fw"),
        "fwType": s.get("fwType"),
        "role": s.get("role"),
        "modules": s.get("modules"),
        "git": s.get("git"),
        "sketchUsed": s.get("sketchUsed"),
        "fw_dirty": s.get("fw_dirty") or s.get("FW_GIT_DIRTY"),
        "resetReason": s.get("resetReason"),
        "restartCause": s.get("restartCause"),
        "bootGen": s.get("bootGen"),
        "usb_gadget": s.get("usb_gadget"),
        "otg": s.get("otg"),
        "dut": s.get("dut"),
        "heap": s.get("heap"),
        "heapMax": s.get("heapMax"),
        "psram": s.get("psram"),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--controller", default="127.0.0.1:8080")
    ap.add_argument("--note", default="live_snapshot")
    ap.add_argument(
        "--peers",
        default="",
        help="extra repo paths to record (comma), e.g. ../hu-probe,../pidrive",
    )
    ap.add_argument(
        "--out",
        default="",
    )
    args = ap.parse_args()

    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    if not args.out:
        args.out = str(ROOT / "docs/evidence" / f"LAB_VERSION_MATRIX_{ts}.json")

    ctrl = get(f"http://{args.controller}/api/health")
    if ctrl.get("err") or ctrl.get("ok") is False:
        # controller may use different health path
        ctrl = get(f"http://{args.controller}/")
        ctrl = {"reachable": "err" not in ctrl, "raw_keys": list(ctrl)[:8]}

    peers = []
    for p in [x.strip() for x in args.peers.split(",") if x.strip()]:
        peers.append(git_head((ROOT / p).resolve() if not Path(p).is_absolute() else Path(p)))

    out = {
        "ok": True,
        "claim": "lab_version_matrix",
        "timestamp_utc": ts,
        "note": args.note,
        "analyzer_repo": git_head(ROOT),
        "version_file": (ROOT / "VERSION").read_text(encoding="utf-8").strip()
        if (ROOT / "VERSION").exists()
        else None,
        "nodes": {
            "host": node_snap(args.host),
            "device": node_snap(args.device),
        },
        "controller": {"url": args.controller, **(ctrl if isinstance(ctrl, dict) else {})},
        "peers": peers,
        "not_claimed": [
            "identical_to_clean_rebuild_unless_note_says_so",
            "hu_field",
            "path_b_phy",
        ],
    }
    host_ok = bool(out["nodes"]["host"].get("ok"))
    dev_ok = bool(out["nodes"]["device"].get("ok"))
    out["ok"] = host_ok and dev_ok

    path = Path(args.out)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(out, indent=2, default=str), encoding="utf-8")
    md = path.with_suffix(".md")
    h = out["nodes"]["host"]
    d = out["nodes"]["device"]
    lines = [
        f"# Lab — Version matrix · {ts}",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**Note:** {args.note}",
        f"**Analyzer git:** {out['analyzer_repo'].get('head')} "
        f"dirty={out['analyzer_repo'].get('dirty')} · VERSION file={out.get('version_file')}",
        "",
        "| Node | IP | name | version | role/git | sketch |",
        "|------|----|------|---------|----------|--------|",
        f"| Host | {h.get('ip')} | {h.get('name')} | {h.get('version')} | "
        f"{h.get('role')}/{h.get('git')} | {h.get('sketchUsed')} |",
        f"| Device | {d.get('ip')} | {d.get('name')} | {d.get('version')} | "
        f"{d.get('role')}/{d.get('git')} | {d.get('sketchUsed')} |",
        "",
        f"**DUT:** {(h.get('dut') or {})}",
        f"**usb_gadget (device):** {d.get('usb_gadget')}",
        "",
        "**Not claimed:** clean rebuild identity unless note says rebuild was performed.",
        "",
        f"JSON: `{path.name}`",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    # stable pointer
    latest = ROOT / "docs/evidence/LAB_VERSION_MATRIX_MEASURED.md"
    latest.write_text(md.read_text(encoding="utf-8"), encoding="utf-8")
    (ROOT / "docs/evidence/LAB_VERSION_MATRIX_MEASURED.json").write_text(
        path.read_text(encoding="utf-8"), encoding="utf-8"
    )
    print(json.dumps({"ok": out["ok"], "wrote": str(path), "host": h.get("version"),
                      "device": d.get("version")}, indent=2))
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
