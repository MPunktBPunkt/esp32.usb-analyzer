# Gate 6 status — Decoder (slice 0.0.17–0.0.18)

**Goal:** MSC BOT/SCSI decode from class hooks (not App-callback fiction as wire truth).

## Slice

| Piece | Status |
|-------|--------|
| Capture `decoded.command` READ_10 / WRITE_10 / CSW + `scsi_opcode` | done (inferred; JSON decimal) |
| `bot_cbw` / `bot_csw` events | lab-inferred markers · **`source: class_api_inferred`** |
| Host MSC **WRITE** probe + Device write-cache (64 LBA) + readback | MEASURED ~250–460 kB/s |
| TinyUSB `mscd_xfer_cb` / real CBW bytes | **not yet** |
| Claim wire CBW == App callback | **REJECTED** (unchanged) |

## API

`POST /api/lab/usb-host-probe?dir=read|write&blocks=&chunk=&lba=`

## Lab notes

- WRITE probe sets a marker byte then readbacks — `pattern_ok` is write/readback integrity.
- Subsequent READ at same LBA may fail `lab_sector_v1` until cache eviction / unused LBA (`lba=`).
