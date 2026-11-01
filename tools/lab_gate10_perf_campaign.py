#!/usr/bin/env python3
"""Gate-10 / dual-ESP performance + capability campaign.

Runs everything the current OTG-loop lab can MEASURE without external PHY BOM.
Writes docs/evidence/LAB_GATE10_PERF_CAMPAIGN.md (+ JSON).
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def _read_json(resp) -> dict:
    return json.loads(resp.read().decode())


def get(url: str, timeout: float = 60) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return _read_json(r)
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")
        try:
            return json.loads(body)
        except Exception:
            raise e


def post(url: str, timeout: float = 180) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return _read_json(r)
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")
        try:
            return json.loads(body)
        except Exception:
            raise e


def run_tool(args: list[str], timeout: float = 180) -> tuple[int, str]:
    try:
        p = subprocess.run(
            args, cwd=str(ROOT), capture_output=True, text=True, timeout=timeout
        )
        out = (p.stdout or "") + (p.stderr or "")
        return p.returncode, out[-4000:]
    except Exception as e:
        return 99, str(e)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--l0st-host", default="192.168.178.187")
    args = ap.parse_args()

    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    report: dict = {
        "classification": "MEASURED",
        "timestamp_utc": ts,
        "claim": "gate10_perf_campaign_lab",
        "not_claimed": [
            "live_otg_phy_tap",
            "true_fs_stick_mitm_unless_wired",
            "bmw_hu_10g",
            "fpga_path_c",
        ],
        "nodes": {},
        "tests": {},
    }

    print("=== status ===")
    for label, ip in (("host", args.host), ("device", args.device)):
        s = get(f"http://{ip}/api/status")
        report["nodes"][label] = {
            "version": s.get("version"),
            "role": s.get("role"),
            "dut": (s.get("dut") or {}).get("present"),
        }
        print(label, s.get("version"), s.get("role"))

    def rec(name: str, ok: bool, detail: dict) -> None:
        report["tests"][name] = {"ok": ok, **detail}
        print(f"[{'OK' if ok else 'FAIL'}] {name}", detail.get("summary", ""))

    # Enum early — before stress that can stall EP0 briefly
    print("=== enum (early) ===")
    try:
        r = post(f"http://{args.host}/api/lab/usb-enum-observe", timeout=20)
        rec(
            "usb_enum_observe",
            bool(r.get("ok")) and bool(r.get("dutPresent")),
            {
                "summary": f"addr={r.get('address')} cfg={r.get('configurationValue')} "
                f"desc={r.get('getDescOk')}"
            },
        )
    except Exception as e:
        rec("usb_enum_observe", False, {"summary": str(e)})

    # A) MSC performance sweep (subset)
    print("=== MSC OTG ===")
    msc_rows = []
    for blocks, chunk in ((32, 4), (128, 4), (256, 4)):
        q = urllib.parse.urlencode(
            {"blocks": blocks, "chunk": chunk, "lba": 0, "dir": "read"}
        )
        t0 = time.time()
        try:
            r = post(f"http://{args.host}/api/lab/usb-host-probe?{q}")
            ok = bool(r.get("ok")) and bool(r.get("pattern_ok", True))
            row = {
                "blocks": blocks,
                "chunk": chunk,
                "ok": ok,
                "rate_kBs": r.get("rate_kBs"),
                "p50_us": r.get("rtt_p50_us"),
                "p95_us": r.get("rtt_p95_us"),
                "wall_s": round(time.time() - t0, 3),
            }
        except Exception as e:
            ok = False
            row = {"blocks": blocks, "chunk": chunk, "ok": False, "err": str(e)}
        msc_rows.append(row)
        print(row)
    rec(
        "usb_otg_msc",
        all(x.get("ok") for x in msc_rows),
        {"summary": f"{len(msc_rows)} cfgs", "rows": msc_rows},
    )

    # B) WiFi TCP integrity
    print("=== TCP integrity ===")
    try:
        q = urllib.parse.urlencode(
            {"peer": args.device, "frames": 32, "frame_bytes": 1024}
        )
        r = post(f"http://{args.host}/api/lab/integrity?{q}", timeout=120)
        ok = bool(r.get("ok")) and int(r.get("gaps") or r.get("gap") or 0) == 0
        rec(
            "tcp_wifi_integrity",
            ok,
            {
                "summary": f"rate={r.get('rate_kBs') or r.get('kBs')} gaps={r.get('gaps', r.get('gap'))}",
                "raw": {k: r.get(k) for k in list(r)[:20]},
            },
        )
    except Exception as e:
        rec("tcp_wifi_integrity", False, {"summary": str(e)})

    # C) Gate 10A feasibility + E1.1
    print("=== 10A / E1.1 ===")
    try:
        r = get(f"http://{args.host}/api/lab/l0-feasibility")
        rec(
            "gate10a_feasibility",
            bool(r.get("ok")) and not r.get("pathA_snifferOk"),
            {"summary": r.get("pathA_reason"), "prtEna": r.get("prtEna")},
        )
    except Exception as e:
        rec("gate10a_feasibility", False, {"summary": str(e)})

    try:
        r = get(
            f"http://{args.host}/api/lab/e11-gpio-sample?samples=100000&traffic_blocks=32",
            timeout=120,
        )
        rec(
            "e11_gpio_sample",
            bool(r.get("ok")) and not r.get("pathA_usable"),
            {
                "summary": f"{r.get('rateKsps')} ksps jitter={r.get('jitterPpNs')}ns destructive={r.get('destructive')}",
                "rateKsps": r.get("rateKsps"),
                "edges": (r.get("transitionsDm") or 0) + (r.get("transitionsDp") or 0),
                "busStable": r.get("busStable"),
            },
        )
    except Exception as e:
        rec("e11_gpio_sample", False, {"summary": str(e)})

    # D) Path B GPIO lab + L0ST
    print("=== Path B GPIO ===")
    rc, out = run_tool(
        [
            sys.executable,
            str(ROOT / "tools" / "lab_path_b_gpio.py"),
            "--host",
            args.host,
            "--mode",
            "txrx_l0st",
            "--rate",
            "100000",
            "--l0st-host",
            args.l0st_host,
            "--decode",
        ],
        timeout=120,
    )
    rec(
        "path_b_gpio_lab",
        rc == 0,
        {"summary": "txrx_l0st+decode" if rc == 0 else out[-500:], "rc": rc},
    )

    # E) L0 sim pipeline
    print("=== L0 sim pipeline ===")
    rc, out = run_tool(
        [
            sys.executable,
            str(ROOT / "tools" / "lab_l0_live_pipeline.py"),
            "--source",
            "sim",
            "--host",
            args.host,
        ],
        timeout=90,
    )
    rec("l0_live_pipeline_sim", rc == 0, {"summary": out.splitlines()[-3:] if out else "", "rc": rc})

    # F) Observer proxy
    print("=== Observer proxy ===")
    rc, out = run_tool(
        [
            sys.executable,
            str(ROOT / "tools" / "lab_observer_effect.py"),
            "--mode",
            "proxy",
            "--host",
            args.host,
            "--device",
            args.device,
        ],
        timeout=180,
    )
    rec("observer_proxy", rc == 0, {"summary": out.splitlines()[-5:] if out else "", "rc": rc})

    # G) Ctrl-neg (after MSC proved bus; EP0 may be tender after proxy)
    print("=== ctrl-neg ===")
    try:
        r = get(f"http://{args.host}/api/lab/usb-ctrl-neg", timeout=60)
        rec(
            "usb_ctrl_neg",
            bool(r.get("ok")) and int(r.get("passed") or 0) >= 1,
            {"summary": f"passed={r.get('passed')}/{r.get('cases')}"},
        )
    except Exception as e:
        rec("usb_ctrl_neg", False, {"summary": str(e)})

    passed = sum(1 for t in report["tests"].values() if t.get("ok"))
    total = len(report["tests"])
    report["summary"] = {"passed": passed, "total": total}

    out_json = ROOT / "docs" / "evidence" / f"LAB_GATE10_PERF_CAMPAIGN_{ts}.json"
    out_md = ROOT / "docs" / "evidence" / "LAB_GATE10_PERF_CAMPAIGN.md"
    out_json.write_text(json.dumps(report, indent=2))
    lines = [
        f"# Lab MEASURED — Gate 10 performance campaign ({ts})",
        "",
        f"**Nodes:** host `{report['nodes'].get('host',{}).get('version')}` · "
        f"device `{report['nodes'].get('device',{}).get('version')}`",
        f"**Result:** **{passed}/{total}** tests OK",
        "",
        "| Test | OK | Summary |",
        "|------|----|---------|",
    ]
    for name, t in report["tests"].items():
        lines.append(
            f"| `{name}` | {'yes' if t.get('ok') else 'NO'} | {t.get('summary')} |"
        )
    lines += [
        "",
        "**Claim:** `gate10_perf_campaign_lab`",
        "**Not claimed:** soldered Path-B PHY · BMW/HU 10G · FPGA Path C · true stick MITM unless wired.",
        "",
        f"JSON: `{out_json.name}`",
    ]
    out_md.write_text("\n".join(lines) + "\n")
    print(f"wrote {out_md}")
    print(f"RESULT {passed}/{total}")
    return 0 if passed == total else 2


if __name__ == "__main__":
    raise SystemExit(main())
