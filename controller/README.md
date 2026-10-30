# Controller (Debian)

Python (planned) control plane running on Debian.

| Module | Responsibility | Gate |
|--------|----------------|------|
| `discovery/` | Accept HELLO / heartbeat | 1 |
| `node_registry/` | Live node state | 1 |
| `sessions/` | Role + session assignment | 3 |
| `capture/` | Batch ingest + integrity | 5 |
| `decoder/` | BOT/SCSI L2 | 6 |
| `storage/` | Session files / replay | 5+ |
| `analysis/` | Diff / timing | 8 |
| `api/` | HTTP/SSE for Web UI | 7 |

## Modes

- **Relay (Mode B):** controller forwards USB-tunnel frames between nodes
- **Direct (Mode A):** controller only assigns peers; nodes tunnel directly

See `docs/ARCHITECTURE.md` and `docs/PROTOCOL.md`.
