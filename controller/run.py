#!/usr/bin/env python3
"""Run discovery TCP + Web API (Slice A).

Usage (from repo root):
  PYTHONPATH=. python3 controller/run.py
  PYTHONPATH=. python3 controller/run.py --http-port 8080 --control-port 9750
"""

from __future__ import annotations

import argparse
import threading

from controller.api.http_server import serve_forever as http_serve
from controller.discovery.tcp_server import serve_forever as tcp_serve


def main() -> None:
    ap = argparse.ArgumentParser(description="usb-analyzer controller (Slice A)")
    ap.add_argument("--control-host", default="0.0.0.0")
    ap.add_argument("--control-port", type=int, default=9750)
    ap.add_argument("--http-host", default="0.0.0.0")
    ap.add_argument("--http-port", type=int, default=8080)
    ap.add_argument(
        "--auto-role",
        default=None,
        help="Optional assign_role on first HELLO (host|device)",
    )
    args = ap.parse_args()

    threading.Thread(
        target=tcp_serve,
        args=(args.control_host, args.control_port, args.auto_role),
        daemon=True,
    ).start()
    http_serve(args.http_host, args.http_port)


if __name__ == "__main__":
    main()
