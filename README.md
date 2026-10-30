# esp32.usb-analyzer

![Version](https://img.shields.io/badge/version-0.0.1-blue)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Gate](https://img.shields.io/badge/gate-0%20architecture-orange)](docs/GATES.md)

Experimental **USB Protocol Analyzer / USB MITM** built from two ESP32-S3 nodes and a Debian control plane.

> Gate 0 status: **Architecture + Evidence + Interfaces + Test Plan**.  
> This is not a working analyzer yet. No claim of USB wire capture, transparent proxy, or fault injection.

## Goal architecture

```text
                         USB SIDE A
                             │
                             ▼
                 ┌─────────────────────┐
                 │     ESP32-S3 #1     │
                 │  USB OTG = HOST     │
                 │  Capture / Forward  │
                 └──────────┬──────────┘
                            │ WiFi / TCP
              ┌─────────────┴─────────────┐
              │          Debian           │
              │  Control / Discovery      │
              │  Capture Collector        │
              │  Web UI / Analysis        │
              └─────────────┬─────────────┘
                            │ WiFi / TCP
                 ┌──────────▼──────────┐
                 │     ESP32-S3 #2     │
                 │  USB OTG = DEVICE   │
                 │  Capture / Forward  │
                 └──────────┬──────────┘
                            │
                            ▼
                         USB SIDE B
```

**Critical design rule:** Debian is primarily the **control plane**. The long-term USB data path is a direct ESP1 ⇄ ESP2 TCP tunnel.

| Mode | Path | Intended use |
|------|------|--------------|
| **B — RELAY** | ESP1 → Debian → ESP2 | Gates 1–3 (simpler bring-up) |
| **A — DIRECT** | ESP1 ═══ TCP ═══ ESP2 (+ Debian observes) | Later analyzer / low-latency proxy |

## Epistemics

Before every substantial implementation decision, consult
[technology-audit](https://github.com/MPunktBPunkt/technology-audit) and classify claims as:

`FACT` · `MEASURED` · `INFERENCE` · `HYPOTHESIS` · `UNKNOWN` · `REJECTED`

Project evidence digest: [`docs/EVIDENCE.md`](docs/EVIDENCE.md).

### Must not claim yet

- D+/D− waveform / Logic-Analyzer-style wire capture on ESP32-S3
- Transparent dual-ESP MITM without observer-effect baseline
- App MSC callbacks ≡ full USB BOT wire events
- USB Bulk kills WiFi as FACT
- Synchronized µs timestamps across two ESPs without calibration

## Repository layout

```text
esp32.usb-analyzer/
├── firmware/          # ESP32-S3 analyzer nodes (host/device roles configurable)
├── controller/        # Debian control plane (discovery, sessions, capture, API)
├── web/               # Lab UI (timeline, packets, statistics, D+/D− research)
├── docs/              # Architecture, protocol, capture, evidence, gates
└── tools/             # Lab helpers (later)
```

## Documents (Gate 0)

| Doc | Purpose |
|-----|---------|
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | System design, planes, modes |
| [`docs/PROTOCOL.md`](docs/PROTOCOL.md) | Node hello, discovery, session control |
| [`docs/CAPTURE.md`](docs/CAPTURE.md) | Capture layers, event schema, buffering |
| [`docs/EVIDENCE.md`](docs/EVIDENCE.md) | technology-audit extraction |
| [`docs/HARDWARE-RISKS.md`](docs/HARDWARE-RISKS.md) | PHY sharing, VBUS, debug paths |
| [`docs/EXPERIMENTS.md`](docs/EXPERIMENTS.md) | D+/D− and research tracks |
| [`docs/GATES.md`](docs/GATES.md) | Gate 0–10 plan with exit criteria |
| [`docs/HUB-BASELINE.md`](docs/HUB-BASELINE.md) | What we reuse from ESP32.esp-hub |

Schemas: [`docs/schemas/`](docs/schemas/).

## Baseline references

- Firmware/UI patterns: [ESP32.esp-hub](https://github.com/MPunktBPunkt/ESP32.esp-hub) — inspiration only, **no hard dependency**
- Evidence authority: [technology-audit](https://github.com/MPunktBPunkt/technology-audit)
- Related field work: hu-probe / PiDrive MSC device stacks (device-layer capture ≠ wire)

## Development strategy

Work only through gates. Next implementable gate after this commit:

1. **Gate 1** — ESP Host node detects a USB device  
2. **Gate 2** — ESP Device node enumerates as a simple gadget  
3. **Gate 3** — Sequenced TCP events between nodes (Relay mode OK)

See [`docs/GATES.md`](docs/GATES.md).

## Hardware assumptions (provisional)

- Two ESP32-S3 boards with native USB-OTG Full-Speed
- Debian host on the same WiFi/LAN
- Debug via UART (or external PHY + USB-Serial/JTAG) — do **not** assume simultaneous internal OTG + Serial/JTAG on one PHY

## License

MIT — see [`LICENSE`](LICENSE).

Note: selective copies from GPL-licensed ESP32.esp-hub must preserve that license on the copied files and may require adjusting this project's license before such merges.
