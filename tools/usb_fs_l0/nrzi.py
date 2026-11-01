"""NRZI + bit-stuffing for USB Full-Speed (lab/offline)."""

from __future__ import annotations

# Line states for optional sample files
SE0, J, K, SE1 = 0, 1, 2, 3


def bit_stuff(bits: list[int]) -> list[int]:
    out: list[int] = []
    ones = 0
    for b in bits:
        out.append(b)
        if b == 1:
            ones += 1
            if ones == 6:
                out.append(0)  # stuff bit
                ones = 0
        else:
            ones = 0
    return out


def bit_unstuff(bits: list[int]) -> list[int]:
    out: list[int] = []
    ones = 0
    i = 0
    while i < len(bits):
        b = bits[i]
        out.append(b)
        if b == 1:
            ones += 1
            if ones == 6:
                i += 1  # skip stuff 0 if present
                if i < len(bits) and bits[i] != 0:
                    # malformed; keep going
                    pass
                ones = 0
        else:
            ones = 0
        i += 1
    return out


def bytes_to_bits_lsb(data: bytes) -> list[int]:
    bits: list[int] = []
    for byte in data:
        for i in range(8):
            bits.append((byte >> i) & 1)
    return bits


def bits_to_bytes_lsb(bits: list[int]) -> bytes:
    out = bytearray()
    for i in range(0, len(bits) - 7, 8):
        v = 0
        for j in range(8):
            v |= (bits[i + j] & 1) << j
        out.append(v)
    return bytes(out)


def nrzi_encode(bits: list[int], start_state: int = J) -> list[int]:
    """Map data bits → line states. 0=transition, 1=no transition. Idle=J."""
    state = start_state
    states: list[int] = []
    for b in bits:
        if b == 0:
            state = K if state == J else J
        states.append(state)
    return states


def nrzi_decode(states: list[int]) -> list[int]:
    """Line states → data bits (assumes only J/K; SE0 ends packet)."""
    bits: list[int] = []
    prev = None
    for s in states:
        if s in (SE0, SE1):
            break
        if prev is None:
            # first state after SYNC handling done elsewhere
            prev = s
            bits.append(1)  # conventionally; caller usually strips SYNC specially
            continue
        bits.append(1 if s == prev else 0)
        prev = s
    return bits


def nrzi_decode_from_idle(states: list[int], idle: int = J) -> list[int]:
    bits: list[int] = []
    prev = idle
    for s in states:
        if s == SE0:
            break
        if s not in (J, K):
            break
        bits.append(1 if s == prev else 0)
        prev = s
    return bits
