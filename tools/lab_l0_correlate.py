#!/usr/bin/env python3
"""Gate 10F — correlate L0 packet batch with L1 capture (offline/synth).

Default: synthetic SETUP/DATA/ACK ↔ mock L1 enum events.
Optional: --l0 file.json --l1 URL or file.

Claim: l0_l1_correlate_lab — NOT live wire until Path B samples exist.
"""
from __future__ import annotations

import argparse
import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from usb_fs_l0.correlate import correlate  # noqa: E402
from usb_fs_l0.decoder import decode_nrzi_bits  # noqa: E402
from usb_fs_l0.gen_synth import synth_get_descriptor_states  # noqa: E402
from usb_fs_l0.sample_io import line_states_from_file  # noqa: E402


def load_json(path_or_url: str) -> dict | list:
    if path_or_url.startswith("http://") or path_or_url.startswith("https://"):
        with urllib.request.urlopen(path_or_url, timeout=15) as r:
            return json.loads(r.read().decode())
    return json.loads(Path(path_or_url).read_text())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--l0", default="", help="l0_packet batch JSON or .l0samp")
    ap.add_argument("--l1", default="", help="capture JSON file or http://host/api/capture")
    ap.add_argument("--synth", action="store_true", help="use built-in synth L0+L1")
    args = ap.parse_args()

    if args.synth or (not args.l0 and not args.l1):
        l0 = decode_nrzi_bits(synth_get_descriptor_states())
        l1 = [
            {"type": "enum_info", "timestamp_us": 0, "note": "get_descriptor"},
            {"type": "enum_info", "timestamp_us": 1, "note": "ep0_data"},
            {"type": "enum_info", "timestamp_us": 2, "note": "status"},
        ]
    else:
        if args.l0.endswith(".l0samp"):
            _, states = line_states_from_file(args.l0)
            l0 = decode_nrzi_bits(states)
        else:
            blob = load_json(args.l0)
            l0 = blob.get("packets") if isinstance(blob, dict) else blob
        blob = load_json(args.l1)
        if isinstance(blob, dict):
            l1 = blob.get("events") or blob.get("items") or []
        else:
            l1 = blob

    r = correlate(l0, l1)
    print(json.dumps(r, indent=2))
    if r.get("matched", 0) < 1:
        print("FAIL: no matches")
        return 1
    print("RESULT OK — L0↔L1 correlator")
    print("claim: l0_l1_correlate_lab (offline/synth)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
