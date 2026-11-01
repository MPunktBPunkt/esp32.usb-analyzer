"""FS USB packet decode from NRZI line-state or raw packet bytes (offline)."""

from __future__ import annotations

from dataclasses import dataclass

from .crc import crc16_usb, token_crc5_ok
from .nrzi import (
    J,
    SE0,
    bit_stuff,
    bit_unstuff,
    bits_to_bytes_lsb,
    bytes_to_bits_lsb,
    nrzi_decode_from_idle,
    nrzi_encode,
)
from .schema import packet_dict

PID_NAMES = {
    0xE1: "OUT",
    0x69: "IN",
    0xA5: "SOF",
    0x2D: "SETUP",
    0xC3: "DATA0",
    0x4B: "DATA1",
    0xD2: "ACK",
    0x5A: "NAK",
    0x1E: "STALL",
    0x96: "NYET",
}

TOKEN_PIDS = {0xE1, 0x69, 0x2D, 0xA5}
DATA_PIDS = {0xC3, 0x4B}
HANDSHAKE_PIDS = {0xD2, 0x5A, 0x1E, 0x96}


def pid_name(pid: int) -> str:
    return PID_NAMES.get(pid & 0xFF, f"UNK_{pid & 0xFF:02X}")


def _pid_valid(pid: int) -> bool:
    low = pid & 0x0F
    high = (pid >> 4) & 0x0F
    return high == ((~low) & 0x0F)


@dataclass
class RawPacket:
    pid: int
    body: bytes  # after PID


def build_token(pid: int, addr: int, endp: int) -> bytes:
    from .crc import crc5_usb

    bits = []
    for i in range(7):
        bits.append((addr >> i) & 1)
    for i in range(4):
        bits.append((endp >> i) & 1)
    c5 = crc5_usb(bits)
    # ADDR | ENDP | CRC5 packed into 2 bytes after PID (16 bits: 7+4+5)
    val = (addr & 0x7F) | ((endp & 0x0F) << 7) | ((c5 & 0x1F) << 11)
    return bytes([pid & 0xFF, val & 0xFF, (val >> 8) & 0xFF])


def build_data(pid: int, payload: bytes) -> bytes:
    c = crc16_usb(payload)
    return bytes([pid & 0xFF]) + payload + bytes([c & 0xFF, (c >> 8) & 0xFF])


def build_handshake(pid: int) -> bytes:
    return bytes([pid & 0xFF])


def packet_bytes_to_nrzi_states(pkt: bytes, idle: int = J) -> list[int]:
    """Encode packet bytes → stuffed → NRZI states, plus SE0 EOP (2 bit times)."""
    # SYNC as data bits 00000001 then packet bits
    sync = [0, 0, 0, 0, 0, 0, 0, 1]
    body = bytes_to_bits_lsb(pkt)
    stuffed = bit_stuff(sync + body)
    states = nrzi_encode(stuffed, start_state=idle)
    states.extend([SE0, SE0])  # EOP
    states.append(idle)  # return to J
    return states


def decode_packet_bytes(pkt: bytes, timestamp_us: int = 0) -> dict:
    """Parse already-framed packet bytes (PID + body), no NRZI."""
    if not pkt:
        return packet_dict(pid="EMPTY", pid_byte=0, raw_valid=False, note="empty")
    pid = pkt[0]
    name = pid_name(pid)
    if not _pid_valid(pid):
        return packet_dict(
            pid=name, pid_byte=pid, raw_valid=False, note="bad_pid_complement",
            timestamp_us=timestamp_us,
        )
    if pid in HANDSHAKE_PIDS:
        return packet_dict(
            timestamp_us=timestamp_us,
            direction="unknown",
            pid=name,
            pid_byte=pid,
            crc_ok=True,
            note="handshake",
        )
    if pid in TOKEN_PIDS:
        if len(pkt) < 3:
            return packet_dict(
                pid=name, pid_byte=pid, raw_valid=False, note="short_token",
                timestamp_us=timestamp_us,
            )
        val = pkt[1] | (pkt[2] << 8)
        addr = val & 0x7F
        endp = (val >> 7) & 0x0F
        c5 = (val >> 11) & 0x1F
        ok = token_crc5_ok(addr, endp, c5)
        direction = "host_to_device"
        return packet_dict(
            timestamp_us=timestamp_us,
            direction=direction,
            pid=name,
            pid_byte=pid,
            addr=addr,
            endp=endp,
            crc_ok=ok,
            crc_value=c5,
            note="token",
        )
    if pid in DATA_PIDS:
        if len(pkt) < 3:
            return packet_dict(
                pid=name, pid_byte=pid, raw_valid=False, note="short_data",
                timestamp_us=timestamp_us,
            )
        payload = pkt[1:-2]
        c16 = pkt[-2] | (pkt[-1] << 8)
        ok = crc16_usb(payload) == c16
        toggle = 0 if pid == 0xC3 else 1
        return packet_dict(
            timestamp_us=timestamp_us,
            direction="unknown",
            pid=name,
            pid_byte=pid,
            data_toggle=toggle,
            payload=payload,
            crc_ok=ok,
            crc_value=c16,
            note="data",
        )
    return packet_dict(
        pid=name, pid_byte=pid, raw_valid=False, note="unhandled_pid",
        timestamp_us=timestamp_us,
    )


def decode_nrzi_bits(states: list[int], timestamp_us: int = 0) -> list[dict]:
    """Split on SE0, NRZI-decode each burst, unstuff, parse packets."""
    packets: list[dict] = []
    i = 0
    t = timestamp_us
    while i < len(states):
        # skip idle J
        while i < len(states) and states[i] == J:
            i += 1
        if i >= len(states):
            break
        burst: list[int] = []
        while i < len(states) and states[i] != SE0:
            burst.append(states[i])
            i += 1
        # consume SE0 run
        while i < len(states) and states[i] == SE0:
            i += 1
        if len(burst) < 8:
            continue
        bits = nrzi_decode_from_idle(burst, idle=J)
        # Expect SYNC 00000001 at start
        if len(bits) < 8:
            continue
        # find sync
        sync_at = None
        for s in range(0, min(16, len(bits) - 7)):
            if bits[s : s + 8] == [0, 0, 0, 0, 0, 0, 0, 1]:
                sync_at = s
                break
        if sync_at is None:
            continue
        body_bits = bit_unstuff(bits[sync_at + 8 :])
        raw = bits_to_bytes_lsb(body_bits)
        if not raw:
            continue
        packets.append(decode_packet_bytes(raw, timestamp_us=t))
        t += 1
    return packets
