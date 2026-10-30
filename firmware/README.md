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

## Tooling (planned)

PlatformIO or Arduino-ESP32 — decision deferred to Gate 1 with board matrix (YD-ESP32-S3 N8R2 as lab default).
