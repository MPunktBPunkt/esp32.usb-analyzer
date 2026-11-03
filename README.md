# esp32.usb-analyzer

![Version](https://img.shields.io/badge/version-0.0.52-blue)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Gate](https://img.shields.io/badge/gate-10%20soft%20%2B%20HID%2Fstimulus-yellow)](docs/GATE10_STATUS.md)

Experimental **USB Protocol Analyzer / USB MITM** lab: two ESP32-S3 nodes + Debian control plane.

> **v0.0.52** — Soft-Lab Capability Audit 7/7 · HID kbd/mouse scenario · BMW P-MOUNT inkl. MODE_SENSE6/PREVENT · Host `boardSourcesVbus=false`.  
> **Nicht claimed:** transparent MITM, hard µs clock lock, passives Live-D+/D− (Path-B PHY offen), HU/NBT-Wire-FACT, USB-IF, High-Speed.

Technology-audit: [`esp32-usb-analyzer-2026-0.0.50`](https://github.com/MPunktBPunkt/technology-audit) (closed) · Upstream Pin folgt diesem Repo.

---

## Lab hardware (was wir verwenden)

| Rolle | Gerät | IP / Anbindung | Details |
|-------|-------|----------------|---------|
| **ESP-A Host** | YD-ESP32-S3 **N8R2** | `192.168.178.88` · MAC `68:b6:b3:29:33:9c` | 8 MiB Flash · 2 MiB Quad-PSRAM · OTG GPIO19/20 · RGB GPIO48 · UART/COM CH343 |
| **ESP-B Device** | YD-ESP32-S3 **N8R2** | `192.168.178.89` · MAC `68:b6:b3:2b:c5:68` | gleiche Board-Klasse; Gadget MSC+Vendor+HID(kbd+mouse) |
| **OTG-Kabel** | USB-C OTG Host↔Device | physisch | Lab-Loop; beide ESPs **self-powered** (eigene Versorgung) |
| **Debian Control** | LXC / Proxmox PC | `192.168.178.187` | Discovery `:9750` · Lab-UI `:8080` · Controller ≥0.0.41 |
| **esp-hub** | Hub-Host | `192.168.178.113:8093` | Register / FW-Distribute (optional) |
| **Fremd-DUT (wechselnd)** | hu-probe / pidrive auf `.89` | OTA-Swap | Visibility-/Stimulus-Cross-Checks — nicht dauerhaft Analyzer |

**Board-Fakten**

- Native USB-OTG Full-Speed only (kein HS).
- OTG-Connector speist **kein** Bus-5 V (`/api/lab/vbus-status` → `boardSourcesVbus=false`). Bus-powered HID braucht self-powered Hub.
- Serial-Monitor nur über **COM/UART**, nicht über den OTG-Port (shared PHY).
- Details: [`docs/hardware/BOARD-YD-ESP32-S3.md`](docs/hardware/BOARD-YD-ESP32-S3.md) · Inventar: [`docs/LAB_INVENTORY.md`](docs/LAB_INVENTORY.md).

```text
PC/LXC UART ── COM (CH343) ── ESP Host .88
DUT / Device-ESP ── USB-C OTG ── ESP Host .88 OTG
ESP Device .89 ── eigene PSU + WiFi ── Debian :8080/:9750
```

---

## Goal architecture

```text
                         USB SIDE A (DUT / Stick / Device-ESP)
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
                         USB SIDE B (PC / Host) — optional MITM wiring
```

| Mode | Path | Status |
|------|------|--------|
| **OTG loop** | Host OTG ↔ Device OTG | **MEASURED** MSC/TCP/HID/Stimulus |
| **A — DIRECT** | ESP1 ══ TCP ══ ESP2 | Gate 3–4 integrity + MSC-proxy |
| **B — RELAY** | via Debian | discovery/control; USB-Daten nicht als Relay-Target |
| **Path B soft** | GPIO10/11 / sim / ingest `:9760` | Decoder-Pipeline MEASURED — **kein** gelötetes PHY |

---

## What we can measure today (0.0.52)

Baseline: [`docs/evidence/LAB_BASELINE_MEASURED.md`](docs/evidence/LAB_BASELINE_MEASURED.md) · Capability: [`docs/reports/2026-11-02-capability-audit-0.0.50.md`](docs/reports/2026-11-02-capability-audit-0.0.50.md)

| Measurement | How | Typical lab result | Claim limit |
|-------------|-----|--------------------|-------------|
| **USB OTG MSC READ** | `/api/lab/usb-host-probe` · `lab_usb_loop_probe.py` | peak **~917 kB/s**, pattern fail **0** | FS stack — not HS |
| **USB OTG MSC WRITE** + readback | `dir=write` | ~250–460 kB/s | Gadget RAM cache |
| **USB RTT** | Probe `rtt_us` | ~0.8 ms/sector · ~4.2 ms/8-sector | Host stack |
| **WiFi TCP integrity** | `/api/lab/integrity` · pipeline | peak **~332 kB/s** (campaign); pipeline sweet ~**436 kB/s** | ≪ USB; not MITM |
| **SF discrimination** | `lab_sf_discrimination.py` | USB≫TCP → SF warranted | Rings **not** implemented |
| **Enum outcome** | `/api/lab/usb-enum-observe` | addr/cfg/GET_DESC | not SETUP MITM |
| **Class-hook BOT CBW/CSW** | `lab_bot_class_hook_verify.py` | `class_hook_mscd_xfer` | not L0 wire |
| **EP0 forward / ctrl-neg** | `lab_usb_ctrl_loop_verify` · `lab_usb_ctrl_neg` | match / 8/8 neg | not control MITM |
| **AOA handshake + bulk + BCL** | `lab_aoa_*` | Lab A/B + echo + BCL frames | not HU wire |
| **HID keyboard + mouse** | `hid-inject` / `hid-mouse` / `hid-observe` · `lab_hid_scenario.py` | „Hello World“ + move/L/R | class IRQ — not L0 |
| **BMW-HU stimulus** | `/api/lab/bmw-hu-stimulus` · `lab_bmw_hu_stimulus.py` | INQUIRY+CAP×3+**MS6×6**+**PREVENT×7**+TUR32+P-MREAD | not NBT FACT |
| **Stimulus↔Feld diff** | `lab_stimulus_feld_diff.py` | P-MOUNT/P-MREAD counts green | not timing identity |
| **Fremd-DUT** | pidrive / hu-probe on `.89` | visibility MEASURED | not HU field |
| **Gate 9 fault/sense** | `lab_fault_inject` · `lab_sense_isolation` · `lab_gate9_extended` | OTG loop MEASURED | not L0 |
| **Hotplug / heap soak** | `lab_hotplug_soak` · `lab_heap_soak` | 1000× soft-reenum stable | not VBUS unplug |
| **Observer effect** | `lab_observer_effect.py` | loop proxy Δ | not PC↔Stick MITM |
| **Path A / E1.1** | `l0-feasibility` · `e11-gpio-sample` | Path A **REJECTED** | — |
| **Path B GPIO / soft / dry-run** | `path-b-gpio` · soft campaign · bringup dryrun | self-loop / synth | not soldered PHY |
| **Wire stimulus / stack-gap** | `wire-stimulus` · `lab_l0_stack_gap` | destructive GPIO19/20 | reboot restores Host |
| **VBUS status** | `/api/lab/vbus-status` | `boardSourcesVbus=false` | HPRT bit ≠ meter 5 V |
| **hu-probe soft sticky** | `lab_power_cycle_sticky.py --soft-only` | msc/aoa_observe/aoa_phone_like | Netzteil-Rail still operator |
| **Capability / release gate** | `lab_capability_audit.py` · `release_gate_capability.py` | **7/7** (+ Gate10 embed) | not Path-B/HU |
| **L0 offline decoder/correlate** | `tools/usb_fs_l0` · host tests | synth SETUP/DATA/ACK | live wire TBD |

### ESP Web UI (per node, `:80`)

| Tab | Functions |
|-----|-----------|
| **Analyzer** | OTG/DUT · USB READ/WRITE Probe · **VBUS/HPRT** · **HID Lab** (Type text / Mouse move·L/R bzw. Observe) · Capture Timeline · TCP Bridge-Perf · Integrity |
| **Status** | HW/heap/PSRAM · Hub · RGB LED colors + identify |
| **OTA** | Local `.bin` upload |

Debian Lab-UI `:8080` — dual lanes, sync, allowlisted lab proxy (Controller ≥0.0.41).

---

## Lab HTTP APIs (Firmware)

Host (`.88`) und Device (`.89`) — Rolle bestimmt, welche Routes sinnvoll sind.

| API | Rolle | Zweck |
|-----|-------|-------|
| `GET /api/status` | beide | OTG, DUT/gadget, hid, vbus, capture counters |
| `POST /api/lab/usb-host-probe` | Host | MSC READ/WRITE probe |
| `POST /api/lab/integrity` | Host | WiFi TCP frames (+ `pipeline=1`) |
| `POST /api/lab/usb-proxy` · `usb-proxy-selftest` · `proxy-source` | Device/Host | MSC proxy arm / selftest |
| `POST /api/lab/usb-ctrl-forward` · `usb-ctrl-neg` | Host/Device | EP0 lab |
| `POST /api/lab/usb-enum-observe` · `soft-reenum` | Host/Device | Enum outcome / soft D+ cycle |
| `POST /api/lab/hid-inject` · `hid-mouse` | Device | Keyboard text/key · mouse move/click |
| `POST /api/lab/hid-observe` | Host | Wait key/mouse report (`kind=`) |
| `POST /api/lab/bmw-hu-stimulus` | Host | P-MOUNT/P-MREAD(+deep) Stimulus |
| `POST /api/lab/aoa-handshake` · `aoa-bulk` · `aoa-bcl` · `aoa-respond` | Host/Device | AOA lab |
| `POST /api/lab/usb-suspend-resume` | Host | HPRT suspend/resume |
| `GET/POST /api/lab/vbus-status` | Host | HPRT power bits + board claim |
| `GET/POST /api/lab/l0-feasibility` · `e11-gpio-sample` | Host | Gate 10A |
| `GET/POST /api/lab/path-b-gpio` · `path-b-gpio-rx` · `gpio-calibrate` | Host | Path B soft |
| `POST /api/lab/wire-stimulus` | Host | Destructive D+/D− (confirm-gated) |
| `POST /api/lab/fault-inject` · `fault-observe` | Device/Host | Gate 9 |
| `POST /api/lab/sync-marker` | beide | Sync offset lab |
| `GET /api/capture` | beide | L1 timeline |
| `POST /api/led` | beide | RGB |

Schemas: [`docs/API.md`](docs/API.md) · Protocol: [`docs/PROTOCOL.md`](docs/PROTOCOL.md).

---

## Test methods & tools

### Ops / flash / release

```bash
./tools/run_lab_controller.sh restart
./tools/install_lab_controller_service.sh
./tools/flash_lab_ota.sh 192.168.178.88                    # Host
ENV=analyzer-s3-device ./tools/flash_lab_ota.sh 192.168.178.89
./tools/lab_version_matrix.py --peers ../hu-probe,../pidrive
./tools/lab_capability_audit.py                            # 7/7 (+ Gate10)
./tools/release_gate_capability.py                         # FW-Tag only if green
```

### USB / TCP baseline

```bash
./tools/lab_usb_loop_probe.py --blocks 512
./tools/lab_baseline_campaign.py
./tools/lab_integrity_test.py --both-ways
./tools/lab_perf_sweep.py --quick
./tools/lab_sf_discrimination.py                           # Mode-A vs USB gap
```

### Proxy / BOT / enum / AOA

```bash
./tools/lab_usb_proxy_arm.py
./tools/lab_usb_proxy_verify.py --arm
./tools/lab_bot_class_hook_verify.py
./tools/lab_sync_offset.py
./tools/lab_usb_ctrl_loop_verify.py
./tools/lab_usb_ctrl_neg.py
./tools/lab_usb_enum_observe.py --reenum
./tools/lab_aoa_handshake_verify.py                        # + --bulk
./tools/lab_aoa_bcl_verify.py --series                     # --unit-only ohne HW
```

### HID / stimulus / Fremd-DUT hygiene

```bash
./tools/lab_hid_interrupt_verify.py
./tools/lab_hid_scenario.py                                # Hello World + mouse
./tools/lab_bmw_hu_stimulus.py --dut huprobe --profile bmw_mount
./tools/lab_stimulus_feld_diff.py --profile bmw_mount      # vs P-MOUNT/P-MREAD
./tools/lab_power_cycle_sticky.py --soft-only              # hu-probe on .89
# ./tools/lab_power_cycle_sticky.py --wait-power-s 180     # + Operator Netzteil
```

### Gate 9 / soak / observer

```bash
./tools/lab_fault_inject.py
./tools/lab_sense_isolation.py
./tools/lab_gate9_extended.py
./tools/lab_clear_halt_vs_bot.py
./tools/lab_hotplug_soak.py --cycles 100
./tools/lab_heap_soak.py --cycles 1000
./tools/lab_observer_effect.py --mode proxy
./tools/lab_observer_control_host.py                       # PC↔Stick checklist
./tools/lab_usb_suspend_resume.py
```

### L0 / Path B / GPIO

```bash
./tools/lab_l0_feasibility.py
./tools/lab_e11_gpio_sample.py
./tools/lab_path_b_gpio.py --decode
./tools/lab_path_b_soft_campaign.py
./tools/lab_path_b_bringup_dryrun.py
./tools/lab_wire_stimulus.py                               # confirm-gated destructive
./tools/lab_gpio_methods_campaign.py
./tools/lab_l0_stack_gap.py
./tools/lab_gate10_perf_campaign.py
./tools/l0_path_b_ingest.py --decode &
./tools/l0_path_b_sampler_sim.py
./tools/lab_l0_live_pipeline.py --source sim --trigger-ep0
./tools/lab_l0_correlate.py --synth
./tests/host/run_usb_fs_l0_tests.sh
```

Evidence landet unter [`docs/evidence/`](docs/evidence/). Claim-Disziplin: jede Evidence mit **Not claimed**-Zeile.

---

## Explicitly **not** measured / not claimed

- Passives Live-D+/D− / gelötetes Path-B-Frontend  
- Transparent MITM PC↔Stick (OTG-Loop ≠ Transparenz)  
- Hard µs-Clock-Lock (nur Offset-Schätzung)  
- Soft-USB / Soft-Reboot ≡ VBUS-Unplug / Netzteil-Cycle  
- USB High-Speed · USB-IF Compliance  
- BMW NBT Wire-FACT (Lab-Stimulus ≠ Feldbeweis)  
- STORE_FORWARD PSRAM-Rings (Diskriminierung MEASURED; SF-0…5 offen)

---

## Epistemics

Consult [technology-audit](https://github.com/MPunktBPunkt/technology-audit) and classify:

`FACT` · `MEASURED` · `INFERENCE` · `HYPOTHESIS` · `UNKNOWN` · `REJECTED`

Digest: [`docs/EVIDENCE.md`](docs/EVIDENCE.md).

---

## Repository layout

```text
esp32.usb-analyzer/
├── firmware/analyzer-node/   # Host + Device envs, esp-hub Web/OTA, CaptureLog
├── firmware/common/          # Shared headers (lab pattern, roles, BCL)
├── controller/               # Debian discovery / registry / lab proxy (:9750/:8080)
├── web/                      # Lab UI assets (Debian)
├── docs/                     # Architecture, gates, hardware/, evidence/, reports/
├── tools/                    # OTA + lab measurement scripts
└── tests/host/               # Offline L0/BCL unit tests
```

## Documents

| Doc | Purpose |
|-----|---------|
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Planes, modes |
| [`docs/GATES.md`](docs/GATES.md) · [`GATE10_STATUS.md`](docs/GATE10_STATUS.md) | Gate map + L0 status |
| [`docs/CAPTURE.md`](docs/CAPTURE.md) · [`API.md`](docs/API.md) · [`PROTOCOL.md`](docs/PROTOCOL.md) | Capture / API / framing |
| [`docs/LAB_INVENTORY.md`](docs/LAB_INVENTORY.md) | `.88` / `.89` / controller |
| [`docs/hardware/BOARD-YD-ESP32-S3.md`](docs/hardware/BOARD-YD-ESP32-S3.md) | Board + VBUS |
| [`docs/hardware/VBUS_SWITCH_CHECKLIST.md`](docs/hardware/VBUS_SWITCH_CHECKLIST.md) | Phys. Unplug HW |
| [`docs/hardware/L0_CAPTURE_POC.md`](docs/hardware/L0_CAPTURE_POC.md) | Path-B BOM |
| [`docs/STORE_FORWARD.md`](docs/STORE_FORWARD.md) | SF-0…5 contract |
| [`docs/evidence/`](docs/evidence/) | MEASURED JSON/MD |
| [`docs/reports/2026-11-02-capability-audit-0.0.50.md`](docs/reports/2026-11-02-capability-audit-0.0.50.md) | Audit Ist-Stand |
| [`docs/reports/2026-11-02-handover-0.0.50.md`](docs/reports/2026-11-02-handover-0.0.50.md) | Blindstellen / Prio |
| [`docs/reports/2026-11-03-post-audit-packages.md`](docs/reports/2026-11-03-post-audit-packages.md) | Post-audit packages |

## Quick start (lab)

1. Flash Host `.88` / Device `.89` (OTA scripts above) — gleiche Analyzer-FW-Familie für Capability.
2. USB-C OTG Host↔Device; beide Boards mit eigener Versorgung.
3. `./tools/run_lab_controller.sh restart` → `http://192.168.178.187:8080/` und/oder `http://192.168.178.88/`.
4. Analyzer-Tab: USB Probe · HID Lab · VBUS Status.
5. Vor FW-Tag: `python3 tools/release_gate_capability.py`.

## License

MIT — see [`LICENSE`](LICENSE).
