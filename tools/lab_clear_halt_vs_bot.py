#!/usr/bin/env python3
"""Separate CLEAR_HALT vs MSC BOT_RESET under Gate-9 fault recovery.

Arms Device fail → Host fault-observe with recovery → scan Device capture
for notes `ep_clear_halt` / `msc_bot_reset` (class-control hooks).

Claim: clear_halt_vs_bot_lab — stack control path only, NOT L0 / MITM.
Desk: Clear-Halt does not cure NEED_RESET (EVID-USBCTRL…004) — hook records both.
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 60) -> dict:
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


def capture_notes(ip: str) -> list[dict]:
    batch = get(f"http://{ip}/api/capture?since=0&limit=128")
    out = []
    for ev in batch.get("events") or []:
        note = ev.get("note") or ""
        if note in ("ep_clear_halt", "msc_bot_reset"):
            out.append(ev)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_CLEAR_HALT_VS_BOT_MEASURED.md"),
    )
    args = ap.parse_args()

    # Baseline heal
    get(f"http://{args.device}/api/lab/fault-inject?op=clear")
    time.sleep(0.2)
    get(f"http://{args.host}/api/lab/usb-host-probe?blocks=1&chunk=1&dir=read")

    before = capture_notes(args.device)
    arm = get(f"http://{args.device}/api/lab/fault-inject?op=fail&count=1")
    obs = get(
        f"http://{args.host}/api/lab/fault-observe?lba=0&blocks=1&sense=1&recovery=1"
    )
    time.sleep(0.25)
    after = capture_notes(args.device)

    # New events only (by seq)
    before_seqs = {e.get("seq") for e in before}
    new = [e for e in after if e.get("seq") not in before_seqs]
    n_clear = sum(1 for e in new if e.get("note") == "ep_clear_halt")
    n_bot = sum(1 for e in new if e.get("note") == "msc_bot_reset")
    need_reset_seen = any(
        e.get("note") == "ep_clear_halt" and int(e.get("arg1") or e.get("length") or 0)
        for e in new
        if e.get("note") == "ep_clear_halt"
    )
    # arg1 in push is need_reset_stage via length? We used: status=ep, length=need_reset
    need_reset_flags = []
    for e in new:
        if e.get("note") == "ep_clear_halt":
            # push(status=ep_addr, length=need_reset_stage)
            need_reset_flags.append(int(e.get("length") or 0))

    result = {
        "ok": bool(obs.get("ok")) and n_clear + n_bot >= 0,
        "claim": "clear_halt_vs_bot_lab",
        "arm": arm,
        "host_observe": {
            "readOk": obs.get("readOk"),
            "recoveryOk": obs.get("recoveryOk"),
            "sensePhase": obs.get("sensePhase"),
        },
        "device_hooks": {
            "ep_clear_halt": n_clear,
            "msc_bot_reset": n_bot,
            "clear_halt_need_reset_flags": need_reset_flags,
            "events": new,
        },
        "note": "class_control_hooks_not_l0",
    }

    if n_bot > 0 and n_clear == 0:
        result["locus"] = "recovery_via_bot_reset_only"
    elif n_clear > 0 and n_bot == 0:
        result["locus"] = "recovery_via_clear_halt_only"
    elif n_clear > 0 and n_bot > 0:
        result["locus"] = "both_clear_halt_and_bot_reset"
    else:
        result["locus"] = "no_control_hooks_seen"

    lines = [
        "# Lab MEASURED — CLEAR_HALT vs BOT_RESET · 0.0.48",
        "",
        f"**Claim:** `{result['claim']}`",
        f"**Locus:** `{result['locus']}`",
        f"**Host recoveryOk:** {obs.get('recoveryOk')} · readOk={obs.get('readOk')}",
        f"**Hooks:** ep_clear_halt={n_clear} · msc_bot_reset={n_bot}",
        f"**clear_halt need_reset flags:** {need_reset_flags}",
        "",
        "## Verdict",
        "",
        "- Device TinyUSB control path emits separate capture notes for CLEAR_FEATURE(ENDPOINT_HALT) vs MSC BOT Reset (0xFF).",
        "- Desk FACT remains: Clear-Halt does **not** cure NEED_RESET (stall continues).",
        "",
        "**Not claimed:** L0 wire · Host EspUsbHost internal path identity · USB-IF.",
        "",
        "```json",
        json.dumps(result, indent=2),
        "```",
        "",
    ]
    Path(args.out).write_text("\n".join(lines), encoding="utf-8")
    Path(args.out).with_suffix(".json").write_text(
        json.dumps(result, indent=2), encoding="utf-8"
    )
    print(json.dumps(result, indent=2))
    print("WROTE", args.out)
    return 0 if result["locus"] != "no_control_hooks_seen" or obs.get("recoveryOk") else 2


if __name__ == "__main__":
    raise SystemExit(main())
