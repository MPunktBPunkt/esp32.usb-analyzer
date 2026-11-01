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

## Run (Slice A: discovery + Web UI)

```bash
cd /path/to/esp32.usb-analyzer
./tools/run_lab_controller.sh restart          # foreground-friendly restart + health check
./tools/install_lab_controller_service.sh     # user systemd autostart (preferred)
# or manually:
PYTHONPATH=. python3 -m controller.run --http-port 8080 --control-port 9750
```

- ESP control: TCP `:9750`
- Lab UI + API: http://\<debian\>:8080/  
- `GET /api/v1/nodes`, `GET /api/v1/stream` (SSE), `POST …/ping`, `POST …/role`
- Logs: `logs/controller.log`

Legacy discovery-only: `python3 controller/discovery/server.py --port 9750`.

## Modes

- **Relay (Mode B):** controller forwards USB-tunnel frames between nodes
- **Direct (Mode A):** controller only assigns peers; nodes tunnel directly

See `docs/ARCHITECTURE.md` and `docs/PROTOCOL.md`.
