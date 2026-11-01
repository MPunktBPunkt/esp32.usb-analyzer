#!/usr/bin/env bash
# Gate 10B–F host unit tests (no ESP hardware).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
python3 "$ROOT/tests/host/test_usb_fs_l0.py"
python3 "$ROOT/tests/host/test_usb_fs_l0_fuzz.py"
python3 "$ROOT/tests/host/test_l0_golden.py"
echo "claim: usb_fs_l0_offline — not live D+/D− / not HU wire"
