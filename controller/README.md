# Controller (Debian)

Python control plane running on Debian.

| Module | Responsibility | Gate |
|--------|----------------|------|
| `discovery/` | Accept HELLO / heartbeat | 1 ✅ |
| `node_registry/` | Live node state (in discovery process for now) | 1 |
| `sessions/` | Role + session assignment | 3 |
| `capture/` | Batch ingest + integrity | 5 |
| `decoder/` | BOT/SCSI L2 | 6 |
| `storage/` | Session files / replay | 5+ |
| `analysis/` | Diff / timing | 8 |
| `api/` | HTTP/SSE for Web UI | 7 |

## Gate 1 discovery

```bash
python3 controller/discovery/server.py --host 0.0.0.0 --port 9750 --auto-role host
```

Prints HELLO lines and a periodic registry dump. Optional `--auto-role host|device`.

## Modes

- **Relay (Mode B):** controller forwards USB-tunnel frames between nodes
- **Direct (Mode A):** controller only assigns peers; nodes tunnel directly

See `docs/ARCHITECTURE.md` and `docs/PROTOCOL.md`.
