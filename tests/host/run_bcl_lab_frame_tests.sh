#!/usr/bin/env bash
# Host unit tests for BclLabFrame.h (no ESP hardware).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/tests/host/test_bcl_lab_frame.cpp"
BIN="$ROOT/tests/host/test_bcl_lab_frame"
INC="$ROOT/firmware/common/include"
g++ -std=c++17 -Wall -Wextra -O1 -I"$INC" -o "$BIN" "$SRC"
"$BIN"
echo "claim: host unit — not USB / not HU wire"
