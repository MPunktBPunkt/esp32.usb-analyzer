"""Full-Speed USB L0 offline toolkit (Gate 10B–F).

Decode NRZI/bit-stuff → packets; correlate with L1 capture JSON.
Claim: offline/synth MEASURED — NOT live D+/D− until Path B samples exist.
"""

from .crc import crc5_usb, crc16_usb
from .decoder import decode_nrzi_bits, pid_name
from .schema import packet_dict

__all__ = [
    "crc5_usb",
    "crc16_usb",
    "decode_nrzi_bits",
    "pid_name",
    "packet_dict",
]
