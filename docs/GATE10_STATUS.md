# Gate 10 status — L0 / D+/D− research (passive first)

**Goal:** Passive Full-Speed USB wire capture that can reconstruct at least  
PID · Token · DATA · Handshake · Payload · CRC · order — **without** claiming the ESP32-S3 internal PHY is the sniffer.

**Not claimed until MEASURED:** oscilloscope UI, HS, transparent MITM, HU wire, L0≡L1 equality.

## Roadmap freeze (post-0.0.37)

| Do now | Park |
|--------|------|
| Gate **10A–10F** | More USB class slices (CDC/UVC/…) |
| Observer-effect baseline (Gate 4 A4 close-out) | High-Speed decision |
| Offline FS decoder + L0↔L1 correlator | Isochronous |
| Hardware path **B** (external tap/PHY) after A falsify | FPGA full analyzer (later Path C) |

L1/L2 TinyUSB stack remains the **reference channel**. L0 is an **independent** instrument.

## Phases

| Phase | Prove | Status |
|-------|-------|--------|
| **10A** | Feasibility: GPIO19/20 vs OTG PHY; DWC status regs (Level-2) | **MEASURED** — Path A falsified; see `LAB_GATE10A_FEASIBILITY.md` |
| **10B** | FS bit path: NRZI + bit-stuff (offline/synth) | **MEASURED** host unit |
| **10C** | Packet decoder: Token/Data/Handshake | **MEASURED** host unit |
| **10D** | CRC5/CRC16 + DATA0/1 | **MEASURED** host unit |
| **10E** | Compact packet records + µs field (schema) | **done** `l0_packet.v1` + `.l0samp` |
| **10F** | L0↔L1 correlation engine | **MEASURED** synth (`lab_l0_correlate.py`); live wire TBD |
| **10G** | Passive BMW/HU capture | **not started** |
| **11\*** | MITM observer / L0-A/B | baseline tool started; full 11 later |

## Architecture (required)

```text
EXISTING L1/L2          NEW L0 (passive)
USB DUT ──► ESP Host ──► WiFi ──► Debian
              ▲
D+/D− ── tap ─┴──► Sampler/PHY-B ──► Decoder ──► correlator ──► Timeline
```

L0 must **not** start as MITM. Passive truth first.

## Hardware paths (decision rule)

| Path | Idea | Gate |
|------|------|------|
| **A** | GPIO sample D+/D− on S3 while OTG owns pins | expect **falsify** / destructive — 10A |
| **B** | External receiver/PHY → digital states → ESP/DMA | **preferred PoC** after A |
| **C** | FPGA/CPLD bit recovery | later if B insufficient |

Requirement text (do not over-constrain):

> We need a passive FS USB wire-capture channel that can reconstruct PID, Token, DATA, Handshake, Payload, CRC, and order.

## Artifacts

| Artifact | Path |
|----------|------|
| 10A evidence | `docs/evidence/LAB_GATE10A_FEASIBILITY.md` |
| Observer baseline | `docs/evidence/LAB_OBSERVER_EFFECT_BASELINE.md` |
| Hardware PoC B | `docs/hardware/L0_CAPTURE_POC.md` |
| Packet schema | `docs/schemas/l0_packet.v1.md` |
| Decoder / correlator | `tools/usb_fs_l0/` |
| Lab tools | `lab_l0_feasibility.py`, `lab_observer_effect.py`, `lab_l0_correlate.py` |

## Claim discipline

- Stack “GET_DESCRIPTOR OK” ≠ Wire SETUP/DATA/ACK.
- Class-hook CBW ≠ DATA0+CRC on the bus.
- Lab AOA/BCL ≠ HU BCL-on-wire.
- No D+/D− panel that implies Level-3 without MEASURED packets.

## Next (after 0.0.39 software Path B pipe)

1. **Solder/procure Path B front-end** (BOM in `L0_CAPTURE_POC.md`) — replace `sampler_sim`
2. Live PHY samples → same ingest/pipeline (already MEASURED with sim)
3. True MITM observer with FS stick on Host + Device→PC (pattern-proxy is loop stand-in)
4. Keep Debian controller up: `systemctl --user status usb-analyzer-controller`
