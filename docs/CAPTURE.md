# Capture model

**Status:** Gate 0 schema + buffering design

## 1. Layers

| Layer | Name | Status for this project |
|-------|------|-------------------------|
| **L0** | USB PHY / Wire (D+/D−, DATA0/1, ACK/NAK) | Offline decoder **MEASURED**; **live** tap UNKNOWN (Path B) — see Gate 10 |
| **L1** | USB Controller / Host-Device stack | GO for design; implement Gate 5 |
| **L2** | Protocol (MSC BOT / SCSI / descriptors) | GO via class hooks; implement Gate 6 |

### Epistemic warnings (from technology-audit)

| Claim | Status |
|-------|--------|
| App `onRead` / `tud_msc_read10_cb` = full CBW event | **REJECTED** |
| Chunk `bufsize` = CDB transfer length | **REJECTED** |
| Host eBPF URB decode = physical packet layer | **REJECTED** |
| SET_ADDRESS = mounted | **REJECTED** — mount at SET_CONFIGURATION |
| Clear-Halt cures `MSC_STAGE_NEED_RESET` | **REJECTED** — needs BOT Reset |
| CBW 31 B / CSW 13 B, sig USBC/USBS | **FACT** |

Prefer TinyUSB class-driver instrumentation points (`mscd_xfer_cb` → `bot_cbw`, `send_csw` → `bot_csw`, enum hooks in `usbd.c`) over App callbacks alone.

## 2. Capture modes

| Mode | Emits |
|------|-------|
| `metadata-only` | Headers, timing, status — no payload |
| `protocol` | + decoded L2 fields when available |
| `payload` | + truncated/raw payload |
| `full` | metadata + protocol + payload (lab; high bandwidth) |

Payload is never mandatory on the wire to Debian.

## 3. Event schema (`capture_event.v1`)

```json
{
  "version": 1,
  "session_id": "20261030-USB-0001",
  "node_id": "usb-analyzer-a1b2",
  "seq": 1234,
  "timestamp_us": 123456789,
  "timestamp_domain": "local",
  "role": "host",
  "fw_sha": "deadbeef",
  "direction": "host_to_device",
  "endpoint": 1,
  "transfer_type": "bulk",
  "event_type": "data",
  "length": 512,
  "status": "ok",
  "usb_state": "configured",
  "payload": null,
  "decoded": {
    "protocol": "msc-bot",
    "command": "READ_10",
    "lba": 17489,
    "transfer_length": 8,
    "tag": 42,
    "residue": 0,
    "csw_status": 0
  }
}
```

### Required fields

`version`, `session_id`, `node_id`, `seq`, `timestamp_us`, `timestamp_domain`, `role`, `event_type`, `status`

### Common `event_type` values

`reset`, `attach`, `detach`, `set_address`, `set_configuration`, `setup`, `data`, `ack_status`, `stall`, `bot_cbw`, `bot_csw`, `bot_data`, `error`, `overflow`, `sync_marker`

Schema file: [`schemas/capture_event.v1.schema.json`](schemas/capture_event.v1.schema.json)

## 4. BOT / SCSI decode notes

When L2 is available, record at least:

| Field | Source |
|-------|--------|
| `tag` | CBW/CSW |
| `total_bytes` / TransferLength | CBW |
| `dir` / Flags | CBW |
| `lun` | CBW |
| `cdb` / command | CBW command[16] |
| `residue` | CSW |
| `csw_status` | CSW (0/1/2) |

Do **not** invent NBT-specific CDB lengths or timeout FACs from desk literature alone — those remain host-specific MEASURED claims elsewhere.

## 5. Buffering

```text
USB ISR/callback
      │
      ▼
 ring buffer (internal RAM preferred for DMA-ish paths;
              PSRAM OK for large event journals — see evidence)
      │
      ▼
 batcher (N events or T ms)
      │
      ▼
 TCP capture ingest
```

| Rule | Reason |
|------|--------|
| Never block USB path on WiFi send | Coex / latency |
| Count `overflow` and `dropped` explicitly | Integrity |
| Prefer internal RAM for USB DMA buffers | S3 quirks signal |
| PSRAM for capture journals when enabled correctly | hu-probe pattern (MEASURED) |

**Evolution:** Gate 0/5 batching is the precursor to the planned **Store-and-Forward** data plane (dual PSRAM rings, 64–128 KiB blocks, sliding-window ACK, water-level backpressure, pipeline stage timestamps). That path is Mode A only and must wait for proxy + integrity baselines — see [`STORE_FORWARD.md`](STORE_FORWARD.md). Do not implement SF as “just another capture_mode enum” without the ring/transport split.

PSRAM enable requires board-correct `BOARD_HAS_PSRAM` + memory type (`qio_qspi` vs OPI). Board PSRAM ≠ auto-used (**REJECTED** assumption).

## 6. Session integrity summary

Required at STOP:

```json
{
  "session_id": "20261030-USB-0001",
  "node_id": "usb-analyzer-a1b2",
  "records": 12045,
  "bytes": 3840000,
  "dropped": 0,
  "overflow": 0,
  "first_seq": 1,
  "last_seq": 12045,
  "gaps": 0,
  "retransmissions": 0,
  "tcp_errors": 0,
  "ring_high_watermark": 0.42
}
```

Schema: [`schemas/integrity.v1.schema.json`](schemas/integrity.v1.schema.json)

## 7. Timeline semantics

Host-side and device-side events are shown on one timeline using:

1. `session_id` + `seq` per node
2. Optional Debian receive time as a third axis
3. Explicit label of `timestamp_domain`

Until a sync protocol exists, UI must show **per-node local time** and warn that cross-node µs alignment is approximate.

## 8. What Gate 5 must prove

A complete session file with:

- L1 events (at least reset/enum/control/bulk status)
- Integrity metrics with `overflow`/`gaps` accounted
- Clear statement of capture depth (callbacks vs class hooks vs URB)
