#!/usr/bin/env python3
"""HID scenario lab: type "Hello World", then mouse move + L/R click.

Topology: Analyzer Host .88 ↔ OTG ↔ Analyzer Device .89 (composite kbd+mouse).
Soft-reenum between keyboard and mouse phases (lab theater — same composite,
NOT two physical devices / NOT VBUS unplug).

Claim: hid_scenario_lab_loop — NOT L0 / MITM / HU / real hotplug.
Also archives Host VBUS/HPRT status (board does not source connector 5V).
"""
from __future__ import annotations

import argparse
import json
import threading
import time
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 15) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def post(url: str, timeout: float = 60) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "err": str(e)}


def observe(host: str, kind: str, wait_ms: int = 5000) -> dict:
    q = urllib.parse.urlencode(
        {"kind": kind, "wait_ms": wait_ms, "clear": 1}
    )
    return post(f"http://{host}/api/lab/hid-observe?{q}", timeout=wait_ms / 1000 + 8)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--text", default="Hello World")
    ap.add_argument("--skip-reenum", action="store_true")
    ap.add_argument(
        "--out",
        default=str(ROOT / "docs/evidence/LAB_HID_SCENARIO_MEASURED.md"),
    )
    args = ap.parse_args()

    h0 = get(f"http://{args.host}/api/status")
    d0 = get(f"http://{args.device}/api/status")
    vbus = get(f"http://{args.host}/api/lab/vbus-status")
    print("host", h0.get("version"), "dut", (h0.get("dut") or {}).get("id"))
    print("device", d0.get("version"), "hid", (d0.get("gadget") or {}).get("hid"),
          "mouse", (d0.get("gadget") or {}).get("hidMouse"))
    print("vbus", json.dumps({k: vbus.get(k) for k in (
        "ok", "prtpwr", "boardSourcesVbus", "boardNote", "prtConn")}, default=str))

    if not (h0.get("dut") or {}).get("present"):
        print("FAIL: need OTG DUT")
        return 2
    if not (d0.get("gadget") or {}).get("hid"):
        print("FAIL: device HID keyboard missing — flash 0.0.51+ device")
        return 2

    phases: list[dict] = []

    # --- Phase A: keyboard text ---
    obs_box: dict = {}
    err: list[str] = []

    def wait_keys() -> None:
        try:
            # Collect several observes while text is typing
            got = []
            deadline = time.time() + 8
            while time.time() < deadline and len(got) < max(3, len(args.text) // 2):
                o = observe(args.host, "key", wait_ms=1500)
                if o.get("ok") or o.get("got"):
                    got.append(o)
                else:
                    break
            obs_box["keys"] = got
        except Exception as e:  # noqa: BLE001
            err.append(str(e))

    th = threading.Thread(target=wait_keys, daemon=True)
    th.start()
    time.sleep(0.2)
    inj = post(
        f"http://{args.device}/api/lab/hid-inject?"
        + urllib.parse.urlencode({"text": args.text}),
        timeout=30,
    )
    th.join(timeout=12)
    phases.append(
        {
            "phase": "keyboard_text",
            "text": args.text,
            "inject": inj,
            "observes": obs_box.get("keys") or [],
            "ok": bool(inj.get("ok")) and bool(obs_box.get("keys")),
        }
    )
    print("keyboard inject", inj.get("ok"), "observes", len(obs_box.get("keys") or []))

    # Soft-reenum theater between phases
    reenum = {}
    if not args.skip_reenum:
        reenum = get(f"http://{args.device}/api/lab/soft-reenum")
        time.sleep(2.0)
        # wait DUT back
        for _ in range(25):
            hs = get(f"http://{args.host}/api/status")
            if (hs.get("dut") or {}).get("present"):
                break
            time.sleep(0.2)
    phases.append({"phase": "soft_reenum", "result": reenum, "ok": True})

    # --- Phase B: mouse move + clicks ---
    mouse_steps = [
        ("move", {"dx": 50, "dy": -30, "click": "none"}),
        ("move2", {"dx": -20, "dy": 40, "click": "none"}),
        ("left", {"click": "left"}),
        ("right", {"click": "right"}),
    ]
    mouse_rows = []
    for name, params in mouse_steps:
        obs_m: dict = {}

        def wait_m(box=obs_m) -> None:
            box.update(observe(args.host, "mouse", wait_ms=2500))

        thm = threading.Thread(target=wait_m, daemon=True)
        thm.start()
        time.sleep(0.15)
        inj_m = post(
            f"http://{args.device}/api/lab/hid-mouse?"
            + urllib.parse.urlencode(params),
            timeout=15,
        )
        thm.join(timeout=5)
        row = {
            "step": name,
            "params": params,
            "inject": inj_m,
            "observe": obs_m,
            "ok": bool(inj_m.get("ok")) and bool(obs_m.get("ok") or obs_m.get("got")),
        }
        mouse_rows.append(row)
        print(
            "mouse",
            name,
            "inj",
            inj_m.get("ok"),
            "obs",
            obs_m.get("ok"),
            "xy",
            obs_m.get("mouseX"),
            obs_m.get("mouseY"),
            "btn",
            obs_m.get("mouseButtons"),
        )
        time.sleep(0.15)

    phases.append(
        {
            "phase": "mouse",
            "rows": mouse_rows,
            "ok": all(r.get("ok") for r in mouse_rows),
        }
    )

    kb_ok = phases[0].get("ok")
    mouse_ok = phases[-1].get("ok")
    # Mouse may need FW 0.0.51 — if gadget lacks hidMouse, fail clearly
    if not (d0.get("gadget") or {}).get("hidMouse"):
        # re-check after reenum
        d1 = get(f"http://{args.device}/api/status")
        if not (d1.get("gadget") or {}).get("hidMouse"):
            mouse_ok = False
            phases[-1]["err"] = "device_hid_mouse_not_enabled_flash_0_0_51"

    out = {
        "ok": kb_ok and mouse_ok and bool(vbus.get("ok")),
        "claim": "hid_scenario_lab_loop",
        "timestamp_utc": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
        "text": args.text,
        "keyboard_ok": kb_ok,
        "mouse_ok": mouse_ok,
        "vbus": vbus,
        "board_sources_vbus": vbus.get("boardSourcesVbus"),
        "phases": phases,
        "observe_errors": err,
        "not_claimed": [
            "l0_wire",
            "transparent_mitm",
            "physical_hotplug_two_devices",
            "connector_5v_meter_reading",
            "hu",
        ],
        "note": (
            "Composite kbd+mouse on one gadget; soft-reenum ≠ VBUS unplug. "
            "Host board does not source OTG 5V — Device ESP is self-powered."
        ),
    }

    md = Path(args.out)
    lines = [
        f"# Lab MEASURED — HID scenario (kbd + mouse) · {h0.get('version')}",
        "",
        f"**Claim:** `{out['claim']}`",
        f"**Text:** `{args.text}` · keyboard_ok={kb_ok} · mouse_ok={mouse_ok}",
        f"**VBUS boardSourcesVbus:** {vbus.get('boardSourcesVbus')} · "
        f"HPRT.prtpwr={vbus.get('prtpwr')} · {vbus.get('boardNote')}",
        "",
        "## Epistemik",
        "",
        "- Device inject → Host EspUsbHost keyboard/mouse callbacks (class path).",
        "- Soft-reenum between phases is lab theater on the **same** composite — "
        "not two physical HID devices.",
        "- Host OTG connector does **not** source 5 V (DevKit/YD class FACT); "
        "HPRT.prtpwr is a software bit, not a meter reading.",
        "",
        f"JSON: `{md.with_suffix('.json').name}`",
        "",
        "```json",
        json.dumps(out, indent=2, default=str)[:8000],
        "```",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    md.with_suffix(".json").write_text(
        json.dumps(out, indent=2, default=str), encoding="utf-8"
    )

    # Dedicated VBUS evidence pointer
    vmd = ROOT / "docs/evidence/LAB_HOST_VBUS_MEASURED.md"
    vmd.write_text(
        "\n".join(
            [
                f"# Lab — Host VBUS / HPRT · {h0.get('version')}",
                "",
                "**Claim:** `host_vbus_status_lab`",
                f"**boardSourcesVbus:** `{vbus.get('boardSourcesVbus')}`",
                f"**HPRT.prtpwr (software):** `{vbus.get('prtpwr')}`",
                f"**Note:** {vbus.get('boardNote')}",
                "",
                "## Verdict",
                "",
                "- YD-ESP32-S3 / DevKitC-1 class: OTG connector does **not** feed bus 5 V.",
                "- Bus-powered-only keyboards/mice need a **self-powered hub** or external 5 V.",
                "- Lab Device ESP is self-powered via its own supply — OTG loop does not prove Host VBUS.",
                "- HPRT.prtpwr may be 1 in software while the connector pin stays unpowered.",
                "",
                "```json",
                json.dumps(vbus, indent=2, default=str),
                "```",
                "",
            ]
        ),
        encoding="utf-8",
    )
    vmd.with_suffix(".json").write_text(
        json.dumps(vbus, indent=2, default=str), encoding="utf-8"
    )

    print(json.dumps({"ok": out["ok"], "keyboard_ok": kb_ok, "mouse_ok": mouse_ok,
                      "boardSourcesVbus": vbus.get("boardSourcesVbus")}, indent=2))
    print("WROTE", md)
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
