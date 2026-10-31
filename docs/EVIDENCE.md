# Evidence digest (Gate 0)

**Authority:** [technology-audit](https://github.com/MPunktBPunkt/technology-audit)  
**Extracted:** 2026-10-30  
**Purpose:** Prevent unchecked assumptions from other projects entering this repo.

Classification used everywhere:

`FACT` · `MEASURED` · `INFERENCE` · `HYPOTHESIS` · `UNKNOWN` · `REJECTED`

External Espressif documentation is cited separately as **DOC** until pinned as an audit evidence item.

---

## 1. ESP32-S3 USB Host / Device

| Status | Claim | Source |
|--------|-------|--------|
| FACT | S2/S3 have USB Host/OTG Full-Speed; Classic ESP32 without OTG Host out of scope | `technologies/esp-usb-host` |
| FACT | S2/S3 Host is FS-only | `EVID-USBH-2026-10-12-003` |
| FACT | Device MSC path = TinyUSB / Arduino USBMSC / esp_tinyusb | `technologies/usb-msc` |
| FACT | Host class support includes HID/CDC/MSC/… | `EVID-USBH-2026-10-12-002` |
| FACT | Many DevKits (e.g. S3-DevKitC-1 OTG) do not source VBUS | `EVID-USBH-2026-10-12-004` |
| REJECTED | usb-msc sheet ⇒ Host can “be the stick” | role confusion |
| UNKNOWN | Dual-ESP Host+Device MITM validated as architecture | no dedicated audit |

## 2. PHY / debug sharing

| Status | Claim | Source |
|--------|-------|--------|
| FACT | Internal USB PHY shared between USB-OTG and USB-Serial/JTAG; switch via eFuse `USB_PHY_SEL` or external PHY | `EVID-USBWIFI-2026-10-14-006`, coex sheet |
| FACT | USBOTG-4289: some batches lack OTG download eFuse; workaround USB-Serial/JTAG | `EVID-S3HW-2026-10-18-005` |
| REJECTED | USBOTG-4289 explains MSC Bulk field failures | claim boundary |
| DOC | ESP32-S3 internal PHY fixed pins: **GPIO20 = D+**, **GPIO19 = D−** | Espressif USB PHY / USB Device docs |
| UNKNOWN in audit corpus | GPIO19/20 pad map (zero hits in technology-audit tree at extract time) | must be pinned as audit evidence before treating as project FACT |
| DOC | External PHY enables simultaneous OTG + Serial/JTAG | Espressif USB PHY overview |

## 3. BOT / MSC observation limits

| Status | Claim | Source |
|--------|-------|--------|
| FACT | CBW 31 B / CSW 13 B; signatures USBC / USBS | `EVID-USBBOT-INST-2026-10-14-001` |
| FACT | Class hooks: `mscd_xfer_cb` → bot_cbw; `send_csw` → residue | instrument audit |
| FACT | `return 0` / BUSY keeps same READ10 open; no CSW until complete | usb-msc sheet |
| REJECTED | App `onRead` = full bot_cbw event | `EVID-USBBOT-INST-…-010` |
| REJECTED | Clear-Halt cures NEED_RESET | `EVID-USBCTRL-2026-10-14-004` |
| REJECTED | SET_ADDRESS = mounted | mount at SET_CONFIGURATION |
| REJECTED | Host-eBPF = physical DATA0/1 layer | instrument claim boundary |
| FACT | Physical packet layer out of scope of BOT observation audits | `projects/usb-bot-observation/SCOPE.md` |
| UNKNOWN | Logger/proxy observer effect / transparency | method B4 |
| MEASURED | hu-probe offline device-stack captures (CBW/`bot_data`) — **not wire** | `EVID-HUPROBE-2026-10-21-*` |

## 4. USB ↔ WiFi coexistence

| Status | Claim | Source |
|--------|-------|--------|
| FACT | Historical `esp_wifi_start` could kill S3 USB Device; PHY fixes exist | `EVID-USBWIFI-2026-10-14-001/002` |
| FACT | `ESP_PHY_ENABLE_USB` / `phy_bbpll_en_usb` workaround | `004/005` |
| FACT | Arduino 2.0.11 `usbd` prio 24 > WiFi 23 | `010` |
| FACT | esp-faq coexistence = WiFi↔BT **not** USB | `013` |
| HYPOTHESIS | Bulk/SOF starves WiFi; PHY under load; power; RF | coex H-register |
| REJECTED | “USB Bulk kills WiFi” as desk FACT | claim boundary |
| MEASURED | PSRAM improves heap; does **not** heal USB↔WiFi death | `EVID-HUPROBE-2026-10-21-015` |
| UNKNOWN | Power vs Starve vs RF winner under sustained HU reads | method open |

## 5. PSRAM / buffering

| Status | Claim | Source |
|--------|-------|--------|
| REJECTED | Board PSRAM = automatically used | silicon sheet |
| REJECTED | 16 MB Octal PSRAM officially supported | `EVID-S3HW-009` |
| REJECTED | On-the-fly MSC needs full disk in PSRAM | usb-msc |
| MEASURED | hu-probe N8R2: QSPI PSRAM + 1 MiB offline capture journal | HUPROBE-001/002/003 |
| FACT (signal) | Prefer DMA buffers in internal RAM (quirks contrast) | S3HW transfer notes |

## 6. Throughput / timing

| Status | Claim | Source |
|--------|-------|--------|
| INFERENCE | USB FS MSC read ~0.5–1.0 MB/s; FS ceiling ~1.2 MB/s | usb-msc author tables |
| REJECTED | Average ≫ 1 MB/s on ESP MSC as FACT | usb-msc |
| UNKNOWN | NBT CDB lengths / Sense / Reset / inactivity as A-level FACT | host-specific |
| FACT | Without VBUS sense, unplug often skips umount; replug re-enums | `EVID-USBCTRL-007/008` |

## 7. Implications for this project

### GO (design / implement with evidence)

1. Capture at **L1/L2 class layer**, not L0 wire.
2. Configurable host/device roles on separate ESPs (FS OTG).
3. Ring buffer + batch TCP; integrity counters.
4. Coex hygiene: verify `ESP_PHY_ENABLE_USB`; do not assume BT-coex docs apply.
5. UART or external-PHY debug path; do not assume dual use of internal PHY.
6. VBUS design for Host-side node.

### Experiment required

| Experiment | Gate |
|------------|------|
| Host node enumerates a FS device | 1 |
| Device node enumerates to a PC host | 2 |
| Sequenced TCP events ESP⇄ESP (relay OK) | 3 |
| Transparent proxy baseline + observer-effect test | 4 |
| Session integrity under load | 5 |
| BOT/SCSI decode vs class hooks | 6 |
| USB↔WiFi discrimination under Bulk | parallel |
| D+/D− sampling feasibility | 10 |

### Must NOT claim

- Logic-analyzer D+/D− capture works on ESP32-S3
- Dual-ESP MITM is electrically/timing transparent
- Cross-ESP µs timestamps are synchronized by default
- App callbacks are wire-equivalent
- Fault injection / protocol mutation results without Gate 4 baseline

## 8. Primary corpus paths

```text
technologies/{usb-msc,esp-usb-host,esp32-s3-silicon,esp32-s3-usb-wifi-coex}/SHEET.md
methods/{usb-bot-command-observation,usb-wifi-coex-discrimination,plug-power-reset-isolation}.md
docs/reports/2026-10-14-{esp32-s3-usb-wifi-coexistence,usb-bot-instrument-sources,usb-control-stall-recovery}.md
docs/reports/2026-10-21-hu-probe-psram-board-free.md
projects/{usb-bot-observation,esp32-pidrive,hu-probe,esp32-msc-gadget-refs}/
```

When this project produces new MEASURED results, add them back into technology-audit — do not leave knowledge only in this repo.

## Lab baseline (usb-analyzer, 2026-10-31)

| Status | Claim | Source |
|--------|-------|--------|
| MEASURED | OTG Host↔Device MSC READ peak **~934 kB/s**, pattern fail **0** (chunk=8, FS) | `docs/evidence/LAB_BASELINE_MEASURED.md` |
| MEASURED | Single-sector MSC RTT p50 **~0.73 ms** | same |
| MEASURED | WiFi TCP ESP↔ESP peak **~139 kB/s**; small frames RTT ~250 ms | same |
| REJECTED | Multi-chunk pattern fails on 0.0.12 Device = USB data loss | gadget multi-LBA fill bug; fixed 0.0.14 |
