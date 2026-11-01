"""Correlate l0_packet.v1 batches with L1 `/api/capture` events (Gate 10F)."""

from __future__ import annotations

from typing import Any


def _l1_kind(ev: dict[str, Any]) -> str:
    t = (ev.get("event_type") or ev.get("type") or ev.get("t") or "").lower()
    note = (ev.get("note") or "").lower()
    dec = ev.get("decoded") or {}
    if isinstance(dec, dict):
        note = note or str(dec.get("note") or "").lower()
        proto = str(dec.get("protocol") or "").lower()
        if proto == "aoa" or "get_descriptor" in note or "setup" in note:
            return "ep0"
        if "bot_cbw" in t or t == "bot_cbw" or "cbw" in note:
            return "bot_cbw"
        if "bot_csw" in t or t == "bot_csw":
            return "bot_csw"
    if "enum" in t or "enum" in note or "get_config" in note or "get_desc" in note:
        return "enum"
    if t in ("bot_cbw", "botcsw", "bot_csw"):
        return t
    if "hid" in t:
        return "hid"
    if "suspend" in t or "resume" in t:
        return "power"
    return t or "other"


def _l0_kind(pkt: dict[str, Any]) -> str:
    pid = (pkt.get("pid") or "").upper()
    if pid == "SETUP":
        return "ep0"
    if pid in ("DATA0", "DATA1") and pkt.get("payload_len", 0) == 31:
        return "bot_cbw"
    if pid in ("DATA0", "DATA1") and pkt.get("payload_len", 0) == 13:
        return "bot_csw"
    if pid in ("ACK", "NAK", "STALL"):
        return "handshake"
    if pid in ("IN", "OUT"):
        return "token"
    return pid.lower() or "other"


def _kinds_compatible(k0: str, k1: str) -> bool:
    if k0 == "ep0" and k1 in ("ep0", "enum"):
        return True
    if k0 == "bot_cbw" and k1 == "bot_cbw":
        return True
    if k0 == "bot_csw" and k1 == "bot_csw":
        return True
    if k0 in ("handshake", "token", "data0", "data1") and k1 in (
        "ep0",
        "enum",
        "bot_cbw",
        "bot_csw",
    ):
        return True
    if k0 == k1:
        return True
    return False


def correlate(
    l0_packets: list[dict[str, Any]],
    l1_events: list[dict[str, Any]],
    window_us: int = 50_000,
    ignore_time: bool = False,
) -> dict[str, Any]:
    """Greedy kind(+time) match. Use ignore_time for synth L0 vs real ESP clocks."""
    matches = []
    used_l1: set[int] = set()
    for i, pkt in enumerate(l0_packets):
        k0 = _l0_kind(pkt)
        t0 = int(pkt.get("timestamp_us") or i)
        best = None
        best_dt = None
        for j, ev in enumerate(l1_events):
            if j in used_l1:
                continue
            k1 = _l1_kind(ev)
            if not _kinds_compatible(k0, k1):
                continue
            t1 = int(ev.get("timestamp_us") or ev.get("t_us") or j)
            dt = abs(t1 - t0)
            if ignore_time:
                # Prefer earliest unused compatible L1 event (order-preserving).
                best = j
                best_dt = dt
                break
            lim = window_us if max(t0, t1) > 1000 else max(window_us, 100)
            if dt <= lim and (best_dt is None or dt < best_dt):
                best = j
                best_dt = dt
        if best is not None:
            used_l1.add(best)
            matches.append(
                {
                    "l0_index": i,
                    "l1_index": best,
                    "l0_pid": pkt.get("pid"),
                    "l1_type": l1_events[best].get("event_type")
                    or l1_events[best].get("type"),
                    "dt_us": best_dt,
                    "kind": k0,
                }
            )
    unmatched_l0 = []
    matched_l0 = {m["l0_index"] for m in matches}
    for i, pkt in enumerate(l0_packets):
        if i in matched_l0:
            continue
        unmatched_l0.append(
            {
                "l0_index": i,
                "pid": pkt.get("pid"),
                "kind": _l0_kind(pkt),
                "crc_ok": pkt.get("crc_ok"),
                "note": "no_compatible_l1_event",
            }
        )
    unmatched_l1 = []
    for j, ev in enumerate(l1_events):
        if j in used_l1:
            continue
        unmatched_l1.append(
            {
                "l1_index": j,
                "type": ev.get("event_type") or ev.get("type"),
                "kind": _l1_kind(ev),
                "note": ev.get("note"),
            }
        )
    return {
        "ok": True,
        "claim": "l0_l1_correlate_lab",
        "note": (
            "ignore_time_kind_order"
            if ignore_time
            else "synthetic_or_offline_not_live_wire"
        ),
        "l0_n": len(l0_packets),
        "l1_n": len(l1_events),
        "matched": len(matches),
        "matches": matches,
        "unmatched_l0": unmatched_l0,
        "unmatched_l1": unmatched_l1,
        "stack_gap_n": len(unmatched_l0),
        "ignore_time": ignore_time,
    }
