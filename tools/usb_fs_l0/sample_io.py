"""`.l0samp` sample container — Path B ingest interface."""

from __future__ import annotations

import struct
from pathlib import Path

MAGIC = b"L0S1"
FMT_LINE_STATE = 1  # uint8 per sample: 0=SE0 1=J 2=K 3=SE1
FMT_NRZI_BITS = 2  # packed bits (unused in v1 tools; line_state preferred)


def write_line_states(path: str | Path, rate_hz: int, states: list[int]) -> None:
    path = Path(path)
    payload = bytes(s & 0xFF for s in states)
    header = struct.pack("<4sIII", MAGIC, 1, rate_hz, FMT_LINE_STATE)
    path.write_bytes(header + struct.pack("<I", len(payload)) + payload)


def read_l0samp(path: str | Path) -> tuple[int, int, bytes]:
    raw = Path(path).read_bytes()
    if len(raw) < 20 or raw[:4] != MAGIC:
        raise ValueError("bad l0samp magic")
    _ver, rate_hz, fmt = struct.unpack_from("<III", raw, 4)
    (nbytes,) = struct.unpack_from("<I", raw, 16)
    payload = raw[20 : 20 + nbytes]
    return rate_hz, fmt, payload


def line_states_from_file(path: str | Path) -> tuple[int, list[int]]:
    rate, fmt, payload = read_l0samp(path)
    if fmt != FMT_LINE_STATE:
        raise ValueError(f"unsupported fmt {fmt}")
    return rate, list(payload)
