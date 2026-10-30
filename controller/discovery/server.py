#!/usr/bin/env python3
"""Backward-compatible entry: prefer `python3 -m controller.run`.

Legacy:
  python3 controller/discovery/server.py --port 9750
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Allow running as a script without PYTHONPATH=.
ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from controller.discovery.tcp_server import serve_forever  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser(description="USB Analyzer discovery (legacy entry)")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=9750)
    ap.add_argument("--auto-role", default=None)
    args = ap.parse_args()
    print(
        "Hinweis: vollständige Lab-UI mit API → PYTHONPATH=. python3 -m controller.run",
        flush=True,
    )
    serve_forever(args.host, args.port, args.auto_role)


if __name__ == "__main__":
    main()
