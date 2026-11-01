# esp32.usb-analyzer

![Version](https://img.shields.io/badge/version-0.0.46-blue)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Gate](https://img.shields.io/badge/gate-7%20timeline%20slice-yellow)](docs/GATE7_STATUS.md)

Experimental **USB Protocol Analyzer / USB MITM** lab: two ESP32-S3 nodes + Debian control plane.

> **v0.0.46** Zwischenstand — Gate 9 fault-inject/sense + soft Path-B campaign (no new HW); Path B soldered PHY still open.  
> **Nicht claimed:** transparent MITM, hard µs clock lock, D+/D− Wire-Capture, HU/BCL-on-Bulk, USB-IF Compliance.

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

## What we can measure today (0.0.39)

Classification: **MEASURED** in lab · see [`docs/evidence/LAB_BASELINE_MEASURED.md`](docs/evidence/LAB_BASELINE_MEASURED.md) · AOA: [`LAB_AOA_HANDSHAKE_MEASURED.md`](docs/evidence/LAB_AOA_HANDSHAKE_MEASURED.md)

| Measurement | How | Typical lab result | Claim limit |
|-------------|-----|--------------------|-------------|
| **USB OTG MSC READ** Host→Device | `POST /api/lab/usb-host-probe?dir=read&blocks=&chunk=` · `tools/lab_usb_loop_probe.py` | ~**650–934 kB/s** (chunk 1…8) | FS stack/BOT — not HS |
| **USB OTG MSC WRITE** Host→Device + readback | `POST /api/lab/usb-host-probe?dir=write&blocks=&chunk=` | ~**250–460 kB/s** (chunk 1…8), `pattern_ok` | Gadget RAM cache (64 LBA), not media |
| **USB pattern integrity** | `lab_sector_v1` on gadget + host verify | **0** fail bytes (chunk≤8, clean LBA) | Lab pattern / write marker |
| **USB sector / transfer RTT** | Probe `rtt_us` min/avg/p50/p95/max | ~**0.73 ms**/sector; ~**2.2 ms**/4-block | Host stack timing |
| **DUT enumeration** | `/api/status` → `otg` + `dut` | VID/PID/serial/speed | L1 descriptors |
| **WiFi TCP integrity** both ways | `POST /api/lab/integrity?peer=&frames=&frame_bytes=` | 0 frame fails | Not USB MITM |
| **WiFi TCP bulk throughput** | `window=` + optional `pipeline=1` | ~**140 kB/s** ack-each → ~**300 kB/s** windowed → ~**436 kB/s** pipeline (`4096×8`) | WiFi RTT; win≥16 cliffs |
| **MSC proxy Host serve** | TCP `MscReq`→Host READ DUT→payload | CRC/pattern OK on loop DUT | Gate‑4 slice; MITM wiring separate |
| **Gadget DUT identity** | DUT snapshot → Device VID/PID + MSC inquiry/capacity + soft re-enum | PC `lsusb` shows DUT-like id after arm | Snapshot apply — **not** control MITM |
| **MSC bulk proxy selftest** | Device proxy-read vs Host DUT raw (`lab_usb_proxy_verify.py`) | byte-exact CRC/hex match | WiFi MSC tunnel — **not** transparent MITM |
| **L1/lab capture timeline** | `GET /api/capture` · Analyzer-Tab · Debian dual lanes | attach/enum/msc_read/msc_write/bot_cbw/csw/tcp_bulk | **Not** D+/D− packets |
| **Dual-node lab UI** | `GET /api/v1/capture/dual` · `:8080` Host/Device lanes | live poll both ESPs | clocks local, not synced |
| **Inferred BOT/SCSI decode** | capture `decoded.command` (Host App path) | `source: class_api_inferred` | App fiction — Device uses class-hook |
| **Class-hook BOT CBW/CSW** | vendored TinyUSB `mscd_xfer_cb` / `send_csw` | `source: class_hook_mscd_xfer` + `payload_hex` | Class driver bytes — **not** D+/D− wire |
| **Cross-node sync offset** | `lab_sync_offset.py` / `POST /api/v1/sync` | median `offset_us` ± residual | Estimate — **not** hard µs lock |
| **EP0 GET_DESCRIPTOR forward** | Device→Host WiFi ctrl (`lab_usb_ctrl_loop_verify.py`) | match local gadget desc (OTG loop) | Lab echo — **not** control MITM |
| **AOA control handshake** | Host 51→52×6→53 vs Device responder; Lab A/B (`lab_aoa_handshake_verify.py`) | protocol≥1, 52×6+START, `18d1:2d00` ~0,55–0,6 s; Lab B STALL | Lab OTG — **not** HU/BCL |
| **AOA capture decode** | `decoded.protocol=aoa` on Host capture | GET_PROTOCOL / SEND_STRING / START | EP0 lab markers — **not** wire |
| **AOA vendor-bulk echo** | Host `/api/lab/aoa-bulk` · verify `--bulk` | byte-exact echo (TinyUSB FIFO path) | Lab Rohr MSC+Vendor — **not** BCL/HU |
| **Lab BCL on AOA bulk** | 8-byte BE + init; Host RX parse; `--series` / host units | HANDSHAKE/DATA/empty echo + unit negatives | Desk FACT framing — **not** HU wire |
| **USB enum outcome** | `/api/lab/usb-enum-observe` + optional soft-reenum | address≠0, config≠0, GET_CONFIGURATION match | Post-enum stack — **not** SETUP MITM / L0 |
| **HID interrupt** | Device `/api/lab/hid-inject` → Host `/api/lab/hid-observe` | keycode/report via EspUsbHost callback | Class IRQ — **not** L0 / HU |
| **Bus Suspend/Resume** | Host HPRT `/api/lab/usb-suspend-resume` | Device suspend/resume counts + post EP0 | Stack HPRT — **not** L0 PHY |
| **EP0 control-neg** | `/api/lab/usb-ctrl-neg` | positives + expected STALL/fail cases | Caller EP0 — **not** SETUP MITM |
| **Gate 10A feasibility** | `/api/lab/l0-feasibility` | HPRT Level-2; Path A sniffer **falsified** | **not** live L0 packets |
| **E1.1 GPIO sample** | `/api/lab/e11-gpio-sample` | Poll rate/jitter/edges + bus stability under MSC | **not** FS L0 sniffer |
| **Path B GPIO lab** | `/api/lab/path-b-gpio` | Bit-bang synth line-states on GPIO10/11 + optional L0ST | **not** live OTG PHY |
| **Wire stimulus** | `/api/lab/wire-stimulus` | Stop Host, drive GPIO19/20 patterns (destructive) | **not** parallel Host |
| **GPIO methods** | `/api/lab/gpio-calibrate` · `path-b-gpio-rx` | Calibrate, cross-link RX, matrix/gap/jitter | **not** PHY tap |
| **L0 offline decoder** | `tools/usb_fs_l0` + host tests | SETUP/DATA/ACK + CRC synth | **not** live D+/D− |
| **L0↔L1 correlate** | `lab_l0_correlate.py --synth` | synthetic matches | live wire TBD (Path B) |
| **Observer-effect baseline** | `lab_observer_effect.py` | direct vs proxy/loop Δ | **not** transparency FACT |
| **RGB identify** | Status-Tab / `POST /api/led` | GPIO48 WS2812 | Board jumper may be needed |

### ESP Web UI (per node, port 80)

| Tab | Functions |
|-----|-----------|
| **Analyzer** | OTG state, DUT, USB-OTG Probe (READ/WRITE), Bridge-Perf charts, Capture Timeline, RX/TX counters |
| **Status** | HW/heap/PSRAM, hub, **RGB LED** colors + identify |
| **OTA** | Local `.bin` upload |

### Lab tools

```bash
./tools/run_lab_controller.sh restart             # Debian UI :8080 + discovery :9750
./tools/install_lab_controller_service.sh        # user systemd autostart for controller
./tools/flash_lab_ota.sh 192.168.178.88          # host env
ENV=analyzer-s3-device ./tools/flash_lab_ota.sh 192.168.178.89

./tools/lab_usb_loop_probe.py --blocks 512
./tools/lab_integrity_test.py --both-ways
./tools/lab_usb_proxy_arm.py
./tools/lab_usb_proxy_verify.py --arm              # MSC bulk proxy byte-exact
./tools/lab_bot_class_hook_verify.py               # Gate-6 real CBW/CSW (OTG loop)
./tools/lab_sync_offset.py                         # Gate-8 offset estimate (WiFi)
./tools/lab_usb_ctrl_loop_verify.py                # Gate-4 EP0 forward (OTG loop)
./tools/lab_aoa_handshake_verify.py                # Lab AOA 51→52→53 + Lab A/B (OTG loop)
./tools/lab_aoa_handshake_verify.py --bulk         # + vendor bulk echo after re-enum
./tools/lab_aoa_bcl_verify.py --series             # BCL HANDSHAKE/DATA/empty + host units
./tools/lab_aoa_bcl_verify.py --unit-only          # BclLabFrame.h host tests (no hardware)
./tools/lab_usb_enum_observe.py --reenum           # address/config + soft-reenum cycle
./tools/lab_hid_interrupt_verify.py                # HID interrupt Device→Host
./tools/lab_usb_suspend_resume.py                  # HPRT suspend/resume + Device events
./tools/lab_usb_ctrl_neg.py                        # EP0 positive + negative series
./tools/lab_l0_feasibility.py                      # Gate 10A HPRT + Path A falsify
./tools/lab_e11_gpio_sample.py                     # E1.1 GPIO19/20 poll rate/jitter/destruct
./tools/lab_path_b_gpio.py --decode                # Path B GPIO bit-bang + L0ST ingest
./tools/lab_wire_stimulus.py                      # destructive OTG D+/D− pattern generator
./tools/lab_gpio_methods_campaign.py              # GPIO calibrate/matrix/cross-link campaign
./tools/lab_l0_stack_gap.py                       # L0 vs TinyUSB gap under wire stimulus
./tools/lab_gate10_perf_campaign.py                # full dual-ESP capability/perf sweep
./tools/lab_observer_effect.py                     # observer-effect baseline (loop|proxy)
./tools/l0_path_b_ingest.py --decode &           # Path B TCP :9760 ingest
./tools/l0_path_b_sampler_sim.py                 # sampler stand-in (no PHY yet)
./tools/lab_l0_live_pipeline.py --source sim --trigger-ep0
./tools/lab_observer_effect.py --mode proxy      # direct USB vs WiFi MSC-proxy
./tools/lab_l0_correlate.py --synth                # L0↔L1 correlator (offline)
./tests/host/run_usb_fs_l0_tests.sh                # FS L0 offline decoder units
./tools/lab_perf_sweep.py --quick              # windowed vs pipeline TCP
./tools/lab_baseline_campaign.py                 # USB + TCP campaign → docs/evidence/
```

### Explicitly **not** measured / not claimed

- Live D+/D− wire capture on bus (Gate 10 Path B — open; offline decoder MEASURED)
- D+/D− / DATA0/1 / ACK/NAK **live** wire capture (Gate 10 — UNKNOWN until Path B)
- Transparent MITM PC↔Stick with observer-effect baseline
- Hard synchronized µs clocks across both ESPs (offset estimate only)
- USB High-Speed
- HU Connected / BCL-on-Bulk (AOA lab bulk ≠ BCL framing)

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
| [`docs/evidence/LAB_BASELINE_MEASURED.md`](docs/evidence/LAB_BASELINE_MEASURED.md) | Lab baseline numbers (MSC/TCP) |
| [`docs/evidence/LAB_AOA_HANDSHAKE_MEASURED.md`](docs/evidence/LAB_AOA_HANDSHAKE_MEASURED.md) | Lab AOA 51/52/53 MEASURED |
| [`docs/LAB_INVENTORY.md`](docs/LAB_INVENTORY.md) | `.88` / `.89` boards |
| [`docs/HUB-BASELINE.md`](docs/HUB-BASELINE.md) | esp-hub Stufe A |

## Quick start (lab)

1. Flash Host `.88` / Device `.89` (OTA scripts above).
2. OTG↔OTG cable between the two ESPs.
3. Open `http://192.168.178.88/` → Analyzer → USB-OTG Probe.
4. Capture: `GET /api/capture` (L1 lab events).

## License

MIT — see [`LICENSE`](LICENSE).
