# Gate 1 status — ESP Host node

**Code status:** implemented + **firmware build SUCCESS** (`analyzer-s3-host`, Arduino-ESP32 3.2.1, EspUsbHost 2.10.1)  
**Lab MEASURED (partial):** both `.88`/`.89` OTA’d to 0.1.0; `.88` UART-refreshed with MAC-stable id  
**Claim depth:** USB Host controller enumeration (L1 descriptors) only

## Exit criteria

| Criterion | Status |
|-----------|--------|
| ESP-A role `host` starts EspUsbHost | code ready |
| Device VID/PID/config/interfaces logged | code ready |
| VBUS / connector notes in logs + docs | done |
| Optional HELLO to Debian discovery | code ready |
| Lab: ENUMERATED observed on real hardware | **UNKNOWN → NEXT TEST** |

## How to run (lab)

### Firmware

```bash
cd firmware/analyzer-node
pio run -e analyzer-s3-host
pio run -e analyzer-s3-host -t upload
pio device monitor -b 115200
```

- Console on **COM/UART**, not the OTG USB port.
- Plug DUT into **USB-OTG** port.
- Expect `usb.begin(): ok`, then after plug: `=== USB ENUMERATED (Gate 1) ===` with `vid:pid=...`.

### Controller (optional for USB prove)

```bash
python3 controller/discovery/server.py --port 9750 --auto-role host
```

Set controller host/port in the WiFi portal (`USB-Analyzer-Setup`) or NVS defaults in `ProjectConfig.h`.

## FACT / MEASURED / UNKNOWN

| Class | Statement |
|-------|-----------|
| FACT | S3 Host is Full-Speed only; HS-only devices fail |
| FACT | Many DevKits do not source VBUS |
| FACT | EspUsbHost 2.x needs Arduino-ESP32 ≥ 3.2.0 |
| MEASURED | `.88` → HELLO `usb-analyzer-339c` role=host, WiFi OK, `usb_devices=0` |
| MEASURED | `.89` OTA 0.1.0 online but still `usb-analyzer-0000` until COM UART reflash |
| UNKNOWN | FS DUT VID:PID enum on these boards (no stick plugged yet) |
| UNKNOWN | Observer effect / MITM (out of Gate 1) |
| REJECTED | “Analyzer works” / wire capture / proxy transparency |

## NEXT TEST

1. Plug FS keyboard/stick into **`.88` OTG** (not COM); watch discovery `usb_devices` / Serial ENUMERATED.
2. Swap COM UART to `.89` → `pio run -e analyzer-s3-host -t upload` (MAC → `usb-analyzer-c568`).
3. Then Gate 2 device role on `.89`.

## Epistemic one-liner

> Gate 1 proves **USB Host stack sees a device**, not that a USB analyzer exists.
