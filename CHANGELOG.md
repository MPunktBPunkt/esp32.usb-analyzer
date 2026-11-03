# Changelog

## Unreleased (post-0.0.1)

- FW 0.0.51: HID scenario — keyboard text inject + mouse move/click; Host mouse observe + `/api/lab/vbus-status`; ESP UI HID/VBUS card; `lab_hid_scenario.py`
- Lab packages post-audit: `lab_version_matrix`, `lab_power_cycle_sticky`, `lab_heap_soak`, `release_gate_capability`, `lab_sf_discrimination`, `lab_observer_control_host`; `msc_reads` semantics; VBUS-switch checklist
- Lab: `tools/lab_capability_audit.py` + Capability-Audit 0.0.50 (7/7 + Gate10 9/9); frische Baseline USB≈917 kB/s / TCP≈332 kB/s
- Controller 0.0.41: lab `/api/v1/lab/proxy` allowlist (GET status/config/capture/events/metrics; POST sync/stimulus/usb-cycle/gadget-reboot)
- Docs: Handover 0.0.50 + Cross-Repo-Bericht/GATE10 auf aktuellen Stand; `aoa_phone_like` sticky MEASURED
- 0.0.50: BMW stimulus +REQUEST_SENSE/MaxLUN/SYNCHRONIZE_CACHE + `bmw_mount_deep`; Lab UI actions (:8080) + controller lab proxy; soft-unplug claim doc; cross-repo sticky/telemetry packages
- 0.0.49: BMW-HU-like Host stimulus (`/api/lab/bmw-hu-stimulus`) + cross-check vs pidrive/hu-probe; hu-probe usb_gadget NVS sticky defect
- 0.0.48: Foreign DUT campaign (Analyzer-Host × pidrive L3 MP3/ID3 + hu-probe AOA Lab A/B); soak n=100; CLEAR_HALT vs BOT_RESET class-control hooks
- 0.0.47: P0 test methods — L0/BCL fuzz, golden `.l0samp` regression, REQUEST_SENSE isolation, hotplug/soak, Gate9 fail_after/fail_mid
- 0.0.46: Software-only while Path-B HW waits — Gate 9 fault-inject/sense/recovery, correlator gap classes, soft Path-B campaign + bring-up dry-run
- 0.0.45: L0∖L1 stack-gap lab (stimulus wire GT vs Device TinyUSB capture)
- FW 0.0.44: GPIO methods suite — calibrate, cross-link RX, matrix/gap/glitch/jitter, sync marker
- FW 0.0.43: Wire-stimulus series + Device mount/unmount counters; LS-rate attempt
- FW 0.0.42: Destructive OTG wire-stimulus (GPIO19/20 after Host end) + lab correlate
- FW 0.0.41: Path B GPIO lab (bit-bang free GPIOs + L0ST) + Gate10 perf campaign; packages 1–5 software/park
- FW 0.0.40: Gate 10A / E1.1 — GPIO19/20 peek-poll under live OTG bus (rate/jitter/destruct MEASURED)
- FW 0.0.39: Path B ingest+live L0 pipeline + OTG-loop-safe MSC proxy-source/pattern + observer proxy mode
- Lab controller: harden HTTP handler, `run_lab_controller.sh`, user-systemd unit — recovers dead `:8080` UI
- FW 0.0.38: Gate 10A–F scaffold — L0 feasibility API, offline FS decoder/correlator, observer-effect baseline tool, Path B PoC docs — **not** live D+/D− / HU
- FW 0.0.37: TCP sliding-window pipeline (`pipeline=1` on `/api/lab/integrity`) — MEASURED vs barrier windowed; **not** USB MITM
- FW 0.0.36: Bus Suspend/Resume lab (Host HPRT + post EP0; Device TinyUSB SUSPEND UNKNOWN on HPRT path) + EP0 control-neg series (`usb-ctrl-neg`) — **not** L0 PHY / SETUP MITM
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
