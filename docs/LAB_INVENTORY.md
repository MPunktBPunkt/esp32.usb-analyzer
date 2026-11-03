# Lab inventory — usb-analyzer

Authorized overwrite targets for Gate testing (Proxmox PC / DebianCursor LXC).  
Stand: Analyzer FW **0.0.52** · siehe auch [`README.md`](../README.md) (Hardware-Abschnitt).

| IP | MAC | Board | Typical role | Notes |
|----|-----|-------|--------------|-------|
| `192.168.178.88` | `68:b6:b3:29:33:9c` | YD-ESP32-S3 N8R2 | **Host** (ESP-A) | 8 MiB + 2 MiB PSRAM; COM `/dev/ttyACM0` (CH343); OTG **ohne** Bus-5 V |
| `192.168.178.89` | `68:b6:b3:2b:c5:68` | YD-ESP32-S3 N8R2 | **Device** (ESP-B) | Analyzer gadget default; OTA-Swap zu hu-probe/pidrive für Fremd-DUT |

Controller / discovery host: `192.168.178.187` (this LXC) — Discovery **:9750**, Lab-UI **:8080**.  
esp-hub: `192.168.178.113:8093`.

**Overwrite authorized** for usb-analyzer Gate tests (hu-probe/pidrive temporarily on `.89` OK).

## USB on Proxmox

- Often both ESPs appear as **USB gadgets** on the PC Host (`04e8:6860` / `18d1:2d00` under hu-probe).
- LXC currently sees one UART: `/dev/ttyACM0` (CH343 COM).
- After Host FW: OTG port is for **DUT**, not for PC gadget mode. Serial = COM/UART.

## Flash

From hu-probe OTA while still on 0.3.x:

```bash
./tools/flash_lab_ota.sh 192.168.178.88
./tools/flash_lab_ota.sh 192.168.178.89
```

Analyzer FW (≥0.0.3) uses esp-hub Stufe A WebUi on port 80:

- `http://192.168.178.88/` — Status/OTA tabs (gzip)  
- `GET /api/status` · `GET /events` · `POST /ota-upload` · `/api/diag/*`  
- Hub register: `192.168.178.113:8093` (FW distribute)  
- OTA: `./tools/flash_lab_ota.sh 192.168.178.88` or Hub push  

First bring-up without HTTP: UART/`pio run -t upload` on COM.

## Restore hu-probe

Cable or Hub OTA with a hu-probe `.esp32s3.bin` (same 8 MiB partition family).
