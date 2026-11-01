#!/usr/bin/env python3
"""Destructive OTG wire stimulus — Host drives GPIO19/20 after stopping USB Host.

Grok/Gate-10 idea: Host as pattern generator (not parallel Host).
Device TinyUSB observes mount/suspend/resume/detach while patterns play.

Claim: wire_stimulus_lab
NOT: USB Host parallel op · Path A sniffer · FS-conformant electrical drive
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


def get_allow_err(url: str, timeout: float = 120) -> dict:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")
        try:
            return json.loads(body)
        except Exception:
            return {"ok": False, "err": body, "http": e.code}


def snap_device(ip: str) -> dict:
    s = get(f"http://{ip}/api/status")
    g = s.get("gadget") or s.get("otg") or {}
    return {
        "version": s.get("version"),
        "otg": (s.get("otg") or {}).get("state"),
        "mounted": g.get("mounted") if isinstance(g, dict) else None,
        "suspended": g.get("suspended") if isinstance(g, dict) else None,
        "suspendCount": g.get("suspendCount"),
        "resumeCount": g.get("resumeCount"),
        "raw_otg": s.get("otg"),
        "raw_gadget": s.get("gadget"),
    }


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


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument(
        "--patterns",
        default="bus_states,se0_reset,sync,eop,glitch,setup_ok,setup_bad_crc",
    )
    ap.add_argument("--rate", type=int, default=100000)
    ap.add_argument("--reboot-ms", type=int, default=2000)
    ap.add_argument(
        "--out",
        default="docs/evidence/LAB_WIRE_STIMULUS_MEASURED.md",
    )
    args = ap.parse_args()

    h0 = get(f"http://{args.host}/api/status")
    d0 = snap_device(args.device)
    print("host", h0.get("version"), "dut", (h0.get("dut") or {}).get("present"))
    print("device before", d0)

    rows = []
    patterns = [p.strip() for p in args.patterns.split(",") if p.strip()]
    for i, pat in enumerate(patterns):
        last = i == len(patterns) - 1
        reboot_ms = args.reboot_ms if last else 0
        # For multi-pattern without reboot between: only last restores host.
        # After first pattern host is already released — subsequent calls still work.
        q = urllib.parse.urlencode(
            {
                "confirm": "wire_stimulus_destructive",
                "pattern": pat,
                "rate": args.rate,
                "reboot_ms": reboot_ms,
            }
        )
        print(f"==> pattern {pat} reboot_ms={reboot_ms}")
        t0 = time.time()
        # Device snapshot mid/after — poll during request if long
        r = get_allow_err(
            f"http://{args.host}/api/lab/wire-stimulus?{q}",
            timeout=180,
        )
        wall = time.time() - t0
        time.sleep(0.3)
        d1 = snap_device(args.device)
        row = {
            "pattern": pat,
            "host": r,
            "device_after": d1,
            "wall_s": round(wall, 3),
        }
        rows.append(row)
        print(json.dumps({"pattern": pat, "ok": r.get("ok"), "err": r.get("err"),
                          "achieved": r.get("achievedRateHz"),
                          "device_otg": d1.get("otg"),
                          "suspendCount": d1.get("suspendCount")}, indent=2))
        if not r.get("ok"):
            print("FAIL pattern", pat)
            break
        if reboot_ms > 0:
            print("waiting host reboot…")
            h1 = wait_host(args.host)
            print("host back", h1.get("version"), "uptime", h1.get("uptime"))
            # wait DUT re-enum
            for _ in range(20):
                st = get(f"http://{args.host}/api/status")
                if (st.get("dut") or {}).get("present"):
                    break
                time.sleep(1)
            # MSC sanity
            try:
                q2 = urllib.parse.urlencode(
                    {"blocks": 8, "chunk": 4, "dir": "read"}
                )
                req = urllib.request.Request(
                    f"http://{args.host}/api/lab/usb-host-probe?{q2}",
                    method="POST",
                    data=b"",
                )
                with urllib.request.urlopen(req, timeout=60) as resp:
                    msc = json.loads(resp.read().decode())
            except Exception as e:
                msc = {"ok": False, "err": str(e)}
            rows.append({"post_reboot_msc": msc})
            print("post MSC", msc.get("ok"), msc.get("rate_kBs"))

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    ok_n = sum(1 for x in rows if x.get("host", {}).get("ok"))
    md = [
        f"# Lab MEASURED — Wire stimulus on GPIO19/20 (0.0.42)",
        "",
        f"**Tool:** `tools/lab_wire_stimulus.py` → `/api/lab/wire-stimulus`",
        f"**Host FW:** `{h0.get('version')}` → reboot restore",
        f"**Rate target:** {args.rate} Hz",
        "",
        "## Idea check (Grok)",
        "",
        "Host can drive D+/D− **only after** stopping the USB Host / releasing the PHY.",
        "This is a **destructive test mode**, not parallel Host operation.",
        "Electrical drive is GPIO-grade (lab cable OK); not USB-IF conformant.",
        "",
        "## Device before",
        "",
        "```json",
        json.dumps(d0, indent=2),
        "```",
        "",
        "## Patterns",
        "",
        "| Pattern | Host OK | Achieved Hz | Device otg after | suspendCount |",
        "|---------|---------|-------------|------------------|--------------|",
    ]
    for row in rows:
        if "host" not in row:
            continue
        h = row["host"]
        d = row.get("device_after") or {}
        md.append(
            f"| `{row['pattern']}` | {h.get('ok')} | {h.get('achievedRateHz')} | "
            f"{d.get('otg')} | {d.get('suspendCount')} |"
        )
    post = next((x for x in rows if "post_reboot_msc" in x), None)
    md += [
        "",
        "## Post-reboot MSC",
        "",
        f"**{'OK' if post and post['post_reboot_msc'].get('ok') else 'FAIL/n/a'}** "
        f"{(post or {}).get('post_reboot_msc', {})}",
        "",
        "## Verdict",
        "",
        f"- Ran **{ok_n}** stimulus pattern(s) with Host stack stopped.",
        "- Useful as **controlled wire stimulus** for Device-stack / future L0 compare.",
        "- Prefer **GPIO10/11 Path B lab** for non-destructive decoder pipe tests.",
        "- Prefer **soldered Path B tap** for real FS capture.",
        "",
        "**Claim:** `wire_stimulus_lab`",
        "**Not claimed:** parallel USB Host · FS 12 Mbit/s soft-host · USB-IF signal quality.",
    ]
    out.write_text("\n".join(md) + "\n")
    print(f"wrote {out}")
    return 0 if ok_n == len(patterns) else 2


if __name__ == "__main__":
    raise SystemExit(main())
