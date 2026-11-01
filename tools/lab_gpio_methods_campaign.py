#!/usr/bin/env python3
"""GPIO lab methods campaign — calibrate, matrix, gap/glitch, jitter, cross-link.

Implements the Gate-10 GPIO test plan without requiring a soldered USB PHY.
Cross-link needs jumpers Host↔Device GPIO10/11/12; without jumpers records
jumperLikely=false (MEASURED absence).

Claim: gpio_methods_campaign_lab
"""
from __future__ import annotations

import argparse
import concurrent.futures
import json
import time
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def get(url: str, timeout: float = 120) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")
        try:
            return json.loads(body)
        except Exception:
            return {"ok": False, "err": body, "http": getattr(e, "code", 0)}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--l0st-host", default="192.168.178.187")
    ap.add_argument("--rate", type=int, default=100000)
    args = ap.parse_args()
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    report: dict = {
        "classification": "MEASURED",
        "timestamp_utc": ts,
        "claim": "gpio_methods_campaign_lab",
        "not_claimed": [
            "soldered_path_b_phy",
            "fs_soft_host",
            "crosslink_without_jumpers_as_pass",
        ],
        "tests": {},
    }

    def rec(name: str, ok: bool, detail: dict) -> None:
        report["tests"][name] = {"ok": ok, **detail}
        print(f"[{'OK' if ok else 'FAIL'}] {name}", detail.get("summary", ""))

    # 1) Calibrate both nodes
    print("=== calibrate ===")
    for label, ip in (("host", args.host), ("device", args.device)):
        r = get(f"http://{ip}/api/lab/gpio-calibrate?spin_us=1000")
        rec(
            f"calibrate_{label}",
            bool(r.get("ok")),
            {
                "summary": f"cyclesPerUs={r.get('cyclesPerUs')} errPpm={r.get('errPpm')}",
                "raw": r,
            },
        )

    # 2) Host self loopback + L0ST (positive control)
    print("=== host self txrx_l0st ===")
    q = urllib.parse.urlencode(
        {
            "mode": "txrx_l0st",
            "pattern": "setup_ok",
            "rate": args.rate,
            "l0st_host": args.l0st_host,
        }
    )
    # start ingest
    import subprocess, sys

    ingest = subprocess.Popen(
        [
            sys.executable,
            str(ROOT / "tools" / "l0_path_b_ingest.py"),
            "--listen",
            "0.0.0.0:9760",
            "--out",
            str(ROOT / "logs" / "l0"),
            "--decode",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(0.5)
    r = get(f"http://{args.host}/api/lab/path-b-gpio?{q}")
    rec(
        "host_self_loopback_l0st",
        bool(r.get("ok")) and bool(r.get("loopbackOk")),
        {
            "summary": f"achieved={r.get('achievedRateHz')} mm={r.get('mismatches')} l0st={r.get('l0stOk')}",
            "raw": r,
        },
    )

    # 3) Error / framing matrix (non-destructive Path B pins)
    print("=== error matrix ===")
    matrix = [
        "setup_ok",
        "setup_bad_crc",
        "data_bad_crc",
        "pid_bad",
        "bus_states",
        "eop",
        "sync",
    ]
    rows = []
    for pat in matrix:
        q = urllib.parse.urlencode(
            {"mode": "txrx", "pattern": pat, "rate": args.rate}
        )
        r = get(f"http://{args.host}/api/lab/path-b-gpio?{q}")
        rows.append(
            {
                "pattern": pat,
                "ok": r.get("ok"),
                "loopbackOk": r.get("loopbackOk"),
                "mismatches": r.get("mismatches"),
                "achieved": r.get("achievedRateHz"),
            }
        )
    rec(
        "error_matrix_path_b",
        all(x.get("ok") for x in rows),
        {"summary": f"{len(rows)} patterns", "rows": rows},
    )

    # 4) Gap sweep
    print("=== gap sweep ===")
    gaps = []
    for g in (1, 8, 32, 64, 256, 1024):
        q = urllib.parse.urlencode(
            {"mode": "txrx", "pattern": f"gap_{g}", "rate": args.rate}
        )
        r = get(f"http://{args.host}/api/lab/path-b-gpio?{q}")
        gaps.append(
            {
                "gap": g,
                "ok": r.get("ok"),
                "states": r.get("statesTx"),
                "achieved": r.get("achievedRateHz"),
            }
        )
    rec(
        "gap_sweep",
        all(x.get("ok") for x in gaps),
        {"summary": f"{len(gaps)} gaps", "rows": gaps},
    )

    # 5) Glitch sweep
    print("=== glitch sweep ===")
    glitches = []
    for kind in ("glitch_se0_", "glitch_k_"):
        for n in (1, 2, 4, 8, 16):
            q = urllib.parse.urlencode(
                {"mode": "txrx", "pattern": f"{kind}{n}", "rate": args.rate}
            )
            r = get(f"http://{args.host}/api/lab/path-b-gpio?{q}")
            glitches.append(
                {
                    "pattern": f"{kind}{n}",
                    "ok": r.get("ok"),
                    "mm": r.get("mismatches"),
                }
            )
    rec(
        "glitch_sweep",
        all(x.get("ok") for x in glitches),
        {"summary": f"{len(glitches)} glitches", "rows": glitches},
    )

    # 6) Jitter sweep
    print("=== jitter sweep ===")
    jitters = []
    for jp in (0, 5, 10, 20, 40):
        q = urllib.parse.urlencode(
            {
                "mode": "txrx",
                "pattern": "jk_burst",
                "rate": args.rate,
                "jitter": jp,
            }
        )
        r = get(f"http://{args.host}/api/lab/path-b-gpio?{q}")
        jitters.append(
            {
                "jitterPct": jp,
                "ok": r.get("ok"),
                "loopbackOk": r.get("loopbackOk"),
                "achieved": r.get("achievedRateHz"),
            }
        )
    rec(
        "jitter_sweep",
        all(x.get("ok") for x in jitters),
        {"summary": f"{len(jitters)} jitters", "rows": jitters},
    )

    # 7) Cross-link: Device RX sync + Host TX sync (needs jumpers)
    print("=== cross-link attempt ===")

    def device_rx():
        q = urllib.parse.urlencode(
            {
                "mode": "rx_sync",
                "samples": 400,
                "rate": args.rate,
                "timeout_ms": 4000,
            }
        )
        return get(f"http://{args.device}/api/lab/path-b-gpio-rx?{q}", timeout=15)

    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as ex:
        fut = ex.submit(device_rx)
        time.sleep(0.3)
        q = urllib.parse.urlencode(
            {
                "mode": "tx_sync",
                "pattern": "setup_ok",
                "rate": args.rate,
            }
        )
        ht = get(f"http://{args.host}/api/lab/path-b-gpio?{q}")
        try:
            dr = fut.result(timeout=12)
        except Exception as e:
            dr = {"ok": False, "err": str(e)}
    jumper = bool(dr.get("jumperLikely"))
    rec(
        "crosslink_host_tx_device_rx",
        bool(ht.get("ok")) and bool(dr.get("ok")),
        {
            "summary": f"syncSeen={dr.get('syncSeen')} edges={dr.get('edges')} jumperLikely={jumper}",
            "host": ht,
            "device": dr,
            "jumper_likely": jumper,
        },
    )

    # 8) Device idle histogram (no TX) — baseline float
    print("=== device idle hist ===")
    q = urllib.parse.urlencode({"mode": "rx", "samples": 200, "rate": args.rate})
    r = get(f"http://{args.device}/api/lab/path-b-gpio-rx?{q}")
    rec(
        "device_idle_histogram",
        bool(r.get("ok")),
        {
            "summary": f"edges={r.get('edges')} J={r.get('histJ')} SE0={r.get('histSe0')}",
            "raw": r,
        },
    )

    # 9) LS-ish bit time attempt on Path B (non-destructive)
    print("=== ls bit-time attempt ===")
    q = urllib.parse.urlencode(
        {"mode": "txrx", "pattern": "eop", "rate": 1_500_000}
    )
    r = get(f"http://{args.host}/api/lab/path-b-gpio?{q}")
    rec(
        "ls_bit_time_attempt",
        bool(r.get("ok")),
        {
            "summary": f"target=1.5M achieved={r.get('achievedRateHz')} loop={r.get('loopbackOk')}",
            "raw": r,
        },
    )

    ingest.terminate()
    try:
        ingest.wait(timeout=2)
    except Exception:
        ingest.kill()

    passed = sum(1 for t in report["tests"].values() if t.get("ok"))
    total = len(report["tests"])
    report["summary"] = {"passed": passed, "total": total}
    out_json = ROOT / "docs" / "evidence" / f"LAB_GPIO_METHODS_{ts}.json"
    out_md = ROOT / "docs" / "evidence" / "LAB_GPIO_METHODS_MEASURED.md"
    out_json.write_text(json.dumps(report, indent=2))
    lines = [
        f"# Lab MEASURED — GPIO methods campaign ({ts})",
        "",
        f"**Result:** **{passed}/{total}**",
        "",
        "| Test | OK | Summary |",
        "|------|----|---------|",
    ]
    for name, t in report["tests"].items():
        lines.append(
            f"| `{name}` | {'yes' if t.get('ok') else 'NO'} | {t.get('summary')} |"
        )
    xl = report["tests"].get("crosslink_host_tx_device_rx") or {}
    lines += [
        "",
        "## Cross-link note",
        "",
        f"jumperLikely={xl.get('jumper_likely')} — without Host↔Device wires on "
        "GPIO10/11/12 this stays false; software+protocol still MEASURED.",
        "",
        "**Claim:** `gpio_methods_campaign_lab`",
        "**Not claimed:** Path-B PHY tap · FS soft-host · cross-link pass without jumpers.",
        "",
        f"JSON: `{out_json.name}`",
        "",
        "Wiring: `docs/hardware/GPIO_LAB_METHODS.md`",
    ]
    out_md.write_text("\n".join(lines) + "\n")
    print(f"wrote {out_md}")
    print(f"RESULT {passed}/{total}")
    return 0 if passed == total else 2


if __name__ == "__main__":
    raise SystemExit(main())
