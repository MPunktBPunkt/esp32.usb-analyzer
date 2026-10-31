# esp32.usb-analyzer

![Version](https://img.shields.io/badge/version-0.0.16-blue)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Gate](https://img.shields.io/badge/gate-5%20capture%20slice-yellow)](docs/GATE5_STATUS.md)

Experimental **USB Protocol Analyzer / USB MITM** lab: two ESP32-S3 nodes + Debian control plane.

> **v0.0.16** Zwischenstand — Host/Device FW, OTG-loop MSC probe, WiFi TCP bulk, MSC-proxy slice, L1 capture timeline.  
> **Nicht claimed:** transparent MITM, D+/D− Wire-Capture, USB-IF Compliance.

## Goal architecture

```text
                         USB SIDE A (DUT / Stick)
                             │
                             ▼
                 ┌─────────────────────┐
                 │     ESP32-S3 #1     │
                 │  USB OTG = HOST     │  .88 lab
                 │  Capture / Forward  │
                 └──────────┬──────────┘
                            │ WiFi / TCP :9752
              ┌─────────────┴─────────────┐
              │          Debian           │
              │  Control / Discovery      │  :9750
              │  Lab Web UI               │  :8080
              └─────────────┬─────────────┘
                            │
                 ┌──────────▼──────────┐
                 │     ESP32-S3 #2     │
                 │  USB OTG = DEVICE   │  .89 lab
                 │  Capture / Forward  │
                 └──────────┬──────────┘
                            │
                            ▼
                         USB SIDE B (PC / Host)
```

| Mode | Path | Status |
|------|------|--------|
| **OTG loop** | Host OTG ↔ Device OTG (Kabel) | MEASURED — MSC probe |
| **A — DIRECT** | ESP1 ══ TCP ══ ESP2 | Gate 3 bulk + Gate 4 MSC-proxy slice |
| **B — RELAY** | via Debian | discovery/control; USB data not relayed yet |

## What we can measure today (0.0.16)

Classification: **MEASURED** in lab · see [`docs/evidence/LAB_BASELINE_MEASURED.md`](docs/evidence/LAB_BASELINE_MEASURED.md)

| Measurement | How | Typical lab result | Claim limit |
|-------------|-----|--------------------|-------------|
| **USB OTG MSC throughput** Host→Device | `POST /api/lab/usb-host-probe?blocks=&chunk=` · `tools/lab_usb_loop_probe.py` | ~**650–934 kB/s** (chunk 1…8) | FS stack/BOT — not HS |
| **USB pattern integrity** | `lab_sector_v1` on gadget + host verify | **0** fail bytes (chunk≤8) | Lab pattern, not arbitrary DUT FS |
| **USB sector / transfer RTT** | Probe `rtt_us` min/avg/p50/p95/max | ~**0.73 ms**/sector; ~**4 ms**/8-block | Host stack timing |
| **DUT enumeration** | `/api/status` → `otg` + `dut` | VID/PID/serial/speed | L1 descriptors |
| **WiFi TCP integrity** both ways | `POST /api/lab/integrity?peer=&frames=&frame_bytes=` | 0 frame fails | Not USB MITM |
| **WiFi TCP bulk throughput** | same + `window=4…12` | ~**140 kB/s** (ack-each) → ~**260–360 kB/s** (windowed) | Protocol RTT-bound |
| **MSC proxy Host serve** | TCP `MscReq`→Host READ DUT→payload | CRC/pattern OK on loop DUT | Gate‑4 slice; MITM wiring separate |
| **L1/lab capture timeline** | `GET /api/capture` · Analyzer-Tab | attach/enum/msc_read/tcp_bulk/… | **Not** D+/D− packets |
| **RGB identify** | Status-Tab / `POST /api/led` | GPIO48 WS2812 | Board jumper may be needed |

### ESP Web UI (per node, port 80)

| Tab | Functions |
|-----|-----------|
| **Analyzer** | OTG state, DUT, USB-OTG Probe, Bridge-Perf charts, Capture Timeline, RX/TX counters |
| **Status** | HW/heap/PSRAM, hub, **RGB LED** colors + identify |
| **OTA** | Local `.bin` upload |

### Lab tools

```bash
./tools/flash_lab_ota.sh 192.168.178.88          # host env
ENV=analyzer-s3-device ./tools/flash_lab_ota.sh 192.168.178.89

./tools/lab_usb_loop_probe.py --blocks 512
./tools/lab_integrity_test.py --both-ways
./tools/lab_perf_sweep.py
./tools/lab_baseline_campaign.py                 # USB + TCP campaign → docs/evidence/
```

### Explicitly **not** measured / not claimed

- D+/D− / DATA0/1 / ACK/NAK wire capture (Gate 10 — UNKNOWN)
- Full BOT CBW/CSW class-hook decode (Gate 6 — next)
- Transparent MITM PC↔Stick with observer-effect baseline
- Synchronized µs clocks across both ESPs
- USB High-Speed

## Epistemics

Consult [technology-audit](https://github.com/MPunktBPunkt/technology-audit) and classify:

`FACT` · `MEASURED` · `INFERENCE` · `HYPOTHESIS` · `UNKNOWN` · `REJECTED`

Digest: [`docs/EVIDENCE.md`](docs/EVIDENCE.md).

## Repository layout

```text
esp32.usb-analyzer/
├── firmware/analyzer-node/   # Host + Device envs, esp-hub Web/OTA, CaptureLog
├── firmware/common/          # Shared headers (lab pattern, roles)
├── controller/               # Debian discovery / registry (:9750)
├── web/                      # Lab UI on Debian (:8080)
├── docs/                     # Architecture, gates, evidence/
└── tools/                    # OTA flash + lab measurement scripts
```

## Documents

| Doc | Purpose |
|-----|---------|
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Planes, modes |
| [`docs/GATES.md`](docs/GATES.md) | Gate 0–10 |
| [`docs/GATE3_STATUS.md`](docs/GATE3_STATUS.md) · [`GATE4`](docs/GATE4_STATUS.md) · [`GATE5`](docs/GATE5_STATUS.md) | Current gate notes |
| [`docs/CAPTURE.md`](docs/CAPTURE.md) | Capture layers / schema |
| [`docs/evidence/LAB_BASELINE_MEASURED.md`](docs/evidence/LAB_BASELINE_MEASURED.md) | Lab baseline numbers |
| [`docs/LAB_INVENTORY.md`](docs/LAB_INVENTORY.md) | `.88` / `.89` boards |
| [`docs/HUB-BASELINE.md`](docs/HUB-BASELINE.md) | esp-hub Stufe A |

## Quick start

```bash
cd firmware/analyzer-node
# Host (.88) — OTG = DUT; Serial = UART/COM
pio run -e analyzer-s3-host
# Device (.89)
pio run -e analyzer-s3-device

# OTA (boards already on WiFi)
../../tools/flash_lab_ota.sh 192.168.178.88
ENV=analyzer-s3-device ../../tools/flash_lab_ota.sh 192.168.178.89
```

- Node UI: `http://192.168.178.88/` · `http://192.168.178.89/`
- Lab UI: `http://192.168.178.187:8080/`
- Controller: `PYTHONPATH=. python3 -m controller.run`

### OTG-loop test (no PC gadget)

1. Flash Host + Device  
2. USB-C between both **OTG** ports  
3. Host `/api/status` → `dut.present`  
4. `tools/lab_usb_loop_probe.py` or Analyzer **USB-OTG Probe**

### MITM wiring (proxy slice)

Device OTG → PC · Host OTG → real stick · then  
`POST http://.89/api/lab/usb-proxy?enable=1&peer=192.168.178.88`  
(Observer-effect baseline still required before transparency claims.)

## Development strategy

1. Gate 1 Host enum — **done (lab)**  
2. Gate 2 Device gadget — **done**  
3. Gate 3 TCP integrity/bulk — **done**  
4. Gate 4 USB proxy — **slice** (MSC READ forward)  
5. Gate 5 Capture — **slice** (L1/lab timeline)  
6. Gate 6+ Decoder / dual timeline / wire research — **next**

## Hardware

- Two YD-ESP32-S3 (N8R2) · USB-OTG Full-Speed · UART/COM for debug  
- RGB LED GPIO48 (often needs RGB solder bridge)  
- VBUS: many DevKits do not source Host 5 V — measure / powered hub

## License

MIT — see [`LICENSE`](LICENSE).

Note: selective copies from GPL-licensed ESP32.esp-hub must preserve that license on the copied files.
