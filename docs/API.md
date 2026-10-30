# Debian Web API (lab UI)

**Status:** design for Gate 7 (usable stubs earlier)  
**Audience:** browser ↔ controller on Debian  
**Not:** ESP↔ESP USB tunnel framing (that stays `PROTOCOL.md`)

Base URL (provisional): `http://<debian>:8080`  
Live stream: **SSE** preferred (`text/event-stream`); WebSocket optional later.

Epistemic labels in UI: every live event must carry `timestamp_domain` and `node_id` / `role`. Do not imply µs sync across nodes without a sync protocol.

---

## 1. What the UI needs to show

```text
HOST (.88)                         DEVICE (.89)
  │                                  │
  │  SETUP / BULK / RESET …          │
  │◄──────── timeline ──────────────►│
  │                                  │
 RX/TX rates · STALL · overflow · TCP health
```

| Pane | Data source |
|------|-------------|
| Node strip | registry + HELLO health |
| Dual timeline | capture events (host lane / device lane) |
| Packet inspector | one event (+ decoded L2 when present) |
| Stats | aggregated counters |
| Command bar | session / role / capture / later fault ops |

---

## 2. REST — read model

### Nodes

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/api/v1/nodes` | Live registry (id, ip, role, fw, rssi, usb_devices, last_hello_age_ms) |
| `GET` | `/api/v1/nodes/{node_id}` | One node + last health |
| `POST` | `/api/v1/nodes/{node_id}/role` | `{ "role": "host"\|"device"\|"unassigned" }` → ESP `assign_role` |

### Sessions

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/api/v1/sessions` | Recent sessions (id, mode, nodes, state) |
| `GET` | `/api/v1/sessions/{session_id}` | Session meta + integrity summaries |
| `POST` | `/api/v1/sessions` | Create: `{ mode, host_node, device_node, capture_mode }` |
| `POST` | `/api/v1/sessions/{session_id}/start` | Bind peers, `session_start` to ESPs |
| `POST` | `/api/v1/sessions/{session_id}/stop` | Stop + request integrity |
| `GET` | `/api/v1/sessions/{session_id}/events` | Query: `?after_seq=&limit=&node_id=&event_type=` |
| `GET` | `/api/v1/sessions/{session_id}/stats` | Snapshot counters |

### Capture / inspector

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/api/v1/sessions/{session_id}/events/{seq}` | Full event (payload if stored) |
| `GET` | `/api/v1/sessions/{session_id}/export` | `?format=jsonl\|ndjson` download |

### Controller health

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/api/v1/health` | controller uptime, connected nodes, ingest ports |

---

## 3. SSE — live stream

`GET /api/v1/stream?session_id=…` (session optional → global bus)

Event types (`event:` field):

| SSE event | Payload | UI use |
|-----------|---------|--------|
| `hello` | node_hello body | node strip refresh |
| `node_gone` | `{ node_id }` | mark offline |
| `session` | session state change | toolbar |
| `usb_event` | `capture_event.v1` | timeline + inspector |
| `stats` | rates / ring / tcp (1–2 Hz) | sparkline strip |
| `cmd_ack` | command result | toast / log |
| `log` | `{ level, msg, node_id? }` | console pane |

Example `usb_event` (already schema’d):

```json
{
  "version": 1,
  "session_id": "20261030-USB-0001",
  "node_id": "usb-analyzer-339c",
  "role": "host",
  "seq": 42,
  "timestamp_us": 123456789,
  "timestamp_domain": "local",
  "direction": "host_to_device",
  "event_type": "bot_cbw",
  "transfer_type": "bulk",
  "endpoint": 1,
  "length": 31,
  "status": "ok",
  "decoded": { "protocol": "msc-bot", "command": "READ_10", "lba": 17489 }
}
```

**RX/TX mapping for graphics**

| `direction` | Host lane | Device lane |
|-------------|-----------|-------------|
| `host_to_device` | TX out | RX in |
| `device_to_host` | RX in | TX out |

Always draw **two lanes** keyed by `role`, not by IP alone.

---

## 4. Commands the UI may send

Commands go REST → controller → ESP control TCP (`PROTOCOL.md`).  
UI never talks to ESPs directly.

### Gate-safe now / soon (observe & session)

| UI action | REST | ESP `msg` | Gate |
|-----------|------|-----------|------|
| Assign role | `POST …/nodes/{id}/role` | `assign_role` | 1+ |
| Ping node | `POST /api/v1/nodes/{id}/ping` | `ping` | 1+ |
| Set capture mode | `POST …/sessions/{id}/capture_mode` | `set_capture_mode` | 5 |
| Start / stop session | `POST …/start\|stop` | `session_start` / `session_stop` | 3+ |
| Request descriptor dump | `POST …/nodes/{id}/usb/dump` | `usb_dump` (new) | 1–2 |
| Soft note / marker | `POST …/sessions/{id}/marker` | stored on Debian only | any |

### After proxy baseline only (dangerous)

| UI action | REST | ESP `msg` | Gate |
|-----------|------|-----------|------|
| Delay | `POST …/inject` `{op:"delay", ms}` | `inject` | 9 |
| Drop / duplicate | `{op:"drop"\|"dup"}` | `inject` | 9 |
| Stall / reset / disconnect | `{op:"stall"\|"reset"\|"disconnect"}` | `inject` | 9 |

**Rule:** inject endpoints return `403` until `session.flags.fault_injection == true` (explicit lab arm).

Suggested inject body:

```json
{
  "op": "delay",
  "ms": 10,
  "match": {
    "event_type": "bot_data",
    "direction": "host_to_device",
    "endpoint": 1
  },
  "count": 1
}
```

---

## 5. Minimal OpenAPI-shaped types

```text
NodeView {
  node_id, mac, ip, role, fw, git_sha,
  wifi_rssi, free_heap, usb_devices,
  last_usb_event?, tcp_state, last_seen_ms
}

SessionView {
  session_id,
  mode: relay|direct|store_forward|live|buffered|store|replay|burst|analyze,
  capture_mode: metadata-only|protocol|payload|full,
  host_node, device_node, state: idle|running|stopping|done,
  flags: { fault_injection: bool },
  transport?: { window, highest_seq_tx, highest_ack_rx, rx_buffer_pct }  // SF later
}

StatsView {
  session_id,
  host: { events_s, bytes_s, stall, reset, overflow, ring },
  device: { … },
  tcp: { rx_s, tx_s, errors, rtt_est_ms? }
}
```

---

## 6. Implementation slices (don’t boil the ocean)

| Slice | Ship | Depends on |
|-------|------|------------|
| **A — Now** | `GET /nodes` + SSE `hello`/`log` from existing discovery | Gate 1 done |
| **B** | Session create/start/stop + dual empty lanes | Gate 3 |
| **C** | `usb_event` SSE + inspector + stats | Gate 5 |
| **D** | BOT decode fields in inspector | Gate 6 |
| **E** | Full timeline UX polish | Gate 7 |
| **F** | Armed inject command bar | Gate 9 |

Slice **A** can land immediately on top of today’s HELLO stream without claiming USB capture.

---

## 7. Ports (Debian)

| Port | Service |
|------|---------|
| `9750` | ESP control (existing) |
| `9751` | ESP capture ingest (later) |
| `8080` | Web UI + `/api/v1` |

---

## 8. Explicit non-goals for the first UI

- D+/D− oscilloscope (Gate 10 / UNKNOWN)
- Pretending host/device timestamps are synchronized
- Vehicle/NBT-specific dashboards
- Unauthenticated inject on a shared LAN without arm flag
