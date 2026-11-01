#!/usr/bin/env python3
"""Gate 9 REQUEST_SENSE isolation (Device expect vs Host observe).

Arms Device fail, pulls Device sense-expect fields, Host sense before/after
recovery — localizes where sense disappears.

Claim: gate9_sense_isolation_lab — NOT L0 / MITM.
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


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_GATE9_SENSE_ISOLATION.md"),
    )
    args = ap.parse_args()

    get(f"http://{args.device}/api/lab/fault-inject?op=clear")
    time.sleep(0.1)
    arm = get(f"http://{args.device}/api/lab/fault-inject?op=fail&count=1")
    obs = get(
        f"http://{args.host}/api/lab/fault-observe?lba=0&blocks=1&sense=1&recovery=1"
    )
    time.sleep(0.15)
    dev = get(f"http://{args.device}/api/lab/fault-inject?op=status")

    device_expect = {
        "senseKey": dev.get("deviceSenseKey"),
        "asc": dev.get("deviceAsc"),
        "asq": dev.get("deviceAsq"),
        "source": dev.get("deviceSenseSource"),
        "failHits": dev.get("failHits"),
        "lastFailOffset": dev.get("lastFailOffset"),
    }
    host_obs = {
        "readOk": obs.get("readOk"),
        "recoveryOk": obs.get("recoveryOk"),
        "senseBeforeOk": obs.get("senseBeforeOk"),
        "senseKeyBefore": obs.get("senseKeyBefore"),
        "ascBefore": obs.get("ascBefore"),
        "senseAfterOk": obs.get("senseAfterOk"),
        "senseKeyAfter": obs.get("senseKeyAfter"),
        "ascAfter": obs.get("ascAfter"),
        "sensePhase": obs.get("sensePhase"),
    }

    # Verdict classes
    device_armed = int(device_expect.get("failHits") or 0) >= 1 and int(
        device_expect.get("senseKey") or 0
    ) == 0x02
    host_read_failed = host_obs.get("readOk") is False
    sense_before_nonzero = int(host_obs.get("senseKeyBefore") or 0) != 0 or int(
        host_obs.get("ascBefore") or 0
    ) != 0
    sense_after_zero = int(host_obs.get("senseKeyAfter") or 0) == 0 and int(
        host_obs.get("ascAfter") or 0
    ) == 0

    if device_armed and host_read_failed and not sense_before_nonzero:
        locus = "host_sense_lost_before_or_during_bot_reset"
    elif device_armed and host_read_failed and sense_before_nonzero and sense_after_zero:
        locus = "sense_cleared_by_recovery_path"
    elif device_armed and host_read_failed and sense_before_nonzero:
        locus = "host_received_nonzero_sense"
    else:
        locus = "inconclusive"

    out = {
        "ok": device_armed and host_read_failed,
        "claim": "gate9_sense_isolation_lab",
        "locus": locus,
        "arm": arm,
        "device_expect": device_expect,
        "host_observe": host_obs,
        "not_claimed": ["l0_wire", "usb_if_sense_compliance"],
    }

    md = Path(args.out)
    lines = [
        "# Lab MEASURED — Gate 9 REQUEST_SENSE isolation",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**Locus:** `{locus}`",
        f"**Device expect:** key={device_expect.get('senseKey')} "
        f"ASC={device_expect.get('asc')} source=`{device_expect.get('source')}`",
        f"**Host before recovery:** ok={host_obs.get('senseBeforeOk')} "
        f"key={host_obs.get('senseKeyBefore')} ASC={host_obs.get('ascBefore')}",
        f"**Host after recovery:** ok={host_obs.get('senseAfterOk')} "
        f"key={host_obs.get('senseKeyAfter')} ASC={host_obs.get('ascAfter')}",
        f"**readOk={host_obs.get('readOk')} recoveryOk={host_obs.get('recoveryOk')}**",
        "",
        "## Verdict",
        "",
        "- Device records expected TinyUSB sense (NOT_READY / 0x3A) at inject time.",
        "- Host logs REQUEST_SENSE before and after recovery separately.",
        f"- Current locus: **{locus}**.",
        "",
        "**Not claimed:** L0 · USB-IF · transparent MITM.",
        "",
        f"JSON: `{md.with_suffix('.json').name}`",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    md.with_suffix(".json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    get(f"http://{args.device}/api/lab/fault-inject?op=clear")
    print("locus", locus, "ok", out["ok"])
    print("wrote", md)
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
