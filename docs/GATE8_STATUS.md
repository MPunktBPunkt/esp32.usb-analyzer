# Gate 8 status — Cross-node sync (slice 0.0.27)

**Goal:** Estimate Host↔Device clock offset for dual timeline; never claim hard µs lock.

## Slice

| Piece | Status |
|-------|--------|
| `POST /api/lab/sync-marker?nonce=` on each ESP | done — emits `sync_marker` |
| Controller `POST /api/v1/sync` multi-sample median + residual | done |
| Dual capture includes `sync.offset_us` when available | done |
| Lab UI Sync button + badge | done |
| Dual-lane display applies offset to Device timestamps (session view) | done (0.0.28) — display-only |
| `tools/lab_sync_offset.py` | done |
| Hard µs lock / PTP / crystal disciplining | **not claimed** |

## Claim

`cross_node_offset_estimate` — HTTP-skewed offset ± residual. Suitable for rough dual-lane alignment only.
