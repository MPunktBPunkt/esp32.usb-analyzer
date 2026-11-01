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
| **10A** | Feasibility: GPIO19/20 vs OTG PHY; DWC status regs (Level-2) | **MEASURED** — Path A falsified; E1.1 rate/jitter/destruct: `LAB_GATE10A_E11_GPIO_SAMPLE.md` |
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
| E1.1 GPIO sample | `docs/evidence/LAB_GATE10A_E11_GPIO_SAMPLE.md` |
| Observer baseline | `docs/evidence/LAB_OBSERVER_EFFECT_BASELINE.md` |
| Hardware PoC B | `docs/hardware/L0_CAPTURE_POC.md` |
| Packet schema | `docs/schemas/l0_packet.v1.md` |
| Decoder / correlator | `tools/usb_fs_l0/` |
| Lab tools | `lab_l0_feasibility.py`, `lab_e11_gpio_sample.py`, `lab_observer_effect.py`, `lab_l0_correlate.py` |

## Claim discipline

- Stack “GET_DESCRIPTOR OK” ≠ Wire SETUP/DATA/ACK.
- Class-hook CBW ≠ DATA0+CRC on the bus.
- Lab AOA/BCL ≠ HU BCL-on-wire.
- No D+/D− panel that implies Level-3 without MEASURED packets.

## Packages after 0.0.40 (status in 0.0.41)

| # | Package | Status |
|---|---------|--------|
| 1 | Path B front-end | **Lab substitute MEASURED** — GPIO10/11 bit-bang + L0ST (`path_b_gpio_lab`); soldered Hi-Z tap still open |
| 2 | Live L0↔L1 on real samples | **MEASURED** via GPIO→ingest→pipeline (+ sim path) |
| 3 | True MITM observer | **Loop proxy MEASURED**; FS-stick↔PC wiring still open |
| 4 | Sampler FW/client | **Shipped** — ESP `path-b-gpio` + `sampler_sim` + ingest protocol |
| 5 | 10G HU / Path C | **Parked** — see below |

### Parked (5)

- **10G** BMW/HU passive capture — needs Path B PHY on vehicle bus + session profile  
- **Path C** FPGA/CPLD — only if Path B sampler rate/jitter insufficient  
- CDC/UVC class slices / HS decision — still parked

## Wire stimulus vs Path B GPIO lab

| Mode | Pins | Host stack | Purpose |
|------|------|------------|---------|
| Path B GPIO lab | GPIO10/11 | **stays up** | Non-destructive decoder/L0ST tests |
| **Wire stimulus** (0.0.42) | GPIO19/20 | **stopped** (`g_usb.end`) | Controlled D+/D− patterns toward Device; reboot to restore |

Tool: `lab_wire_stimulus.py` · confirm=`wire_stimulus_destructive` · claim `wire_stimulus_lab`.  
0.0.43: series API (`patterns=`), Device `mountCount`/`unmountCount`/`lastBusEvent`, LS-rate attempt MEASURED.
0.0.44: GPIO methods suite — see `docs/hardware/GPIO_LAB_METHODS.md` + `LAB_GPIO_METHODS_MEASURED.md`.

## Next hardware

1. Procure/solder Path B Hi-Z tap (BOM) — replace GPIO lab / sim as *wire* source  
2. Optional: jumper Host GPIO10/11 → second ESP RX for dual-node sampler  
3. FS stick on Host + Device→PC for true observer MITM
