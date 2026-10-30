# Control & transport protocol

**Status:** Gate 0 interface draft (v0)  
**Transport:** TCP over WiFi/LAN  
**Encoding:** newline-delimited JSON (NDJSON) for control; binary framing reserved for high-rate capture later

Default Debian control port (provisional): `9750`.  
Capture ingest port (provisional): `9751`.  
Peer USB-tunnel port (Mode A, provisional): `9752`.

Ports are configuration, not FACT.

## 1. Message envelope

Every control message:

```json
{
  "v": 1,
  "msg": "hello",
  "ts_us": 123456789,
  "node_id": "usb-analyzer-a1b2",
  "session_id": null,
  "body": {}
}
```

| Field | Meaning |
|-------|---------|
| `v` | Protocol version |
| `msg` | Message type |
| `ts_us` | Sender monotonic µs (`esp_timer_get_time` class) |
| `node_id` | Stable device id |
| `session_id` | Null until assigned |
| `body` | Type-specific payload |

## 2. Node HELLO (`node_hello.v1`)

Sent on control connect and periodically as heartbeat.

```json
{
  "device_id": "usb-analyzer-a1b2",
  "mac": "aa:bb:cc:dd:ee:ff",
  "ip": "192.168.1.101",
  "fw": "0.1.0",
  "git_sha": "deadbeef",
  "hw": {
    "chip": "ESP32-S3",
    "rev": "v0.2",
    "board": "YD-ESP32-S3-N8R2"
  },
  "usb": {
    "controller": "otg-fs",
    "phy": "internal",
    "role_capability": ["host", "device"]
  },
  "capabilities": ["usb-host", "usb-device", "capture", "tcp", "psram"],
  "role": "unassigned",
  "uptime_s": 12,
  "free_heap": 180000,
  "free_psram": 1500000,
  "psram_size": 2097152,
  "wifi_rssi": -62,
  "tcp_state": "control_connected"
}
```

**Rules**

- No static peer IPs in firmware
- Role in HELLO is current runtime role, not a permanent identity
- Missing PSRAM → `free_psram: 0`, omit false capability claims

Schema: [`schemas/node_hello.v1.schema.json`](schemas/node_hello.v1.schema.json)

## 3. Control messages (Debian → ESP)

| `msg` | Purpose |
|-------|---------|
| `assign_role` | Set `host` / `device` / `unassigned` |
| `session_start` | Bind session id, mode A/B, peer, capture mode |
| `session_stop` | End session; request integrity summary |
| `peer_info` | Mode A: peer IP/port + auth token |
| `set_capture_mode` | `metadata-only` / `protocol` / `payload` / `full` |
| `ping` | Liveness |
| `config` | Non-USB knobs (batch size, heartbeat period) |

### `session_start` body (example)

```json
{
  "session_id": "20261030-USB-0001",
  "role": "host",
  "mode": "relay",
  "peer": {
    "node_id": "usb-analyzer-c3d4",
    "ip": "192.168.1.102",
    "port": 9752
  },
  "capture_mode": "protocol",
  "tunnel_token": "opaque-session-token"
}
```

For Mode B (`relay`), `peer` may point at Debian itself.

## 4. Control messages (ESP → Debian)

| `msg` | Purpose |
|-------|---------|
| `hello` | Registration / heartbeat |
| `role_ack` | Role applied |
| `session_ack` | Session bound |
| `health` | RSSI, heap, ring occupancy, USB state |
| `integrity` | End-of-session counters |
| `error` | Structured failure |

## 5. USB tunnel framing (data plane)

**Gate 0 definition only** — implementation starts Gate 3/4.

Logical record:

```text
magic  u32  = 0x55534254  ('USBT')
ver    u16  = 1
flags  u16
seq    u32
len    u32
type   u16   # setup / data / status / reset / stall / ...
ep     u8
dir    u8
status u16
ts_us  u64   # local monotonic
payload[len]
```

Capture events may reuse the same logical fields as JSON batches for Relay mode.

## 6. Timestamp domains

| `timestamp_domain` | Meaning |
|--------------------|---------|
| `local` | Node monotonic clock only |
| `session` | Offset-corrected using Debian-mediated sync (future) |
| `wall` | Wall clock if NTP available — optional, never assumed equal across nodes |

**Do not claim** that two `local` timestamps are comparable without documenting a sync method and residual error.

## 7. Capture ingest

ESP → Debian batches:

```json
{
  "v": 1,
  "msg": "capture_batch",
  "session_id": "20261030-USB-0001",
  "node_id": "usb-analyzer-a1b2",
  "batch_seq": 7,
  "events": [ /* capture_event.v1 */ ]
}
```

See [`CAPTURE.md`](CAPTURE.md).

## 8. Security (lab defaults)

Gate 0: trusted LAN only.  
Future: TLS optional, tunnel tokens, no secrets in git (WiFiManager pattern from ESP32.esp-hub).
