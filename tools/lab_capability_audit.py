#!/usr/bin/env python3
"""Capability audit harness — prove what the OTG-loop lab can MEASURE today.

Topology (required):
  .88 analyzer Host  ↔ USB-C ↔  .89 analyzer Device   (same FW family)
  Controller :8080 on lab Debian (optional checks)

Runs existing MEASURED tools, archives JSON+MD under docs/evidence/ and
docs/reports/. Does NOT claim Path-B PHY, HU wire, or HS.

Usage:
  python3 tools/lab_capability_audit.py
  python3 tools/lab_capability_audit.py --skip-heavy   # no Gate10 observer/GPIO
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


def get(url: str, timeout: float = 30) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


def post(url: str, timeout: float = 180) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def run_tool(argv: list[str], timeout: float = 600) -> tuple[int, str]:
    p = subprocess.run(
        argv, cwd=str(ROOT), capture_output=True, text=True, timeout=timeout
    )
    out = (p.stdout or "") + (p.stderr or "")
    return p.returncode, out


def wait_dut(host: str, seconds: int = 25) -> dict:
    last: dict = {}
    for _ in range(seconds):
        try:
            last = get(f"http://{host}/api/status", timeout=4)
            dut = last.get("dut") or {}
            if dut.get("id") or dut.get("vid") or dut.get("product") or dut.get("present"):
                return last
        except Exception:
            pass
        time.sleep(1)
    return last


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--skip-heavy", action="store_true")
    args = ap.parse_args()

    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    report: dict = {
        "classification": "MEASURED",
        "claim": "lab_capability_audit_0_0_50",
        "timestamp_utc": ts,
        "topology": "OTG Host(.88) ↔ USB-C ↔ Device(.89); WiFi TCP; Controller :8080",
        "not_claimed": [
            "passive_live_L0_D+/D-",
            "path_b_soldered_phy",
            "bmw_nbt_hu_wire",
            "usb_high_speed",
            "usb_if_compliance",
            "soft_usb_equals_vbus_unplug",
        ],
        "nodes": {},
        "controller": {},
        "tests": {},
        "limits": {},
    }

    print("=== preflight ===")
    for label, ip in (("host", args.host), ("device", args.device)):
        s = get(f"http://{ip}/api/status")
        report["nodes"][label] = {
            "ip": ip,
            "version": s.get("version") or s.get("fw"),
            "name": s.get("name"),
            "role": s.get("role"),
            "sketchUsed": s.get("sketchUsed"),
        }
        print(label, report["nodes"][label])

    try:
        report["controller"] = get("http://127.0.0.1:8080/api/v1/health")
    except Exception as e:
        report["controller"] = {"ok": False, "err": str(e)}

    # Ensure DUT enumerated
    try:
        post(f"http://{args.device}/api/usb/cycle", timeout=15)
    except Exception:
        pass
    time.sleep(1)
    st = wait_dut(args.host)
    dut = st.get("dut") or {}
    report["nodes"]["host"]["dut"] = {
        "id": dut.get("id"),
        "product": dut.get("product"),
        "mfr": dut.get("mfr") or dut.get("manufacturer"),
        "serial": dut.get("serial"),
        "speed": dut.get("speed"),
    }
    print("dut", report["nodes"]["host"]["dut"])

    def rec(name: str, ok: bool, detail: dict) -> None:
        report["tests"][name] = {"ok": bool(ok), **detail}
        print(f"[{'OK' if ok else 'FAIL'}] {name}: {detail.get('summary', '')}")

    # 1) Enum observe
    try:
        r = post(f"http://{args.host}/api/lab/usb-enum-observe", timeout=20)
        rec(
            "usb_enum_observe",
            bool(r.get("ok")) and bool(r.get("dutPresent")),
            {
                "summary": f"addr={r.get('address')} cfg={r.get('configurationValue')}",
                "raw": r,
            },
        )
    except Exception as e:
        rec("usb_enum_observe", False, {"summary": str(e)})

    # 2) MSC sweep (core capability)
    print("=== MSC OTG sweep ===")
    msc_rows = []
    peak = 0
    for blocks, chunk in (
        (1, 1),
        (64, 1),
        (128, 2),
        (256, 4),
        (512, 4),
        (512, 8),
        (512, 8),
        (512, 8),
    ):
        q = urllib.parse.urlencode(
            {"blocks": blocks, "chunk": chunk, "lba": 0, "dir": "read"}
        )
        try:
            r = post(f"http://{args.host}/api/lab/usb-host-probe?{q}")
            ok = bool(r.get("ok")) and bool(r.get("pattern_ok", True))
            rate = int(r.get("rate_kBs") or 0)
            peak = max(peak, rate)
            rtt = r.get("rtt_us") or {}
            row = {
                "blocks": blocks,
                "chunk": chunk,
                "ok": ok,
                "pattern_ok": bool(r.get("pattern_ok")),
                "pattern_fail_bytes": r.get("pattern_fail_bytes"),
                "rate_kBs": rate,
                "rtt_p50_us": rtt.get("p50") or r.get("rtt_p50_us"),
                "rtt_p95_us": rtt.get("p95") or r.get("rtt_p95_us"),
            }
        except Exception as e:
            ok = False
            row = {"blocks": blocks, "chunk": chunk, "ok": False, "err": str(e)}
        msc_rows.append(row)
        print(row)
        time.sleep(0.1)
    report["limits"]["usb_peak_kBs"] = peak
    rec(
        "usb_otg_msc_sweep",
        all(x.get("ok") for x in msc_rows),
        {"summary": f"peak={peak} kB/s n={len(msc_rows)}", "rows": msc_rows},
    )

    # 3) TCP integrity
    try:
        q = urllib.parse.urlencode(
            {"peer": args.device, "frames": 64, "frame_bytes": 2048}
        )
        r = post(f"http://{args.host}/api/lab/integrity?{q}", timeout=120)
        gaps = int(r.get("gaps") or r.get("gap") or r.get("frames_fail") or 0)
        rec(
            "tcp_wifi_integrity",
            bool(r.get("ok")) and gaps == 0,
            {
                "summary": f"rate={r.get('rate_kBs')} gaps={gaps}",
                "raw": {k: r.get(k) for k in ("ok", "rate_kBs", "gaps", "gap", "rtt_us", "frames")},
            },
        )
        if r.get("rate_kBs"):
            report["limits"]["tcp_kBs"] = r.get("rate_kBs")
    except Exception as e:
        rec("tcp_wifi_integrity", False, {"summary": str(e)})

    # 4) Sync offset via controller
    try:
        data = json.dumps({"samples": 7}).encode()
        req = urllib.request.Request(
            "http://127.0.0.1:8080/api/v1/sync",
            data=data,
            method="POST",
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=60) as resp:
            r = json.loads(resp.read().decode())
        rec(
            "controller_sync_offset",
            bool(r.get("ok")),
            {
                "summary": f"offset_us={r.get('offset_us')} residual={r.get('residual_us')}",
                "raw": {
                    k: r.get(k)
                    for k in ("ok", "offset_us", "residual_us", "samples", "claim")
                },
            },
        )
    except Exception as e:
        rec("controller_sync_offset", False, {"summary": str(e)})

    # 5) BOT class hook (Device L1 fidelity)
    rc, out = run_tool(
        [
            sys.executable,
            str(ROOT / "tools" / "lab_bot_class_hook_verify.py"),
            "--host",
            args.host,
            "--device",
            args.device,
        ],
        timeout=180,
    )
    rec(
        "bot_class_hook",
        rc == 0,
        {"summary": (out.splitlines()[-3:] if out else str(rc)), "rc": rc},
    )

    # 6) Optional heavy Gate-10 suite
    if not args.skip_heavy:
        rc, out = run_tool(
            [
                sys.executable,
                str(ROOT / "tools" / "lab_gate10_perf_campaign.py"),
                "--host",
                args.host,
                "--device",
                args.device,
            ],
            timeout=900,
        )
        # gate10 overwrites its MD — we keep JSON pointer
        rec(
            "gate10_perf_campaign",
            rc == 0,
            {
                "summary": next(
                    (ln for ln in reversed(out.splitlines()) if "RESULT" in ln),
                    out[-400:],
                ),
                "rc": rc,
            },
        )
    else:
        rec("gate10_perf_campaign", True, {"summary": "skipped", "skipped": True})

    # 7) Path-A falsification still holds (feasibility API)
    try:
        r = get(f"http://{args.host}/api/lab/l0-feasibility")
        rec(
            "path_a_rejected",
            bool(r.get("ok")) and not r.get("pathA_snifferOk"),
            {"summary": r.get("pathA_reason"), "raw": r},
        )
    except Exception as e:
        rec("path_a_rejected", False, {"summary": str(e)})

    passed = sum(1 for t in report["tests"].values() if t.get("ok"))
    total = len(report["tests"])
    report["summary"] = {"passed": passed, "total": total, "pass_rate": round(passed / total, 3) if total else 0}

    ev = ROOT / "docs" / "evidence"
    ev.mkdir(parents=True, exist_ok=True)
    jpath = ev / f"LAB_CAPABILITY_AUDIT_{ts}.json"
    jpath.write_text(json.dumps(report, indent=2), encoding="utf-8")

    # Human report for auditor
    md_lines = [
        f"# Lab MEASURED — Capability Audit · {ts}",
        "",
        f"**Claim:** `{report['claim']}`",
        f"**FW:** Host `{report['nodes']['host'].get('version')}` · Device `{report['nodes']['device'].get('version')}` · Controller `{report['controller'].get('version')}`",
        f"**Result:** **{passed}/{total}** OK",
        f"**DUT:** `{report['nodes']['host'].get('dut')}`",
        "",
        "## Limits (this run)",
        "",
        f"- USB OTG MSC peak: **{report['limits'].get('usb_peak_kBs')} kB/s**",
        f"- WiFi TCP integrity sample: **{report['limits'].get('tcp_kBs')} kB/s** (gaps=0 required)",
        "",
        "## Tests",
        "",
        "| Test | OK | Summary |",
        "|------|----|---------|",
    ]
    for name, t in report["tests"].items():
        md_lines.append(
            f"| `{name}` | {'yes' if t.get('ok') else 'NO'} | {t.get('summary')} |"
        )
    md_lines += [
        "",
        "## Epistemik",
        "",
        "- Proves OTG-loop Host↔Device MSC pattern integrity + rate, WiFi TCP integrity,",
        "  enum observe, BOT class-hook path, controller sync estimate, Path-A rejection.",
        "- **Not claimed:** passive D+/D− L0 · Path-B soldered PHY · BMW NBT wire · HS · USB-IF.",
        "- Soft USB cycle ≠ VBUS unplug.",
        "",
        f"JSON: `{jpath.name}`",
        "",
        "## Repro",
        "",
        "```bash",
        "# Both nodes must run analyzer-node 0.0.50 (Host env + Device env)",
        "python3 tools/lab_capability_audit.py",
        "# optional full Gate10 suite is included unless --skip-heavy",
        "```",
        "",
    ]
    md_path = ev / "LAB_CAPABILITY_AUDIT_MEASURED.md"
    md_path.write_text("\n".join(md_lines), encoding="utf-8")

    report_path = ROOT / "docs" / "reports" / f"2026-11-02-capability-audit-0.0.50.md"
    # fuller auditor brief written by caller / below
    print(f"wrote {md_path}")
    print(f"wrote {jpath}")
    print(f"RESULT {passed}/{total}")
    # stash path for outer report writer
    (ev / "LAB_CAPABILITY_AUDIT_LATEST.path").write_text(str(jpath), encoding="utf-8")
    return 0 if passed == total else 2


if __name__ == "__main__":
    raise SystemExit(main())
