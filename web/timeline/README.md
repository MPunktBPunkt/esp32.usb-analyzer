# Timeline (Gate 7)

Dual Host/Device L1 capture lanes live in the main lab UI (`/`):

- Polls `GET /api/v1/capture/dual`
- Events from ESP `CaptureLog` (not D+/D−)
- Each lane uses the node's local `timestamp_us` — **not synchronized**

Packet inspector / cross-node sync → later Gate 7/8 slices.
