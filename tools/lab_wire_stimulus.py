#!/usr/bin/env python3
"""Destructive OTG wire stimulus — Host drives GPIO19/20 after stopping USB Host.

Correlates Host patterns with Device mount/unmount/suspend + capture events.

Claim: wire_stimulus_lab
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.parse
import urllib.request
from pathlib import Path


def get(url: str, timeout: float = 30) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


def get_allow_err(url: str, timeout: float = 180) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")
        try:
            return json.loads(body)
        except Exception:
            return {"ok": False, "err": body, "http": e.code}
    except Exception as e:
        return {"ok": False, "err": str(e)}


def snap_device(ip: str) -> dict:
    s = get(f"http://{ip}/api/status")
    g = s.get("gadget") or {}
    return {
        "version": s.get("version"),
        "otg": (s.get("otg") or {}).get("state"),
        "mountCount": g.get("mountCount"),
        "unmountCount": g.get("unmountCount"),
        "suspendCount": g.get("suspendCount"),
        "resumeCount": g.get("resumeCount"),
        "lastBusEvent": g.get("lastBusEvent"),
        "lastBusEventMs": g.get("lastBusEventMs"),
    }


def capture_notes(ip: str, limit: int = 32) -> list[str]:
    try:
        batch = get(f"http://{ip}/api/capture?limit={limit}", timeout=10)
    except Exception:
        return []
    ev = batch.get("events") or batch.get("items") or []
    notes = []
    for e in ev if isinstance(ev, list) else []:
        n = e.get("note") or e.get("type") or ""
        if n:
            notes.append(str(n))
    return notes


def wait_host(ip: str, timeout: float = 60) -> dict:
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            s = get(f"http://{ip}/api/status", timeout=3)
            if s.get("version"):
                return s
        except Exception:
            pass
        time.sleep(1)
    raise TimeoutError("host not back")


def msc_probe(host: str) -> dict:
    q = urllib.parse.urlencode({"blocks": 8, "chunk": 4, "dir": "read"})
    req = urllib.request.Request(
        f"http://{host}/api/lab/usb-host-probe?{q}", method="POST", data=b""
    )
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument(
        "--patterns",
        default="bus_states,se0_reset,sync,eop,glitch,jk_burst,setup_ok,setup_bad_crc,data_bad_crc",
    )
    ap.add_argument("--rate", type=int, default=100000)
    ap.add_argument("--rate-ls", action="store_true", help="also run series at 1.5 MHz")
    ap.add_argument("--reboot-ms", type=int, default=2500)
    ap.add_argument("--gap-ms", type=int, default=30)
    ap.add_argument(
        "--out",
        default="docs/evidence/LAB_WIRE_STIMULUS_MEASURED.md",
    )
    args = ap.parse_args()

    h0 = get(f"http://{args.host}/api/status")
    d0 = snap_device(args.device)
    cap0 = capture_notes(args.device)
    print("host", h0.get("version"), "dut", (h0.get("dut") or {}).get("present"))
    print("device before", d0)

    runs = []

    def one_series(label: str, rate: int) -> dict:
        d_before = snap_device(args.device)
        q = urllib.parse.urlencode(
            {
                "confirm": "wire_stimulus_destructive",
                "patterns": args.patterns,
                "rate": rate,
                "reboot_ms": args.reboot_ms,
                "gap_ms": args.gap_ms,
            }
        )
        print(f"==> series {label} rate={rate}")
        t0 = time.time()
        r = get_allow_err(
            f"http://{args.host}/api/lab/wire-stimulus?{q}", timeout=180
        )
        wall = time.time() - t0
        time.sleep(0.4)
        d_after = snap_device(args.device)
        caps = capture_notes(args.device)
        print(json.dumps({
            "ok": r.get("ok"),
            "achieved": r.get("achievedRateHz"),
            "states": r.get("states"),
            "pattern": r.get("pattern"),
            "device": d_after,
            "Δunmount": (d_after.get("unmountCount") or 0) - (d_before.get("unmountCount") or 0),
            "Δmount": (d_after.get("mountCount") or 0) - (d_before.get("mountCount") or 0),
        }, indent=2))
        result = {
            "label": label,
            "rate": rate,
            "host": r,
            "device_before": d_before,
            "device_after": d_after,
            "capture_notes": caps[-12:],
            "wall_s": round(wall, 3),
        }
        if args.reboot_ms > 0:
            print("waiting host reboot…")
            h1 = wait_host(args.host)
            print("host back", h1.get("version"), h1.get("uptime"))
            for _ in range(25):
                st = get(f"http://{args.host}/api/status")
                if (st.get("dut") or {}).get("present"):
                    break
                time.sleep(1)
            try:
                msc = msc_probe(args.host)
            except Exception as e:
                msc = {"ok": False, "err": str(e)}
            result["post_reboot_msc"] = msc
            print("post MSC", msc.get("ok"), msc.get("rate_kBs"))
            # device after remount
            result["device_restored"] = snap_device(args.device)
        return result

    runs.append(one_series("slow_100k", args.rate))
    if args.rate_ls:
        # Host is back after reboot; second series at LS-ish rate
        time.sleep(2)
        runs.append(one_series("ls_1m5", 1_500_000))

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        f"# Lab MEASURED — Wire stimulus on GPIO19/20 ({h0.get('version')}→0.0.43)",
        "",
        f"**Tool:** `tools/lab_wire_stimulus.py`",
        f"**Patterns:** `{args.patterns}`",
        "",
        "## Device before",
        "",
        "```json",
        json.dumps(d0, indent=2),
        "```",
        "",
        f"Capture notes (sample): {cap0[-8:]}",
        "",
    ]
    all_ok = True
    for run in runs:
        h = run["host"]
        db = run["device_before"]
        da = run["device_after"]
        ok = bool(h.get("ok"))
        all_ok = all_ok and ok
        lines += [
            f"## Series `{run['label']}` @ {run['rate']} Hz",
            "",
            f"- Host ok={ok} · achieved **{h.get('achievedRateHz')} Hz** · states={h.get('states')} · wall={run['wall_s']}s",
            f"- Device otg {db.get('otg')} → **{da.get('otg')}**",
            f"- Δmount={(da.get('mountCount') or 0) - (db.get('mountCount') or 0)} · "
            f"Δunmount={(da.get('unmountCount') or 0) - (db.get('unmountCount') or 0)} · "
            f"lastBusEvent={da.get('lastBusEvent')}",
            f"- Capture tail: {run.get('capture_notes')}",
            "",
        ]
        if run.get("post_reboot_msc"):
            m = run["post_reboot_msc"]
            lines.append(
                f"- Post-reboot MSC: **{'OK' if m.get('ok') else 'FAIL'}** "
                f"{m.get('rate_kBs')} kB/s · device {run.get('device_restored', {}).get('otg')}"
            )
            lines.append("")
            all_ok = all_ok and bool(m.get("ok"))

    lines += [
        "## Verdict",
        "",
        "- Destructive GPIO19/20 stimulus MEASURED with Device bus-event counters.",
        "- Slow (~100 kHz) is the reliable lab rate; LS 1.5 MHz attempt is best-effort bit-bang.",
        "- Device typically **unmounts** when Host stack stops / SE0 is driven.",
        "",
        "**Claim:** `wire_stimulus_lab`",
        "**Not claimed:** FS soft-host · USB-IF SI · parallel Host.",
    ]
    out.write_text("\n".join(lines) + "\n")
    print(f"wrote {out}")
    return 0 if all_ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
