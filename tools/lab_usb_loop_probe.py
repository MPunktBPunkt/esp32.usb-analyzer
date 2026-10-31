#!/usr/bin/env python3
"""OTG-loop MSC probe: Host ESP reads Device ESP over the USB-C cable.

Path: .88 Host OTG ──cable── .89 Device MSC
Not WiFi. Not Gate-4 MITM proxy.
"""
from __future__ import annotations

import argparse
import json
import urllib.parse
import urllib.request


def probe(ip: str, blocks: int, lba: int = 0) -> dict:
    q = urllib.parse.urlencode({"blocks": blocks, "lba": lba})
    req = urllib.request.Request(
        f"http://{ip}/api/lab/usb-host-probe?{q}", method="POST", data=b""
    )
    with urllib.request.urlopen(req, timeout=180) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.178.88", help="USB Host ESP IP")
    ap.add_argument("--blocks", type=int, default=128)
    ap.add_argument("--lba", type=int, default=0)
    args = ap.parse_args()

    print(f"==> USB OTG MSC probe on {args.host}  blocks={args.blocks}")
    try:
        r = probe(args.host, args.blocks, args.lba)
    except Exception as e:
        print("FAIL", e)
        return 1
    print(json.dumps(r, indent=2))
    ok = bool(r.get("ok"))
    print(
        "RESULT",
        "OK" if ok else "FAIL",
        f"rate={r.get('rate_kBs')} kB/s",
        f"pattern_ok={r.get('pattern_ok')}",
        f"path={r.get('path')}",
    )
    return 0 if ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
