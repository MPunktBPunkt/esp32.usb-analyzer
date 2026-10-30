# Firmware

ESP32-S3 analyzer node firmware lives here.

**Gate 0:** directory layout and interface notes only — no USB stack implementation yet.

## Layout

```text
firmware/
├── analyzer-node/
│   ├── host/          # USB OTG Host role implementation (Gate 1+)
│   ├── device/        # USB OTG Device role implementation (Gate 2+)
│   ├── capture/       # Ring buffer, event emit, integrity
│   ├── protocol/      # Control-plane NDJSON client
│   ├── transport/     # TCP tunnel (relay/direct)
│   └── diagnostics/   # Health, UART logs, panic markers
└── common/            # Shared types, HELLO builder, config
```

## Rules

- One binary; role assigned at runtime by Debian
- No hardcoded peer IPs
- No secrets in source (WiFiManager / NVS)
- Capture must not block USB path on TCP send
- Claim depth must match instrumentation (class hook vs App callback)

## Tooling (Gate 1)

PlatformIO + **pioarduino** Arduino-ESP32 **3.2.1** (required by EspUsbHost 2.x).  
Lab default board: YD-ESP32-S3 N8R2 class — see `docs/hardware/BOARD-YD-ESP32-S3.md`.

```bash
cd firmware/analyzer-node
~/.platformio/penv/bin/pio run -e analyzer-s3-host
```
