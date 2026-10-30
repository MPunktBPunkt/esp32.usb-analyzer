# Gate 1 status — ESP Host node

**Code status:** implemented + **firmware build SUCCESS** (`analyzer-s3-host`, Arduino-ESP32 3.2.1, EspUsbHost 2.10.1)  
**Lab MEASURED:** pending — flash + plug FS device required  
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
| MEASURED | *(fill after lab)* board, device VID:PID, speed, enum latency |
| UNKNOWN | Observer effect / MITM (out of Gate 1) |
| REJECTED | “Analyzer works” / wire capture / proxy transparency |

## NEXT TEST

1. Flash YD-ESP32-S3 N8R2 (or equivalent).
2. Enumerate a known FS keyboard or flash drive.
3. Paste serial excerpt into this file under MEASURED.
4. Then start Gate 2 (device node).

## Epistemic one-liner

> Gate 1 proves **USB Host stack sees a device**, not that a USB analyzer exists.
