# Gate 7 status — Web timeline (slice 0.0.19)

**Goal:** Live dual-node capture timeline + packet inspector + stats. Timestamp domains labeled.

## 0.0.19 slice

| Piece | Status |
|-------|--------|
| Controller proxy `GET /api/v1/capture?role=` · `/api/v1/capture/dual` | done |
| Debian lab UI Host/Device capture lanes | done |
| `timestamp_domain=local_per_node` labeled | done |
| Packet inspector (click → JSON) | done (0.0.19+) |
| Event type filters + live stats | done (0.0.22) |
| Sync µs / cross-node align | **not yet** |
| Claim synchronized dual clocks | **REJECTED** until Gate 8 |

## API

```text
GET /api/v1/capture?role=host|device&since=&limit=
GET /api/v1/capture/dual?since_host=&since_device=&limit=
```

Proxies to each ESP `GET /api/capture` (CORS-safe). Lab IP fallback `.88` / `.89` when HELLO role missing.
