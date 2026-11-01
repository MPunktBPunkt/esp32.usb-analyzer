"""l0_packet.v1 helpers."""

from __future__ import annotations

from typing import Any


def packet_dict(
    *,
    timestamp_us: int = 0,
    direction: str = "unknown",
    pid: str,
    pid_byte: int,
    addr: int | None = None,
    endp: int | None = None,
    data_toggle: int | None = None,
    payload: bytes = b"",
    crc_ok: bool = True,
    crc_value: int = 0,
    raw_valid: bool = True,
    note: str = "",
) -> dict[str, Any]:
    return {
        "version": 1,
        "schema": "l0_packet.v1",
        "timestamp_us": timestamp_us,
        "timestamp_domain": "capture_local",
        "dir": direction,
        "pid": pid,
        "pid_byte": pid_byte & 0xFF,
        "addr": addr,
        "endp": endp,
        "data_toggle": data_toggle,
        "payload_hex": payload.hex(),
        "payload_len": len(payload),
        "crc_ok": bool(crc_ok),
        "crc_value": crc_value,
        "raw_valid": bool(raw_valid),
        "note": note,
    }


def batch_dict(packets: list[dict[str, Any]]) -> dict[str, Any]:
    return {"version": 1, "schema": "l0_packet_batch.v1", "packets": packets}
