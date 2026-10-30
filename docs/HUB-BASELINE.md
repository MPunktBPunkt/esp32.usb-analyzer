# ESP32.esp-hub baseline (selective reuse)

**Source:** [MPunktBPunkt/ESP32.esp-hub](https://github.com/MPunktBPunkt/ESP32.esp-hub) (`esp-hub-base` ~v1.10.5)  
**Policy:** Inspiration and copy-by-need. **No git submodule / package dependency** required for this repo to build.

## Useful patterns

| Area | Hub capability | Analyzer use |
|------|----------------|--------------|
| WiFi | WiFiManager captive portal; no SSID/PSK in git | Same rule |
| Health | Heartbeat fields: chip, heap, PSRAM, RSSI | Map into `node_hello.v1` |
| Web UI | SSE status pages, gzip static UI | Lab UI patterns later |
| OTA | Hub push / web upload lifecycle | Optional after Gate 3 |
| Diagnostics | DiagLog, PanicTail, Checkpoint, Recovery | Node diagnostics module |
| HardwareInfo | Board/PSRAM detection | HELLO `hw` + capabilities |
| Modules | `BaseModule` / ModuleManager | Optional structure for host/device/capture |

## Do not copy blindly

| Hub assumption | Analyzer reality |
|----------------|------------------|
| Single node talking to ioBroker hub | Two nodes + Debian control plane |
| Heartbeat as primary protocol | Dedicated discovery/session/capture protocol |
| Generic IO dashboard | USB timeline / MITM lab UI |
| Classic ESP32 + S3 family breadth | **S3 USB-OTG required** for analyzer nodes |
| Optional USB flash via adapter hub | Analyzer USB is the device under test path |

## Suggested later extraction (not Gate 0)

1. Port `HardwareInfo` + PSRAM reporting into `firmware/common`
2. Reuse WiFiManager bootstrap
3. Adapt WebUi/SSE only after controller API exists
4. Keep GPL-3.0 attribution when copying substantial hub files (may force a project license review before merge)

## Standalone rule

CI, releases, and firmware builds for `esp32.usb-analyzer` must succeed without cloning ESP32.esp-hub.
