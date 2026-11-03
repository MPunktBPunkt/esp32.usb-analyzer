#!/usr/bin/env python3
"""Power-rail sticky test for hu-probe usb_gadget modes.

Soft-reboot sticky is already MEASURED (LAB_HUPROBE_USB_GADGET_NVS_FIX).
This harness separates Soft-Reboot vs **Netzteil/Power-Cycle**.

Modes tested: msc, aoa_observe, aoa_phone_like (configurable).

Usage:
  # Soft-only (automated) — baseline
  python3 tools/lab_power_cycle_sticky.py --device 192.168.178.89 --soft-only

  # Full: soft + pause for operator power-cycle per mode
  python3 tools/lab_power_cycle_sticky.py --device 192.168.178.89 --wait-power-s 180

Requires hu-probe on --device (modules/fwType). Analyzer gadget is rejected.
Claim: huprobe_power_cycle_sticky_lab — NOT VBUS-switch HW proof.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 12) -> dict:
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


def post_json(url: str, body: dict, timeout: float = 30) -> dict:
    data = json.dumps(body).encode()
    req = urllib.request.Request(
        url, data=data, method="POST", headers={"Content-Type": "application/json"}
    )
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


def post(url: str, timeout: float = 60) -> dict:
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


def wait_up(ip: str, seconds: int = 120) -> dict:
    last: dict = {}
    for _ in range(seconds):
        last = get(f"http://{ip}/api/status", timeout=4)
        if last.get("version") or last.get("fw") or last.get("usb_gadget"):
            return last
        time.sleep(1)
    return last


def is_huprobe(st: dict) -> bool:
    """Strict: analyzer NVS display name may still be 'Hu-probe' — trust modules/fwType."""
    mods = st.get("modules") or []
    if isinstance(mods, list) and any(str(m).lower() == "analyzer" for m in mods):
        return False
    if st.get("role") in ("host", "device") and st.get("gadget") is not None:
        # analyzer-node device gadget block
        return False
    if isinstance(mods, list) and any("hu" in str(m).lower() for m in mods):
        return True
    ft = str(st.get("fwType") or "").lower()
    if "huprobe" in ft or "hu-probe" in ft or "hu_probe" in ft:
        return True
    # Real hu-probe always exposes usb_gadget in status when config present
    return st.get("usb_gadget") is not None and "read_count" in st


def set_gadget(ip: str, mode: str) -> dict:
    # hu-probe POST /api/config
    r = post_json(f"http://{ip}/api/config", {"usbGadget": mode, "enableHub": False})
    if r.get("err") and "http_" in str(r.get("err")):
        # try query form used elsewhere
        r = post(f"http://{ip}/api/config?usbGadget={mode}&enableHub=false")
    return r


def soft_reboot(ip: str) -> dict:
    for path in (
        f"http://{ip}/api/connected/gadget/reboot",  # hu-probe
        f"http://{ip}/api/reboot",
        f"http://{ip}/api/restart",
        f"http://{ip}/reboot",
    ):
        r = post(path, timeout=8)
        err = str(r.get("err") or "")
        if "http_404" in err or "http_405" in err:
            continue
        if r.get("err") and "Not Found" in str(r):
            continue
        return {"path": path, **r}
    return {"ok": False, "err": "no_reboot_endpoint"}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument(
        "--modes",
        default="msc,aoa_observe,aoa_phone_like",
        help="comma-separated usbGadget modes",
    )
    ap.add_argument("--soft-only", action="store_true")
    ap.add_argument("--wait-power-s", type=int, default=180)
    ap.add_argument("--settle-s", type=float, default=8.0)
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_POWER_CYCLE_STICKY_MEASURED.md"),
    )
    args = ap.parse_args()
    modes = [m.strip() for m in args.modes.split(",") if m.strip()]

    st0 = get(f"http://{args.device}/api/status")
    if not is_huprobe(st0):
        out = {
            "ok": False,
            "claim": "huprobe_power_cycle_sticky_lab",
            "err": "device_not_huprobe",
            "status_keys": {
                "name": st0.get("name"),
                "version": st0.get("version"),
                "modules": st0.get("modules"),
                "role": st0.get("role"),
                "usb_gadget": st0.get("usb_gadget"),
            },
            "hint": "OTA hu-probe to --device before power-cycle sticky; analyzer OTG loop must yield .89",
        }
        Path(args.out).write_text(
            "# Lab — Power-cycle sticky · BLOCKED\n\n"
            f"**Claim:** `{out['claim']}`\n\n"
            "Device is not hu-probe — cannot test usbGadget NVS sticky.\n\n"
            f"```json\n{json.dumps(out, indent=2)}\n```\n",
            encoding="utf-8",
        )
        Path(args.out).with_suffix(".json").write_text(
            json.dumps(out, indent=2), encoding="utf-8"
        )
        print(json.dumps(out, indent=2))
        return 3

    rows: list[dict] = []
    print("hu-probe", st0.get("version") or st0.get("fw"), "gadget", st0.get("usb_gadget"))

    for mode in modes:
        row: dict = {"mode": mode, "soft": {}, "power": {}}
        print(f"=== mode {mode} ===")
        cfg = set_gadget(args.device, mode)
        time.sleep(0.5)
        mid = get(f"http://{args.device}/api/status")
        row["soft"]["set"] = {
            "config": {k: cfg.get(k) for k in ("ok", "gadgetNvsOk", "usbGadget", "err") if k in cfg or True},
            "usb_gadget": mid.get("usb_gadget"),
            "gadgetNvsOk": cfg.get("gadgetNvsOk"),
        }
        print("set", mid.get("usb_gadget"), "nvs", cfg.get("gadgetNvsOk"))

        rb = soft_reboot(args.device)
        time.sleep(args.settle_s)
        after_soft = wait_up(args.device, 90)
        soft_ok = after_soft.get("usb_gadget") == mode
        row["soft"].update(
            {
                "reboot": rb,
                "usb_gadget": after_soft.get("usb_gadget"),
                "resetReason": after_soft.get("resetReason"),
                "ok": soft_ok,
            }
        )
        print("soft sticky", soft_ok, after_soft.get("usb_gadget"), after_soft.get("resetReason"))

        if args.soft_only:
            rows.append(row)
            continue

        print(
            f"OPERATOR: power-cycle Netzteil/rail of {args.device} NOW "
            f"(mode should stay '{mode}'). Waiting ≤{args.wait_power_s}s …",
            flush=True,
        )
        # Wait until down then up, or just until status returns with new bootGen
        boot0 = after_soft.get("bootGen")
        uptime0 = after_soft.get("uptime")
        deadline = time.time() + args.wait_power_s
        saw_down = False
        after_pwr: dict = {}
        while time.time() < deadline:
            s = get(f"http://{args.device}/api/status", timeout=3)
            if s.get("err"):
                saw_down = True
                time.sleep(1)
                continue
            if saw_down or s.get("bootGen") != boot0 or s.get("uptime") != uptime0:
                # allow settle after power return
                time.sleep(args.settle_s)
                after_pwr = wait_up(args.device, 60)
                break
            time.sleep(1)
        else:
            after_pwr = get(f"http://{args.device}/api/status")
            row["power"]["err"] = "timeout_waiting_power_cycle"
            row["power"]["ok"] = False
            rows.append(row)
            print("TIMEOUT — no power-cycle detected")
            continue

        pwr_ok = after_pwr.get("usb_gadget") == mode
        row["power"].update(
            {
                "saw_down": saw_down,
                "usb_gadget": after_pwr.get("usb_gadget"),
                "resetReason": after_pwr.get("resetReason"),
                "bootGen": after_pwr.get("bootGen"),
                "ok": pwr_ok,
            }
        )
        print("power sticky", pwr_ok, after_pwr.get("usb_gadget"), after_pwr.get("resetReason"))
        rows.append(row)

    soft_pass = all(r.get("soft", {}).get("ok") for r in rows)
    if args.soft_only:
        power_pass = None
        overall = soft_pass
    else:
        power_pass = all(r.get("power", {}).get("ok") for r in rows) and len(rows) == len(modes)
        overall = soft_pass and bool(power_pass)

    report = {
        "ok": overall,
        "claim": "huprobe_power_cycle_sticky_lab",
        "timestamp_utc": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
        "device": args.device,
        "fw": st0.get("version") or st0.get("fw"),
        "soft_only": args.soft_only,
        "modes": modes,
        "soft_pass": soft_pass,
        "power_pass": power_pass,
        "rows": rows,
        "not_claimed": [
            "controlled_vbus_switch_hw",
            "analyzer_nvs_modes",
            "aoa_phone_to_accessory_full_enum",
        ],
    }

    lines = [
        f"# Lab MEASURED — Power-cycle sticky · hu-probe",
        "",
        f"**Claim:** `{report['claim']}`",
        f"**FW:** {report['fw']} · soft_only={args.soft_only}",
        f"**Soft sticky:** {soft_pass} · **Power sticky:** {power_pass}",
        "",
        "## Epistemik",
        "",
        "- Soft `ESP.restart` ≠ Netzteil/Power-Cycle (rail drop).",
        "- Soft sticky already known MEASURED; this run revalidates and optionally adds rail.",
        "- **Not claimed:** VBUS OTG switch HW · phone→accessory Vollenum.",
        "",
        "## Rows",
        "",
        "```json",
        json.dumps(rows, indent=2, default=str),
        "```",
        "",
        f"JSON: `{Path(args.out).with_suffix('.json').name}`",
        "",
    ]
    Path(args.out).write_text("\n".join(lines), encoding="utf-8")
    Path(args.out).with_suffix(".json").write_text(
        json.dumps(report, indent=2, default=str), encoding="utf-8"
    )
    print(json.dumps({"ok": overall, "soft_pass": soft_pass, "power_pass": power_pass}, indent=2))
    print("WROTE", args.out)
    return 0 if overall else 2


if __name__ == "__main__":
    raise SystemExit(main())
