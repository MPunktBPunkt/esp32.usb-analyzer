# Web UI (lab)

Technical lab surface — not a marketing page.

## Planned views

| Path | Purpose | Gate |
|------|---------|------|
| `timeline/` | Synchronized Host↔Device event lanes | 7 |
| `packets/` | Packet inspector | 7 |
| `usb/` | Protocol/state machine view | 7 |
| `statistics/` | USB/TCP/ring live stats | 7 |
| `dplus-dminus/` | Level-1 protocol state first; Level 2/3 research only | 7 / 10 |

## Slice A (now)

Static UI served by `controller.run` on `:8080`:

- Host / Device lanes (node health from HELLO)
- Node list with ping / role buttons
- SSE live log

Capture timeline fills in at Gate 5+.

```bash
PYTHONPATH=. python3 -m controller.run
# open http://192.168.178.187:8080/
```

## Rules

- Label `timestamp_domain` in the UI
- Never imply D+/D− waveforms without Gate 10 MEASURED proof
- Prefer one clear composition per view (lab density OK; no fake oscilloscope chrome)
