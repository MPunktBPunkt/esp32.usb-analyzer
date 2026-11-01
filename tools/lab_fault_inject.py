#!/usr/bin/env python3
"""Gate 9 fault injection lab (OTG Host↔Device loop, no new HW).

Device arms MSC onRead faults; Host observes READ fail + REQUEST_SENSE + recovery.

Ops: delay | fail | disconnect | clear
Claim: gate9_fault_inject_lab — NOT L0 / transparent MITM.
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
            return {"ok": False, "err": str(e)}
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument(
        "--ops",
        default="fail,delay,disconnect",
        help="comma ops: fail,delay,disconnect",
    )
    ap.add_argument("--delay-ms", type=int, default=80)
    ap.add_argument("--out", default=str(ROOT / "docs/evidence/LAB_GATE9_FAULT_MEASURED.md"))
    args = ap.parse_args()

    h = get(f"http://{args.host}/api/status")
    d = get(f"http://{args.device}/api/status")
    print("host", h.get("version"), "otg", (h.get("otg") or {}).get("state"))
    print("device", d.get("version"), "otg", (d.get("otg") or {}).get("state"))

    # Baseline READ must work
    base = get(f"http://{args.host}/api/lab/usb-host-probe?blocks=2&chunk=1&dir=read&lba=0")
    print("baseline", "ok=", base.get("ok"), "kB/s=", base.get("rate_kBs"))
    if not base.get("ok"):
        print("FAIL baseline MSC")
        return 1

    results: list[dict] = []
    for op in [x.strip() for x in args.ops.split(",") if x.strip()]:
        get(f"http://{args.device}/api/lab/fault-inject?op=clear")
        time.sleep(0.1)
        if op == "fail":
            arm = get(f"http://{args.device}/api/lab/fault-inject?op=fail&count=1")
        elif op == "delay":
            q = urllib.parse.urlencode(
                {"op": "delay", "ms": args.delay_ms, "count": 1}
            )
            arm = get(f"http://{args.device}/api/lab/fault-inject?{q}")
        elif op == "disconnect":
            arm = get(
                f"http://{args.device}/api/lab/fault-inject?op=disconnect&after=1"
            )
        else:
            print("skip unknown op", op)
            continue
        print(f"arm[{op}]", arm.get("ok"), arm.get("op"), arm.get("err"))

        obs = get(
            f"http://{args.host}/api/lab/fault-observe?lba=0&blocks=1&sense=1&recovery=1",
            timeout=60,
        )
        print(
            f"obs[{op}]",
            "readOk=",
            obs.get("readOk"),
            "sense=",
            obs.get("senseKey"),
            obs.get("asc"),
            "recoveryOk=",
            obs.get("recoveryOk"),
            "readMs=",
            obs.get("readMs"),
        )
        # After disconnect wait for re-enum
        if op == "disconnect":
            time.sleep(2.5)
            rec = get(
                f"http://{args.host}/api/lab/usb-host-probe?blocks=1&chunk=1&dir=read&lba=0"
            )
            obs["post_disconnect_probe_ok"] = bool(rec.get("ok"))
            print("post_disconnect_probe", rec.get("ok"))

        get(f"http://{args.device}/api/lab/fault-inject?op=clear")
        # Heal bus after fail/delay
        heal = get(
            f"http://{args.host}/api/lab/usb-host-probe?blocks=1&chunk=1&dir=read&lba=0"
        )
        row = {
            "op": op,
            "arm": {k: arm.get(k) for k in ("ok", "op", "delayMs", "failRemaining")},
            "observe": obs,
            "heal_ok": bool(heal.get("ok")),
        }
        results.append(row)
        time.sleep(0.3)

    # Pass criteria (MEASURED expectations)
    pass_n = 0
    notes: list[str] = []
    for row in results:
        op = row["op"]
        o = row["observe"]
        if op == "fail":
            # expect read fail OR sense present; recovery ideally ok after one-shot fail
            ok = (not o.get("readOk")) and bool(o.get("ok"))
            if ok:
                pass_n += 1
            notes.append(
                f"fail: readOk={o.get('readOk')} senseKey={o.get('senseKey')} "
                f"asc={o.get('asc')} recoveryOk={o.get('recoveryOk')} → {'PASS' if ok else 'CHECK'}"
            )
        elif op == "delay":
            # delay should inflate readMs vs baseline (~few ms); still may succeed
            ok = bool(o.get("ok")) and int(o.get("readMs") or 0) >= max(
                20, args.delay_ms // 2
            )
            if ok:
                pass_n += 1
            notes.append(
                f"delay: readMs={o.get('readMs')} (arm {args.delay_ms}ms) "
                f"readOk={o.get('readOk')} → {'PASS' if ok else 'CHECK'}"
            )
        elif op == "disconnect":
            ok = bool(o.get("post_disconnect_probe_ok")) or bool(row.get("heal_ok"))
            if ok:
                pass_n += 1
            notes.append(
                f"disconnect: post_probe={o.get('post_disconnect_probe_ok')} "
                f"heal={row.get('heal_ok')} → {'PASS' if ok else 'CHECK'}"
            )

    out = {
        "ok": pass_n == len(results) and len(results) > 0,
        "claim": "gate9_fault_inject_lab",
        "host_version": h.get("version"),
        "device_version": d.get("version"),
        "baseline_kBs": base.get("rate_kBs"),
        "pass_n": pass_n,
        "ops_n": len(results),
        "results": results,
        "notes": notes,
        "not_claimed": ["l0_wire", "transparent_mitm", "hu"],
    }

    md = Path(args.out)
    md.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        f"# Lab MEASURED — Gate 9 Fault Injection · {h.get('version')}",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**Pass:** {pass_n}/{len(results)}",
        f"**Baseline MSC:** {base.get('rate_kBs')} kB/s",
        "",
        "## Ops",
        "",
    ]
    for n in notes:
        lines.append(f"- {n}")
    lines += [
        "",
        "## Verdict",
        "",
        "- Device `fault-inject` arms MSC `onRead` delay/fail/disconnect.",
        "- Host `fault-observe` records READ outcome, SCSI sense, recovery READ.",
        "- TinyUSB maps fail → CSW FAILED + medium_not_present sense (class path).",
        "",
        "**Not claimed:** L0 D+/D− · transparent MITM · HU.",
        "",
        f"JSON: `{md.with_suffix('.json').name}`",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    md.with_suffix(".json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    print("wrote", md, "ok=", out["ok"])
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
