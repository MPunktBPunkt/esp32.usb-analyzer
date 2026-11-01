"""Generate synthetic SETUP + DATA0 + ACK NRZI sample for decoder tests."""

from __future__ import annotations

from .decoder import build_data, build_handshake, build_token, packet_bytes_to_nrzi_states
from .nrzi import J


def synth_get_descriptor_states() -> list[int]:
    # SETUP token addr=1 ep=0
    setup = build_token(0x2D, addr=1, endp=0)
    # 8-byte SETUP data: GET_DESCRIPTOR device
    data = bytes([0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00])
    data0 = build_data(0xC3, data)
    ack = build_handshake(0xD2)
    states: list[int] = []
    for pkt in (setup, data0, ack):
        states.extend(packet_bytes_to_nrzi_states(pkt, idle=J))
        # inter-packet idle
        states.extend([J] * 8)
    return states
