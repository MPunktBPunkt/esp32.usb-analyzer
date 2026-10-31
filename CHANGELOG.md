# Changelog

## Unreleased (post-0.0.1)

- FW: Status-Tab RGB-LED Steuerung (`POST /api/led`) — Farben + Identify-Blink zum Unterscheiden der beiden ESPs (GPIO48 WS2812)
- FW 0.0.11: Analyzer-Tab Bridge-Perf Button + Throughput/Latenz-Charts; `lastIntegrity` in `/api/status`
- FW 0.0.12: OTG-loop MSC probe (`POST /api/lab/usb-host-probe`) — Host liest Device über USB-Kabel (Pattern + Rate); DUT in Status; `tools/lab_usb_loop_probe.py`
- FW 0.0.13: MSC probe multi-block `chunk=1..8`; baseline campaign tool
- FW 0.0.14: Device multi-LBA MSC pattern fill (fixes false pattern fails on chunk>1)
- MEASURED baseline: `docs/evidence/LAB_BASELINE_MEASURED.md` — USB ~934 kB/s / 0 loss; TCP ~139 kB/s
- FW 0.0.15: Gate‑4 slice — windowed TCP bulk (`window=`), MSC READ proxy Device→Host (`/api/lab/usb-proxy`)
- FW 0.0.16: Gate‑5 slice — `CaptureLog` + `GET /api/capture` + Analyzer Timeline (L1 lab, not wire)
- FW 0.0.17: Gate‑6 slice — MSC WRITE probe + readback; capture DECODE READ10/WRITE10 + inferred bot_cbw/csw
- FW 0.0.18: Capture JSON fix (`scsi_opcode` decimal); bot_cbw WRITE vs READ; CSW decode; gadget write-cache 64 LBA; READ bot markers
- FW 0.0.20: TinyUSB `tud_msc_*_complete_cb` capture markers (`class_complete_cb`); lab UI event inspector
- 0.0.19: Gate‑7 slice — Debian dual capture timeline (`/api/v1/capture/dual`) + lab UI Host/Device lanes

- Debian Slice A: `controller.run` HTTP `:8080` + SSE + lab Web UI
- Gate 2 scaffold: `analyzer-s3-device` MSC gadget (build OK; lab flash pending)
- FW 0.0.2: LocalWeb on ESP (`/` + `/api/status` + `/ota-upload`, esp-hub contract)
- LocalWeb UI: `web/index.html` gzip-PROGMEM (esp-hub tabs Status/OTA + SSE `/events`)
- FW 0.0.3: esp-hub Stufe A vendored (`src/esphub/`) — WebUi/OtaService/DiagLog/HubClient; LocalWeb removed
- FW 0.0.4: ESP Web-UI Tab **Analyzer** (RX/TX + chart placeholders until Gate 3+)
- FW 0.0.5: Gate 3 TCP integrity (`:9752`, `POST /api/lab/integrity`) — ESP↔ESP byte match; not USB MITM
- FW 0.0.6: Analyzer tab first/default in ESP Web-UI
- FW 0.0.7: DataPlane faster CRC + WiFi sleep off; `tools/lab_perf_sweep.py`
- Gate 4 status doc: needs DUT on Host OTG before USB proxy
- FW 0.0.8: Integrity returns RTT µs (min/avg/p50/p95/max); `tools/lab_perf_latency.py`
- FW 0.0.9: Analyzer tab shows USB OTG connected state (host DUT count / device mounted)

## 0.0.1 — 2026-10-30

Zwischenstand / first tagged release.

- Gate 0: architecture, evidence digest, schemas, gates
- Gate 1: ESP32-S3 Host bring-up (`EspUsbHost`), HELLO/discovery, lab `.88`/`.89`
- Debian API design (`docs/API.md`)
- Store-and-Forward parked as planned Mode C (`docs/STORE_FORWARD.md`)

Not claimed: MITM proxy, wire capture, STORE_FORWARD, fault injection.
