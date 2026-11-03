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
0.0.45: L0∖L1 **stack-gap** under wire stimulus (`lab_l0_stack_gap.py`, `L0_STACK_GAP.md`) — closest answer to „an TinyUSB vorbei“ until Path B PHY.  
0.0.46: **No-HW slice** — Gate 9 fault-inject/sense (`lab_fault_inject.py`); correlator `true_gap_n` / `handshake_expected_n`; soft Path-B campaign + bring-up dry-run (`lab_path_b_soft_campaign.py`, `lab_path_b_bringup_dryrun.py`).

## Next hardware

1. Procure/solder Path B Hi-Z tap (BOM) — replace GPIO lab / sim as *wire* source  
2. Optional: jumper Host GPIO10/11 → second ESP RX for dual-node sampler  
3. FS stick on Host + Device→PC for true observer MITM — method: `lab_observer_control_host.py`  
4. VBUS switch — `docs/hardware/VBUS_SWITCH_CHECKLIST.md`

## Post-0.0.50 software packages (tools)

| Package | Tool | Notes |
|---------|------|-------|
| Version matrix | `lab_version_matrix.py` | live archive |
| Power-cycle sticky | `lab_power_cycle_sticky.py` | needs hu-probe on `.89` + operator rail |
| Heap soak | `lab_heap_soak.py` | heapMax leak vs churn |
| Release gate | `release_gate_capability.py` | wraps capability audit |
| SF discrimination | `lab_sf_discrimination.py` | Mode-A gap; SF rings still not implemented |
| Control-host observer | `lab_observer_control_host.py` | checklist before MITM claim |
| msc_reads semantics | `LAB_MSC_READS_SEMANTICS.md` | hu-probe counter rule |

## No-HW packages (0.0.46)

| # | Package | Status |
|---|---------|--------|
| A | Gate 9 delay/fail/disconnect + REQUEST_SENSE observe | **MEASURED** (OTG loop) |
| B | Soft Path-B stack-gap campaign (synth/GPIO vs live L1) | **MEASURED** |
| C | Path-B bring-up dry-run + HW checklist | **MEASURED** (offline+live) |

**Übergabe nächste KI:** [`docs/reports/HANDOVER_0.0.46.md`](reports/HANDOVER_0.0.46.md) — Messinventar, Blindstellen B0–B9, Testmethoden, P0–P3.

### P0 test methods (0.0.47)

| Method | Tool / test | Status |
|--------|-------------|--------|
| Property fuzz L0 | `tests/host/test_usb_fs_l0_fuzz.py` | host unit |
| BCL random fuzz | `test_bcl_lab_frame.cpp` `test_fuzz_random_buffers` | host unit |
| Golden `.l0samp` regression | `tests/host/test_l0_golden.py` + fixtures | host unit |
| REQUEST_SENSE isolation | `lab_sense_isolation.py` | lab MEASURED |
| Hotplug/soak | `lab_hotplug_soak.py` | lab MEASURED |
| Gate9 fail_after / fail_mid | `lab_gate9_extended.py` | lab MEASURED |

### No-HW follow-ups (0.0.48)

| Method | Tool / evidence | Status |
|--------|-----------------|--------|
| Foreign DUT pidrive L3 (16 MiB FAT16 + MP3/ID3) | `LAB_PIDRIVE_FOREIGN_DUT_MEASURED` | MEASURED |
| Foreign DUT hu-probe AOA Lab A/B | `LAB_HUPROBE_AOA_FOREIGN_DUT_MEASURED` | MEASURED |
| Soak n=100 soft-reenum | `lab_hotplug_soak.py --cycles 100` | MEASURED (0 fail) |
| CLEAR_HALT vs BOT_RESET hooks | `lab_clear_halt_vs_bot.py` | MEASURED (`both_…`) |
| Physical VBUS unplug | — | still open |
| `aoa_phone_like` sticky boot | hu-probe NVS 0.3.35 | MEASURED (soft reboot; Power-Cycle still P0) |

### BMW-like Host stimulus (0.0.49 → 0.0.50)

| Method | Tool / evidence | Status |
|--------|-----------------|--------|
| Host `/api/lab/bmw-hu-stimulus` (+Sense/MaxLUN/Sync, `bmw_mount_deep`) | Host FW **0.0.50** | shipped |
| Cross-check vs pidrive L3 | `LAB_BMW_HU_STIMULUS_PIDRIVE_MEASURED` | MEASURED — DUT counters byte-exact |
| Cross-check vs hu-probe MSC | `LAB_BMW_HU_STIMULUS_HUPROBE_MEASURED` | MEASURED — visible; `read_count` semantics ≠ Host READ count |
| AOA plane negative (pre-fix) | earlier sticky-AOA run | MEASURED — zero MSC counters (correct) |
| hu-probe NVS sticky defect | `LAB_HUPROBE_USB_GADGET_NVS_STICKY` | MEASURED (pre-0.3.35) |
| hu-probe NVS sticky fix | `LAB_HUPROBE_USB_GADGET_NVS_FIX` | MEASURED — `msc`/`aoa_observe`/`aoa_phone_like` |
| Soft-unplug claim boundary | `LAB_SOFT_UNPLUG_CLAIM` | docs |
| Lab UI + proxy allowlist | Controller **0.0.41** | shipped |
| Handover | `docs/reports/2026-11-02-handover-0.0.50.md` | — |
