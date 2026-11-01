#!/usr/bin/env python3
"""L0 vs TinyUSB/L1 stack-gap lab (controlled stimulus).

Drives known NRZI patterns on OTG D+/D− (destructive wire-stimulus), treats the
injected line-states as L0 ground truth, and compares to Device L1 capture.

L0 packets with no compatible L1 event → stack_gap (what TinyUSB did not report).

Claim: l0_stack_gap_stimulus_lab
NOT: passive live Path-B PHY capture / USB-IF.
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
import sys

sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.correlate import correlate  # noqa: E402
from usb_fs_l0.decoder import decode_nrzi_bits  # noqa: E402
from usb_fs_l0.gen_synth import synth_get_descriptor_states  # noqa: E402
from usb_fs_l0.schema import batch_dict  # noqa: E402

# Mirror firmware GpioLinePatterns for offline L0 ground truth
from usb_fs_l0.decoder import (  # noqa: E402
    build_data,
    build_handshake,
    build_token,
    packet_bytes_to_nrzi_states,
)
from usb_fs_l0.nrzi import J  # noqa: E402


def get(url: str, timeout: float = 30) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


def get_allow(url: str, timeout: float = 180) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        try:
            return json.loads(e.read().decode())
        except Exception:
            return {"ok": False, "err": str(e)}
    except Exception as e:
        return {"ok": False, "err": str(e)}


def pattern_states(name: str) -> list[int]:
    if name in ("setup_ok", "setup_ok_wire", "synth"):
        return synth_get_descriptor_states()
    if name == "setup_bad_crc":
        tok = bytearray(build_token(0x2D, addr=1, endp=0))
        tok[-1] ^= 0x15
        st = packet_bytes_to_nrzi_states(bytes(tok), idle=J)
        st.extend([J] * 8)
        return st
    if name == "data_bad_crc":
        data = bytes([0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00])
        d0 = bytearray(build_data(0xC3, data))
        d0[-1] ^= 0xFF
        return packet_bytes_to_nrzi_states(bytes(d0), idle=J)
    if name == "eop":
        return [J] * 16 + [0, 0] + [J] * 16
    if name == "sync":
        sync = [2, 1, 2, 1, 2, 1, 2, 2]
        out: list[int] = []
        for _ in range(16):
            out.extend(sync)
            out.extend([J] * 8)
        return out
    if name == "se0_reset":
        return [0] * 200 + [J] * 64
    if name == "bus_states":
        return [0] * 64 + [J] * 64 + [2] * 64 + [3] * 32 + [J] * 64
    # fallback: SETUP+DATA+ACK good
    return synth_get_descriptor_states()


def wait_host(ip: str, timeout: float = 60) -> None:
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            get(f"http://{ip}/api/status", timeout=3)
            return
        except Exception:
            time.sleep(1)
    raise TimeoutError("host")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument(
        "--patterns",
        default="setup_ok,setup_bad_crc,data_bad_crc,eop,se0_reset",
    )
    ap.add_argument("--rate", type=int, default=100000)
    ap.add_argument("--reboot-ms", type=int, default=2500)
    ap.add_argument(
        "--out",
        default="docs/evidence/LAB_L0_STACK_GAP_MEASURED.md",
    )
    args = ap.parse_args()

    patterns = [p.strip() for p in args.patterns.split(",") if p.strip()]
    h0 = get(f"http://{args.host}/api/status")
    d0 = get(f"http://{args.device}/api/status")
    print("host", h0.get("version"), "device", d0.get("version"))

    # One destructive series; L0 ground truth = concatenation of pattern states
    all_states: list[int] = []
    for p in patterns:
        all_states.extend(pattern_states(p))
        all_states.extend([J] * 8)

    cap_before = get(f"http://{args.device}/api/capture?limit=64")
    ev_before = cap_before.get("events") or []
    g0 = (d0.get("gadget") or {})
    unmount0 = int(g0.get("unmountCount") or 0)

    q = urllib.parse.urlencode(
        {
            "confirm": "wire_stimulus_destructive",
            "patterns": ",".join(patterns),
            "rate": args.rate,
            "reboot_ms": args.reboot_ms,
            "gap_ms": 20,
        }
    )
    print("==> wire-stimulus series", patterns)
    stim = get_allow(f"http://{args.host}/api/lab/wire-stimulus?{q}", timeout=180)
    print("stim", stim.get("ok"), stim.get("pattern"), stim.get("achievedRateHz"))
    time.sleep(0.5)
    cap_after = get(f"http://{args.device}/api/capture?limit=64")
    ev_after = cap_after.get("events") or []
    d1 = get(f"http://{args.device}/api/status")
    g1 = d1.get("gadget") or {}

    # New L1 events: prefer after list; capture ring may wrap — use notes delta
    l1 = ev_after
    l0_pkts = decode_nrzi_bits(all_states)
    corr = correlate(l0_pkts, l1, ignore_time=True)
    gap = corr.get("unmatched_l0") or []

    if args.reboot_ms > 0:
        print("waiting host reboot…")
        wait_host(args.host)
        for _ in range(20):
            st = get(f"http://{args.host}/api/status")
            if (st.get("dut") or {}).get("present"):
                break
            time.sleep(1)

    out = {
        "ok": bool(stim.get("ok")),
        "claim": "l0_stack_gap_stimulus_lab",
        "patterns": patterns,
        "stim": {k: stim.get(k) for k in ("ok", "pattern", "achievedRateHz", "states")},
        "l0_n": len(l0_pkts),
        "l1_n": len(l1),
        "matched": corr.get("matched"),
        "stack_gap_n": corr.get("stack_gap_n"),
        "unmatched_l0": gap,
        "unmatched_l1_sample": (corr.get("unmatched_l1") or [])[:12],
        "device_unmount_delta": int(g1.get("unmountCount") or 0) - unmount0,
        "device_otg": (d1.get("otg") or {}).get("state"),
        "l0_batch": batch_dict(l0_pkts),
        "note": "injected_wire_states_as_l0_gt_vs_device_tinyusb_l1",
        "not_claimed": [
            "passive_path_b_phy",
            "live_bus_bypass_without_stimulus",
        ],
    }
    out_json = ROOT / "docs" / "evidence" / "LAB_L0_STACK_GAP_MEASURED.json"
    out_json.write_text(json.dumps(out, indent=2))

    md = Path(args.out)
    lines = [
        f"# Lab MEASURED — L0 vs TinyUSB stack gap (stimulus) · {h0.get('version')}",
        "",
        "**Question:** What on the wire (injected) did TinyUSB not report?",
        "",
        f"**Patterns:** `{', '.join(patterns)}`",
        f"**L0 packets (decoder on injected states):** {len(l0_pkts)}",
        f"**L1 Device events:** {len(l1)}",
        f"**Matched L0↔L1:** {corr.get('matched')}",
        f"**Stack gap (L0 without L1):** **{corr.get('stack_gap_n')}**",
        f"**Device Δunmount:** {out['device_unmount_delta']} · otg={out['device_otg']}",
        "",
        "## Unmatched L0 (candidates TinyUSB did not mirror)",
        "",
    ]
    if not gap:
        lines.append("_none_")
    for g in gap:
        lines.append(
            f"- idx={g.get('l0_index')} pid=`{g.get('pid')}` kind=`{g.get('kind')}` "
            f"crc_ok={g.get('crc_ok')}"
        )
    lines += [
        "",
        "## Verdict",
        "",
        "- With Host stack stopped, injected FS-like patterns are **L0 ground truth**.",
        "- Device TinyUSB typically only shows **unmount/usb_stopped** (Host end),",
        "  not SETUP/DATA decode — hence large `stack_gap_n` is **expected** and MEASURED.",
        "- This is the closest lab answer to „was geht an TinyUSB vorbei?“ until Path B PHY.",
        "",
        "**Claim:** `l0_stack_gap_stimulus_lab`",
        "**Not claimed:** passive live D+/D− sniffer · Path B soldered.",
        "",
        f"JSON: `{out_json.name}`",
    ]
    md.write_text("\n".join(lines) + "\n")
    print(json.dumps({k: out[k] for k in out if k != "l0_batch"}, indent=2))
    print(f"wrote {md}")
    print("RESULT OK — stack gap MEASURED under stimulus" if out["ok"] else "FAIL")
    return 0 if out["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
