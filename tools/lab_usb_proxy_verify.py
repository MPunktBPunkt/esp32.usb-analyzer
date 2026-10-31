#!/usr/bin/env python3
"""Gate-4 A4: verify MSC bulk proxy Device→Host DUT (byte-exact).

Paths MEASURED:
  1) Device selftest  POST /api/lab/usb-proxy-selftest  (WiFi MSC proxy)
  2) Host raw read    POST /api/lab/usb-host-read       (OTG DUT)
  Compare hex/CRC — claim: msc_bulk_proxy_selftest (NOT transparent MITM).

Optional:
  3) PC block device READ of the Device gadget (if visible on this host).

Wiring:
  FS-DUT ──OTG── Host .88 (blue)     ← required (not OTG↔OTG loop)
  PC     ──OTG── Device .89 (green)  ← optional for path 3
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import struct
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import zlib


def get(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=10) as r:
        return json.loads(r.read().decode())


def post(url: str, timeout: float = 60) -> dict:
    req = urllib.request.Request(url, method="POST", data=b"")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")
        try:
            return json.loads(body)
        except Exception:
            return {"ok": False, "err": f"http_{e.code}", "body": body[:300]}


def crc32_hex(data: bytes) -> str:
    return f"{zlib.crc32(data) & 0xffffffff:08x}"


def find_gadget_disk(vid: str, pid: str) -> str | None:
    """Best-effort: find /dev/sdX matching USB VID:PID via udev/sysfs."""
    vid = (vid or "").lower().replace("0x", "")
    pid = (pid or "").lower().replace("0x", "")
    if len(vid) == 4 and len(pid) == 4:
        for path in glob.glob("/sys/block/sd*/device"):
            # USB storage: ../../../../../../idVendor
            cur = path
            for _ in range(8):
                cur = os.path.dirname(cur)
                v = os.path.join(cur, "idVendor")
                p = os.path.join(cur, "idProduct")
                if os.path.isfile(v) and os.path.isfile(p):
                    with open(v) as f:
                        sv = f.read().strip().lower()
                    with open(p) as f:
                        sp = f.read().strip().lower()
                    if sv == vid and sp == pid:
                        name = os.path.basename(os.path.dirname(path))
                        return f"/dev/{name}"
                    break
    return None


def read_pc_blocks(dev: str, lba: int, blocks: int) -> bytes:
    with open(dev, "rb", buffering=0) as f:
        f.seek(lba * 512)
        data = f.read(blocks * 512)
    if len(data) != blocks * 512:
        raise IOError(f"short read {len(data)}")
    return data


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88")
    ap.add_argument("--device", default="192.168.178.89")
    ap.add_argument("--lba", type=int, default=0)
    ap.add_argument("--blocks", type=int, default=4)
    ap.add_argument("--arm", action="store_true", help="run lab_usb_proxy_arm first")
    ap.add_argument("--pc-dev", default="", help="force PC blockdev e.g. /dev/sdb")
    ap.add_argument("--skip-pc", action="store_true")
    ap.add_argument("--write-marker", action="store_true",
                    help="Host writes marker then Device proxy-read must match")
    args = ap.parse_args()

    if args.blocks < 1 or args.blocks > 16:
        print("blocks must be 1..16")
        return 2

    if args.arm:
        arm_py = os.path.join(os.path.dirname(__file__), "lab_usb_proxy_arm.py")
        rc = subprocess.call(
            [sys.executable, arm_py, "--host", args.host, "--device", args.device]
        )
        if rc != 0:
            print("arm failed rc=", rc)
            return rc

    # Soft re-enum can briefly detach Host DUT — wait for it.
    hdut = {}
    g = {}
    h = {}
    d = {}
    for _ in range(20):
        h = get(f"http://{args.host}/api/status")
        d = get(f"http://{args.device}/api/status")
        hdut = h.get("dut") or {}
        g = d.get("gadget") or {}
        if hdut.get("present") and (not args.arm or g.get("proxy")):
            break
        time.sleep(0.5)

    print("host", h.get("version"), "dut", hdut.get("id"), hdut.get("product"))
    print("device", d.get("version"), "proxy", g.get("proxy"),
          "applied", g.get("appliedId"), "ident", g.get("identityApplied"),
          "hits", g.get("proxyHits"), "fails", g.get("proxyFails"))

    if not hdut.get("present"):
        print("FAIL: no Host DUT — plug FS stick into BLUE .88")
        return 2
    if not g.get("proxy"):
        print("FAIL: proxy not armed — run with --arm or lab_usb_proxy_arm.py")
        return 2

    # Detect OTG loop (Host DUT is the analyzer gadget itself).
    dut_id = (hdut.get("id") or "").lower()
    prod = (hdut.get("product") or "").upper()
    if dut_id.startswith("303a:") and ("ANALYZER" in prod or "ESP" in prod
                                       or (hdut.get("serial") or "") == "lab-gadget"):
        print("FAIL: Host DUT is Device gadget (OTG loop). "
              "Unplug OTG↔OTG; put FS stick on BLUE (.88), PC on GREEN (.89).")
        return 2

    q = urllib.parse.urlencode({"lba": args.lba, "blocks": args.blocks})

    if args.write_marker:
        # Use Host write probe of 1 block as marker seed, then compare reads.
        wq = urllib.parse.urlencode({
            "dir": "write", "lba": args.lba, "blocks": 1, "chunk": 1,
        })
        wr = post(f"http://{args.host}/api/lab/usb-host-probe?{wq}", timeout=90)
        print("host-write-marker", "ok" if wr.get("ok") else "FAIL",
              wr.get("err"))
        if not wr.get("ok"):
            return 5

    t0 = time.time()
    host_r = post(f"http://{args.host}/api/lab/usb-host-read?{q}", timeout=60)
    t_host = time.time() - t0
    t1 = time.time()
    dev_r = post(f"http://{args.device}/api/lab/usb-proxy-selftest?{q}",
                 timeout=60)
    t_dev = time.time() - t1

    print("host-read", "ok" if host_r.get("ok") else "FAIL",
          "crc", host_r.get("crc32"), f"{host_r.get('duration_ms')}ms",
          f"wall={t_host*1000:.0f}ms")
    print("proxy-selftest", "ok" if dev_r.get("ok") else "FAIL",
          "crc", dev_r.get("crc32"), f"{dev_r.get('duration_ms')}ms",
          f"wall={t_dev*1000:.0f}ms",
          "hits", dev_r.get("proxyHits"), "fails", dev_r.get("proxyFails"))

    if not host_r.get("ok") or not dev_r.get("ok"):
        print("FAIL: read path error", host_r.get("err"), dev_r.get("err"))
        return 1

    host_hex = host_r.get("hex") or ""
    dev_hex = dev_r.get("hex") or ""
    match = host_hex == dev_hex and host_r.get("crc32") == dev_r.get("crc32")
    print("compare", "MATCH" if match else "MISMATCH",
          "bytes", args.blocks * 512,
          "claim=msc_bulk_proxy_selftest")
    if not match:
        # show first differing offset
        for i in range(0, min(len(host_hex), len(dev_hex)), 2):
            if host_hex[i:i+2] != dev_hex[i:i+2]:
                print(f"  first diff at byte {i//2}: "
                      f"host={host_hex[i:i+2]} proxy={dev_hex[i:i+2]}")
                break
        return 1

    # Optional PC path
    if not args.skip_pc:
        applied = (g.get("appliedId") or dut_id or "").lower()
        parts = applied.split(":")
        vid, pid = (parts + ["", ""])[:2]
        dev = args.pc_dev or find_gadget_disk(vid, pid)
        if not dev:
            print("pc-path SKIP (no matching blockdev on this host)")
        else:
            try:
                pc = read_pc_blocks(dev, args.lba, args.blocks)
                pc_crc = crc32_hex(pc)
                pc_hex = pc.hex()
                ok_pc = pc_hex == host_hex
                print("pc-read", dev, "crc", pc_crc,
                      "MATCH" if ok_pc else "MISMATCH")
                if not ok_pc:
                    return 4
                print("claim=msc_bulk_proxy_pc_to_dut (PC bytes == Host DUT)")
            except PermissionError:
                print("pc-path SKIP (need root for", dev, ")")
            except Exception as e:
                print("pc-path ERR", e)
                return 4

    print("RESULT OK — MSC bulk proxy Device→Host DUT byte-exact")
    print("NOT claimed: transparent MITM / USB control forward")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
