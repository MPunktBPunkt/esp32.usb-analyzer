# esp32.usb-analyzer

![Version](https://img.shields.io/badge/version-0.0.21-blue)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Gate](https://img.shields.io/badge/gate-7%20timeline%20slice-yellow)](docs/GATE7_STATUS.md)

Experimental **USB Protocol Analyzer / USB MITM** lab: two ESP32-S3 nodes + Debian control plane.

> **v0.0.21** Zwischenstand — Host/Device FW, OTG-loop MSC READ+WRITE, WiFi TCP bulk, MSC-proxy slice, L1 capture + inferred BOT/SCSI decode.  
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
| **OTG loop** | Host OTG ↔ Device OTG (Kabel) | MEASURED — MSC READ + WRITE |
| **A — DIRECT** | ESP1 ══ TCP ══ ESP2 | Gate 3 bulk + Gate 4 MSC-proxy slice |
| **B — RELAY** | via Debian | discovery/control; USB data not relayed yet |

## What we can measure today (0.0.18)

Classification: **MEASURED** in lab · see [`docs/evidence/LAB_BASELINE_MEASURED.md`](docs/evidence/LAB_BASELINE_MEASURED.md)

| Measurement | How | Typical lab result | Claim limit |
|-------------|-----|--------------------|-------------|
| **USB OTG MSC READ** Host→Device | `POST /api/lab/usb-host-probe?dir=read&blocks=&chunk=` · `tools/lab_usb_loop_probe.py` | ~**650–934 kB/s** (chunk 1…8) | FS stack/BOT — not HS |
| **USB OTG MSC WRITE** Host→Device + readback | `POST /api/lab/usb-host-probe?dir=write&blocks=&chunk=` | ~**250–460 kB/s** (chunk 1…8), `pattern_ok` | Gadget RAM cache (64 LBA), not media |
| **USB pattern integrity** | `lab_sector_v1` on gadget + host verify | **0** fail bytes (chunk≤8, clean LBA) | Lab pattern / write marker |
| **USB sector / transfer RTT** | Probe `rtt_us` min/avg/p50/p95/max | ~**0.73 ms**/sector; ~**2.2 ms**/4-block | Host stack timing |
| **DUT enumeration** | `/api/status` → `otg` + `dut` | VID/PID/serial/speed | L1 descriptors |
| **WiFi TCP integrity** both ways | `POST /api/lab/integrity?peer=&frames=&frame_bytes=` | 0 frame fails | Not USB MITM |
| **WiFi TCP bulk throughput** | same + `window=4…12` | ~**140 kB/s** (ack-each) → ~**260–360 kB/s** (windowed) | Protocol RTT-bound |
| **MSC proxy Host serve** | TCP `MscReq`→Host READ DUT→payload | CRC/pattern OK on loop DUT | Gate‑4 slice; MITM wiring separate |
| **L1/lab capture timeline** | `GET /api/capture` · Analyzer-Tab · Debian dual lanes | attach/enum/msc_read/msc_write/bot_cbw/csw/tcp_bulk | **Not** D+/D− packets |
| **Dual-node lab UI** | `GET /api/v1/capture/dual` · `:8080` Host/Device lanes | live poll both ESPs | clocks local, not synced |
| **Inferred BOT/SCSI decode** | capture `decoded.command` READ_10 / WRITE_10 / CSW | `source: class_api_inferred` | Not wire CBW bytes |
| **RGB identify** | Status-Tab / `POST /api/led` | GPIO48 WS2812 | Board jumper may be needed |

### ESP Web UI (per node, port 80)

| Tab | Functions |
|-----|-----------|
| **Analyzer** | OTG state, DUT, USB-OTG Probe (READ/WRITE), Bridge-Perf charts, Capture Timeline, RX/TX counters |
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
- Real TinyUSB CBW/CSW byte capture (Gate 6 — next; today = class-API inferred)
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
| [`docs/GATE3_STATUS.md`](docs/GATE3_STATUS.md) · [`GATE4`](docs/GATE4_STATUS.md) · [`GATE5`](docs/GATE5_STATUS.md) · [`GATE6`](docs/GATE6_STATUS.md) · [`GATE7`](docs/GATE7_STATUS.md) | Current gate notes |
| [`docs/CAPTURE.md`](docs/CAPTURE.md) | Capture layers / schema |
| [`docs/evidence/LAB_BASELINE_MEASURED.md`](docs/evidence/LAB_BASELINE_MEASURED.md) | Lab baseline numbers |
| [`docs/LAB_INVENTORY.md`](docs/LAB_INVENTORY.md) | `.88` / `.89` boards |
| [`docs/HUB-BASELINE.md`](docs/HUB-BASELINE.md) | esp-hub Stufe A |

## Quick start (lab)

1. Flash Host `.88` / Device `.89` (OTA scripts above).
2. OTG↔OTG cable between the two ESPs.
3. Open `http://192.168.178.88/` → Analyzer → USB-OTG Probe.
4. Capture: `GET /api/capture` (L1 lab events).

## License

MIT — see [`LICENSE`](LICENSE).
