# Lab AOA control handshake — MEASURED (OTG loop)

**Date:** 2026-10-31  
**Firmware:** Host/Device **0.0.32** (handshake since 0.0.29/30; bulk echo 0.0.32)  
**Topology:** `.88` Host OTG ↔ USB-C ↔ `.89` Device MSC+Vendor soft-composite  
**Repro:** `./tools/lab_aoa_handshake_verify.py` · bulk: `… --bulk`

Classification: **MEASURED** (this lab). Desk FACT path from
`technology-audit/technologies/usb-aoa` + hu-probe C1b lab notes.
**Not** NBT/HU wire, **not** BCL, **not** accessory bulk interface.

## Claim boundary

| In scope | Out of scope |
|----------|----------------|
| EP0 AOA 51→52×6→53 on OTG loop | HU Connected / BCL framing |
| Device EP0 responder + Host accessory role | Phone/AAIdrive UI |
| Soft-reenum VID/PID → `18d1:2d00` | BCL framing on bulk |
| Lab A (respond ON) / Lab B (respond OFF) | Transparent MITM |
| Vendor bulk OUT/IN echo (lab Rohr) | HU Connected |

## MEASURED (Lab A, profile=`lab`)

| Step | Result |
|------|--------|
| GET_PROTOCOL (51) | **PASS** — protocol **1** (AOA 1.0 LE) |
| SEND_STRING (52) ×6 | **PASS** — Host `stringsOk=6`; Device `aoa52=6` |
| START (53) | **PASS** |
| Re-enum | **PASS** — `303a:1001` → **`18d1:2d00`** |
| Duration (51…reenum) | **~553 ms** (0.0.30 Lab A; band matches hu-probe Phone lab ~0,5–0,6 s) |
| Capture decode | Host `decoded.protocol=aoa` · `GET_PROTOCOL`/`SEND_STRING`/`START` |

Device counters after Lab A: `aoa51=1` / `aoa52=6` / `aoa53=1` / `aoa51Ok=1`.

## MEASURED (Lab bulk echo, 0.0.32)

| Step | Result |
|------|--------|
| Host `vendorOpen` continuous | **PASS** — EP OUT `0x02` / IN `0x82`, MPS 64 |
| Host bulk OUT `AOA-LAB` (7 B) | **PASS** |
| Device TinyUSB FIFO RX → echo TX | **PASS** — `bulkRx=7` `bulkTx=7` |
| Host RX match | **PASS** — `duration_ms≈30` |

Root cause of 0.0.31 gap: Arduino `USBVendor` RX queue alone missed OUT; fix = poll `tud_vendor_n_available/read/write` in Device loop + Host continuous IN drain.

## Lab B (responder OFF)

Host handshake **fails** (expected STALL/fail on 51). Device counters do not complete a full 51+52×6+53 rise. Confirms respond path is gated (hu-probe Lab B pattern).

## Timing note

AOA AOSP defines **order**, not handshake deadlines. Documented elsewhere:

- AOSP Car `AoapInterface`: bulk timeout **2000 ms** (stack, not Spec)
- hu-probe Phone lab: re-enum **~0,5–0,6 s** after 53
- This OTG lab: **~0,55 s** Host `duration_ms` including soft-reenum wait

## Epistemic

| Claim | Class |
|-------|-------|
| ESP Host can drive AOA control against ESP Device responder | **MEASURED** |
| Soft-reenum presents Google accessory VID/PID (MSC+Vendor composite) | **MEASURED** |
| Vendor bulk echo Host↔Device after/without START | **MEASURED** (lab Rohr) |
| Lab BCL 8-byte BE + init over AOA bulk echo | **MEASURED** (0.0.33 desk header) |
| This proves HU AOA or BCL-on-HU-wire | **REJECTED** / not claimed |
