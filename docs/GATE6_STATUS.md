# Gate 6 status — Decoder (slice 0.0.17–0.0.26)

**Goal:** MSC BOT/SCSI decode from class hooks (not App-callback fiction as wire truth).

## Slice

| Piece | Status |
|-------|--------|
| Capture `decoded.command` READ_10 / WRITE_10 / CSW + `scsi_opcode` | done |
| App-inferred `bot_cbw` / `bot_csw` on Device | **removed** (0.0.26) — Host probe path still may emit inferred |
| TinyUSB `tud_msc_*_complete_cb` stage markers | done (0.0.20+) · notes only |
| TinyUSB `mscd_xfer_cb` / `send_csw` real CBW/CSW bytes | **done (0.0.26)** — vendored `src/tinyusb_patch/msc_device.c` (TinyUSB 0.18.0); `source: class_hook_mscd_xfer` + `payload_hex` |
| Claim wire CBW == App callback | **REJECTED** (unchanged) |
| D+/D− / PHY wire capture | **not claimed** |

## API / tools

`POST /api/lab/usb-host-probe?dir=read|write&blocks=&chunk=&lba=`  
`GET /api/capture` on Device  
`tools/lab_bot_class_hook_verify.py` — OTG Host↔Device cable

## Lab notes

- Vendor patch must stay aligned with Arduino-ESP32 TinyUSB **0.18.0**.
- Prebuilt `libarduino_tinyusb.a` still linked; our `msc_device.c.o` supplies symbols first so the archive member is skipped.
- WRITE probe sets a marker byte then readbacks — `pattern_ok` is write/readback integrity.
