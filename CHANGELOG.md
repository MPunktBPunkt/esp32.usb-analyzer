# Changelog

## Unreleased (post-0.0.1)

- FW 0.0.35: Control-state enum outcome (`/api/lab/usb-enum-observe`, soft-reenum) + HID interrupt lab (Device keyboard inject / Host observe) — **not** SETUP MITM / L0 / HU
- FW 0.0.34: BCL coverage — Host unit tests (`tests/host/`), RX-side `bclValidateFrame` (not TX re-parse), lab series HANDSHAKE/DATA/`plhex`/header-only, `bclFindHeader` resync — **not** HU wire / parser-on-wire
- FW 0.0.33: Lab BCL 8-byte BE frame over AOA bulk (`/api/lab/aoa-bcl`, `lab_aoa_bcl_verify.py`) — Desk header FACT; **not** HU wire / Etch/SAS
- FW 0.0.32: AOA bulk echo MEASURED — TinyUSB `tud_vendor_*` FIFO path + Host continuous RX; `lab_aoa_handshake_verify.py --bulk` — **not** BCL/HU
- FW 0.0.31: AOA via USBVendor.onRequest (MSC+Vendor soft-composite); `/api/lab/aoa-bulk` scaffold — Host vendorOpen/OUT ok, Device RX still **UNKNOWN**/fail; handshake Lab A/B remains PASS — **not** BCL/HU wire
- FW 0.0.30: AOA capture decode (`protocol:aoa` GET_PROTOCOL/SEND_STRING/START) + Lab A/B in `lab_aoa_handshake_verify.py` + `docs/evidence/LAB_AOA_HANDSHAKE_MEASURED.md` — **not** HU/BCL/bulk
- FW 0.0.29: Lab AOA control handshake (51→52×6→53) — Device EP0 responder + Host `/api/lab/aoa-handshake`; optional soft-reenum `18D1:2D00`; `tools/lab_aoa_handshake_verify.py` — Desk FACT from technology-audit/`usb-aoa` + hu-probe C1b lab; **not** HU wire / BCL / accessory bulk
- FW 0.0.28: EP0 string-descriptor forward (`desc=string&index=`); dual-lane session timestamp display after sync — **not** hard µs lock / control MITM
- FW 0.0.27: Gate‑8 sync markers (`/api/lab/sync-marker`, controller `/api/v1/sync`, `lab_sync_offset.py`) + Gate‑4 EP0 GET_DESCRIPTOR forward (`kCmdCtrl*`, `/api/lab/usb-ctrl-forward`, `lab_usb_ctrl_loop_verify.py`) — **not** hard µs lock / control MITM
- FW 0.0.26: Gate‑6 TinyUSB class-hook CBW/CSW — vendored `msc_device.c` (0.18.0) + `class_hook_mscd_xfer` capture payload; Device drops App-inferred bot markers; `tools/lab_bot_class_hook_verify.py` — **not** wire PHY / MITM
- FW 0.0.25: Gate‑4 A4 MSC bulk proxy selftest — `usb-host-read` + `usb-proxy-selftest` byte-exact; proxyHits/Fails; proxy-ON never falls back to lab pattern; `tools/lab_usb_proxy_verify.py` — **not** transparent MITM
- FW 0.0.24: Gate‑4 gadget identity from DUT snapshot (VID/PID + MSC inquiry/capacity + soft re-enum); `lab_usb_proxy_arm.py` probes Host then verifies applied id — **not** USB control MITM
- FW 0.0.23: persistent MSC-proxy TCP session; DUT info snapshot forward; TCP window default 8 / max 64
- 0.0.22: Gate‑7 capture filters + stats; Host enum capture carries product/address
- FW 0.0.21: Gate‑4 MSC WRITE proxy (Device→Host DUT) + `tools/lab_usb_proxy_arm.py`
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
