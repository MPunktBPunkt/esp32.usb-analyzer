# analyzer-node

Single firmware image for ESP32-S3 analyzer nodes.

## Gate 1 — Host enumeration

Environment: `analyzer-s3-host`

```bash
pio run -e analyzer-s3-host
pio run -e analyzer-s3-host -t upload
pio device monitor -b 115200   # use COM/UART
```

On success you should see:

1. `usb.begin(): ok`
2. After plugging a FS device: `=== USB ENUMERATED (Gate 1) ===` with VID:PID

WiFi portal SSID: `USB-Analyzer-Setup`  
Controller defaults: see `include/ProjectConfig.h`

## Roles

Build default is **host** (`ANALYZER_DEFAULT_ROLE_HOST`).  
Debian may still send `assign_role` over the control TCP channel.

Device-role USB gadget code lands in Gate 2.

## Layout

| Path | Purpose |
|------|---------|
| `src/host/` | EspUsbHost bring-up |
| `src/protocol/` | NDJSON HELLO / role_ack |
| `src/diagnostics/` | WiFiManager bootstrap |
| `src/common/` | device id helpers |
| `../common/include` | shared headers (`Role.h`) |
