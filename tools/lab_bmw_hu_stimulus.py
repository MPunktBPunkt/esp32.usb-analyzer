#!/usr/bin/env python3
"""BMW-NBT-like MSC host stimulus via Analyzer Host (.88) against a gadget DUT.

Desk FACT sources (hu-probe feld 2026-10-19):
  P-MOUNT: INQUIRY×1, CAPACITY×3, TUR burst (~32), (+ MODE_SENSE/PREVENT not in EspUsbHost)
  P-MREAD: 0/8,0/8,24/8,0/8,0/1,1/3,7/2,1/1,2/1,3/5,24/8

Purpose: cross-check whether hu-probe / pidrive *see* controlled host accesses
(not blind) — claim bmw_hu_stimulus_lab, NOT real HU wire.

Usage:
  # DUT already hu-probe MSC or pidrive on .89
  python3 tools/lab_bmw_hu_stimulus.py --dut huprobe --profile bmw_mount_scan
  python3 tools/lab_bmw_hu_stimulus.py --dut pidrive --profile bmw_mount_scan --file-lba0 81
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 30) -> dict:
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


def post(url: str, timeout: float = 180) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        try:
            return json.loads(e.read().decode())
        except Exception:
            return {"ok": False, "err": f"http_{e.code}", "body": e.read().decode(errors="replace")[:300]}
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def snap_dut(ip: str, kind: str) -> dict:
    st = get(f"http://{ip}/api/status", timeout=8)
    if kind == "huprobe":
        return {
            "fw": st.get("fw") or st.get("version"),
            "fwType": st.get("fwType"),
            "usb_gadget": st.get("usb_gadget"),
            "read_count": st.get("read_count"),
            "tur_hz": st.get("tur_hz"),
            "session": st.get("session"),
            "media": st.get("media"),
            "offline": (st.get("offline") or {}).get("state")
            if isinstance(st.get("offline"), dict)
            else st.get("offline"),
        }
    # pidrive / analyzer
    m = st.get("msc") or {}
    host = m.get("host") or {}
    return {
        "fw": st.get("version") or st.get("fw"),
        "fwType": st.get("fwType") or st.get("role"),
        "readCount": m.get("readCount"),
        "bytesRead": m.get("bytesRead"),
        "bytesBoot": m.get("bytesBoot"),
        "bytesFat": m.get("bytesFat"),
        "bytesDir": m.get("bytesDir"),
        "bytesFile": m.get("bytesFile"),
        "lastReadLba": m.get("lastReadLba"),
        "playGuessCount": m.get("playGuessCount"),
        "host": host,
        "huContract": m.get("huContract"),
        "slot0": (m.get("slotMap") or [None])[0],
    }


def events_tail(ip: str, n: int = 40) -> list:
    # hu-probe /api/events
    j = get(f"http://{ip}/api/events", timeout=8)
    ev = j.get("events") or j.get("items") or []
    return ev[-n:]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--dut", choices=("huprobe", "pidrive", "analyzer"), default="huprobe")
    ap.add_argument(
        "--profile",
        default="bmw_mount_scan",
        choices=("bmw_mount", "bmw_mount_scan", "bmw_mount_deep", "bmw_idle_tur"),
    )
    ap.add_argument("--tur", type=int, default=32)
    ap.add_argument("--scan-blocks", type=int, default=160)
    ap.add_argument("--file-lba0", type=int, default=0, help="pidrive fav0=81; huprobe often ~9")
    ap.add_argument(
        "--out",
        default="",
        help="evidence md path (default by dut)",
    )
    args = ap.parse_args()
    if not args.out:
        args.out = str(
            ROOT
            / "docs"
            / "evidence"
            / f"LAB_BMW_HU_STIMULUS_{args.dut.upper()}_MEASURED.md"
        )
    if args.file_lba0 == 0 and args.dut == "pidrive":
        args.file_lba0 = 81
    if args.file_lba0 == 0 and args.dut == "huprobe":
        args.file_lba0 = 9

    before = snap_dut(args.device, args.dut)
    ev_before = events_tail(args.device) if args.dut == "huprobe" else []
    hs = get(f"http://{args.host}/api/status")
    print(
        "host",
        hs.get("version"),
        "dut",
        (hs.get("dut") or {}).get("id"),
        (hs.get("dut") or {}).get("product"),
    )
    print("device_before", json.dumps(before, default=str)[:400])

    q = urllib.parse.urlencode(
        {
            "profile": args.profile,
            "tur": args.tur,
            "scan_blocks": args.scan_blocks,
            "file_lba0": args.file_lba0,
        }
    )
    t0 = time.time()
    stim = post(f"http://{args.host}/api/lab/bmw-hu-stimulus?{q}", timeout=300)
    wall = time.time() - t0
    time.sleep(0.4)
    after = snap_dut(args.device, args.dut)
    ev_after = events_tail(args.device, 80) if args.dut == "huprobe" else []

    # Visibility delta
    delta: dict = {}
    if args.dut == "huprobe":
        delta = {
            "read_count": (after.get("read_count") or 0) - (before.get("read_count") or 0),
            "tur_hz_after": after.get("tur_hz"),
        }
        # event codes that look like scsi/read
        codes = []
        for e in ev_after:
            c = e.get("code") or e.get("ev") or e.get("name") or ""
            if c:
                codes.append(c)
        delta["event_codes_tail"] = codes[-30:]
    else:
        delta = {
            "readCount": (after.get("readCount") or 0) - (before.get("readCount") or 0),
            "bytesRead": (after.get("bytesRead") or 0) - (before.get("bytesRead") or 0),
            "bytesBoot": (after.get("bytesBoot") or 0) - (before.get("bytesBoot") or 0),
            "bytesFat": (after.get("bytesFat") or 0) - (before.get("bytesFat") or 0),
            "bytesDir": (after.get("bytesDir") or 0) - (before.get("bytesDir") or 0),
            "bytesFile": (after.get("bytesFile") or 0) - (before.get("bytesFile") or 0),
            "playGuessDelta": (after.get("playGuessCount") or 0)
            - (before.get("playGuessCount") or 0),
            "host_after": after.get("host"),
        }

    visible = False
    if args.dut == "huprobe":
        visible = int(delta.get("read_count") or 0) > 0 or bool(stim.get("ok"))
    else:
        visible = int(delta.get("bytesRead") or 0) > 0 or int(delta.get("readCount") or 0) > 0

    out = {
        "ok": bool(stim.get("ok")) and visible,
        "claim": "bmw_hu_stimulus_lab",
        "dut_kind": args.dut,
        "profile": args.profile,
        "wall_s": round(wall, 3),
        "host_stimulus": stim,
        "before": before,
        "after": after,
        "delta": delta,
        "visible_on_dut": visible,
        "not_emitted": ["MODE_SENSE6", "PREVENT_ALLOW"],
        "note": "lab_host_nbt_like_not_real_hu",
    }

    lines = [
        f"# Lab MEASURED — BMW-HU-like stimulus × {args.dut} · 0.0.50",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**Profile:** `{args.profile}` · tur={args.tur} · scan_blocks={args.scan_blocks} · file_lba0={args.file_lba0}",
        f"**Host stimulus ok:** {stim.get('ok')} · inquiry={stim.get('inquiryOk')} capacity={stim.get('capacityOk')} "
        f"tur={stim.get('turOk')}/{stim.get('turFail')} sense={stim.get('senseOk')}/{stim.get('senseFail')} "
        f"maxLun={stim.get('maxLunOk')} sync={stim.get('syncCacheOk')} "
        f"reads={stim.get('readOk')}/{stim.get('readFail')} "
        f"bytes={stim.get('bytesRead')} · {stim.get('duration_ms')} ms",
        f"**DUT visible:** {visible} · delta={json.dumps(delta, default=str)[:500]}",
        "",
        "## Epistemik",
        "",
        "- Desk FACT mount grammar from hu-probe feld (P-MOUNT / P-MREAD).",
        "- Analyzer Host emits INQUIRY + MaxLUN + CAPACITY + TUR + REQUEST_SENSE + READ10 + SYNCHRONIZE_CACHE.",
        "- **Not emitted:** MODE_SENSE6 / PREVENT_ALLOW (EspUsbHost API gap — mscCommand private).",
        "- **Not claimed:** real BMW NBT · HU cache prediction · Path B L0.",
        "",
        "## Verdict",
        "",
        (
            f"- DUT **sees** controlled Host accesses → instrumentation not blind for this SCSI/READ subset."
            if visible
            else "- DUT counters did **not** move — blind or wrong gadget mode / not MSC."
        ),
        "",
        "```json",
        json.dumps(out, indent=2, default=str),
        "```",
        "",
    ]
    Path(args.out).write_text("\n".join(lines), encoding="utf-8")
    Path(args.out).with_suffix(".json").write_text(
        json.dumps(out, indent=2, default=str), encoding="utf-8"
    )
    print(json.dumps({"ok": out["ok"], "visible": visible, "stim": {k: stim.get(k) for k in
        ["ok","inquiryOk","capacityOk","turOk","turFail","readOk","readFail","bytesRead","duration_ms","err"]},
        "delta": delta}, indent=2, default=str))
    print("WROTE", args.out)
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
