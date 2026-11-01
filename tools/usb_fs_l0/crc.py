"""USB FS CRC5 (token) and CRC16 (data) — USB 2.0 §8.3.5."""

from __future__ import annotations


def crc5_usb(data_bits_lsb_first: list[int]) -> int:
    """CRC5 over token fields (addr+endp+… as 11 bits typical), poly 0x05."""
    crc = 0x1F
    for bit in data_bits_lsb_first:
        if ((crc >> 4) ^ bit) & 1:
            crc = ((crc << 1) ^ 0x05) & 0x1F
        else:
            crc = (crc << 1) & 0x1F
    return (~crc) & 0x1F


def crc16_usb(data: bytes) -> int:
    """CRC16 over data payload, poly 0x8005, init 0xFFFF, residue check."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return (~crc) & 0xFFFF


def token_crc5_ok(addr: int, endp: int, crc5: int) -> bool:
    """Validate token CRC5 for ADDR[0:6] + ENDP[0:3] (11 bits)."""
    bits: list[int] = []
    for i in range(7):
        bits.append((addr >> i) & 1)
    for i in range(4):
        bits.append((endp >> i) & 1)
    return crc5_usb(bits) == (crc5 & 0x1F)
