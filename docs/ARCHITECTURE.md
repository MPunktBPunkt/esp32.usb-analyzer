# Architecture

**Status:** Gate 0 design  
**Epistemic rule:** Do not promote HYPOTHESIS/UNKNOWN to FACT in this document.

## 1. System overview

Two ESP32-S3 analyzer nodes plus one Debian control plane form a programmable USB MITM / protocol analyzer lab.

```text
USB SIDE A ──► ESP-A (role=host) ════ TCP tunnel ════ ESP-B (role=device) ──► USB SIDE B
                      │                                      │
                      └──────────── Debian ──────────────────┘
                         discovery · session · capture collector · UI
```

### Planes

| Plane | Responsibility | Path |
|-------|----------------|------|
| **Control plane** | Discovery, role/session assignment, start/stop, health, config | ESP ↔ Debian |
| **USB data plane** | Forward USB transfers between host-side and device-side nodes | ESP-A ⇄ ESP-B (Mode A) or via Debian relay (Mode B) |
| **Capture plane** | Structured events + integrity metrics | ESP → Debian (batched); optional mirror of tunnel events |

Debian is **not** required to sit in the USB data path for every packet.

## 2. Operating modes

### Mode B — RELAY (Gates 1–3)

```text
ESP-A ═══════► Debian ═══════► ESP-B
                  │
               capture
```

- Simpler bring-up: one TCP peer (Debian) per ESP
- Higher latency; useful for protocol/debug
- Debian can drop/inspect freely (lab only)

### Mode A — DIRECT (target analyzer)

```text
ESP-A ═══════════ TCP ═══════════ ESP-B
   │                                │
   └──────────── Debian ────────────┘
              control + observe
```

- Lower latency; Debian outage need not tear the tunnel immediately
- Peer addresses come from Debian session assignment — ESPs do not discover each other
- Capture may still stream to Debian asynchronously

### Mode C — STORE_FORWARD (planned, not implemented)

```text
ESP-A  USB → PSRAM rings ══ TCP blocks ══ PSRAM rings → USB  ESP-B
         │                                              │
         └──────────── Debian control / observe ────────┘
```

USB and WiFi meet only via block handoff — not per-event TCP. Preferred serious experiment mode after Gate 4/5. Full design: [`STORE_FORWARD.md`](STORE_FORWARD.md).

Related lab modes on the same hardware: `LIVE`, `BUFFERED`, `STORE`, `REPLAY`, `BURST`, `FAULT`, `ANALYZE`.

## 3. Node roles

Roles are **assigned by Debian**, not permanently baked into firmware binaries.

| Role | USB OTG mode | Typical placement |
|------|--------------|-------------------|
| `host` | Host | Side A (talks to real USB device or host under test depending on setup) |
| `device` | Device | Side B (presents gadget to real USB host) |
| `unassigned` | Idle / diagnostics | Fresh boot before session |

A single firmware image should advertise capabilities (`usb-host`, `usb-device`, `capture`, `tcp`, `psram`) and accept runtime role assignment.

**Naming note:** In a classic MITM between a PC host and a USB stick:

- ESP facing the stick runs as **USB Host**
- ESP facing the PC runs as **USB Device**

Exact Side A / Side B wiring is session configuration, not a FACT about BMW NBT setups until documented in a session profile.

## 4. Discovery & session flow

```text
boot → WiFi DHCP → TCP connect Debian:control
     → HELLO (node_hello.v1)
     → Debian Node Registry
     → SESSION ASSIGN (id, role, peer, mode, capture_mode)
     → [Mode A] peer TCP connect
     → START
     → capture batches + health
     → STOP → integrity summary
```

Session IDs look like `20261030-USB-0001`. Every capture event carries `session_id`, `node_id`, `seq`, timestamps with `timestamp_domain`.

Details: [`PROTOCOL.md`](PROTOCOL.md).

## 5. Capture layers

| Layer | Content | Gate |
|-------|---------|------|
| L0 Wire / PHY | D+/D− samples, DATA0/1, ACK/NAK | Gate 10 research — **UNKNOWN** capability |
| L1 Controller | Reset, enum, setup/control/bulk, EP, status, STALL, timing | Gate 5 |
| L2 Protocol | MSC BOT CBW/CSW, SCSI CDB when visible via class hooks | Gate 6 |

**REJECTED:** equating TinyUSB App `onRead` callbacks with full BOT wire events.  
Use class-driver hooks (`bot_cbw` / `bot_csw`) where possible. See [`CAPTURE.md`](CAPTURE.md) and [`EVIDENCE.md`](EVIDENCE.md).

## 6. Buffering

USB realtime path must not block on WiFi TX:

```text
USB event → RAM/PSRAM ring → batch → TCP
```

Overflow counters are mandatory. Integrity fields per session: records, bytes, dropped, overflow, first/last seq, gaps, retransmissions, TCP errors.

## 7. Proxy maturity

| Stage | Behavior |
|-------|----------|
| Capture-only | Observe without forwarding (host or device alone) |
| Transparent forward | Proxy without mutation (Gate 4) |
| Store-and-forward | Dual PSRAM rings + block TCP + water-level ACK (planned; [`STORE_FORWARD.md`](STORE_FORWARD.md)) |
| Fault injection | delay / drop / duplicate / stall / reset / disconnect (Gate 9); prefer block-level once SF exists |
| Protocol mutation | CBW/CDB/length/timing edits — only after stable baseline |

## 8. Debian controller modules

| Module | Duty |
|--------|------|
| `discovery` | Accept HELLO, heartbeat |
| `node_registry` | Live node state |
| `sessions` | Assign roles, peers, modes |
| `capture` | Collect batches, integrity |
| `decoder` | L2 BOT/SCSI decode |
| `storage` | Session persistence / replay |
| `analysis` | Diff, timing, latency |
| `api` | REST/SSE for Web UI |

## 9. Web UI (lab)

Minimum surfaces (Gate 7+):

- Live Host↔Device timeline
- Packet inspector
- Statistics (USB + TCP + ring occupancy)
- D+/D− panel: **protocol state only** until Gate 10 experiments say otherwise

## 10. What ESP32.esp-hub contributes

**Stufe A (verbindlich):** Core vendored from `esp-hub-base` v1.10.5 into `firmware/analyzer-node/src/esphub/` — DiagLog/Recovery stack, HardwareInfo/CpuLoad, WebUi/OtaService, HubClient. See [`HUB-BASELINE.md`](HUB-BASELINE.md).

Planes stay split:

- **Hub** — FW distribute / project switch (`HubClient`)
- **Debian** — analyzer discovery/session/lab UI (`ControlClient`)
- **USB/TCP** — measurement data path (Host/Device nodes)

Not yet: full `AppCore` or USB-as-`BaseModule` (Stufe C/D).

## 11. Open architecture risks

| Risk | Status |
|------|--------|
| Dual-ESP MITM electrical/timing transparency | UNKNOWN — needs Gate 4 baseline |
| USB ↔ WiFi under Bulk load | HYPOTHESIS set in technology-audit; lab discrimination required |
| Clock sync between ESPs | UNKNOWN without calibration protocol |
| Simultaneous OTG + USB-Serial/JTAG on internal PHY | FACT: shared PHY — design UART/external PHY debug |
| VBUS powering Host-side ESP | FACT: many DevKits do not source VBUS |
