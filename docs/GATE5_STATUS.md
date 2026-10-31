# Gate 5 status — Capture (slice started 0.0.16)

**Goal:** L1 session events + integrity summary (`overflow` / `dropped` / `gaps`).

## In 0.0.16

| Piece | Status |
|-------|--------|
| `CaptureLog` ring (256 events) | done |
| `GET /api/capture?since=&limit=` | done — `capture_batch` JSON |
| Status `capture{}` | seq / stored / dropped / overflow |
| Hooks: Host attach/detach/enum, MSC probe/proxy read | done |
| Hooks: Device USB start/stop, sampled gadget reads, proxy rsp | done |
| Hooks: TCP bulk summary | done |
| Analyzer UI timeline | done (ESP web) |
| Dual-node Debian timeline (Gate 7) | not yet |
| TinyUSB class-hook BOT CBW/CSW (Gate 6) | not yet |
| D+/D− wire (Gate 10) | **not claimed** |

## Claim

**MEASURED / lab:** stack-visible L1/lab events.  
**REJECTED / not claimed:** full bus analyzer / every packet on the wire.
