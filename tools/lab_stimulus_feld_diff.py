#!/usr/bin/env python3
"""Diff Analyzer BMW-HU stimulus vs hu-probe Feld P-MOUNT / P-MREAD grammar.

Desk FACT source: hu-probe docs/feld/2026-10-19-unabhaengige-auswertung.md
  P-MOUNT: INQUIRY×1, READ_CAPACITY10×3, MODE_SENSE6×6, PREVENT_ALLOW×7, TUR≈32
  P-MREAD: 0/8,0/8,24/8,0/8,0/1,1/3,7/2,1/1,2/1,3/5,24/8

Runs Host /api/lab/bmw-hu-stimulus and scores emitted counts vs FACT.
Optionally snaps hu-probe msc_reads/read_count delta.

Claim: bmw_stimulus_feld_diff_lab — NOT real HU wire equality.
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# FACT multiset (counts) — mount phase
P_MOUNT = {
    "INQUIRY": 1,
    "READ_CAPACITY10": 3,
    "MODE_SENSE6": 6,
    "PREVENT_ALLOW": 7,
    "TUR": 32,
}

# FACT ordered READ10 lba/blocks
P_MREAD = [
    (0, 8),
    (0, 8),
    (24, 8),
    (0, 8),
    (0, 1),
    (1, 3),
    (7, 2),
    (1, 1),
    (2, 1),
    (3, 5),
    (24, 8),
]


def get(url: str, timeout: float = 20) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def post(url: str, timeout: float = 300) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        try:
            return json.loads(e.read().decode())
        except Exception:
            return {"ok": False, "err": f"http_{e.code}"}
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--profile", default="bmw_mount_scan")
    ap.add_argument("--tur", type=int, default=32)
    ap.add_argument("--file-lba0", type=int, default=9)
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_STIMULUS_FELD_DIFF_MEASURED.md"),
    )
    args = ap.parse_args()

    h0 = get(f"http://{args.host}/api/status")
    d0 = get(f"http://{args.device}/api/status")
    before = {
        "msc_reads": d0.get("msc_reads"),
        "read_count": d0.get("read_count"),
        "usb_gadget": d0.get("usb_gadget"),
        "fw": d0.get("fw") or d0.get("version"),
    }
    print("host", h0.get("version"), "dut", (h0.get("dut") or {}).get("product"))
    print("device", before)

    q = urllib.parse.urlencode(
        {
            "profile": args.profile,
            "tur": args.tur,
            "scan_blocks": 0 if args.profile == "bmw_mount" else 160,
            "file_lba0": args.file_lba0,
        }
    )
    # Use bmw_mount for pure P-MOUNT/P-MREAD counts (no extra scan)
    if args.profile.startswith("bmw_mount") and "scan" not in args.profile and "deep" not in args.profile:
        q = urllib.parse.urlencode({"profile": "bmw_mount", "tur": args.tur})

    t0 = time.time()
    stim = post(f"http://{args.host}/api/lab/bmw-hu-stimulus?{q}", timeout=300)
    wall = time.time() - t0
    time.sleep(0.4)
    d1 = get(f"http://{args.device}/api/status")
    after = {
        "msc_reads": d1.get("msc_reads"),
        "read_count": d1.get("read_count"),
    }
    delta = {
        "msc_reads": (after.get("msc_reads") or 0) - (before.get("msc_reads") or 0),
        "read_count": (after.get("read_count") or 0) - (before.get("read_count") or 0),
    }

    emitted = {
        "INQUIRY": int(stim.get("inquiryOk") or 0),
        "READ_CAPACITY10": int(stim.get("capacityOk") or 0),
        "MODE_SENSE6": int(stim.get("modeSenseOk") or 0),
        "PREVENT_ALLOW": int(stim.get("preventOk") or 0),
        "TUR": int(stim.get("turOk") or 0),
        "READ10_ok": int(stim.get("readOk") or 0),
    }
    fails = {
        "MODE_SENSE6_fail": int(stim.get("modeSenseFail") or 0),
        "PREVENT_fail": int(stim.get("preventFail") or 0),
        "TUR_fail": int(stim.get("turFail") or 0),
        "READ_fail": int(stim.get("readFail") or 0),
    }

    rows = []
    mount_ok = True
    for op, want in P_MOUNT.items():
        got = emitted.get(op, 0)
        # TUR: allow >= want-2 if burst slightly short
        if op == "TUR":
            ok = got >= max(1, want - 2)
        else:
            ok = got >= want
        if not ok:
            mount_ok = False
        rows.append({"op": op, "want": want, "got": got, "ok": ok})

    pmread_n = len(P_MREAD)
    read_ok = emitted["READ10_ok"] >= pmread_n and fails["READ_fail"] == 0
    mode_prevent_present = (
        emitted["MODE_SENSE6"] >= P_MOUNT["MODE_SENSE6"]
        and emitted["PREVENT_ALLOW"] >= P_MOUNT["PREVENT_ALLOW"]
    )

    out = {
        "ok": bool(stim.get("ok")) and mount_ok and read_ok,
        "claim": "bmw_stimulus_feld_diff_lab",
        "timestamp_utc": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
        "feld_source": "hu-probe/docs/feld/2026-10-19-unabhaengige-auswertung.md",
        "profile": stim.get("profile") or args.profile,
        "wall_s": round(wall, 3),
        "p_mount_rows": rows,
        "p_mread_expected_n": pmread_n,
        "p_mread_pairs": P_MREAD,
        "emitted": emitted,
        "fails": fails,
        "mode_prevent_in_stimulus": mode_prevent_present,
        "dut_delta": delta,
        "stimulus": {k: stim.get(k) for k in (
            "ok", "inquiryOk", "capacityOk", "turOk", "turFail", "senseOk",
            "modeSenseOk", "modeSenseFail", "preventOk", "preventFail",
            "readOk", "readFail", "bytesRead", "duration_ms", "err", "note",
        )},
        "not_claimed": [
            "byte_identical_to_nbt_trace",
            "timing_identical_to_hu",
            "real_hu_wire",
        ],
        "verdict": (
            "P-MOUNT/P-MREAD counts covered incl. MODE_SENSE6+PREVENT"
            if mode_prevent_present and mount_ok and read_ok
            else "gap vs Feld FACT — see rows"
        ),
    }

    md = Path(args.out)
    lines = [
        f"# Lab MEASURED — Stimulus ↔ Feld P-MOUNT/P-MREAD diff · {h0.get('version')}",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**Stimulus ok:** {stim.get('ok')} · mount_ok={mount_ok} · pmread_ok={read_ok} · "
        f"mode+prevent={mode_prevent_present}",
        f"**DUT delta:** msc_reads={delta['msc_reads']} read_count={delta['read_count']}",
        "",
        "## P-MOUNT (Feld FACT vs emitted)",
        "",
        "| Op | want | got | ok |",
        "|----|-----:|----:|:--:|",
    ]
    for row in rows:
        lines.append(
            f"| {row['op']} | {row['want']} | {row['got']} | {'yes' if row['ok'] else 'NO'} |"
        )
    lines += [
        "",
        f"**P-MREAD:** expect {pmread_n} READ10 · got readOk={emitted['READ10_ok']} "
        f"fail={fails['READ_fail']}",
        "",
        "## Epistemik",
        "",
        "- Feld FACT from hu-probe 2026-10-19 independent analysis (all BMW mounts).",
        "- Analyzer emits lab approximation — not timing/wire identity with NBT.",
        "- MODE_SENSE6/PREVENT required by P-MOUNT FACT; shipped via mscCommand lab unlock.",
        "",
        f"JSON: `{md.with_suffix('.json').name}`",
        "",
        "```json",
        json.dumps(out, indent=2, default=str),
        "```",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    md.with_suffix(".json").write_text(
        json.dumps(out, indent=2, default=str), encoding="utf-8"
    )
    print(json.dumps({
        "ok": out["ok"],
        "mount_ok": mount_ok,
        "read_ok": read_ok,
        "mode_prevent": mode_prevent_present,
        "emitted": emitted,
        "dut_delta": delta,
    }, indent=2))
    print("WROTE", md)
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
